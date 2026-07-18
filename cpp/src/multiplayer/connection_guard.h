// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Connection Guard — rate limiting, brute-force protection
#pragma once
#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QTimer>
#include <qmath.h>

namespace ShadowLauncher {

class ConnectionGuard : public QObject {
    Q_OBJECT
public:
    explicit ConnectionGuard(QObject* parent = nullptr);

    // Returns true if connection is allowed
    bool allowConnect(const QString& ip);
    bool allowJoinRoom(const QString& ip);
    bool allowPacket(const QString& peerId);

    void cleanupExpired();

signals:
    void blocked(const QString& ip, const QString& reason);

private:
    struct RateEntry { QList<qint64> times; };
    QHash<QString, RateEntry> m_connectLog;   // IP → timestamps
    QHash<QString, RateEntry> m_joinLog;      // IP → timestamps
    QHash<QString, RateEntry> m_packetLog;    // peerId → timestamps

    // Limits
    static constexpr int kMaxConnPerMinute = 20;     // TCP accept rate
    static constexpr int kMaxJoinPerMinute = 6;      // Room code attempts
    static constexpr int kJoinCooldown = 3;          // Failures before cooldown
    static constexpr int kJoinCooldownSec = 60;      // Cooldown duration
    static constexpr int kMaxPacketsPerSec = 100;    // Per-client packet rate

    QTimer* m_cleanupTimer;
    bool checkAndAdd(const QString& key, QHash<QString, RateEntry>& log,
                     int maxCount, qint64 windowMs, bool& hitCooldown, int cooldownThreshold = -1);
};

} // namespace
