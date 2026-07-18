// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "settings_backend.h"
#include "../core/version_isolation.h"
#include "../utils/logger.h"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QCoreApplication>
#include <QThread>
#include <future>
#include <algorithm>
#include <climits>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace ShadowLauncher {

// ============================================================
// Constants
// ============================================================

static const char* COMMON_JAVA_DIRS[] = {
    "C:\\Program Files\\Java",
    "C:\\Program Files\\Eclipse Adoptium",
    "C:\\Program Files\\Microsoft",
    "C:\\Program Files\\Eclipse Foundation",
    "C:\\Program Files\\Amazon Corretto",
    "C:\\Program Files\\Azul",
    "C:\\Program Files\\Zulu",
    "C:\\Program Files\\BellSoft",
    "C:\\Program Files (x86)\\Java",
    "C:\\Program Files (x86)\\Eclipse Adoptium",
    "C:\\Program Files (x86)\\Microsoft",
    "C:\\Program Files (x86)\\Minecraft Launcher\\runtime",
    "C:\\XboxGames\\Minecraft Launcher\\Content\\runtime",
    "%USERPROFILE%\\scoop\\apps",
    "%LOCALAPPDATA%\\Programs",
};

// ============================================================
// Constructor
// ============================================================

SettingsBackend::SettingsBackend(QObject* parent)
    : QObject(parent)
    , m_minMemoryMB(512)
    , m_maxMemoryMB(2048)
    
{
    // Create VersionIsolation (game dir set later)
    m_isolation = new VersionIsolation(this);
    connect(m_isolation, &VersionIsolation::isolationChanged,
            this, &SettingsBackend::isolationChanged);

    // Game dir is set later by ShadowBackend via setMinecraftDir()
    
    // Load persisted settings
    loadSettings();

    // Auto-detect Java asynchronously (defer to event loop)
    // This avoids blocking the UI during startup
    QTimer::singleShot(100, this, &SettingsBackend::doAutoDetect);
}

// ============================================================
// Settings persistence (QSettings)
// ============================================================

void SettingsBackend::loadSettings()
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());

    m_javaPath = s.value(QStringLiteral("java/path"), QString()).toString();
    m_javaVersion = s.value(QStringLiteral("java/version"), QString()).toString();
    m_javaMajor = s.value(QStringLiteral("java/major"), 0).toInt();
    m_javaReady = (m_javaMajor > 0);

    m_minMemoryMB = s.value(QStringLiteral("memory/minMB"), 512).toInt();
    m_maxMemoryMB = s.value(QStringLiteral("memory/maxMB"), 2048).toInt();
    m_autoMemory = s.value(QStringLiteral("memory/autoMemory"), true).toBool();
    m_embeddedLoginEnabled = s.value(QStringLiteral("general/embeddedLogin"), false).toBool();
    m_languageIndex = s.value(QStringLiteral("general/languageIndex"), 0).toInt();
    m_launchLanguageIndex = m_languageIndex;  // snapshot for restart detection
    m_lastLaunchedVersion = s.value(QStringLiteral("general/lastVersion"), QString()).toString();
    m_lastSelectedVersion = s.value(QStringLiteral("general/lastSelectedVersion"), QString()).toString();

    // Custom background
    m_customBgPath = s.value(QStringLiteral("customBg/path"), QString()).toString();
    m_sidebarOpacity = s.value(QStringLiteral("customBg/sidebarOpacity"), 0.90).toDouble();
    m_contentOpacity = s.value(QStringLiteral("customBg/contentOpacity"), 0.70).toDouble();
    m_cropX = s.value(QStringLiteral("customBg/cropX"), 0.5).toDouble();
    m_cropY = s.value(QStringLiteral("customBg/cropY"), 0.5).toDouble();

    // Auto-language mode
    m_autoLangMode = s.value(QStringLiteral("general/autoLangMode"), 1).toInt();

    // Download settings
    m_fileDownloadSource = s.value(QStringLiteral("download/fileSource"), 1).toInt();
    m_listDownloadSource = s.value(QStringLiteral("download/listSource"), 1).toInt();
    m_maxDownloadThreads = s.value(QStringLiteral("download/threadLimit"), 64).toInt();
    m_downloadSpeedLimitMB = s.value(QStringLiteral("download/speedLimitMB"), -1.0).toDouble();
}

void SettingsBackend::saveSettings()
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());

    s.setValue(QStringLiteral("java/path"), m_javaPath);
    s.setValue(QStringLiteral("java/version"), m_javaVersion);
    s.setValue(QStringLiteral("java/major"), m_javaMajor);

    s.setValue(QStringLiteral("memory/minMB"), m_minMemoryMB);
    s.setValue(QStringLiteral("memory/maxMB"), m_maxMemoryMB);
    s.setValue(QStringLiteral("memory/autoMemory"), m_autoMemory);
    s.setValue(QStringLiteral("general/embeddedLogin"), m_embeddedLoginEnabled);
    s.setValue(QStringLiteral("general/languageIndex"), m_languageIndex);
    s.setValue(QStringLiteral("general/lastVersion"), m_lastLaunchedVersion);
    s.setValue(QStringLiteral("general/lastSelectedVersion"), m_lastSelectedVersion);

    // Custom background
    s.setValue(QStringLiteral("customBg/path"), m_customBgPath);
    s.setValue(QStringLiteral("customBg/sidebarOpacity"), m_sidebarOpacity);
    s.setValue(QStringLiteral("customBg/contentOpacity"), m_contentOpacity);
    s.setValue(QStringLiteral("customBg/cropX"), m_cropX);
    s.setValue(QStringLiteral("customBg/cropY"), m_cropY);

    // Auto-language mode
    s.setValue(QStringLiteral("general/autoLangMode"), m_autoLangMode);

    // Download settings
    s.setValue(QStringLiteral("download/fileSource"), m_fileDownloadSource);
    s.setValue(QStringLiteral("download/listSource"), m_listDownloadSource);
    s.setValue(QStringLiteral("download/threadLimit"), m_maxDownloadThreads);
    s.setValue(QStringLiteral("download/speedLimitMB"), m_downloadSpeedLimitMB);
}

// ============================================================
// Async Java auto-detect (called via QTimer after constructor)
// ============================================================

void SettingsBackend::doAutoDetect()
{
    // Skip if we already have a valid Java from saved settings
    if (m_javaReady && QFileInfo::exists(m_javaPath)) {
        emit logMessage(tr("Java 已配置: %1 (版本 %2)")
                            .arg(m_javaPath, m_javaVersion));
        // Still scan in background to populate the list
        scanJavaInstallations();
        return;
    }

    // No Java configured — start async scan; auto-select happens in scan completion
    emit logMessage(tr("正在后台自动检测 Java..."));
    scanJavaInstallations();
    saveSettings();
}

// ============================================================
// Properties
// ============================================================

void SettingsBackend::setJavaPath(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.isFile()) {
        emit logMessage(tr("[失败] 路径不存在: %1").arg(path));
        return;
    }
    JavaInfo info = getJavaInfo(path);
    if (info.major == 0) {
        emit logMessage(tr("[失败] 无法获取 Java 版本: %1").arg(path));
        return;
    }
    m_javaPath = info.path;
    m_javaVersion = info.version;
    m_javaMajor = info.major;

    if (!m_javaReady) {
        m_javaReady = true;
        emit javaReadyChanged();
    }

    saveSettings();
    qCInfo(logUI) << QStringLiteral("Java路径: ") << path;
    emit logMessage(tr("[完成] Java 已设置: %1 (版本 %2)")
                        .arg(path, info.version));
    emit javaPathChanged();
}

// ============================================================
// Java Detection Slots
// ============================================================

const QVector<SettingsBackend::JavaInfo>& SettingsBackend::cachedJavaList()
{
    // Return cache immediately — no sync disk scan
    // Cache is populated by scanJavaInstallations() (async) or loadSettings()
    return m_cachedJavaList;
}

QVariantList SettingsBackend::scanJavaInstallations()
{
    if (m_javaScanning) {
        emit logMessage(tr("Java 扫描已在进行中..."));
        return {};
    }
    m_javaScanning = true;
    emit logMessage(tr("正在后台扫描 Java 安装..."));
    emit javaPathChanged();  // signal QML to show loading state

    // Use std::thread::detach() — NOT std::async whose future destructor blocks!
    std::thread([this]() {
        QVector<JavaInfo> results = findAllJava();
        std::sort(results.begin(), results.end(),
                  [](const JavaInfo& a, const JavaInfo& b) {
                      return a.major > b.major;
                  });

        QMetaObject::invokeMethod(this, [this, results]() {
            m_cachedJavaList = results;
            m_javaCacheValid = true;
            m_javaScanning = false;
            emit javaPathChanged();
            if (!results.isEmpty()) {
                emit logMessage(tr("找到 %1 个 Java 安装，最新: Java %2")
                                    .arg(results.size())
                                    .arg(results.first().major));
                if (!m_javaReady || !QFileInfo::exists(m_javaPath)) {
                    autoSelectJava();
                }
            } else {
                emit logMessage(tr("[警告] 未在系统中找到 Java 安装"));
            }
        }, Qt::QueuedConnection);
    }).detach();

    return {};  // return immediately, results arrive via signal
}

QVariantList SettingsBackend::availableJavaList()
{
    // Return cached list immediately — no sync disk scan
    QVariantList list;
    for (const auto& j : m_cachedJavaList) {
        list.append(QVariantMap{
            { QStringLiteral("path"), j.path },
            { QStringLiteral("version"), j.version },
            { QStringLiteral("major"), j.major }
        });
    }
    return list;
}

void SettingsBackend::selectJavaByIndex(int index)
{
    const auto& results = cachedJavaList();
    if (index < 0 || index >= results.size()) {
        emit logMessage(tr("[失败] Java 索引无效: %1").arg(index));
        return;
    }
    const auto& info = results[index];
    m_javaPath = info.path;
    m_javaVersion = info.version;
    m_javaMajor = info.major;
    m_javaReady = true;
    saveSettings();
    emit logMessage(tr("[完成] 已切换 Java %1: %2")
                        .arg(info.major).arg(info.path));
    emit javaPathChanged();
    emit javaReadyChanged();
}

QString SettingsBackend::autoSelectJava()
{
    const auto& results = cachedJavaList();
    const JavaInfo* best = nullptr;
    for (const auto& j : results) {
        if (j.major >= 17) { best = &j; break; }
    }
    if (!best && !results.isEmpty()) best = &results.first();
    if (best) {
        m_javaPath = best->path;
        m_javaVersion = best->version;
        m_javaMajor = best->major;
        m_javaReady = true;
        saveSettings();
        emit logMessage(tr("[完成] 已选择 Java %1: %2")
                            .arg(best->major).arg(best->path));
        emit javaPathChanged();
        emit javaReadyChanged();
        return best->path;
    }
    emit logMessage(tr("[警告] 未找到合适的 Java，请手动指定"));
    return {};
}

QString SettingsBackend::findJavaForVersion(int requiredMajor)
{
    // Searches cached Java list for best match to required major version.
    // Strategy:
    //   1. Exact match (== requiredMajor) → always preferred
    //   2. Closest higher match (>= requiredMajor, smallest diff) → compatible
    //   3. Java 8 special: must be exact (Java 9+ breaks pre-1.6 LaunchWrapper)
    const auto& results = cachedJavaList();
    
    if (results.isEmpty()) {
        qCWarning(logLaunch) << QStringLiteral("[JAVA] 缓存中未找到Java安装");
        return {};
    }
    
    const JavaInfo* exactMatch = nullptr;
    const JavaInfo* closestHigher = nullptr;
    int closestDiff = INT_MAX;
    
    for (const auto& j : results) {
        if (j.major == requiredMajor) {
            exactMatch = &j;
            break; // exact match always wins, stop early
        }
        if (j.major >= requiredMajor) {
            int diff = j.major - requiredMajor;
            if (diff < closestDiff) {
                closestDiff = diff;
                closestHigher = &j;
            }
        }
    }
    
    const JavaInfo* best = exactMatch ? exactMatch : closestHigher;
    
    // Java 8: exact match mandatory (Java 9+ breaks LaunchWrapper for old versions)
    if (!exactMatch && requiredMajor == 8) {
        qCWarning(logLaunch) << QStringLiteral("[JAVA] 需要Java 8但未找到")
                             << "Java 9+ is incompatible with pre-1.6 Forge/LaunchWrapper";
        return {};
    }
    
    if (best) {
        qCInfo(logLaunch) << QStringLiteral("[JAVA] Requires Java %1 → matched Java %2%3 at %4")
                            .arg(requiredMajor).arg(best->major)
                            .arg(best == exactMatch ? QString()
                                 : QStringLiteral(" (+%1)").arg(best->major - requiredMajor))
                            .arg(best->path);
        return best->path;
    }
    
    qCWarning(logLaunch) << QStringLiteral("[JAVA] 缺少Java %1 未找到匹配").arg(requiredMajor)
                          << "but no compatible Java ≥" << requiredMajor << "found";
    return {};
}

QString SettingsBackend::detectJava() { return autoSelectJava(); }

QString SettingsBackend::openJavaFileDialog()
{
    // Use the QML engine's root object as parent to avoid modal issues
    QWidget* parentWidget = nullptr;
    QString path = QFileDialog::getOpenFileName(
        parentWidget, tr("选择 Java 可执行文件"),
        QStringLiteral("C:\\Program Files\\Java"),
        tr("Java 可执行文件 (java.exe javaw.exe);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        setJavaPath(path);
        return path;
    }
    return {};
}

QString SettingsBackend::browseJava() { return openJavaFileDialog(); }

// ============================================================
// Memory (Bug 6 fix: use int for QML compatibility)
// ============================================================

QVariantMap SettingsBackend::getMemoryStatus()
{
    QVariantMap map;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);
    if (GlobalMemoryStatusEx(&s)) {
        int totalMB = static_cast<int>(s.ullTotalPhys / (1024 * 1024));
        int availMB = static_cast<int>(s.ullAvailPhys / (1024 * 1024));
        map[QStringLiteral("total")]       = totalMB;
        map[QStringLiteral("available")]   = availMB;
        map[QStringLiteral("percent")]     = static_cast<int>(s.dwMemoryLoad);
        map[QStringLiteral("recommended")] = qBound(1024, availMB / 2, 8192);
        return map;
    }
#endif
    // Fallback: round-trip estimate
    int totalMB = 8192;
    int percent = 50;
    map[QStringLiteral("total")]       = totalMB;
    map[QStringLiteral("available")]   = totalMB / 2;
    map[QStringLiteral("percent")]     = percent;
    map[QStringLiteral("recommended")] = 2048;
    return map;
}

void SettingsBackend::setMinMemory(int mb)
{
    m_minMemoryMB = qBound(256, mb, m_maxMemoryMB);
    saveSettings();
    emit memorySettingsChanged();
}

void SettingsBackend::setMaxMemory(int mb)
{
    m_maxMemoryMB = qBound(m_minMemoryMB, mb, 32768);
    saveSettings();
    qCInfo(logUI) << QStringLiteral("游戏内存: ") << mb << QStringLiteral("MB");
    emit memorySettingsChanged();
}

void SettingsBackend::setAutoMemoryEnabled(bool enabled)
{
    if (m_autoMemory == enabled) return;
    m_autoMemory = enabled;
    saveSettings();
    qCInfo(logUI) << QStringLiteral("自动内存分配: ") << (enabled ? QStringLiteral("开") : QStringLiteral("关"));
    emit memorySettingsChanged();
}

// ============================================================
// Per-Version Memory Override
// ============================================================

int SettingsBackend::versionMemoryMode(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionMemory/") + versionId + QStringLiteral("/mode"), 0).toInt();
}

void SettingsBackend::setVersionMemoryMode(const QString& versionId, int mode)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionMemory/") + versionId + QStringLiteral("/mode"), mode);
    s.sync();
    qCInfo(logUI) << QStringLiteral("版本内存模式: ") << versionId << QStringLiteral(" -> ") << mode;
}

int SettingsBackend::versionMemoryManualMB(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionMemory/") + versionId + QStringLiteral("/manualMB"), 2048).toInt();
}

void SettingsBackend::setVersionMemoryManualMB(const QString& versionId, int mb)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionMemory/") + versionId + QStringLiteral("/manualMB"), mb);
    s.sync();
    qCInfo(logUI) << QStringLiteral("版本内存: ") << versionId << QStringLiteral(" -> ") << mb << QStringLiteral("MB");
}

// ── Per-version Java/launch overrides ──

int SettingsBackend::versionJavaMode(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/javaMode"), 0).toInt();
}
void SettingsBackend::setVersionJavaMode(const QString& versionId, int mode)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/javaMode"), mode);
    s.sync();
}

int SettingsBackend::versionJvmArgsMode(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/jvmArgsMode"), 0).toInt();
}
void SettingsBackend::setVersionJvmArgsMode(const QString& versionId, int mode)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/jvmArgsMode"), mode);
    s.sync();
}
QString SettingsBackend::versionJvmArgs(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/jvmArgs"), QString()).toString();
}
void SettingsBackend::setVersionJvmArgs(const QString& versionId, const QString& args)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/jvmArgs"), args);
    s.sync();
}

int SettingsBackend::versionGameArgsMode(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/gameArgsMode"), 0).toInt();
}
void SettingsBackend::setVersionGameArgsMode(const QString& versionId, int mode)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/gameArgsMode"), mode);
    s.sync();
}
QString SettingsBackend::versionGameArgs(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/gameArgs"), QString()).toString();
}
void SettingsBackend::setVersionGameArgs(const QString& versionId, const QString& args)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/gameArgs"), args);
    s.sync();
}

int SettingsBackend::versionHighPerfGpuMode(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/highPerfGpuMode"), 0).toInt();
}
void SettingsBackend::setVersionHighPerfGpuMode(const QString& versionId, int mode)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/highPerfGpuMode"), mode);
    s.sync();
}
bool SettingsBackend::versionHighPerfGpu(const QString& versionId) const
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    return s.value(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/highPerfGpu"), false).toBool();
}
void SettingsBackend::setVersionHighPerfGpu(const QString& versionId, bool v)
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.setValue(QStringLiteral("versionLaunch/") + versionId + QStringLiteral("/highPerfGpu"), v);
    s.sync();
}

// ============================================================
// ============================================================
void SettingsBackend::setEmbeddedLoginEnabled(bool v)
{
    if (m_embeddedLoginEnabled != v) {
        m_embeddedLoginEnabled = v;
        saveSettings();
        emit embeddedLoginChanged();
    }
}

// ============================================================
// Isolation (delegates to VersionManager later)
// ============================================================

void SettingsBackend::setMinecraftDir(const QString& dir)
{
    if (m_gameDir != dir && !dir.isEmpty()) {
        m_gameDir = dir;
        m_isolation->setGameDir(dir);
        QDir().mkpath(m_gameDir + QStringLiteral("/versions"));
        QDir().mkpath(m_gameDir + QStringLiteral("/libraries"));
        QDir().mkpath(m_gameDir + QStringLiteral("/assets"));
        emit logMessage(tr("游戏目录已设置: %1").arg(m_gameDir));
    }
}

void SettingsBackend::setIsolationEnabled(bool enabled)
{
    m_isolation->setEnabled(enabled);
    emit logMessage(enabled
        ? tr("版本隔离已开启 — 各版本拥有独立的存档/截图/配置")
        : tr("版本隔离已关闭 — 所有版本共享游戏目录"));
    // isolationChanged signal is emitted by VersionIsolation
}

void SettingsBackend::migrateVersionToIsolated(const QString& versionId)
{
    bool ok = m_isolation->migrateToIsolated(versionId);
    if (ok) {
        emit logMessage(tr("版本 %1 已迁移到隔离模式").arg(versionId));
    } else {
        emit logMessage(tr("版本 %1 迁移失败：未找到版本 JSON").arg(versionId));
    }
}

QString SettingsBackend::getVersionGameDir(const QString& versionId) const
{
    return m_isolation->getVersionGameDir(versionId);
}

bool SettingsBackend::isolationEnabled() const
{
    return m_isolation->isEnabled();
}

void SettingsBackend::setIsolationGameDir(const QString& dir)
{
    m_isolation->setGameDir(dir);
}

// ============================================================
// Path Operations
// ============================================================

void SettingsBackend::openGameDir()
{
    QDir().mkpath(m_gameDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_gameDir));
}

void SettingsBackend::openVersionDir(const QString& versionId)
{
    QString d = m_gameDir + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(d);
    QDesktopServices::openUrl(QUrl::fromLocalFile(d));
}

void SettingsBackend::deleteVersion(const QString& versionId)
{
    int count = 0;

    // Helper: remove a directory completely with fallback
    auto forceRemoveDir = [](const QString& dirPath) -> bool {
        for (int retry = 0; retry < 3; retry++) {
            if (retry > 0) QThread::msleep(500);
            QDir d(dirPath);
            if (!d.exists()) return true;
            bool ok = d.removeRecursively();
            if (ok && !QDir(dirPath).exists()) return true;

            // Manual cleanup: strip all permissions, then remove each file
            qWarning() << "[deleteVersion] retry" << (retry+1) << "manual cleanup for" << dirPath;
            for (const QFileInfo& fi : QDir(dirPath).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
                if (fi.isDir()) {
                    QDir(fi.absoluteFilePath()).removeRecursively();
                } else {
                    QFile f(fi.absoluteFilePath());
                    // Aggressively strip all restrictions before removal (Windows locked files)
                    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                   | QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                   | QFileDevice::ReadOther | QFileDevice::WriteOther);
                    if (!f.remove()) {
                        qWarning() << "[deleteVersion] cannot remove file:" << fi.absoluteFilePath();
                    }
                }
            }
            if (!QDir(dirPath).exists()) return true;
        }
        qWarning() << "[deleteVersion] Failed to fully remove" << dirPath << "after 3 retries";
        return QDir().rmdir(dirPath);
    };

    // 1. Delete the vanilla version folder
    QString verDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    if (QDir(verDir).exists()) {
        if (forceRemoveDir(verDir)) count++;
    }
    // 2. Delete any mod-loader variant folders (e.g. "1.21.1-forge-65.0.0")
    QDir versionsRoot(m_gameDir + QStringLiteral("/versions"));
    if (versionsRoot.exists()) {
        QString prefix = versionId + QStringLiteral("-");
        QFileInfoList entries = versionsRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : entries) {
            if (fi.fileName() == versionId) continue;
            if (fi.fileName().startsWith(prefix)) {
                if (forceRemoveDir(fi.absoluteFilePath())) {
                    count++;
                    emit logMessage(QStringLiteral("\u5df2\u5220\u9664\u52a0\u8f7d\u5668\u7248\u672c: %1").arg(fi.fileName()));
                }
            }
        }
    }
    // 3. Check again & log if folder still exists
    if (QDir(verDir).exists()) {
        emit logMessage(QStringLiteral("\u26a0\ufe0f \u65e0\u6cd5\u5b8c\u5168\u5220\u9664\u76ee\u5f55: %1\uff0c\u8bf7\u624b\u52a8\u6e05\u7406").arg(verDir));
    }
    // 4. Delete the asset index file
    QString ai = m_gameDir + QStringLiteral("/assets/indexes/")
                 + versionId + QStringLiteral(".json");
    if (QFileInfo::exists(ai)) {
        QFile::remove(ai);
        count++;
    }
    emit logMessage(QStringLiteral("\u5df2\u5220\u9664\u7248\u672c: %1 \uff08\u5171\u6e05\u7406 %2 \u4e2a\u6587\u4ef6\u5939\uff09").arg(versionId).arg(count));
}

void SettingsBackend::openPath(const QString& path)
{
    QFileInfo fi(QDir::cleanPath(path));
    QString target = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    QDir().mkpath(target);
    if (QFileInfo::exists(target))
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    else
        emit logMessage(tr("路径不存在: %1").arg(path));
}

// ============================================================
// Private: findAllJava
// ============================================================

QVector<SettingsBackend::JavaInfo> SettingsBackend::findAllJava()
{
    QVector<JavaInfo> results;
    QSet<QString>     seenBinDirs;

    auto add = [&](const QString& p) { tryAddJavaResult(p, seenBinDirs, results); };

    // 1. JAVA_HOME
    QString javaHome = qEnvironmentVariable("JAVA_HOME");
    if (!javaHome.isEmpty()) add(findJavaInDir(javaHome));

    // 2. PATH ("where java")
    add(findJavaOnPath());

    // 3. Scan common directories (depth-limited)
    for (const char* raw : COMMON_JAVA_DIRS) {
        QString searchDir = QString::fromLocal8Bit(raw);
        // Expand %VAR% tokens
        static const QRegularExpression varRe(QStringLiteral("%([^%]+)%"));
        auto m = varRe.match(searchDir);
        if (m.hasMatch()) {
            QString val = qEnvironmentVariable(m.captured(1).toLocal8Bit());
            if (val.isEmpty()) continue;
            searchDir.replace(m.captured(0), val);
        }
        QDir d(searchDir);
        if (!d.exists()) continue;

        // Check top level + depth-1 subdirectories only (Java never deeper than 2 levels)
        // Registry for precision; directory scan is supplementary
        add(findJavaInDir(searchDir));
        const auto entries = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& entry : entries) {
            add(findJavaInDir(entry.absoluteFilePath()));
        }
    }

    // 4. Windows Registry
#ifdef Q_OS_WIN
    const wchar_t* regPaths[] = {
        L"SOFTWARE\\JavaSoft\\JDK",
        L"SOFTWARE\\JavaSoft\\Java Runtime Environment",
        L"SOFTWARE\\Eclipse Adoptium\\JDK",
        L"SOFTWARE\\Eclipse Foundation\\JDK",
        L"SOFTWARE\\Microsoft\\JDK",
    };
    for (const auto* rp : regPaths) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, rp, 0,
                          KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
            continue;
        DWORD idx = 0;
        wchar_t name[256];
        DWORD nameLen = 256;
        while (RegEnumKeyExW(hKey, idx, name, &nameLen,
                             nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            HKEY hSub;
            if (RegOpenKeyExW(hKey, name, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                wchar_t home[512];
                DWORD sz = sizeof(home), type;
                if (RegQueryValueExW(hSub, L"JavaHome", nullptr, &type,
                                     reinterpret_cast<BYTE*>(home), &sz) == ERROR_SUCCESS
                    && type == REG_SZ) {
                    add(findJavaInDir(QString::fromWCharArray(home)));
                }
                RegCloseKey(hSub);
            }
            ++idx;
            nameLen = 256;
        }
        RegCloseKey(hKey);
    }
#endif

    return results;
}

// ============================================================
// Private Helpers
// ============================================================

QString SettingsBackend::findJavaInDir(const QString& dirPath)
{
    static const char* cands[] = { "bin/java.exe", "bin/javaw.exe", "java.exe", "javaw.exe" };
    for (const char* c : cands) {
        QString p = QDir::cleanPath(dirPath + QLatin1Char('/') + QString::fromLatin1(c));
        if (QFileInfo::exists(p)) return p;
    }
    return {};
}

QString SettingsBackend::findJavaOnPath()
{
    QProcess proc;
    proc.start(QStringLiteral("where"), QStringList() << QStringLiteral("java"));
    proc.waitForFinished(5000);
    if (proc.exitCode() == 0) {
        QStringList lines = QString::fromLocal8Bit(
            proc.readAllStandardOutput()).split(QRegularExpression(QStringLiteral("[\r\n]")),
                                                Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            QString p = QDir::cleanPath(lines.first().trimmed());
            if (QFileInfo::exists(p)) return p;
        }
    }
    return {};
}

bool SettingsBackend::tryAddJavaResult(const QString& exePath,
                                        QSet<QString>& seenBinDirs,
                                        QVector<JavaInfo>& out)
{
    if (exePath.isEmpty()) return false;
    QFileInfo fi(exePath);
    if (!fi.isFile()) return false;
    QString binDir = QDir::cleanPath(fi.absolutePath()).toLower();
    if (seenBinDirs.contains(binDir)) return false;
    JavaInfo info = getJavaInfo(exePath);
    if (info.major == 0) return false;
    seenBinDirs.insert(binDir);
    out.append(info);
    return true;
}

SettingsBackend::JavaInfo SettingsBackend::getJavaInfo(const QString& exePath)
{
    JavaInfo info;
    info.path = exePath;
    QProcess proc;
    proc.start(exePath, QStringList() << QStringLiteral("-version"));
    proc.waitForFinished(10000);
    QString output = QString::fromLocal8Bit(proc.readAllStandardError());
    if (output.isEmpty()) output = QString::fromLocal8Bit(proc.readAllStandardOutput());
    if (output.isEmpty()) return info;

    QRegularExpression re(QStringLiteral("version\\s+\"([^\"]+)\""));
    auto match = re.match(output);
    if (!match.hasMatch()) {
        QRegularExpression re2(QStringLiteral("version\\s+(\\S+)"));
        match = re2.match(output);
    }
    if (match.hasMatch()) {
        info.version = match.captured(1);
        info.major = parseMajorVersion(info.version);
    }
    return info;
}

int SettingsBackend::parseMajorVersion(const QString& versionStr)
{
    QString n = versionStr;
    n.replace(QLatin1Char('_'), QLatin1Char('.'));
    QStringList parts = n.split(QLatin1Char('.'));
    if (parts.isEmpty()) return 0;
    if (parts.size() >= 2 && parts[0] == QStringLiteral("1")) {
        bool ok; int v = parts[1].toInt(&ok); return ok ? v : 0;
    }
    bool ok; int v = parts[0].toInt(&ok); return ok ? v : 0;
}

void SettingsBackend::setAutoLangMode(int mode)
{
    if (mode < 0 || mode > 2) return;
    if (mode == m_autoLangMode) return;
    m_autoLangMode = mode;
    saveSettings();
    emit autoLangModeChanged();
}

void SettingsBackend::setLanguageIndex(int idx)
{
    if (idx < 0 || idx > 2) return;
    if (idx == m_languageIndex) return;
    m_languageIndex = idx;
    saveSettings();
    emit languageChanged();
}

void SettingsBackend::restartApp()
{
    QString exe = QCoreApplication::applicationFilePath();
    QProcess::startDetached(exe, {}, QCoreApplication::applicationDirPath());
    QCoreApplication::quit();
}

} // namespace ShadowLauncher
