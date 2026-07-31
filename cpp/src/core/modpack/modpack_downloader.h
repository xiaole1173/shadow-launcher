// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_downloader.h — 整合包远端模组下载层（网络层）。
//
// 参照 主流启动器 的 LoaderDownload 思路：
//   1. CurseForge：POST /v1/mods/files 批量换取真实下载地址（x-api-key 鉴权，
//      50 个 fileId/批），按 modules 判定资源类型（mods/resourcepacks/shaderpacks），
//      下载完成比对 API 返回的 fileLength。
//   2. Modrinth：直接使用 index.json 的 downloads[0]（+ 备用地址），
//      下载完成比对 sha1。
//   3. 受控并发（默认 3 路，兼顾 CF API IP 风控）、单文件失败重试一次、
//      强制终止清空队列与临时文件。
//
// 说明：不修改既有网络底层（HttpClient 为既有稳定模块），仅通过其公开
// 异步 API 组装；下载写入 .part 临时文件，成功后原子改名，失败/取消即删。

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkReply>
#include <QPointer>

#include <atomic>
#include <functional>

#include "modpack_common.h"

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

    // 启动：先解析 CF 下载地址，再并发下载。includeOptional=true 时连可选文件一起下。
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
        QStringList urls;      // 尝试顺序：[镜像 CDN, 官方直链, 备用源…]，逐个降级
        int urlIdx = 0;        // 当前尝试到第几个 URL
        QString savePath;      // 最终路径
        QString tmpPath;       // .part 临时路径
        QString fileName;
        bool inFlight = false;
        bool finished = false;
        bool ok = false;
        QString error;
    };

    // ── 镜像优先 + 官方兜底请求（异步链式；4xx 除 429 外不降级）──
    void apiWithFallback(bool isPost,
                         const QString& mirrorUrl, const QString& officialUrl,
                         const QByteArray& body, const QString& apiKey,
                         const std::function<void(int, const QByteArray&)>& cb);

    // ── CF 地址解析 ──
    void resolveBatch(int startIndex);                 // 按 50 个/批发 POST /v1/mods/files
    void onResolveBatchDone(int startIndex, int status, const QByteArray& body);
    void onResolveBatchFailed(int startIndex, const QString& err);
    void resolveDownloadUrls();                        // downloadUrl 缺失的条目逐个补解析
    void startDownloadUrlResolve(int idx);

    // ── 下载队列 ──
    void scheduleNext();
    void startItem(int idx);
    void onItemDone(int idx, bool ok, const QString& err);
    void verifyItem(int idx);                          // QtConcurrent 哈希校验
    void onVerifyDone(int idx, bool ok, const QString& err);
    void finalizeItem(int idx, bool ok, const QString& err);
    void finishIfAllDone();
    void cleanupTmp();

    static QByteArray fileSha1(const QString& path);   // 工作线程流式哈希
    static QString classifyCategory(const QString& fileName, const QJsonArray& modules);

    HttpClient* m_http = nullptr;
    QString m_apiKey;
    QString m_targetDir;
    QList<ModpackRemoteFile>* m_files = nullptr;       // 由任务层持有，运行期不做结构变更
    bool m_includeOptional = false;

    QList<DlItem> m_items;                             // 按 files 顺序展开（含跳过项占位）
    int m_total = 0;
    int m_completed = 0;
    int m_failed = 0;
    int m_skipped = 0;

    int m_activeSlots = 0;
    static constexpr int kMaxConcurrent = 3;

    bool m_running = false;
    bool m_cancelled = false;
    bool m_resolveDone = false;
    QList<int> m_downloadUrlPending;   // 待补解析 download-url 的条目（镜像优先）
    std::function<void(const QString&)> m_overwriteHook;   // 覆盖旧文件前的回调
    QList<QPointer<QNetworkReply>> m_inflight;   // 在途请求（取消时 abort）
};

} // namespace ShadowLauncher
