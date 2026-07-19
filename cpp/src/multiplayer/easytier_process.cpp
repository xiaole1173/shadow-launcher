// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "easytier_process.h"
#include "relay_crypto.h"    // Decrypted relay endpoint
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
#include "../utils/logger.h"

using namespace ShadowLauncher;

EasyTierProcess::EasyTierProcess(QObject* parent)
    : QObject(parent)
    , m_outputTimer(new QTimer(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_outputTimer->setInterval(500);
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
                            const QString& hostname)
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

    QString relayEp = Relay::relayEndpoint();

    // ── Build TOML config file (all settings, no sensitive data on CLI) ──
    // Remove previous config file
    if (!m_peerConfigPath.isEmpty()) {
        QFile::remove(m_peerConfigPath);
        m_peerConfigPath.clear();
    }
    {
        QTemporaryFile tmpFile(QDir::tempPath() + QStringLiteral("/shadow_easytier_XXXXXX.toml"));
        tmpFile.setAutoRemove(false);
        tmpFile.open();
        m_peerConfigPath = tmpFile.fileName();

        QByteArray toml;
        // Note: TOML peers is left out intentionally — easytier 2.6.4 ignores
        // `peers = [...]` at runtime despite passing --check-config.
        // Peers are passed via --peers CLI arg instead.
        toml.append("[network]\n");
        toml.append(QStringLiteral("name = \"%1\"\n").arg(networkName).toUtf8());
        toml.append(QStringLiteral("secret = \"%1\"\n").arg(networkKey).toUtf8());
        toml.append("dhcp = true\n");
        tmpFile.write(toml);
        tmpFile.close();
        secureWipe(toml);
    }

    // CLI: only non-sensitive info (sensitive params via environment block)
    QStringList args;
    args << "--config-file" << m_peerConfigPath;
    if (!hostname.isEmpty())
        args << "--hostname" << hostname;

    // Wipe sensitive data from local memory before starting process
    secureWipe(relayEp);

#ifdef Q_OS_WIN
    // ── Build command line (no sensitive data — passed via env block) ──
    QString argStr = args.join(QLatin1Char(' '));

    // ── Create elevated process with custom environment block ──
    // ShellExecuteEx(runas) doesn't propagate SetEnvironmentVariableW changes.
    // Instead, get the linked (elevated) token via UAC and use
    // CreateProcessAsUserW with a custom environment block containing ET_* vars.

    // 1. Get current process token
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(),
            TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &hToken)) {
        emit errorOccurred(QStringLiteral("获取进程令牌失败"));
        return;
    }

    // 2. Get the linked (elevated) token via UAC
    TOKEN_LINKED_TOKEN linkedToken = {nullptr};
    DWORD size = 0;
    if (!GetTokenInformation(hToken, TokenLinkedToken,
            &linkedToken, sizeof(linkedToken), &size) || !linkedToken.LinkedToken) {
        CloseHandle(hToken);
        // Fallback: try ShellExecuteEx(runas) with just config file
        // (peers will be on CLI as last resort)
        qCWarning(logNet) << QStringLiteral("[EasyTier] 无法获取提升令牌，回退到ShellExecuteEx");
        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
        sei.lpVerb = L"runas";
        sei.lpFile = reinterpret_cast<const wchar_t*>(exe.utf16());
        sei.lpParameters = reinterpret_cast<const wchar_t*>(argStr.utf16());
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            m_winProcess = sei.hProcess;
            m_ready = false;
            m_outputTimer->start();
            m_timeoutTimer->start();
            emit stateChanged(QStringLiteral("等待 UAC 确认..."));
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED)
                emit errorOccurred(QStringLiteral("需要管理员权限"));
            else
                emit errorOccurred(QStringLiteral("启动 EasyTier 失败 (错误码: %1)").arg(err));
        }
        return;
    }

    // 3. Build custom UNICODE environment block with ET_* vars
    // Format: list of UTF-16LE null-terminated strings, ending with double null.
    QByteArray envBlockRaw;
    {
        // Helper to append a UTF-16LE null-terminated string
        auto appendWide = [&](const QString& s) {
            int cb = (s.length() + 1) * sizeof(QChar);
            envBlockRaw.append(reinterpret_cast<const char*>(s.utf16()), cb);
        };
        // Inherit essential vars so the child can find DLLs
        auto inheritVar = [&](const wchar_t* name) {
            wchar_t buf[4096] = {0};
            DWORD sz = GetEnvironmentVariableW(name, buf, 4096);
            if (sz > 0 && sz < 4096)
                appendWide(QString::fromWCharArray(name) + QStringLiteral("=") + QString::fromWCharArray(buf));
        };
        inheritVar(L"PATH");
        inheritVar(L"SYSTEMROOT");
        inheritVar(L"TMP");
        inheritVar(L"USERPROFILE");
        // Add our ET_* vars
        appendWide(QStringLiteral("ET_NETWORK_NAME=") + networkName);
        appendWide(QStringLiteral("ET_NETWORK_SECRET=") + networkKey);
        appendWide(QStringLiteral("ET_PEERS=") + relayEp);
        appendWide(QStringLiteral("ET_DHCP=true"));
        // 4. Double null terminator (two UTF-16 null chars = 4 zero bytes)
        envBlockRaw.append(QByteArray(4, '\0'));
    }

    // 4. Create the elevated process with custom environment
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {nullptr};

    // Convert args to a writable form (CreateProcessW modifies the buffer)
    std::wstring cmdLine = (exe + QStringLiteral(" ") + argStr).toStdWString();
    std::wstring exePath = exe.toStdWString();

    BOOL created = CreateProcessAsUserW(
        linkedToken.LinkedToken,    // elevated token
        exePath.c_str(),            // application name
        cmdLine.data(),             // command line
        nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT,  // use our UNICODE environment block
        const_cast<char*>(envBlockRaw.constData()),    // custom environment
        nullptr, &si, &pi);

    // Cleanup tokens
    CloseHandle(linkedToken.LinkedToken);
    CloseHandle(hToken);

    if (created) {
        CloseHandle(pi.hThread);
        m_winProcess = pi.hProcess;
        m_ready = false;
        m_outputTimer->start();
        m_timeoutTimer->start();
        qCInfo(logNet) << QStringLiteral("[EasyTier] 已启动 (环境变量方式) PID=%1").arg(pi.dwProcessId);
        emit stateChanged(QStringLiteral("正在创建虚拟网络..."));
    } else {
        DWORD err = GetLastError();
        qCWarning(logNet) << QStringLiteral("[EasyTier] CreateProcessAsUser失败 错误码=%1 回退到ShellExecuteEx").arg(err);
        // Fallback: ShellExecuteEx with --peers on CLI (env vars don't survive UAC)
        QString fallbackArgs = argStr + QStringLiteral(" --peers ") + relayEp;
        SHELLEXECUTEINFOW sei = {sizeof(sei)};
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
        sei.lpVerb = L"runas";
        sei.lpFile = reinterpret_cast<const wchar_t*>(exe.utf16());
        sei.lpParameters = reinterpret_cast<const wchar_t*>(fallbackArgs.utf16());
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei) && sei.hProcess) {
            m_winProcess = sei.hProcess;
            m_ready = false;
            m_outputTimer->start();
            m_timeoutTimer->start();
            emit stateChanged(QStringLiteral("等待 UAC 确认..."));
        } else {
            DWORD err2 = GetLastError();
            if (err2 == ERROR_CANCELLED)
                emit errorOccurred(QStringLiteral("需要管理员权限"));
            else
                emit errorOccurred(QStringLiteral("启动 EasyTier 失败 (错误码: %1)").arg(err2));
        }
    }
#else
    // macOS / Linux: QProcess works fine; config file handles all sensitive params
    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &EasyTierProcess::onProcessStarted);
    connect(m_process, &QProcess::errorOccurred, this, &EasyTierProcess::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &EasyTierProcess::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &EasyTierProcess::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EasyTierProcess::onProcessFinished);

    qCInfo(logNet) << QStringLiteral("[EasyTier] 启动 exe=%1 hostname=%2")
        .arg(exe, hostname.isEmpty() ? QStringLiteral("(默认)") : hostname);
    m_process->start(exe, args);
    m_timeoutTimer->start();
#endif
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
}

void EasyTierProcess::onProcessStarted()
{
    qCInfo(logNet) << QStringLiteral("[EasyTier] 进程已启动");
    m_outputTimer->start();
}

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
            qCInfo(logNet) << QStringLiteral("[EasyTier] CLI输出: %1").arg(output);
            parsePeerList(output);
        });
    }

    qCInfo(logNet) << QStringLiteral("[EasyTier] 轮询peer列表");
    m_cliProcess->start(cliExe, {QStringLiteral("peer")});
}

void EasyTierProcess::parsePeerList(const QString& output)
{
    // Parse peer list for virtual IP
    QRegularExpression ipRx(QStringLiteral(R"(\|?\s*(\d+\.\d+\.\d+\.\d+)(?:/\d+)?\s*\|)"));

    if (m_virtualIp.isEmpty()) {
        auto m = ipRx.match(output);
        if (m.hasMatch()) {
            m_virtualIp = m.captured(1);
            qCInfo(logNet) << QStringLiteral("[EasyTier] 虚拟IP已获取 ip=%1").arg(m_virtualIp);
        }
        // Also compute deterministic IP as fallback
        if (m_virtualIp.isEmpty()) {
            QByteArray hash = QCryptographicHash::hash(
                m_networkName.toUtf8(), QCryptographicHash::Sha256
            );
            m_virtualIp = QStringLiteral("10.%1.%2.%3")
                .arg(static_cast<quint8>(hash[0]))
                .arg(static_cast<quint8>(hash[1]))
                .arg(static_cast<quint8>(hash[2]));
        }
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
    // easytier-cli peer outputs a table with IPs when connected.
    if (!m_ready && !m_virtualIp.isEmpty()) {
        // We have a virtual IP from the peer list → network is up
        bool hasPeer = output.contains(QRegularExpression(QStringLiteral(R"(\|\s*\d+\.\d+\.\d+\.\d+)")));
        if (hasPeer) {
            m_ready = true;
            m_timeoutTimer->stop();
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
    qCInfo(logNet) << QStringLiteral("[EasyTier] stdout: %1").arg(line);

    // EasyTier virtual IP patterns: usually 10.x or 172.x or 192.168.x
    static QRegularExpression ipRx(
        QStringLiteral(R"(\b(10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.\d{1,3}\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3})\b)")
    );

    // Detect virtual IP (skip relay server IP)
    if (!m_ready) {
        auto m = ipRx.match(line);
        if (m.hasMatch()) {
            QString ip = m.captured(1);
            QString prefix = Relay::relayPrefix();
            if (!prefix.isEmpty() && !ip.startsWith(prefix))
                m_virtualIp = ip;
        }
    }

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
