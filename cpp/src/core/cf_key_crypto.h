// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// CF API Key 解密运行时 —— 与 tools/encrypt_cf_key.py 严格对应。
//
// 链路：HKDF-SHA256(salt, IKM) 派生 32 字节 AES-256 密钥
//       → AES-256-GCM 解密（12 字节 nonce，16 字节 tag）
//
// 实现：
//   - SHA-256/HMAC/HKDF：手写，基于 Qt QCryptographicHash（RFC 5869，
//     已用 RFC 测试向量验证）
//   - AES-256-GCM：Windows CNG（bcrypt.dll，系统自带，零第三方依赖）
//     —— BCRYPT_AES_ALGORITHM + GCM 链模式，微软官方实现，免手写 AES/GHASH
//
// 密文材料由本地私有头文件 cf_api_key_local.h（git 忽略）经
// SHADOW_CF_ENC_*_HEX 宏提供；本文件不含任何密钥材料，可安全入库。

#pragma once

#include <QCryptographicHash>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#endif

namespace ShadowLauncher {

namespace CfKeyCrypto {

// ── HMAC-SHA256 ──
inline QByteArray hmacSha256(const QByteArray& key, const QByteArray& msg)
{
    constexpr int kBlock = 64;
    QByteArray k = key;
    if (k.size() > kBlock)
        k = QCryptographicHash::hash(k, QCryptographicHash::Sha256);
    if (k.size() < kBlock)
        k = k + QByteArray(kBlock - k.size(), '\0');

    QByteArray ipad(kBlock, 0x36), opad(kBlock, 0x5c);
    for (int i = 0; i < kBlock; ++i) {
        ipad[i] = static_cast<char>(static_cast<quint8>(ipad[i]) ^ static_cast<quint8>(k.at(i)));
        opad[i] = static_cast<char>(static_cast<quint8>(opad[i]) ^ static_cast<quint8>(k.at(i)));
    }
    return QCryptographicHash::hash(
        opad + QCryptographicHash::hash(ipad + msg, QCryptographicHash::Sha256),
        QCryptographicHash::Sha256);
}

// ── HKDF-SHA256 (RFC 5869) ──
inline QByteArray hkdfSha256(const QByteArray& ikm, const QByteArray& salt,
                             const QByteArray& info, int length = 32)
{
    const QByteArray prk = hmacSha256(salt.isEmpty() ? QByteArray(32, '\0') : salt, ikm);
    QByteArray out, t;
    quint8 counter = 1;
    while (out.size() < length) {
        t = hmacSha256(prk, t + info + QByteArray(1, static_cast<char>(counter++)));
        out += t;
    }
    return out.left(length);
}

// ── AES-256-GCM 解密（Windows CNG）──
// 返回空 QByteArray 表示 tag 校验失败或参数非法。
inline QByteArray gcmDecrypt(const QByteArray& key, const QByteArray& nonce,
                             const QByteArray& ciphertext, const QByteArray& tag,
                             const QByteArray& aad = QByteArray())
{
#ifdef Q_OS_WIN
    if (key.size() != 32 || nonce.size() != 12 || tag.size() != 16) return {};

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st)) return {};

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                           reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                    reinterpret_cast<PUCHAR>(const_cast<char*>(key.constData())),
                                    static_cast<ULONG>(key.size()), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce   = reinterpret_cast<PUCHAR>(const_cast<char*>(nonce.constData()));
    info.cbNonce   = static_cast<ULONG>(nonce.size());
    info.pbTag     = reinterpret_cast<PUCHAR>(const_cast<char*>(tag.constData()));
    info.cbTag     = static_cast<ULONG>(tag.size());
    if (!aad.isEmpty()) {
        info.pbAuthData = reinterpret_cast<PUCHAR>(const_cast<char*>(aad.constData()));
        info.cbAuthData = static_cast<ULONG>(aad.size());
    }

    QByteArray out(ciphertext.size(), '\0');
    ULONG done = 0;
    st = BCryptDecrypt(hKey,
                       reinterpret_cast<PUCHAR>(const_cast<char*>(ciphertext.constData())),
                       static_cast<ULONG>(ciphertext.size()),
                       &info,
                       nullptr, 0,                       // GCM：IV 由 nonce 承载
                       reinterpret_cast<PUCHAR>(out.data()),
                       static_cast<ULONG>(out.size()),
                       &done, 0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(st)) return {};                 // 含 STATUS_AUTH_TAG_MISMATCH
    out.resize(static_cast<int>(done));
    return out;
#else
    Q_UNUSED(key); Q_UNUSED(nonce); Q_UNUSED(ciphertext); Q_UNUSED(tag); Q_UNUSED(aad);
    return {};
#endif
}

// 便捷入口：从 cf_api_key_local.h 的 HEX 宏解密 CF API Key。
// 任一宏缺失/为空 → 返回空串（调用方回退环境变量/配置文件）。
inline QString decryptEmbeddedCfKey(const char* ikmHex, const char* saltHex,
                                    const char* nonceHex, const char* cipherHex,
                                    const char* tagHex)
{
    auto fromHex = [](const char* s) -> QByteArray {
        if (!s || !*s) return {};
        const QByteArray h(s);
        QByteArray out(h.size() / 2, '\0');
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        for (int i = 0; i + 1 < h.size(); i += 2) {
            const int hi = nib(h.at(i)), lo = nib(h.at(i+1));
            if (hi < 0 || lo < 0) return {};
            out[i/2] = static_cast<char>((hi << 4) | lo);
        }
        return out;
    };

    const QByteArray ikm   = fromHex(ikmHex);
    const QByteArray salt  = fromHex(saltHex);
    const QByteArray nonce = fromHex(nonceHex);
    const QByteArray ct    = fromHex(cipherHex);
    const QByteArray tag   = fromHex(tagHex);
    if (ikm.isEmpty() || ct.isEmpty() || tag.size() != 16) return {};

    const QByteArray key = hkdfSha256(ikm, salt, QByteArrayLiteral("ShadowLauncher-CF-Key-v1"), 32);
    const QByteArray pt  = gcmDecrypt(key, nonce, ct, tag);
    return QString::fromUtf8(pt);
}

} // namespace CfKeyCrypto
} // namespace ShadowLauncher
