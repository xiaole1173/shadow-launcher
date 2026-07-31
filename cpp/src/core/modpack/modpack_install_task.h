// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_install_task.h — 整合包导入任务调度层（参照 主流启动器 LoaderCombo 流程）。
//
// 业务流程（对齐主流启动器实现 ModModpack.vb）：
//   选择文件 → 识别 CF/Modrinth → 解析清单 → 解压 overrides 至目标游戏目录
//   → 批量下载模组并校验 → 安装 MC+加载器（复用 VersionBackend merged 安装）
//   → 生成/核对 version.json → 注册版本列表 → 完成信号
//
// 失败/取消：回滚已生成文件与目录（含覆盖文件备份恢复），不留垃圾残留。
// 路径策略：动态读取版本隔离开关（VersionIsolation::getVersionGameDir），
//   隔离开启 → versions/{name}/game 私有目录；关闭 → 公共游戏根目录。
//   全程禁止硬编码路径。

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QMap>
#include <QElapsedTimer>
#include <QTimer>

#include <atomic>

#include "modpack_common.h"

namespace ShadowLauncher {

class ModpackParser;
class ModpackDownloader;
class VersionBackend;
class VersionIsolation;

class ModpackInstallTask : public QObject {
    Q_OBJECT
public:
    explicit ModpackInstallTask(QObject* parent = nullptr);
    ~ModpackInstallTask() override;

    // 依赖注入：版本后端（MC+加载器安装）、隔离开关（路径策略）、游戏根目录
    void configure(VersionBackend* vb, VersionIsolation* iso,
                   const QString& gameDir, const QString& cfApiKey);

    void start(const QString& zipPath, bool includeOptional);
    void cancel();
    bool isBusy() const { return m_busy; }

    // 供 QML 展示的模组列表（status 随下载实时更新）
    QVariantList modItems() const;

signals:
    void stepChanged(const QString& stepName);
    void progressChanged(qreal total, const QString& text, const QString& file);
    void fileProgressChanged(const QString& fileName, qreal fileProgress);
    void logLine(const QString& msg);
    void modItemsChanged();
    void finished(bool success, const QString& versionName, const QString& error);

private:
    enum class Phase {
        Idle, Parse, Extract, Download, McInstall, Finalize, Done, Error, Cancelled
    };

    // ── 流程步骤 ──
    void runParse();          // 工作线程：探测格式 + 解析清单 + 计算路径
    void onParsed(bool ok, const QString& error);
    void runExtract();        // 工作线程：解压 overrides → 目标游戏目录
    void onExtracted(int fileCount);
    void runDownload();       // 主线程异步：模组批量下载
    void onDownloadAllFinished(bool cancelled);
    void runMcInstall();      // 复用 VersionBackend（merged 安装）
    void onMcInstallFinished(bool ok);
    void runFinalize();
    void ensureVersionJsonFallback(int attempt);  // 兜底：清单补全 version.json
    void completeImport();    // 刷新版本列表 + 成功信号
    void fail(const QString& error);      // 统一失败出口（含回滚）
    void finishCancelled();               // 取消出口（含回滚）
    void rollback();                      // 删除自建文件/恢复备份/清理版本目录

    // ── 工具 ──
    QString findFreeVersionName(const QString& base) const;
    void setStep(const QString& name);
    void setProgress(qreal p, const QString& text, const QString& file = {});
    void log(const QString& msg);
    void registerCreatedFile(const QString& path);
    void registerOverwrite(const QString& path);

    // ── 原生任务卡片（InstallCardModel 轮询通道）──
    void initTaskCard();                 // 注册卡片 + 6 步骤初始化
    void syncCard();                     // 进度/阶段/速度 → 卡片
    void setCardStep(int idx, const QString& status, int pct);  // 更新单步骤并同步
    void syncCardAttachments();          // mods/logs/info → 卡片
    void pushCardLog(const QString& msg);
    void refreshCardMods();              // 从 m_meta.files 重建模组明细
    void finishCard(bool success, const QString& err);  // 终态（完成/失败/取消）
    void removeCard();

    QString m_cardId;
    QVariantList m_cardSteps;    // 6 阶段 [{name,status,percentage,show}]
    QVariantList m_cardMods;     // [{name,size,status,error,progress}]
    QVariantList m_cardLogs;     // [{text,color}] 上限 300
    QVariantMap m_cardInfo;      // {name,version,mc,loader,format,modCount,fileCount,targetName}
    qint64 m_cardSpeed = 0;      // 卡片速度（下载阶段 EMA / MC 阶段读会话）
    qint64 m_lastBytes = 0;
    qint64 m_lastBytesMs = 0;
    QString m_mcSessionId;       // MC 阶段轮询的会话 id（merged=targetName / vanilla=mcVersion）
    QTimer* m_mcPollTimer = nullptr;     // MC 阶段 300ms 轮询进度/速度
    QTimer* m_attachSyncTimer = nullptr; // 下载阶段 500ms 同步附件
    bool m_cardDone = false;

    // ── 依赖 ──
    VersionBackend* m_vb = nullptr;
    VersionIsolation* m_iso = nullptr;
    QString m_gameDir;        // .minecraft 根
    QString m_cfApiKey;

    // ── 任务状态 ──
    std::atomic<bool> m_cancel{false};
    bool m_busy = false;
    Phase m_phase = Phase::Idle;
    QString m_stepName;
    QString m_statusText;
    qreal m_progress = 0.0;
    QString m_currentFile;

    // ── 输入 / 解析结果 ──
    QString m_zipPath;
    bool m_includeOptional = false;
    ModpackMeta m_meta;
    QString m_targetName;     // 版本文件夹名 / 版本 id
    QString m_versionDir;     // {gameDir}/versions/{targetName}
    QString m_resourceDir;    // 资源落盘目录（隔离开关动态计算）
    bool m_versionDirCreated = false;

    // ── 回滚追踪 ──
    QSet<QString> m_createdFiles;            // 本任务创建的文件（失败时删除）
    QSet<QString> m_createdDirs;             // 本任务创建的目录（失败时若空则删）
    QMap<QString, QString> m_overwriteBackup;// 被覆盖的旧文件 → 备份路径（失败时恢复）
    QString m_backupDir;                     // 备份根目录（%TEMP%/shadow-modpack-backup-xxx）

    // ── 子组件 ──
    ModpackDownloader* m_downloader = nullptr;

    // ── MC/加载器安装联动 ──
    bool m_installingMc = false;
    QString m_installedVanillaId;  // 纯原版路径：先装 vanilla 再改名
    bool m_vanillaPreinstalled = false;
};

} // namespace ShadowLauncher
