// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Scaffolding Protocol Constants + Binary Codec
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Scaffolding {

// ── Standard protocol names ──
static const QString kPing("c:ping");
static const QString kProtocols("c:protocols");
static const QString kServerPort("c:server_port");
static const QString kPlayerPing("c:player_ping");
static const QString kPlayerPong("c:player_pong");
static const QString kPlayerProfilesList("c:player_profiles_list");
static const QString kPlayerEasyTierId("c:player_easytier_id");

// Must-implement basic set
QStringList basicProtocols();

// ── Center discovery hostname pattern ──
static const QString kCenterHostnamePrefix("scaffolding-mc-server-");

// ── Heartbeat interval ──
constexpr int kHeartbeatIntervalMs = 5000;

// ── Response status codes ──
constexpr quint8 kStatusOk = 0;
constexpr quint8 kStatusServerNotStarted = 32;
constexpr quint8 kStatusUnknown = 255;

// ── Big-endian binary packet codec ──
QByteArray buildPacket(const QString& type, const QByteArray& body);
QString packProtocolList(const QStringList& protocols);
QStringList unpackProtocolList(const QString& packed);

} // namespace Scaffolding
