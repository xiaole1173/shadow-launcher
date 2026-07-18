// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "connection_guard.h"
#include <QDateTime>
#include <QDebug>
#include "../utils/logger.h"

namespace ShadowLauncher {

ConnectionGuard::ConnectionGuard(QObject* parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
{
    m_cleanupTimer->setInterval(60000); // cleanup every 60s
    connect(m_cleanupTimer, &QTimer::timeout, this, &ConnectionGuard::cleanupExpired);
    m_cleanupTimer->start();
}

bool ConnectionGuard::allowConnect(const QString& ip)
{
    bool c;
    if (!checkAndAdd(ip, m_connectLog, kMaxConnPerMinute, 60000, c)) {
        emit blocked(ip, QStringLiteral("连接过于频繁，已被暂时限制"));
        return false;
    }
    return true;
}

bool ConnectionGuard::allowJoinRoom(const QString& ip)
{
    bool c;
    if (!checkAndAdd(ip, m_joinLog, kMaxJoinPerMinute, 60000, c,
                     kJoinCooldown)) {
        if (c)
            emit blocked(ip, QStringLiteral("操作过于频繁，请 %1 秒后再试").arg(kJoinCooldownSec));
        else
            emit blocked(ip, QStringLiteral("操作过于频繁，已被暂时限制"));
        return false;
    }
    return true;
}

bool ConnectionGuard::allowPacket(const QString& peerId)
{
    bool c;
    if (!checkAndAdd(peerId, m_packetLog, kMaxPacketsPerSec, 1000, c)) {
        // Don't spam toast for packet drops — just silently drop
        return false;
    }
    return true;
}

bool ConnectionGuard::checkAndAdd(const QString& key,
                                   QHash<QString, RateEntry>& log,
                                   int maxCount, qint64 windowMs,
                                   bool& hitCooldown,
                                   int cooldownThreshold)
{
    hitCooldown = false;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto& entry = log[key];
    qint64 cutoff = now - windowMs;

    // Remove expired entries
    while (!entry.times.isEmpty() && entry.times.first() < cutoff)
        entry.times.removeFirst();

    // Cooldown check: if we've had N+ failures, enforce cooldown
    if (cooldownThreshold > 0 && entry.times.size() >= cooldownThreshold) {
        qint64 lastAttempt = entry.times.last();
        qint64 cooldownUntil = lastAttempt + kJoinCooldownSec * 1000;
        if (now < cooldownUntil) {
            hitCooldown = true;
            return false;
        }
        // Cooldown expired, clear and retry
        entry.times.clear();
    }

    if (entry.times.size() >= maxCount) {
        return false;
    }

    entry.times.append(now);
    return true;
}

void ConnectionGuard::cleanupExpired()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    auto cleanup = [&](QHash<QString, RateEntry>& log) {
        QStringList expired;
        for (auto it = log.begin(); it != log.end(); ++it) {
            while (!it->times.isEmpty() && it->times.first() < now - 120000)
                it->times.removeFirst();
            if (it->times.isEmpty())
                expired.append(it.key());
        }
        for (const auto& k : expired)
            log.remove(k);
    };

    cleanup(m_connectLog);
    cleanup(m_joinLog);
    cleanup(m_packetLog);
}

} // namespace
