#include "launcher.h"
#include "../utils/logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

// QZipReader is a Qt private API in QtGui.
// If linking fails, add  Qt6::GuiPrivate  to target_link_libraries in CMakeLists.txt.
#include <private/qzipreader_p.h>

namespace ShadowLauncher {

// Default optimized JVM args (same as Python version)
static const char* DEFAULT_JVM_ARGS[] = {
    "-XX:+UseG1GC",
    "-XX:+UnlockExperimentalVMOptions",
    "-XX:G1NewSizePercent=20",
    "-XX:G1ReservePercent=20",
    "-XX:MaxGCPauseMillis=50",
    "-XX:G1HeapRegionSize=32M",
    "-XX:+DisableExplicitGC",
    "-Dfml.ignoreInvalidMinecraftCertificates=true",
    "-Dfml.ignorePatchDiscrepancies=true",
};

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

    // --- Load version JSON ---
    QString jsonPath = m_gameDir + QStringLiteral("/versions/") + versionId
                       + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit launchFinished(false, QStringLiteral("无法读取版本配置: %1").arg(jsonPath));
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit launchFinished(false, QStringLiteral("版本配置格式错误: %1").arg(parseErr.errorString()));
        return;
    }

    QJsonObject versionJson = doc.object();

    // --- Extract natives (before building args so library path exists) ---
    extractNatives(versionId, versionJson);

    // --- Ensure options.txt has language setting ---
    ensureOptionsTxt(versionId);

    // --- Build arguments ---
    QStringList args = buildArgs(versionId, maxMemoryMB, versionJson);

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
        emit launchProgress(text);
    }
}

void Launcher::onReadyReadStderr()
{
    QByteArray data = m_process->readAllStandardError();
    QString text = QString::fromUtf8(data).trimmed();
    if (!text.isEmpty()) {
        emit launchProgress(text);
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
        "",                              // NoError
        "无法启动 Java 进程，请检查路径",  // FailedToStart
        "Minecraft 进程崩溃",             // Crashed
        "进程超时",                        // Timedout
        "写入错误",                        // WriteError
        "读取错误"                         // ReadError
    };

    int idx = static_cast<int>(error);
    QString msg = (idx >= 1 && idx <= 5)
        ? QStringLiteral("启动失败: %1").arg(QString::fromLatin1(messages[idx]))
        : QStringLiteral("未知进程错误");

    emit launchProgress(msg);
    emit launchFinished(false, msg);
}

// ============================================================
// Private Helpers — Validation
// ============================================================

bool Launcher::validateLaunch(const QString& versionId, const QString& javaPath,
                               QString& errorMsg) const
{
    // 1. Java executable exists
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

    // 3. Version JAR exists
    QString jarPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (!QFileInfo::exists(jarPath)) {
        errorMsg = QStringLiteral("版本核心文件缺失: %1").arg(jarPath);
        return false;
    }

    // 4. Version JSON exists
    QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    if (!QFileInfo::exists(jsonPath)) {
        errorMsg = QStringLiteral("版本配置文件缺失: %1").arg(jsonPath);
        return false;
    }

    return true;
}

// ============================================================
// Private Helpers — Force Kill
// ============================================================

void Launcher::forceKill()
{
    if (!m_process) return;

    qint64 pid = m_process->processId();
    if (pid <= 0) {
        m_process->kill();
        m_process->waitForFinished(1000);
        return;
    }

#ifdef Q_OS_WIN
    // Windows: kill process tree
    QString cmd = QStringLiteral("taskkill /F /T /PID %1").arg(pid);
    QProcess::startDetached(cmd);

    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
#else
    m_process->kill();
    m_process->waitForFinished(3000);
#endif
}

// ============================================================
// Private Helpers — Argument Building (from version JSON)
// ============================================================

static bool shouldIncludeLibrary(const QJsonObject& lib)
{
    // Check library rules for platform filtering
    QJsonArray rules = lib[QStringLiteral("rules")].toArray();
    if (rules.isEmpty()) return true;

    for (const QJsonValue& ruleVal : rules) {
        QJsonObject rule = ruleVal.toObject();
        QJsonObject osCond = rule[QStringLiteral("os")].toObject();
        QString action = rule[QStringLiteral("action")].toString(QStringLiteral("allow"));

        if (!osCond.isEmpty()) {
            QString osName = osCond[QStringLiteral("name")].toString().toLower();
#ifdef Q_OS_WIN
            bool isMatch = osName.contains(QStringLiteral("windows"));
#elif defined(Q_OS_MACOS)
            bool isMatch = osName.contains(QStringLiteral("osx"));
#else
            bool isMatch = osName.contains(QStringLiteral("linux"));
#endif
            if (isMatch) return action == QStringLiteral("allow");
        } else {
            // Rule without OS condition matches all platforms
            return action == QStringLiteral("allow");
        }
    }

    // No rule matched this platform — exclude (default deny)
    return false;
}

static QString resolveLibraryPath(const QJsonObject& lib, const QString& librariesDir)
{
    // Try artifact path first
    QJsonObject downloads = lib[QStringLiteral("downloads")].toObject();
    QJsonObject artifact = downloads[QStringLiteral("artifact")].toObject();
    if (!artifact.isEmpty()) {
        QString path = artifact[QStringLiteral("path")].toString();
        return librariesDir + QStringLiteral("/") + path;
    }

    // Try classifiers for platform natives
    QJsonObject classifiers = downloads[QStringLiteral("classifiers")].toObject();
    if (!classifiers.isEmpty()) {
#ifdef Q_OS_WIN
        QString clsKey = QStringLiteral("natives-windows");
#elif defined(Q_OS_MACOS)
        QString clsKey = QStringLiteral("natives-osx");
#else
        QString clsKey = QStringLiteral("natives-linux");
#endif
        // Try 64-bit first, then fallback
        for (const auto& suffix : {QString(), QStringLiteral("-64"), QStringLiteral("-32")}) {
            QString key = clsKey + suffix;
            QJsonObject clsArtifact = classifiers[key].toObject();
            if (!clsArtifact.isEmpty()) {
                QString path = clsArtifact[QStringLiteral("path")].toString();
                return librariesDir + QStringLiteral("/") + path;
            }
        }
    }

    return {};
}

static QStringList buildClasspath(const QString& versionId, const QJsonObject& versionJson,
                                   const QString& gameDir)
{
    QStringList cp;
    QString libsDir = gameDir + QStringLiteral("/libraries");

    // Add version JAR first
    QString versionJar = gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (QFileInfo::exists(versionJar)) {
        cp.append(versionJar);
    }

    // Add libraries from version JSON
    QJsonArray libraries = versionJson[QStringLiteral("libraries")].toArray();
    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();
        if (!shouldIncludeLibrary(lib)) continue;

        QString libPath = resolveLibraryPath(lib, libsDir);
        if (!libPath.isEmpty() && QFileInfo::exists(libPath)) {
            cp.append(libPath);
        }
    }

    return cp;
}

QStringList Launcher::buildArgs(const QString& versionId, int maxMemoryMB,
                                 const QJsonObject& versionJson) const
{
    QStringList args;

    // ── JVM memory ──
    args << QStringLiteral("-Xmx%1M").arg(maxMemoryMB);
    args << QStringLiteral("-Xms%1M").arg(qMin(512, maxMemoryMB / 2));

    // ── Optimized JVM flags ──
    for (const char* arg : DEFAULT_JVM_ARGS) {
        args << QString::fromLatin1(arg);
    }

    // ── Version jar path for Forge ──
    QString versionJar = m_gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    args << QStringLiteral("-Dminecraft.client.jar=%1").arg(versionJar);

    // ── Natives path ──
    QString nativesDir = m_gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/natives");
    args << QStringLiteral("-Djava.library.path=%1").arg(nativesDir);

    // ── Classpath from version JSON ──
    QStringList cp = buildClasspath(versionId, versionJson, m_gameDir);
    if (!cp.isEmpty()) {
        args << QStringLiteral("-cp");
#ifdef Q_OS_WIN
        args << cp.join(QStringLiteral(";"));
#else
        args << cp.join(QStringLiteral(":"));
#endif
    }

    // ── Main class ──
    QString mainClass = versionJson[QStringLiteral("mainClass")].toString();
    if (mainClass.isEmpty()) {
        mainClass = QStringLiteral("net.minecraft.client.main.Main");
    }
    args << mainClass;

    // ── Minecraft arguments ──
    // Parse arguments from version JSON
    QJsonObject arguments = versionJson[QStringLiteral("arguments")].toObject();
    QJsonArray gameArgs;

    if (!arguments.isEmpty()) {
        // Modern format (1.13+): arguments.game
        gameArgs = arguments[QStringLiteral("game")].toArray();
    } else {
        // Legacy format: minecraftArguments string
        QString mcArgs = versionJson[QStringLiteral("minecraftArguments")].toString();
        if (!mcArgs.isEmpty()) {
            // Split by spaces, respecting quoted strings
            // Simplified: just split by spaces for common args
            const QStringList parts = mcArgs.split(QLatin1Char(' '));
            for (const QString& part : parts) {
                if (!part.isEmpty()) gameArgs.append(part);
            }
        }
    }

    // Process game arguments, expanding placeholders
    for (const QJsonValue& argVal : gameArgs) {
        if (argVal.isString()) {
            QString arg = argVal.toString();
            // Replace placeholders
            arg.replace(QStringLiteral("${auth_player_name}"), QStringLiteral("{username}"));
            arg.replace(QStringLiteral("${version_name}"), versionId);
            arg.replace(QStringLiteral("${game_directory}"),
                        m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game"));
            arg.replace(QStringLiteral("${assets_root}"),
                        m_gameDir + QStringLiteral("/assets"));
            arg.replace(QStringLiteral("${assets_index_name}"), versionId);
            arg.replace(QStringLiteral("${auth_uuid}"), QStringLiteral("00000000-0000-0000-0000-000000000000"));
            arg.replace(QStringLiteral("${auth_access_token}"), QStringLiteral("0"));
            arg.replace(QStringLiteral("${user_type}"), QStringLiteral("mojang"));
            arg.replace(QStringLiteral("${version_type}"),
                        versionJson[QStringLiteral("type")].toString(QStringLiteral("release")));

            if (!arg.isEmpty()) args << arg;
        }
    }

    // Ensure game directory exists
    QDir().mkpath(m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game"));

    return args;
}

// ============================================================
// Private Helpers — Natives Extraction
// ============================================================

bool Launcher::extractNatives(const QString& versionId, const QJsonObject& versionJson)
{
    QString nativesDir = m_gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/natives");

    qCInfo(logLaunch) << "extractNatives: starting for" << versionId << "→" << nativesDir;

    // Idempotent: skip if natives already extracted
    QDir nd(nativesDir);
    if (nd.exists()) {
        QStringList nativeFilters;
#ifdef Q_OS_WIN
        nativeFilters << QStringLiteral("*.dll");
#elif defined(Q_OS_MACOS)
        nativeFilters << QStringLiteral("*.dylib") << QStringLiteral("*.jnilib");
#else
        nativeFilters << QStringLiteral("*.so");
#endif
        QStringList existing = nd.entryList(nativeFilters, QDir::Files);
        if (!existing.isEmpty()) {
            qCDebug(logLaunch) << "Natives already extracted, skipping:" << nativesDir
                               << "(" << existing.size() << "files)";
            return true;
        }
    }

    QDir().mkpath(nativesDir);

    const QString libsDir = m_gameDir + QStringLiteral("/libraries");
    const QJsonArray libraries = versionJson[QStringLiteral("libraries")].toArray();

    // Determine platform-specific native classifier prefix
#ifdef Q_OS_WIN
    const QString nativePrefix = QStringLiteral("natives-windows");
#elif defined(Q_OS_MACOS)
    const QString nativePrefix = QStringLiteral("natives-osx");
#else
    const QString nativePrefix = QStringLiteral("natives-linux");
#endif

    qCInfo(logLaunch) << "extractNatives: scanning" << libraries.size() << "libraries for" << nativePrefix;

    int extractedCount = 0;
    int jarCount = 0;

    for (const QJsonValue& libVal : libraries) {
        QJsonObject lib = libVal.toObject();
        if (!shouldIncludeLibrary(lib)) continue;

        QString libName = lib[QStringLiteral("name")].toString();
        QJsonObject downloads = lib[QStringLiteral("downloads")].toObject();

        // Collect jar paths from both old & new format
        QStringList pendingJars;
        QStringList excludePatterns;

        // --- New format (1.21+): natives classifier in the name ---
        // e.g. "org.lwjgl:lwjgl-glfw:3.3.3:natives-windows"
        if (libName.contains(QStringLiteral(":") + nativePrefix)) {
            QJsonObject artifact = downloads[QStringLiteral("artifact")].toObject();
            QString path = artifact[QStringLiteral("path")].toString();
            if (!path.isEmpty()) {
                QString jarPath = libsDir + QStringLiteral("/") + path;
                if (QFileInfo::exists(jarPath)) {
                    pendingJars.append(jarPath);
                }
            }
        }

        // --- Old format (1.8-1.20): classifiers section ---
        QJsonObject classifiers = downloads[QStringLiteral("classifiers")].toObject();
        for (auto it = classifiers.begin(); it != classifiers.end(); ++it) {
            QString clsName = it.key();
            if (!clsName.startsWith(nativePrefix)) continue;

            QJsonObject clsArt = it.value().toObject();
            QString path = clsArt[QStringLiteral("path")].toString();
            if (path.isEmpty()) continue;

            QString jarPath = libsDir + QStringLiteral("/") + path;
            if (QFileInfo::exists(jarPath)) {
                pendingJars.append(jarPath);
            }
        }

        // Get exclude patterns from library extract config (shared by both formats)
        QJsonObject extract = lib[QStringLiteral("extract")].toObject();
        QJsonArray excludeArr = extract[QStringLiteral("exclude")].toArray();
        for (const QJsonValue& exVal : excludeArr) {
            excludePatterns.append(exVal.toString());
        }

        // Process all pending native jars
        for (const QString& jarPath : pendingJars) {
            jarCount++;

            QZipReader zipReader(jarPath);
            if (zipReader.status() != QZipReader::NoError) {
                qCWarning(logLaunch) << "extractNatives: failed to open jar:" << jarPath;
                continue;
            }

            const auto fileList = zipReader.fileInfoList();
            for (const auto& fi : fileList) {
                QString fileName = fi.filePath;

                if (fi.isDir) continue;
                if (fileName.startsWith(QStringLiteral("META-INF"))) continue;

                QString lower = fileName.toLower();
                if (!lower.endsWith(QStringLiteral(".dll"))
                    && !lower.endsWith(QStringLiteral(".so"))
                    && !lower.endsWith(QStringLiteral(".dylib"))
                    && !lower.endsWith(QStringLiteral(".jnilib"))) {
                    continue;
                }

                bool excluded = false;
                for (const QString& pattern : excludePatterns) {
                    if (fileName.startsWith(pattern)) { excluded = true; break; }
                }
                if (excluded) continue;

                QByteArray data = zipReader.fileData(fileName);
                if (!data.isEmpty()) {
                    QString baseName = QFileInfo(fileName).fileName();
                    QString destPath = nativesDir + QStringLiteral("/") + baseName;
                    QFile destFile(destPath);
                    if (destFile.open(QIODevice::WriteOnly)) {
                        destFile.write(data);
                        destFile.close();
                        extractedCount++;
                    }
                }
            }
            zipReader.close();
        }
    }

    qCInfo(logLaunch) << "extractNatives: complete —" << jarCount << "native jars, extracted" << extractedCount << "files →" << nativesDir;
    return extractedCount > 0 || jarCount == 0;
}

// ============================================================
// Private Helpers — Language Detection & options.txt
// ============================================================

void Launcher::ensureOptionsTxt(const QString& versionId)
{
    QString gameDir = m_gameDir + QStringLiteral("/versions/") + versionId
                      + QStringLiteral("/game");
    QString optionsPath = gameDir + QStringLiteral("/options.txt");

    // Detect language from system locale
    QString lang;
    QLocale sysLocale = QLocale::system();
    QLocale::Language sysLang = sysLocale.language();

    switch (sysLang) {
    case QLocale::Chinese:
        lang = QStringLiteral("zh_cn");
        break;
    case QLocale::Japanese:
        lang = QStringLiteral("ja_jp");
        break;
    case QLocale::Korean:
        lang = QStringLiteral("ko_kr");
        break;
    default:
        lang = QStringLiteral("en_us");
        break;
    }

    QString langLine = QStringLiteral("lang:") + lang;

    // Read existing options.txt
    QStringList lines;
    bool foundLang = false;

    QFile file(optionsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.startsWith(QStringLiteral("lang:"))) {
                lines.append(langLine);
                foundLang = true;
            } else if (!line.isEmpty()) {
                lines.append(line);
            }
        }
        file.close();
    }

    if (!foundLang) {
        lines.append(langLine);
    }

    // Write back
    QDir().mkpath(gameDir);
    QFile outFile(optionsPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&outFile);
        for (const QString& line : lines) {
            out << line << QStringLiteral("\n");
        }
        outFile.close();
        qCDebug(logLaunch) << "options.txt updated, lang:" << lang
                           << "file:" << optionsPath;
    } else {
        qCWarning(logLaunch) << "Failed to write options.txt:" << optionsPath;
    }
}

} // namespace ShadowLauncher
