// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Port Request — probe a specific port for availability, or get any free port.
// Aligns with Terracotta ports.rs: request_specific() + request()
#pragma once
#include <QtGlobal>
#include <QByteArray>

namespace PortRequest {

// Try to claim a specific localhost port by briefly binding a TCP listener.
// Returns the port if available; returns 0 on conflict.
quint16 requestSpecific(quint16 port);

// Get any free ephemeral port (bind to 127.0.0.1:0).
// Falls back to basePort + 35780 on failure (aligns with Terracotta's fallback).
quint16 requestFree(quint16 basePort = 0);

} // namespace PortRequest
