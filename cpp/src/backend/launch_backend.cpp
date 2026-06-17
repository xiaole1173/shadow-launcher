#include "launch_backend.h"
#include "../core/launcher.h"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace ShadowLauncher {

// ============================================================
// Game directory default (same as SettingsBackend)
// ============================================================

static QString defaultGameDir()
{
    return QDir::homePath() + QStringLiteral("/.shadow/minecraft");
}

// ============================================================
// Constructor / Destructor
// ============================================================

LaunchBackend::LaunchBackend(QObject* parent)
    : QObject(parent)
    , m_launcher(new Launcher(this))
    , m_gameDir(defaultGameDir())
{
    QDir().mkpath(m_gameDir + QStringLiteral("/versions"));
    QDir().mkpath(m_gameDir + QStringLiteral("/libraries"));
    QDir().mkpath(m_gameDir + QStringLiteral("/assets"));

    m_launcher->setGameDir(m_gameDir);

    connect(m_launcher, &Launcher::launchStarted,
            this, &LaunchBackend::onLaunchStarted);
    connect(m_launcher, &Launcher::launchProgress,
            this, &LaunchBackend::onLaunchProgress);
    connect(m_launcher, &Launcher::launchFinished,
            this, &LaunchBackend::onLaunchFinished);
}

LaunchBackend::~LaunchBackend() = default;

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

    m_launcher->setGameDir(m_gameDir);
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
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);
    if (GlobalMemoryStatusEx(&s)) {
        qint64 availMB = static_cast<qint64>(s.ullAvailPhys / (1024 * 1024));
        return qBound(1024, static_cast<int>(availMB / 2), 8192);
    }
#endif
    return 2048; // fallback
}

// ============================================================
// Slot: getSystemMemory
// ============================================================

int LaunchBackend::getSystemMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);
    if (GlobalMemoryStatusEx(&s)) {
        return static_cast<int>(s.ullTotalPhys / (1024 * 1024));
    }
#endif
    return 0;
}

// ============================================================
// Slot: getMemoryStatus
// ============================================================

QVariantMap LaunchBackend::getMemoryStatus()
{
    QVariantMap map;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);
    if (GlobalMemoryStatusEx(&s)) {
        qint64 totalMB = static_cast<qint64>(s.ullTotalPhys / (1024 * 1024));
        qint64 availMB = static_cast<qint64>(s.ullAvailPhys  / (1024 * 1024));
        map[QStringLiteral("total")]       = QVariant::fromValue(totalMB);
        map[QStringLiteral("available")]   = QVariant::fromValue(availMB);
        map[QStringLiteral("percent")]     = static_cast<int>(s.dwMemoryLoad);
        map[QStringLiteral("recommended")] = qBound(1024, static_cast<int>(availMB / 2), 8192);
        return map;
    }
#endif
    map[QStringLiteral("total")]       = 0;
    map[QStringLiteral("available")]   = 0;
    map[QStringLiteral("percent")]     = 0;
    map[QStringLiteral("recommended")] = 2048;
    return map;
}

// ============================================================
// Private Slots: Launcher signal forwarding
// ============================================================

void LaunchBackend::onLaunchStarted()
{
    m_launching = true;
    emit launchStateChanged();
    emit minecraftStarted();
    emit isRunningChanged();
}

void LaunchBackend::onLaunchProgress(const QString& message)
{
    // Forward as status text; increment progress loosely
    // Launcher emits text messages without numeric progress,
    // so we do a simple heuristic: bump toward 90% capped.
    if (m_launchProgress < 90) {
        m_launchProgress = qMin(90, m_launchProgress + 5);
    }
    m_launchStatus = message;
    emit launchProgressChanged(m_launchProgress, message);
    emit logMessage(message);
}

void LaunchBackend::onLaunchFinished(bool success, const QString& errorMsg)
{
    if (success) {
        m_launchProgress = 100;
        m_launchStatus = QStringLiteral("启动完成");
        emit launchProgressChanged(100, m_launchStatus);
    } else {
        m_launchProgress = 0;
        m_launchStatus.clear();
        emit launchProgressChanged(0, errorMsg.isEmpty()
            ? QStringLiteral("启动失败") : errorMsg);
    }

    m_launching = false;
    m_cancelled = false;
    emit minecraftStopped();
    emit isRunningChanged();
    emit launchStateChanged();
    emit logMessage(errorMsg.isEmpty()
        ? QStringLiteral("Minecraft 已退出")
        : QStringLiteral("启动失败: %1").arg(errorMsg));
}

} // namespace ShadowLauncher
