// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QStringList>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QPointer>
#include <memory>
#include <atomic>
#include <functional>

namespace ShadowLauncher {
    class HttpClient;
    class FileDownloader;
    class DownloadQueue;
    class VersionBackend;
}

// ── Modpack import engine ──
// Handles parsing, installing, and downloading for Modrinth (.mrpack) and
// CurseForge (.zip) modpack formats in a self-contained async flow.
namespace ShadowLauncher {
class ModpackImporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY progressChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY hasResultChanged)
    Q_PROPERTY(QString resultName READ resultName NOTIFY hasResultChanged)
    Q_PROPERTY(QString resultVersionId READ resultVersionId NOTIFY hasResultChanged)
    // Mod download list for QML display
    Q_PROPERTY(QVariantList modItems READ modItems NOTIFY modListChanged)

public:
    enum Format { Unknown = 0, Modrinth, CurseForge };
    Q_ENUM(Format)

    enum Step {
        StepNone = 0,
        StepParsing,
        StepInstallMc,
        StepInstallLoader,
        StepDownloadMods,
        StepExtractOverrides,
        StepFinished,
        StepError
    };
    Q_ENUM(Step)

    explicit ModpackImporter(QObject* parent = nullptr);
    ~ModpackImporter() override;

    // ── Properties ──
    bool isBusy() const { return m_busy; }
    QString statusText() const { return m_statusText; }
    qreal progress() const { return m_progress; }
    QString currentFile() const { return m_currentFile; }
    bool hasResult() const { return m_hasResult; }
    QString resultName() const { return m_resultName; }
    QString resultVersionId() const { return m_resultVersionId; }
    QVariantList modItems() const { return m_modItems; }

    // ── Public API ──
    Q_INVOKABLE void startImport(const QString& zipFilePath);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void dismissResult();  // Called from QML after user dismisses success

    // Set VersionBackend for loader installation
    void setVersionBackend(VersionBackend* vb);

    // Static helpers for modpack format detection
    static Format detectFormat(const QString& zipPath);
    static QJsonObject parseManifest(const QString& zipPath, Format fmt);

signals:
    void busyChanged();
    void progressChanged();
    void hasResultChanged();
    void modListChanged();
    // Import completed/failed
    void importFinished(bool success, const QString& versionName, const QString& error);

private:
    // Internal steps
    void setStep(Step s);
    void setProgress(qreal p, const QString& text, const QString& file = {});
    void setError(const QString& message);
    void finishImport(const QString& name, const QString& verId);

    // Parsing
    struct ModpackMeta {
        Format format = Unknown;
        QString name;
        QString versionId;
        QString summary;
        QString mcVersion;
        QString loaderType;    // "forge", "fabric-loader", "neoforge", "quilt-loader"
        QString loaderVersion;
        QList<QVariantMap> mods;        // For download
        bool hasOverrides = false;
        QStringList overridesPaths;      // Files in overrides/ dir
    };
    bool parseZip(const QString& path);

    // Installation helpers
    bool installMcVersion(const QString& versionId, const QString& targetName);
    bool installModLoader(const QString& mcVersion, const QString& loaderType,
                          const QString& loaderVersion, const QString& targetName);
    bool downloadMods();
    bool extractOverrides(const QString& gameDir);


    // Network
    QByteArray downloadUrl(const QString& url);

    // State
    std::atomic<bool> m_cancelled{false};
    bool m_busy = false;
    QString m_statusText;
    qreal m_progress = 0.0;
    QString m_currentFile;
    bool m_hasResult = false;
    QString m_resultName;
    QString m_resultVersionId;
    QVariantList m_modItems;     // {name, size, status} for QML
    Step m_currentStep = StepNone;

    // Parsed metadata
    ModpackMeta m_meta;
    QString m_zipPath;
    QString m_targetName;        // The final version folder name
    QString m_mcDir;             // .minecraft root
    QString m_versionsDir;       // .minecraft/versions/
    QString m_modsDir;           // .minecraft/versions/targetName/mods/

    // Internal flow control
    void proceedToInstall();
    void proceedToModDownload();
    void proceedToOverlayExtract();

    // VersionBackend for loader installation
    VersionBackend* m_versionBackend = nullptr;

    // Network helpers
    HttpClient* m_http = nullptr;
    DownloadQueue* m_downloadQueue = nullptr;
    int m_modsTotal = 0;
};

} // namespace ShadowLauncher
