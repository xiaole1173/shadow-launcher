#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>
#include <QAtomicInt>
#include "utils/types.h"
#include "parallel_downloader.h"

namespace ShadowLauncher {

// ============================================================
// Mirror source configuration
// ============================================================

struct MirrorSource {
    QString name;
    QString desc;
    QString manifestUrl;
    QString versionMetaHost;
    QString resourceBase;
    QString libraryBase;
    QString jarHost;
    bool isDefault = false;

    static MirrorSource bmclapi();
    static MirrorSource mojang();
    static MirrorSource mcbbs();
    static QVector<MirrorSource> allMirrors();
};

// ============================================================
// VersionDownloader — Minecraft version install pipeline
// ============================================================

class VersionDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(int completedFiles READ completedFiles NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(QString state READ stateStr NOTIFY stateChanged)

public:
    explicit VersionDownloader(QObject* parent = nullptr);
    ~VersionDownloader() override;

    // Configuration
    void setMirror(const MirrorSource& mirror);
    void setMinecraftDir(const QString& dir);
    void setMaxWorkers(int workers);

    // Main entry: download a Minecraft version
    void downloadVersion(const QJsonObject& versionJson, const QString& versionId);

    // Lifecycle
    void pause();
    void resume();
    void cancel();

    // State queries
    int completedFiles() const;
    int totalFiles() const;
    qint64 downloadedBytes() const;
    qint64 totalBytes() const;
    QString stateStr() const;
    bool isRunning() const;

    // Integrity verification (public for backend use)
    QStringList verifyIntegrity(const QJsonObject& versionJson, const QString& versionId);

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& fileName, qint64 received, qint64 total);

    /// Emitted during integrity verification phase only.
    /// Does NOT overlap with downloadProgress — verify has its own signal.
    void verifyProgressChanged(int checked, int total);

    void logMessage(const QString& msg);
    void downloadFinished(bool success, const QString& errorMsg);
    void stateChanged();

private slots:
    void onAllFinished(bool success, int failedCount);

private:
    // --- Pipeline steps ---
    bool downloadAssetIndex(const QJsonObject& assetIdx);
    QMap<QString, QJsonObject> parseAssetIndex(const QJsonObject& assetIdx);
    void collectTasks(const QJsonObject& versionJson, const QString& versionId,
                      const QMap<QString, QJsonObject>& assetObjects,
                      QVector<DownloadTask>& tasks);
    bool shouldDownloadLibrary(const QJsonObject& lib) const;
    void addLibraryTasks(const QJsonObject& lib, QVector<DownloadTask>& tasks);
    QString buildMirrorUrl(const QString& originalUrl, const QString& kind) const;

    // --- Verify phase (SHA1 scan) ---
    QStringList verifyAllFiles(const QJsonObject& versionJson, const QString& versionId);

    // --- Helpers ---
    static bool verifySha1(const QString& filePath, const QString& expected);
    static QString formatSize(qint64 bytes);
    void emitProgress(const QString& name);

    // --- Members ---
    MirrorSource m_mirror;
    QString m_minecraftDir;
    int m_maxWorkers = 32;

    ParallelDownloader* m_downloader = nullptr;

    QAtomicInt m_completedFiles{0};
    QAtomicInt m_totalFiles{0};
    QAtomicInt m_totalBytes{0};
    QAtomicInt m_downloadedBytes{0};

    enum State { Idle, Running, Paused, Cancelled, Verifying, Done, Failed };
    State m_state = Idle;

    QString m_currentVersionId;

    // Cached for integrity verification
    QMap<QString, QJsonObject> m_assetObjects;
    QJsonObject m_currentVersionJson;
    QStringList m_taskDestPaths;
};

} // namespace ShadowLauncher
