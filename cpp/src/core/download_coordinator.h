#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>

namespace ShadowLauncher {

// ============================================================
// DownloadCoordinator — lightweight preflight: connectivity + bytes
// Delegates actual downloads to existing VersionDownloader/ModLoaderInstaller
// ============================================================

class DownloadCoordinator : public QObject {
    Q_OBJECT

public:
    struct Source {
        QString taskId;       // "mc_json", "forge", "fabric", etc.
        QString url;          // URL for HEAD test + size fetch
        qint64  totalBytes = 0;
    };

    explicit DownloadCoordinator(QObject* parent = nullptr);
    ~DownloadCoordinator() override;

    void addSource(const QString& taskId, const QString& url);
    void start();  // Run connectivity → fetch sizes → emit ready

    QVector<Source> sources() const { return m_sources; }
    qint64 totalBytes() const { return m_totalBytes; }

signals:
    void phaseChanged(const QString& phase);
    void connectivityFailed(const QString& taskId, const QString& reason);
    void ready(qint64 totalBytes);   // Preflight complete — launch downloads now

private slots:
    void onHeadReply();

private:
    QNetworkAccessManager* m_nam;
    QVector<Source> m_sources;
    qint64 m_totalBytes = 0;
    int m_pendingHeads = 0;

    void runHeads();
    void onAllHeadsDone();
};

} // namespace ShadowLauncher
