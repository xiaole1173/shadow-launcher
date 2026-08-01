// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>

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
        int offset, int limit);
    Q_INVOKABLE QVariantMap getModCategories();
    Q_INVOKABLE void searchShadersEx(const QString& query, const QStringList& gameVersions,
        const QStringList& categories, const QStringList& performance,
        const QStringList& loader, int offset, int limit);
    Q_INVOKABLE void downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir = QString());
    Q_INVOKABLE void searchResourcepacks(const QString& query, const QString& gameVersion = {}, int offset = 0, const QStringList& categories = {});
    // 翻页预取（只预热缓存，不产生聚合信号，与真实搜索物理隔离）
    Q_INVOKABLE void prefetchModsEx(const QString& query, const QString& loader,
        const QString& category, const QStringList& gameVersions,
        int offset, int limit);
    Q_INVOKABLE void prefetchShadersEx(const QString& query, const QStringList& gameVersions,
        const QStringList& categories, int offset, int limit);
    Q_INVOKABLE void prefetchResourcepacks(const QString& query, const QString& gameVersion,
        const QStringList& categories, int offset, int limit);
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

signals:
    void downloadProgressChanged(int completed, int total, const QString& fileName);
    void downloadStateChanged();
    void downloadFinished(const QString& slug, bool success, const QString& filePath);
    void modFileDownloadStarted(int downloadId, const QString& fileName, qint64 fileSize, const QString& displayName);
    void modFileDownloadProgress(int downloadId, qint64 received, qint64 total, qint64 speed);
    void modFileDownloadFinished(int downloadId, bool success, const QString& filePath, const QString& displayName);
    void modFileDownloadFailed(int downloadId, const QString& errorDetail, const QString& displayName);
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
    QVariantList m_modMrResults, m_modCfResults;
    int m_modPending = 0;
    QVariantList m_shaderMrResults, m_shaderCfResults;
    int m_shaderPending = 0;
    QVariantList m_rpMrResults, m_rpCfResults;
    int m_rpPending = 0;
    // 本代是否已降级过官方（防镜像异常空时重复降级）
    bool m_mrFallbackUsed = false;
    bool m_shaderFallbackUsed = false;
    bool m_rpFallbackUsed = false;
    // 超时兜底后本代是否已发过结果（防止迟到响应二次 emit → 列表重复刷新/闪动）
    bool m_modEmitted = false;
    bool m_shaderEmitted = false;
    bool m_rpEmitted = false;
    void tryAggregateMod(int gen);
    void tryAggregateShader(int gen);
    void tryAggregateRp(int gen);
    bool m_downloading = false;
    int m_dlProgress = 0;
    int m_dlTotal = 0;
    int m_dlSpeed = 0;
    qint64 m_dlLastBytes = 0;
    qint64 m_dlLastMs = 0;
    QString m_dlFile;
    QString m_minecraftDir;
    SearchKind m_searchKind = SearchKind::Mod;

    // Mod/Shader 搜索结果解析（字段与 QML 端约定一致，两路径共用）
    QVariantList parseSearchResponseItems(const QJsonArray& results) const;
};

} // namespace ShadowLauncher
