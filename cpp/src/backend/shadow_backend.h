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
class ModManager;

class ShadowBackend : public QObject {
    Q_OBJECT
    // ── Account ──
    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    Q_PROPERTY(QString offlineUsername READ offlineUsername NOTIFY accountChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY accountChanged)
    Q_PROPERTY(QString accountUuid READ accountUuid NOTIFY accountChanged)
    Q_PROPERTY(QString offlineUuid READ offlineUuid NOTIFY accountChanged)
    Q_PROPERTY(QString skinPath READ skinPath NOTIFY skinReady)
    Q_PROPERTY(QString offlineSkinPath READ offlineSkinPath NOTIFY offlineSkinReady)
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
    Q_PROPERTY(QVariantMap lastCrash READ lastCrash NOTIFY crashDetected)
    Q_PROPERTY(QString launchStatus READ launchStatus NOTIFY launchProgressChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY runningCountChanged)
    Q_PROPERTY(QString launchVersion READ launchVersion NOTIFY launchStateChanged)
    Q_PROPERTY(QString launchUsername READ launchUsername NOTIFY launchStateChanged)

    // ── Resource ──
    Q_PROPERTY(bool downloading READ isResourceDownloading NOTIFY resourceDownloadStateChanged)
    Q_PROPERTY(int resourceDownloadProgress READ resourceDownloadProgress NOTIFY resourceDownloadProgress)
    Q_PROPERTY(int resourceDownloadTotal READ resourceDownloadTotal NOTIFY resourceDownloadProgress)
    Q_PROPERTY(QString resourceDownloadFile READ resourceDownloadFile NOTIFY resourceDownloadProgress)

    Q_PROPERTY(QObject* modManager READ modManager CONSTANT)

    // ── App ──
    Q_PROPERTY(QString gameDir READ gameDir NOTIFY gameDirChanged)
    Q_PROPERTY(QString dataDir READ appDataDir CONSTANT)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool devMode READ devMode CONSTANT)

    // ── Version details/summary ──
    Q_PROPERTY(QVariantList versionDetails READ versionDetails NOTIFY versionDetailsReady)
    Q_PROPERTY(QVariantMap currentVersionSummary READ currentVersionSummary NOTIFY currentVersionSummaryChanged)

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
    QString offlineUsername() const;
    bool isOnline() const;
    QString accountUuid() const;
    QString offlineUuid() const;
    QString skinPath() const;
    QString offlineSkinPath() const;
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
    QVariantMap currentVersionSummary() const { return m_currentVersionSummary; }

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

    // ── Crash getters ──
    QVariantMap lastCrash() const { return m_lastCrash; }

    // ── Resource getters ──
    bool isResourceDownloading() const;
    int resourceDownloadProgress() const { return m_resourceDlProgress; }
    int resourceDownloadTotal() const { return m_resourceDlTotal; }
    QString resourceDownloadFile() const { return m_resourceDlFile; }

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
    Q_INVOKABLE void updateOfflineSkin(const QString& username);
    Q_INVOKABLE void microsoftLogin();
    Q_INVOKABLE void cancelMicrosoftLogin();
    Q_INVOKABLE void logout();
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
    Q_INVOKABLE bool openGameDir(const QString& versionId = {});
    Q_INVOKABLE bool openLatestLog(const QString& versionId = {});
    Q_INVOKABLE bool openLogsFolder(const QString& versionId = {});
    Q_INVOKABLE bool openCrashLog(const QString& versionId = {});
    Q_INVOKABLE bool openSavesFolder(const QString& versionId = {});
    Q_INVOKABLE bool openScreenshotsFolder(const QString& versionId = {});
    Q_INVOKABLE bool openModsFolder(const QString& versionId = {});
    Q_INVOKABLE bool openResourcePacksFolder(const QString& versionId = {});
    Q_INVOKABLE bool openShaderPacksFolder(const QString& versionId = {});
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
    Q_INVOKABLE void launch(const QString& versionId, bool online);
    Q_INVOKABLE void cancelLaunch();
    Q_INVOKABLE void killGameProcess();
    Q_INVOKABLE void killMinecraft();
    Q_INVOKABLE void killGameById(int index);
    Q_INVOKABLE QVariantList runningGames();
    Q_INVOKABLE QVariantList getPopularMods(const QString& loader);
    Q_INVOKABLE QVariantList getShaderList();
    Q_INVOKABLE void searchMods(const QString& query, const QString& loader = {});
    Q_INVOKABLE void searchModsEx(const QString& query, const QString& loader,
        const QString& category, const QString& gameVersion,
        const QString& environment, const QString& license,
        int offset, int limit);
    Q_INVOKABLE QVariantMap getModCategories();
    Q_INVOKABLE void searchShaders(const QString& query, const QString& gameVersion = {});
    Q_INVOKABLE void downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void searchResourcepacks(const QString& query, const QString& gameVersion = {}, int offset = 0, const QStringList& categories = {});
    Q_INVOKABLE void downloadResourcepack(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void fetchResourcepackVersions(const QStringList& slugs);  // batch-fetch game_versions
    Q_INVOKABLE void fetchModVersions(const QStringList& slugs);
    Q_INVOKABLE void fetchShaderVersions(const QStringList& slugs);

    // Mod file download
    Q_INVOKABLE int downloadModFile(const QString& url, const QString& savePath, const QString& displayName,
                                    qint64 expectedSize, const QString& sha1, qint64 receivedOffset = 0, int resumeId = -1);
    Q_INVOKABLE void cancelModFileDownload(int downloadId);
    Q_INVOKABLE void pauseModFileDownload(int downloadId);
    Q_INVOKABLE void resumeModFileDownload(int downloadId);
    Q_INVOKABLE void retryModFileDownload(int downloadId);
    Q_INVOKABLE void cacheIconAsync(const QString& webpUrl);  // async: download webp �?ffmpeg �?PNG, emits iconCached
    Q_INVOKABLE QString cachedIconPath(const QString& webpUrl) const;  // sync: check cache, return file:/// or ""

    // ── Mod Loader version queries (BMCLAPI) ──
    Q_INVOKABLE void queryForgeVersions(const QString& mcVersion);
    Q_INVOKABLE void queryFabricVersions(const QString& mcVersion);
    Q_INVOKABLE void queryNeoForgeVersions(const QString& mcVersion);
    Q_INVOKABLE void queryOptifineVersions(const QString& mcVersion);

    // Mod loader installation
    Q_INVOKABLE void installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName);
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void setTheme(const QString& theme);
    Q_INVOKABLE QVariantMap checkAll(const QString& versionId);
    Q_INVOKABLE void setGameDir(const QString& dir);
    Q_INVOKABLE int getAutoMemory();
    Q_INVOKABLE void setCloseOnLaunch(bool v) { m_closeOnLaunch = v; emit generalSettingsChanged(); }
    Q_INVOKABLE void setAutoMemoryEnabled(bool) {}  // stub
    Q_INVOKABLE void setJvmArgs(const QString& args);
    Q_INVOKABLE void setGameArgs(const QString&) {}  // stub
    Q_INVOKABLE void copyToClipboard(const QString& text);

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
    Q_INVOKABLE void openConfigFolder() { openGameDir({}); }
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
    void microsoftLoginProgress(const QString& step, const QString& detail);
    void microsoftLoginSuccess(const QString& username, const QString& uuid);
    void microsoftLoginFailed(const QString& error);
    void skinReady();
    void offlineSkinReady();
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
    void currentVersionSummaryChanged();
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
    // ── Crash detection ──
    void crashDetected(const QVariantMap& report);
    void isRunningChanged();
    void runningCountChanged();
    void resourceDownloadStateChanged();
    void resourcepackSearchCompleted(const QVariantList& results, int totalHits);
    void resourcepackSearchFailed(const QString& error);
    void resourcepackDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackVersionsLoaded(const QVariantMap& slugToVersions);
    void resourcepackVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void resourcepackVersionsProgress(int done, int total);

    void modVersionsLoaded(const QVariantMap& slugToVersions);
    void modVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void modVersionsProgress(int done, int total);

    // Mod file download signals
    void modFileDownloadStarted(int downloadId, const QString& fileName, qint64 fileSize, const QString& displayName);
    void modFileDownloadProgress(int downloadId, qint64 received, qint64 total);
    void modFileDownloadFinished(int downloadId, bool success, const QString& filePath, const QString& displayName);
    void modFileDownloadFailed(int downloadId, const QString& errorDetail, const QString& displayName);

    void shaderVersionsLoaded(const QVariantMap& slugToVersions);
    void shaderVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void shaderVersionsProgress(int done, int total);
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

    // ── Mod loader version signals ──
    void forgeVersionsReady(const QVariantList& versions);
    void fabricVersionsReady(const QVariantList& versions);
    void neoforgeVersionsReady(const QVariantList& versions);
    void optifineVersionsReady(const QVariantList& versions);

    // ── Icon cache signal ──
    void iconCached(const QString& webpUrl, const QString& pngPath);

    // ── Auto-test navigation signal ──
    // pageIndex: 0=Launch, 1=Download, 2=Settings
    // subTab: for Download page �?0=MC, 1=Mod, 2=Shader, 3=RP; otherwise ignored
    void navigateToRequested(int pageIndex, int subTab);

    // ── Auto-test: open RP detail page ──
    void openRpDetailRequested(const QString& slug);
    void openModDetailRequested(const QString& slug);
    void openShaderDetailRequested(const QString& slug);

    // ── Auto-test: UI interaction signals ──
    void setRpShowPreReleases(bool show);
    void openRpVersionMenu();
    void expandRpDetailGroup(const QString& major);
    void selectRpDetailSubVer(const QString& version);

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
    QObject* modManager() const;  // QML exposed (returns m_resource->modManager())

private:
    int requiredJavaMajor(const QString& versionId);
    QString gameDirForVersion(const QString& versionId) const;

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
    QVariantMap m_lastCrash;
    QString m_verifyReportPath;
    bool m_closeOnLaunch = false;
    QString m_gameDir;
    QVariantMap m_gameDirInfo;
    QVariantList m_versionDetails;
    QVariantMap m_currentVersionSummary;

    // ── Resource download progress tracking ──
    int m_resourceDlProgress = 0;
    int m_resourceDlTotal = 0;
    QString m_resourceDlFile;
};
}
