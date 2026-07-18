// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// AES-256-GCM decrypt — flat opaque blob interface
#pragma once
#include <QString>
#include <QByteArray>

// Low-level AES-256-GCM decrypt (used by relay + beta validation)
// Returns empty QByteArray on failure/zero-length input.
QByteArray aesGcmDecrypt(
    const uint8_t* nonce, uint32_t nonceLen,
    const uint8_t* ct,    uint32_t ctLen,
    const uint8_t* tag,   uint32_t tagLen,
    const uint8_t* rawKey, const uint8_t* salt);

namespace Relay {

// Returns decrypted relay endpoint: "tcp://host:port"
QString relayEndpoint();

// Returns decrypted IP prefix for filtering: "x.x."
QString relayPrefix();

} // namespace Relay
