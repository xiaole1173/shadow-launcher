#pragma once

#include <QObject>
#include <QString>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <functional>

namespace ShadowLauncher {

class ModLoaderInstaller : public QObject {
    Q_OBJECT

public:
    explicit ModLoaderInstaller(QObject* parent = nullptr);
    ~ModLoaderInstaller() override;

    void setGameDir(const QString& dir) { m_gameDir = dir; }
    QString gameDir() const { return m_gameDir; }
    void setModsDir(const QString& dir) { m_modsDir = dir; }
    QString modsDir() const { return m_modsDir.isEmpty() ? m_gameDir + "/mods" : m_modsDir; }

    void installForge(const QString& mcVersion, const QString& forgeVersion, const QString& installName);
    void installFabric(const QString& mcVersion, const QString& fabricVersion, const QString& installName);
    void installNeoForge(const QString& mcVersion, const QString& neoVersion, const QString& installName);
    void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                         const QString& forgeVersion, const QString& installName);

    bool isRunning() const { return m_running; }
    void cancel();

signals:
    // Step-level progress (for phase text)
    void progressChanged(int step, int totalSteps, const QString& description);
    // Byte-level progress (for single-file progress bar)
    void byteProgress(const QString& fileName, qint64 received, qint64 total, qint64 speed);
    // Verification
    void verifyStarted();
    void verifyFinished(bool ok);
    // Final
    void finished(bool success, const QString& error);
    void logMessage(const QString& msg);
    // Pause between verify and install (for parallel MC download)
    void waitingForMC();
    // Sub-progress within a step (for installer stdout parsing)
    void stepProgress(int step, int percentage);

private:
    // Download helpers
    void downloadToFile(const QString& url, const QString& savePath,
                        std::function<void(bool ok, const QString& error)> done);
    void downloadToMemory(const QString& url,
                          std::function<void(bool ok, const QByteArray& data)> done,
                          const QString& fileNameHint = QString());
    void downloadSmall(const QString& url,
                       std::function<void(bool ok, const QByteArray& data)> done);

    bool ensureVanillaInstalled(const QString& mcVersion);
    QString versionsDir() const { return m_gameDir + "/versions"; }
    QString computeSha1(const QByteArray& data);
    void emitByteProgress(const QString& name, qint64 received, qint64 total);

    // Temp .minecraft isolation for OptiFine standalone installer
    QString setupTempMc();
    void collectForgeOutput(const QString& tempMc, const QString& jarPath);
    void cleanupTempMc(const QString& tempDir);
    void copyRecursive(const QString& srcDir, const QString& dstDir);

    // Forge/NeoForge
    void forgeStep1_downloadInstaller();
    void forgeStep2_verify(const QByteArray& jarData);
    void neoStep1_downloadInstaller();
    void neoStep2_verify(const QByteArray& jarData);
    // PCL-style install (extract version.json → download libraries → write config)
    void forgeStep3_PCLinstall(const QByteArray& jarData);
    void neoForgeStep3_PCLinstall(const QByteArray& jarData);
    void runInstallerProcess(const QByteArray& jarData);

    // Fabric
    void fabricStep1_downloadProfile();
    void fabricStep2_verify(const QByteArray& bmclProfile);
    void fabricStep3_writeVersion(const QByteArray& profileData);

    // Optifine standalone
    void optifineStep2_install(const QByteArray& jarData, const QString& filename);

    // ── state ──
    QString m_gameDir;
    QString m_modsDir;
    QString m_mcVersion;
    QString m_loaderVersion;
    QString m_installName;
    QString m_loaderType;
    int m_currentStep = 0;
    int m_totalSteps = 0;
    bool m_running = false;
    bool m_cancelled = false;
    bool m_usedFallback = false;
    QString m_optifineForgeVersion;
    bool m_optifineUseOfficial = false;

    // byte progress tracking
    qint64 m_bytesReceived = 0;
    qint64 m_bytesLast = 0;
    QElapsedTimer m_speedTimer;
};

} // namespace ShadowLauncher
