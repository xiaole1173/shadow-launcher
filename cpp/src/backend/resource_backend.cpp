// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "resource_backend.h"
#include "../core/resource_fetch_engine.h"
#include "core/mod_manager.h"
#include "core/http_client.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

namespace ShadowLauncher {

// ============================================================
// Constructor / Destructor
// ============================================================

ResourceBackend::ResourceBackend(QObject* parent)
    : QObject(parent)
    , m_modMgr(new ModManager(this))
{
    connect(m_modMgr, &ModManager::searchCompleted,
            this, &ResourceBackend::onSearchCompleted);
    connect(m_modMgr, &ModManager::downloadProgress,
            this, &ResourceBackend::onDownloadProgress);
    connect(m_modMgr, &ModManager::downloadFinished,
            this, &ResourceBackend::onDownloadFinished);
    connect(m_modMgr, &ModManager::resourcepackSearchCompleted,
            this, &ResourceBackend::onResourcepackSearchCompleted);
    connect(m_modMgr, &ModManager::resourcepackDownloadFinished,
            this, &ResourceBackend::onResourcepackDownloadFinished);
    connect(m_modMgr, &ModManager::resourcepackVersionsLoaded,
            this, &ResourceBackend::onResourcepackVersionsLoaded);
    connect(m_modMgr, &ModManager::resourcepackVersionsPartial,
            this, &ResourceBackend::resourcepackVersionsPartial);
    connect(m_modMgr, &ModManager::resourcepackVersionsProgress,
            this, &ResourceBackend::resourcepackVersionsProgress);
    connect(m_modMgr, &ModManager::modVersionsLoaded,
            this, &ResourceBackend::onModVersionsLoaded);
    connect(m_modMgr, &ModManager::modVersionsPartial,
            this, &ResourceBackend::modVersionsPartial);
    connect(m_modMgr, &ModManager::modVersionsProgress,
            this, &ResourceBackend::modVersionsProgress);
    connect(m_modMgr, &ModManager::shaderVersionsLoaded,
            this, &ResourceBackend::onShaderVersionsLoaded);
    connect(m_modMgr, &ModManager::shaderVersionsPartial,
            this, &ResourceBackend::shaderVersionsPartial);
    connect(m_modMgr, &ModManager::shaderVersionsProgress,
            this, &ResourceBackend::shaderVersionsProgress);
    connect(m_modMgr, &ModManager::logMessage,
            this, &ResourceBackend::logMessage);

    // Mod file download
    connect(m_modMgr, &ModManager::modFileDownloadStarted,
            this, &ResourceBackend::onModFileDownloadStarted);
    connect(m_modMgr, &ModManager::modFileDownloadProgress,
            this, &ResourceBackend::onModFileDownloadProgress);
    connect(m_modMgr, &ModManager::modFileDownloadFinished,
            this, &ResourceBackend::onModFileDownloadFinished);
    connect(m_modMgr, &ModManager::modFileDownloadFailed,
            this, &ResourceBackend::onModFileDownloadFailed);
}

ResourceBackend::~ResourceBackend() = default;

void ResourceBackend::setFetchEngine(ResourceFetchEngine* e)
{
    m_fetchEngine = e;
    if (m_modMgr) m_modMgr->setFetchEngine(e); // ModManager 搜索同样走引擎
}

// ============================================================
// Public Slots — Popular Mods / Shaders (offline data)
// ============================================================

QVariantList ResourceBackend::getPopularMods(const QString& loader)
{
    const auto mods = ModManager::getPopularMods(loader);
    QVariantList result;
    for (auto it = mods.cbegin(); it != mods.cend(); ++it) {
        const QJsonObject& info = it.value();
        QVariantMap entry;
        entry[QStringLiteral("slug")]     = it.key();
        entry[QStringLiteral("title")]    = info[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]     = info[QStringLiteral("desc")].toString();
        entry[QStringLiteral("icon")]     = info[QStringLiteral("icon")].toString();
        entry[QStringLiteral("category")] = info[QStringLiteral("category")].toString();
        result.append(entry);
    }
    emit logMessage(tr("加载 %1 个 %2 推荐Mod")
                        .arg(result.size())
                        .arg(loader.toUpper()));
    return result;
}

QVariantList ResourceBackend::getShaderList()
{
    const auto shaders = ModManager::getShaderList();
    QVariantList result;
    for (auto it = shaders.cbegin(); it != shaders.cend(); ++it) {
        const QJsonObject& info = it.value();
        QVariantMap entry;
        entry[QStringLiteral("slug")]      = it.key();
        entry[QStringLiteral("title")]     = info[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]      = info[QStringLiteral("desc")].toString();
        entry[QStringLiteral("icon")]      = info[QStringLiteral("icon")].toString();
        entry[QStringLiteral("downloads")] = 0;
        result.append(entry);
    }
    emit logMessage(tr("加载 %1 个光影包").arg(result.size()));
    return result;
}

// ============================================================
// Public Slots — Online Search
// ============================================================

void ResourceBackend::searchMods(const QString& query, const QString& loader)
{
    m_searchKind = SearchKind::Mod;
    QStringList loaders;
    if (!loader.isEmpty())
        loaders << loader;

    emit logMessage(tr("[搜索] 正在搜索Mod: %1 (%2)...").arg(query, loader));
    m_modMgr->searchModrinthProjects(query, {QStringLiteral("project_type:mod")}, {}, loaders, 0, 20,
                             QStringLiteral("relevance"));
}

void ResourceBackend::searchModsEx(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    const QString& environment, const QString& license,
    int offset, int limit)
{
    m_searchKind = SearchKind::Mod;
    QStringList loaders;
    if (!loader.isEmpty())
        loaders << loader;

    // Build Modrinth facets as OR groups:
    // [["project_type:mod"], ["categories:X"], ["client_side:required"], ["license:mit",...]]
    QJsonArray facetArr;

    // Base: project_type = mod
    facetArr.append(QJsonArray{QStringLiteral("project_type:mod")});

    // Category (single, OR with project_type)
    if (!category.isEmpty()) {
        facetArr.append(QJsonArray{QStringLiteral("categories:") + category});
    }

    // Game versions — OR group
    if (!gameVersions.isEmpty()) {
        QJsonArray verGroup;
        for (const QString& v : gameVersions)
            verGroup.append(QStringLiteral("versions:") + v);
        facetArr.append(verGroup);
    }

    // Environment
    if (!environment.isEmpty()) {
        if (environment == QStringLiteral("client"))
            facetArr.append(QJsonArray{QStringLiteral("client_side:required")});
        else if (environment == QStringLiteral("server"))
            facetArr.append(QJsonArray{QStringLiteral("server_side:required")});
    }

    // License — OR group of common open source licenses
    if (license == QStringLiteral("open_source")) {
        QJsonArray licGroup;
        licGroup.append(QStringLiteral("license:mit"));
        licGroup.append(QStringLiteral("license:gpl-3.0"));
        licGroup.append(QStringLiteral("license:lgpl-3.0"));
        licGroup.append(QStringLiteral("license:apache-2.0"));
        licGroup.append(QStringLiteral("license:mpl-2.0"));
        licGroup.append(QStringLiteral("license:bsd-3-clause"));
        licGroup.append(QStringLiteral("license:unlicense"));
        facetArr.append(licGroup);
    }

    emit logMessage(tr("[搜索] 搜索Mod(ex): q=%1 loader=%2 cat=%3 ver=%4 env=%5 lic=%6 offset=%7")
                        .arg(query, loader, category, gameVersions.join(','),
                             environment, license).arg(offset));

    // Build URL directly (bypass searchModrinthProjects for proper facet handling)
    QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
    QUrlQuery params;
    if (!query.isEmpty())
        params.addQueryItem(QStringLiteral("query"), query);
    // Add loaders as separate OR group
    if (!loaders.isEmpty()) {
        QJsonArray ldGroup;
        for (const QString& l : loaders)
            ldGroup.append(QStringLiteral("categories:") + l);
        facetArr.append(ldGroup);
    }
    params.addQueryItem(QStringLiteral("facets"),
                        QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("relevance"));
    url.setQuery(params);

    m_modMgr->setBusy(true);
    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(tr("请求URL: %1").arg(url.toString()));

    const auto onOk = [this](int status, const QByteArray& body) {
        if (status == 200) {
            int totalHits = 0;
            QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
            emit modSearchResultsReady(parseSearchResponseItems(results));
            emit logMessage(tr("搜索完成，共 %1 个结果").arg(totalHits));
        } else {
            emit logMessage(tr("搜索失败: HTTP %1").arg(status));
        }
        m_modMgr->setBusy(false);
    };
    const auto onFail = [this](const QString& error) {
        emit logMessage(tr("网络错误: %1").arg(error));
        m_modMgr->setBusy(false);
    };
    if (m_fetchEngine) {
        // 资源拉取引擎（司南）：并发控制 + 结果缓存（翻页/切 tab 秒开）
        m_fetchEngine->getJson(url.toString(), true, onOk, onFail);
    } else {
        HttpClient::instance().get(url.toString(), onOk, onFail);
    }
}

QVariantMap ResourceBackend::getModCategories()
{
    QVariantMap map;
    map[QStringLiteral("adventure")] = tr("冒险类");
    map[QStringLiteral("cursed")] = tr("猎奇诡异类");
    map[QStringLiteral("decoration")] = tr("装饰类");
    map[QStringLiteral("economy")] = tr("经济系统类");
    map[QStringLiteral("equipment")] = tr("装备武器类");
    map[QStringLiteral("food")] = tr("食物食材类");
    map[QStringLiteral("game-mechanics")] = tr("游戏机制类");
    map[QStringLiteral("library")] = tr("前置依赖库");
    map[QStringLiteral("magic")] = tr("魔法类");
    map[QStringLiteral("management")] = tr("管理辅助类");
    map[QStringLiteral("minigame")] = tr("迷你小游戏类");
    map[QStringLiteral("mobs")] = tr("生物怪物类");
    map[QStringLiteral("optimization")] = tr("性能优化类");
    map[QStringLiteral("social")] = tr("社交交互类");
    map[QStringLiteral("storage")] = tr("仓储存储类");
    map[QStringLiteral("technology")] = tr("科技工业类");
    map[QStringLiteral("transportation")] = tr("交通载具类");
    map[QStringLiteral("utility")] = tr("实用工具类");
    map[QStringLiteral("world-generation")] = tr("世界生成类");
    return map;
}

void ResourceBackend::searchShadersEx(
    const QString& query, const QStringList& gameVersions,
    const QStringList& categories, const QStringList& performance,
    const QStringList& loader, int offset, int limit)
{
    emit logMessage(tr("[SHADER] 搜索光影: q=%1 vers=%2 cats=%3 perf=%4 loader=%5 offset=%6").arg(
        query, gameVersions.join(","), categories.join(","),
        performance.join(","), loader.join(",")).arg(offset));

    // ═══ 串台修复（2026-08-01）：与 searchModsEx 对称，HttpClient 直连 +
    // 硬编码发射 shaderSearchResultsReady，完全绕开 ModManager::searchCompleted
    // 与 m_searchKind 共享标志。原实现走 ModManager 信号 + m_searchKind 路由：
    // 并发搜索时（Mod 页与光影页先后触发），后发请求覆盖 m_searchKind，
    // 导致光影响应被路由到 modSearchResultsReady（Mod 页显示光影）/反之。
    // 直连后两条路径物理隔离，无任何共享状态，互串彻底消除。
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:shader")});
    // 每个过滤条件独立成组（AND 语义，与原 ModManager 路径行为一致）
    for (const QString& c : categories)
        facetArr.append(QJsonArray{QStringLiteral("categories:") + c});
    for (const QString& p : performance)
        facetArr.append(QJsonArray{QStringLiteral("categories:") + p});
    for (const QString& l : loader)
        facetArr.append(QJsonArray{QStringLiteral("categories:") + l});
    if (!gameVersions.isEmpty()) {
        QJsonArray verGroup;
        for (const QString& v : gameVersions)
            verGroup.append(QStringLiteral("versions:") + v);
        facetArr.append(verGroup);
    }

    QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
    QUrlQuery params;
    if (!query.isEmpty())
        params.addQueryItem(QStringLiteral("query"), query);
    params.addQueryItem(QStringLiteral("facets"),
                        QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
    url.setQuery(params);

    m_modMgr->setBusy(true);
    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(tr("请求URL: %1").arg(url.toString()));

    const auto onOk = [this](int status, const QByteArray& body) {
        if (status == 200) {
            int totalHits = 0;
            QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
            emit shaderSearchResultsReady(parseSearchResponseItems(results));
            emit logMessage(tr("光影搜索完成，共 %1 个结果").arg(totalHits));
        } else {
            emit logMessage(tr("光影搜索失败: HTTP %1").arg(status));
        }
        m_modMgr->setBusy(false);
    };
    const auto onFail = [this](const QString& error) {
        emit logMessage(tr("光影搜索网络错误: %1").arg(error));
        m_modMgr->setBusy(false);
    };
    if (m_fetchEngine) {
        // 资源拉取引擎（司南）：并发控制 + 结果缓存（翻页/切 tab 秒开）
        m_fetchEngine->getJson(url.toString(), true, onOk, onFail);
    } else {
        HttpClient::instance().get(url.toString(), onOk, onFail);
    }
}

// ============================================================
// Public Slots — Download
// ============================================================

void ResourceBackend::downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir)
{
    if (m_downloading) {
        emit logMessage(tr("已有下载任务进行中"));
        return;
    }

    m_downloading = true;
    emit downloadStateChanged();

    QString destDir = minecraftDir.isEmpty() ? m_minecraftDir : minecraftDir;
    emit logMessage(tr("正在查找 %1 MC%2 的最新版本...").arg(slug, gameVersion));
    m_modMgr->downloadMod(slug, gameVersion, QStringLiteral("fabric"), destDir,
                          QStringLiteral("mods"));
}

void ResourceBackend::downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir)
{
    if (m_downloading) {
        emit logMessage(tr("已有下载任务进行中"));
        return;
    }

    m_downloading = true;
    emit downloadStateChanged();

    QString destDir = minecraftDir.isEmpty() ? m_minecraftDir : minecraftDir;
    emit logMessage(tr("正在查找 %1 MC%2 的最新版本...").arg(slug, gameVersion));
    m_modMgr->downloadShader(slug, gameVersion, destDir);
}

void ResourceBackend::searchResourcepacks(const QString& query, const QString& gameVersion, int offset, const QStringList& categories)
{
    emit logMessage(tr("[MODRINTH] 搜索资源包: q=%1 gv=%2 offset=%3 cats=%4").arg(query, gameVersion).arg(offset).arg(categories.join(QChar(','))));
    m_modMgr->searchResourcepacks(query, gameVersion.isEmpty()
                                  ? QStringList{} : QStringList{gameVersion},
                                  categories, offset);
}

void ResourceBackend::downloadResourcepack(const QString& slug, const QString& gameVersion, const QString& minecraftDir)
{
    if (m_downloading) {
        emit logMessage(tr("[警告]️ 已有下载任务进行中"));
        return;
    }

    m_downloading = true;
    emit downloadStateChanged();

    emit logMessage(tr("[搜索] 正在查找资源包 %1 ...").arg(slug));
    QString destDir = minecraftDir.isEmpty() ? m_minecraftDir : minecraftDir;
    m_modMgr->downloadResourcepack(slug, gameVersion, destDir);
}

void ResourceBackend::fetchResourcepackVersions(const QStringList& slugs)
{
    m_modMgr->fetchResourcepackVersions(slugs);
}

void ResourceBackend::fetchModVersions(const QStringList& slugs)
{
    m_modMgr->fetchModVersions(slugs);
}

void ResourceBackend::fetchShaderVersions(const QStringList& slugs)
{
    m_modMgr->fetchShaderVersions(slugs);
}

void ResourceBackend::cancelDownload()
{
    m_modMgr->cancel();
}

// ============================================================
// Private Slots — ModManager signal forwarding
// ============================================================

void ResourceBackend::onSearchCompleted(const QJsonArray& results, int /*totalHits*/)
{
    // 兼容路径：仅 searchMods（非 Ex，QML 已不使用）会经 ModManager 触发本槽。
    // Mod/Shader 实际搜索均已改为 HttpClient 直连 + 硬编码发射（searchModsEx /
    // searchShadersEx），m_searchKind 不再有并发写入来源，此处路由保留作兜底。
    emit logMessage(tr("找到 %1 个结果").arg(results.size()));
    if (m_searchKind == SearchKind::Mod)
        emit modSearchResultsReady(parseSearchResponseItems(results));
    else
        emit shaderSearchResultsReady(parseSearchResponseItems(results));
}

QVariantList ResourceBackend::parseSearchResponseItems(const QJsonArray& results) const
{
    // Mod/Shader 搜索结果统一解析（字段与 QML 端约定一致）：
    // - versions ← gameVersions（parseSearchResponse 将原始 versions 映射到此字段）
    // - loadersList ← loaders（原始 categories，parseSearchResponse 已把加载器从
    //   categories 剔除进 loaders 字段，用 catList 筛选永远为空）
    QVariantList list;
    static const QStringList knownLoaders = {QLatin1String("fabric"), QLatin1String("forge"),
                                             QLatin1String("quilt"), QLatin1String("neoforge"),
                                             QLatin1String("rift"), QLatin1String("liteloader"),
                                             QLatin1String("iris"), QLatin1String("optifine")};
    for (const QJsonValue& item : results) {
        const QJsonObject obj = item.toObject();
        QVariantMap entry;
        entry[QStringLiteral("slug")]      = obj[QStringLiteral("slug")].toString();
        entry[QStringLiteral("title")]     = obj[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]      = obj[QStringLiteral("description")].toString();
        entry[QStringLiteral("icon")]      = obj[QStringLiteral("iconUrl")].toString();
        entry[QStringLiteral("downloads")] = obj[QStringLiteral("downloads")].toInt();
        const QJsonArray cats = obj[QStringLiteral("categories")].toArray();
        QStringList catList, typeList;
        QString loader;
        for (const QJsonValue& cv : cats) {
            const QString c = cv.toString();
            catList << c;
            if (knownLoaders.contains(c)) {
                if (loader.isEmpty()) loader = c;
            } else {
                typeList << c;
            }
        }
        entry[QStringLiteral("loader")]     = loader;
        entry[QStringLiteral("categories")] = QVariant(catList);
        entry[QStringLiteral("typeList")]   = QVariant(typeList);
        entry[QStringLiteral("clientSide")] = obj[QStringLiteral("client_side")].toString();
        QStringList allLoaders;
        const QJsonArray rawCats = obj[QStringLiteral("loaders")].toArray();
        for (const QJsonValue& cv : rawCats) {
            const QString c = cv.toString();
            if (knownLoaders.contains(c)) allLoaders.append(c);
        }
        entry[QStringLiteral("loadersList")] = allLoaders;
        entry[QStringLiteral("versions")]     = obj[QStringLiteral("gameVersions")].toVariant();
        entry[QStringLiteral("dateModified")]= obj[QStringLiteral("updated")].toString();
        entry[QStringLiteral("license")]    = obj[QStringLiteral("license")].toVariant();
        list.append(entry);
    }
    return list;
}

void ResourceBackend::onDownloadProgress(const QString& name, qint64 received, qint64 total)
{
    // ── Speed tracking (instantaneous delta / elapsed) ──
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 delta = received - m_dlLastBytes;
    qint64 dt = m_dlLastMs > 0 ? (nowMs - m_dlLastMs) : 0;
    if (delta > 0 && dt > 0) {
        m_dlSpeed = static_cast<int>((delta * 1000) / dt);
    } else if (delta > 0 && dt == 0) {
        // First pulse: record timestamp, don't compute speed yet
    } else if (dt > 30000) {
        m_dlSpeed = 0;
    }
    m_dlLastBytes = received;
    m_dlLastMs = nowMs;

    m_dlProgress = static_cast<int>(received);
    m_dlTotal    = static_cast<int>(total);
    m_dlFile     = name;
    emit downloadProgressChanged(m_dlProgress, m_dlTotal, m_dlFile);
}

void ResourceBackend::onDownloadFinished(const QString& slug, bool success,
                                         const QString& filePath)
{
    m_downloading = false;
    m_dlProgress  = 0;
    m_dlTotal     = 0;
    m_dlSpeed     = 0;
    m_dlLastBytes = 0;
    m_dlLastMs    = 0;
    m_dlFile.clear();
    emit downloadStateChanged();

    if (success) {
        emit logMessage(tr("[完成] %1 下载完成").arg(slug));
    } else {
        emit logMessage(tr("[失败] %1 下载失败，请检查版本兼容性").arg(slug));
    }

    emit downloadFinished(slug, success, filePath);
}

void ResourceBackend::onResourcepackSearchCompleted(const QJsonArray& results, int totalHits)
{
    QVariantList list;
    for (const QJsonValue& item : results) {
        const QJsonObject obj = item.toObject();
        QVariantMap entry;
        entry[QStringLiteral("slug")]      = obj[QStringLiteral("slug")].toString();
        entry[QStringLiteral("title")]     = obj[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]      = obj[QStringLiteral("description")].toString();
        entry[QStringLiteral("icon")]      = obj[QStringLiteral("iconUrl")].toString();
        entry[QStringLiteral("downloads")] = obj[QStringLiteral("downloads")].toInt();
        entry[QStringLiteral("author")]    = obj[QStringLiteral("author")].toString();
        entry[QStringLiteral("updated")]   = obj[QStringLiteral("updated")].toString();
        entry[QStringLiteral("source")]    = QStringLiteral("Modrinth");
        // Categories as QVariant string list
        QJsonArray cats = obj[QStringLiteral("categories")].toArray();
        QStringList catList;
        for (const QJsonValue& catVal : cats) {
            catList.append(catVal.toString());
        }
        entry[QStringLiteral("categories")] = catList;

        // Features (split by parseSearchResponse)
        QJsonArray feats = obj[QStringLiteral("features")].toArray();
        QStringList featList;
        for (const QJsonValue& fv : feats) {
            featList.append(fv.toString());
        }
        entry[QStringLiteral("features")] = featList;

        // Resolutions (split by parseSearchResponse)
        QJsonArray resos = obj[QStringLiteral("resolutions")].toArray();
        QStringList resList;
        for (const QJsonValue& rv : resos) {
            resList.append(rv.toString());
        }
        entry[QStringLiteral("resolutions")] = resList;
        list.append(entry);
    }
    emit logMessage(tr("找到 %1 个资源包").arg(list.size()));
    emit resourcepackSearchCompleted(list, totalHits);
}

void ResourceBackend::onResourcepackDownloadFinished(const QString& slug, bool success,
                                                      const QString& filePath)
{
    m_downloading = false;
    m_dlProgress  = 0;
    m_dlTotal     = 0;
    m_dlFile.clear();
    emit downloadStateChanged();
    emit resourcepackDownloadFinished(slug, success, filePath);
}

void ResourceBackend::onResourcepackVersionsLoaded(const QVariantMap& slugToVersions)
{
    emit resourcepackVersionsLoaded(slugToVersions);
}

void ResourceBackend::onModVersionsLoaded(const QVariantMap& slugToVersions)
{
    emit modVersionsLoaded(slugToVersions);
}

void ResourceBackend::onShaderVersionsLoaded(const QVariantMap& slugToVersions)
{
    emit shaderVersionsLoaded(slugToVersions);
}

// ============================================================
// Mod file download — proxy + forwarding
// ============================================================

int ResourceBackend::downloadModFile(const QString& url, const QString& savePath,
                                      const QString& displayName, qint64 expectedSize,
                                      const QString& sha1, qint64 receivedOffset, int resumeId)
{
    return m_modMgr->downloadModFile(url, savePath, displayName, expectedSize, sha1, receivedOffset, resumeId);
}

void ResourceBackend::cancelModFileDownload(int downloadId)
{
    m_modMgr->cancelModFileDownload(downloadId);
}

void ResourceBackend::pauseModFileDownload(int downloadId)
{
    m_modMgr->pauseModFileDownload(downloadId);
}

void ResourceBackend::resumeModFileDownload(int downloadId)
{
    m_modMgr->resumeModFileDownload(downloadId);
}

void ResourceBackend::retryModFileDownload(int downloadId)
{
    m_modMgr->retryModFileDownload(downloadId);
}

void ResourceBackend::onModFileDownloadStarted(int downloadId, const QString& fileName,
                                                qint64 fileSize, const QString& displayName)
{
    emit modFileDownloadStarted(downloadId, fileName, fileSize, displayName);
}

void ResourceBackend::onModFileDownloadProgress(int downloadId, qint64 received, qint64 total, qint64 speed)
{
    emit modFileDownloadProgress(downloadId, received, total, speed);
}

void ResourceBackend::onModFileDownloadFinished(int downloadId, bool success,
                                                 const QString& filePath, const QString& displayName)
{
    emit modFileDownloadFinished(downloadId, success, filePath, displayName);
}

void ResourceBackend::onModFileDownloadFailed(int downloadId, const QString& errorDetail,
                                               const QString& displayName)
{
    emit modFileDownloadFailed(downloadId, errorDetail, displayName);
}

} // namespace ShadowLauncher
