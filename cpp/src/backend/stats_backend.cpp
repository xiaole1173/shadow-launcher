// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "stats_backend.h"

#include "../core/version_isolation.h"
#include "../utils/logger.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <algorithm>

namespace ShadowLauncher {

StatsBackend::StatsBackend(const QString& gameDir, VersionIsolation* vi, QObject* parent)
    : QObject(parent)
    , m_gameDir(gameDir)
    , m_isolation(vi)
{
}

void StatsBackend::refresh()
{
    if (m_loading) return;

    qCInfo(logApp) << QStringLiteral("[时长统计] 开始扫描文件系统");

    m_loading = true;
    emit loadingChanged();

    const QString versionsDir = m_gameDir + QStringLiteral("/versions");
    const QString gameDir    = m_gameDir;
    VersionIsolation* iso    = m_isolation;

    QList<VerInfo> infos;
    QDir vdir(versionsDir);
    if (vdir.exists()) {
        const QStringList subDirs = vdir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& subDir : subDirs) {
            infos.append({subDir, subDir});
        }
    }

    qCInfo(logApp) << QStringLiteral("[时长统计] 发现版本目录 数量=%1 路径=%2").arg(infos.size()).arg(versionsDir);

    auto* watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher]() {
        m_versionStats = watcher->result();

        // Sort by hours descending
        std::sort(m_versionStats.begin(), m_versionStats.end(), [](const QVariant& a, const QVariant& b) {
            return a.toMap().value(QStringLiteral("hours")).toDouble() > b.toMap().value(QStringLiteral("hours")).toDouble();
        });

        m_totalHours = 0;
        for (const QVariant& v : m_versionStats) {
            m_totalHours += v.toMap().value(QStringLiteral("hours")).toDouble();
        }
        m_empty = m_versionStats.isEmpty();
        m_loading = false;

        emit statsChanged();
        emit loadingChanged();
        qCInfo(logApp) << QStringLiteral("[时长统计] 扫描完成 版本数=%1 总时长=%2小时").arg(m_versionStats.size()).arg(m_totalHours);
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([infos, gameDir, iso]() -> QVariantList {
        QVariantList result;
        for (const VerInfo& vi : infos) {
            QString mcDir;
            if (iso && iso->isEnabled()) {
                mcDir = gameDir + QStringLiteral("/versions/") + vi.id + QStringLiteral("/game");
            } else {
                mcDir = gameDir;
            }

            qCInfo(logApp) << QStringLiteral("[时长统计] 扫描版本 版本=%1 路径=%2").arg(vi.id, mcDir);
            double hours = scanSaveDir(mcDir);
            qCInfo(logApp) << QStringLiteral("[时长统计] 版本时长 版本=%1 小时=%2").arg(vi.id).arg(hours);

            QVariantMap map;
            map[QStringLiteral("versionId")]   = vi.id;
            map[QStringLiteral("displayName")] = vi.displayName;
            map[QStringLiteral("hours")]       = hours;
            result.append(map);
        }
        return result;
    }));
}

double StatsBackend::scanSaveDir(const QString& mcDir)
{
    const QString savesDir = mcDir + QStringLiteral("/saves");
    QDir saves(savesDir);
    if (!saves.exists()) return 0;

    double totalHours = 0;

    for (const QString& world : saves.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        // Stats dir moved from saves/<world>/stats/ to saves/<world>/players/stats/ in MC 1.13+
        QStringList statDirs;
        statDirs << (savesDir + QStringLiteral("/") + world + QStringLiteral("/stats"));
        statDirs << (savesDir + QStringLiteral("/") + world + QStringLiteral("/players/stats"));

        for (const QString& statsPath : statDirs) {
            QDir statsDir(statsPath);
            if (!statsDir.exists()) continue;

            for (const QString& statFile : statsDir.entryList({QStringLiteral("*.json")}, QDir::Files)) {
                QFile f(statsDir.absoluteFilePath(statFile));
                if (!f.open(QIODevice::ReadOnly)) continue;

                QJsonParseError err;
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
                f.close();

                if (err.error != QJsonParseError::NoError || !doc.isObject()) continue;

                QJsonObject root   = doc.object();
                QJsonObject stats  = root.value(QStringLiteral("stats")).toObject();
                QJsonObject custom = stats.value(QStringLiteral("minecraft:custom")).toObject();

                // MC renamed play_one_minute to play_time in 1.13+
                qint64 ticks = (qint64)custom.value(QStringLiteral("minecraft:play_time")).toDouble();
                if (ticks <= 0)
                    ticks = (qint64)custom.value(QStringLiteral("minecraft:play_one_minute")).toDouble();

                if (ticks > 0)
                    totalHours += ticks / 72000.0;
            }
        }
    }

    return totalHours;
}

} // namespace ShadowLauncher
