#include "shadow_backend.h"
#include "../core/version_downloader.h"
#include "../core/version_isolation.h"
#include "account_backend.h"
#include "app_backend.h"
#include "check_backend.h"
#include "launch_backend.h"
#include "resource_backend.h"
#include "settings_backend.h"
#include "version_backend.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStorageInfo>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

namespace ShadowLauncher {

ShadowBackend::ShadowBackend(QObject* parent)
    : QObject(parent)
{
    // Create all 7 sub-backends
    m_app = new AppBackend(this);
    m_account = new AccountBackend(this);
    m_settings = new SettingsBackend(this);
    m_check = new CheckBackend(this);
    m_version = new VersionBackend(this);
    m_launch = new LaunchBackend(this);
    m_resource = new ResourceBackend(this);

    // ── Sync game directories: ALL backends use the same path ──
    m_version->setGameDir(m_app->gameDir());
    m_settings->setMinecraftDir(m_app->gameDir());
    m_settings->setIsolationGameDir(m_app->gameDir());
    m_launch->setGameDir(m_app->gameDir());
    m_version->setIsolation(m_settings->isolation());

    // ── Signal forwarding: AccountBackend → ShadowBackend ──
    connect(m_account, &AccountBackend::accountChanged,
            this, &ShadowBackend::accountChanged);
    connect(m_account, &AccountBackend::skinReady,
            this, &ShadowBackend::skinReady);
    connect(m_account, &AccountBackend::offlineHistoryChanged,
            this, &ShadowBackend::offlineHistoryChanged);
    connect(m_account, &AccountBackend::logMessage,
            this, &ShadowBackend::logMessage);
    // Persist username on every login
    connect(m_account, &AccountBackend::accountChanged,
            this, [this]() {
                QString u = m_account->username();
                if (!u.isEmpty()) {
                    QSettings s(QCoreApplication::organizationName(),
                                QCoreApplication::applicationName());
                    s.setValue(QStringLiteral("account/lastUsername"), u);
                }
            });

    // ── Signal forwarding: SettingsBackend → ShadowBackend ──
    connect(m_settings, &SettingsBackend::javaPathChanged,
            this, &ShadowBackend::javaPathChanged);
    connect(m_settings, &SettingsBackend::memorySettingsChanged,
            this, &ShadowBackend::memorySettingsChanged);
    connect(m_settings, &SettingsBackend::generalSettingsChanged,
            this, &ShadowBackend::generalSettingsChanged);
    connect(m_settings, &SettingsBackend::isolationChanged,
            this, &ShadowBackend::isolationChanged);
    connect(m_settings, &SettingsBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Signal forwarding: CheckBackend → ShadowBackend ──
    connect(m_check, &CheckBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Signal forwarding: VersionBackend → ShadowBackend ──
    connect(m_version, &VersionBackend::versionListReady,
            this, &ShadowBackend::versionListReady);
    connect(m_version, &VersionBackend::installedVersionsChanged,
            this, &ShadowBackend::installedVersionsChanged);
    connect(m_version, &VersionBackend::selectedVersionChanged,
            this, &ShadowBackend::selectedVersionChanged);
    connect(m_version, &VersionBackend::installStateChanged,
            this, &ShadowBackend::installStateChanged);
    connect(m_version, &VersionBackend::installProgressChanged,
            this, &ShadowBackend::installProgressChanged);
    connect(m_version, &VersionBackend::installTotalChanged,
            this, &ShadowBackend::installTotalChanged);
    // installFileProgress signal with different signature:
    // VersionBackend emits installFileProgress(const QString&), ShadowBackend emits installFileProgress()
    connect(m_version, &VersionBackend::installFileProgress,
            this, [this](const QString&) { emit installFileProgress(); });
    connect(m_version, &VersionBackend::installBytesProgress,
            this, &ShadowBackend::installBytesProgress);
    // installPhaseChanged signal with different signature:
    // VersionBackend emits installPhaseChanged(const QString&), ShadowBackend emits installPhaseChanged()
    connect(m_version, &VersionBackend::installPhaseChanged,
            this, [this](const QString&) {
                emit installPhaseChanged();
                emit verifyRunningChanged();
            });
    connect(m_version, &VersionBackend::installFinished,
            this, &ShadowBackend::installFinished);
    connect(m_version, &VersionBackend::installSpeedChanged,
            this, &ShadowBackend::installSpeedChanged);
    connect(m_version, &VersionBackend::installPausedChanged,
            this, [this](bool) { emit installPausedChanged(); });
    connect(m_version, &VersionBackend::logMessage,
            this, &ShadowBackend::logMessage);
    connect(m_version, &VersionBackend::verifyStarted,
            this, &ShadowBackend::verifyStarted);
    connect(m_version, &VersionBackend::verifyProgress,
            this, [this](int checked, int total) {
                emit verifyProgress(checked, total);
                emit verifyCheckedChanged();
                emit verifyTotalChanged();
            });
    connect(m_version, &VersionBackend::verifyFinished,
            this, &ShadowBackend::verifyFinished);
    connect(m_version, &VersionBackend::verifyFailedFiles,
            this, [this](const QStringList& failedFiles) {
                // Generate error report
                if (!failedFiles.isEmpty()) {
                    QDir reportsDir(m_gameDir + QStringLiteral("/verify_reports"));
                    if (!reportsDir.exists()) reportsDir.mkpath(QStringLiteral("."));
                    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
                    m_verifyReportPath = reportsDir.filePath(
                        QStringLiteral("verify_failed_%1.txt").arg(timestamp));
                    QFile report(m_verifyReportPath);
                    if (report.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream out(&report);
                        out << QStringLiteral("Shadow Launcher — 版本完整性校验报告\n");
                        out << QStringLiteral("======================================\n");
                        out << QStringLiteral("时间: ") << QDateTime::currentDateTime().toString(Qt::ISODate) << QStringLiteral("\n");
                        out << QStringLiteral("版本: ") << m_version->selectedVersion() << QStringLiteral("\n");
                        out << QStringLiteral("异常文件数: ") << failedFiles.size() << QStringLiteral("\n");
                        out << QStringLiteral("--------------------------------------\n");
                        for (int i = 0; i < failedFiles.size(); ++i) {
                            out << (i + 1) << QStringLiteral(". ") << failedFiles[i] << QStringLiteral("\n");
                        }
                        out << QStringLiteral("--------------------------------------\n");
                        out << QStringLiteral("请使用「一键修复」重新下载损坏/缺失的文件。\n");
                    }
                } else {
                    m_verifyReportPath.clear();
                }
                emit verifyFailedFiles(failedFiles);
            });

    // ── Signal forwarding: LaunchBackend → ShadowBackend ──
    connect(m_launch, &LaunchBackend::launchProgressChanged,
            this, &ShadowBackend::launchProgressChanged);
    connect(m_launch, &LaunchBackend::launchStateChanged,
            this, &ShadowBackend::launchStateChanged);
    connect(m_launch, &LaunchBackend::minecraftStarted,
            this, &ShadowBackend::minecraftStarted);
    connect(m_launch, &LaunchBackend::minecraftStopped,
            this, &ShadowBackend::minecraftStopped);
    connect(m_launch, &LaunchBackend::isRunningChanged,
            this, &ShadowBackend::isRunningChanged);
    connect(m_launch, &LaunchBackend::runningCountChanged,
            this, &ShadowBackend::runningCountChanged);
    connect(m_launch, &LaunchBackend::logMessage,
            this, &ShadowBackend::logMessage);
    connect(m_launch, &LaunchBackend::launchCheckProgress,
            this, &ShadowBackend::launchCheckProgress);
    connect(m_launch, &LaunchBackend::launchCheckFailed,
            this, &ShadowBackend::launchCheckFailed);
    connect(m_launch, &LaunchBackend::launchCheckMissingFiles,
            this, &ShadowBackend::launchCheckMissingFiles);
    connect(m_launch, &LaunchBackend::launchCheckWarning,
            this, &ShadowBackend::launchCheckWarning);

    // ── Load persisted login mode ──
    {
        QSettings s(QCoreApplication::organizationName(),
                    QCoreApplication::applicationName());
        m_lastLoginMode = s.value(QStringLiteral("account/lastLoginMode"), 1).toInt();
        QString lastUser = s.value(QStringLiteral("account/lastUsername")).toString();
        if ((m_lastLoginMode == 1 || m_lastLoginMode == 0) && !lastUser.isEmpty()) {
            // Auto-restore login on startup
            QTimer::singleShot(100, this, [this, lastUser]() {
                m_account->offlineLogin(lastUser);
            });
        }
    }

    // ── Signal forwarding: ResourceBackend → ShadowBackend ──
    connect(m_resource, &ResourceBackend::downloadStateChanged,
            this, &ShadowBackend::resourceDownloadStateChanged);
    connect(m_resource, &ResourceBackend::downloadProgressChanged,
            this, &ShadowBackend::resourceDownloadProgress);
    // downloadFinished → resourceDownloadDone
    connect(m_resource, &ResourceBackend::downloadFinished,
            this, [this](const QString&, bool success, const QString&) {
                emit resourceDownloadDone(success);
            });
    connect(m_resource, &ResourceBackend::searchResultsReady,
            this, &ShadowBackend::searchResultsReady);
    connect(m_resource, &ResourceBackend::resourcepackSearchCompleted,
            this, &ShadowBackend::resourcepackSearchCompleted);
    connect(m_resource, &ResourceBackend::resourcepackSearchFailed,
            this, &ShadowBackend::resourcepackSearchFailed);
    connect(m_resource, &ResourceBackend::resourcepackDownloadFinished,
            this, &ShadowBackend::resourcepackDownloadFinished);
    connect(m_resource, &ResourceBackend::resourcepackVersionsLoaded,
            this, &ShadowBackend::resourcepackVersionsLoaded);
    connect(m_resource, &ResourceBackend::resourcepackVersionsPartial,
            this, &ShadowBackend::resourcepackVersionsPartial);
    connect(m_resource, &ResourceBackend::resourcepackVersionsProgress,
            this, &ShadowBackend::resourcepackVersionsProgress);
    connect(m_resource, &ResourceBackend::logMessage,
            this, &ShadowBackend::logMessage);

    // ── Signal forwarding: AppBackend → ShadowBackend ──
    connect(m_app, &AppBackend::gameDirChanged,
            this, &ShadowBackend::gameDirChanged);
    connect(m_app, &AppBackend::themeChanged,
            this, &ShadowBackend::themeChanged);
    connect(m_app, &AppBackend::logMessage,
            this, &ShadowBackend::logMessage);
}

// ============================================================
// Account property getters
// ============================================================

QString ShadowBackend::username() const {
    return m_account->username();
}

bool ShadowBackend::isOnline() const {
    return m_account->isOnline();
}

QString ShadowBackend::accountUuid() const {
    return m_account->accountUuid();
}

QString ShadowBackend::skinPath() const {
    return m_account->skinPath();
}

QStringList ShadowBackend::offlineUsernames() const {
    return m_account->offlineUsernames();
}

// ============================================================
// Settings property getters
// ============================================================

QString ShadowBackend::javaPath() const {
    return m_settings->javaPath();
}

QString ShadowBackend::javaVersion() const {
    return m_settings->javaVersion();
}

int ShadowBackend::javaMajor() const {
    return m_settings->javaMajor();
}

bool ShadowBackend::javaInstalled() const {
    return m_settings->isJavaReady();
}

int ShadowBackend::minMemoryMb() const {
    return m_settings->minMemoryMB();
}

int ShadowBackend::maxMemoryMb() const {
    return m_settings->maxMemoryMB();
}

bool ShadowBackend::closeAfterLaunch() const {
    return m_settings->closeAfterLaunch();
}

bool ShadowBackend::isolationEnabled() const {
    return m_isolationEnabled;
}

QVariantList ShadowBackend::availableJavaList() const {
    return m_settings->availableJavaList();
}

int ShadowBackend::lastLoginMode() const {
    return m_lastLoginMode;
}

void ShadowBackend::setLastLoginMode(int mode) {
    if (m_lastLoginMode != mode) {
        m_lastLoginMode = mode;
        // Persist
        QSettings s(QCoreApplication::organizationName(),
                    QCoreApplication::applicationName());
        s.setValue(QStringLiteral("account/lastLoginMode"), mode);
        emit loginModeChanged();
    }
}

// ============================================================
// Version property getters
// ============================================================

QString ShadowBackend::selectedVersion() const {
    return m_version->selectedVersion();
}

QStringList ShadowBackend::versionIds() const {
    return m_version->versionIds();
}

QVariantList ShadowBackend::versionList() const {
    return m_version->versionInfoList();
}

QStringList ShadowBackend::installedVersions() const {
    return m_version->installedIds();
}

bool ShadowBackend::isInstalling() const {
    return m_version->isInstalling();
}

int ShadowBackend::installProgress() const {
    return m_version->installProgress();
}

int ShadowBackend::installTotal() const {
    return m_version->installTotal();
}

QString ShadowBackend::installFile() const {
    return m_version->installFile();
}

bool ShadowBackend::verifyRunning() const {
    return m_version && m_version->installPhase() == QStringLiteral("verifying");
}

int ShadowBackend::verifyChecked() const {
    return m_version ? m_version->verifyChecked() : 0;
}

int ShadowBackend::verifyTotal() const {
    return m_version ? m_version->verifyTotal() : 0;
}

bool ShadowBackend::installPaused() const {
    return m_version ? m_version->isInstallPaused() : false;
}

QString ShadowBackend::installVersionId() const {
    return m_version->installVersionId();
}

QString ShadowBackend::installPhase() const {
    return m_version->installPhase();
}

qint64 ShadowBackend::installSpeed() const {
    return m_version ? m_version->installSpeed() : 0;
}

qint64 ShadowBackend::installBytesDownloaded() const {
    return m_version ? m_version->installBytesDownloaded() : 0;
}

qint64 ShadowBackend::installBytesTotal() const {
    return m_version ? m_version->installBytesTotal() : 0;
}

QStringList ShadowBackend::releaseVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type == QStringLiteral("release")) list.append(v.id);
    }
    return list;
}

QStringList ShadowBackend::snapshotVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type == QStringLiteral("snapshot")) list.append(v.id);
    }
    return list;
}

QStringList ShadowBackend::oldVersions() const {
    QStringList list;
    auto versions = m_version->cachedMcVersions();
    for (const auto& v : versions) {
        if (v.type != QStringLiteral("release") && v.type != QStringLiteral("snapshot"))
            list.append(v.id);
    }
    return list;
}

void ShadowBackend::refreshVersionDetails()
{
    QDir gameDir(m_app->gameDir());
    QString versionsPath = gameDir.absoluteFilePath(QStringLiteral("versions"));
    QDir versionsDir(versionsPath);

    m_versionDetails.clear();
    if (!versionsDir.exists()) {
        emit versionDetailsReady();
        return;
    }

    // Map of known MC versions to their types (from cached manifest)
    QMap<QString, QString> knownTypes;
    auto cached = m_version->cachedMcVersions();
    for (const auto& v : cached) {
        knownTypes[v.id] = v.type;
    }

    const QStringList entries = versionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& versionId : entries) {
        QString verPath = versionsDir.filePath(versionId);
        QString jarPath = verPath + QStringLiteral("/") + versionId + QStringLiteral(".jar");
        if (!QFileInfo::exists(jarPath)) continue;

        QVariantMap detail;
        detail[QStringLiteral("id")] = versionId;

        // Version type from manifest or heuristic
        QString vtype = knownTypes.value(versionId, QString());
        if (vtype.isEmpty()) {
            // Heuristic: versions containing 'w' followed by number are snapshots
            if (versionId.contains(QRegularExpression(QStringLiteral("\\dw\\d|alpha|beta|inf"))))
                vtype = QStringLiteral("old");
            else
                vtype = QStringLiteral("release");
        }
        detail[QStringLiteral("versionType")] = vtype;

        // Detect mod loader
        QString loaderType = QStringLiteral("原版");
        QString loaderVersion;
        // Check for Forge
        QString forgeDir = verPath + QStringLiteral("/mods");
        if (QDir(forgeDir).exists()) {
            // Check for Fabric
            QString fabricJson = verPath + QStringLiteral("/fabric-installer.json");
            if (QFileInfo::exists(fabricJson)) {
                loaderType = QStringLiteral("Fabric");
            } else {
                loaderType = QStringLiteral("Forge");
            }
        }
        // Check for NeoForge
        if (QFileInfo::exists(verPath + QStringLiteral("/neoforge"))) {
            loaderType = QStringLiteral("NeoForge");
        }
        // Check for Quilt
        if (QFileInfo::exists(verPath + QStringLiteral("/quilt"))) {
            loaderType = QStringLiteral("Quilt");
        }
        detail[QStringLiteral("loaderType")] = loaderType;
        detail[QStringLiteral("loaderVersion")] = loaderVersion;

        // Compute total size
        qint64 totalSize = 0;
        QDirIterator it(verPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            totalSize += it.fileInfo().size();
        }
        detail[QStringLiteral("sizeBytes")] = totalSize;

        // Count mods
        int modCount = 0;
        if (QDir(forgeDir).exists()) {
            QDirIterator modIt(forgeDir, QStringList() << QStringLiteral("*.jar"), QDir::Files);
            while (modIt.hasNext()) { modIt.next(); modCount++; }
        }
        detail[QStringLiteral("modCount")] = modCount;

        m_versionDetails.append(detail);
    }

    emit versionDetailsReady();
    emit logMessage(QStringLiteral("已扫描 %1 个已安装版本").arg(m_versionDetails.size()));
}

void ShadowBackend::refreshGameDirInfo()
{
    QDir gameDir(m_app->gameDir());
    QVariantMap info;

    // Count installed versions
    QString versionsPath = gameDir.absoluteFilePath(QStringLiteral("versions"));
    QDir versionsDir(versionsPath);
    int versionCount = 0;
    qint64 totalSize = 0;

    if (versionsDir.exists()) {
        const QStringList entries = versionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& versionId : entries) {
            QString jarPath = versionsDir.filePath(versionId + QStringLiteral("/") + versionId + QStringLiteral(".jar"));
            if (QFileInfo::exists(jarPath)) versionCount++;

            QDirIterator it(versionsDir.filePath(versionId), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) { it.next(); totalSize += it.fileInfo().size(); }
        }
    }
    info[QStringLiteral("versionCount")] = versionCount;

    // Count mods
    QString modsPath = gameDir.absoluteFilePath(QStringLiteral("mods"));
    QDir modsDir(modsPath);
    int modCount = 0;
    if (modsDir.exists()) {
        QDirIterator modIt(modsPath, QStringList() << QStringLiteral("*.jar"), QDir::Files);
        while (modIt.hasNext()) { modIt.next(); modCount++; }
    }
    info[QStringLiteral("modCount")] = modCount;

    // Format size
    QString sizeDisplay;
    if (totalSize >= 1073741824) {
        sizeDisplay = QString::number(totalSize / 1073741824.0, 'f', 2) + QStringLiteral(" GB");
    } else if (totalSize >= 1048576) {
        sizeDisplay = QString::number(totalSize / 1048576.0, 'f', 1) + QStringLiteral(" MB");
    } else {
        sizeDisplay = QString::number(totalSize / 1024.0, 'f', 0) + QStringLiteral(" KB");
    }
    info[QStringLiteral("sizeDisplay")] = sizeDisplay;

    m_gameDirInfo = info;
    emit gameDirChanged();
}

QVariantMap ShadowBackend::systemMemoryInfo() const {
    return m_settings->getMemoryStatus();
}

// ============================================================
// Launch property getters
// ============================================================

bool ShadowBackend::isLaunching() const {
    return m_launch->isLaunching();
}

int ShadowBackend::launchProgress() const {
    return m_launch->launchProgress();
}

QString ShadowBackend::launchStatus() const {
    return m_launch->launchStatus();
}

bool ShadowBackend::isRunning() const {
    return m_launch->isRunning();
}

int ShadowBackend::runningCount() const {
    return m_launch->runningCount();
}

// ============================================================
// Resource property getters
// ============================================================

bool ShadowBackend::isResourceDownloading() const {
    return m_resource->isDownloading();
}

// ============================================================
// App property getters
// ============================================================

QString ShadowBackend::gameDir() const {
    return m_app->gameDir();
}

QString ShadowBackend::appDataDir() const {
    return m_app->dataDir();
}

QString ShadowBackend::theme() const {
    return m_app->theme();
}

bool ShadowBackend::devMode() const {
    return m_app->devMode();
}

// ============================================================
// Q_INVOKABLE methods — Account
// ============================================================

void ShadowBackend::offlineLogin(const QString& username) {
    m_account->offlineLogin(username);
}

void ShadowBackend::logout() {
    m_account->logout();
}

QString ShadowBackend::getSkinUrl(const QString& username) const {
    return m_account->getSkinUrl(username);
}

// ============================================================
// Q_INVOKABLE methods — Settings / Java
// ============================================================

QVariantList ShadowBackend::scanJavaInstallations() {
    return m_settings->scanJavaInstallations();
}

QString ShadowBackend::autoSelectJava() {
    return m_settings->autoSelectJava();
}

QString ShadowBackend::detectJava() {
    return m_settings->autoSelectJava();
}

QString ShadowBackend::jvmArgs() const {
    return m_app->jvmArgs();
}

void ShadowBackend::setJvmArgs(const QString& args) {
    m_app->setJvmArgs(args);
    emit logMessage(QStringLiteral("[JVM] GC参数已更新: %1").arg(args));
    emit jvmArgsChanged();
}

QString ShadowBackend::browseJava() {
    return m_settings->browseJava();
}

void ShadowBackend::selectJavaByIndex(int index) {
    m_settings->selectJavaByIndex(index);
}

QVariantMap ShadowBackend::getMemoryStatus() {
    return m_settings->getMemoryStatus();
}

void ShadowBackend::setMinMemory(int mb) {
    m_settings->setMinMemory(mb);
}

void ShadowBackend::setMaxMemory(int mb) {
    m_settings->setMaxMemory(mb);
}

void ShadowBackend::setIsolationEnabled(bool enabled) {
    m_isolationEnabled = enabled;
    m_settings->setIsolationEnabled(enabled);
}

QString ShadowBackend::getVersionGameDir(const QString& versionId) const {
    return m_settings->getVersionGameDir(versionId);
}

void ShadowBackend::migrateVersionToIsolated(const QString& versionId) {
    m_settings->migrateVersionToIsolated(versionId);
}

qint64 ShadowBackend::diskFree() const
{
    QStorageInfo storage(m_app->gameDir());
    if (storage.isValid() && storage.bytesAvailable() > 0) {
        return storage.bytesAvailable();
    }
    // Fallback: query root drive
    QStorageInfo root(QDir::rootPath());
    return root.isValid() ? root.bytesAvailable() : 100LL * 1024 * 1024 * 1024;
}

int ShadowBackend::diskPercent() const
{
    QStorageInfo storage(m_app->gameDir());
    if (storage.isValid() && storage.bytesTotal() > 0) {
        return static_cast<int>(100.0 * (1.0 - static_cast<double>(storage.bytesAvailable()) / storage.bytesTotal()));
    }
    QStorageInfo root(QDir::rootPath());
    if (root.isValid() && root.bytesTotal() > 0) {
        return static_cast<int>(100.0 * (1.0 - static_cast<double>(root.bytesAvailable()) / root.bytesTotal()));
    }
    return 30;
}

void ShadowBackend::openGameDir() {
    m_settings->openGameDir();
}

void ShadowBackend::openLatestLog() {
    QString logsDir = m_app->gameDir() + QStringLiteral("/logs");
    QString latestLog = logsDir + QStringLiteral("/latest.log");
    QDir().mkpath(logsDir);
    if (QFileInfo::exists(latestLog)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(latestLog));
        emit logMessage(QStringLiteral("已打开最新日志"));
    } else {
        // Open the logs folder if latest.log doesn't exist
        QDesktopServices::openUrl(QUrl::fromLocalFile(logsDir));
        emit logMessage(QStringLiteral("日志文件不存在，已打开日志目录"));
    }
}

void ShadowBackend::openLogsFolder() {
    QString logsDir = m_app->gameDir() + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(logsDir));
    emit logMessage(QStringLiteral("已打开日志目录"));
}

void ShadowBackend::openVersionDir(const QString& versionId) {
    m_settings->openVersionDir(versionId);
}

void ShadowBackend::deleteVersion(const QString& versionId) {
    m_settings->deleteVersion(versionId);
}

// ============================================================
// Q_INVOKABLE methods — Version
// ============================================================

void ShadowBackend::refreshVersionList() {
    m_version->refreshVersionList();
}

void ShadowBackend::refreshInstalled() {
    m_version->refreshInstalled();
}

void ShadowBackend::refreshInstalledList() {
    m_version->refreshInstalled();
}

void ShadowBackend::installVersion(const QString& versionId, int sourceIndex) {
    m_version->installVersion(versionId, sourceIndex);
}

QVariantList ShadowBackend::getMirrorSources() {
    QVariantList result;
    const auto mirrors = MirrorSource::allMirrors();
    for (int i = 0; i < mirrors.size(); ++i) {
        const auto& m = mirrors[i];
        QVariantMap item;
        item[QStringLiteral("index")] = i;
        item[QStringLiteral("name")] = m.name;
        item[QStringLiteral("desc")] = m.desc;
        item[QStringLiteral("isDefault")] = m.isDefault;
        item[QStringLiteral("attribution")] = m.attribution;
        item[QStringLiteral("isAvailable")] = m.isAvailable;
        result.append(item);
    }
    return result;
}

QString ShadowBackend::bmclapiComplianceNotice() {
    return QString::fromUtf8(kBMCLAPIComplianceNotice);
}

void ShadowBackend::cancelInstall() {
    m_version->cancelInstall();
}

void ShadowBackend::pauseInstall() {
    m_version->pauseInstall();
}

void ShadowBackend::resumeInstall() {
    m_version->resumeInstall();
}

void ShadowBackend::cancelQueuedDownload(const QString& versionId) {
    if (m_version) m_version->cancelQueuedDownload(versionId);
}

void ShadowBackend::setSelectedVersion(const QString& versionId) {
    m_version->setSelectedVersion(versionId);
}

// ============================================================
// Q_INVOKABLE methods — Launch
// ============================================================

void ShadowBackend::launch(const QString& versionId) {
    QString username = m_account->username();
    QString javaPath = m_settings->javaPath();
    int maxMemory = m_settings->maxMemoryMB();
    QString jvmArgs = m_app->jvmArgs();
    m_launchVersion = versionId;
    m_launchUsername = username;
    m_launch->launch(versionId, username, javaPath, maxMemory, jvmArgs);
}

void ShadowBackend::cancelLaunch() {
    m_launch->cancelLaunch();
}

void ShadowBackend::killGameProcess() {
    m_launch->killGameProcess();
}

void ShadowBackend::killMinecraft() {
    m_launch->killGameProcess();
}

void ShadowBackend::killGameById(int index) {
    m_launch->killGameById(index);
}

QVariantList ShadowBackend::runningGames() {
    return m_launch->runningGames();
}

int ShadowBackend::getAutoMemory() {
    return m_launch->getAutoMemory();
}

// ============================================================
// Q_INVOKABLE methods — Resource
// ============================================================

QVariantList ShadowBackend::getPopularMods(const QString& loader) {
    return m_resource->getPopularMods(loader);
}

QVariantList ShadowBackend::getShaderList() {
    return m_resource->getShaderList();
}

void ShadowBackend::searchMods(const QString& query, const QString& loader) {
    m_resource->searchMods(query, loader);
}

void ShadowBackend::downloadMod(const QString& slug, const QString& loader) {
    m_resource->downloadMod(slug, loader);
}

void ShadowBackend::downloadShader(const QString& slug) {
    m_resource->downloadShader(slug);
}

void ShadowBackend::searchResourcepacks(const QString& query, const QString& gameVersion, int offset) {
    m_resource->searchResourcepacks(query, gameVersion, offset);
}

void ShadowBackend::downloadResourcepack(const QString& slug, const QString& gameVersion) {
    m_resource->downloadResourcepack(slug, gameVersion);
}

void ShadowBackend::fetchResourcepackVersions(const QStringList& slugs) {
    m_resource->fetchResourcepackVersions(slugs);
}

// ============================================================
// fetchResourcepackVersions
// Q_INVOKABLE methods — App
// ============================================================

void ShadowBackend::setTheme(const QString& theme) {
    m_app->setTheme(theme);
}

void ShadowBackend::setGameDir(const QString& dir) {
    m_app->setGameDir(dir);
    // Sync all backends to the new directory
    m_version->setGameDir(dir);
    m_settings->setMinecraftDir(dir);
    m_settings->setIsolationGameDir(dir);
    m_launch->setGameDir(dir);
}

// ============================================================
// Q_INVOKABLE methods — Version management
// ============================================================

void ShadowBackend::verifyVersion(const QString& versionId) {
    m_version->verifyVersion(versionId);
}

void ShadowBackend::cleanCorruptVersion(const QString& versionId) {
    m_version->cleanCorruptVersion(versionId);
}

bool ShadowBackend::renameVersion(const QString& oldId, const QString& newId) {
    return m_version->renameVersion(oldId, newId);
}

bool ShadowBackend::cloneVersion(const QString& sourceId, const QString& newId) {
    return m_version->cloneVersion(sourceId, newId);
}

QString ShadowBackend::copyVersionPath(const QString& versionId) {
    return m_version->copyVersionPath(versionId);
}

void ShadowBackend::repairVersion(const QString& versionId) {
    m_version->repairVersion(versionId);
}

void ShadowBackend::openVerifyReport() {
    if (!m_verifyReportPath.isEmpty() && QFile::exists(m_verifyReportPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_verifyReportPath));
    }
}

// ============================================================
// Q_INVOKABLE methods — Check
// ============================================================

QVariantMap ShadowBackend::checkAll(const QString& versionId) {
    return m_check->checkAll(versionId,
                             m_settings->javaPath(),
                             m_settings->maxMemoryMB(),
                             m_app->gameDir());
}

} // namespace ShadowLauncher
