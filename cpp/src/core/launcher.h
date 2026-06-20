#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QJsonObject>

namespace ShadowLauncher {

class Launcher : public QObject {
    Q_OBJECT

public:
    explicit Launcher(QObject* parent = nullptr);
    ~Launcher() override;

    // ---- API ----
    void start(const QString& versionId, const QString& javaPath,
               int maxMemoryMB, const QString& jvmArgs = {});
    void cancel();
    void killProcess();
    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }

    // ---- Configuration ----
    void setGameDir(const QString& dir) { m_gameDir = dir; }
    QString gameDir() const { return m_gameDir; }
    void setJvmArgs(const QString& args) { m_jvmArgs = args; }
    QString jvmArgs() const { return m_jvmArgs; }
    void setAuthInfo(const QString& username, const QString& uuid, const QString& accessToken, bool isOnline) {
        m_authName = username;
        m_authUuid = uuid;
        m_authToken = accessToken;
        m_isOnline = isOnline;
    }

signals:
    void launchProgress(const QString& message);
    void launchFinished(bool success, const QString& errorMsg);
    void launchStarted();

private slots:
    void onProcessStarted();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    bool validateLaunch(const QString& versionId, const QString& javaPath, QString& errorMsg) const;
    void forceKill();
    QStringList buildArgs(const QString& versionId, int maxMemoryMB, const QJsonObject& versionJson) const;
    bool extractNatives(const QString& versionId, const QJsonObject& versionJson);
    void ensureOptionsTxt(const QString& versionId);
    static bool evaluateRules(const QJsonArray& rules);
    static bool evaluateRule(const QJsonObject& rule);
    void ensureLegacyAssets();

    QProcess* m_process = nullptr;
    QString m_gameDir;
    QString m_currentVersionId;
    QString m_jvmArgs;
    QString m_authName;
    QString m_authUuid;
    QString m_authToken;
    bool m_isOnline = false;
    bool m_cancelling = false;
};

} // namespace ShadowLauncher
