// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QJsonObject>
#include <QFile>

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
               bool highPerfGpu = false,
               const QString& resolvedJsonPath = QString(),
               const QString& resolvedJarPath = QString());
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
    /// Set detected region (ISO 3166-1 alpha-2) for mode=2
    void setDetectedRegion(const QString& region) { m_detectedRegion = region; }
    /// Set the version-specific game directory (respects isolation mode)
    void setVersionGameDir(const QString& dir) { m_versionGameDir = dir; }
    /// Set desired Minecraft window resolution (default: 854x480)
    void setResolution(int width, int height) { m_resWidth = width; m_resHeight = height; }

    /// Last N lines of raw process output (for crash analysis).
    QStringList recentOutput(int maxLines = 300) const;
    /// Path of the full JVM output log (all stdout+stderr lines, overwritten per launch).
    QString jvmFullLogPath() const { return m_jvmFullLogPath; }

    void setAuthInfo(const QString& username, const QString& uuid, const QString& accessToken, bool isOnline) {
        m_authName = username;
        m_authUuid = uuid;
        m_authToken = accessToken;
        m_isOnline = isOnline;
    }

    /// 生成启动脚本（.bat 文本，脱机启动/排障用，2026-08-07）——复用 buildArgs 完整参数组装
    QString buildLaunchScript(const QString& versionId, const QString& javaPath,
                              int maxMemoryMB, const QString& jvmArgs, const QString& gameArgs,
                              bool highPerfGpu);

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

    // Filter routine MC INFO/Trace/DEBUG output (returns true = drop)
    bool isMcOutputNoise(const QString& line) const;

private:
    bool validateLaunch(const QString& versionId, const QString& javaPath, QString& errorMsg) const;
    void forceKill();
    QStringList buildArgs(const QString& versionId, int maxMemoryMB, const QJsonObject& versionJson) const;
    bool extractNatives(const QString& versionId, const QJsonObject& versionJson);
    void ensureOptionsTxt();
    static bool evaluateRules(const QJsonArray& rules);
    static bool evaluateRule(const QJsonObject& rule);
    /// 在版本目录中查找有效的版本 JSON（支持目录名≠文件名）
    static QString findVersionJson(const QString& verDir, const QString& dirName);
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
    QStringList m_outputRing;  // ring buffer of raw stdout/stderr lines (for crash analysis)
    QString m_jvmFullLogPath;  // full JVM output log path ("" if not opened)
    QFile m_jvmFullLog;        // full JVM output log file handle
    QString m_detectedRegion;
    QString m_versionGameDir;
    int m_javaMajorVersion = 0;  // Cache: Java major version of the JVM used for this launch
    int m_resWidth = 854;
    int m_resHeight = 480;
};


} // namespace ShadowLauncher
