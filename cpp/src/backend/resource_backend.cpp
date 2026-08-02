// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "resource_backend.h"
#include "../core/resource_fetch_engine.h"
#include "../core/cf_api.h"
#include <QTimer>
#include <QRegularExpression>
#include <QSet>
#include "core/mod_manager.h"
#include "core/http_client.h"
#include "utils/logger.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>
#include <memory>

namespace ShadowLauncher {

// ── 双源去重合并：Modrinth 优先（同名模组 CF 不展示），按加权下载量降序混排 ──
// 加权依据（实测 2026-08-02）：Modrinth 头部下载量约为 CF 的 1/2~1/3（Fabric API 2.2亿 vs JEI 6亿），
// Modrinth×2.5 可达到前 10 约 5/5 均衡混合（×1.5 CF 霸榜、×3 Modrinth 反霸、×5 完全淹没 CF）
static QString normTitle(const QString& s)
{
    QString n = s.toLower();
    n.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
    return n;
}
static QVariantList mergeDedupSorted(const QVariantList& mrItems, const QVariantList& cfItems, double mrMult)
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
    std::sort(merged.begin(), merged.end(), [mrMult](const QVariant& a, const QVariant& b) {
        auto w = [mrMult](const QVariant& v) {
            const QVariantMap m = v.toMap();
            const double d = m.value(QStringLiteral("downloads")).toDouble();
            return m.value(QStringLiteral("source")).toString() == QStringLiteral("CurseForge") ? d : d * mrMult;
        };
        return w(a) > w(b);
    });
    return merged;
}

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
    int page, int limit)
{
    // ═══ 池子架构（对齐 主流启动器）：双源结果累积成池，翻页时续拉合并 ═══
    // 页与页之间保持全局加权排序连续性：池 = 已拉双源的去重+加权排序全量。
    m_searchKind = SearchKind::Mod;
    const QString key = query + QLatin1Char('|') + loader + QLatin1Char('|') + category
                      + QLatin1Char('|') + gameVersions.join(QLatin1Char(',')) + QLatin1Char('|')
                      + environment + QLatin1Char('|') + license;
    m_modSearchPage = limit > 0 ? page / limit : 0;   // QML 传 offset，换算成页号
    m_modSearchLimit = limit;

    const bool cfOnly = CfApi::isCfCategory(category);
    // 环境/许可证为 Modrinth 独占筛选（CF 无对应查询参数）：
    // 激活时一律 Modrinth-only，杜绝 CF 结果绕过筛选混入；
    // 若同时选了 CF 分类，也放弃 CF 侧（分类+独占筛选组合无有效实现）。
    const bool filterMrOnly = !environment.isEmpty() || !license.isEmpty();
    const bool cfOnlyEffective = cfOnly && !filterMrOnly;
    const bool mrOnly = filterMrOnly || (!cfOnly && !category.isEmpty());
    m_modSearchCfOnly = cfOnlyEffective;
    m_modSearchMrOnly = mrOnly;
    m_modSearchQuery = query;
    m_modSearchLoader = loader;
    m_modSearchCategory = category;
    m_modSearchEnv = environment;
    m_modSearchLic = license;
    m_modSearchVersions = gameVersions;

    // 搜索词/条件变化 → 重置池子与游标
    if (key != m_modSearchKey) {
        ++m_searchGen;   // 新搜索代次：旧响应全部作废
        m_modSearchKey = key;
        m_modPool.clear();
        m_modMrAll.clear();
        m_modCfAll.clear();
        m_modMrOffset = 0;
        m_modCfOffset = 0;
        m_modMrMore = true;
        m_modCfMore = true;
        m_mrFallbackUsed = false;
        m_modShownCount = 0;
        emit logMessage(tr("[池子] Mod 搜索重置: q=%1 offset=%2 limit=%3").arg(query).arg(page * limit).arg(limit));
    }

    // 确保池子足够显示目标页（池不够 → 双源续拉；池够 → 直接发）
    ensureModPool(m_modSearchCfOnly, m_modSearchMrOnly, query, loader, category, gameVersions, environment, license);
}

// 池子驱动：Mod 双源续拉 + 合并 + 发信号
void ResourceBackend::ensureModPool(bool cfOnly, bool mrOnly,
    const QString& query, const QString& loader, const QString& category,
    const QStringList& gameVersions, const QString& environment, const QString& license)
{
    const int need = (m_modSearchPage + 1) * m_modSearchLimit;
    if (m_modPending > 0) return;  // 上一轮还没回完，等回调

    // 池已够或双源都耗尽 → 直接发当前池
    if (m_modPool.size() >= need || (!m_modMrMore && !m_modCfMore)) {
        emitModPool();
        return;
    }

    // 双源各拉一页（从各自游标继续）
    const int gen = m_searchGen;
    QStringList loaders;
    if (!loader.isEmpty()) loaders << loader;
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:mod")});
    // 仅 Modrinth 分类进 facet；CF 分类（cf:xxx）是 CF 专属值，Modrinth 侧一律丢弃
    if (!category.isEmpty() && !CfApi::isCfCategory(category))
        facetArr.append(QJsonArray{QStringLiteral("categories:") + category});
    if (!gameVersions.isEmpty()) {
        QJsonArray verGroup;
        for (const QString& v : gameVersions)
            verGroup.append(QStringLiteral("versions:") + v);
        facetArr.append(verGroup);
    }
    if (!environment.isEmpty()) {
        if (environment == QStringLiteral("client"))
            facetArr.append(QJsonArray{QStringLiteral("client_side:required")});
        else if (environment == QStringLiteral("server"))
            facetArr.append(QJsonArray{QStringLiteral("server_side:required")});
    }
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
    if (!loaders.isEmpty()) {
        QJsonArray ldGroup;
        for (const QString& l : loaders)
            ldGroup.append(QStringLiteral("categories:") + l);
        facetArr.append(ldGroup);
    }

    m_modPending = 0;
    m_modMgr->setBusy(true);

    // ── Modrinth（游标续拉）──
    if (!cfOnly && m_modMrMore) {
        QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
        QUrlQuery params;
        if (!query.isEmpty())
            params.addQueryItem(QStringLiteral("query"), query);
        params.addQueryItem(QStringLiteral("facets"),
                            QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
        params.addQueryItem(QStringLiteral("offset"), QString::number(m_modMrOffset));
        params.addQueryItem(QStringLiteral("limit"), QString::number(m_modSearchLimit));
        params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
        url.setQuery(params);
        ++m_modPending;
        const auto onMrOk = [this, gen, url](int status, const QByteArray& body) {
            if (gen != m_searchGen) return;
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
                // 镜像异常空 → 降级官方
                if (totalHits == 0 && results.isEmpty() && !m_mrFallbackUsed) {
                    m_mrFallbackUsed = true;
                    emit logMessage(tr("Modrinth 镜像返回空(异常)，降级官方 API"));
                    QUrl fbUrl = url;
                    fbUrl.setHost(QStringLiteral("api.modrinth.com"));
                    fbUrl.setPath(QStringLiteral("/v2/search"));
                    fbUrl.setScheme(QStringLiteral("https"));
                    const auto onFbOk = [this, gen](int s2, const QByteArray& b2) {
                        if (gen != m_searchGen) return;
                        if (s2 == 200) {
                            int t2 = 0;
                            QJsonArray r2 = m_modMgr->parseSearchResponse(b2, t2);
                            m_modMrAll.append(parseSearchResponseItems(r2));
                            m_modMrOffset += r2.size();
                            m_modMrMore = m_modMrOffset < t2;
                            emit logMessage(tr("官方降级成功: +%1 条").arg(r2.size()));
                        } else {
                            m_modMrMore = false;
                            emit logMessage(tr("Modrinth 官方降级失败: HTTP %1").arg(s2));
                        }
                        onModSourceDone(gen);
                    };
                    const auto onFbFail = [this, gen](const QString& e2) {
                        if (gen != m_searchGen) return;
                        m_modMrMore = false;
                        emit logMessage(tr("Modrinth 官方降级网络错误: %1").arg(e2));
                        onModSourceDone(gen);
                    };
                    if (m_fetchEngine)
                        m_fetchEngine->getJson(fbUrl.toString(), true, onFbOk, onFbFail);
                    else
                        HttpClient::instance().get(fbUrl.toString(), onFbOk, onFbFail);
                    return;  // 等待降级回调 onModSourceDone
                }
                m_modMrAll.append(parseSearchResponseItems(results));
                m_modMrOffset += results.size();
                m_modMrMore = m_modMrOffset < totalHits;
            } else {
                m_modMrMore = false;
                emit logMessage(tr("Modrinth 搜索失败: HTTP %1").arg(status));
            }
            onModSourceDone(gen);
        };
        const auto onMrFail = [this, gen](const QString& error) {
            if (gen != m_searchGen) return;
            m_modMrMore = false;
            emit logMessage(tr("Modrinth 网络错误: %1").arg(error));
            onModSourceDone(gen);
        };
        if (m_fetchEngine)
            m_fetchEngine->getJson(url.toString(), true, onMrOk, onMrFail);
        else
            HttpClient::instance().get(url.toString(), onMrOk, onMrFail);
    }

    // ── CurseForge（游标续拉）──
    // mrOnly 时 CF 侧不参与（CF 无环境/许可证参数，且独占筛选下不应混入未过滤结果）
    if (m_cfApi && !mrOnly && m_modCfMore) {
        ++m_modPending;
        m_cfApi->search(6, query, CfApi::cfCategoryId(category),
                        gameVersions.isEmpty() ? QString() : gameVersions.first(),
                        loader, m_modCfOffset, m_modSearchLimit,
            [this, gen](const QVariantList& items, int total) {
                if (gen != m_searchGen) return;
                m_modCfAll.append(items);
                m_modCfOffset += items.size();
                m_modCfMore = m_modCfOffset < total;
                onModSourceDone(gen);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                m_modCfMore = false;
                emit logMessage(tr("CurseForge 搜索失败: %1").arg(err));
                onModSourceDone(gen);
            });
    }

    // 单源模式（cfOnly 只有 CF；mrOnly 只有 Modrinth）时确保有请求
    if (m_modPending == 0) {
        // 无可发请求（该源已耗尽或不可用）→ 直接发当前池
        emitModPool();
    } else {
        // 超时兜底：8s 未回 → 发当前池（防无限等待）
        QTimer::singleShot(8000, this, [this, gen]() {
            if (gen != m_searchGen) return;
            if (m_modPending > 0) {
                m_modPending = 0;
                emitModPool();
            }
        });
    }
}

// 单源回调汇聚：一轮源都回后，合并入池、检查是否需要继续拉、发信号
void ResourceBackend::onModSourceDone(int gen)
{
    if (gen != m_searchGen) return;
    if (--m_modPending > 0) return;  // 等两个源都回
    m_modMgr->setBusy(false);

    // ═══ 冻结区 + 候选区：已显示的前 shownCount 条永不重排 ═══
    // 已显示部分（QML 已渲染的页）保持原序 → 滚动/切页不闪动；
    // 只有未显示的候选区会因新数据重排（用户看不到变化，无感知）。
    QVariantList frozen = m_modPool.mid(0, m_modShownCount);
    // 从双源原始数据中排除已显示项（按归一化标题去重）
    QSet<QString> frozenKeys;
    for (const QVariant& v : frozen)
        frozenKeys.insert(normTitle(v.toMap().value(QStringLiteral("title")).toString()));
    QVariantList mrRest, cfRest;
    for (const QVariant& v : m_modMrAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            mrRest.append(v);
    }
    for (const QVariant& v : m_modCfAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            cfRest.append(v);
    }
    const QVariantList candidate = mergeDedupSorted(mrRest, cfRest, 2.5);
    m_modPool = frozen + candidate;
    emit logMessage(tr("[池子] Mod 合并: 冻结 %1 + 候选 %2 → 池 %3 条")
                        .arg(frozen.size()).arg(candidate.size()).arg(m_modPool.size()));
    // 检查是否还需继续拉（池不够目标页 且 源未耗尽）
    const int need = (m_modSearchPage + 1) * m_modSearchLimit;
    if (m_modPool.size() < need && (m_modMrMore || m_modCfMore)) {
        ensureModPool(m_modSearchCfOnly, m_modSearchMrOnly,
                      m_modSearchQuery, m_modSearchLoader, m_modSearchCategory,
                      m_modSearchVersions, m_modSearchEnv, m_modSearchLic);
        return;
    }
    emitModPool();
}

void ResourceBackend::emitModPool()
{
    m_modMgr->setBusy(false);
    // 推进冻结边界：当前请求页已展示 → 前 (page+1)*limit 条冻结
    const int shown = (m_modSearchPage + 1) * m_modSearchLimit;
    if (shown > m_modShownCount)
        m_modShownCount = qMin(shown, m_modPool.size());
    emit modSearchResultsReady(m_modPool);
}


// ── 翻页预取：只预热司南引擎缓存，不碰 gen/pending/emit ──
// 旧实现让预取复用 searchModsEx：预取会 ++m_searchGen 并重置 pending，
// 打断在途主搜索（主搜索响应被 gen 检查丢弃 → 列表卡 loading/闪动）。
// 预取改为独立入口：请求结果只进 getJson 缓存(cacheable=true) + 图标缓存，
// 翻页/搜索时引擎缓存命中秒开；不产生任何聚合信号，与真实搜索物理隔离。
void ResourceBackend::prefetchModsEx(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    int offset, int limit)
{
    if (!m_fetchEngine) return;
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:mod")});
    if (!category.isEmpty())
        facetArr.append(QJsonArray{QStringLiteral("categories:") + category});
    if (!loader.isEmpty())
        facetArr.append(QJsonArray{QStringLiteral("categories:") + loader});
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
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("relevance"));
    url.setQuery(params);
    m_fetchEngine->getJson(url.toString(), true,
        [](int, const QByteArray&) { /* 只进缓存，丢弃结果 */ },
        [](const QString&) { /* 静默 */ });
}

void ResourceBackend::prefetchShadersEx(const QString& query, const QStringList& gameVersions,
    const QStringList& categories, int offset, int limit)
{
    if (!m_fetchEngine) return;
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:shader")});
    for (const QString& c : categories)
        facetArr.append(QJsonArray{QStringLiteral("categories:") + c});
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
    m_fetchEngine->getJson(url.toString(), true,
        [](int, const QByteArray&) { /* 只进缓存，丢弃结果 */ },
        [](const QString&) { /* 静默 */ });
}

void ResourceBackend::prefetchResourcepacks(const QString& query, const QString& gameVersion,
    const QStringList& categories, int offset, int limit)
{
    if (!m_fetchEngine) return;
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
    params.addQueryItem(QStringLiteral("facets"),
                        QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    params.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
    url.setQuery(params);
    m_fetchEngine->getJson(url.toString(), true,
        [](int, const QByteArray&) { /* 只进缓存，丢弃结果 */ },
        [](const QString&) { /* 静默 */ });
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
    // ═══ 池子架构（对齐 主流启动器）═══
    const QString key = query + QLatin1Char('|') + gameVersions.join(QLatin1Char(','))
                      + QLatin1Char('|') + categories.join(QLatin1Char(','))
                      + QLatin1Char('|') + performance.join(QLatin1Char(','))
                      + QLatin1Char('|') + loader.join(QLatin1Char(','));
    m_shaderSearchPage = limit > 0 ? offset / limit : 0;
    m_shaderSearchLimit = limit;
    m_shaderSearchQuery = query;
    m_shaderSearchVersions = gameVersions;
    m_shaderSearchCats = categories;
    m_shaderSearchPerf = performance;
    m_shaderSearchLoader = loader;

    if (key != m_shaderSearchKey) {
        ++m_searchGen;   // 新搜索代次：旧响应全部作废
        m_shaderSearchKey = key;
        m_shaderPool.clear();
        m_shaderMrAll.clear();
        m_shaderCfAll.clear();
        m_shaderMrOffset = 0;
        m_shaderCfOffset = 0;
        m_shaderMrMore = true;
        m_shaderCfMore = true;
        m_shaderFallbackUsed = false;
        m_shaderShownCount = 0;
        emit logMessage(tr("[池子] 光影搜索重置: offset=%1 limit=%2").arg(offset).arg(limit));
    }
    ensureShaderPool();
}

// 池子驱动：光影双源续拉 + 合并 + 发信号
void ResourceBackend::ensureShaderPool()
{
    const int need = (m_shaderSearchPage + 1) * m_shaderSearchLimit;
    if (m_shaderPending > 0) return;
    if (m_shaderPool.size() >= need || (!m_shaderMrMore && !m_shaderCfMore)) {
        emitShaderPool();
        return;
    }

    const int gen = m_searchGen;
    const QString& query = m_shaderSearchQuery;
    const QStringList& gameVersions = m_shaderSearchVersions;
    const QStringList& categories = m_shaderSearchCats;
    const QStringList& performance = m_shaderSearchPerf;
    const QStringList& loader = m_shaderSearchLoader;

    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:shader")});
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

    m_shaderPending = 0;
    m_modMgr->setBusy(true);

    if (m_shaderMrMore) {
        QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
        QUrlQuery params;
        if (!query.isEmpty())
            params.addQueryItem(QStringLiteral("query"), query);
        params.addQueryItem(QStringLiteral("facets"),
                            QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
        params.addQueryItem(QStringLiteral("offset"), QString::number(m_shaderMrOffset));
        params.addQueryItem(QStringLiteral("limit"), QString::number(m_shaderSearchLimit));
        params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
        url.setQuery(params);
        ++m_shaderPending;
        const auto onMrOk = [this, gen, url](int status, const QByteArray& body) {
            if (gen != m_searchGen) return;
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
                if (totalHits == 0 && results.isEmpty() && !m_shaderFallbackUsed) {
                    m_shaderFallbackUsed = true;
                    emit logMessage(tr("光影镜像返回空(异常)，降级官方 API"));
                    QUrl fbUrl = url;
                    fbUrl.setHost(QStringLiteral("api.modrinth.com"));
                    fbUrl.setPath(QStringLiteral("/v2/search"));
                    fbUrl.setScheme(QStringLiteral("https"));
                    const auto onFbOk = [this, gen](int s2, const QByteArray& b2) {
                        if (gen != m_searchGen) return;
                        if (s2 == 200) {
                            int t2 = 0;
                            QJsonArray r2 = m_modMgr->parseSearchResponse(b2, t2);
                            m_shaderMrAll.append(parseSearchResponseItems(r2));
                            m_shaderMrOffset += r2.size();
                            m_shaderMrMore = m_shaderMrOffset < t2;
                            emit logMessage(tr("光影官方降级成功: +%1 条").arg(r2.size()));
                        } else {
                            m_shaderMrMore = false;
                            emit logMessage(tr("光影官方降级失败: HTTP %1").arg(s2));
                        }
                        onShaderSourceDone(gen);
                    };
                    const auto onFbFail = [this, gen](const QString& e2) {
                        if (gen != m_searchGen) return;
                        m_shaderMrMore = false;
                        emit logMessage(tr("光影官方降级网络错误: %1").arg(e2));
                        onShaderSourceDone(gen);
                    };
                    if (m_fetchEngine)
                        m_fetchEngine->getJson(fbUrl.toString(), true, onFbOk, onFbFail);
                    else
                        HttpClient::instance().get(fbUrl.toString(), onFbOk, onFbFail);
                    return;
                }
                m_shaderMrAll.append(parseSearchResponseItems(results));
                m_shaderMrOffset += results.size();
                m_shaderMrMore = m_shaderMrOffset < totalHits;
            } else {
                m_shaderMrMore = false;
                emit logMessage(tr("光影搜索失败: HTTP %1").arg(status));
            }
            onShaderSourceDone(gen);
        };
        const auto onMrFail = [this, gen](const QString& error) {
            if (gen != m_searchGen) return;
            m_shaderMrMore = false;
            emit logMessage(tr("光影搜索网络错误: %1").arg(error));
            onShaderSourceDone(gen);
        };
        if (m_fetchEngine)
            m_fetchEngine->getJson(url.toString(), true, onMrOk, onMrFail);
        else
            HttpClient::instance().get(url.toString(), onMrOk, onMrFail);
    }

    // ── CurseForge 光影（仅支持 gameVersion+loader；categories/performance 为
    //    Modrinth 独占筛选 → 激活时跳过 CF，避免 CF 结果绕过筛选混入）──
    if (m_cfApi && m_shaderCfMore && categories.isEmpty() && performance.isEmpty()) {
        ++m_shaderPending;
        m_cfApi->search(6552, query, 0,
                        gameVersions.isEmpty() ? QString() : gameVersions.first(),
                        loader.isEmpty() ? QString() : loader.first(),
                        m_shaderCfOffset, m_shaderSearchLimit,
            [this, gen](const QVariantList& items, int total) {
                if (gen != m_searchGen) return;
                m_shaderCfAll.append(items);
                m_shaderCfOffset += items.size();
                m_shaderCfMore = m_shaderCfOffset < total;
                onShaderSourceDone(gen);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                m_shaderCfMore = false;
                emit logMessage(tr("CurseForge 光影搜索失败: %1").arg(err));
                onShaderSourceDone(gen);
            });
    }

    if (m_shaderPending == 0) {
        emitShaderPool();
    } else {
        QTimer::singleShot(8000, this, [this, gen]() {
            if (gen != m_searchGen) return;
            if (m_shaderPending > 0) {
                m_shaderPending = 0;
                emitShaderPool();
            }
        });
    }
}

void ResourceBackend::onShaderSourceDone(int gen)
{
    if (gen != m_searchGen) return;
    if (--m_shaderPending > 0) return;
    m_modMgr->setBusy(false);
    // 冻结区 + 候选区（防滚动闪动）
    QVariantList frozen = m_shaderPool.mid(0, m_shaderShownCount);
    QSet<QString> frozenKeys;
    for (const QVariant& v : frozen)
        frozenKeys.insert(normTitle(v.toMap().value(QStringLiteral("title")).toString()));
    QVariantList mrRest, cfRest;
    for (const QVariant& v : m_shaderMrAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            mrRest.append(v);
    }
    for (const QVariant& v : m_shaderCfAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            cfRest.append(v);
    }
    const QVariantList candidate = mergeDedupSorted(mrRest, cfRest, 2.5);
    m_shaderPool = frozen + candidate;
    emit logMessage(tr("[池子] 光影合并: 冻结 %1 + 候选 %2 → 池 %3 条")
                        .arg(frozen.size()).arg(candidate.size()).arg(m_shaderPool.size()));
    const int need = (m_shaderSearchPage + 1) * m_shaderSearchLimit;
    if (m_shaderPool.size() < need && (m_shaderMrMore || m_shaderCfMore)) {
        ensureShaderPool();
        return;
    }
    emitShaderPool();
}

void ResourceBackend::emitShaderPool()
{
    m_modMgr->setBusy(false);
    const int shown = (m_shaderSearchPage + 1) * m_shaderSearchLimit;
    if (shown > m_shaderShownCount)
        m_shaderShownCount = qMin(shown, m_shaderPool.size());
    emit shaderSearchResultsReady(m_shaderPool);
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
    // ═══ 池子架构（对齐 主流启动器）═══
    const QString key = query + QLatin1Char('|') + gameVersion + QLatin1Char('|')
                      + categories.join(QLatin1Char(','));
    m_rpSearchPage = 20 > 0 ? offset / 20 : 0;
    m_rpSearchLimit = 20;
    m_rpSearchQuery = query;
    m_rpSearchVersion = gameVersion;
    m_rpSearchCats = categories;

    if (key != m_rpSearchKey) {
        ++m_searchGen;   // 新搜索代次：旧响应全部作废
        m_rpSearchKey = key;
        m_rpPool.clear();
        m_rpMrAll.clear();
        m_rpCfAll.clear();
        m_rpMrOffset = 0;
        m_rpCfOffset = 0;
        m_rpMrMore = true;
        m_rpCfMore = true;
        m_rpFallbackUsed = false;
        m_rpShownCount = 0;
        emit logMessage(tr("[池子] 资源包搜索重置: offset=%1").arg(offset));
    }
    ensureRpPool();
}

void ResourceBackend::ensureRpPool()
{
    const int need = (m_rpSearchPage + 1) * m_rpSearchLimit;
    if (m_rpPending > 0) return;
    if (m_rpPool.size() >= need || (!m_rpMrMore && !m_rpCfMore)) {
        emitRpPool();
        return;
    }

    const int gen = m_searchGen;
    const QString& query = m_rpSearchQuery;
    const QString& gameVersion = m_rpSearchVersion;
    const QStringList& categories = m_rpSearchCats;

    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:resourcepack")});
    if (!gameVersion.isEmpty())
        facetArr.append(QJsonArray{QStringLiteral("versions:") + gameVersion});
    for (const QString& c : categories)
        facetArr.append(QJsonArray{QStringLiteral("categories:") + c});

    m_rpPending = 0;
    m_modMgr->setBusy(true);

    if (m_rpMrMore) {
        QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
        QUrlQuery params;
        if (!query.isEmpty())
            params.addQueryItem(QStringLiteral("query"), query);
        params.addQueryItem(QStringLiteral("facets"), QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
        params.addQueryItem(QStringLiteral("offset"), QString::number(m_rpMrOffset));
        params.addQueryItem(QStringLiteral("limit"), QString::number(m_rpSearchLimit));
        params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
        url.setQuery(params);
        ++m_rpPending;
        const auto onRpOk = [this, gen, url](int status, const QByteArray& body) {
            if (gen != m_searchGen) return;
            if (status == 200) {
                int total = 0;
                QJsonArray hits = m_modMgr->parseSearchResponse(body, total);
                if (total == 0 && hits.isEmpty() && !m_rpFallbackUsed) {
                    m_rpFallbackUsed = true;
                    emit logMessage(tr("[RP] 镜像返回空(异常)，降级官方 API"));
                    QUrl fbUrl = url;
                    fbUrl.setHost(QStringLiteral("api.modrinth.com"));
                    fbUrl.setPath(QStringLiteral("/v2/search"));
                    fbUrl.setScheme(QStringLiteral("https"));
                    const auto onFbOk = [this, gen](int s2, const QByteArray& b2) {
                        if (gen != m_searchGen) return;
                        if (s2 == 200) {
                            int t2 = 0;
                            QJsonArray r2 = m_modMgr->parseSearchResponse(b2, t2);
                            QVariantList l2;
                            for (const QJsonValue& item : r2) {
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
                                entry[QStringLiteral("categories")]  = QVariantList{};
                                entry[QStringLiteral("features")]    = QVariantList{};
                                entry[QStringLiteral("resolutions")] = QVariantList{};
                                l2.append(entry);
                            }
                            m_rpMrAll.append(l2);
                            m_rpMrOffset += l2.size();
                            m_rpMrMore = m_rpMrOffset < t2;
                            emit logMessage(tr("[RP] 官方降级成功: +%1 条").arg(l2.size()));
                        } else {
                            m_rpMrMore = false;
                            emit logMessage(tr("[RP] 官方降级失败: HTTP %1").arg(s2));
                        }
                        onRpSourceDone(gen);
                    };
                    const auto onFbFail = [this, gen](const QString& e2) {
                        if (gen != m_searchGen) return;
                        m_rpMrMore = false;
                        emit logMessage(tr("[RP] 官方降级网络错误: %1").arg(e2));
                        onRpSourceDone(gen);
                    };
                    if (m_fetchEngine)
                        m_fetchEngine->getJson(fbUrl.toString(), true, onFbOk, onFbFail);
                    else
                        HttpClient::instance().get(fbUrl.toString(), onFbOk, onFbFail);
                    return;
                }
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
                m_rpMrAll.append(list);
                m_rpMrOffset += list.size();
                m_rpMrMore = m_rpMrOffset < total;
            } else {
                m_rpMrMore = false;
                emit logMessage(tr("[RP] Modrinth 搜索失败: HTTP %1").arg(status));
            }
            onRpSourceDone(gen);
        };
        const auto onRpFail = [this, gen](const QString& error) {
            if (gen != m_searchGen) return;
            m_rpMrMore = false;
            emit logMessage(tr("[RP] Modrinth 网络错误: %1").arg(error));
            onRpSourceDone(gen);
        };
        if (m_fetchEngine)
            m_fetchEngine->getJson(url.toString(), true, onRpOk, onRpFail);
        else
            HttpClient::instance().get(url.toString(), onRpOk, onRpFail);
    }

    // ── CurseForge 资源包（CF 搜索仅支持 gameVersion，categoryId 恒 0；
    //    分类/特性/分辨率筛选为 Modrinth 独占 → 激活时跳过 CF）──
    if (m_cfApi && m_rpCfMore && categories.isEmpty()) {
        ++m_rpPending;
        m_cfApi->search(12, query, 0, gameVersion, QString(), m_rpCfOffset, m_rpSearchLimit,
            [this, gen](const QVariantList& items, int total) {
                if (gen != m_searchGen) return;
                m_rpCfAll.append(items);
                m_rpCfOffset += items.size();
                m_rpCfMore = m_rpCfOffset < total;
                onRpSourceDone(gen);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                m_rpCfMore = false;
                emit logMessage(tr("[RP] CurseForge 搜索失败: %1").arg(err));
                onRpSourceDone(gen);
            });
    }

    if (m_rpPending == 0) {
        emitRpPool();
    } else {
        QTimer::singleShot(8000, this, [this, gen]() {
            if (gen != m_searchGen) return;
            if (m_rpPending > 0) {
                m_rpPending = 0;
                emitRpPool();
            }
        });
    }
}

void ResourceBackend::onRpSourceDone(int gen)
{
    if (gen != m_searchGen) return;
    if (--m_rpPending > 0) return;
    m_modMgr->setBusy(false);
    // 冻结区 + 候选区（防滚动闪动）
    QVariantList frozen = m_rpPool.mid(0, m_rpShownCount);
    QSet<QString> frozenKeys;
    for (const QVariant& v : frozen)
        frozenKeys.insert(normTitle(v.toMap().value(QStringLiteral("title")).toString()));
    QVariantList mrRest, cfRest;
    for (const QVariant& v : m_rpMrAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            mrRest.append(v);
    }
    for (const QVariant& v : m_rpCfAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            cfRest.append(v);
    }
    const QVariantList candidate = mergeDedupSorted(mrRest, cfRest, 2.5);
    m_rpPool = frozen + candidate;
    emit logMessage(tr("[池子] 资源包合并: 冻结 %1 + 候选 %2 → 池 %3 条")
                        .arg(frozen.size()).arg(candidate.size()).arg(m_rpPool.size()));
    const int need = (m_rpSearchPage + 1) * m_rpSearchLimit;
    if (m_rpPool.size() < need && (m_rpMrMore || m_rpCfMore)) {
        ensureRpPool();
        return;
    }
    emitRpPool();
}

void ResourceBackend::emitRpPool()
{
    m_modMgr->setBusy(false);
    const int shown = (m_rpSearchPage + 1) * m_rpSearchLimit;
    if (shown > m_rpShownCount)
        m_rpShownCount = qMin(shown, m_rpPool.size());
    emit resourcepackSearchCompleted(m_rpPool, m_rpPool.size());
}


// ═══════════════════════════════════════════════════════════
// 整合包双源搜索（Modrinth project_type:modpack + CF classId=4471）
// 池子架构与 Mod/Shader/RP 完全一致：双源累积成池 → 去重加权混排 →
// QML 按页切片；冻结区 + 候选区防滚动闪动。
// ═══════════════════════════════════════════════════════════

void ResourceBackend::searchModpacksEx(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    int page, int limit)
{
    const QString key = query + QLatin1Char('|') + loader + QLatin1Char('|') + category
                      + QLatin1Char('|') + gameVersions.join(QLatin1Char(','));
    m_packSearchPage = limit > 0 ? page / limit : 0;
    m_packSearchLimit = limit;
    m_packSearchQuery = query;
    m_packSearchLoader = loader;
    m_packSearchCategory = category;
    m_packSearchVersions = gameVersions;

    if (key != m_packSearchKey) {
        ++m_searchGen;
        m_packSearchKey = key;
        m_packPool.clear();
        m_packMrAll.clear();
        m_packCfAll.clear();
        m_packMrOffset = 0;
        m_packCfOffset = 0;
        m_packMrMore = true;
        m_packCfMore = true;
        m_packFallbackUsed = false;
        m_packShownCount = 0;
        emit logMessage(tr("[池子] 整合包搜索重置: q=%1 loader=%2 cat=%3")
            .arg(query).arg(loader).arg(category));
    }
    ensurePackPool();
}

void ResourceBackend::ensurePackPool()
{
    const int need = (m_packSearchPage + 1) * m_packSearchLimit;
    if (m_packPending > 0) return;
    if (m_packPool.size() >= need || (!m_packMrMore && !m_packCfMore)) {
        emitPackPool();
        return;
    }

    const int gen = m_searchGen;
    const QString& query = m_packSearchQuery;
    const QString& loader = m_packSearchLoader;
    const QString& category = m_packSearchCategory;
    const QStringList& gameVersions = m_packSearchVersions;

    // ── Modrinth：project_type:modpack + 加载器/分类/版本 facets ──
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:modpack")});
    if (!category.isEmpty() && !CfApi::isCfCategory(category))
        facetArr.append(QJsonArray{QStringLiteral("categories:") + category});
    if (!loader.isEmpty())
        facetArr.append(QJsonArray{QStringLiteral("categories:") + loader});
    if (!gameVersions.isEmpty()) {
        QJsonArray verGroup;
        for (const QString& v : gameVersions)
            verGroup.append(QStringLiteral("versions:") + v);
        facetArr.append(verGroup);
    }

    m_packPending = 0;
    m_modMgr->setBusy(true);

    if (m_packMrMore) {
        QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
        QUrlQuery params;
        if (!query.isEmpty())
            params.addQueryItem(QStringLiteral("query"), query);
        params.addQueryItem(QStringLiteral("facets"),
                            QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
        params.addQueryItem(QStringLiteral("offset"), QString::number(m_packMrOffset));
        params.addQueryItem(QStringLiteral("limit"), QString::number(m_packSearchLimit));
        params.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
        url.setQuery(params);
        ++m_packPending;
        const auto onMrOk = [this, gen, url](int status, const QByteArray& body) {
            if (gen != m_searchGen) return;
            if (status == 200) {
                int totalHits = 0;
                QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
                if (totalHits == 0 && results.isEmpty() && !m_packFallbackUsed) {
                    m_packFallbackUsed = true;
                    emit logMessage(tr("整合包镜像返回空(异常)，降级官方 API"));
                    QUrl fbUrl = url;
                    fbUrl.setHost(QStringLiteral("api.modrinth.com"));
                    fbUrl.setPath(QStringLiteral("/v2/search"));
                    fbUrl.setScheme(QStringLiteral("https"));
                    const auto onFbOk = [this, gen](int s2, const QByteArray& b2) {
                        if (gen != m_searchGen) return;
                        if (s2 == 200) {
                            int t2 = 0;
                            QJsonArray r2 = m_modMgr->parseSearchResponse(b2, t2);
                            m_packMrAll.append(parseSearchResponseItems(r2));
                            m_packMrOffset += r2.size();
                            m_packMrMore = m_packMrOffset < t2;
                            emit logMessage(tr("整合包官方降级成功: +%1 条").arg(r2.size()));
                        } else {
                            m_packMrMore = false;
                            emit logMessage(tr("整合包官方降级失败: HTTP %1").arg(s2));
                        }
                        onPackSourceDone(gen);
                    };
                    const auto onFbFail = [this, gen](const QString& e2) {
                        if (gen != m_searchGen) return;
                        m_packMrMore = false;
                        emit logMessage(tr("整合包官方降级网络错误: %1").arg(e2));
                        onPackSourceDone(gen);
                    };
                    if (m_fetchEngine)
                        m_fetchEngine->getJson(fbUrl.toString(), true, onFbOk, onFbFail);
                    else
                        HttpClient::instance().get(fbUrl.toString(), onFbOk, onFbFail);
                    return;
                }
                m_packMrAll.append(parseSearchResponseItems(results));
                m_packMrOffset += results.size();
                m_packMrMore = m_packMrOffset < totalHits;
            } else {
                m_packMrMore = false;
                emit logMessage(tr("整合包 Modrinth 搜索失败: HTTP %1").arg(status));
            }
            onPackSourceDone(gen);
        };
        const auto onMrFail = [this, gen](const QString& error) {
            if (gen != m_searchGen) return;
            m_packMrMore = false;
            emit logMessage(tr("整合包 Modrinth 网络错误: %1").arg(error));
            onPackSourceDone(gen);
        };
        if (m_fetchEngine)
            m_fetchEngine->getJson(url.toString(), true, onMrOk, onMrFail);
        else
            HttpClient::instance().get(url.toString(), onMrOk, onMrFail);
    }

    // ── CurseForge：classId=4471（分类/加载器/版本均有效，实测）──
    if (m_cfApi && m_packCfMore) {
        ++m_packPending;
        m_cfApi->search(4471, query, CfApi::cfCategoryId(category),
                        gameVersions.isEmpty() ? QString() : gameVersions.first(),
                        loader, m_packCfOffset, m_packSearchLimit,
            [this, gen](const QVariantList& items, int total) {
                if (gen != m_searchGen) return;
                m_packCfAll.append(items);
                m_packCfOffset += items.size();
                m_packCfMore = m_packCfOffset < total;
                onPackSourceDone(gen);
            },
            [this, gen](const QString& err) {
                if (gen != m_searchGen) return;
                m_packCfMore = false;
                emit logMessage(tr("整合包 CurseForge 搜索失败: %1").arg(err));
                onPackSourceDone(gen);
            });
    }

    if (m_packPending == 0) {
        emitPackPool();
    } else {
        QTimer::singleShot(8000, this, [this, gen]() {
            if (gen != m_searchGen) return;
            if (m_packPending > 0) {
                m_packPending = 0;
                emitPackPool();
            }
        });
    }
}

void ResourceBackend::onPackSourceDone(int gen)
{
    if (gen != m_searchGen) return;
    if (--m_packPending > 0) return;
    m_modMgr->setBusy(false);

    QVariantList frozen = m_packPool.mid(0, m_packShownCount);
    QSet<QString> frozenKeys;
    for (const QVariant& v : frozen)
        frozenKeys.insert(normTitle(v.toMap().value(QStringLiteral("title")).toString()));
    QVariantList mrRest, cfRest;
    for (const QVariant& v : m_packMrAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            mrRest.append(v);
    }
    for (const QVariant& v : m_packCfAll) {
        if (!frozenKeys.contains(normTitle(v.toMap().value(QStringLiteral("title")).toString())))
            cfRest.append(v);
    }
    const QVariantList candidate = mergeDedupSorted(mrRest, cfRest, 2.5);
    m_packPool = frozen + candidate;

    // ── 统一字符串化（防 QML 预编译模式下 QVariantList → QQmlListModel）──
    // ListModel role 若为数组，delegate 赋给 QString 属性报
    // "Unable to assign QQmlListModel to QString"。versions/loadersList 统一 join 成字符串。
    for (auto& v : m_packPool) {
        QVariantMap m = v.toMap();
        bool changed = false;
        const QStringList keys = { QStringLiteral("versions"),
                                   QStringLiteral("loadersList") };
        for (const QString& key : keys) {
            const QVariant val = m.value(key);
            // QStringList 的 userType 是 QMetaType::QStringList（≠ QVariantList）
            if (val.userType() == QMetaType::QVariantList || val.userType() == QMetaType::QStringList) {
                QStringList sl;
                const QVariantList vl = val.toList();
                for (const auto& e : vl) sl << e.toString();
                m.insert(key, sl.join(QLatin1Char(',')));
                changed = true;
            }
        }
        if (changed) v = m;
    }
    emit logMessage(tr("[池子] 整合包合并: 冻结 %1 + 候选 %2 → 池 %3 条")
                        .arg(frozen.size()).arg(candidate.size()).arg(m_packPool.size()));
    const int need = (m_packSearchPage + 1) * m_packSearchLimit;
    if (m_packPool.size() < need && (m_packMrMore || m_packCfMore)) {
        ensurePackPool();
        return;
    }
    emitPackPool();
}

void ResourceBackend::emitPackPool()
{
    m_modMgr->setBusy(false);
    const int shown = (m_packSearchPage + 1) * m_packSearchLimit;
    if (shown > m_packShownCount)
        m_packShownCount = qMin(shown, m_packPool.size());
    emit modpackSearchResultsReady(m_packPool);
}

// ── 整合包详情版本列表：Modrinth slug → fetchModVersions；CF 数字 id → fetchModVersionsCf ──
void ResourceBackend::fetchModpackVersions(const QString& slug, const QString& gameVersion, const QString& loader)
{
    if (slug.isEmpty()) { emit modVersionsPartial(slug, {}, {}); return; }
    const bool isCf = slug.contains(QRegularExpression(QStringLiteral("^\\d+$")));
    if (isCf) {
        fetchModVersionsCf(slug, gameVersion, loader);
    } else {
        fetchModVersions({slug});
    }
}

// 整合包翻页预取：只预热司南引擎缓存（不碰 gen/pending/emit，与 prefetchModsEx 同构）
void ResourceBackend::prefetchModpacks(const QString& query, const QString& loader,
    const QString& category, const QStringList& gameVersions,
    int offset, int limit)
{
    if (!m_fetchEngine) return;
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:modpack")});
    if (!category.isEmpty() && !CfApi::isCfCategory(category))
        facetArr.append(QJsonArray{QStringLiteral("categories:") + category});
    if (!loader.isEmpty())
        facetArr.append(QJsonArray{QStringLiteral("categories:") + loader});
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
    m_fetchEngine->getJson(url.toString(), true,
        [this](int, const QByteArray& body) {
            // 预热图标缓存（仅限 200 响应）
            int totalHits = 0;
            QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
            QStringList urls;
            for (const QJsonValue& v : results) {
                const QString u = v.toObject().value(QStringLiteral("iconUrl")).toString();
                if (!u.isEmpty()) urls.append(u);
            }
            if (!urls.isEmpty() && m_fetchEngine)
                m_fetchEngine->prefetchIcons(urls);
        },
        [](const QString&) {});
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

// ── CF 详情页前置依赖解析（Modrinth 优先映射）──
// 每个 dep：先取 CF 名称/图标（fetchModInfo），再按名称在 Modrinth 检索
// （searchModrinthForDep，镜像空→官方降级）；命中 → 用 Modrinth slug/title/icon
// （点击进 Modrinth 详情页）；未命中 → 保留 CF 数据（slug=数字 id，可进 CF 详情）。
void ResourceBackend::resolveCfDependencies(const QString& modId, const QVariantList& deps)
{
    const int total = qMin(deps.size(), 8);
    if (!m_cfApi || total <= 0) { emit cfDependenciesResolved(modId, {}); return; }

    auto out = std::make_shared<QVariantList>();
    auto proc = std::make_shared<std::function<void(int)>>();
    *proc = [this, modId, deps, total, out, proc](int idx) {
        if (idx >= total) {
            emit cfDependenciesResolved(modId, *out);
            return;
        }
        const QVariantMap dep = deps[idx].toMap();
        const QString pid = dep.value(QStringLiteral("project_id")).toString();
        const QString depType = dep.value(QStringLiteral("dependency_type"),
                                           QStringLiteral("required")).toString();
        auto fallbackEntry = [pid, depType]() {
            QVariantMap e;
            e[QStringLiteral("project_id")] = pid;
            e[QStringLiteral("dependency_type")] = depType;
            e[QStringLiteral("title")] = pid;   // 无名称时显示数字 id（原始行为）
            e[QStringLiteral("slug")] = pid;    // 数字 slug → 点击进 CF 详情
            e[QStringLiteral("icon_url")] = QString();
            e[QStringLiteral("description")] = QString();
            e[QStringLiteral("source")] = QStringLiteral("CurseForge");
            return e;
        };
        m_cfApi->fetchModInfo(pid,
            [this, out, proc, idx, depType, pid, fallbackEntry](const QVariantMap& info) {
                const QString cfName = info.value(QStringLiteral("name")).toString();
                if (cfName.isEmpty()) {
                    out->append(fallbackEntry());
                    (*proc)(idx + 1);
                    return;
                }
                // 有 CF 名称 → 尝试 Modrinth 映射
                searchModrinthForDep(cfName,
                    [this, out, proc, idx, depType, pid, info, cfName, fallbackEntry](const QVariantMap& mr) {
                        QVariantMap entry;
                        entry[QStringLiteral("project_id")] = pid;
                        entry[QStringLiteral("dependency_type")] = depType;
                        if (!mr.isEmpty()) {
                            // Modrinth 命中：名称/图标/跳转全部用 Modrinth 数据
                            entry[QStringLiteral("title")] = mr.value(QStringLiteral("title")).toString();
                            entry[QStringLiteral("slug")] = mr.value(QStringLiteral("slug")).toString();
                            entry[QStringLiteral("icon_url")] = mr.value(QStringLiteral("icon")).toString();
                            entry[QStringLiteral("description")] = mr.value(QStringLiteral("desc")).toString();
                            entry[QStringLiteral("source")] = QStringLiteral("Modrinth");
                            emit logMessage(tr("[依赖] %1 → Modrinth: %2").arg(cfName, mr.value(QStringLiteral("title")).toString()));
                        } else {
                            // Modrinth 无匹配 → 保留 CF 数据
                            entry[QStringLiteral("title")] = cfName;
                            entry[QStringLiteral("slug")] = info.value(QStringLiteral("slug"), pid).toString();
                            entry[QStringLiteral("icon_url")] = info.value(QStringLiteral("icon_url")).toString();
                            entry[QStringLiteral("description")] = info.value(QStringLiteral("summary")).toString();
                            entry[QStringLiteral("source")] = QStringLiteral("CurseForge");
                        }
                        out->append(entry);
                        (*proc)(idx + 1);
                    });
            },
            [out, proc, idx, fallbackEntry](const QString&) {
                // CF 信息获取失败：保留原始数字 id
                out->append(fallbackEntry());
                (*proc)(idx + 1);
            });
    };
    (*proc)(0);
}

// 按名称在 Modrinth 检索（镜像空→官方降级）；命中取归一化标题精确匹配，
// 无精确则取下载量最高的包含匹配；返回 {slug,title,icon,desc}，空 map = 未命中。
void ResourceBackend::searchModrinthForDep(const QString& name, std::function<void(const QVariantMap&)> done)
{
    QUrl url(QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("query"), name);
    QJsonArray facetArr;
    facetArr.append(QJsonArray{QStringLiteral("project_type:mod")});
    params.addQueryItem(QStringLiteral("facets"),
                        QJsonDocument(facetArr).toJson(QJsonDocument::Compact));
    params.addQueryItem(QStringLiteral("limit"), QStringLiteral("8"));
    params.addQueryItem(QStringLiteral("index"), QStringLiteral("relevance"));
    url.setQuery(params);

    const QString normName = normTitle(name);
    const auto parseHits = [this, done, normName](const QByteArray& body) {
        QVariantMap exact, fallback;
        int fallbackDl = -1;
        int totalHits = 0;
        QJsonArray results = m_modMgr->parseSearchResponse(body, totalHits);
        for (const QJsonValue& v : results) {
            const QJsonObject obj = v.toObject();
            const QString title = obj.value(QStringLiteral("title")).toString();
            const QVariantMap m = {
                {QStringLiteral("slug"), obj.value(QStringLiteral("slug")).toString()},
                {QStringLiteral("title"), title},
                {QStringLiteral("icon"), obj.value(QStringLiteral("iconUrl")).toString()},
                {QStringLiteral("desc"), obj.value(QStringLiteral("description")).toString()}
            };
            if (normTitle(title) == normName) { exact = m; break; }
            const int dl = obj.value(QStringLiteral("downloads")).toInt();
            if (dl > fallbackDl) { fallbackDl = dl; fallback = m; }
        }
        if (done) done(!exact.isEmpty() ? exact : fallback);
    };
    const auto onOk = [this, url, parseHits, done](int status, const QByteArray& body) {
        if (status != 200) { if (done) done({}); return; }
        int totalHits = 0;
        QJsonArray r = m_modMgr->parseSearchResponse(body, totalHits);
        // 镜像异常空 → 官方降级一次
        if (totalHits == 0 && r.isEmpty()) {
            QUrl fbUrl = url;
            fbUrl.setHost(QStringLiteral("api.modrinth.com"));
            fbUrl.setPath(QStringLiteral("/v2/search"));
            fbUrl.setScheme(QStringLiteral("https"));
            const auto onFbOk = [parseHits, done](int s2, const QByteArray& b2) {
                if (s2 == 200) parseHits(b2);
                else if (done) done({});
            };
            const auto onFbFail = [done](const QString&) { if (done) done({}); };
            if (m_fetchEngine)
                m_fetchEngine->getJson(fbUrl.toString(), true, onFbOk, onFbFail);
            else
                HttpClient::instance().get(fbUrl.toString(), onFbOk, onFbFail);
            return;
        }
        parseHits(body);
    };
    const auto onFail = [done](const QString&) { if (done) done({}); };
    if (m_fetchEngine)
        m_fetchEngine->getJson(url.toString(), true, onOk, onFail);
    else
        HttpClient::instance().get(url.toString(), onOk, onFail);
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
        // versions 统一为逗号分隔字符串（与 CurseForge toUnified 一致），
        // 避免 QML 端混用数组/字符串导致 .join TypeError → 列表渲染中断
        entry[QStringLiteral("versions")]     = obj[QStringLiteral("gameVersions")].toVariant().toStringList().join(QLatin1Char(','));
        entry[QStringLiteral("dateModified")]= obj[QStringLiteral("updated")].toString();
        entry[QStringLiteral("license")]    = obj[QStringLiteral("license")].toVariant();
        entry[QStringLiteral("source")]     = QStringLiteral("Modrinth");
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
