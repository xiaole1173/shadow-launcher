// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "easytier_process.h"
#include "elevated_session.h"
#include "../utils/secure_wipe.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFile>
#include <QTemporaryFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../utils/logger.h"

using namespace ShadowLauncher;

EasyTierProcess::EasyTierProcess(QObject* parent)
    : QObject(parent)
    , m_outputTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_outputTimer->setInterval(500);  // 初始500ms轮询，就绪后5s
    connect(m_outputTimer, &QTimer::timeout, this, &EasyTierProcess::onTimerTick);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(60000); // 60s timeout
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_ready) {
            stop();
            emit errorOccurred(QString::fromUtf8("联机网络连接超时\n请确认网络畅通后重试"));
        }
    });
}

EasyTierProcess::~EasyTierProcess()
{
    stop();
}

QString EasyTierProcess::findEasyTierExe() const
{
    // Look in bin/ next to the executable
    QStringList paths = {
        QCoreApplication::applicationDirPath() + "/bin/easytier-core.exe",
        QCoreApplication::applicationDirPath() + "/bin/easytier-core.exe",
        QCoreApplication::applicationDirPath() + "/../../bin/easytier-core.exe",
        QCoreApplication::applicationDirPath() + "/easytier-core.exe",
        QCoreApplication::applicationDirPath() + "/../bin/easytier-core.exe",
    };
    for (const auto& p : paths) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    return QString();
}

QString EasyTierProcess::findEasyTierCli() const
{
    QStringList paths = {
        QCoreApplication::applicationDirPath() + "/bin/easytier-cli.exe",
        QCoreApplication::applicationDirPath() + "/bin/easytier-cli.exe",
        QCoreApplication::applicationDirPath() + "/../../bin/easytier-cli.exe",
        QCoreApplication::applicationDirPath() + "/easytier-cli.exe",
        QCoreApplication::applicationDirPath() + "/../bin/easytier-cli.exe",
    };
    for (const auto& p : paths) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    return QString();
}

void EasyTierProcess::start(const QString& networkName, const QString& networkKey,
                            const QString& hostname,
                            const QList<quint16>& whitelistPorts)
{
    stop();

    m_networkName = networkName;
    m_networkKey = networkKey;
    m_hostname = hostname;
    m_ready = false;
    m_virtualIp.clear();

    qCInfo(logNet) << QStringLiteral("[EasyTier] 开始启动 网络名=%1 主机名=%2").arg(networkName, hostname);

    QString exe = findEasyTierExe();
    if (exe.isEmpty()) {
        emit errorOccurred(QStringLiteral("找不到 easytier-core.exe，请将 EasyTier 放在 bin/ 目录"));
        return;
    }

    // Pick a deterministic RPC port from network key (align with Terracotta: PortRequest::EasyTierRPC)
    QByteArray rpcSeed = m_networkKey.toUtf8();
    quint32 rpcHash = qChecksum(rpcSeed);
    m_rpcPort = 15880 + (rpcHash % (65535 - 15880));
    // Ensure distinct from other ports
    if (m_rpcPort == 11010) m_rpcPort++;

    // Kill any stale easytier-core processes to avoid port conflicts (default port 11010)
    {
        QProcess killer;
        killer.start("taskkill", {"/f", "/im", "easytier-core.exe"});
        killer.waitForFinished(3000);
    }

    // Use EasyTier community public nodes (same as Terracotta/主流启动器).
    // No private relay infrastructure needed — reduces legal surface.
    // Public nodes: EasyTier community shared nodes sponsored by cloud providers.
    static const auto kPublicPeers = {
        QStringLiteral("tcp://public.easytier.top:11010"),
        QStringLiteral("tcp://public2.easytier.cn:54321"),
        QStringLiteral("https://etnode.zkitefly.eu.org/node1"),
        QStringLiteral("https://etnode.zkitefly.eu.org/node2"),
    };

    qCInfo(logNet) << QStringLiteral("[EasyTier] 使用社区公共中继节点");

    // Align with Terracotta architecture:
    //   Host: --no-tun --ipv4 10.144.144.1 --hostname ... --tcp-whitelist {port}
    //   Guest: --no-tun (DHCP from TOML)
    //   Both: peers / network_identity from TOML config file
    //   Guest connects via port-forward to 127.0.0.1 instead of TUN virtual IP
    bool isHost = !hostname.isEmpty();

    QByteArray tomlContent;
    tomlContent.append("peers = [");
    bool first = true;
    for (const auto& peer : kPublicPeers) {
        if (!first) tomlContent.append(", ");
        tomlContent.append("\"" + peer.toUtf8() + "\"");
        first = false;
    }
    tomlContent.append("]\n");
    if (isHost) {
        // Host: fixed IP in --ipv4, no DHCP needed
        tomlContent.append("dhcp = false\n");
    } else {
        // Guest: DHCP assigned by EasyTier
        tomlContent.append("dhcp = true\n");
    }
    tomlContent.append("\n[network_identity]\n");
    tomlContent.append(QStringLiteral("network_name = \"%1\"\n").arg(networkName).toUtf8());
    tomlContent.append(QStringLiteral("network_secret = \"%1\"\n").arg(networkKey).toUtf8());

    QTemporaryFile tmpFile(QDir::tempPath() + QStringLiteral("/shadow_easytier_XXXXXX.toml"));
    tmpFile.setAutoRemove(false);
    tmpFile.open();
    tmpFile.write(tomlContent);
    tmpFile.close();
    m_peerConfigPath = tmpFile.fileName();
    secureWipe(tomlContent);

    QStringList args;
    args << "--config-file" << m_peerConfigPath;
    args << "--no-tun";  // No TUN device — align with Terracotta

    // Standard args (align with Terracotta defaults)
    args << "--compression" << "zstd";
    args << "--multi-thread";
    args << "--latency-first";
    args << "--enable-kcp-proxy";
    args << "--p2p-only";
    args << "-l" << QStringLiteral("udp://0.0.0.0:0");
    args << "-l" << QStringLiteral("tcp://0.0.0.0:0");
    // Explicit RPC port (align with Terracotta: PortRequest::EasyTierRPC)
    args << "-r" << QString::number(m_rpcPort);

    if (isHost) {
        args << "--ipv4" << QStringLiteral("10.144.144.1");
        args << "--hostname" << hostname;
        // Whitelist both scaffold port and MC port
        for (quint16 p : whitelistPorts) {
            if (p > 0) {
                args << "--tcp-whitelist" << QString::number(p);
                args << "--udp-whitelist" << QString::number(p);
            }
        }
    }

    // Start easytier-core with config file (peers, dhcp, name/secret).
    // Public nodes in TOML are sufficient — no connector add fallback needed.
    if (ElevatedSession::isActive()) {
        startViaQProcess(exe, args, QByteArray());
        // Delete config file after easytier has read it (startup ~100ms, safe margin 1s)
        QTimer::singleShot(1000, this, [this]() {
            if (!m_peerConfigPath.isEmpty()) {
                QFile::remove(m_peerConfigPath);
                qCInfo(logNet) << QStringLiteral("[EasyTier] 临时配置文件已删除");
                m_peerConfigPath.clear();
            }
        });
        return;
    }

    // ── Non-elevated: should never reach here (createRoom/joinRoom self-elevate) ──
}

void EasyTierProcess::stop()
{
    qCInfo(logNet) << QStringLiteral("[EasyTier] 停止进程");
    m_outputTimer->stop();
    m_timeoutTimer->stop();

    if (m_cliProcess) {
        m_cliProcess->disconnect();
        if (m_cliProcess->state() == QProcess::Running)
            m_cliProcess->kill();
        m_cliProcess->deleteLater();
        m_cliProcess = nullptr;
    }

#ifdef Q_OS_WIN
    if (m_winProcess) {
        TerminateProcess(m_winProcess, 0);
        CloseHandle(m_winProcess);
        m_winProcess = nullptr;
    }
#endif

    if (m_process) {
        m_process->disconnect();
        if (m_process->state() == QProcess::Running) {
            m_process->terminate();
            if (!m_process->waitForFinished(3000))
                m_process->kill();
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    // Clean up temporary config file
    if (!m_peerConfigPath.isEmpty()) {
        QFile::remove(m_peerConfigPath);
        m_peerConfigPath.clear();
    }

    m_virtualIp.clear();
    m_centerPort = 0;
    m_ready = false;
}

bool EasyTierProcess::addPortForward(const QString& localAddr, quint16 localPort,
                                        const QString& remoteAddr, quint16 remotePort,
                                        const QString& proto)
{
    QString cliExe = findEasyTierCli();
    if (cliExe.isEmpty()) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 找不到 easytier-cli.exe，无法创建端口转发");
        return false;
    }

    QStringList args;
    // Point CLI to our easytier-core RPC (align with Terracotta)
    if (m_rpcPort > 0) {
        args << QStringLiteral("-p") << QStringLiteral("127.0.0.1:%1").arg(m_rpcPort);
    }
    args << QStringLiteral("port-forward") << QStringLiteral("add") << proto
         << QStringLiteral("%1:%2").arg(localAddr).arg(localPort)
         << QStringLiteral("%1:%2").arg(remoteAddr).arg(remotePort);

    qCInfo(logNet) << QStringLiteral("[EasyTier] 创建端口转发 %1 %2:%3 -> %4:%5")
        .arg(proto).arg(localAddr).arg(localPort).arg(remoteAddr).arg(remotePort);

    QProcess proc;
    proc.start(cliExe, args);
    if (!proc.waitForFinished(5000)) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 端口转发超时");
        return false;
    }

    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    if (!out.isEmpty())
        qCInfo(logNet) << QStringLiteral("[EasyTier] port-forward 输出: %1").arg(out);
    if (!err.isEmpty())
        qCWarning(logNet) << QStringLiteral("[EasyTier] port-forward 错误: %1").arg(err);

    bool ok = (proc.exitCode() == 0);
    if (!ok)
        qCWarning(logNet) << QStringLiteral("[EasyTier] 端口转发失败 退出码=%1").arg(proc.exitCode());
    return ok;
}

void EasyTierProcess::addRelayConnector(const QString& relayEp)
{
    if (relayEp.isEmpty())
        return;

    // Verify easytier-core is still running before adding connector
    if (!m_process || m_process->state() != QProcess::Running) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 核心进程未运行，跳过动态添加中继节点");
        return;
    }

    QString cliExe = findEasyTierCli();
    if (cliExe.isEmpty()) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 找不到 easytier-cli.exe，跳过动态添加中继节点");
        return;
    }

    // Start a brief QProcess to call `easytier-cli connector add <url>`
    // This process exits after ~100ms. The relay IP only appears on this
    // short-lived child's CLI — the easytier-core daemon has zero relay exposure.
    QStringList args;
    if (m_rpcPort > 0) {
        args << QStringLiteral("-p") << QStringLiteral("127.0.0.1:%1").arg(m_rpcPort);
    }
    args << QStringLiteral("connector") << QStringLiteral("add") << relayEp;

    QProcess* cliProc = new QProcess(this);
    connect(cliProc, &QProcess::finished, cliProc, &QObject::deleteLater);

    qCInfo(logNet) << QStringLiteral("[EasyTier] 动态添加中继节点 (short-lived CLI)");
    cliProc->start(cliExe, args);

    // Optional: wait briefly so we can log success/failure
    if (cliProc->waitForFinished(5000)) {
        QString out = QString::fromUtf8(cliProc->readAllStandardOutput()).trimmed();
        QString err = QString::fromUtf8(cliProc->readAllStandardError()).trimmed();
        if (!out.isEmpty())
            qCInfo(logNet) << QStringLiteral("[EasyTier] connector add 输出: %1").arg(out);
        if (!err.isEmpty())
            qCWarning(logNet) << QStringLiteral("[EasyTier] connector add 错误: %1").arg(err);
    }
}

void EasyTierProcess::startViaQProcess(const QString& exe, const QStringList& args,
                                       const QByteArray& tomlData)
{
    Q_UNUSED(tomlData);
    qCInfo(logNet) << QStringLiteral("[EasyTier] 通过QProcess启动 (配置文件中已含公共中继节点)");

    // Prevent duplicate start
    if (m_process && m_process->state() == QProcess::Running) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 已经在运行");
        return;
    }

    // Use QProcess (we're already elevated, no runas needed)
    if (!m_process)
        m_process = new QProcess(this);

    connect(m_process, &QProcess::started, this, &EasyTierProcess::onProcessStarted);
    connect(m_process, &QProcess::errorOccurred, this, &EasyTierProcess::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &EasyTierProcess::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &EasyTierProcess::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EasyTierProcess::onProcessFinished);

    // No ET_* env vars set — easytier starts with zero sensitive data.
    // Public peers in --config-file TOML are sufficient.
    m_process->start(exe, args);

    if (!m_process->waitForStarted(5000)) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 进程启动超时");
    }

    m_ready = false;
    m_timeoutTimer->start();
    m_outputTimer->start();
    emit stateChanged(QStringLiteral("正在创建虚拟网络..."));
}

bool EasyTierProcess::isRunning() const
{
#ifdef Q_OS_WIN
    if (m_winProcess) {
        DWORD exitCode = 0;
        return GetExitCodeProcess(m_winProcess, &exitCode) && exitCode == STILL_ACTIVE;
    }
#endif
    return m_process && m_process->state() == QProcess::Running;
}

void EasyTierProcess::onTimerTick()
{
#ifdef Q_OS_WIN
    if (m_winProcess) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(m_winProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            // Process exited
            CloseHandle(m_winProcess);
            m_winProcess = nullptr;
            m_outputTimer->stop();
            m_timeoutTimer->stop();
            if (!m_ready) {
                emit errorOccurred(QStringLiteral("EasyTier 进程意外退出 (退出码: %1)").arg(exitCode));
            }
            m_ready = false;
            return;
        }
        pollPeerList();
        return;
    }
#endif
    checkOutput();
    pollPeerList();
}

void EasyTierProcess::onProcessStarted()
{
    qCInfo(logNet) << QStringLiteral("[EasyTier] 进程已启动");
    m_outputTimer->start();
}

static QString s_lastPeerTable; // cache: only log on change

void EasyTierProcess::pollPeerList()
{
    if (m_cliProcess && m_cliProcess->state() == QProcess::Running)
        return; // previous poll still running

    QString cliExe = findEasyTierCli();
    if (cliExe.isEmpty()) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] CLI未找到 重试中");
        return;
    }

    if (!m_cliProcess) {
        m_cliProcess = new QProcess(this);
        connect(m_cliProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this]() {
            QString output = QString::fromUtf8(m_cliProcess->readAllStandardOutput())
                           + QString::fromUtf8(m_cliProcess->readAllStandardError());
            // Only log peer table when content changes (avoids log spam)
            if (output != s_lastPeerTable) {
                qCInfo(logNet) << QStringLiteral("[EasyTier] Peer列表更新: %1").arg(output);
                s_lastPeerTable = output;
            }
            parsePeerList(output);
        });
    }

    // 轮询在后台进行，不输出日志避免刷屏
    QStringList cliArgs;
    if (m_rpcPort > 0) {
        cliArgs << QStringLiteral("-p") << QStringLiteral("127.0.0.1:%1").arg(m_rpcPort);
    }
    cliArgs << QStringLiteral("peer");
    m_cliProcess->start(cliExe, cliArgs);
}

void EasyTierProcess::parsePeerList(const QString& output)
{
    // Always update virtual IP from peer table (covers both initial and DHCP-assigned)
    QRegularExpression ipRx(QStringLiteral(R"(\|?\s*(\d+\.\d+\.\d+\.\d+)(?:/\d+)?\s*\|)"));

    auto m = ipRx.match(output);
    if (m.hasMatch()) {
        QString newIp = m.captured(1);
        if (newIp != m_virtualIp) {
            m_virtualIp = newIp;
            qCInfo(logNet) << QStringLiteral("[EasyTier] 虚拟IP已获取 ip=%1").arg(m_virtualIp);
            emit virtualIpChanged(m_virtualIp);
        }
    }

    // Deterministic IP as fallback if peer table has no entries yet
    if (m_virtualIp.isEmpty()) {
        QByteArray hash = QCryptographicHash::hash(
            m_networkName.toUtf8(), QCryptographicHash::Sha256
        );
        m_virtualIp = QStringLiteral("10.%1.%2.%3")
            .arg(static_cast<quint8>(hash[0]))
            .arg(static_cast<quint8>(hash[1]))
            .arg(static_cast<quint8>(hash[2]));
    }

    // Parse hostname for center port
    if (!m_hostname.isEmpty() && m_centerPort == 0) {
        static QRegularExpression portRx(QStringLiteral(R"(scaffolding-mc-server-(\d+))"));
        auto pm = portRx.match(output);
        if (pm.hasMatch()) {
            m_centerPort = pm.captured(1).toUShort();
        }
    }

    // Check if peer list has entries → network is ready.
    if (!m_ready && !m_virtualIp.isEmpty()) {
        bool hasPeer = output.contains(QRegularExpression(QStringLiteral(R"(\|\s*\d+\.\d+\.\d+\.\d+)")));
        if (hasPeer) {
            m_ready = true;
            m_timeoutTimer->stop();
            m_outputTimer->setInterval(5000);  // 就绪后降频到5秒，虚拟IP已通过virtualIpChanged信号更新
            qCInfo(logNet) << QStringLiteral("[EasyTier] 网络就绪 virtual_ip=%1").arg(m_virtualIp);
            emit networkReady(m_virtualIp);
        }
    }

    // Also check if process is still running via peer output
    if (output.isEmpty() || output.contains(QStringLiteral("Error"))) {
        // EasyTier might not be running yet — that's OK, keep polling
    }
}

void EasyTierProcess::onProcessError(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
    case QProcess::FailedToStart:
        msg = QStringLiteral("EasyTier 启动失败，请确认 easytier-core.exe 存在");
        break;
    case QProcess::Crashed:
        msg = QStringLiteral("EasyTier 意外崩溃");
        break;
    case QProcess::Timedout:
        msg = QStringLiteral("EasyTier 操作超时");
        break;
    default:
        msg = QStringLiteral("EasyTier 错误: %1").arg(error);
        break;
    }
    emit errorOccurred(msg);
}

void EasyTierProcess::onReadyRead()
{
    // Accumulate output for periodic parsing
    m_outputBuffer += m_process->readAllStandardOutput();
    m_outputBuffer += m_process->readAllStandardError();
}

void EasyTierProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    qCInfo(logNet) << QStringLiteral("[EasyTier] 进程已退出 退出码=%1").arg(exitCode);
    m_outputTimer->stop();
    m_timeoutTimer->stop();
    m_ready = false;
    // Log stderr for debugging (exit code 1 usually means config/startup error)
    if (!m_outputBuffer.isEmpty()) {
        qCInfo(logNet) << QStringLiteral("[EasyTier] 进程输出: %1").arg(m_outputBuffer);
        m_outputBuffer.clear();
    }
    if (exitCode != 0 && !m_ready) {
        emit errorOccurred(QString::fromUtf8("EasyTier 异常退出，请重试"));
    }
}

void EasyTierProcess::checkOutput()
{
    if (m_outputBuffer.isEmpty()) return;

    QStringList lines = m_outputBuffer.split('\n', Qt::KeepEmptyParts);
    // Keep incomplete last line in buffer
    m_outputBuffer = lines.takeLast();

    for (const auto& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        parseOutputLine(trimmed);
    }
}

QList<EasyTierPeerInfo> EasyTierProcess::parsePeerListJson(const QString& jsonOutput) const
{
    QList<EasyTierPeerInfo> result;
    QJsonDocument doc = QJsonDocument::fromJson(jsonOutput.toUtf8());
    if (!doc.isArray()) return result;

    for (const auto& item : doc.array()) {
        QJsonObject obj = item.toObject();
        EasyTierPeerInfo info;
        info.hostname = obj[QStringLiteral("hostname")].toString();
        info.ipv4 = obj[QStringLiteral("ipv4")].toString();
        info.isLocal = (obj[QStringLiteral("cost")].toString() == QStringLiteral("Local"));

        // Parse NAT type (align with Terracotta mapping)
        QString natStr = obj[QStringLiteral("nat_type")].toString();
        if (natStr == QStringLiteral("OpenInternet"))
            info.natType = EasyTierNatType::OpenInternet;
        else if (natStr == QStringLiteral("NoPat"))
            info.natType = EasyTierNatType::NoPAT;
        else if (natStr == QStringLiteral("FullCone"))
            info.natType = EasyTierNatType::FullCone;
        else if (natStr == QStringLiteral("Restricted"))
            info.natType = EasyTierNatType::Restricted;
        else if (natStr == QStringLiteral("PortRestricted"))
            info.natType = EasyTierNatType::PortRestricted;
        else if (natStr == QStringLiteral("Symmetric"))
            info.natType = EasyTierNatType::Symmetric;
        else if (natStr == QStringLiteral("SymUdpFirewall"))
            info.natType = EasyTierNatType::SymmetricUdpWall;
        else if (natStr == QStringLiteral("SymmetricEasyInc"))
            info.natType = EasyTierNatType::SymmetricEasyIncrease;
        else if (natStr == QStringLiteral("SymmetricEasyDec"))
            info.natType = EasyTierNatType::SymmetricEasyDecrease;
        else
            info.natType = EasyTierNatType::Unknown;

        result.append(info);
    }
    return result;
}

void EasyTierProcess::parseOutputLine(const QString& line)
{
    // 不逐行记录stdout（虚拟IP从easytier-cli peer表格获取，不从stdout regex提取，
    // 因为stdout中可能出现本机真实局域网IP导致误捕获）

    // EasyTier v2.6 says "peer added" instead of "ready"
    if (!m_ready && line.contains(QStringLiteral("peer added"), Qt::CaseInsensitive)) {
        m_ready = true;
        m_timeoutTimer->stop();
        qCInfo(logNet) << QStringLiteral("[EasyTier] Peer已加入 网络就绪 virtual_ip=%1").arg(m_virtualIp);

        if (m_virtualIp.isEmpty()) {
            QByteArray hash = QCryptographicHash::hash(
                m_networkName.toUtf8(), QCryptographicHash::Sha256
            );
            m_virtualIp = QStringLiteral("10.%1.%2.%3")
                .arg(static_cast<quint8>(hash[0]))
                .arg(static_cast<quint8>(hash[1]))
                .arg(static_cast<quint8>(hash[2]));
        }

        emit networkReady(m_virtualIp);
    }

    // Parse hostname for center port
    if (!m_hostname.isEmpty()) {
        static QRegularExpression portRx(QStringLiteral(R"(scaffolding-mc-server-(\d+))"));
        auto pm = portRx.match(m_hostname);
        if (pm.hasMatch()) {
            m_centerPort = pm.captured(1).toUShort();
        }
    }
}
