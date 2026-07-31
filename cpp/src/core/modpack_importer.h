// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QStringList>

#include <memory>

namespace ShadowLauncher {

class ModpackInstallTask;
class VersionBackend;
class VersionIsolation;

// ── Modpack import engine（门面）──
//
// 重构说明：旧实现（PowerShell 解压 / 硬编码路径 / 嵌套事件循环 / MCIM 镜像）
// 整体废弃。本类保持 QML 既有接口面不变（busy/statusText/progress/currentFile/
// hasResult/resultName/resultVersionId/modItems + startImport/cancelImport/
// dismissResult + importFinished），实际流程委托给分层模块：
//   ModpackParser（解析层）→ ZipArchive（文件工具）→ ModpackDownloader（网络层）
//   → ModpackInstallTask（任务调度层）
// 新增信号（logLine / stepChanged / fileProgressChanged）供后续 QML 使用，纯增量。
class ModpackImporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY progressChanged)
    Q_PROPERTY(QString currentStep READ currentStep NOTIFY stepChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY hasResultChanged)
    Q_PROPERTY(QString resultName READ resultName NOTIFY hasResultChanged)
    Q_PROPERTY(QString resultVersionId READ resultVersionId NOTIFY hasResultChanged)
    // Mod download list for QML display
    Q_PROPERTY(QVariantList modItems READ modItems NOTIFY modListChanged)
    // 实时运行日志（供 QML 日志面板）
    Q_PROPERTY(QString lastLogLine READ lastLogLine NOTIFY logLine)

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
    bool isBusy() const;
    QString statusText() const;
    qreal progress() const;
    QString currentFile() const;
    QString currentStep() const;
    bool hasResult() const;
    QString resultName() const;
    QString resultVersionId() const;
    QVariantList modItems() const;
    QString lastLogLine() const { return m_lastLogLine; }

    // ── Public API（QML 既有接口，保持不变）──
    Q_INVOKABLE void startImport(const QString& zipFilePath,
                                 bool includeOptional = false);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void dismissResult();  // Called from QML after user dismisses success

    // ── 依赖注入 ──
    void setVersionBackend(VersionBackend* vb);
    void setGameDir(const QString& dir);
    void setIsolation(VersionIsolation* iso);
    // CurseForge API Key（官方 x-api-key 鉴权）。为空时自动回退读取
    // 环境变量 SHADOW_CF_API_KEY 与 {gameDir}/config/cf_api_key.json。
    Q_INVOKABLE void setCurseForgeApiKey(const QString& key);

    // Static helpers for modpack format detection（保留兼容）
    static Format detectFormat(const QString& zipPath);
    static QJsonObject parseManifest(const QString& zipPath, Format fmt);

signals:
    void busyChanged();
    void progressChanged();
    void hasResultChanged();
    void modListChanged();
    // 新增（增量，供后续 QML）：
    void stepChanged();
    void logLine(const QString& msg);
    void fileProgressChanged(const QString& fileName, qreal fileProgress);
    // Import completed/failed（既有接口）
    void importFinished(bool success, const QString& versionName, const QString& error);

private:
    void loadApiKeyFromConfig();
    void updateFromTask();
    void clearResult();

    ModpackInstallTask* m_task = nullptr;
    VersionBackend* m_versionBackend = nullptr;
    VersionIsolation* m_isolation = nullptr;
    QString m_gameDir;
    QString m_apiKey;
    bool m_apiKeyLoaded = false;

    // 转发用镜像状态
    bool m_busy = false;
    QString m_statusText;
    qreal m_progress = 0.0;
    QString m_currentFile;
    QString m_currentStep;
    bool m_hasResult = false;
    QString m_resultName;
    QString m_resultVersionId;
    QString m_lastLogLine;
    QVariantList m_modItems;
};

} // namespace ShadowLauncher
