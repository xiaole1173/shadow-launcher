#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QStringList>
#include <functional>

namespace ShadowLauncher {

/// ─────────────────────────────────────────────────────────────
/// 资源拉取引擎（代号：司南 sinan）
///
/// 针对实测慢因（2026-08-01）设计：
///   1. 镜像单请求 2~8s、20 并发反而更慢 → 严格并发控制（API 2 / 图标 3）
///   2. 旧实现图标双重下载（IconCache + QML Image 各一次）→ 未命中返回空，
///      由 iconReady 信号驱动 UI 更新，全链路每图仅下载一次
///   3. 无缓存/无重试 → 三层缓存（QML Image 内存 → 磁盘缩略图 88px → 磁盘原图）
///      + 失败重试（图标 2 次退避 / API 1 次）
///   4. 搜索无缓存 → 结果内存 LRU（TTL 300s），翻页/切 tab 秒开
///
/// 多源适配：getJson 是通用 API 入口（换 URL 即换源）；CurseForge / 整合包
/// 浏览后续只需调用同一入口，无需改动 UI。
/// ─────────────────────────────────────────────────────────────
class ResourceFetchEngine : public QObject {
    Q_OBJECT
public:
    static constexpr const char* kEngineId = "sinan";

    /// cacheRoot：磁盘缓存根（如 {dataDir}/cache/res）
    explicit ResourceFetchEngine(const QString& cacheRoot, QObject* parent = nullptr);

    // ── API JSON（搜索列表等）──
    using JsonDone = std::function<void(int status, const QByteArray& body)>;
    using JsonFail = std::function<void(const QString& error)>;
    /// cacheable=true 且 300s 内同 URL 有缓存 → 直接回调缓存（绕开镜像延迟）
    void getJson(const QString& url, bool cacheable, JsonDone done, JsonFail fail);

    // ── 图标 ──
    /// 磁盘缓存命中 → 返回本地 file:// 路径；未命中 → 返回空并排队下载（完成后 emit iconReady）
    QString iconLocalPath(const QString& url, bool preferThumb = true);
    /// 批量预取（搜索结果到齐后调用）
    void prefetchIcons(const QStringList& urls);

    // 统计（日志/调试）
    int pendingIcons() const { return m_iconQueue.size(); }
    int activeIcons() const { return m_iconActive.size(); }
    int jsonCacheSize() const { return m_jsonCache.size(); }

signals:
    /// 图标就绪（本地路径）。QML 监听此信号，把 model 中 icon==url 的项更新为 localPath
    void iconReady(const QString& url, const QString& localPath);

private:
    struct ApiReq {
        QString url;
        bool cacheable = false;
        JsonDone done;
        JsonFail fail;
        int retries = 0;
    };
    struct IconReq {
        QString url;
        int retries = 0;
    };

    void pumpApi();
    void pumpIcons();
    void startApiRequest(ApiReq req);
    void startIconDownload(const QString& url);
    void onIconData(const QString& url, const QByteArray& data, bool ok, const QString& err);
    void makeThumbnail(const QString& url, const QByteArray& data);

    QString iconFile(const QString& url) const;    // 原图   {root}/icons/{hash}.png
    QString thumbFile(const QString& url) const;   // 缩略图 {root}/thumbs/{hash}_88.png
    static QString hashUrl(const QString& url);
    static QString fileUrl(const QString& path);

    QNetworkAccessManager m_nam;
    QString m_cacheRoot;

    QQueue<ApiReq> m_apiQueue;
    int m_apiActive = 0;

    QQueue<IconReq> m_iconQueue;
    QSet<QString> m_iconQueued;    // 已在队列
    QSet<QString> m_iconActive;    // 下载中（去重）
    QHash<QString, int> m_iconRetries;

    struct JsonEntry { QByteArray body; qint64 ts; };
    QHash<QString, JsonEntry> m_jsonCache;
    QStringList m_jsonOrder;       // FIFO 淘汰顺序

    static constexpr qint64 kJsonTtlMs = 300000; // 300s
    static constexpr int kJsonMax = 64;
    static constexpr int kApiConcurrent = 2;
    static constexpr int kIconConcurrent = 6; // 实测（2026-08-01）：镜像最优并发=6（3并发12张79s，6并发14s，9并发又降速）
    static constexpr int kIconRetryMax = 2;
    static constexpr int kApiRetryMax = 1;
    static constexpr int kThumbSize = 88;
};

} // namespace ShadowLauncher
