// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 Shadow
#pragma once

#include <QObject>
#include <QString>
#include <QVersionNumber>
#include <QTimer>
#include <QNetworkReply>
#include <QJsonObject>

namespace ShadowLauncher {

/// Coordinates the full update lifecycle: check → download (resumable) → install on next launch.
///
/// State machine:
///   IDLE → CHECKING → AVAILABLE → DOWNLOADING → READY → (relaunch)
///                                    ↓
///                                PAUSED (user quit mid-download)
///
/// State is persisted to `_update/state.json` so downloads survive restarts.
class UpdateManager : public QObject
{
    Q_OBJECT
public:
    enum State {
        Idle,
        Checking,
        Available,     // New version found, download not started
        Downloading,   // Download in progress
        Paused,        // Download interrupted (partial file exists)
        Ready,         // Download complete, pending install on next launch
    };
    Q_ENUM(State)

    explicit UpdateManager(QObject* parent = nullptr);

    void setCurrentVersion(const QString& v) { m_currentVersion = v; }
    void setQtVersion(const QString& v) { m_qtVersion = v; }
    void setResourceEpoch(int e) { m_resourceEpoch = e; }
    void setRepo(const QString& owner, const QString& repo);

    State state() const { return m_state; }
    bool isBusy() const { return m_state == Checking || m_state == Downloading; }

    // ── Actions ──

    /// Check for updates (silent — no user-visible signals besides state changes).
    /// Called on startup.
    void checkSilent();

    /// Check for updates (user-visible — emits toast-compatible signals).
    /// Called when user clicks "检查更新".
    void checkUserInitiated();

    /// Check if a ready-to-install update exists from a previous session.
    /// If true, the caller should launch SLUpdater and quit.
    bool hasPendingReady() const;

    /// Resume a paused download from last session (called after startup).
    void resumePausedDownload();

    // ── Persistence ──

    QString stateDir() const;

signals:
    void stateChanged(State newState);

    // User-visible feedback (for QML toasts / overlays)
    void toastMessage(const QString& message);
    void updateAvailableForInstall(const QString& version);
    void downloadProgress(qint64 received, qint64 total);
    void downloadComplete();       // Ready for next-launch install
    void downloadFailed(const QString& error);
    void checkCompleted(bool updateAvailable, const QString& version);

private:
    void setState(State s);
    void doCheck();
    void onCompatJsonReady(const QByteArray& body);
    void pickAssetAndDownload(bool forceFull);
    void startDownload();
    void resumeDownload(qint64 resumeFrom);
    void saveState();
    void onDownloadFinished(bool ok, const QString& error, int httpStatus = 0);

    State m_state = Idle;
    bool m_userInitiated = false;

    QString m_currentVersion;
    QString m_qtVersion;
    int m_resourceEpoch = 0;
    QString m_apiUrl;
    QString m_expectedSha256;
    QString m_releaseNotes;

    // Release metadata (from API, before compat check)
    QJsonObject m_releaseCache;

    // Download state (persisted)
    QString m_downloadVersion;      // e.g. "v0.2.2-beta"
    QString m_downloadUrl;
    QString m_downloadPath;
    QString m_downloadFileName;
    qint64 m_downloadTotal = 0;
    qint64 m_downloadReceived = 0;
    QNetworkReply* m_activeReply = nullptr;

    // Suppress state file saves during init
    bool m_initPhase = true;
};

} // namespace ShadowLauncher
