#include "java_backend.h"
#include "core/file_downloader.h"
#include <QNetworkReply>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>

using namespace ShadowDownloader;

namespace ShadowLauncher {

JavaBackend::JavaBackend(QObject *parent)
    : QObject(parent)
    , m_baseUrl("https://mirrors.tuna.tsinghua.edu.cn/Adoptium")
    , m_nam(new QNetworkAccessManager(this))
{
    refreshVersions();
}

// ── Setters trigger cascading fetches ──

void JavaBackend::setSelectedVersion(const QString &v) {
    if (m_selVersion == v) return;
    m_selVersion = v;
    setSelectedType("");  // cascades → setSelectedArch("") → setSelectedOS("")
    m_types.clear();
    emit selectedVersionChanged();
    emit javaTypesChanged();
    if (!v.isEmpty()) fetchTypes();
}

void JavaBackend::setSelectedType(const QString &t) {
    if (m_selType == t) return;
    m_selType = t;
    setSelectedArch("");  // cascades → setSelectedOS("")
    m_archs.clear();
    emit selectedTypeChanged();
    emit javaArchsChanged();
    if (!t.isEmpty()) fetchArchs();
}

void JavaBackend::setSelectedArch(const QString &a) {
    if (m_selArch == a) return;
    m_selArch = a;
    setSelectedOS("");
    m_oses.clear();
    emit selectedArchChanged();
    emit javaOSesChanged();
    if (!a.isEmpty()) fetchOSes();
}

void JavaBackend::setSelectedOS(const QString &o) {
    if (m_selOS == o) return;
    m_selOS = o;
    m_files.clear();
    emit selectedOSChanged();
    emit javaFilesChanged();
    if (!o.isEmpty()) fetchFiles();
}

// ── Fetch methods ──

void JavaBackend::refreshVersions() {
    fetchUrl(m_baseUrl + "/", [this](const QStringList &dirs) {
        m_versions.clear();
        for (const auto &d : dirs) {
            // Only numeric version directories
            bool ok;
            d.toInt(&ok);
            if (ok) m_versions.append(d);
        }
        std::sort(m_versions.begin(), m_versions.end(), [](const QString &a, const QString &b) {
            return a.toInt() > b.toInt();
        });
        emit javaVersionsChanged();
    });
}

void JavaBackend::fetchTypes() {
    QString url = m_baseUrl + "/" + m_selVersion + "/";
    fetchUrl(url, [this](const QStringList &dirs) {
        m_types = dirs;
        emit javaTypesChanged();
    });
}

void JavaBackend::fetchArchs() {
    QString url = m_baseUrl + "/" + m_selVersion + "/" + m_selType + "/";
    fetchUrl(url, [this](const QStringList &dirs) {
        m_archs = dirs;
        emit javaArchsChanged();
    });
}

void JavaBackend::fetchOSes() {
    QString url = m_baseUrl + "/" + m_selVersion + "/" + m_selType + "/" + m_selArch + "/";
    fetchUrl(url, [this](const QStringList &dirs) {
        m_oses = dirs;
        emit javaOSesChanged();
    });
}

void JavaBackend::fetchFiles() {
    QString url = m_baseUrl + "/" + m_selVersion + "/" + m_selType + "/" + m_selArch + "/" + m_selOS + "/";
    fetchFileList(url, [this](const QVariantList &files) {
        m_files = files;
        emit javaFilesChanged();
    });
}

// ── URL fetching + parsing ──

void JavaBackend::fetchUrl(const QString &url,
                           std::function<void(const QStringList &)> callback,
                           std::function<void(int, const QString &)> onError)
{
    m_fetching = true;
    emit fetchingChanged();

    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, onError]() {
        reply->deleteLater();
        m_fetching = false;
        emit fetchingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            emit logMessage(QString("获取列表失败: %1").arg(reply->errorString()));
            if (onError) onError(reply->error(), reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        QStringList items = parseDirListing(data, false);
        callback(items);
    });
}

void JavaBackend::fetchFileList(const QString &url,
                                std::function<void(const QVariantList &)> callback)
{
    m_fetching = true;
    emit fetchingChanged();

    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    QNetworkReply *reply = m_nam->get(req);

    QString capturedUrl = url;  // copy for lambda
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, capturedUrl]() {
        reply->deleteLater();
        m_fetching = false;
        emit fetchingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            emit logMessage(QString("获取文件列表失败: %1").arg(reply->errorString()));
            return;
        }

        QByteArray data = reply->readAll();

        // Parse only <td class="link"> entries with size info
        static QRegularExpression re(
            R"(<td class="link">\s*<a\s+href=\"([^\"]+)\"[^>]*>([^<]+)</a>\s*</td>\s*<td[^>]*>([^<]+)</td>)",
            QRegularExpression::CaseInsensitiveOption
        );

        QVariantList files;
        auto it = re.globalMatch(QString::fromUtf8(data));
        while (it.hasNext()) {
            auto m = it.next();
            QString href = m.captured(1);
            QString name = m.captured(2);
            QString sizeStr = m.captured(3).trimmed();

            // Skip directories (href ends with /)
            if (href.endsWith("/")) continue;
            // Skip parent directory
            if (href == "../") continue;

            if (name.isEmpty()) name = href;

            // Parse size
            qint64 size = 0;
            if (!sizeStr.isEmpty() && sizeStr != "-") {
                // Could be "141.1 MB" or "5242880" format
                static QRegularExpression sizeRe(R"(([\d.]+)\s*([KMGT]?B?))");
                auto sm = sizeRe.match(sizeStr);
                if (sm.hasMatch()) {
                    double val = sm.captured(1).toDouble();
                    QString unit = sm.captured(2).toUpper();
                    if (unit == "KB" || unit == "K") val *= 1024;
                    else if (unit == "MB" || unit == "M") val *= 1024 * 1024;
                    else if (unit == "GB" || unit == "G") val *= 1024 * 1024 * 1024;
                    else if (unit == "TB" || unit == "T") val *= 1024LL * 1024 * 1024 * 1024;
                    size = (qint64)val;
                } else {
                    size = sizeStr.toLongLong();
                }
            }

            // Only include .msi and .zip (and .tar.gz)
            QString lower = name.toLower();
            if (!lower.endsWith(".msi") && !lower.endsWith(".zip") && !lower.endsWith(".tar.gz"))
                continue;

            QJsonObject obj;
            obj["name"] = name;
            obj["url"] = capturedUrl + name;
            obj["size"] = size;
            obj["ext"] = lower.endsWith(".msi") ? "msi" : (lower.endsWith(".tar.gz") ? "tar.gz" : "zip");
            files.append(obj.toVariantMap());
        }

        emit logMessage(QString("找到 %1 个安装包").arg(files.size()));
        callback(files);
    });
}

QStringList JavaBackend::parseDirListing(const QByteArray &html, bool filesOnly)
{
    QStringList items;

    // Match <td class="link"><a href="name/"> entries (avoids footer links)
    static QRegularExpression re(
        R"(<td class="link">\s*<a\s+href=\"([^\"]+)\"[^>]*>([^<]*)</a>\s*</td>)",
        QRegularExpression::CaseInsensitiveOption
    );

    auto it = re.globalMatch(QString::fromUtf8(html));
    while (it.hasNext()) {
        auto m = it.next();
        QString href = m.captured(1);
        QString text = m.captured(2);

        // Skip parent dir
        if (href == "../" || href == "/") continue;

        // Get clean name
        QString name = text.isEmpty() ? href : text;
        name.replace("/", "");

        if (filesOnly) {
            // Only files, not dirs
            if (href.endsWith("/")) continue;
            items.append(name);
        } else {
            // Only dirs, not files
            if (!href.endsWith("/")) continue;
            items.append(name);
        }
    }

    items.removeDuplicates();
    return items;
}

// ── Download ──

QString JavaBackend::fileDownloadUrl(const QString &filename) const
{
    return m_baseUrl + "/" + m_selVersion + "/" + m_selType + "/"
           + m_selArch + "/" + m_selOS + "/" + filename;
}

QString JavaBackend::javaInstallDir() const
{
    // Save to launcher's java cache directory
    return QDir::currentPath() + "/java_cache";
}

void JavaBackend::downloadJavaFileTo(const QString &filename, const QString &outDir, qint64 expectedSize)
{
    QString url = fileDownloadUrl(filename);

    // Recreate downloader each time so totalBytes resets to 0
    if (m_downloader) {
        m_downloader->deleteLater();
        m_downloader = nullptr;
    }

    m_downloader = new FileDownloader(this);
    m_downloader->setMaxThreads(64);

    connect(m_downloader, &FileDownloader::progressChanged, this,
            [this](int, int, qint64 dl, qint64 total) {
                double speed = m_downloader->currentSpeedMBps();
                int pct = total > 0 ? (int)(dl * 100 / total) : 0;
                emit downloadProgress(pct, dl, total, speed);
            });

    connect(m_downloader, &FileDownloader::fileFinished, this,
        [this](const QString &path, bool ok) {
            emit downloadFinished(ok, path);
        });

    connect(m_downloader, &FileDownloader::logMessage, this,
        [this](const QString &msg) {
            emit logMessage(msg);
        });

    QDir().mkpath(outDir);
    QString outPath = outDir + "/" + filename;
    QFile::remove(outPath);

    m_downloader->addFile(outPath, filename, {url}, expectedSize, QByteArray());
    m_downloader->start();

    emit logMessage(QString("开始下载: %1").arg(filename));
}

void JavaBackend::downloadJavaFile(const QString &filename)
{
    downloadJavaFileTo(filename, javaInstallDir(), 0);
}

void JavaBackend::cancelDownload()
{
    if (m_downloader) {
        // FileDownloader doesn't have cancel, but we can delete it
        m_downloader->deleteLater();
        m_downloader = nullptr;
    }
}

} // namespace ShadowLauncher
