// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "minecraft_layout.h"

namespace ShadowLauncher {

class VersionIsolation : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY isolationChanged)

public:
    explicit VersionIsolation(QObject* parent = nullptr);

    void setGameDir(const QString& dir);
    QString gameDir() const { return m_gameDir; }

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isVersionIsolated(const QString& versionId) const;

    QString getVersionGameDir(const QString& versionId) const;
    bool migrateToIsolated(const QString& versionId);

    // ── 外部 .minecraft 目录布局覆盖（2026-08-18）──
    // 切换为外部目录（PCL2/HMCL/官方）时，getVersionGameDir 改用布局感知解析，
    // 尊重原目录的版本隔离/非隔离形态；并禁止向该目录写入版本隔离配置。
    void setFolderLayout(MinecraftLayout layout) { m_folderLayout = layout; }
    MinecraftLayout folderLayout() const { return m_folderLayout; }
    bool isForeignFolder() const { return m_folderLayout != MinecraftLayout::Unknown; }

signals:
    void isolationChanged();

private:
    void loadConfig();
    void saveConfig();
    QString configPath() const;

    QString m_gameDir;
    bool m_enabled = true;
    QStringList m_isolatedVersions;
    MinecraftLayout m_folderLayout = MinecraftLayout::Unknown;  // Unknown=自有目录（传统行为）
};

} // namespace ShadowLauncher
