#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>

namespace ShadowLauncher {

class VersionManager;
class VersionDownloader;

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
    int installProgress() const { return m_installProgress; }
    int installTotal() const { return m_installTotal; }
    QString installFile() const { return m_installFile; }
    QString installVersionId() const { return m_installVersionId; }
    QString installPhase() const { return m_installPhase; }
    QStringList versionIds() const { return m_versionIds; }
    QStringList installedIds() const { return m_installedIds; }
    QString selectedVersion() const { return m_selectedVersion; }

    // Slots
    Q_INVOKABLE void setSelectedVersion(const QString& versionId);
    Q_INVOKABLE void refreshVersionList();
    Q_INVOKABLE void refreshInstalled();
    Q_INVOKABLE void installVersion(const QString& versionId, int sourceIndex = 0);
    Q_INVOKABLE void cancelInstall();
    Q_INVOKABLE void pauseInstall();
    Q_INVOKABLE void resumeInstall();

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

    QStringList m_versionIds;
    QStringList m_installedIds;
    QString m_selectedVersion;

    bool m_installing = false;
    int m_installProgress = 0;
    int m_installTotal = 0;
    QString m_installFile;
    qint64 m_installBytesDl = 0;
    qint64 m_installBytesTotal = 0;
    QString m_installPhase = "idle";
    QString m_installVersionId;
};

} // namespace ShadowLauncher
