#include "launch_backend.h"
#include "../core/launcher.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
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
    emit logMessage(QStringLiteral("启动 %1 | %2").arg(versionId, username));

    // ============================================================
    // Pre-launch checks (enhanced validation)
    // ============================================================

    // Check 1: Java executable
    emit launchCheckProgress(QStringLiteral("检查 Java 可执行文件..."));
    emit launchProgressChanged(5, QStringLiteral("检查 Java 可执行文件..."));
    if (!QFileInfo::exists(javaPath)) {
        m_launching = false;
        emit launchProgressChanged(0, QString());
        emit launchStateChanged();
        emit logMessage(QStringLiteral("启动失败: Java 未找到"));
        return;
    }

    // Check 2: Java architecture
    emit launchCheckProgress(QStringLiteral("检查 Java 架构..."));
    emit launchProgressChanged(10, QStringLiteral("检查 Java 架构..."));
    QString arch = checkJavaArchitecture(javaPath);
    if (arch == QStringLiteral("32")) {
        if (maxMemoryMB > 1536) {
            maxMemoryMB = 1536;
            emit launchCheckWarning(
                QStringLiteral("检测到 32 位 Java，已将内存限制为 1536MB"));
        }
    }

    // Check 3: Version directory
    emit launchCheckProgress(QStringLiteral("检查版本核心文件..."));
    emit launchProgressChanged(20, QStringLiteral("检查版本核心文件..."));
    QString versionDir = m_launcher->gameDir() + QStringLiteral("/versions/") + versionId;
    if (!QDir(versionDir).exists()) {
        m_launching = false;
        emit launchProgressChanged(0, QString());
        emit launchStateChanged();
        emit logMessage(QStringLiteral("启动失败: 版本目录不存在"));
        return;
    }

    // Check 4: Version JAR
    QString jarPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (!QFileInfo::exists(jarPath)) {
        m_launching = false;
        emit launchProgressChanged(0, QString());
        emit launchStateChanged();
        emit logMessage(QStringLiteral("启动失败: 版本核心文件缺失"));
        return;
    }

    // Check 5: Version JSON validity
    emit launchCheckProgress(QStringLiteral("检查版本配置文件..."));
    emit launchProgressChanged(30, QStringLiteral("检查版本配置文件..."));
    QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    {
        QFile jsonFile(jsonPath);
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            m_launching = false;
            emit launchProgressChanged(0, QString());
            emit launchStateChanged();
            emit logMessage(QStringLiteral("启动失败: 版本配置文件缺失"));
            return;
        }
        QJsonParseError parseErr;
        QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
        jsonFile.close();
        if (parseErr.error != QJsonParseError::NoError) {
            emit launchCheckWarning(
                QStringLiteral("版本配置文件格式错误: %1").arg(parseErr.errorString()));
        }
    }

    // Check 6: Natives
    emit launchCheckProgress(QStringLiteral("检查运行库..."));
    emit launchProgressChanged(40, QStringLiteral("检查运行库..."));
    if (!checkVersionHasNatives(versionId)) {
        emit launchCheckWarning(
            QStringLiteral("未检测到运行库，可能需要下载或解压 natives"));
    }

    // Check 7: Memory limits
    emit launchCheckProgress(QStringLiteral("检查内存限制..."));
    emit launchProgressChanged(50, QStringLiteral("检查内存限制..."));
    if (maxMemoryMB < 512) {
        emit launchCheckWarning(
            QStringLiteral("内存分配不足 512MB，可能影响游戏运行"));
    }

    m_launcher->start(versionId, javaPath, maxMemoryMB);
}

// ============================================================
// Slot: cancelLaunch
// ============================================================

void LaunchBackend::cancelLaunch()
{
    m_cancelled = true;
    m_launcher->cancel();

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
    m_launcher->cancel();

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
    m_launchProgress = 10;
    m_launchStatus = QStringLiteral("Minecraft 进程已启动");
    emit launchProgressChanged(10, m_launchStatus);
    emit minecraftStarted();
    emit isRunningChanged();
    emit logMessage(QStringLiteral("Minecraft 进程已启动"));
}

void LaunchBackend::onLaunchProgress(const QString& message)
{
    m_launchProgress = qMin(m_launchProgress + 5, 95);
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
        emit logMessage(QStringLiteral("Minecraft 启动成功"));
    } else {
        m_launchProgress = 0;
        m_launchStatus.clear();
        emit launchProgressChanged(0, errorMsg);
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
// Pre-launch check: Natives presence
// ============================================================

bool LaunchBackend::checkVersionHasNatives(const QString& versionId)
{
    if (!m_launcher) return false;

    const QString gameDir = m_launcher->gameDir();
    const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;

    // Check primary natives path: {versionId}-natives/
    QString primaryNatives = versionDir + QStringLiteral("/") + versionId + QStringLiteral("-natives");
    if (QDir(primaryNatives).exists()) {
        QDirIterator it(primaryNatives, QStringList() << QStringLiteral("*.dll"), QDir::Files);
        if (it.hasNext()) {
            emit logMessage(QStringLiteral("找到运行库: %1").arg(primaryNatives));
            return true;
        }
    }

    // Check fallback: natives/
    QString fallbackNatives = versionDir + QStringLiteral("/natives");
    if (QDir(fallbackNatives).exists()) {
        QDirIterator it(fallbackNatives, QStringList() << QStringLiteral("*.dll"), QDir::Files);
        if (it.hasNext()) {
            emit logMessage(QStringLiteral("找到运行库: %1").arg(fallbackNatives));
            return true;
        }
    }

    emit logMessage(QStringLiteral("未检测到运行库文件 (.dll)"));
    return false;
}

} // namespace ShadowLauncher
