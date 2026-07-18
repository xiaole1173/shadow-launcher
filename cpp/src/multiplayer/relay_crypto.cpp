// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// AES-256-GCM decryption for relay server addresses & beta auth URL
// Uses Windows CNG (bcrypt.dll) — zero extra deps
#if __has_include("encrypted_addr_local.h")
#include "encrypted_addr_local.h"
#else
#include "encrypted_addr.h"
#endif

#include <windows.h>
#include <bcrypt.h>
#include <QString>
#include <QByteArray>

#pragma comment(lib, "bcrypt.lib")

namespace {

static void buildKey(uint8_t out[32], const uint8_t* rawKey, const uint8_t* salt)
{
    memcpy(out, rawKey, 32);
    for (int i = 0; i < 32; ++i)
        out[i] ^= salt[i % 8];
}

static QByteArray aesGcmDecrypt(
    const uint8_t* nonce, uint32_t nonceLen,
    const uint8_t* ct,    uint32_t ctLen,
    const uint8_t* tag,   uint32_t tagLen,
    const uint8_t* rawKey, const uint8_t* salt)
{
    if (ctLen == 0) return {};

    uint8_t key[32];
    buildKey(key, rawKey, salt);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0))
        { SecureZeroMemory(key, 32); return {}; }

    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
            (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))
        { BCryptCloseAlgorithmProvider(hAlg, 0); SecureZeroMemory(key, 32); return {}; }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key, 32, 0))
        { BCryptCloseAlgorithmProvider(hAlg, 0); SecureZeroMemory(key, 32); return {}; }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = (PUCHAR)nonce;
    auth.cbNonce = nonceLen;
    auth.pbTag   = (PUCHAR)tag;
    auth.cbTag   = tagLen;

    ULONG outLen = 0;
    QByteArray plain(static_cast<int>(ctLen), Qt::Uninitialized);
    NTSTATUS s = BCryptDecrypt(hKey,
        (PUCHAR)ct, ctLen,
        &auth, nullptr, 0,
        (PUCHAR)plain.data(), ctLen, &outLen, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    SecureZeroMemory(key, 32);

    if (s) return {};
    plain.resize(static_cast<int>(outLen));
    return plain;
}

static QString decryptToString(
    const uint8_t* nonce, uint32_t nonceLen,
    const uint8_t* ct,    uint32_t ctLen,
    const uint8_t* tag,   uint32_t tagLen,
    const uint8_t* rawKey, const uint8_t* salt)
{
    QByteArray plain = aesGcmDecrypt(nonce, nonceLen, ct, ctLen, tag, tagLen, rawKey, salt);
    if (plain.isEmpty()) return {};
    QString result = QString::fromUtf8(plain);
    SecureZeroMemory(plain.data(), plain.size());
    return result;
}

} // anonymous namespace

namespace Relay {

QString relayEndpoint()
{
    return decryptToString(kRelayNonce, 12, kRelayCT, kRelayCTLen, kRelayTag, 16, kRawKey, kSalt);
}

QString relayPrefix()
{
    return decryptToString(kPrefixNonce, 12, kPrefixCT, kPrefixCTLen, kPrefixTag, 16, kRawKey, kSalt);
}

} // namespace Relay

// Worker namespace — only available when local config is built in
#if __has_include("encrypted_addr_local.h")
namespace Worker {

QString workerEndpoint()
{
    return decryptToString(kWorkerNonce, 12, kWorkerCT, kWorkerCTLen, kWorkerTag, 16, kRawKey, kSalt);
}

} // namespace Worker
#endif
