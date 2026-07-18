// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "launcher.h"
#include "../utils/logger.h"
#include "mc_language.h"

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

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <tlhelp32.h>
#endif

// QZipReader is a Qt private API in QtGui.
// If linking fails, add  Qt6::GuiPrivate  to target_link_libraries in CMakeLists.txt.
#include <private/qzipreader_p.h>

namespace ShadowLauncher {

// Default optimized JVM args (G1GC tuning)
static const char* DEFAULT_JVM_ARGS[] = {
    "-XX:+UseG1GC",
    "-XX:+UnlockExperimentalVMOptions",
    "-XX:G1NewSizePercent=20",
    "-XX:G1ReservePercent=20",
    "-XX:MaxGCPauseMillis=50",
    "-XX:G1HeapRegionSize=32M",
    "-XX:-UseAdaptiveSizePolicy",
    "-XX:-OmitStackTraceInFastThrow",
    "-XX:+IgnoreUnrecognizedVMOptions",
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
                     const QString& jvmArgs, const QString& gameArgs, bool highPerfGpu)
{
    // --- Validate ---
    QString errorMsg;
    if (!validateLaunch(versionId, javaPath, errorMsg)) {
        emit launchFinished(false, errorMsg);
        return;
    }

    // --- Abort if already running ---
    if (isRunning()) {
        emit launchFinished(false, tr("另一个 Minecraft 实例已在运行"));
        return;
    }

    m_jvmArgs = jvmArgs;
    m_gameArgs = gameArgs;
    m_highPerfGpu = highPerfGpu;
    m_currentVersionId = versionId;
    m_cancelling = false;

    // --- Load version JSON ---
    QString jsonPath = m_gameDir + QStringLiteral("/versions/") + versionId
                       + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        emit launchFinished(false, tr("无法读取版本配置: %1").arg(jsonPath));
        return;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();

    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit launchFinished(false, tr("版本配置格式错误: %1").arg(parseErr.errorString()));
        return;
    }

    QJsonObject versionJson = doc.object();

    // --- Extract natives (before building args so library path exists) ---
    extractNatives(versionId, versionJson);

    // --- Ensure options.txt has language setting ---
    // Mode 0: off; Mode 1: system locale (default); Mode 2: IP region (set during install, skip)
    if (m_autoLangMode == 1) {
        ensureOptionsTxt(versionId);
    }

    // --- Build arguments ---
    QStringList args = buildArgs(versionId, maxMemoryMB, versionJson);

    emit launchProgress(tr("启动 Minecraft %1...").arg(versionId));

    m_process->setWorkingDirectory(m_gameDir);

    // High-performance GPU: set env vars for NVIDIA Optimus / AMD Switchable Graphics
    if (m_highPerfGpu) {
        auto env = m_process->processEnvironment();
        env.insert(QStringLiteral("SHIM_MCCOMPAT"), QStringLiteral("0x800000001"));
        m_process->setProcessEnvironment(env);
    }

    m_process->start(javaPath, args);
}

void Launcher::cancel()
{
    if (!isRunning()) return;

    qCInfo(logLaunch) << QStringLiteral("停止游戏 版本=%1 pid=%2").arg(m_currentVersionId).arg(m_process ? m_process->processId() : -1);
    m_cancelling = true;
    emit launchProgress(tr("正在停止 Minecraft..."));

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
    // Always fire — QProcess may think it's done (detached window) but OS process still alive
    if (m_pid > 0) {
        forceKill();
    } else if (m_process) {
        m_process->kill();
    }
}

// ============================================================
// Private Slots
// ============================================================

void Launcher::onProcessStarted()
{
    m_pid = m_process->processId();
    emit launchStarted();
    emit launchProgress(tr("Minecraft 进程已启动 (PID: %1)")
                        .arg(m_pid));
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
        QString msg = tr("Minecraft 异常退出 (退出码: %1)").arg(exitCode);
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
        ? tr("启动失败: %1").arg(QString::fromLatin1(messages[idx]))
        : tr("未知进程错误");

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
        errorMsg = tr("Java 未找到: %1").arg(javaPath);
        return false;
    }

    // 2. Version directory exists
    QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    if (!QDir(versionDir).exists()) {
        errorMsg = tr("版本目录不存在: %1").arg(versionDir);
        return false;
    }

    // 3. Version JAR exists
    QString jarPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (!QFileInfo::exists(jarPath)) {
        errorMsg = tr("版本核心文件缺失: %1").arg(jarPath);
        return false;
    }

    // 4. Version JSON exists
    QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    if (!QFileInfo::exists(jsonPath)) {
        errorMsg = tr("版本配置文件缺失: %1").arg(jsonPath);
        return false;
    }

    return true;
}

// ============================================================
// Private Helpers — Force Kill
// ============================================================

void Launcher::forceKill()
{
    qint64 pid = m_pid > 0 ? m_pid : (m_process ? m_process->processId() : 0);
    if (pid <= 0) return;

#ifdef Q_OS_WIN
    // Take a snapshot of all processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        // Fallback: try taskkill
        QProcess::execute(QStringLiteral("taskkill"), QStringList() << QStringLiteral("/F") << QStringLiteral("/T") << QStringLiteral("/PID") << QString::number(pid));
        return;
    }

    // First pass: kill the target process itself
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (hProc) {
        TerminateProcess(hProc, 1);
        CloseHandle(hProc);
    }

    // Second pass: kill all child processes
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (pe.th32ParentProcessID == (DWORD)pid) {
                HANDLE hChild = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hChild) {
                    TerminateProcess(hChild, 1);
                    CloseHandle(hChild);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
#else
    if (m_process) m_process->kill();
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

// Derive relative library path from Maven coordinate ("group:artifact:version")
// e.g. "net.fabricmc:fabric-loader:0.19.3" → "net/fabricmc/fabric-loader/0.19.3/fabric-loader-0.19.3.jar"
static QString mavenNameToPath(const QString& mavenName)
{
    const QStringList parts = mavenName.split(QLatin1Char(':'));
    if (parts.size() < 3) return {};
    QString group = parts[0];
    QString artifact = parts[1];
    QString version = parts[2];
    QString groupPath = group.replace(QLatin1Char('.'), QLatin1Char('/'));
    return groupPath + QLatin1Char('/') + artifact + QLatin1Char('/')
           + version + QLatin1Char('/') + artifact + QLatin1Char('-') + version + QStringLiteral(".jar");
}

// Construct download URL from library entry (used by Fabric library downloader)
//   url field  → "https://maven.fabricmc.net/"
//   name field → "net.fabricmc:fabric-loader:0.19.3"
//  Output     → "https://maven.fabricmc.net/net/fabricmc/fabric-loader/0.19.3/fabric-loader-0.19.3.jar"
static QString mavenDownloadUrl(const QJsonObject& lib, const QString& bmclapiPrefix = QStringLiteral("https://bmclapi2.bangbang93.com/maven/"))
{
    QString mavenName = lib[QStringLiteral("name")].toString();
    QString relPath = mavenNameToPath(mavenName);
    if (relPath.isEmpty()) return {};
    QString baseUrl = lib[QStringLiteral("url")].toString(QStringLiteral("https://maven.fabricmc.net/"));
    // Replace official Maven with BMCLAPI mirror for faster download in China
    baseUrl.replace(QStringLiteral("https://maven.fabricmc.net/"), bmclapiPrefix);
    baseUrl.replace(QStringLiteral("https://maven.neoforged.net/"), bmclapiPrefix);
    if (!baseUrl.endsWith(QLatin1Char('/'))) baseUrl += QLatin1Char('/');
    return baseUrl + relPath;
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

    // Fallback: derive path from Maven name (e.g. net.fabricmc:fabric-loader:0.19.3)
    QString mavenPath = mavenNameToPath(lib[QStringLiteral("name")].toString());
    if (!mavenPath.isEmpty())
        return librariesDir + QStringLiteral("/") + mavenPath;

    return {};
}

static QStringList buildClasspath(const QString& versionId, const QJsonObject& versionJson,
                                   const QString& gameDir,
                                   const QSet<QString>& moduleExcludePaths = {})
{
    QStringList cp;
    QString libsDir = gameDir + QStringLiteral("/libraries");

    // Add version JAR first
    QString versionJar = gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (QFileInfo::exists(versionJar)) {
        cp.append(versionJar);
    }

    // Collect libraries from version JSON + all inheritsFrom parents
    QSet<QString> seenLibs;  // deduplicate by resolved path
    QStringList versionStack;  // JSON chain, youngest first
    QJsonObject currentJson = versionJson;
    QString currentId = versionId;
    while (true) {
        // Add libraries from current JSON
        QJsonArray libraries = currentJson[QStringLiteral("libraries")].toArray();
        for (const QJsonValue& libVal : libraries) {
            QJsonObject lib = libVal.toObject();
            if (!shouldIncludeLibrary(lib)) continue;

            QString libPath = resolveLibraryPath(lib, libsDir);
            if (!libPath.isEmpty() && QFileInfo::exists(libPath)) {
                // Skip JARs whose group:artifact is on the module path (NeoForge -p flag)
                // Uses prefix match to exclude ALL versions, not just exact JAR paths
                QString relativePath = libPath.mid(libsDir.length() + 1);
                bool isModuleJar = false;
                for (const QString& prefix : moduleExcludePaths) {
                    if (relativePath.startsWith(prefix)) {
                        isModuleJar = true;
                        break;
                    }
                }
                if (isModuleJar) {
                    qDebug() << "[Launch] Skipping module-path JAR:" << relativePath;
                    continue;
                }
                // Deduplicate: same JAR may appear in both Forge and base JSON
                if (!seenLibs.contains(libPath)) {
                    seenLibs.insert(libPath);
                    cp.append(libPath);
                }
            }
        }

        // Walk up inheritsFrom chain
        QString parentId = currentJson[QStringLiteral("inheritsFrom")].toString();
        if (parentId.isEmpty() || parentId == currentId) break;
        // Prevent infinite loops
        if (versionStack.contains(parentId)) break;
        versionStack.append(parentId);

        QString parentPath = gameDir + QStringLiteral("/versions/") + parentId
                           + QStringLiteral("/") + parentId + QStringLiteral(".json");
        QFile pf(parentPath);
        if (!pf.open(QIODevice::ReadOnly)) break;
        QJsonParseError parseErr;
        QJsonDocument parentDoc = QJsonDocument::fromJson(pf.readAll(), &parseErr);
        pf.close();
        if (parseErr.error != QJsonParseError::NoError) break;
        currentJson = parentDoc.object();
        currentId = parentId;
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
    // Collect arguments.jvm from version JSON chain (Forge/NeoForge may add module flags)
    QStringList chainJvmArgs;
    {
        QJsonObject chainJson = versionJson;
        QString chainId = versionId;
        QStringList visited;
        while (true) {
            QJsonObject argsObj = chainJson[QStringLiteral("arguments")].toObject();
            QJsonArray jvmRaw = argsObj[QStringLiteral("jvm")].toArray();
            for (const QJsonValue& val : jvmRaw) {
                if (val.isString()) {
                    chainJvmArgs.append(val.toString());
                } else if (val.isObject()) {
                    QJsonObject obj = val.toObject();
                    QJsonArray rulesArr = obj[QStringLiteral("rules")].toArray();
                    if (!rulesArr.isEmpty() && !evaluateRules(rulesArr)) continue;
                    QJsonValue value = obj[QStringLiteral("value")];
                    if (value.isString()) {
                        chainJvmArgs.append(value.toString());
                    } else if (value.isArray()) {
                        for (const QJsonValue& v : value.toArray()) {
                            if (v.isString()) chainJvmArgs.append(v.toString());
                        }
                    }
                }
            }
            // Walk inheritsFrom
            QString parentId = chainJson[QStringLiteral("inheritsFrom")].toString();
            if (parentId.isEmpty() || visited.contains(parentId)) break;
            visited.append(parentId);
            QString parentPath = m_gameDir + QStringLiteral("/versions/") + parentId
                               + QStringLiteral("/") + parentId + QStringLiteral(".json");
            QFile pf(parentPath);
            if (!pf.open(QIODevice::ReadOnly)) break;
            QJsonParseError pe;
            QJsonDocument pd = QJsonDocument::fromJson(pf.readAll(), &pe);
            pf.close();
            if (pe.error != QJsonParseError::NoError) break;
            chainJson = pd.object();
            chainId = parentId;
        }
    }
    // Add chain JVM args after memory flags, before user/default flags
    // Replace NeoForge template variables (${library_directory}, ${classpath_separator}, etc.)
    // Minecraft 26.2+ also uses ${natives_directory}, ${classpath}, etc.
    const QString libDir = m_gameDir + QStringLiteral("/libraries");
    const QString nativesDir = m_gameDir + QStringLiteral("/versions/") + versionId
                               + QStringLiteral("/natives");
#ifdef Q_OS_WIN
    const QString classpathSep(QStringLiteral(";"));
#else
    const QString classpathSep(QStringLiteral(":"));
#endif
    for (const QString& a : chainJvmArgs) {
        QString arg = a;
        arg.replace(QStringLiteral("${library_directory}"), libDir);
        arg.replace(QStringLiteral("${classpath_separator}"), classpathSep);
        arg.replace(QStringLiteral("${version_name}"), versionId);
        arg.replace(QStringLiteral("${natives_directory}"), nativesDir);
        args << arg;
    }

    // Always apply default optimized G1GC flags first
    for (const char* arg : DEFAULT_JVM_ARGS) {
        args << QString::fromLatin1(arg);
    }

    // Append user-provided custom JVM args (space-separated, may override defaults)
    if (!m_jvmArgs.isEmpty()) {
        const QStringList customArgs = m_jvmArgs.split(QRegularExpression(QStringLiteral("\\s+")),
                                                       Qt::SkipEmptyParts);
        for (const auto& arg : customArgs) {
            args << arg;
        }
    }

    // ── Version jar path for Forge ──
    QString versionJar = m_gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    args << QStringLiteral("-Dminecraft.client.jar=%1").arg(versionJar);

    // ── Natives path (nativesDir already computed above) ──
    args << QStringLiteral("-Djava.library.path=%1").arg(nativesDir);

    // ── Extract module-path group:artifact prefixes (from NeoForge -p flag)
    //     to exclude ALL versions from classpath, not just exact JAR paths
    QSet<QString> moduleExclude;
    bool inModulePath = false;
    for (const QString& a : args) {
        if (a == QStringLiteral("-p")) { inModulePath = true; continue; }
        if (inModulePath) {
            const QString libPrefix = m_gameDir + QStringLiteral("/libraries/");
            const QStringList jars = a.split(classpathSep, Qt::SkipEmptyParts);
            for (const QString& jar : jars) {
                if (jar.startsWith(libPrefix)) {
                    QString rel = jar.mid(libPrefix.length());
                    // Trim version/ and filename.jar → group/artifact/ prefix
                    // e.g. org/ow2/asm/asm/9.7/asm-9.7.jar → org/ow2/asm/asm
                    QStringList parts = rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
                    if (parts.size() >= 3) {
                        parts.removeLast();  // remove filename.jar
                        if (!parts.isEmpty()) parts.removeLast();  // remove version/
                        if (!parts.isEmpty()) {
                            moduleExclude.insert(parts.join(QLatin1Char('/')) + QLatin1Char('/'));
                        }
                    }
                }
            }
            inModulePath = false;
        }
    }

    // ── Classpath from version JSON ──
    QStringList cp = buildClasspath(versionId, versionJson, m_gameDir, moduleExclude);
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
    // Walk inheritsFrom chain: child args (Forge --launchTarget) FIRST, then parent args
    // modlauncher strips its own flags and passes the rest to Minecraft
    QJsonArray gameArgs;
    {
        // First: collect args from leaf to root, then reverse to get root→leaf order
        QJsonArray chainArgs;  // root-args ... child-args (will reverse)
        QJsonObject chainJson = versionJson;
        QString chainId = versionId;
        QStringList visitedArgs;
        while (true) {
            QJsonObject argsObj = chainJson[QStringLiteral("arguments")].toObject();
            if (!argsObj.isEmpty()) {
                // Modern format (1.13+): arguments.game
                const QJsonArray raw = argsObj[QStringLiteral("game")].toArray();
                for (const QJsonValue& val : raw) {
                    if (val.isString()) {
                        chainArgs.append(val.toString());
                    } else if (val.isObject()) {
                        QJsonObject obj = val.toObject();
                        QJsonArray rulesArr = obj[QStringLiteral("rules")].toArray();
                        if (!rulesArr.isEmpty()) {
                            bool include = evaluateRules(rulesArr);
                            if (!include) continue;
                        }
                        QJsonValue value = obj[QStringLiteral("value")];
                        if (value.isString()) {
                            chainArgs.append(value.toString());
                        } else if (value.isArray()) {
                            for (const QJsonValue& v : value.toArray()) {
                                if (v.isString()) chainArgs.append(v.toString());
                            }
                        }
                    }
                }
            } else {
                // Legacy format: minecraftArguments string
                QString mcArgs = chainJson[QStringLiteral("minecraftArguments")].toString();
                if (!mcArgs.isEmpty()) {
                    static const QRegularExpression argSplitter(
                        QStringLiteral(R"("(?:[^"\\]|\\.)*"|\S+)"));
                    QRegularExpressionMatchIterator it = argSplitter.globalMatch(mcArgs);
                    while (it.hasNext()) {
                        QRegularExpressionMatch m = it.next();
                        QString part = m.captured(0);
                        if (part.startsWith(QLatin1Char('"')) && part.endsWith(QLatin1Char('"')))
                            part = part.mid(1, part.size() - 2);
                        if (!part.isEmpty()) chainArgs.append(part);
                    }
                }
            }
            // Walk up
            QString parentId = chainJson[QStringLiteral("inheritsFrom")].toString();
            if (parentId.isEmpty() || visitedArgs.contains(parentId)) break;
            visitedArgs.append(parentId);
            QString parentPath = m_gameDir + QStringLiteral("/versions/") + parentId
                               + QStringLiteral("/") + parentId + QStringLiteral(".json");
            QFile pf(parentPath);
            if (!pf.open(QIODevice::ReadOnly)) break;
            QJsonParseError pe;
            QJsonDocument pd = QJsonDocument::fromJson(pf.readAll(), &pe);
            pf.close();
            if (pe.error != QJsonParseError::NoError) break;
            chainJson = pd.object();
            chainId = parentId;
        }
        // chainArgs is [child-args..., parent-args...]
        // Child args (--launchTarget forge_client) come first, parent args follow
        // No reverse needed — this is the correct order for modlauncher
        gameArgs = chainArgs;
    }

    // Read asset index ID from version JSON chain (not just the leaf JSON)
    // For Forge/NeoForge: parent MC version JSON has "assets" or "assetIndex" field
    QString assetIndexId;
    {
        QJsonObject chainJson = versionJson;
        QString chainId = versionId;
        QStringList visited;
        while (true) {
            // Try "assetIndex.id" (modern format, 1.7.2+)
            QJsonObject idx = chainJson[QStringLiteral("assetIndex")].toObject();
            if (!idx.isEmpty()) {
                assetIndexId = idx[QStringLiteral("id")].toString();
            }
            // Fallback: "assets" field (legacy format or post-rename)
            if (assetIndexId.isEmpty()) {
                QJsonValue av = chainJson[QStringLiteral("assets")];
                if (av.isString()) assetIndexId = av.toString();
            }
            if (!assetIndexId.isEmpty()) break;

            QString parentId = chainJson[QStringLiteral("inheritsFrom")].toString();
            if (parentId.isEmpty() || visited.contains(parentId)) break;
            visited.append(parentId);
            QString parentPath = m_gameDir + QStringLiteral("/versions/") + parentId
                               + QStringLiteral("/") + parentId + QStringLiteral(".json");
            QFile pf(parentPath);
            if (!pf.open(QIODevice::ReadOnly)) break;
            QJsonParseError pe;
            QJsonDocument pd = QJsonDocument::fromJson(pf.readAll(), &pe);
            pf.close();
            if (pe.error != QJsonParseError::NoError) break;
            chainJson = pd.object();
            chainId = parentId;
        }
        // Final fallback: use leaf version ID
        if (assetIndexId.isEmpty()) assetIndexId = versionId;
    }

    // Build legacy virtual asset directory for 1.7.2 and pre-1.6
    if (assetIndexId == QStringLiteral("legacy") || assetIndexId == QStringLiteral("pre-1.6")) {
        const_cast<Launcher*>(this)->ensureLegacyAssets(assetIndexId);

        // Also populate game resources/ for pre-1.6 (s3.amazonaws.com is dead)
        if (assetIndexId == QStringLiteral("pre-1.6")) {
            QString gameResDir = m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game/resources");
            QString virtualDir = m_gameDir + QStringLiteral("/assets/virtual/") + assetIndexId;
            QDir().mkpath(gameResDir);
            // Copy sound/music/newsound/sound3 from virtual assets to game resources
            const QStringList resDirs = {QStringLiteral("sound"), QStringLiteral("newsound"),
                                         QStringLiteral("sound3"), QStringLiteral("music"),
                                         QStringLiteral("newmusic"), QStringLiteral("streaming")};
            int resCopied = 0;
            for (const QString& sub : resDirs) {
                QString srcDir = virtualDir + QLatin1Char('/') + sub;
                QString dstDir = gameResDir + QLatin1Char('/') + sub;
                if (!QDir(srcDir).exists()) continue;
                QDir().mkpath(dstDir);
                QDirIterator it(srcDir, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    QString rel = QDir(srcDir).relativeFilePath(it.filePath());
                    QString dst = dstDir + QLatin1Char('/') + rel;
                    if (!QFileInfo::exists(dst)) {
                        QDir().mkpath(QFileInfo(dst).absolutePath());
                        QFile::copy(it.filePath(), dst);
                        resCopied++;
                    }
                }
            }
            if (resCopied > 0)
                qCInfo(logLaunch) << QStringLiteral("游戏资源已复制 数量=%1 目标=%2").arg(resCopied).arg(gameResDir);
        }
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
                        m_gameDir + QStringLiteral("/assets/virtual/") + assetIndexId);  // legacy/pre-1.6
            arg.replace(QStringLiteral("${user_properties}"), QStringLiteral("{}"));
            arg.replace(QStringLiteral("${auth_session}"), m_authToken.isEmpty() ? QStringLiteral("0") : m_authToken);  // pre-1.6
            arg.replace(QStringLiteral("${resolution_width}"), QStringLiteral("854"));
            arg.replace(QStringLiteral("${resolution_height}"), QStringLiteral("480"));
            arg.replace(QStringLiteral("${clientid}"), QStringLiteral(""));
            arg.replace(QStringLiteral("${auth_xuid}"), QStringLiteral(""));
            arg.replace(QStringLiteral("${launcher_name}"), QStringLiteral("ShadowLauncher"));
            arg.replace(QStringLiteral("${launcher_version}"), QStringLiteral("1.0"));
            arg.replace(QStringLiteral("${profile_name}"), versionId);
            arg.replace(QStringLiteral("${quickPlayPath}"), QString());  // not used

            if (!arg.isEmpty()) args << arg;
        }
    }

    // ── User-provided game args (e.g. --width 1920 --height 1080) ──
    if (!m_gameArgs.isEmpty()) {
        const QStringList userGameArgs = m_gameArgs.split(QRegularExpression(QStringLiteral("\\s+")),
                                                          Qt::SkipEmptyParts);
        for (const auto& arg : userGameArgs) {
            args << arg;
        }
    }

    // Ensure game directory exists
    QDir().mkpath(m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game"));

    return args;
}

// ============================================================
// Private Helpers — Natives Extraction
// ============================================================

// Helper: resolve all libraries including inherited versions (Forge/NeoForge inheritsFrom chain)
static QJsonArray resolveMergedLibraries(const QString& gameDir, const QJsonObject& topJson) {
    QJsonArray all = topJson.value(QStringLiteral("libraries")).toArray();
    QString inherits = topJson.value(QStringLiteral("inheritsFrom")).toString();
    QStringList seen;
    while (!inherits.isEmpty() && !seen.contains(inherits)) {
        seen.append(inherits);
        QFile f(gameDir + QStringLiteral("/versions/") + inherits + QStringLiteral("/") + inherits + QStringLiteral(".json"));
        if (!f.open(QIODevice::ReadOnly)) break;
        QJsonObject parent = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        if (parent.isEmpty()) break;
        for (const auto& lib : parent.value(QStringLiteral("libraries")).toArray())
            all.append(lib);
        inherits = parent.value(QStringLiteral("inheritsFrom")).toString();
    }
    return all;
}

bool Launcher::extractNatives(const QString& versionId, const QJsonObject& versionJson)
{
    QString nativesDir = m_gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/natives");

    qCInfo(logLaunch) << QStringLiteral("开始解压natives 版本=%1 目标=%2").arg(versionId, nativesDir);

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
    const QJsonArray libraries = resolveMergedLibraries(m_gameDir, versionJson);

    // Determine platform-specific native classifier prefix
#ifdef Q_OS_WIN
    const QString nativePrefix = QStringLiteral("natives-windows");
#elif defined(Q_OS_MACOS)
    const QString nativePrefix = QStringLiteral("natives-osx");
#else
    const QString nativePrefix = QStringLiteral("natives-linux");
#endif

    qCInfo(logLaunch) << QStringLiteral("扫描natives库 数量=%1 平台=%2").arg(libraries.size()).arg(nativePrefix);

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
                qCWarning(logLaunch) << QStringLiteral("无法打开natives JAR 路径=%1").arg(jarPath);
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

    qCInfo(logLaunch) << QStringLiteral("natives解压完成 JAR数=%1 文件数=%2 目标=%3").arg(jarCount).arg(extractedCount).arg(nativesDir);
    return extractedCount > 0 || jarCount == 0;
}

// ============================================================
// Private Helpers — Language Detection & options.txt
// ============================================================

void Launcher::ensureOptionsTxt(const QString& versionId)
{
    QString gameDir = m_gameDir + QStringLiteral("/versions/") + versionId
                      + QStringLiteral("/game");
    QString mcLang = mc_language::localeToMinecraftLang(QLocale::system());
    mc_language::writeOptionsTxt(gameDir, mcLang);
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

    // Check feature constraints — normal user has ZERO special features
    if (rule.contains(QStringLiteral("features")) && !rule[QStringLiteral("features")].toObject().isEmpty()) {
        return false;  // any unknown feature → not a match for normal user
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
    return false;  // no rule matched → exclude (Minecraft spec)
}

// ============================================================
// Legacy Asset Virtual Directory (1.7.2)
// ============================================================

void Launcher::ensureLegacyAssets(const QString& assetIndexId)
{
    // Reads assets/indexes/<id>.json and builds assets/virtual/<id>/
    // from the hash-based assets/objects/ directory.
    // Needed for: legacy (1.7.2), pre-1.6 (1.0), and similar old asset systems.

    QString legacyDir = m_gameDir + QStringLiteral("/assets/virtual/") + assetIndexId;
    QString indexFile = m_gameDir + QStringLiteral("/assets/indexes/") + assetIndexId + QStringLiteral(".json");

    // Check if the index file exists (skip re-build if nothing to read)
    if (!QFileInfo::exists(indexFile)) {
        qCWarning(logLaunch) << QStringLiteral("旧版资源索引不存在 路径=%1").arg(indexFile);
        return;
    }

    // Always process — per-file check prevents redundant copies

    QFile file(indexFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logLaunch) << QStringLiteral("无法打开旧版资源索引JSON");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    QJsonObject objects = doc.object()[QStringLiteral("objects")].toObject();
    if (objects.isEmpty()) return;

    QString objectsDir = m_gameDir + QStringLiteral("/assets/objects");
    int created = 0;

    qCInfo(logLaunch) << QStringLiteral("开始构建旧版资源虚拟目录");

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

    qCInfo(logLaunch) << QStringLiteral("旧版资源构建完成 文件数=%1 目标=%2").arg(created).arg(legacyDir);
}

} // namespace ShadowLauncher
