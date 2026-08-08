#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QNetworkAccessManager>

namespace ShadowDownloader { class FileDownloader; }

namespace ShadowLauncher {
class JavaRuntimeInstaller;

class JavaBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList javaVersions READ javaVersions NOTIFY javaVersionsChanged)
    Q_PROPERTY(QStringList javaTypes READ javaTypes NOTIFY javaTypesChanged)
    Q_PROPERTY(QStringList javaArchs READ javaArchs NOTIFY javaArchsChanged)
    Q_PROPERTY(QStringList javaOSes READ javaOSes NOTIFY javaOSesChanged)
    Q_PROPERTY(QVariantList javaFiles READ javaFiles NOTIFY javaFilesChanged)
    Q_PROPERTY(bool fetching READ fetching NOTIFY fetchingChanged)

    Q_PROPERTY(QString selectedVersion READ selectedVersion WRITE setSelectedVersion NOTIFY selectedVersionChanged)
    Q_PROPERTY(QString selectedType READ selectedType WRITE setSelectedType NOTIFY selectedTypeChanged)
    Q_PROPERTY(QString selectedArch READ selectedArch WRITE setSelectedArch NOTIFY selectedArchChanged)
    Q_PROPERTY(QString selectedOS READ selectedOS WRITE setSelectedOS NOTIFY selectedOSChanged)

    // ── 一键安装所需 Java ──
    Q_PROPERTY(QString cpuArch READ cpuArch CONSTANT)
    Q_PROPERTY(bool javaInstalling READ javaInstalling NOTIFY javaInstallStateChanged)
    Q_PROPERTY(int javaInstallStep READ javaInstallStep NOTIFY javaInstallStateChanged)
    Q_PROPERTY(int javaInstallTotal READ javaInstallTotal CONSTANT)
    Q_PROPERTY(QString javaInstallStatus READ javaInstallStatus NOTIFY javaInstallStateChanged)
    // ── 下载进度（异步下载实时更新） ──
    Q_PROPERTY(int javaDownloadPercent READ javaDownloadPercent NOTIFY javaDownloadProgressChanged)
    Q_PROPERTY(qint64 javaDownloadBytes READ javaDownloadBytes NOTIFY javaDownloadProgressChanged)
    Q_PROPERTY(qint64 javaDownloadTotal READ javaDownloadTotal NOTIFY javaDownloadProgressChanged)
    Q_PROPERTY(double javaDownloadSpeedMBps READ javaDownloadSpeedMBps NOTIFY javaDownloadProgressChanged)

public:
    explicit JavaBackend(QObject *parent = nullptr);

    QStringList javaVersions() const { return m_versions; }
    QStringList javaTypes() const { return m_types; }
    QStringList javaArchs() const { return m_archs; }
    QStringList javaOSes() const { return m_oses; }
    QVariantList javaFiles() const { return m_files; }
    bool fetching() const { return m_fetching; }

    QString selectedVersion() const { return m_selVersion; }
    void setSelectedVersion(const QString &v);
    QString selectedType() const { return m_selType; }
    void setSelectedType(const QString &t);
    QString selectedArch() const { return m_selArch; }
    void setSelectedArch(const QString &a);
    QString selectedOS() const { return m_selOS; }
    void setSelectedOS(const QString &o);

    Q_INVOKABLE void refreshVersions();
    Q_INVOKABLE void fetchTypes();
    Q_INVOKABLE void fetchArchs();
    Q_INVOKABLE void fetchOSes();
    Q_INVOKABLE void fetchFiles();
    Q_INVOKABLE QString fileDownloadUrl(const QString &filename) const;
    Q_INVOKABLE void downloadJavaFile(const QString &filename);
    Q_INVOKABLE void downloadJavaFileTo(const QString &filename, const QString &outDir, qint64 expectedSize = 0);
    Q_INVOKABLE QString javaInstallDir() const;
    Q_INVOKABLE void cancelDownload();

    // ── 一键安装所需 Java ──
    Q_INVOKABLE void installRequiredJavas();
    Q_INVOKABLE void cancelJavaInstall();
    /// 启动流程自动安装单个版本 Java（worker 下载/解压，回调回主线程）：
    /// onDone(ok, error, javaExe)。安装完成自动触发 javaInstalled 信号。
    /// 注：非 Q_INVOKABLE——std::function 参数 moc 不支持，仅 C++ 侧（ShadowBackend）调用
    void installJavaForLaunch(int majorVersion,
                              std::function<void(bool, const QString&, const QString&)> onDone);
    /// 前置检测：扫描系统已有 Java，返回 [{major, version, path, isJdk}]
    Q_INVOKABLE QVariantList scanSystemJavas();
    Q_INVOKABLE QVariantList detectedSystemJavas() const;
    QString cpuArch() const;
    bool javaInstalling() const;
    int javaInstallStep() const;
    int javaInstallTotal() const;
    QString javaInstallStatus() const;
    int javaDownloadPercent() const;
    qint64 javaDownloadBytes() const;
    qint64 javaDownloadTotal() const;
    double javaDownloadSpeedMBps() const;

signals:
    void javaVersionsChanged();
    void javaTypesChanged();
    void javaArchsChanged();
    void javaOSesChanged();
    void javaFilesChanged();
    void fetchingChanged();

    void selectedVersionChanged();
    void selectedTypeChanged();
    void selectedArchChanged();
    void selectedOSChanged();

    void downloadProgress(int pct, qint64 dlBytes, qint64 totalBytes, double speedMBps);
    void downloadFinished(bool ok, const QString &path);
    void logMessage(const QString &msg);

    // ── 一键安装所需 Java ──
    void javaInstallStateChanged();
    void javaInstalled(const QString &label, const QString &path, bool skipped);
    void javaInstallFinished(bool ok, const QString &error);
    /// 前置检测完成
    void systemJavaScanFinished();
    /// 下载进度实时更新
    void javaDownloadProgressChanged();

private:
    void fetchUrl(const QString &url, std::function<void(const QStringList &)> callback,
                  std::function<void(int code, const QString &)> onError = nullptr);
    void fetchFileList(const QString &url, std::function<void(const QVariantList &)> callback);

    QStringList parseDirListing(const QByteArray &html, bool filesOnly = false);
    QString m_baseUrl;
    QNetworkAccessManager *m_nam;
    ShadowDownloader::FileDownloader *m_downloader = nullptr;

    QStringList m_versions;
    QStringList m_types;
    QStringList m_archs;
    QStringList m_oses;
    QVariantList m_files;
    bool m_fetching = false;

    QString m_selVersion;
    QString m_selType;
    QString m_selArch;
    QString m_selOS;

    JavaRuntimeInstaller* m_runtimeInstaller = nullptr;
};

} // namespace ShadowLauncher
