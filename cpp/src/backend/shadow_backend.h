#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace ShadowLauncher {

// Forward declarations of all 7 backends
class AppBackend;
class AccountBackend;
class SettingsBackend;
class CheckBackend;
class VersionBackend;
class LaunchBackend;
class ResourceBackend;

class ShadowBackend : public QObject {
    Q_OBJECT
    // --- Account properties (delegated from AccountBackend) ---
    Q_PROPERTY(QString username READ username NOTIFY accountChanged)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY accountChanged)
    Q_PROPERTY(QString accountUuid READ accountUuid NOTIFY accountChanged)
    Q_PROPERTY(QString skinPath READ skinPath NOTIFY skinReady)
    Q_PROPERTY(QStringList offlineUsernames READ offlineUsernames NOTIFY offlineHistoryChanged)
    Q_PROPERTY(int lastLoginMode READ lastLoginMode WRITE setLastLoginMode NOTIFY loginModeChanged)

    // --- Settings properties ---
    Q_PROPERTY(QString javaPath READ javaPath NOTIFY javaPathChanged)
    Q_PROPERTY(QString javaVersion READ javaVersion NOTIFY javaPathChanged)
    Q_PROPERTY(int javaMajor READ javaMajor NOTIFY javaPathChanged)
    Q_PROPERTY(int minMemoryMb READ minMemoryMb NOTIFY memorySettingsChanged)
    Q_PROPERTY(int maxMemoryMb READ maxMemoryMb NOTIFY memorySettingsChanged)
    Q_PROPERTY(bool closeAfterLaunch READ closeAfterLaunch NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool isolationEnabled READ isolationEnabled NOTIFY isolationChanged)
    Q_PROPERTY(QVariantList availableJavaList READ availableJavaList CONSTANT)

    // --- Version properties ---
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY selectedVersionChanged)
    Q_PROPERTY(QStringList versionIds READ versionIds NOTIFY versionListReady)
    Q_PROPERTY(QVariantList versionList READ versionList NOTIFY versionListReady)
    Q_PROPERTY(QStringList installedVersions READ installedVersions NOTIFY installedVersionsChanged)
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installStateChanged)
    Q_PROPERTY(int installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(int installTotal READ installTotal NOTIFY installTotalChanged)
    Q_PROPERTY(QString installFile READ installFile NOTIFY installFileProgress)
    Q_PROPERTY(QString installVersionId READ installVersionId NOTIFY installStateChanged)
    Q_PROPERTY(QString installPhase READ installPhase NOTIFY installPhaseChanged)

    // --- Launch properties ---
    Q_PROPERTY(bool launching READ isLaunching NOTIFY launchStateChanged)
    Q_PROPERTY(int launchProgress READ launchProgress NOTIFY launchProgressChanged)
    Q_PROPERTY(QString launchStatus READ launchStatus NOTIFY launchProgressChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)

    // --- Resource properties ---
    Q_PROPERTY(bool downloading READ isResourceDownloading NOTIFY resourceDownloadStateChanged)

    // --- App properties ---
    Q_PROPERTY(QString gameDir READ gameDir NOTIFY gameDirChanged)
    Q_PROPERTY(QString dataDir READ appDataDir CONSTANT)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(bool devMode READ devMode CONSTANT)

public:
    explicit ShadowBackend(QObject* parent = nullptr);

    // All getters (delegate to sub-backends)
    QString username() const;
    bool isOnline() const;
    QString accountUuid() const;
    QString skinPath() const;
    QStringList offlineUsernames() const;
    QString javaPath() const;
    QString javaVersion() const;
    int javaMajor() const;
    int minMemoryMb() const;
    int maxMemoryMb() const;
    bool closeAfterLaunch() const;
    bool isolationEnabled() const;
    QVariantList availableJavaList() const;
    int lastLoginMode() const;
    void setLastLoginMode(int mode);
    QString selectedVersion() const;
    QStringList versionIds() const;
    QVariantList versionList() const;
    QStringList installedVersions() const;
    bool isInstalling() const;
    int installProgress() const;
    int installTotal() const;
    QString installFile() const;
    QString installVersionId() const;
    QString installPhase() const;
    bool isLaunching() const;
    int launchProgress() const;
    QString launchStatus() const;
    bool isRunning() const;
    bool isResourceDownloading() const;
    QString gameDir() const;
    QString appDataDir() const;
    QString theme() const;
    bool devMode() const;

    // Q_INVOKABLE methods — directly delegate to sub-backends
    Q_INVOKABLE void offlineLogin(const QString& username);
    Q_INVOKABLE void logout();
    Q_INVOKABLE QString getSkinUrl(const QString& username = {}) const;
    Q_INVOKABLE QVariantList scanJavaInstallations();
    Q_INVOKABLE QString autoSelectJava();
    Q_INVOKABLE QString detectJava();  // alias for autoSelectJava
    Q_INVOKABLE QString browseJava();
    Q_INVOKABLE QVariantMap getMemoryStatus();
    Q_INVOKABLE void setMinMemory(int mb);
    Q_INVOKABLE void setMaxMemory(int mb);
    Q_INVOKABLE void setIsolationEnabled(bool enabled);
    Q_INVOKABLE void openGameDir();
    Q_INVOKABLE void openVersionDir(const QString& versionId);
    Q_INVOKABLE void deleteVersion(const QString& versionId);
    Q_INVOKABLE void refreshVersionList();
    Q_INVOKABLE void refreshInstalled();
    Q_INVOKABLE void refreshInstalledList();
    Q_INVOKABLE void installVersion(const QString& versionId, int sourceIndex = 0);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE void launch(const QString& versionId);
    Q_INVOKABLE void cancelLaunch();
    Q_INVOKABLE void killGameProcess();
    Q_INVOKABLE void killMinecraft();
    Q_INVOKABLE QVariantList getPopularMods(const QString& loader);
    Q_INVOKABLE QVariantList getShaderList();
    Q_INVOKABLE void searchMods(const QString& query, const QString& loader = {});
    Q_INVOKABLE void downloadMod(const QString& slug, const QString& loader);
    Q_INVOKABLE void downloadShader(const QString& slug);
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void setTheme(const QString& theme);
    Q_INVOKABLE QVariantMap checkAll(const QString& versionId);
    Q_INVOKABLE void setGameDir(const QString& dir);
    Q_INVOKABLE int getAutoMemory();

signals:
    void accountChanged();
    void skinReady();
    void offlineHistoryChanged();
    void javaPathChanged();
    void memorySettingsChanged();
    void generalSettingsChanged();
    void isolationChanged();
    void versionListReady();
    void installedVersionsChanged();
    void selectedVersionChanged();
    void installStateChanged();
    void installProgressChanged();
    void installTotalChanged();
    void installFileProgress();
    void installBytesProgress(qint64 dl, qint64 total);
    void installPhaseChanged();
    void installFinished(bool success);
    void launchProgressChanged(int progress, const QString& status);
    void launchStateChanged();
    void minecraftStarted();
    void minecraftStopped();
    void isRunningChanged();
    void resourceDownloadStateChanged();
    void resourceDownloadProgress(int completed, int total, const QString& fileName);
    void resourceDownloadDone(bool success);
    void searchResultsReady(const QVariantList& results);
    void gameDirChanged();
    void themeChanged();
    void loginModeChanged();
    void logMessage(const QString& msg);

public:
    // Direct access to sub-backends (for main.cpp setup)
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

    bool m_isolationEnabled = false;
    int m_lastLoginMode = 1;  // 0=online, 1=offline (default)
    QString m_gameDir;
};
}
