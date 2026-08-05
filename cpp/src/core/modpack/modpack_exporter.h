// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_exporter.h — 整合包导出（.mrpack，Modrinth 格式，2026-08-05）。
//
// 流程：读取已安装版本 → 解析加载器依赖 → 收集 mods + overrides → 后台线程
// 打包 modrinth.index.json + mods/ + overrides/。与导入（ModpackImporter）闭环。
//
// mrpack 规范（Modrinth Pack Format v1）：
//   modrinth.index.json { formatVersion:1, game, versionId, name, summary,
//                         files[{path, hashes{sha1,sha512}, env?}],
//                         dependencies{minecraft, forge|fabric-loader|...} }
//   overrides/  → 覆写 .minecraft 根的用户文件（config、saves 等）

#pragma once

#include <QObject>
#include <QString>
#include <QAtomicInteger>

namespace ShadowLauncher {

class ModpackExporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)

public:
    explicit ModpackExporter(QObject* parent = nullptr);

    void setGameDir(const QString& dir) { m_gameDir = dir; }

    bool isBusy() const { return m_busy; }
    qreal progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }

    /// 导出已安装版本为 .mrpack。outPath 为完整目标路径（含 .mrpack 后缀）。
    Q_INVOKABLE void exportVersion(const QString& versionId, const QString& displayName,
                                   bool includeSaves, bool includeResourcepacks,
                                   bool includeShaderpacks, const QString& outPath);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void progressChanged();
    void finished(bool success, const QString& outPath, const QString& error);

private:
    void setProgress(qreal p, const QString& text);

    QString m_gameDir;
    QAtomicInteger<int> m_cancel{0};
    bool m_busy = false;
    qreal m_progress = 0.0;
    QString m_statusText;
};

} // namespace ShadowLauncher
