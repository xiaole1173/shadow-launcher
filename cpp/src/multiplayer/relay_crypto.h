// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Public API for encrypted relay address decryption
#pragma once
#include <QString>

namespace Relay {

// Returns decrypted relay endpoint: "tcp://host:port"
QString relayEndpoint();

// Returns decrypted IP prefix for filtering: "x.x."
QString relayPrefix();

} // namespace Relay

#if __has_include("encrypted_addr_local.h")
namespace Worker {
QString workerEndpoint();
} // namespace Worker
#endif
