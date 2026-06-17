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

    bool isDownloading() const { return m_downloading; }
    int dlProgress() const { return m_dlProgress; }
    int dlTotal() const { return m_dlTotal; }
    QString dlFile() const { return m_dlFile; }

    // Slots
    Q_INVOKABLE QVariantList getPopularMods(const QString& loader);
    Q_INVOKABLE QVariantList getShaderList();
    Q_INVOKABLE void searchMods(const QString& query, const QString& loader = {});
    Q_INVOKABLE void downloadMod(const QString& slug, const QString& loader);
    Q_INVOKABLE void downloadShader(const QString& slug);
    Q_INVOKABLE void cancelDownload();

signals:
    void downloadProgressChanged(int completed, int total, const QString& fileName);
    void downloadStateChanged();
    void downloadFinished(const QString& slug, bool success, const QString& filePath);
    void searchResultsReady(const QVariantList& results);
    void logMessage(const QString& msg);

private slots:
    void onSearchCompleted(const QJsonArray& results, int totalHits);
    void onDownloadProgress(const QString& name, qint64 received, qint64 total);
    void onDownloadFinished(const QString& slug, bool success, const QString& filePath);

private:
    ModManager* m_modMgr = nullptr;
    bool m_downloading = false;
    int m_dlProgress = 0;
    int m_dlTotal = 0;
    QString m_dlFile;
    QString m_minecraftDir;
};

} // namespace ShadowLauncher
