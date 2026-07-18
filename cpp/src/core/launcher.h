// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QJsonObject>

namespace ShadowLauncher {

// Forward declarations
class SettingsBackend;

class Launcher : public QObject {
    Q_OBJECT

public:
    explicit Launcher(QObject* parent = nullptr);
    ~Launcher() override;

    // ---- API ----
    void start(const QString& versionId, const QString& javaPath,
               int maxMemoryMB, const QString& jvmArgs = {}, const QString& gameArgs = {},
               bool highPerfGpu = false);
    void cancel();
    void killProcess();
    qint64 pid() const { return m_pid; }
    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }

    // ---- Configuration ----
    void setGameDir(const QString& dir) { m_gameDir = dir; }
    QString gameDir() const { return m_gameDir; }
    void setJvmArgs(const QString& args) { m_jvmArgs = args; }
    QString jvmArgs() const { return m_jvmArgs; }
    void setGameArgs(const QString& args) { m_gameArgs = args; }
    QString gameArgs() const { return m_gameArgs; }
    /// Set auto-language mode (0=off, 1=system locale, 2=IP region)
    void setAutoLangMode(int mode) { m_autoLangMode = mode; }

    void setAuthInfo(const QString& username, const QString& uuid, const QString& accessToken, bool isOnline) {
        m_authName = username;
        m_authUuid = uuid;
        m_authToken = accessToken;
        m_isOnline = isOnline;
    }

signals:
    void launchProgress(const QString& message);
    void launchFinished(bool success, const QString& errorMsg);
    void launchStarted();

private slots:
    void onProcessStarted();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    bool validateLaunch(const QString& versionId, const QString& javaPath, QString& errorMsg) const;
    void forceKill();
    QStringList buildArgs(const QString& versionId, int maxMemoryMB, const QJsonObject& versionJson) const;
    bool extractNatives(const QString& versionId, const QJsonObject& versionJson);
    void ensureOptionsTxt(const QString& versionId);
    static bool evaluateRules(const QJsonArray& rules);
    static bool evaluateRule(const QJsonObject& rule);
    void ensureLegacyAssets(const QString& assetIndexId);

    QProcess* m_process = nullptr;
    qint64 m_pid = 0;
    QString m_gameDir;
    QString m_currentVersionId;
    QString m_jvmArgs;
    QString m_gameArgs;
    bool m_highPerfGpu = false;
    QString m_authName;
    QString m_authUuid;
    QString m_authToken;
    bool m_isOnline = false;
    bool m_cancelling = false;
    int m_autoLangMode = 1;  // 0=off, 1=system locale, 2=IP region
};


} // namespace ShadowLauncher
