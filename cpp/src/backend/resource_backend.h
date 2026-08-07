// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <functional>

namespace ShadowLauncher {

class ModManager;
class ResourceFetchEngine;
class CfApi;

class ResourceBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadStateChanged)
    Q_PROPERTY(int dlProgress READ dlProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(int dlTotal READ dlTotal NOTIFY downloadProgressChanged)
    Q_PROPERTY(int dlSpeed READ dlSpeed NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString dlFile READ dlFile NOTIFY downloadProgressChanged)

public:
    explicit ResourceBackend(QObject* parent = nullptr);
    ~ResourceBackend() override;

    /// 注入共享资源拉取引擎（由 ShadowBackend 创建后传入，搜索走引擎缓存/并发）
    void setFetchEngine(ResourceFetchEngine* e);

    // ── CurseForge 详情页版本（复用 modVersionsPartial 等信号，QML 零改动）──
    Q_INVOKABLE void fetchModVersionsCf(const QString& modId, const QString& gameVersion = {}, const QString& loader = {});
    Q_INVOKABLE void fetchShaderVersionsCf(const QString& modId, const QString& gameVersion = {}, const QString& loader = {});
    Q_INVOKABLE void fetchResourcepackVersionsCf(const QString& modId, const QString& gameVersion = {}, const QString& loader = {});
    /// CF 分类静态表（QML 叠加下拉用）：classId 6/12/6552/4471
    Q_INVOKABLE QVariantList cfCategories(int classId) const;

    ModManager* modManager() const { return m_modMgr; }

    bool isDownloading() const { return m_downloading; }
    int dlProgress() const { return m_dlProgress; }
    int dlTotal() const { return m_dlTotal; }
    int dlSpeed() const { return m_dlSpeed; }
    QString dlFile() const { return m_dlFile; }

    // Slots
    Q_INVOKABLE QVariantList getPopularMods(const QString& loader);
    Q_INVOKABLE QVariantList getShaderList();
    Q_INVOKABLE void searchMods(const QString& query, const QString& loader = {});
    Q_INVOKABLE void searchModsEx(const QString& query, const QString& loader,
        const QString& category, const QStringList& gameVersions,
        const QString& environment, const QString& license,
        int offset, int limit, const QString& source = {});
    Q_INVOKABLE QVariantMap getModCategories();
    Q_INVOKABLE void searchShadersEx(const QString& query, const QStringList& gameVersions,
        const QStringList& categories, const QStringList& performance,
        const QStringList& loader, int offset, int limit, const QString& source = {});
    Q_INVOKABLE void downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void searchResourcepacks(const QString& query, const QString& gameVersion = {}, int offset = 0, const QStringList& categories = {}, const QString& source = {});
    /// 整合包双源搜索（Modrinth project_type:modpack + CF classId=4471，池子架构）
    Q_INVOKABLE void searchModpacksEx(const QString& query, const QString& loader,
        const QString& category, const QStringList& gameVersions,
        int offset, int limit, const QString& source = {});
    /// 数据包双源搜索（Modrinth project_type:datapack + CF classId=6945，池子架构）
    /// sort: ""=下载量, "updated"=更新时间, "name"=名称（Modrinth index / CF sortField 映射）
    Q_INVOKABLE void searchDatapacksEx(const QString& query, const QString& category,
        const QStringList& gameVersions, const QString& sort,
        int offset, int limit, const QString& source = {});
    /// 整合包详情版本列表（Modrinth slug → fetchModVersions；CF 数字 id → fetchModVersionsCf）
    /// 复用 modVersionsPartial 信号回传，QML 与 Mod 详情页同构
    Q_INVOKABLE void fetchModpackVersions(const QString& slug, const QString& gameVersion = {}, const QString& loader = {});
    // 翻页预取（只预热缓存，不产生聚合信号，与真实搜索物理隔离）
    Q_INVOKABLE void prefetchModsEx(const QString& query, const QString& loader,
        const QString& category, const QStringList& gameVersions,
        int offset, int limit, const QString& source = {});
    Q_INVOKABLE void prefetchShadersEx(const QString& query, const QStringList& gameVersions,
        const QStringList& categories, int offset, int limit, const QString& source = {});
    Q_INVOKABLE void prefetchResourcepacks(const QString& query, const QString& gameVersion,
        const QStringList& categories, int offset, int limit, const QString& source = {});
    /// 整合包翻页预取（只预热司南缓存 + 图标，不产生聚合信号）
    Q_INVOKABLE void prefetchModpacks(const QString& query, const QString& loader,
        const QString& category, const QStringList& gameVersions,
        int offset, int limit, const QString& source = {});
    /// 数据包翻页预取（同构）
    Q_INVOKABLE void prefetchDatapacks(const QString& query, const QString& category,
        const QStringList& gameVersions, const QString& sort,
        int offset, int limit, const QString& source = {});
    Q_INVOKABLE void downloadResourcepack(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void fetchResourcepackVersions(const QStringList& slugs);
    Q_INVOKABLE void fetchModVersions(const QStringList& slugs);
    Q_INVOKABLE void fetchShaderVersions(const QStringList& slugs);
    Q_INVOKABLE void cancelDownload();

    // Mod file download (user-chosen path)
    Q_INVOKABLE int downloadModFile(const QString& url, const QString& savePath, const QString& displayName,
                                    qint64 expectedSize, const QString& sha1, qint64 receivedOffset = 0, int resumeId = -1);
    Q_INVOKABLE void cancelModFileDownload(int downloadId);
    Q_INVOKABLE void pauseModFileDownload(int downloadId);
    Q_INVOKABLE void resumeModFileDownload(int downloadId);
    Q_INVOKABLE void retryModFileDownload(int downloadId);

    /// CF 详情页前置依赖解析：先取 CF 名称/图标，再按名称在 Modrinth 检索映射
    /// （命中 → Modrinth slug/title/icon，点击进 Modrinth 详情；未命中 → 保留 CF 数据）
    Q_INVOKABLE void resolveCfDependencies(const QString& modId, const QVariantList& deps);
    /// CF 详情页直接拉依赖（/mods/{id} latestFiles）：镜像 files 端点 dependencies 恒空，
    /// mod 详情端点的 latestFiles 才带依赖（实测 2026-08-07）→ 拉出后走 resolveCfDependencies 映射
    Q_INVOKABLE void fetchCfDependencies(const QString& modId);

signals:
    void downloadProgressChanged(int completed, int total, const QString& fileName);
    void downloadStateChanged();
    void downloadFinished(const QString& slug, bool success, const QString& filePath);
    void modFileDownloadStarted(int downloadId, const QString& fileName, qint64 fileSize, const QString& displayName);
    void modFileDownloadProgress(int downloadId, qint64 received, qint64 total, qint64 speed);
    void modFileDownloadFinished(int downloadId, bool success, const QString& filePath, const QString& displayName);
    void modFileDownloadFailed(int downloadId, const QString& errorDetail, const QString& displayName);
    /// 整合包搜索完成（池子全量，QML 按页切片）
    void modpackSearchResultsReady(const QVariantList& results);
    /// 数据包搜索完成（池子全量，QML 按页切片）
    void datapackSearchResultsReady(const QVariantList& results);
    /// CF 前置依赖解析完成（QML 回填依赖卡片）
    void cfDependenciesResolved(const QString& modId, const QVariantList& deps);
    void searchResultsReady(const QVariantList& results);  // deprecated — use modSearchResultsReady / shaderSearchResultsReady
    void modSearchResultsReady(const QVariantList& results);
    void shaderSearchResultsReady(const QVariantList& results);
    void resourcepackSearchCompleted(const QVariantList& results, int totalHits);
    void resourcepackSearchFailed(const QString& error);
    void resourcepackDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackVersionsLoaded(const QVariantMap& slugToVersions);
    void resourcepackVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void resourcepackVersionsProgress(int done, int total);

    void modVersionsLoaded(const QVariantMap& slugToVersions);
    void modVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void modVersionsProgress(int done, int total);

    void shaderVersionsLoaded(const QVariantMap& slugToVersions);
    void shaderVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void shaderVersionsProgress(int done, int total);
    void logMessage(const QString& msg);

private slots:
    void onSearchCompleted(const QJsonArray& results, int totalHits);
    void onResourcepackSearchCompleted(const QJsonArray& results, int totalHits);
    void onResourcepackDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void onResourcepackVersionsLoaded(const QVariantMap& slugToVersions);
    void onModVersionsLoaded(const QVariantMap& slugToVersions);
    void onShaderVersionsLoaded(const QVariantMap& slugToVersions);
    void onDownloadProgress(const QString& name, qint64 received, qint64 total);
    void onDownloadFinished(const QString& slug, bool success, const QString& filePath);

    // Mod file download forwarding
    void onModFileDownloadStarted(int downloadId, const QString& fileName, qint64 fileSize, const QString& displayName);
    void onModFileDownloadProgress(int downloadId, qint64 received, qint64 total, qint64 speed);
    void onModFileDownloadFinished(int downloadId, bool success, const QString& filePath, const QString& displayName);
    void onModFileDownloadFailed(int downloadId, const QString& errorDetail, const QString& displayName);

private:
    enum class SearchKind { Mod, Shader };
    ModManager* m_modMgr = nullptr;
    ResourceFetchEngine* m_fetchEngine = nullptr;
    CfApi* m_cfApi = nullptr;

    // ── 双源聚合状态（代次号防并发搜索污染）──
    int m_searchGen = 0;

    // ── 池子架构（对齐 主流启动器）：双源结果累积成池，翻页时从池子续拉 ──
    // 池 = 已拉取双源结果的 去重+加权排序 全量；QML 按页切片显示。
    // 每 Tab 一组：m_xxxPool(合并池) + m_xxxMrAll/m_xxxCfAll(各源已拉原始) + 游标。
    // 搜索 → 重置；翻页 → 池够则直接发，不够则双源续拉(offset=游标)合并。
    QVariantList m_modPool;
    QVariantList m_modMrAll, m_modCfAll;
    int m_modMrOffset = 0, m_modCfOffset = 0;
    int m_modPending = 0;
    bool m_modMrMore = true, m_modCfMore = true;
    bool m_modSearchActive = false;
    int m_modShownCount = 0;   // 已显示条数（冻结区边界：前 N 条永不重排，防滚动闪动）
    int m_modSearchPage = 0, m_modSearchLimit = 30;
    QString m_modSearchKey;
    QString m_modSearchQuery, m_modSearchLoader, m_modSearchCategory, m_modSearchEnv, m_modSearchLic;
    QString m_modSearchSource;
    QStringList m_modSearchVersions;
    bool m_modSearchCfOnly = false, m_modSearchMrOnly = false;

    QVariantList m_shaderPool;
    QVariantList m_shaderMrAll, m_shaderCfAll;
    int m_shaderMrOffset = 0, m_shaderCfOffset = 0;
    int m_shaderPending = 0;
    bool m_shaderMrMore = true, m_shaderCfMore = true;
    bool m_shaderSearchActive = false;
    int m_shaderShownCount = 0;   // 冻结区边界
    int m_shaderSearchPage = 0, m_shaderSearchLimit = 50;
    QString m_shaderSearchKey;
    QString m_shaderSearchQuery, m_shaderSearchSource;
    QStringList m_shaderSearchVersions, m_shaderSearchCats, m_shaderSearchPerf, m_shaderSearchLoader;

    QVariantList m_rpPool;
    QVariantList m_rpMrAll, m_rpCfAll;
    int m_rpMrOffset = 0, m_rpCfOffset = 0;
    int m_rpPending = 0;
    bool m_rpMrMore = true, m_rpCfMore = true;
    bool m_rpSearchActive = false;
    int m_rpShownCount = 0;   // 冻结区边界
    int m_rpSearchPage = 0, m_rpSearchLimit = 20;
    QString m_rpSearchKey;
    QString m_rpSearchQuery, m_rpSearchVersion, m_rpSearchSource;
    QStringList m_rpSearchCats;

    // ── 整合包池子（Modrinth project_type:modpack + CF classId=4471）──
    QVariantList m_packPool;
    QVariantList m_packMrAll, m_packCfAll;
    int m_packMrOffset = 0, m_packCfOffset = 0;
    int m_packPending = 0;
    bool m_packMrMore = true, m_packCfMore = true;
    bool m_packSearchActive = false;
    int m_packShownCount = 0;   // 冻结区边界
    int m_packSearchPage = 0, m_packSearchLimit = 20;
    QString m_packSearchKey;
    QString m_packSearchQuery, m_packSearchLoader, m_packSearchCategory, m_packSearchSource;
    QStringList m_packSearchVersions;
    bool m_packFallbackUsed = false;
    void ensurePackPool();
    void onPackSourceDone(int gen);
    void emitPackPool();

    // ── 数据包池子（Modrinth project_type:datapack + CF classId=6945）──
    QVariantList m_dpPool;
    QVariantList m_dpMrAll, m_dpCfAll;
    int m_dpMrOffset = 0, m_dpCfOffset = 0;
    int m_dpPending = 0;
    bool m_dpMrMore = true, m_dpCfMore = true;
    bool m_dpSearchActive = false;
    int m_dpShownCount = 0;   // 冻结区边界
    int m_dpSearchPage = 0, m_dpSearchLimit = 20;
    QString m_dpSearchKey;
    QString m_dpSearchQuery, m_dpSearchCategory, m_dpSearchSort, m_dpSearchSource;
    QStringList m_dpSearchVersions;
    bool m_dpFallbackUsed = false;
    void ensureDpPool();
    void onDpSourceDone(int gen);
    void emitDpPool();

    // 本代是否已降级过官方（防镜像异常空时重复降级）
    bool m_mrFallbackUsed = false;
    bool m_shaderFallbackUsed = false;
    bool m_rpFallbackUsed = false;
    void tryAggregateMod(int gen);
    void tryAggregateShader(int gen);
    void tryAggregateRp(int gen);
    // 池子架构辅助（对齐 主流启动器 分页思路）
    void ensureModPool(bool cfOnly, bool mrOnly,
        const QString& query, const QString& loader, const QString& category,
        const QStringList& gameVersions, const QString& environment, const QString& license);
    void onModSourceDone(int gen);
    void emitModPool();
    void ensureShaderPool();
    void onShaderSourceDone(int gen);
    void emitShaderPool();
    void ensureRpPool();
    void onRpSourceDone(int gen);
    void emitRpPool();
    bool m_downloading = false;
    int m_dlProgress = 0;
    int m_dlTotal = 0;
    int m_dlSpeed = 0;
    qint64 m_dlLastBytes = 0;
    qint64 m_dlLastMs = 0;
    QString m_dlFile;
    QString m_minecraftDir;
    SearchKind m_searchKind = SearchKind::Mod;

    // CF 前置依赖解析：按名称在 Modrinth 检索（镜像空→官方降级），命中取 Modrinth 数据
    void searchModrinthForDep(const QString& name, std::function<void(const QVariantMap&)> done);

    // Mod/Shader 搜索结果解析（字段与 QML 端约定一致，两路径共用）
    QVariantList parseSearchResponseItems(const QJsonArray& results) const;
};

} // namespace ShadowLauncher
