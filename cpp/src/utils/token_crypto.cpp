// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "token_crypto.h"
#include "logger.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QString>

#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <wincrypt.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace ShadowLauncher {

QByteArray TokenCrypto::s_key;
QString TokenCrypto::s_keyPath;

// ═══════════════════════════════════════════════════
// Key management (DPAPI-protected)
// ═══════════════════════════════════════════════════

bool TokenCrypto::ensureKey()
{
    if (!s_key.isEmpty())
        return true;

    s_key = loadOrCreateKey();
    return !s_key.isEmpty();
}

QByteArray TokenCrypto::loadOrCreateKey()
{
    // Path: %APPDATA%/ShadowTeam/Shadow Launcher/shadow/.token_key
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + QStringLiteral("/shadow");
    s_keyPath = dataDir + QStringLiteral("/.token_key");

    QFileInfo fi(s_keyPath);
    QDir().mkpath(fi.absolutePath());

    QFile file(s_keyPath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray blob = file.readAll();
        file.close();
        if (!blob.isEmpty()) {
            QByteArray key = unprotectKey(blob);
            if (key.size() == 32) {
                qCInfo(logAccount) << QStringLiteral("[TokenCrypto] 加载已有密钥");
                return key;
            }
            qCWarning(logAccount) << QStringLiteral("[TokenCrypto] 已有密钥无效 重新生成");
        }
    }

    // Generate new 256-bit random key
    QByteArray newKey(32, Qt::Uninitialized);
    QRandomGenerator *rng = QRandomGenerator::system();
    for (int i = 0; i < 32; ++i)
        newKey[i] = static_cast<char>(rng->bounded(256));

    QByteArray blob = protectKey(newKey);
    if (blob.isEmpty()) {
        qCCritical(logAccount) << QStringLiteral("[TokenCrypto] 密钥保护失败");
        return {};
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(blob);
        file.close();
        // Hide the key file
        SetFileAttributesW(reinterpret_cast<LPCWSTR>(s_keyPath.utf16()), FILE_ATTRIBUTE_HIDDEN);
        qCInfo(logAccount) << QStringLiteral("[TokenCrypto] 新密钥已生成并保护");
    } else {
        qCCritical(logAccount) << QStringLiteral("[TokenCrypto] 密钥文件写入失败");
        return {};
    }

    return newKey;
}

QByteArray TokenCrypto::protectKey(const QByteArray &key)
{
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(key.constData()));
    inBlob.cbData = static_cast<DWORD>(key.size());

    DATA_BLOB outBlob = {0, nullptr};

    if (!CryptProtectData(&inBlob,
                          L"ShadowLauncher Token Encryption Key",
                          nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN,
                          &outBlob)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] CryptProtectData失败 错误码=%1").arg(GetLastError());
        return {};
    }

    QByteArray result(reinterpret_cast<const char *>(outBlob.pbData),
                      static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return result;
}

QByteArray TokenCrypto::unprotectKey(const QByteArray &blob)
{
    DATA_BLOB inBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData()));
    inBlob.cbData = static_cast<DWORD>(blob.size());

    DATA_BLOB outBlob = {0, nullptr};

    if (!CryptUnprotectData(&inBlob,
                            nullptr, nullptr, nullptr, nullptr,
                            0,
                            &outBlob)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] CryptUnprotectData失败 错误码=%1").arg(GetLastError());
        return {};
    }

    QByteArray result(reinterpret_cast<const char *>(outBlob.pbData),
                      static_cast<int>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return result;
}

// ═══════════════════════════════════════════════════
// Detection
// ═══════════════════════════════════════════════════

bool TokenCrypto::looksEncrypted(const QString &s)
{
    if (s.isEmpty()) return false;
    // Encrypted blobs are Base64 with 12B nonce + 16B tag + at least 1B payload = 28B
    // Base64-encoded → at least 40 chars (ceil(28*4/3) ≈ 38 + padding)
    if (s.length() < 38) return false;
    // Check if it looks like Base64 (only valid chars)
    for (const QChar &c : s) {
        ushort u = c.unicode();
        if (!((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') ||
              (u >= '0' && u <= '9') || u == '+' || u == '/' || u == '='))
            return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════
// AES-256-GCM via BCrypt
// ═══════════════════════════════════════════════════

static QByteArray aesGcmEncrypt(const QByteArray &key, const QByteArray &plaintext)
{
    if (key.size() != 32) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] AES-GCM加密失败 密钥长度不是32字节");
        return {};
    }

    // 1. Open algorithm provider
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] BCryptOpenAlgorithmProvider失败 状态=%1").arg(status);
        return {};
    }

    // 2. Set GCM mode
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                               sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] BCryptSetProperty GCM失败 状态=%1").arg(status);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    // 3. Generate symmetric key
    BCRYPT_KEY_HANDLE hKey = nullptr;
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                        (PUCHAR)key.constData(), 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] BCryptGenerateSymmetricKey失败 状态=%1").arg(status);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    // 4. Generate 12-byte random nonce
    QByteArray nonce(12, Qt::Uninitialized);
    QRandomGenerator *rng = QRandomGenerator::system();
    for (int i = 0; i < 12; ++i)
        nonce[i] = static_cast<char>(rng->bounded(256));

    // 5. Setup auth info
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    QByteArray tag(16, Qt::Uninitialized);
    authInfo.pbNonce = (PUCHAR)nonce.data();
    authInfo.cbNonce = 12;
    authInfo.pbTag = (PUCHAR)tag.data();
    authInfo.cbTag = 16;

    // 6. Encrypt
    QByteArray ciphertext(plaintext.size(), Qt::Uninitialized);
    ULONG cbResult = 0;
    status = BCryptEncrypt(hKey,
                           (PUCHAR)plaintext.constData(), plaintext.size(),
                           &authInfo,
                           nullptr, 0,
                           (PUCHAR)ciphertext.data(), ciphertext.size(),
                           &cbResult, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] BCryptEncrypt失败 状态=%1").arg(status);
        return {};
    }

    // 7. Assemble: nonce || ciphertext || tag → Base64
    QByteArray combined = nonce + ciphertext + tag;
    return combined.toBase64();
}

static QByteArray aesGcmDecrypt(const QByteArray &key, const QByteArray &combined)
{
    if (key.size() != 32) return {};
    if (combined.size() < 12 + 1 + 16) return {}; // min: nonce(12) + 1B data + tag(16)

    const QByteArray nonce = combined.left(12);
    const QByteArray tag = combined.right(16);
    const QByteArray ciphertext = combined.mid(12, combined.size() - 12 - 16);

    // 1. Open algorithm
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) return {};

    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                               sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    // 2. Generate key
    BCRYPT_KEY_HANDLE hKey = nullptr;
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                        (PUCHAR)key.constData(), 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    // 3. Setup auth info
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)nonce.constData();
    authInfo.cbNonce = 12;
    authInfo.pbTag = (PUCHAR)tag.constData();
    authInfo.cbTag = 16;

    // 4. Decrypt
    QByteArray plaintext(ciphertext.size(), Qt::Uninitialized);
    ULONG cbResult = 0;
    status = BCryptDecrypt(hKey,
                           (PUCHAR)ciphertext.constData(), ciphertext.size(),
                           &authInfo,
                           nullptr, 0,
                           (PUCHAR)plaintext.data(), plaintext.size(),
                           &cbResult, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        qCWarning(logAccount) << QStringLiteral("[TokenCrypto] BCryptDecrypt失败 (认证标签不匹配或数据损坏) 状态=%1").arg(status);
        return {};
    }

    return plaintext;
}

// ═══════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════

QString TokenCrypto::encrypt(const QString &plaintext)
{
    if (plaintext.isEmpty()) return {};
    if (!ensureKey()) return {};

    QByteArray utf8 = plaintext.toUtf8();
    QByteArray combined = aesGcmEncrypt(s_key, utf8);
    if (combined.isEmpty()) return {};

    return QString::fromLatin1(combined);
}

QString TokenCrypto::decrypt(const QString &ciphertext)
{
    if (ciphertext.isEmpty()) return {};
    if (!ensureKey()) return {};

    QByteArray combined = QByteArray::fromBase64(ciphertext.toLatin1());
    if (combined.isEmpty()) return {};

    QByteArray utf8 = aesGcmDecrypt(s_key, combined);
    if (utf8.isEmpty()) return {};

    return QString::fromUtf8(utf8);
}

} // namespace ShadowLauncher
