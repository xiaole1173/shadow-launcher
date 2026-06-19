#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

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

    void getModVersions(
        const QString& slug,
        const QStringList& loaders = {},
        const QStringList& gameVersions = {}
    );

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

    // Batch-fetch project game_versions for search result cards
    void fetchResourcepackVersions(const QStringList& slugs);

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

    void downloadProgress(const QString& name, qint64 received, qint64 total);
    void downloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackSearchCompleted(const QJsonArray& results, int totalHits);
    void resourcepackSearchFailed(const QString& error);
    void resourcepackDownloadFinished(const QString& slug, bool success, const QString& filePath);
    void resourcepackVersionsLoaded(const QVariantMap& slugToVersions);  // slug → QStringList (major versions)
    void resourcepackVersionsPartial(const QString& slug, const QStringList& versions, const QVariantMap& details);  // incremental: per slug + version details
    void resourcepackVersionsProgress(int done, int total);  // 0..total
    void logMessage(const QString& msg);

    void busyChanged();

private slots:
    void onVersionsForDownload(const QString& slug, const QJsonArray& files);

private:
    bool m_busy = false;
    void setBusy(bool busy);
    QJsonArray parseSearchResponse(const QByteArray& data, int& outTotalHits);
    QJsonArray parseVersionsResponse(const QByteArray& data);
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
};

} // namespace ShadowLauncher
