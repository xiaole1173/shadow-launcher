#pragma once

#include <QObject>
#include <QVector>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QMap>
#include <QStringList>

#include "utils/types.h"

namespace ShadowLauncher {

class ParallelDownloader : public QObject {
    Q_OBJECT
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(int completedFiles READ completedFiles NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 downloadedBytes READ downloadedBytes NOTIFY progressChanged)
    Q_PROPERTY(QString state READ stateStr NOTIFY stateChanged)

public:
    explicit ParallelDownloader(int maxWorkers = 8, QObject* parent = nullptr);
    ~ParallelDownloader() override;

    // --- Task management ---
    void addTask(const DownloadTask& task);
    void addTasks(const QVector<DownloadTask>& tasks);

    // --- Lifecycle ---
    void start();
    void pause();
    void resume();
    void cancel();

    // --- State queries ---
    int totalFiles() const;
    int completedFiles() const;
    qint64 totalBytes() const;
    qint64 downloadedBytes() const;
    QString stateStr() const;
    bool isRunning() const;

signals:
    void progressChanged(int completedFiles, int totalFiles,
                         qint64 downloadedBytes, qint64 totalBytes);
    void fileProgress(const QString& fileName, qint64 received, qint64 total);
    void fileCompleted(const QString& fileName, bool success);
    void allFinished(bool success, int failedCount);
    void logMessage(const QString& msg);
    void stateChanged();

private:
    // --- Worker entry point (runs in dedicated threads) ---
    void runWorker();

    // --- Single-file download helpers ---
    bool downloadSingleFile(const DownloadTask& task);
    bool downloadSingleUrl(const QString& url, const QString& destPath,
                           qint64& bytesReceived);

    // --- Mirror racing: first-2-parallel ---
    bool downloadMirrorRace(const QStringList& urls,
                            const QString& destPath,
                            const QString& sha1);

    // --- SHA1 helpers ---
    static QString sha1Hex(const QString& filePath);
    static bool verifySha1File(const QString& filePath,
                               const QString& expected);
    static QString formatSize(qint64 bytes);

    // --- Internal state ---
    QVector<DownloadTask> m_tasks;
    int m_maxWorkers;

    // Atomic counters (thread-safe, no mutex needed)
    QAtomicInt m_completedFiles{0};
    QAtomicInt m_totalBytes{0};
    QAtomicInt m_downloadedBytes{0};
    QAtomicInt m_failedCount{0};
    QAtomicInt m_activeWorkers{0};
    QAtomicInt m_nextTask{0};

    enum State { Idle, Running, Paused, Cancelled, Done };
    State m_state = Idle;

    QMutex m_mutex;
    QWaitCondition m_pauseCond;
    bool m_cancelled = false;
    bool m_paused = false;

    // Worker thread handles (for join / cleanup)
    QList<QThread*> m_workers;

    // Mirror registry: url → fallback mirrors
    QMap<QString, QStringList> m_mirrors;
};

} // namespace ShadowLauncher
