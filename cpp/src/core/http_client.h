// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — HTTP client wrapper (驿道 v2：高速多线程通用下载引擎)
// 2026-08-02 v2：大文件自动 Range 多线程分片 + 307 预解析 + 停滞检测推广 + SHA1/大小校验内建
// 设计要点：
//   1. API 请求（get/post/getRaw/put/deleteResource）保持单连接，零变化
//   2. 文件传输（download/downloadWithReply）> 4MB 自动分片（实测 8 连接收益最佳）
//   3. 分片前先探测：Range 支持判定 + 307 重定向预解析（直接打最终 URL，省每片一次重定向）
//   4. 停滞检测/片级重试/校验统一内建

#pragma once

#include <QObject>
#include <QThread>
#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QList>
#include <QVector>
#include <QMutex>
#include <string>
#include <functional>
#include <memory>

namespace ShadowLauncher {

// ============================================================
// Shared config: all network layers must use these
// ============================================================

// Browser-like UA for Forge Maven / Cloudflare-protected CDNs
static constexpr const char* kDefaultUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) ShadowLauncher/1.0 Chrome/125.0.0.0 Safari/537.36";

struct NetworkConfig {
    std::string userAgent = "ShadowLauncher/1.0";
    std::string proxyHost;       // empty = no proxy
    int proxyPort = 0;
    std::string proxyUser;
    std::string proxyPass;
    int connectTimeoutMs = 10000;
    int totalTimeoutMs = 10000;
};

// ============================================================
// 驿道 v2 分片传输常量（2026-08-02 实测校准）
// ============================================================

static constexpr qint64 kShardThresholdBytes = 4 * 1024 * 1024;  // 低于此值不分片（小文件/API 零开销）
static constexpr qint64 kShardSizeBytes      = 2 * 1024 * 1024;  // 每片目标大小
static constexpr int    kMinShards           = 4;                // 最小分片数
static constexpr int    kMaxShards           = 4;                // 最大分片数（curl 实测 8 连接最优，但 Qt 同时 8 连接在 Modrinth 高峰触发限流/被拒；4 为保守定稿，待用户链路实测校准）
static constexpr int    kShardConnectTimeoutMs = 20000;          // 片/探测首包超时（慢链路宽容）
static constexpr int    kShardIdleTimeoutMs    = 30000;          // 片无数据超时
static constexpr int    kShardMaxRetries       = 3;              // 单片最大重试次数
static constexpr int    kStallIntervalMs       = 2000;           // 停滞检测间隔
static constexpr int    kStallTicks            = 3;              // 连续 N 次无进展 → abort（6s）

// ============================================================
// HttpClient — singleton network layer
// ============================================================

class HttpClient : public QObject {
    Q_OBJECT
public:
    /// 文件下载任务句柄（替代裸 QNetworkReply*：分片场景无单一 reply）
    /// 注意：无 Q_OBJECT（嵌套类不支持 moc 元对象；本类不需要信号/槽）
    class DownloadHandle : public QObject {
    public:
        ~DownloadHandle() override;
        /// 取消任务：abort 全部在途分片 + 清理 .part*（线程安全，可任意时刻调用）
        void abort();
        bool isActive() const;
    private:
        explicit DownloadHandle(QObject* parent = nullptr);
        void addReply(QNetworkReply* r);
        void onReplyFinished(QNetworkReply* r);
        friend class HttpClient;
        QVector<QNetworkReply*> m_replies;
        mutable QMutex m_mutex;
        bool m_aborted = false;
    };

    static HttpClient& instance();

    void setProxy(const QString& host, int port,
                  const QString& user = {}, const QString& pass = {});
    void setUserAgent(const QString& ua);
    NetworkConfig config() const { return m_config; }

    // ── API 请求（单连接，零变化）──
    void get(const QString& url,
             std::function<void(int status, const QByteArray& body)> callback,
             std::function<void(const QString& error)> onError = nullptr);

    // ── 文件传输（>4MB 自动多线程分片）──
    // expectedSize/expectedSha1：可选完整性校验（不符自动重下一次）
    void download(const QString& url, const QString& savePath,
                  std::function<void(qint64 received, qint64 total)> progress,
                  std::function<void(bool ok, const QString& error)> done,
                  qint64 expectedSize = -1,
                  const QString& expectedSha1 = {});

    void downloadWithFallback(const QString& url, const QString& savePath,
                  std::function<void(qint64 received, qint64 total)> progress,
                  std::function<void(bool ok, const QString& error)> done,
                  qint64 expectedSize = -1,
                  const QString& expectedSha1 = {});

    // 返回 DownloadHandle* 供 abort（分片/单连接统一句柄）
    // resumeFrom: bytes already downloaded (-1 = fresh, >=0 = append to existing file，走单连接)
    DownloadHandle* downloadWithReply(const QString& url, const QString& savePath,
                  std::function<void(qint64 received, qint64 total)> progress,
                  std::function<void(bool ok, const QString& error)> done,
                  qint64 resumeFrom = -1,
                  qint64 expectedSize = -1,
                  const QString& expectedSha1 = {});

    // POST with raw body (returns QNetworkReply* for async handling)
    QNetworkReply* post(const QNetworkRequest& request, const QByteArray& body);

    // GET with custom request (returns QNetworkReply* for async handling)
    QNetworkReply* getRaw(const QNetworkRequest& request);

    // PUT with custom request + body (returns QNetworkReply* for async handling)
    QNetworkReply* put(const QNetworkRequest& request, const QByteArray& body);

    // DELETE with custom request (returns QNetworkReply* for async handling)
    QNetworkReply* deleteResource(const QNetworkRequest& request);

    void abortDownload(QNetworkReply* reply);
    void abortDownload(DownloadHandle* handle);

    QNetworkAccessManager* manager() const { return m_manager; }

signals:
    void proxyChanged();

private:
    HttpClient();
    ~HttpClient() override;

    // 单连接下载（小文件/不支持 Range/断点续传路径；停滞检测+重试内建）
    void startSingle(const QString& url, const QString& savePath,
                     std::function<void(qint64, qint64)> progress,
                     std::function<void(bool, const QString&)> done,
                     qint64 resumeFrom, qint64 expectedSize,
                     const QString& expectedSha1, DownloadHandle* handle);

    // 分片下载入口：探测（Range 判定 + 307 预解析）→ 分片并行 → 合并
    void startChunked(const QString& url, const QString& savePath,
                      std::function<void(qint64, qint64)> progress,
                      std::function<void(bool, const QString&)> done,
                      qint64 expectedSize, const QString& expectedSha1,
                      DownloadHandle* handle);

    // 分片核心（探测成功后调用）
    void runChunked(const QString& finalUrl, qint64 total,
                    const QString& savePath,
                    std::function<void(qint64, qint64)> progress,
                    std::function<void(bool, const QString&)> done,
                    qint64 expectedSize, const QString& expectedSha1,
                    DownloadHandle* handle);

    // 完整性校验（size + sha1），失败返回 false
    static bool verifyFile(const QString& path, qint64 expectedSize,
                           const QString& expectedSha1, QString* errOut);

    NetworkConfig m_config;
    QNetworkAccessManager* m_manager = nullptr;
    QNetworkAccessManager* m_manager2 = nullptr;   // 分片双 QNAM 轮询（对齐山海经）
};

} // namespace ShadowLauncher
