// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// MigrationManager implementation — v0.4.0-beta directory restructure migration.

#include "migration_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QProcess>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QProgressDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStorageInfo>
#include <QDateTime>
#include "../utils/logger.h"

Q_LOGGING_CATEGORY(logMigrate, "Shadow.Migration")

// ═════════════════════════════════════════════════════════════════════════════
// Public
// ═════════════════════════════════════════════════════════════════════════════

MigrationManager::MigrationManager(QObject* parent)
    : QObject(parent)
{
}

bool MigrationManager::needsMigration()
{
    QString appDir = QCoreApplication::applicationDirPath();
    bool hasLauncher = QDir(appDir + QStringLiteral("/launcher")).exists();
    bool hasFlag = QFileInfo::exists(appDir + QStringLiteral("/.migrated"));
    bool migrating = QFileInfo::exists(appDir + QStringLiteral("/.migrating"));
    // If .migrating exists, previous run crashed mid-migration — retry
    return (!hasLauncher || migrating) && !hasFlag;
}

// ═════════════════════════════════════════════════════════════════════════════
// Run migration (synchronous, blocks caller via QEventLoop)
// ═════════════════════════════════════════════════════════════════════════════

bool MigrationManager::runMigration(QString& errorMsg)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString launcherDir = appDir + QStringLiteral("/launcher");
    QString binDir = launcherDir + QStringLiteral("/bin");
    QString updateDir = appDir + QStringLiteral("/_update");

    // ── Determine version tag from current exe or fallback ──
    // The full package filename: ShadowLauncher_v0.4.0-beta.7z
    // We read SHADOW_DISPLAY_VERSION from the build (compiled-in define)
    QString versionTag = QStringLiteral("v0.4.0-beta");
    // We can't access SHADOW_DISPLAY_VERSION from here (it's in main.cpp defines),
    // so we detect from appVersion()
    QString appVer = QCoreApplication::applicationVersion();
    if (!appVer.isEmpty())
        versionTag = QStringLiteral("v") + appVer;

    QString fullPackageName = QStringLiteral("ShadowLauncher_%1.7z").arg(versionTag);
    QString fullPackageUrl = m_baseUrl + QStringLiteral("/download/") + versionTag
                             + QStringLiteral("/") + fullPackageName;
    QString sevenZrUrl = m_baseUrl + QStringLiteral("/download/") + versionTag
                         + QStringLiteral("/7zr.exe");

    QString archivePath = updateDir + QStringLiteral("/") + fullPackageName;
    QString sevenZrPath = binDir + QStringLiteral("/7zr.exe");

    // ── Disk space check (need ~100MB extra for download + extraction) ──
    {
        QStorageInfo storage(appDir);
        if (storage.isValid() && storage.bytesAvailable() >= 0) {
            qint64 availMB = storage.bytesAvailable() / (1024 * 1024);
            if (availMB < 200) {
                errorMsg = QStringLiteral("磁盘空间不足（仅剩 %1 MB），需要至少 200 MB 可用空间来完成更新")
                               .arg(availMB);
                qCWarning(logMigrate) << errorMsg;
                return false;
            }
        }
    }

    qCInfo(logMigrate) << QStringLiteral("[迁移] 开始 版本=%1 全量包=%2")
                          .arg(versionTag, fullPackageName);

    // ── Write .migrating flag (crash recovery) ──
    {
        QFile f(appDir + QStringLiteral("/.migrating"));
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

    // ── Step 1: Create launcher/bin/ ──
    QDir().mkpath(binDir);
    QDir().mkpath(updateDir);

    // ── Step 2: Download 7zr.exe (只依赖下载，不弹进度框) ──
    qCInfo(logMigrate) << "[迁移] 下载 7zr.exe...";
    if (!downloadTo(sevenZrUrl, sevenZrPath, 60000,
                    QStringLiteral("下载 7zr.exe..."), 0, 10, errorMsg)) {
        return false;
    }

    // ── Step 3: Download full package ──
    qCInfo(logMigrate) << "[迁移] 下载全量包...";
    if (!downloadTo(fullPackageUrl, archivePath, 300000,
                    QStringLiteral("下载全量包..."), 10, 70, errorMsg)) {
        return false;
    }

    // ── Step 4: Extract with 7zr.exe ──
    // 排除 ShadowLauncher.exe（已通过增量更新替换）
    qCInfo(logMigrate) << "[迁移] 解压中...";
    QStringList excludes;
    excludes << QStringLiteral("ShadowLauncher.exe");
    if (!extractPackage(archivePath, appDir, excludes, 120000, errorMsg)) {
        return false;
    }

    // ── Step 5: Clean up ──
    QFile::remove(archivePath);

    // ── Remove .migrating flag ──
    QFile::remove(appDir + QStringLiteral("/.migrating"));

    // ── Step 6: Write .migrated flag ──
    QFile flagFile(appDir + QStringLiteral("/.migrated"));
    if (flagFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QByteArray content = QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8();
        flagFile.write(content);
        flagFile.close();
    }
    qCInfo(logMigrate) << "[迁移] 完成";

    // 重置 errorMsg（成功时清空）
    errorMsg.clear();
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Private helpers
// ═════════════════════════════════════════════════════════════════════════════

bool MigrationManager::downloadTo(const QString& url, const QString& destPath,
                                   int timeoutMs,
                                   const QString& stageLabel, int basePercent, int rangePercent,
                                   QString& errorMsg)
{
    QDir().mkpath(QFileInfo(destPath).absolutePath());

    QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setTransferTimeout(timeoutMs);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    bool finished = false;
    QString failMsg;

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);

    // Use a temp file to avoid partial write on failure
    QString tmpPath = destPath + QStringLiteral(".download");
    QFile file(tmpPath);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [&](qint64 received, qint64 total) {
        if (total > 0) {
            int pct = basePercent + static_cast<int>(rangePercent * received / total);
            emit progressChanged(stageLabel, qBound(basePercent, pct, basePercent + rangePercent));
        }
    });

    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();

        if (reply->error() != QNetworkReply::NoError) {
            failMsg = QStringLiteral("网络错误: %1 (HTTP %2)")
                          .arg(reply->errorString())
                          .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        } else {
            // Success: write received data to file
            QByteArray data = reply->readAll();
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                // Atomic rename
                QFile::remove(destPath);
                if (QFile::rename(tmpPath, destPath)) {
                    finished = true;
                } else {
                    failMsg = QStringLiteral("无法写入目标文件: %1").arg(destPath);
                }
            } else {
                failMsg = QStringLiteral("无法创建临时文件: %1").arg(tmpPath);
            }
        }
    } else {
        // Timeout
        failMsg = QStringLiteral("下载超时（%1 秒）").arg(timeoutMs / 1000);
    }

    reply->deleteLater();

    // Clean up temp file on failure
    if (!finished)
        QFile::remove(tmpPath);

    if (!finished) {
        errorMsg = QStringLiteral("%1: %2").arg(stageLabel, failMsg);
        qCWarning(logMigrate) << "[迁移] 下载失败:" << errorMsg;
        return false;
    }

    qCInfo(logMigrate) << "[迁移] 下载完成:" << destPath;
    return true;
}

bool MigrationManager::extractPackage(const QString& archivePath,
                                       const QString& targetDir,
                                       const QStringList& excludes, int timeoutMs,
                                       QString& errorMsg)
{
    if (!QFileInfo::exists(archivePath)) {
        errorMsg = QStringLiteral("找不到已下载的全量包文件: %1").arg(archivePath);
        return false;
    }

    // Build 7zr.exe command
    QStringList args;
    args << QStringLiteral("x");                          // extract with full paths
    args << archivePath;
    args << QStringLiteral("-o") + QDir::toNativeSeparators(targetDir);  // output dir
    args << QStringLiteral("-y");                         // auto-yes all prompts

    // Exclude files
    for (const QString& ex : excludes)
        args << QStringLiteral("-x!") + ex;

    QString sevenZrPath = targetDir + QStringLiteral("/launcher/bin/7zr.exe");
    if (!QFileInfo::exists(sevenZrPath)) {
        errorMsg = QStringLiteral("找不到 7zr.exe: %1").arg(sevenZrPath);
        return false;
    }

    emit progressChanged(QStringLiteral("解压中..."), 80);

    qCInfo(logMigrate) << "[迁移] 解压命令:" << sevenZrPath << args;

    QProcess proc;
    proc.start(sevenZrPath, args);
    if (!proc.waitForStarted(5000)) {
        errorMsg = QStringLiteral("7zr.exe 启动失败: %1").arg(proc.errorString());
        return false;
    }

    // Poll progress from 7zr output lines
    QTimer pollTimer;
    pollTimer.setInterval(500);
    connect(&pollTimer, &QTimer::timeout, this, [&]() {
        int pct = 80;
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        // 7zr outputs lines like "75% 10 file" — extract percentage
        for (const QString& line : output.split(QStringLiteral("\n"))) {
            int pctIdx = line.indexOf(QChar('%'));
            if (pctIdx >= 1) {
                // Extract number right before '%' — could be 1-3 digits
                int start = pctIdx;
                while (start > 0 && line[start - 1].isDigit()) start--;
                bool ok = false;
                int v = line.mid(start, pctIdx - start).trimmed().toInt(&ok);
                if (ok && v >= 0 && v <= 100) {
                    pct = qMin(80 + v * 20 / 100, 99);
                    break;
                }
            }
        }
        emit progressChanged(QStringLiteral("解压中..."), pct);
    });
    pollTimer.start();

    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        pollTimer.stop();
        errorMsg = QStringLiteral("解压超时（%1 秒）").arg(timeoutMs / 1000);
        return false;
    }
    pollTimer.stop();

    if (proc.exitCode() != 0) {
        QString stderrOut = QString::fromUtf8(proc.readAllStandardError());
        errorMsg = QStringLiteral("7zr.exe 解压失败 (退出码=%1): %2")
                       .arg(proc.exitCode())
                       .arg(stderrOut.left(200));
        return false;
    }

    emit progressChanged(QStringLiteral("覆盖完成"), 100);
    qCInfo(logMigrate) << "[迁移] 解压成功";
    return true;
}
