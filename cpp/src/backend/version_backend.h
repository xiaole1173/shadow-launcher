#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QPair>
#include <QThread>
#include <QAtomicInt>
#include <QMap>
#include <QVector>
#include <memory>

#include "../utils/types.h"

namespace ShadowLauncher {

class VersionManager;
class VersionDownloader;
class VersionIsolation;
class ModLoaderInstaller;

class VersionBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installStateChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY installStateChanged)
    Q_PROPERTY(int installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(int installTotal READ installTotal NOTIFY installProgressChanged)
    Q_PROPERTY(QString installFile READ installFile NOTIFY installProgressChanged)
    Q_PROPERTY(QString installVersionId READ installVersionId NOTIFY installStateChanged)
    Q_PROPERTY(QString installPhase READ installPhase NOTIFY installPhaseChanged)
    Q_PROPERTY(int verifyChecked READ verifyChecked NOTIFY verifyProgressChanged)
    Q_PROPERTY(int verifyTotal READ verifyTotal NOTIFY verifyProgressChanged)
    Q_PROPERTY(bool installPaused READ isInstallPaused NOTIFY installPausedChanged)
    Q_PROPERTY(QStringList versionIds READ versionIds NOTIFY versionListReady)
    Q_PROPERTY(QStringList installedIds READ installedIds NOTIFY installedVersionsChanged)
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY selectedVersionChanged)

public:
    explicit VersionBackend(QObject* parent = nullptr);
    ~VersionBackend() override;

    bool isInstalling() const { return m_installing || (m_activeCount > 0); }
    int activeCount() const { return m_activeCount; }

    /// Return cached version list with id + type for QML
    QVariantList versionInfoList() const;
    /// Access to raw cached versions from VersionManager
    QVector<McVersion> cachedMcVersions() const;
    int installProgress() const { return m_installProgress; }
    int installTotal() const { return m_installTotal; }
    QString installFile() const { return m_installFile; }
    QString installVersionId() const { return m_modLoaderInstallId.isEmpty() ? (m_activeIds.isEmpty() ? QString() : m_activeIds.first()) : m_modLoaderInstallId; }
    QString installPhase() const { return m_installPhase; }
    qint64 installSpeed() const { return m_installSpeed; }
    qint64 installBytesDownloaded() const { return m_installBytesDl; }
    qint64 installBytesTotal() const { return m_installBytesTotal; }
    int verifyChecked() const { return m_verifyChecked; }
    int verifyTotal() const { return m_verifyTotal; }
    bool isInstallPaused() const { return m_installPaused; }
    QStringList versionIds() const { return m_versionIds; }
    QStringList installedIds() const { return m_installedIds; }
    QString selectedVersion() const { return m_selectedVersion; }

    // Slots
    Q_INVOKABLE void setGameDir(const QString& dir);
    void setIsolation(class VersionIsolation* iso) { m_isolation = iso; }
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void refreshVersionList();
    Q_INVOKABLE void refreshInstalled();
    Q_INVOKABLE void installVersion(const QString& versionId, int sourceIndex = 0);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE void cancelInstall(const QString& versionId);
    Q_INVOKABLE void pauseInstall();
    Q_INVOKABLE void resumeInstall();
    Q_INVOKABLE void cancelQueuedDownload(const QString& versionId);
    Q_INVOKABLE QVariantList downloadQueue() const;
    Q_INVOKABLE QVariantList activeDownloads() const;
    Q_INVOKABLE QString getVersionGameDir(const QString& versionId) const;

    // ── Version management operations ──
    Q_INVOKABLE void verifyVersion(const QString& versionId);
    Q_INVOKABLE void cancelVerify();
    void cancelActiveDownload(const QString& versionId);  // Rollback sync state on fetch fail
    Q_INVOKABLE void cleanCorruptVersion(const QString& versionId);
    Q_INVOKABLE void repairVersion(const QString& versionId);
    Q_INVOKABLE bool renameVersion(const QString& oldId, const QString& newId);
    Q_INVOKABLE bool cloneVersion(const QString& sourceId, const QString& newId);
    Q_INVOKABLE QString copyVersionPath(const QString& versionId);

    // Mod loader installation
    Q_INVOKABLE void installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName);
    Q_INVOKABLE void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                       const QString& forgeVersion, const QString& installName);
    Q_INVOKABLE void cancelModLoaderInstall();
    Q_INVOKABLE bool isModLoaderInstalling() const;

signals:
    void versionListReady();
    void installedVersionsChanged();
    void selectedVersionChanged();
    void installStateChanged();
    void installProgressChanged();
    void installTotalChanged();
    void installFileProgress(const QString& name);
    void installBytesProgress(qint64 dl, qint64 total);
    void installPhaseChanged(const QString& phase);
    void installFinished(bool success);
    void installSpeedChanged(qint64 speed);
    void logMessage(const QString& msg);

    // ── Version management signals ──
    void verifyStarted();
    void verifyProgress(int checked, int total);
    void verifyProgressChanged(int checked, int total);
    void verifyFinished(bool allPassed);
    void verifyFailedFiles(const QStringList& failedFiles);
    void installPausedChanged(bool paused);
    void verifyCancelled();
    void downloadQueueChanged();

private slots:
    void onVersionDownloadProgress(int cf, int tf, qint64 db, qint64 tb);
    void onVersionDownloadLog(const QString& msg);
    void onVersionDownloadFinished(bool success, const QString& error);

private:
    void updateInstalledList();
    void setInstalling(bool v);
    void setInstallPhase(const QString& phase);

    VersionManager* m_versionMgr = nullptr;
    class VersionIsolation* m_isolation = nullptr;
    ModLoaderInstaller* m_mlInstaller = nullptr;
    QString m_gameDir;

    QStringList m_versionIds;
    QStringList m_installedIds;
    QString m_selectedVersion;

    // ── Concurrency: max 2 parallel MC version downloads ──
    int m_activeCount = 0;
    bool m_installing = false;
    QString m_modLoaderInstallId;             // track mod loader install name
    static constexpr int MAX_CONCURRENT = 2;
    QVector<QString> m_activeIds;  // active installing version IDs (ordered)

    // ── Per-download progress state ──
    struct DlState {
        int progress = 0;
        int total = 0;
        qint64 bytesDl = 0;
        qint64 bytesTotal = 0;
        qint64 speed = 0;
        QString file;
        QString phase = QStringLiteral("idle");
    };
    // Map versionId → per-download progress
    QMap<QString, DlState> m_dlStates;
    // Map versionId → active downloader
    QMap<QString, VersionDownloader*> m_downloaders;

    bool m_initialFetchDone = false;
    int m_installProgress = 0;
    int m_installTotal = 0;
    QString m_installFile;
    qint64 m_installBytesDl = 0;
    qint64 m_installBytesTotal = 0;
    qint64 m_installSpeed = 0;
    QString m_installPhase = "idle";
    int m_verifyChecked = 0;
    // Speed calculation
    QElapsedTimer m_speedTimer;
    qint64 m_lastSpeedBytes = 0;
    // Install queue
    QQueue<QPair<QString, int>> m_installQueue;
    int m_verifyTotal = 0;
    bool m_installPaused = false;

    // Helpers for multi-downloader
    VersionDownloader* primaryDownloader() const;
    QString primaryVersionId() const;
    void syncPrimaryProgress();
    void updateDownloadProgress(const QString& versionId, int cf, int tf, qint64 db, qint64 tb);
    void updateDownloadFile(const QString& versionId, const QString& fileName, qint64 received, qint64 total);
    void startNextFromQueue();

    // Background verify worker thread
    class VerifyWorker;
    QThread* m_verifyThread = nullptr;
    VerifyWorker* m_verifyWorker = nullptr;
    QStringList m_failedPathsCache;  // stored for cleanup after verify

    // Deprecated — no longer used (worker manages its own cancel)
    QAtomicInt m_verifyCancelled = 0;
};

// ============================================================
// VerifyWorker — background SHA1 verification
// ============================================================

struct VerifyItem {
    QString path;
    QString sha1;
    QString name;
};

class VersionBackend::VerifyWorker : public QObject {
    Q_OBJECT
public:
    explicit VerifyWorker(QObject* parent = nullptr) : QObject(parent) {}

    void setItems(const QVector<VerifyItem>& items) { m_items = items; }
    void cancel() { m_cancelled.storeRelease(1); }

public slots:
    void process();

signals:
    void progressChecked(int checked, int total);
    void cancelled(int checked, int total);
    void finished(bool allPassed, const QStringList& failedFiles,
                  const QStringList& failedPaths);

private:
    static QString sha1FileFast(const QString& filePath);

    QVector<VerifyItem> m_items;
    QAtomicInt m_cancelled = 0;
    int m_failed = 0;
    QStringList m_failedFiles;
    QStringList m_failedPaths;
};

} // namespace ShadowLauncher
