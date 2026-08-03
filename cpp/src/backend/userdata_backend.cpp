// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "userdata_backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent>

#include <QtCore/private/qzipwriter_p.h>
#include <QtCore/private/qzipreader_p.h>

#include "../utils/logger.h"

namespace ShadowLauncher {

UserDataBackend::UserDataBackend(QObject* parent)
    : QObject(parent)
{
}

QVariantList UserDataBackend::pendingItems() const
{
    QVariantList result;
    for (const auto& item : m_validatedItems) {
        QVariantMap m;
        m["name"] = item.name;
        m["displayName"] = item.displayName;
        m["relPath"] = item.relPath;
        m["sizeBytes"] = item.sizeBytes;
        m["isDir"] = item.isDir;
        m["riskLevel"] = item.riskLevel;
        m["warning"] = item.warning;
        result.append(m);
    }
    return result;
}

// ─── Export ───────────────────────────────────────────────────

void UserDataBackend::exportUserData(const QString& gameDir, const QString& versionId,
                                      const QString& outputPath)
{
    if (m_exporting) {
        emit logMessage("[userdata] Export already in progress");
        return;
    }
    m_exporting = true;
    emit exportStateChanged();
    emit logMessage(QStringLiteral("[userdata] Exporting user data for %1 → %2")
                        .arg(versionId, outputPath));

    // Run in worker thread to keep UI responsive
    QtConcurrent::run([this, gameDir, versionId, outputPath]() {
        doExport(gameDir, versionId, outputPath);
    });
}

void UserDataBackend::doExport(const QString& gameDir, const QString& versionId,
                                const QString& outputPath)
{
    // 用户数据现在直接位于版本目录根（旧版曾有 {版本}/game 子目录，已废弃，不再查找）
    QString verDir = gameDir + "/versions/" + versionId;
    QDir vd(verDir);
    if (!vd.exists()) {
        qCWarning(logApp) << "[userdata] 导出失败: 版本目录不存在" << verDir;
        QMetaObject::invokeMethod(this, [this, outputPath, verDir]() {
            m_exporting = false;
            emit exportStateChanged();
            emit exportFinished(false, outputPath, QStringLiteral("版本目录不存在: %1").arg(verDir));
            emit logMessage("[userdata] Export failed: version directory not found");
        }, Qt::QueuedConnection);
        return;
    }

    emit exportProgress(5, QStringLiteral("准备中..."));

    // Step 1: Create temp directory
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCWarning(logApp) << "[userdata] 导出失败: 无法创建临时目录";
        QMetaObject::invokeMethod(this, [this, outputPath]() {
            m_exporting = false;
            emit exportStateChanged();
            emit exportFinished(false, outputPath, QStringLiteral("无法创建临时目录"));
        }, Qt::QueuedConnection);
        return;
    }
    QString tempPath = tempDir.path();
    emit exportProgress(10, QStringLiteral("复制数据..."));

    // Step 2: Copy user data to temp root（直接散开，zip 内不再包 game/ 目录）
    QString tempGame = tempPath;

    // Recursive copy
    std::function<void(const QString&, const QString&)> copyDir =
        [&](const QString& srcPath, const QString& dstPath) {
        QDir srcD(srcPath);
        QDir dstD(dstPath);
        dstD.mkpath(".");
        QFileInfoList entries = srcD.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& info : entries) {
            QString dst = dstPath + "/" + info.fileName();
            if (info.isDir()) {
                copyDir(info.absoluteFilePath(), dst);
            } else {
                QFile::copy(info.absoluteFilePath(), dst);
            }
        }
    };

    // 用户数据白名单（对齐导入端 rules；*.jar / *.json / downloads / natives 等版本本体与运行缓存不导出）
    static const QStringList dataNames = {
        QStringLiteral("saves"),          // 存档世界
        QStringLiteral("mods"),           // 模组
        QStringLiteral("config"),         // 模组配置
        QStringLiteral("resourcepacks"),  // 资源包
        QStringLiteral("shaderpacks"),    // 光影包
        QStringLiteral("screenshots"),    // 截图
        QStringLiteral("logs"),           // 日志
        QStringLiteral("defaultconfigs"), // 默认配置
        QStringLiteral("profilekeys"),    // 玩家密钥
        QStringLiteral("crash-reports"),  // 崩溃报告
        QStringLiteral("options.txt"),    // 游戏设置
        QStringLiteral("optionsof.txt"),  // OptiFine 设置
        QStringLiteral("servers.dat"),    // 服务器列表
        QStringLiteral("usercache.json"), // 用户缓存
        QStringLiteral("launcher_profiles.json"),
        QStringLiteral("allowed_sellers.json"),
    };
    int copiedCount = 0;
    for (const QString& name : dataNames) {
        QString src = verDir + "/" + name;
        if (!QFileInfo::exists(src)) continue;
        QString dst = tempGame + "/" + name;
        if (QFileInfo(src).isDir()) {
            copyDir(src, dst);
        } else {
            QDir().mkpath(QFileInfo(dst).absolutePath());
            QFile::copy(src, dst);
        }
        ++copiedCount;
    }
    if (copiedCount == 0) {
        qCWarning(logApp) << "[userdata] 导出失败: 版本目录未找到可导出的用户数据" << verDir;
        QMetaObject::invokeMethod(this, [this, outputPath]() {
            m_exporting = false;
            emit exportStateChanged();
            emit exportFinished(false, outputPath, QStringLiteral("版本目录中未找到可导出的用户数据"));
        }, Qt::QueuedConnection);
        return;
    }

    // Count total size for progress
    qint64 totalSize = 0;
    qint64 copiedSize = 0;
    std::function<void(const QString&)> countSize = [&](const QString& path) {
        QDir d(path);
        QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& info : entries) {
            if (info.isDir()) countSize(info.absoluteFilePath());
            else totalSize += info.size();
        }
    };
    countSize(tempGame);

    emit exportProgress(50, totalSize > 0
        ? QStringLiteral("打包中 (%1 MB)...").arg(totalSize / 1024.0 / 1024.0, 0, 'f', 1)
        : QStringLiteral("打包中..."));

    // Step 3: Generate README.txt
    QString versionJsonPath = gameDir + "/versions/" + versionId + "/" + versionId + ".json";
    QString loaderType, loaderVersion;
    {
        QFile vf(versionJsonPath);
        if (vf.open(QIODevice::ReadOnly)) {
            QByteArray raw = vf.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                // Detect loader from arguments
                QJsonArray gameArgs = obj["arguments"].toObject()["game"].toArray();
                for (const QJsonValue& val : gameArgs) {
                    if (val.isString()) {
                        QString s = val.toString();
                        if (s.startsWith("--fml.neoForgeVersion")) {
                            loaderType = "neoforge";
                        } else if (s.startsWith("--fml.forgeVersion")) {
                            loaderType = "forge";
                        } else if (s.startsWith("--fabric")) {
                            loaderType = "fabric";
                        }
                    }
                }
                if (obj.contains("inheritsFrom")) {
                    // Version inherits from MC - extract actual MC version
                    // For simplicity: use the version ID to guess MC version
                }
            }
        }
    }

    // Extract MC version from versionId (e.g., "26.2-neoforge-26.2.0.7-beta" → "26.2")
    QString mcVersion = versionId;
    int dashPos = mcVersion.indexOf('-');
    if (dashPos > 0) mcVersion = mcVersion.left(dashPos);

    QString readme;
    QTextStream rs(&readme);
    rs << "# Shadow Launcher User Data Export\n";
    rs << "version_id = " << versionId << "\n";
    rs << "mc_version = " << mcVersion << "\n";
    rs << "loader = " << (loaderType.isEmpty() ? "vanilla" : loaderType) << "\n";
    rs << "loader_version = " << (loaderVersion.isEmpty() ? "N/A" : loaderVersion) << "\n";
    rs << "export_date = " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";

    QFile readmeFile(tempPath + "/README.txt");
    if (readmeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        readmeFile.write(readme.toUtf8());
        readmeFile.close();
    }

    emit exportProgress(65, QStringLiteral("压缩中..."));

    // Step 4: Create ZIP
    QZipWriter zip(outputPath);
    if (zip.status() != QZipWriter::NoError) {
        qCWarning(logApp) << "[userdata] 导出失败: 无法创建ZIP文件" << outputPath << "status=" << static_cast<int>(zip.status());
        QMetaObject::invokeMethod(this, [this, outputPath]() {
            m_exporting = false;
            emit exportStateChanged();
            emit exportFinished(false, outputPath, QStringLiteral("无法创建ZIP文件"));
        }, Qt::QueuedConnection);
        return;
    }

    std::function<void(const QString&, const QString&)> addToZip =
        [&](const QString& dirPath, const QString& zipPrefix) {
        QDir d(dirPath);
        QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& info : entries) {
            QString relPath = zipPrefix + info.fileName();
            if (info.isDir()) {
                addToZip(info.absoluteFilePath(), relPath + "/");
            } else {
                QFile file(info.absoluteFilePath());
                if (file.open(QIODevice::ReadOnly)) {
                    // zlib compression level 6 (good balance)
                    zip.addFile(relPath, file.readAll());
                }
            }
        }
    };

    addToZip(tempPath, "");  // includes README.txt and game/
    zip.close();

    // Cleanup temp dir happens automatically when QTemporaryDir goes out of scope

    QMetaObject::invokeMethod(this, [this, outputPath]() {
        m_exporting = false;
        emit exportStateChanged();
        emit exportFinished(true, outputPath, QString());
        emit logMessage(QStringLiteral("[userdata] Export complete: %1").arg(outputPath));
    }, Qt::QueuedConnection);
    qCInfo(logApp) << "[userdata] 导出完成" << outputPath << "项数=" << copiedCount;
}

// ─── Validate / Parse ────────────────────────────────────────

QVector<ImportItemInfo> UserDataBackend::parseArchiveItems(const QString& archivePath,
                                                             const QString& gameDir,
                                                             QString& outVersionId,
                                                             QString& outMcVersion,
                                                             QString& outLoader,
                                                             QString& outLoaderVer,
                                                             QString& errorMsg)
{
    QVector<ImportItemInfo> items;

    // Open ZIP
    QZipReader zip(archivePath);
    if (zip.status() != QZipReader::NoError) {
        errorMsg = QStringLiteral("不是有效的ZIP文件");
        return items;
    }

    // Check for README.txt
    QByteArray readmeData = zip.fileData("README.txt");
    if (readmeData.isEmpty()) {
        errorMsg = QStringLiteral("缺少描述文件 README.txt，无法识别");
        return items;
    }

    // Parse README.txt
    QString readme = QString::fromUtf8(readmeData);
    QMap<QString, QString> meta;
    QStringList lines = readme.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
        int eqPos = trimmed.indexOf('=');
        if (eqPos < 0) continue;
        QString key = trimmed.left(eqPos).trimmed();
        QString val = trimmed.mid(eqPos + 1).trimmed();
        meta[key] = val;
    }

    outVersionId = meta.value("version_id");
    outMcVersion = meta.value("mc_version");
    outLoader = meta.value("loader", "vanilla");
    outLoaderVer = meta.value("loader_version");

    if (outVersionId.isEmpty()) {
        errorMsg = QStringLiteral("描述文件格式无效：缺少 version_id");
        return items;
    }

    // 兼容两种包结构：旧包有 game/ 前缀，新导出格式直接散开（saves/mods/... 在 zip 根）
    QList<QZipReader::FileInfo> allFiles = zip.fileInfoList();
    bool hasGameDir = false;
    for (const auto& fi : allFiles) {
        if (fi.filePath.startsWith("game/")) {
            hasGameDir = true;
            break;
        }
    }
    if (!hasGameDir) {
        // 新格式：检查 zip 根是否有已知用户数据条目
        static const QStringList knownTops = {
            QStringLiteral("saves"), QStringLiteral("mods"), QStringLiteral("config"),
            QStringLiteral("resourcepacks"), QStringLiteral("shaderpacks"), QStringLiteral("screenshots"),
            QStringLiteral("logs"), QStringLiteral("defaultconfigs"), QStringLiteral("profilekeys"),
            QStringLiteral("crash-reports"), QStringLiteral("options.txt"), QStringLiteral("optionsof.txt"),
            QStringLiteral("servers.dat"), QStringLiteral("usercache.json"),
            QStringLiteral("launcher_profiles.json"), QStringLiteral("allowed_sellers.json"),
        };
        bool hasTop = false;
        for (const auto& fi : allFiles) {
            QString p = fi.filePath;
            if (p == QStringLiteral("README.txt") || p.isEmpty() || p.endsWith(QLatin1Char('/'))) continue;
            int slash = p.indexOf(QLatin1Char('/'));
            QString top = (slash >= 0) ? p.left(slash) : p;
            if (knownTops.contains(top)) { hasTop = true; break; }
        }
        if (!hasTop) {
            errorMsg = QStringLiteral("数据目录结构异常：缺少用户数据目录");
            return items;
        }
    }

    // Classify items inside game/
    struct RiskRule {
        QString name;
        QString display;
        bool isDir;
        int risk;  // 0=safe, 1=caution, 2=dangerous
        QString warning;
    };

    QVector<RiskRule> rules = {
        {"saves",         "存档世界",    true,  0, ""},
        {"resourcepacks", "资源包",      true,  0, ""},
        {"shaderpacks",   "光影包",      true,  0, ""},
        {"screenshots",   "截图",        true,  0, ""},
        {"options.txt",   "游戏设置",    false, 0, ""},
        {"servers.dat",   "服务器列表",  false, 0, ""},
        {"mods",          "模组",        true,  2, "危险！模组与版本/加载器强绑定，可能导致游戏崩溃"},
        {"config",        "模组配置",    true,  2, "危险！配置文件可能与新版本不兼容"},
        {"defaultconfigs","默认配置",    true,  1, "注意：安装时会被加载器默认配置覆盖"},
        {"profilekeys",   "玩家密钥",    true,  1, "注意：密钥可能因账号变更而失效"},
    };

    QSet<QString> foundDirs;
    for (const auto& fi : allFiles) {
        QString path = fi.filePath;
        QString rel;
        if (hasGameDir) {
            if (!path.startsWith("game/")) continue;
            rel = path.mid(5);  // strip "game/"
        } else {
            if (path == QStringLiteral("README.txt") || path.endsWith(QLatin1Char('/'))) continue;
            rel = path;
        }
        if (rel.isEmpty()) continue;

        // Get top-level entry name
        int slashPos = rel.indexOf('/');
        QString topName = (slashPos >= 0) ? rel.left(slashPos) : rel;
        if (topName.isEmpty()) continue;
        foundDirs.insert(topName);
    }

    for (const auto& rule : rules) {
        if (foundDirs.contains(rule.name)) {
            ImportItemInfo info;
            info.name = rule.name;
            info.displayName = rule.display;
            info.relPath = hasGameDir ? ("game/" + rule.name) : rule.name;
            info.isDir = rule.isDir;
            info.riskLevel = rule.risk;
            info.warning = rule.warning;
            // Estimate size
            for (const auto& fi : allFiles) {
                if (fi.filePath.startsWith(info.relPath)) {
                    info.sizeBytes += fi.size;  // uncompressed size
                }
            }
            items.append(info);
        }
    }

    if (items.isEmpty()) {
        errorMsg = QStringLiteral("无可导入的有效用户数据");
    }

    return items;
}

void UserDataBackend::validateArchive(const QString& gameDir, const QString& archivePath)
{
    emit logMessage(QStringLiteral("[userdata] Validating archive: %1").arg(archivePath));

    // Run parsing in worker thread
    QtConcurrent::run([this, gameDir, archivePath]() {
        QString versionId, mcVersion, loader, loaderVer, errorMsg;
        QVector<ImportItemInfo> items = parseArchiveItems(
            archivePath, gameDir, versionId, mcVersion, loader, loaderVer, errorMsg);

        QVariantList itemList;
        for (const auto& item : items) {
            QVariantMap m;
            m["name"] = item.name;
            m["displayName"] = item.displayName;
            m["relPath"] = item.relPath;
            m["sizeBytes"] = item.sizeBytes;
            m["isDir"] = item.isDir;
            m["riskLevel"] = item.riskLevel;
            m["warning"] = item.warning;
            itemList.append(m);
        }

        bool success = !items.isEmpty() && errorMsg.isEmpty();
        QMetaObject::invokeMethod(this, [this, success, archivePath, itemList, errorMsg, items, versionId, mcVersion, loader, loaderVer]() {
            m_validatedItems = items;
            m_srcVersionId = versionId;
            m_srcMcVersion = mcVersion;
            m_srcLoader = loader;
            m_srcLoaderVer = loaderVer;
            emit validateFinished(success, archivePath, itemList, errorMsg);
        }, Qt::QueuedConnection);
    });
}

// ─── Pending Import ────────────────────────────────────────────

void UserDataBackend::setPendingImport(const QString& archivePath, const QVariantList& /*items*/)
{
    m_pendingArchivePath = archivePath;
    emit pendingArchiveChanged();
    emit logMessage(QStringLiteral("[userdata] Pending import set: %1").arg(archivePath));
}

void UserDataBackend::cancelPendingImport()
{
    m_pendingArchivePath.clear();
    m_validatedItems.clear();
    emit pendingArchiveChanged();
    emit logMessage("[userdata] Pending import cancelled");
}

// ─── Execute Import ──────────────────────────────────────────

void UserDataBackend::executeImport(const QString& gameDir, const QString& targetVersionId)
{
    if (m_pendingArchivePath.isEmpty()) {
        emit importFinished(false, targetVersionId, QStringLiteral("没有待导入的用户数据"));
        return;
    }

    m_importing = true;
    emit importStateChanged();
    emit logMessage(QStringLiteral("[userdata] Importing user data to %1").arg(targetVersionId));

    QtConcurrent::run([this, gameDir, targetVersionId]() {
        // 用户数据直接写入目标版本目录根（旧版曾有 {版本}/game 子目录，已废弃）
        QString targetVerDir = gameDir + "/versions/" + targetVersionId;
        QDir().mkpath(targetVerDir);

        // Open ZIP
        QZipReader zip(m_pendingArchivePath);
        if (zip.status() != QZipReader::NoError) {
            QMetaObject::invokeMethod(this, [this, targetVersionId]() {
                m_importing = false;
                emit importStateChanged();
                emit importFinished(false, targetVersionId, QStringLiteral("ZIP文件读取失败"));
                emit logMessage("[userdata] Import failed: cannot open ZIP");
            }, Qt::QueuedConnection);
            return;
        }

        QList<QZipReader::FileInfo> allFiles = zip.fileInfoList();

        // 兼容两种包结构：旧包 game/ 前缀，新导出格式直接散开
        bool hasGameDir = false;
        for (const auto& fi : allFiles) {
            if (fi.filePath.startsWith("game/")) { hasGameDir = true; break; }
        }
        auto stripPrefix = [hasGameDir](const QString& fp) -> QString {
            return hasGameDir ? fp.mid(5) : fp;
        };

        int totalItems = 0;
        int processedItems = 0;

        // Count items to process
        for (const auto& fi : allFiles) {
            QString fp = fi.filePath;
            if (fi.isDir) continue;
            if (hasGameDir) {
                if (!fp.startsWith("game/")) continue;
            } else if (fp == QStringLiteral("README.txt")) {
                continue;
            }
            totalItems++;
        }

        // Import each file
        for (const auto& fi : allFiles) {
            if (fi.isDir) continue;  // directories auto-created
            QString fp = fi.filePath;
            if (hasGameDir) {
                if (!fp.startsWith("game/")) continue;
            } else if (fp == QStringLiteral("README.txt")) {
                continue;  // 描述文件不导入
            }

            QString relPath = stripPrefix(fp);
            QString dstPath = targetVerDir + "/" + relPath;
            QDir().mkpath(QFileInfo(dstPath).absolutePath());

            // Conflict resolution for options.txt and servers.dat
            if (QFileInfo::exists(dstPath)) {
                QString fileName = QFileInfo(dstPath).fileName();
                if (fileName == "options.txt") {
                    // Skip: don't overwrite new settings
                    processedItems++;
                    continue;
                }
                if (fileName == "servers.dat") {
                    // TODO: merge servers.dat (for now, skip if exists)
                    processedItems++;
                    continue;
                }
            }

            // Write file
            QFile out(dstPath);
            if (out.open(QIODevice::WriteOnly)) {
                QByteArray data = zip.fileData(fi.filePath);
                out.write(data);
                out.close();
            }

            processedItems++;
            if (totalItems > 0) {
                int pct = 10 + (processedItems * 85 / totalItems);
                emit importProgress(pct, QStringLiteral("导入中... (%1/%2)")
                                               .arg(processedItems).arg(totalItems));
            }
        }

        zip.close();

        // Cleanup temp (ZIP is not extracted to temp in this simple approach;
        // QZipReader reads directly from the archive)
        m_pendingArchivePath.clear();
        m_validatedItems.clear();

        QMetaObject::invokeMethod(this, [this, targetVersionId]() {
            m_importing = false;
            emit importStateChanged();
            emit pendingArchiveChanged();
            emit importFinished(true, targetVersionId, QString());
            emit logMessage(QStringLiteral("[userdata] Import complete for %1").arg(targetVersionId));
        }, Qt::QueuedConnection);
    });
}

} // namespace ShadowLauncher
