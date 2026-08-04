// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QVariantList>
#include <QElapsedTimer>
#include <QLockFile>
#include <functional>

#include "http_client.h"

namespace ShadowLauncher {

/// 一键安装所需 Java 运行时（设置-关于页入口）。
///
/// 从 Tuna Adoptium 镜像下载 ZIP 并解压到 java_cache/{major}/（便携式，
/// 不写注册表，与 主流启动器/主流启动器 一致）。装完后被 ModLoaderInstaller::findJavaPath
/// 自动发现（其第 2 步扫描 java_cache/{major}/bin/java.exe）。
///
/// 版本策略（2026-08-04 实测确认）：
///   - Java 8 / 17 / 25 全部安装 JRE（Tuna 镜像均有 JRE 构建；
///     游戏运行与 Forge/NeoForge 安装都只需 JRE——安装器全程 java -cp 跑 jar 无 javac 调用）
///   - JRE 体积约为 JDK 的 1/4
/// 架构策略（Windows）：
///   - x64 → x64；x86 → x32（Tuna 目录名）；ARM64 → x64（Win11 Prism 模拟运行，
///     实测 Temurin 17/25 无 Windows ARM64 构建，Adoptium API 0 结果确认）
///   - arm32 → 不支持
class JavaRuntimeInstaller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString cpuArch READ cpuArch CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY progressChanged)
    Q_PROPERTY(int totalSteps READ totalSteps CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)
    // ── 下载进度（异步下载期间实时更新） ──
    Q_PROPERTY(int downloadPercent READ downloadPercent NOTIFY downloadProgressChanged)
    Q_PROPERTY(qint64 downloadBytes READ downloadBytes NOTIFY downloadProgressChanged)
    Q_PROPERTY(qint64 downloadTotal READ downloadTotal NOTIFY downloadProgressChanged)
    Q_PROPERTY(double downloadSpeedMBps READ downloadSpeedMBps NOTIFY downloadProgressChanged)

public:
    explicit JavaRuntimeInstaller(QObject* parent = nullptr);

    /// 检测当前 CPU 架构（映射到 Tuna 镜像目录名）: x64 / x32 / aarch64 / arm / unknown
    static QString detectCpuArch();
    QString cpuArch() const { return m_cpuArch; }

    bool running() const { return m_running; }
    int currentStep() const { return m_currentStep; }
    int totalSteps() const { return 3; }
    QString statusText() const { return m_statusText; }
    int downloadPercent() const { return m_dlPercent; }
    qint64 downloadBytes() const { return m_dlBytes; }
    qint64 downloadTotal() const { return m_dlTotal; }
    double downloadSpeedMBps() const { return m_dlSpeedMBps; }

    /// 一键安装 Java 8 (JRE) + 17 (JDK) + 25 (JDK)，跳过已安装
    Q_INVOKABLE void installRequiredJavas();
    /// 取消（当前版本下载完成后停止后续）
    Q_INVOKABLE void cancelInstall();

    /// 已检测到的系统 Java（前置检测）: [{major, version, path, isJdk}]
    QVariantList detectedSystemJavas() const;
    /// 重新执行前置检测（返回同上）
    Q_INVOKABLE QVariantList scanSystemJavas();

    /// 单个版本是否需要安装（基于前置检测）
    /// major: 目标主版本；targetIsJdk: 目标是否 JDK
    /// 规则：已有同 major 任意类型（JRE/JDK 均可）→ 不需要（JRE 已满足运行场景）
    bool isRequired(int major, bool targetIsJdk) const;
    /// 已检测到的同 major Java 的显示名（"Java 17 (JDK)"）
    QString existingJavaLabel(int major) const;
    /// 已检测到的同 major Java 的路径
    QString existingJavaPath(int major) const;

    /// 安装单个版本（异步入口：列目录→下载→解压→验证→回调）。
    /// onDone(ok, errorMsg, javaExe)
    void installJavaAsync(int majorVersion, const QString& type,
                          std::function<void(bool, const QString&, const QString&)> onDone);

signals:
    void progressChanged();
    void runningChanged();
    /// 下载进度实时更新（百分比 0-100 / 字节 / 速度 MB/s）
    void downloadProgressChanged();
    /// 单个版本安装完成（label: "Java 8 (JRE)" 等；path: java.exe 路径；skipped: 已存在跳过）
    void javaInstalled(const QString& label, const QString& path, bool skipped);
    /// 全部完成
    void finished(bool ok, const QString& error);
    void logMessage(const QString& msg);
    /// 前置检测完成（QML 更新"已检测到/将安装"状态）
    void systemJavaScanFinished();

private:
    /// 校验 java.exe 真实主版本号（java -version 解析）
    static int verifyJavaMajor(const QString& javaExe);
    /// 递归查找 bin/java.exe（非标准 ZIP 布局兜底）
    static QString findJavaExeRecursive(const QString& dir);

    /// 扫描完成后执行实际安装流程（由 scanSystemJavas 的完成回调触发）
    void runInstallAfterScan();
    /// 安装链：处理第 idx 个版本（异步串联，状态存成员避免悬空引用）
    void installNext(int idx);
    /// 安装链：单个版本异步安装完成回调（含自动重试）
    void onInstallDone(int idx, bool ok, const QString& error, const QString& exe);
    /// 启动清理：删除上次崩溃/失败残留的临时 zip 与残缺目录
    void cleanupStaleCache();

    /// 已检测系统 Java 缓存: {major, version, path, isJdk}
    QVariantList m_detectedJavas;
    /// 已扫描路径去重
    QSet<QString> m_seenBinDirs;
    /// 过滤已知无意义路径（System32 等）
    static bool isSpecialPath(const QString& binDir);

    // ── 安装链状态（成员：异步回调触发时局部变量已销毁，必须存成员） ──
    struct PlanItem {
        int major = 0;
        QString type;
        QString label;
    };
    QList<PlanItem> m_planItems;
    int m_installedCount = 0;
    int m_skippedCount = 0;

    QString m_cpuArch;
    bool m_running = false;
    bool m_cancelled = false;
    bool m_scanThreadRunning = false;
    bool m_waitingForScan = false;
    bool m_retriedThisRound = false;   // 当前版本已自动重试过（每个版本仅重试 1 次）
    int m_lastFailedMajor = 0;
    int m_currentStep = 0;
    QString m_statusText;

    // ── 下载进度状态 ──
    int m_dlPercent = 0;
    qint64 m_dlBytes = 0;
    qint64 m_dlTotal = 0;
    double m_dlSpeedMBps = 0.0;
    qint64 m_dlLastBytes = 0;
    QElapsedTimer m_dlTimer;

    // ── 异步安装状态机 ──
    struct InstallJob {
        int major = 0;
        QString type;
        QString zipUrl;
        QString zipPath;     // 临时 zip 文件路径
        QString javaDir;
        QString javaExe;
        std::function<void(bool, const QString&, const QString&)> onDone;
        QLockFile* lock = nullptr;
        HttpClient::DownloadHandle* dlHandle = nullptr;
        int retryCount = 0;   // 已重试次数（网络抖动自动重试 1 次）
    };
    InstallJob m_job;
    void stepFetchZipList();
    void stepDownloadZip();
    void stepExtractZip();
    void failJob(const QString& error);
    void finishJob(const QString& javaExe);
};

} // namespace ShadowLauncher
