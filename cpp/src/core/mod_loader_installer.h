// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QDir>

class QZipReader;
#include <QJsonArray>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QAtomicInt>
#include <QFutureWatcher>
#include <QNetworkReply>
#include <QVector>
#include "http_client.h"   // HttpClient::DownloadHandle

#include <functional>
#include <memory>
#include <atomic>

namespace ShadowLauncher {

class ModLoaderInstaller : public QObject {
    Q_OBJECT

public:
    explicit ModLoaderInstaller(QObject* parent = nullptr);
    ~ModLoaderInstaller() override;

    void setGameDir(const QString& dir) { m_gameDir = dir; }
    QString gameDir() const { return m_gameDir; }
    void setModsDir(const QString& dir) { m_modsDir = dir; }

    // Set forge Maven branch suffix before calling installForge (e.g. "1.10.0", "mc172")
    void setForgeBranch(const QString& branch) { m_forgeBranch = branch; }
    QString modsDir() const { return m_modsDir.isEmpty() ? m_gameDir + "/mods" : m_modsDir; }

    void installForge(const QString& mcVersion, const QString& forgeVersion, const QString& installName,
                     const QString& expectedSha1 = {});
    void installFabric(const QString& mcVersion, const QString& fabricVersion, const QString& installName);
    void installNeoForge(const QString& mcVersion, const QString& neoVersion, const QString& installName);
    void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                         const QString& forgeVersion, const QString& installName,
                         const QString& bmclType = QString(), const QString& bmclPatch = QString());
    void installOptifineFromJar(const QByteArray& jarData, const QString& mcVersion, const QString& installName,
                                const QString& bmclType = QString(), const QString& bmclPatch = QString());

    bool isRunning() const { return m_running; }
    /// 安装器库（install_profile libraries）并行下载中——即使 m_running=false
    /// （verify-only 模式），下载速度也应计入安装速度（2026-08-15）
    bool installerLibsActive() const { return m_installerLibsRunning; }
    QString loaderVersion() const { return m_loaderVersion; }
    bool hasCachedJar() const { return !m_cachedJar.isEmpty(); }

    void installForgeFromData(const QByteArray& installerJar, const QString& mcVersion,
                             const QString& forgeVersion, const QString& installName);
    // Continue after verify-only: start install phase
    void forgeContinueInstall();
    void installNeoForgeFromData(const QByteArray& installerJar, const QString& mcVersion,
                                  const QString& neoVersion, const QString& installName);
    void neoForgeContinueInstall();

    /// 下载安装器库（install_profile 的 libraries）：verify 完成后立即启动，
    /// 与 MC 下载并行；安装阶段前确保完成（m_installerLibsDone）
    void forgeStepLibs(const QByteArray& jarData);

    /// 安装 worker（后台收尾任务）是否在运行（取消时删 tempDir 前检查）
    bool installWorkerBusy() const { return m_installWorkerRunning.load(); }
    /// bootstrapper java 进程是否在跑（取消时删 tempDir 前检查）
    bool bootstrapperRunning() const { return m_bootstrapperWatcher && m_bootstrapperWatcher->isRunning(); }
    /// 同步安装执行中（installLegacy2/1 主线程跑，含 QEventLoop 等驿道下载）：
    /// 取消时删 tempDir 前必须检查，否则撞上写文件崩溃（2026-08-08）
    bool syncInstallRunning() const { return m_syncInstallRunning; }
    void cancel();

    // Fabric parallel install: start downloading MC + Fabric at the same time
    void setParallelMode(bool v) { m_parallelMode = v; }
    /// 下载源策略：true=官方源优先（maven/libraries 等资源按用户全局设置路由），
    /// false=BMCLAPI 镜像优先（默认）。Fabric/Forge 库下载与安装器主文件生效。
    void setPreferOfficial(bool v) { m_preferOfficial = v; }
    void fabricFinalize();  // Called after MC completes in parallel mode

signals:
    // Step-level progress (for phase text)
    void progressChanged(int step, int totalSteps, const QString& description);
    // Byte-level progress (for single-file progress bar)
    void byteProgress(const QString& fileName, qint64 received, qint64 total, qint64 speed);
    // Verification
    void verifyStarted();
    void verifyFinished(bool ok);
    // Final
    void finished(bool success, const QString& error);
    void logMessage(const QString& msg);
    // User-facing toast (e.g. auto Java download progress during loader install)
    void toastMessage(const QString& msg);
    // 自动下载便携 Java 完成（majorVersion）——供上层触发 Java 列表刷新（不弹 toast）
    void javaAutoInstalled(int majorVersion);
    // Pause between verify and install (for parallel MC download)
    void waitingForMC();
    // Sub-progress within a step (for installer stdout parsing)
    void stepProgress(int step, int percentage);
    // 安装器库下载开始/完成（供版本后端联动步骤状态）
    void installerLibsStarted();
    void installerLibsDone();
    // 安装器库下载失败（至少一个文件所有源均失败）——版本后端应将步骤标 failed 而非 completed
    void installerLibsFailed(const QString& err);
    // 安装器库文件级进度（done/total）——步骤右侧显示“剩余 x 个文件”
    void installerLibsFileProgress(int done, int total);

private:
    // Download helpers
    void downloadToFile(const QString& url, const QString& savePath,
                        std::function<void(bool ok, const QString& error)> done,
                        bool reportProgress = true,
                        std::function<void(qint64, qint64)> rawProgress = {});
    void downloadToMemory(const QString& url,
                          std::function<void(bool ok, const QByteArray& data)> done,
                          const QString& fileNameHint = QString(),
                          bool reportProgress = true);
    /// TrueRace: fire multiple URLs concurrently, return first successful data
    void downloadToMemoryRace(const QStringList& urls,
                              std::function<void(bool ok, const QByteArray& data)> done,
                              const QString& fileNameHint = QString());
    void downloadSmall(const QString& url,
                       std::function<void(bool ok, const QByteArray& data)> done);

    bool ensureVanillaInstalled(const QString& mcVersion);
    QString versionsDir() const { return m_gameDir + "/versions"; }
    QString computeSha1(const QByteArray& data);
    void emitByteProgress(const QString& name, qint64 received, qint64 total);

    // Temp .minecraft isolation for OptiFine standalone installer
    QString setupTempMc();
    /// 将整个临时 .minecraft 复制回游戏目录（比选择性复制更可靠）
    void copyOptifineTempMc(const QString& tempMcPath);
    void cleanupTempMc(const QString& tempDir);
    void copyRecursive(const QString& srcDir, const QString& dstDir);
    /// Flatten inheritsFrom chain for OptiFine installer output — removes parent dependency
    /// so the vanilla MC version folder can be safely deleted.
    void flattenOptifineVersion(const QString& versionId);

    // Forge/NeoForge
    /// Resolve a version ID to its actual directory path (handles name mismatch)
    QString findVersionDir(const QString& versionId) const;

    // ── 后台安装任务（安装收尾重活块搬离主线程，治 UI 卡顿/冻结）──
    /// 在后台线程执行纯文件/网络/进程重活，完成后回主线程执行 onDone（继续流程/emit）
    void runInstallTask(std::function<void()> task, std::function<void()> onDone = {});

    QFutureWatcher<void>* m_installWorker = nullptr;
    std::atomic<bool> m_installWorkerRunning{false};
    std::atomic<bool> m_installWorkerFailed{false};   // worker 内失败标志（onDone 前检查，中止流程）
    std::shared_ptr<std::atomic<bool>> m_installCancelled;   // worker 取消标志（QEventLoop/QProcess 轮询）
    std::function<void()> m_pendingInstallOnDone;   // worker 完成后主线程继续流程的回调

    /// 后台线程：写安装器 JAR（剥离签名）+ client jar 复制 + mappings 预下载/TSRG 转换
    void bootstrapperPrepare(const QByteArray& jarData, QString installerJarPath,
                             const QString& javaPath, bool isNeoForge);
    /// 主线程：launcher_profiles.json + 启动 bootstrapper（QProcess 本体已 QtConcurrent 异步）
    void runBootstrapperLaunch(const QString& installerJarPath, const QString& javaPath,
                               bool isNeoForge);

    void forgeStep1_downloadInstaller();
    void forgeStep2_verify(const QByteArray& jarData);
    /// 统一版本库下载（Legacy 2 共用，对齐主流启动器实现 GameLibrariesTask）：
    /// rules 检查 + natives classifier + 多源 + 跳过已存在（2026-08-07）
    int downloadVersionLibraries(const QJsonArray& libs);
    void neoStep1_downloadInstaller();
    void neoStep2_verify(const QByteArray& jarData);
    // Extract & install — three-way branch (Legacy2 / Legacy1 / Bootstrapper)
    void forgeStep3_install(const QByteArray& jarData);
    void forgeStep3_prepareImpl(const QByteArray& jarData, const QString& mavenVer);  // worker 内执行
    void forgeStep3_route(const QByteArray& jarData);                                  // 主线程：决策三分支
    // Legacy 2: has "install" field → universal JAR + inheritsFrom JSON
    void installLegacy2(const QByteArray& jarData, const QJsonObject& profile);
    // Legacy 1: has "json" field, no install, no processors → maven/ + version JSON
    void installLegacy1(const QByteArray& jarData, const QJsonObject& profile);
    // Method A: Bootstrapper (processors / spec≥1 / NeoForge)
    void runBootstrapperProcess(const QByteArray& jarData);
    void onBootstrapperFinished();

    // Async bootstrapper result
    struct BootstrapperResult {
        bool success = false;
        int exitCode = -1;
        bool timedOut = false;
        QString installerJarPath;
        QString loaderName;
        QStringList oldVersions;
        QString foundSub;
        QString errorMsg;
    };
    static BootstrapperResult runBootstrapperSync(
        const QString& javaPath, const QStringList& launchArgs,
        const QString& installerJarPath, const QString& loaderName,
        const QStringList& oldVersions, const QString& versionsDirPath,
        int timeoutMs, std::function<void(int)> onStepProgress,
        std::shared_ptr<std::atomic<bool>> cancelledFlag = nullptr);

    QByteArray m_cachedJar;
    bool m_verifyOnly = false;
    // 安装器库（install_profile libraries）并行下载状态
    bool m_installerLibsDone = false;
    bool m_installerLibsRunning = false;
    bool m_pendingInstallAfterLibs = false;
    // 2026-08-14：小文件批量场景免 Range 探测（安装器库下载提速）
    bool m_downloadSkipProbe = false;
    void installNeoForge(const QByteArray& jarData, const QJsonObject& profile);
    void renameVersionFolder(const QString& oldName, const QString& newName);
    /// Extract the embedded forge-installer.jar (helper) to temp, return path
    static QString extractBootstrapperPath();
    /// Extract the embedded java-wrapper.jar (oolloo.jlw.Wrapper) to temp, return path
    static QString extractJavaWrapperPath();
    /// Find a usable Java on PATH or common install dirs
    QString findJavaPath(int minVersion = 8);
    /// Auto-download Java from Tuna Adoptium mirror (ZIP), extract to java_cache/
    QString downloadAndExtractJava(int minVersion);

    // Post-bootstrapper: 扁平化 JSON + 复制 JAR
    void finalizeBootstrapperInstall();

    // Fabric
    void fabricStep1_downloadProfile();
    void fabricStep2_downloadLibraries(const QByteArray& profileData);
    void fabricStep3_writeVersion(const QByteArray& profileData);
    void fabricStep3_writeVersionImpl(const QByteArray& profileData);   // worker 内执行

public:
    // OptiFine: 通过 adloadx 解析官方下载地址
    static QString resolveOptifineOfficialUrl(const QString& filename);
    /// Quick validity check: data must start with ZIP magic (PK) and be >100KB
    static bool isValidZip(const QByteArray& data) {
        return data.size() > 100 * 1024 && data.size() >= 4
            && static_cast<quint8>(data[0]) == 'P' && static_cast<quint8>(data[1]) == 'K';}

private:
    // Optifine standalone
    void optifineStep2_install(const QByteArray& jarData, const QString& filename);
    void runOptifineInstaller(const QByteArray& jarData);  // javaw fallback
    void installOptifineSynthetic(const QByteArray& jarData);  // construct from type+patch
    void installOptifineFromProfile(const QJsonObject& profile, const QZipReader& reader,
                                      const QByteArray& jarData);  // extract from profile

    // ── state ──
    QString m_gameDir;
    QString m_modsDir;
    QString m_mcVersion;
    QString m_loaderVersion;
    QString m_forgeBranch; // Maven branch suffix (e.g. "1.10.0", "mc172")
    QString m_installName;
    QString m_optifineBmclType;   // BMCLAPI type for OptiFine library path
    QString m_optifineBmclPatch;  // BMCLAPI patch for OptiFine library path
    QString m_loaderType;
    int m_currentStep = 0;
    int m_totalSteps = 0;
    bool m_running = false;
    QString m_expectedForgeSha1;   // cached from Forge version list (skip SHA1 network request)
    bool m_cancelled = false;
    // 同步安装执行中（installLegacy2/1 主线程跑，含 QEventLoop 等驿道下载）：
    // destroyMergedContext 的 workerBusy 检查需覆盖它，否则取消时删 tempDir 撞上写文件崩溃
    bool m_syncInstallRunning = false;
    // In-flight HttpClient replies (downloadToFile). Aborted in cancel() so their
    // completion callbacks run while `this` is still alive (destroyed right after
    // cancel() by destroyMergedContext).
    QVector<HttpClient::DownloadHandle*> m_activeReplies;
    QString m_optifineForgeVersion;
    bool m_optifineUseOfficial = false;
    bool m_parallelMode = false;  // Fabric: don't auto-advance to write phase
    bool m_preferOfficial = false; // 下载源策略：true=官方 maven 优先（镜像兜底）

    // Fabric library download state
    struct FabricLibTask {
        QString url;
        QString savePath;
        qint64 size = 0;
        bool downloaded = false;
        QString error;
    };
    QVector<FabricLibTask> m_fabricLibTasks;
    qint64 m_fabricLibBytesDone = 0;
    qint64 m_fabricLibBytesTotal = 0;
    int m_fabricLibIndex = 0;
    QByteArray m_fabricProfileData;  // held between parallel download and finalize

    // Bootstrapper 结果（先复制 JSON，再在单独步骤中扁平化）
    bool m_bootstrapperOk = false;
    QString m_bootstrapperError;
    QString m_postJsonPath;

    // byte progress tracking
    qint64 m_bytesReceived = 0;
    qint64 m_bytesLast = 0;
    QElapsedTimer m_speedTimer;

    // Cancellation flag shared with bootstrapper background thread
    std::shared_ptr<std::atomic<bool>> m_bootstrapperCancelled;

    // Async bootstrapper watcher
    QFutureWatcher<BootstrapperResult>* m_bootstrapperWatcher = nullptr;
};

} // namespace ShadowLauncher
