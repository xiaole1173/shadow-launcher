#include "launcher.h"
#include "logger.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace ShadowLauncher {

// ---- Constructor / Destructor ----

Launcher::Launcher(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::started,
            this, &Launcher::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &Launcher::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &Launcher::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Launcher::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &Launcher::onProcessError);

    Logger::info("Launcher initialized");
}

Launcher::~Launcher()
{
    if (isRunning()) {
        cancel();
        m_process->waitForFinished(3000);
    }
}

// ---- Public API ----

void Launcher::start(const QString& versionId,
                     const QString& javaPath,
                     int maxMemory,
                     const QJsonObject& versionJson)
{
    if (isRunning()) {
        emit launchFinished(false, "A Minecraft process is already running");
        return;
    }

    if (m_gameDir.isEmpty()) {
        emit launchFinished(false, "Game directory not set");
        return;
    }

    if (!QFileInfo::exists(javaPath)) {
        emit launchFinished(false, "Java executable not found: " + javaPath);
        return;
    }

    m_versionId = versionId;
    m_cancelling = false;
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();

    // 1) Build JVM arguments
    QStringList jvmArgs = buildJvmArgs(versionId, maxMemory, versionJson);

    // 2) Build classpath
    QString classpath = buildClasspath(versionJson);

    // 3) Main class
    QString mainClass = versionJson.value("mainClass").toString("net.minecraft.client.main.Main");

    // 4) Minecraft arguments
    QStringList mcArgs;
    mcArgs << "--username" << "ShadowPlayer"
           << "--version" << versionId
           << "--gameDir" << (m_gameDir + "/versions/" + versionId + "/game")
           << "--assetsDir" << (m_gameDir + "/assets")
           << "--assetIndex" << versionJson.value("assetIndex").toObject().value("id").toString(versionId)
           << "--uuid" << QUuid::createUuid().toString(QUuid::WithoutBraces)
           << "--accessToken" << "0"
           << "--userType" << "mojang"
           << "--versionType" << versionJson.value("type").toString("release");

    // 5) Full command
    QStringList fullCmd;
    fullCmd << jvmArgs;
    fullCmd << "-cp" << classpath;
    fullCmd << mainClass;
    fullCmd << mcArgs;

    Logger::info(QString("Launching: %1 %2").arg(javaPath, fullCmd.join(' ')));

    emit launchProgress("Preparing launch...");
    emit launchStarted();

    m_process->setProgram(javaPath);
    m_process->setArguments(fullCmd);

    // Set working directory to version dir
    QString versionDir = m_gameDir + "/versions/" + versionId;
    m_process->setWorkingDirectory(versionDir);

    m_process->start();
}

void Launcher::cancel()
{
    if (!isRunning())
        return;

    m_cancelling = true;
    emit launchProgress("Cancelling...");

    Logger::info("Cancelling Minecraft process");

#ifdef Q_OS_WIN
    forceKillProcessTree();
#else
    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
    }
#endif
}

bool Launcher::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

qint64 Launcher::pid() const
{
    if (!m_process)
        return 0;
    return m_process->processId();
}

// ---- Private Slots ----

void Launcher::onProcessStarted()
{
    emit launchProgress("Minecraft started (PID: " + QString::number(m_process->processId()) + ")");
    Logger::info(QString("Minecraft PID: %1").arg(m_process->processId()));
}

void Launcher::onReadyReadStdout()
{
    m_stdoutBuffer.append(m_process->readAllStandardOutput());

    // Emit complete lines
    while (true) {
        int idx = m_stdoutBuffer.indexOf('\n');
        if (idx < 0)
            break;
        QByteArray line = m_stdoutBuffer.left(idx).trimmed();
        m_stdoutBuffer.remove(0, idx + 1);
        if (!line.isEmpty()) {
            QString lineStr = QString::fromUtf8(line);
            emit launchOutput(lineStr);
        }
    }
}

void Launcher::onReadyReadStderr()
{
    m_stderrBuffer.append(m_process->readAllStandardError());

    while (true) {
        int idx = m_stderrBuffer.indexOf('\n');
        if (idx < 0)
            break;
        QByteArray line = m_stderrBuffer.left(idx).trimmed();
        m_stderrBuffer.remove(0, idx + 1);
        if (!line.isEmpty()) {
            QString lineStr = QString::fromUtf8(line);
            emit launchOutput(lineStr);
        }
    }
}

void Launcher::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // Flush remaining buffers
    if (!m_stdoutBuffer.isEmpty()) {
        emit launchOutput(QString::fromUtf8(m_stdoutBuffer.trimmed()));
    }
    if (!m_stderrBuffer.isEmpty()) {
        emit launchOutput(QString::fromUtf8(m_stderrBuffer.trimmed()));
    }

    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);

    if (m_cancelling) {
        emit launchFinished(true, "Cancelled");
        Logger::info("Minecraft process cancelled");
    } else if (success) {
        emit launchFinished(true, QString());
        Logger::info("Minecraft exited normally");
    } else {
        QString err = QString("Minecraft exited with code %1 (%2)")
            .arg(exitCode)
            .arg(exitStatus == QProcess::CrashExit ? "crashed" : "unknown");
        emit launchFinished(false, err);
        Logger::error(err);
    }

    m_cancelling = false;
}

void Launcher::onProcessError(QProcess::ProcessError error)
{
    QString errMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errMsg = "Failed to start Minecraft process (Java not found or permission denied)";
        break;
    case QProcess::Crashed:
        errMsg = "Minecraft process crashed";
        break;
    case QProcess::Timedout:
        errMsg = "Minecraft process timed out";
        break;
    case QProcess::WriteError:
        errMsg = "Failed to write to Minecraft process";
        break;
    case QProcess::ReadError:
        errMsg = "Failed to read from Minecraft process";
        break;
    default:
        errMsg = "Unknown process error";
        break;
    }

    Logger::error(errMsg);
    emit launchFinished(false, errMsg);
}

// ---- Private Helpers ----

QStringList Launcher::buildJvmArgs(const QString& versionId,
                                    int maxMemory,
                                    const QJsonObject& versionJson)
{
    QStringList args;

    // Memory
    int minMemory = qMin(maxMemory / 2, 2048);
    args << QString("-Xmx%1M").arg(maxMemory);
    args << QString("-Xms%1M").arg(minMemory);

    // G1GC
    args << "-XX:+UseG1GC"
         << "-XX:+UnlockExperimentalVMOptions"
         << "-XX:G1NewSizePercent=20"
         << "-XX:G1ReservePercent=20"
         << "-XX:MaxGCPauseMillis=50"
         << "-XX:G1HeapRegionSize=32M";

    // Version jar path
    QString versionJar = m_gameDir + "/versions/" + versionId + "/" + versionId + ".jar";
    args << QString("-Dminecraft.client.jar=%1").arg(versionJar);

    // Native libraries
    QString nativesDir = m_gameDir + "/versions/" + versionId + "/" + versionId + "-natives";
    args << QString("-Djava.library.path=%1").arg(nativesDir);

    // Shadow launcher marker
    args << "-Dshadow.launcher=true";

    return args;
}

QString Launcher::buildClasspath(const QJsonObject& versionJson)
{
    QString versionId = versionJson.value("id").toString();
    QString versionJar = m_gameDir + "/versions/" + versionId + "/" + versionId + ".jar";
    QString libsDir = m_gameDir + "/libraries";

    QStringList cp;
    cp << versionJar;

    // Parse libraries from version JSON
    QJsonArray libraries = versionJson.value("libraries").toArray();
    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();
        QString name = lib.value("name").toString();
        if (name.isEmpty())
            continue;

        // Skip platform-specific libs that don't match Windows
        QJsonArray rules = lib.value("rules").toArray();
        if (!rules.isEmpty()) {
            bool allowed = false;
            for (const QJsonValue& ruleVal : rules) {
                QJsonObject rule = ruleVal.toObject();
                QString action = rule.value("action").toString();
                QJsonObject os = rule.value("os").toObject();
                QString osName = os.value("name").toString();

                if (osName.isEmpty()) {
                    allowed = (action == "allow");
                } else if (osName == "windows") {
                    allowed = (action == "allow");
                } else {
                    allowed = (action == "disallow");
                }
            }
            if (!allowed)
                continue;
        }

        // Build library path
        QString libPath = buildLibPath(name, versionJson);
        if (!libPath.isEmpty() && QFileInfo::exists(libPath)) {
            cp << libPath;
        }
    }

    return cp.join(';');
}

QString Launcher::buildLibPath(const QString& libName, const QJsonObject& /*versionJson*/)
{
    QString libsDir = m_gameDir + "/libraries";

    // Parse Maven-style name: "group:artifact:version"
    QStringList parts = libName.split(':');
    if (parts.size() < 3)
        return QString();

    QString group = parts[0];
    QString artifact = parts[1];
    QString version = parts[2];

    QString groupPath = group.replace('.', '/');
    QString jarName = artifact + "-" + version + ".jar";

    return libsDir + "/" + groupPath + "/" + artifact + "/" + version + "/" + jarName;
}

QString Launcher::extractNatives(const QString& versionId, const QJsonObject& /*versionJson*/)
{
    // Stub: return the expected natives directory
    // Full implementation would parse versionJson for native libraries and extract .dll files
    QString nativesDir = m_gameDir + "/versions/" + versionId + "/" + versionId + "-natives";
    QDir().mkpath(nativesDir);
    return nativesDir;
}

#ifdef Q_OS_WIN
void Launcher::forceKillProcessTree()
{
    qint64 targetPid = m_process->processId();
    if (targetPid <= 0)
        return;

    // Strategy: use taskkill /F /T to kill entire tree
    QString cmd = QString("taskkill /F /T /PID %1").arg(targetPid);

    // Try up to 3 times
    for (int i = 0; i < 3 && isRunning(); i++) {
        QProcess killer;
        killer.start("taskkill", QStringList() << "/F" << "/T" << "/PID" << QString::number(targetPid));
        killer.waitForFinished(2000);
    }

    // Final fallback
    if (isRunning()) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}
#else
void Launcher::forceKillProcessTree()
{
    m_process->terminate();
    if (!m_process->waitForFinished(3000)) {
        m_process->kill();
    }
}
#endif

} // namespace ShadowLauncher
