#include "mod_loader_installer.h"
#include "http_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QProcess>
#include <QDebug>

using namespace ShadowLauncher;

ModLoaderInstaller::ModLoaderInstaller(QObject* parent) : QObject(parent) {}
ModLoaderInstaller::~ModLoaderInstaller() = default;

void ModLoaderInstaller::cancel() {
    m_cancelled = true;
    m_running = false;
}

// ============================================================
// Public API
// ============================================================

void ModLoaderInstaller::installForge(const QString& mcVersion, const QString& forgeVersion, const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = forgeVersion;
    m_installName = installName; m_loaderType = "forge";
    m_totalSteps = 2; m_currentStep = 0;

    qDebug() << "[ModLoader] Forge: MC" << mcVersion << "Forge" << forgeVersion;
    forgeStep1_downloadInstallerJar();
}

void ModLoaderInstaller::installFabric(const QString& mcVersion, const QString& fabricVersion, const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = fabricVersion;
    m_installName = installName; m_loaderType = "fabric";
    m_totalSteps = 2; m_currentStep = 0;

    qDebug() << "[ModLoader] Fabric: MC" << mcVersion << "Fabric" << fabricVersion;
    fabricStep1_downloadProfile();
}

void ModLoaderInstaller::installNeoForge(const QString& mcVersion, const QString& neoVersion, const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = neoVersion;
    m_installName = installName; m_loaderType = "neoforge";
    m_totalSteps = 2; m_currentStep = 0;

    qDebug() << "[ModLoader] NeoForge: MC" << mcVersion << "NeoForge" << neoVersion;
    forgeStep1_downloadInstallerJar();
}

void ModLoaderInstaller::installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                          const QString& /*forgeVersion*/, const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = optifineVersion;
    m_installName = installName; m_loaderType = "optifine";
    m_totalSteps = 1; m_currentStep = 0;

    qDebug() << "[ModLoader] Optifine: MC" << mcVersion << "Optifine" << optifineVersion;

    QString filename = QString("OptiFine_%1_%2.jar").arg(mcVersion, optifineVersion);
    QString url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/download").arg(mcVersion, filename);

    HttpClient::instance().get(url, [this](int status, const QByteArray& data) {
        if (m_cancelled || status != 200) { emit finished(false, "Optifine download failed"); m_running = false; return; }
        QString modsDir = m_gameDir + "/mods";
        QDir().mkpath(modsDir);
        QString filename = QString("OptiFine_%1_%2.jar").arg(m_mcVersion, m_loaderVersion);
        QFile f(modsDir + "/" + filename);
        f.open(QIODevice::WriteOnly); f.write(data); f.close();
        emit progressChanged(1, 1, "Optifine installed");
        emit finished(true, QString());
        m_running = false;
    });
}

// ============================================================
// Forge / NeoForge
// ============================================================

void ModLoaderInstaller::forgeStep1_downloadInstallerJar() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "Downloading installer...");

    QString verArg = (m_loaderType == "neoforge")
        ? m_loaderVersion
        : m_mcVersion + "-" + m_loaderVersion;

    QString url = QString("https://bmclapi2.bangbang93.com/forge/download/%1/installer").arg(verArg);
    qDebug() << "[ModLoader] Installer URL:" << url;

    HttpClient::instance().get(url, [this](int status, const QByteArray& data) {
        if (m_cancelled || status != 200) { emit finished(false, "Installer download failed"); m_running = false; return; }
        forgeStep2_runInstaller(data);
    });
}

void ModLoaderInstaller::forgeStep2_runInstaller(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "Running installer...");

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/forge-installer-" + m_installName + ".jar";

    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "Cannot write temp JAR");
        m_running = false;
        return;
    }
    jarFile.write(jarData);
    jarFile.close();

    QProcess* proc = new QProcess(this);
    QString javaPath = "java";
    QStringList args;
    args << "-jar" << jarPath << "--installClient" << m_gameDir;

    qDebug() << "[ModLoader] Running:" << javaPath << args;

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        QFile::remove(jarPath);

        if (m_cancelled || exitCode != 0) {
            qWarning() << "[ModLoader] Installer failed, exit:" << exitCode;
            emit finished(false, QString("Installer failed (exit %1)").arg(exitCode));
            m_running = false;
            return;
        }

        qDebug() << "[ModLoader] Installer completed";
        emit progressChanged(2, m_totalSteps, "Install complete");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start(javaPath, args);
}

// ============================================================
// Fabric
// ============================================================

void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "Downloading Fabric profile...");

    QString url = QString("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/%1/%2/profile/json")
                      .arg(m_mcVersion, m_loaderVersion);

    qDebug() << "[ModLoader] Fabric profile URL:" << url;

    HttpClient::instance().get(url, [this](int status, const QByteArray& data) {
        if (m_cancelled || status != 200) { emit finished(false, "Fabric profile download failed"); m_running = false; return; }
        fabricStep2_createVersionJson(data);
    });
}

void ModLoaderInstaller::fabricStep2_createVersionJson(const QByteArray& profileData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "Creating version config...");

    QJsonDocument doc = QJsonDocument::fromJson(profileData);
    if (doc.isNull()) { emit finished(false, "Invalid Fabric profile JSON"); m_running = false; return; }

    QJsonObject json = doc.object();
    json["id"] = m_installName;

    QString versionsPath = m_gameDir + "/versions";
    QString verDir = versionsPath + "/" + m_installName;
    QDir().mkpath(verDir);

    QString jsonPath = verDir + "/" + m_installName + ".json";
    QFile f(jsonPath);
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    f.close();

    // Copy vanilla jar
    QString vanillaJar = versionsPath + "/" + m_mcVersion + "/" + m_mcVersion + ".jar";
    QString targetJar = verDir + "/" + m_installName + ".jar";
    if (QFile::exists(vanillaJar)) {
        if (QFile::exists(targetJar)) QFile::remove(targetJar);
        QFile::copy(vanillaJar, targetJar);
    }

    qDebug() << "[ModLoader] Fabric version JSON:" << jsonPath;
    emit progressChanged(2, m_totalSteps, "Fabric install complete");
    emit finished(true, QString());
    m_running = false;
}
