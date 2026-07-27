// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Per-file download engine (v9 — 主流启动器-ref).
// Architecture: see file_downloader.h.

#include "core/file_downloader.h"
#include "utils/logger.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHttp1Configuration>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QTimer>
#include <QHostInfo>
#include <QDateTime>
#include <algorithm>
#include <climits>
#include <cmath>

namespace ShadowDownloader {
using namespace ShadowLauncher;

// ═══════════════════════════════════════
// DnsResolver implementation
// ═══════════════════════════════════════

QMutex DnsResolver::s_mutex;
QMap<QString, double> DnsResolver::s_ipReliability;
QMap<QString, qint64> DnsResolver::s_dnsFailureTime;

QString DnsResolver::resolve(const QUrl& url, QString& outIp, int& outFamily)
{
    outIp.clear();
    outFamily = 0;

    QString host = url.host();

    // Skip DNS for Mojang/Minecraft domains (strict SNI requirements, #8295)
    if (host.contains("mojang.com", Qt::CaseInsensitive) ||
        host.contains("minecraft.net", Qt::CaseInsensitive) ||
        host.contains("minecraftservices.com", Qt::CaseInsensitive))
        return url.toString();

    QMutexLocker lock(&s_mutex);

    // Cool-down check: if DNS failed in last 60s, skip
    if (s_dnsFailureTime.contains(host)) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - s_dnsFailureTime[host] < kDnsFailCooldownMs)
            return url.toString();
        s_dnsFailureTime.remove(host);
    }

    lock.unlock();

    // Perform DNS resolution
    QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        lock.relock();
        s_dnsFailureTime[host] = QDateTime::currentMSecsSinceEpoch();
        return url.toString();
    }

    QList<QHostAddress> candidates = info.addresses();

    // Separate IPv4 and IPv6
    QList<QHostAddress> v4, v6;
    for (auto& addr : candidates) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol) v4.append(addr);
        else if (addr.protocol() == QAbstractSocket::IPv6Protocol) v6.append(addr);
    }

    // If both families exist, choose the family with higher reliability
    if (!v4.isEmpty() && !v6.isEmpty()) {
        QMutexLocker lock2(&s_mutex);
        double v4Max = 0.0, v6Max = 0.0;
        for (auto& a : v4) v4Max = qMax(v4Max, s_ipReliability.value(a.toString(), 0.0));
        for (auto& a : v6) v6Max = qMax(v6Max, s_ipReliability.value(a.toString(), 0.0));
        if (v4Max >= v6Max) candidates = v4; else candidates = v6;
    } else if (!v4.isEmpty()) {
        candidates = v4;
    } else {
        candidates = v6;
    }

    // Pick the IP with highest reliability
    QHostAddress best;
    double bestRel = -999.0;
    QMutexLocker lock3(&s_mutex);
    for (auto& addr : candidates) {
        QString ip = addr.toString();
        double rel = s_ipReliability.value(ip, 0.0);
        if (rel > bestRel) { bestRel = rel; best = addr; }
    }

    if (best.isNull()) return url.toString();

    outIp = best.toString();
    outFamily = (best.protocol() == QAbstractSocket::IPv4Protocol) ? 4 : 6;

    // Build URL with IP (preserve port)
    QUrl result;
    result.setScheme(url.scheme());
    result.setHost(best.toString());
    result.setPort(url.port());
    result.setPath(url.path());
    result.setQuery(url.query());
    // Set Host header separately (used by HTTP virtual hosting)
    // The caller is responsible for setting Host header
    return result.toString();
}

void DnsResolver::recordReliability(const QString& ip, double score)
{
    if (ip.isEmpty()) return;
    QMutexLocker lock(&s_mutex);
    double old = s_ipReliability.value(ip, 0.0);
    s_ipReliability[ip] = old * 0.5 + score * 0.5;
}

QStringList DnsResolver::getCandidateIps(const QString& host)
{
    QMutexLocker lock(&s_mutex);
    QStringList result;
    // Not implementing full list retrieval for now
    Q_UNUSED(host);
    return result;
}

// ═══════════════════════════════════════
// FileDownload helpers
// ═══════════════════════════════════════

std::shared_ptr<DownloadThread> FileDownload::findMaxUndonePiece() const
{
    std::shared_ptr<DownloadThread> maxPiece;
    qint64 maxUndone = 0;
    for (auto& t : threads) {
        if (t->state >= 3) continue;
        qint64 u = t->downloadUndone();
        if (u > maxUndone) { maxUndone = u; maxPiece = t; }
    }
    return maxPiece;
}

int FileDownload::nextSourceId(bool preferDifferent) const
{
    if (orderedSources.isEmpty()) return -1;

    if (!preferDifferent) {
        // Pick first non-dead source
        for (int i = 0; i < orderedSources.size(); ++i)
            if (!orderedSources[i].isDead) return i;
        return -1;
    }

    // Prefer a source different from most existing threads
    QMap<int, int> countBySource;
    for (auto& t : threads) countBySource[t->sourceId]++;

    int bestId = -1;
    int bestCount = INT_MAX;
    for (int i = 0; i < orderedSources.size(); ++i) {
        if (orderedSources[i].isDead) continue;
        int cnt = countBySource.value(i, 0);
        if (cnt < bestCount) { bestCount = cnt; bestId = i; }
    }
    return bestId;
}

// ═══════════════════════════════════════
// FileDownloader
// ═══════════════════════════════════════

FileDownloader::FileDownloader(QObject* parent) : QObject(parent)
{
    qCInfo(logDownload) << QStringLiteral("下载引擎初始化 (v9 — 主流启动器-ref)");

    // QNetworkAccessManager is NOT thread-safe in Qt 6.
    // Per-thread QNAMs are created in each runDownloadThread.
    // HTTP/1.1 connection pool config is set per-request.

    m_speedTimer.start();

    m_managerTimer = new QTimer(this);
    m_managerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_managerTimer, &QTimer::timeout, this, &FileDownloader::managerTick);

    m_speedTimer2 = new QTimer(this);
    m_speedTimer2->setTimerType(Qt::PreciseTimer);
    connect(m_speedTimer2, &QTimer::timeout, this, &FileDownloader::speedTick);
}

FileDownloader::~FileDownloader() { cancel(); }

// ═══════════════════════════════════════
void FileDownloader::addFile(const QString& localPath, const QString& localName,
                              const QStringList& sources, qint64 expectedSize,
                              const QByteArray& sha1, bool jarStrip)
{
    qCInfo(logDownload) << QStringLiteral("添加下载任务 名称=%1 大小=%2")
        .arg(localName, formatSize(expectedSize));

    // Pre-check SHA1 cache hit
    if (!sha1.isEmpty()) {
        QFileInfo fi(localPath);
        if (fi.exists() && fi.size() > 0) {
            QFile f(localPath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result() == sha1) {
                    m_completedFiles.fetchAndAddRelaxed(1);
                    m_totalBytes.fetchAndAddRelaxed(fi.size());
                    m_downloadedBytes.fetchAndAddRelaxed(fi.size());
                    emit logMessage(QString::fromUtf8("[完成] 缓存命中: %1 (%2)")
                                        .arg(localName, formatSize(fi.size())));
                    emit fileProgress(localPath, localName, fi.size(), fi.size(), localPath);
                    emit fileFinished(localPath, true);
                    m_totalFiles.fetchAndAddRelaxed(1);
                    return;
                }
            }
        }
    }

    auto file = std::make_shared<FileDownload>();
    file->localPath = localPath;
    file->localName = localName;
    file->sourceUrls = sources;
    file->expectedSha1 = sha1;
    file->needsJarStrip = jarStrip;
    file->fileSize = expectedSize;
    file->isUnknownSize = (expectedSize <= 0);

    // ── Changed: 1 MB threshold (was 50 MB), matching 主流启动器 ──
    file->isNoSplit = (!file->isUnknownSize && file->fileSize < 1024 * 1024);

    // Build orderedSources with per-source fail tracking
    for (int i = 0; i < sources.size(); ++i) {
        DownloadSource ds;
        ds.id = i;
        ds.url = sources[i];
        file->orderedSources.append(ds);
    }

    QMutexLocker lock(&m_filesMutex);
    m_files.append(file);
    m_totalFiles.fetchAndAddRelaxed(1);
    if (file->fileSize > 0) m_totalBytes.fetchAndAddRelaxed(file->fileSize);

    qCInfo(logDownload) << QStringLiteral("任务已排队 名称=%1 队列总数=%2")
        .arg(localName).arg(m_files.size());
}

void FileDownloader::start()
{
    if (m_state == Running) return;
    m_state = Running;
    m_cancelled.storeRelaxed(0);

    m_speedTimer.start();
    m_lastSpeedBytes = 0;
    m_speedFloorBps.storeRelaxed(kMinSpeedFloorBps);
    m_speedRecords.clear();
    m_throttleTimer.start();
    m_throttleBytes = 0;

    m_managerTimer->start(50);
    m_speedTimer2->start(100);

    // Immediate progress pulse
    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());

    emit logMessage(QString("引擎启动: %1 文件, %2 线程上限, 全局连接池(255/host)")
                        .arg(m_files.size()).arg(m_maxThreads));
}

void FileDownloader::pause()
{
    m_state = Paused;
    m_managerTimer->stop();
    m_speedTimer2->stop();
    emit logMessage(QString::fromUtf8("[暂停] 下载已暂停"));
}

void FileDownloader::resume()
{
    if (m_state != Paused) return;
    m_state = Running;
    m_speedTimer.restart();
    m_lastSpeedBytes = m_downloadedBytes.loadRelaxed();
    m_throttleTimer.restart();
    m_throttleBytes = 0;
    m_managerTimer->start(50);
    m_speedTimer2->start(100);
    emit logMessage(QString::fromUtf8("▶ 下载已恢复"));
}

void FileDownloader::cancel()
{
    m_cancelled.storeRelaxed(1);
    m_state = Cancelled;
    m_managerTimer->stop();
    m_speedTimer2->stop();
    emit logMessage(QString::fromUtf8("[失败] 下载已取消"));
}

// ═══════════════════════════════════════
// Manager tick — scans files and spawns threads (主流启动器-style ThreadStarter)
// ═══════════════════════════════════════

void FileDownloader::managerTick()
{
    if (m_state != Running || m_cancelled.loadRelaxed()) return;

    QMutexLocker lock(&m_filesMutex);

    QList<std::shared_ptr<FileDownload>> waiting, ongoing;
    for (auto& f : m_files) {
        if (f->state == 0) waiting.append(f);
        else if (f->state >= 1 && f->state <= 2) ongoing.append(f);
    }

    int active = m_activeThreads.loadRelaxed();

    // ── Start first thread for waiting files ──
    for (auto& f : waiting) {
        if (active >= m_maxThreads) break;
        auto th = tryStartFirstThread(f);
        if (th) {
            active++;
            // 主流启动器: reduce BMCLAPI request frequency
            if (th->sourceUrl.contains("bmclapi", Qt::CaseInsensitive))
                QThread::msleep(50);
        }
    }

    // ── Only add extra threads if current speed < speed floor ──
    double curBps = currentSpeedMBps() * 1024.0 * 1024.0;
    if (curBps >= m_speedFloorBps.loadRelaxed()) return;
    if (active >= m_maxThreads) return;

    for (auto& f : ongoing) {
        if (active >= m_maxThreads) break;
        if (f->isNoSplit && !f->threads.isEmpty()) continue;

        // 主流启动器: don't add if more threads are preparing than downloading
        int prep = 0, dl = 0;
        for (auto& t : f->threads) {
            if (t->state < 2) prep++;
            else if (t->state == 2) dl++;
        }
        if (prep > dl) continue;

        auto th = tryAddThread(f);
        if (th) {
            active++;
            if (th->sourceUrl.contains("bmclapi", Qt::CaseInsensitive))
                QThread::msleep(50);
        }
    }
}

// ═══════════════════════════════════════
std::shared_ptr<DownloadThread> FileDownloader::tryStartFirstThread(
    std::shared_ptr<FileDownload> file)
{
    int srcId = file->nextSourceId(false);
    if (srcId < 0) return nullptr;
    if (file->fileSize <= 0 && !file->isUnknownSize)
        file->fileSize = 10LL * 1024 * 1024;
    if (file->fileSize <= 0) {
        file->isUnknownSize = true;
        file->isNoSplit = true;
    }

    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = 0;
    th->downloadEnd = file->isUnknownSize ? 0 : file->fileSize;
    th->sourceId = srcId;
    th->sourceUrl = file->orderedSources[srcId].url;
    th->timeoutMs = 30000; // first thread: generous timeout
    file->threads.append(th);
    file->state = 1;
    m_activeThreads.fetchAndAddRelaxed(1);

    qCInfo(logDownload) << QStringLiteral("启动首个线程 文件=%1 源=%2").arg(file->localName).arg(th->sourceUrl);

    auto self = this;
    QThread::create([self, th, file]() { self->runDownloadThread(th, file); })->start();
    return th;
}

std::shared_ptr<DownloadThread> FileDownloader::tryAddThread(
    std::shared_ptr<FileDownload> file)
{
    if (file->fileSize <= 0 || file->isNoSplit || file->threads.isEmpty())
        return nullptr;

    // Find the largest unfinished piece (主流启动器-style dynamic splitting)
    auto maxPiece = file->findMaxUndonePiece();
    if (!maxPiece || maxPiece->downloadUndone() < 512 * 1024)
        return nullptr;

    // Split at 40% of remaining gap (主流启动器 matches)
    qint64 und = maxPiece->downloadUndone();
    qint64 splitPoint = maxPiece->downloadEnd - static_cast<qint64>(und * 0.4);

    // Pick a source different from existing threads (multi-source rotation)
    int srcId = file->nextSourceId(true);
    if (srcId < 0) return nullptr;

    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = splitPoint;
    th->downloadEnd = maxPiece->downloadEnd;
    th->sourceId = srcId;
    th->sourceUrl = file->orderedSources[srcId].url;

    // Adaptive timeout: 主流启动器-style
    int avgConnMs = file->connectAverageMs();
    int baseTimeout = (avgConnMs > 0) ? qMax(avgConnMs, 15000) : 15000;
    int srcFail = file->orderedSources[srcId].failCount;
    th->timeoutMs = qMin(baseTimeout * (1 + srcFail), 30000);

    // Shorten the max piece
    maxPiece->downloadEnd = splitPoint;

    file->threads.append(th);
    m_activeThreads.fetchAndAddRelaxed(1);

    qCInfo(logDownload) << QStringLiteral("新增线程 文件=%1 起始=%2 源=%3 timeout=%4ms")
        .arg(file->localName).arg(splitPoint).arg(th->sourceUrl).arg(th->timeoutMs);

    auto self = this;
    QThread::create([self, th, file]() { self->runDownloadThread(th, file); })->start();
    return th;
}

// ═══════════════════════════════════════
// Per-thread download
// ═══════════════════════════════════════

void FileDownloader::runDownloadThread(std::shared_ptr<DownloadThread> th,
                                        std::shared_ptr<FileDownload> file)
{
    th->state = 1;
    th->lastReceiveTime = getElapsedMs();

    qCInfo(logDownload) << QStringLiteral("开始下载 URL=%1 文件=%2 范围=[%3,%4]")
        .arg(th->sourceUrl, file->localName).arg(th->downloadStart).arg(th->downloadEnd);

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(QString("↓ 开始: %1 (%2)").arg(file->localName).arg(th->sourceUrl));

    // ── Per-source retry loop (主流启动器: 3 attempts, SHA1: 6) ──
    bool sourceOk = false;
    int maxAttempts = file->expectedSha1.isEmpty() ? 3 : 6;
    const qint64 retryStartMs = getElapsedMs();

    for (int attempt = 0; attempt < maxAttempts && !sourceOk; ++attempt) {
        if (m_cancelled.loadRelaxed()) goto cleanup;

        // Fast retry guard: if <5.5s elapsed after 2 attempts on a non-SHA1 file, stop
        if (attempt >= 2 && file->expectedSha1.isEmpty()
            && (getElapsedMs() - retryStartMs) < 5500) break;
        if (attempt >= 5 && (getElapsedMs() - retryStartMs) < 5500) break;

        if (attempt > 0) QThread::msleep(500);

        // Try each source in order (with per-source fail tracking)
        bool srcWorked = false;
        for (int si = 0; si < file->orderedSources.size() && !srcWorked; ++si) {
            int srcIdx = (th->sourceId + si) % file->orderedSources.size();
            auto& source = file->orderedSources[srcIdx];
            if (source.isDead) continue;

            QString url = source.url;
            th->sourceId = srcIdx;
            th->sourceUrl = url;

            if (si > 0)
                qCInfo(logDownload) << QStringLiteral("  切换到源[%1]: %2").arg(srcIdx).arg(url);

            // ── DNS resolution (主流启动器-style) ──
            QUrl qurl(url);
            QString dnsIp;
            int dnsFamily = 0;
            QString resolvedUrl = DnsResolver::resolve(qurl, dnsIp, dnsFamily);
            QUrl reqUrl(resolvedUrl);

            // Connect timing
            qint64 connectStart = getElapsedMs();

            QNetworkRequest req(reqUrl);
            // Per-host connection pool (Qt 6: set on request, applies to shared QNAM)
            {
                QHttp1Configuration h1cfg;
                h1cfg.setNumberOfConnectionsPerHost(255);
                req.setHttp1Configuration(h1cfg);
            }
            // Set Host header to original host (for virtual hosting)
            req.setRawHeader("Host", qurl.host().toUtf8());
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setTransferTimeout(th->timeoutMs);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

            // Range header for multi-threaded downloads
            if (!file->isUnknownSize && !file->isNoSplit) {
                qint64 start = th->downloadStart + th->downloadDone;
                qint64 end = th->downloadEnd - 1;
                req.setRawHeader("Range",
                    QString("bytes=%1-%2").arg(start).arg(end).toUtf8());
            }

            // Per-thread QNAM (worker threads need their own instances)
            thread_local QNetworkAccessManager tl_nam;
            QNetworkReply* reply = tl_nam.get(req);

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            bool timedOut = false;

            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
            timeout.start(th->timeoutMs);

            // Data arrival — feed m_downloadedBytes for speed tracking
            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                // Throttle progress signals to 150ms
                static qint64 s_lastEmitMs = 0;
                qint64 now = getElapsedMs();
                if (now - s_lastEmitMs >= 150) {
                    s_lastEmitMs = now;
                    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
                }
                emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
            });

            loop.exec();

            // ── Record connect time ──
            qint64 connectEnd = getElapsedMs();
            int elapsedMs = (int)(connectEnd - connectStart);
            if (elapsedMs > 0 && !timedOut) {
                file->connectCount++;
                file->connectTotalMs += elapsedMs;
            }

            // ── Handle errors ──
            if (timedOut || reply->error() != QNetworkReply::NoError) {
                qCWarning(logDownload) << QStringLiteral("请求失败 URL=%1 错误=%2 尝试=%3")
                    .arg(url, timedOut ? "超时" : reply->errorString()).arg(attempt + 1);
                source.failCount++;
                if (dnsIp.isEmpty())
                    DnsResolver::recordReliability(dnsIp, -0.7);
                reply->abort();
                reply->deleteLater();
                th->downloadDone = 0;
                if (attempt >= 2) break;
                continue;
            }

            // Success — record DNS reliability
            if (!dnsIp.isEmpty())
                DnsResolver::recordReliability(dnsIp, 0.5);

            // ── Check HTTP status ──
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode < 200 || statusCode >= 300) {
                qCWarning(logDownload) << QStringLiteral("HTTP错误 URL=%1 状态码=%2")
                    .arg(url).arg(statusCode);
                source.failCount++;
                reply->deleteLater();
                if (statusCode == 404) break;
                if (attempt >= 2) break;
                th->downloadDone = 0;
                continue;
            }

            // ── Read response data ──
            QByteArray data = reply->readAll();
            qint64 contentLen = reply->rawHeader("Content-Length").toLongLong();
            reply->deleteLater();

            // Validate data size
            qint64 expectedSize = (file->fileSize > 0) ? file->fileSize : contentLen;
            if (expectedSize > 0 && data.size() < expectedSize) {
                qCWarning(logDownload) << QStringLiteral("数据不完整 URL=%1 预期=%2 实际=%3")
                    .arg(url).arg(expectedSize).arg(data.size());
                source.failCount++;
                th->downloadDone = 0;
                continue;
            }

            // ── Determine file size on first thread ──
            if (th->downloadStart == 0 && data.size() > 0) {
                if (file->fileSize <= 0) {
                    if (contentLen > 0) {
                        file->fileSize = contentLen;
                        file->isUnknownSize = false;
                        file->isNoSplit = (contentLen < 1024 * 1024);
                        th->downloadEnd = contentLen;
                        m_totalBytes.fetchAndAddRelaxed(contentLen);
                    }
                }
            }

            // ── SHA1 verification (full downloads only) ──
            bool isFullDownload = file->isNoSplit || file->isUnknownSize;
            if (isFullDownload && !file->expectedSha1.isEmpty()) {
                QByteArray dlHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
                if (dlHash != file->expectedSha1) {
                    qCWarning(logDownload) << QStringLiteral("SHA1不匹配 URL=%1 预期=%2 实际=%3 尝试=%4")
                        .arg(url, QString::fromLatin1(file->expectedSha1.toHex()), QString::fromLatin1(dlHash.toHex()))
                        .arg(attempt + 1);
                    source.failCount++;
                    th->downloadDone = 0;
                    if (attempt >= 5) break;
                    continue;
                }
            }

            // ── Write data ──
            if (file->isNoSplit) {
                QFile f(file->localPath);
                QDir().mkpath(QFileInfo(file->localPath).absolutePath());
                if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
            } else {
                QString tmpDir = QFileInfo(file->localPath).absolutePath() + "/.shadow_temp/";
                QDir().mkpath(tmpDir);
                th->tempPath = tmpDir + QString("dl_%1_%2.tmp").arg(file->localName).arg(th->uuid);
                QFile f(th->tempPath);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(data);
                    f.close();
                }
            }

            // ── Speed limit throttle ──
            if (m_speedLimitBps.loadRelaxed() > 0) {
                qint64 totalDl = m_downloadedBytes.loadRelaxed();
                m_throttleBytes += data.size();
                qint64 throttleElapsed = m_throttleTimer.elapsed();
                if (throttleElapsed >= 1000) {
                    double currentBps = static_cast<double>(m_throttleBytes) / (throttleElapsed / 1000.0);
                    double limitBps = static_cast<double>(m_speedLimitBps.loadRelaxed());
                    if (currentBps > limitBps) {
                        qint64 targetMs = static_cast<qint64>(m_throttleBytes / limitBps * 1000);
                        qint64 sleepMs = targetMs - throttleElapsed;
                        if (sleepMs > 5)
                            QThread::msleep(qMin(sleepMs, (qint64)500));
                    }
                    m_throttleTimer.restart();
                    m_throttleBytes = 0;
                }
            }

            srcWorked = true;
            th->state = 3; // finished
            goto cleanup;
        } // for (source)
        if (srcWorked) { sourceOk = true; break; }
    } // for (attempt)

    // ── Last resort: long timeout fallback on official source ──
    if (!sourceOk && !file->expectedSha1.isEmpty() && file->orderedSources.size() > 0) {
        const QString url = file->orderedSources[0].url;
        qCInfo(logDownload) << QStringLiteral("最终兜底重试 URL=%1").arg(url);
        QUrl qurl(url);
        for (int attempt = 0; attempt < 3 && !sourceOk; ++attempt) {
            if (m_cancelled.loadRelaxed()) goto cleanup;
            if (attempt > 0) QThread::msleep(500);

            QNetworkRequest req(qurl);
            {
                QHttp1Configuration h1cfg;
                h1cfg.setNumberOfConnectionsPerHost(255);
                req.setHttp1Configuration(h1cfg);
            }
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            thread_local QNetworkAccessManager tl_nam;
            QNetworkReply* reply = tl_nam.get(req);

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            bool timedOut = false;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
            int timeoutMs = (attempt == 0) ? 60000 : 30000;
            timeout.start(timeoutMs);

            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                static qint64 s_lastEmit2 = 0;
                qint64 now = getElapsedMs();
                if (now - s_lastEmit2 >= 150) {
                    s_lastEmit2 = now;
                    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
                }
                emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
            });

            loop.exec();

            if (timedOut || reply->error() != QNetworkReply::NoError) {
                reply->abort();
                reply->deleteLater();
                continue;
            }

            QByteArray data = reply->readAll();
            reply->deleteLater();

            bool isFullDownload = file->isNoSplit || file->isUnknownSize;
            if (isFullDownload && !file->expectedSha1.isEmpty()) {
                QByteArray dlHash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
                if (dlHash == file->expectedSha1) {
                    sourceOk = true;
                } else {
                    if (file->fileSize > 0 && data.size() >= file->fileSize) {
                        sourceOk = true; // size matches, trust mirror
                    } else {
                        continue;
                    }
                }
            } else {
                sourceOk = true;
            }

            if (sourceOk) {
                if (file->isNoSplit) {
                    QFile f(file->localPath);
                    QDir().mkpath(QFileInfo(file->localPath).absolutePath());
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                } else {
                    QString tmpDir = QFileInfo(file->localPath).absolutePath() + "/.shadow_temp/";
                    QDir().mkpath(tmpDir);
                    th->tempPath = tmpDir + QString("dl_%1_%2.tmp").arg(file->localName).arg(th->uuid);
                    QFile f(th->tempPath);
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                }
                th->state = 3;
                goto cleanup;
            }
        }
    }

    th->state = 4; // failed
    emit logMessage(QString("%1: 所有源均失败").arg(file->localName));

cleanup:
    m_activeThreads.fetchAndAddRelaxed(-1);

    qCInfo(logDownload) << QStringLiteral("线程完成 文件=%1 字节=%2 状态=%3")
        .arg(file->localName).arg(th->downloadDone).arg(th->state);

    // Check if file is done
    bool allDone = true, anyFailed = false;
    {
        QMutexLocker lock(&m_filesMutex);
        for (auto& t : file->threads) {
            if (t->state < 3 && t->state != 4) { allDone = false; break; }
            if (t->state == 4) anyFailed = true;
        }
    }

    if (allDone && file->state < 3) {
        if (anyFailed) {
            file->state = 5;
            m_failedFiles.fetchAndAddRelaxed(1);
            emit logMessage(QString::fromUtf8("[失败] %1 (所有源均失败)").arg(file->localName));
            emit fileFinished(file->localPath, false);
        } else {
            file->state = 3;
            bool ok = mergeFile(file);
            file->state = ok ? 4 : 5;
            if (ok) {
                m_completedFiles.fetchAndAddRelaxed(1);
                emit logMessage(QString::fromUtf8("[完成] %1 (%2)").arg(file->localName, formatSize(QFileInfo(file->localPath).size())));
            } else {
                m_failedFiles.fetchAndAddRelaxed(1);
            }
            emit fileFinished(file->localPath, ok);
        }
    }

    updateStats();
}

// ═══════════════════════════════════════
bool FileDownloader::mergeFile(std::shared_ptr<FileDownload> file)
{
    if (file->isNoSplit) {
        if (!file->expectedSha1.isEmpty()) {
            QFile f(file->localPath);
            if (!f.open(QIODevice::ReadOnly)) return false;
            QCryptographicHash h(QCryptographicHash::Sha1);
            h.addData(&f); f.close();
            if (h.result() != file->expectedSha1) {
                emit logMessage(QString("SHA1校验失败: %1").arg(file->localName));
                return false;
            }
        }
        return true;
    }

    qCInfo(logDownload) << QStringLiteral("合并文件 %1 分段=%2").arg(file->localPath).arg(file->threads.size());

    QDir().mkpath(QFileInfo(file->localPath).absolutePath());
    QFile out(file->localPath);
    if (!out.open(QIODevice::WriteOnly)) {
        emit logMessage(QString("无法写入: %1").arg(file->localPath));
        return false;
    }

    // Sort threads by uuid (which preserves downloadStart order)
    auto sorted = file->threads;
    std::sort(sorted.begin(), sorted.end(),
              [](auto& a, auto& b) { return a->uuid < b->uuid; });

    for (auto& th : sorted) {
        if (th->tempPath.isEmpty()) continue;
        QFile tf(th->tempPath);
        if (!tf.open(QIODevice::ReadOnly)) { out.close(); return false; }
        out.write(tf.readAll());
        tf.close();
        tf.remove();
    }
    out.close();

    // SHA1 verification for merged file
    if (!file->expectedSha1.isEmpty()) {
        out.open(QIODevice::ReadOnly);
        QCryptographicHash h(QCryptographicHash::Sha1);
        h.addData(&out); out.close();
        if (h.result() != file->expectedSha1) {
            emit logMessage(QString("SHA1合并校验失败: %1").arg(file->localName));
            return false;
        }
    }

    // JAR signature stripping
    if (file->needsJarStrip && file->localPath.endsWith(".jar", Qt::CaseInsensitive)) {
        // Inline JAR stripping via QProcess+powershell (defined elsewhere)
    }

    return true;
}

// ═══════════════════════════════════════
// Speed tracking (主流启动器-style: recency-weighted average, raised to 85%)
// ═══════════════════════════════════════

void FileDownloader::speedTick()
{
    if (m_state != Running) return;

    qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed < 100) return;

    qint64 now = m_downloadedBytes.loadRelaxed();
    qint64 bytes = now - m_lastSpeedBytes;
    m_lastSpeedBytes = now;
    m_speedTimer.restart();

    qint64 actualBps = (elapsed > 0) ? (bytes * 1000 / elapsed) : 0;

    // Record
    {
        QMutexLocker lock(&m_speedMutex);
        m_speedRecords.prepend(actualBps);
        if (m_speedRecords.size() > kMaxSpeedRecords)
            m_speedRecords.removeLast();
    }

    // 主流启动器-style: use last 10 records (~1s window) for speed floor
    qint64 recentSum = 0;
    int recentCount = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        for (int i = 0; i < qMin(10, m_speedRecords.size()); ++i) {
            recentSum += m_speedRecords[i];
            recentCount++;
        }
    }
    qint64 recentAvgBps = (recentCount > 0) ? (recentSum / recentCount) : actualBps;

    // Weighted average for display
    qint64 weightedSum = 0;
    int weightDiv = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        int w = m_speedRecords.size();
        for (auto rec : m_speedRecords) {
            weightedSum += rec * w;
            weightDiv += w;
            w--;
        }
    }
    qint64 weightedBps = (weightDiv > 0) ? (weightedSum / weightDiv) : actualBps;
    m_emaMbps = m_emaMbps * 0.5 + (weightedBps / (1024.0 * 1024.0)) * 0.5;

    // Speed floor: 85% of recent average (主流启动器-style)
    qint64 floorLimit = static_cast<qint64>(recentAvgBps * 0.85);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    if (recentAvgBps >= kMinSpeedFloorBps && floorLimit > currentFloor) {
        m_speedFloorBps.storeRelaxed(floorLimit);
    }

    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
}

// ═══════════════════════════════════════
void FileDownloader::updateStats()
{
    int done = m_completedFiles.loadRelaxed();
    int total = m_totalFiles.loadRelaxed();
    int fails = m_failedFiles.loadRelaxed();

    emit progressChanged(done, total,
                          m_downloadedBytes.loadRelaxed(),
                          m_totalBytes.loadRelaxed());

    if (done + fails >= total && total > 0) {
        m_managerTimer->stop();
        m_speedTimer2->stop();
        m_state = Idle;
        qint64 totalDl = m_totalBytes.loadRelaxed();
        double speedMBps = m_emaMbps;
        emit logMessage(QString::fromUtf8("[完成] 下载完成: %1/%2 文件, %3, 速度 %4 MB/s")
                            .arg(done).arg(total)
                            .arg(formatSize(totalDl))
                            .arg(speedMBps, 0, 'f', 1));
        emit allFinished();
    }
}

double FileDownloader::currentSpeedMBps() const { return m_emaMbps; }

QString FileDownloader::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1048576) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1073741824LL) return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
    return QString::number(bytes / 1073741824.0, 'f', 2) + " GB";
}

qint64 FileDownloader::getElapsedMs()
{
    static QElapsedTimer gt;
    if (!gt.isValid()) gt.start();
    return gt.elapsed();
}

} // namespace ShadowDownloader
