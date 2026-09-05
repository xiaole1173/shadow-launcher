// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "launcher.h"
#include "../utils/logger.h"
#include "mc_language.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>
#include <QVersionNumber>

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

// Default optimized JVM args (non-GC tuning — GC策略见 collectGcArgs)
static const char* DEFAULT_JVM_ARGS[] = {
    "-XX:-OmitStackTraceInFastThrow",
    "-XX:+IgnoreUnrecognizedVMOptions",
    "-XX:+DisableExplicitGC",
    "-Dfml.ignoreInvalidMinecraftCertificates=true",
    "-Dfml.ignorePatchDiscrepancies=true",
    "-Dlog4j2.formatMsgNoLookups=true",  // CVE-2021-44228 Log4j RCE 防御
};

// ── 短路径转换（Windows 8.3 短文件名）──
// 缓解 Java8/LWJGL2 世代非 ASCII 路径下加载 native DLL 失败的问题。
// 注意：8.3 短名含 '~'，会破坏 Forge 的 jij: URI，因此只在 legacy（Java≤8 且非
// Jar-in-Jar）场景使用；现代版本一律走完整路径（见 buildArgs 的 useShortPaths）。
#ifdef Q_OS_WIN
static QString toShortPath(const QString& path)
{
    if (path.isEmpty()) return path;
    // 已经是纯 ASCII 且不含空格等特殊字符 → 无需转换
    // （注意：短路径本身就是纯 ASCII，这里只是提前退出优化）
    QByteArray utf8 = path.toUtf8();
    bool allAscii = true;
    for (char c : utf8) {
        if (static_cast<unsigned char>(c) > 127) { allAscii = false; break; }
    }
    if (allAscii) return QDir::toNativeSeparators(path);

    DWORD len = GetShortPathNameW(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, 0);
    if (len == 0) {
        qCWarning(logLaunch) << QStringLiteral("[启动] 短路径转换失败 路径=%1 错误=%2")
                             .arg(path).arg(GetLastError());
        return QDir::toNativeSeparators(path); // 回退到原生路径
    }
    std::vector<wchar_t> buf(len + 1);
    DWORD written = GetShortPathNameW(reinterpret_cast<LPCWSTR>(path.utf16()),
                                       buf.data(), len + 1);
    if (written == 0 || written > len) {
        qCWarning(logLaunch) << QStringLiteral("[启动] 短路径转换写入失败 路径=%1 错误=%2")
                             .arg(path).arg(GetLastError());
        return QDir::toNativeSeparators(path);
    }
    QString result = QString::fromWCharArray(buf.data());
    qCDebug(logLaunch) << QStringLiteral("短路径转换: %1 → %2").arg(path, result);
    return result;
}
#else
static QString toShortPath(const QString& path) { return path; }
#endif

// ── JVM 参数引号感知拆分（2026-08-17）──
// 输入形如: -Xmx4G -javaagent:"D:\MC FAN\.minecraft\authlib-injector.jar"=https://...
// 规则：
//   - 空白（空格/制表）分隔参数
//   - 双引号包裹的部分整体保留（内部空格不拆分），引号本身从参数中剥离
//   - 转义：参数内 "" 表示一个字面双引号
// 返回值是可直接交给 QProcess 的参数列表（QProcess 不再二次 shell 解析，
// 因此剥离引号是正确行为；但 bat 导出脚本走 cmd /c 时由另一处再包引号）。
static QStringList tokenizeJvmArgs(const QString& input)
{
    QStringList result;
    QString cur;
    bool inQuote = false;
    const int n = input.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = input.at(i);
        if (c == QLatin1Char('"')) {
            if (i + 1 < n && input.at(i + 1) == QLatin1Char('"')) {
                // 转义引号 ""
                cur += QLatin1Char('"');
                ++i;
            } else {
                inQuote = !inQuote;
            }
        } else if (c.isSpace() && !inQuote) {
            if (!cur.isEmpty()) {
                result.append(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.isEmpty())
        result.append(cur);
    return result;
}

// ── bat 命令行参数引用（2026-08-10）──
// QProcess 数组传参会自动给含空格参数加引号；bat 字符串拼接不会 → 含空格参数
// （如 Fabric 26.x 的 "-DFabricMcEmu= net.minecraft.client.main.Main "）会被 cmd 拆成
// 两个参数 → JVM 把 mainClass 当主类、-cp 失效 → ClassNotFoundException。
// 空参数（--clientId 后的空值）也必须保留为 ""，否则后续参数错位。
// 规则：空 → ""；含 空格/tab/&|<>^" → 双引号包裹（内部 " → \"，对齐 CRT 解析）；否则原样。
static QString quoteBatArg(const QString& arg)
{
    if (arg.isEmpty()) return QStringLiteral("\"\"");
    bool needQuote = false;
    for (QChar c : arg) {
        if (c == QLatin1Char(' ') || c == QLatin1Char('\t') || c == QLatin1Char('"')
            || c == QLatin1Char('&') || c == QLatin1Char('|') || c == QLatin1Char('<')
            || c == QLatin1Char('>') || c == QLatin1Char('^')) {
            needQuote = true;
            break;
        }
    }
    if (!needQuote) return arg;
    QString q = QStringLiteral("\"");
    for (QChar c : arg) {
        if (c == QLatin1Char('"')) q += QStringLiteral("\\\"");
        else q += c;
    }
    q += QLatin1Char('"');
    return q;
}

// ── 智能 GC 策略选择 ──
// Java 21+ → 分代 ZGC, Java 15-20 → ZGC, Java 14- → G1GC
// 策略: Java 21+ → 分代 ZGC (性能最优), Java 15-20 → ZGC, Java 14- → G1GC
// ZGC 需要 Windows 10 1809+ (build 17763)，不支持时回退到 G1GC
// gcMode: 0=自动（智能选择） 1=分代ZGC 优先 2=仅 G1GC 3=不指定（返回空，跟随自定义参数）
// ============================================================
// 日志脱敏（2026-08-11）：启动参数/JVM 输出写入日志前过滤敏感凭据
// （正版/外置登录 accessToken、老版 --session 等；对齐主流启动器实现 FilterAccessToken）
// ============================================================
QString sanitizeLaunchLog(const QString& input)
{
    QString out = input;
    // 命令行参数：--accessToken *** / --accessToken=<v> / --session <v> / --session=<v>
    static const QRegularExpression reFlag(
        QStringLiteral("(--(?:accessToken|session)\\s*=\\s*|--(?:accessToken|session)\\s+)([^\\s]+)"));
    out.replace(reFlag, QStringLiteral("\\1<hidden>"));
    // JSON 形式（游戏/模组回显）："accessToken":"***" / "access_token":"***" / "session":"v"
    static const QRegularExpression reJson(
        QStringLiteral("(\"(?:accessToken|access_token|session)\"\\s*:\\s*\")([^\"]*)(\")"));
    out.replace(reJson, QStringLiteral("\\1<hidden>\\3"));
    // 键值形式（无 -- 前缀）：accessToken=v / access_token=*** / auth_session=v
    static const QRegularExpression reKv(
        QStringLiteral("(?<![\\w-])((?:accessToken|access_token|auth_session)\\s*=\\s*)([^\\s,;\"']+)"));
    out.replace(reKv, QStringLiteral("\\1<hidden>"));
    return out;
}

// 对齐主流启动器实现 LaunchAdvanceGC 四档语义（SetupType 0/1/2/3）
static QStringList collectGcArgs(int javaMajor, bool debugMode, int gcMode = 0)
{
    QStringList gc;

    if (gcMode == 3) {
        qCInfo(logLaunch) << QStringLiteral("[启动] GC策略: 不指定（跟随自定义参数）");
        return gc;  // 空——buildArgs 不再注入任何 GC 参数
    }

    bool canUseZgc = false;
#ifdef Q_OS_WIN
    // Windows 10 1809+ (build 17763) required for ZGC
    // QSysInfo::productVersion() returns e.g. "10.0.22631"
    const QString pv = QSysInfo::productVersion();
    const QStringList parts = pv.split(QLatin1Char('.'));
    if (parts.size() >= 3) {
        int major = parts[0].toInt();
        int build = parts[2].toInt();
        canUseZgc = (major >= 10 && build >= 17763);
    }
#else
    canUseZgc = true;
#endif

    // 模式 2：强制 G1GC（主流启动器 SetupType=2“仅 G1GC”）
    // 模式 1：ZGC 优先（主流启动器 SetupType=1：Java 21+ 分代 ZGC，20- G1GC）
    // 模式 0：自动（主流启动器 SetupType=0：Java 21+ 分代 ZGC，15-20 ZGC，14- G1GC）
    bool useZgc = false;
    if (gcMode == 1) {
        useZgc = canUseZgc && javaMajor >= 21;
    } else if (gcMode == 2) {
        useZgc = false;
    } else {  // 0 auto
        useZgc = canUseZgc && javaMajor >= 15;
    }

    if (useZgc) {
        gc << QStringLiteral("-XX:+UnlockExperimentalVMOptions");
        gc << QStringLiteral("-XX:+UseZGC");
        // -XX:+ZGenerational: default on Java 23+, optional on 21-22
        if (javaMajor == 21 || javaMajor == 22) {
            gc << QStringLiteral("-XX:+ZGenerational");
        }
        if (javaMajor >= 24) {
            gc << QStringLiteral("-XX:+UseCompactObjectHeaders");
        }
        qCInfo(logLaunch) << QStringLiteral("[启动] GC策略: ZGC (Java=%1, 模式=%2)").arg(javaMajor).arg(gcMode);
    } else {
        // G1GC (optimized)
        gc << QStringLiteral("-XX:+UseG1GC");
        gc << QStringLiteral("-XX:+UnlockExperimentalVMOptions");
        gc << QStringLiteral("-XX:G1NewSizePercent=20");
        gc << QStringLiteral("-XX:G1ReservePercent=20");
        gc << QStringLiteral("-XX:MaxGCPauseMillis=50");
        gc << QStringLiteral("-XX:G1HeapRegionSize=32M");
        gc << QStringLiteral("-XX:-UseAdaptiveSizePolicy");
        if (!debugMode) {
            gc << QStringLiteral("-XX:+PerfDisableSharedMem");
        }
        if (javaMajor == 8) {
            gc << QStringLiteral("-XX:+ParallelRefProcEnabled");
        }
        qCInfo(logLaunch) << QStringLiteral("[启动] GC策略: G1GC (ZGC不可用或强制G1, Win10=%1, Java=%2, 模式=%3)")
                           .arg(canUseZgc).arg(javaMajor).arg(gcMode);
    }

    return gc;
}

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
    // 启动器退出时游戏应独立存活（Java 与启动器无父子依赖）。
    // detach() 已标记 m_detached，此时不再 forceKill；
    // 仅当用户主动停止游戏、或 Launcher 因其它原因销毁而进程仍在时，才强制结束。
    if (isRunning() && !m_detached) {
        forceKill();
    }
}

// ============================================================
// Public API
// ============================================================

// ── pre-1.6 启动前确保 servers.dat 存在（2026-08-19）──
// 1.3.2 等版本的 ServerList 缺少 null 检查：servers.dat 缺失时
// CompressedStreamTools.read(File) 返回 null → 后续 getTag("servers")
// NPE → 游戏启动即崩溃（表现为"没有声音"：OpenAL 初始化后崩溃）。
// 写入合法空 NBT（root Compound { "servers" = 空 List }），gzip 封装
// （Java GZIPInputStream 读取），作为启动器辅助文件。
static void ensureServersDat(const QString& gameDir)
{
    if (gameDir.isEmpty()) return;
    const QString path = gameDir + QStringLiteral("/servers.dat");
    if (QFileInfo::exists(path)) return;

    // NBT 二进制：0A 00 00 | 09 00 07 "servers" | 0A 00 00 00 00 | 00
    QByteArray nbt;
    nbt.append(char(0x0A)); nbt.append(char(0x00)); nbt.append(char(0x00)); // root Compound, 空名
    nbt.append(char(0x09)); nbt.append(char(0x00)); nbt.append(char(0x07)); // TAG_List "servers"
    nbt.append(QByteArrayLiteral("servers"));
    nbt.append(char(0x0A)); nbt.append(char(0x00)); nbt.append(char(0x00));
    nbt.append(char(0x00)); nbt.append(char(0x00)); // 元素类型=Compound, 长度 0
    nbt.append(char(0x00)); // TAG_End

    // QCompress → zlib 格式；剥 2 字节 zlib 头 + 4 字节 adler 得 raw deflate
    const QByteArray z = qCompress(nbt);
    const QByteArray deflate = z.mid(2, z.size() - 2 - 4);

    // CRC32（zlib crc32）
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < nbt.size(); ++i) {
        crc ^= quint8(nbt.at(i));
        for (int k = 0; k < 8; ++k)
            crc = (crc >> 1) ^ (0xEDB88320u & quint32(0u - (crc & 1u)));
    }
    crc = ~crc;

    QByteArray gz;
    gz.append(char(0x1F)); gz.append(char(0x8B)); gz.append(char(0x08)); gz.append(char(0x00));
    gz.append(char(0x00)); gz.append(char(0x00)); gz.append(char(0x00)); gz.append(char(0x00));
    gz.append(char(0x00)); gz.append(char(0xFF)); // gzip 头：magic, deflate, mtime=0, xfl=0, os=255
    gz.append(deflate);
    gz.append(char(crc & 0xFF)); gz.append(char((crc >> 8) & 0xFF));
    gz.append(char((crc >> 16) & 0xFF)); gz.append(char((crc >> 24) & 0xFF));
    const quint32 isize = quint32(nbt.size());
    gz.append(char(isize & 0xFF)); gz.append(char((isize >> 8) & 0xFF));
    gz.append(char((isize >> 16) & 0xFF)); gz.append(char((isize >> 24) & 0xFF));

    QDir().mkpath(gameDir);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(gz);
        f.close();
    }
}

void Launcher::start(const QString& versionId, const QString& javaPath, int maxMemoryMB,
                     const QString& jvmArgs, const QString& gameArgs, bool highPerfGpu,
                     const QString& resolvedJsonPath, const QString& resolvedJarPath)
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

    // ── 全量 JVM 输出日志：每次启动覆盖，stdout+stderr 全量落盘 ──
    // 崩溃分析导出时同时产出两份：jvm-output.txt（全量）+ jvm-output-recent.txt（截取）
    m_jvmFullLog.close();
    m_jvmFullLogPath = m_gameDir + QStringLiteral("/logs/shadow-jvm-output.log");
    QDir().mkpath(m_gameDir + QStringLiteral("/logs"));
    m_jvmFullLog.setFileName(m_jvmFullLogPath);
    if (!m_jvmFullLog.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qCWarning(logApp) << "[启动] 无法打开全量 JVM 输出日志:" << m_jvmFullLogPath;
        m_jvmFullLogPath.clear();
    }

    // Detect Java major version from the executable (used by buildArgs for --add-opens)
    // ── 32 位判定 + 系统内存（JVM 补全参数用，2026-08-08）──
    m_is32BitJvm = false;
    {
        QProcess javap;
        javap.start(javaPath, {QStringLiteral("-version")});
        javap.waitForFinished(5000);
        QString output = QString::fromLocal8Bit(javap.readAllStandardError());
        if (output.isEmpty()) output = QString::fromLocal8Bit(javap.readAllStandardOutput());
        QRegularExpression jre(QStringLiteral("version\\s+\"([^\"]+)\""));
        auto m = jre.match(output);
        if (m.hasMatch()) {
            QString ver = m.captured(1);
            ver.replace(QLatin1Char('_'), QLatin1Char('.'));
            QStringList parts = ver.split(QLatin1Char('.'));
            if (!parts.isEmpty()) {
                // Java 8 reports as "1.8.0_xxx" → major=8; Java 9+ reports as "9", "17", "25"
                int v = parts[0].toInt();
                if (v == 1 && parts.size() >= 2) v = parts[1].toInt();
                m_javaMajorVersion = v;
            }
        }
        // 32 位 JVM：Java 输出 "32-Bit"（Java 8）或 "32-Bit Server VM"
        if (output.contains(QStringLiteral("32-Bit"))) {
            m_is32BitJvm = true;
        }
        // 系统物理内存（JIT 优化组阈值：>4GB 才启用）
#ifdef Q_OS_WIN
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) {
            m_totalSystemMemoryMB = static_cast<qint64>(ms.ullTotalPhys / (1024 * 1024));
        }
#endif
        qCInfo(logLaunch) << QStringLiteral("[启动] Java 主版本=%1 32位=%2 系统内存=%3MB")
            .arg(m_javaMajorVersion).arg(m_is32BitJvm).arg(m_totalSystemMemoryMB);
    }

    // --- Load version JSON (支持灵活路径) ---
    QString jsonPath = resolvedJsonPath;
    if (jsonPath.isEmpty()) {
        jsonPath = m_gameDir + QStringLiteral("/versions/") + versionId
                   + QStringLiteral("/") + versionId + QStringLiteral(".json");
    }
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
    // Mode 0: off; Mode 1: system locale (default); Mode 2: IP region
    if (m_autoLangMode == 1 || m_autoLangMode == 2) {
        ensureOptionsTxt();
    }

    // --- Build arguments ---
    QStringList args = buildArgs(versionId, maxMemoryMB, versionJson);

    emit launchProgress(tr("启动 Minecraft %1...").arg(versionId));

    m_process->setWorkingDirectory(m_versionGameDir);

    // ── 进程环境：统一增量构建，启动前一次性应用 ──
    // ⚠ 严禁各块各自 setProcessEnvironment(systemEnvironment() + 自己的修改)：
    // 后执行的块会把前一块的修改整个覆盖。实测 2026-08-19：pre-1.6 的 APPDATA
    // 覆盖被“高性能 GPU”块（systemEnvironment()+SHIM_MCCOMPAT）冲掉 → 游戏
    // getAppDir 落到真实 %APPDATA%\.minecraft → 1.3.2 资源扫错目录 → 无声，
    // 且世界/存档写到真实 AppData 污染用户数据。
    // 必须基于完整父进程环境做增量修改：QProcess::processEnvironment() 在从未
    // setProcessEnvironment 时返回空对象，直接用它会导致子进程环境被清空
    // （PATH/APPDATA/USERPROFILE 全部丢失 → 依赖 %APPDATA% 的 mod 如
    //  ModernFix readGlobalProperties 会因 getenv("APPDATA")=null 而 NPE 崩溃）
    QProcessEnvironment procEnv = QProcessEnvironment::systemEnvironment();
    bool procEnvModified = false;

    // Pre-1.6: override APPDATA env var so getAppDir("minecraft") returns our game dir.
    // Old Minecraft (net.minecraft.client.Minecraft) never reads --gameDir;
    // its getAppDir("minecraft") constructs the path as %APPDATA%\.minecraft
    // （对齐 HMCL DefaultLauncher：APPDATA = 游戏 .minecraft 的父目录）。
    // Shared mode:  m_versionGameDir ends in ".minecraft" → set APPDATA to its parent.
    // Isolated mode: m_versionGameDir = versions/{id}/game →
    //   create a junction .minecraft→game inside the version dir,
    //   then set APPDATA to the version dir so getAppDir lands on the junction.
    {
        static const QRegularExpression pre16Rgx(QStringLiteral(R"(^1\.(\d+))"));
        auto pre16M = pre16Rgx.match(m_currentVersionId);
        bool isPre16 = pre16M.hasMatch() && pre16M.captured(1).toInt() < 6;
        if (isPre16) {
            QString versionGameDir = QDir::toNativeSeparators(
                QDir(m_versionGameDir).absolutePath());

            if (versionGameDir.endsWith(QStringLiteral("\.minecraft"))) {
                // Shared mode: set APPDATA to parent so getAppDir → m_versionGameDir
                QDir parentDir(m_versionGameDir);
                parentDir.cdUp();
                procEnv.insert(QStringLiteral("APPDATA"),
                               QDir::toNativeSeparators(parentDir.absolutePath()));
            } else {
                // Isolated mode: version game dir is either versions/{id}/game
                // (standard) or versions/{id} (scattered layout, no game/ subdir —
                // most installs, 2026-08-07 修正：getVersionGameDir 返回实际内容位置)
                // getAppDir("minecraft") always appends ".minecraft":
                //   - game/ layout:  junction at versions/{id}/.minecraft → game
                //   - scattered:     junction at versions/.minecraft → versions/{id}
                const QString vgd = QDir(m_versionGameDir).absolutePath();
                const bool isGameSubdir = vgd.endsWith(QStringLiteral("/game"))
                                        || vgd.endsWith(QStringLiteral("\\game"));
                QString versionDir;
                QString junction;
                if (isGameSubdir) {
                    QDir gameDir(m_versionGameDir);
                    gameDir.cdUp();  // now at versions/{id}/
                    versionDir = QDir::toNativeSeparators(gameDir.absolutePath());
                    junction = versionDir + QDir::separator() + QStringLiteral(".minecraft");
                } else {
                    // Scattered layout: game dir IS versions/{id}
                    QDir vdir(m_versionGameDir);
                    vdir.cdUp();  // now at versions/
                    versionDir = QDir::toNativeSeparators(vdir.absolutePath());
                    junction = versionDir + QDir::separator() + QStringLiteral(".minecraft");
                }
                if (!QFileInfo::exists(junction)) {
                    QProcess mklink;
                    mklink.start(QStringLiteral("cmd"),
                                 {QStringLiteral("/c"), QStringLiteral("mklink"),
                                  QStringLiteral("/J"),
                                  QDir::toNativeSeparators(junction),
                                  QDir::toNativeSeparators(m_versionGameDir)});
                    mklink.waitForFinished(5000);
                    if (QFileInfo::exists(junction))
                        qCInfo(logLaunch) << QStringLiteral("[启动] pre-1.6 junction 已创建: %1 → %2")
                            .arg(junction).arg(QDir::toNativeSeparators(m_versionGameDir));
                    else
                        qCWarning(logLaunch) << QStringLiteral("[启动] pre-1.6 junction 创建失败: %1")
                            .arg(junction);
                }
                procEnv.insert(QStringLiteral("APPDATA"), versionDir);
            }
            // 2026-08-19：pre-1.6 启动前确保 servers.dat 存在（游戏读 appDir/servers.dat，
            // 缺失即 NPE 崩溃——1.3.2 等版本 ServerList 无 null 检查）。
            ensureServersDat(m_versionGameDir);
            qCInfo(logLaunch) << QStringLiteral("[启动] pre-1.6 APPDATA 覆盖: %1")
                .arg(procEnv.value(QStringLiteral("APPDATA")));
            procEnvModified = true;
        }
    }

    // High-performance GPU: set env vars for NVIDIA Optimus / AMD Switchable Graphics
    if (m_highPerfGpu) {
        // 只往统一 procEnv 里增量插入，绝不再 setProcessEnvironment——否则会覆盖
        // pre-1.6 的 APPDATA 覆盖（2026-08-19 实测导致游戏目录错位、无声）。
        procEnv.insert(QStringLiteral("SHIM_MCCOMPAT"), QStringLiteral("0x800000001"));
        procEnvModified = true;

        // ── 注册表 GPU 偏好（对齐主流启动器实现 SetGPUPreference）：HKCU\Software\Microsoft\DirectX\UserGpuPreferences
        //    GpuPreference=2 让 NVIDIA/AMD 驱动优先使用独显（对 Optimus 双显卡更通用）
#ifdef Q_OS_WIN
        {
            const QString regPath = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\DirectX\\UserGpuPreferences");
            QSettings gpuPref(regPath, QSettings::NativeFormat);
            gpuPref.setValue(javaPath, QStringLiteral("GpuPreference=2;"));
            gpuPref.sync();
            qCInfo(logLaunch) << QStringLiteral("[启动] 已写入 GPU 高性能注册表: %1").arg(javaPath);
        }
#endif
    }

    // 统一应用进程环境（若有修改）
    if (procEnvModified)
        m_process->setProcessEnvironment(procEnv);

    qCInfo(logLaunch) << QStringLiteral("[启动] 启动参数: %1").arg(sanitizeLaunchLog(args.join(QLatin1Char(' '))));
    qCInfo(logLaunch) << QStringLiteral("[启动] 启动参数共 %1 个").arg(args.size());

    // 启动前自定义命令（异步，不阻塞）——主流启动器 preLaunchCommand 对齐
    runPreLaunchCommand();

    m_process->start(javaPath, args);
}

void Launcher::cancel()
{
    if (!isRunning()) return;

    qCInfo(logLaunch) << QStringLiteral("[启动] 停止游戏 版本=%1 PID=%2").arg(m_currentVersionId).arg(m_process ? m_process->processId() : -1);
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

/// 启动器退出时调用：把运行中的游戏进程与启动器解耦。
/// 游戏是独立 java 进程，关闭启动器不应连带关闭它。
void Launcher::detach()
{
    if (m_detached) return;
    m_detached = true;
    qCInfo(logLaunch) << QStringLiteral("[启动] 分离游戏进程 PID=%1（启动器退出后游戏继续运行）")
                             .arg(m_pid);

    // 1) 断开信号：分离后游戏输出/退出不再回调启动器（避免访问即将销毁的对象）
    if (m_process)
        disconnect(m_process, nullptr, this, nullptr);
    disconnect(this, nullptr, nullptr, nullptr);

    // 2) 停止窗口标题覆盖轮询
    if (m_titleTimer) {
        m_titleTimer->stop();
        m_titleTimer = nullptr;
    }

    // 3) 关闭管道：stdin 写端 + stdout/stderr 读端。游戏继续运行时写端
    //    失效即静默失败，不会因管道缓冲存满而阻塞 java。
    if (m_process) {
        m_process->closeWriteChannel();
        m_process->closeReadChannel(QProcess::StandardOutput);
        m_process->closeReadChannel(QProcess::StandardError);
    }
    if (m_jvmFullLog.isOpen()) {
        m_jvmFullLog.close();
        m_jvmFullLogPath.clear();
    }

    // 4) 关键：QProcess 的析构函数会对"仍在运行"的子进程调用 kill() +
    //    waitForFinished()（Qt 文档明确）。所以绝不能让它随 Launcher 析构被 delete——
    //    把它摘出对象树并放弃持有（对象泄漏，随本进程退出由 OS 回收），
    //    游戏进程才能真正独立存活。
    if (m_process) {
        m_process->setParent(nullptr);
        m_process = nullptr;
    }
}

/// 接管一个已脱离启动器的游戏进程（detach 后下次启动恢复强制结束）。
/// 此时没有 QProcess 对象——仅凭 PID 用 forceKill（纯 Win32 TerminateProcess）
/// 即可结束进程树；m_detached=true 保证该 Launcher 若被析构不会误杀（正常应走 killGameByPid）。
void Launcher::adoptDetachedProcess(qint64 pid)
{
    m_pid = pid;
    m_detached = true;
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
    qCInfo(logLaunch) << QStringLiteral("[启动] 游戏启动完成 版本=%1").arg(m_currentVersionId);

    // ── 进程优先级（主流启动器 LaunchArgumentPriority：0=高 1=中 2=低）──
#ifdef Q_OS_WIN
    if (m_processPriority != 1) {
        HANDLE hProc = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(m_pid));
        if (hProc) {
            DWORD prio = (m_processPriority == 2) ? BELOW_NORMAL_PRIORITY_CLASS
                       : (m_processPriority == 0) ? ABOVE_NORMAL_PRIORITY_CLASS
                       : NORMAL_PRIORITY_CLASS;
            if (SetPriorityClass(hProc, prio)) {
                qCInfo(logLaunch) << QStringLiteral("[启动] 已设置进程优先级=%1 (PID=%2)")
                    .arg(m_processPriority == 0 ? QStringLiteral("高") : QStringLiteral("低")).arg(m_pid);
            } else {
                qCWarning(logLaunch) << QStringLiteral("[启动] 设置进程优先级失败 错误=%1").arg(GetLastError());
            }
            CloseHandle(hProc);
        } else {
            qCWarning(logLaunch) << QStringLiteral("[启动] 打开进程失败，无法设置优先级 PID=%1").arg(m_pid);
        }
    }
#endif

    // 窗口标题覆盖（尽力而为，异步轮询 FindWindow）
    applyWindowTitleOverride();
}

void Launcher::onReadyReadStdout()
{
    // JVM 标准输出写入启动器日志（2026-08-07 重新开启：诊断辅助，曾按用户要求关闭）。
    // 逐行 qCInfo（日志文件按天轮转），同时保留：排空管道、crash ring buffer、
    // 全量 shadow-jvm-output.log、launchProgress 进度提示。
    QByteArray data = m_process->readAllStandardOutput();
    QString text = QString::fromUtf8(data).trimmed();
    if (text.isEmpty())
        return;

    // ── 启动器日志：JVM 标准输出（逐行，避免超长行撑爆单条日志）──
    {
        const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
        for (const QString& l : lines) {
            const QString t = l.trimmed();
            if (t.isEmpty()) continue;
            qCInfo(logLaunch) << QStringLiteral("[JVM 输出] ") + sanitizeLaunchLog(t);
        }
    }

    // ── Crash analysis ring buffer: keep raw lines (unfiltered) ──
    {
        const QStringList rawLines = QString::fromUtf8(data).split(QLatin1Char('\n'));
        for (const QString& raw : rawLines) {
            const QString t = raw.trimmed();
            if (t.isEmpty()) continue;
            m_outputRing.append(t);
            if (m_outputRing.size() > 600)
                m_outputRing.removeFirst();
        }
    }

    // ── Full JVM output log (all lines, for crash-analysis export) ──
    if (m_jvmFullLog.isOpen())
        m_jvmFullLog.write(data);

    // Filter: discard routine MC INFO/Trace/DEBUG output, keep errors/crashes
    // Process line-by-line so a mixed chunk (INFO + ERROR) keeps the ERROR part
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || isMcOutputNoise(trimmed))
            continue;
        emit launchProgress(trimmed);
    }
}

void Launcher::onReadyReadStderr()
{
    // JVM 错误输出写入启动器日志（2026-08-07 重新开启：诊断辅助，曾按用户要求关闭）
    QByteArray data = m_process->readAllStandardError();
    QString text = QString::fromUtf8(data).trimmed();
    if (!text.isEmpty()) {
        // ── 启动器日志：JVM 错误输出（逐行）──
        {
            const QStringList errLines = QString::fromUtf8(data).split(QLatin1Char('\n'));
            for (const QString& l : errLines) {
                const QString t = l.trimmed();
                if (t.isEmpty()) continue;
                qCInfo(logLaunch) << QStringLiteral("[JVM 错误输出] ") + sanitizeLaunchLog(t);
            }
        }

        // ── Crash analysis ring buffer ──
        const QStringList rawLines = QString::fromUtf8(data).split(QLatin1Char('\n'));
        for (const QString& raw : rawLines) {
            const QString t = raw.trimmed();
            if (t.isEmpty()) continue;
            m_outputRing.append(t);
            if (m_outputRing.size() > 600)
                m_outputRing.removeFirst();
        }

        emit launchProgress(text);
    }

    // ── Full JVM output log (stderr too) ──
    if (m_jvmFullLog.isOpen())
        m_jvmFullLog.write(data);
}

QStringList Launcher::recentOutput(int maxLines) const
{
    if (maxLines <= 0 || m_outputRing.size() <= maxLines)
        return m_outputRing;
    return m_outputRing.mid(m_outputRing.size() - maxLines);
}

// ── MC output noise filter: drop routine INFO/Trace/DEBUG log lines ──
// Returns true if the line is ordinary MC runtime noise (render, recipes, resources, etc.)
bool Launcher::isMcOutputNoise(const QString& line) const
{
    // MC log4j format: [thread/LEVEL] or [LEVEL]
    // Drop /INFO], /TRACE], /DEBUG] — keep /WARN], /ERROR], /FATAL]
    // Also check for bare INFO] (some mods use non-standard format)
    if (line.contains(QLatin1String("/INFO]")) ||
        line.contains(QLatin1String("/TRACE]")) ||
        line.contains(QLatin1String("/DEBUG]")) ||
        line.startsWith(QLatin1String("[INFO]")))
        return true;

    // Common MC mod loader noise patterns (Forge/NeoForge/Fabric)
    if (line.contains(QLatin1String("Loading ")) ||
        line.contains(QLatin1String("Loaded ")) ||
        line.contains(QLatin1String("Registering ")) ||
        line.contains(QLatin1String("Starting ")))
        return true;

    // Sound engine startup (repeated every re-connect)
    if (line.contains(QLatin1String("Sound engine started")))
        return true;

    // Resource manager reload
    if (line.contains(QLatin1String("Reloading ResourceManager")) ||
        line.contains(QLatin1String("ResourceManager reload")))
        return true;

    // Recipe loading (MC dumps all recipes on startup)
    if (line.contains(QLatin1String(" recipe")) &&
        (line.contains(QLatin1String("load")) || line.contains(QLatin1String("Loaded"))))
        return true;

    return false;
}

void Launcher::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    // 停止窗口标题持续覆盖
    if (m_titleTimer)
        m_titleTimer->stop();

    // flush 全量 JVM 输出，确保崩溃分析/导出时文件完整
    if (m_jvmFullLog.isOpen())
        m_jvmFullLog.flush();

    // 游戏退出后自定义命令（异步）——主流启动器 postExitCommand 对齐
    runPostExitCommand();

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
// 2026-08-19：支持 HMCL 式 4 段 classifier 名（downloads 为空的条目走此回退）。
//   "net.minecraftforge:forge:26.2-65.1.1:client" → forge-26.2-65.1.1-client.jar。
//   3 段名行为与原实现完全一致，不引入回归。
static QString mavenNameToPath(const QString& mavenName)
{
    const QStringList parts = mavenName.split(QLatin1Char(':'));
    if (parts.size() < 3) return {};
    QString group = parts[0];
    QString artifact = parts[1];
    QString version = parts[2];
    QString groupPath = group.replace(QLatin1Char('.'), QLatin1Char('/'));
    QString result = groupPath + QLatin1Char('/') + artifact + QLatin1Char('/')
                     + version + QLatin1Char('/') + artifact + QLatin1Char('-') + version;
    if (parts.size() >= 4)
        result += QLatin1Char('-') + parts[3];
    return result + QStringLiteral(".jar");
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

// ── log4j-slf4j 绑定去重辅助（2026-08-14 NeoForge 1.20.6 修复）──
// 判断 maven 名是否为 slf4j 绑定（log4j-slf4j-impl / log4j-slf4j2-impl / log4j-slf4j18-impl）
static bool isSlf4jBinding(const QString& mavenName)
{
    return mavenName.contains(QStringLiteral("log4j-slf4j"));
}

// 提取 group:artifact 键（忽略版本与 @jar 后缀）→ 同一 artifact 去重用
static QString slf4jArtifactKey(const QString& mavenName)
{
    // "org.apache.logging.log4j:log4j-slf4j2-impl:2.19.0@jar" → "log4j-slf4j2-impl"
    QStringList parts = mavenName.split(QLatin1Char(':'));
    return parts.size() >= 2 ? parts[1] : mavenName;
}

// 从 maven 名提取版本号（容忍 @jar 后缀）
static QVersionNumber extractVersion(const QString& mavenName)
{
    QStringList parts = mavenName.split(QLatin1Char(':'));
    if (parts.size() < 3) return {};
    QString ver = parts[2];
    int atIdx = ver.indexOf(QLatin1Char('@'));
    if (atIdx >= 0) ver = ver.left(atIdx);
    return QVersionNumber::fromString(ver);
}

// 从已解析的库路径提取版本号（路径 .../artifact/ver/artifact-ver.jar）
static QVersionNumber extractVersionFromPath(const QString& libPath)
{
    // 取倒数第二段（版本目录）
    QStringList parts = libPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() >= 2)
        return QVersionNumber::fromString(parts[parts.size() - 2]);
    return {};
}

// 按 maven name 解析库路径（用于移除已入 cp 的旧 slf4j 绑定路径）
static QString resolveLibraryPathForName(const QString& mavenName, const QString& librariesDir)
{
    QString name = mavenName;
    int atIdx = name.indexOf(QLatin1Char('@'));
    if (atIdx >= 0) name = name.left(atIdx);  // 去掉 @jar 后缀
    QStringList parts = name.split(QLatin1Char(':'));
    if (parts.size() < 3) return {};
    QString groupPath = parts[0].replace(QLatin1Char('.'), QLatin1Char('/'));
    return librariesDir + QLatin1Char('/') + groupPath + QLatin1Char('/') + parts[1]
           + QLatin1Char('/') + parts[2] + QLatin1Char('/') + parts[1]
           + QLatin1Char('-') + parts[2] + QStringLiteral(".jar");
}

static QStringList buildClasspath(const QString& versionId, const QJsonObject& versionJson,
                                   const QString& gameDir,
                                   const QSet<QString>& moduleExcludePaths = {},
                                   bool useShortPaths = true)
{
    QStringList cp;
    // 短路径策略：仅 legacy（Java ≤ 8 且无 Jar-in-Jar）用 8.3 短路径；
    // 现代版本用完整路径（避免 8.3 短名里的 '~' 破坏 Forge 的 jij: URI）。
    QString libsDir = (useShortPaths ? toShortPath(gameDir) : gameDir)
                      + QStringLiteral("/libraries");

    // Precompute version jar path (added LAST — see end of function)
    QString versionJar = gameDir + QStringLiteral("/versions/") + versionId
                         + QStringLiteral("/") + versionId + QStringLiteral(".jar");

    // ── slf4j 绑定去重表（artifactKey → 保留的 maven name，版本比较用）──
    QHash<QString, QString> slf4jBestName;

    // Collect libraries from version JSON + all inheritsFrom parents
    QSet<QString> seenLibs;  // deduplicate by resolved path
    QStringList versionStack;  // JSON chain, youngest first
    QJsonObject currentJson = versionJson;
    QString currentId = versionId;

    // Helper: find actual version JSON path given an ID (may differ from directory name)
    auto resolveJsonPath = [&](const QString& id) -> QString {
        QString path = gameDir + QStringLiteral("/versions/") + id
                     + QStringLiteral("/") + id + QStringLiteral(".json");
        if (QFileInfo::exists(path))
            return path;
        // Fallback: scan all version directories for a JSON whose "id" matches
        QDir versionsDir(gameDir + QStringLiteral("/versions"));
        const QStringList dirs = versionsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& dir : dirs) {
            QDir vdir(versionsDir.absoluteFilePath(dir));
            const QStringList jsonFiles = vdir.entryList({QStringLiteral("*.json")}, QDir::Files);
            for (const QString& jsonFile : jsonFiles) {
                QString fp = vdir.absoluteFilePath(jsonFile);
                QFile f(fp);
                if (!f.open(QIODevice::ReadOnly)) continue;
                QJsonParseError pe;
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
                f.close();
                if (pe.error == QJsonParseError::NoError && doc.isObject()) {
                    if (doc.object()[QStringLiteral("id")].toString() == id)
                        return fp;
                }
            }
        }
        return QString();
    };

    while (true) {
        // Add libraries from current JSON
        QJsonArray libraries = currentJson[QStringLiteral("libraries")].toArray();
        for (const QJsonValue& libVal : libraries) {
            QJsonObject lib = libVal.toObject();
            if (!shouldIncludeLibrary(lib)) continue;

            QString libPath = resolveLibraryPath(lib, libsDir);
            if (!libPath.isEmpty() && QFileInfo::exists(libPath)) {
                // Skip JARs whose group:artifact is on the module path (NeoForge -p flag)
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

                // ── log4j-slf4j 绑定去重（2026-08-14 NeoForge 1.20.6 启动崩溃修复）──
                // 背景：NeoForge 20.6.139 的 version JSON 同时列出
                //   log4j-slf4j2-impl:2.19.0@jar（NeoForge 条目，Automatic-Module-Name
                //   错误地为 org.apache.logging.log4j.slf4j）与
                //   log4j-slf4j2-impl:2.22.1（MC 父 JSON 条目，module-info 正确为
                //   org.apache.logging.log4j.slf4j2.impl）。
                // 启动参数 --add-modules ALL-MODULE-PATH 会同时加载两个模块，
                // 两者都导出 org.apache.logging.slf4j 包 → ResolutionException：
                //   "Modules ...slf4j2.impl and ...slf4j export package org.apache.logging.slf4j"
                // 处理：同一 artifact 的多个 slf4j 绑定只保留最高版本（module-info
                // 正确的现代版本）。slf4j 绑定先记入 map 不直接入 cp，循环结束后
                // 统一把保留版本加入 cp —— 避免中途移除短路径路径的匹配问题。
                const QString libName = lib[QStringLiteral("name")].toString();
                if (isSlf4jBinding(libName)) {
                    QString artifactKey = slf4jArtifactKey(libName);
                    auto it = slf4jBestName.find(artifactKey);
                    if (it == slf4jBestName.end()) {
                        slf4jBestName.insert(artifactKey, libName);
                    } else {
                        QVersionNumber cand = extractVersion(libName);
                        QVersionNumber cur = extractVersion(it.value());
                        if (cand > cur) {
                            qDebug() << "[Launch] slf4j binding dedup: 升级到" << libName;
                            slf4jBestName.insert(artifactKey, libName);
                        } else {
                            qDebug() << "[Launch] slf4j binding dedup: 保留" << it.value()
                                     << "排除" << libName;
                        }
                    }
                    continue;  // 不入 cp，循环后统一加入保留版本
                }

                if (!seenLibs.contains(libPath)) {
                    seenLibs.insert(libPath);
                    cp.append(libPath);
                }
            }
        }

        // Also add the parent version's JAR to classpath (Forge needs MC classes)
        QString parentId = currentJson[QStringLiteral("inheritsFrom")].toString();
        if (parentId.isEmpty() || parentId == currentId) break;
        if (versionStack.contains(parentId)) break;
        versionStack.append(parentId);

        QString parentJsonPath = resolveJsonPath(parentId);
        if (parentJsonPath.isEmpty()) break;

        // Add parent version JAR to classpath
        QFileInfo pji(parentJsonPath);
        QString parentJar = pji.absolutePath() + QStringLiteral("/") + pji.completeBaseName() + QStringLiteral(".jar");
        if (QFileInfo::exists(parentJar) && !seenLibs.contains(parentJar)) {
            seenLibs.insert(parentJar);
            cp.append(parentJar);
            qCInfo(logLaunch) << QStringLiteral("[启动] 添加父版本 JAR: %1").arg(parentJar);
        }

        QFile pf(parentJsonPath);
        if (!pf.open(QIODevice::ReadOnly)) break;
        QJsonParseError parseErr;
        QJsonDocument parentDoc = QJsonDocument::fromJson(pf.readAll(), &parseErr);
        pf.close();
        if (parseErr.error != QJsonParseError::NoError) break;
        currentJson = parentDoc.object();
        currentId = parentId;
    }

    // 循环结束后统一加入保留的 slf4j 绑定（去重后的最高版本）
    for (auto it = slf4jBestName.constBegin(); it != slf4jBestName.constEnd(); ++it) {
        QString keptPath = resolveLibraryPathForName(it.value(), libsDir);
        if (!keptPath.isEmpty() && QFileInfo::exists(keptPath) && !seenLibs.contains(keptPath)) {
            seenLibs.insert(keptPath);
            cp.append(keptPath);
            qDebug() << "[Launch] slf4j binding kept:" << it.value() << keptPath;
        }
    }

    // Add version's own jar LAST (after all libraries), so that library JARs
    // (like optifine:OptiFine which contains class-file-level patches) take
    // precedence over the version jar's vanilla classes in the classpath.
    //
    // For standalone versions (regular MC / flattened OptiFine):
    //   libraries + patched libs → version jar (vanilla copy)
    //   The JVM loads each class from the first JAR in classpath order;
    //   OptiFine's library must come first to provide GLX.isUsingFBOs().
    //
    // For inheriting versions (Forge, NeoForge, inheriting OptiFine):
    //   The inherited parent jar is already added during the inheritsFrom walk
    //   above. Adding the version's own jar here is harmless (it won't shadow
    //   library patches since libraries were added before).
    if (!cp.contains(versionJar)) {
        if (QFileInfo::exists(versionJar)) {
            cp.append(versionJar);
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

    // ── Temp dir: old MC versions (ImageIO) try to write to C:\WINDOWS on Win10/11 ──
    // Without this, javax.imageio.ImageIO fails with AccessDeniedException.
    args << "-Djava.io.tmpdir=" + QDir::toNativeSeparators(m_gameDir);

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
            QString parentPath = m_gameDir + QStringLiteral("/versions/") + parentId;
            QString parentJson = findVersionJson(parentPath, parentId);
            if (parentJson.isEmpty()) {
                parentJson = m_gameDir + QStringLiteral("/versions/") + parentId
                           + QStringLiteral("/") + parentId + QStringLiteral(".json");
                if (!QFileInfo::exists(parentJson)) break;
            }
            QFile pf(parentJson);
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
    // ── 短路径策略（2026-08-29 修正）──
    // 8.3 短路径段必然含 '~'，而 Forge 1.19.3+/NeoForge 的 Jar-in-Jar 用 'jij:' URI
    // （分隔符就是 '~'，见 JarJarFileSystems 的 URI_SPLIT_REGEX）。--gameDir/-cp 一旦
    // 是短路径，mods 扫描出来的 jij: URI 就会被 '~' 拆坏 → 内嵌库（如 MixinExtras）
    // 加载失败 → 崩（实锤：落幕曲 ending_library StyleMixin @Local 糖报
    // "Invalid descriptor ... LocalBooleanRef"）。短路径只对 Java8/LWJGL2 世代
    // （≤1.12.2）的非 ASCII native 加载有价值，现代版本一律用完整路径。
    const bool usesJarInJar = versionUsesJarInJar(versionId, versionJson, m_gameDir);
    const bool useShortPaths = (m_javaMajorVersion <= 8) && !usesJarInJar;
    if (usesJarInJar && QDir::toNativeSeparators(m_versionGameDir).contains(QLatin1Char('~'))) {
        qCWarning(logLaunch) << QStringLiteral(
            "[启动] 警告：Jar-in-Jar 版本运行目录含 '~'（%1），Forge 的 jij: URI 会被拆坏，"
            "游戏可能崩溃。请把游戏目录换成不含 '~' 的纯英文路径。")
            .arg(QDir::toNativeSeparators(m_versionGameDir));
    }
    const QString gameDirForLaunch = useShortPaths ? toShortPath(m_gameDir) : m_gameDir;
    const QString libDir = gameDirForLaunch + QStringLiteral("/libraries");
    const QString nativesDir = gameDirForLaunch + QStringLiteral("/versions/") + versionId
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
        // Fabric 26.x：jvm 参数 "-DFabricMcEmu= net.minecraft.client.main.Main " 等号后带空格，
        // 透传到命令行会被拆成两个参数（-DFabricMcEmu= 空值 + mainClass 提前 → JVM 把
        // mainClass 当主类、-cp 失效 → ClassNotFoundException）。对齐主流启动器实现 ModLaunch
        // .Replace("McEmu= ", "McEmu=")：去掉等号后的空格，属性值 = 游戏主类
        // （KnotClient 从 -cp 加载游戏类，2026-08-10 实锤 26.1-fabric 导出脚本启动失败）
        arg.replace(QStringLiteral("-DFabricMcEmu= "), QStringLiteral("-DFabricMcEmu="));
        // Java 8 doesn't support --add-exports / --add-opens → filter them out
        if (m_javaMajorVersion < 9 && (arg.startsWith(QStringLiteral("--add-exports")) ||
                                        arg.startsWith(QStringLiteral("--add-opens")))) {
            continue;
        }
        args << arg;
    }

    // Always apply default optimized flags (non-GC)
    for (const char* arg : DEFAULT_JVM_ARGS) {
        args << QString::fromLatin1(arg);
    }

    // ── 智能 GC 策略（仅当 chain JVM args 与自定义 JVM args 均未指定 GC 时）──
    // 如果版本 JSON 已指定 GC，尊重其选择；否则自动选择最优 GC
    // 2026-08-20 修复（内测 26.2 实锤 "Multiple garbage collectors selected"）：
    // 用户自定义 JVM 参数（m_jvmArgs）若已含 GC 选择（如 -XX:+UseZGC），自动注入的
    // G1 组会与之共存 → JVM 初始化失败直接退出（退出码 1）。与 chainHasGc 同规则检测。
    bool chainHasGc = false;
    auto detectGcFlag = [](const QString& a) -> bool {
        if (a.startsWith(QStringLiteral("-XX:+Use")) || a.startsWith(QStringLiteral("-XX:-Use"))) {
            return a.contains(QStringLiteral("GC")) || a.contains(QStringLiteral("gc"));
        }
        return false;
    };
    for (const QString& a : chainJvmArgs) {
        if (detectGcFlag(a)) {
            chainHasGc = true;
            break;
        }
    }
    if (!chainHasGc && !m_jvmArgs.isEmpty()) {
        for (const QString& a : tokenizeJvmArgs(m_jvmArgs)) {
            if (detectGcFlag(a)) {
                chainHasGc = true;
                qCInfo(logLaunch) << QStringLiteral("[启动] 自定义 JVM 参数已指定 GC (%1)，跳过自动选择").arg(a);
                break;
            }
        }
    }
    if (!chainHasGc) {
        const bool debugMode = false;  // Release mode: 关闭 PerfDisableSharedMem
        for (const QString& gcArg : collectGcArgs(m_javaMajorVersion, debugMode, m_gcMode)) {
            args << gcArg;
        }
    } else {
        qCInfo(logLaunch) << QStringLiteral("[启动] 版本 JSON 或自定义参数已指定 GC 策略，跳过自动选择");
    }

    // ── JVM 版本补全参数（对齐主流启动器实现 DefaultLauncher）──
    auto hasArgPrefix = [&](const QString& prefix) -> bool {
        for (const QString& a : args) {
            if (a.startsWith(prefix)) return true;
        }
        return false;
    };
    // Java 24/25: --sun-misc-unsafe-memory-access=allow（26.2 用 Java 25 需要）
    if (m_javaMajorVersion == 24 || m_javaMajorVersion == 25) {
        if (!hasArgPrefix(QStringLiteral("--sun-misc-unsafe-memory-access="))) {
            args << QStringLiteral("--sun-misc-unsafe-memory-access=allow");
        }
    }
    // Java 16: --illegal-access=permit（同主流启动器）
    if (m_javaMajorVersion == 16) {
        if (!hasArgPrefix(QStringLiteral("--illegal-access="))) {
            args << QStringLiteral("--illegal-access=permit");
        }
    }
    // 32 位 JVM：-Xss 1m（主流启动器：32 位默认 320KB 栈导致 1.13 崩 StackOverflowError）
    if (m_is32BitJvm) {
        args << QStringLiteral("-Xss1m");
    }
    // JIT 优化组（主流启动器：64 位 + 内存 >4GB 时启用）
    if (!m_is32BitJvm && m_totalSystemMemoryMB > 4096) {
        if (!hasArgPrefix(QStringLiteral("-XX:ReservedCodeCacheSize="))) {
            args << QStringLiteral("-XX:ReservedCodeCacheSize=400M");
        }
        if (!hasArgPrefix(QStringLiteral("-XX:MaxNodeLimit="))) {
            args << QStringLiteral("-XX:MaxNodeLimit=240000");
        }
        if (!hasArgPrefix(QStringLiteral("-XX:NodeLimitFudgeFactor="))) {
            args << QStringLiteral("-XX:NodeLimitFudgeFactor=8000");
        }
        if (!hasArgPrefix(QStringLiteral("-XX:TieredCompileTaskTimeout="))) {
            args << QStringLiteral("-XX:TieredCompileTaskTimeout=10000");
        }
        if (m_javaMajorVersion >= 8) {
            if (!hasArgPrefix(QStringLiteral("-XX:NmethodSweepActivity="))) {
                args << QStringLiteral("-XX:NmethodSweepActivity=1");
            }
        }
    }
    if (m_javaMajorVersion <= 8) {
        if (!hasArgPrefix(QStringLiteral("-XX:MaxInlineLevel="))) {
            args << QStringLiteral("-XX:MaxInlineLevel=15");
        }
    }

    // ── JVM encoding params (prevent CJK log garbling) ──
    // Check if chainJvmArgs already set these before adding
    if (m_javaMajorVersion > 8) {
        if (!hasArgPrefix(QStringLiteral("-Dstdout.encoding="))) {
            args << QStringLiteral("-Dstdout.encoding=UTF-8");
        }
        if (!hasArgPrefix(QStringLiteral("-Dstderr.encoding="))) {
            args << QStringLiteral("-Dstderr.encoding=UTF-8");
        }
    }
    if (m_javaMajorVersion >= 18) {
        if (!hasArgPrefix(QStringLiteral("-Dfile.encoding="))) {
            args << QStringLiteral("-Dfile.encoding=COMPAT");
        }
    }

    // Java 9+ needs --add-opens for reflective access (ModLauncher, Forge, LWJGL 2, etc.)
    // Java 8 doesn't support --add-opens (unrecognized option → crash)
    if (m_javaMajorVersion >= 9) {
        args << QStringLiteral("--add-opens") << QStringLiteral("java.base/java.lang=ALL-UNNAMED");
        args << QStringLiteral("--add-opens") << QStringLiteral("java.base/java.util=ALL-UNNAMED");
        args << QStringLiteral("--add-opens") << QStringLiteral("java.base/java.lang.reflect=ALL-UNNAMED");
        args << QStringLiteral("--add-opens") << QStringLiteral("java.base/java.text=ALL-UNNAMED");
        args << QStringLiteral("--add-opens") << QStringLiteral("java.desktop/java.awt=ALL-UNNAMED");
        args << QStringLiteral("--add-opens") << QStringLiteral("java.base/java.net=ALL-UNNAMED");
        args << QStringLiteral("--add-opens") << QStringLiteral("java.base/java.nio=ALL-UNNAMED");
    }

    // Append user-provided custom JVM args (space-separated, may override defaults)
    // ── 2026-08-17：引号感知拆分 ──
    // 原实现按 \s+ 拆分，含空格的路径（如 -javaagent:"D:\MC FAN\...\authlib-injector.jar"=url）
    // 会被劈裂成 -javaagent:"D:\MC + FAN\... → JVM 报 "Error opening zip file or JAR
    // manifest missing"。改为引号感知 tokenizer："" 内部按字面量保留（含空格），
    // 引号本身从参数中剥离（QProcess 直接收到正确参数，不再二次 shell 解析）。
    if (!m_jvmArgs.isEmpty()) {
        const QStringList customArgs = tokenizeJvmArgs(m_jvmArgs);
        for (const auto& arg : customArgs) {
            args << arg;
        }
    }

    // ── Natives path (only if version JSON doesn't already set it) ──
    // MC 26.2+ JSON has -Djava.library.path=${natives_directory}/java in arguments.jvm
    if (!hasArgPrefix(QStringLiteral("-Djava.library.path="))) {
        args << QStringLiteral("-Djava.library.path=%1").arg(nativesDir);
    }

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
    QStringList cp = buildClasspath(versionId, versionJson, m_gameDir, moduleExclude, useShortPaths);

    // ── Forge 1.16.x universal jar ──
    // 版本 JSON 的 libraries 只引用 installer stub (212KB)，不含 universal jar (2.5MB)
    // 必须手动添加到 classpath，否则 forge mod 不会加载（参考：主流启动器 做法）
    // 注意：libraries 目录可能有多个 forge 版本残留，必须精确匹配
    if (versionId.contains(QStringLiteral("forge"))) {
        // Extract forge version from versionId: "1.16.5-forge-36.2.42" → "1.16.5-36.2.42"
        int forgeIdx = versionId.indexOf(QStringLiteral("-forge-"));
        if (forgeIdx < 0) forgeIdx = versionId.indexOf(QStringLiteral("-forge"));
        if (forgeIdx >= 0) {
            QString mcPart = versionId.left(forgeIdx);  // "1.16.5"
            int dashAfterMc = versionId.indexOf(QLatin1Char('-'), forgeIdx + 6);
            QString fv = (dashAfterMc >= 0)
                       ? mcPart + versionId.mid(dashAfterMc)  // "1.16.5-36.2.42"
                       : QString();
            // Also try: scan for mcVersion-forgeVersion pattern directly
            if (fv.isEmpty() || mcPart.isEmpty()) {
                // Fallback: parse from the version JSON's --fml.forgeVersion
                // Already parsed in getMcVersion() but that's complex here.
                // Just scan all dirs and match mc prefix + forge suffix.
            }
            if (!fv.isEmpty()) {
                QString universal = m_gameDir + QStringLiteral("/libraries/net/minecraftforge/forge/")
                                  + fv + QStringLiteral("/forge-") + fv + QStringLiteral("-universal.jar");
                if (QFileInfo::exists(universal) && !cp.contains(universal)) {
                    cp.append(universal);
                }
            }
        }
    }

    // Build classpath string (only if version JSON doesn't already provide -cp)
    // MC 26.2+ version JSON includes "-cp ${classpath}" in arguments.jvm
    // Older versions rely on launcher to add -cp
    bool jsonHasCp = false;
    for (const QString& a : args) {
        if (a == QStringLiteral("-cp") || a == QStringLiteral("-classpath")) {
            jsonHasCp = true;
            break;
        }
    }

    QString cpJoined;
    if (!cp.isEmpty()) {
#ifdef Q_OS_WIN
        cpJoined = cp.join(QStringLiteral(";"));
#else
        cpJoined = cp.join(QStringLiteral(":"));
#endif
    }

    // Replace JVM template ${classpath} in already-added args
    // (version JSON's arguments.jvm contains -cp ${classpath})
    if (!cpJoined.isEmpty()) {
        bool replaced = false;
        for (auto it = args.begin(); it != args.end(); ++it) {
            if (*it == QStringLiteral("${classpath}")) {
                *it = cpJoined;
                replaced = true;
            }
        }

        if (!replaced && !jsonHasCp) {
            // Old version JSON: no -cp in JVM args → we add it
            args << QStringLiteral("-cp");
            args << cpJoined;
        }
    }

    // ── Main class ──
    QString mainClass = versionJson[QStringLiteral("mainClass")].toString();
    if (mainClass.isEmpty()) {
        mainClass = QStringLiteral("net.minecraft.client.main.Main");
    }
    // Pre-1.6 JSONs may have mainClass hijacked to launchwrapper.Launch by version APIs,
    // but the actual JAR doesn't contain launchwrapper — use Minecraft's original main class.
    // ⚠ 例外：Forge 老版本（1.4.7 等）的 mainClass 是 FMLRelauncher（FML 引导器，
    //   不是 hijack）——覆盖会丢 FML 初始化 → “找不到主类 net.minecraft.client.Minecraft”
    //   + versions 目录异常创建 .minecraft 快捷方式（MC 把版本目录当工作目录）。
    //   2026-08-07 实测 1.4.7+forge 启动崩溃，仅当 mainClass 非 FML/Forge 才覆盖。
    static const QRegularExpression pre16Ver(QStringLiteral(R"(^1\.(\d+))"));
    QRegularExpressionMatch pre16Match = pre16Ver.match(versionId);
    const bool isPre16 = pre16Match.hasMatch() && pre16Match.captured(1).toInt() < 6;
    const bool isFmlMain = mainClass.contains(QStringLiteral("FMLRelauncher"))
                        || mainClass.contains(QStringLiteral("fml.relauncher"))
                        || mainClass.contains(QStringLiteral("forge"));
    if (isPre16 && !isFmlMain) {
        mainClass = QStringLiteral("net.minecraft.client.Minecraft");
    }
    // 2026-08-19：pre-1.6 声音修复 —— 游戏资源线程(amx)会先请求
    // http://s3.amazonaws.com/MinecraftResources/（已死），只有请求**失败**后
    // 才 catch 扫描本地 resources/ 注册声音。s3 挂起时（尤其国内网络可能 20-30s）
    // 声音被无限推迟。强制 http 走死代理(127.0.0.1:1) → 立即 ConnectException →
    // 资源线程秒进 catch → 本地扫描 → 声音即时注册。pre-1.6 游戏的 http 仅用于
    // s3 资源，不影响其他功能。
    if (isPre16) {
        args << QStringLiteral("-Dhttp.proxyHost=127.0.0.1")
             << QStringLiteral("-Dhttp.proxyPort=1");
    }
    args << mainClass;

    // ── Minecraft arguments ──
    // Walk inheritsFrom chain: child args (Forge --launchTarget) FIRST, then parent args
    // modlauncher strips its own flags and passes the rest to Minecraft

    // Forge 1.16.5+ modlauncher: FMLClientLaunchProvider.setup() should add --mavenRoots and
    // --mods (forge universal coordinate) before beginModScan(). If setup() is not reached or
    // fails silently, MavenDirectoryLocator gets empty modCoords and forge mod won't register.
    // As a safety net, inject these args explicitly here:

    // Collect game args from version JSON chain (declared before forge block for early prepend)
    QJsonArray gameArgs;
    {
        // Collect args from version JSON chain
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
            QString parentPath = m_gameDir + QStringLiteral("/versions/") + parentId;
            QString parentJson = findVersionJson(parentPath, parentId);
            if (parentJson.isEmpty()) {
                parentJson = m_gameDir + QStringLiteral("/versions/") + parentId
                           + QStringLiteral("/") + parentId + QStringLiteral(".json");
                if (!QFileInfo::exists(parentJson)) break;
            }
            QFile pf(parentJson);
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

    // ── Forge/NeoForge 1.16.5+ 安全网: 注入 --mavenRoots --mods ──
    // FMLClientLaunchProvider.setup() should add these, but if setup() fails silently
    // MavenDirectoryLocator gets empty modCoords and forge mod won't register.
    if (versionId.contains(QStringLiteral("forge")) || versionId.contains(QStringLiteral("neoforge"))) {
        // Check if gameArgs already has --mavenRoots (JSON chain provided them)
        bool hasMavenRoots = false;
        for (int i = 0; i < gameArgs.size(); ++i) {
            if (gameArgs[i].toString() == QStringLiteral("--mavenRoots")) {
                hasMavenRoots = true;
                break;
            }
    }
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
            QString parentPath = m_gameDir + QStringLiteral("/versions/") + parentId;
            QString parentJson = findVersionJson(parentPath, parentId);
            if (parentJson.isEmpty()) {
                parentJson = m_gameDir + QStringLiteral("/versions/") + parentId
                           + QStringLiteral("/") + parentId + QStringLiteral(".json");
                if (!QFileInfo::exists(parentJson)) break;
            }
            QFile pf(parentJson);
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
            // 2026-08-19 修复（声音缺失）：资源必须复制到游戏实际读取的位置 =
            // m_versionGameDir（散装布局=versions/<id>；legacy game/ 布局=versions/<id>/game）。
            // 旧代码写死 versions/<id>/game/resources → 散装布局下游戏从
            // versions/<id>/resources 读取 → 复制过去的声音/音乐找不到 → 无声。
            QString gameResDir = m_versionGameDir + QStringLiteral("/resources");
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
                qCInfo(logLaunch) << QStringLiteral("[启动] 游戏资源已复制 数量=%1 目标=%2").arg(resCopied).arg(gameResDir);
        }
    }

    // Process game arguments, expanding placeholders
    for (const QJsonValue& argVal : gameArgs) {
        if (argVal.isString()) {
            QString arg = argVal.toString();
            // Replace placeholders
            arg.replace(QStringLiteral("${auth_player_name}"), m_authName.isEmpty() ? QStringLiteral("{username}") : m_authName);
            arg.replace(QStringLiteral("${version_name}"), versionId);
            // ── 版本隔离：${game_directory} 必须指向版本隔离目录 ──
            // 当版本隔离启用时，m_versionGameDir 指向 versions/{id}/game（或 versions/{id}）
            // 当版本隔离关闭时，m_versionGameDir == m_gameDir（根目录），行为不变
            arg.replace(QStringLiteral("${game_directory}"),
                        useShortPaths ? toShortPath(m_versionGameDir) : m_versionGameDir);
            arg.replace(QStringLiteral("${assets_root}"),
                        gameDirForLaunch + QStringLiteral("/assets"));
            arg.replace(QStringLiteral("${assets_index_name}"), assetIndexId);
            arg.replace(QStringLiteral("${auth_uuid}"), m_authUuid.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : m_authUuid);
            arg.replace(QStringLiteral("${auth_access_token}"), m_authToken.isEmpty() ? QStringLiteral("0") : m_authToken);
            arg.replace(QStringLiteral("${user_type}"), m_isOnline ? QStringLiteral("msa") : QStringLiteral("mojang"));
            arg.replace(QStringLiteral("${version_type}"),
                        versionJson[QStringLiteral("type")].toString(QStringLiteral("release")));
            arg.replace(QStringLiteral("${game_assets}"),
                        gameDirForLaunch + QStringLiteral("/assets/virtual/") + assetIndexId);  // legacy/pre-1.6
            arg.replace(QStringLiteral("${user_properties}"), QStringLiteral("{}"));
            arg.replace(QStringLiteral("${auth_session}"), m_authToken.isEmpty() ? QStringLiteral("0") : m_authToken);  // pre-1.6
            arg.replace(QStringLiteral("${resolution_width}"), QString::number(m_resWidth));
            arg.replace(QStringLiteral("${resolution_height}"), QString::number(m_resHeight));
            arg.replace(QStringLiteral("${clientid}"), QStringLiteral(""));
            arg.replace(QStringLiteral("${auth_xuid}"), QStringLiteral(""));
            arg.replace(QStringLiteral("${launcher_name}"), QStringLiteral("ShadowLauncher"));
            arg.replace(QStringLiteral("${launcher_version}"), QStringLiteral("1.0"));
            arg.replace(QStringLiteral("${profile_name}"), versionId);
            arg.replace(QStringLiteral("${quickPlayPath}"), QString());  // not used

            // 2026-08-10：不再过滤空参数——${clientid}/${auth_xuid} 等替换为空后必须保留
            // （--clientId 后跟空值参数），否则参数错位；bat 导出侧 quoteBatArg 已保留为 ""
            args << arg;
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

    // ── OptiFine + Forge/LiteLoader tweakClass 顺序修复 ──
    // OptiFineForgeTweaker 必须在 --tweakClass 链的末尾，否则 Forge 找不到
    // 同时修复常见的错误名称: optifine.OptiFineTweaker → optifine.OptiFineForgeTweaker
    // 注意: 独立 OptiFine（无 Forge）不应改名，否则 OptiFineForgeTweaker 可能
    // 改变 LaunchWrapper 的参数传递行为，导致游戏收不到 --accessToken 等参数。
    {
        auto hasTweakClass = [&](const QString& name) -> int {
            for (int j = 0; j < args.size(); ++j) {
                if (args[j] == QStringLiteral("--tweakClass") && j + 1 < args.size()
                    && args[j + 1] == name)
                    return j;
            }
            return -1;
        };

        // Only rename OptiFineTweaker → OptiFineForgeTweaker when Forge is present.
        // Check: version ID contains "forge", or there's >1 --tweakClass (non-OptiFine).
        int tweakCount = 0;
        for (int j = 0; j < args.size(); ++j) {
            if (args[j] == QStringLiteral("--tweakClass")) ++tweakCount;
        }
        bool hasForge = versionId.contains(QStringLiteral("forge")) || tweakCount > 1;
        qCInfo(logLaunch) << QStringLiteral("[启动] TweakClass 修复: 版本=%1, 数量=%2, 含Forge=%3")
            .arg(versionId).arg(tweakCount).arg(hasForge);

        // 修复错误的 OptiFineTweaker 名称（仅当 Forge 存在时）
        if (hasForge) {
            // Only rename to ForgeTweaker when Forge is actually present
            int wrongIdx = hasTweakClass(QStringLiteral("optifine.OptiFineTweaker"));
            if (wrongIdx >= 0) {
                args[wrongIdx + 1] = QStringLiteral("optifine.OptiFineForgeTweaker");
                qCInfo(logLaunch) << QStringLiteral("[启动] 修正 TweakClass: optifine.OptiFineTweaker → optifine.OptiFineForgeTweaker");
            }
            // 将 OptiFineForgeTweaker 移到 --tweakClass 链末尾
            int forgeIdx = hasTweakClass(QStringLiteral("optifine.OptiFineForgeTweaker"));
            if (forgeIdx >= 0) {
                QString tweakClass = args.takeAt(forgeIdx);     // --tweakClass
                QString className = args.takeAt(forgeIdx);       // optifine.OptiFineForgeTweaker
                args << tweakClass << className;
                qCInfo(logLaunch) << QStringLiteral("[启动] OptiFineForgeTweaker 已移至参数末尾");
            }
        }
    }

    // Ensure game directory exists (could be game/ or version root for scattered structure)
    QDir().mkpath(m_versionGameDir);

    // ── 启动细节参数追加（全屏 / 自动进服，主流启动器 QuickPlay 语义）──
    appendGameDetailArgs(args, versionJson);

    return args;
}

// ============================================================
// Private Helpers — Launch Detail Args (全屏 / 自动进服)
// ============================================================

void Launcher::appendGameDetailArgs(QStringList& args, const QJsonObject& versionJson) const
{
    // ── 全屏启动（--fullscreen，主流启动器 LaunchArgumentWindowType=0 语义）──
    if (m_fullscreen) {
        if (!args.contains(QStringLiteral("--fullscreen"))) {
            args << QStringLiteral("--fullscreen");
            qCInfo(logLaunch) << QStringLiteral("[启动] 已启用全屏启动");
        }
    }

    // ── 自动进服（主流启动器 QuickPlay 语义）──
    // 新版（2023-04-05 后，1.20+）：--quickPlayMultiplayer <addr>
    // 老版（1.20-）：--server <host> --port <port>（主流启动器 ReleaseTime > 2023/4/4 判定）
    if (!m_autoJoinServer.isEmpty()) {
        QString addr = m_autoJoinServer.trimmed();
        // 去重：用户自定义 gameArgs 已带 --server/--quickPlayMultiplayer 时不重复注入
        bool alreadyInjected = false;
        for (const QString& a : args) {
            if (a == QStringLiteral("--server") || a == QStringLiteral("--quickPlayMultiplayer")
                || a.startsWith(QStringLiteral("--quickPlayMultiplayer="))) {
                alreadyInjected = true;
                break;
            }
        }
        if (!alreadyInjected) {
            // 版本发布时间判定（主流启动器 ReleaseTime > 2023-04-04 → QuickPlay）
            bool useQuickPlay = false;
            QString releaseTime = versionJson.value(QStringLiteral("releaseTime")).toString();
            if (!releaseTime.isEmpty()) {
                QDateTime rt = QDateTime::fromString(releaseTime, Qt::ISODate);
                if (rt.isValid() && rt > QDateTime(QDate(2023, 4, 4), QTime(0, 0), Qt::UTC)) {
                    useQuickPlay = true;
                }
            } else {
                // 无 releaseTime（老版 JSON）：尝试从版本号判断 1.20+
                static const QRegularExpression reVer(QStringLiteral(R"(^1\.(\d+))"));
                QRegularExpressionMatch m2 = reVer.match(m_currentVersionId);
                if (m2.hasMatch()) {
                    int minor = m2.captured(1).toInt();
                    useQuickPlay = (minor >= 20);
                }
            }

            if (useQuickPlay) {
                args << QStringLiteral("--quickPlayMultiplayer") << addr;
                qCInfo(logLaunch) << QStringLiteral("[启动] 自动进服(QuickPlay): %1").arg(addr);
            } else {
                QString host = addr;
                int port = 25565;
                int colon = addr.indexOf(QLatin1Char(':'));
                if (colon > 0) {
                    host = addr.left(colon);
                    bool ok = false;
                    int p = addr.mid(colon + 1).toInt(&ok);
                    if (ok && p > 0 && p <= 65535) port = p;
                }
                args << QStringLiteral("--server") << host << QStringLiteral("--port") << QString::number(port);
                qCInfo(logLaunch) << QStringLiteral("[启动] 自动进服(老版): %1:%2").arg(host).arg(port);
            }
        }
    }
}

// ============================================================
// Private Helpers — Pre/Post Commands & Window Title
// ============================================================

void Launcher::runPreLaunchCommand()
{
    if (m_preLaunchCommand.trimmed().isEmpty()) return;
    // 异步执行，不阻塞主线程（对齐主流启动器实现 preLaunchCommand；主流启动器 用 Loader 同步但我们绝不阻塞 UI）
    QProcess* p = new QProcess;  // 不挂靠 Launcher 生命周期，避免启动器销毁时连带 kill
    p->setWorkingDirectory(m_versionGameDir.isEmpty() ? m_gameDir : m_versionGameDir);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            p, [p](int code, QProcess::ExitStatus) {
                qCInfo(logLaunch) << QStringLiteral("[启动] 启动前命令完成 退出码=%1").arg(code);
                p->deleteLater();
            });
    qCInfo(logLaunch) << QStringLiteral("[启动] 执行启动前命令: %1").arg(m_preLaunchCommand);
    // 经 shell 执行整条命令（支持参数/空格/引号/内建/批处理/重定向）。
    // Windows 用 setNativeArguments 把命令行原样交给 cmd.exe（不做 Qt 逐参数引用），
    // 否则含双引号/空格的路径会在多层转义后被改坏，导致“找不到文件”。
#ifdef Q_OS_WIN
    p->setProgram(QStringLiteral("cmd.exe"));
    p->setNativeArguments(QStringLiteral("/c ") + m_preLaunchCommand);
    p->start();
#else
    p->start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), m_preLaunchCommand });
#endif
}

void Launcher::runPostExitCommand()
{
    if (m_postExitCommand.trimmed().isEmpty()) return;
    QProcess* p = new QProcess;  // 不挂靠 Launcher 生命周期，否则 launchFinished 后 Launcher 被销毁、此进程被连带 kill
    p->setWorkingDirectory(m_versionGameDir.isEmpty() ? m_gameDir : m_versionGameDir);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            p, [p](int code, QProcess::ExitStatus) {
                qCInfo(logLaunch) << QStringLiteral("[启动] 退出后命令完成 退出码=%1").arg(code);
                p->deleteLater();
            });
    qCInfo(logLaunch) << QStringLiteral("[启动] 执行退出后命令: %1").arg(m_postExitCommand);
    // 同 runPreLaunchCommand：原样交给 shell，避免引号/空格被多层转义改坏
#ifdef Q_OS_WIN
    p->setProgram(QStringLiteral("cmd.exe"));
    p->setNativeArguments(QStringLiteral("/c ") + m_postExitCommand);
    p->start();
#else
    p->start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), m_postExitCommand });
#endif
}

void Launcher::applyWindowTitleOverride()
{
    if (m_windowTitleOverride.trimmed().isEmpty()) return;
    if (m_pid <= 0) return;
#ifdef Q_OS_WIN
    // 持续覆盖：整个游戏运行期间周期性把标题改回目标值。只改一次很快会被游戏/Mod
    // 在加载过程中的多次重写覆盖掉，因此需要反复校验并纠正。
    // 窗口按类名识别（GLFW=新版 LWJGL3、LWJGL=旧版、SunAwtFrame=老旧 AWT），
    // 避免误改辅助窗口或 GLFW 消息窗。
    qCInfo(logLaunch) << QStringLiteral("[启动] 持续应用游戏窗口标题: %1").arg(m_windowTitleOverride);

    if (!m_titleTimer) {
        m_titleTimer = new QTimer(this);
        m_titleTimer->setInterval(150);
        const QString target = m_windowTitleOverride;
        connect(m_titleTimer, &QTimer::timeout, this, [this, target]() {
            if (m_pid <= 0) return;
            struct Ctx { DWORD pid; HWND hwnd; };
            Ctx ctx{ static_cast<DWORD>(m_pid), nullptr };
            EnumWindows([](HWND h, LPARAM lp) -> BOOL {
                Ctx* c = reinterpret_cast<Ctx*>(lp);
                if (!IsWindowVisible(h)) return TRUE;
                wchar_t cls[64] = { 0 };
                GetClassNameW(h, cls, 63);
                const QString cn = QString::fromWCharArray(cls);
                const bool isGame = cn == QLatin1String("GLFW30")
                                 || cn == QLatin1String("LWJGL")
                                 || cn == QLatin1String("SunAwtFrame");
                if (!isGame) return TRUE;
                DWORD wpid = 0;
                GetWindowThreadProcessId(h, &wpid);
                if (wpid == c->pid) {
                    c->hwnd = h;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&ctx));

            if (!ctx.hwnd) return;
            wchar_t cur[256] = { 0 };
            GetWindowTextW(ctx.hwnd, cur, 256);
            if (target == QString::fromWCharArray(cur))
                return;  // 已是目标标题，跳过，避免无谓重设引起闪烁
            SetWindowTextW(ctx.hwnd, reinterpret_cast<LPCWSTR>(target.utf16()));
        });
    }
    m_titleTimer->start();
#endif
}

QString Launcher::buildLaunchScript(const QString& versionId, const QString& javaPath,
                                    int maxMemoryMB, const QString& jvmArgs,
                                    const QString& gameArgs, bool highPerfGpu)
{
    // 复用 start() 的成员设置 + 版本 JSON 读取（不启动进程，仅组装命令行）
    m_jvmArgs = jvmArgs;
    m_gameArgs = gameArgs;
    m_highPerfGpu = highPerfGpu;
    m_currentVersionId = versionId;

    // Java 主版本 + 32 位判定（buildArgs 的 --add-opens / -Xss1m 需要，2026-08-10 与 start() 对齐）
    m_is32BitJvm = false;
    {
        QProcess javap;
        javap.start(javaPath, {QStringLiteral("-version")});
        javap.waitForFinished(5000);
        QString output = QString::fromLocal8Bit(javap.readAllStandardError());
        if (output.isEmpty()) output = QString::fromLocal8Bit(javap.readAllStandardOutput());
        // version "1.8.0_xxx" / "17.0.19" 提取主版本
        int vi = output.indexOf(QLatin1Char('"'));
        int vj = vi >= 0 ? output.indexOf(QLatin1Char('"'), vi + 1) : -1;
        if (vi >= 0 && vj > vi) {
            QString ver = output.mid(vi + 1, vj - vi - 1);
            ver.replace(QLatin1Char('_'), QLatin1Char('.'));
            QStringList parts = ver.split(QLatin1Char('.'));
            if (!parts.isEmpty()) {
                int v = parts[0].toInt();
                if (v == 1 && parts.size() >= 2) v = parts[1].toInt();
                m_javaMajorVersion = v;
            }
        }
        // 32 位 JVM：Java 输出 "32-Bit"（对齐 start() 的判定）
        if (output.contains(QStringLiteral("32-Bit"))) {
            m_is32BitJvm = true;
        }
        // 系统物理内存（JIT 优化组阈值：>4GB 才启用）
#ifdef Q_OS_WIN
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) {
            m_totalSystemMemoryMB = static_cast<qint64>(ms.ullTotalPhys / (1024 * 1024));
        }
#endif
        qCInfo(logLaunch) << QStringLiteral("[启动] 导出脚本 Java 主版本=%1 32位=%2 系统内存=%3MB")
            .arg(m_javaMajorVersion).arg(m_is32BitJvm).arg(m_totalSystemMemoryMB);
    }

    // 读版本 JSON
    QString jsonPath = m_gameDir + QStringLiteral("/versions/") + versionId
                       + QStringLiteral("/") + versionId + QStringLiteral(".json");
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        qCWarning(logApp) << "[启动] 导出脚本失败: 无法读取版本配置" << jsonPath;
        return QString();
    }
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &parseErr);
    jsonFile.close();
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return QString();
    QJsonObject versionJson = doc.object();

    // 对齐 start()：脚本启动前也写语言选项到 options.txt（否则脚本启动后 MC 语言
    // 是默认 en_us，与启动器启动不一致；2026-08-07 用户反馈）
    if (m_autoLangMode == 1 || m_autoLangMode == 2)
        ensureOptionsTxt();

    const QStringList args = buildArgs(versionId, maxMemoryMB, versionJson);
    if (args.isEmpty()) return QString();

    const QString workDir = QDir::toNativeSeparators(
        m_versionGameDir.isEmpty() ? m_gameDir : m_versionGameDir);
    const QString javaNative = QDir::toNativeSeparators(javaPath);

    // ── GPU 高性能（对齐 start()：SHIM_MCCOMPAT 环境变量 + UserGpuPreferences 注册表）──
    QString gpuBlock;
    if (m_highPerfGpu) {
        gpuBlock += QStringLiteral("set SHIM_MCCOMPAT=0x800000001\r\n");
        gpuBlock += QStringLiteral("reg add \"HKCU\\Software\\Microsoft\\DirectX\\UserGpuPreferences\" /v \"%1\" /d \"GpuPreference=2;\" /f >nul 2>&1\r\n")
                        .arg(javaNative);
    }

    // ── pre/post 启动命令（对齐主流启动器实现 SaveBatch：原样插入；pre 在 java 前同步执行，post 在游戏退出后）──
    QString preBlock, postBlock;
    if (!m_preLaunchCommand.trimmed().isEmpty())
        preBlock = QStringLiteral("rem 启动前命令\r\n%1\r\n").arg(m_preLaunchCommand);
    if (!m_postExitCommand.trimmed().isEmpty())
        postBlock = QStringLiteral("rem 退出后命令\r\n%1\r\n").arg(m_postExitCommand);

    // ── 进程优先级 / 窗口标题覆盖 → PowerShell 包装启动 ──
    // bat 的 java 行会同步阻塞到游戏退出，期间无法改优先级/标题；
    // 用 Start-Process 启动 + PriorityClass 设置 + MainWindowHandle 轮询改标题（对齐启动器行为）。
    // 命令经 UTF-16LE → Base64 用 -EncodedCommand 传入，规避 bat 引号/特殊字符全部问题。
    QString psPriority;
    if (m_processPriority == 0) psPriority = QStringLiteral(" -PriorityClass AboveNormal");
    else if (m_processPriority == 2) psPriority = QStringLiteral(" -PriorityClass BelowNormal");
    const bool needPsWrap = !psPriority.isEmpty() || !m_windowTitleOverride.trimmed().isEmpty();

    QString javaExecLine;
    if (needPsWrap) {
        // PowerShell 数组元素：空/含空格参数用双引号字符串（join 后 CreateProcess 正确分组）
        auto psStr = [](const QString& s) {
            QString escaped = QString(s).replace(QLatin1Char('\''), QStringLiteral("''"));
            if (s.isEmpty() || s.contains(QLatin1Char(' ')) || s.contains(QLatin1Char('\t')))
                return QStringLiteral("'\"%1\"'").arg(escaped.replace(QLatin1Char('"'), QStringLiteral("\\\"")));
            return QStringLiteral("'%1'").arg(escaped);
        };
        QStringList argList;
        for (const QString& a : args) argList << psStr(a);
        QString ps = QStringLiteral("$p=Start-Process -FilePath %1 -ArgumentList @(%2) -WorkingDirectory %3%4 -PassThru;")
                         .arg(psStr(javaNative), argList.join(QLatin1Char(',')), psStr(workDir), psPriority);
        if (!m_windowTitleOverride.trimmed().isEmpty()) {
            ps += QStringLiteral(" Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public class WU{[DllImport(\"user32.dll\")]public static extern bool SetWindowText(System.IntPtr h,string t);}';");
            ps += QStringLiteral(" for($i=0;$i -lt 240;$i++){ $p.Refresh(); if($p.MainWindowHandle -ne [System.IntPtr]::Zero){ [WU]::SetWindowText($p.MainWindowHandle,%1); break }; Start-Sleep -Milliseconds 500 };")
                      .arg(psStr(m_windowTitleOverride.trimmed()));
        }
        ps += QStringLiteral(" $p.WaitForExit()");
        // UTF-16LE → Base64（-EncodedCommand 要求 UTF-16LE/UTF-8 编码的 Base64）
        QByteArray utf16;
        const QString& psRef = ps;
        for (QChar c : psRef) {
            utf16.append(char(c.unicode() & 0xFF));
            utf16.append(char((c.unicode() >> 8) & 0xFF));
        }
        javaExecLine = QStringLiteral("powershell -NoProfile -EncodedCommand %1")
                           .arg(QString::fromLatin1(utf16.toBase64()));
    } else {
        QStringList argStr;
        for (const QString& a : args) argStr << quoteBatArg(a);
        javaExecLine = QStringLiteral("\"%1\" %2").arg(javaNative, argStr.join(QLatin1Char(' ')));
    }

    // ── pre-1.6：.bat 也要覆盖 APPDATA（游戏 getAppDir 只认 %APPDATA%\.minecraft，
    //    与 start() 的 APPDATA 逻辑保持一致；隔离布局先建 junction 再指向版本目录）──
    QString pre16EnvBlock;
    {
        static const QRegularExpression pre16Rgx(QStringLiteral(R"(^1\.(\d+))"));
        auto pre16M = pre16Rgx.match(versionId);
        bool isPre16 = pre16M.hasMatch() && pre16M.captured(1).toInt() < 6;
        if (isPre16) {
            const QString workDirNative = QDir::toNativeSeparators(workDir);
            if (workDirNative.endsWith(QStringLiteral("\.minecraft"))) {
                // Shared: APPDATA = 父目录 → %APPDATA%\.minecraft = 版本目录
                QDir parentDir(workDir);
                parentDir.cdUp();
                pre16EnvBlock += QStringLiteral("set \"APPDATA=%1\"\r\n")
                    .arg(QDir::toNativeSeparators(parentDir.absolutePath()));
            } else {
                // Isolated: junction <versions>\.minecraft → <版本目录>，APPDATA = <versions>
                const bool isGameSubdir = workDirNative.endsWith(QStringLiteral("\\game"));
                QString versionDir;
                if (isGameSubdir) {
                    QDir gd(workDir);
                    gd.cdUp();
                    versionDir = QDir::toNativeSeparators(gd.absolutePath());
                } else {
                    QDir vd(workDir);
                    vd.cdUp();
                    versionDir = QDir::toNativeSeparators(vd.absolutePath());
                }
                const QString junction = versionDir + QStringLiteral("\\.minecraft");
                pre16EnvBlock += QStringLiteral("if not exist \"%1\" mklink /J \"%1\" \"%2\"\r\n")
                    .arg(QDir::toNativeSeparators(junction),
                         QDir::toNativeSeparators(workDir));
                pre16EnvBlock += QStringLiteral("set \"APPDATA=%1\"\r\n").arg(versionDir);
            }
        }
    }

    // 组装 .bat（UTF-8 输出 + 主流启动器 式结构 + 完整命令行 + 错误暂停）
    QString script;
    script += QStringLiteral("@echo off\r\n");
    script += QStringLiteral("chcp 65001 >nul\r\n");
    script += QStringLiteral("rem Shadow Launcher start script - version %1\r\n").arg(versionId);
    script += QStringLiteral("rem Generated: %1\r\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    script += QStringLiteral("title 启动 - %1\r\n").arg(versionId);
    script += QStringLiteral("echo 游戏正在启动，请稍候。\r\n");
    script += QStringLiteral("cd /d \"%1\"\r\n").arg(workDir);
    script += gpuBlock;
    script += pre16EnvBlock;
    script += preBlock;
    script += javaExecLine + QStringLiteral("\r\n");
    script += postBlock;
    script += QStringLiteral("echo 游戏已退出。\r\n");
    script += QStringLiteral("pause\r\n");

    // ── 登录凭据策略（2026-08-10）：保留有效 token ──
    // 正版/外置导出脚本需带有效 token 才能建立正版会话（否则游戏内 Realms 直接失效）；
    // 时效验证/刷新由 ShadowBackend::exportLaunchScript 负责，这里只负责：
    // 有 token → 脚本头部加凭据提示；token 为空 → buildArgs 已用 0 占位（离线）。
    if (!m_authToken.isEmpty() && m_authToken != QLatin1String("0")) {
        script.replace(QStringLiteral("rem Shadow Launcher start script"),
                       QStringLiteral("rem 注意：本脚本包含正版登录凭据（accessToken），请勿分享给他人！\r\nrem Shadow Launcher start script"));
    }

    // ── bat 转义：% → %%（对齐主流启动器实现 SaveBatch .Replace("%","%%")，防 cmd 变量误展开）──
    script.replace(QLatin1Char('%'), QStringLiteral("%%"));

    return script;
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

    qCInfo(logLaunch) << QStringLiteral("[启动] 开始解压运行库 版本=%1 目标=%2").arg(versionId, nativesDir);

    // Always extract — idempotent per-file comparison, not whole-directory skip
    QDir().mkpath(nativesDir);

    const QString libsDir = m_gameDir + QStringLiteral("/libraries");
    const QJsonArray libraries = resolveMergedLibraries(m_gameDir, versionJson);

    // ── 预扫描：收集所有应存在的 native 文件名（basename only）──
    QSet<QString> expectedFiles;

    // Determine platform-specific native classifier prefix
#ifdef Q_OS_WIN
    const QString nativePrefix = QStringLiteral("natives-windows");
#elif defined(Q_OS_MACOS)
    const QString nativePrefix = QStringLiteral("natives-osx");
#else
    const QString nativePrefix = QStringLiteral("natives-linux");
#endif

    qCInfo(logLaunch) << QStringLiteral("[启动] 扫描运行库 数量=%1 平台=%2").arg(libraries.size()).arg(nativePrefix);

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
                qCWarning(logLaunch) << QStringLiteral("[启动] 无法打开运行库 JAR 路径=%1").arg(jarPath);
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
                    expectedFiles.insert(baseName);
                    QString destPath = nativesDir + QStringLiteral("/") + baseName;

                    // ── Per-file idempotent: skip if size matches ──
                    QFileInfo destInfo(destPath);
                    if (destInfo.exists() && destInfo.size() == data.size()) {
                        qCDebug(logLaunch) << QStringLiteral("Natives文件已存在且大小一致 跳过:%1").arg(baseName);
                        continue;
                    }
                    if (destInfo.exists()) {
                        qCDebug(logLaunch) << QStringLiteral("Natives文件大小不一致 重新解压:%1 旧=%2 新=%3")
                                           .arg(baseName).arg(destInfo.size()).arg(data.size());
                    }

                    QFile destFile(destPath);
                    if (destFile.open(QIODevice::WriteOnly)) {
                        destFile.write(data);
                        destFile.close();
                        extractedCount++;
                    } else {
                        qCWarning(logLaunch) << QStringLiteral("[启动] 运行库写入失败 文件=%1 错误=%2")
                                             .arg(baseName, destFile.errorString());
                    }
                }
            }
            zipReader.close();
        }
    }

    qCInfo(logLaunch) << QStringLiteral("[启动] 运行库解压完成 JAR数=%1 文件数=%2 目标=%3").arg(jarCount).arg(extractedCount).arg(nativesDir);

    // ── 清理旧版本残留的 DLL（防止版本切换时加载错误的 native）──
    {
        QDir nd(nativesDir);
        const auto files = nd.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        int cleaned = 0;
        for (const QFileInfo& fi : files) {
            if (!expectedFiles.contains(fi.fileName())) {
                if (QFile::remove(fi.absoluteFilePath())) {
                    qCDebug(logLaunch) << QStringLiteral("Natives清理残留: %1").arg(fi.fileName());
                    cleaned++;
                }
            }
        }
        if (cleaned > 0) {
            qCInfo(logLaunch) << QStringLiteral("[启动] 运行库清理完成 残留文件数=%1").arg(cleaned);
        }
    }

    return extractedCount > 0 || jarCount == 0;
}

// ============================================================
// Private Helpers — Language Detection & options.txt
// ============================================================

// ── 版本 JSON 灵活查找（目录名≠文件名时使用）──
QString Launcher::findVersionJson(const QString& verDir, const QString& dirName)
{
    // Fast path: 只做存在性检查，避免不必要的 JSON 解析
    QString path = verDir + QStringLiteral("/") + dirName + QStringLiteral(".json");
    if (QFileInfo::exists(path)) return path;

    // Slow path: scan all .json files for valid version descriptors
    QDir vdir(verDir);
    const QStringList jsons = vdir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    for (const QString& jf : jsons) {
        if (jf == dirName + QStringLiteral(".json")) continue;
        if (jf == QStringLiteral("authlib-injector.json")) continue;
        path = verDir + QStringLiteral("/") + jf;
        QFile jf2(path);
        if (jf2.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(jf2.readAll(), &err);
            jf2.close();
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains(QStringLiteral("mainClass"))
                    && obj.contains(QStringLiteral("type"))
                    && obj.contains(QStringLiteral("id")))
                    return path;
            }
        }
    }
    return QString();
}

// ── Jar-in-Jar 检测（现代 Forge 1.19.3+ / NeoForge）──
// 这些 loader 的 jij: URI 用 '~' 做分层分隔符（JarJarFileSystems.URI_SPLIT_REGEX = "~"）。
// Windows 8.3 短路径段（如 SHADOW~1.1）必然含 '~'，短路径从 --gameDir 污染 mods 扫描后，
// jij: URI 会被拆坏 → 内嵌库（MixinExtras 等）加载失败。含 JarJar 库的版本必须禁用短路径。
bool Launcher::versionUsesJarInJar(const QString& versionId, const QJsonObject& versionJson,
                                   const QString& gameDir)
{
    auto hasJarJarLib = [](const QJsonObject& json) {
        const QJsonArray libs = json[QStringLiteral("libraries")].toArray();
        for (const QJsonValue& v : libs) {
            const QJsonObject lib = v.toObject();
            const QString name = lib[QStringLiteral("name")].toString();
            // net.minecraftforge:JarJarSelector / JarJarFileSystems / JarJarMetadata
            if (name.startsWith(QStringLiteral("net.minecraftforge:JarJar")))
                return true;
        }
        return false;
    };

    QJsonObject cur = versionJson;
    QString id = versionId;
    QStringList visited;
    while (true) {
        if (hasJarJarLib(cur)) return true;
        const QString parentId = cur[QStringLiteral("inheritsFrom")].toString();
        if (parentId.isEmpty() || parentId == id || visited.contains(parentId))
            return false;
        visited.append(parentId);
        const QString parentJsonPath = findVersionJson(
            gameDir + QStringLiteral("/versions/") + parentId, parentId);
        if (parentJsonPath.isEmpty()) return false;
        QFile pf(parentJsonPath);
        if (!pf.open(QIODevice::ReadOnly)) return false;
        QJsonParseError pe;
        const QJsonDocument pd = QJsonDocument::fromJson(pf.readAll(), &pe);
        pf.close();
        if (pe.error != QJsonParseError::NoError) return false;
        cur = pd.object();
        id = parentId;
    }
}

void Launcher::ensureOptionsTxt()
{
    if (m_versionGameDir.isEmpty()) return;

    // ── Yosbr Mod 兼容：如果 config/yosbr/options.txt 存在，使用它而不是根目录的
    QString optionsDir = m_versionGameDir;
    if (QFileInfo::exists(m_versionGameDir + QStringLiteral("/config/yosbr/options.txt"))) {
        optionsDir = m_versionGameDir + QStringLiteral("/config/yosbr");
        qCInfo(logLaunch) << QStringLiteral("[启动] 检测到 Yosbr Mod options.txt");
    }
    QString mcLang;
    if (m_autoLangMode == 2 && !m_detectedRegion.isEmpty()) {
        mcLang = mc_language::regionToMinecraftLang(m_detectedRegion);
    } else {
        mcLang = mc_language::localeToMinecraftLang(QLocale::system());
    }

    // Minecraft 1.10 及更早版本的语言代码末两位必须大写
    // (如 zh_cn -> zh_CN, en_us -> en_US)，否则语言设置不生效
    // 1.11+ 使用全小写, 1.0- 无语言选项
    static const QRegularExpression reVer(QStringLiteral(R"(^1\.(\d+))"));
    QRegularExpressionMatch match = reVer.match(m_currentVersionId);
    if (match.hasMatch()) {
        bool ok = false;
        int minorVer = match.captured(1).toInt(&ok);
        if (ok && minorVer <= 10 && mcLang.length() >= 2) {
            // 末两位转大写: zh_cn -> zh_CN, en_us -> en_US, fr_fr -> fr_FR
            mcLang = mcLang.left(mcLang.length() - 2)
                   + mcLang.right(2).toUpper();
        }
    }

    mc_language::writeOptionsTxt(optionsDir, mcLang);
}

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
        return false;  // any unknown feature — not a match for normal user
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
        qCWarning(logLaunch) << QStringLiteral("[启动] 旧版资源索引不存在 路径=%1").arg(indexFile);
        return;
    }

    // Always process — per-file check prevents redundant copies

    QFile file(indexFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logLaunch) << QStringLiteral("[启动] 无法打开旧版资源索引文件");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    QJsonObject objects = doc.object()[QStringLiteral("objects")].toObject();
    if (objects.isEmpty()) return;

    QString objectsDir = m_gameDir + QStringLiteral("/assets/objects");
    int created = 0;

    qCInfo(logLaunch) << QStringLiteral("[启动] 开始构建旧版资源虚拟目录");

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

    qCInfo(logLaunch) << QStringLiteral("[启动] 旧版资源构建完成 文件数=%1 目标=%2").arg(created).arg(legacyDir);
}

} // namespace ShadowLauncher
