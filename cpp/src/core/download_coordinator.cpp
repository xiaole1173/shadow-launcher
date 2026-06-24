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
        emit ready(0);
        return;
    }

    emit phaseChanged(QStringLiteral("连通性测试..."));
    runHeads();
}

void DownloadCoordinator::runHeads()
{
    m_pendingHeads = m_sources.size();
    m_totalBytes = 0;

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

    QString taskId = reply->property("taskId").toString();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "[Coordinator] FAILED:" << taskId << reply->errorString();
        emit connectivityFailed(taskId, reply->errorString());
        return;
    }

    // Collect Content-Length
    QByteArray cl = reply->rawHeader("Content-Length");
    bool ok = false;
    qint64 bytes = cl.toLongLong(&ok);
    if (ok && bytes > 0) {
        for (auto& src : m_sources) {
            if (src.taskId == taskId) {
                src.totalBytes = bytes;
                m_totalBytes += bytes;
                break;
            }
        }
    }

    qDebug() << "[Coordinator] OK:" << taskId << "size=" << bytes;

    m_pendingHeads--;
    if (m_pendingHeads <= 0) {
        onAllHeadsDone();
    }
}

void DownloadCoordinator::onAllHeadsDone()
{
    emit phaseChanged(QStringLiteral("准备下载..."));
    qDebug() << "[Coordinator] All sources OK. Total bytes:" << m_totalBytes;
    emit ready(m_totalBytes);
}

} // namespace ShadowLauncher
