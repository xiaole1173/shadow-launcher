#include "launch_backend.h"
#include "../core/launcher.h"

#include <QDir>
#include <QFileInfo>

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
    emit launchProgressChanged(0, QStringLiteral("正在进行启动前检查..."));
    emit launchStateChanged();
    emit logMessage(QStringLiteral("启动 %1 | %2").arg(versionId, username));

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

} // namespace ShadowLauncher
