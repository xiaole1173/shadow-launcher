// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "easytier_process.h"
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
    // Reset poll cadence: a previous "ready" session may have lowered it to 5s.
    // The launcher no longer relaunches per-room, so restore the 500ms startup poll.
    m_outputTimer->setInterval(500);

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

    // NOTE: do NOT taskkill /f /im easytier-core.exe here. That kills EVERY
    // easytier-core on the machine — including another launcher's instance
    // (主流启动器/Terracotta embedded easytier) during same-PC testing, which made
    // the host side crash. Our own stale instance is cleaned by stop() above.

    // Use EasyTier community public nodes (same as Terracotta/主流启动器).
    // No private relay infrastructure needed — reduces legal surface.
    // Public nodes: EasyTier community shared nodes sponsored by cloud providers.
    // terracotta.glavo.site is Terracotta's own public relay — all Terracotta clients
    // connect to it, so including it maximizes interop with 主流启动器/other launchers.
    static const auto kPublicPeers = {
        QStringLiteral("https://terracotta.glavo.site/acebc7d8-1208-47fd-b212-d03ac49e36e0"),
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

    // CRITICAL: easytier TOML expects [[peer]] uri = "..." table-array syntax.
    // A top-level `peers = [...]` array is silently ignored (verified against
    // easytier-core 2.6.4) — the instance then connects to NO public node and is
    // invisible to guests. This was the interop bug that made 主流启动器 report
    // "Cannot find scaffolding server".
    QByteArray tomlContent;
    for (const auto& peer : kPublicPeers) {
        tomlContent.append("[[peer]]\n");
        tomlContent.append("uri = \"" + peer.toUtf8() + "\"\n");
    }
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
    } else {
        // Terracotta guest args: -d (DHCP) + whitelist 0 (allow-all inbound).
        // Without --tcp-whitelist=0 the easytier port-forward may reject
        // inbound delivery (empty whitelist ≠ allow-all) — same-PC interop
        // with Terracotta hosts failed until these were added.
        args << "-d";
        args << "--tcp-whitelist" << QStringLiteral("0");
        args << "--udp-whitelist" << QStringLiteral("0");
    }

    // Start easytier-core with config file (peers, dhcp, name/secret).
    // Public nodes in TOML are sufficient — no connector add fallback needed.
    //
    // easytier-core runs NON-elevated: with --no-tun it never creates a TUN
    // device (no wintun) and uses only userspace sockets, so it needs no admin.
    // Verified 2026-08-21: it starts cleanly at Medium Mandatory Level (binds
    // listeners + RPC, no permission errors). Sensitive params (network name /
    // secret) live in the temp TOML config and are NEVER put on the command line.
    startViaQProcess(exe, args, QByteArray());
}

void EasyTierProcess::stop()
{
    qCInfo(logNet) << QStringLiteral("[EasyTier] 停止进程");
    m_outputTimer->stop();
    m_timeoutTimer->stop();
    // Reset cached NAT so a fresh session starts from Unknown (peer gone → difficulty falls back)
    if (m_currentNatType != EasyTierNatType::Unknown) {
        m_currentNatType = EasyTierNatType::Unknown;
        emit localNatTypeChanged(static_cast<int>(EasyTierNatType::Unknown));
    }

    if (m_cliProcess) {
        m_cliProcess->disconnect();
        if (m_cliProcess->state() == QProcess::Running)
            m_cliProcess->kill();
        m_cliProcess->deleteLater();
        m_cliProcess = nullptr;
    }

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

bool EasyTierProcess::setTcpWhitelist(const QList<quint16>& tcpPorts)
{
    QString cliExe = findEasyTierCli();
    if (cliExe.isEmpty()) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 找不到 easytier-cli.exe，无法更新端口白名单");
        return false;
    }

    // Build "80,443,8000-9000" style list
    QStringList portStrs;
    for (quint16 p : tcpPorts) {
        if (p > 0)
            portStrs << QString::number(p);
    }
    if (portStrs.isEmpty())
        return false;

    QStringList args;
    if (m_rpcPort > 0)
        args << QStringLiteral("-p") << QStringLiteral("127.0.0.1:%1").arg(m_rpcPort);
    args << QStringLiteral("whitelist") << QStringLiteral("set-tcp")
         << portStrs.join(QLatin1Char(','));

    qCInfo(logNet) << QStringLiteral("[EasyTier] 更新TCP端口白名单 %1").arg(portStrs.join(QLatin1Char(',')));

    QProcess proc;
    proc.start(cliExe, args);
    if (!proc.waitForFinished(5000)) {
        qCWarning(logNet) << QStringLiteral("[EasyTier] 更新白名单超时");
        return false;
    }

    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    if (!out.isEmpty())
        qCInfo(logNet) << QStringLiteral("[EasyTier] whitelist 输出: %1").arg(out);
    if (!err.isEmpty())
        qCWarning(logNet) << QStringLiteral("[EasyTier] whitelist 错误: %1").arg(err);

    bool ok = (proc.exitCode() == 0);
    if (!ok)
        qCWarning(logNet) << QStringLiteral("[EasyTier] 更新白名单失败 退出码=%1").arg(proc.exitCode());
    return ok;
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

    // All platforms: plain QProcess — --no-tun needs no elevation.
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
    return m_process && m_process->state() == QProcess::Running;
}

void EasyTierProcess::onTimerTick()
{
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

// Parse a table cell / string into a NAT type (specific patterns checked before generic ones)
EasyTierNatType EasyTierProcess::parseNatCell(const QString& cell)
{
    const QString s = cell.trimmed();
    if (s.isEmpty())
        return EasyTierNatType::Unknown;

    if (s.contains(QStringLiteral("OpenInternet")))       return EasyTierNatType::OpenInternet;
    if (s.contains(QStringLiteral("SymmetricEasyInc")) ||
        s.contains(QStringLiteral("SymmetricEasyIncrease"))) return EasyTierNatType::SymmetricEasyIncrease;
    if (s.contains(QStringLiteral("SymmetricEasyDec")) ||
        s.contains(QStringLiteral("SymmetricEasyDecrease"))) return EasyTierNatType::SymmetricEasyDecrease;
    if (s.contains(QStringLiteral("SymUdpFirewall")) ||
        s.contains(QStringLiteral("SymUdpWall")) ||
        s.contains(QStringLiteral("SymmetricUdpWall")))     return EasyTierNatType::SymmetricUdpWall;
    if (s.contains(QStringLiteral("Symmetric")))            return EasyTierNatType::Symmetric;
    if (s.contains(QStringLiteral("PortRestricted")) ||
        s.contains(QStringLiteral("Port Restricted")))      return EasyTierNatType::PortRestricted;
    if (s.contains(QStringLiteral("Restricted")))           return EasyTierNatType::Restricted;
    if (s.contains(QStringLiteral("FullCone")) ||
        s.contains(QStringLiteral("Full Cone")))             return EasyTierNatType::FullCone;
    if (s.contains(QStringLiteral("NoPat")) ||
        s.contains(QStringLiteral("No Pat")) ||
        s.contains(QStringLiteral("NoPAT")))                 return EasyTierNatType::NoPAT;
    return EasyTierNatType::Unknown;
}

void EasyTierProcess::parsePeerList(const QString& output)
{
    // ── NAT extraction: find the LOCAL peer row and store its NAT type (not just log it) ──
    // Peer table (easytier-cli 2.6.x) is pipe-separated:
    //   | ipv4 | hostname | cost | lat(ms) | loss | rx | tx | tunnel | NAT | version |
    // Scan all cells of each row so column order doesn't matter.
    // Prefer the row whose IP matches our virtual IP; fall back to first cost="Local" row.
    {
        static QRegularExpression ipCellRx(QStringLiteral(R"((\d+\.\d+\.\d+\.\d+))"));

        EasyTierNatType localNat = EasyTierNatType::Unknown;
        EasyTierNatType firstLocalRowNat = EasyTierNatType::Unknown;
        bool foundSelf = false;
        bool foundAnyNatRow = false;

        const QStringList lines = output.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (!line.contains(QLatin1Char('|')))
                continue;
            QStringList cells;
            for (const QString& c : line.split(QLatin1Char('|'))) {
                const QString t = c.trimmed();
                if (!t.isEmpty())
                    cells << t;
            }
            // Skip header/separator rows
            if (cells.size() < 3)
                continue;
            if (cells.contains(QStringLiteral("No")) || cells.contains(QStringLiteral("IP")))
                continue;  // header

            bool isLocalRow = cells.contains(QStringLiteral("Local"));
            QString rowIp;
            for (const QString& cell : cells) {
                auto m = ipCellRx.match(cell);
                if (m.hasMatch()) {
                    rowIp = m.captured(1);
                    break;
                }
            }

            EasyTierNatType rowNat = EasyTierNatType::Unknown;
            for (const QString& cell : cells) {
                EasyTierNatType t = parseNatCell(cell);
                if (t != EasyTierNatType::Unknown) {
                    rowNat = t;
                    break;  // one NAT cell per row
                }
            }
            if (rowNat == EasyTierNatType::Unknown)
                continue;
            foundAnyNatRow = true;

            // Our own row: IP matches the virtual IP we already track
            if (!m_virtualIp.isEmpty() && rowIp == m_virtualIp) {
                localNat = rowNat;
                foundSelf = true;
                break;
            }
            // Fallback candidate: first cost="Local" row
            if (isLocalRow && firstLocalRowNat == EasyTierNatType::Unknown)
                firstLocalRowNat = rowNat;
        }

        if (!foundSelf)
            localNat = firstLocalRowNat;

        // Peer table no longer has any NAT rows (core gone / network dropped) → reset to Unknown
        if (!foundAnyNatRow && m_currentNatType != EasyTierNatType::Unknown) {
            m_currentNatType = EasyTierNatType::Unknown;
            qCInfo(logNet) << QStringLiteral("[EasyTier] 对等列表消失 本地NAT重置为Unknown");
            emit localNatTypeChanged(static_cast<int>(EasyTierNatType::Unknown));
        } else if (localNat != EasyTierNatType::Unknown && localNat != m_currentNatType) {
            m_currentNatType = localNat;
            qCInfo(logNet) << QStringLiteral("[EasyTier] 本地NAT类型更新: %1").arg(static_cast<int>(m_currentNatType));
            emit localNatTypeChanged(static_cast<int>(m_currentNatType));
        }
    }

    // Always update virtual IP from peer table (covers both initial and DHCP-assigned).
    // CRITICAL: only the cost=Local row is OUR OWN ip — matching any row can
    // grab the host's virtual IP (e.g. 10.144.144.1) and corrupt m_virtualIp/
    // m_centerIp, breaking guest discovery of Terracotta hosts.
    QString selfIp;
    {
        const QStringList lines = output.split(QLatin1Char('\n'));
        for (const QString& line : lines) {
            if (!line.contains(QLatin1String("Local")))
                continue;
            QStringList cells;
            for (const QString& c : line.split(QLatin1Char('|'))) {
                const QString t = c.trimmed();
                if (!t.isEmpty())
                    cells << t;
            }
            for (const QString& cell : cells) {
                QRegularExpression re(QStringLiteral(R"(\d+\.\d+\.\d+\.\d+)"));
                auto m = re.match(cell);
                if (m.hasMatch()) {
                    selfIp = m.captured(0);
                    break;
                }
            }
            if (!selfIp.isEmpty())
                break;
        }
    }
    if (!selfIp.isEmpty() && selfIp != m_virtualIp) {
        m_virtualIp = selfIp;
        qCInfo(logNet) << QStringLiteral("[EasyTier] 虚拟IP已获取 ip=%1").arg(m_virtualIp);
        emit virtualIpChanged(m_virtualIp);
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
