// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// ModDownloadEngine — 模组海量小文件专属下载引擎。
//
// 设计目标（与公共 FileDownloader 物理隔离）：
//   - 只服务 mrpack / Curse 整合包模组批量下载；MC 本体/加载器/库文件仍走 FileDownloader。
//   - 引擎级共享 QNetworkAccessManager（连接池复用，PCL ThreadClient 语义）；
//   - 全异步事件驱动（主线程调度，无 QThreadPool 阻塞事件循环 → 杜绝"线程卡死拖队列"）；
//   - 每源快速失败 → 镜像降级官方 → 全部源单线程兜底一遍（SourcesOnce 式）→ 有限队列轮重试；
//   - 动态超时：Min(Max(ConnectAvg,15s)*(1+FailCount),30s) + 无数据空闲超时（PCL 同款）；
//   - 镜像限频：MCIM/BMCLAPI 每启一线程间隔 ≥100ms，防镜像限流饿死连接；
//   - 独立 SpeedMeter：100ms 采样 + 30 条线性加权滑动窗口，停流自然滑落归零；
//   - SHA1/大小校验 + Accept-Encoding: identity（防 gzip 导致哈希不符）。
//
// 对外契约与 FileDownloader 完全同构，ModpackDownloader 可直接替换使用，
// 上层（DownloadQueueCard/取消/回滚/Overlay/步骤展示）零改动。

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QStringList>
#include <QFile>
#include <QPointer>
#include <memory>

namespace ShadowDownloader {

class ModDownloadEngine : public QObject {
    Q_OBJECT
public:
    explicit ModDownloadEngine(QObject* parent = nullptr);
    ~ModDownloadEngine() override;

    // ── 与 FileDownloader 同构的公共契约 ──
    void addFile(const QString& localPath, const QString& localName,
                 const QStringList& sources, qint64 expectedSize = -1,
                 const QByteArray& sha1 = QByteArray(),
                 bool jarStrip = false);
    void start();
    void cancel();
    bool isRunning() const { return m_state == Running; }
    int completedFiles() const { return m_completedFiles; }
    int totalFiles() const { return m_totalFiles; }
    int failedFiles() const { return m_failedFiles; }
    qint64 downloadedBytes() const { return m_downloadedBytes; }
    qint64 totalBytes() const { return m_totalBytes; }
    double currentSpeedMBps() const { return m_emaMbps; }

    // ── 模组专属配置 ──
    void setMaxThreads(int n) { m_maxThreads = qBound(1, n, 64); }
    void setCfApiKey(const QString& key) { m_cfApiKey = key; }   // CF 官方 CDN 下载认证（edge.forgecdn.net 7/16 起必带）
    /// 全局下载源策略：0=镜像优先 1=官方优先 2=自动切换（仅用于源排序说明，当前源顺序由 addFile 决定）
    void setSourcePolicy(int p) { m_sourcePolicy = p; }
    /// 镜像 host 每启一线程的限频间隔（默认 100ms）
    void setMirrorRateLimitMs(int ms) { m_mirrorRateLimitMs = qMax(0, ms); }

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& url, const QString& fileName,
                      qint64 received, qint64 total,
                      const QString& savePath = QString());
    void fileFinished(const QString& localPath, bool success);
    void allFinished();
    void logMessage(const QString& msg);

private:
    struct Item {
        // 静态
        QString localPath;
        QString localName;
        QStringList sources;
        qint64 fileSize = -1;
        QByteArray expectedSha1;
        // 运行态
        int state = 0;                 // 0=pending 1=active 2=done 3=fail
        int sourceIdx = 0;             // 当前尝试的源下标
        int failCount = 0;             // 本文件累计失败次数（动态超时放大用）
        bool fallbackPassDone = false; // SourcesOnce 式"全部源单线程兜底"已完成
        qint64 received = 0;
        qint64 total = 0;
        bool enoughBytes = false;    // 已收字节达到 manifest 预期大小（服务器 CL 异常时主动收尾，2026-08-10）
        qint64 resumeFrom = 0;       // 断点续传起点（慢速换源保留的已下字节，2026-08-10）
        bool rangeChecked = false;   // 续传请求是否已确认服务器返回 206
        QString tmpPath;
        QString error;
        QPointer<QNetworkReply> reply;
        QPointer<QFile> outFile;
        QPointer<QTimer> idleTimer;
        qint64 launchMs = 0;            // 请求发起时刻（连接耗时统计）
        qint64 firstByteMs = 0;        // 首包时间（连接耗时统计）
        // 慢速看门狗（尾程提速）
        qint64 slowSinceMs = 0;         // 连续低速起始时刻（0=未触发）
        qint64 lastWatchBytes = 0;      // 看门狗上次采样的 received
        int slowSwitchCount = 0;        // 看门狗换源累计（全源都慢时停止换源，避免误判失败）
    };

    enum State { Idle, Running, Cancelled };

    void pump();                                   // 调度：填满并发槽
    void launchRequest(std::shared_ptr<Item> it);  // 用当前源发起请求
    QString pickSource(std::shared_ptr<Item> it, bool* exhausted);
    void onReplyProgress(std::shared_ptr<Item> it, qint64 recv, qint64 total);
    void onReadyRead(std::shared_ptr<Item> it);
    void onReplyFinished(std::shared_ptr<Item> it);
    void sourceFailed(std::shared_ptr<Item> it, const QString& why);
    void finishItem(std::shared_ptr<Item> it, bool ok, const QString& err);
    void tryStartNextRound();
    void finishAll();
    void speedTick();
    void watchTick();                          // 慢速看门狗：500ms 扫描低速文件 → 换源
    bool verifyFile(const std::shared_ptr<Item>& it) const;
    bool isMirrorHost(const QString& url) const;

    State m_state = Idle;
    bool m_cancelled = false;
    QString m_cfApiKey;           // CF API key（官方 edge CDN 下载认证）
    int m_maxThreads = 12;
    int m_sourcePolicy = 0;
    int m_mirrorRateLimitMs = 100;
    int m_round = 0;                 // 当前重试轮（0=首轮）
    static constexpr int kMaxRounds = 5;   // 整合包不容放过任何模组：补位重试上限（每文件至多 5 轮×多源+兜底）
    static constexpr int kProgressEmitThrottleMs = 150;  // 进度发射节流

    QList<std::shared_ptr<Item>> m_items;    // 全部条目（含终态）
    QList<std::shared_ptr<Item>> m_pending;  // 当前轮未终态条目
    int m_active = 0;
    int m_totalFiles = 0;
    int m_completedFiles = 0;
    int m_failedFiles = 0;
    qint64 m_downloadedBytes = 0;
    qint64 m_totalBytes = 0;

    QNetworkAccessManager m_nam;             // 引擎级共享（连接池复用）
    QTimer m_pumpTimer;                      // 50ms 调度
    QTimer m_speedTimer;                     // 100ms 采样
    QTimer m_watchTimer;                     // 500ms 慢速看门狗
    // 慢速看门狗阈值：每 500ms tick 增量 <64KB（≈128KB/s）且连续 2000ms → 换源
    static constexpr qint64 kSlowBytesPerTick = 64 * 1024;
    static constexpr qint64 kSlowTriggerMs = 2000;
    static constexpr double kGlobalSlowGateMbps = 1.0;   // 1MB/s（2026-08-10 二修：0.25→1.0）
    static constexpr int kMaxSlowSwitches = 6;   // 每文件看门狗换源上限（全源慢时停止，宁可慢爬不误判失败）
    QElapsedTimer m_speedClock;
    qint64 m_lastSpeedBytes = 0;
    QList<qint64> m_speedRecords;            // 30 条 × 100ms ≈ 3s 窗口
    double m_emaMbps = 0.0;
    qint64 m_lastProgressEmitMs = 0;
    qint64 m_lastMirrorLaunchMs = 0;         // 镜像限频
    qint64 m_connectTotalMs = 0;             // 连接耗时统计（动态超时）
    int m_connectCount = 0;
};

} // namespace ShadowDownloader
