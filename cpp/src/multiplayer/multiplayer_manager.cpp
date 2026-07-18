// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "multiplayer_manager.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDataStream>
#include <QIODevice>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QRandomGenerator>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDebug>
#include "../utils/logger.h"

namespace ShadowLauncher {

MultiplayerManager::MultiplayerManager(QObject* parent)
    : QObject(parent)
    , m_easyTier(new EasyTierProcess(this))
    , m_guard(new ConnectionGuard(this))
    , m_playerName(QStringLiteral("Steve"))
    , m_pingTimer(new QTimer(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_discoverTimer(new QTimer(this))
    , m_idleTimer(new QTimer(this))
{
    connect(m_guard, &ConnectionGuard::blocked, this, [this](const QString& ip, const QString& reason) {
        qCWarning(logNet) << QStringLiteral("[联机] 连接被阻断 ip=%1 原因=%2").arg(ip, reason);
        emit errorOccurred(reason);
    });

    m_pingTimer->setInterval(3000);
    connect(m_pingTimer, &QTimer::timeout, this, &MultiplayerManager::sendPing);
    QByteArray machineSeed = QSysInfo::machineUniqueId();
    if (machineSeed.isEmpty())
        machineSeed = QSysInfo::productType().toUtf8() + QSysInfo::productVersion().toUtf8();
    m_machineId = QString::fromLatin1(
        QCryptographicHash::hash(machineSeed + "scaffolding-mc", QCryptographicHash::Sha256)
            .toHex()
            .left(32)
    );

    m_supportedProtocols = Scaffolding::basicProtocols();
    m_supportedProtocols << Scaffolding::kPlayerEasyTierId;

    connect(m_easyTier, &EasyTierProcess::networkReady, this, &MultiplayerManager::onNetworkReady);
    connect(m_easyTier, &EasyTierProcess::errorOccurred, this, &MultiplayerManager::onEasyTierError);

    m_heartbeatTimer->setInterval(Scaffolding::kHeartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MultiplayerManager::sendHeartbeat);

    // Heartbeat watchdog: remove guests with no heartbeat for 15s
    m_heartbeatWatchdog = new QTimer(this);
    m_heartbeatWatchdog->setInterval(5000);
    connect(m_heartbeatWatchdog, &QTimer::timeout, this, [this]() {
        if (m_role != Host) return;
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (int i = m_players.size() - 1; i >= 0; --i) {
            QVariantMap p = m_players[i].toMap();
            if (p["kind"].toString() != QStringLiteral("GUEST")) continue;
            QString mid = p["machine_id"].toString();
            qint64 lastHb = m_lastHeartbeat.value(mid, 0);
            if (lastHb > 0 && (now - lastHb) > 15000) {
                qCWarning(logNet) << QStringLiteral("[联机] 宾客心跳超时 id=%1").arg(mid);
                m_players.removeAt(i);
                m_lastHeartbeat.remove(mid);
                m_latency.remove(mid);
                emit playersChanged();
                if (m_players.size() <= 1)
                    setState(WaitingForGuests, QStringLiteral("等待玩家加入..."));
                broadcastPlayers();
            }
        }
    });
    m_heartbeatWatchdog->start();

    // Host idle timeout: auto-close room after 5min with no guests
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(5 * 60 * 1000);
    connect(m_idleTimer, &QTimer::timeout, this, &MultiplayerManager::onIdleTimeout);
}

MultiplayerManager::~MultiplayerManager()
{
    leaveRoom();
}

// ─────────────────────────────────────────
// QML Interface
// ─────────────────────────────────────────

void MultiplayerManager::createRoom()
{
    if (m_state != Idle) {
        emit errorOccurred(QStringLiteral("请先退出当前房间"));
        return;
    }

    setRole(Host);

    auto parts = RoomCode::generate();
    m_roomCode = parts.displayCode;
    m_networkName = parts.networkName;
    m_networkKey = parts.networkKey;
    emit roomCodeChanged();

    QByteArray portSeed = m_networkKey.toUtf8();
    quint32 portHash = qChecksum(portSeed);
    m_mcPort = 1025 + (portHash % (65535 - 1025));
    m_centerPort = m_mcPort;

    qCInfo(logNet) << QStringLiteral("[联机] 创建房间 房间码=%1 MC端口=%2").arg(m_roomCode).arg(m_mcPort);

    QString hostname = Scaffolding::kCenterHostnamePrefix + QString::number(m_mcPort);
    setState(CreatingRoom, QStringLiteral("正在创建房间..."));
    startEasyTier(m_networkName, m_networkKey, hostname);
}

void MultiplayerManager::joinRoom(const QString& code)
{
    if (m_state != Idle) {
        emit errorOccurred(QStringLiteral("请先退出当前房间"));
        return;
    }

    auto parts = RoomCode::parse(code);
    if (!parts) {
        // Track failed join attempts (brute-force protection)
        if (m_guard && !m_guard->allowJoinRoom(m_machineId)) {
            emit errorOccurred(QStringLiteral("操作过于频繁，请稍后再试"));
            return;
        }
        emit errorOccurred(QStringLiteral("房间码无效，请检查后重试"));
        return;
    }

    setRole(Guest);
    m_roomCode = parts->displayCode;
    m_networkName = parts->networkName;
    m_networkKey = parts->networkKey;
    emit roomCodeChanged();

    qCInfo(logNet) << QStringLiteral("[联机] 加入房间 房间码=%1").arg(m_roomCode);

    setState(JoiningNetwork, QStringLiteral("正在加入联机网络..."));
    startEasyTier(m_networkName, m_networkKey);
}

void MultiplayerManager::leaveRoom()
{
    qCInfo(logNet) << QStringLiteral("[联机] 离开房间");
    m_heartbeatTimer->stop();
    m_discoverTimer->stop();
    if (m_discoverTimeoutTimer)
        m_discoverTimeoutTimer->stop();
    m_idleTimer->stop();

    m_roomCode.clear();
    m_centerIp.clear();
    m_centerPort = 0;
    m_players.clear();
    m_centerProtocols.clear();
    m_readBuffer.clear();

    setState(Idle, QString());
    setRole(None);
    emit roomCodeChanged();
    emit playersChanged();

    // Defer heavy cleanup to avoid UI freeze
    QTimer::singleShot(0, this, [this]() {
        for (auto* sock : m_guests) {
            sock->disconnect();
            sock->deleteLater();
        }
        m_guests.clear();
        m_guestBuffers.clear();
        m_guestIds.clear();

        if (m_server) {
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
        }

        if (m_socket) {
            m_socket->disconnect();
            m_socket->deleteLater();
            m_socket = nullptr;
        }

        if (m_peerQuery) {
            m_peerQuery->kill();
            m_peerQuery->deleteLater();
            m_peerQuery = nullptr;
        }

        m_easyTier->stop();
    });
}

void MultiplayerManager::copyRoomCode()
{
    if (!m_roomCode.isEmpty()) {
        QGuiApplication::clipboard()->setText(m_roomCode);
    }
}

void MultiplayerManager::setPlayerName(const QString& name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed == m_playerName) return;
    m_playerName = trimmed;
    emit playerNameChanged();
    qCInfo(logNet) << QStringLiteral("[联机] 玩家名设置 name=%1").arg(m_playerName);
}

void MultiplayerManager::prepareServerProperties(const QString& gameDir, const QString& versionId)
{
    if (m_state == Idle) return; // Not in a room

    // Extract major version for pre-1.16 check
    QStringList parts = versionId.split('.');
    int major = parts.size() > 1 ? parts.at(1).toInt() : 0;

    if (versionId.startsWith('1') && major > 0 && major < 16) {
        qDebug() << "[Multiplayer] Pre-1.16 server: online-mode is bound to client login state.";
        qDebug() << "[Multiplayer] Host must use offline login for offline guests to join.";
        // server.properties is ignored — no file change needed
        // offline-mode guest can join ONLY if host is also offline
        return;
    }

    QString path = gameDir + QStringLiteral("/server.properties");
    QFile file(path);

    QString content;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
        file.close();
    }

    // Ensure online-mode=false (1.16+ reads server.properties)
    static QRegularExpression reOnline(
        QStringLiteral(R"(^\s*online-mode\s*=\s*\S+)"),
        QRegularExpression::MultilineOption
    );
    if (content.contains(reOnline))
        content.replace(reOnline, QStringLiteral("online-mode=false"));
    else
        content += QStringLiteral("\nonline-mode=false\n");

    // Ensure enforce-secure-profile=false (1.19+)
    static QRegularExpression reSecure(
        QStringLiteral(R"(^\s*enforce-secure-profile\s*=\s*\S+)"),
        QRegularExpression::MultilineOption
    );
    if (content.contains(reSecure))
        content.replace(reSecure, QStringLiteral("enforce-secure-profile=false"));
    else
        content += QStringLiteral("enforce-secure-profile=false\n");

    QFile out(path);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        out.write(content.toUtf8());
        out.close();
        qCInfo(logNet) << QStringLiteral("[联机] 写入server.properties online-mode=false 路径=%1").arg(path);
    } else {
        qCWarning(logNet) << QStringLiteral("[联机] 无法写入server.properties 路径=%1").arg(path);
    }
}

// ─────────────────────────────────────────
// Internal
// ─────────────────────────────────────────

void MultiplayerManager::startEasyTier(const QString& networkName, const QString& networkKey,
                                       const QString& hostname)
{
    m_easyTier->start(networkName, networkKey, hostname);
}

void MultiplayerManager::setState(State s, const QString& text)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged();
    }
    if (m_stateText != text) {
        m_stateText = text;
        emit stateTextChanged();
    }
}

void MultiplayerManager::setRole(Role r)
{
    if (m_role != r) {
        m_role = r;
        emit roleChanged();
    }
}

// ─────────────────────────────────────────
// EasyTier callbacks
// ─────────────────────────────────────────

void MultiplayerManager::onNetworkReady(const QString& virtualIp)
{
    qCInfo(logNet) << QStringLiteral("[联机] EasyTier就绪 virtual_ip=%1").arg(virtualIp);
    m_centerIp = virtualIp;

    if (m_role == Host) {
        // Start TCP server for scaffolding protocol
        startHostServer();

    } else {
        // Discover the center
        setState(Discovering, QStringLiteral("正在查找联机中心..."));
        m_discoverTimer->setInterval(2000);
        connect(m_discoverTimer, &QTimer::timeout, this, &MultiplayerManager::doDiscoverCenter);
        m_discoverTimer->start();

        // 30s discovery timeout per protocol spec
        if (!m_discoverTimeoutTimer) {
            m_discoverTimeoutTimer = new QTimer(this);
            m_discoverTimeoutTimer->setSingleShot(true);
            connect(m_discoverTimeoutTimer, &QTimer::timeout, this, &MultiplayerManager::onDiscoverTimeout);
        }
        m_discoverTimeoutTimer->start(60000);

        doDiscoverCenter();
    }
}

void MultiplayerManager::onEasyTierError(const QString& msg)
{
    m_discoverTimer->stop();
    if (m_discoverTimeoutTimer)
        m_discoverTimeoutTimer->stop();
    setState(Error, msg);
    emit errorOccurred(msg);
}

// ─────────────────────────────────────────
// Host: Start TCP server
// ─────────────────────────────────────────

void MultiplayerManager::startHostServer()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &MultiplayerManager::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, m_centerPort)) {
        emit errorOccurred(QStringLiteral("无法启动联机服务: %1").arg(m_server->errorString()));
        return;
    }

    setState(WaitingForGuests, QStringLiteral("等待玩家加入..."));
    m_idleTimer->start();
    emit minecraftPortReady(static_cast<int>(m_mcPort));

    // Host adds self to player list
    QVariantMap hostPlayer;
    hostPlayer["name"] = m_playerName;
    hostPlayer["machine_id"] = m_machineId;
    hostPlayer["hostname"] = QSysInfo::machineHostName();
    hostPlayer["kind"] = QStringLiteral("HOST");
    hostPlayer["ip"] = m_easyTier ? m_easyTier->virtualIp() : QString();
    hostPlayer["latency"] = 0;
    m_players << hostPlayer;
    emit playersChanged();

    qCInfo(logNet) << QStringLiteral("[联机] 主机服务已启动 联机端口=%1 MC端口=%2").arg(m_centerPort).arg(m_mcPort);
}

void MultiplayerManager::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* sock = m_server->nextPendingConnection();
        QString peerIp = sock->peerAddress().toString();

        // Rate limit
        if (m_guard && !m_guard->allowConnect(peerIp)) {
            qCWarning(logNet) << QStringLiteral("[联机] 频率限制命中 ip=%1").arg(peerIp);
            auto packet = Scaffolding::buildPacket(
                QStringLiteral("error"),
                QByteArrayLiteral("rate_limited")
            );
            sock->write(packet);
            sock->disconnectFromHost();
            sock->deleteLater();
            continue;
        }

        // Check capacity (host + guests ≤ kMaxPlayers)
        int currentCount = 1 + m_guests.size(); // 1 = host
        if (currentCount >= kMaxPlayers) {
            qCInfo(logNet) << QStringLiteral("[联机] 房间已满 拒绝连接");
            auto packet = Scaffolding::buildPacket(
                QStringLiteral("error"),
                QByteArrayLiteral("room_full")
            );
            sock->write(packet);
            sock->disconnectFromHost();
            sock->deleteLater();
            emit errorOccurred(QStringLiteral("房间已满（%1/%1）").arg(kMaxPlayers));
            continue;
        }

        bool wasEmpty = m_guests.isEmpty();
        m_guests.append(sock);
        m_guestBuffers[sock] = QByteArray();
        m_guestIps[sock] = peerIp;
        if (wasEmpty)
            m_idleTimer->stop();

        connect(sock, &QTcpSocket::readyRead, this, &MultiplayerManager::onGuestSocketReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &MultiplayerManager::onGuestDisconnected);

        qCInfo(logNet) << QStringLiteral("[联机] 新宾客连接 ip=%1").arg(sock->peerAddress().toString());
    }
}

void MultiplayerManager::onGuestSocketReadyRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    m_guestBuffers[sock] += sock->readAll();

    // Parse complete packets
    QByteArray& buf = m_guestBuffers[sock];
    while (buf.size() >= 5) {
        quint8 typeLen = static_cast<quint8>(buf[0]);
        if (buf.size() < 1 + typeLen + 4)
            break;

        quint32 bodyLen;
        QDataStream ds(buf.mid(1 + typeLen, 4));
        ds.setByteOrder(QDataStream::BigEndian);
        ds >> bodyLen;

        int packetTotal = 1 + typeLen + 4 + static_cast<int>(bodyLen);
        if (buf.size() < packetTotal)
            break;

        QByteArray packet = buf.left(packetTotal);
        buf.remove(0, packetTotal);
        processPacket(packet, sock);
    }
}

void MultiplayerManager::onGuestDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    QString mid = m_guestIds.value(sock);
    m_guests.removeAll(sock);
    m_guestBuffers.remove(sock);
    m_guestIds.remove(sock);
    m_guestIps.remove(sock);
    sock->deleteLater();
    qCInfo(logNet) << QStringLiteral("[联机] 宾客离开 id=%1").arg(mid);

    // Remove guest from player list
    for (int i = m_players.size() - 1; i >= 0; --i) {
        QVariantMap p = m_players[i].toMap();
        if (p["machine_id"].toString() == mid) {
            m_players.removeAt(i);
            break;
        }
    }
    emit playersChanged();

    // Update state text back if no guests left
    if (m_role == Host && m_players.size() <= 1) {
        setState(WaitingForGuests, QStringLiteral("等待玩家加入..."));
        m_idleTimer->start();
    }

    // Re-broadcast player list
    broadcastPlayers();
}

// ─────────────────────────────────────────
// Center Discovery (guest mode)
// ─────────────────────────────────────────

void MultiplayerManager::doDiscoverCenter()
{
    if (m_role != Guest) return;

    // Try to query EasyTier peer list via CLI
    if (!m_peerQuery) {
        m_peerQuery = new QProcess(this);
        connect(m_peerQuery, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &MultiplayerManager::onPeerListReady);
    }

    if (m_peerQuery->state() != QProcess::Running) {
        // Find easytier-cli.exe
        QString cliDir = QCoreApplication::applicationDirPath();
        QStringList cliPaths = {
            cliDir + "/bin/easytier-cli.exe",
            cliDir + "/../../bin/easytier-cli.exe",
            cliDir + "/easytier-cli.exe",
        };
        QString cliPath;
        for (const auto& p : cliPaths) {
            if (QFileInfo::exists(p)) {
                cliPath = p;
                break;
            }
        }

        if (!QFileInfo::exists(cliPath)) {
            return;
        }

        m_peerQuery->start(cliPath, {"peer"});
    }
}

void MultiplayerManager::onPeerListReady()
{
    if (!m_peerQuery) return;

    QString output = QString::fromUtf8(m_peerQuery->readAllStandardOutput());
    output += QString::fromUtf8(m_peerQuery->readAllStandardError());

    // Extract scaffolding-mc-server-{port} AND its IP from CLI table output
    // Format: | 10.126.126.1/24 | scaffolding-mc-server-56654 | Local | ...
    QRegularExpression hostRx(
        QStringLiteral(R"(\|?\s*(\d+\.\d+\.\d+\.\d+)(?:/\d+)?\s*\|[^|]*?)") +
        QRegularExpression::escape(Scaffolding::kCenterHostnamePrefix) +
        QStringLiteral(R"((\d+))")
    );

    auto m = hostRx.match(output);
    if (m.hasMatch()) {
        QString hostIp = m.captured(1);       // host's virtual IP from CLI table
        quint16 port = m.captured(2).toUShort();
        if (port > 1024 && port <= 65535) {
            m_centerPort = port;
            m_centerIp = hostIp;              // use host's actual IP, not our own
            m_discoverTimer->stop();
            m_discoverTimeoutTimer->stop();
            qCInfo(logNet) << QStringLiteral("[联机] 发现中心 ip=%1 端口=%2").arg(hostIp).arg(port);
            connectToCenter(hostIp, port);
            return;
        }
    }

    // Fallback: discovery failed — log and retry (don't guess IP)
    if (!m_centerIp.isEmpty() && m_centerPort == 0) {
        qCInfo(logNet) << QStringLiteral("[联机] Peer列表为空 DHT中尚无主机 重试中");
    }
}

void MultiplayerManager::onDiscoverTimeout()
{
    qCWarning(logNet) << QStringLiteral("[联机] 发现超时 30秒");
    m_discoverTimer->stop();
    m_discoverTimeoutTimer->stop();
    m_easyTier->stop();

    setState(Idle, QString());
    m_centerIp.clear();
    m_centerPort = 0;
    m_roomCode.clear();
    emit roomCodeChanged();
    emit errorOccurred(QStringLiteral("未找到联机中心，请确认房主已创建房间"));
}

void MultiplayerManager::onIdleTimeout()
{
    qCInfo(logNet) << QStringLiteral("[联机] 房间空闲超时 5分钟无宾客 关闭");
    emit errorOccurred(QStringLiteral("房间已空闲超时，自动关闭"));
    leaveRoom();
}

void MultiplayerManager::connectToCenter(const QString& virtualIp, quint16 port)
{
    if (m_socket) {
        m_socket->disconnect();
        m_socket->deleteLater();
    }

    qCInfo(logNet) << QStringLiteral("[联机] 连接中心 ip=%1 端口=%2").arg(virtualIp).arg(port);
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &MultiplayerManager::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MultiplayerManager::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MultiplayerManager::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead, this, &MultiplayerManager::onSocketReadyRead);

    setState(Connecting, QStringLiteral("正在连接到 %1:%2").arg(virtualIp).arg(port));
    m_socket->connectToHost(virtualIp, port);
}

// ─────────────────────────────────────────
// Guest: TCP Socket callbacks
// ─────────────────────────────────────────

void MultiplayerManager::onSocketConnected()
{
    qCInfo(logNet) << QStringLiteral("[联机] 宾客TCP已连接");
    m_discoverTimer->stop();
    m_discoverTimeoutTimer->stop();
    setState(Connected, QStringLiteral("已连接"));

    m_heartbeatTimer->start();
    sendHeartbeat();

    auto packet = Scaffolding::buildPacket(
        Scaffolding::kProtocols,
        Scaffolding::packProtocolList(m_supportedProtocols).toUtf8()
    );
    m_socket->write(packet);
}

void MultiplayerManager::onSocketDisconnected()
{
    qCInfo(logNet) << QStringLiteral("[联机] 宾客TCP已断开");
    m_heartbeatTimer->stop();
    if (m_state != Idle)
        setState(Error, QStringLiteral("与联机中心的连接已断开"));
}

void MultiplayerManager::onSocketError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err);
    if (m_role == Guest && m_state == Connecting) {
        return; // Discovery still running
    }
    emit errorOccurred(QStringLiteral("网络错误: %1").arg(
        m_socket ? m_socket->errorString() : QStringLiteral("unknown")));
}

void MultiplayerManager::onSocketReadyRead()
{
    if (!m_socket) return;
    m_readBuffer += m_socket->readAll();

    while (m_readBuffer.size() >= 5) {
        quint8 typeLen = static_cast<quint8>(m_readBuffer[0]);
        if (m_readBuffer.size() < 1 + typeLen + 4)
            break;

        quint32 bodyLen;
        QDataStream ds(m_readBuffer.mid(1 + typeLen, 4));
        ds.setByteOrder(QDataStream::BigEndian);
        ds >> bodyLen;

        int packetTotal = 1 + typeLen + 4 + static_cast<int>(bodyLen);
        if (m_readBuffer.size() < packetTotal)
            break;

        QByteArray packet = m_readBuffer.left(packetTotal);
        m_readBuffer.remove(0, packetTotal);
        processPacket(packet, m_socket);
    }
}

// ─────────────────────────────────────────
// Shared: Protocol packet processing
// ─────────────────────────────────────────

void MultiplayerManager::processPacket(const QByteArray& data, QTcpSocket* socket)
{
    // Rate limit per-client packets
    QString peerId = socket ? socket->peerAddress().toString() : QStringLiteral("local");
    if (m_guard && !m_guard->allowPacket(peerId)) {
        return; // drop silently
    }

    quint8 typeLen = static_cast<quint8>(data[0]);
    QString type = QString::fromUtf8(data.mid(1, typeLen));

    quint32 bodyLen;
    QDataStream ds(data.mid(1 + typeLen, 4));
    ds.setByteOrder(QDataStream::BigEndian);
    ds >> bodyLen;

    QByteArray body = data.mid(5 + typeLen, static_cast<int>(bodyLen));

    if (type == Scaffolding::kPing) {
        if (m_role == Host)
            handlePing(body, socket);
        else
            ;  // guest gets ping response, already logged
    } else if (type == Scaffolding::kProtocols) {
        if (m_role == Host)
            handleProtocols(body, socket);
        else
            handleGuestProtocolsResponse(body);
    } else if (type == Scaffolding::kServerPort) {
        if (m_role == Host)
            handleServerPort(body, socket);
        else
            handleGuestServerPort(body);
    } else if (type == Scaffolding::kPlayerPing) {
        if (m_role == Host)
            handlePlayerPing(body, socket);
        else {
            // Guest: echo back as pong for RTT measurement
            QByteArray pongBody = body;
            auto packet = Scaffolding::buildPacket(Scaffolding::kPlayerPong, pongBody);
            m_socket->write(packet);
        }
    } else if (type == Scaffolding::kPlayerPong) {
        if (m_role == Host)
            handlePlayerPong(body, socket);
    } else if (type == Scaffolding::kPlayerProfilesList) {
        if (m_role == Host)
            handlePlayerProfilesList(body, socket);
        else
            handlePlayerProfilesResponse(body);
    }
}

// ─────────────────────────────────────────
// Host protocol handlers
// ─────────────────────────────────────────

void MultiplayerManager::handlePing(const QByteArray& body, QTcpSocket* socket)
{
    // Echo back the same body
    auto packet = Scaffolding::buildPacket(Scaffolding::kPing, body);
    socket->write(packet);
}

void MultiplayerManager::handleProtocols(const QByteArray& body, QTcpSocket* socket)
{
    QStringList guestProtocols = Scaffolding::unpackProtocolList(QString::fromUtf8(body));
    qCInfo(logNet) << QStringLiteral("[联机] 宾客协议列表: %1").arg(guestProtocols.join(QStringLiteral(", ")));

    // Return intersection
    QStringList common;
    for (const auto& p : m_supportedProtocols) {
        if (guestProtocols.contains(p))
            common << p;
    }

    auto packet = Scaffolding::buildPacket(
        Scaffolding::kProtocols,
        Scaffolding::packProtocolList(common).toUtf8()
    );
    socket->write(packet);
}

void MultiplayerManager::handleServerPort(const QByteArray& /*body*/, QTcpSocket* socket)
{
    QByteArray portData(2, 0);
    QDataStream ds(&portData, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << m_mcPort;

    auto packet = Scaffolding::buildPacket(Scaffolding::kServerPort, portData);
    socket->write(packet);

    qCInfo(logNet) << QStringLiteral("[联机] 发送服务器端口 port=%1").arg(m_mcPort);
}

// ── Guest: protocol negotiation response ──
void MultiplayerManager::handleGuestProtocolsResponse(const QByteArray& body)
{
    QStringList centerProtocols = Scaffolding::unpackProtocolList(QString::fromUtf8(body));
    qCInfo(logNet) << QStringLiteral("[联机] 中心协议列表: %1").arg(centerProtocols.join(QStringLiteral(", ")));

    // Compute intersection
    m_centerProtocols.clear();
    for (const auto& p : m_supportedProtocols) {
        if (centerProtocols.contains(p))
            m_centerProtocols << p;
    }
    qCInfo(logNet) << QStringLiteral("[联机] 协商协议: %1").arg(m_centerProtocols.join(QStringLiteral(", ")));

    // Step 3: request server port
    requestServerPort();
}

void MultiplayerManager::requestServerPort()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    auto packet = Scaffolding::buildPacket(
        Scaffolding::kServerPort,
        QByteArray()  // empty body per spec
    );
    m_socket->write(packet);
    qCInfo(logNet) << QStringLiteral("[联机] 请求服务器端口");
}

void MultiplayerManager::handleGuestServerPort(const QByteArray& body)
{
    if (body.size() >= 2) {
        QDataStream ds(body);
        ds.setByteOrder(QDataStream::BigEndian);
        quint16 port;
        ds >> port;
        m_mcPort = port;
        qCInfo(logNet) << QStringLiteral("[联机] 收到服务器端口 port=%1").arg(port);
    }
    // spec: 0xFFFF = server not started, ignore for now
}

void MultiplayerManager::handlePlayerPing(const QByteArray& body, QTcpSocket* socket)
{
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();
    QString name = obj[QStringLiteral("name")].toString();
    QString mid = obj[QStringLiteral("machine_id")].toString();

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Register player
    if (!m_guestIds.contains(socket)) {
        m_guestIds[socket] = mid;
        qCInfo(logNet) << QStringLiteral("[联机] 新玩家加入 name=%1 id=%2").arg(name, mid);

        // Use socket peer address for guest IP (TUN gives us the virtual IP directly)
        QString guestIp = socket->peerAddress().toString();
        if (guestIp.startsWith(QLatin1String("::ffff:")))
            guestIp = guestIp.mid(7);
        m_guestIps[socket] = guestIp;

        QVariantMap player;
        player[QStringLiteral("name")] = name;
        player[QStringLiteral("machine_id")] = mid;
        player[QStringLiteral("hostname")] = obj[QStringLiteral("hostname")].toString(name);
        player[QStringLiteral("kind")] = QStringLiteral("GUEST");
        player[QStringLiteral("ip")] = guestIp;
        player[QStringLiteral("latency")] = m_latency.value(mid, -1);
        if (obj.contains(QLatin1String("vendor")))
            player[QStringLiteral("vendor")] = obj[QLatin1String("vendor")].toString();

        m_players << player;
        m_lastHeartbeat[mid] = now;
        emit playersChanged();

        // Update host state text on first guest
        if (m_players.size() > 1 && m_state == WaitingForGuests)
            setState(WaitingForGuests, QStringLiteral("在线"));

        broadcastPlayers();
    } else {
        // Update heartbeat time for existing player
        m_lastHeartbeat[mid] = now;
        for (int i = 0; i < m_players.size(); ++i) {
            QVariantMap p = m_players[i].toMap();
            if (p[QStringLiteral("machine_id")].toString() == mid) {
                p[QStringLiteral("latency")] = m_latency.value(mid, -1);
                // Refresh IP from socket
                QString ip = socket->peerAddress().toString();
                if (ip.startsWith(QLatin1String("::ffff:")))
                    ip = ip.mid(7);
                if (!ip.isEmpty()) p[QStringLiteral("ip")] = ip;
                m_players[i] = p;
                emit playersChanged();
                break;
            }
        }
    }

    // Reply with pong (for guest's own latency measurement)
    QJsonObject pong;
    pong[QStringLiteral("ts")] = static_cast<double>(now);
    pong[QStringLiteral("machine_id")] = mid;
    auto packet = Scaffolding::buildPacket(
        Scaffolding::kPlayerPong,
        QJsonDocument(pong).toJson(QJsonDocument::Compact)
    );
    socket->write(packet);
}

void MultiplayerManager::handlePlayerProfilesList(const QByteArray& /*body*/, QTcpSocket* socket)
{
    QJsonArray arr;
    for (const auto& p : m_players) {
        arr.append(QJsonObject::fromVariantMap(p.toMap()));
    }

    auto packet = Scaffolding::buildPacket(
        Scaffolding::kPlayerProfilesList,
        QJsonDocument(arr).toJson(QJsonDocument::Compact)
    );
    socket->write(packet);
}

// ─────────────────────────────────────────
// Guest: heartbeat
// ─────────────────────────────────────────

void MultiplayerManager::sendHeartbeat()
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    QJsonObject heartbeat;
    heartbeat[QStringLiteral("name")] = m_playerName;
    heartbeat[QStringLiteral("machine_id")] = m_machineId;
    heartbeat[QStringLiteral("hostname")] = QSysInfo::machineHostName();
    heartbeat[QStringLiteral("vendor")] = QStringLiteral("shadow");

    if (m_centerProtocols.contains(Scaffolding::kPlayerEasyTierId)) {
        heartbeat[QStringLiteral("easytier_id")] = QString();
    }

    auto packet = Scaffolding::buildPacket(
        Scaffolding::kPlayerPing,
        QJsonDocument(heartbeat).toJson(QJsonDocument::Compact)
    );
    m_socket->write(packet);
}

void MultiplayerManager::sendPing()
{
    if (m_role != Host || m_state < Connected) return;

    QJsonObject ping;
    ping["ts"] = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
    auto packet = Scaffolding::buildPacket(
        Scaffolding::kPlayerPing,
        QJsonDocument(ping).toJson(QJsonDocument::Compact)
    );
    for (auto* guest : m_guests) {
        if (guest->state() == QAbstractSocket::ConnectedState)
            guest->write(packet);
    }
}

void MultiplayerManager::handlePlayerPong(const QByteArray& body, QTcpSocket* socket)
{
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 sentTs = static_cast<qint64>(obj["ts"].toDouble());
    if (sentTs > 0) {
        int rtt = static_cast<int>(now - sentTs);
        QString mid = m_guestIds.value(socket);
        if (!mid.isEmpty()) {
            m_latency[mid] = rtt;
            for (int i = 0; i < m_players.size(); ++i) {
                QVariantMap p = m_players[i].toMap();
                if (p["machine_id"].toString() == mid) {
                    p["latency"] = rtt;
                    m_players[i] = p;
                    emit playersChanged();
                    break;
                }
            }
        }
        qCInfo(logNet) << QStringLiteral("[联机] 宾客延迟 RTT=%1ms id=%2").arg(rtt).arg(mid);
    }
}

void MultiplayerManager::handlePlayerProfilesResponse(const QByteArray& body)
{
    QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isArray()) {
        m_players.clear();
        for (const auto& v : doc.array()) {
            QVariantMap player = v.toObject().toVariantMap();
            m_players << player;
        }
        emit playersChanged();
    }
}

// ─────────────────────────────────────────
// Broadcast
// ─────────────────────────────────────────

void MultiplayerManager::broadcastPlayers()
{
    for (auto* sock : m_guests) {
        QJsonArray arr;
        for (const auto& p : m_players) {
            arr.append(QJsonObject::fromVariantMap(p.toMap()));
        }

        auto packet = Scaffolding::buildPacket(
            Scaffolding::kPlayerProfilesList,
            QJsonDocument(arr).toJson(QJsonDocument::Compact)
        );
        sock->write(packet);
    }
}

// ── Static: offline player head path (Steve/Alex algorithm) ──
QString MultiplayerManager::playerHeadPath(const QString& name, const QString& dataDir)
{
    QString model = QStringLiteral("steve");
    QString trimmed = name.trimmed();

    if (!trimmed.isEmpty()) {
        // Minecraft Java: OfflinePlayer:name → MD5 → hash LSB determines Steve/Alex
        QByteArray input = QStringLiteral("OfflinePlayer:").toUtf8() + trimmed.toUtf8();
        QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);
        int parity = (hash[3] & 1) ^ (hash[7] & 1) ^ (hash[11] & 1) ^ (hash[15] & 1);
        model = parity ? QStringLiteral("alex") : QStringLiteral("steve");
    }

    QString path = dataDir + QStringLiteral("/assets/skins/") + model + QStringLiteral("_head.png");
    if (QFileInfo::exists(path))
        return QStringLiteral("file:///") + path;
    return QString();
}

} // namespace ShadowLauncher
