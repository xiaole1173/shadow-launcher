// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Multiplayer Manager — orchestrates EasyTier + Scaffolding protocol
#pragma once
#include <QObject>
#include <QVariantList>
#include <QTcpSocket>
#include <QTcpServer>
#include <QProcess>
#include <QTimer>
#include <QJsonArray>
#include <memory>
#include "room_code.h"
#include "easytier_process.h"
#include "scaffolding_protocol.h"
#include "connection_guard.h"

namespace ShadowLauncher {

class MultiplayerManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString roomCode READ roomCode NOTIFY roomCodeChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY stateTextChanged)
    Q_PROPERTY(int maxPlayers READ maxPlayers CONSTANT)
    Q_PROPERTY(QVariantList players READ players NOTIFY playersChanged)
    Q_PROPERTY(Role role READ role NOTIFY roleChanged)

public:
    enum State {
        Idle,
        CreatingRoom,
        JoiningNetwork,
        Discovering,
        Connecting,
        Connected,
        WaitingForGuests,
        Error
    };
    Q_ENUM(State)

    enum Role { None, Host, Guest };
    Q_ENUM(Role)

    static constexpr int kMaxPlayers = 5;  // Including host

    explicit MultiplayerManager(QObject* parent = nullptr);
    ~MultiplayerManager() override;

    QString roomCode() const { return m_roomCode; }
    State state() const { return m_state; }
    QString stateText() const { return m_stateText; }
    int maxPlayers() const { return kMaxPlayers; }
    QVariantList players() const { return m_players; }
    Role role() const { return m_role; }
    QString playerName() const { return m_playerName; }

    // ── QML-callable ──
    Q_INVOKABLE static QString playerHeadPath(const QString& name, const QString& dataDir);
    Q_INVOKABLE void createRoom();
    Q_INVOKABLE void joinRoom(const QString& code);
    Q_INVOKABLE void leaveRoom();
    Q_INVOKABLE void copyRoomCode();
    Q_INVOKABLE void prepareServerProperties(const QString& gameDir, const QString& versionId);
    Q_INVOKABLE void setPlayerName(const QString& name);

    Q_PROPERTY(QString playerName READ playerName NOTIFY playerNameChanged)

signals:
    void roomCodeChanged();
    void stateChanged();
    void stateTextChanged();
    void playersChanged();
    void roleChanged();
    void minecraftPortReady(int port);
    void errorOccurred(const QString& msg);
    void playerNameChanged();

private slots:
    void onNetworkReady(const QString& virtualIp);
    void onEasyTierError(const QString& msg);

    // ── Client (guest) mode ──
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError err);
    void onSocketReadyRead();

    // ── Server (host) mode ──
    void onNewConnection();
    void onGuestSocketReadyRead();
    void onGuestDisconnected();

    // ── Discovery + Heartbeat ──
    void sendHeartbeat();
    void sendPing();
    void doDiscoverCenter();
    void onPeerListReady();
    void onDiscoverTimeout();
    void onIdleTimeout();

private:
    void setState(State s, const QString& text);
    void setRole(Role r);
    void startEasyTier(const QString& networkName, const QString& networkKey,
                       const QString& hostname = QString());
    void connectToCenter(const QString& virtualIp, quint16 port);
    void startHostServer();

    // Protocol packet processing (shared by host/guest)
    void processPacket(const QByteArray& data, QTcpSocket* socket);
    void handlePing(const QByteArray& body, QTcpSocket* socket);
    void handleProtocols(const QByteArray& body, QTcpSocket* socket);
    void handleServerPort(const QByteArray& body, QTcpSocket* socket);
    void handlePlayerPing(const QByteArray& body, QTcpSocket* socket);
    void handlePlayerPong(const QByteArray& body, QTcpSocket* socket);
    void handlePlayerProfilesList(const QByteArray& body, QTcpSocket* socket);
    void handlePlayerProfilesResponse(const QByteArray& body);

    // ── Guest protocol negotiation flow ──
    void handleGuestProtocolsResponse(const QByteArray& body);
    void requestServerPort();
    void handleGuestServerPort(const QByteArray& body);

    void broadcastPlayers();

    EasyTierProcess* m_easyTier = nullptr;
    ConnectionGuard* m_guard = nullptr;

    // Latency measurement
    QTimer* m_pingTimer = nullptr;
    QHash<QString, qint64> m_pingSentTimes;    // machineId → send timestamp ms
    QHash<QString, int> m_latency;             // machineId → ms
    QHash<QString, qint64> m_lastHeartbeat;   // machineId → last heartbeat ms
    QTimer* m_heartbeatWatchdog = nullptr;     // detects dead guests
    QTcpServer* m_server = nullptr;       // host mode
    QList<QTcpSocket*> m_guests;          // host mode: connected guest sockets
    QMap<QTcpSocket*, QByteArray> m_guestBuffers;
    QMap<QTcpSocket*, QString> m_guestIds;
    QMap<QTcpSocket*, QString> m_guestIps;

    QTcpSocket* m_socket = nullptr;       // guest mode
    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_discoverTimer = nullptr;
    QTimer* m_discoverTimeoutTimer = nullptr;  // 60s discovery timeout
    QTimer* m_idleTimer = nullptr;             // 5min idle timeout (host only)
    QProcess* m_peerQuery = nullptr;      // easyTier peer list query

    State m_state = Idle;
    Role m_role = None;
    QString m_stateText;
    QString m_roomCode;
    QString m_networkName;
    QString m_networkKey;
    QString m_centerIp;
    quint16 m_centerPort = 0;
    quint16 m_mcPort = 0;
    QString m_machineId;
    QString m_playerName;
    QVariantList m_players;
    QStringList m_supportedProtocols;
    QStringList m_centerProtocols;
    QByteArray m_readBuffer;
};

} // namespace ShadowLauncher
