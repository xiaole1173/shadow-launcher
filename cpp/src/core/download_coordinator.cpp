// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "download_coordinator.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QDebug>

namespace ShadowLauncher {

DownloadCoordinator::DownloadCoordinator(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

DownloadCoordinator::~DownloadCoordinator() = default;

void DownloadCoordinator::addSource(const QString& taskId, const QString& url)
{
    Source s;
    s.taskId = taskId;
    s.url = url;
    m_sources.append(s);
}

void DownloadCoordinator::start()
{
    if (m_sources.isEmpty()) {
        emit ready(0, 0);
        return;
    }

    emit phaseChanged(tr("连通性测试..."));
    runHeads();
}

void DownloadCoordinator::runHeads()
{
    m_pendingHeads = m_sources.size();
    m_totalBytes = 0;
    m_failedSources.clear();
    m_readyEmitted = false;
    m_winnerIndex = -1;

    for (const auto& src : m_sources) {
        QUrl url(src.url);
        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
        req.setTransferTimeout(10000);

        QNetworkReply* reply = m_nam->head(req);
        reply->setProperty("taskId", src.taskId);
        connect(reply, &QNetworkReply::finished, this, &DownloadCoordinator::onHeadReply);
    }

    qDebug() << "[Coordinator] Testing" << m_pendingHeads << "sources...";
}

void DownloadCoordinator::onHeadReply()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    // Already declared a winner — ignore late replies
    if (m_readyEmitted) return;

    QString taskId = reply->property("taskId").toString();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[Coordinator] FAILED:" << taskId << reply->errorString();
        m_failedSources.insert(taskId);
        emit sourceTested(taskId, false);
        m_pendingHeads--;
        if (m_pendingHeads <= 0) {
            onAllHeadsDone();
        }
        return;
    }

    // Collect Content-Length
    QByteArray cl = reply->rawHeader("Content-Length");
    bool ok = false;
    qint64 bytes = cl.toLongLong(&ok);
    if (ok && bytes > 0) {
        for (int i = 0; i < m_sources.size(); ++i) {
            if (m_sources[i].taskId == taskId) {
                m_sources[i].totalBytes = bytes;
                m_totalBytes = bytes;  // only count the winner's bytes
                m_winnerIndex = i;
                break;
            }
        }
    }

    qDebug() << "[Coordinator] OK:" << taskId << "size=" << bytes;
    emit sourceTested(taskId, true);

    // First source to pass wins — don't wait for slow/blocked sources
    m_readyEmitted = true;
    emit phaseChanged(tr("准备下载..."));
    emit ready(m_winnerIndex >= 0 ? m_winnerIndex : 0, m_totalBytes);
}

void DownloadCoordinator::onAllHeadsDone()
{
    // Check if all sources failed
    const int totalSources = m_sources.size();
    if (m_failedSources.size() >= totalSources) {
        qWarning() << "[Coordinator] ALL sources failed." << m_failedSources;
        for (const QString& taskId : m_failedSources) {
            emit connectivityFailed(taskId, tr("网络不可达"));
        }
        return;
    }

    // At least one source passed — find the first passed source index
    int passedIndex = 0;
    for (int i = 0; i < m_sources.size(); ++i) {
        if (!m_failedSources.contains(m_sources[i].taskId)) {
            passedIndex = i;
            break;
        }
    }

    emit phaseChanged(tr("准备下载..."));
    qDebug() << "[Coordinator] Using source index" << passedIndex
             << "Total bytes:" << m_totalBytes
             << "Failed:" << m_failedSources;
    emit ready(passedIndex, m_totalBytes);
}

} // namespace ShadowLauncher
