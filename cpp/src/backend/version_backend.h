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
    // Per-install step model (backward compat: returns primary install)
    Q_PROPERTY(QVariantList installSteps READ installSteps NOTIFY installStepsChanged)
    Q_PROPERTY(qreal installTotalProgress READ installTotalProgress NOTIFY installTotalProgressChanged)
    Q_PROPERTY(qreal installSmoothProgress READ installSmoothProgress NOTIFY installSmoothProgressChanged)
    Q_PROPERTY(int installRemainingSteps READ installRemainingSteps NOTIFY installStepsChanged)
    // Multi-card active installs
    Q_PROPERTY(QVariantList activeInstalls READ activeInstalls NOTIFY activeInstallsChanged)
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

    QVariantList versionInfoList() const;
    QVector<McVersion> cachedMcVersions() const;
    int installProgress() const { return m_installProgress; }
    int installTotal() const { return m_installTotal; }
    QString installFile() const { return m_installFile; }
    QString installVersionId() const { return m_modLoaderInstallId.isEmpty() ? (m_activeIds.isEmpty() ? QString() : m_activeIds.first()) : m_modLoaderInstallId; }
    QString installPhase() const { return m_installPhase; }
    // Backward compat — returns primary install's steps
    QVariantList installSteps() const { return m_installSteps; }
    qreal installTotalProgress() const { return m_installTotalProgress; }
    qreal installSmoothProgress() const { return m_installSmoothProgress; }
    int installRemainingSteps() const;
    // Multi-card
    QVariantList activeInstalls() const;
    qint64 installSpeed() const { return m_installSpeed; }
    qint64 installBytesDownloaded() const { return m_installBytesDl; }
    qint64 installBytesTotal() const { return m_installBytesTotal; }
    int verifyChecked() const { return m_verifyChecked; }
    int verifyTotal() const { return m_verifyTotal; }
    bool isInstallPaused() const { return m_installPaused; }
    QStringList versionIds() const { return m_versionIds; }
    QStringList installedIds() const { return m_installedIds; }
    QStringList activeVersionNames() const {
        QStringList names = m_installedIds;
        for (const QString& v : m_activeIds) {
            if (!names.contains(v)) names.append(v);
        }
        if (!m_modLoaderInstallId.isEmpty() && !names.contains(m_modLoaderInstallId))
            names.append(m_modLoaderInstallId);
        if (m_hasPendingLoader && !m_pendingLoaderName.isEmpty() && !names.contains(m_pendingLoaderName))
            names.append(m_pendingLoaderName);
        return names;
    }
    QString selectedVersion() const { return m_selectedVersion; }

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

    Q_INVOKABLE void verifyVersion(const QString& versionId);
    Q_INVOKABLE void cancelVerify();
    void cancelActiveDownload(const QString& versionId);
    Q_INVOKABLE void cleanCorruptVersion(const QString& versionId);
    Q_INVOKABLE void repairVersion(const QString& versionId);
    Q_INVOKABLE bool renameVersion(const QString& oldId, const QString& newId);
    Q_INVOKABLE bool cloneVersion(const QString& sourceId, const QString& newId);
    Q_INVOKABLE QString copyVersionPath(const QString& versionId);

    Q_INVOKABLE void installModLoader(const QString& mcVersion, const QString& loaderType,
                                       const QString& loaderVersion, const QString& installName);
    Q_INVOKABLE void installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                       const QString& forgeVersion, const QString& installName);
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
    void installProgressChanged();
    void installTotalChanged();
    void installFileProgress(const QString& name);
    void installBytesProgress(qint64 dl, qint64 total);
    void installPhaseChanged(const QString& phase);
    void installFinished(bool success);
    void installSpeedChanged(qint64 speed);
    void installStepsChanged();
    void installTotalProgressChanged();
    void installSmoothProgressChanged();
    void activeInstallsChanged();
    void logMessage(const QString& msg);

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

    int m_activeCount = 0;
    bool m_installing = false;
    QString m_modLoaderInstallId;
    static constexpr int MAX_CONCURRENT = 2;
    QVector<QString> m_activeIds;

    // Merged install: cumulative byte tracking per MC download step (0=json, 1=libraries, 2=assets)
    qint64 m_mergedMcStepDone[3] = {};
    qint64 m_mergedMcStepTotal[3] = {};
    // Merged install: global byte pool (cross-phase aggregation)
    qint64 m_mergedMcBytesDl = 0;    // MC version phase downloaded
    qint64 m_mergedMcBytesAll = 0;   // MC version phase total (from JSON)
    qint64 m_mergedMlBytesDl = 0;    // Mod loader phase downloaded (current file's received)
    qint64 m_mergedMlBytesAll = 0;   // Mod loader phase total (estimated)
    qint64 m_mergedMlBytesDone = 0;  // Sum of completed ML files
    qint64 m_mergedMlFileTotal = 0;  // Current ML file's total (for file-transition detection)

    struct DlState {
        int progress = 0;
        int total = 0;
        qint64 bytesDl = 0;
        qint64 bytesTotal = 0;
        qint64 speed = 0;
        QString file;
        QString phase = QStringLiteral("idle");
        // EWMA speed tracking (α=0.3)
        qint64 speedLastBytes = 0;
        qint64 speedLastTimeMs = 0;
    };
    QMap<QString, DlState> m_dlStates;
    QMap<QString, VersionDownloader*> m_downloaders;

    bool m_initialFetchDone = false;
    int m_installProgress = 0;
    int m_installTotal = 0;
    QString m_installFile;
    qint64 m_installBytesDl = 0;
    qint64 m_installBytesTotal = 0;
    qint64 m_installSpeed = 0;
    QString m_installPhase = "idle";

    // Per-install step model + extra cards (resource, mod)
    QVariantList m_installSteps;
    QMap<QString, QVariantMap> m_extraCards;  // cardId → {installId,displayName,type,totalProgress,speed,installPhase}
    bool m_installFailed = false;
    QString m_installError;

    // Pending loader install (waiting for vanilla MC to finish)
    QString m_pendingLoaderMc;
    QString m_pendingLoaderType;
    QString m_pendingLoaderVer;
    QString m_pendingLoaderName;
    bool m_hasPendingLoader = false;

    // Merged install: MC + loader in one card
    bool m_isMergedInstall = false;
    QString m_mergedMcVersion;
    QString m_mergedLoaderType;
    QString m_mergedLoaderVer;
    int m_mergedLoadedStep = 0;  // which of the 9 steps is currently active

    qreal m_installTotalProgress = 0.0;
    qreal m_installSmoothProgress = 0.0;

    // Throttle activeInstallsChanged emissions (avoid UI flicker from 100ms updates)
    QTimer m_activeInstallsThrottle;
    bool m_activeInstallsPending = false;

    void rebuildSteps(const QStringList& names, const QVector<qreal>& weights = {},
                      const QVector<bool>& showFlags = {});
    void updateStep(int index, const QString& status, int percentage, qint64 bytesRecv = 0, qint64 bytesTotal = 0);
    void emitActiveInstallsChanged();
    void computeTotalProgress();

    int m_verifyChecked = 0;
    int m_verifyTotal = 0;
    bool m_installPaused = false;
    // Global sliding speed window (for primary/backward-compat speed display)
    QList<QPair<qint64, qint64>> m_speedWindow;
    QQueue<QPair<QString, int>> m_installQueue;

    VersionDownloader* primaryDownloader() const;
    QString primaryVersionId() const;
    void syncPrimaryProgress();
    void updateDownloadProgress(const QString& versionId, int cf, int tf, qint64 db, qint64 tb);
    void updateDownloadFile(const QString& versionId, const QString& fileName, qint64 received, qint64 total);
    void startNextFromQueue();

    class VerifyWorker;
    QThread* m_verifyThread = nullptr;
    VerifyWorker* m_verifyWorker = nullptr;
    QStringList m_failedPathsCache;
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
