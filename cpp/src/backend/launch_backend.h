#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace ShadowLauncher {

class Launcher;

class LaunchBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool launching READ isLaunching NOTIFY launchStateChanged)
    Q_PROPERTY(int launchProgress READ launchProgress NOTIFY launchProgressChanged)
    Q_PROPERTY(QString launchStatus READ launchStatus NOTIFY launchProgressChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)

public:
    explicit LaunchBackend(QObject* parent = nullptr);
    ~LaunchBackend() override;

    bool isLaunching() const { return m_launching; }
    int launchProgress() const { return m_launchProgress; }
    QString launchStatus() const { return m_launchStatus; }
    bool isRunning() const;

    // ---- Slots ----
    Q_INVOKABLE void launch(const QString& versionId, const QString& username,
                            const QString& javaPath, int maxMemoryMB);
    Q_INVOKABLE void cancelLaunch();
    Q_INVOKABLE void killGameProcess();
    Q_INVOKABLE int getAutoMemory();
    Q_INVOKABLE int getSystemMemory();
    Q_INVOKABLE QVariantMap getMemoryStatus();

    // ---- Called by ShadowBackend to sync game directory ----
    void setGameDir(const QString& dir);

signals:
    void launchProgressChanged(int progress, const QString& status);
    void launchStateChanged();
    void minecraftStarted();
    void minecraftStopped();
    void isRunningChanged();
    void logMessage(const QString& msg);

private slots:
    void onLaunchStarted();
    void onLaunchProgress(const QString& message);
    void onLaunchFinished(bool success, const QString& errorMsg);

private:
    Launcher* m_launcher = nullptr;
    bool m_launching = false;
    int m_launchProgress = 0;
    QString m_launchStatus;
    bool m_cancelled = false;
};

} // namespace ShadowLauncher
