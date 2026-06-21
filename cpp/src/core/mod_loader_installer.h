#pragma once

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>

namespace ShadowLauncher {

class ModLoaderInstaller : public QObject {
    Q_OBJECT

public:
    explicit ModLoaderInstaller(QObject* parent = nullptr);
    ~ModLoaderInstaller() override;

    void setGameDir(const QString& dir) { m_gameDir = dir; }
    QString gameDir() const { return m_gameDir; }

    void installForge(const QString& mcVersion, const QString& forgeVersion, const QString& installName);
    void installFabric(const QString& mcVersion, const QString& fabricVersion, const QString& installName);
    void installNeoForge(const QString& mcVersion, const QString& neoVersion, const QString& installName);
    void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                         const QString& forgeVersion, const QString& installName);

    bool isRunning() const { return m_running; }
    void cancel();

signals:
    void progressChanged(int step, int totalSteps, const QString& description);
    void finished(bool success, const QString& error);
    void logMessage(const QString& msg);

private:
    void downloadFile(const QString& url, std::function<void(QByteArray)> onDone);
    bool ensureVanillaInstalled(const QString& mcVersion);
    QString versionsDir() const { return m_gameDir + "/versions"; }

    // Forge/NeoForge
    void forgeStep1_downloadInstallerJar();
    void forgeStep2_runInstaller(const QByteArray& jarData);

    // Fabric
    void fabricStep1_downloadProfile();
    void fabricStep2_createVersionJson(const QByteArray& profileData);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_gameDir;
    QString m_mcVersion;
    QString m_loaderVersion;
    QString m_installName;
    QString m_loaderType; // "forge" | "neoforge" | "fabric" | "optifine"
    int m_currentStep = 0;
    int m_totalSteps = 0;
    bool m_running = false;
    bool m_cancelled = false;
};

} // namespace ShadowLauncher
