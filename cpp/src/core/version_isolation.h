// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

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

    QStringList isolatedVersions() const;
    bool isVersionIsolated(const QString& versionId) const;

    QString getVersionGameDir(const QString& versionId) const;
    bool migrateToIsolated(const QString& versionId);

signals:
    void isolationChanged();

private:
    void loadConfig();
    void saveConfig();
    QString configPath() const;

    QString m_gameDir;
    bool m_enabled = true;
    QStringList m_isolatedVersions;
};

} // namespace ShadowLauncher
