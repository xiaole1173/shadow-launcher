// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// MigrationManager — 一次性目录结构迁移（v0.4.0-beta 专用）。
//
// 背景：v0.4.0-beta 将 Qt DLL/plugins/qml 等散乱文件收拢到 launcher/ 子目录。
// 旧用户通过增量更新只替换了 ShadowLauncher.exe，launcher/ 目录并不存在。
// MigrationManager 负责：
//   1. 下载 7zr.exe → launcher/bin/
//   2. 下载全量包（不含 exe）→ _update/
//   3. 用 7zr.exe 解压覆盖
//   4. 重启启动器
//
// 此迁移仅运行一次，完成后写 `.migrated` 标记文件。

#pragma once

#include <QObject>
#include <QString>
#include <QNetworkReply>

class MigrationManager : public QObject {
    Q_OBJECT
public:
    explicit MigrationManager(QObject* parent = nullptr);
    ~MigrationManager() override = default;

    /// 检查是否需要迁移。如果 launcher/ 不存在且无 .migrated 标记，返回 true。
    static bool needsMigration();

    /// 执行迁移。阻塞调用（内部使用 QEventLoop），完成后自动重启启动器。
    /// 如果过程中出错，返回 false 并留 errorMsg。
    bool runMigration(QString& errorMsg);

    // 配置：baseUrl 是 Gitee 下载前缀
    void setRepoUrl(const QString& url) { m_baseUrl = url; }

signals:
    void progressChanged(const QString& stage, int percent);
    void errorOccurred(const QString& msg);

private:
    bool downloadTo(const QString& url, const QString& destPath, int timeoutMs,
                    const QString& stageLabel, int basePercent, int rangePercent,
                    QString& errorMsg);
    bool extractPackage(const QString& archivePath, const QString& targetDir,
                        const QStringList& excludes, int timeoutMs,
                        QString& errorMsg);

    QString m_baseUrl;
};
