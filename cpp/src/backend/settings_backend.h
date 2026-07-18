// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QTimer>

namespace ShadowLauncher {

class VersionIsolation;

class SettingsBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString javaPath READ javaPath WRITE setJavaPath NOTIFY javaPathChanged)
    Q_PROPERTY(QString javaVersion READ javaVersion NOTIFY javaPathChanged)
    Q_PROPERTY(int javaMajor READ javaMajor NOTIFY javaPathChanged)
    Q_PROPERTY(int minMemoryMB READ minMemoryMB NOTIFY memorySettingsChanged)
    Q_PROPERTY(int maxMemoryMB READ maxMemoryMB NOTIFY memorySettingsChanged)
    Q_PROPERTY(bool autoMemoryEnabled READ autoMemoryEnabled WRITE setAutoMemoryEnabled NOTIFY memorySettingsChanged)
    Q_PROPERTY(bool javaReady READ isJavaReady NOTIFY javaPathChanged)
    Q_PROPERTY(bool isolationEnabled READ isolationEnabled NOTIFY isolationChanged)
    Q_PROPERTY(bool embeddedLoginEnabled READ embeddedLoginEnabled WRITE setEmbeddedLoginEnabled NOTIFY embeddedLoginChanged)
    Q_PROPERTY(int languageIndex READ languageIndex WRITE setLanguageIndex NOTIFY languageChanged)
    Q_PROPERTY(bool languageChanged READ isLanguageChanged NOTIFY languageChanged)
    Q_PROPERTY(QString customBgPath READ customBgPath WRITE setCustomBgPath NOTIFY customBgChanged)
    Q_PROPERTY(qreal sidebarOpacity READ sidebarOpacity WRITE setSidebarOpacity NOTIFY customBgChanged)
    Q_PROPERTY(qreal contentOpacity READ contentOpacity WRITE setContentOpacity NOTIFY customBgChanged)
    Q_PROPERTY(qreal cropX READ cropX WRITE setCropX NOTIFY customBgChanged)
    Q_PROPERTY(qreal cropY READ cropY WRITE setCropY NOTIFY customBgChanged)
    Q_PROPERTY(int fileDownloadSource READ fileDownloadSource WRITE setFileDownloadSource NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int listDownloadSource READ listDownloadSource WRITE setListDownloadSource NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int maxDownloadThreads READ maxDownloadThreads WRITE setMaxDownloadThreads NOTIFY downloadSettingsChanged)
    Q_PROPERTY(double downloadSpeedLimitMB READ downloadSpeedLimitMB WRITE setDownloadSpeedLimitMB NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int autoLangMode READ autoLangMode WRITE setAutoLangMode NOTIFY autoLangModeChanged)

public:
    explicit SettingsBackend(QObject* parent = nullptr);

    QString javaPath() const { return m_javaPath; }
    QString javaVersion() const { return m_javaVersion; }
    int javaMajor() const { return m_javaMajor; }
    int minMemoryMB() const { return m_minMemoryMB; }
    int maxMemoryMB() const { return m_maxMemoryMB; }
    bool autoMemoryEnabled() const { return m_autoMemory; }
    QString lastLaunchedVersion() const { return m_lastLaunchedVersion; }
    void setLastLaunchedVersion(const QString& v) {
        m_lastLaunchedVersion = v;
        saveSettings();
    }
    QString lastSelectedVersion() const { return m_lastSelectedVersion; }
    void setLastSelectedVersion(const QString& v) {
        m_lastSelectedVersion = v;
        saveSettings();
    }
    bool isJavaReady() const { return m_javaReady; }

    void setJavaPath(const QString& path);

    // ---- Slots ----
    Q_INVOKABLE QVariantList scanJavaInstallations();
    Q_INVOKABLE bool isJavaScanning() const { return m_javaScanning; }
    Q_INVOKABLE QString autoSelectJava();
    Q_INVOKABLE QString detectJava();         // QML alias
    Q_INVOKABLE QVariantMap getMemoryStatus();
    Q_INVOKABLE void setMinMemory(int mb);
    Q_INVOKABLE void setMaxMemory(int mb);
    Q_INVOKABLE void setAutoMemoryEnabled(bool enabled);

    // Per-version memory override
    Q_INVOKABLE int versionMemoryMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionMemoryMode(const QString& versionId, int mode);
    Q_INVOKABLE int versionMemoryManualMB(const QString& versionId) const;
    Q_INVOKABLE void setVersionMemoryManualMB(const QString& versionId, int mb);

    // ── Per-version Java/launch overrides ──
    Q_INVOKABLE int versionJavaMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionJavaMode(const QString& versionId, int mode);
    Q_INVOKABLE int versionJvmArgsMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionJvmArgsMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionJvmArgs(const QString& versionId) const;
    Q_INVOKABLE void setVersionJvmArgs(const QString& versionId, const QString& args);

    Q_INVOKABLE int versionGameArgsMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionGameArgsMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionGameArgs(const QString& versionId) const;
    Q_INVOKABLE void setVersionGameArgs(const QString& versionId, const QString& args);

    Q_INVOKABLE int versionHighPerfGpuMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionHighPerfGpuMode(const QString& versionId, int mode);
    Q_INVOKABLE bool versionHighPerfGpu(const QString& versionId) const;
    Q_INVOKABLE void setVersionHighPerfGpu(const QString& versionId, bool v);

    Q_INVOKABLE QVariantList availableJavaList();
    Q_INVOKABLE void selectJavaByIndex(int index);
    Q_INVOKABLE QString findJavaForVersion(int requiredMajor);
    Q_INVOKABLE QString openJavaFileDialog();
    Q_INVOKABLE QString browseJava();          // QML alias
    void setMinecraftDir(const QString& dir);
    Q_INVOKABLE void setIsolationEnabled(bool enabled);
    Q_INVOKABLE void migrateVersionToIsolated(const QString& versionId);
    Q_INVOKABLE QString getVersionGameDir(const QString& versionId) const;
    Q_INVOKABLE bool isolationEnabled() const;
    Q_INVOKABLE void openGameDir();
    Q_INVOKABLE void openVersionDir(const QString& versionId);
    Q_INVOKABLE void deleteVersion(const QString& versionId);
    Q_INVOKABLE void openPath(const QString& path);
    bool embeddedLoginEnabled() const { return m_embeddedLoginEnabled; }
    Q_INVOKABLE void setEmbeddedLoginEnabled(bool v);
    int languageIndex() const { return m_languageIndex; }
    Q_INVOKABLE void setLanguageIndex(int idx);
    bool isLanguageChanged() const { return m_languageIndex != m_launchLanguageIndex; }
    Q_INVOKABLE void restartApp();

    // Custom background
    QString customBgPath() const { return m_customBgPath; }
    void setCustomBgPath(const QString& path) { m_customBgPath = path; saveSettings(); emit customBgChanged(); }
    qreal sidebarOpacity() const { return m_sidebarOpacity; }
    void setSidebarOpacity(qreal v) { m_sidebarOpacity = v; saveSettings(); emit customBgChanged(); }
    qreal contentOpacity() const { return m_contentOpacity; }
    void setContentOpacity(qreal v) { m_contentOpacity = v; saveSettings(); emit customBgChanged(); }
    qreal cropX() const { return m_cropX; }
    void setCropX(qreal v) { m_cropX = qBound(0.0, v, 1.0); saveSettings(); emit customBgChanged(); }
    Q_INVOKABLE void updateCrop(qreal x, qreal y) {
        m_cropX = qBound(0.0, x, 1.0);
        m_cropY = qBound(0.0, y, 1.0);
        saveSettings();
        emit customBgChanged();
    }
    qreal cropY() const { return m_cropY; }
    void setCropY(qreal v) { m_cropY = qBound(0.0, v, 1.0); saveSettings(); emit customBgChanged(); }

    // ── Auto-language mode ──
    int autoLangMode() const { return m_autoLangMode; }
    Q_INVOKABLE void setAutoLangMode(int mode);

    // Download settings
    int fileDownloadSource() const { return m_fileDownloadSource; }
    void setFileDownloadSource(int v) { m_fileDownloadSource = v; saveSettings(); emit downloadSettingsChanged(); }
    int listDownloadSource() const { return m_listDownloadSource; }
    void setListDownloadSource(int v) { m_listDownloadSource = v; saveSettings(); emit downloadSettingsChanged(); }
    int maxDownloadThreads() const { return m_maxDownloadThreads; }
    void setMaxDownloadThreads(int v) { m_maxDownloadThreads = qBound(1, v, 128); saveSettings(); emit downloadSettingsChanged(); }
    double downloadSpeedLimitMB() const { return m_downloadSpeedLimitMB; }
    void setDownloadSpeedLimitMB(double v) { m_downloadSpeedLimitMB = (v < 0) ? -1.0 : qBound(0.1, v, 20.0); saveSettings(); emit downloadSettingsChanged(); }
    void setIsolationGameDir(const QString& dir);
    VersionIsolation* isolation() const { return m_isolation; }
    QString gameDir() const { return m_gameDir; }

signals:
    void javaPathChanged();
    void javaReadyChanged();
    void memorySettingsChanged();
    void generalSettingsChanged();
    void isolationChanged();
    void embeddedLoginChanged();
    void languageChanged();
    void customBgChanged();
    void downloadSettingsChanged();
    void autoLangModeChanged();
    void logMessage(const QString& msg);

private:
    struct JavaInfo {
        QString path;
        QString version;
        int major = 0;
    };

    void loadSettings();
    void saveSettings();
    void doAutoDetect();
    const QVector<JavaInfo>& cachedJavaList();

    QVector<JavaInfo> findAllJava();
    JavaInfo getJavaInfo(const QString& exePath);
    int parseMajorVersion(const QString& versionStr);
    bool tryAddJavaResult(const QString& exePath, QSet<QString>& seenBinDirs,
                          QVector<JavaInfo>& out);
    QString findJavaInDir(const QString& dirPath);
    QString findJavaOnPath();

    QString m_javaPath;
    QString m_javaVersion;
    int m_javaMajor = 0;
    int m_minMemoryMB = 512;
    int m_maxMemoryMB = 2048;
    bool m_autoMemory = true;
    QString m_lastLaunchedVersion;
    QString m_lastSelectedVersion;
    bool m_javaReady = false;
    QString m_gameDir;
    bool m_embeddedLoginEnabled = false;
    int m_languageIndex = 0;
    int m_launchLanguageIndex = 0;
    VersionIsolation* m_isolation = nullptr;

    // Custom background
    QString m_customBgPath;
    qreal m_sidebarOpacity = 0.90;
    qreal m_contentOpacity = 0.70;
    qreal m_cropX = 0.5;
    qreal m_cropY = 0.5;

    // Download settings
    int m_fileDownloadSource = 1;      // 0=PreferMirror, 1=PreferOfficial, 2=AutoSwitch
    int m_listDownloadSource = 1;
    int m_maxDownloadThreads = 64;
    double m_downloadSpeedLimitMB = -1; // -1 = unlimited

    int m_autoLangMode = 1;  // 0=off, 1=system locale, 2=IP region

    // Cache for Java scan results (expensive operation)
    QVector<JavaInfo> m_cachedJavaList;
    bool m_javaCacheValid = false;
    bool m_javaScanning = false;
};

} // namespace ShadowLauncher
