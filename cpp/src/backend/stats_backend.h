// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>

namespace ShadowLauncher {

class VersionIsolation;

class StatsBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(double totalHours   READ totalHours   NOTIFY statsChanged)
    Q_PROPERTY(QVariantList versionStats READ versionStats NOTIFY statsChanged)
    Q_PROPERTY(bool loading        READ loading       NOTIFY loadingChanged)
    Q_PROPERTY(bool isEmpty        READ isEmpty       NOTIFY statsChanged)
    Q_PROPERTY(QString gameDir     READ gameDir       CONSTANT)

public:
    explicit StatsBackend(const QString& gameDir, VersionIsolation* vi, QObject* parent = nullptr);

    double totalHours()   const { return m_totalHours; }
    QVariantList versionStats() const { return m_versionStats; }
    bool loading()        const { return m_loading; }
    bool isEmpty()        const { return m_empty; }

    Q_INVOKABLE void refresh();
    QString gameDir() const { return m_gameDir; }

signals:
    void statsChanged();
    void loadingChanged();

private:
    struct VerInfo {
        QString id;
        QString displayName;
    };

    QVariantList scanAllVersions();
    static double scanSaveDir(const QString& mcDir);

    QString           m_gameDir;
    VersionIsolation* m_isolation;
    QVariantList      m_versionStats;
    double            m_totalHours = 0;
    bool              m_loading = false;
    bool              m_empty = true;
};

} // namespace ShadowLauncher
