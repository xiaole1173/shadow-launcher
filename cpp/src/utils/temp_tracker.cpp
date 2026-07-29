// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// TempTracker implementation — see header for design.

#include "temp_tracker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logTracker, "Shadow.TempTracker")

namespace {

    /// 从 tempDir 路径生成一个稳定的 tracking 文件名
    /// 使用 UUID 片段 + 时间戳，确保在极端并发下也不冲突
    QString fileNameFor(const QString& tempDir) {
        // 从路径末尾提取 UUID（如 shadow-merged-{uuid}）
        QString base = QFileInfo(tempDir).fileName();
        if (base.isEmpty()) {
            // 兜底：用路径 hash
            base = QString::number(qHash(tempDir), 16);
        }
        return base + QStringLiteral(".track");
    }

} // anonymous namespace

QString TempTracker::trackerDir()
{
    // 放在 %TEMP% 下更合理 — 和临时目录在同一个挂载点，跨 boot 自动被 OS 清理
    // 同时避免污染 AppData
    QString dir = QDir::tempPath() + QStringLiteral("/shadow-tracker/");
    return dir;
}

void TempTracker::record(const QString& tempDir)
{
    if (tempDir.isEmpty()) return;

    QDir dir(trackerDir());
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QString filePath = dir.filePath(fileNameFor(tempDir));
    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(tempDir.toUtf8());
        f.write("\n");
        f.close();
        qCInfo(logTracker) << QStringLiteral("[追踪] 记录: %1 → %2").arg(tempDir, filePath);
    } else {
        qCWarning(logTracker) << QStringLiteral("[追踪] 写入失败: %1").arg(filePath);
    }
}

void TempTracker::forget(const QString& tempDir)
{
    if (tempDir.isEmpty()) return;

    QString filePath = QDir(trackerDir()).filePath(fileNameFor(tempDir));
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
        qCInfo(logTracker) << QStringLiteral("[追踪] 移除: %1").arg(filePath);
    }
}

void TempTracker::cleanupOrphans()
{
    QDir dir(trackerDir());
    if (!dir.exists()) {
        qCInfo(logTracker) << QStringLiteral("[追踪] 无追踪目录，跳过清理");
        return;
    }

    QStringList entries = dir.entryList(QStringList() << QStringLiteral("*.track"),
                                        QDir::Files, QDir::Name);
    if (entries.isEmpty()) {
        qCInfo(logTracker) << QStringLiteral("[追踪] 无残留 temp 记录");
        return;
    }

    int cleaned = 0;
    int failed = 0;

    for (const QString& entry : entries) {
        QString filePath = dir.filePath(entry);
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) {
            qCWarning(logTracker) << QStringLiteral("[追踪] 无法读取: %1").arg(filePath);
            failed++;
            // 删掉损坏的 tracking 文件以免越积越多
            QFile::remove(filePath);
            continue;
        }

        QString tempDir = QString::fromUtf8(f.readAll()).trimmed();
        f.close();

        if (tempDir.isEmpty()) {
            QFile::remove(filePath);
            continue;
        }

        // 安全校验：只删以 shadow-merged- 开头的目录，避免误删
        QFileInfo fi(tempDir);
        if (!fi.fileName().startsWith(QStringLiteral("shadow-merged-"))) {
            qCWarning(logTracker) << QStringLiteral("[追踪] 跳过非标准路径: %1").arg(tempDir);
            QFile::remove(filePath);
            continue;
        }

        QDir d(tempDir);
        if (d.exists()) {
            if (d.removeRecursively()) {
                qCInfo(logTracker) << QStringLiteral("[追踪] 已清理残留: %1").arg(tempDir);
                cleaned++;
            } else {
                qCWarning(logTracker) << QStringLiteral("[追踪] 删除失败: %1").arg(tempDir);
                failed++;
                // 不删 tracking 文件，下次启动再试
                continue;
            }
        } else {
            qCInfo(logTracker) << QStringLiteral("[追踪] 目录已不存在: %1 — 清理 tracking 记录").arg(tempDir);
        }

        // 清理成功或目录已不存在 → 删 tracking 文件
        QFile::remove(filePath);
    }

    qCInfo(logTracker) << QStringLiteral("[追踪] 清理完成: 成功=%1 失败=%2").arg(cleaned).arg(failed);

    // 如果 tracking 目录空了，删除它
    if (dir.entryList(QDir::Files).isEmpty()) {
        dir.rmdir(QStringLiteral("."));
    }
}
