#include "launch_backend.h"
#include "account_backend.h"
#include "../core/launcher.h"
#include "../core/crash_detector.h"
#include "../utils/logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <psapi.h>
#endif

namespace ShadowLauncher {

// ============================================================
// Constructor / Destructor
// ============================================================

LaunchBackend::LaunchBackend(QObject* parent)
    : QObject(parent)
{
    qCInfo(logLaunch) << "LaunchBackend constructed";
}

LaunchBackend::~LaunchBackend()
{
    // Clean up all running launchers
    killGameProcess();
}

// ============================================================
// Configuration
// ============================================================

void LaunchBackend::setGameDir(const QString& dir)
{
    m_gameDir = dir;
}

// ============================================================
// Slot: launch
// ============================================================

void LaunchBackend::launch(const QString& versionId, const QString& username,
                           const QString& javaPath, int maxMemoryMB,
                           const QString& jvmArgs)
{
    if (m_launching) {
        emit logMessage(QStringLiteral("已在启动中"));
        return;
    }

    m_launching = true;
    m_cancelled = false;
    m_launchProgress = 0;

    m_launchStatus = QStringLiteral("正在准备...");
    emit launchStateChanged();
    qCInfo(logLaunch) << "Launch requested:" << versionId
                      << "java:" << javaPath
                      << "memory:" << maxMemoryMB << "MB"
                      << "jvmArgs:" << jvmArgs;
    emit logMessage(QStringLiteral("启动 %1 | %2 | JVM: %3").arg(versionId, username,
                      jvmArgs.isEmpty() ? QStringLiteral("默认G1GC") : jvmArgs));

    // ============================================================
    // Async pre-launch checks (stepped with QTimer)
    // ============================================================

    // Save parameters for async step processing
    m_pendingVersionId = versionId;
    m_pendingJavaPath = javaPath;
    m_pendingMaxMemory = maxMemoryMB;
    m_pendingJvmArgs = jvmArgs;
    m_checkStep = 0;

    if (!m_checkTimer) {
        m_checkTimer = new QTimer(this);
        m_checkTimer->setInterval(300);  // 300ms per step for visible progress
        connect(m_checkTimer, &QTimer::timeout, this, &LaunchBackend::runNextCheck);
    }
    m_checkTimer->start();
}

// ============================================================
// Slot: cancelLaunch
// ============================================================

void LaunchBackend::cancelLaunch()
{
    qCDebug(logLaunch) << "[PROCESS] cancelLaunch() called";
    m_cancelled = true;
    if (m_checkTimer) m_checkTimer->stop();

    // If a game process was already started, kill it
    if (m_activeLauncher) {
        m_activeLauncher->killProcess();
        if (m_runningLaunchers.contains(m_activeLauncher)) {
            m_runningLaunchers.removeOne(m_activeLauncher);
            emit runningCountChanged();
            if (m_runningLaunchers.isEmpty()) emit isRunningChanged();
        }
        m_activeLauncher->deleteLater();
        m_activeLauncher = nullptr;
        emit logMessage(QStringLiteral("用户取消启动，已结束游戏进程"));
    } else {
        emit logMessage(QStringLiteral("用户取消了启动"));
    }

    m_launching = false;
    m_launchProgress = 0;
    m_launchStatus.clear();
    emit launchProgressChanged(0, QString());
    emit launchStateChanged();
}

// ============================================================
// Slot: killGameProcess
// ============================================================

void LaunchBackend::killGameProcess()
{
    qCDebug(logLaunch) << "[PROCESS] killGameProcess() called —" << m_runningLaunchers.size() << "games to kill";
    for (Launcher* launcher : m_runningLaunchers) {
        launcher->killProcess();  // Kill immediately with taskkill /F /T
        launcher->deleteLater();
    }
    m_runningLaunchers.clear();

    m_launching = false;
    m_launchProgress = 0;
    m_launchStatus.clear();
    emit launchProgressChanged(0, QString());
    emit runningCountChanged();
    emit isRunningChanged();
    emit logMessage(QStringLiteral("已强制结束所有游戏进程"));
}

void LaunchBackend::setAuthInfo(const QString& username, const QString& uuid,
                           const QString& accessToken, bool isOnline)
{
    m_authName = username;
    m_authUuid = uuid;
    m_authToken = accessToken;
    m_authIsOnline = isOnline;
}

// ============================================================
// Slot: getAutoMemory
// ============================================================

int LaunchBackend::getAutoMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        // Recommend 50% of available memory, min 512MB, max 16GB
        auto availableMB = static_cast<int>(memStatus.ullAvailPhys / (1024 * 1024));
        int recommended = availableMB / 2;
        // Cap at 80% of total physical (for 32-bit Java: ~2GB hard limit)
        auto totalMB = static_cast<int>(memStatus.ullTotalPhys / (1024 * 1024));
        recommended = qMin(recommended, static_cast<int>(totalMB * 0.8));
        return qBound(512, recommended, 16384);
    }
#endif
    return 2048; // fallback: 2GB
}

// ============================================================
// Slot: getSystemMemory
// ============================================================

int LaunchBackend::getSystemMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return static_cast<int>(memStatus.ullTotalPhys / (1024 * 1024));
    }
#endif
    return 4096;
}

// ============================================================
// Slot: getMemoryStatus
// ============================================================

QVariantMap LaunchBackend::getMemoryStatus()
{
    QVariantMap status;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        auto totalMB = static_cast<int>(memStatus.ullTotalPhys / (1024 * 1024));
        auto availMB = static_cast<int>(memStatus.ullAvailPhys / (1024 * 1024));
        int usedMB = totalMB - availMB;
        int usagePercent = totalMB > 0 ? (usedMB * 100 / totalMB) : 0;

        status[QStringLiteral("totalMB")] = totalMB;
        status[QStringLiteral("availMB")] = availMB;
        status[QStringLiteral("usedMB")] = usedMB;
        status[QStringLiteral("usagePercent")] = usagePercent;
        status[QStringLiteral("recommendedMB")] = getAutoMemory();
    }
#endif
    return status;
}

// ============================================================
// Async Pre-launch Check Steps
// ============================================================

void LaunchBackend::abortCheck(const QString& phase, const QString& reason)
{
    qCDebug(logLaunch) << "[PROGRESS] ABORT: " << phase << "—" << reason;
    m_launching = false;
    m_activeLauncher = nullptr;
    if (m_checkTimer) {
        m_checkTimer->stop();
    }
    emit launchProgressChanged(0, reason);
    emit launchCheckFailed(phase, reason);
    emit launchStateChanged();
    qCCritical(logLaunch) << "Pre-launch check FAILED:" << phase << "—" << reason;
    emit logMessage(QStringLiteral("启动失败: %1").arg(reason));
}

void LaunchBackend::runNextCheck()
{
    if (m_cancelled) {
        m_checkTimer->stop();
        return;
    }

    switch (m_checkStep) {
    case 0: {
        // Step 0 (5%): Login status
        emit launchCheckProgress(QStringLiteral("检查登录状态..."));
        m_launchProgress = 5;
        emit launchProgressChanged(5, QStringLiteral("检查登录状态..."));
        qCDebug(logLaunch) << "[PROGRESS] 5% - 检查登录状态...";
        if (m_authName.isEmpty()) {
            abortCheck(QStringLiteral("登录状态"),
                       QStringLiteral("请先登录后再启动游戏"));
            return;
        }
        // For online auth: refresh the Minecraft token before launching
        if (m_authIsOnline && m_account) {
            qCInfo(logLaunch) << "Pre-launch check: refreshing online token...";
            // Check if account has a refresh token; if not, skip refresh and proceed
            if (m_account->msRefreshToken().isEmpty()) {
                qCInfo(logLaunch) << "No refresh token available, skipping refresh";
            } else {
                m_checkTimer->stop();  // Pause until refresh completes
                // Timeout guard: resume after 12s even if refresh never responds
                QTimer::singleShot(12000, this, [this]() {
                    if (m_checkTimer && !m_checkTimer->isActive()) {
                        qCWarning(logLaunch) << "Token refresh timed out, proceeding anyway";
                        m_checkStep++;
                        m_checkTimer->start();
                    }
                });
                // Temporary connection: resume on refresh result
                QMetaObject::Connection* conn = new QMetaObject::Connection();
                *conn = connect(m_account, &AccountBackend::tokenRefreshed, this,
                    [this, conn](bool ok) {
                        disconnect(*conn);
                        delete conn;
                        if (ok) {
                            qCInfo(logLaunch) << "Pre-launch check passed: token refreshed";
                            m_authToken = m_account->mcToken();
                        } else {
                            qCWarning(logLaunch) << "Token refresh failed, proceeding with existing token";
                        }
                        // Either way, continue the check
                        if (m_checkTimer && !m_checkTimer->isActive()) {
                            m_checkStep++;
                            m_checkTimer->start();
                        }
                    });
                m_account->refreshMicrosoftToken();
                return;  // Don't advance; wait for callback
            }
        }
        qCInfo(logLaunch) << "Pre-launch check passed: login status";
        break;
    }
    case 1: {
        // Step 1 (10%): Java environment
        emit launchCheckProgress(QStringLiteral("检查 Java 环境..."));
        m_launchProgress = 10;
        emit launchProgressChanged(10, QStringLiteral("检查 Java 环境..."));
        qCDebug(logLaunch) << "[PROGRESS] 10% - 检查 Java 环境...";
        if (!QFileInfo::exists(m_pendingJavaPath)) {
            abortCheck(QStringLiteral("Java 可执行文件"),
                       QStringLiteral("路径: %1").arg(m_pendingJavaPath));
            return;
        }
        QString arch = checkJavaArchitecture(m_pendingJavaPath);
        if (arch == QStringLiteral("32")) {
            if (m_pendingMaxMemory > 1536) {
                m_pendingMaxMemory = 1536;
                emit launchCheckWarning(QStringLiteral("检测到 32 位 Java，已限制内存 1536MB"));
            }
        }
        qCInfo(logLaunch) << "Pre-launch check passed: Java executable";
        break;
    }
    case 2: {
        // Step 2 (30%): Version core files
        emit launchCheckProgress(QStringLiteral("检查版本文件..."));
        m_launchProgress = 30;
        emit launchProgressChanged(30, QStringLiteral("检查版本文件..."));
        qCDebug(logLaunch) << "[PROGRESS] 30% - 检查版本文件...";
        QString versionDir = m_gameDir + QStringLiteral("/versions/") + m_pendingVersionId;
        if (!QDir(versionDir).exists()) {
            abortCheck(QStringLiteral("版本目录"), QStringLiteral("目录不存在")); return;
        }
        QString jarPath = versionDir + QStringLiteral("/") + m_pendingVersionId + QStringLiteral(".jar");
        if (!QFileInfo::exists(jarPath)) {
            abortCheck(QStringLiteral("核心 Jar"), QStringLiteral("文件不存在")); return;
        }
        QString jsonPath = versionDir + QStringLiteral("/") + m_pendingVersionId + QStringLiteral(".json");
        QFile jsonFile(jsonPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            abortCheck(QStringLiteral("版本配置"), QStringLiteral("配置文件缺失")); return;
        }
        QByteArray jsonBytes = jsonFile.readAll();
        jsonFile.close();
        QJsonParseError parseErr;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonBytes, &parseErr);
        if (parseErr.error != QJsonParseError::NoError)
            emit launchCheckWarning(QStringLiteral("JSON 格式警告"));
        // Verify JSON's id matches the version folder name (catches corrupted install from naming bugs)
        if (jsonDoc.isObject()) {
            QString jsonId = jsonDoc.object().value(QStringLiteral("id")).toString();
            if (!jsonId.isEmpty() && jsonId != m_pendingVersionId) {
                abortCheck(QStringLiteral("版本 JSON ID 不匹配"),
                    QStringLiteral("版本名 \"%1\" 与配置内 id \"%2\" 不一致。建议重新安装此版本。")
                        .arg(m_pendingVersionId, jsonId));
                return;
            }
        }
        qCInfo(logLaunch) << "Pre-launch check passed: version directory and JAR exist";
        break;
    }
    case 3: {
        // Step 3 (50%): Dependencies
        emit launchCheckProgress(QStringLiteral("检查依赖文件..."));
        m_launchProgress = 50;
        emit launchProgressChanged(50, QStringLiteral("检查依赖文件..."));
        qCDebug(logLaunch) << "[PROGRESS] 50% - 检查依赖文件...";
        QStringList missingLibs = checkVersionLibraries(m_pendingVersionId);
        if (!missingLibs.isEmpty()) {
            QStringList d = missingLibs.mid(0, 5);
            QString detail = d.join(QStringLiteral(", "));
            if (missingLibs.size() > 5) detail += QStringLiteral(" ...共%1个").arg(missingLibs.size());
            abortCheck(QStringLiteral("依赖库文件"), QStringLiteral("缺少 %1 个").arg(missingLibs.size())); return;
        }
        QStringList missingNatives = checkVersionMissingNatives(m_pendingVersionId);
        if (!missingNatives.isEmpty()) {
            QStringList d = missingNatives.mid(0, 5);
            QString detail = d.join(QStringLiteral(", "));
            if (missingNatives.size() > 5) detail += QStringLiteral(" ...共%1个").arg(missingNatives.size());
            abortCheck(QStringLiteral("运行库文件"), QStringLiteral("缺少 %1 个").arg(missingNatives.size())); return;
        }
        qCInfo(logLaunch) << "Pre-launch check passed: all library files present";
        break;
    }
    case 4: {
        // Step 4 (65%): Memory
        emit launchCheckProgress(QStringLiteral("检查内存分配..."));
        m_launchProgress = 65;
        emit launchProgressChanged(65, QStringLiteral("检查内存分配..."));
        qCDebug(logLaunch) << "[PROGRESS] 65% - 检查内存分配...";
        if (m_pendingMaxMemory < 512)
            emit launchCheckWarning(QStringLiteral("内存不足 512MB，可能影响运行"));
        qCInfo(logLaunch) << "Pre-launch checks all passed — starting Minecraft";
        break;
    }
    case 5: {
        // Step 5 (75%): Launch
        m_checkTimer->stop();
        emit launchCheckProgress(QStringLiteral("正在启动..."));
        m_launchProgress = 75;
        emit launchProgressChanged(75, QStringLiteral("正在启动 Minecraft..."));
        qCDebug(logLaunch) << "[PROGRESS] 75% - 正在启动 Minecraft...";
        Launcher* launcher = new Launcher(this);
        launcher->setGameDir(m_gameDir);
        launcher->setAuthInfo(m_authName, m_authUuid, m_authToken, m_authIsOnline);
        launcher->setProperty("launchVersion", m_pendingVersionId);
        // Connect signals
        m_activeLauncher = launcher;  // only this launcher's progress feeds the overlay
        connect(launcher, &Launcher::launchProgress, this, &LaunchBackend::onLaunchProgress);
        connect(launcher, &Launcher::launchStarted, this, [this, launcher]() { handleLaunchStarted(launcher); });
        connect(launcher, &Launcher::launchFinished, this, [this, launcher](bool ok, const QString& err) { handleLaunchFinished(launcher, ok, err); });
        launcher->start(m_pendingVersionId, m_pendingJavaPath, m_pendingMaxMemory,
                        m_pendingJvmArgs);
        return;
    }
    default:
        m_checkTimer->stop();
        return;
    }

    m_checkStep++;
}


// ============================================================
// Private Slots
// ============================================================

void LaunchBackend::handleLaunchStarted(Launcher* launcher)
{
    // Add to running list
    m_runningLaunchers.append(launcher);
    qCDebug(logLaunch) << "[PROCESS] Game added to running list (total:" << m_runningLaunchers.size() << ")";
    emit runningCountChanged();
    emit isRunningChanged();

    // Phase 3: Process started, wait for window
    m_launchProgress = 80;
    m_launchStatus = QStringLiteral("进程已启动，等待窗口...");
    emit launchProgressChanged(80, m_launchStatus);
    qCDebug(logLaunch) << "[PROGRESS] 80% - 进程已启动，等待窗口...";
    emit minecraftStarted();

    // After 3s, check if process still alive
    QTimer::singleShot(3000, this, [this, launcher]() {
        if (!launcher->isRunning()) {
            emit launchProgressChanged(0, QStringLiteral("游戏进程意外退出"));
            emit launchCheckFailed(QStringLiteral("进程存活"), QStringLiteral("游戏进程在窗口出现前退出"));
            emit logMessage(QStringLiteral("启动失败: 进程意外退出"));
            // Remove from list (guard: may have been removed by killGameByPid)
            if (m_runningLaunchers.contains(launcher)) {
                m_runningLaunchers.removeOne(launcher);
                launcher->deleteLater();
                emit runningCountChanged();
                if (m_runningLaunchers.isEmpty()) emit isRunningChanged();
                emit minecraftStopped();
                qCDebug(logLaunch) << "[PROCESS] Death check: game process died, minecraftStopped emitted";
            } else {
                qCDebug(logLaunch) << "[PROCESS] Death check skipped (already removed by kill)";
            }
            m_launching = false;
            emit launchStateChanged();
            return;
        }
        // Phase 4: Window ready
        m_launchProgress = 100;
        m_launchStatus = QStringLiteral("启动完成");
        emit launchProgressChanged(100, m_launchStatus);
        qCDebug(logLaunch) << "[PROGRESS] 100% - 启动完成 (窗口就绪)";
        qCInfo(logLaunch) << "Minecraft launch complete";
        emit logMessage(QStringLiteral("Minecraft 启动完成"));
        m_activeLauncher = nullptr;  // release progress isolation
        // Delay m_launching=false until after overlay animation
        QTimer::singleShot(2200, this, [this]() {
            m_launching = false;
            qCDebug(logLaunch) << "[STATE] m_launching = false (after overlay animation)";
            emit launchStateChanged();
        });
    });
}

void LaunchBackend::onLaunchProgress(const QString& message)
{
    // Only accept progress from the currently-launching launcher
    if (sender() != m_activeLauncher) {
        return;
    }

    // Once Minecraft process has started, freeze progress at 100%
    if (m_launchProgress >= 95) {
        emit logMessage(message);
        return;
    }

    // Before Minecraft starts, only show our curated status messages
    // Raw Minecraft log lines from Launcher::launchProgress are logged but not shown
    if (m_launchProgress >= 50) {
        // Post-check phase: Minecraft is starting, don't show raw log
        emit logMessage(message);
        return;
    }

    m_launchProgress = qMin(m_launchProgress + 5, 50);
    m_launchStatus = message;
    emit launchProgressChanged(m_launchProgress, message);
    emit logMessage(message);
}

void LaunchBackend::handleLaunchFinished(Launcher* launcher, bool success, const QString& errorMsg)
{
    // Guard: launcher may have been removed by killGameByPid already
    if (!m_runningLaunchers.contains(launcher)) {
        qCDebug(logLaunch) << "[PROCESS] handleLaunchFinished skipped (already removed by kill)";
        return;
    }

    // Remove from running list
    m_runningLaunchers.removeOne(launcher);
    qCDebug(logLaunch) << "[PROCESS] Game removed from running list (total:" << m_runningLaunchers.size() << ") success=" << success;
    launcher->deleteLater();
    emit runningCountChanged();
    if (m_runningLaunchers.isEmpty()) emit isRunningChanged();

    // Always emit stopped for UI to react
    emit minecraftStopped();
    qCDebug(logLaunch) << "[PROCESS] minecraftStopped emitted";

    if (!m_launching) return;  // Already handled in handleLaunchStarted death check

    m_launching = false;
    if (success) {
        m_launchProgress = 100;
        m_launchStatus = QStringLiteral("启动完成");
        emit launchProgressChanged(100, QStringLiteral("启动完成"));
        qCInfo(logLaunch) << "Minecraft launch succeeded";
        emit logMessage(QStringLiteral("Minecraft 启动成功"));
    } else {
        m_launchProgress = 0;
        m_launchStatus.clear();
        emit launchProgressChanged(0, errorMsg);
        qCCritical(logLaunch) << "Minecraft launch failed:" << errorMsg;
        emit logMessage(QStringLiteral("启动失败: %1").arg(errorMsg));

        // ── Crash detection: scan for crash reports ──
        CrashDetector detector;
        CrashReport cr = detector.scanLatestCrash(m_gameDir);
        if (cr.isValid) {
            QVariantMap report;
            report[QStringLiteral("type")] = cr.type;
            report[QStringLiteral("reason")] = cr.reason;
            report[QStringLiteral("description")] = cr.description;
            report[QStringLiteral("suspectedMods")] = cr.suspectedMods;
            report[QStringLiteral("filePath")] = cr.filePath;
            report[QStringLiteral("timestamp")] = cr.timestamp;
            report[QStringLiteral("isValid")] = true;
            qCDebug(logLaunch) << "[CRASH] emitting crashDetected" << cr.type << cr.reason;
            emit crashDetected(report);
        }
    }
    emit launchStateChanged();
}

// ============================================================
// Pre-launch check: Java architecture (32/64 bit)
// ============================================================

QString LaunchBackend::checkJavaArchitecture(const QString& javaPath)
{
#ifdef Q_OS_WIN
    QFile f(javaPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit logMessage(QStringLiteral("无法读取 Java 可执行文件以检测架构"));
        return QStringLiteral("64");  // assume 64-bit as safe default
    }

    // Read PE header
    // DOS header: offset 0x3C contains e_lfanew (PE signature offset)
    if (!f.seek(0x3C)) {
        f.close();
        return QStringLiteral("64");
    }

    QByteArray peOffsetData = f.read(4);
    if (peOffsetData.size() < 4) {
        f.close();
        return QStringLiteral("64");
    }

    // e_lfanew is little-endian DWORD
    quint32 peOffset = static_cast<quint32>(
        static_cast<unsigned char>(peOffsetData[0]) |
        (static_cast<unsigned char>(peOffsetData[1]) << 8) |
        (static_cast<unsigned char>(peOffsetData[2]) << 16) |
        (static_cast<unsigned char>(peOffsetData[3]) << 24));

    // Seek to PE signature and verify "PE\0\0"
    if (!f.seek(peOffset)) {
        f.close();
        return QStringLiteral("64");
    }

    QByteArray peSig = f.read(4);
    if (peSig.size() < 4 || peSig[0] != 'P' || peSig[1] != 'E') {
        f.close();
        emit logMessage(QStringLiteral("无法识别 PE 签名，假设为 64-bit"));
        return QStringLiteral("64");
    }

    // COFF header (20 bytes after PE signature):
    // offset 0: Machine (2 bytes)
    QByteArray coffHeader = f.read(20);
    f.close();

    if (coffHeader.size() < 2) return QStringLiteral("64");

    quint16 machine = static_cast<quint16>(
        static_cast<unsigned char>(coffHeader[0]) |
        (static_cast<unsigned char>(coffHeader[1]) << 8));

    // 0x014C = x86 (32-bit), 0x8664 = x64 (64-bit)
    if (machine == 0x014C) {
        emit logMessage(QStringLiteral("检测到 32 位 Java (x86)"));
        return QStringLiteral("32");
    } else if (machine == 0x8664) {
        emit logMessage(QStringLiteral("检测到 64 位 Java (x64)"));
        return QStringLiteral("64");
    } else if (machine == 0xAA64) {
        emit logMessage(QStringLiteral("检测到 ARM64 Java"));
        return QStringLiteral("64");
    }

    emit logMessage(QStringLiteral("未知架构 Machine=0x%1，假设为 64-bit")
                        .arg(machine, 4, 16, QLatin1Char('0')));
    return QStringLiteral("64");
#else
    Q_UNUSED(javaPath)
    return QStringLiteral("64");
#endif
}

// ============================================================
// Pre-launch check: Library file completeness
// ============================================================

// Helper: resolve all libraries including inherited versions (Forge/NeoForge inheritsFrom chain)
static QJsonArray resolveMergedLibraries(const QString& gameDir, const QJsonObject& topJson) {
    QJsonArray all = topJson.value(QStringLiteral("libraries")).toArray();
    QString inherits = topJson.value(QStringLiteral("inheritsFrom")).toString();
    QStringList seen;
    while (!inherits.isEmpty() && !seen.contains(inherits)) {
        seen.append(inherits);
        QFile f(gameDir + QStringLiteral("/versions/") + inherits + QStringLiteral("/") + inherits + QStringLiteral(".json"));
        if (!f.open(QIODevice::ReadOnly)) break;
        QJsonObject parent = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        if (parent.isEmpty()) break;
        for (const auto& lib : parent.value(QStringLiteral("libraries")).toArray())
            all.append(lib);
        inherits = parent.value(QStringLiteral("inheritsFrom")).toString();
    }
    return all;
}

QStringList LaunchBackend::checkVersionLibraries(const QString& versionId)
{
    QStringList missing;

    const QString gameDir = m_gameDir;
    const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    const QString libsDir = gameDir + QStringLiteral("/libraries");

    // Read version JSON
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit logMessage(QStringLiteral("⚠ 无法读取版本 JSON 以检查库文件"));
        return missing;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(QStringLiteral("⚠ 版本 JSON 解析失败，无法检查库文件"));
        return missing;
    }

    QJsonObject versionJson = doc.object();
    QJsonArray libraries = resolveMergedLibraries(gameDir, versionJson);

    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();

        // Skip platform-specific rules (client-side only)
        QJsonArray rules = lib.value(QStringLiteral("rules")).toArray();
        bool skip = false;
        for (const QJsonValue& ruleVal : rules) {
            QJsonObject rule = ruleVal.toObject();
            QString action = rule.value(QStringLiteral("action")).toString();
            QJsonObject os = rule.value(QStringLiteral("os")).toObject();
            QString osName = os.value(QStringLiteral("name")).toString();
            // If rule says "allow" for non-windows or "disallow" for windows
            if (!osName.isEmpty()) {
                bool isWindows = osName.contains(QStringLiteral("windows"), Qt::CaseInsensitive);
                if (action == QStringLiteral("allow") && !isWindows) {
                    skip = true;
                    break;
                }
                if (action == QStringLiteral("disallow") && isWindows) {
                    skip = true;
                    break;
                }
            }
        }
        if (skip) continue;

        QJsonObject downloads = lib.value(QStringLiteral("downloads")).toObject();

        // Check main artifact
        QJsonObject artifact = downloads.value(QStringLiteral("artifact")).toObject();
        if (!artifact.isEmpty()) {
            QString path = artifact.value(QStringLiteral("path")).toString();
            QString fullPath = libsDir + QStringLiteral("/") + path;
            if (!QFileInfo::exists(fullPath)) {
                missing.append(path);
            }
        }

        // Check native classifiers (Windows)
        QJsonObject classifiers = downloads.value(QStringLiteral("classifiers")).toObject();
        for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
            QString key = it.key().toLower();
            if (key.contains(QStringLiteral("natives-windows")) ||
                key.contains(QStringLiteral("natives-windows"))) {
                QJsonObject clsArt = it.value().toObject();
                QString path = clsArt.value(QStringLiteral("path")).toString();
                if (!path.isEmpty()) {
                    QString fullPath = libsDir + QStringLiteral("/") + path;
                    if (!QFileInfo::exists(fullPath)) {
                        missing.append(path);
                    }
                }
            }
        }
    }

    if (!missing.isEmpty()) {
        qCWarning(logLaunch) << "Missing" << missing.size() << "library files for" << versionId;
        emit logMessage(QStringLiteral("⚠ 缺少 %1 个依赖库文件").arg(missing.size()));
    } else {
        qCInfo(logLaunch) << "All library files present for" << versionId;
        emit logMessage(QStringLiteral("✅ 所有依赖库文件完整"));
    }

    return missing;
}

// ============================================================
// Pre-launch check: Natives presence
// ============================================================

bool LaunchBackend::checkVersionHasNatives(const QString& versionId)
{
    return checkVersionMissingNatives(versionId).isEmpty();
}

QStringList LaunchBackend::checkVersionMissingNatives(const QString& versionId)
{
    QStringList missing;

    const QString gameDir = m_gameDir;
    const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");

    // First, check if extracted natives exist
    QString primaryNatives = versionDir + QStringLiteral("/") + versionId + QStringLiteral("-natives");
    QString fallbackNatives = versionDir + QStringLiteral("/natives");
    bool hasExtractedNatives = false;

    if (QDir(primaryNatives).exists()) {
        QDirIterator it(primaryNatives, QStringList() << QStringLiteral("*.dll"), QDir::Files);
        if (it.hasNext()) {
            emit logMessage(QStringLiteral("找到运行库: %1").arg(primaryNatives));
            hasExtractedNatives = true;
        }
    }
    if (!hasExtractedNatives && QDir(fallbackNatives).exists()) {
        QDirIterator it(fallbackNatives, QStringList() << QStringLiteral("*.dll"), QDir::Files);
        if (it.hasNext()) {
            emit logMessage(QStringLiteral("找到运行库: %1").arg(fallbackNatives));
            hasExtractedNatives = true;
        }
    }

    // If extracted natives exist, we're good
    if (hasExtractedNatives) {
        return missing;
    }

    // No extracted natives — check if native library JARs exist and report missing ones
    const QString libsDir = gameDir + QStringLiteral("/libraries");

    // Read version JSON to find native libraries
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit logMessage(QStringLiteral("⚠ 无法读取版本 JSON 以检查原生库"));
        missing.append(QStringLiteral("(无法读取版本配置)"));
        return missing;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit logMessage(QStringLiteral("⚠ 版本 JSON 解析失败"));
        missing.append(QStringLiteral("(版本配置解析失败)"));
        return missing;
    }

    QJsonObject versionJson = doc.object();
    QJsonArray libraries = resolveMergedLibraries(gameDir, versionJson);

    bool foundAnyNative = false;
    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();

        // Skip platform-specific rules
        QJsonArray rules = lib.value(QStringLiteral("rules")).toArray();
        bool skip = false;
        for (const QJsonValue& ruleVal : rules) {
            QJsonObject rule = ruleVal.toObject();
            QString action = rule.value(QStringLiteral("action")).toString();
            QJsonObject os = rule.value(QStringLiteral("os")).toObject();
            QString osName = os.value(QStringLiteral("name")).toString();
            if (!osName.isEmpty()) {
                bool isWindows = osName.contains(QStringLiteral("windows"), Qt::CaseInsensitive);
                if (action == QStringLiteral("allow") && !isWindows) { skip = true; break; }
                if (action == QStringLiteral("disallow") && isWindows) { skip = true; break; }
            }
        }
        if (skip) continue;

        QJsonObject downloads = lib.value(QStringLiteral("downloads")).toObject();
        QJsonObject classifiers = downloads.value(QStringLiteral("classifiers")).toObject();
        for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
            QString key = it.key().toLower();
            if (key.contains(QStringLiteral("natives-windows"))) {
                foundAnyNative = true;
                QJsonObject clsArt = it.value().toObject();
                QString path = clsArt.value(QStringLiteral("path")).toString();
                if (!path.isEmpty()) {
                    QString fullPath = libsDir + QStringLiteral("/") + path;
                    if (!QFileInfo::exists(fullPath)) {
                        missing.append(QStringLiteral("natives/%1").arg(path));
                    }
                }
            }
        }
    }

    if (!foundAnyNative) {
        // No native libraries defined in JSON — this is fine for some versions
        emit logMessage(QStringLiteral("版本未定义原生库 (可能无需原生库)"));
        return missing; // empty = OK
    }

    if (!missing.isEmpty()) {
        emit logMessage(QStringLiteral("⚠ 缺少 %1 个原生库文件").arg(missing.size()));
    } else {
        emit logMessage(QStringLiteral("✅ 所有原生库文件完整"));
    }

    return missing;
}

// ============================================================
// Multi-instance: kill one game by PID
// ============================================================

void LaunchBackend::killGameByPid(qint64 pid)
{
    qCDebug(logLaunch) << "[PROCESS] killGameByPid(" << pid << ") —" << m_runningLaunchers.size() << "games total";
    for (int i = 0; i < m_runningLaunchers.size(); ++i) {
        if (m_runningLaunchers[i]->pid() == pid) {
            Launcher* launcher = m_runningLaunchers.takeAt(i);
            launcher->killProcess();
            launcher->deleteLater();
            emit runningCountChanged();
            if (m_runningLaunchers.isEmpty()) emit isRunningChanged();
            emit logMessage(QStringLiteral("已结束游戏进程 (PID: %1)").arg(pid));
            return;
        }
    }
    qCWarning(logLaunch) << "[PROCESS] killGameByPid: PID" << pid << "not found";
}

// ============================================================
// Multi-instance: list running games
// ============================================================

QVariantList LaunchBackend::runningGames() const
{
    QVariantList list;
    
    // Count instances per version for (1), (2) suffixes
    QMap<QString, int> versionCounts;
    for (int i = 0; i < m_runningLaunchers.size(); ++i) {
        QString ver = m_runningLaunchers[i]->property("launchVersion").toString();
        versionCounts[ver]++;
    }
    
    QMap<QString, int> versionSeen;
    for (int i = 0; i < m_runningLaunchers.size(); ++i) {
        QVariantMap info;
        qint64 pid = m_runningLaunchers[i]->pid();
        info["pid"] = QVariant::fromValue(pid);
        QString ver = m_runningLaunchers[i]->property("launchVersion").toString();
        info["version"] = ver;
        
        if (versionCounts[ver] > 1) {
            versionSeen[ver]++;
            info["displayVersion"] = ver + " (" + QString::number(versionSeen[ver]) + ")";
        } else {
            info["displayVersion"] = ver;
        }
        
        list.append(info);
    }
    return list;
}

} // namespace ShadowLauncher

