#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QJsonObject>

namespace ShadowLauncher {

class Launcher : public QObject {
    Q_OBJECT

public:
    explicit Launcher(QObject* parent = nullptr);
    ~Launcher() override;

    // ---- Configuration ----
    void setGameDir(const QString& path) { m_gameDir = path; }
    QString gameDir() const { return m_gameDir; }

    // ---- Launch ----
    /// Start Minecraft process
    /// @param versionId  e.g. "1.21.4"
    /// @param javaPath   Full path to java.exe
    /// @param maxMemory  Max heap in MB
    /// @param versionJson  Parsed version JSON (needed for mainClass, classpath, natives)
    void start(const QString& versionId,
               const QString& javaPath,
               int maxMemory,
               const QJsonObject& versionJson);

    /// Send SIGTERM and force kill process tree
    void cancel();

    /// Whether a Minecraft process is currently running
    bool isRunning() const;

    /// Get process PID (0 if not running)
    qint64 pid() const;

signals:
    /// Log message from the launch process (stdout/stderr lines)
    void launchOutput(const QString& line);

    /// Status update
    void launchProgress(const QString& msg);

    /// Launch completed
    void launchFinished(bool success, const QString& errorMsg);

    /// Launch started
    void launchStarted();

private slots:
    void onProcessStarted();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    /// Build JVM arguments (-Xmx, -Xms, G1GC, etc.)
    QStringList buildJvmArgs(const QString& versionId,
                             int maxMemory,
                             const QJsonObject& versionJson);

    /// Build the classpath from version JSON
    QString buildClasspath(const QJsonObject& versionJson);

    /// Extract native libraries for the version
    QString extractNatives(const QString& versionId, const QJsonObject& versionJson);

    /// Force kill the process tree on Windows
    void forceKillProcessTree();

    QString buildLibPath(const QString& libName, const QJsonObject& versionJson);

    // ---- Members ----
    QProcess* m_process = nullptr;
    QString m_gameDir;
    QString m_versionId;
    bool m_cancelling = false;

    // Buffer for partial lines
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
};

} // namespace ShadowLauncher
