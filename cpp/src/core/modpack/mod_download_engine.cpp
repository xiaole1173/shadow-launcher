// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "mod_download_engine.h"
#include "../engine_identity.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUuid>
#include <QUrl>
#include <QDebug>
#include <algorithm>

#include "../../utils/logger.h"

namespace ShadowDownloader {
using namespace ShadowLauncher;

ModDownloadEngine::ModDownloadEngine(QObject* parent)
    : QObject(parent)
{
    m_pumpTimer.setInterval(50);
    m_pumpTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_pumpTimer, &QTimer::timeout, this, &ModDownloadEngine::pump);

    m_speedTimer.setInterval(100);
    m_speedTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_speedTimer, &QTimer::timeout, this, &ModDownloadEngine::speedTick);

    // 慢速看门狗：500ms 扫描低速文件 → 换源（治尾程龟速）
    m_watchTimer.setInterval(500);
    m_watchTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_watchTimer, &QTimer::timeout, this, &ModDownloadEngine::watchTick);

    m_speedClock.start();
}

ModDownloadEngine::~ModDownloadEngine()
{
    // 取消所有在途请求并清理临时文件
    for (auto& it : m_items) {
        if (it->reply) it->reply->abort();
        if (it->outFile && it->outFile->isOpen()) it->outFile->close();
        if (!it->tmpPath.isEmpty()) QFile::remove(it->tmpPath);
    }
}

void ModDownloadEngine::addFile(const QString& localPath, const QString& localName,
                                const QStringList& sources, qint64 expectedSize,
                                const QByteArray& sha1, bool jarStrip)
{
    auto it = std::make_shared<Item>();
    it->localPath = localPath;
    it->localName = localName;
    it->sources = sources;
    it->fileSize = expectedSize;
    it->expectedSha1 = sha1;
    it->total = expectedSize > 0 ? expectedSize : 0;
    m_items.append(it);
    m_totalFiles++;
    if (expectedSize > 0) m_totalBytes += expectedSize;
}

void ModDownloadEngine::start()
{
    if (m_state == Running) return;
    m_state = Running;
    m_cancelled = false;
    m_round = 0;
    m_active = 0;
    m_pending.clear();

    // 任务大小降序（对齐夸父/山海经）：大文件先下，尾程只剩小文件，
    // 避免最后几个大文件单连接硬啃导致“越下越慢”的观感
    std::stable_sort(m_items.begin(), m_items.end(),
        [](const std::shared_ptr<Item>& a, const std::shared_ptr<Item>& b) {
            return a->fileSize > b->fileSize;   // 未知大小(-1)自然沉底
        });

    for (auto& it : m_items) {
        it->state = 0;
        it->sourceIdx = 0;
        it->failCount = 0;
        it->fallbackPassDone = false;
        it->received = 0;
        it->error.clear();
        it->slowSinceMs = 0;
        it->lastWatchBytes = 0;
        it->slowSwitchCount = 0;
        m_pending.append(it);
    }
    m_lastSpeedBytes = m_downloadedBytes;
    m_speedClock.restart();
    m_pumpTimer.start();
    m_speedTimer.start();
    m_watchTimer.start();
    emit progressChanged(m_completedFiles, m_totalFiles, m_downloadedBytes, m_totalBytes);
    emit logMessage(engineBanner("jingwei"));
    emit logMessage(QStringLiteral("[精卫] 模组下载引擎启动 文件数=%1 最大并发=%2")
                        .arg(m_totalFiles).arg(m_maxThreads));
    pump();
}

void ModDownloadEngine::cancel()
{
    if (m_state != Running) return;
    m_state = Cancelled;
    m_cancelled = true;
    m_pumpTimer.stop();
    m_speedTimer.stop();
    m_watchTimer.stop();
    for (auto& it : m_items) {
        if (it->state == 1 && it->reply) it->reply->abort();
    }
    emit logMessage(QStringLiteral("[精卫] 模组下载已取消"));
    // 注意：不 emit allFinished —— 上层 ModpackDownloader::cancel 自行收尾
}

// ═════════════════════════════════════════════════════════════════════════
// 调度
// ═════════════════════════════════════════════════════════════════════════

void ModDownloadEngine::pump()
{
    if (m_state != Running) return;

    // 填满并发槽
    while (m_active < m_maxThreads) {
        if (m_pending.isEmpty()) {
            // ── 失败文件补位重试（核心调度改进）──
            // 整合包不容放过任何模组：正常队列已空且有并发空槽时，把上一轮失败的
            // 文件重新入队重试——不干等整轮结束（旧逻辑要 active==0 才重试，
            // 失败发生在早期时其他几百个文件全下完才轮到它）。
            // 轮次保护：每补位一批计一轮，达到 kMaxRounds-1 后不再补位（由 finishAll 收尾）。
            if (m_round >= kMaxRounds - 1) break;
            int requeued = 0;
            for (auto& it : m_items) {
                if (it->state == 3) {
                    it->state = 0; it->sourceIdx = 0; it->failCount = 0;
                    it->fallbackPassDone = false; it->received = 0; it->error.clear();
                    m_failedFiles--;
                    m_pending.append(it);
                    requeued++;
                }
            }
            if (requeued == 0) break;
            m_round++;
            emit logMessage(QStringLiteral("[精卫] [补位重试] 第 %1/%2 轮：%3 个失败文件重新入队")
                                .arg(m_round + 1).arg(kMaxRounds).arg(requeued));
            continue;   // 继续填槽
        }
        auto it = m_pending.takeFirst();
        if (it->state != 0) continue;
        it->state = 1;
        m_active++;
        // 镜像限频：MCIM/BMCLAPI 每启一线程间隔 ≥ m_mirrorRateLimitMs
        if (isMirrorHost(it->sources.isEmpty() ? QString() : it->sources.first())) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 wait = m_lastMirrorLaunchMs + m_mirrorRateLimitMs - now;
            if (wait > 0) {
                QTimer::singleShot(wait, this, [this, it]() {
                    if (m_state == Running && it->state == 1 && !it->reply)
                        launchRequest(it);
                });
                continue;   // 槽已占用，等限频后启动
            }
            m_lastMirrorLaunchMs = now;
        }
        launchRequest(it);
    }
}

bool ModDownloadEngine::isMirrorHost(const QString& url) const
{
    const QString host = QUrl(url).host().toLower();
    return host.contains(QStringLiteral("mcimirror"))
        || host.contains(QStringLiteral("bmclapi"));
}

QString ModDownloadEngine::pickSource(std::shared_ptr<Item> it, bool* exhausted)
{
    *exhausted = false;
    if (it->sourceIdx < it->sources.size()) {
        return it->sources[it->sourceIdx];
    }
    if (!it->fallbackPassDone && !it->sources.isEmpty()) {
        // 全部源失败 → SourcesOnce 式：单线程逐源整体兜底一遍
        it->fallbackPassDone = true;
        it->sourceIdx = 0;
        return it->sources[0];
    }
    *exhausted = true;
    return QString();
}

void ModDownloadEngine::launchRequest(std::shared_ptr<Item> it)
{
    bool exhausted = false;
    const QString url = pickSource(it, &exhausted);
    if (exhausted) {
        finishItem(it, false, it->error.isEmpty()
                                ? QStringLiteral("所有下载源均失败")
                                : it->error);
        return;
    }
    if (url.isEmpty()) {
        finishItem(it, false, QStringLiteral("无可用下载地址"));
        return;
    }

    // 临时文件（成功后原子改名到最终路径；失败/取消清理）
    it->tmpPath = it->localPath + QStringLiteral(".tmp-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QDir().mkpath(QFileInfo(it->localPath).absolutePath());
    auto* f = new QFile(it->tmpPath, this);
    if (!f->open(QIODevice::WriteOnly)) {
        delete f;
        finishItem(it, false, QStringLiteral("无法写入: %1").arg(it->tmpPath));
        return;
    }
    it->outFile = f;
    it->received = 0;
    it->enoughBytes = false;   // 新请求重置（2026-08-10）
    it->firstByteMs = 0;

    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    // 官方 edge CDN 自 2026-07-16 起强制要求 API key（x-api-key header），否则 401
    if (!m_cfApiKey.isEmpty() && url.contains(QLatin1String("edge.forgecdn.net")))
        req.setRawHeader("x-api-key", m_cfApiKey.toUtf8());
    // 禁用 HTTP/2（镜像 H2 连接不稳定）+ identity 编码（防 gzip 致 SHA1 不符）
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setRawHeader("Accept-Encoding", "identity");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam.get(req);
    it->reply = reply;
    it->launchMs = QDateTime::currentMSecsSinceEpoch();

    // 动态超时：Min(Max(ConnectAvg,15s)*(1+FailCount),30s)（PCL 同款）
    const qint64 connectAvg = m_connectCount > 0 ? m_connectTotalMs / m_connectCount : 15000;
    const qint64 timeoutMs = qMin(qMax(connectAvg, qint64(15000)) * (1 + it->failCount), qint64(30000));
    auto* idle = new QTimer(this);
    idle->setSingleShot(true);
    idle->setInterval(static_cast<int>(timeoutMs));
    connect(idle, &QTimer::timeout, this, [this, it]() {
        if (it->reply) it->reply->abort();
    });
    idle->start();
    it->idleTimer = idle;

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, it](qint64 recv, qint64 total) { onReplyProgress(it, recv, total); });
    connect(reply, &QNetworkReply::readyRead, this,
            [this, it]() { onReadyRead(it); });
    connect(reply, &QNetworkReply::finished, this,
            [this, it]() { onReplyFinished(it); });

    emit logMessage(QStringLiteral("[精卫] 启动 文件=%1 源=%2 超时=%3ms")
                        .arg(it->localName, url).arg(timeoutMs));
}

void ModDownloadEngine::onReplyProgress(std::shared_ptr<Item> it, qint64 recv, qint64 total)
{
    if (m_cancelled || it->state != 1) return;
    if (total > 0) it->total = total;
    const qint64 delta = recv - it->received;
    if (delta > 0) {
        it->received = recv;
        m_downloadedBytes += delta;
    }
    // 150ms 节流，避免高频跨对象信号
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastProgressEmitMs >= kProgressEmitThrottleMs) {
        m_lastProgressEmitMs = now;
        emit progressChanged(m_completedFiles, m_totalFiles, m_downloadedBytes, m_totalBytes);
        emit fileProgress(it->reply ? it->reply->url().toString() : QString(),
                          it->localName, it->received, it->total, it->localPath);
    }
}

void ModDownloadEngine::onReadyRead(std::shared_ptr<Item> it)
{
    if (m_cancelled || !it->reply || !it->outFile || it->state != 1) return;
    if (it->firstByteMs == 0) {
        it->firstByteMs = QDateTime::currentMSecsSinceEpoch();
        // 连接耗时统计（动态超时基线）：首包时刻 - 发起时刻
        const qint64 connMs = qMax<qint64>(0, qMin<qint64>(it->firstByteMs - it->launchMs, 60000));
        m_connectTotalMs += connMs;
        m_connectCount++;
    }
    // 空闲超时重置：收到数据即视为活跃
    if (it->idleTimer) it->idleTimer->start();
    const QByteArray data = it->reply->readAll();
    if (!data.isEmpty()) {
        it->outFile->write(data);
    }
    // 2026-08-10：服务器 Content-Length 异常（> 实际数据，连接挂起不关闭）时
    // reply 永不 finished → 空闲超时被数据活动重置 → 无限下载（实测整合包最后
    // 2 个模组"速度在跳永不完成"，18:19:35 后无任何完成/失败/超时日志）。
    // 已收字节达到 manifest 预期大小即主动收尾：abort → finished → 校验路径
    // （大小/SHA1 定真伪，不符则换源重试，不再无限下载）。
    if (it->fileSize > 0 && it->received >= it->fileSize && !it->enoughBytes) {
        it->enoughBytes = true;
        if (it->reply) it->reply->abort();
    }
}

void ModDownloadEngine::onReplyFinished(std::shared_ptr<Item> it)
{
    if (!it->reply) return;
    QNetworkReply* reply = it->reply;
    it->reply = nullptr;
    if (it->idleTimer) { it->idleTimer->stop(); it->idleTimer->deleteLater(); it->idleTimer = nullptr; }

    if (m_cancelled) {
        // 取消路径：不计数、不 emit fileFinished，上层自行收尾
        if (it->outFile) { it->outFile->close(); it->outFile->deleteLater(); it->outFile = nullptr; }
        QFile::remove(it->tmpPath);
        it->tmpPath.clear();
        m_active--;
        it->state = 0;
        reply->deleteLater();
        return;
    }

    const QNetworkReply::NetworkError err = reply->error();
    const bool httpOk = err == QNetworkReply::NoError;
    if (!httpOk) {
        reply->deleteLater();
        // 2026-08-10：主动收尾（已收字节达到预期大小，服务器 CL 异常）→ 走校验路径
        // （大小/SHA1 定真伪；而不是当作失败换源，更不是无限等 finished）
        if (!it->enoughBytes) {
            // 看门狗换源的 abort：用准确文案（避免 “Operation canceled” 误导最终失败原因）
            QString why = reply->errorString();
            if (err == QNetworkReply::OperationCanceledError && it->slowSwitchCount > 0)
                why = QStringLiteral("慢速源已切换");
            sourceFailed(it, why);
            return;
        }
    }

    // 关闭输出文件
    if (it->outFile) {
        it->outFile->close();
        it->outFile->deleteLater();
        it->outFile = nullptr;
    }

    // 校验：SHA1 优先；无 SHA1 时校验大小
    if (!verifyFile(it)) {
        reply->deleteLater();
        sourceFailed(it, QStringLiteral("校验失败（SHA1/大小不符）"));
        return;
    }

    reply->deleteLater();
    finishItem(it, true, QString());
}

bool ModDownloadEngine::verifyFile(const std::shared_ptr<Item>& it) const
{
    QFileInfo fi(it->tmpPath);
    if (!fi.exists() || fi.size() <= 0) return false;
    if (!it->expectedSha1.isEmpty()) {
        QFile f(it->tmpPath);
        if (!f.open(QIODevice::ReadOnly)) return false;
        QCryptographicHash h(QCryptographicHash::Sha1);
        h.addData(&f);
        f.close();
        return h.result().toHex() == it->expectedSha1;
    }
    if (it->fileSize > 0) return fi.size() == it->fileSize;
    return true;
}

void ModDownloadEngine::sourceFailed(std::shared_ptr<Item> it, const QString& why)
{
    it->failCount++;
    it->sourceIdx++;
    it->received = 0;
    if (it->outFile) { it->outFile->close(); it->outFile->deleteLater(); it->outFile = nullptr; }
    if (!it->tmpPath.isEmpty()) { QFile::remove(it->tmpPath); it->tmpPath.clear(); }
    it->error = why;

    bool exhausted = false;
    const QString next = pickSource(it, &exhausted);
    if (exhausted) {
        finishItem(it, false, why);
        return;
    }
    emit logMessage(QStringLiteral("[精卫] 源失败 %1 文件=%2 错误=%3 → 切换 %4")
                        .arg(QString::number(it->sourceIdx), it->localName, why, next));
    launchRequest(it);   // 槽位保持占用，就地换源重试
}

void ModDownloadEngine::finishItem(std::shared_ptr<Item> it, bool ok, const QString& err)
{
    if (it->state == 2 || it->state == 3) return;
    if (it->outFile) { it->outFile->close(); it->outFile->deleteLater(); it->outFile = nullptr; }
    if (it->idleTimer) { it->idleTimer->stop(); it->idleTimer->deleteLater(); it->idleTimer = nullptr; }

    if (ok) {
        // 原子落盘：临时文件 → 最终路径
        QFile::remove(it->localPath);
        if (!QFile::rename(it->tmpPath, it->localPath)) {
            QFile::remove(it->tmpPath);
            it->tmpPath.clear();
            ok = false;
        } else {
            it->tmpPath.clear();
        }
    }

    if (ok) {
        it->state = 2;
        m_completedFiles++;
        emit logMessage(QStringLiteral("[精卫] [完成] %1").arg(it->localName));
    } else {
        if (!it->tmpPath.isEmpty()) { QFile::remove(it->tmpPath); it->tmpPath.clear(); }
        it->state = 3;
        m_failedFiles++;
        it->error = err.isEmpty() ? QStringLiteral("文件落盘失败: %1").arg(it->localPath) : err;
        emit logMessage(QStringLiteral("[精卫] [失败] %1: %2").arg(it->localName, it->error));
    }
    emit fileFinished(it->localPath, ok);
    m_active--;

    emit progressChanged(m_completedFiles, m_totalFiles, m_downloadedBytes, m_totalBytes);

    if (m_active == 0 && m_pending.isEmpty()) {
        if (m_state == Running) tryStartNextRound();
        else finishAll();
    } else {
        pump();
    }
}

void ModDownloadEngine::tryStartNextRound()
{
    if (m_cancelled || m_state != Running) return;
    if (m_round >= kMaxRounds - 1) { finishAll(); return; }

    int failedCount = 0;
    for (auto& it : m_items) {
        if (it->state == 3) { failedCount++; it->state = 0; it->sourceIdx = 0; it->failCount = 0; it->fallbackPassDone = false; it->received = 0; it->error.clear(); m_pending.append(it); }
    }
    if (failedCount == 0) { finishAll(); return; }

    m_round++;
    m_failedFiles -= failedCount;
    emit logMessage(QStringLiteral("[精卫] [重试] 第 %1/%2 轮：%3 个失败文件整体重试")
                        .arg(m_round + 1).arg(kMaxRounds).arg(failedCount));
    pump();
}

void ModDownloadEngine::finishAll()
{
    if (m_state != Running) return;
    m_state = Idle;
    m_pumpTimer.stop();
    m_speedTimer.stop();
    m_watchTimer.stop();
    emit progressChanged(m_completedFiles, m_totalFiles, m_downloadedBytes, m_totalBytes);
    emit logMessage(QStringLiteral("[精卫] [完成] 模组下载引擎结束 成功=%1 失败=%2 共%3")
                        .arg(m_completedFiles).arg(m_failedFiles).arg(m_totalFiles));
    emit allFinished();
}

// ═════════════════════════════════════════════════════════════════════════
// SpeedMeter — 与 FileDownloader 同款：100ms 采样 + 30 条线性加权窗口
// ═════════════════════════════════════════════════════════════════════════

void ModDownloadEngine::speedTick()
{
    if (m_state != Running) return;

    const qint64 elapsed = m_speedClock.restart();
    if (elapsed <= 0) return;

    const qint64 now = m_downloadedBytes;
    qint64 bytes = now - m_lastSpeedBytes;
    if (bytes < 0) bytes = 0;
    m_lastSpeedBytes = now;

    const qint64 actualBps = bytes * 1000 / elapsed;
    m_speedRecords.prepend(actualBps);
    if (m_speedRecords.size() > 30) m_speedRecords.removeLast();

    qint64 weightedSum = 0;
    int weightDiv = 0;
    int w = m_speedRecords.size();
    for (qint64 rec : m_speedRecords) { weightedSum += rec * w; weightDiv += w; w--; }
    const qint64 currentBps = weightDiv > 0 ? weightedSum / weightDiv : actualBps;
    m_emaMbps = currentBps / (1024.0 * 1024.0);   // 窗口值直接作为展示值（停流自然归零）

    emit progressChanged(m_completedFiles, m_totalFiles, m_downloadedBytes, m_totalBytes);
}

// ═════════════════════════════════════════════════════════════════════════
// 慢速看门狗 — 每 500ms 扫描活动文件，低速持续 2s 则换源（治尾程龟速）
// ═════════════════════════════════════════════════════════════════════════

void ModDownloadEngine::watchTick()
{
    if (m_state != Running) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto& it : m_items) {
        if (it->state != 1 || !it->reply) continue;

        // 换源中（abort 已发出，等 onReplyFinished → sourceFailed → launchRequest 重置）
        if (it->slowSinceMs < 0) continue;

        // 首包前不检测（连接建立期由 idle 超时兜底）
        if (it->firstByteMs == 0) {
            it->lastWatchBytes = it->received;
            it->slowSinceMs = 0;
            continue;
        }
        // 换源/启动后 3s 冷却（新连接提速期，避免误伤）
        if (now - it->launchMs < 3000) {
            it->lastWatchBytes = it->received;
            it->slowSinceMs = 0;
            continue;
        }

        const qint64 delta = it->received - it->lastWatchBytes;
        it->lastWatchBytes = it->received;

        if (delta >= kSlowBytesPerTick) {
            // 这个 tick 够快 → 重置低速计时
            it->slowSinceMs = 0;
            continue;
        }

        // 低速 tick
        if (it->slowSinceMs == 0) {
            it->slowSinceMs = now;
            continue;
        }
        if (now - it->slowSinceMs < kSlowTriggerMs)
            continue;

        // 连续低速 ≥2s：换源（有限次）
        // 单源无意义（重下同源大概率还是慢），直接放弃看门狗
        if (it->sources.size() <= 1) {
            it->slowSinceMs = 0;
            continue;
        }
        // 还有源可换吗？（正常下一个源 或 触发兜底回绕第一个源）
        const bool hasNext = it->sourceIdx + 1 < it->sources.size()
                          || (!it->fallbackPassDone && !it->sources.isEmpty());
        if (!hasNext) {
            // 全部源都已试过：放弃看门狗，宁可慢爬也不误判失败
            it->slowSinceMs = 0;
            continue;
        }
        if (it->slowSwitchCount >= kMaxSlowSwitches) {
            it->slowSinceMs = 0;
            continue;
        }

        it->slowSwitchCount++;
        it->slowSinceMs = -1;   // 换源中标记：onReplyFinished 到达前不再触发（abort 异步窗口）
        it->lastWatchBytes = it->received;
        emit logMessage(QStringLiteral("[精卫] [慢速换源] 文件=%1 第%2次 当前源速度过慢 → 切换")
                            .arg(it->localName).arg(it->slowSwitchCount));
        // abort 触发 onReplyFinished(OperationCanceled) → sourceFailed → pickSource 换下一个源
        if (it->reply) it->reply->abort();
    }
}

} // namespace ShadowDownloader
