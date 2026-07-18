// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

// ── AES-256-GCM token encryption via Windows CNG/BCrypt ──
// Key: 256-bit random, protected by DPAPI (per-user, non-exportable)
// Format: Base64( nonce(12B) || ciphertext || tag(16B) )
//
// Usage:
//   QString encrypted = TokenCrypto::encrypt(plaintext);
//   QString decrypted = TokenCrypto::decrypt(encrypted);

#include <QString>

namespace ShadowLauncher {

class TokenCrypto {
public:
    // Returns true if the string looks like an encrypted blob (Base64, sufficient length)
    static bool looksEncrypted(const QString &s);

    // Encrypt plaintext → Base64 ciphertext. Returns empty string on failure.
    static QString encrypt(const QString &plaintext);

    // Decrypt Base64 ciphertext → plaintext. Returns empty string on failure.
    static QString decrypt(const QString &ciphertext);

private:
    static bool ensureKey();
    static QByteArray loadOrCreateKey();
    static QByteArray protectKey(const QByteArray &key);
    static QByteArray unprotectKey(const QByteArray &blob);

    static QByteArray s_key;          // 32-byte AES-256 key (cached after first load)
    static QString s_keyPath;         // path to DPAPI-protected key file
};

} // namespace ShadowLauncher
