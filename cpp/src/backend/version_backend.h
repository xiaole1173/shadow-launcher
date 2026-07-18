// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QThread>
#include <QAtomicInt>
#include <QMap>
#include <QVector>
#include <QAbstractListModel>
#include <memory>

#include "../utils/types.h"
#include "../core/progress_tracker.h"

namespace ShadowLauncher {

// --- InstallCard: per-card data struct ---
struct InstallCard {
    QString iid;
    QString name;
    QString type;
    qreal progress = 0.0;
    qint64 speed = 0;
    QString phase;
    int remaining = 0;
    QVariantList steps;
    bool failed = false;
    QString error;
    bool totalProgressVisible = true;  // show total bar only during download phase
    bool hasUserDataImport = false;    // has pending user data to import after install
    bool canCancel = true;             // false during user data import phase
    qint64 importFailedAtMs = 0;        // timestamp when import failed (for 60s countdown)
};

// --- InstallSession: per-install isolated state (supports concurrent merged installs) ---
struct InstallSession {
    QVariantList steps;
    qreal totalProgress = 0.0;
    qreal smoothProgress = 0.0;
    bool isMerged = false;
    bool mcDownloadDone = false;      // MC download + verify phase complete
    bool loaderDownloadReady = false; // Loader file downloaded + verified
    QByteArray loaderDownloadData;    // Cached loader file bytes
    int loaderVerifyStep = -1;         // Step index for verify (5 for forge)
    int loaderStepIdx = -1;            // Active ML step index for byteProgress update
    QString mcVersion;
    QString loaderType;
    QString loaderVer;
    bool failed = false;
    bool hasPendingLoader = false;
    QString pendingLoaderName;
    QString pendingLoaderVer;
    qreal pendingLoaderWeight = 0.0;
    QString pendingLoaderMc;       // MC version waiting for pending loader
    QString pendingLoaderType;     // "optifine" / "forge" / etc for pending loader
    QString error;                 // Install error message on failure
    int loadedStep = 0;  // which step is currently active
    bool fabricApiPending = false; // Fabric API download in progress (parallel)
    bool loaderFinishedWaitingMC = false; // Loader done but MC still downloading (merged Fabric)
    bool optifineJarParallel = false; // OptiFine JAR downloading in parallel with MC
    bool optifineJarDone = false;     // OptiFine JAR download complete
    QByteArray optifineJarData;       // Cached OptiFine JAR from parallel download
    QString bmclType;                  // BMCLAPI type field for URL construction
    QString bmclPatch;                 // BMCLAPI patch field for URL construction
    
    // Per-install MC download byte tracking (was global singletons)
    qint64 mcStepDone[3] = {};
    qint64 mcStepTotal[3] = {};
    QSet<QString> mcFileAdded;
    qint64 mcBytesDl = 0;
    qint64 mcBytesAll = 0;
    
    // Per-install ML download byte tracking
    qint64 mlBytesDl = 0;
    qint64 mlBytesAll = 0;
    qint64 mlBytesDone = 0;
    
    // EWMA speed tracking for ML byte progress
    qint64 mlSpeedLastBytes = 0;
    qint64 mlSpeedLastTimeMs = 0;
    qint64 mlRawSpeed = 0;  // raw download speed from byteProgress signal
    
    // Current ML file total for file-transition detection
    qint64 mlFileTotal = 0;
    
    // Pending loader params (persisted for auto-start after MC completes)
    QString forgeInstallerSha1;
    QString fabricApiVersion;
    QString fabricApiUrl;
    QString fabricApiSavePath;
    QString fabricApiFinalPath;      // final mods/ path (API downloaded to temp, moved here on success)

    // User data import
    bool hasImportPending = false;
    QString importArchivePath;         // ZIP path for pending user data import
    qint64 importFailedAtMs = 0;        // timestamp when import failed
};

// --- InstallCardModel: QAbstractListModel with explicit role names ---
class InstallCardModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY generationChanged)
    Q_PROPERTY(int generation READ generation NOTIFY generationChanged)
public:
    enum CardRoles {
        IidRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        ProgressRole,
        SpeedRole,
        PhaseRole,
        RemainingRole,
        StepsRole,
        FailedRole,
        ErrorRole,
        TotalProgressVisibleRole,
        HasUserDataImportRole,
        CanCancelRole,
        ImportFailedAtMsRole
    };

    explicit InstallCardModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void rebuild(const QVector<InstallCard>& cards);
    int count() const { return m_cards.size(); }
    int generation() const { return m_generation; }

signals:
    void generationChanged();

private:
    QVector<InstallCard> m_cards;
    int m_generation = 0;
};

class VersionManager;
class VersionDownloader;
class VersionIsolation;
class ModLoaderInstaller;

class VersionBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installStateChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY installStateChanged)
    Q_PROPERTY(QString installVersionId READ installVersionId NOTIFY installStateChanged)
    Q_PROPERTY(QString installPhase READ installPhase NOTIFY installPhaseChanged)
    Q_PROPERTY(QString activeInstallId READ activeInstallId NOTIFY installStateChanged)
    Q_PROPERTY(QObject* installCardsModel READ installCardsModel CONSTANT)
    Q_PROPERTY(int verifyChecked READ verifyChecked NOTIFY verifyProgressChanged)
    Q_PROPERTY(int verifyTotal READ verifyTotal NOTIFY verifyProgressChanged)
    Q_PROPERTY(QStringList versionIds READ versionIds NOTIFY versionListReady)
    Q_PROPERTY(QStringList installedIds READ installedIds NOTIFY installedVersionsChanged)
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY selectedVersionChanged)

public:
    explicit VersionBackend(QObject* parent = nullptr);
    ~VersionBackend() override;

    bool isInstalling() const { return m_installing || (m_activeCount > 0); }
    int activeCount() const { return m_activeCount; }

    QVariantList versionInfoList() const;
    QVector<McVersion> cachedMcVersions() const;
    QString installVersionId() const { return m_modLoaderInstallId.isEmpty() ? (m_activeIds.isEmpty() ? QString() : m_activeIds.first()) : m_modLoaderInstallId; }
    QString installPhase() const { return m_installPhase; }
    QString activeInstallId() const { return m_activeIds.isEmpty() ? QString() : m_activeIds.first(); }
    int installRemainingSteps(const QString& sessionId) const;
    QObject* installCardsModel() const { return m_installCardsModel; }
    int verifyChecked() const { return m_verifyChecked; }
    int verifyTotal() const { return m_verifyTotal; }
    bool isVerifyRunning() const { return m_verifyRunning.loadRelaxed() != 0; }
    bool isRepairRunning() const { return m_repairRunning.loadRelaxed() != 0; }
    QStringList versionIds() const { return m_versionIds; }
    QStringList installedIds() const { return m_installedIds; }
    QStringList activeVersionNames() const {
        QStringList names = m_installedIds;
        for (const QString& v : m_activeIds) {
            if (!names.contains(v)) names.append(v);
        }
        if (!m_modLoaderInstallId.isEmpty() && !names.contains(m_modLoaderInstallId))
            names.append(m_modLoaderInstallId);
        for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
            if (it.value().hasPendingLoader && !it.value().pendingLoaderName.isEmpty()
                && !names.contains(it.value().pendingLoaderName))
                names.append(it.value().pendingLoaderName);
        }
        return names;
    }
    QString selectedVersion() const { return m_selectedVersion; }

    Q_INVOKABLE void setGameDir(const QString& dir);
    void setIsolation(class VersionIsolation* iso) { m_isolation = iso; }

    // Auto-language settings
    void setAutoLangMode(int mode) { m_autoLangMode = mode; }
    void setDetectedRegion(const QString& region) { m_detectedRegion = region; }

    VersionManager* versionManager() const { return m_versionMgr; }
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void refreshVersionList();
    Q_INVOKABLE void refreshInstalled();
    Q_INVOKABLE void installVersion(const QString& versionId);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE void cancelCurrentInstall();
    Q_INVOKABLE void cancelVersionInstall(const QString& versionId);
    Q_INVOKABLE void cancelQueuedDownload(const QString& versionId);
    void prefetchVersionJson(const QString& versionId);
    Q_INVOKABLE QVariantList downloadQueue() const;
    Q_INVOKABLE QVariantList activeDownloads() const;
    Q_INVOKABLE QString getVersionGameDir(const QString& versionId) const;

    Q_INVOKABLE void verifyVersion(const QString& versionId);
    Q_INVOKABLE void cancelVerify();
    void cancelActiveDownload(const QString& versionId);
    // On cancel/fail: delete the entire version folder,
    // with a safety guard — skip if saves/ already exists (user has played).
    void cleanupCanceledVersion(const QString& versionId, const QString& gameDir);
    Q_INVOKABLE void cleanCorruptVersion(const QString& versionId);
    Q_INVOKABLE void repairVersion(const QString& versionId);
    Q_INVOKABLE bool renameVersion(const QString& oldId, const QString& newId);
    Q_INVOKABLE bool cloneVersion(const QString& sourceId, const QString& newId);
    Q_INVOKABLE QString copyVersionPath(const QString& versionId);

    // User data import integration
    Q_INVOKABLE void setPendingUserDataImport(const QString& installId, const QString& archivePath);
    Q_INVOKABLE void cancelPendingUserDataImport(const QString& installId);
    Q_INVOKABLE void dismissCard(const QString& installId);  // remove card from progress page

    Q_INVOKABLE void installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName,
                                       const QString& fabricApiVersion = QString(),
                                       const QString& fabricApiUrl = QString(),
                                       const QString& fabricApiSavePath = QString(),
                                       const QString& forgeInstallerSha1 = QString());
    Q_INVOKABLE void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                       const QString& forgeVersion, const QString& installName,
                                       const QString& bmclType = QString(), const QString& bmclPatch = QString());
    Q_INVOKABLE void installOptifineJar(const QString& mcVersion, const QString& optifineVersion,
                                         const QString& bmclType = QString(), const QString& bmclPatch = QString());
    void finishOptifineMerged(const QString& mcVersion, const QString& installName);
    void delegateOptifineInstall(const QString& mcVersion, const QString& installName,
                                  const QByteArray& jarData);
    void startOptifineJarParallel(const QString& installName, const QString& mcVersion,
                                   const QString& optifineVersion, const QString& bmclType, const QString& bmclPatch);
    void onParallelOptifineDone(const QString& installName, const QByteArray& jarData);
    Q_INVOKABLE void cancelModLoaderInstall();
    Q_INVOKABLE bool isModLoaderInstalling() const;

    // Resource / Mod download cards
    Q_INVOKABLE void addResourceCard(const QString& cardId, const QString& displayName);
    Q_INVOKABLE void updateResourceCard(const QString& cardId, qreal progress, const QString& status);
    Q_INVOKABLE void removeResourceCard(const QString& cardId);

signals:
    void versionListReady();
    void installedVersionsChanged();
    void selectedVersionChanged();
    void installStateChanged();
    void installPhaseChanged(const QString& phase);
    void installFinished(bool success);
    void installComplete(const QString& installId);
    void logMessage(const QString& msg);
    void mcJsonReady(const QString& versionId);

    void verifyStarted();
    void verifyProgress(int checked, int total);
    void verifyProgressChanged(int checked, int total);
    void verifyFinished(bool allPassed);
    void verifyFailedFiles(const QStringList& failedFiles);
    void verifyCancelled();
    void repairRunningChanged();
    void downloadQueueChanged();
    void downloadQueueFull(const QString& displayName);

private slots:
    void onVersionDownloadLog(const QString& msg);
    void onVersionDownloadFinished(bool success, const QString& error);

private:
    void proceedToLoaderInstall(const QString& installId);
    void updateInstalledList();
    void setInstalling(bool v);
    void finishInstall(const QString& installName);
    void setInstallPhase(const QString& phase);

    VersionManager* m_versionMgr = nullptr;
    class VersionIsolation* m_isolation = nullptr;
    int m_autoLangMode = 1;
    QString m_detectedRegion;
    ModLoaderInstaller* m_mlInstaller = nullptr;
    QString m_gameDir;

    QStringList m_versionIds;
    QStringList m_installedIds;
    QString m_selectedVersion;
    QMap<QString, QByteArray> m_prefetchedJson;  // version JSON prefetch cache

    int m_activeCount = 0;
    bool m_installing = false;
    QString m_modLoaderInstallId;
    static constexpr int MAX_CONCURRENT = 5;
    QVector<QString> m_activeIds;
    QSet<QString> m_userCancelledIds;  // versions cancelled by user (suppress error in finish handler)

    struct DlState {
        int progress = 0;            // download file count (cf from progressChanged)
        int total = 0;               // download file total (tf from progressChanged)
        int verifyChecked = 0;       // verify phase: items checked (independent from progress/total)
        int verifyTotal = 0;         // verify phase: total items to check
        qint64 bytesDl = 0;
        qint64 bytesTotal = 0;
        qint64 speed = 0;
        QString file;
        QString phase = QStringLiteral("idle");
        // Sliding-window speed tracking (3s window)
        static constexpr qint64 kSpeedWindowMs = 3000;
        struct SpeedSample { qint64 timeMs; qint64 bytes; };
        QVector<SpeedSample> speedWindow;
        qint64 speedLastTimeMs = 0;
        // Per-category byte tracking (0=versions, 1=libraries, 2=assets)
        qint64 catBytesDl[3] = {};
        qint64 catBytesTotal[3] = {};
        qint64 catBytesDoneBase[3] = {};
        int catFilesDone[3] = {};    // per-category completed file count
        int catFilesTotal[3] = {};   // per-category total file count
        bool catsFullyDone = false;  // set by fileProgress when all categories complete
        bool downloadsDone = false;  // sticky: set once when downloads finish, stays true through verify
    };
    QMap<QString, DlState> m_dlStates;
    QMap<QString, VersionDownloader*> m_downloaders;

    bool m_initialFetchDone = false;
    qint64 m_installBytesDl = 0;
    qint64 m_installBytesTotal = 0;
    QString m_installPhase = "idle";

    // Unified speed/progress tracker (replaces per-session EWMA instances)
    ProgressTracker *m_progressTracker = nullptr;

    // Per-install state (keyed by installId, supports concurrent merged installs)
    QMap<QString, InstallSession> m_sessions;
    QMap<QString, QString> m_pendingImports;  // installId → archivePath
    QMap<QString, QVariantMap> m_extraCards;

    InstallSession& session(const QString& installId);
    InstallSession& activeSession();

    // Throttle card rebuilds (300ms interval to avoid flicker)
    QTimer m_cardsRebuildThrottle;
    bool m_cardsRebuildPending = false;
    QElapsedTimer m_cardsTimer;

    InstallCardModel* m_installCardsModel = nullptr;

    void rebuildSteps(const QString& installId, const QStringList& names, const QVector<qreal>& weights = {},
                      const QVector<bool>& showFlags = {});
    void updateStep(const QString& installId, int index, const QString& status, int percentage, qint64 bytesRecv = 0, qint64 bytesTotal = 0);
    void showStep(const QString& installId, int index);  // Make a hidden step visible and active
    void hideStep(const QString& installId, int index);   // Hide a step
    void rebuildInstallCards();
    void doRebuildInstallCards();
    void activateVerifyOnDownloadsDone(const QString& versionId);

    // User data import step
    void startUserDataImport(const QString& installId);

    int m_verifyChecked = 0;
    int m_verifyTotal = 0;
    QQueue<QString> m_installQueue;

    VersionDownloader* primaryDownloader() const;
    QString primaryVersionId() const;
    void syncPrimaryProgress();
    void updateDownloadProgress(const QString& versionId, int cf, int tf, qint64 db, qint64 tb);
    void updateDownloadFile(const QString& versionId, const QString& url, const QString& fileName, qint64 received, qint64 total, const QString& savePath = QString());
    void startNextFromQueue();

    class VerifyWorker;
    QThread* m_verifyThread = nullptr;
    VerifyWorker* m_verifyWorker = nullptr;
    QStringList m_failedPathsCache;
    QAtomicInt m_verifyRunning = 0;
    QAtomicInt m_repairRunning = 0;
    QAtomicInt m_verifyCancelled = 0;
};

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
