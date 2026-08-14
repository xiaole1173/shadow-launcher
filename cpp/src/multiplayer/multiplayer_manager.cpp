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
    , m_heartbeatTimer(new QTimer(this))
    , m_discoverTimer(new QTimer(this))
    , m_idleTimer(new QTimer(this))
{
    connect(m_guard, &ConnectionGuard::blocked, this, [this](const QString& ip, const QString& reason) {
        qCWarning(logNet) << QStringLiteral("[联机] 连接被阻断 ip=%1 原因=%2").arg(ip, reason);
        emit errorOccurred(reason);
    });

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

    // Reactive difficulty: EasyTier peer-table poll re-detects local NAT → recompute + push to QML
    connect(m_easyTier, &EasyTierProcess::localNatTypeChanged,
            this, &MultiplayerManager::onLocalNatTypeChanged);

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
    connect(m_mcHealthTimer, &QTimer::timeout, this, &MultiplayerManager::checkMcHealth);

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

    // MC server port: initially 0, will be set by MCScanner when real MC server is detected
    m_mcPort = 0;

    // Scaffold protocol port: separate from MC port (align with Terracotta)
    QByteArray scaffoldSeed = m_networkKey.toUtf8();
    QByteArray scaffoldHash = QCryptographicHash::hash(scaffoldSeed, QCryptographicHash::Sha256);
    m_centerPort = 20000 + (static_cast<quint16>(scaffoldHash[0]) << 8 | scaffoldHash[1]) % 10000;
    if (m_centerPort < 20000)
        m_centerPort = 20001;  // safety fallback

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

    qCInfo(logNet) << QStringLiteral("[联机] 创建房间(已提权) 房间码=%1").arg(m_roomCode);

    setState(CreatingRoom, QStringLiteral("正在创建房间..."));
    // MC port unknown until scanner detects it; EasyTier starts with scaffold port only
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
    // Stop host MC presence timeout if active
    if (m_mcPresenceTimeoutTimer) {
        m_mcPresenceTimeoutTimer->stop();
        m_mcPresenceTimeoutTimer->deleteLater();
        m_mcPresenceTimeoutTimer = nullptr;
    }
    m_profileSyncTimer->stop();
    m_mcHealthFailures = 0;
    m_connectionDifficulty = DiffUnknown;
    m_fingerprintVerified = false;
    m_mcServerName.clear();
    emit mcServerInfoChanged();

    // Stop host MC scanner if active
    if (m_hostMcScanner) {
        m_hostMcScanner->stop();
        m_hostMcScanner->deleteLater();
        m_hostMcScanner = nullptr;
    }

    m_roomCode.clear();
    m_centerIp.clear();
    m_centerPort = 0;
    m_players.clear();
    m_centerProtocols.clear();
    m_readBuffer.clear();
    m_pendingResponses.clear();
    m_localMcPort = 0;
    m_connectRetries = 0;

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

void MultiplayerManager::stopEasyTierNow()
{
    if (m_easyTier)
        m_easyTier->stop();
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
    int current = m_state.load();
    int target = static_cast<int>(s);
    if (current != target) {
        m_state.store(target);
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
        // Increased from 2000ms to 5000ms to reduce process creation frequency (EasyTier CLI process creation is expensive)
        m_discoverTimer->setInterval(5000);
        // Disconnect first to prevent duplicate signal bindings when onNetworkReady fires multiple times
        m_discoverTimer->disconnect();
        connect(m_discoverTimer, &QTimer::timeout, this, &MultiplayerManager::doDiscoverCenter);
        m_discoverTimer->start();

        // 60s discovery timeout per protocol spec
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

    // Terracotta-aligned: listen on ALL interfaces (0.0.0.0), NOT just 127.0.0.1.
    // EasyTier delivers port-forward traffic to the virtual IP (10.144.144.1:port),
    // and a 127.0.0.1-only bind rejects it (guest sees 10060/10054 timeouts).
    // Terracotta's ScaffoldingServer binds Ipv4Addr::UNSPECIFIED for the same reason.
    if (!m_server->listen(QHostAddress::Any, m_centerPort)) {
        emit errorOccurred(QStringLiteral("无法启动联机服务: %1").arg(m_server->errorString()));
        return;
    }

    // Terracotta-aligned: MC port detected solely via LAN multicast scanner.
    // Removed all fixed-port TCP probes — scanner is the single source of truth.
    // 2026-08-15：文案改指引式——"等待MC服务器启动"小白看不懂，改为明确操作步骤
    setState(WaitingForMcServer, QStringLiteral("请打开游戏，进入单人世界后按 「Esc → 对局域网开放」"));

    // Start MC LAN multicast scanner to detect real MC server ports
    // Aligned with Terracotta set_scanning: scanner detects [MOTD][AD]{port}[/AD] on 224.0.2.60:4445
    if (!m_hostMcScanner) {
        m_hostMcScanner = new McScanner(this);
        connect(m_hostMcScanner, &McScanner::serversChanged, this, &MultiplayerManager::onHostMcDetected);
    }
    static const QString kSelfMotd = QStringLiteral("\u8054\u673A MC\u670D\u52A1\u5668");
    // Terracotta guests broadcast their own FakeServer MOTD
    // ("§6§l双击进入陶瓦联机大厅（请保持陶瓦运行）") — must NOT be
    // mistaken for a real MC server by the host scanner.
    static const QString kTerracottaMotd = QStringLiteral("\u00a76\u00a7l\u53cc\u51fb\u8fdb\u5165\u9676\u74e6\u8054\u673a\u5927\u5385\uff08\u8bf7\u4fdd\u6301\u9676\u74e6\u8fd0\u884c\uff09");
    m_hostMcScanner->start([this](const QString& motd) -> bool {
        return motd != kSelfMotd && motd != kTerracottaMotd && !motd.trimmed().isEmpty();
    });
    m_idleTimer->start();
    m_mcHealthFailures = 0;
    // Scanner handles presence detection;
    // Presence timeout: if MC doesn't respond within 2 minutes, auto-close
    // Use member timer instead of QTimer::singleShot to prevent use-after-free
    m_mcPresenceTimeoutTimer = new QTimer(this);
    m_mcPresenceTimeoutTimer->setSingleShot(true);
    m_mcPresenceTimeoutTimer->setInterval(kMcPresenceTimeoutMs);
    connect(m_mcPresenceTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (m_state == WaitingForMcServer) {
            qCWarning(logNet) << QStringLiteral("[联机] MC服务器启动超时 %1秒 自动关闭").arg(kMcPresenceTimeoutMs / 1000);
            emit errorOccurred(QStringLiteral("MC服务器启动超时，联机会话结束"));
            leaveRoom();
        }
    });
    m_mcPresenceTimeoutTimer->start();

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
            auto packet = Scaffolding::buildResponse(
                Scaffolding::kStatusUnknown,
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
            auto packet = Scaffolding::buildResponse(
                Scaffolding::kStatusUnknown,
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
    // NOTE: removed — Terracotta guests use a synchronous send_sync model;
    // unsolicited pushes would be misread as responses to their requests.
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

    // Skip if previous process is still running (prevents process creation spam)
    if (m_peerQuery->state() == QProcess::Running)
        return;

    {
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

        // Point CLI at OUR easytier-core RPC port (deterministic 15880+, not the default 11010)
        // NOTE: -o json is a TOP-LEVEL cli option — it must come BEFORE the `peer`
        // subcommand. "peer -o json" errors out (unexpected argument) and the
        // output is an error text, not JSON → discovery never finds the host.
        QStringList cliArgs;
        if (m_easyTier && m_easyTier->rpcPort() > 0)
            cliArgs << QStringLiteral("-p") << QStringLiteral("127.0.0.1:%1").arg(m_easyTier->rpcPort());
        cliArgs << QStringLiteral("-o") << QStringLiteral("json")
                << QStringLiteral("peer") << QStringLiteral("list");
        m_peerQuery->start(cliPath, cliArgs);
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

            // ── Ghost-host guard ──
            // Terracotta's peer-center caches network info; a host that went
            // offline may still appear with a valid ipv4 but cost=relay(n)
            // (no live tunnel). Forwarding to it then RSTs after ~10s.
            // Only accept a host with an ACTIVE connection (cost=p2p, or
            // Local when host runs on the same machine).
            QString costStr = obj[QStringLiteral("cost")].toString();
            if (costStr != QStringLiteral("p2p") && costStr != QStringLiteral("Local")) {
                qCInfo(logNet) << QStringLiteral("[联机] 跳过非活跃主机 cost=%1 hostname=%2")
                    .arg(costStr).arg(hostname);
                continue;
            }

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

            // Remember host NAT for reactive difficulty updates when local NAT changes
            m_hostNatType = hostNat;

            // Calculate connection difficulty (align with Terracotta)
            m_connectionDifficulty = calcConnectionDifficulty(localNat, hostNat);
            qCInfo(logNet) << QStringLiteral("[联机] 发现中心 ip=%1 端口=%2 NAT=local:%3/host:%4 难度=%5")
                .arg(ipv4).arg(port)
                .arg(localNatStr).arg(hostNatStr)
                .arg(static_cast<int>(m_connectionDifficulty));
            emit connectionDifficultyChanged();

            // ── Terracotta-style: port-forward → connect via 127.0.0.1 ──
            // Terracotta uses a RANDOM local port (PortRequest::Scaffolding.request())
            // bound to 0.0.0.0 — NOT the host's scaffold port. Same-PC testing:
            // the host's ScaffoldingServer already listens on 0.0.0.0:13448, so
            // requestSpecific(13448) collides and the ping never reaches the tunnel.
            quint16 localPort = PortRequest::requestFree(21234);
            if (!m_easyTier->addPortForward(QStringLiteral("0.0.0.0"), localPort,
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
    m_pendingResponses.enqueue(Scaffolding::kPing);
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

    // ── Terracotta-aligned retry: the guest keeps re-opening the session until
    // fingerprint verification passes (Terracotta loops 60×4s). The easytier
    // tunnel may not be ready the moment we connect (peer still relay(2)), so
    // a single-shot connect dies in ~10s. Retry with backoff until verified.
    if (m_role == Guest && !m_fingerprintVerified && m_connectRetries < kGuestConnectMaxRetries) {
        m_connectRetries++;
        qCWarning(logNet) << QStringLiteral("[联机] 指纹未验证 重连 #%1").arg(m_connectRetries);
        QTimer::singleShot(3000, this, [this]() {
            if (m_role != Guest || m_state == Idle || m_state == Error)
                return;
            // Re-connect through the SAME easytier port-forward (tunnel is up now)
            if (!m_centerIp.isEmpty() && m_centerPort > 0 && m_easyTier) {
                // Re-establish the port-forward (rules may have been lost on tunnel re-route)
                quint16 localPort = PortRequest::requestFree(21234);
                if (m_easyTier->addPortForward(QStringLiteral("0.0.0.0"), localPort,
                                               m_centerIp, m_centerPort, QStringLiteral("tcp"))) {
                    qCInfo(logNet) << QStringLiteral("[联机] 重连转发端口=%1 -> %2:%3")
                        .arg(localPort).arg(m_centerIp).arg(m_centerPort);
                    connectToCenter(QStringLiteral("127.0.0.1"), localPort);
                } else {
                    emit errorOccurred(QStringLiteral("重连失败：无法创建联机隧道"));
                }
            }
        });
        return;  // do not go Error yet
    }

    m_connectRetries = 0;
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

    // Terracotta RESPONSE format: [status 1B][bodyLen 4B BE][body] — no type field.
    // Responses arrive in the same order as our requests (Terracotta's server
    // processes one request per loop iteration), so we match them FIFO.
    while (m_readBuffer.size() >= 5) {
        quint8 status = static_cast<quint8>(m_readBuffer[0]);

        quint32 bodyLen;
        QDataStream ds(m_readBuffer.mid(1, 4));
        ds.setByteOrder(QDataStream::BigEndian);
        ds >> bodyLen;

        int packetTotal = 5 + static_cast<int>(bodyLen);
        if (m_readBuffer.size() < packetTotal)
            break;

        QByteArray body = m_readBuffer.mid(5, static_cast<int>(bodyLen));
        m_readBuffer.remove(0, packetTotal);
        handleGuestResponse(status, body);
    }
}

// ── Guest: route Terracotta responses (no type field) by FIFO request order ──
void MultiplayerManager::handleGuestResponse(quint8 status, const QByteArray& body)
{
    if (m_pendingResponses.isEmpty()) {
        qCWarning(logNet) << QStringLiteral("[联机] 收到意外响应 status=%1 (无待处理请求)").arg(status);
        return;
    }

    QString expect = m_pendingResponses.dequeue();
    if (status != Scaffolding::kStatusOk) {
        qCWarning(logNet) << QStringLiteral("[联机] 请求 %1 失败 status=%2").arg(expect).arg(status);
        if (expect == Scaffolding::kServerPort) {
            // Terracotta: fail(32) = MC server not started yet
            emit errorOccurred(QStringLiteral("联机服务器尚未就绪，请稍后再试"));
            leaveRoom();
        }
        return;
    }

    if (expect == Scaffolding::kPing) {
        handleGuestPingResponse(body);
    } else if (expect == Scaffolding::kProtocols) {
        handleGuestProtocolsResponse(body);
    } else if (expect == Scaffolding::kServerPort) {
        handleGuestServerPort(body);
    } else if (expect == Scaffolding::kPlayerProfilesList) {
        handlePlayerProfilesResponse(body);
    } else if (expect == Scaffolding::kPlayerPing) {
        // Heartbeat response — nothing to do
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
    // Echo back the same body (Terracotta response format: [status 0][len][body])
    auto packet = Scaffolding::buildResponse(Scaffolding::kStatusOk, body);
    socket->write(packet);
}

void MultiplayerManager::handleProtocols(const QByteArray& body, QTcpSocket* socket)
{
    QStringList guestProtocols = Scaffolding::unpackProtocolList(QString::fromUtf8(body));
    qCInfo(logNet) << QStringLiteral("[联机] 宾客协议列表: %1").arg(guestProtocols.join(QStringLiteral(", ")));

    // Return intersection (Terracotta response format: [status 0][len][body])
    QStringList common;
    for (const auto& p : m_supportedProtocols) {
        if (guestProtocols.contains(p))
            common << p;
    }

    auto packet = Scaffolding::buildResponse(
        Scaffolding::kStatusOk,
        Scaffolding::packProtocolList(common).toUtf8()
    );
    socket->write(packet);
}

void MultiplayerManager::handleServerPort(const QByteArray& /*body*/, QTcpSocket* socket)
{
    // Terracotta: fail(32) when MC server not started yet; otherwise 2-byte BE port.
    if (m_mcPort == 0 || m_state != WaitingForGuests) {
        auto packet = Scaffolding::buildResponse(Scaffolding::kStatusServerNotStarted, QByteArray());
        socket->write(packet);
        qCWarning(logNet) << QStringLiteral("[联机] 拒绝发送服务器端口 MC未就绪");
        return;
    }

    QByteArray portData(2, 0);
    QDataStream ds(&portData, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << m_mcPort;

    auto packet = Scaffolding::buildResponse(Scaffolding::kStatusOk, portData);
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
        m_connectRetries = 0;

        // ── Step 2: protocol negotiation ──
        m_heartbeatTimer->start();
        sendHeartbeat();

        m_pendingResponses.enqueue(Scaffolding::kProtocols);
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

        m_pendingResponses.enqueue(Scaffolding::kProtocols);
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

    m_pendingResponses.enqueue(Scaffolding::kServerPort);
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

        // TCP port-forward (Terracotta binds 0.0.0.0 — UNSPECIFIED)
        if (!m_easyTier->addPortForward(QStringLiteral("0.0.0.0"), localMcPort,
                                        hostIp, port, QStringLiteral("tcp"))) {
            qCWarning(logNet) << QStringLiteral("[联机] 无法创建MC TCP端口转发");
        }

        // UDP port-forward for mod compatibility (SimpleVoiceChat etc.)
        // Aligns with Terracotta: forwards both TCP and UDP
        m_easyTier->addPortForward(QStringLiteral("0.0.0.0"), localMcPort,
                                   hostIp, port, QStringLiteral("udp"));

        m_localMcPort = localMcPort;
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

    // Reply with pong (for guest's own latency measurement) — Terracotta response format
    QJsonObject pong;
    pong[QStringLiteral("ts")] = static_cast<double>(now);
    pong[QStringLiteral("machine_id")] = mid;
    auto packet = Scaffolding::buildResponse(
        Scaffolding::kStatusOk,
        QJsonDocument(pong).toJson(QJsonDocument::Compact)
    );
    socket->write(packet);
}

void MultiplayerManager::handlePlayerProfilesList(const QByteArray& /*body*/, QTcpSocket* socket)
{
    // Terracotta guest REQUIRES each profile to carry name/machine_id/vendor/kind.
    // Missing vendor breaks the whole list parse on the guest side.
    QJsonArray arr;
    for (const auto& p : m_players) {
        QJsonObject obj = QJsonObject::fromVariantMap(p.toMap());
        if (!obj.contains(QLatin1String("vendor")))
            obj[QStringLiteral("vendor")] = QStringLiteral("shadow");
        arr.append(obj);
    }

    auto packet = Scaffolding::buildResponse(
        Scaffolding::kStatusOk,
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

    m_pendingResponses.enqueue(Scaffolding::kPlayerPing);
    auto packet = Scaffolding::buildPacket(
        Scaffolding::kPlayerPing,
        QJsonDocument(heartbeat).toJson(QJsonDocument::Compact)
    );
    m_socket->write(packet);
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

    // Add new profiles from server that aren't consumed (Terracotta-aligned:
    // pushes HOST and GUEST alike — `if !used[i] && kind != LOCAL`)
    for (int i = 0; i < serverProfiles.size(); ++i) {
        if (used[i]) continue;
        if (serverProfiles[i].kind != QStringLiteral("GUEST")
            && serverProfiles[i].kind != QStringLiteral("HOST"))
            continue;
        if (serverProfiles[i].machineId == m_machineId) continue;  // don't add self

        QVariantMap newPlayer;
        newPlayer[QStringLiteral("name")] = serverProfiles[i].name;
        newPlayer[QStringLiteral("machine_id")] = serverProfiles[i].machineId;
        newPlayer[QStringLiteral("vendor")] = serverProfiles[i].vendor;
        newPlayer[QStringLiteral("kind")] = serverProfiles[i].kind;
        newPlayer[QStringLiteral("latency")] = -1;
        m_players.append(newPlayer);
        changed = true;
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

// Async 0xFE probe with a per-call temporary QTcpSocket (signal/lambda cleanup,
// no member socket). Fires every 5s via m_mcHealthTimer after the scanner
// confirms the real MC port. 3 consecutive failures → tear down the session.
void MultiplayerManager::checkMcHealth()
{
    if (m_role != Host || m_state == Idle || m_state == Error || m_state == WaitingForMcServer)
        return;

    qCInfo(logNet) << QStringLiteral("[联机] 执行MC健康检测 port=%1").arg(m_mcPort);

    // Create a temporary heap-allocated socket (self-cleaning via deleteLater on completion/error/timeout)
    QTcpSocket* sock = new QTcpSocket(this);
    sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    // ── Connected: send 0xFE legacy ping, wait 1s for response ──
    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        QByteArray ping(1, static_cast<char>(0xFE));
        sock->write(ping);
        QTimer::singleShot(1000, sock, [this, sock]() {
            if (sock->property("_probeDone").toBool())
                return;
            qCWarning(logNet) << QStringLiteral("[联机] MC健康检测 响应超时 port=%1").arg(m_mcPort);
            sock->setProperty("_probeDone", true);
            handleHealthCheckFailure();
            sock->abort();
            sock->deleteLater();
        });
    });

    // ── Data received: expect 0xFF response ──
    // Set a flag on the socket to mark intentional completion so error/disconnected handlers can skip.
    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        QByteArray resp = sock->read(1);
        if (resp.size() == 1 && static_cast<quint8>(resp[0]) == 0xFF) {
            m_mcHealthFailures = 0;
            qCInfo(logNet) << QStringLiteral("[联机] MC服务器健康检查正常 port=%1").arg(m_mcPort);
            sock->setProperty("_probeDone", true);
            sock->disconnect();
            sock->deleteLater();
        } else {
            qCWarning(logNet) << QStringLiteral("[联机] MC健康检测 响应数据异常 port=%1").arg(m_mcPort);
            sock->setProperty("_probeDone", true);
            handleHealthCheckFailure();
            sock->disconnect();
            sock->deleteLater();
        }
    });

    // ── Connection error: port closed / connection refused / timeout ──
    // Only count as failure if we haven't already completed the probe successfully.
    // NOTE: _probeDone is set BEFORE handleHealthCheckFailure so the follow-up
    // disconnected/abort signals from this same socket can never double-count.
    connect(sock, &QTcpSocket::errorOccurred, this, [this, sock](QAbstractSocket::SocketError err) {
        if (sock->property("_probeDone").toBool()) {
            // Probe already completed (success or counted failure); this is just cleanup noise
            sock->deleteLater();
            return;
        }
        qCWarning(logNet) << QStringLiteral("[联机] MC健康检测 连接错误 err=%1 port=%2").arg(err).arg(m_mcPort);
        // RemoteHostClosedError = MC server actively closed connection = real failure, same as others
        sock->setProperty("_probeDone", true);
        handleHealthCheckFailure();
        sock->deleteLater();
    });

    // ── Disconnected after connection: check if intentional ──
    connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
        if (sock->property("_probeDone").toBool()) {
            sock->deleteLater();
            return;
        }
        qCWarning(logNet) << QStringLiteral("[联机] MC健康检测 意外断开 port=%1").arg(m_mcPort);
        sock->setProperty("_probeDone", true);
        handleHealthCheckFailure();
        sock->deleteLater();
    });

    // ── 3s connect timeout: connection never established ──
    QTimer::singleShot(3000, sock, [this, sock]() {
        if (sock->property("_probeDone").toBool())
            return;
        if (sock->state() != QAbstractSocket::ConnectedState) {
            qCWarning(logNet) << QStringLiteral("[联机] MC健康检测 连接超时 port=%1").arg(m_mcPort);
            sock->setProperty("_probeDone", true);
            handleHealthCheckFailure();
            sock->abort();
            sock->deleteLater();
        }
    });

    sock->connectToHost(QStringLiteral("127.0.0.1"), m_mcPort);
}

// Health check failure counter
void MultiplayerManager::handleHealthCheckFailure()

{
    m_mcHealthFailures++;
    qCWarning(logNet) << QStringLiteral("[联机] MC服务器健康检查失败 (#%1) port=%2")
        .arg(m_mcHealthFailures).arg(m_mcPort);

    if (m_mcHealthFailures >= kMcHealthMaxFailures) {
        qCWarning(logNet) << QStringLiteral("[联机] MC服务器已断开，终止联机会话 port=%1").arg(m_mcPort);
        m_mcHealthTimer->stop();
        m_mcHealthFailures = 0;
        emit errorOccurred(QStringLiteral("MC服务器连接已断开，联机会话结束"));
        leaveRoom();
    }
}


// ── Scanner-based MC server detection handler ──
// Aligned with Terracotta set_scanning: real MC server port detected via LAN multicast.
// Overrides the generated m_mcPort with the real scanner-detected port.
// ── Scanner-based MC server detection handler ──
// Executed when MCScanner detects real MC server broadcast on LAN multicast.
// CRITICAL: Scanner cleanup deferred via QTimer::singleShot to avoid
// Qt6Network.dll access violation (0xc0000005) caused by synchronous
// socket stop/delete + signal emit re-entry in the same event loop iteration.
void MultiplayerManager::onHostMcDetected()
{
    // Block signal re-entry during detection
    blockSignals(true);

    if (m_role != Host || m_state != WaitingForMcServer) {
        blockSignals(false);
        return;
    }

    if (!m_hostMcScanner) {
        blockSignals(false);
        return;
    }

    QList<quint16> ports = m_hostMcScanner->ports();
    if (ports.isEmpty()) {
        blockSignals(false);
        return;
    }

    quint16 realMcPort = ports.first();
    if (realMcPort == 0 || realMcPort == m_mcPort) {
        blockSignals(false);
        return;
    }

    qCInfo(logNet) << QStringLiteral("[联机] MC扫描器检测到真实MC服务端口: 生成=%1 实际=%2").arg(m_mcPort).arg(realMcPort);
    m_mcPort = realMcPort;

    // Terracotta-aligned: host easytier whitelist must include the real MC port,
    // otherwise guest port-forwards to it are rejected. We started easytier before
    // the scanner found the MC port (whitelist = [scaffold] only), so add it now.
    if (m_easyTier) {
        m_easyTier->setTcpWhitelist({m_centerPort, m_mcPort});
    }

    // Read MOTD from scanner results
    auto results = m_hostMcScanner ? m_hostMcScanner->results() : QList<McScanResult>();
    for (const auto& r : results) {
        if (r.port == realMcPort) {
            m_mcServerName = r.motd;
            break;
        }
    }
    emit mcServerInfoChanged();

    // Read port before deferred cleanup (may be last valid access to m_hostMcScanner)
    quint16 detectedPort = realMcPort;

    // Defer scanner cleanup to NEXT event loop iteration via QTimer::singleShot(0, ...)
    // This avoids synchronous socket stop/deleteLater + signal emit re-entry in same tick.
    // Safe because: m_hostMcScanner is on main thread (same as this), deleteLater runs async.
    McScanner* scannerToClean = m_hostMcScanner;
    m_hostMcScanner = nullptr;  // Prevent re-entry via secondary serversChanged
    scannerToClean->disconnect(this);
    QTimer::singleShot(0, this, [scannerToClean]() {
        scannerToClean->stop();
        scannerToClean->deleteLater();
    });

    // Clean up presence timeout timer
    if (m_mcPresenceTimeoutTimer)
        m_mcPresenceTimeoutTimer->stop();

    // Transition to ready state
    qCInfo(logNet) << QStringLiteral("[联机] MC服务器已就绪 port=%1 (扫描器检测)").arg(detectedPort);
    m_mcHealthFailures = 0;
    m_mcHealthTimer->start();
    blockSignals(false);
    emit minecraftPortReady(static_cast<int>(detectedPort));
    if (m_state == WaitingForMcServer) {
        setState(WaitingForGuests, QStringLiteral("等待玩家加入..."));
    }

    // Trigger connection difficulty update for host (queries EasyTier local NAT)
    requestDifficultyUpdate();
}

// ── Guest MC connection verification (0xFE handshake, align with Terracotta) ──
// ─────────────────────────────────────────

void MultiplayerManager::verifyMcConnection()
{
    // Terracotta verifies the LOCAL forwarded port (check_mc_conn(local_port)),
    // not the remote MC port — the local listen port may differ when the
    // requested port was occupied (dynamic fallback).
    quint16 verifyPort = m_localMcPort != 0 ? m_localMcPort : m_mcPort;
    if (m_role != Guest || m_mcPort == 0 || verifyPort == 0)
        return;

    // Async 0xFE handshake with a per-call temporary socket (same self-cleaning
    // pattern as checkMcHealth) — no waitFor* blocking on the main thread.
    QTcpSocket* testSocket = new QTcpSocket(this);
    testSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    // Connected: send legacy 0xFE ping, expect 0xFF within 2s
    connect(testSocket, &QTcpSocket::connected, this, [this, testSocket, verifyPort]() {
        testSocket->write(QByteArray(1, static_cast<char>(0xFE)));
        QTimer::singleShot(2000, testSocket, [this, testSocket, verifyPort]() {
            if (testSocket->property("_verifyDone").toBool())
                return;
            qCWarning(logNet) << QStringLiteral("[联机] MC连接验证 响应超时 port=%1").arg(verifyPort);
            testSocket->setProperty("_verifyDone", true);
            testSocket->abort();
            testSocket->deleteLater();
            scheduleMcVerifyRetry();
        });
    });

    // Data received: expect first byte 0xFF (legacy server list ping response)
    connect(testSocket, &QTcpSocket::readyRead, this, [this, testSocket, verifyPort]() {
        QByteArray response = testSocket->read(1);
        if (response.size() == 1 && static_cast<quint8>(response[0]) == 0xFF) {
            qCInfo(logNet) << QStringLiteral("[联机] MC连接验证通过 port=%1").arg(verifyPort);
            testSocket->setProperty("_verifyDone", true);
            testSocket->disconnect();
            testSocket->deleteLater();
            completeGuestJoin(verifyPort, true);
        } else {
            qCWarning(logNet) << QStringLiteral("[联机] MC连接验证 响应数据异常 port=%1").arg(m_mcPort);
            testSocket->setProperty("_verifyDone", true);
            testSocket->disconnect();
            testSocket->deleteLater();
            scheduleMcVerifyRetry();
        }
    });

    // Connection failed (refused / closed): real failure → retry
    connect(testSocket, &QTcpSocket::errorOccurred, this, [this, testSocket, verifyPort]() {
        if (testSocket->property("_verifyDone").toBool()) {
            testSocket->deleteLater();
            return;
        }
        qCWarning(logNet) << QStringLiteral("[联机] MC连接验证 连接失败 port=%1").arg(verifyPort);
        testSocket->setProperty("_verifyDone", true);
        testSocket->deleteLater();
        scheduleMcVerifyRetry();
    });

    // 2s connect timeout
    QTimer::singleShot(2000, testSocket, [this, testSocket, verifyPort]() {
        if (testSocket->property("_verifyDone").toBool())
            return;
        if (testSocket->state() != QAbstractSocket::ConnectedState) {
            qCWarning(logNet) << QStringLiteral("[联机] MC连接验证 连接超时 port=%1").arg(verifyPort);
            testSocket->setProperty("_verifyDone", true);
            testSocket->abort();
            testSocket->deleteLater();
            scheduleMcVerifyRetry();
        }
    });

    testSocket->connectToHost(QStringLiteral("127.0.0.1"), verifyPort);
}

// Retry scheduling shared by all async failure paths (one count per attempt)
void MultiplayerManager::scheduleMcVerifyRetry()
{
    if (m_role != Guest)
        return;

    m_mcVerifyRetries++;
    qCInfo(logNet) << QStringLiteral("[联机] MC连接验证中 尝试 #%1 port=%2")
        .arg(m_mcVerifyRetries).arg(m_mcPort);

    if (m_mcVerifyRetries >= kMcVerifyMaxRetries) {
        qCWarning(logNet) << QStringLiteral("[联机] MC连接验证失败，已达最大重试次数 继续加入");
        m_mcVerifyRetries = 0;
        // Terracotta-aligned: do NOT fail the join — MC may still be booting.
        // FakeServer + profile sync proceed regardless.
        completeGuestJoin(m_localMcPort != 0 ? m_localMcPort : m_mcPort, false);
    } else {
        QTimer::singleShot(1500, this, &MultiplayerManager::verifyMcConnection);
    }
}

// ── Terracotta-aligned guest join completion ──
// Terracotta's start_guest runs the 0xFE probe up to 8 times but ALWAYS
// proceeds ("MC connection is OK.") — the probe is informational, not a gate.
void MultiplayerManager::completeGuestJoin(quint16 verifyPort, bool verified)
{
    qCInfo(logNet) << QStringLiteral("[联机] 完成宾客加入 verified=%1 port=%2")
        .arg(verified).arg(verifyPort);

    // Add LOCAL profile for self (align with Terracotta ProfileKind::LOCAL)
    QVariantMap localSelf;
    localSelf[QStringLiteral("name")] = m_playerName;
    localSelf[QStringLiteral("machine_id")] = m_machineId;
    localSelf[QStringLiteral("hostname")] = QSysInfo::machineHostName();
    localSelf[QStringLiteral("kind")] = QStringLiteral("LOCAL");
    localSelf[QStringLiteral("latency")] = 0;
    m_players << localSelf;
    emit playersChanged();

    // FakeServer announces the LOCAL forwarded port so the MC client
    // auto-discovers the proxied server on 127.0.0.1 (Terracotta-aligned).
    startFakeServer(verifyPort);

    // Profile sync pulls host + guests every 5s (fixes "only self visible")
    m_profileSyncTimer->start();
    syncGuestProfiles();
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
    m_pendingResponses.enqueue(Scaffolding::kPlayerProfilesList);
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
    // Unknown / no peer connection → unknown difficulty (UI shows 未知)
    if (local == EasyTierNatType::Unknown && remote == EasyTierNatType::Unknown)
        return DiffUnknown;

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

// ── Reactive difficulty update: EasyTier peer-table poll refreshed the local NAT type ──
// Recomputes the difficulty and pushes it to QML via connectionDifficultyChanged.
void MultiplayerManager::onLocalNatTypeChanged(int natType)
{
    // Only meaningful while inside a session with a role
    if (m_role == None || m_state == Idle || m_state == Error)
        return;

    auto localNat = static_cast<EasyTierNatType>(natType);

    ConnectionDifficulty prev = m_connectionDifficulty;
    if (m_role == Host) {
        // Host difficulty reflects how easy it is for guests to reach us
        m_connectionDifficulty = calcConnectionDifficulty(localNat, localNat);
    } else if (m_role == Guest) {
        m_connectionDifficulty = calcConnectionDifficulty(localNat, m_hostNatType);
    } else {
        return;
    }

    if (m_connectionDifficulty != prev) {
        qCInfo(logNet) << QStringLiteral("[联机] 连接难度响应式更新 NAT=%1 难度=%2")
            .arg(static_cast<int>(localNat)).arg(static_cast<int>(m_connectionDifficulty));
        emit connectionDifficultyChanged();
    }
}

// ── Request connection difficulty update (host: query local NAT type via EasyTier CLI) ──
void MultiplayerManager::requestDifficultyUpdate()
{
    if (m_role == Host && m_state != Idle) {
        // Use standalone one-shot QProcess to avoid conflicting with m_peerQuery (used by guest discovery)
        QString cliDir = QCoreApplication::applicationDirPath();
        QStringList cliPaths = {
            cliDir + QStringLiteral("/bin/easytier-cli.exe"),
            cliDir + QStringLiteral("/../../bin/easytier-cli.exe"),
            cliDir + QStringLiteral("/easytier-cli.exe"),
        };
        QString cliPath;
        for (const auto& p : cliPaths) {
            if (QFileInfo::exists(p)) { cliPath = p; break; }
        }
        if (!QFileInfo::exists(cliPath))
            return;

        QProcess* natQuery = new QProcess(this);
        connect(natQuery, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, natQuery](int, QProcess::ExitStatus) {
            QString output = QString::fromUtf8(natQuery->readAllStandardOutput());
            natQuery->deleteLater();

            QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
            if (!doc.isArray() || m_role != Host) return;

            EasyTierNatType localNat = EasyTierNatType::Unknown;
            for (const auto& item : doc.array()) {
                QJsonObject obj = item.toObject();
                if (obj[QStringLiteral("cost")].toString() == QStringLiteral("Local")) {
                    QString natStr = obj[QStringLiteral("nat_type")].toString();
                    if (natStr == QStringLiteral("OpenInternet"))        localNat = EasyTierNatType::OpenInternet;
                    else if (natStr == QStringLiteral("NoPat"))          localNat = EasyTierNatType::NoPAT;
                    else if (natStr == QStringLiteral("FullCone"))       localNat = EasyTierNatType::FullCone;
                    else if (natStr == QStringLiteral("Restricted"))     localNat = EasyTierNatType::Restricted;
                    else if (natStr == QStringLiteral("PortRestricted")) localNat = EasyTierNatType::PortRestricted;
                    else if (natStr == QStringLiteral("Symmetric"))      localNat = EasyTierNatType::Symmetric;
                    break;
                }
            }

            ConnectionDifficulty prev = m_connectionDifficulty;
            m_connectionDifficulty = calcConnectionDifficulty(localNat, localNat);
            if (m_connectionDifficulty != prev) {
                qCInfo(logNet) << QStringLiteral("[联机] 主机连接难度更新 NAT=%1 难度=%2")
                    .arg(static_cast<int>(localNat)).arg(static_cast<int>(m_connectionDifficulty));
                emit connectionDifficultyChanged();
            }
        });

        QStringList cliArgs;
        if (m_easyTier && m_easyTier->rpcPort() > 0)
            cliArgs << QStringLiteral("-p") << QStringLiteral("127.0.0.1:%1").arg(m_easyTier->rpcPort());
        cliArgs << QStringLiteral("peer") << QStringLiteral("-o") << QStringLiteral("json");
        natQuery->start(cliPath, cliArgs);
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
