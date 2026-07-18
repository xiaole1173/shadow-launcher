// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace ShadowLauncher {

// Per-item info returned by validateArchive
struct ImportItemInfo {
    QString name;          // file/dir name, e.g. "saves"
    QString displayName;   // human-readable, e.g. "存档世界"
    QString relPath;       // path inside game/, e.g. "game/saves/"
    qint64 sizeBytes = 0;
    bool isDir = false;
    int riskLevel = 0;     // 0=safe, 1=caution, 2=dangerous
    QString warning;       // warning text for riskLevel>0
};

class UserDataBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportStateChanged)
    Q_PROPERTY(bool importing READ isImporting NOTIFY importStateChanged)
    Q_PROPERTY(QString pendingArchivePath READ pendingArchivePath NOTIFY pendingArchiveChanged)
    Q_PROPERTY(QVariantList pendingItems READ pendingItems NOTIFY pendingArchiveChanged)

public:
    explicit UserDataBackend(QObject* parent = nullptr);

    bool isExporting() const { return m_exporting; }
    bool isImporting() const { return m_importing; }
    QString pendingArchivePath() const { return m_pendingArchivePath; }
    QVariantList pendingItems() const;

    // Export: pack {gameDir}/versions/{versionId}/game/ into outputPath.zip
    Q_INVOKABLE void exportUserData(const QString& gameDir, const QString& versionId,
                                     const QString& outputPath);

    // Validate a ZIP archive, return list of importable items (via signal)
    Q_INVOKABLE void validateArchive(const QString& gameDir, const QString& archivePath);

    // Store pending import for later execution
    Q_INVOKABLE void setPendingImport(const QString& archivePath, const QVariantList& items);

    // Cancel pending import
    Q_INVOKABLE void cancelPendingImport();

    // Execute the pending import into targetVersionDir/game/
    Q_INVOKABLE void executeImport(const QString& gameDir, const QString& targetVersionId);

    // Extract archive to temp and return parsed items (called from worker thread)
    static QVector<ImportItemInfo> parseArchiveItems(const QString& archivePath,
                                                       const QString& gameDir,
                                                       QString& outVersionId,
                                                       QString& outMcVersion,
                                                       QString& outLoader,
                                                       QString& outLoaderVer,
                                                       QString& errorMsg);

signals:
    void exportProgress(int percent, const QString& status);
    void exportFinished(bool success, const QString& path, const QString& error);
    void exportStateChanged();

    void validateFinished(bool success, const QString& archivePath,
                          const QVariantList& items, const QString& error);
    void importStateChanged();
    void pendingArchiveChanged();

    void importProgress(int percent, const QString& status);
    void importFinished(bool success, const QString& versionId, const QString& error);

    void logMessage(const QString& msg);

private:
    void doExport(const QString& gameDir, const QString& versionId, const QString& outputPath);

    int m_riskForItem(const QString& name, bool isDir,
                       const QString& srcLoader, const QString& dstLoader) const;

    bool m_exporting = false;
    bool m_importing = false;
    QString m_pendingArchivePath;

    // Parsed items from last validateArchive call
    QVector<ImportItemInfo> m_validatedItems;

    // Info parsed from README.txt
    QString m_srcVersionId;
    QString m_srcMcVersion;
    QString m_srcLoader;
    QString m_srcLoaderVer;
};

} // namespace ShadowLauncher
