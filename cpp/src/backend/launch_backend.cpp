#include "launch_backend.h"
#include "../utils/logger.h"
#include "../core/launcher.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    , m_launcher(new Launcher(this))
{
    qCInfo(logLaunch) << "LaunchBackend constructed";

    connect(m_launcher, &Launcher::launchStarted,
            this, &LaunchBackend::onLaunchStarted);
    connect(m_launcher, &Launcher::launchProgress,
            this, &LaunchBackend::onLaunchProgress);
    connect(m_launcher, &Launcher::launchFinished,
            this, &LaunchBackend::onLaunchFinished);
}

LaunchBackend::~LaunchBackend() = default;

// ============================================================
// Configuration
// ============================================================

void LaunchBackend::setGameDir(const QString& dir)
{
    if (m_launcher) {
        m_launcher->setGameDir(dir);
    }
}

// ============================================================
// Property: isRunning
// ============================================================

bool LaunchBackend::isRunning() const
{
    return m_launcher && m_launcher->isRunning();
}

// ============================================================
// Slot: launch
// ============================================================

void LaunchBackend::launch(const QString& versionId, const QString& username,
                           const QString& javaPath, int maxMemoryMB)
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
                      << "memory:" << maxMemoryMB << "MB";
    emit logMessage(QStringLiteral("启动 %1 | %2").arg(versionId, username));

    // ============================================================
    // Pre-launch checks (enhanced validation)
    // ============================================================

    // Check 1: Java executable
    emit launchCheckProgress(QStringLiteral("检查 Java 可执行文件..."));
    emit launchProgressChanged(5, QStringLiteral("检查 Java 可执行文件..."));
    if (!QFileInfo::exists(javaPath)) {
        m_launching = false;
        emit launchProgressChanged(0, QStringLiteral("Java 未找到"));
        emit launchCheckFailed(QStringLiteral("Java 可执行文件"),
                               QStringLiteral("路径: %1").arg(javaPath));
        emit launchStateChanged();
        qCCritical(logLaunch) << "Pre-launch check FAILED: Java executable not found";
        emit logMessage(QStringLiteral("启动失败: Java 未找到"));
        return;
    }
    qCInfo(logLaunch) << "Pre-launch check passed: Java executable";

    // Check 2: Java architecture
    emit launchCheckProgress(QStringLiteral("检查 Java 架构..."));
    emit launchProgressChanged(10, QStringLiteral("检查 Java 架构..."));
    QString arch = checkJavaArchitecture(javaPath);
    if (arch == QStringLiteral("32")) {
        qCWarning(logLaunch) << "32-bit Java detected — limiting memory to 1536 MB";
        if (maxMemoryMB > 1536) {
            maxMemoryMB = 1536;
            emit launchCheckWarning(
                QStringLiteral("检测到 32 位 Java，已将内存限制为 1536MB"));
        }
    }

    // Check 3: Version directory
    emit launchCheckProgress(QStringLiteral("检查版本核心文件..."));
    emit launchProgressChanged(15, QStringLiteral("检查版本核心文件..."));
    QString versionDir = m_launcher->gameDir() + QStringLiteral("/versions/") + versionId;
    if (!QDir(versionDir).exists()) {
        m_launching = false;
        emit launchProgressChanged(0, QStringLiteral("版本目录不存在"));
        emit launchCheckFailed(QStringLiteral("版本目录"),
                               QStringLiteral("目录不存在: %1").arg(versionDir));
        emit launchStateChanged();
        qCCritical(logLaunch) << "Pre-launch check FAILED: version dir not found:" << versionDir;
        emit logMessage(QStringLiteral("启动失败: 版本目录不存在"));
        return;
    }
    qCInfo(logLaunch) << "Pre-launch check passed: version directory exists";

    // Check 4: Version JAR
    QString jarPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (!QFileInfo::exists(jarPath)) {
        m_launching = false;
        emit launchProgressChanged(0, QStringLiteral("核心 Jar 缺失"));
        emit launchCheckFailed(QStringLiteral("核心 Jar"),
                               QStringLiteral("文件不存在: %1").arg(jarPath));
        emit launchStateChanged();
        qCCritical(logLaunch) << "Pre-launch check FAILED: JAR not found:" << jarPath;
        emit logMessage(QStringLiteral("启动失败: 版本核心文件缺失"));
        return;
    }
    qCInfo(logLaunch) << "Pre-launch check passed: version JAR exists";

    // Check 5: Version JSON validity
    emit launchCheckProgress(QStringLiteral("检查版本配置文件..."));
    emit launchProgressChanged(20, QStringLiteral("检查版本配置文件..."));
    QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    {
        QFile jsonFile(jsonPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            m_launching = false;
            emit launchProgressChanged(0, QStringLiteral("版本配置文件缺失"));
            emit launchCheckFailed(QStringLiteral("版本配置"),
                                   QStringLiteral("配置文件不存在: %1").arg(jsonPath));
            emit launchStateChanged();
            qCCritical(logLaunch) << "Pre-launch check FAILED: version JSON not found:" << jsonPath;
            emit logMessage(QStringLiteral("启动失败: 版本配置文件缺失"));
            return;
        }
        QJsonParseError parseErr;
        QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
        jsonFile.close();
        if (parseErr.error != QJsonParseError::NoError) {
            qCWarning(logLaunch) << "Version JSON parse warning:" << parseErr.errorString();
            emit launchCheckWarning(
                QStringLiteral("版本配置文件格式错误: %1").arg(parseErr.errorString()));
        }
    }
    qCInfo(logLaunch) << "Pre-launch check passed: version JSON valid";

    // Check 5.5: Library files completeness (reads version JSON)
    emit launchCheckProgress(QStringLiteral("检查依赖库文件..."));
    emit launchProgressChanged(30, QStringLiteral("检查依赖库文件..."));
    {
        QStringList missingLibs = checkVersionLibraries(versionId);
        if (!missingLibs.isEmpty()) {
            // Show first 8 missing files in the details message
            QStringList displayList = missingLibs.mid(0, 8);
            QString detail = displayList.join(QStringLiteral(", "));
            if (missingLibs.size() > 8) {
                detail += QStringLiteral(" ... 等共 %1 个文件").arg(missingLibs.size());
            }
            m_launching = false;
            emit launchProgressChanged(0, QStringLiteral("依赖库文件缺失"));
            emit launchCheckMissingFiles(missingLibs);
            emit launchCheckFailed(QStringLiteral("依赖库文件"),
                                   QStringLiteral("缺少 %1 个文件: %2").arg(missingLibs.size()).arg(detail));
            emit launchStateChanged();
            qCCritical(logLaunch) << "Pre-launch check FAILED:" << missingLibs.size() << "library files missing";
            emit logMessage(QStringLiteral("启动失败: 依赖库文件缺失 (%1 个)").arg(missingLibs.size()));
            return;
        }
    }
    qCInfo(logLaunch) << "Pre-launch check passed: all library files present";

    // Check 6: Natives
    emit launchCheckProgress(QStringLiteral("检查运行库..."));
    emit launchProgressChanged(40, QStringLiteral("检查运行库..."));
    {
        QStringList missingNatives = checkVersionMissingNatives(versionId);
        if (!missingNatives.isEmpty()) {
            // Build a detailed message
            QStringList displayList = missingNatives.mid(0, 5);
            QString detail = displayList.join(QStringLiteral(", "));
            if (missingNatives.size() > 5) {
                detail += QStringLiteral(" ... 等共 %1 个文件").arg(missingNatives.size());
            }
            m_launching = false;
            emit launchProgressChanged(0, QStringLiteral("缺少原生库文件"));
            emit launchCheckMissingFiles(missingNatives);
            emit launchCheckFailed(QStringLiteral("原生库文件"),
                                   QStringLiteral("缺少 %1 个原生库文件: %2")
                                       .arg(missingNatives.size()).arg(detail));
            emit launchStateChanged();
            qCCritical(logLaunch) << "Pre-launch check FAILED:" << missingNatives.size()
                                  << "native library files missing";
            emit logMessage(QStringLiteral("启动失败: 原生库文件缺失 (%1 个)").arg(missingNatives.size()));
            return;
        }
    }
    qCInfo(logLaunch) << "Pre-launch check passed: all native libraries present";

    // Check 7: Memory limits
    emit launchCheckProgress(QStringLiteral("检查内存限制..."));
    emit launchProgressChanged(50, QStringLiteral("检查内存限制..."));
    if (maxMemoryMB < 512) {
        qCWarning(logLaunch) << "Pre-launch check: memory < 512 MB may cause issues";
        emit launchCheckWarning(
            QStringLiteral("内存分配不足 512MB，可能影响游戏运行"));
    }
    qCInfo(logLaunch) << "Pre-launch checks all passed — starting Minecraft";

    emit launchCheckProgress(QStringLiteral("正在启动 Minecraft..."));
    emit launchProgressChanged(55, QStringLiteral("正在启动 Minecraft..."));

    m_launcher->start(versionId, javaPath, maxMemoryMB);
}

// ============================================================
// Slot: cancelLaunch
// ============================================================

void LaunchBackend::cancelLaunch()
{
    m_cancelled = true;
    m_launcher->killProcess();  // Kill immediately

    m_launching = false;
    m_launchProgress = 0;
    m_launchStatus.clear();
    emit launchProgressChanged(0, QString());
    emit launchStateChanged();
    emit logMessage(QStringLiteral("用户取消了启动"));
}

// ============================================================
// Slot: killGameProcess
// ============================================================

void LaunchBackend::killGameProcess()
{
    m_launcher->killProcess();  // Kill immediately with taskkill /F /T
    emit logMessage(QStringLiteral("已强制结束游戏进程"));

    m_launching = false;
    m_launchProgress = 0;
    m_launchStatus.clear();
    emit launchProgressChanged(0, QString());
    emit isRunningChanged();
    emit logMessage(QStringLiteral("已强制结束游戏进程"));
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
// Private Slots
// ============================================================

void LaunchBackend::onLaunchStarted()
{
    m_launchProgress = 95;
    m_launchStatus = QStringLiteral("游戏正在运行");
    emit launchProgressChanged(95, m_launchStatus);
    emit minecraftStarted();
    emit isRunningChanged();
    qCInfo(logLaunch) << "Minecraft process started";
    emit logMessage(QStringLiteral("Minecraft 进程已启动"));
}

void LaunchBackend::onLaunchProgress(const QString& message)
{
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

void LaunchBackend::onLaunchFinished(bool success, const QString& errorMsg)
{
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
    }
    emit launchStateChanged();
    emit minecraftStopped();
    emit isRunningChanged();
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

QStringList LaunchBackend::checkVersionLibraries(const QString& versionId)
{
    QStringList missing;
    if (!m_launcher) return missing;

    const QString gameDir = m_launcher->gameDir();
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
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();

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
    if (!m_launcher) return missing;

    const QString gameDir = m_launcher->gameDir();
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
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();

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

} // namespace ShadowLauncher
