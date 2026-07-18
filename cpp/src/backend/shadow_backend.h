// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QTranslator>
#include <QEvent>
#include <QNetworkReply>
#include <QPointer>

class QQmlEngine;

namespace ShadowLauncher {

class AppBackend;
class AccountBackend;
class SettingsBackend;
class CheckBackend;
class VersionBackend;
class LaunchBackend;
class ResourceBackend;
class StatsBackend;
class JavaBackend;
class UserDataBackend;
class ModManager;
class LocalModManager;
class IconCache;
class MultiplayerManager;
class UpdateManager;

class ShadowBackend : public QObject {
    Q_OBJECT
    // Allow the static helper to access reply tracking members
    friend void queryModLoaderApi(ShadowBackend*, const QString&, const QString&,
                                  std::function<QVariantList(const QByteArray&)>,
                                  std::function<void(const QVariantList&)>);
    // ── Account ──
    Q_PROPERTY(QObject* account READ account CONSTANT)
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
    Q_PROPERTY(bool isolationEnabled READ isolationEnabled NOTIFY isolationChanged)
    Q_PROPERTY(bool embeddedLoginEnabled READ embeddedLoginEnabled WRITE setEmbeddedLoginEnabled NOTIFY embeddedLoginChanged)
    Q_PROPERTY(QVariantList availableJavaList READ availableJavaList NOTIFY javaPathChanged)

    // ── Version ──
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY selectedVersionChanged)
    Q_PROPERTY(QStringList versionIds READ versionIds NOTIFY versionListReady)
    Q_PROPERTY(QVariantList versionList READ versionList NOTIFY versionListReady)
    Q_PROPERTY(QStringList releaseVersions READ releaseVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList snapshotVersions READ snapshotVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList oldVersions READ oldVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList aprilFoolVersions READ aprilFoolVersions NOTIFY versionListReady)
    Q_PROPERTY(QStringList installedVersions READ installedVersions NOTIFY installedVersionsChanged)
    Q_PROPERTY(QStringList activeVersionNames READ activeVersionNames NOTIFY activeVersionNamesChanged)
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installStateChanged)
    Q_PROPERTY(QString installVersionId READ installVersionId NOTIFY installStateChanged)
    Q_PROPERTY(QString installPhase READ installPhase NOTIFY installPhaseChanged)

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
    Q_PROPERTY(QObject* multiplayer READ multiplayer CONSTANT)

    // ── App ──
    Q_PROPERTY(QString gameDir READ gameDir NOTIFY gameDirChanged)
    Q_PROPERTY(QString dataDir READ appDataDir CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool devMode READ devMode CONSTANT)

    // ── Modpack Import ──
    Q_PROPERTY(QObject* modpackImporter READ modpackImporter CONSTANT)

    // ── Update ──
    Q_PROPERTY(bool updateChecking READ updateChecking NOTIFY updateCheckingChanged)
    Q_PROPERTY(int updateState READ updateState NOTIFY updateStateChanged)

    // ── Agreements ──
    Q_PROPERTY(bool agreementAccepted READ agreementAccepted NOTIFY agreementAcceptedChanged)
    Q_PROPERTY(QString betaAgreementHtml READ betaAgreementHtml CONSTANT)
    Q_PROPERTY(QString privacyAgreementHtml READ privacyAgreementHtml CONSTANT)
    Q_PROPERTY(QString termsAgreementHtml READ termsAgreementHtml CONSTANT)
    Q_PROPERTY(bool markAgreed READ isMarkAgreed WRITE setMarkAgreed)
    bool isMarkAgreed() const { return false; }
    void setMarkAgreed(bool v);

    // ── Custom background ──
    Q_PROPERTY(QString customBgPath READ customBgPath NOTIFY customBgChanged)
    Q_PROPERTY(qreal sidebarOpacity READ sidebarOpacity NOTIFY customBgChanged)
    Q_PROPERTY(qreal contentOpacity READ contentOpacity NOTIFY customBgChanged)
    Q_PROPERTY(qreal cropX READ cropX WRITE setCropX NOTIFY customBgChanged)
    Q_PROPERTY(qreal cropY READ cropY WRITE setCropY NOTIFY customBgChanged)

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
    Q_PROPERTY(bool autoMemoryEnabled READ autoMemoryEnabled NOTIFY memorySettingsChanged)
    Q_PROPERTY(QVariantMap systemMemoryInfo READ systemMemoryInfo NOTIFY memorySettingsChanged)
    Q_PROPERTY(QVariantList availableCapes READ availableCapes CONSTANT)
    Q_PROPERTY(QString loginType READ loginType CONSTANT)
    Q_PROPERTY(QString selectedSkinPath READ selectedSkinPath NOTIFY skinChanged)
    Q_PROPERTY(bool wardrobeBusy READ isWardrobeBusy NOTIFY wardrobeBusyChanged)
    Q_PROPERTY(int fileDownloadSource READ fileDownloadSource WRITE setFileDownloadSource NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int listDownloadSource READ listDownloadSource WRITE setListDownloadSource NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int maxDownloadThreads READ maxDownloadThreads WRITE setMaxDownloadThreads NOTIFY downloadSettingsChanged)
    Q_PROPERTY(double downloadSpeedLimitMB READ downloadSpeedLimitMB WRITE setDownloadSpeedLimitMB NOTIFY downloadSettingsChanged)
    Q_PROPERTY(QString jvmArgs READ jvmArgs NOTIFY jvmArgsChanged)
    Q_PROPERTY(QString gameArgs READ gameArgs NOTIFY gameArgsChanged)
    Q_PROPERTY(bool highPerfGpu READ highPerfGpu NOTIFY highPerfGpuChanged)

    // ── Verify ──
    Q_PROPERTY(bool verifyRunning READ verifyRunning NOTIFY verifyRunningChanged)
    Q_PROPERTY(bool repairRunning READ repairRunning NOTIFY repairRunningChanged)
    Q_PROPERTY(int verifyChecked READ verifyChecked NOTIFY verifyCheckedChanged)
    Q_PROPERTY(int verifyTotal READ verifyTotal NOTIFY verifyTotalChanged)
    Q_PROPERTY(bool verifyResultOk READ verifyResultOk NOTIFY verifyResultTextChanged)
    Q_PROPERTY(QString verifyResultText READ verifyResultText NOTIFY verifyResultTextChanged)
    // InstallCardModel (QAbstractListModel) — exposed as QObject* for QML compat
    Q_PROPERTY(QObject* installCardsModel READ installCardsModel CONSTANT)

    // ── Beta key gate ──
    Q_PROPERTY(QString betaStatus READ betaStatus NOTIFY betaStatusChanged)

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
    bool isolationEnabled() const;
    int fileDownloadSource() const;
    void setFileDownloadSource(int v);
    int listDownloadSource() const;
    void setListDownloadSource(int v);
    int maxDownloadThreads() const;
    void setMaxDownloadThreads(int v);
    double downloadSpeedLimitMB() const;
    void setDownloadSpeedLimitMB(double v);
    bool embeddedLoginEnabled() const;
    Q_INVOKABLE void setEmbeddedLoginEnabled(bool v);

    // ── Custom background ──
    QString customBgPath() const;
    Q_INVOKABLE void setCustomBgPath(const QString& path);
    qreal sidebarOpacity() const;
    Q_INVOKABLE void setSidebarOpacity(qreal v);
    qreal contentOpacity() const;
    Q_INVOKABLE void setContentOpacity(qreal v);
    qreal cropX() const;
    Q_INVOKABLE void setCropX(qreal v);
    qreal cropY() const;
    Q_INVOKABLE void setCropY(qreal v);
    Q_INVOKABLE void updateCrop(qreal x, qreal y);
    Q_INVOKABLE QString pickBackgroundImage();

    QVariantList availableJavaList() const;

    // ── Version getters ──
    QString selectedVersion() const;
    QStringList versionIds() const;
    QVariantList versionList() const;
    QStringList releaseVersions() const;
    QStringList snapshotVersions() const;
    QStringList oldVersions() const;
    QStringList aprilFoolVersions() const;
    QStringList installedVersions() const;
    QStringList activeVersionNames() const;
    bool isInstalling() const;
    QString installVersionId() const;
    QString installPhase() const;
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
    QString appVersion() const;
    QString theme() const;
    bool devMode() const;
    bool agreementAccepted() const;
    QString betaAgreementHtml() const;
    QString privacyAgreementHtml() const;
    QString termsAgreementHtml() const;

    // ── Game info stubs ──
    QVariantMap gameDirInfo() const { return m_gameDirInfo; }
    QVariantList gameDirectories() const { return {}; }
    qint64 diskFree() const;
    int diskPercent() const;
    bool autoMemoryEnabled() const;
    QVariantMap systemMemoryInfo() const;
    QString gameArgs() const;
    bool highPerfGpu() const;
    QString jvmArgs() const;

    // ── Language hot-switch ──
    void setEngine(QQmlEngine* engine) { m_engine = engine; }
    Q_INVOKABLE void switchLanguage(int index);
    Q_INVOKABLE int readLanguageFile() const;
    void writeLanguageFile(int index) const;
    bool event(QEvent* event) override;

    // ── Verify stubs ──
    bool verifyRunning() const;
    bool repairRunning() const;
    int verifyChecked() const;
    int verifyTotal() const;
    QObject* installCardsModel() const;
    bool verifyResultOk() const;
    QString verifyResultText() const;
    void setVerifyResult(const QString& text, bool ok);

    // ── Mod/file list stubs ──
    Q_INVOKABLE QVariantList listMods(const QString& versionId = {}) const;
    Q_INVOKABLE QVariantList listResourcePacks(const QString& versionId = {}) const;
    Q_INVOKABLE QVariantList listSaves(const QString& versionId = {}) const;

    // ── Q_INVOKABLE methods ──
    Q_INVOKABLE void offlineLogin(const QString& username);
    Q_INVOKABLE void updateOfflineSkin(const QString& username);
    Q_INVOKABLE void removeOfflineUsername(const QString& username);
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
    Q_INVOKABLE bool openLauncherLogsFolder();
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
    Q_INVOKABLE void installVersion(const QString& versionId);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE void cancelVersionInstall(const QString& versionId);
    Q_INVOKABLE void launch(const QString& versionId, bool online);
    Q_INVOKABLE void cancelLaunch();
    Q_INVOKABLE void killGameProcess();
    Q_INVOKABLE void killMinecraft();
    Q_INVOKABLE void killGameByPid(qint64 pid);
    Q_INVOKABLE QVariantList runningGames();
    Q_INVOKABLE QVariantList getPopularMods(const QString& loader);
    Q_INVOKABLE QVariantList getShaderList();
    Q_INVOKABLE void searchMods(const QString& query, const QString& loader = {});
    Q_INVOKABLE void searchModsEx(const QString& query, const QString& loader,
        const QString& category, const QString& gameVersion,
        const QString& environment, const QString& license,
        int offset, int limit);
    Q_INVOKABLE QVariantMap getModCategories();
    Q_INVOKABLE void searchShadersEx(const QString& query, const QStringList& gameVersions,
        const QStringList& categories, const QStringList& performance,
        const QStringList& loader, int offset, int limit);
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

    // ═══ Wardrobe (衣帽间) ═══
    Q_INVOKABLE void browseSkin();  // File dialog
    Q_INVOKABLE void uploadSkin(const QString& skinPath, int modelType);  // Mojang API
    Q_INVOKABLE void saveWardrobeSettings(const QString& skinPath, const QString& capeId, int modelType);
    Q_INVOKABLE void saveSkinToFile();
    Q_INVOKABLE QVariantList availableCapes() const;
    Q_INVOKABLE QString loginType() const;
    QString selectedSkinPath() const { return m_selectedSkinPath; }
    bool isWardrobeBusy() const { return m_wardrobeBusy; }
    Q_INVOKABLE void cacheIconAsync(const QString& webpUrl);  // async: download webp �?ffmpeg �?PNG, emits iconCached
    Q_INVOKABLE QString cachedIconPath(const QString& webpUrl) const;  // sync: check cache, return file:/// or ""

    // ── Mod Loader version queries (BMCLAPI) ──
    Q_INVOKABLE void queryForgeVersions(const QString& mcVersion);
    Q_INVOKABLE void queryFabricVersions(const QString& mcVersion);
    Q_INVOKABLE void queryNeoForgeVersions(const QString& mcVersion);
    Q_INVOKABLE void queryOptifineVersions(const QString& mcVersion);
    Q_INVOKABLE void queryFabricApiVersions(const QString& mcVersion);

    // Cancel all in-flight mod-loader version queries (called when install starts)
    Q_INVOKABLE void cancelModLoaderQueries();

    // Cache Forge installer SHA1 from version list (avoids extra network fetch for verification)
    Q_INVOKABLE void cacheForgeInstallerSha1(const QString& mcVer, const QString& forgeVer, const QString& sha1);
    QString getForgeInstallerSha1(const QString& mcVer, const QString& forgeVer) const;

    Q_INVOKABLE bool installFabricApi(const QString& version, const QString& url, const QString& savePath);

    // Mod loader installation
    Q_INVOKABLE void installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName,
                                       const QString& fabricApiVersion = QString(),
                                       const QString& fabricApiUrl = QString(),
                                       const QString& fabricApiSavePath = QString(),
                                       const QString& forgeInstallerSha1 = QString());
    Q_INVOKABLE void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                       const QString& forgeVersion, const QString& installName,
                                       const QString& bmclType = QString(), const QString& bmclPatch = QString());
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void setTheme(const QString& theme);
    Q_INVOKABLE QVariantMap checkAll(const QString& versionId);
    Q_INVOKABLE void setGameDir(const QString& dir);
    Q_INVOKABLE int getAutoMemory();
    Q_INVOKABLE void setAutoMemoryEnabled(bool enabled);

    // Per-version memory override
    Q_INVOKABLE int versionMemoryMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionMemoryMode(const QString& versionId, int mode);
    Q_INVOKABLE int versionMemoryManualMB(const QString& versionId) const;
    Q_INVOKABLE void setVersionMemoryManualMB(const QString& versionId, int mb);
    Q_INVOKABLE int resolvedMemoryMB(const QString& versionId);

    // Per-version Java/launch overrides (0=follow global, 1=custom)
    Q_INVOKABLE int versionJavaMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionJavaMode(const QString& versionId, int mode);
    Q_INVOKABLE int versionJvmArgsMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionJvmArgsMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionJvmArgs(const QString& versionId) const;
    Q_INVOKABLE void setVersionJvmArgs(const QString& versionId, const QString& args);
    Q_INVOKABLE QString resolvedJvmArgs(const QString& versionId) const;

    Q_INVOKABLE int versionGameArgsMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionGameArgsMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionGameArgs(const QString& versionId) const;
    Q_INVOKABLE void setVersionGameArgs(const QString& versionId, const QString& args);
    Q_INVOKABLE QString resolvedGameArgs(const QString& versionId) const;

    Q_INVOKABLE int versionHighPerfGpuMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionHighPerfGpuMode(const QString& versionId, int mode);
    Q_INVOKABLE bool versionHighPerfGpu(const QString& versionId) const;
    Q_INVOKABLE void setVersionHighPerfGpu(const QString& versionId, bool v);
    Q_INVOKABLE bool resolvedHighPerfGpu(const QString& versionId) const;

    // Global setters
    Q_INVOKABLE void setJvmArgs(const QString& args);
    Q_INVOKABLE void setGameArgs(const QString& args);
    Q_INVOKABLE void setHighPerfGpu(bool v);
    Q_INVOKABLE void copyToClipboard(const QString& text);

    // ── Q_INVOKABLE versions of Q_PROPERTY-only getters ──
    Q_INVOKABLE bool isModdedVersion(const QString& versionId) const { Q_UNUSED(versionId); return false; }

    // ── Path/document stubs ──
    Q_INVOKABLE void openJavaFileDialog() { browseJava(); }
    Q_INVOKABLE void pickJava() { browseJava(); }
    Q_INVOKABLE void checkFileChanges() {}
    Q_INVOKABLE void deleteMod(const QString& filename, const QString& versionId = {});
    Q_INVOKABLE void deleteResourcePack(const QString& filename, const QString& versionId = {});
    Q_INVOKABLE bool importMod(const QString& filePath, const QString& versionId = {});
    Q_INVOKABLE void deleteSave(const QString& saveName, const QString& versionId = {});
    Q_INVOKABLE void migrateVersion(const QString& versionId);
    Q_INVOKABLE void openConfigFolder() { openGameDir({}); }
    Q_INVOKABLE void removeGameDir(int) {}
    Q_INVOKABLE void cancelQueuedDownload(const QString& versionId);
    Q_INVOKABLE void verifyVersion(const QString& versionId);
    Q_INVOKABLE void cleanCorruptVersion(const QString& versionId);
    Q_INVOKABLE bool renameVersion(const QString& oldId, const QString& newId);
    Q_INVOKABLE bool renameVersion(const QString& oldId);  // single-param stub (dialog handled in QML)
    Q_INVOKABLE bool cloneVersion(const QString& sourceId, const QString& newId);
    Q_INVOKABLE bool cloneVersion(const QString& sourceId);  // auto-generates "{name} (副本)"
    Q_INVOKABLE QString copyVersionPath(const QString& versionId);
    Q_INVOKABLE void repairVersion(const QString& versionId);
    Q_INVOKABLE void cancelVerify();
    Q_INVOKABLE void openVerifyReport();

    // ── Beta key gate ──
    Q_INVOKABLE void submitBetaKey(const QString& key);
    QString betaStatus() const { return m_betaStatus; }
    static QString loadBetaKey();
    static bool validateBetaKey(const QString& key, QString* outError = nullptr);
    static bool saveBetaKey(const QString& key);

    // ── Modpack Import ──
    QObject* modpackImporter() const { return m_modpackImporter; }

    // ── Update ──
    Q_INVOKABLE void checkForUpdate();
    bool updateChecking() const;
    int updateState() const;

signals:
    void betaVerified();
    void betaKeyInvalid(const QString& reason);
    void betaStatusChanged();

    // ── Update signals ──
    void updateCheckingChanged();
    void updateStateChanged();
    void toastMessage(const QString& message);
    void updateDownloadProgress(qint64 received, qint64 total);
    void updateChangelogAvailable(const QString& version, const QString& notes);
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
    void gameArgsChanged();
    void highPerfGpuChanged();
    void versionLaunchSettingsChanged(const QString& versionId);
    void launchBlocked(const QString& reason);
    void generalSettingsChanged();
    void downloadSettingsChanged();
    void isolationChanged();
    void embeddedLoginChanged();
    void versionListReady();
    void versionDetailsReady();
    void installedVersionsChanged();
    void activeVersionNamesChanged();
    void selectedVersionChanged();
    void selectedVersionClearedAfterDelete();
    void currentVersionSummaryChanged();
    void installStateChanged();
    void installPhaseChanged();
    void installFinished(bool success);
    void installComplete(const QString& installName);
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

    // Fabric API signals
    void fabricApiVersionsReady(const QVariantList& versions);

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
    void repairRunningChanged();
    void verifyCheckedChanged();
    void verifyTotalChanged();
    void verifyResultTextChanged();
    void downloadQueueChanged();
    void downloadQueueFull(const QString& displayName);
    void searchResultsReady(const QVariantList& results);  // deprecated
    void modSearchResultsReady(const QVariantList& results);
    void shaderSearchResultsReady(const QVariantList& results);
    void gameDirChanged();
    void themeChanged();
    void agreementAcceptedChanged();
    void loginModeChanged();
    void customBgChanged();
    void logMessage(const QString& msg);
    void skinChanged();
    void wardrobeBusyChanged();
    void wardrobeError(const QString& error);

    // ── Mod loader version signals ──
    void forgeVersionsReady(const QVariantList& versions);
    void fabricVersionsReady(const QVariantList& versions);
    void neoforgeVersionsReady(const QVariantList& versions);
    void optifineVersionsReady(const QVariantList& versions);

    // ── Game stats signals ──
    void statsChanged();
    void statsLoadingChanged();

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
    Q_INVOKABLE SettingsBackend* settings() const { return m_settings; }
    Q_INVOKABLE void logUiMsg(const QString& msg);
    CheckBackend* check() const { return m_check; }
    VersionBackend* version() const { return m_version; }
    LaunchBackend* launchBackend() const { return m_launch; }
    ResourceBackend* resource() const { return m_resource; }
    Q_INVOKABLE StatsBackend* statsBackend() const { return m_stats; }
    Q_INVOKABLE JavaBackend* javaBackend() const { return m_java; }
    Q_INVOKABLE UserDataBackend* userDataBackend() const { return m_userData; }

    // Game stats (exposed directly on backend for QML compatibility)
    Q_PROPERTY(double totalGameHours READ totalGameHours NOTIFY statsChanged)
    Q_PROPERTY(QVariantList versionGameStats READ versionGameStats NOTIFY statsChanged)
    Q_PROPERTY(bool statsLoading READ statsLoading NOTIFY statsLoadingChanged)
    Q_PROPERTY(bool statsEmpty READ statsEmpty NOTIFY statsChanged)
    Q_INVOKABLE void refreshGameStats();
    double totalGameHours() const;
    QVariantList versionGameStats() const;
    bool statsLoading() const;
    bool statsEmpty() const;
    QObject* modManager() const;  // QML exposed (returns m_resource->modManager())
    QObject* multiplayer() const;

    // ── Icon cache (QML-callable methods: mod / shader / rp) ──
    Q_INVOKABLE QString resolveIconUrl(const QString &url);
    Q_INVOKABLE void cacheIconBatchAsync(const QStringList &urls);
    Q_INVOKABLE QString iconCachedPath(const QString &url) const;
    Q_INVOKABLE QString resolveShaderIconUrl(const QString &url);
    Q_INVOKABLE void cacheShaderIconBatchAsync(const QStringList &urls);
    Q_INVOKABLE QString resolveRpIconUrl(const QString &url);
    Q_INVOKABLE void cacheRpIconBatchAsync(const QStringList &urls);


private:
    int requiredJavaMajor(const QString& versionId);
    static int inferJavaByMcVersion(const QString& mcVersion);
    QString gameDirForVersion(const QString& versionId) const;
    void syncPlayerName();
    void checkChangelog();
    void loadJavaRuntimeSettings();
    void saveJvmArgs();
    void saveGameArgs();
    void saveHighPerfGpu();

    AppBackend* m_app = nullptr;
    AccountBackend* m_account = nullptr;
    SettingsBackend* m_settings = nullptr;
    CheckBackend* m_check = nullptr;
    VersionBackend* m_version = nullptr;
    LaunchBackend* m_launch = nullptr;
    ResourceBackend* m_resource = nullptr;
    StatsBackend*   m_stats = nullptr;
    JavaBackend*    m_java = nullptr;
    UserDataBackend* m_userData = nullptr;
    IconCache* m_modIconCache = nullptr;
    IconCache* m_shaderIconCache = nullptr;
    IconCache* m_rpIconCache = nullptr;
    MultiplayerManager* m_multiplayer = nullptr;
    LocalModManager* m_localMods = nullptr;
    class GeoIpService* m_geoIp = nullptr;
    QObject* m_modpackImporter = nullptr;

    bool m_isolationEnabled = true;
    int m_lastLoginMode = 1;
    QString m_launchVersion;
    QString m_launchUsername;

    // ── Beta key ──
    QString m_betaStatus;

    // ── Translation hot-switch ──
    QTranslator* m_translator = nullptr;
    QQmlEngine* m_engine = nullptr;
    QString m_currentLang = QStringLiteral("zh_CN");
    QVariantMap m_lastCrash;
    QString m_verifyReportPath;
    QString m_gameDir;
    QVariantMap m_gameDirInfo;
    QVariantList m_versionDetails;
    QVariantMap m_currentVersionSummary;

    // ── Resource download progress tracking ──
    int m_resourceDlProgress = 0;
    int m_resourceDlTotal = 0;
    QString m_resourceDlFile;
    QMap<int, QString> m_modDownloadCards;  // dlId → cardId for mod file cards

    // Mod-loader query tracking (for cancellation when install starts)
    QList<QPointer<QNetworkReply>> m_modLoaderReplies;
    bool m_modLoaderQueriesCancelled = false;

    // Forge installer SHA1 cache (populated from version list API)
    QMap<QString, QString> m_forgeInstallerSha1Cache;  // "mcVer-forgeVer" -> sha1

    // ── Update ──
    UpdateManager* m_updateManager = nullptr;

    // ── Wardrobe cache ──
    QVariantList m_capeCache;
    void initCapeCache();
    QString m_selectedSkinPath;
    bool m_wardrobeBusy = false;

    // ── Verify results ──
    QString m_verifyResultText;
    bool m_verifyResultOk = true;
};
}
