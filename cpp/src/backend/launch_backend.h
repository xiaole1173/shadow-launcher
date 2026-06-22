#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QList>

namespace ShadowLauncher {

class AccountBackend;
class Launcher;
class CrashDetector;

class LaunchBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool launching READ isLaunching NOTIFY launchStateChanged)
    Q_PROPERTY(int launchProgress READ launchProgress NOTIFY launchProgressChanged)
    Q_PROPERTY(QString launchStatus READ launchStatus NOTIFY launchProgressChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY runningCountChanged)

public:
    explicit LaunchBackend(QObject* parent = nullptr);
    ~LaunchBackend() override;

    bool isLaunching() const { return m_launching; }
    int launchProgress() const { return m_launchProgress; }
    QString launchStatus() const { return m_launchStatus; }
    bool isRunning() const { return !m_runningLaunchers.isEmpty(); }
    int runningCount() const { return m_runningLaunchers.size(); }

    // ---- Slots ----
    Q_INVOKABLE void launch(const QString& versionId, const QString& username,
                            const QString& javaPath, int maxMemoryMB,
                            const QString& jvmArgs = {});
    Q_INVOKABLE void cancelLaunch();
    Q_INVOKABLE void killGameProcess();  // kill ALL running games
    Q_INVOKABLE void killGameByPid(qint64 pid);  // kill one game by PID
    Q_INVOKABLE QVariantList runningGames() const;  // [{version,pid,displayVersion}, ...]
    Q_INVOKABLE int getAutoMemory();
    Q_INVOKABLE int getSystemMemory();
    Q_INVOKABLE QVariantMap getMemoryStatus();

    // ---- Pre-launch checks ----
    Q_INVOKABLE QString checkJavaArchitecture(const QString& javaPath);
    Q_INVOKABLE bool checkVersionHasNatives(const QString& versionId);
    Q_INVOKABLE QStringList checkVersionMissingNatives(const QString& versionId);
    Q_INVOKABLE QStringList checkVersionLibraries(const QString& versionId);

    // Auth info for online mode
    void setAuthInfo(const QString& username, const QString& uuid,
                    const QString& accessToken, bool isOnline);

    // ---- Called by ShadowBackend to sync game directory ----
    void setGameDir(const QString& dir);
    void setAccount(AccountBackend* account) { m_account = account; }

signals:
    void launchProgressChanged(int progress, const QString& status);
    void launchStateChanged();
    void minecraftStarted();
    void minecraftStopped();
    void crashDetected(const QVariantMap& report);
    void isRunningChanged();
    void runningCountChanged();
    void logMessage(const QString& msg);

    // ── Pre-launch check signals ──
    void launchCheckProgress(const QString& step);
    void launchCheckFailed(const QString& phase, const QString& details);
    void launchCheckMissingFiles(const QStringList& files);
    void launchCheckWarning(const QString& warning);

private slots:
    void onLaunchProgress(const QString& message);

    // ── Async pre-launch check steps ──
    void runNextCheck();
    void abortCheck(const QString& phase, const QString& reason);

private:
    void handleLaunchStarted(Launcher* launcher);
    void handleLaunchFinished(Launcher* launcher, bool success, const QString& errorMsg);

    AccountBackend* m_account = nullptr;
    Launcher* m_activeLauncher = nullptr;  // only accept progress from this launcher
    QList<Launcher*> m_runningLaunchers;
    QString m_gameDir;
    bool m_launching = false;
    int m_launchProgress = 0;
    QString m_launchStatus;
    bool m_cancelled = false;

    // Auth info for online mode
    QString m_authName;
    QString m_authUuid;
    QString m_authToken;
    bool m_authIsOnline = false;

    // ── Async check state ──
    QTimer* m_checkTimer = nullptr;
    int m_checkStep = 0;
    QString m_pendingVersionId;
    QString m_pendingJavaPath;
    int m_pendingMaxMemory = 0;
    QString m_pendingJvmArgs;
};

} // namespace ShadowLauncher
