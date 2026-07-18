// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>

namespace ShadowLauncher {

class ModManager;

class ResourceBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadStateChanged)
    Q_PROPERTY(int dlProgress READ dlProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(int dlTotal READ dlTotal NOTIFY downloadProgressChanged)
    Q_PROPERTY(QString dlFile READ dlFile NOTIFY downloadProgressChanged)

public:
    explicit ResourceBackend(QObject* parent = nullptr);
    ~ResourceBackend() override;

    ModManager* modManager() const { return m_modMgr; }

    bool isDownloading() const { return m_downloading; }
    int dlProgress() const { return m_dlProgress; }
    int dlTotal() const { return m_dlTotal; }
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
    void modFileDownloadProgress(int downloadId, qint64 received, qint64 total);
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
    void onModFileDownloadProgress(int downloadId, qint64 received, qint64 total);
    void onModFileDownloadFinished(int downloadId, bool success, const QString& filePath, const QString& displayName);
    void onModFileDownloadFailed(int downloadId, const QString& errorDetail, const QString& displayName);

private:
    enum class SearchKind { Mod, Shader };
    ModManager* m_modMgr = nullptr;
    bool m_downloading = false;
    int m_dlProgress = 0;
    int m_dlTotal = 0;
    QString m_dlFile;
    QString m_minecraftDir;
    SearchKind m_searchKind = SearchKind::Mod;
};

} // namespace ShadowLauncher
