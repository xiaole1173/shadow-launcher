// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// Per-file download engine (v8).
// See file_downloader.h for architecture overview.

#include "core/file_downloader.h"
#include "utils/logger.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QEventLoop>
#include <algorithm>

namespace ShadowDownloader {

// ═══════════════════════════════════════
FileDownloader::FileDownloader(QObject* parent) : QObject(parent)
{
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
    // Pre-check SHA1 cache hit
    if (!sha1.isEmpty()) {
        QFileInfo fi(localPath);
        if (fi.exists() && fi.size() > 0) {
            QFile f(localPath);
            if (f.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha1);
                hash.addData(&f);
                f.close();
                if (hash.result().toHex() == sha1) {
                    m_completedFiles.fetchAndAddRelaxed(1);
                    m_totalBytes.fetchAndAddRelaxed(fi.size());
                    m_downloadedBytes.fetchAndAddRelaxed(fi.size());
                    emit logMessage(QString::fromUtf8("[完成] 缓存命中: %1 (%2)").arg(localName, formatSize(fi.size())));
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
    file->orderedSources = sources;
    file->expectedSha1 = sha1;
    file->needsJarStrip = jarStrip;
    file->fileSize = expectedSize;
    file->isUnknownSize = (expectedSize <= 0);
    file->isNoSplit = (!file->isUnknownSize && file->fileSize < 1024 * 1024);

    QMutexLocker lock(&m_filesMutex);
    m_files.append(file);
    m_totalFiles.fetchAndAddRelaxed(1);
    if (file->fileSize > 0) m_totalBytes.fetchAndAddRelaxed(file->fileSize);
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

    m_managerTimer->start(50);
    m_speedTimer2->start(100);

    // Immediate progress pulse (in case timer hasn't fired yet)
    emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                          m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());

    emit logMessage(QString("引擎启动: %1 文件, %2 线程上限").arg(m_files.size()).arg(m_maxThreads));
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

    // Start first thread for waiting files
    for (auto& f : waiting) {
        if (active >= m_maxThreads) break;
        auto th = tryStartFirstThread(f);
        if (th) { active++; if (th->sourceUrl.contains("bmclapi")) QThread::msleep(50); }
    }

    if (active >= m_maxThreads) return;

    // Expand threads (speed < floor → add)
    double curMbps = currentSpeedMBps();
    if (curMbps * 1024 * 1024 >= m_speedFloorBps.loadRelaxed()) return;

    for (auto& f : ongoing) {
        if (active >= m_maxThreads) break;
        if (f->isNoSplit && !f->threads.isEmpty()) continue;

        int prep = 0, dl = 0;
        for (auto& t : f->threads) {
            if (t->state < 2) prep++; else if (t->state == 2) dl++;
        }
        if (prep > dl) continue;

        auto th = tryAddThread(f);
        if (th) { active++; if (th->sourceUrl.contains("bmclapi")) QThread::msleep(50); }
    }
}

// ═══════════════════════════════════════
std::shared_ptr<DownloadThread> FileDownloader::tryStartFirstThread(
    std::shared_ptr<FileDownload> file)
{
    if (file->orderedSources.isEmpty()) return nullptr;
    if (file->fileSize <= 0 && !file->isUnknownSize) file->fileSize = 10LL * 1024 * 1024;
    if (file->fileSize <= 0) { file->isUnknownSize = true; file->isNoSplit = true; }

    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = 0;
    th->downloadEnd = file->isUnknownSize ? 0 : file->fileSize;
    th->sourceUrl = file->orderedSources.first();
    file->threads.append(th);
    file->state = 1;
    m_activeThreads.fetchAndAddRelaxed(1);

    emit logMessage(QString("▶ 启动线程: %1 [%2 → %3] %4")
                        .arg(file->localName)
                        .arg(th->downloadStart).arg(th->downloadEnd)
                        .arg(th->sourceUrl));

    auto self = this;
    QThread::create([self, th, file]() { self->runDownloadThread(th, file); })->start();
    return th;
}

std::shared_ptr<DownloadThread> FileDownloader::tryAddThread(
    std::shared_ptr<FileDownload> file)
{
    if (file->fileSize <= 0 || file->isNoSplit || file->threads.isEmpty()) return nullptr;

    std::shared_ptr<DownloadThread> maxPiece;
    qint64 maxUndone = 0;
    for (auto& t : file->threads) {
        if (t->state >= 3) continue;
        qint64 u = t->downloadUndone();
        if (u > maxUndone) { maxUndone = u; maxPiece = t; }
    }
    if (!maxPiece || maxUndone < 512 * 1024) return nullptr;

    qint64 splitPoint = maxPiece->downloadEnd - static_cast<qint64>(maxUndone * 0.4);
    auto th = std::make_shared<DownloadThread>();
    th->uuid = m_nextUuid.fetchAndAddRelaxed(1);
    th->downloadStart = splitPoint;
    th->downloadEnd = maxPiece->downloadEnd;
    th->sourceUrl = file->orderedSources.first();
    maxPiece->downloadEnd = splitPoint;
    file->threads.append(th);
    m_activeThreads.fetchAndAddRelaxed(1);

    auto self = this;
    QThread::create([self, th, file]() { self->runDownloadThread(th, file); })->start();
    return th;
}

// ═══════════════════════════════════════
void FileDownloader::runDownloadThread(std::shared_ptr<DownloadThread> th,
                                        std::shared_ptr<FileDownload> file)
{
    th->state = 1;
    th->lastReceiveTime = getElapsedMs();

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(QString("↓ 开始下载: %1 (%2)").arg(file->localName).arg(th->sourceUrl));

    QNetworkAccessManager mgr;

    for (int sourceIdx = 0; sourceIdx < file->orderedSources.size(); ++sourceIdx) {
        if (m_cancelled.loadRelaxed()) goto cleanup;

        QString url = file->orderedSources[sourceIdx];
        th->sourceUrl = url;

        QUrl qurl(url);

        // 3-attempt per source: 10s → 30s → 4s
        bool sourceOk = false;
        qint64 startTimeMs = getElapsedMs();
        for (int attempt = 0; attempt < 3 && !sourceOk; ++attempt) {
            if (m_cancelled.loadRelaxed()) goto cleanup;

            int timeoutMs;
            switch (attempt) {
                case 0: timeoutMs = 10000; break;
                case 1: timeoutMs = 30000; break;
                default: timeoutMs = 4000; break;
            }
            if (attempt >= 2 && (getElapsedMs() - startTimeMs) < 5500) break;
            if (attempt > 0) QThread::msleep(500);

            QNetworkRequest req{qurl};
            req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
            req.setTransferTimeout(timeoutMs);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

            if (!file->isUnknownSize) {
                qint64 start = th->downloadStart + th->downloadDone;
                qint64 end = th->downloadEnd - 1;
                req.setRawHeader("Range", QString("bytes=%1-%2").arg(start).arg(end).toUtf8());
            }

            QNetworkReply* reply = mgr.get(req);

            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            bool timedOut = false;

            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
            timeout.start(timeoutMs);

            connect(reply, &QNetworkReply::downloadProgress,
                    [&](qint64 received, qint64 total) {
                qint64 delta = received - th->downloadDone;
                if (delta > 0) {
                    m_downloadedBytes.fetchAndAddRelaxed(delta);
                    th->downloadDone = received;
                }
                th->lastReceiveTime = getElapsedMs();
                emit fileProgress(th->sourceUrl, file->localName, received, total, file->localPath);
                emit progressChanged(m_completedFiles.loadRelaxed(), m_totalFiles.loadRelaxed(),
                                      m_downloadedBytes.loadRelaxed(), m_totalBytes.loadRelaxed());
            });

            loop.exec();

            if (timedOut || reply->error() != QNetworkReply::NoError) {
                reply->abort();
                reply->deleteLater();
                continue;
            }

            sourceOk = true;
            QByteArray data = reply->readAll();
            qint64 contentLen = reply->rawHeader("Content-Length").toLongLong();
            reply->deleteLater();

            // Determine file size on first thread
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

            // Write data
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

            th->state = 3; // finished
            goto cleanup;
        } // for (attempt)
    } // for (sourceIdx)

    th->state = 4; // failed
    emit logMessage(QString("%1: 所有源均失败").arg(file->localName));

cleanup:
    m_activeThreads.fetchAndAddRelaxed(-1);

    // Check if file done
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
            emit logMessage(QString::fromUtf8("[失败] 下载失败: %1 (所有源均失败)").arg(file->localName));
            emit fileFinished(file->localPath, false);
        } else {
            file->state = 3;
            bool ok = mergeFile(file);
            file->state = ok ? 4 : 5;
            if (ok) m_completedFiles.fetchAndAddRelaxed(1);
            else m_failedFiles.fetchAndAddRelaxed(1);
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
            if (h.result().toHex() != file->expectedSha1) {
                emit logMessage(QString("SHA1校验失败: %1").arg(file->localName));
                return false;
            }
        }
        return true;
    }

    QDir().mkpath(QFileInfo(file->localPath).absolutePath());
    QFile out(file->localPath);
    if (!out.open(QIODevice::WriteOnly)) {
        emit logMessage(QString("无法写入: %1").arg(file->localPath));
        return false;
    }

    auto sorted = file->threads;
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a->uuid < b->uuid; });

    for (auto& th : sorted) {
        if (th->tempPath.isEmpty()) continue;
        QFile tf(th->tempPath);
        if (!tf.open(QIODevice::ReadOnly)) { out.close(); return false; }
        out.write(tf.readAll());
        tf.close();
        tf.remove();
    }
    out.close();

    if (!file->expectedSha1.isEmpty()) {
        out.open(QIODevice::ReadOnly);
        QCryptographicHash h(QCryptographicHash::Sha1);
        h.addData(&out); out.close();
        if (h.result().toHex() != file->expectedSha1) {
            emit logMessage(QString("SHA1合并校验失败: %1").arg(file->localName));
            return false;
        }
    }
    return true;
}

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

    qint64 actualBps = bytes * 1000 / elapsed;

    {
        QMutexLocker lock(&m_speedMutex);
        m_speedRecords.prepend(actualBps);
        if (m_speedRecords.size() > kMaxSpeedRecords) m_speedRecords.removeLast();
    }

    qint64 weightedSum = 0;
    int weightDiv = 0;
    {
        QMutexLocker lock(&m_speedMutex);
        int w = m_speedRecords.size();
        for (auto rec : m_speedRecords) { weightedSum += rec * w; weightDiv += w; w--; }
    }
    qint64 currentBps = (weightDiv > 0) ? (weightedSum / weightDiv) : actualBps;

    m_emaMbps = m_emaMbps * 0.5 + (currentBps / (1024.0 * 1024.0)) * 0.5;

    qint64 floorLimit = static_cast<qint64>(currentBps * 0.85);
    qint64 currentFloor = m_speedFloorBps.loadRelaxed();
    if (currentBps >= kMinSpeedFloorBps && floorLimit > currentFloor) {
        m_speedFloorBps.storeRelaxed(floorLimit);
    }

    // ── Periodic progress emission (for UI polling) ──
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
        const qint64 totalDl = m_totalBytes.loadRelaxed();
        const double speedMBps = m_emaMbps;
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
