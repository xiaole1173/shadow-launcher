#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>

#include "../utils/types.h"

namespace ShadowLauncher {

class VersionManager;
class VersionDownloader;
class VersionIsolation;

class VersionBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installStateChanged)
    Q_PROPERTY(int installProgress READ installProgress NOTIFY installProgressChanged)
    Q_PROPERTY(int installTotal READ installTotal NOTIFY installProgressChanged)
    Q_PROPERTY(QString installFile READ installFile NOTIFY installProgressChanged)
    Q_PROPERTY(QString installVersionId READ installVersionId NOTIFY installStateChanged)
    Q_PROPERTY(QString installPhase READ installPhase NOTIFY installPhaseChanged)
    Q_PROPERTY(QStringList versionIds READ versionIds NOTIFY versionListReady)
    Q_PROPERTY(QStringList installedIds READ installedIds NOTIFY installedVersionsChanged)
    Q_PROPERTY(QString selectedVersion READ selectedVersion NOTIFY selectedVersionChanged)

public:
    explicit VersionBackend(QObject* parent = nullptr);
    ~VersionBackend() override;

    bool isInstalling() const { return m_installing; }

    /// Return cached version list with id + type for QML
    QVariantList versionInfoList() const;
    /// Access to raw cached versions from VersionManager
    QVector<McVersion> cachedMcVersions() const;
    int installProgress() const { return m_installProgress; }
    int installTotal() const { return m_installTotal; }
    QString installFile() const { return m_installFile; }
    QString installVersionId() const { return m_installVersionId; }
    QString installPhase() const { return m_installPhase; }
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
    Q_INVOKABLE void pauseInstall();
    Q_INVOKABLE void resumeInstall();
    Q_INVOKABLE QString getVersionGameDir(const QString& versionId) const;

    // ── Version management operations ──
    Q_INVOKABLE void verifyVersion(const QString& versionId);
    Q_INVOKABLE bool renameVersion(const QString& oldId, const QString& newId);
    Q_INVOKABLE bool cloneVersion(const QString& sourceId, const QString& newId);
    Q_INVOKABLE QString copyVersionPath(const QString& versionId);

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
    void logMessage(const QString& msg);

    // ── Version management signals ──
    void verifyStarted();
    void verifyProgress(int checked, int total);
    void verifyFinished(bool allPassed);

private slots:
    void onVersionDownloadProgress(int cf, int tf, qint64 db, qint64 tb);
    void onVersionDownloadLog(const QString& msg);
    void onVersionDownloadFinished(bool success, const QString& error);

private:
    void updateInstalledList();
    void setInstalling(bool v);
    void setInstallPhase(const QString& phase);

    VersionManager* m_versionMgr = nullptr;
    VersionDownloader* m_downloader = nullptr;
    class VersionIsolation* m_isolation = nullptr;
    QString m_gameDir;

    QStringList m_versionIds;
    QStringList m_installedIds;
    QString m_selectedVersion;

    bool m_installing = false;
    bool m_initialFetchDone = false;
    int m_installProgress = 0;
    int m_installTotal = 0;
    QString m_installFile;
    qint64 m_installBytesDl = 0;
    qint64 m_installBytesTotal = 0;
    QString m_installPhase = "idle";
    QString m_installVersionId;
};

} // namespace ShadowLauncher
