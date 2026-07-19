// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QString>
#include <QProcessEnvironment>

// ── Elevated session handoff ──
// Non-elevated instance saves multiplayer params to a temp config,
// then starts an elevated copy of itself. The elevated instance
// reads the config and continues from where the non-elevated left off.
namespace ElevatedSession {

// ── State query ──
bool isActive();
void setActive(bool active);

// ── Stored session data (read by elevated instance) ──
struct SessionData {
    QString networkName;
    QString networkKey;
    QString relayEp;
    QString hostname;
    QString roomCode;
    quint16 mcPort = 0;
};

SessionData currentSession();
void setCurrentSession(const SessionData& data);

// ── Stored relay endpoint (convenience, read by EasyTierProcess) ──
QString relayEndpoint();
void setRelayEndpoint(const QString& v);

// ── Save full multiplayer state to a temp config file ──
// Returns the temp file path. Caller should pass this as --elevate-config
// to the elevated instance.
QString saveElevationConfig(const QString& networkName,
                           const QString& networkKey,
                           const QString& relayEp,
                           const QString& hostname,
                           const QString& roomCode,
                           quint16 mcPort);

// ── Load config from file and delete it ──
SessionData loadAndDelete(const QString& configPath);

// ── Build QProcessEnvironment for easytier-core child process ──
QProcessEnvironment buildEnvironment(const QString& networkName,
                                    const QString& networkKey);

} // namespace ElevatedSession
