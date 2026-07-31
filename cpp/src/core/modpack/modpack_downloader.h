// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_downloader.h — 整合包远端模组下载层（网络层 + 引擎适配层）。
//
// 参照 主流启动器 的 LoaderDownload 思路：
//   1. CurseForge：POST /v1/mods/files 批量换取真实下载地址（x-api-key 鉴权，
//      50 个 fileId/批），按 modules 判定资源类型（mods/resourcepacks/shaderpacks），
//      下载完成比对 API 返回的 fileLength。
//   2. Modrinth：直接使用 index.json 的 downloads[0]（+ 备用地址），
//      下载完成比对 sha1。
//   3. 镜像优先 + 官方兜底（MCIM 镜像 / CF·MR 官方端点，429/5xx/超时降级）。
//
// 下载执行复用既有成熟引擎 ShadowDownloader::FileDownloader（多线程分片、断点续传、
// 多源自动降级、SHA1/大小校验、主机健康、限速、EMA 网速），本类只承担：
//   - CF 批量地址解析（REST API，与文件下载正交，引擎不含）
//   - 可选文件跳过 / 落盘路径编排 / 覆盖备份与新建登记钩子（任务级事务语义）
//   - 引擎信号 → 任务层信号桥接（index 关联、队列进度折算、取消语义）
// 对外接口（start/cancel/signals）保持不变，任务层与 QML 无感。
//
// 说明：不修改既有网络底层（HttpClient 为既有稳定模块），仅通过其公开
// 异步 API 组装 CF API 请求；文件本体下载交给 FileDownloader 引擎实例
// （每次 start 新建，任务结束/取消即释放，与 MC 下载引擎实例完全隔离）。

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkReply>
#include <QPointer>
#include <QSet>

#include <atomic>
#include <functional>

#include "modpack_common.h"

namespace ShadowDownloader { class FileDownloader; }

namespace ShadowLauncher {

class HttpClient;

class ModpackDownloader : public QObject {
    Q_OBJECT
public:
    explicit ModpackDownloader(QObject* parent = nullptr);

    void setApiKey(const QString& key) { m_apiKey = key; }
    void setTargetDir(const QString& dir) { m_targetDir = dir; }
    void setFiles(QList<ModpackRemoteFile>* files) { m_files = files; }
    // 覆盖钩子：落盘前若目标已存在旧文件则回调（任务层用于回滚备份）
    void setOverwriteHook(const std::function<void(const QString& savePath)>& hook) { m_overwriteHook = hook; }
    // 新建文件钩子：成功落盘后回调（任务层登记回滚，失败/取消时清理）
    void setCreatedHook(const std::function<void(const QString& savePath)>& hook) { m_createdHook = hook; }

    // 启动：先解析 CF 下载地址，再经 FileDownloader 引擎并发下载。
    // includeOptional=true 时连可选文件一起下。
    void start(bool includeOptional);
    void cancel();
    bool isRunning() const { return m_running; }

signals:
    void statusChanged(const QString& text);          // 阶段文案（"正在解析下载地址…"）
    void fileProgress(int index, const QString& name, qint64 received, qint64 total);
    void fileFinished(int index, bool success, const QString& error);
    void queueProgress(int completed, int total, int failed);
    void allFinished(bool cancelled);
    void logLine(const QString& msg);

private:
    struct DlItem {
        int index = -1;
        QStringList urls;      // 尝试顺序：[镜像 CDN, 官方直链, 备用源…] → 引擎 orderedSources
        QString savePath;      // 最终路径
        QString fileName;
        bool finished = false;
        bool ok = false;
        QString error;
    };

    // ── 镜像优先 + 官方兜底请求（异步链式；4xx 除 429 外不降级）──
    void apiWithFallback(bool isPost,
                         const QString& mirrorUrl, const QString& officialUrl,
                         const QByteArray& body, const QString& apiKey,
                         const std::function<void(int, const QByteArray&)>& cb);

    // ── CF 地址解析（REST API 层，保留自研）──
    void resolveBatch(int startIndex);                 // 按 50 个/批发 POST /v1/mods/files
    void onResolveBatchDone(int startIndex, int status, const QByteArray& body);
    void onResolveBatchFailed(int startIndex, const QString& err);
    void resolveDownloadUrls();                        // downloadUrl 缺失的条目逐个补解析
    void startDownloadUrlResolve(int idx);

    // ── 引擎适配层（ShadowDownloader::FileDownloader）──
    void startEngineDownloads();   // 地址就绪 → 构造引擎 + addFile 编排 + 启动
    int findIndexBySavePath(const QString& path) const;
    void onEngineProgress(int completed, int total, qint64 bytes, qint64 allBytes);
    void onEngineFileProgress(const QString& url, const QString& fileName,
                              qint64 received, qint64 total);
    void onEngineFileFinished(const QString& localPath, bool success);
    void onEngineAllFinished();

    static QString classifyCategory(const QString& fileName, const QJsonArray& modules);

    HttpClient* m_http = nullptr;
    QString m_apiKey;
    QString m_targetDir;
    QList<ModpackRemoteFile>* m_files = nullptr;       // 由任务层持有，运行期不做结构变更
    bool m_includeOptional = false;

    QList<DlItem> m_items;                             // 按 files 顺序展开（含跳过项占位）
    int m_total = 0;
    int m_failed = 0;          // 解析阶段失败计数（引擎失败用 fd->failedFiles()）
    int m_skippedCount = 0;    // 可选跳过 + 取消未开始 计数（并入 queueProgress completed）

    bool m_running = false;
    bool m_cancelled = false;
    QList<int> m_downloadUrlPending;   // 待补解析 download-url 的条目（镜像优先）
    std::function<void(const QString&)> m_overwriteHook;   // 覆盖旧文件前的回调
    std::function<void(const QString&)> m_createdHook;     // 新建文件落盘后的回调（回滚登记）
    QList<QPointer<QNetworkReply>> m_inflight;   // CF API 在途请求（取消时 abort）

    ShadowDownloader::FileDownloader* m_fd = nullptr;  // 引擎实例（每次 start 新建）
    QSet<QString> m_preExisting;   // addFile 前已存在的目标（覆盖场景，成功不登记新建）
    qint64 m_lastQueueEmitMs = 0;  // queueProgress 桥接限频（200ms）
    qint64 m_lastFileProgMs = 0;   // fileProgress 桥接限频（200ms）
    QString m_lastEngineError;     // 引擎最近一条失败/校验日志（失败详情透传卡片）
    qint64 m_lastLogEmitMs = 0;    // 常规日志 500ms 合并限频（防日志风暴拖死主线程）
    // ── 队列级兜底重试（首轮完成后自动收集失败文件，按现有引擎规则跑第二遍）──
    bool m_retryRoundDone = false; // 兜底重试轮已执行（每任务最多一轮）
    int m_round1Done = 0;          // 首轮成功数（重试轮进度基数）
    int m_round1Failed = 0;        // 首轮失败数（重试轮剩余失败数由此递减）
};

} // namespace ShadowLauncher
