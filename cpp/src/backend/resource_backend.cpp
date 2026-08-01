// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "resource_backend.h"
#include "../core/resource_fetch_engine.h"
#include "../core/cf_api.h"
#include <QTimer>
#include <QRegularExpression>
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
    if (e && !m_cfApi) m_cfApi = new CfApi(e, this); // CurseForge 源适配器
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

    // ── 双源聚合：Modrinth + CurseForge ──
    // 分类值 "cf:xxx" → 仅 CF；Modrinth 分类 → 仅 Modrinth；空 → 双源
    const bool cfOnly = CfApi::isCfCategory(category);
    const bool mrOnly = !cfOnly && !category.isEmpty();
    const int cfCatId = CfApi::cfCategoryId(category);
    const int gen = ++m_searchGen;
    m_modMrResults.clear();
    m_modCfResults.clear();
    m_modPending = 0;
    m_modMgr->setBusy(true);

    if (!cfOnly) {
        // Modrinth（facet 已按原分类构造；cf: 分类时跳过）
        if (!ShadowLauncher::suppressUrlLog())
            emit logMessage(tr("请求URL: %1").arg(url.toString()));
        ++m_modPending;
        const auto onMrOk = [this, gen](int status, const QByteArray& body) {
            if (gen != m_searchGen) return;
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
                m_modMrResults = parseSearchResponseItems(results);
            } else {
                emit logMessage(tr("Modrinth 搜索失败: HTTP %1").arg(status));
            }
            tryAggregateMod(gen, true);
        };
        const auto onMrFail = [this, gen](const QString& error) {
            if (gen != m_searchGen) return;
            emit logMessage(tr("Modrinth 网络错误: %1").arg(error));
            tryAggregateMod(gen, true);
        };
        if (m_fetchEngine)
            m_fetchEngine->getJson(url.toString(), true, onMrOk, onMrFail);
        else
            HttpClient::instance().get(url.toString(), onMrOk, onMrFail);
    }

    if (m_cfApi && (!mrOnly || cfOnly)) {
        // CurseForge（cfOnly 或双源）
        ++m_modPending;
        m_cfApi->search(6, query, cfCatId,
                        gameVersions.isEmpty() ? QString() : gameVersions.first(),
                        loader, offset, limit,
            [this, gen](const QVariantList& items, int) {
                if (gen != m_searchGen) return;
                m_modCfResults = items;
                tryAggregateMod(gen, false);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                emit logMessage(tr("CurseForge 搜索失败: %1").arg(err));
                tryAggregateMod(gen, false);
            });
    }

    // 超时兜底：任一源 8s 未回 → 强制聚合（已回部分）
    QTimer::singleShot(8000, this, [this, gen]() {
        if (gen != m_searchGen) return;
        if (m_modPending > 0) {
            m_modPending = 0;
            m_modTimeoutForced = true;
            tryAggregateMod(gen, false);
        }
    });
}

// ── 双源去重合并：Modrinth 优先（同名模组 CF 不展示），按下载量降序混排 ──
static QString normTitle(const QString& s)
{
    QString n = s.toLower();
    n.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    return n;
}
static QVariantList mergeDedupSorted(const QVariantList& mrItems, const QVariantList& cfItems)
{
    QSet<QString> seen;
    QVariantList merged;
    auto appendDedup = [&](const QVariantList& items) {
        for (const QVariant& v : items) {
            const QVariantMap m = v.toMap();
            const QString key = normTitle(m.value(QStringLiteral("title")).toString());
            if (key.isEmpty()) { merged.append(v); continue; }
            if (seen.contains(key)) continue;
            seen.insert(key);
            merged.append(v);
        }
    };
    appendDedup(mrItems);  // Modrinth 先（优先保留）
    appendDedup(cfItems);  // CF 只补充特有
    std::sort(merged.begin(), merged.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value(QStringLiteral("downloads")).toDouble()
             > b.toMap().value(QStringLiteral("downloads")).toDouble();
    });
    return merged;
}

// CF 特有项：Modrinth 结果里没有的（归一化标题匹配）——增量插入用
static QVariantList cfOnlyItems(const QVariantList& mrItems, const QVariantList& cfItems)
{
    QSet<QString> mrKeys;
    for (const QVariant& v : mrItems)
        mrKeys.insert(normTitle(v.toMap().value(QStringLiteral("title")).toString()));
    QVariantList out;
    for (const QVariant& v : cfItems) {
        const QString key = normTitle(v.toMap().value(QStringLiteral("title")).toString());
        if (key.isEmpty() || !mrKeys.contains(key))
            out.append(v);
    }
    return out;
}

void ResourceBackend::tryAggregateMod(int gen, bool mrDone)
{
    if (gen != m_searchGen) return;
    --m_modPending;
    if (mrDone && m_modPending > 0) {
        // 渐进第一波：Modrinth 先回 → 先显示（不等 CF）
        m_modMgr->setBusy(false);
        emit modSearchResultsReady(m_modMrResults);
        return;
    }
    if (m_modPending > 0) return;
    m_modMgr->setBusy(false);
    if (m_modTimeoutForced) {
        m_modTimeoutForced = false;
        // 超时兜底：发合并全量（清空重填）
        const QVariantList merged = mergeDedupSorted(m_modMrResults, m_modCfResults);
        emit logMessage(tr("搜索完成(超时兜底): %1 条").arg(merged.size()));
        emit modSearchResultsReady(merged);
        return;
    }
    if (!m_modMrResults.isEmpty()) {
        // 正常双源：CF 特有项增量插入（QML 动画插入，不清空）
        const QVariantList cfOnly = cfOnlyItems(m_modMrResults, m_modCfResults);
        emit logMessage(tr("CF 特有项 %1 条增量插入").arg(cfOnly.size()));
        emit modCfInserted(cfOnly);
    } else {
        // Modrinth 失败但 CF 有结果 → 直接显示 CF
        emit modSearchResultsReady(m_modCfResults);
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

    // ── 双源聚合：Modrinth + CurseForge（光影 classId=6552，加载器/版本过滤）──
    const int gen = ++m_searchGen;
    m_shaderMrResults.clear();
    m_shaderCfResults.clear();
    m_shaderPending = 0;
    m_modMgr->setBusy(true);

    if (!ShadowLauncher::suppressUrlLog())
        emit logMessage(tr("请求URL: %1").arg(url.toString()));
    ++m_shaderPending;
    const auto onMrOk = [this, gen](int status, const QByteArray& body) {
        if (gen != m_searchGen) return;
        if (status == 200) {
            int totalHits = 0;
            QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
            m_shaderMrResults = parseSearchResponseItems(results);
        } else {
            emit logMessage(tr("光影搜索失败: HTTP %1").arg(status));
        }
        tryAggregateShader(gen, true);
    };
    const auto onMrFail = [this, gen](const QString& error) {
        if (gen != m_searchGen) return;
        emit logMessage(tr("光影搜索网络错误: %1").arg(error));
        tryAggregateShader(gen, true);
    };
    if (m_fetchEngine)
        m_fetchEngine->getJson(url.toString(), true, onMrOk, onMrFail);
    else
        HttpClient::instance().get(url.toString(), onMrOk, onMrFail);

    if (m_cfApi) {
        ++m_shaderPending;
        m_cfApi->search(6552, query, 0,
                        gameVersions.isEmpty() ? QString() : gameVersions.first(),
                        loader.isEmpty() ? QString() : loader.first(), offset, limit,
            [this, gen](const QVariantList& items, int) {
                if (gen != m_searchGen) return;
                m_shaderCfResults = items;
                tryAggregateShader(gen, false);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                emit logMessage(tr("CurseForge 光影搜索失败: %1").arg(err));
                tryAggregateShader(gen, false);
            });
    }

    QTimer::singleShot(8000, this, [this, gen]() {
        if (gen != m_searchGen) return;
        if (m_shaderPending > 0) {
            m_shaderPending = 0;
            m_shaderTimeoutForced = true;
            tryAggregateShader(gen, false);
        }
    });
}

void ResourceBackend::tryAggregateShader(int gen, bool mrDone)
{
    if (gen != m_searchGen) return;
    --m_shaderPending;
    if (mrDone && m_shaderPending > 0) {
        m_modMgr->setBusy(false);
        emit shaderSearchResultsReady(m_shaderMrResults);
        return;
    }
    if (m_shaderPending > 0) return;
    m_modMgr->setBusy(false);
    if (m_shaderTimeoutForced) {
        m_shaderTimeoutForced = false;
        const QVariantList merged = mergeDedupSorted(m_shaderMrResults, m_shaderCfResults);
        emit logMessage(tr("光影搜索完成(超时兜底): %1 条").arg(merged.size()));
        emit shaderSearchResultsReady(merged);
        return;
    }
    if (!m_shaderMrResults.isEmpty()) {
        const QVariantList cfOnly = cfOnlyItems(m_shaderMrResults, m_shaderCfResults);
        emit logMessage(tr("CF 光影特有项 %1 条增量插入").arg(cfOnly.size()));
        emit shaderCfInserted(cfOnly);
    } else {
        emit shaderSearchResultsReady(m_shaderCfResults);
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
    emit logMessage(tr("[RP] 搜索资源包: q=%1 gv=%2 offset=%3 cats=%4").arg(query, gameVersion).arg(offset).arg(categories.join(QChar(','))));

    // ── 双源聚合：Modrinth 直连 + CurseForge（classId=12）──
    const int gen = ++m_searchGen;
    m_rpMrResults.clear();
    m_rpCfResults.clear();
    m_rpPending = 0;
    m_modMgr->setBusy(true);

    // Modrinth：直连 mcimirror（facet: project_type=resourcepack）
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:resourcepack")});
    if (!gameVersion.isEmpty())
        facetArr.append(QJsonArray{QStringLiteral("versions:") + gameVersion});
    for (const QString& c : categories)
        facetArr.append(QJsonArray{QStringLiteral("categories:") + c});
    QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
    QUrlQuery params;
    if (!query.isEmpty())
        params.addQueryItem(QStringLiteral("query"), query);
    params.addQueryItem(QStringLiteral("facets"), QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"), QString::number(20));
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
    url.setQuery(params);

    ++m_rpPending;
    const auto onRpOk = [this, gen](int status, const QByteArray& body) {
        if (gen != m_searchGen) return;
        if (status == 200) {
            int total = 0;
            QJsonArray hits = m_modMgr->parseSearchResponse(body, total);
            // 与 onResourcepackSearchCompleted 相同的字段转换
            QVariantList list;
            for (const QJsonValue& item : hits) {
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
                QStringList catList, featList, resList;
                for (const QJsonValue& cv : obj[QStringLiteral("categories")].toArray())
                    catList.append(cv.toString());
                for (const QJsonValue& fv : obj[QStringLiteral("features")].toArray())
                    featList.append(fv.toString());
                for (const QJsonValue& rv : obj[QStringLiteral("resolutions")].toArray())
                    resList.append(rv.toString());
                entry[QStringLiteral("categories")]  = catList;
                entry[QStringLiteral("features")]    = featList;
                entry[QStringLiteral("resolutions")] = resList;
                list.append(entry);
            }
            m_rpMrResults = list;
        } else {
            emit logMessage(tr("[RP] Modrinth 搜索失败: HTTP %1").arg(status));
        }
        tryAggregateRp(gen, true);
    };
    const auto onRpFail = [this, gen](const QString& error) {
        if (gen != m_searchGen) return;
        emit logMessage(tr("[RP] Modrinth 网络错误: %1").arg(error));
        tryAggregateRp(gen, true);
    };
    if (m_fetchEngine)
        m_fetchEngine->getJson(url.toString(), true, onRpOk, onRpFail);
    else
        HttpClient::instance().get(url.toString(), onRpOk, onRpFail);

    if (m_cfApi) {
        ++m_rpPending;
        m_cfApi->search(12, query, 0, gameVersion, QString(), offset, 20,
            [this, gen](const QVariantList& items, int) {
                if (gen != m_searchGen) return;
                m_rpCfResults = items;
                tryAggregateRp(gen, false);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                emit logMessage(tr("[RP] CurseForge 搜索失败: %1").arg(err));
                tryAggregateRp(gen, false);
            });
    }

    QTimer::singleShot(8000, this, [this, gen]() {
        if (gen != m_searchGen) return;
        if (m_rpPending > 0) {
            m_rpPending = 0;
            m_rpTimeoutForced = true;
            tryAggregateRp(gen, false);
        }
    });
}

void ResourceBackend::tryAggregateRp(int gen, bool mrDone)
{
    if (gen != m_searchGen) return;
    --m_rpPending;
    if (mrDone && m_rpPending > 0) {
        m_modMgr->setBusy(false);
        emit resourcepackSearchCompleted(m_rpMrResults, m_rpMrResults.size());
        return;
    }
    if (m_rpPending > 0) return;
    m_modMgr->setBusy(false);
    if (m_rpTimeoutForced) {
        m_rpTimeoutForced = false;
        const QVariantList merged = mergeDedupSorted(m_rpMrResults, m_rpCfResults);
        emit logMessage(tr("[RP] 搜索完成(超时兜底): %1 条").arg(merged.size()));
        emit resourcepackSearchCompleted(merged, merged.size());
        return;
    }
    if (!m_rpMrResults.isEmpty()) {
        const QVariantList cfOnly = cfOnlyItems(m_rpMrResults, m_rpCfResults);
        emit logMessage(tr("[RP] CF 特有项 %1 条增量插入").arg(cfOnly.size()));
        emit rpCfInserted(cfOnly);
    } else {
        emit resourcepackSearchCompleted(m_rpCfResults, m_rpCfResults.size());
    }
}

// ── CurseForge 详情页版本（复用 modVersionsPartial 等信号）──
void ResourceBackend::fetchModVersionsCf(const QString& modId, const QString& gameVersion, const QString& loader)
{
    if (!m_cfApi) { emit modVersionsPartial(modId, {}, {}); return; }
    m_cfApi->fetchFilesAsVersions(modId, gameVersion, loader,
        [this, modId](const QStringList& versions, const QVariantMap& details) {
            emit modVersionsPartial(modId, versions, details);
        },
        [this, modId](const QString&) { emit modVersionsPartial(modId, {}, {}); });
}

void ResourceBackend::fetchShaderVersionsCf(const QString& modId, const QString& gameVersion, const QString& loader)
{
    if (!m_cfApi) { emit shaderVersionsPartial(modId, {}, {}); return; }
    m_cfApi->fetchFilesAsVersions(modId, gameVersion, loader,
        [this, modId](const QStringList& versions, const QVariantMap& details) {
            emit shaderVersionsPartial(modId, versions, details);
        },
        [this, modId](const QString&) { emit shaderVersionsPartial(modId, {}, {}); });
}

void ResourceBackend::fetchResourcepackVersionsCf(const QString& modId, const QString& gameVersion, const QString& loader)
{
    if (!m_cfApi) { emit resourcepackVersionsPartial(modId, {}, {}); return; }
    m_cfApi->fetchFilesAsVersions(modId, gameVersion, loader,
        [this, modId](const QStringList& versions, const QVariantMap& details) {
            emit resourcepackVersionsPartial(modId, versions, details);
        },
        [this, modId](const QString&) { emit resourcepackVersionsPartial(modId, {}, {}); });
}

QVariantList ResourceBackend::cfCategories(int classId) const
{
    return CfApi::categories(classId);
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
