// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// AES-256-GCM decryption for flat opaque blob (relay server & Worker URL)
// Uses Windows CNG (bcrypt.dll) — zero extra deps
//
// The encrypted blob is split across 5 compiled fragments (separate .obj files),
// each with scrambled byte order. At runtime, they're assembled and descrambled
// into a single 243-byte buffer. This frustrates static analysis by:
//   1. Splitting the blob across multiple compilation units
//   2. Using a per-fragment random byte permutation (scramble)

#if __has_include("encrypted_frag_1.h")
// Real fragments exist (local dev build with encrypted_addr.py)
#include "encrypted_frag_1.h"
#include "encrypted_frag_2.h"
#include "encrypted_frag_3.h"
#include "encrypted_frag_4.h"
#include "encrypted_frag_5.h"
#endif

#include "encrypted_addr.h"  // Offset constants (used by both real and placeholder)

#include <windows.h>
#include <bcrypt.h>
#include <QString>
#include <QByteArray>

#pragma comment(lib, "bcrypt.lib")

namespace {

static void xorKey(uint8_t out[32], const uint8_t* rawKey, const uint8_t* salt)
{
    memcpy(out, rawKey, 32);
    for (int i = 0; i < 32; ++i)
        out[i] ^= salt[i % 8];
}

// ── Blob assembly: reassemble + descramble the 243-byte blob from fragments ──
#if __has_include("encrypted_frag_1.h")

struct BlobAssembler {
    uint8_t data[243] = {0};
    BlobAssembler() {
        size_t off = 0;
        auto restore = [&](const uint8_t* d, const uint8_t* p, size_t cnt) {
            for (size_t i = 0; i < cnt; ++i)
                this->data[off + p[i]] = d[i];
        };
        restore(Frag1::data, Frag1::perm, 49); off += 49;
        restore(Frag2::data, Frag2::perm, 49); off += 49;
        restore(Frag3::data, Frag3::perm, 49); off += 49;
        restore(Frag4::data, Frag4::perm, 48); off += 48;
        restore(Frag5::data, Frag5::perm, 48);
    }
};

static const uint8_t* getBlob()
{
    static const BlobAssembler blob;
    return blob.data;
}

#else
// Open source / CI build — all-zero placeholder blob
static const uint8_t* getBlob() { return kBlob; }
#endif

} // anonymous namespace

// ── Public: returns assembled blob pointer ──
const uint8_t* getAssembledBlob() { return getBlob(); }

// ── Public AES-256-GCM helper (used by multiplayer & beta validation) ──

QByteArray aesGcmDecrypt(
    const uint8_t* nonce, uint32_t nonceLen,
    const uint8_t* ct,    uint32_t ctLen,
    const uint8_t* tag,   uint32_t tagLen,
    const uint8_t* rawKey, const uint8_t* salt)
{
    if (ctLen == 0) return {};

    uint8_t key[32];
    xorKey(key, rawKey, salt);

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

// ── Namespace helpers for relay ──

namespace Relay {

QString relayEndpoint()
{
    QByteArray plain = aesGcmDecrypt(
        getBlob() + kRelayNonceOff, kRelayNonceLen,
        getBlob() + kRelayCTOff, kRelayCTLen,
        getBlob() + kRelayTagOff, kRelayTagLen,
        getBlob() + kRelayKeyOff, getBlob() + kRelaySaltOff);
    if (plain.isEmpty()) return {};
    return QString::fromUtf8(plain);
}

QString relayPrefix()
{
    QByteArray plain = aesGcmDecrypt(
        getBlob() + kRelayPrefixNonceOff, kRelayPrefixNonceLen,
        getBlob() + kRelayPrefixCTOff, kRelayPrefixCTLen,
        getBlob() + kRelayPrefixTagOff, kRelayPrefixTagLen,
        getBlob() + kRelayKeyOff, getBlob() + kRelaySaltOff);
    if (plain.isEmpty()) return {};
    return QString::fromUtf8(plain);
}

} // namespace Relay
