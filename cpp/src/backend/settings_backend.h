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
    Q_PROPERTY(qreal backgroundBlur READ backgroundBlur WRITE setBackgroundBlur NOTIFY customBgChanged)
    Q_PROPERTY(qreal cropX READ cropX WRITE setCropX NOTIFY customBgChanged)
    Q_PROPERTY(qreal cropY READ cropY WRITE setCropY NOTIFY customBgChanged)
    Q_PROPERTY(int fileDownloadSource READ fileDownloadSource WRITE setFileDownloadSource NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int listDownloadSource READ listDownloadSource WRITE setListDownloadSource NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int maxDownloadThreads READ maxDownloadThreads WRITE setMaxDownloadThreads NOTIFY downloadSettingsChanged)
    Q_PROPERTY(double downloadSpeedLimitMB READ downloadSpeedLimitMB WRITE setDownloadSpeedLimitMB NOTIFY downloadSettingsChanged)
    Q_PROPERTY(int autoLangMode READ autoLangMode WRITE setAutoLangMode NOTIFY autoLangModeChanged)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowSettingsChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowSettingsChanged)
    // ── 启动细节（低垂果实批，2026-08-08：对齐 主流启动器/主流启动器）──
    Q_PROPERTY(int gcMode READ gcMode WRITE setGcMode NOTIFY launchDetailChanged)
    Q_PROPERTY(int processPriority READ processPriority WRITE setProcessPriority NOTIFY launchDetailChanged)
    Q_PROPERTY(bool fullscreenEnabled READ fullscreenEnabled WRITE setFullscreenEnabled NOTIFY launchDetailChanged)
    Q_PROPERTY(QString autoJoinServer READ autoJoinServer WRITE setAutoJoinServer NOTIFY launchDetailChanged)
    Q_PROPERTY(QString windowTitleOverride READ windowTitleOverride WRITE setWindowTitleOverride NOTIFY launchDetailChanged)
    Q_PROPERTY(QString preLaunchCommand READ preLaunchCommand WRITE setPreLaunchCommand NOTIFY launchDetailChanged)
    Q_PROPERTY(QString postExitCommand READ postExitCommand WRITE setPostExitCommand NOTIFY launchDetailChanged)

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

    // ── 启动细节（版本级覆盖，对齐主流启动器实现 VersionAdvanceGC/VersionServerEnter）──
    Q_INVOKABLE int versionGcMode(const QString& versionId) const;      // 0=跟随全局 1-3=覆盖
    Q_INVOKABLE void setVersionGcMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionAutoJoinServer(const QString& versionId) const;  // 空=跟随全局
    Q_INVOKABLE void setVersionAutoJoinServer(const QString& versionId, const QString& addr);
    // 全屏/窗口标题/命令：mode 语义同 highPerfGpu（0=跟随全局 1=版本覆盖）
    Q_INVOKABLE int versionFullscreenMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionFullscreenMode(const QString& versionId, int mode);
    Q_INVOKABLE bool versionFullscreen(const QString& versionId) const;
    Q_INVOKABLE void setVersionFullscreen(const QString& versionId, bool v);
    Q_INVOKABLE int versionWindowTitleMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionWindowTitleMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionWindowTitle(const QString& versionId) const;
    Q_INVOKABLE void setVersionWindowTitle(const QString& versionId, const QString& v);
    Q_INVOKABLE int versionPreLaunchMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionPreLaunchMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionPreLaunchCommand(const QString& versionId) const;
    Q_INVOKABLE void setVersionPreLaunchCommand(const QString& versionId, const QString& v);
    Q_INVOKABLE int versionPostExitMode(const QString& versionId) const;
    Q_INVOKABLE void setVersionPostExitMode(const QString& versionId, int mode);
    Q_INVOKABLE QString versionPostExitCommand(const QString& versionId) const;
    Q_INVOKABLE void setVersionPostExitCommand(const QString& versionId, const QString& v);

    Q_INVOKABLE QVariantList availableJavaList();
    Q_INVOKABLE void selectJavaByIndex(int index);
    Q_INVOKABLE void removeJavaFromList(int index);
    Q_INVOKABLE QVariantList persistedJavaList();
    Q_INVOKABLE QString findJavaForVersion(int requiredMajor, int maxMajor = 0);
    // maxMajor>0 时限制在 [requiredMajor, maxMajor] 区间（老版本 Mixin 兼容上限）
    Q_INVOKABLE QString openJavaFileDialog();
    Q_INVOKABLE QString browseJava();          // QML alias
    // ── 设置导入导出（2026-08-08：主流启动器 CacheExportConfig 对齐）──
    /// 导出全部启动器设置到指定文件（ini 格式），成功返回 true
    Q_INVOKABLE bool exportSettingsToFile(const QString& path);
    /// 从文件导入设置（合并，覆盖已有值），成功返回 true
    Q_INVOKABLE bool importSettingsFromFile(const QString& path);
    /// 导出的设置内容预览（供 QML 确认弹窗展示）
    Q_INVOKABLE QString exportSettingsPreview() const;
    void setMinecraftDir(const QString& dir);
    Q_INVOKABLE void setIsolationEnabled(bool enabled);
    Q_INVOKABLE void migrateVersionToIsolated(const QString& versionId);
    Q_INVOKABLE QString getVersionGameDir(const QString& versionId) const;
    Q_INVOKABLE bool isolationEnabled() const;
    Q_INVOKABLE void openGameDir();
    Q_INVOKABLE bool openVersionDir(const QString& versionId);
    Q_INVOKABLE void deleteVersion(const QString& versionId);
    bool embeddedLoginEnabled() const { return m_embeddedLoginEnabled; }
    Q_INVOKABLE void setEmbeddedLoginEnabled(bool v);
    int languageIndex() const { return m_languageIndex; }
    Q_INVOKABLE void setLanguageIndex(int idx);
    bool isLanguageChanged() const { return m_languageIndex != m_launchLanguageIndex; }

    // ── 启动细节 getter/setter（全局）──
    int gcMode() const { return m_gcMode; }
    void setGcMode(int v) { m_gcMode = v; saveSettings(); emit launchDetailChanged(); }
    int processPriority() const { return m_processPriority; }
    void setProcessPriority(int v) { m_processPriority = v; saveSettings(); emit launchDetailChanged(); }
    bool fullscreenEnabled() const { return m_fullscreenEnabled; }
    void setFullscreenEnabled(bool v) { m_fullscreenEnabled = v; saveSettings(); emit launchDetailChanged(); }
    QString autoJoinServer() const { return m_autoJoinServer; }
    void setAutoJoinServer(const QString& v) { m_autoJoinServer = v; saveSettings(); emit launchDetailChanged(); }
    QString windowTitleOverride() const { return m_windowTitleOverride; }
    void setWindowTitleOverride(const QString& v) { m_windowTitleOverride = v; saveSettings(); emit launchDetailChanged(); }
    QString preLaunchCommand() const { return m_preLaunchCommand; }
    void setPreLaunchCommand(const QString& v) { m_preLaunchCommand = v; saveSettings(); emit launchDetailChanged(); }
    QString postExitCommand() const { return m_postExitCommand; }
    void setPostExitCommand(const QString& v) { m_postExitCommand = v; saveSettings(); emit launchDetailChanged(); }

    // Custom background
    QString customBgPath() const { return m_customBgPath; }
    void setCustomBgPath(const QString& path) { m_customBgPath = path; saveSettings(); emit customBgChanged(); }
    qreal sidebarOpacity() const { return m_sidebarOpacity; }
    void setSidebarOpacity(qreal v) { m_sidebarOpacity = v; saveSettings(); emit customBgChanged(); }
    qreal contentOpacity() const { return m_contentOpacity; }
    void setContentOpacity(qreal v) { m_contentOpacity = v; saveSettings(); emit customBgChanged(); }
    qreal backgroundBlur() const { return m_backgroundBlur; }
    void setBackgroundBlur(qreal v) { m_backgroundBlur = qBound(0.0, v, 1.0); saveSettings(); emit customBgChanged(); }
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
    // Returns combo box index for QML (mode → idx mapping: 0→2, 1→0, 2→1)
    int autoLangModeComboIndex() const;
    Q_INVOKABLE void setAutoLangMode(int mode);
    // Window resolution
    int windowWidth() const { return m_windowWidth; }
    void setWindowWidth(int w) { if (w >= 100 && w <= 7680 && w != m_windowWidth) { m_windowWidth = w; saveSettings(); emit windowSettingsChanged(); } }
    int windowHeight() const { return m_windowHeight; }
    void setWindowHeight(int h) { if (h >= 100 && h <= 4320 && h != m_windowHeight) { m_windowHeight = h; saveSettings(); emit windowSettingsChanged(); } }

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
    /// 一次 Java 扫描完整结束（刷新按钮/自动检测完成后弹 toast）
    void javaScanFinished();
    void javaReadyChanged();
    void memorySettingsChanged();
    void generalSettingsChanged();
    void isolationChanged();
    void embeddedLoginChanged();
    void languageChanged();
    void customBgChanged();
    void downloadSettingsChanged();
    void autoLangModeChanged();
    /// 启动细节设置变更（GC/优先级/全屏/自动进服/窗口标题/pre-post 命令）
    void launchDetailChanged();
    void windowSettingsChanged();
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
    void loadJavaList();
    void saveJavaList(const QVector<JavaInfo>& list);
    bool isPathInCandidateDir(const QString& binDir) const;

    QVector<JavaInfo> findAllJava();
    JavaInfo getJavaInfo(const QString& exePath);
    int parseMajorVersion(const QString& versionStr);
    bool tryAddJavaResult(const QString& exePath, QSet<QString>& seenBinDirs,
                          QVector<JavaInfo>& out);
    QString findJavaInDir(const QString& dirPath);
    void findJavaRecursive(const QString& dirPath, int maxDepth, int currentDepth,
                           QSet<QString>& seenBinDirs,
                           QVector<JavaInfo>& results);
    bool isJavaSpecialPath(const QString& binDir) const;

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
    qreal m_backgroundBlur = 0.0;
    qreal m_cropX = 0.5;
    qreal m_cropY = 0.5;

    // Download settings
    // 全局源策略默认 PreferOfficial(1)——仅影响 MC 下载；加载器下载源独立
    // 于 version_backend / mod_loader_installer（默认镜像优先，2026-08-15）
    int m_fileDownloadSource = 1;      // 0=PreferMirror, 1=PreferOfficial, 2=AutoSwitch
    int m_listDownloadSource = 1;
    int m_maxDownloadThreads = 64;
    double m_downloadSpeedLimitMB = -1; // -1 = unlimited

    int m_autoLangMode = 1;  // 0=off, 1=system locale, 2=IP region
    int m_windowWidth = 854;
    int m_windowHeight = 480;

    // ── 启动细节（低垂果实批，2026-08-08）──
    int m_gcMode = 0;                 // 0=自动 1=分代ZGC优先 2=仅G1GC 3=不指定
    int m_processPriority = 1;        // 0=高 1=中 2=低（主流启动器 LaunchArgumentPriority）
    bool m_fullscreenEnabled = false; // --fullscreen
    QString m_autoJoinServer;         // 自动进服 host[:port]
    QString m_windowTitleOverride;    // 游戏窗口标题覆盖
    QString m_preLaunchCommand;       // 启动前命令
    QString m_postExitCommand;        // 退出后命令

    // Cache for Java scan results (expensive operation)
    QVector<JavaInfo> m_cachedJavaList;
    bool m_javaCacheValid = false;
    bool m_javaScanning = false;
    QSet<QString> m_javaRemovedPaths;
};

} // namespace ShadowLauncher
