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

void Launcher::start(const QString& versionId, const QString& javaPath, int maxMemoryMB,
                     const QString& jvmArgs)
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

    m_jvmArgs = jvmArgs;
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
// Public: Immediate force kill
// ============================================================

void Launcher::killProcess()
{
    if (!isRunning()) return;
    forceKill();
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
    // Windows: kill process tree (synchronous — wait for taskkill to finish)
    QProcess killer;
    killer.start(QStringLiteral("taskkill"), QStringList() << QStringLiteral("/F") << QStringLiteral("/T") << QStringLiteral("/PID") << QString::number(pid));
    killer.waitForFinished(8000);

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
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

    // ── JVM flags: custom args replace defaults, otherwise use optimized G1GC ──
    if (!m_jvmArgs.isEmpty()) {
        // User-provided custom JVM args (space-separated tokens)
        const QStringList customArgs = m_jvmArgs.split(QRegularExpression(QStringLiteral("\\s+")),
                                                       Qt::SkipEmptyParts);
        for (const auto& arg : customArgs) {
            args << arg;
        }
    } else {
        // Default optimized G1GC flags
        for (const char* arg : DEFAULT_JVM_ARGS) {
            args << QString::fromLatin1(arg);
        }
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
        // Modern format (1.13+): arguments.game — filter by rules, then expand
        const QJsonArray raw = arguments[QStringLiteral("game")].toArray();
        for (const QJsonValue& val : raw) {
            if (val.isString()) {
                gameArgs.append(val.toString());
            } else if (val.isObject()) {
                QJsonObject obj = val.toObject();
                QJsonArray rulesArr = obj[QStringLiteral("rules")].toArray();
                if (!rulesArr.isEmpty()) {
                    bool include = evaluateRules(rulesArr);
                    if (!include) continue;
                }
                QJsonValue value = obj[QStringLiteral("value")];
                if (value.isString()) {
                    gameArgs.append(value.toString());
                } else if (value.isArray()) {
                    for (const QJsonValue& v : value.toArray()) {
                        if (v.isString()) gameArgs.append(v.toString());
                    }
                }
            }
        }
    } else {
        // Legacy format: minecraftArguments string
        QString mcArgs = versionJson[QStringLiteral("minecraftArguments")].toString();
        if (!mcArgs.isEmpty()) {
            // Split by spaces, respecting quoted strings
            static const QRegularExpression argSplitter(
                QStringLiteral(R"("(?:[^"\\]|\\.)*"|\S+)"));
            QRegularExpressionMatchIterator it = argSplitter.globalMatch(mcArgs);
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                QString part = m.captured(1);
                if (part.startsWith(QLatin1Char('"')) && part.endsWith(QLatin1Char('"')))
                    part = part.mid(1, part.size() - 2);
                if (!part.isEmpty()) gameArgs.append(part);
            }
        }
    }

    // Read asset index ID from version JSON (e.g. "27", not "1.21.10")
    QJsonObject assetIndex = versionJson[QStringLiteral("assetIndex")].toObject();
    QString assetIndexId = assetIndex[QStringLiteral("id")].toString();
    if (assetIndexId.isEmpty()) assetIndexId = versionId;  // legacy fallback

    // Build legacy virtual asset directory for 1.7.2
    if (assetIndexId == QStringLiteral("legacy")) {
        const_cast<Launcher*>(this)->ensureLegacyAssets();
    }

    // Process game arguments, expanding placeholders
    for (const QJsonValue& argVal : gameArgs) {
        if (argVal.isString()) {
            QString arg = argVal.toString();
            // Replace placeholders
            arg.replace(QStringLiteral("${auth_player_name}"), m_authName.isEmpty() ? QStringLiteral("{username}") : m_authName);
            arg.replace(QStringLiteral("${version_name}"), versionId);
            arg.replace(QStringLiteral("${game_directory}"),
                        m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game"));
            arg.replace(QStringLiteral("${assets_root}"),
                        m_gameDir + QStringLiteral("/assets"));
            arg.replace(QStringLiteral("${assets_index_name}"), assetIndexId);
            arg.replace(QStringLiteral("${auth_uuid}"), m_authUuid.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : m_authUuid);
            arg.replace(QStringLiteral("${auth_access_token}"), m_authToken.isEmpty() ? QStringLiteral("0") : m_authToken);
            arg.replace(QStringLiteral("${user_type}"), m_isOnline ? QStringLiteral("msa") : QStringLiteral("mojang"));
            arg.replace(QStringLiteral("${version_type}"),
                        versionJson[QStringLiteral("type")].toString(QStringLiteral("release")));
            arg.replace(QStringLiteral("${game_assets}"),
                        m_gameDir + QStringLiteral("/assets/virtual/legacy"));  // 1.7.2 legacy
            arg.replace(QStringLiteral("${user_properties}"), QStringLiteral("{}"));
            arg.replace(QStringLiteral("${auth_session}"), m_authToken.isEmpty() ? QStringLiteral("0") : m_authToken);  // pre-1.6
            arg.replace(QStringLiteral("${resolution_width}"), QStringLiteral("854"));
            arg.replace(QStringLiteral("${resolution_height}"), QStringLiteral("480"));
            arg.replace(QStringLiteral("${launcher_name}"), QStringLiteral("ShadowLauncher"));
            arg.replace(QStringLiteral("${launcher_version}"), QStringLiteral("1.0"));
            arg.replace(QStringLiteral("${profile_name}"), versionId);
            arg.replace(QStringLiteral("${quickPlayPath}"), QString());  // not used

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

// ============================================================
// Helpers — Rules Evaluation (1.13+ arguments)
// ============================================================

bool Launcher::evaluateRule(const QJsonObject& rule)
{
    QString action = rule[QStringLiteral("action")].toString(QStringLiteral("allow"));

    // Check OS constraints
    if (rule.contains(QStringLiteral("os"))) {
        QJsonObject os = rule[QStringLiteral("os")].toObject();
        QString osName = os[QStringLiteral("name")].toString();
#ifdef Q_OS_WIN
        bool osMatch = (osName == QStringLiteral("windows"));
#elif defined(Q_OS_MACOS)
        bool osMatch = (osName == QStringLiteral("osx"));
#else
        bool osMatch = (osName == QStringLiteral("linux"));
#endif
        if (!osMatch) return false;
    }

    // Check feature constraints — we are NOT a demo/realm/quickPlay user
    if (rule.contains(QStringLiteral("features"))) {
        QJsonObject features = rule[QStringLiteral("features")].toObject();
        if (features.contains(QStringLiteral("is_demo_user")))
            return false;  // normal user is not demo
        if (features.contains(QStringLiteral("has_custom_resolution")))
            return false;
        if (features.contains(QStringLiteral("is_quick_play_singleplayer")))
            return false;
        if (features.contains(QStringLiteral("is_quick_play_multiplayer")))
            return false;
        if (features.contains(QStringLiteral("is_quick_play_realms")))
            return false;
    }

    return (action == QStringLiteral("allow"));
}

bool Launcher::evaluateRules(const QJsonArray& rules)
{
    // Default: include
    // If any rule matches with "allow", include
    // If any rule matches with "disallow", exclude
    for (const QJsonValue& val : rules) {
        QJsonObject rule = val.toObject();
        if (evaluateRule(rule)) {
            return rule[QStringLiteral("action")].toString() == QStringLiteral("allow");
        }
    }
    return true;  // no rule matched → default allow
}

// ============================================================
// Legacy Asset Virtual Directory (1.7.2)
// ============================================================

void Launcher::ensureLegacyAssets()
{
    // Reads assets/indexes/legacy.json and builds assets/virtual/legacy/
    // from the hash-based assets/objects/ directory.
    // This is only needed for Minecraft 1.7.2 (assetIndex.id == "legacy").

    QString legacyDir = m_gameDir + QStringLiteral("/assets/virtual/legacy");
    QString indexFile = m_gameDir + QStringLiteral("/assets/indexes/legacy.json");

    // Already done
    if (QDir(legacyDir).exists() && !QDir(legacyDir).isEmpty(QDir::Dirs | QDir::Files))
        return;

    if (!QFileInfo::exists(indexFile)) {
        qCWarning(logLaunch) << "Legacy asset index not found:" << indexFile;
        return;
    }

    QFile file(indexFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logLaunch) << "Cannot open legacy.json";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    QJsonObject objects = doc.object()[QStringLiteral("objects")].toObject();
    if (objects.isEmpty()) return;

    QString objectsDir = m_gameDir + QStringLiteral("/assets/objects");
    int created = 0;

    qCInfo(logLaunch) << "Building legacy asset virtual directory...";

    for (auto it = objects.begin(); it != objects.end(); ++it) {
        QString virtualPath = it.key();  // e.g. "minecraft/lang/zh_cn.lang"
        QJsonObject info = it->toObject();
        QString hash = info[QStringLiteral("hash")].toString();
        if (hash.isEmpty()) continue;

        QString sourcePath = objectsDir + QLatin1Char('/')
                           + hash.left(2) + QLatin1Char('/') + hash;
        QString destPath = legacyDir + QLatin1Char('/') + virtualPath;

        QFileInfo destFi(destPath);
        QDir().mkpath(destFi.absolutePath());

        if (!QFileInfo::exists(destPath) && QFileInfo::exists(sourcePath)) {
            if (QFile::copy(sourcePath, destPath))
                created++;
        }
    }

    qCInfo(logLaunch) << "Legacy assets built:" << created << "files →" << legacyDir;
}

} // namespace ShadowLauncher
