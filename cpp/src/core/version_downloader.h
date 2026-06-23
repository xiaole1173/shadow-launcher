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

    // BMCLAPI 协议合规相关
    QString healthCheckUrl;     // 用于镜像健康检测的探测 URL
    QString attribution;        // 使用协议标注文案（BMCLAPI/MCBBS 必须标注来源）
    bool isAvailable = true;    // 运行时可用状态

    static MirrorSource bmclapi();
    static MirrorSource mojang();
    static MirrorSource mcbbs();
    static QVector<MirrorSource> allMirrors();
};

// BMCLAPI 协议合规公告（下载页面必须展示）
constexpr const char* kBMCLAPIComplianceNotice =
    "📦 下载加速由 BMCLAPI / MCBBS 镜像提供\n"
    "文件归源站点 (Mojang/Fabric/Forge 等) 所有\n"
    "Shadow Launcher 不对镜像文件内容承担责任";

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

    // Per-category total bytes (pre-computed from task list)
    qint64 categoryTotalBytes(int cat) const { return m_categoryTotalBytes[cat]; }

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& url, const QString& fileName, qint64 received, qint64 total);

    /// Emitted during integrity verification phase only.
    /// Does NOT overlap with downloadProgress — verify has its own signal.
    void verifyProgressChanged(int checked, int total);

    void logMessage(const QString& msg);
    void downloadFinished(bool success, const QString& errorMsg);
    void stateChanged();

    /// Emitted when files fail to download (all mirrors exhausted).
    /// QML can use this to show a retry UI or display missing file lists.
    void downloadFailedFiles(const QStringList& failedFiles);

private slots:
    void onAllFinished(bool success, int failedCount,
                       const QStringList& failedFiles);

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

    // --- Mirror fallback ---
    void retryWithNextMirror();

    // --- Members ---
    MirrorSource m_mirror;
    QString m_minecraftDir;
    int m_maxWorkers = 32;

    // Mirror fallback: tracks current position in fallback chain
    QVector<MirrorSource> m_fallbackChain;
    int m_fallbackIndex = 0;
    static constexpr double kFallbackThreshold = 0.3;

    ParallelDownloader* m_downloader = nullptr;

    QAtomicInt m_completedFiles{0};
    QAtomicInt m_totalFiles{0};
    QAtomicInteger<qint64> m_totalBytes{0};
    QAtomicInteger<qint64> m_downloadedBytes{0};

    enum State { Idle, Running, Paused, Cancelled, Verifying, Done, Failed };
    State m_state = Idle;

    QString m_currentVersionId;

    // Cached for integrity verification
    QMap<QString, QJsonObject> m_assetObjects;
    QJsonObject m_currentVersionJson;
    QStringList m_taskDestPaths;

    // Per-category total bytes (pre-computed from all tasks)
    qint64 m_categoryTotalBytes[3] = {0, 0, 0};
};

} // namespace ShadowLauncher
