// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow
#include "icon_cache.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QDebug>

namespace ShadowLauncher {

IconCache::IconCache(const QString &cacheDir, QObject *parent)
    : QObject(parent)
    , m_cacheDir(cacheDir)
    , m_nam(new QNetworkAccessManager(this))
{
    QDir().mkpath(m_cacheDir);
    qDebug() << "[IconCache] cache dir:" << m_cacheDir;
}

QString IconCache::hashUrl(const QString &url) const
{
    QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1);
    return QString::fromLatin1(hash.toHex()).left(16);
}

QString IconCache::localPathFor(const QString &url) const
{
    return m_cacheDir + "/" + hashUrl(url) + ".png";
}

QString IconCache::cachedPath(const QString &url) const
{
    QString lp = localPathFor(url);
    if (QFileInfo::exists(lp)) {
        return QUrl::fromLocalFile(lp).toString();
    }
    return {};
}

QString IconCache::resolveUrl(const QString &url)
{
    QString cached = cachedPath(url);
    if (!cached.isEmpty()) {
        return cached;
    }
    if (!m_pendingDownloads.contains(url)) {
        downloadOne(url);
    }
    return url;
}

void IconCache::downloadOne(const QString &url)
{
    m_pendingDownloads.insert(url);
    QNetworkReply *reply = m_nam->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [this, url, reply]() {
        reply->deleteLater();
        m_pendingDownloads.remove(url);
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "[IconCache] download failed:" << url << reply->errorString();
            return;
        }
        QByteArray data = reply->readAll();
        if (data.isEmpty()) return;

        QString lp = localPathFor(url);
        QFile f(lp);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
            qDebug() << "[IconCache] cached:" << lp << data.size() << "bytes";
            emit iconCached(url, QUrl::fromLocalFile(lp).toString());
        }
    });
}

void IconCache::cacheBatchAsync(const QStringList &urls)
{
    for (const QString &url : urls) {
        if (cachedPath(url).isEmpty() && !m_pendingDownloads.contains(url)) {
            downloadOne(url);
        }
    }
}

} // namespace ShadowLauncher
