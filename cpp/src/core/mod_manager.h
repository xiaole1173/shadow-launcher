// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

class QNetworkReply;
class QFile;

namespace ShadowLauncher {

// --- Data structures ---

struct ModInfo {
    QString slug;
    QString title;
    QString description;
    QString iconUrl;
    QString author;
    int downloads = 0;
    QStringList categories;
    QStringList gameVersions;
    QStringList loaders;
    QString latestVersion;
};

struct ModFile {
    QString name;
    QString url;
    QString filename;
    qint64 size = 0;
    QString sha1;
    QStringList gameVersions;
    QStringList loaders;
    bool isPrimary = false;
};

struct DependencyInfo {
    QString projectId;      // Modrinth project ID (e.g. "P7dR8mSH")
    QString slug;           // resolved slug
    QString title;          // resolved title
    QString dependencyType; // "required" | "optional" | "incompatible" | "embedded"
};

struct ModSearchResult {
    QVector<ModInfo> mods;
    int total = 0;
    int offset = 0;
    int limit = 0;
};

// --- Resource pack specific ---

struct ResourcepackInfo {
    QString slug;
    QString title;
    QString description;
    QString iconUrl;
    int downloads = 0;
    QStringList categories;
    QStringList gameVersions;
};

// --- Main class ---

class ModManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    explicit ModManager(QObject* parent = nullptr);

    // Modrinth API
    void searchModrinth(
        const QString& query,
        const QStringList& categories = {},
        const QStringList& gameVersions = {},
        const QStringList& loaders = {},
        int offset = 0,
        int limit = 20,
        const QString& sort = "downloads"
    );

    // Search with raw facets (e.g., "project_type:mod", "project_type:shader")
    void searchModrinthProjects(
        const QString& query,
        const QStringList& facets = {},
        const QStringList& gameVersions = {},
        const QStringList& loaders = {},
        int offset = 0,
        int limit = 20,
        const QString& sort = "downloads"
    );

    void getModVersions(
        const QString& slug,
        const QStringList& loaders = {},
        const QStringList& gameVersions = {}
    );

    // Fetch dependency info for a mod (required + optional mods)
    Q_INVOKABLE void getModDependencies(const QString& slug, const QString& gameVersion = {});

    // Download a specific mod file to user-chosen path
    Q_INVOKABLE int downloadModFile(const QString& url, const QString& savePath, const QString& displayName,
                                    qint64 expectedSize, const QString& sha1, qint64 receivedOffset = 0, int resumeId = -1);
    Q_INVOKABLE void cancelModFileDownload(int downloadId);
    Q_INVOKABLE void pauseModFileDownload(int downloadId);
    Q_INVOKABLE void resumeModFileDownload(int downloadId);
    Q_INVOKABLE void retryModFileDownload(int downloadId);

    void downloadMod(
        const QString& slug,
        const QString& gameVersion,
        const QString& loader,
        const QString& minecraftDir,
        const QString& targetDir = "mods"
    );

    // Resource packs
    void searchResourcepacks(
        const QString& query,
        const QStringList& gameVersions = {},
        const QStringList& categories = {},
        int offset = 0,
        int limit = 20
    );

    void downloadResourcepack(
        const QString& slug,
        const QString& gameVersion,
        const QString& minecraftDir
    );

    // Shader packs — download (reuses Modrinth version fetch + MCIM CDN)
    void downloadShader(
        const QString& slug,
        const QString& gameVersion,
        const QString& minecraftDir
    );

    // Batch-fetch project game_versions for search result cards
    void fetchResourcepackVersions(const QStringList& slugs);
    void fetchModVersions(const QStringList& slugs);
    void fetchShaderVersions(const QStringList& slugs);

    // Popular mods (offline data)
    static QMap<QString, QJsonObject> getPopularMods(const QString& loader);
    static QMap<QString, QJsonObject> getShaderList();
    static QMap<QString, QString> getCategories();

    void cancel();
    bool isBusy() const;

signals:
    void searchCompleted(const QJsonArray& results, int totalHits);
    void searchFailed(const QString& error);

    void versionsFetched(const QString& slug, const QJsonArray& files);
    void versionsFailed(const QString& slug, const QString& error);
    void dependenciesResolved(const QString& slug, const QJsonArray& dependencies);

    void downloadProgress(const QString& name, qint64 received, qint64 total);
    void downloadFinished(const QString& slug, bool success, const QString& filePath);
    void modDownloadStarted(const QString& slug);
    void shaderSearchCompleted(const QJsonArray& results, int totalHits);
    void shaderSearchFailed(const QString& error);
    void shaderDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackSearchCompleted(const QJsonArray& results, int totalHits);
    void resourcepackSearchFailed(const QString& error);
    void resourcepackDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackVersionsLoaded(const QVariantMap& slugToVersions);  // slug → QStringList (major versions)
    void resourcepackVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);  // incremental: per slug + version details
    void resourcepackVersionsProgress(int done, int total);  // 0..total

    // Mod & Shader version detail signals
    void modVersionsLoaded(const QVariantMap& slugToVersions);
    void modVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void modVersionsProgress(int done, int total);

    // Mod file download (user-chosen save path)
    void modFileDownloadStarted(int downloadId, const QString& fileName, qint64 fileSize, const QString& displayName);
    void modFileDownloadProgress(int downloadId, qint64 received, qint64 total);
    void modFileDownloadFinished(int downloadId, bool success, const QString& filePath, const QString& displayName);
    void modFileDownloadFailed(int downloadId, const QString& errorDetail, const QString& displayName);

    void shaderVersionsLoaded(const QVariantMap& slugToVersions);
    void shaderVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);
    void shaderVersionsProgress(int done, int total);

    void logMessage(const QString& msg);

    void busyChanged();

private slots:
    void onVersionsForDownload(const QString& slug, const QJsonArray& files);

public:
    // Exposed for ResourceBackend to use with custom search URLs
    QJsonArray parseSearchResponse(const QByteArray& data, int& outTotalHits);
    QJsonArray parseVersionsResponse(const QByteArray& data);
    void setBusy(bool busy);

private:
    bool m_busy = false;
    QString buildSearchUrl(
        const QString& query, const QStringList& categories,
        const QStringList& gameVersions, const QStringList& loaders,
        int offset, int limit, const QString& sort) const;
    static QString formatFileSize(qint64 bytes);

    struct PendingDownload {
        QString slug;
        QString gameVersion;
        QString loader;
        QString targetDir;
        bool active = false;
    };
    PendingDownload m_pendingDownload;
    QString m_minecraftDir;
    int m_lastTotalHits = 0;

    // Mod file download tracking
    struct ActiveModDownload {
        int id = 0;
        QString url;
        QString savePath;
        QString displayName;
        qint64 expectedSize = 0;
        QString sha1;
        qint64 received = 0;
        bool cancelled = false;
        bool paused = false;
        bool finished = false;
        bool failed = false;
        QString errorDetail;
        QNetworkReply* reply = nullptr;
        QFile* tmpFile = nullptr;
        QString tmpPath;
    };
    QMap<int, ActiveModDownload> m_activeModDownloads;
    int m_nextModDownloadId = 1;
};

} // namespace ShadowLauncher
