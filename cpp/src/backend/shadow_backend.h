#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace ShadowLauncher {

class AppBackend;
class AccountBackend;
class SettingsBackend;
class CheckBackend;
class VersionBackend;
class LaunchBackend;
class ResourceBackend;

class ShadowBackend : public QObject {
    Q_OBJECT
    // ── Account ──
    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY accountChanged)
    Q_PROPERTY(QString accountUuid READ accountUuid NOTIFY accountChanged)
    Q_PROPERTY(QString skinPath READ skinPath NOTIFY skinReady)
    Q_PROPERTY(QStringList offlineUsernames READ offlineUsernames NOTIFY offlineHistoryChanged)
    Q_PROPERTY(int lastLoginMode READ lastLoginMode WRITE setLastLoginMode NOTIFY loginModeChanged)

    // ── Settings ──
    Q_PROPERTY(QString javaPath READ javaPath NOTIFY javaPathChanged)
    Q_PROPERTY(QString javaVersion READ javaVersion NOTIFY javaPathChanged)
    Q_PROPERTY(int javaMajor READ javaMajor NOTIFY javaPathChanged)
    Q_PROPERTY(bool javaInstalled READ javaInstalled NOTIFY javaPathChanged)
    Q_PROPERTY(bool javaReady READ javaInstalled NOTIFY javaPathChanged)
    Q_PROPERTY(int minMemoryMb READ minMemoryMb NOTIFY memorySettingsChanged)
    Q_PROPERTY(int maxMemoryMb READ maxMemoryMb NOTIFY memorySettingsChanged)
    Q_PROPERTY(bool closeAfterLaunch READ closeAfterLaunch NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool isolationEnabled READ isolationEnabled NOTIFY isolationChanged)
    Q_PROPERTY(QVariantList availableJavaList READ availableJavaList NOTIFY javaPathChanged)

    // ── Version ──
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY selectedVersionChanged)
    Q_PROPERTY(QStringList versionIds READ versionIds NOTIFY versionListReady)
    Q_PROPERTY(QVariantList versionList READ versionList NOTIFY versionListReady)
    Q_PROPERTY(QStringList releaseVersions READ releaseVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList snapshotVersions READ snapshotVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList oldVersions READ oldVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList installedVersions READ installedVersions NOTIFY installedVersionsChanged)
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installStateChanged)
    Q_PROPERTY(int installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(int installTotal READ installTotal NOTIFY installTotalChanged)
    Q_PROPERTY(QString installFile READ installFile NOTIFY installFileProgress)
    Q_PROPERTY(QString installVersionId READ installVersionId NOTIFY installStateChanged)
    Q_PROPERTY(QString installPhase READ installPhase NOTIFY installPhaseChanged)
    Q_PROPERTY(qint64 installSpeed READ installSpeed NOTIFY installSpeedChanged)
    Q_PROPERTY(qint64 installBytesDownloaded READ installBytesDownloaded NOTIFY installBytesProgress)
    Q_PROPERTY(qint64 installBytesTotal READ installBytesTotal NOTIFY installBytesProgress)
    Q_PROPERTY(bool installPaused READ installPaused NOTIFY installPausedChanged)

    // ── Launch ──
    Q_PROPERTY(bool launching READ isLaunching NOTIFY launchStateChanged)
    Q_PROPERTY(int launchProgress READ launchProgress NOTIFY launchProgressChanged)
    Q_PROPERTY(QString launchStatus READ launchStatus NOTIFY launchProgressChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY runningCountChanged)
    Q_PROPERTY(QString launchVersion READ launchVersion NOTIFY launchStateChanged)
    Q_PROPERTY(QString launchUsername READ launchUsername NOTIFY launchStateChanged)

    // ── Resource ──
    Q_PROPERTY(bool downloading READ isResourceDownloading NOTIFY resourceDownloadStateChanged)

    // ── App ──
    Q_PROPERTY(QString gameDir READ gameDir NOTIFY gameDirChanged)
    Q_PROPERTY(QString dataDir READ appDataDir CONSTANT)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool devMode READ devMode CONSTANT)

    // ── Version details/summary ──
    Q_PROPERTY(QVariantList versionDetails READ versionDetails NOTIFY versionDetailsReady)
    Q_PROPERTY(QString currentVersionSummary READ currentVersionSummary CONSTANT)

    // ── Download queue ──
    Q_PROPERTY(QVariantList downloadQueue READ downloadQueue NOTIFY downloadQueueChanged)
    Q_PROPERTY(QVariantList activeDownloads READ activeDownloads NOTIFY downloadQueueChanged)

    // ── Game info ──
    Q_PROPERTY(QVariantMap gameDirInfo READ gameDirInfo NOTIFY gameDirChanged)
    Q_PROPERTY(QVariantList gameDirectories READ gameDirectories CONSTANT)
    Q_PROPERTY(qint64 diskFree READ diskFree CONSTANT)
    Q_PROPERTY(int diskPercent READ diskPercent CONSTANT)
    Q_PROPERTY(bool closeOnLaunch READ closeOnLaunch NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool autoMemoryEnabled READ autoMemoryEnabled NOTIFY memorySettingsChanged)
    Q_PROPERTY(QVariantMap systemMemoryInfo READ systemMemoryInfo NOTIFY memorySettingsChanged)
    Q_PROPERTY(QString gameArgs READ gameArgs CONSTANT)
    Q_PROPERTY(QString jvmArgs READ jvmArgs NOTIFY jvmArgsChanged)
    Q_PROPERTY(QString javaCompatibility READ javaCompatibility CONSTANT)

    // ── Verify ──
    Q_PROPERTY(bool verifyRunning READ verifyRunning NOTIFY verifyRunningChanged)
    Q_PROPERTY(int verifyChecked READ verifyChecked NOTIFY verifyCheckedChanged)
    Q_PROPERTY(int verifyTotal READ verifyTotal NOTIFY verifyTotalChanged)
    Q_PROPERTY(bool verifyResultOk READ verifyResultOk CONSTANT)
    Q_PROPERTY(QString verifyResultText READ verifyResultText CONSTANT)

public:
    explicit ShadowBackend(QObject* parent = nullptr);

    // ── Account getters ──
    QString username() const;
    bool isOnline() const;
    QString accountUuid() const;
    QString skinPath() const;
    QStringList offlineUsernames() const;
    int lastLoginMode() const;
    void setLastLoginMode(int mode);

    // ── Settings getters ──
    QString javaPath() const;
    QString javaVersion() const;
    int javaMajor() const;
    bool javaInstalled() const;
    int minMemoryMb() const;
    int maxMemoryMb() const;
    bool closeAfterLaunch() const;
    bool isolationEnabled() const;
    QVariantList availableJavaList() const;

    // ── Version getters ──
    QString selectedVersion() const;
    QStringList versionIds() const;
    QVariantList versionList() const;
    QStringList releaseVersions() const;
    QStringList snapshotVersions() const;
    QStringList oldVersions() const;
    QStringList installedVersions() const;
    bool isInstalling() const;
    int installProgress() const;
    int installTotal() const;
    QString installFile() const;
    QString installVersionId() const;
    QString installPhase() const;
    qint64 installSpeed() const;
    qint64 installBytesDownloaded() const;
    qint64 installBytesTotal() const;
    QVariantList versionDetails() const { return m_versionDetails; }
    QString currentVersionSummary() const { return {}; }

    // ── Download queue ──
    QVariantList downloadQueue() const;
    QVariantList activeDownloads() const;

    // ── Launch getters ──
    bool isLaunching() const;
    int launchProgress() const;
    QString launchStatus() const;
    bool isRunning() const;
    int runningCount() const;
    QString launchVersion() const { return m_launchVersion; }
    QString launchUsername() const { return m_launchUsername; }
    void setLaunchVersion(const QString& v) { m_launchVersion = v; }
    void setLaunchUsername(const QString& u) { m_launchUsername = u; }

    // ── Resource getters ──
    bool isResourceDownloading() const;

    // ── App getters ──
    QString gameDir() const;
    QString appDataDir() const;
    QString theme() const;
    bool devMode() const;

    // ── Game info stubs ──
    QVariantMap gameDirInfo() const { return m_gameDirInfo; }
    QVariantList gameDirectories() const { return {}; }
    qint64 diskFree() const;
    int diskPercent() const;
    bool closeOnLaunch() const { return m_closeOnLaunch; }
    bool autoMemoryEnabled() const { return true; }
    QVariantMap systemMemoryInfo() const;
    QString gameArgs() const { return {}; }
    QString jvmArgs() const;
    QString javaCompatibility() const { return QStringLiteral("OK"); }

    // ── Verify stubs ──
    bool verifyRunning() const;
    int verifyChecked() const;
    int verifyTotal() const;
    bool installPaused() const;
    int verifyProgressDone() const { return 0; }
    int verifyProgressTotal() const { return 0; }
    bool verifyResultOk() const { return true; }
    QString verifyResultText() const { return {}; }
    QString verifyVersion() const { return {}; }

    // ── Mod/file list stubs ──
    Q_INVOKABLE QVariantList listMods() const { return {}; }
    Q_INVOKABLE QVariantList listResourcePacks() const { return {}; }
    Q_INVOKABLE QVariantList listSaves() const { return {}; }

    // ── Q_INVOKABLE methods ──
    Q_INVOKABLE void offlineLogin(const QString& username);
    Q_INVOKABLE void logout();
    Q_INVOKABLE QString getSkinUrl(const QString& username = {}) const;
    Q_INVOKABLE QVariantList scanJavaInstallations();
    Q_INVOKABLE QString autoSelectJava();
    Q_INVOKABLE QString detectJava();
    Q_INVOKABLE QString browseJava();
    Q_INVOKABLE void selectJavaByIndex(int index);
    Q_INVOKABLE QVariantMap getMemoryStatus();
    Q_INVOKABLE void setMinMemory(int mb);
    Q_INVOKABLE void setMaxMemory(int mb);
    Q_INVOKABLE void setIsolationEnabled(bool enabled);
    Q_INVOKABLE QString getVersionGameDir(const QString& versionId) const;
    Q_INVOKABLE void migrateVersionToIsolated(const QString& versionId);
    Q_INVOKABLE void openGameDir();
    Q_INVOKABLE void openVersionDir(const QString& versionId);
    Q_INVOKABLE void deleteVersion(const QString& versionId);
    Q_INVOKABLE void refreshVersionList();
    Q_INVOKABLE void refreshInstalled();
    Q_INVOKABLE void refreshInstalledList();
    Q_INVOKABLE void refreshVersionDetails();
    Q_INVOKABLE void refreshGameDirInfo();
    Q_INVOKABLE void installVersion(const QString& versionId, int sourceIndex = 0);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE QVariantList getMirrorSources();
    Q_INVOKABLE QString bmclapiComplianceNotice();
    Q_INVOKABLE void launch(const QString& versionId);
    Q_INVOKABLE void cancelLaunch();
    Q_INVOKABLE void killGameProcess();
    Q_INVOKABLE void killMinecraft();
    Q_INVOKABLE void killGameById(int index);
    Q_INVOKABLE QVariantList runningGames();
    Q_INVOKABLE QVariantList getPopularMods(const QString& loader);
    Q_INVOKABLE QVariantList getShaderList();
    Q_INVOKABLE void searchMods(const QString& query, const QString& loader = {});
    Q_INVOKABLE void downloadMod(const QString& slug, const QString& loader);
    Q_INVOKABLE void downloadShader(const QString& slug);
    Q_INVOKABLE void searchResourcepacks(const QString& query, const QString& gameVersion = {}, int offset = 0);
    Q_INVOKABLE void downloadResourcepack(const QString& slug, const QString& gameVersion);
    Q_INVOKABLE void fetchResourcepackVersions(const QStringList& slugs);  // batch-fetch game_versions
    Q_INVOKABLE void cacheIconAsync(const QString& webpUrl);  // async: download webp → ffmpeg → PNG, emits iconCached
    Q_INVOKABLE QString cachedIconPath(const QString& webpUrl) const;  // sync: check cache, return file:/// or ""
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void setTheme(const QString& theme);
    Q_INVOKABLE QVariantMap checkAll(const QString& versionId);
    Q_INVOKABLE void setGameDir(const QString& dir);
    Q_INVOKABLE int getAutoMemory();
    Q_INVOKABLE void setCloseOnLaunch(bool v) { m_closeOnLaunch = v; emit generalSettingsChanged(); }
    Q_INVOKABLE void setAutoMemoryEnabled(bool) {}  // stub
    Q_INVOKABLE void setJvmArgs(const QString& args);
    Q_INVOKABLE void setGameArgs(const QString&) {}  // stub

    // ── Q_INVOKABLE versions of Q_PROPERTY-only getters ──
    Q_INVOKABLE bool isModdedVersion(const QString& versionId) const { Q_UNUSED(versionId); return false; }

    // ── Path/document stubs ──
    Q_INVOKABLE void openJavaFileDialog() { browseJava(); }
    Q_INVOKABLE void pickJava() { browseJava(); }
    Q_INVOKABLE void checkFileChanges() {}
    Q_INVOKABLE void deleteMod(const QString&) {}
    Q_INVOKABLE void deleteResourcePack(const QString&) {}
    Q_INVOKABLE void deleteSave(const QString&) {}
    Q_INVOKABLE void migrateVersion(const QString&) {}
    Q_INVOKABLE void openConfigFolder() { openGameDir(); }
    Q_INVOKABLE void openCrashLog() { openGameDir(); }
    Q_INVOKABLE void openLatestLog();
    Q_INVOKABLE void openLogsFolder();
    Q_INVOKABLE void openModsFolder() {}
    Q_INVOKABLE void openResourcePacksFolder() {}
    Q_INVOKABLE void openSavesFolder() {}
    Q_INVOKABLE void openScreenshotsFolder() {}
    Q_INVOKABLE void openShaderPacksFolder() {}
    Q_INVOKABLE void removeGameDir(int) {}
    Q_INVOKABLE void pauseInstall();
    Q_INVOKABLE void resumeInstall();
    Q_INVOKABLE void cancelQueuedDownload(const QString& versionId);
    Q_INVOKABLE void verifyVersion(const QString& versionId);
    Q_INVOKABLE void cleanCorruptVersion(const QString& versionId);
    Q_INVOKABLE bool renameVersion(const QString& oldId, const QString& newId);
    Q_INVOKABLE bool cloneVersion(const QString& sourceId, const QString& newId);
    Q_INVOKABLE QString copyVersionPath(const QString& versionId);
    Q_INVOKABLE void repairVersion(const QString& versionId);
    Q_INVOKABLE void openVerifyReport();

signals:
    void accountChanged();
    void skinReady();
    void offlineHistoryChanged();
    void javaPathChanged();
    void javaReadyChanged();
    void memorySettingsChanged();
    void jvmArgsChanged();
    void generalSettingsChanged();
    void isolationChanged();
    void versionListReady();
    void versionDetailsReady();
    void installedVersionsChanged();
    void selectedVersionChanged();
    void installStateChanged();
    void installProgressChanged();
    void installTotalChanged();
    void installFileProgress();
    void installBytesProgress(qint64 dl, qint64 total);
    void installPhaseChanged();
    void installFinished(bool success);
    void installSpeedChanged();
    void launchProgressChanged(int progress, const QString& status);
    void launchStateChanged();
    void minecraftStarted();
    void minecraftStopped();
    void isRunningChanged();
    void runningCountChanged();
    void resourceDownloadStateChanged();
    void resourcepackSearchCompleted(const QVariantList& results, int totalHits);
    void resourcepackSearchFailed(const QString& error);
    void resourcepackDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackVersionsLoaded(const QVariantMap& slugToVersions);
    void resourcepackVersionsPartial(const QString& slug, const QStringList& versions);
    void resourcepackVersionsProgress(int done, int total);
    void resourceDownloadProgress(int completed, int total, const QString& fileName);
    void resourceDownloadDone(bool success);
    void verifyRunningChanged();
    void verifyCheckedChanged();
    void verifyTotalChanged();
    void installPausedChanged();
    void downloadQueueChanged();
    void searchResultsReady(const QVariantList& results);
    void gameDirChanged();
    void themeChanged();
    void loginModeChanged();
    void logMessage(const QString& msg);

    // ── Icon cache signal ──
    void iconCached(const QString& webpUrl, const QString& pngPath);

    // ── Auto-test navigation signal ──
    // pageIndex: 0=Launch, 1=Download, 2=Settings
    // subTab: for Download page — 0=MC, 1=Mod, 2=Shader, 3=RP; otherwise ignored
    void navigateToRequested(int pageIndex, int subTab);

    // ── Verify signals ──
    void verifyStarted();
    void verifyProgress(int checked, int total);
    void verifyFinished(bool allPassed);
    void verifyFailedFiles(const QStringList& failedFiles);

    // ── Pre-launch check signals ──
    void launchCheckProgress(const QString& step);
    void launchCheckFailed(const QString& phase, const QString& details);
    void launchCheckMissingFiles(const QStringList& files);
    void launchCheckWarning(const QString& warning);

public:
    AppBackend* app() const { return m_app; }
    AccountBackend* account() const { return m_account; }
    SettingsBackend* settings() const { return m_settings; }
    CheckBackend* check() const { return m_check; }
    VersionBackend* version() const { return m_version; }
    LaunchBackend* launchBackend() const { return m_launch; }
    ResourceBackend* resource() const { return m_resource; }

private:
    AppBackend* m_app = nullptr;
    AccountBackend* m_account = nullptr;
    SettingsBackend* m_settings = nullptr;
    CheckBackend* m_check = nullptr;
    VersionBackend* m_version = nullptr;
    LaunchBackend* m_launch = nullptr;
    ResourceBackend* m_resource = nullptr;

    bool m_isolationEnabled = true;
    int m_lastLoginMode = 1;
    QString m_launchVersion;
    QString m_launchUsername;
    QString m_verifyReportPath;
    bool m_closeOnLaunch = false;
    QString m_gameDir;
    QVariantMap m_gameDirInfo;
    QVariantList m_versionDetails;
};
}
