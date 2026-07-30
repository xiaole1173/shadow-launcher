// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Minecraft LAN Scanner — listens on 224.0.2.60:4445 for [MOTD]...[/MOTD][AD]port[/AD]
// Aligns with Terracotta scanning.rs: multicast discovery with MOTD filter + 5s expiry
#pragma once
#include <QObject>
#include <QList>
#include <QUdpSocket>
#include <QTimer>
#include <QHash>
#include <functional>

namespace ShadowLauncher {

// Scan result: a LAN-discovered Minecraft server
struct McScanResult {
    quint16 port = 0;
    QString motd;
    QString hostAddress;  // source IP of the broadcast
    qint64 lastSeenMs = 0;  // timestamp of last received beacon
};

class McScanner : public QObject {
    Q_OBJECT
public:
    explicit McScanner(QObject* parent = nullptr);
    ~McScanner() override;

    // Start scanning. motdFilter returns true if we should include a server with this MOTD.
    // Typically: [](const QString& m) { return m != MOTD; } to exclude self.
    void start(std::function<bool(const QString&)> motdFilter);
    void stop();

    // Get current list of discovered ports (non-expired, within 5s window)
    QList<quint16> ports() const;

    // Get full scan results (port + motd + host)
    QList<McScanResult> results() const;

signals:
    // Emitted when the discovered server list changes (port added or expired)
    void serversChanged();

private slots:
    void onDatagramReceived();
    void onCleanupTick();

private:
    void processDatagram(const QByteArray& data, const QString& sourceIp);
    void cleanupExpired();

    QUdpSocket* m_socketV4 = nullptr;         // IPv4 multicast listener
    std::function<bool(const QString&)> m_motdFilter;

    // Discovered servers keyed by port
    QHash<quint16, McScanResult> m_servers;
    QTimer* m_cleanupTimer = nullptr;

    static constexpr qint64 kExpiryMs = 5000;  // 5s expiry (aligns with Terracotta)
    static constexpr quint16 kMulticastPort = 4445;
    static const QString kMcMulticastAddrV4;
};

} // namespace ShadowLauncher
