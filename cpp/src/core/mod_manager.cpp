#include "mod_manager.h"
#include "http_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDir>
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
    emit logMessage(QStringLiteral("正在搜索 Modrinth..."));

    QString url = buildSearchUrl(query, categories, gameVersions, loaders,
                                 offset, limit, sort);

    HttpClient::instance().get(url,
        [this](int status, const QByteArray& body) {
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = parseSearchResponse(body, totalHits);
                m_lastTotalHits = totalHits;
                emit searchCompleted(results, totalHits);
                emit logMessage(QStringLiteral("搜索完成，共 %1 个结果").arg(totalHits));
            } else {
                emit searchFailed(QStringLiteral("搜索失败: HTTP %1").arg(status));
            }
            setBusy(false);
        },
        [this](const QString& error) {
            emit searchFailed(QStringLiteral("网络错误: %1").arg(error));
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
    emit logMessage(QStringLiteral("正在获取 %1 的版本列表...").arg(slug));

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
                emit logMessage(QStringLiteral("获取到 %1 的 %2 个文件版本")
                                    .arg(slug).arg(files.size()));
                emit versionsFetched(slug, files);
            } else {
                emit versionsFailed(slug,
                    QStringLiteral("获取版本失败: HTTP %1").arg(status));
            }
            setBusy(false);
        },
        [this, slug](const QString& error) {
            emit versionsFailed(slug,
                QStringLiteral("网络错误: %1").arg(error));
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

    emit logMessage(QStringLiteral("正在查找 %1 的 %2/%3 版本...")
                        .arg(slug, loader, gameVersion));

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
            QStringLiteral("❌ 未找到 %1 的 %2 %3 版本")
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
        QStringLiteral("找到 %1 个文件，选择: %2 (%3)")
            .arg(files.size())
            .arg(filename, formatFileSize(fileSize)));

    // Build destination path
    QString destDir = m_minecraftDir + QStringLiteral("/") + m_pendingDownload.targetDir;
    QDir().mkpath(destDir);
    QString destPath = destDir + QStringLiteral("/") + filename;

    emit logMessage(QStringLiteral("开始下载: %1").arg(dlUrl));

    // Use HttpClient::download() for the actual transfer.
    // TODO: switch to ParallelDownloader with mirror support once implemented.
    HttpClient::instance().download(
        dlUrl, destPath,
        [this, slug](qint64 received, qint64 total) {
            emit downloadProgress(slug, received, total);
        },
        [this, slug, destPath, filename](bool ok, const QString& error) {
            setBusy(false);
            if (ok) {
                emit logMessage(QStringLiteral("✅ %1 下载完成!").arg(filename));
            } else {
                emit logMessage(QStringLiteral("❌ %1 下载失败: %2").arg(filename, error));
            }
            emit downloadFinished(slug, ok, ok ? destPath : QString());
        }
    );
}

// ═══════════════════════════════════════════════════════════
// Resource packs — search + download
// ═══════════════════════════════════════════════════════════

void ModManager::searchResourcepacks(
    const QString& query,
    const QStringList& gameVersions,
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

    emit logMessage(QStringLiteral("[MODRINTH] 搜索资源包: %1").arg(url.toString()));

    HttpClient::instance().get(url.toString(),
        [this](int /*status*/, const QByteArray& data) {
            setBusy(false);
            int total = 0;
            QJsonArray hits = parseSearchResponse(data, total);
            emit resourcepackSearchCompleted(hits, total);
            emit logMessage(QStringLiteral("[MODRINTH] 资源包搜索结果: %1/%2").arg(hits.size()).arg(total));
        },
        [this](const QString& err) {
            setBusy(false);
            emit resourcepackSearchFailed(err);
            emit logMessage(QStringLiteral("[MODRINTH] 资源包搜索失败: %1").arg(err));
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

    emit logMessage(QStringLiteral("[MODRINTH] 下载资源包: %1 MC%2").arg(slug, gameVersion));

    // Step 1: fetch versions, pick matching one, download primary file
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
                emit resourcepackDownloadFinished(slug, false, {});
                return;
            }

            // Pick best file: match game version, or first
            QJsonObject bestFile;
            for (const QJsonValue& fv : files) {
                QJsonObject f = fv.toObject();
                QJsonArray gvs = f[QStringLiteral("gameVersions")].toArray();
                bool match = false;
                for (const QJsonValue& gv : gvs) {
                    if (gv.toString() == gameVersion) { match = true; break; }
                }
                if (match || bestFile.isEmpty()) {
                    bestFile = f;
                    if (match) break;
                }
            }

            if (bestFile.isEmpty()) {
                setBusy(false);
                emit resourcepackDownloadFinished(slug, false, {});
                return;
            }

            QString dlUrl = bestFile[QStringLiteral("url")].toString();
            QString filename = bestFile[QStringLiteral("filename")].toString();
            if (filename.isEmpty()) filename = slug + QStringLiteral(".zip");

            // Rewrite cdn.modrinth.com → MCIM CDN mirror
            dlUrl.replace(QStringLiteral("cdn.modrinth.com"), MCIM_CDN);
            dlUrl.replace(QStringLiteral("cdn-alt.modrinth.com"), MCIM_CDN);

            QString destDir = minecraftDir + QStringLiteral("/resourcepacks");
            QDir().mkpath(destDir);
            QString destPath = destDir + QStringLiteral("/") + filename;

            emit logMessage(QStringLiteral("[MODRINTH] 资源包文件: %1 → %2").arg(filename, dlUrl));

            HttpClient::instance().download(
                dlUrl, destPath,
                [this, slug](qint64 received, qint64 total) {
                    emit downloadProgress(slug, received, total);
                },
                [this, slug, destPath, filename](bool ok, const QString& error) {
                    setBusy(false);
                    if (ok)
                        emit logMessage(QStringLiteral("[MODRINTH] 资源包下载完成: %1").arg(filename));
                    else
                        emit logMessage(QStringLiteral("[MODRINTH] 资源包下载失败: %1").arg(error));
                    emit resourcepackDownloadFinished(slug, ok, ok ? destPath : QString());
                }
            );
        },
        [this, slug](const QString& err) {
            setBusy(false);
            emit resourcepackDownloadFinished(slug, false, {});
            emit logMessage(QStringLiteral("[MODRINTH] 获取版本列表失败: %1").arg(err));
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

    emit logMessage(QStringLiteral("[MODRINTH] 获取 %1 个资源包版本 (批量 %2/次)")
                    .arg(slugs.size()).arg(kBatchSize));

    // Emit progress signal so QML can show loading
    emit resourcepackVersionsProgress(0, slugs.size());

    // Shared version parser (avoids code duplication)
    auto processOne = [this, results, pending, resolved, total, slugList]() {
        int done = ++(*resolved);
        emit logMessage(QStringLiteral("[MODRINTH-BATCH] 完成 %1/%2").arg(done).arg(*total));
        emit resourcepackVersionsProgress(done, *total);
        if (done >= *total) {
            emit resourcepackVersionsLoaded(*results);
            emit logMessage(QStringLiteral("[MODRINTH-BATCH] 全部完成 (%1 个)").arg(*total));
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

        emit logMessage(QStringLiteral("[MODRINTH-BATCH] 启动批次 [%1..%2] (共%3个)")
                        .arg(start).arg(end - 1).arg(end - start));

        for (int i = start; i < end; ++i) {
            const QString& slug = (*slugList)[i];
            QUrl url(MODRINTH_API + "/project/" + slug + "/version");

            emit logMessage(QStringLiteral("[MODRINTH-BATCH] HTTP GET #%1 slug=%2").arg(i).arg(slug));

            HttpClient::instance().get(url.toString(),
                [this, slug, i, results, processOne](int status, const QByteArray& data) {
                    emit logMessage(QStringLiteral("[MODRINTH-BATCH] #%1 slug=%2 HTTP=%3 size=%4")
                                    .arg(i).arg(slug).arg(status).arg(data.size()));
                    QJsonArray versions = parseVersionsResponse(data);
                    QStringList majorVersions;
                    QSet<QString> seen;

                    for (const QJsonValue& v : versions) {
                        QJsonObject ver = v.toObject();
                        QJsonArray gvs = ver[QStringLiteral("gameVersions")].toArray();
                        for (const QJsonValue& gv : gvs) {
                            QString gvStr = gv.toString();
                            static QRegularExpression majorRe(QStringLiteral("^(\\d+\\.\\d+)"));
                            auto match = majorRe.match(gvStr);
                            if (match.hasMatch()) {
                                QString major = match.captured(1);  // "1.21" not "1.21.X"
                                if (!seen.contains(major)) {
                                    seen.insert(major);
                                    majorVersions.append(major);
                                }
                            } else {
                                if (!seen.contains(gvStr)) {
                                    seen.insert(gvStr);
                                    majorVersions.append(gvStr);
                                }
                            }
                        }
                    }

                    std::sort(majorVersions.begin(), majorVersions.end(),
                        [](const QString& a, const QString& b) {
                            if (a == QStringLiteral("全版本")) return true;
                            if (b == QStringLiteral("全版本")) return false;
                            static QRegularExpression verRe(QStringLiteral("^(\\d+)\\.(\\d+)"));
                            auto ma = verRe.match(a);
                            auto mb = verRe.match(b);
                            if (ma.hasMatch() && mb.hasMatch()) {
                                int aMaj = ma.captured(1).toInt();
                                int bMaj = mb.captured(1).toInt();
                                if (aMaj != bMaj) return aMaj > bMaj;
                                return ma.captured(2).toInt() > mb.captured(2).toInt();
                            }
                            return a > b;
                        });

                    (*results)[slug] = majorVersions;
                    emit resourcepackVersionsPartial(slug, majorVersions);
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
        mod[QStringLiteral("categories")]    = hit.value(QStringLiteral("categories"));
        mod[QStringLiteral("gameVersions")]  = hit.value(QStringLiteral("versions"));
        // Modrinth uses "categories" for loaders in search results
        mod[QStringLiteral("loaders")]       = hit.value(QStringLiteral("categories"));
        mod[QStringLiteral("latestVersion")] = hit.value(QStringLiteral("latest_version"));

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

            files.append(file);
        }
    }

    return files;
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
        addFabric(QStringLiteral("sodium"),              QStringLiteral("Sodium"),              QStringLiteral("极致性能优化，FPS提升巨大"),           QStringLiteral("⚡"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("iris"),                 QStringLiteral("Iris Shaders"),        QStringLiteral("光影加载器，Fabric首选"),              QStringLiteral("✨"), QStringLiteral("shader"));
        addFabric(QStringLiteral("lithium"),              QStringLiteral("Lithium"),             QStringLiteral("服务端性能优化，无兼容性问题"),         QStringLiteral("🚀"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("phosphor"),             QStringLiteral("Phosphor"),            QStringLiteral("光照引擎优化（已替代为Starlight）"),    QStringLiteral("💡"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("starlight"),            QStringLiteral("Starlight"),           QStringLiteral("新一代光照引擎，性能飞升"),            QStringLiteral("🌟"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("ferritecore"),          QStringLiteral("FerriteCore"),         QStringLiteral("大幅降低内存占用"),                     QStringLiteral("🧠"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("modmenu"),              QStringLiteral("Mod Menu"),            QStringLiteral("Fabric必备，Mod列表查看"),             QStringLiteral("📋"), QStringLiteral("utility"));
        addFabric(QStringLiteral("cloth-config"),         QStringLiteral("Cloth Config"),        QStringLiteral("配置API，很多Mod依赖它"),              QStringLiteral("⚙️"), QStringLiteral("library"));
        addFabric(QStringLiteral("sodium-extra"),         QStringLiteral("Sodium Extra"),        QStringLiteral("Sodium功能扩展"),                      QStringLiteral("🔧"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("reeses-sodium-options"),QStringLiteral("Reese's Sodium Options"), QStringLiteral("Sodium选项增强"),                QStringLiteral("🎛️"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("continuity"),           QStringLiteral("Continuity"),          QStringLiteral("连接纹理（类似OptiFine的玻璃效果）"),  QStringLiteral("🧱"), QStringLiteral("decoration"));
        addFabric(QStringLiteral("lambda"),               QStringLiteral("LambdaBetterGrass"),   QStringLiteral("更好的草地渲染"),                      QStringLiteral("🌿"), QStringLiteral("decoration"));
        addFabric(QStringLiteral("entityculling"),        QStringLiteral("Entity Culling"),      QStringLiteral("实体渲染优化"),                        QStringLiteral("👁️"), QStringLiteral("optimization"));
        addFabric(QStringLiteral("indium"),               QStringLiteral("Indium"),              QStringLiteral("Sodium + Fabric渲染API兼容"),          QStringLiteral("🔄"), QStringLiteral("library"));
        addFabric(QStringLiteral("fabric-api"),           QStringLiteral("Fabric API"),          QStringLiteral("Fabric核心API，必装"),                 QStringLiteral("🧵"), QStringLiteral("library"));
        addFabric(QStringLiteral("lambdynamiclights"),    QStringLiteral("LambDynamicLights"),   QStringLiteral("动态光源（手持火把发光）"),           QStringLiteral("🔥"), QStringLiteral("decoration"));
        addFabric(QStringLiteral("zoomify"),              QStringLiteral("Zoomify"),             QStringLiteral("缩放Mod（类似OptiFine的C键）"),       QStringLiteral("🔍"), QStringLiteral("utility"));
        addFabric(QStringLiteral("detail-armor-bar"),     QStringLiteral("Detail Armor Bar"),    QStringLiteral("详细盔甲耐久显示"),                    QStringLiteral("🛡️"), QStringLiteral("utility"));
        addFabric(QStringLiteral("xaeros-minimap"),       QStringLiteral("Xaero's Minimap"),     QStringLiteral("小地图Mod"),                           QStringLiteral("🗺️"), QStringLiteral("utility"));
        addFabric(QStringLiteral("journeymap"),           QStringLiteral("JourneyMap"),          QStringLiteral("实时地图Mod"),                         QStringLiteral("🗺️"), QStringLiteral("utility"));

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
        addForge(QStringLiteral("jei"),                   QStringLiteral("JEI"),                 QStringLiteral("物品配方查看器，必备"),               QStringLiteral("📖"), QStringLiteral("utility"));
        addForge(QStringLiteral("optifine"),              QStringLiteral("OptiFine"),            QStringLiteral("经典优化+光影Mod"),                    QStringLiteral("⚡"), QStringLiteral("optimization"));
        addForge(QStringLiteral("create"),                QStringLiteral("Create"),              QStringLiteral("机械动力模组，工业化"),                QStringLiteral("⚙️"), QStringLiteral("technology"));
        addForge(QStringLiteral("botania"),               QStringLiteral("Botania"),             QStringLiteral("植物魔法，自然魔法"),                  QStringLiteral("🌺"), QStringLiteral("magic"));
        addForge(QStringLiteral("tinkers-construct"),     QStringLiteral("Tinkers' Construct"),  QStringLiteral("匠魂，自定义工具"),                    QStringLiteral("🔨"), QStringLiteral("equipment"));
        addForge(QStringLiteral("iron-chests"),           QStringLiteral("Iron Chests"),         QStringLiteral("更多箱子类型"),                        QStringLiteral("📦"), QStringLiteral("storage"));
        addForge(QStringLiteral("applied-energistics-2"), QStringLiteral("Applied Energistics 2"),QStringLiteral("AE2，数字化存储"),                  QStringLiteral("💾"), QStringLiteral("storage"));
        addForge(QStringLiteral("thermal-expansion"),     QStringLiteral("Thermal Expansion"),   QStringLiteral("热能扩展，科技模组"),                  QStringLiteral("🔥"), QStringLiteral("technology"));
        addForge(QStringLiteral("mekanism"),              QStringLiteral("Mekanism"),            QStringLiteral("通用机械，高科技工业"),                QStringLiteral("⚛️"), QStringLiteral("technology"));
        addForge(QStringLiteral("biomes-o-plenty"),       QStringLiteral("Biomes O' Plenty"),    QStringLiteral("超多生物群系"),                        QStringLiteral("🌲"), QStringLiteral("world-generation"));
        addForge(QStringLiteral("twilight-forest"),       QStringLiteral("Twilight Forest"),     QStringLiteral("暮色森林，经典冒险"),                  QStringLiteral("🌳"), QStringLiteral("adventure"));
        addForge(QStringLiteral("pams-harvestcraft"),     QStringLiteral("Pam's HarvestCraft"),  QStringLiteral("更多农作物和食物"),                    QStringLiteral("🌾"), QStringLiteral("food"));
        addForge(QStringLiteral("quark"),                 QStringLiteral("Quark"),               QStringLiteral("夸克，小而美的功能合集"),              QStringLiteral("🧩"), QStringLiteral("utility"));
        addForge(QStringLiteral("chisel"),                QStringLiteral("Chisel"),              QStringLiteral("凿子，建筑方块变体"),                  QStringLiteral("🪨"), QStringLiteral("decoration"));
        addForge(QStringLiteral("waystones"),             QStringLiteral("Waystones"),           QStringLiteral("传送石碑"),                            QStringLiteral("🏛️"), QStringLiteral("utility"));
        addForge(QStringLiteral("gravestone"),            QStringLiteral("GraveStone Mod"),      QStringLiteral("死亡不掉落，墓碑留存"),                QStringLiteral("🪦"), QStringLiteral("utility"));
        addForge(QStringLiteral("jade"),                  QStringLiteral("Jade"),                QStringLiteral("方块/实体信息显示"),                   QStringLiteral("ℹ️"), QStringLiteral("utility"));
        addForge(QStringLiteral("the-one-probe"),         QStringLiteral("The One Probe"),       QStringLiteral("信息探头，查看方块详情"),              QStringLiteral("🔬"), QStringLiteral("utility"));
        addForge(QStringLiteral("curios"),                QStringLiteral("Curios API"),          QStringLiteral("饰品API，多模组依赖"),                 QStringLiteral("💍"), QStringLiteral("library"));
        addForge(QStringLiteral("patchouli"),             QStringLiteral("Patchouli"),           QStringLiteral("模组说明书API"),                       QStringLiteral("📚"), QStringLiteral("library"));

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

        add(QStringLiteral("bsl"),                     QStringLiteral("BSL Shaders"),                QStringLiteral("经典全能光影，性能画质平衡"),         QStringLiteral("✨"));
        add(QStringLiteral("complementary"),            QStringLiteral("Complementary Shaders"),       QStringLiteral("BSL魔改，画面更精美"),                QStringLiteral("🎨"));
        add(QStringLiteral("complementary-reimagined"), QStringLiteral("Complementary Reimagined"),    QStringLiteral("互补重制版，极致画面"),                QStringLiteral("🖼️"));
        add(QStringLiteral("sildurs-vibrant"),          QStringLiteral("Sildur's Vibrant Shaders"),   QStringLiteral("色彩鲜艳，光影柔和"),                 QStringLiteral("🌈"));
        add(QStringLiteral("sildurs-enhanced-default"), QStringLiteral("Sildur's Enhanced Default"),  QStringLiteral("轻量级，低配机可选"),                 QStringLiteral("💡"));
        add(QStringLiteral("seus"),                    QStringLiteral("SEUS"),                        QStringLiteral("经典光追级光影"),                      QStringLiteral("☀️"));
        add(QStringLiteral("seus-ptgi"),               QStringLiteral("SEUS PTGI"),                   QStringLiteral("路径追踪光影，画质巅峰"),              QStringLiteral("🌟"));
        add(QStringLiteral("solas"),                   QStringLiteral("Solas Shader"),                QStringLiteral("纳斯达克同款 (Derivative)"),           QStringLiteral("🌅"));
        add(QStringLiteral("rethinking-voxels"),       QStringLiteral("Rethinking Voxels"),           QStringLiteral("体素光照，原生风格"),                  QStringLiteral("🔆"));
        add(QStringLiteral("photon"),                  QStringLiteral("Photon Shader"),               QStringLiteral("新锐光影，画面梦幻"),                  QStringLiteral("💫"));
        add(QStringLiteral("astra"),                   QStringLiteral("AstraLex Shaders"),            QStringLiteral("BSL分支，特效丰富"),                  QStringLiteral("🌌"));
        add(QStringLiteral("chocapic13"),              QStringLiteral("Chocapic13's Shaders"),        QStringLiteral("老牌光影，多配置档"),                  QStringLiteral("🌄"));
        add(QStringLiteral("makeup-ultra-fast"),       QStringLiteral("MakeUp Ultra Fast"),           QStringLiteral("超轻量，低配福利"),                    QStringLiteral("⚡"));
        add(QStringLiteral("vanilla-plus"),            QStringLiteral("Vanilla Plus"),                QStringLiteral("原版风格增强"),                        QStringLiteral("🌿"));
        add(QStringLiteral("bliss"),                   QStringLiteral("Bliss Shader"),                QStringLiteral("自然写实风格"),                        QStringLiteral("🏞️"));
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
        categories[QStringLiteral("adventure")]       = QStringLiteral("冒险");
        categories[QStringLiteral("cursed")]          = QStringLiteral("整活");
        categories[QStringLiteral("decoration")]      = QStringLiteral("装饰");
        categories[QStringLiteral("economy")]         = QStringLiteral("经济");
        categories[QStringLiteral("equipment")]       = QStringLiteral("装备");
        categories[QStringLiteral("food")]            = QStringLiteral("食物");
        categories[QStringLiteral("game-mechanics")]  = QStringLiteral("游戏机制");
        categories[QStringLiteral("library")]         = QStringLiteral("前置库");
        categories[QStringLiteral("magic")]           = QStringLiteral("魔法");
        categories[QStringLiteral("management")]      = QStringLiteral("管理");
        categories[QStringLiteral("minigame")]        = QStringLiteral("小游戏");
        categories[QStringLiteral("mobs")]            = QStringLiteral("生物");
        categories[QStringLiteral("optimization")]    = QStringLiteral("优化");
        categories[QStringLiteral("social")]          = QStringLiteral("社交");
        categories[QStringLiteral("storage")]         = QStringLiteral("存储");
        categories[QStringLiteral("technology")]      = QStringLiteral("科技");
        categories[QStringLiteral("transportation")]  = QStringLiteral("运输");
        categories[QStringLiteral("utility")]         = QStringLiteral("工具");
        categories[QStringLiteral("world-generation")]= QStringLiteral("世界生成");
    }

    return categories;
}

} // namespace ShadowLauncher
