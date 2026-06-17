// Shadow Launcher — VersionBackend
// QML-facing backend for version list, installation, and lifecycle management.
// Bridges VersionManager (fetch/cache) and VersionDownloader (install pipeline).

#include "version_backend.h"
#include "../core/version_manager.h"
#include "../core/version_downloader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace ShadowLauncher {

// ============================================================
// Construction / Destruction
// ============================================================

VersionBackend::VersionBackend(QObject* parent)
    : QObject(parent)
{
    // ── VersionManager: fetch + cache version manifest ──
    m_versionMgr = new VersionManager(this);
    m_versionMgr->setDataDir(QDir::homePath() + QStringLiteral("/.shadow"));
    m_versionMgr->setGameDir(QDir::homePath() + QStringLiteral("/.shadow/minecraft"));

    // Manifest ready → populate m_versionIds → notify QML
    connect(m_versionMgr, &VersionManager::versionsReady, this,
            [this](const QVector<McVersion>& versions) {
                m_versionIds.clear();
                for (const auto& v : versions) {
                    m_versionIds.append(v.id);
                }
                emit logMessage(
                    QStringLiteral("版本列表已加载 (%1 个版本)")
                        .arg(m_versionIds.size()));
                emit versionListReady();
            });

    // Manifest fetch error
    connect(m_versionMgr, &VersionManager::fetchError, this,
            [this](const QString& err) {
                emit logMessage(
                    QStringLiteral("获取版本列表失败: %1").arg(err));
            });

    // Initial fetch
    refreshVersionList();
    refreshInstalled();
}

VersionBackend::~VersionBackend() = default;

// ============================================================
// Slots — version selection
// ============================================================

void VersionBackend::setSelectedVersion(const QString& versionId)
{
    if (m_selectedVersion != versionId) {
        m_selectedVersion = versionId;
        emit selectedVersionChanged();
    }
}

// ============================================================
// Slots — version list
// ============================================================

QVariantList VersionBackend::versionInfoList() const
{
    QVariantList list;
    QVector<McVersion> versions = m_versionMgr->cachedVersions();
    for (const auto& v : versions) {
        QVariantMap m;
        m[QStringLiteral("id")] = v.id;
        m[QStringLiteral("type")] = v.type;
        list.append(m);
    }
    return list;
}

void VersionBackend::refreshVersionList()
{
    emit logMessage(QStringLiteral("正在获取版本列表..."));
    m_versionMgr->fetchVersions();
}

void VersionBackend::refreshInstalled()
{
    updateInstalledList();
    emit installedVersionsChanged();
}

// ============================================================
// Slots — install lifecycle
// ============================================================

void VersionBackend::installVersion(const QString& versionId, int sourceIndex)
{
    // ── Guard: prevent concurrent installs ──
    if (m_installing) {
        return;
    }

    // ── Resolve mirror source ──
    QVector<MirrorSource> mirrors = MirrorSource::allMirrors();
    MirrorSource mirror = (sourceIndex >= 0 && sourceIndex < mirrors.size())
                              ? mirrors[sourceIndex]
                              : mirrors[0]; // default: BMCLAPI

    // ── Find version metadata in cached list ──
    QVector<McVersion> versions = m_versionMgr->cachedVersions();
    McVersion targetVersion;
    bool found = false;
    for (const auto& v : versions) {
        if (v.id == versionId) {
            targetVersion = v;
            found = true;
            break;
        }
    }

    if (!found) {
        emit logMessage(
            QStringLiteral("未找到版本: %1").arg(versionId));
        return;
    }

    // ── Fetch detailed version JSON from version.url (async) ──
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(targetVersion.url));
    req.setTransferTimeout(15000);
    QNetworkReply* reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, nam, versionId, mirror]() {
                reply->deleteLater();
                nam->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    emit logMessage(
                        QStringLiteral("获取版本信息失败: %1")
                            .arg(reply->errorString()));
                    return;
                }

                QJsonParseError parseErr;
                QJsonDocument doc = QJsonDocument::fromJson(
                    reply->readAll(), &parseErr);

                if (parseErr.error != QJsonParseError::NoError ||
                    !doc.isObject()) {
                    emit logMessage(
                        QStringLiteral("版本信息格式错误: %1")
                            .arg(parseErr.errorString()));
                    return;
                }

                QJsonObject versionJson = doc.object();

                // ── Create & configure downloader ──
                m_downloader = new VersionDownloader(this);
                m_downloader->setMirror(mirror);
                m_downloader->setMinecraftDir(m_versionMgr->gameDir());
                m_downloader->setMaxWorkers(32);

                // ── Connect downloader signals ──
                connect(m_downloader,
                        &VersionDownloader::progressChanged, this,
                        &VersionBackend::onVersionDownloadProgress);

                connect(m_downloader,
                        &VersionDownloader::fileProgress, this,
                        [this](const QString& fileName,
                               qint64 /*received*/,
                               qint64 /*total*/) {
                            m_installFile = fileName;
                            emit installFileProgress(fileName);
                        });

                connect(m_downloader,
                        &VersionDownloader::verifyProgress, this,
                        [this](int checked, int total) {
                            setInstallPhase(
                                QStringLiteral("校验中..."));
                            m_installProgress = checked;
                            m_installTotal  = total;
                            emit installProgressChanged();
                        });

                connect(m_downloader,
                        &VersionDownloader::logMessage, this,
                        &VersionBackend::onVersionDownloadLog);

                connect(m_downloader,
                        &VersionDownloader::downloadFinished,
                        this,
                        &VersionBackend::onVersionDownloadFinished);

                // ── Start install ──
                setInstallPhase(QStringLiteral("准备中..."));
                m_installVersionId = versionId;
                setInstalling(true);
                emit logMessage(
                    QStringLiteral("开始安装 %1...").arg(versionId));

                m_downloader->downloadVersion(versionJson,
                                              versionId);
            });
}

void VersionBackend::cancelInstall()
{
    if (m_downloader) {
        m_downloader->cancel();
    }
    setInstalling(false);
    emit logMessage(QStringLiteral("安装已取消"));
}

void VersionBackend::pauseInstall()
{
    if (m_downloader) {
        m_downloader->pause();
    }
}

void VersionBackend::resumeInstall()
{
    if (m_downloader) {
        m_downloader->resume();
    }
}

// ============================================================
// Private slots — signal forwarding
// ============================================================

void VersionBackend::onVersionDownloadProgress(int cf, int tf,
                                               qint64 db, qint64 tb)
{
    m_installProgress    = cf;
    m_installTotal       = tf;
    m_installBytesDl     = db;
    m_installBytesTotal  = tb;

    // Phase detection: if file count > 0 we're downloading
    if (m_installPhase != QStringLiteral("校验中...")) {
        setInstallPhase(QStringLiteral("下载中..."));
    }

    emit installProgressChanged();
    emit installTotalChanged();
    emit installBytesProgress(db, tb);
}

void VersionBackend::onVersionDownloadLog(const QString& msg)
{
    emit logMessage(msg);
}

void VersionBackend::onVersionDownloadFinished(bool success,
                                               const QString& error)
{
    setInstalling(false);
    setInstallPhase(QStringLiteral("idle"));
    emit installFinished(success);

    if (success) {
        emit logMessage(
            QStringLiteral("✅ %1 安装完成")
                .arg(m_installVersionId));
        refreshInstalled();
    } else {
        QString errDetail = error.isEmpty()
                                ? QStringLiteral("未知错误")
                                : error;
        emit logMessage(
            QStringLiteral("❌ %1 安装失败: %2")
                .arg(m_installVersionId, errDetail));
    }
}

// ============================================================
// Private helpers
// ============================================================

void VersionBackend::updateInstalledList()
{
    m_installedIds.clear();

    const QString versionsDir =
        m_versionMgr->gameDir() + QStringLiteral("/versions");
    QDir dir(versionsDir);

    if (!dir.exists()) {
        return;
    }

    const QStringList subDirs =
        dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& subDir : subDirs) {
        // Check for {versionId}/{versionId}.jar
        const QString jarPath =
            versionsDir + QStringLiteral("/") + subDir +
            QStringLiteral("/") + subDir + QStringLiteral(".jar");

        if (QFileInfo::exists(jarPath)) {
            m_installedIds.append(subDir);
        }
    }
}

void VersionBackend::setInstalling(bool v)
{
    if (m_installing != v) {
        m_installing = v;
        emit installStateChanged();
    }
}

void VersionBackend::setInstallPhase(const QString& phase)
{
    if (m_installPhase != phase) {
        m_installPhase = phase;
        emit installPhaseChanged(phase);
    }
}

} // namespace ShadowLauncher
