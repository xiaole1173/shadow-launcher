#pragma once
#include <QObject>
#include <QString>
#include <QSet>
#include <QNetworkAccessManager>
#include <QCryptographicHash>

namespace ShadowLauncher {

class IconCache : public QObject
{
    Q_OBJECT
public:
    explicit IconCache(const QString &cacheDir, QObject *parent = nullptr);

    // Returns local file:/// path if cached, or empty QString if not
    Q_INVOKABLE QString cachedPath(const QString &url) const;

    // Resolve: returns local path if cached, otherwise returns original URL
    // and starts async download
    Q_INVOKABLE QString resolveUrl(const QString &url);

    // Batch download all uncached icons from a list of URLs
    Q_INVOKABLE void cacheBatchAsync(const QStringList &urls);

signals:
    void iconCached(const QString &url, const QString &localPath);

private:
    void downloadOne(const QString &url);
    QString hashUrl(const QString &url) const;
    QString localPathFor(const QString &url) const;

    QString m_cacheDir;
    QNetworkAccessManager *m_nam;
    QSet<QString> m_pendingDownloads;
};

} // namespace ShadowLauncher
