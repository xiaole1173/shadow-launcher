// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QByteArray>
#include <QElapsedTimer>

#include "../core/step_pipeline.h"
#include "../utils/types.h"  // InstallCard struct

namespace ShadowLauncher {

// ── DownloadSession — 单个安装实例 ──
// 管理一个安装任务的生命周期、步骤管线、进度跟踪
// 目标是逐步替代 version_backend 中现有的 DownloadSession struct
class DownloadSession : public QObject {
    Q_OBJECT
public:
    explicit DownloadSession(const QString& versionId, QObject* parent = nullptr);
    ~DownloadSession() override;

    // ── 标识 ──
    QString versionId() const { return m_versionId; }
    QString sessionId() const { return m_sessionId; }
    void setSessionId(const QString& id) { m_sessionId = id; }

    // ── StepPipeline 访问 ──
    StepPipeline* pipeline() const { return m_pipeline; }
    StepModel* stepModel() const { return m_pipeline ? m_pipeline->stepModel() : nullptr; }
    QObject* stepModelObj() const { return m_pipeline ? m_pipeline->model() : nullptr; }

    // ── 状态 ──
    bool isFailed() const { return m_failed; }
    QString errorMessage() const { return m_error; }
    bool isMerged() const { return m_isMerged; }
    void setMerged(bool v) { m_isMerged = v; }

    // ── 进度 (瞬时速度) ──
    qreal totalProgress() const;        // weighted pipeline progress
    qint64 currentSpeed() const { return m_speed; }
    void recordBytes(qint64 bytesRecv, qint64 bytesTotal);
    /// 由上层（VersionBackend）推送引擎 EMA 速度（唯一速度源），
    /// 卡片速度一律取此值，前端不再自行差分计算。
    void setSpeed(qint64 bps) { m_speed = qMax<qint64>(0, bps); }
    void resetSpeed();

    // ── 控制 ──
    void cancel();          // 取消所有步骤
    void markFailed(const QString& err);
    void reset();

    // ── 错误处理 ──
    void setError(const QString& err) { m_error = err; }
    void clearFailure() { m_failed = false; m_error.clear(); emit progressUpdated(); }

    // ── 卡片数据导出 (适配旧 InstallCardModel) ──
    InstallCard toCard(const QString& iid, const QString& name, const QString& type) const;

signals:
    void progressUpdated();     // 进度/速度变化 → 触发卡片增量更新

    // ═══════════════════════════════════════════════
    // 旧 struct 字段 — 逐步迁移中
    // ═══════════════════════════════════════════════
public:
    // ── 步骤 (从 struct 迁入) ──
    QVariantList steps;

    // ── 进度 (raw, 非计算值) ──
    qreal m_rawTotalProgress = 0.0;

    // ── MC 下载状态 ──
    bool mcDownloadDone = false;
    qint64 mcStepDone[3] = {};
    qint64 mcStepTotal[3] = {};
    QSet<QString> mcFileAdded;
    qint64 mcBytesDl = 0;
    qint64 mcBytesAll = 0;

    // ── 加载器状态 ──
    bool loaderDownloadReady = false;
    QByteArray loaderDownloadData;
    int loaderVerifyStep = -1;
    int loaderStepIdx = -1;
    QString loaderType;
    QString loaderVer;
    bool hasPendingLoader = false;
    QString pendingLoaderName;
    QString pendingLoaderVer;
    qreal pendingLoaderWeight = 0.0;
    QString pendingLoaderMc;
    QString pendingLoaderType;
    QString forgeInstallerSha1;
    QString fabricApiVersion;
    QString fabricApiUrl;
    QString fabricApiSavePath;
    QString fabricApiFinalPath;
    bool fabricApiPending = false;
    bool loaderFinishedWaitingMC = false;
    bool optifineJarParallel = false;
    bool optifineJarDone = false;
    bool optifineInstallTriggered = false;
    QByteArray optifineJarData;
    QString bmclType;
    QString bmclPatch;

    // ── ML 下载字节跟踪 ──
    qint64 mlBytesDl = 0;
    qint64 mlBytesAll = 0;
    qint64 mlBytesDone = 0;
    qint64 mlFileTotal = 0;
    qint64 mlSpeed = 0;          // ML 下载瞬时速度 (bytes/s)
    /// 2026-08-15：加载器相关下载（主文件/安装器库）进行中标志。
    /// 聚合卡片速度时用它替代脆弱的 isModLoaderInstalling()——
    /// 驿道下载主文件阶段 installer 未 running（verify-only），
    /// isModLoaderInstalling() 返回 false → 主文件速度不计入（用户实测）。
    bool loaderDownloadActive = false;
    qint64 fabSpeed = 0;         // Fabric API 下载速度 (bytes/s, delta/200ms)
    qint64 fabSpeedLastBytes = 0; // 上次 Fabric API 接收字节
    qint64 fabSpeedLastMs = 0;    // 上次 Fabric API 速度时间戳

    // ── 用户数据导入 ──
    bool hasImportPending = false;
    // ── 步骤跟踪 ──
    int loadedStep = 0;
    QString importArchivePath;
    qint64 importFailedAtMs = 0;


    // ── 平滑进度 (EWMA) ──
    qreal smoothProgress = 0.0;

    // ── MC 版本 (从 struct 迁入) ──
    QString mcVersion;

private:
    QString m_versionId;
    QString m_sessionId;
    bool m_failed = false;
    bool m_isMerged = false;
    QString m_error;

    // ── Speed：唯一速度源为上层推送的引擎 EMA（setSpeed），本地不再维护第二套算法 ──
    qint64 m_speed = 0;

    StepPipeline* m_pipeline = nullptr;
};

} // namespace ShadowLauncher
