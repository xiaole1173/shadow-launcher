// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "multiplayer_manager.h"
#include "mc_scanner.h"
#include "port_request.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
#include <QUdpSocket>
#include <QDateTime>
#include <QDebug>
#include "../utils/logger.h"
#include "elevated_session.h"
#include "relay_crypto.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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
    connect(m_easyTier, &EasyTierProcess::virtualIpChanged, this, [this](const QString& ip) {
        m_centerIp = ip;
        qCInfo(logNet) << QStringLiteral("[联机] 虚拟IP修正 center_ip=%1").arg(ip);
        // Update host player IP in QML player list too
        if (m_role == Host && !m_players.isEmpty()) {
            QVariantMap host = m_players[0].toMap();
            if (host["kind"].toString() == QStringLiteral("HOST")) {
                host["ip"] = ip;
                m_players[0] = host;
                emit playersChanged();
            }
        }
    });
    connect(m_easyTier, &EasyTierProcess::errorOccurred, this, &MultiplayerManager::onEasyTierError);

    m_heartbeatTimer->setInterval(Scaffolding::kHeartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MultiplayerManager::sendHeartbeat);

    // Heartbeat watchdog: remove guests with no heartbeat for 10s (align with Terracotta)
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
            if (lastHb > 0 && (now - lastHb) > 10000) {
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

    // Host MC server health check (align with Terracotta: 5s loop, 3 failures = exception)
    m_mcHealthTimer = new QTimer(this);
    m_mcHealthTimer->setInterval(kMcHealthCheckIntervalMs);
    connect(m_mcHealthTimer, &QTimer::timeout, this, &MultiplayerManager::checkMcServerHealth);

    // Guest profile sync timer (pull from host every 5s, align with Terracotta)
    m_profileSyncTimer = new QTimer(this);
    m_profileSyncTimer->setInterval(5000);
    connect(m_profileSyncTimer, &QTimer::timeout, this, &MultiplayerManager::syncGuestProfiles);

    // MC LAN Scanner: initialize lazily when scanning starts
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

    // MC server port: deterministic from network key
    QByteArray portSeed = m_networkKey.toUtf8();
    quint32 portHash = qChecksum(portSeed);
    m_mcPort = 1025 + (portHash % (65535 - 1025));

    // Scaffold protocol port: separate from MC port (align with Terracotta)
    QByteArray scaffoldHash = QCryptographicHash::hash(portSeed, QCryptographicHash::Sha256);
    m_centerPort = 20000 + (static_cast<quint16>(scaffoldHash[0]) << 8 | scaffoldHash[1]) % 10000;
    if (m_centerPort == m_mcPort)
        m_centerPort++;  // ensure distinct

    QString hostname = Scaffolding::kCenterHostnamePrefix + QString::number(m_centerPort);

    // If not elevated, save state and relaunch elevated
    if (!ElevatedSession::isActive()) {
        QString relayEp = Relay::relayEndpoint();
        QString configPath = ElevatedSession::saveElevationConfig(
            m_networkName, m_networkKey, relayEp,
            hostname, m_roomCode, m_mcPort);

        qCInfo(logNet) << QStringLiteral("[联机] 需提权 配置=%1").arg(configPath);

        QString exePath = QCoreApplication::applicationFilePath();
        QString elevateArgs = QStringLiteral("--elevated --elevate-config \"%1\" --navigate 2")
            .arg(configPath);

        // Preserve dev mode flag if active
        if (QCoreApplication::arguments().contains(QStringLiteral("--dev")))
            elevateArgs += QStringLiteral(" --dev");

#ifdef Q_OS_WIN
        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = reinterpret_cast<const wchar_t*>(exePath.utf16());
        sei.lpParameters = reinterpret_cast<const wchar_t*>(elevateArgs.utf16());
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExW(&sei) && sei.hProcess) {
            CloseHandle(sei.hProcess);
            setState(CreatingRoom, QStringLiteral("正在提权，请等待..."));
            QTimer::singleShot(1500, qApp, &QCoreApplication::quit);
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED) {
                emit errorOccurred(QStringLiteral("提权被取消"));
            } else {
                emit errorOccurred(QStringLiteral("提权失败 (错误码: %1)").arg(err));
            }
            setState(Idle, {});
            m_roomCode.clear();
            emit roomCodeChanged();
        }
#else
        emit errorOccurred(QStringLiteral("提权仅在Windows上支持"));
        setState(Idle, {});
        m_roomCode.clear();
        emit roomCodeChanged();
#endif
        return;
    }

    qCInfo(logNet) << QStringLiteral("[联机] 创建房间(已提权) 房间码=%1 MC端口=%2").arg(m_roomCode).arg(m_mcPort);

    setState(CreatingRoom, QStringLiteral("正在创建房间..."));
    startEasyTier(m_networkName, m_networkKey, hostname);
}

void MultiplayerManager::restoreHostSession(const QString& networkName,
                                                     const QString& networkKey,
                                                     const QString& roomCode,
                                                     quint16 mcPort,
                                                     const QString& hostname)
{
    setRole(Host);
    m_roomCode = roomCode;
    m_networkName = networkName;
    m_networkKey = networkKey;
    m_mcPort = mcPort;
    // Parse scaffolding port from hostname (align with Terracotta: hostname = "scaffolding-mc-server-{port}")
    static QRegularExpression scRx(QStringLiteral(R"(scaffolding-mc-server-(\d+))"));
    auto scMatch = scRx.match(hostname);
    m_centerPort = scMatch.hasMatch() ? static_cast<quint16>(scMatch.captured(1).toUShort()) : mcPort;
    if (m_centerPort == m_mcPort && m_centerPort > 0)
        m_centerPort++;  // ensure distinct like in createRoom
    emit roomCodeChanged();

    qCInfo(logNet) << QStringLiteral("[联机] 恢复主机会话 房间码=%1 MC端口=%2 (提权后)")
        .arg(m_roomCode).arg(m_mcPort);

    setState(CreatingRoom, QStringLiteral("正在创建房间..."));
    startEasyTier(m_networkName, m_networkKey, hostname);
}

void MultiplayerManager::restoreGuestSession(const QString& networkName,
                                              const QString& networkKey,
                                              const QString& roomCode)
{
    setRole(Guest);
    m_roomCode = roomCode;
    m_networkName = networkName;
    m_networkKey = networkKey;
    emit roomCodeChanged();

    qCInfo(logNet) << QStringLiteral("[联机] 恢复宾客会话 房间码=%1 (提权后)")
        .arg(m_roomCode);

    setState(JoiningNetwork, QStringLiteral("正在加入联机网络..."));
    startEasyTier(m_networkName, m_networkKey);
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

    // If not elevated, trigger self-elevation
    if (!ElevatedSession::isActive()) {
        QString relayEp = Relay::relayEndpoint();
        QString configPath = ElevatedSession::saveElevationConfig(
            m_networkName, m_networkKey, relayEp,
            QString(), m_roomCode, 0, QStringLiteral("guest"));

        qCInfo(logNet) << QStringLiteral("[联机] 需提权(加入房间) 配置=%1").arg(configPath);

        QString exePath = QCoreApplication::applicationFilePath();
        QString elevateArgs = QStringLiteral("--elevated --elevate-config \"%1\" --navigate 2")
            .arg(configPath);
        if (QCoreApplication::arguments().contains(QStringLiteral("--dev")))
            elevateArgs += QStringLiteral(" --dev");

#ifdef Q_OS_WIN
        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = reinterpret_cast<const wchar_t*>(exePath.utf16());
        sei.lpParameters = reinterpret_cast<const wchar_t*>(elevateArgs.utf16());
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExW(&sei) && sei.hProcess) {
            CloseHandle(sei.hProcess);
            setState(JoiningNetwork, QStringLiteral("正在提权，请等待..."));
            QTimer::singleShot(1500, qApp, &QCoreApplication::quit);
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED)
                emit errorOccurred(QStringLiteral("提权被取消"));
            else
                emit errorOccurred(QStringLiteral("提权失败 (错误码: %1)").arg(err));
            setState(Idle, {});
            m_roomCode.clear();
            emit roomCodeChanged();
        }
#else
        emit errorOccurred(QStringLiteral("提权仅在Windows上支持"));
        setState(Idle, {});
        m_roomCode.clear();
        emit roomCodeChanged();
#endif
        return;
    }

    qCInfo(logNet) << QStringLiteral("[联机] 加入房间(已提权) 房间码=%1").arg(m_roomCode);

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
    m_mcHealthTimer->stop();
    m_profileSyncTimer->stop();
    m_mcHealthFailures = 0;
    m_connectionDifficulty = DiffUnknown;
    m_fingerprintVerified = false;

    stopScanning();

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

    stopFakeServer();

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

// ── MC LAN Scanning (align with Terracotta scanning.rs) ──

void MultiplayerManager::startScanning()
{
    if (!m_scanner) {
        m_scanner = new McScanner(this);
        connect(m_scanner, &McScanner::serversChanged, this, [this]() {
            emit scanResultsChanged();
        });
    }

    // Filter out our own FakeServer announcements (align with Terracotta's filter: |m| m != MOTD)
    static const QString kSelfMotd = QStringLiteral("\u8054\u673A MC\u670D\u52A1\u5668");  // "联机 MC服务器"
    m_scanner->start([this](const QString& motd) -> bool {
        return motd != kSelfMotd && !motd.trimmed().isEmpty();
    });

    qCInfo(logNet) << QStringLiteral("[联机] MC LAN扫描已启动");
}

void MultiplayerManager::stopScanning()
{
    if (m_scanner) {
        m_scanner->stop();
    }
}

QVariantList MultiplayerManager::scanResults() const
{
    QVariantList results;
    if (!m_scanner) return results;

    for (const auto& r : m_scanner->results()) {
        QVariantMap entry;
        entry[QStringLiteral("port")] = static_cast<int>(r.port);
        entry[QStringLiteral("motd")] = r.motd;
        entry[QStringLiteral("hostAddress")] = r.hostAddress;
        results.append(entry);
    }
    return results;
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

    // Bind only to 127.0.0.1 (not exposed to physical LAN)
    static QRegularExpression reServerIp(
        QStringLiteral(R"(^\s*server-ip\s*=\s*\S+)"),
        QRegularExpression::MultilineOption
    );
    if (content.contains(reServerIp))
        content.replace(reServerIp, QStringLiteral("server-ip=127.0.0.1"));
    else
        content += QStringLiteral("server-ip=127.0.0.1\n");

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
    if (m_role == Host) {
        // Whitelist both scaffold port and MC server port
        m_easyTier->start(networkName, networkKey, hostname,
                          {m_centerPort, m_mcPort});
    } else {
        m_easyTier->start(networkName, networkKey);
    }
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
        // Disconnect first to prevent duplicate signal bindings when onNetworkReady fires multiple times
        m_discoverTimer->disconnect();
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

    // With --no-tun, EasyTier delivers port-forward traffic to 127.0.0.1.
    // Binding only to localhost prevents physical LAN access.
    if (!m_server->listen(QHostAddress(QStringLiteral("127.0.0.1")), m_centerPort)) {
        emit errorOccurred(QStringLiteral("无法启动联机服务: %1").arg(m_server->errorString()));
        return;
    }

    setState(WaitingForGuests, QStringLiteral("等待玩家加入..."));
    m_idleTimer->start();
    m_mcHealthFailures = 0;
    m_mcHealthTimer->start();
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

        m_peerQuery->start(cliPath, {QStringLiteral("peer"), QStringLiteral("-o"), QStringLiteral("json")});
    }
}

void MultiplayerManager::onPeerListReady()
{
    if (!m_peerQuery) return;

    QString output = QString::fromUtf8(m_peerQuery->readAllStandardOutput());

    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
    if (!doc.isArray()) {
        qCInfo(logNet) << QStringLiteral("[联机] Peer列表JSON解析失败 重试中");
        return;
    }

    // Parse all peers for NAT info and calculate connection difficulty (align with Terracotta)
    EasyTierNatType localNat = EasyTierNatType::Unknown;
    EasyTierNatType hostNat = EasyTierNatType::Unknown;
    QString localNatStr;
    QString hostNatStr;

    for (const auto& item : doc.array()) {
        QJsonObject obj = item.toObject();
        QString hostname = obj[QStringLiteral("hostname")].toString();
        QString ipv4 = obj[QStringLiteral("ipv4")].toString();
        QString natTypeStr = obj[QStringLiteral("nat_type")].toString();
        bool isLocal = (obj[QStringLiteral("cost")].toString() == QStringLiteral("Local"));

        // Parse NAT type
        auto parseNat = [](const QString& s) -> EasyTierNatType {
            if (s == QStringLiteral("OpenInternet")) return EasyTierNatType::OpenInternet;
            if (s == QStringLiteral("NoPat"))          return EasyTierNatType::NoPAT;
            if (s == QStringLiteral("FullCone"))        return EasyTierNatType::FullCone;
            if (s == QStringLiteral("Restricted"))      return EasyTierNatType::Restricted;
            if (s == QStringLiteral("PortRestricted"))  return EasyTierNatType::PortRestricted;
            if (s == QStringLiteral("Symmetric"))       return EasyTierNatType::Symmetric;
            if (s == QStringLiteral("SymUdpFirewall"))  return EasyTierNatType::SymmetricUdpWall;
            if (s == QStringLiteral("SymmetricEasyInc"))return EasyTierNatType::SymmetricEasyIncrease;
            if (s == QStringLiteral("SymmetricEasyDec"))return EasyTierNatType::SymmetricEasyDecrease;
            return EasyTierNatType::Unknown;
        };

        if (isLocal) {
            localNat = parseNat(natTypeStr);
            localNatStr = natTypeStr;
        }

        if (hostname.startsWith(Scaffolding::kCenterHostnamePrefix)) {
            if (ipv4.isEmpty()) continue;
            hostNat = parseNat(natTypeStr);
            hostNatStr = natTypeStr;

            bool ok = false;
            QString portStr = hostname.mid(Scaffolding::kCenterHostnamePrefix.length());
            quint16 port = portStr.toUShort(&ok);
            if (!ok || port <= 1024 || port > 65535) continue;

            m_centerPort = port;
            m_centerIp = ipv4;
            m_discoverTimer->stop();
            m_discoverTimeoutTimer->stop();

            // Calculate connection difficulty (align with Terracotta)
            m_connectionDifficulty = calcConnectionDifficulty(localNat, hostNat);
            qCInfo(logNet) << QStringLiteral("[联机] 发现中心 ip=%1 端口=%2 NAT=local:%3/host:%4 难度=%5")
                .arg(ipv4).arg(port)
                .arg(localNatStr).arg(hostNatStr)
                .arg(static_cast<int>(m_connectionDifficulty));
            emit connectionDifficultyChanged();

            // ── Terracotta-style: port-forward → connect via 127.0.0.1 ──
            // Try to keep the same port (Terracotta: request_specific); fallback to free
            quint16 localPort = PortRequest::requestSpecific(port);
            if (localPort == 0) {
                localPort = PortRequest::requestFree(21234);
            }
            if (!m_easyTier->addPortForward(QStringLiteral("127.0.0.1"), localPort,
                                            ipv4, port, QStringLiteral("tcp"))) {
                emit errorOccurred(QStringLiteral("无法创建联机隧道端口"));
                return;
            }

            qCInfo(logNet) << QStringLiteral("[联机] 端口转发已建立 本地端口=%1").arg(localPort);
            connectToCenter(QStringLiteral("127.0.0.1"), localPort);
            return;
        }
    }

    // Fallback: no host found yet — retry
    qCInfo(logNet) << QStringLiteral("[联机] Peer列表中无主机 重试中");
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
    m_fingerprintVerified = false;

    // ── Terracotta-style: verify scaffolding server identity with fingerprint ping ──
    auto fp = Scaffolding::buildPacket(
        Scaffolding::kPing,
        scaffoldingFingerprint()
    );
    m_socket->write(fp);
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
            handleGuestPingResponse(body);
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
    } else {
        qCInfo(logNet) << QStringLiteral("[联机] 未知协议类型: %1").arg(type);
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

// ── Guest: ping response / fingerprint verification (align with Terracotta) ──
void MultiplayerManager::handleGuestPingResponse(const QByteArray& body)
{
    // Verify 16-byte fingerprint echo
    const QByteArray& expected = scaffoldingFingerprint();
    if (body == expected) {
        qCInfo(logNet) << QStringLiteral("[联机] 指纹验证通过");
        m_fingerprintVerified = true;

        // ── Step 2: protocol negotiation ──
        m_heartbeatTimer->start();
        sendHeartbeat();

        auto packet = Scaffolding::buildPacket(
            Scaffolding::kProtocols,
            Scaffolding::packProtocolList(m_supportedProtocols).toUtf8()
        );
        m_socket->write(packet);
    } else if (body.size() == expected.size()) {
        // Fingerprint mismatch — log and continue (non-fatal for backwards compat)
        qCWarning(logNet) << QStringLiteral("[联机] 指纹验证失败 (size正确但内容不匹配)");
        m_fingerprintVerified = false;

        m_heartbeatTimer->start();
        sendHeartbeat();

        auto packet = Scaffolding::buildPacket(
            Scaffolding::kProtocols,
            Scaffolding::packProtocolList(m_supportedProtocols).toUtf8()
        );
        m_socket->write(packet);
    }
    // If body doesn't match at all, ignore (might be a different kind of ping)
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

        // ── Terracotta-style: port request + port-forward for MC connection ──
        // Aligns with Terracotta room.rs:
        //   let local_port = PortRequest::request_specific(port)
        //       .unwrap_or_else(|e| { PortRequest::Minecraft.request() });
        QString hostIp = m_centerIp;
        quint16 localMcPort = PortRequest::requestSpecific(port);
        if (localMcPort == 0) {
            // Requested port is occupied — fall back to any free ephemeral port
            localMcPort = PortRequest::requestFree(21234);
            qCInfo(logNet) << QStringLiteral("[联机] MC端口%1占位 使用动态端口%2").arg(port).arg(localMcPort);
        } else {
            qCInfo(logNet) << QStringLiteral("[联机] MC端口%1可用").arg(port);
        }

        // TCP port-forward
        if (!m_easyTier->addPortForward(QStringLiteral("127.0.0.1"), localMcPort,
                                        hostIp, port, QStringLiteral("tcp"))) {
            qCWarning(logNet) << QStringLiteral("[联机] 无法创建MC TCP端口转发");
        }

        // UDP port-forward for mod compatibility (SimpleVoiceChat etc.)
        // Aligns with Terracotta: forwards both TCP and UDP
        m_easyTier->addPortForward(QStringLiteral("127.0.0.1"), localMcPort,
                                   hostIp, port, QStringLiteral("udp"));

        qCInfo(logNet) << QStringLiteral("[联机] MC端口转发已建立 本地端口=%1 远程端口=%2").arg(localMcPort).arg(port);
        emit minecraftPortReady(static_cast<int>(localMcPort));

        // ── Terracotta-style: verify MC connection before declaring OK ──
        setState(VerifyingConnection, QStringLiteral("正在验证MC连接..."));
        m_mcVerifyRetries = 0;
        verifyMcConnection();
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
    if (!doc.isArray()) return;

    // ── Terracotta-style merge: sort server profiles, merge with local ──
    // Parse server profiles
    struct ServerProfile {
        QString name;
        QString machineId;
        QString vendor;
        QString kind;    // "HOST" or "GUEST"
    };

    QList<ServerProfile> serverProfiles;
    for (const auto& item : doc.array()) {
        QJsonObject obj = item.toObject();
        ServerProfile sp;
        sp.name = obj[QStringLiteral("name")].toString();
        sp.machineId = obj[QStringLiteral("machine_id")].toString();
        sp.vendor = obj[QStringLiteral("vendor")].toString();
        sp.kind = obj[QStringLiteral("kind")].toString();
        if (!sp.machineId.isEmpty())
            serverProfiles.append(sp);
    }

    // Sort by machine_id (align with Terracotta: binary_search_by_key)
    std::sort(serverProfiles.begin(), serverProfiles.end(),
              [](const ServerProfile& a, const ServerProfile& b) {
                  return a.machineId < b.machineId;
              });

    // Check no machine_id conflicts in server profiles
    for (int i = 1; i < serverProfiles.size(); ++i) {
        if (serverProfiles[i].machineId == serverProfiles[i-1].machineId) {
            qCWarning(logNet) << QStringLiteral("[联机] 服务器profile列表存在machine_id冲突");
            return;
        }
    }

    // Check HOST exists
    bool hasHost = false;
    for (const auto& sp : serverProfiles) {
        if (sp.kind == QStringLiteral("HOST")) {
            hasHost = true;
            break;
        }
    }
    if (!hasHost) {
        qCWarning(logNet) << QStringLiteral("[联机] 服务器profile列表中无主机");
        return;
    }

    // Merge: iterate local profiles, update/remove as needed
    QList<bool> used(serverProfiles.size(), false);
    bool changed = false;

    for (int i = m_players.size() - 1; i >= 0; --i) {
        QVariantMap local = m_players[i].toMap();
        QString localMid = local[QStringLiteral("machine_id")].toString();
        QString localKind = local[QStringLiteral("kind")].toString();

        // Find by machine_id in server profiles (binary search since sorted)
        int foundIdx = -1;
        int lo = 0, hi = serverProfiles.size() - 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            if (serverProfiles[mid].machineId == localMid) {
                foundIdx = mid;
                break;
            } else if (serverProfiles[mid].machineId < localMid) {
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }

        if (localKind == QStringLiteral("HOST")) {
            if (foundIdx >= 0 && serverProfiles[foundIdx].kind == QStringLiteral("HOST")) {
                // Update host name if changed
                if (local[QStringLiteral("name")].toString() != serverProfiles[foundIdx].name) {
                    local[QStringLiteral("name")] = serverProfiles[foundIdx].name;
                    m_players[i] = local;
                    changed = true;
                }
                used[foundIdx] = true;
            } else {
                qCWarning(logNet) << QStringLiteral("[联机] HOST profile在服务器上失效");
                return;
            }
        } else if (localKind == QStringLiteral("LOCAL")) {
            used[foundIdx >= 0 ? foundIdx : 0] = false;  // don't consume
            // Keep local profile as-is
        } else if (localKind == QStringLiteral("GUEST")) {
            if (foundIdx >= 0 && serverProfiles[foundIdx].kind == QStringLiteral("GUEST")) {
                if (used[foundIdx]) {
                    // Already consumed — duplicate entry, remove
                    m_players.removeAt(i);
                    changed = true;
                } else {
                    // Update name/vendor
                    if (local[QStringLiteral("name")].toString() != serverProfiles[foundIdx].name) {
                        local[QStringLiteral("name")] = serverProfiles[foundIdx].name;
                        changed = true;
                    }
                    if (local[QStringLiteral("vendor")].toString() != serverProfiles[foundIdx].vendor) {
                        local[QStringLiteral("vendor")] = serverProfiles[foundIdx].vendor;
                        changed = true;
                    }
                    if (changed)
                        m_players[i] = local;
                    used[foundIdx] = true;
                }
            } else if (foundIdx >= 0 && serverProfiles[foundIdx].kind == QStringLiteral("HOST")) {
                // Guest's machine_id now registered as host — remove local guest entry
                m_players.removeAt(i);
                changed = true;
            } else {
                // Guest not in server list — they disconnected, remove
                m_players.removeAt(i);
                changed = true;
            }
        }
    }

    // Add new profiles from server that aren't consumed
    for (int i = 0; i < serverProfiles.size(); ++i) {
        if (used[i]) continue;
        if (serverProfiles[i].kind == QStringLiteral("GUEST")) {
            if (serverProfiles[i].machineId == m_machineId) continue;  // don't add self

            QVariantMap newPlayer;
            newPlayer[QStringLiteral("name")] = serverProfiles[i].name;
            newPlayer[QStringLiteral("machine_id")] = serverProfiles[i].machineId;
            newPlayer[QStringLiteral("vendor")] = serverProfiles[i].vendor;
            newPlayer[QStringLiteral("kind")] = QStringLiteral("GUEST");
            newPlayer[QStringLiteral("latency")] = -1;
            m_players.append(newPlayer);
            changed = true;
        }
    }

    if (changed) {
        emit playersChanged();
    }
}

// ─────────────────────────────────────────
// Broadcast
// ─────────────────────────────────────────

void MultiplayerManager::startFakeServer(quint16 port)
{
    stopFakeServer();

    m_fakeServerSocket = new QUdpSocket(this);
    m_fakeServerSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);

    // MC LAN discovery: UDP multicast 224.0.2.60:4445
    // Format: [MOTD]server_name[/MOTD][AD]port[/AD]
    QByteArray data = QStringLiteral("[MOTD]\u8054\u673A MC\u670D\u52A1\u5668[/MOTD][AD]%1[/AD]")
                         .arg(port).toUtf8();

    m_fakeServerTimer = new QTimer(this);
    connect(m_fakeServerTimer, &QTimer::timeout, this, [this, data]() {
        if (!m_fakeServerSocket)
            return;
        // Broadcast to MC LAN discovery multicast address
        m_fakeServerSocket->writeDatagram(data,
            QHostAddress(QStringLiteral("224.0.2.60")), 4445);
    });

    qCInfo(logNet) << QStringLiteral("[联机] FakeServer已启动 端口=%1").arg(port);
    m_fakeServerTimer->start(1500);  // every 1.5s per MC protocol
}

void MultiplayerManager::stopFakeServer()
{
    if (m_fakeServerTimer) {
        m_fakeServerTimer->stop();
        m_fakeServerTimer->deleteLater();
        m_fakeServerTimer = nullptr;
    }
    if (m_fakeServerSocket) {
        m_fakeServerSocket->deleteLater();
        m_fakeServerSocket = nullptr;
    }
}

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

// ─────────────────────────────────────────
// Host MC server health check (align with Terracotta)
// ─────────────────────────────────────────

void MultiplayerManager::checkMcServerHealth()
{
    if (m_role != Host || m_state == Idle || m_state == Error)
        return;

    // Try TCP connect to local MC server port
    QTcpSocket testSocket;
    testSocket.connectToHost(QStringLiteral("127.0.0.1"), m_mcPort);
    bool connected = testSocket.waitForConnected(2000);

    if (connected) {
        // Send 0xFE (legacy server list ping) and expect 0xFF response
        testSocket.write(QByteArray(1, static_cast<char>(0xFE)));
        testSocket.waitForBytesWritten(500);
        if (testSocket.waitForReadyRead(2000)) {
            QByteArray response = testSocket.read(1);
            if (response.size() == 1 && static_cast<quint8>(response[0]) == 0xFF) {
                m_mcHealthFailures = 0;
                testSocket.disconnectFromHost();
                return;
            }
        }
        testSocket.disconnectFromHost();
    }

    // Connection failed
    m_mcHealthFailures++;
    qCWarning(logNet) << QStringLiteral("[联机] MC服务器健康检查失败 (#%1) port=%2")
        .arg(m_mcHealthFailures).arg(m_mcPort);

    if (m_mcHealthFailures >= kMcHealthMaxFailures) {
        qCWarning(logNet) << QStringLiteral("[联机] MC服务器已断开，终止联机会话");
        m_mcHealthTimer->stop();
        emit errorOccurred(QStringLiteral("MC服务器连接已断开，联机会话结束"));
        leaveRoom();
    }
}

// ─────────────────────────────────────────
// Guest MC connection verification (0xFE handshake, align with Terracotta)
// ─────────────────────────────────────────

void MultiplayerManager::verifyMcConnection()
{
    if (m_role != Guest || m_mcPort == 0)
        return;

    QTcpSocket testSocket;
    testSocket.connectToHost(QStringLiteral("127.0.0.1"), m_mcPort);
    bool connected = testSocket.waitForConnected(2000);

    if (connected) {
        // Send 0xFE legacy ping, expect 0xFF response
        testSocket.write(QByteArray(1, static_cast<char>(0xFE)));
        testSocket.waitForBytesWritten(500);
        if (testSocket.waitForReadyRead(2000)) {
            QByteArray response = testSocket.read(1);
            if (response.size() == 1 && static_cast<quint8>(response[0]) == 0xFF) {
                qCInfo(logNet) << QStringLiteral("[联机] MC连接验证通过 port=%1").arg(m_mcPort);
                testSocket.disconnectFromHost();

                // Add LOCAL profile for self (align with Terracotta ProfileKind::LOCAL)
                QVariantMap localSelf;
                localSelf[QStringLiteral("name")] = m_playerName;
                localSelf[QStringLiteral("machine_id")] = m_machineId;
                localSelf[QStringLiteral("hostname")] = QSysInfo::machineHostName();
                localSelf[QStringLiteral("kind")] = QStringLiteral("LOCAL");
                localSelf[QStringLiteral("latency")] = 0;
                m_players << localSelf;
                emit playersChanged();

                // Connection OK — start FakeServer and proceed
                startFakeServer(m_mcPort);

                // Start profile sync timer (pull profiles from host every 5s)
                m_profileSyncTimer->start();
                syncGuestProfiles();
                return;
            }
        }
        testSocket.disconnectFromHost();
    }

    // Retry
    m_mcVerifyRetries++;
    qCInfo(logNet) << QStringLiteral("[联机] MC连接验证中 尝试 #%1 port=%2")
        .arg(m_mcVerifyRetries).arg(m_mcPort);

    if (m_mcVerifyRetries >= kMcVerifyMaxRetries) {
        qCWarning(logNet) << QStringLiteral("[联机] MC连接验证失败，已达最大重试次数");
        m_mcVerifyRetries = 0;
        emit errorOccurred(QStringLiteral("无法连接到联机服务器的MC端口"));
        setState(Error, QStringLiteral("MC连接验证失败"));
    } else {
        // Retry after 1.5s
        QTimer::singleShot(1500, this, &MultiplayerManager::verifyMcConnection);
    }
}

// ─────────────────────────────────────────
// Guest profile sync: actively pull from host (align with Terracotta)
// ─────────────────────────────────────────

void MultiplayerManager::syncGuestProfiles()
{
    if (m_role != Guest || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    // Send player_ping as heartbeat and profile sync request
    sendHeartbeat();

    // Also request full profile list
    auto packet = Scaffolding::buildPacket(
        Scaffolding::kPlayerProfilesList,
        QByteArray()
    );
    m_socket->write(packet);
}

// ─────────────────────────────────────────
// Connection difficulty from NAT types (align with Terracotta)
// ─────────────────────────────────────────

MultiplayerManager::ConnectionDifficulty MultiplayerManager::calcConnectionDifficulty(
    EasyTierNatType local, EasyTierNatType remote) const
{
    auto isType = [&](const QList<EasyTierNatType>& types) -> bool {
        return types.contains(local) || types.contains(remote);
    };

    if (isType({EasyTierNatType::OpenInternet}))
        return DiffEasiest;
    if (isType({EasyTierNatType::NoPAT, EasyTierNatType::FullCone}))
        return DiffSimple;
    if (isType({EasyTierNatType::Restricted, EasyTierNatType::PortRestricted}))
        return DiffMedium;
    return DiffTough;
}

// ─────────────────────────────────────────
// Scaffolding fingerprint for ping verification (align with Terracotta)
// The fingerprint is a 16-byte magic sequence used to verify the
// scaffolding server identity before protocol negotiation.
// ─────────────────────────────────────────

const QByteArray& MultiplayerManager::scaffoldingFingerprint()
{
    static const QByteArray kFingerprint = QByteArray::fromHex(
        QStringLiteral("41574844863740595744924396998501").toLatin1()
    );
    return kFingerprint;
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
