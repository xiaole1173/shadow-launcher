#include "launcher.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>

namespace ShadowLauncher {

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
}

Launcher::~Launcher()
{
    if (isRunning()) {
        forceKill();
    }
}

// ============================================================
// Public API
// ============================================================

void Launcher::start(const QString& versionId, const QString& javaPath, int maxMemoryMB)
{
    // --- Validate ---
    QString errorMsg;
    if (!validateLaunch(versionId, javaPath, errorMsg)) {
        emit launchFinished(false, errorMsg);
        return;
    }

    // --- Abort if already running ---
    if (isRunning()) {
        emit launchFinished(false, QStringLiteral("另一个 Minecraft 实例已在运行"));
        return;
    }

    m_currentVersionId = versionId;
    m_cancelling = false;

    // --- Build arguments ---
    QStringList args = buildArgs(versionId, maxMemoryMB);

    emit launchProgress(QStringLiteral("启动 Minecraft %1...").arg(versionId));

    m_process->setWorkingDirectory(m_gameDir);
    m_process->start(javaPath, args);
}

void Launcher::cancel()
{
    if (!isRunning()) return;

    m_cancelling = true;
    emit launchProgress(QStringLiteral("正在停止 Minecraft..."));

    // Graceful close
    m_process->closeWriteChannel();

    // After 2s grace, force kill
    QTimer::singleShot(2000, this, [this]() {
        if (isRunning()) {
            forceKill();
        }
    });
}

// ============================================================
// Private Slots
// ============================================================

void Launcher::onProcessStarted()
{
    emit launchStarted();
    emit launchProgress(QStringLiteral("Minecraft 进程已启动 (PID: %1)")
                        .arg(m_process->processId()));
}

void Launcher::onReadyReadStdout()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString text = QString::fromUtf8(data).trimmed();
    if (!text.isEmpty()) {
        emit launchProgress(QStringLiteral("[stdout] %1").arg(text));
    }
}

void Launcher::onReadyReadStderr()
{
    QByteArray data = m_process->readAllStandardError();
    QString text = QString::fromUtf8(data).trimmed();
    if (!text.isEmpty()) {
        emit launchProgress(QStringLiteral("[stderr] %1").arg(text));
    }
}

void Launcher::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    if (m_cancelling) {
        emit launchFinished(true, QString());
    } else if (exitCode == 0) {
        emit launchFinished(true, QString());
    } else {
        QString msg = QStringLiteral("Minecraft 异常退出 (退出码: %1)").arg(exitCode);
        emit launchFinished(false, msg);
    }
}

void Launcher::onProcessError(QProcess::ProcessError error)
{
    static const char* messages[] = {
        "",                         // NoError
        "无法启动 Java 进程，请检查路径",
        "Minecraft 进程崩溃",
        "进程超时",
        "写入错误",                 // WriteError
        "读取错误"                  // ReadError
    };

    int idx = static_cast<int>(error);
    QString msg = (idx >= 1 && idx <= 5)
        ? QStringLiteral("启动失败: %1").arg(QString::fromLatin1(messages[idx]))
        : QStringLiteral("未知进程错误");

    emit launchProgress(msg);
    emit launchFinished(false, msg);
}

// ============================================================
// Private Helpers
// ============================================================

bool Launcher::validateLaunch(const QString& versionId, const QString& javaPath,
                               QString& errorMsg) const
{
    // 1. Java exists
    if (!QFileInfo::exists(javaPath)) {
        errorMsg = QStringLiteral("Java 未找到: %1").arg(javaPath);
        return false;
    }

    // 2. Version directory exists
    QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    if (!QDir(versionDir).exists()) {
        errorMsg = QStringLiteral("版本目录不存在: %1").arg(versionDir);
        return false;
    }

    // 3. Version JSON exists
    QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    if (!QFileInfo::exists(jsonPath)) {
        errorMsg = QStringLiteral("版本配置文件缺失: %1").arg(jsonPath);
        return false;
    }

    return true;
}

void Launcher::forceKill()
{
    if (!m_process) return;

    qint64 pid = m_process->processId();
    if (pid <= 0) {
        m_process->kill();
        m_process->waitForFinished(1000);
        return;
    }

    // Windows: kill process tree
    QString cmd = QStringLiteral("taskkill /F /T /PID %1").arg(pid);
    QProcess::startDetached(cmd);

    // Give taskkill time, then direct kill
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QStringList Launcher::buildArgs(const QString& versionId, int maxMemoryMB) const
{
    // Due to Maven-style library dependencies, full classpath construction
    // requires parsing the version JSON. This simplified version passes
    // the core arguments; a full implementation would use install_profile
    // from the Minecraft launcher metadata.
    
    QStringList args;

    // JVM memory
    args << QStringLiteral("-Xmx%1M").arg(maxMemoryMB);
    args << QStringLiteral("-Xms%1M").arg(qMin(512, maxMemoryMB / 4));

    // Game directory for this version
    QString gameDir = m_gameDir + QStringLiteral("/versions/") + versionId
                      + QStringLiteral("/game");
    QDir().mkpath(gameDir);

    // Native libraries path
    args << QStringLiteral("-Djava.library.path=")
            + m_gameDir + QStringLiteral("/versions/") + versionId
            + QStringLiteral("/natives");

    // Build classpath from installed libraries
    QString libDir = m_gameDir + QStringLiteral("/libraries");
    QStringList jars;

    if (QDir(libDir).exists()) {
        QDirIterator it(libDir, QStringList() << QStringLiteral("*.jar"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            jars.append(it.next());
        }
    }

    // Add version jar
    QString versionJar = m_gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (QFileInfo::exists(versionJar)) {
        jars.prepend(versionJar);
    }

    // Build classpath string (Windows: semicolon separator)
    QString classpath = jars.join(QStringLiteral(";"));

    if (!classpath.isEmpty()) {
        args << QStringLiteral("-cp") << classpath;
    }

    // Main class
    args << QStringLiteral("net.minecraft.client.main.Main");

    // Minecraft arguments
    args << QStringLiteral("--username") << QStringLiteral("{username}");
    args << QStringLiteral("--version") << versionId;
    args << QStringLiteral("--gameDir") << gameDir;
    args << QStringLiteral("--assetsDir") << m_gameDir + QStringLiteral("/assets");
    args << QStringLiteral("--assetIndex") << versionId;
    args << QStringLiteral("--accessToken") << QStringLiteral("0");
    args << QStringLiteral("--userType") << QStringLiteral("mojang");
    args << QStringLiteral("--versionType") << QStringLiteral("release");

    return args;
}

} // namespace ShadowLauncher
