// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "mod_manager.h"
#include "utils/logger.h"
#include "http_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QTimer>
#include <functional>
#include <memory>

namespace ShadowLauncher {

// ============================================================
// Modrinth API constants
// ============================================================

// Primary: MCIM mirror (faster for China mainland users)
//   API:  api.modrinth.com  → mod.mcimirror.top/modrinth
//   CDN:  cdn.modrinth.com  → mod.mcimirror.top
//   Note: mcim-files.pysio.online is DEAD, use mod.mcimirror.top for CDN
static const QString MODRINTH_API = QStringLiteral("https://mod.mcimirror.top/modrinth/v2");
static const QString MODRINTH_API_FALLBACK = QStringLiteral("https://api.modrinth.com/v2");

// MCIM CDN mirror (for file/image downloads)
static const QString MCIM_CDN = QStringLiteral("https://mod.mcimirror.top");

// ============================================================
// Constructor
// ============================================================

ModManager::ModManager(QObject* parent)
    : QObject(parent)
{
    // Connect versionsFetched signal for download flow interception
    connect(this, &ModManager::versionsFetched,
            this, &ModManager::onVersionsForDownload);
}

// ============================================================
// Busy state
// ============================================================

void ModManager::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool ModManager::isBusy() const
{
    return m_busy;
}

// ============================================================
// searchModrinth()
// ============================================================

void ModManager::searchModrinth(
    const QString& query,
    const QStringList& categories,
    const QStringList& gameVersions,
    const QStringList& loaders,
    int offset,
    int limit,
    const QString& sort)
{
    setBusy(true);
    emit logMessage(tr("正在搜索 Modrinth..."));

    QString url = buildSearchUrl(query, categories, gameVersions, loaders,
                                 offset, limit, sort);

    HttpClient::instance().get(url,
        [this](int status, const QByteArray& body) {
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = parseSearchResponse(body, totalHits);
                m_lastTotalHits = totalHits;
                emit searchCompleted(results, totalHits);
                emit logMessage(tr("搜索完成，共 %1 个结果").arg(totalHits));
            } else {
                emit searchFailed(tr("搜索失败: HTTP %1").arg(status));
            }
            setBusy(false);
        },
        [this](const QString& error) {
            emit searchFailed(tr("网络错误: %1").arg(error));
            setBusy(false);
        }
    );
}

void ModManager::searchModrinthProjects(
    const QString& query,
    const QStringList& facets,
    const QStringList& gameVersions,
    const QStringList& loaders,
    int offset,
    int limit,
    const QString& sort)
{
    setBusy(true);
    emit logMessage(tr("正在搜索 Modrinth (facets=%1)...").arg(facets.join(",")));

    QJsonArray facetArr;
    if (!facets.isEmpty()) {
        for (const QString& f : facets) {
            QJsonArray single;
            single.append(f);
            facetArr.append(single);
        }
    }
    if (!gameVersions.isEmpty()) {
        QJsonArray verFacet;
        for (const QString& v : gameVersions)
            verFacet.append(QStringLiteral("versions:") + v);
        facetArr.append(verFacet);
    }
    if (!loaders.isEmpty()) {
        QJsonArray ldFacet;
        for (const QString& l : loaders)
            ldFacet.append(QStringLiteral("categories:") + l);
        facetArr.append(ldFacet);
    }

    QUrl url(MODRINTH_API + "/search");
    QUrlQuery params;
    if (!query.isEmpty())
        params.addQueryItem(QStringLiteral("query"), query);
    params.addQueryItem(QStringLiteral("facets"),
                        QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    params.addQueryItem(QStringLiteral("index"), sort);
    url.setQuery(params);

    HttpClient::instance().get(url.toString(),
        [this](int status, const QByteArray& body) {
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = parseSearchResponse(body, totalHits);
                m_lastTotalHits = totalHits;
                emit searchCompleted(results, totalHits);
                emit logMessage(tr("搜索完成，共 %1 个结果").arg(totalHits));
            } else {
                emit searchFailed(tr("搜索失败: HTTP %1").arg(status));
            }
            setBusy(false);
        },
        [this](const QString& error) {
            emit searchFailed(tr("网络错误: %1").arg(error));
            setBusy(false);
        }
    );
}

// ============================================================
// getModVersions()
// ============================================================

void ModManager::getModVersions(
    const QString& slug,
    const QStringList& loaders,
    const QStringList& gameVersions)
{
    setBusy(true);
    emit logMessage(tr("正在获取 %1 的版本列表...").arg(slug));

    QUrl url(MODRINTH_API + "/project/" + slug + "/version");
    QUrlQuery params;

    if (!loaders.isEmpty()) {
        QJsonDocument doc(QJsonArray::fromStringList(loaders));
        params.addQueryItem(QStringLiteral("loaders"),
                            QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
    if (!gameVersions.isEmpty()) {
        QJsonDocument doc(QJsonArray::fromStringList(gameVersions));
        params.addQueryItem(QStringLiteral("game_versions"),
                            QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }

    url.setQuery(params);

    HttpClient::instance().get(url.toString(),
        [this, slug](int status, const QByteArray& body) {
            if (status == 200) {
                QJsonArray files = parseVersionsResponse(body);
                emit logMessage(tr("获取到 %1 的 %2 个文件版本")
                                    .arg(slug).arg(files.size()));
                emit versionsFetched(slug, files);
            } else {
                emit versionsFailed(slug,
                    tr("获取版本失败: HTTP %1").arg(status));
            }
            setBusy(false);
        },
        [this, slug](const QString& error) {
            emit versionsFailed(slug,
                tr("网络错误: %1").arg(error));
            setBusy(false);
        }
    );
}

// ============================================================
// downloadMod()
// ============================================================

void ModManager::downloadMod(
    const QString& slug,
    const QString& gameVersion,
    const QString& loader,
    const QString& minecraftDir,
    const QString& targetDir)
{
    m_minecraftDir = minecraftDir;
    m_pendingDownload = {slug, gameVersion, loader, targetDir, true};

    emit logMessage(tr("正在查找 %1 的 %2/%3 版本...")
                        .arg(slug, loader, gameVersion));
    emit modDownloadStarted(slug);

    // Fetch versions; onVersionsForDownload slot handles the download
    getModVersions(slug, {loader}, {gameVersion});
}

// ============================================================
// Private slot: handles version fetch for download flow
// ============================================================

void ModManager::onVersionsForDownload(const QString& slug, const QJsonArray& files)
{
    if (!m_pendingDownload.active || m_pendingDownload.slug != slug)
        return;

    m_pendingDownload.active = false;

    if (files.isEmpty()) {
        emit logMessage(
            tr("[失败] 未找到 %1 的 %2 %3 版本")
                .arg(slug, m_pendingDownload.loader, m_pendingDownload.gameVersion));
        emit downloadFinished(slug, false, QString());
        return;
    }

    // Pick primary file, fallback to first
    QJsonObject chosen;
    for (const QJsonValue& fv : files) {
        QJsonObject f = fv.toObject();
        if (f.value(QStringLiteral("isPrimary")).toBool()) {
            chosen = f;
            break;
        }
    }
    if (chosen.isEmpty()) {
        chosen = files.first().toObject();
    }

    QString filename  = chosen.value(QStringLiteral("filename")).toString();
    qint64  fileSize  = static_cast<qint64>(
                            chosen.value(QStringLiteral("size")).toDouble());
    QString dlUrl     = chosen.value(QStringLiteral("url")).toString();
    QString sha1      = chosen.value(QStringLiteral("sha1")).toString();

    emit logMessage(
        tr("找到 %1 个文件，选择: %2 (%3)")
            .arg(files.size())
            .arg(filename, formatFileSize(fileSize)));

    // Build destination path
    QString destDir = m_minecraftDir + QStringLiteral("/") + m_pendingDownload.targetDir;
    QDir().mkpath(destDir);
    QString destPath = destDir + QStringLiteral("/") + filename;

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(tr("开始下载: %1").arg(dlUrl));

    // Use HttpClient::download() for the actual transfer.
    // TODO: switch to ParallelDownloader with mirror support once implemented.
    HttpClient::instance().downloadWithFallback(
        dlUrl, destPath,
        [this, slug](qint64 received, qint64 total) {
            emit downloadProgress(slug, received, total);
        },
        [this, slug, destPath, filename](bool ok, const QString& error) {
            setBusy(false);
            if (ok) {
                emit logMessage(tr("[完成] %1 下载完成!").arg(filename));
            } else {
                emit logMessage(tr("[失败] %1 下载失败: %2").arg(filename, error));
            }
            emit downloadFinished(slug, ok, ok ? destPath : QString());
        }
    );
}

// ═══════════════════════════════════════════════════════════
// Mod file download — user-chosen save path
// ═══════════════════════════════════════════════════════════

int ModManager::downloadModFile(const QString& url, const QString& savePath,
                                 const QString& displayName, qint64 expectedSize,
                                 const QString& sha1, qint64 receivedOffset, int resumeId)
{
    int id = (resumeId >= 0) ? resumeId : m_nextModDownloadId++;
    ActiveModDownload dl;
    dl.id = id;
    dl.url = url;
    dl.savePath = savePath;
    dl.displayName = displayName;
    dl.expectedSize = expectedSize;
    dl.sha1 = sha1;
    m_activeModDownloads[id] = dl;

    emit logMessage(tr("[下载] 开始下载 Mod: %1 → %2").arg(displayName, savePath));
    if (resumeId < 0) {
        emit modFileDownloadStarted(id, QFileInfo(savePath).fileName(), expectedSize, displayName);
    }

    qint64 offset = receivedOffset;
    QNetworkReply* reply = HttpClient::instance().downloadWithReply(
        url, savePath,
        [this, id, offset](qint64 received, qint64 total) {
            auto it = m_activeModDownloads.find(id);
            if (it == m_activeModDownloads.end() || it->cancelled || it->finished || it->paused) return;
            it->received = received;
            emit modFileDownloadProgress(id, received, total);
        },
        [this, id, savePath, displayName, sha1](bool ok, const QString& error) {
            auto it = m_activeModDownloads.find(id);
            if (it == m_activeModDownloads.end()) return;
            if (it->cancelled) {
                QFile::remove(savePath);
                QFile::remove(savePath + ".tmp");
                m_activeModDownloads.erase(it);
                return;
            }
            if (it->paused) {
                it->reply = nullptr;
                return;
            }
            if (ok) {
                if (!sha1.isEmpty()) {
                    QFile f(savePath);
                    if (f.open(QIODevice::ReadOnly)) {
                        QCryptographicHash hash(QCryptographicHash::Sha1);
                        hash.addData(&f);
                        QString actual = hash.result().toHex().toLower();
                        f.close();
                        if (actual != sha1.toLower()) {
                            QFile::remove(savePath);
                            QString errMsg = tr("SHA1 校验失败\n期望: %1\n实际: %2\n文件已损坏，已被删除")
                                .arg(sha1, actual);
                            emit logMessage(QStringLiteral("[失败] %1 — %2").arg(displayName, errMsg.replace("\n", " ")));
                            it->failed = true;
                            it->errorDetail = errMsg;
                            emit modFileDownloadFailed(id, errMsg, displayName);
                            return;
                        }
                    }
                }
                it->finished = true;
                emit logMessage(tr("[完成] Mod 下载完成: %1").arg(displayName));
                emit modFileDownloadFinished(id, true, savePath, displayName);
            } else {
                QFile::remove(savePath);
                QFile::remove(savePath + ".tmp");
                QString errDetail = error.isEmpty()
                    ? tr("网络连接失败，请检查网络后重试")
                    : tr("下载错误: %1").arg(error);
                emit logMessage(tr("[失败] Mod 下载失败: %1 — %2").arg(displayName, errDetail));
                it->failed = true;
                it->errorDetail = errDetail;
                emit modFileDownloadFailed(id, errDetail, displayName);
            }
        },
        offset
    );

    if (reply) {
        auto it2 = m_activeModDownloads.find(id);
        if (it2 != m_activeModDownloads.end()) it2->reply = reply;
    }
    return id;
}

void ModManager::cancelModFileDownload(int downloadId)
{
    auto it = m_activeModDownloads.find(downloadId);
    if (it == m_activeModDownloads.end()) return;
    it->cancelled = true;
    if (it->reply) {
        HttpClient::instance().abortDownload(it->reply);
        it->reply = nullptr;
    }
    QFile::remove(it->savePath);
    QFile::remove(it->savePath + ".tmp");
    emit logMessage(tr("[取消] 已取消 Mod 下载: %1").arg(it->displayName));
}

void ModManager::pauseModFileDownload(int downloadId)
{
    auto it = m_activeModDownloads.find(downloadId);
    if (it == m_activeModDownloads.end() || it->finished || it->failed) return;
    it->paused = true;
    if (it->reply) {
        HttpClient::instance().abortDownload(it->reply);
        it->reply = nullptr;
    }
    emit logMessage(tr("[暂停] 已暂停 Mod 下载: %1 (%2/%3)")
        .arg(it->displayName).arg(it->received).arg(it->expectedSize));
    emit modFileDownloadProgress(downloadId, it->received, it->expectedSize);
}

void ModManager::resumeModFileDownload(int downloadId)
{
    auto it = m_activeModDownloads.find(downloadId);
    if (it == m_activeModDownloads.end() || !it->paused) return;
    emit logMessage(tr("▶ 继续 Mod 下载: %1").arg(it->displayName));
    QString url = it->url;
    QString savePath = it->savePath;
    QString displayName = it->displayName;
    qint64 expectedSize = it->expectedSize;
    QString sha1 = it->sha1;
    qint64 received = it->received;
    int oldId = it->id;
    m_activeModDownloads.erase(it);
    downloadModFile(url, savePath, displayName, expectedSize, sha1, received, oldId);
}

void ModManager::retryModFileDownload(int downloadId)
{
    auto it = m_activeModDownloads.find(downloadId);
    if (it == m_activeModDownloads.end()) return;
    QString url = it->url;
    QString savePath = it->savePath;
    QString displayName = it->displayName;
    qint64 expectedSize = it->expectedSize;
    QString sha1 = it->sha1;
    QFile::remove(savePath);
    m_activeModDownloads.erase(it);
    emit logMessage(tr("[重试] 重试下载 Mod: %1").arg(displayName));
    qint64 offset = it->received;
    downloadModFile(url, savePath, displayName, expectedSize, sha1, offset);
}

// ═══════════════════════════════════════════════════════════
// Resource packs — search + download
// ═══════════════════════════════════════════════════════════

void ModManager::searchResourcepacks(
    const QString& query,
    const QStringList& gameVersions,
    const QStringList& categories,
    int offset, int limit)
{
    setBusy(true);

    // Build facets: force project_type=resourcepack
    QJsonArray facets;
    QJsonArray typeFacet;
    typeFacet.append(QStringLiteral("project_type:resourcepack"));
    facets.append(typeFacet);

    if (!gameVersions.isEmpty()) {
        QJsonArray verFacet;
        for (const QString& v : gameVersions)
            verFacet.append(QStringLiteral("versions:") + v);
        facets.append(verFacet);
    }

    if (!categories.isEmpty()) {
        QJsonArray catFacet;
        for (const QString& cat : categories)
            catFacet.append(QStringLiteral("categories:") + cat);
        // Use OR facet (multiple values in one array = OR)
        facets.append(catFacet);
    }

    QUrl url(MODRINTH_API + "/search");
    QUrlQuery params;
    if (!query.isEmpty())
        params.addQueryItem(QStringLiteral("query"), query);
    params.addQueryItem(QStringLiteral("facets"),
                        QJsonDocument(facets).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
    url.setQuery(params);

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(tr("[MODRINTH] 搜索资源包: %1").arg(url.toString()));

    HttpClient::instance().get(url.toString(),
        [this](int /*status*/, const QByteArray& data) {
            setBusy(false);
            int total = 0;
            QJsonArray hits = parseSearchResponse(data, total);
            emit resourcepackSearchCompleted(hits, total);
            emit logMessage(tr("[MODRINTH] 资源包搜索结果: %1/%2").arg(hits.size()).arg(total));
        },
        [this](const QString& err) {
            setBusy(false);
            emit resourcepackSearchFailed(err);
            emit logMessage(tr("[MODRINTH] 资源包搜索失败: %1").arg(err));
        }
    );
}

void ModManager::downloadResourcepack(
    const QString& slug,
    const QString& gameVersion,
    const QString& minecraftDir)
{
    setBusy(true);
    m_minecraftDir = minecraftDir;

    emit logMessage(tr("[MODRINTH] 下载资源包: %1 MC%2").arg(slug, gameVersion));

    // Step 1: fetch versions filtered by game_version, pick matching primary file
    QUrl versionsUrl(MODRINTH_API + "/project/" + slug + "/version");
    QUrlQuery vp;
    if (!gameVersion.isEmpty())
        vp.addQueryItem(QStringLiteral("game_versions"),
                        QStringLiteral("[\"%1\"]").arg(gameVersion));
    versionsUrl.setQuery(vp);

    HttpClient::instance().get(versionsUrl.toString(),
        [this, slug, gameVersion, minecraftDir](int /*status*/, const QByteArray& data) {
            QJsonArray files = parseVersionsResponse(data);
            if (files.isEmpty()) {
                setBusy(false);
                emit logMessage(tr("[MODRINTH] 无文件匹配 MC%1").arg(gameVersion));
                emit resourcepackDownloadFinished(slug, false, {});
                return;
            }

            // Pick best file: match game version, prefer primary
            QJsonObject bestFile;
            for (const QJsonValue& fv : files) {
                QJsonObject f = fv.toObject();
                QJsonArray gvs = f[QStringLiteral("gameVersions")].toArray();
                bool match = false;
                for (const QJsonValue& gv : gvs) {
                    if (gv.toString() == gameVersion) { match = true; break; }
                }
                if (match) {
                    if (f.value(QStringLiteral("isPrimary")).toBool()) { bestFile = f; break; }
                    if (bestFile.isEmpty()) bestFile = f;
                }
            }
            // Fallback: use first file if no exact match
            if (bestFile.isEmpty()) bestFile = files.first().toObject();

            QString dlUrl = bestFile[QStringLiteral("url")].toString();
            QString filename = bestFile[QStringLiteral("filename")].toString();
            if (filename.isEmpty()) filename = slug + QStringLiteral(".zip");

            // Rewrite cdn.modrinth.com → MCIM CDN mirror
            dlUrl.replace(QStringLiteral("cdn.modrinth.com"), MCIM_CDN);
            dlUrl.replace(QStringLiteral("cdn-alt.modrinth.com"), MCIM_CDN);

            QString destDir = minecraftDir + QStringLiteral("/resourcepacks");
            QDir().mkpath(destDir);
            QString destPath = destDir + QStringLiteral("/") + filename;

            if (!ShadowLauncher::suppressUrlLog())
                emit logMessage(tr("[MODRINTH] 资源包文件: %1 → %2").arg(filename, dlUrl));

            HttpClient::instance().downloadWithFallback(
                dlUrl, destPath,
                [this, slug](qint64 received, qint64 total) {
                    emit downloadProgress(slug, received, total);
                },
                [this, slug, destPath, filename](bool ok, const QString& error) {
                    setBusy(false);
                    if (ok)
                        emit logMessage(tr("[MODRINTH] 资源包下载完成: %1").arg(filename));
                    else
                        emit logMessage(tr("[MODRINTH] 资源包下载失败: %1").arg(error));
                    emit resourcepackDownloadFinished(slug, ok, ok ? destPath : QString());
                }
            );
        },
        [this, slug](const QString& err) {
            setBusy(false);
            emit resourcepackDownloadFinished(slug, false, {});
            emit logMessage(tr("[MODRINTH] 获取版本列表失败: %1").arg(err));
        }
    );
}

// ═══════════════════════════════════════════════════════════
// Shader packs — download (same pattern as resource packs)
// ═══════════════════════════════════════════════════════════

void ModManager::downloadShader(
    const QString& slug,
    const QString& gameVersion,
    const QString& minecraftDir)
{
    setBusy(true);
    m_minecraftDir = minecraftDir;

    emit logMessage(tr("[MODRINTH] 下载光影: %1 MC%2").arg(slug, gameVersion));

    // Step 1: fetch versions filtered by game_version, pick matching primary file
    QUrl versionsUrl(MODRINTH_API + "/project/" + slug + "/version");
    QUrlQuery vp;
    if (!gameVersion.isEmpty())
        vp.addQueryItem(QStringLiteral("game_versions"),
                        QStringLiteral("[\"%1\"]").arg(gameVersion));
    versionsUrl.setQuery(vp);

    HttpClient::instance().get(versionsUrl.toString(),
        [this, slug, gameVersion, minecraftDir](int /*status*/, const QByteArray& data) {
            QJsonArray files = parseVersionsResponse(data);
            if (files.isEmpty()) {
                setBusy(false);
                emit logMessage(tr("[MODRINTH] 无光影文件匹配 MC%1").arg(gameVersion));
                emit shaderDownloadFinished(slug, false, {});
                return;
            }

            // Pick best file: match game version, prefer primary
            QJsonObject bestFile;
            for (const QJsonValue& fv : files) {
                QJsonObject f = fv.toObject();
                QJsonArray gvs = f[QStringLiteral("gameVersions")].toArray();
                bool match = false;
                for (const QJsonValue& gv : gvs) {
                    if (gv.toString() == gameVersion) { match = true; break; }
                }
                if (match) {
                    if (f.value(QStringLiteral("isPrimary")).toBool()) { bestFile = f; break; }
                    if (bestFile.isEmpty()) bestFile = f;
                }
            }
            // Fallback: use first file if no exact match
            if (bestFile.isEmpty()) bestFile = files.first().toObject();

            QString dlUrl = bestFile[QStringLiteral("url")].toString();
            QString filename = bestFile[QStringLiteral("filename")].toString();
            if (filename.isEmpty()) filename = slug + QStringLiteral(".zip");

            // Rewrite cdn.modrinth.com → MCIM CDN mirror
            dlUrl.replace(QStringLiteral("cdn.modrinth.com"), MCIM_CDN);
            dlUrl.replace(QStringLiteral("cdn-alt.modrinth.com"), MCIM_CDN);

            QString destDir = minecraftDir + QStringLiteral("/shaderpacks");
            QDir().mkpath(destDir);
            QString destPath = destDir + QStringLiteral("/") + filename;

            if (!ShadowLauncher::suppressUrlLog())
                emit logMessage(tr("[MODRINTH] 光影文件: %1 → %2").arg(filename, dlUrl));

            HttpClient::instance().downloadWithFallback(
                dlUrl, destPath,
                [this, slug](qint64 received, qint64 total) {
                    emit downloadProgress(slug, received, total);
                },
                [this, slug, destPath, filename](bool ok, const QString& error) {
                    setBusy(false);
                    if (ok)
                        emit logMessage(tr("[MODRINTH] 光影下载完成: %1").arg(filename));
                    else
                        emit logMessage(tr("[MODRINTH] 光影下载失败: %1").arg(error));
                    emit shaderDownloadFinished(slug, ok, ok ? destPath : QString());
                }
            );
        },
        [this, slug](const QString& err) {
            setBusy(false);
            emit shaderDownloadFinished(slug, false, {});
            emit logMessage(tr("[MODRINTH] 获取光影版本失败: %1").arg(err));
        }
    );
}

void ModManager::fetchResourcepackVersions(const QStringList& slugs)
{
    if (slugs.isEmpty()) return;

    // Shared state across all batches
    auto results = std::make_shared<QVariantMap>();
    auto total = std::make_shared<int>(slugs.size());
    auto pending = std::make_shared<int>(slugs.size());
    auto resolved = std::make_shared<int>(0);
    auto slugList = std::make_shared<QStringList>(slugs);
    auto index = std::make_shared<int>(0);

    static const int kBatchSize = 5;  // Max concurrent version fetches

    emit logMessage(tr("[MODRINTH] 获取 %1 个资源包版本 (批量 %2/次)")
                    .arg(slugs.size()).arg(kBatchSize));

    // Emit progress signal so QML can show loading
    emit resourcepackVersionsProgress(0, slugs.size());

    // Shared version parser (avoids code duplication)
    auto processOne = [this, results, pending, resolved, total, slugList]() {
        int done = ++(*resolved);
        emit logMessage(tr("[MODRINTH-BATCH] 完成 %1/%2").arg(done).arg(*total));
        emit resourcepackVersionsProgress(done, *total);
        if (done >= *total) {
            emit resourcepackVersionsLoaded(*results);
            emit logMessage(tr("[MODRINTH-BATCH] 全部完成 (%1 个)").arg(*total));
        }
    };

    // Recursive batch launcher (uses shared_ptr to avoid self-capture UB)
    auto launchBatchPtr = std::make_shared<std::function<void()>>();
    auto indexPtr = index;
    *launchBatchPtr = [=]() {
        int start = *indexPtr;
        int end = qMin(start + kBatchSize, slugList->size());
        if (start >= end) return;

        *indexPtr = end;

        emit logMessage(tr("[MODRINTH-BATCH] 启动批次 [%1..%2] (共%3个)")
                        .arg(start).arg(end - 1).arg(end - start));

        for (int i = start; i < end; ++i) {
            const QString& slug = (*slugList)[i];
            QUrl url(MODRINTH_API + "/project/" + slug + "/version");

            emit logMessage(QStringLiteral("[MODRINTH-BATCH] HTTP GET #%1 slug=%2").arg(i).arg(slug));

            HttpClient::instance().get(url.toString(),
                [this, slug, i, results, processOne](int status, const QByteArray& data) {
                    emit logMessage(QStringLiteral("[MODRINTH-BATCH] #%1 slug=%2 HTTP=%3 size=%4")
                                    .arg(i).arg(slug).arg(status).arg(data.size()));
                    // Collect all unique game_versions (MC versions this pack supports)
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    QJsonArray verObjs = doc.array();
                    QSet<QString> seen;
                    QStringList allGameVersions;

                    for (const QJsonValue& vv : verObjs) {
                        QJsonObject ver = vv.toObject();
                        QJsonArray gvs = ver.value(QStringLiteral("game_versions")).toArray();
                        for (const QJsonValue& gv : gvs) {
                            QString gvStr = gv.toString();
                            if (!seen.contains(gvStr)) {
                                seen.insert(gvStr);
                                allGameVersions.append(gvStr);
                            }
                        }
                    }

                    // Sort by MC version (newest first)
                    std::sort(allGameVersions.begin(), allGameVersions.end(),
                        [](const QString& a, const QString& b) {
                            auto parse = [](const QString& v) -> std::tuple<int,int,int> {
                                auto pts = v.split('.');
                                return {
                                    pts.size() > 0 ? pts[0].toInt() : 0,
                                    pts.size() > 1 ? pts[1].toInt() : 0,
                                    pts.size() > 2 ? pts[2].toInt() : 0
                                };
                            };
                            return parse(a) > parse(b);
                        });

                    // Build detailMap: for each game_version, find the newest pack version that supports it
                    QVariantMap detailMap;
                    for (const QString& gv : allGameVersions) {
                        for (const QJsonValue& vv : verObjs) {
                            QJsonObject ver = vv.toObject();
                            QJsonArray gvs = ver["game_versions"].toArray();
                            bool found = false;
                            for (const QJsonValue& g : gvs) {
                                if (g.toString() == gv) { found = true; break; }
                            }
                            if (found) {
                                QVariantMap detail;
                                detail["version_number"] = ver["version_number"].toString();
                                detail["name"] = ver["name"].toString();
                                detail["downloads"] = ver["downloads"].toInt();
                                detail["date_published"] = ver["date_published"].toString();
                                detail["version_type"] = ver["version_type"].toString();
                                // Download file info (first primary file)
                                QJsonArray files = ver["files"].toArray();
                                for (const QJsonValue& fv : files) {
                                    QJsonObject f = fv.toObject();
                                    if (f["primary"].toBool()) {
                                        detail["url"] = f["url"].toString();
                                        detail["filename"] = f["filename"].toString();
                                        detail["size"] = qint64(f["size"].toDouble());
                                        detail["sha1"] = f["hashes"].toObject()["sha1"].toString();
                                        break;
                                    }
                                }
                                if (!detail.contains("url") && !files.isEmpty()) {
                                    QJsonObject f = files[0].toObject();
                                    detail["url"] = f["url"].toString();
                                    detail["filename"] = f["filename"].toString();
                                    detail["size"] = qint64(f["size"].toDouble());
                                    detail["sha1"] = f["hashes"].toObject()["sha1"].toString();
                                }
                                detailMap[gv] = detail;
                                break;
                            }
                        }
                    }

                    (*results)[slug] = allGameVersions;
                    emit resourcepackVersionsPartial(slug, allGameVersions, detailMap);
                    processOne();
                },
                [this, slug, processOne](const QString& err) {
                    Q_UNUSED(err); Q_UNUSED(slug);
                    processOne();  // Count failures too
                }
            );
        }

        // Schedule next batch after a short delay
        if (*indexPtr < slugList->size()) {
            QTimer::singleShot(100, [launchBatchPtr]() { (*launchBatchPtr)(); });
        }
    };

    // Start first batch
    (*launchBatchPtr)();
}

// ============================================================
// fetchModVersions() — clone of fetchResourcepackVersions
// for Mod tab detail page
// ============================================================

void ModManager::fetchModVersions(const QStringList& slugs)
{
    if (slugs.isEmpty()) return;

    auto results = std::make_shared<QVariantMap>();
    auto total = std::make_shared<int>(slugs.size());
    auto pending = std::make_shared<int>(slugs.size());
    auto resolved = std::make_shared<int>(0);
    auto slugList = std::make_shared<QStringList>(slugs);
    auto index = std::make_shared<int>(0);

    static const int kBatchSize = 5;

    emit logMessage(tr("[MODRINTH-MOD] 获取 %1 个Mod版本 (批量 %2/次)")
                    .arg(slugs.size()).arg(kBatchSize));
    emit modVersionsProgress(0, slugs.size());

    auto processOne = [this, results, pending, resolved, total, slugList]() {
        int done = ++(*resolved);
        emit logMessage(tr("[MODRINTH-MOD-BATCH] 完成 %1/%2").arg(done).arg(*total));
        emit modVersionsProgress(done, *total);
        if (done >= *total) {
            emit modVersionsLoaded(*results);
            emit logMessage(tr("[MODRINTH-MOD-BATCH] 全部完成 (%1 个)").arg(*total));
        }
    };

    auto launchBatchPtr = std::make_shared<std::function<void()>>();
    auto indexPtr = index;
    *launchBatchPtr = [=]() {
        int start = *indexPtr;
        int end = qMin(start + kBatchSize, slugList->size());
        if (start >= end) return;
        *indexPtr = end;

        for (int i = start; i < end; ++i) {
            const QString& slug = (*slugList)[i];
            QUrl url(MODRINTH_API + "/project/" + slug + "/version");

            emit logMessage(QStringLiteral("[MODRINTH-MOD-BATCH] HTTP GET #%1 slug=%2").arg(i).arg(slug));

            HttpClient::instance().get(url.toString(),
                [this, slug, i, results, processOne](int status, const QByteArray& data) {
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    QJsonArray verObjs = doc.array();
                    QSet<QString> seen;
                    QStringList allGameVersions;

                    for (const QJsonValue& vv : verObjs) {
                        QJsonObject ver = vv.toObject();
                        QJsonArray gvs = ver.value(QStringLiteral("game_versions")).toArray();
                        for (const QJsonValue& gv : gvs) {
                            QString gvStr = gv.toString();
                            if (!seen.contains(gvStr)) {
                                seen.insert(gvStr);
                                allGameVersions.append(gvStr);
                            }
                        }
                    }

                    std::sort(allGameVersions.begin(), allGameVersions.end(),
                        [](const QString& a, const QString& b) {
                            auto parse = [](const QString& v) -> std::tuple<int,int,int> {
                                auto pts = v.split('.');
                                return {
                                    pts.size() > 0 ? pts[0].toInt() : 0,
                                    pts.size() > 1 ? pts[1].toInt() : 0,
                                    pts.size() > 2 ? pts[2].toInt() : 0
                                };
                            };
                            return parse(a) > parse(b);
                        });

                    // Build detailMap keyed by "gameVersion|loader" so that
                    // multi-loader mods (Iris: Fabric + NeoForge, etc.) show
                    // all loader variants per game version.
                    QVariantMap detailMap;
                    QStringList compositeVersions;
                    QSet<QString> dedupComposite;

                    for (const QString& gv : allGameVersions) {
                        for (const QJsonValue& vv : verObjs) {
                            QJsonObject ver = vv.toObject();
                            QJsonArray gvs = ver["game_versions"].toArray();
                            bool found = false;
                            for (const QJsonValue& g : gvs) {
                                if (g.toString() == gv) { found = true; break; }
                            }
                            if (found) {
                                QJsonArray loaders = ver["loaders"].toArray();
                                QStringList loaderList;
                                for (const QJsonValue& l : loaders) loaderList << l.toString();
                                QString primaryLoader = loaderList.isEmpty() ? QString() : loaderList.first();
                                QString compositeKey = gv + QStringLiteral("|") + primaryLoader;

                                // Skip if this (gameVersion, loader) combo already seen
                                if (dedupComposite.contains(compositeKey))
                                    continue;
                                dedupComposite.insert(compositeKey);

                                QVariantMap detail;
                                detail["version_number"] = ver["version_number"].toString();
                                detail["name"] = ver["name"].toString();
                                detail["downloads"] = ver["downloads"].toInt();
                                detail["date_published"] = ver["date_published"].toString();
                                detail["version_type"] = ver["version_type"].toString();
                                detail["id"] = ver["id"].toString();
                                detail["game_version"] = gv;  // plain MC version for QML display
                                // Primary file
                                QJsonArray files = ver["files"].toArray();
                                for (const QJsonValue& fv : files) {
                                    QJsonObject f = fv.toObject();
                                    if (f["primary"].toBool()) {
                                        detail["url"] = f["url"].toString();
                                        detail["filename"] = f["filename"].toString();
                                        detail["size"] = static_cast<qint64>(f["size"].toDouble());
                                        QJsonObject hashes = f["hashes"].toObject();
                                        detail["sha1"] = hashes["sha1"].toString();
                                        break;
                                    }
                                }
                                if (!detail.contains("url") && !files.isEmpty()) {
                                    QJsonObject f = files.first().toObject();
                                    detail["url"] = f["url"].toString();
                                    detail["filename"] = f["filename"].toString();
                                    detail["size"] = static_cast<qint64>(f["size"].toDouble());
                                    QJsonObject hashes = f["hashes"].toObject();
                                    detail["sha1"] = hashes["sha1"].toString();
                                }
                                detail["loaders"] = loaderList;
                                detailMap[compositeKey] = detail;
                                compositeVersions.append(compositeKey);
                                // Continue iterating — don't break, collect all loader
                                // variants for this game version
                            }
                        }
                    }

                    (*results)[slug] = compositeVersions;
                    emit modVersionsPartial(slug, compositeVersions, detailMap);
                    processOne();
                },
                [this, slug, processOne](const QString& err) {
                    Q_UNUSED(err); Q_UNUSED(slug);
                    processOne();
                }
            );
        }

        if (*indexPtr < slugList->size()) {
            QTimer::singleShot(100, [launchBatchPtr]() { (*launchBatchPtr)(); });
        }
    };

    (*launchBatchPtr)();
}

// ============================================================
// fetchShaderVersions() — clone of fetchResourcepackVersions
// for Shader tab detail page
// ============================================================

void ModManager::fetchShaderVersions(const QStringList& slugs)
{
    if (slugs.isEmpty()) return;

    auto results = std::make_shared<QVariantMap>();
    auto total = std::make_shared<int>(slugs.size());
    auto pending = std::make_shared<int>(slugs.size());
    auto resolved = std::make_shared<int>(0);
    auto slugList = std::make_shared<QStringList>(slugs);
    auto index = std::make_shared<int>(0);

    static const int kBatchSize = 5;

    emit logMessage(tr("[MODRINTH-SHADER] 获取 %1 个光影版本 (批量 %2/次)")
                    .arg(slugs.size()).arg(kBatchSize));
    emit shaderVersionsProgress(0, slugs.size());

    auto processOne = [this, results, pending, resolved, total, slugList]() {
        int done = ++(*resolved);
        emit logMessage(tr("[MODRINTH-SHADER-BATCH] 完成 %1/%2").arg(done).arg(*total));
        emit shaderVersionsProgress(done, *total);
        if (done >= *total) {
            emit shaderVersionsLoaded(*results);
            emit logMessage(tr("[MODRINTH-SHADER-BATCH] 全部完成 (%1 个)").arg(*total));
        }
    };

    auto launchBatchPtr = std::make_shared<std::function<void()>>();
    auto indexPtr = index;
    *launchBatchPtr = [=]() {
        int start = *indexPtr;
        int end = qMin(start + kBatchSize, slugList->size());
        if (start >= end) return;
        *indexPtr = end;

        for (int i = start; i < end; ++i) {
            const QString& slug = (*slugList)[i];
            QUrl url(MODRINTH_API + "/project/" + slug + "/version");

            emit logMessage(QStringLiteral("[MODRINTH-SHADER-BATCH] HTTP GET #%1 slug=%2").arg(i).arg(slug));

            HttpClient::instance().get(url.toString(),
                [this, slug, i, results, processOne](int status, const QByteArray& data) {
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    QJsonArray verObjs = doc.array();
                    QSet<QString> seen;
                    QStringList allGameVersions;

                    for (const QJsonValue& vv : verObjs) {
                        QJsonObject ver = vv.toObject();
                        QJsonArray gvs = ver.value(QStringLiteral("game_versions")).toArray();
                        for (const QJsonValue& gv : gvs) {
                            QString gvStr = gv.toString();
                            if (!seen.contains(gvStr)) {
                                seen.insert(gvStr);
                                allGameVersions.append(gvStr);
                            }
                        }
                    }

                    std::sort(allGameVersions.begin(), allGameVersions.end(),
                        [](const QString& a, const QString& b) {
                            auto parse = [](const QString& v) -> std::tuple<int,int,int> {
                                auto pts = v.split('.');
                                return {
                                    pts.size() > 0 ? pts[0].toInt() : 0,
                                    pts.size() > 1 ? pts[1].toInt() : 0,
                                    pts.size() > 2 ? pts[2].toInt() : 0
                                };
                            };
                            return parse(a) > parse(b);
                        });

                    QVariantMap detailMap;
                    for (const QString& gv : allGameVersions) {
                        for (const QJsonValue& vv : verObjs) {
                            QJsonObject ver = vv.toObject();
                            QJsonArray gvs = ver["game_versions"].toArray();
                            bool found = false;
                            for (const QJsonValue& g : gvs) {
                                if (g.toString() == gv) { found = true; break; }
                            }
                            if (found) {
                                QVariantMap detail;
                                detail["version_number"] = ver["version_number"].toString();
                                detail["name"] = ver["name"].toString();
                                detail["downloads"] = ver["downloads"].toInt();
                                detail["date_published"] = ver["date_published"].toString();
                                detail["version_type"] = ver["version_type"].toString();
                                // Loaders
                                QJsonArray loadersArr = ver["loaders"].toArray();
                                QStringList loadersList;
                                for (const QJsonValue& l : loadersArr)
                                    loadersList.append(l.toString());
                                detail["loaders"] = loadersList;
                                // Primary file (download URL)
                                QJsonArray filesArr = ver["files"].toArray();
                                for (const QJsonValue& fv : filesArr) {
                                    QJsonObject fobj = fv.toObject();
                                    if (fobj.value("primary").toBool(false)) {
                                        detail["url"] = fobj["url"].toString();
                                        detail["filename"] = fobj["filename"].toString();
                                        detail["size"] = fobj["size"].toInt();
                                        detail["sha1"] = fobj["hashes"].toObject()["sha1"].toString();
                                        break;
                                    }
                                }
                                detailMap[gv] = detail;
                                break;
                            }
                        }
                    }

                    (*results)[slug] = allGameVersions;
                    emit shaderVersionsPartial(slug, allGameVersions, detailMap);
                    processOne();
                },
                [this, slug, processOne](const QString& err) {
                    Q_UNUSED(err); Q_UNUSED(slug);
                    processOne();
                }
            );
        }

        if (*indexPtr < slugList->size()) {
            QTimer::singleShot(100, [launchBatchPtr]() { (*launchBatchPtr)(); });
        }
    };

    (*launchBatchPtr)();
}

// ============================================================
// cancel()
// ============================================================

void ModManager::cancel()
{
    m_pendingDownload.active = false;
    setBusy(false);
}

// ============================================================
// URL builder
// ============================================================

QString ModManager::buildSearchUrl(
    const QString& query, const QStringList& categories,
    const QStringList& gameVersions, const QStringList& loaders,
    int offset, int limit, const QString& sort) const
{
    QUrl url(MODRINTH_API + "/search");
    QUrlQuery params;

    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"),  QString::number(limit));
    params.addQueryItem(QStringLiteral("index"),  sort);

    if (!query.isEmpty()) {
        params.addQueryItem(QStringLiteral("query"), query);
    }

    // Build facets JSON as array of string arrays (Modrinth v2 API format)
    QJsonArray facets;

    if (!categories.isEmpty()) {
        QJsonArray catFacet;
        for (const QString& cat : categories) {
            catFacet.append(QStringLiteral("categories:") + cat);
        }
        facets.append(catFacet);
    }

    if (!gameVersions.isEmpty()) {
        QJsonArray verFacet;
        for (const QString& ver : gameVersions) {
            verFacet.append(QStringLiteral("versions:") + ver);
        }
        facets.append(verFacet);
    }

    if (!loaders.isEmpty()) {
        QJsonArray loaderFacet;
        for (const QString& l : loaders) {
            // Modrinth encodes loaders as categories in facets
            loaderFacet.append(QStringLiteral("categories:") + l);
        }
        facets.append(loaderFacet);
    }

    if (!facets.isEmpty()) {
        QJsonDocument facetDoc(facets);
        params.addQueryItem(QStringLiteral("facets"),
            QString::fromUtf8(facetDoc.toJson(QJsonDocument::Compact)));
    }

    url.setQuery(params);
    return url.toString();
}

// ============================================================
// Response parsers
// ============================================================

QJsonArray ModManager::parseSearchResponse(const QByteArray& data, int& outTotalHits)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    outTotalHits = root.value(QStringLiteral("total_hits")).toInt();

    const QJsonArray hits = root.value(QStringLiteral("hits")).toArray();
    QJsonArray results;

    for (const QJsonValue& hitVal : hits) {
        QJsonObject hit = hitVal.toObject();
        QJsonObject mod;

        mod[QStringLiteral("slug")]          = hit.value(QStringLiteral("slug"));
        mod[QStringLiteral("title")]         = hit.value(QStringLiteral("title"));
        mod[QStringLiteral("description")]   = hit.value(QStringLiteral("description"));
        mod[QStringLiteral("iconUrl")]       = hit.value(QStringLiteral("icon_url"));
        mod[QStringLiteral("author")]        = hit.value(QStringLiteral("author"));
        mod[QStringLiteral("downloads")]     = hit.value(QStringLiteral("downloads"));
        mod[QStringLiteral("updated")]       = hit.value(QStringLiteral("date_modified"));
        mod[QStringLiteral("gameVersions")]  = hit.value(QStringLiteral("versions"));
        // Modrinth uses "categories" for loaders in search results
        mod[QStringLiteral("loaders")]       = hit.value(QStringLiteral("categories"));
        mod[QStringLiteral("latestVersion")] = hit.value(QStringLiteral("latest_version"));

        // Split categories into 3 dimensions: resolution, feature, category
        const QJsonArray categoriesArray = hit.value(QStringLiteral("categories")).toArray();
        QStringList resList, featList, catList;
        for (const QJsonValue& cv : categoriesArray) {
            QString c = cv.toString().toLower();
            // Resolution: "16x", "32x", "8x-", "512x+", etc.
            // Modrinth uses "8x-" (≤8x) and "512x+" (≥512x) — strip suffix for classification
            QString cStripped = c;
            if (cStripped.endsWith(QLatin1String("-")) || cStripped.endsWith(QLatin1String("+")))
                cStripped.chop(1);
            if (cStripped.endsWith(QLatin1String("x")) && cStripped.length() <= 5) {
                bool ok = false;
                cStripped.chopped(1).toInt(&ok);
                if (ok) {
                    resList.append(cStripped);  // store "8x" not "8x-"
                    continue;
                }
            }
            // Feature: known Modrinth feature tags
            if (c == QLatin1String("audio") || c == QLatin1String("blocks") ||
                c == QLatin1String("core-shaders") || c == QLatin1String("core_shaders") ||
                c == QLatin1String("entities") || c == QLatin1String("environment") ||
                c == QLatin1String("equipment") || c == QLatin1String("fonts") ||
                c == QLatin1String("gui") || c == QLatin1String("items") ||
                c == QLatin1String("locale") || c == QLatin1String("models") ||
                c == QLatin1String("minecraft")) {
                featList.append(c);
                continue;
            }
            // Everything else → category
            catList.append(c);
        }
        mod[QStringLiteral("resolutions")] = QJsonArray::fromStringList(resList);
        mod[QStringLiteral("features")]    = QJsonArray::fromStringList(featList);
        mod[QStringLiteral("categories")]  = QJsonArray::fromStringList(catList);

        results.append(mod);
    }

    return results;
}

QJsonArray ModManager::parseVersionsResponse(const QByteArray& data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray versions = doc.array();
    QJsonArray files;

    for (const QJsonValue& verVal : versions) {
        QJsonObject ver = verVal.toObject();
        QString verName   = ver.value(QStringLiteral("name")).toString();
        QString verNumber = ver.value(QStringLiteral("version_number")).toString();

        const QJsonArray verFiles = ver.value(QStringLiteral("files")).toArray();
        QJsonArray gameVersions   = ver.value(QStringLiteral("game_versions")).toArray();
        QJsonArray loaders        = ver.value(QStringLiteral("loaders")).toArray();

        for (const QJsonValue& fv : verFiles) {
            QJsonObject f = fv.toObject();
            QJsonObject hashes = f.value(QStringLiteral("hashes")).toObject();

            QJsonObject file;
            file[QStringLiteral("name")]  = verName + QStringLiteral(" (") + verNumber + QStringLiteral(")");
            file[QStringLiteral("url")]   = f.value(QStringLiteral("url"));
            file[QStringLiteral("filename")] = f.value(QStringLiteral("filename"));
            file[QStringLiteral("size")]  = f.value(QStringLiteral("size"));
            file[QStringLiteral("sha1")]  = hashes.value(QStringLiteral("sha1"));
            file[QStringLiteral("gameVersions")] = gameVersions;
            file[QStringLiteral("loaders")]      = loaders;
            file[QStringLiteral("isPrimary")]    = f.value(QStringLiteral("primary"));
            file[QStringLiteral("versionNumber")] = verNumber;  // "1.21.11"

            files.append(file);
        }
    }

    return files;
}

// ============================================================
// getModDependencies() — fetch required + optional deps
// ============================================================

void ModManager::getModDependencies(const QString& slug, const QString& gameVersion)
{
    if (slug.isEmpty()) return;

    emit logMessage(tr("[MODRINTH] 正在查询 %1 的前置模组...").arg(slug));

    QUrl url(MODRINTH_API + "/project/" + slug + "/version");
    if (!gameVersion.isEmpty()) {
        QUrlQuery params;
        QJsonArray gvFilter;
        gvFilter.append(gameVersion);
        params.addQueryItem(QStringLiteral("game_versions"),
            QString::fromUtf8(QJsonDocument(gvFilter).toJson(QJsonDocument::Compact)));
        url.setQuery(params);
    }

    HttpClient::instance().get(url.toString(),
        [this, slug](int status, const QByteArray& body) {
            if (status != 200) {
                emit logMessage(tr("[MODRINTH] 获取 %1 版本失败: HTTP %2").arg(slug).arg(status));
                emit dependenciesResolved(slug, QJsonArray());
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(body);
            QJsonArray versions = doc.array();

            // Collect all required/optional dependencies (deduplicated by project_id)
            QMap<QString, QPair<QString, QString>> depMap;  // projectId → (type, versionId)
            for (const QJsonValue& vv : versions) {
                QJsonObject ver = vv.toObject();
                QJsonArray deps = ver.value(QStringLiteral("dependencies")).toArray();
                for (const QJsonValue& dv : deps) {
                    QJsonObject dep = dv.toObject();
                    QString depType = dep.value(QStringLiteral("dependency_type")).toString();
                    if (depType != QStringLiteral("required") && depType != QStringLiteral("optional"))
                        continue;
                    QString pid = dep.value(QStringLiteral("project_id")).toString();
                    if (pid.isEmpty()) continue;
                    if (!depMap.contains(pid)) {
                        QString vid = dep.value(QStringLiteral("version_id")).toString();
                        depMap[pid] = {depType, vid};
                    }
                }
            }

            if (depMap.isEmpty()) {
                emit logMessage(tr("[MODRINTH] %1 无前置/可选依赖").arg(slug));
                emit dependenciesResolved(slug, QJsonArray());
                return;
            }

            // Batch resolve project IDs to slugs + titles
            QUrl projUrl(MODRINTH_API + "/projects");
            QUrlQuery pq;
            QStringList ids = depMap.keys();
            pq.addQueryItem(QStringLiteral("ids"),
                QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(ids)).toJson(QJsonDocument::Compact)));
            projUrl.setQuery(pq);

            HttpClient::instance().get(projUrl.toString(),
                [this, slug, depMap](int pStatus, const QByteArray& pBody) {
                    QJsonArray result;

                    if (pStatus == 200) {
                        QJsonDocument pDoc = QJsonDocument::fromJson(pBody);
                        QJsonArray projects = pDoc.array();

                        // Build id→project info map
                        QMap<QString, QJsonObject> projMap;
                        for (const QJsonValue& pv : projects) {
                            QJsonObject proj = pv.toObject();
                            QString pid = proj.value(QStringLiteral("id")).toString();
                            QJsonObject info;
                            info[QStringLiteral("slug")] = proj.value(QStringLiteral("slug"));
                            info[QStringLiteral("title")] = proj.value(QStringLiteral("title"));
                            info[QStringLiteral("icon_url")] = proj.value(QStringLiteral("icon_url"));
                            info[QStringLiteral("description")] = proj.value(QStringLiteral("description"));
                            projMap[pid] = info;
                        }

                        for (auto it = depMap.begin(); it != depMap.end(); ++it) {
                            QJsonObject depObj;
                            depObj[QStringLiteral("project_id")] = it.key();
                            depObj[QStringLiteral("dependency_type")] = it.value().first;

                            auto resolved = projMap.value(it.key());
                            depObj[QStringLiteral("slug")] = resolved.value(QStringLiteral("slug"));
                            depObj[QStringLiteral("title")] = resolved.value(QStringLiteral("title")).toString().isEmpty() ? it.key() : resolved.value(QStringLiteral("title")).toString();
                            depObj[QStringLiteral("icon_url")] = resolved.value(QStringLiteral("icon_url"));
                            depObj[QStringLiteral("description")] = resolved.value(QStringLiteral("description"));

                            result.append(depObj);
                        }
                    } else {
                        // Fallback: emit with just project IDs
                        for (auto it = depMap.begin(); it != depMap.end(); ++it) {
                            QJsonObject depObj;
                            depObj[QStringLiteral("project_id")] = it.key();
                            depObj[QStringLiteral("dependency_type")] = it.value().first;
                            depObj[QStringLiteral("title")] = it.key();  // fallback to ID
                            result.append(depObj);
                        }
                    }

                    emit logMessage(tr("[MODRINTH] %1 的前置模组: %2 个").arg(slug).arg(result.size()));
                    emit dependenciesResolved(slug, result);
                },
                [this, slug](const QString& err) {
                    emit logMessage(tr("[MODRINTH] 批量解析项目失败: %1").arg(err));
                    emit dependenciesResolved(slug, QJsonArray());
                }
            );
        },
        [this, slug](const QString& err) {
            emit logMessage(tr("[MODRINTH] 获取 %1 版本列表失败: %2").arg(slug).arg(err));
            emit dependenciesResolved(slug, QJsonArray());
        }
    );
}

// ============================================================
// Utility
// ============================================================

QString ModManager::formatFileSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
}

// ============================================================
// Static data — Popular Mods
// ============================================================

QMap<QString, QJsonObject> ModManager::getPopularMods(const QString& loader)
{
    static bool init = false;
    static QMap<QString, QMap<QString, QJsonObject>> allMods;

    if (!init) {
        init = true;

        // Fabric
        QMap<QString, QJsonObject> fabric;
        auto addFabric = [&](const QString& slug, const QString& title,
                             const QString& desc, const QString& icon,
                             const QString& category) {
            QJsonObject o;
            o[QStringLiteral("title")]    = title;
            o[QStringLiteral("desc")]     = desc;
            o[QStringLiteral("icon")]     = icon;
            o[QStringLiteral("category")] = category;
            fabric[slug] = o;
        };
        addFabric(QStringLiteral("sodium"),              QStringLiteral("Sodium"),              tr("极致性能优化，FPS提升巨大"),           QStringLiteral("[优化]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("iris"),                 QStringLiteral("Iris Shaders"),        tr("光影加载器，Fabric首选"),              QStringLiteral("[光影]"), QStringLiteral("shader"));
        addFabric(QStringLiteral("lithium"),              QStringLiteral("Lithium"),             tr("服务端性能优化，无兼容性问题"),         QStringLiteral("[加速]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("phosphor"),             QStringLiteral("Phosphor"),            tr("光照引擎优化（已替代为Starlight）"),    QStringLiteral("[光照]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("starlight"),            QStringLiteral("Starlight"),           tr("新一代光照引擎，性能飞升"),            QStringLiteral("[优质]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("ferritecore"),          QStringLiteral("FerriteCore"),         tr("大幅降低内存占用"),                     QStringLiteral("[内存]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("modmenu"),              QStringLiteral("Mod Menu"),            tr("Fabric必备，Mod列表查看"),             QStringLiteral("[列表]"), QStringLiteral("utility"));
        addFabric(QStringLiteral("cloth-config"),         QStringLiteral("Cloth Config"),        tr("配置API，很多Mod依赖它"),              QStringLiteral("[配置]"), QStringLiteral("library"));
        addFabric(QStringLiteral("sodium-extra"),         QStringLiteral("Sodium Extra"),        tr("Sodium功能扩展"),                      QStringLiteral("[工具]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("reeses-sodium-options"),QStringLiteral("Reese's Sodium Options"), tr("Sodium选项增强"),                QStringLiteral("[调节]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("continuity"),           QStringLiteral("Continuity"),          tr("连接纹理（类似OptiFine的玻璃效果）"),  QStringLiteral("[纹理]"), QStringLiteral("decoration"));
        addFabric(QStringLiteral("lambda"),               QStringLiteral("LambdaBetterGrass"),   tr("更好的草地渲染"),                      QStringLiteral("[自然]"), QStringLiteral("decoration"));
        addFabric(QStringLiteral("entityculling"),        QStringLiteral("Entity Culling"),      tr("实体渲染优化"),                        QStringLiteral("[视觉]"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("indium"),               QStringLiteral("Indium"),              tr("Sodium + Fabric渲染API兼容"),          QStringLiteral("[兼容]"), QStringLiteral("library"));
        addFabric(QStringLiteral("fabric-api"),           QStringLiteral("Fabric API"),          tr("Fabric核心API，必装"),                 QStringLiteral("[核心]"), QStringLiteral("library"));
        addFabric(QStringLiteral("lambdynamiclights"),    QStringLiteral("LambDynamicLights"),   tr("动态光源（手持火把发光）"),           QStringLiteral("[热能]"), QStringLiteral("decoration"));
        addFabric(QStringLiteral("zoomify"),              QStringLiteral("Zoomify"),             tr("缩放Mod（类似OptiFine的C键）"),       QStringLiteral("[缩放]"), QStringLiteral("utility"));
        addFabric(QStringLiteral("detail-armor-bar"),     QStringLiteral("Detail Armor Bar"),    tr("详细盔甲耐久显示"),                    QStringLiteral("[防御]"), QStringLiteral("utility"));
        addFabric(QStringLiteral("xaeros-minimap"),       QStringLiteral("Xaero's Minimap"),     tr("小地图Mod"),                           QStringLiteral("[地图]"), QStringLiteral("utility"));
        addFabric(QStringLiteral("journeymap"),           QStringLiteral("JourneyMap"),          tr("实时地图Mod"),                         QStringLiteral("[地图]"), QStringLiteral("utility"));

        // Forge
        QMap<QString, QJsonObject> forge;
        auto addForge = [&](const QString& slug, const QString& title,
                            const QString& desc, const QString& icon,
                            const QString& category) {
            QJsonObject o;
            o[QStringLiteral("title")]    = title;
            o[QStringLiteral("desc")]     = desc;
            o[QStringLiteral("icon")]     = icon;
            o[QStringLiteral("category")] = category;
            forge[slug] = o;
        };
        addForge(QStringLiteral("jei"),                   QStringLiteral("JEI"),                 tr("物品配方查看器，必备"),               QStringLiteral("[手册]"), QStringLiteral("utility"));
        addForge(QStringLiteral("optifine"),              QStringLiteral("OptiFine"),            tr("经典优化+光影Mod"),                    QStringLiteral("[优化]"), QStringLiteral("optimization"));
        addForge(QStringLiteral("create"),                QStringLiteral("Create"),              tr("机械动力模组，工业化"),                QStringLiteral("[配置]"), QStringLiteral("technology"));
        addForge(QStringLiteral("botania"),               QStringLiteral("Botania"),             tr("植物魔法，自然魔法"),                  QStringLiteral("[植物]"), QStringLiteral("magic"));
        addForge(QStringLiteral("tinkers-construct"),     QStringLiteral("Tinkers' Construct"),  tr("匠魂，自定义工具"),                    QStringLiteral("[工具]"), QStringLiteral("equipment"));
        addForge(QStringLiteral("iron-chests"),           QStringLiteral("Iron Chests"),         tr("更多箱子类型"),                        QStringLiteral("[存储]"), QStringLiteral("storage"));
        addForge(QStringLiteral("applied-energistics-2"), QStringLiteral("Applied Energistics 2"),tr("AE2，数字化存储"),                  QStringLiteral("[存储]"), QStringLiteral("storage"));
        addForge(QStringLiteral("thermal-expansion"),     QStringLiteral("Thermal Expansion"),   tr("热能扩展，科技模组"),                  QStringLiteral("[热能]"), QStringLiteral("technology"));
        addForge(QStringLiteral("mekanism"),              QStringLiteral("Mekanism"),            tr("通用机械，高科技工业"),                QStringLiteral("[科技]"), QStringLiteral("technology"));
        addForge(QStringLiteral("biomes-o-plenty"),       QStringLiteral("Biomes O' Plenty"),    tr("超多生物群系"),                        QStringLiteral("[森林]"), QStringLiteral("world-generation"));
        addForge(QStringLiteral("twilight-forest"),       QStringLiteral("Twilight Forest"),     tr("暮色森林，经典冒险"),                  QStringLiteral("[森林]"), QStringLiteral("adventure"));
        addForge(QStringLiteral("pams-harvestcraft"),     QStringLiteral("Pam's HarvestCraft"),  tr("更多农作物和食物"),                    QStringLiteral("[作物]"), QStringLiteral("food"));
        addForge(QStringLiteral("quark"),                 QStringLiteral("Quark"),               tr("夸克，小而美的功能合集"),              QStringLiteral("[组件]"), QStringLiteral("utility"));
        addForge(QStringLiteral("chisel"),                QStringLiteral("Chisel"),              tr("凿子，建筑方块变体"),                  QStringLiteral("[建筑]"), QStringLiteral("decoration"));
        addForge(QStringLiteral("waystones"),             QStringLiteral("Waystones"),           tr("传送石碑"),                            QStringLiteral("[建筑]"), QStringLiteral("utility"));
        addForge(QStringLiteral("gravestone"),            QStringLiteral("GraveStone Mod"),      tr("死亡不掉落，墓碑留存"),                QStringLiteral("[功能]"), QStringLiteral("utility"));
        addForge(QStringLiteral("jade"),                  QStringLiteral("Jade"),                tr("方块/实体信息显示"),                   QStringLiteral("[信息]"), QStringLiteral("utility"));
        addForge(QStringLiteral("the-one-probe"),         QStringLiteral("The One Probe"),       tr("信息探头，查看方块详情"),              QStringLiteral("[探索]"), QStringLiteral("utility"));
        addForge(QStringLiteral("curios"),                QStringLiteral("Curios API"),          tr("饰品API，多模组依赖"),                 QStringLiteral("[饰品]"), QStringLiteral("library"));
        addForge(QStringLiteral("patchouli"),             QStringLiteral("Patchouli"),           tr("模组说明书API"),                       QStringLiteral("[书籍]"), QStringLiteral("library"));

        allMods[QStringLiteral("fabric")] = fabric;
        allMods[QStringLiteral("forge")]  = forge;
    }

    return allMods.value(loader);
}

// ============================================================
// Static data — Shader Packs
// ============================================================

QMap<QString, QJsonObject> ModManager::getShaderList()
{
    static QMap<QString, QJsonObject> shaders;

    if (shaders.isEmpty()) {
        auto add = [&](const QString& slug, const QString& title,
                       const QString& desc, const QString& icon) {
            QJsonObject o;
            o[QStringLiteral("title")] = title;
            o[QStringLiteral("desc")]  = desc;
            o[QStringLiteral("icon")]  = icon;
            shaders[slug] = o;
        };

        add(QStringLiteral("bsl"),                     QStringLiteral("BSL Shaders"),                tr("经典全能光影，性能画质平衡"),         QStringLiteral("[光影]"));
        add(QStringLiteral("complementary"),            QStringLiteral("Complementary Shaders"),       tr("BSL魔改，画面更精美"),                QStringLiteral("[色彩]"));
        add(QStringLiteral("complementary-reimagined"), QStringLiteral("Complementary Reimagined"),    tr("互补重制版，极致画面"),                QStringLiteral("[图像]"));
        add(QStringLiteral("sildurs-vibrant"),          QStringLiteral("Sildur's Vibrant Shaders"),   tr("色彩鲜艳，光影柔和"),                 QStringLiteral("[色彩]"));
        add(QStringLiteral("sildurs-enhanced-default"), QStringLiteral("Sildur's Enhanced Default"),  tr("轻量级，低配机可选"),                 QStringLiteral("[光照]"));
        add(QStringLiteral("seus"),                    QStringLiteral("SEUS"),                        tr("经典光追级光影"),                      QStringLiteral("[日照]"));
        add(QStringLiteral("seus-ptgi"),               QStringLiteral("SEUS PTGI"),                   tr("路径追踪光影，画质巅峰"),              QStringLiteral("[优质]"));
        add(QStringLiteral("solas"),                   QStringLiteral("Solas Shader"),                tr("纳斯达克同款 (Derivative)"),           QStringLiteral("[晨昏]"));
        add(QStringLiteral("rethinking-voxels"),       QStringLiteral("Rethinking Voxels"),           tr("体素光照，原生风格"),                  QStringLiteral("[光照]"));
        add(QStringLiteral("photon"),                  QStringLiteral("Photon Shader"),               tr("新锐光影，画面梦幻"),                  QStringLiteral("[特效]"));
        add(QStringLiteral("astra"),                   QStringLiteral("AstraLex Shaders"),            tr("BSL分支，特效丰富"),                  QStringLiteral("[星空]"));
        add(QStringLiteral("chocapic13"),              QStringLiteral("Chocapic13's Shaders"),        tr("老牌光影，多配置档"),                  QStringLiteral("[山景]"));
        add(QStringLiteral("makeup-ultra-fast"),       QStringLiteral("MakeUp Ultra Fast"),           tr("超轻量，低配福利"),                    QStringLiteral("[优化]"));
        add(QStringLiteral("vanilla-plus"),            QStringLiteral("Vanilla Plus"),                tr("原版风格增强"),                        QStringLiteral("[自然]"));
        add(QStringLiteral("bliss"),                   QStringLiteral("Bliss Shader"),                tr("自然写实风格"),                        QStringLiteral("[风景]"));
    }

    return shaders;
}

// ============================================================
// Static data — Categories (English → 中文)
// ============================================================

QMap<QString, QString> ModManager::getCategories()
{
    static QMap<QString, QString> categories;

    if (categories.isEmpty()) {
        categories[QStringLiteral("adventure")]       = tr("冒险");
        categories[QStringLiteral("cursed")]          = tr("整活");
        categories[QStringLiteral("decoration")]      = tr("装饰");
        categories[QStringLiteral("economy")]         = tr("经济");
        categories[QStringLiteral("equipment")]       = tr("装备");
        categories[QStringLiteral("food")]            = tr("食物");
        categories[QStringLiteral("game-mechanics")]  = tr("游戏机制");
        categories[QStringLiteral("library")]         = tr("前置库");
        categories[QStringLiteral("magic")]           = tr("魔法");
        categories[QStringLiteral("management")]      = tr("管理");
        categories[QStringLiteral("minigame")]        = tr("小游戏");
        categories[QStringLiteral("mobs")]            = tr("生物");
        categories[QStringLiteral("optimization")]    = tr("优化");
        categories[QStringLiteral("social")]          = tr("社交");
        categories[QStringLiteral("storage")]         = tr("存储");
        categories[QStringLiteral("technology")]      = tr("科技");
        categories[QStringLiteral("transportation")]  = tr("运输");
        categories[QStringLiteral("utility")]         = tr("工具");
        categories[QStringLiteral("world-generation")]= tr("世界生成");
    }

    return categories;
}

} // namespace ShadowLauncher
