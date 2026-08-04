// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "java_runtime_installer.h"

#include "http_client.h"
#include "../utils/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
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

    // 安装清单：{版本, 类型, 标签}
    struct Item { int major; QString type; QString label; };
    const QList<Item> items = {
        { 8,  QStringLiteral("jre"), QStringLiteral("Java 8 (JRE)") },
        { 17, QStringLiteral("jdk"), QStringLiteral("Java 17 (JDK)") },
        { 25, QStringLiteral("jdk"), QStringLiteral("Java 25 (JDK)") },
    };

    int installedCount = 0, skippedCount = 0;
    for (const Item& item : items) {
        if (m_cancelled) {
            emit finished(false, tr("Java 安装已取消"));
            m_running = false;
            emit runningChanged();
            return;
        }
        m_currentStep++;
        emit progressChanged();

        const bool isJdkTarget = (item.type == QLatin1String("jdk"));
        const QString cacheExe = QDir::currentPath()
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
            skippedCount++;
            continue;
        }

        // 启动器自身缓存已有（版本校验通过）→ 跳过
        if (QFile::exists(cacheExe) && verifyJavaMajor(cacheExe) == item.major) {
            qCInfo(logJava) << QStringLiteral("[Java安装] %1 已存在于缓存，跳过").arg(item.label);
            m_statusText = tr("%1 已在启动器缓存中，跳过").arg(item.label);
            emit progressChanged();
            emit javaInstalled(item.label, cacheExe, true);
            skippedCount++;
            continue;
        }

        m_statusText = tr("正在安装 %1...").arg(item.label);
        emit progressChanged();

        const QString exe = installJava(item.major, item.type);
        if (exe.isEmpty()) {
            // 下载/解压失败 — 检查是否因取消
            if (m_cancelled) {
                emit finished(false, tr("Java 安装已取消"));
            } else {
                emit finished(false, tr("%1 安装失败，请检查网络后重试").arg(item.label));
            }
            m_running = false;
            emit runningChanged();
            return;
        }
        emit javaInstalled(item.label, exe, false);
        installedCount++;
    }

    m_statusText = tr("全部 Java 就绪（已安装 %1 个，跳过 %2 个）")
                       .arg(installedCount).arg(skippedCount);
    emit progressChanged();
    emit finished(true, {});
    m_running = false;
    emit runningChanged();
}

void JavaRuntimeInstaller::cancelInstall()
{
    m_cancelled = true;
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
                                              .split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
                for (const QString& line : lines)
                    collect(QDir::cleanPath(line.trimmed()));
            }
        }

        // 3. 启动器自身缓存 java_cache/*/bin/java.exe
        {
            const QString cacheRoot = QDir::currentPath() + QStringLiteral("/java_cache");
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
// 单版本安装
// ============================================================

QString JavaRuntimeInstaller::installJava(int majorVersion, const QString& type)
{
    // 架构降级：ARM64 → x64（Prism 模拟）
    QString arch = m_cpuArch;
    if (arch == QLatin1String("aarch64"))
        arch = QStringLiteral("x64");

    return downloadAndExtract(majorVersion, type, arch);
}

// ============================================================
// 下载 + 解压（Tuna Adoptium 镜像）
// ============================================================

QString JavaRuntimeInstaller::downloadAndExtract(int majorVersion, const QString& type, const QString& arch)
{
    const QString baseDir = QDir::currentPath() + QStringLiteral("/java_cache/");
    const QString javaDir = baseDir + QString::number(majorVersion);
    const QString javaExe = javaDir + QStringLiteral("/bin/java.exe");

    // 已存在（下载中/已完成）
    if (QFile::exists(javaExe)) {
        qCInfo(logJava) << QStringLiteral("[Java安装] %1 已存在于缓存").arg(majorVersion);
        return javaExe;
    }

    // 并发锁
    QDir().mkpath(baseDir);
    QLockFile lockFile(baseDir + QStringLiteral("/jdk-%1.lock").arg(majorVersion));
    lockFile.setStaleLockTime(600000);  // 10 min
    if (!lockFile.tryLock(30000)) {
        qCWarning(logJava) << QStringLiteral("[Java安装] %1 下载被其他进程锁定").arg(majorVersion);
        return {};
    }
    if (QFile::exists(javaExe)) {
        qCInfo(logJava) << QStringLiteral("[Java安装] %1 已被其他进程下载").arg(majorVersion);
        return javaExe;
    }

    // Tuna Adoptium 目录结构: {base}/{version}/{type}/{arch}/windows/
    const QString mirrorBase = QStringLiteral("https://mirrors.tuna.tsinghua.edu.cn/Adoptium/%1/%2/%3/windows/")
                                   .arg(majorVersion).arg(type, arch);

    QNetworkAccessManager* nam = HttpClient::instance().manager();
    const auto fetchText = [nam](const QUrl& url, int timeoutMs, QByteArray* out) -> bool {
        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        QNetworkReply* reply = nam->get(req);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();
        const bool ok = (reply->error() == QNetworkReply::NoError) && timer.isActive();
        if (ok) *out = reply->readAll();
        reply->deleteLater();
        return ok;
    };

    // 1. 列目录，找 .zip
    QByteArray html;
    if (!fetchText(QUrl(mirrorBase), 15000, &html)) {
        qCWarning(logJava) << QStringLiteral("[Java安装] 获取 %1 文件列表失败").arg(mirrorBase);
        return {};
    }
    static const QRegularExpression zipRe(QLatin1String("<a href=\"([^\"]+\\.zip)\""));
    QStringList zipUrls;
    auto it = zipRe.globalMatch(QString::fromUtf8(html));
    while (it.hasNext()) {
        auto m = it.next();
        zipUrls.append(mirrorBase + m.captured(1));
    }
    if (zipUrls.isEmpty()) {
        qCWarning(logJava) << QStringLiteral("[Java安装] 镜像中未找到 %1 %2 %3 的 .zip 文件")
                                  .arg(majorVersion).arg(type, arch);
        return {};
    }
    // 取最新（Tuna 按字母序，最新版通常排最后）
    const QString zipUrl = zipUrls.last();

    // 2. 下载 ZIP（整体读入内存，与 ModLoaderInstaller 同策略）
    m_statusText = tr("正在下载 Java %1 (%2)...").arg(majorVersion).arg(arch);
    emit progressChanged();
    QByteArray zipData;
    if (!fetchText(QUrl(zipUrl), 600000, &zipData)) {
        qCWarning(logJava) << QStringLiteral("[Java安装] 下载 %1 ZIP 失败").arg(majorVersion);
        return {};
    }

    // 3. 解压（剥离顶层 jdk-x.x.x+/ 目录）
    m_statusText = tr("正在解压 Java %1...").arg(majorVersion);
    emit progressChanged();
    QBuffer buf;
    buf.setData(zipData);
    if (!buf.open(QIODevice::ReadOnly)) {
        qCWarning(logJava) << QStringLiteral("[Java安装] 打开 ZIP 缓冲失败");
        return {};
    }
    {
        QZipReader reader(&buf);
        const QList<QZipReader::FileInfo> entries = reader.fileInfoList();

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
            qCInfo(logJava) << QStringLiteral("[Java安装] %1 ZIP 顶层目录: %2，剥离").arg(majorVersion).arg(commonPrefix);

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
            QString outPath = javaDir + QStringLiteral("/") + rel;
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            QFile out(outPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(reader.fileData(entry.filePath));
                out.close();
                extracted++;
            }
        }
        reader.close();
        qCInfo(logJava) << QStringLiteral("[Java安装] %1 解压完成: %2 个文件").arg(majorVersion).arg(extracted);
    }

    if (!QFile::exists(javaExe)) {
        const QString found = findJavaExeRecursive(javaDir);
        if (found.isEmpty()) {
            qCWarning(logJava) << QStringLiteral("[Java安装] %1 解压后未找到 java.exe").arg(majorVersion);
            return {};
        }
        return found;
    }

    // 4. 验证
    const int realMajor = verifyJavaMajor(javaExe);
    if (realMajor != majorVersion) {
        qCWarning(logJava) << QStringLiteral("[Java安装] %1 版本校验失败 实际=%2").arg(majorVersion).arg(realMajor);
        return {};
    }
    qCInfo(logJava) << QStringLiteral("[Java安装] Java %1 已就绪: %2").arg(majorVersion).arg(javaExe);
    return javaExe;
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
