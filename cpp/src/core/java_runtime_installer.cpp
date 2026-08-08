// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "java_runtime_installer.h"

#include "http_client.h"
#include "../utils/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QLockFile>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QVariantMap>
#include <thread>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <private/qzipreader_p.h>

namespace ShadowLauncher {

// ============================================================
// 构造 / 架构检测
// ============================================================

JavaRuntimeInstaller::JavaRuntimeInstaller(QObject* parent)
    : QObject(parent)
    , m_cpuArch(detectCpuArch())
{
    qCInfo(logJava) << QStringLiteral("[Java安装] 初始化 架构=%1").arg(m_cpuArch);
    // 前置检测完成后自动继续安装流程（installRequiredJavas 触发扫描后等待此回调）
    connect(this, &JavaRuntimeInstaller::systemJavaScanFinished,
            this, &JavaRuntimeInstaller::runInstallAfterScan);
    cleanupStaleCache();
}

/// 启动清理：删除上次崩溃/失败残留的临时 zip 与残缺目录
void JavaRuntimeInstaller::cleanupStaleCache()
{
    // 统一用 applicationDirPath（与安装缓存 javaInstallDir 一致，避免 currentPath 漂移）
    const QString cacheRoot = QCoreApplication::applicationDirPath() + QStringLiteral("/java_cache");
    QDir root(cacheRoot);
    if (!root.exists()) return;

    // 1. 残留临时 zip（注意 . 开头文件默认被 QDir 过滤，需 QDir::Hidden）
    const QStringList tmpZips = root.entryList({ QStringLiteral(".tmp-jdk-*.zip") },
                                               QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QString& f : tmpZips) {
        QFile::remove(cacheRoot + QStringLiteral("/") + f);
        qCInfo(logJava) << QStringLiteral("[Java安装] 清理残留临时文件: %1").arg(f);
    }

    // 2. 残缺版本目录（有 bin/java.exe 但校验失败，或目录非空但无有效 java）
    const QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& d : dirs) {
        bool ok = false;
        d.toInt(&ok);
        if (!ok) continue;  // 只处理数字版本目录
        const QString exe = cacheRoot + QStringLiteral("/") + d + QStringLiteral("/bin/java.exe");
        if (QFile::exists(exe)) {
            // 完整校验：版本匹配 + 核心文件齐全（防"-version 能跑但 lib 缺失"的残缺）
            const int major = verifyJavaMajor(exe);
            if (major == d.toInt() && isJavaComplete(exe))
                continue;  // 有效缓存，保留
        }
        // 目录存在但 java.exe 缺失或校验失败 → 半解压残留
        QDir sub(cacheRoot + QStringLiteral("/") + d);
        if (sub.isEmpty()) {
            sub.rmdir(cacheRoot + QStringLiteral("/") + d);  // 空目录
        } else {
            sub.removeRecursively();
            qCInfo(logJava) << QStringLiteral("[Java安装] 清理残缺 Java 缓存目录: %1").arg(d);
        }
    }
}

QString JavaRuntimeInstaller::detectCpuArch()
{
    // 1. QSysInfo 报告（Qt 编译目标架构，够用）
    QString arch = QSysInfo::currentCpuArchitecture();  // x86_64 / i686 / arm / arm64 ...

    // 2. Windows 下用 PROCESSOR_ARCHITECTURE 环境变量做权威检测
    //    （Qt 的 currentCpuArchitecture 在 32 位进程跑 64 位系统时会误报 x86）
#ifdef Q_OS_WIN
    QByteArray env = qgetenv("PROCESSOR_ARCHITECTURE");
    if (!env.isEmpty()) {
        const QString pa = QString::fromLatin1(env).toLower();
        if (pa.contains(QLatin1String("amd64")) || pa.contains(QLatin1String("x86_64")))
            return QStringLiteral("x64");
        if (pa.contains(QLatin1String("arm64")))
            return QStringLiteral("aarch64");
        if (pa.contains(QLatin1String("x86")))
            return QStringLiteral("x32");
        // PROCESSOR_ARCHITEW6432: 32 位进程跑 64 位系统
        QByteArray wow = qgetenv("PROCESSOR_ARCHITEW6432");
        if (wow.contains("AMD64") || wow.contains("arm64"))
            arch = QString::fromLatin1(wow).toLower();
    }
#endif

    if (arch == QLatin1String("x86_64") || arch == QLatin1String("amd64"))
        return QStringLiteral("x64");
    if (arch == QLatin1String("i386") || arch == QLatin1String("i486")
        || arch == QLatin1String("i586") || arch == QLatin1String("i686")
        || arch == QLatin1String("x86"))
        return QStringLiteral("x32");
    if (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"))
        return QStringLiteral("aarch64");
    if (arch == QLatin1String("arm"))
        return QStringLiteral("arm");
    return QStringLiteral("unknown");
}

// ============================================================
// 一键安装
// ============================================================

void JavaRuntimeInstaller::installRequiredJavas()
{
    if (m_running) {
        emit logMessage(tr("Java 安装已在运行中"));
        return;
    }
    m_running = true;
    m_cancelled = false;
    m_currentStep = 0;
    emit runningChanged();
    emit progressChanged();

    // ── 前置检测（异步完整扫描）→ 完成后自动进入安装流程 ──
    m_statusText = tr("正在扫描系统中已有的 Java...");
    emit progressChanged();
    if (m_scanThreadRunning) {
        // 扫描已在进行中（如设置页加载时触发过）——等待其完成回调
        m_waitingForScan = true;
        emit logMessage(tr("Java 扫描已在进行中，等待完成..."));
    } else {
        m_waitingForScan = true;
        scanSystemJavas();
    }
}

/// 扫描完成回调：执行实际安装
void JavaRuntimeInstaller::runInstallAfterScan()
{
    if (!m_running) {
        m_waitingForScan = false;
        return;  // 安装未在进行（纯 UI 刷新场景）
    }
    m_waitingForScan = false;
    const int detectedCount = m_detectedJavas.size();
    emit logMessage(tr("前置检测完成：系统中已有 %1 个 Java 运行时").arg(detectedCount));

    // 架构降级策略
    if (m_cpuArch == QLatin1String("aarch64")) {
        // Temurin 17/25 无 Windows ARM64（Adoptium API 实测 0 结果），
        // Win11 ARM64 通过 Prism x64 模拟运行
        emit logMessage(tr("当前系统为 ARM64，将安装 x64 版 Java（Windows 11 可通过模拟运行）"));
    } else if (m_cpuArch == QLatin1String("arm")) {
        emit finished(false, tr("当前系统为 32 位 ARM，暂不支持自动安装 Java。请手动安装 Java 后重试。"));
        m_running = false;
        emit runningChanged();
        return;
    } else if (m_cpuArch == QLatin1String("unknown")) {
        emit finished(false, tr("无法检测系统架构，请手动安装 Java。"));
        m_running = false;
        emit runningChanged();
        return;
    }

    // 安装清单：{版本, 类型, 标签}（成员 m_planItems，异步回调需要）
    m_planItems = {
        { 8,  QStringLiteral("jre"), QStringLiteral("Java 8 (JRE)") },
        { 17, QStringLiteral("jre"), QStringLiteral("Java 17 (JRE)") },
        { 25, QStringLiteral("jre"), QStringLiteral("Java 25 (JRE)") },
    };
    m_installedCount = 0;
    m_skippedCount = 0;

    // 异步串联安装三个版本（每个版本下载/解压/验证完成后才进入下一个）
    installNext(0);
}

/// 安装链：处理第 idx 个版本
void JavaRuntimeInstaller::installNext(int idx)
{
    if (m_cancelled) {
        emit finished(false, tr("Java 安装已取消"));
        m_running = false;
        emit runningChanged();
        return;
    }
    if (idx >= m_planItems.size()) {
        m_statusText = tr("全部 Java 就绪（已安装 %1 个，跳过 %2 个）")
                           .arg(m_installedCount).arg(m_skippedCount);
        emit progressChanged();
        emit finished(true, {});
        m_running = false;
        emit runningChanged();
        return;
    }

    const PlanItem& item = m_planItems[idx];
    m_currentStep = idx + 1;
    m_retriedThisRound = false;
    m_lastFailedMajor = item.major;
    emit progressChanged();

    const bool isJdkTarget = (item.type == QLatin1String("jdk"));
    const QString cacheExe = QCoreApplication::applicationDirPath()
        + QStringLiteral("/java_cache/%1/bin/java.exe").arg(item.major);

    // ── 前置检测跳过逻辑 ──
    // 规则：已有同 major 任意类型（JRE/JDK）→ 不需要重复安装。
    // 理由（实际应用场景）：
    //   - 游戏运行需要 JRE，JDK 是超集但运行场景不需要额外工具链
    //   - 用户已装 Java 17 JRE → 游戏/Forge 安装器都能跑，无需再装 17 JDK
    //   - 只有该 major 完全缺失时才按预设类型安装
    if (!isRequired(item.major, isJdkTarget)) {
        m_statusText = tr("已检测到 %1，跳过").arg(existingJavaLabel(item.major));
        emit progressChanged();
        emit javaInstalled(item.label, existingJavaPath(item.major), true);
        m_skippedCount++;
        installNext(idx + 1);
        return;
    }

    // 启动器自身缓存已有（版本 + 完整性校验通过）→ 跳过
    if (QFile::exists(cacheExe)
        && verifyJavaMajor(cacheExe) == item.major
        && isJavaComplete(cacheExe)) {
        qCInfo(logJava) << QStringLiteral("[Java安装] %1 已存在于缓存，跳过").arg(item.label);
        m_statusText = tr("%1 已在启动器缓存中，跳过").arg(item.label);
        emit progressChanged();
        emit javaInstalled(item.label, cacheExe, true);
        m_skippedCount++;
        installNext(idx + 1);
        return;
    }

    m_statusText = tr("正在安装 %1...").arg(item.label);
    emit progressChanged();

    installJavaAsync(item.major, item.type,
        [this, idx](bool ok, const QString& error, const QString& exe) {
            onInstallDone(idx, ok, error, exe);
        });
}

/// 单个版本安装完成回调（含自动重试）
void JavaRuntimeInstaller::onInstallDone(int idx, bool ok, const QString& error, const QString& exe)
{
    const PlanItem item = m_planItems[idx];  // 值拷贝（retry lambda 捕获用）
    if (!ok) {
        // 网络抖动自动重试一次（非取消场景）
        if (!m_cancelled && !m_retriedThisRound) {
            m_retriedThisRound = true;
            m_statusText = tr("%1 安装失败，正在自动重试...").arg(item.label);
            emit progressChanged();
            emit logMessage(tr("%1 安装失败（%2），正在自动重试...").arg(item.label, error));
            // 延迟 800ms 后重试（给网络/锁释放时间）
            QTimer::singleShot(800, this, [this, idx, item]() {
                installJavaAsync(item.major, item.type,
                    [this, idx](bool ok2, const QString& err2, const QString& exe2) {
                        onInstallDone(idx, ok2, err2, exe2);
                    });
            });
            return;
        }
        if (m_cancelled) {
            emit finished(false, tr("Java 安装已取消"));
        } else {
            emit finished(false, error.isEmpty()
                ? tr("%1 安装失败，请检查网络后重试").arg(item.label)
                : tr("%1 安装失败: %2").arg(item.label, error));
        }
        m_running = false;
        emit runningChanged();
        return;
    }
    emit javaInstalled(item.label, exe, false);
    m_installedCount++;
    installNext(idx + 1);
}

void JavaRuntimeInstaller::cancelInstall()
{
    m_cancelled = true;
    // 立即中止进行中的下载（如果有）
    if (m_job.dlHandle) {
        m_job.dlHandle->abort();
        m_job.dlHandle = nullptr;
    }
    emit logMessage(tr("Java 安装取消请求已发送（当前版本下载完成后停止）"));
}

// ============================================================
// 前置检测：扫描系统已有 Java
// ============================================================

QVariantList JavaRuntimeInstaller::detectedSystemJavas() const
{
    return m_detectedJavas;
}

QVariantList JavaRuntimeInstaller::scanSystemJavas()
{
    // 完整扫描在后台线程执行，避免阻塞 UI（递归扫目录可能耗时数百 ms）
    if (m_scanThreadRunning)
        return m_detectedJavas;
    m_scanThreadRunning = true;

    std::thread([this]() {
        QVariantList results;
        QSet<QString> seen;

        auto collect = [&](const QString& exePath) {
            if (exePath.isEmpty()) return;
            const QFileInfo fi(exePath);
            if (!fi.isFile()) return;
            const QString binDir = QDir::cleanPath(fi.absolutePath()).toLower();
            if (seen.contains(binDir) || isSpecialPath(binDir)) return;

            // 版本解析
            QProcess proc;
            proc.start(exePath, { QStringLiteral("-version") });
            if (!proc.waitForFinished(8000) || proc.exitCode() != 0)
                return;
            const QString output = QString::fromUtf8(proc.readAllStandardError());
            static const QRegularExpression verRe(QStringLiteral("version\\s+\"([^\"]+)\""));
            auto m = verRe.match(output);
            if (!m.hasMatch()) return;
            QString ver = m.captured(1);
            ver.replace(QLatin1Char('_'), QLatin1Char('.'));
            const QStringList parts = ver.split(QLatin1Char('.'));
            if (parts.isEmpty()) return;
            int major = parts[0].toInt();
            if (major == 1 && parts.size() > 1)
                major = parts[1].toInt();  // 1.8.0 → 8
            if (major <= 0) return;

            // 完整性校验：-version 能跑 ≠ 完整（bin 解压了但 lib/modules 缺失时
            // 照样能输出版本号，但实际无法运行）。残缺 Java 不列入检测列表，
            // 视为缺失 → 会重新安装。
            if (!isJavaComplete(exePath)) {
                qCInfo(logJava) << QStringLiteral("[Java安装] 检测到残缺 Java（忽略）: %1").arg(exePath);
                return;
            }

            // JDK 判定：同目录存在 javac.exe
            const bool isJdk = QFile::exists(QDir(fi.absolutePath()).filePath(QStringLiteral("javac.exe")));
            seen.insert(binDir);

            QVariantMap entry;
            entry[QStringLiteral("major")] = major;
            entry[QStringLiteral("version")] = ver;
            entry[QStringLiteral("path")] = exePath;
            entry[QStringLiteral("isJdk")] = isJdk;
            results.append(entry);
            qCInfo(logJava) << QStringLiteral("[Java安装] 检测到 Java %1 %2: %3")
                                   .arg(major).arg(isJdk ? QStringLiteral("JDK") : QStringLiteral("JRE")).arg(exePath);
        };

        auto scanDir = [&](const QString& dir, int depth) {
            auto impl = [&](auto&& self, const QString& d, int cur) -> void {
                if (cur > depth) return;
                QDir qd(d);
                if (!qd.exists()) return;
                // 本目录标准布局 bin/java.exe
                const QString direct = d + QStringLiteral("/bin/java.exe");
                if (QFile::exists(direct)) collect(direct);
                const QString flat = d + QStringLiteral("/java.exe");
                if (QFile::exists(flat)) collect(flat);
                // 递归子目录
                const auto entries = qd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
                for (const auto& entry : entries) {
                    if (entry.isSymLink() || entry.isShortcut()) continue;
                    const QString lower = entry.fileName().toLower();
                    if (lower == QLatin1String("system32") || lower == QLatin1String("syswow64")
                        || lower == QLatin1String("windows"))
                        continue;
                    self(self, entry.absoluteFilePath(), cur + 1);
                }
            };
            impl(impl, dir, 0);
        };

        // 1. JAVA_HOME / JDK_HOME
        const QString javaHome = qEnvironmentVariable("JAVA_HOME");
        if (!javaHome.isEmpty()) scanDir(javaHome, 3);
        const QString jdkHome = qEnvironmentVariable("JDK_HOME");
        if (!jdkHome.isEmpty() && jdkHome != javaHome) scanDir(jdkHome, 3);

        // 2. PATH (where java)
        {
            QProcess proc;
            proc.start(QStringLiteral("where"), { QStringLiteral("java") });
            if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
                const QStringList lines = QString::fromLocal8Bit(proc.readAllStandardOutput())
                                              .split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts);
                for (const QString& line : lines)
                    collect(QDir::cleanPath(line.trimmed()));
            }
        }

        // 3. 启动器自身缓存 java_cache/*/bin/java.exe
        {
            const QString cacheRoot = QCoreApplication::applicationDirPath() + QStringLiteral("/java_cache");
            QDir cd(cacheRoot);
            if (cd.exists()) {
                for (const QString& sub : cd.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
                    scanDir(cacheRoot + QStringLiteral("/") + sub, 2);
            }
        }

        // 4. 常见安装目录（递归完整扫描，深度 5）
        {
            static const QStringList commonRoots = {
                QStringLiteral("C:/Program Files/Java"),
                QStringLiteral("C:/Program Files/Eclipse Adoptium"),
                QStringLiteral("C:/Program Files/Adoptium"),
                QStringLiteral("C:/Program Files/Amazon Corretto"),
                QStringLiteral("C:/Program Files/Microsoft"),
                QStringLiteral("C:/Program Files/Zulu"),
                QStringLiteral("C:/Program Files (x86)/Java"),
                QStringLiteral("C:/Program Files (x86)/Eclipse Adoptium"),
                QStringLiteral("C:/Program Files (x86)/Adoptium"),
                QStringLiteral("C:/Program Files (x86)/Zulu"),
                QStringLiteral("%USERPROFILE%/.jdks"),
                // MC 官方 runtime（自建 .minecraft 与官方启动器都会装；jre-legacy/jre-x64 等）
                QStringLiteral("%APPDATA%/.minecraft/runtime"),
                QStringLiteral("%USERPROFILE%/AppData/Roaming/.minecraft/runtime"),
                QStringLiteral("C:/Program Files (x86)/Minecraft Launcher/runtime"),
                QStringLiteral("C:/Program Files (x86)/Minecraft/runtime"),
            };
            for (const QString& raw : commonRoots) {
                QString root = raw;
                if (root.startsWith(QLatin1Char('%'))) {
                    const int end = root.indexOf(QLatin1Char('%'), 1);
                    if (end > 1) {
                        const QString var = root.mid(1, end - 1);
                        const QString val = qEnvironmentVariable(var.toLocal8Bit());
                        if (val.isEmpty()) continue;
                        root = val + root.mid(end + 1);
                    }
                }
                scanDir(root, 5);
            }
        }

        // 5. Windows 注册表（JDK/JRE JavaHome）
        {
#ifdef Q_OS_WIN
            const wchar_t* regPaths[] = {
                L"SOFTWARE\\JavaSoft\\JDK",
                L"SOFTWARE\\JavaSoft\\Java Runtime Environment",
                L"SOFTWARE\\Eclipse Adoptium\\JDK",
                L"SOFTWARE\\Eclipse Foundation\\JDK",
                L"SOFTWARE\\Microsoft\\JDK",
            };
            for (const auto* rp : regPaths) {
                HKEY hKey;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, rp, 0,
                                  KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
                    continue;
                DWORD idx = 0;
                wchar_t name[256];
                DWORD nameLen = 256;
                while (RegEnumKeyExW(hKey, idx, name, &nameLen,
                                     nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    HKEY hSub;
                    if (RegOpenKeyExW(hKey, name, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                        wchar_t home[512];
                        DWORD sz = sizeof(home), type;
                        if (RegQueryValueExW(hSub, L"JavaHome", nullptr, &type,
                                             reinterpret_cast<BYTE*>(home), &sz) == ERROR_SUCCESS
                            && type == REG_SZ) {
                            scanDir(QString::fromWCharArray(home), 3);
                        }
                        RegCloseKey(hSub);
                    }
                    ++idx;
                    nameLen = 256;
                }
                RegCloseKey(hKey);
            }
#endif
        }

        // 回到主线程发布结果
        QMetaObject::invokeMethod(this, [this, results]() {
            m_detectedJavas = results;
            m_seenBinDirs.clear();
            m_scanThreadRunning = false;
            qCInfo(logJava) << QStringLiteral("[Java安装] 前置检测完成 共检测到 %1 个 Java")
                                   .arg(results.size());
            emit systemJavaScanFinished();
        }, Qt::QueuedConnection);
    }).detach();

    return m_detectedJavas;
}

bool JavaRuntimeInstaller::isSpecialPath(const QString& binDir)
{
    const QString lower = binDir.toLower();
    return lower.contains(QLatin1String("javapath_target_"))
        || lower.contains(QLatin1String("java8path_target_"))
        || lower.contains(QLatin1String("javatmp"))
        || lower.contains(QLatin1String("system32"))
        || lower.contains(QLatin1String("syswow64"));
}

bool JavaRuntimeInstaller::isRequired(int major, bool targetIsJdk) const
{
    Q_UNUSED(targetIsJdk)
    // 规则：已有同 major 任意类型 → 不需要。
    // JRE 已满足运行场景（游戏/Forge 安装器都只是跑 jar），
    // 只有该 major 完全缺失时才安装。
    for (const auto& entry : m_detectedJavas) {
        if (entry.toMap().value(QStringLiteral("major")).toInt() == major)
            return false;
    }
    return true;
}

/// 已检测到的同 major Java 的显示名（"Java 17 (JDK)"）
static QString existingJavaLabelFor(const QVariantList& list, int major)
{
    for (const auto& entry : list) {
        const QVariantMap e = entry.toMap();
        if (e.value(QStringLiteral("major")).toInt() == major) {
            return QStringLiteral("Java %1 (%2)")
                .arg(major)
                .arg(e.value(QStringLiteral("isJdk")).toBool() ? QStringLiteral("JDK") : QStringLiteral("JRE"));
        }
    }
    return QStringLiteral("Java %1").arg(major);
}

QString JavaRuntimeInstaller::existingJavaLabel(int major) const
{
    return existingJavaLabelFor(m_detectedJavas, major);
}

QString JavaRuntimeInstaller::existingJavaPath(int major) const
{
    for (const auto& entry : m_detectedJavas) {
        const QVariantMap e = entry.toMap();
        if (e.value(QStringLiteral("major")).toInt() == major)
            return e.value(QStringLiteral("path")).toString();
    }
    return {};
}

// ============================================================
// 单版本异步安装（状态机：列目录 → 下载 → 解压 → 验证）
// ============================================================

void JavaRuntimeInstaller::installJavaAsync(int majorVersion, const QString& type,
                                            std::function<void(bool, const QString&, const QString&)> onDone)
{
    m_job = InstallJob{};
    m_job.major = majorVersion;
    m_job.type = type;
    m_job.retryCount = 0;
    m_job.javaDir = QCoreApplication::applicationDirPath() + QStringLiteral("/java_cache/%1").arg(majorVersion);
    m_job.javaExe = m_job.javaDir + QStringLiteral("/bin/java.exe");
    m_job.zipPath = QCoreApplication::applicationDirPath()
        + QStringLiteral("/java_cache/.tmp-jdk-%1.zip").arg(majorVersion);
    m_job.onDone = std::move(onDone);

    // 已存在（完整校验通过：版本正确 + 核心文件齐全）→ 直接完成
    if (QFile::exists(m_job.javaExe)
        && verifyJavaMajor(m_job.javaExe) == m_job.major
        && isJavaComplete(m_job.javaExe)) {
        finishJob(m_job.javaExe);
        return;
    }
    // 存在但残缺/版本不符（上次解压中断残留：bin 解压了但 lib/modules 缺失）→ 删除重装
    if (QFile::exists(m_job.javaExe)) {
        qCWarning(logJava) << QStringLiteral("[Java安装] %1 缓存残缺（版本校验或完整性校验失败），删除重装")
                                  .arg(m_job.major);
        QDir(m_job.javaDir).removeRecursively();
    }

    // 并发锁
    QDir().mkpath(QCoreApplication::applicationDirPath() + QStringLiteral("/java_cache"));
    m_job.lock = new QLockFile(QCoreApplication::applicationDirPath()
        + QStringLiteral("/java_cache/jdk-%1.lock").arg(majorVersion));
    m_job.lock->setStaleLockTime(600000);
    if (!m_job.lock->tryLock(1000)) {
        failJob(tr("Java %1 下载被其他进程锁定").arg(majorVersion));
        return;
    }
    // 锁后复查：其他进程可能已装好（完整校验）
    if (QFile::exists(m_job.javaExe)
        && verifyJavaMajor(m_job.javaExe) == m_job.major
        && isJavaComplete(m_job.javaExe)) {
        finishJob(m_job.javaExe);
        return;
    }
    if (QFile::exists(m_job.javaExe)) {
        // 残缺（其他进程下载中断）→ 清理后继续自己装
        QDir(m_job.javaDir).removeRecursively();
    }

    stepFetchZipList();
}

void JavaRuntimeInstaller::stepFetchZipList()
{
    // 架构降级：ARM64 → x64（Prism 模拟）
    QString arch = m_cpuArch;
    if (arch == QLatin1String("aarch64"))
        arch = QStringLiteral("x64");

    const QString mirrorBase = QStringLiteral("https://mirrors.tuna.tsinghua.edu.cn/Adoptium/%1/%2/%3/windows/")
                                   .arg(m_job.major).arg(m_job.type, arch);

    QNetworkAccessManager* nam = HttpClient::instance().manager();
    QNetworkRequest req{QUrl(mirrorBase)};
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    QNetworkReply* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mirrorBase]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failJob(tr("获取 Java %1 文件列表失败: %2").arg(m_job.major).arg(reply->errorString()));
            return;
        }
        const QString html = QString::fromUtf8(reply->readAll());
        static const QRegularExpression zipRe(QLatin1String("<a href=\"([^\"]+\\.zip)\""));
        QStringList zipUrls;
        auto it = zipRe.globalMatch(html);
        while (it.hasNext())
            zipUrls.append(mirrorBase + it.next().captured(1));
        if (zipUrls.isEmpty()) {
            failJob(tr("镜像中未找到 Java %1 (%2) 的 .zip 文件").arg(m_job.major).arg(m_job.type));
            return;
        }
        m_job.zipUrl = zipUrls.last();  // 字母序最新
        stepDownloadZip();
    });
}

void JavaRuntimeInstaller::stepDownloadZip()
{
    m_statusText = tr("正在下载 Java %1...").arg(m_job.major);
    emit progressChanged();

    // 进度复位
    m_dlPercent = 0; m_dlBytes = 0; m_dlTotal = 0; m_dlSpeedMBps = 0.0;
    m_dlLastBytes = 0; m_dlTimer.start();
    emit downloadProgressChanged();

    QFile::remove(m_job.zipPath);

    m_job.dlHandle = HttpClient::instance().downloadWithReply(
        m_job.zipUrl, m_job.zipPath,
        [this](qint64 received, qint64 total) {
            m_dlBytes = received;
            m_dlTotal = total;
            m_dlPercent = total > 0 ? qMin(100, static_cast<int>(received * 100 / total)) : 0;
            // 速度：每 500ms 采样一次
            const qint64 elapsedMs = m_dlTimer.elapsed();
            if (elapsedMs >= 500) {
                const qint64 delta = received - m_dlLastBytes;
                m_dlSpeedMBps = (delta * 1000.0) / (elapsedMs * 1024.0 * 1024.0);
                m_dlLastBytes = received;
                m_dlTimer.restart();
            }
            emit downloadProgressChanged();
        },
        [this](bool ok, const QString& err) {
            if (!ok) {
                failJob(tr("下载 Java %1 失败: %2").arg(m_job.major).arg(err));
                return;
            }
            stepExtractZip();
        });
}

void JavaRuntimeInstaller::stepExtractZip()
{
    m_dlPercent = 100;
    emit downloadProgressChanged();

    m_statusText = tr("正在解压 Java %1...").arg(m_job.major);
    emit progressChanged();

    // 解压是 CPU+IO 密集操作（40-180MB、数百文件），必须在后台线程执行，
    // 否则 UI 线程阻塞 → 转圈/进度条冻结（表现为"卡顿"）。
    const QString zipPath = m_job.zipPath;
    const QString javaDir = m_job.javaDir;
    const QString javaExe = m_job.javaExe;
    const int major = m_job.major;

    std::thread([this, zipPath, javaDir, javaExe, major]() {
        // ── 后台线程：读 zip + 解压 + 剥离顶层目录 ──
        QFile zipFile(zipPath);
        if (!zipFile.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [this, major]() {
                failJob(tr("无法读取 Java %1 压缩包").arg(major));
            }, Qt::QueuedConnection);
            return;
        }
        const QByteArray zipData = zipFile.readAll();
        zipFile.close();
        QFile::remove(zipPath);

        QBuffer buf;
        buf.setData(zipData);
        if (!buf.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [this, major]() {
                failJob(tr("Java %1 压缩包损坏").arg(major));
            }, Qt::QueuedConnection);
            return;
        }

        QZipReader reader(&buf);
        const QList<QZipReader::FileInfo> entries = reader.fileInfoList();

        // 剥离顶层目录（Adoptium zip 均带一层 jdk-x.x.x+/ 或 jre-x.x.x+/）
        QString commonPrefix;
        int nonDir = 0;
        for (const auto& entry : entries) {
            if (entry.isDir) continue;
            ++nonDir;
            const QString fp = entry.filePath;
            const int slash = fp.indexOf(QLatin1Char('/'));
            const QString top = (slash < 0) ? fp : fp.left(slash);
            if (commonPrefix.isEmpty()) commonPrefix = top;
            else if (commonPrefix != top) { commonPrefix.clear(); break; }
        }
        if (nonDir > 0 && !commonPrefix.isEmpty())
            qCInfo(logJava) << QStringLiteral("[Java安装] %1 ZIP 顶层目录: %2，剥离").arg(major).arg(commonPrefix);

        int extracted = 0;
        for (const auto& entry : entries) {
            if (entry.isDir || entry.isSymLink) continue;
            QString rel = entry.filePath;
            if (!commonPrefix.isEmpty()) {
                if (rel.startsWith(commonPrefix + QLatin1Char('/')))
                    rel = rel.mid(commonPrefix.length() + 1);
                else if (rel == commonPrefix)
                    continue;
            }
            const QString outPath = javaDir + QStringLiteral("/") + rel;
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            QFile out(outPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(reader.fileData(entry.filePath));
                out.close();
                extracted++;
            }
        }
        reader.close();
        qCInfo(logJava) << QStringLiteral("[Java安装] %1 解压完成: %2 个文件").arg(major).arg(extracted);

        // ── 回主线程：验证 + 完成/失败 ──
        QMetaObject::invokeMethod(this, [this, javaExe, javaDir, major]() {
            // 验证
            QString exe = javaExe;
            if (!QFile::exists(exe)) {
                const QString found = findJavaExeRecursive(javaDir);
                if (found.isEmpty()) {
                    failJob(tr("Java %1 解压后未找到 java.exe").arg(major));
                    return;
                }
                exe = found;
            }
            const int realMajor = verifyJavaMajor(exe);
            if (realMajor != major) {
                failJob(tr("Java %1 版本校验失败（实际 %2）").arg(major).arg(realMajor));
                return;
            }
            // 完整性校验：核心文件必须齐全（lib/modules、jvm.dll 等）
            if (!isJavaComplete(exe)) {
                failJob(tr("Java %1 解压不完整（缺少核心文件），请重试").arg(major));
                return;
            }

            qCInfo(logJava) << QStringLiteral("[Java安装] Java %1 已就绪: %2").arg(major).arg(exe);
            finishJob(exe);
        }, Qt::QueuedConnection);
    }).detach();
}

void JavaRuntimeInstaller::failJob(const QString& error)
{
    qCWarning(logJava) << QStringLiteral("[Java安装] 安装失败: %1").arg(error);
    if (m_job.dlHandle) {
        m_job.dlHandle->abort();
        m_job.dlHandle = nullptr;
    }
    // 清理残留：临时 zip + 半解压目录（避免下次检测到残缺 java.exe 误判"已安装"）
    QFile::remove(m_job.zipPath);
    if (!m_job.javaDir.isEmpty()) {
        QDir(m_job.javaDir).removeRecursively();
        qCInfo(logJava) << QStringLiteral("[Java安装] 已清理残留目录: %1").arg(m_job.javaDir);
    }
    if (m_job.lock) {
        m_job.lock->unlock();
        delete m_job.lock;
        m_job.lock = nullptr;
    }
    auto cb = std::move(m_job.onDone);
    m_job = InstallJob{};
    if (cb) cb(false, error, {});
}

void JavaRuntimeInstaller::finishJob(const QString& javaExe)
{
    if (m_job.dlHandle) {
        m_job.dlHandle->abort();
        m_job.dlHandle = nullptr;
    }
    QFile::remove(m_job.zipPath);
    if (m_job.lock) {
        m_job.lock->unlock();
        delete m_job.lock;
        m_job.lock = nullptr;
    }
    auto cb = std::move(m_job.onDone);
    const QString exeCopy = javaExe;  // 拷贝：m_job 即将被清空
    m_job = InstallJob{};
    if (cb) cb(true, {}, exeCopy);
}

// ============================================================
// 工具
// ============================================================

int JavaRuntimeInstaller::verifyJavaMajor(const QString& javaExe)
{
    QProcess proc;
    proc.start(javaExe, { QStringLiteral("-version") });
    if (!proc.waitForFinished(8000))
        return 0;
    const QString output = QString::fromUtf8(proc.readAllStandardError());
    static const QRegularExpression re(QStringLiteral("version\\s+\"([^\"]+)\""));
    auto m = re.match(output);
    if (!m.hasMatch()) return 0;
    QString ver = m.captured(1);
    ver.replace(QLatin1Char('_'), QLatin1Char('.'));
    const QStringList parts = ver.split(QLatin1Char('.'));
    if (parts.isEmpty()) return 0;
    int major = parts[0].toInt();
    if (major == 1 && parts.size() > 1)
        major = parts[1].toInt();  // 1.8.0 → 8
    return major;
}

/// 校验 Java 安装是否完整（-version 能跑 ≠ 完整：bin 解压了但 lib 缺失时
/// -version 照样能输出版本号，但实际启动游戏/安装器会失败）。
/// 检查运行必需的核心文件：
///   - Java 9+：lib/modules（模块镜像，缺失则 JVM 无法加载任何类）
///   - Java 8：lib/rt.jar（核心运行时库）
///   - 通用：bin/server/jvm.dll（JVM 本体）、release（版本标识）
bool JavaRuntimeInstaller::isJavaComplete(const QString& javaExe)
{
    const QString binDir = QFileInfo(javaExe).absolutePath();  // .../bin
    const QString base = QDir(binDir).absolutePath();           // .../bin
    // 根目录 = bin 的上一级（{root}/bin/java.exe → {root}）
    QDir baseDir(base);
    const QString root = baseDir.filePath(QStringLiteral(".."));

    // 1. JVM 本体 DLL（Windows）
#ifdef Q_OS_WIN
    const QString jvmDll = binDir + QStringLiteral("/server/jvm.dll");
    if (!QFile::exists(jvmDll))
        return false;
#endif

    // 2. release 文件（所有 Java 8+ 都有）
    if (!QFile::exists(root + QStringLiteral("/release")))
        return false;

    // 3. 模块镜像 / 核心运行时库（区分 Java 8 与 9+）
    const int major = verifyJavaMajor(javaExe);
    if (major <= 0)
        return false;
    if (major >= 9) {
        // Java 9+：lib/modules 是模块镜像，缺失则 JVM 无法加载任何类
        return QFile::exists(root + QStringLiteral("/lib/modules"));
    }
    // Java 8：rt.jar 在 {root}/lib/rt.jar 或 {root}/jre/lib/rt.jar
    return QFile::exists(root + QStringLiteral("/lib/rt.jar"))
        || QFile::exists(root + QStringLiteral("/jre/lib/rt.jar"));
}

QString JavaRuntimeInstaller::findJavaExeRecursive(const QString& dir)
{
    const QString direct = dir + QStringLiteral("/bin/java.exe");
    if (QFile::exists(direct)) return direct;
    QDir d(dir);
    if (!d.exists()) return {};
    const QStringList subdirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sub : subdirs) {
        const QString found = findJavaExeRecursive(dir + QStringLiteral("/") + sub);
        if (!found.isEmpty()) return found;
    }
    return {};
}

} // namespace ShadowLauncher
