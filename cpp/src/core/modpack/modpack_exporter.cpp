// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_exporter.cpp — 整合包导出（.mrpack，Modrinth 格式）。
//
// 后台线程（QtConcurrent::run）执行全部磁盘 IO 与压缩，主线程只收进度/结果
// 信号（invokeMethod 回投）。线程约定：ZipArchive 单工作线程串行调用。

#include "modpack_exporter.h"

#include "zip_archive.h"
#include "../../utils/logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtConcurrent>

#include <memory>

namespace ShadowLauncher {

ModpackExporter::ModpackExporter(QObject* parent) : QObject(parent) {}

void ModpackExporter::setProgress(qreal p, const QString& text)
{
    // 后台线程调用 → 回主线程更新（跨线程写成员有数据竞争）
    QMetaObject::invokeMethod(this, [this, p, text]() {
        m_progress = p;
        m_statusText = text;
        emit progressChanged();
    });
}

void ModpackExporter::cancel()
{
    m_cancel.storeRelaxed(1);
}

void ModpackExporter::exportVersion(const QString& versionId, const QString& displayName,
                                    bool includeSaves, bool includeResourcepacks,
                                    bool includeShaderpacks, const QString& outPath)
{
    if (m_busy) return;
    if (versionId.isEmpty() || outPath.isEmpty()) {
        emit finished(false, outPath, tr("参数不完整"));
        return;
    }
    m_busy = true;
    m_cancel.storeRelaxed(0);
    setProgress(0.0, tr("准备导出..."));
    emit busyChanged();

    const QString gameDir = m_gameDir;

    // ── 后台打包线程：不碰 QObject 非原子成员 ──
    QtConcurrent::run([this, gameDir, versionId, displayName,
                       includeSaves, includeResourcepacks, includeShaderpacks, outPath]() {
        auto finish = [this, outPath](bool ok, const QString& err) {
            const QString out = outPath;
            QMetaObject::invokeMethod(this, [this, ok, out, err]() {
                m_busy = false;
                emit busyChanged();
                emit finished(ok, out, err);
            });
        };

        const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
        const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
        if (!QFileInfo::exists(jsonPath)) {
            finish(false, tr("版本 %1 不存在或已损坏").arg(versionId));
            return;
        }

        // ── 1. 解析版本 JSON：MC 版本 + 加载器依赖 ──
        QString mcVersion;
        QJsonObject deps;   // key: minecraft/forge/fabric-loader/neoforge/quilt-loader
        {
            QFile f(jsonPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
                mcVersion = root.value(QStringLiteral("id")).toString();
                const QJsonArray libs = root.value(QStringLiteral("libraries")).toArray();
                for (const auto& lv : libs) {
                    const QString name = lv.toObject().value(QStringLiteral("name")).toString();
                    const int lastColon = name.lastIndexOf(QLatin1Char(':'));
                    if (lastColon <= 0) continue;
                    const QString ver = name.mid(lastColon + 1);
                    if (name.startsWith(QStringLiteral("net.minecraftforge:forge:")))
                        deps.insert(QStringLiteral("forge"), ver);
                    else if (name.contains(QStringLiteral("fabric-loader")))
                        deps.insert(QStringLiteral("fabric-loader"), ver);
                    else if (name.startsWith(QStringLiteral("net.neoforged:neoforge:")))
                        deps.insert(QStringLiteral("neoforge"), ver);
                    else if (name.contains(QStringLiteral("quilt-loader")))
                        deps.insert(QStringLiteral("quilt-loader"), ver);
                }
            }
        }
        if (mcVersion.isEmpty()) {
            finish(false, tr("无法解析版本 %1 的元数据").arg(versionId));
            return;
        }

        // ── 2. 收集 mods（排除 .disabled）──
        struct ModEntry { QString diskPath; QString relPath; };
        QList<ModEntry> mods;
        QDir modsDir(gameDir + QStringLiteral("/mods"));
        if (modsDir.exists()) {
            QDirIterator it(modsDir.absolutePath(), QStringList() << QStringLiteral("*.jar"),
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString p = it.next();
                if (p.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) continue;
                mods.append({p, modsDir.relativeFilePath(p)});
            }
        }

        // ── 3. overrides 条目（config 常含；saves/resourcepacks/shaderpacks 按开关）──
        struct OverrideEntry { QString diskPath; QString relPath; bool isDir; };
        QList<OverrideEntry> overrides;
        auto addOverrideDir = [&](const QString& sub) {
            const QString d = gameDir + QStringLiteral("/") + sub;
            if (QDir(d).exists()) overrides.append({d, sub, true});
        };
        auto addOverrideFile = [&](const QString& name) {
            const QString p = gameDir + QStringLiteral("/") + name;
            if (QFileInfo::exists(p)) overrides.append({p, name, false});
        };
        addOverrideDir(QStringLiteral("config"));
        addOverrideFile(QStringLiteral("options.txt"));
        addOverrideFile(QStringLiteral("servers.dat"));
        if (includeResourcepacks) addOverrideDir(QStringLiteral("resourcepacks"));
        if (includeShaderpacks)   addOverrideDir(QStringLiteral("shaderpacks"));
        if (includeSaves)         addOverrideDir(QStringLiteral("saves"));

        // ── 4. 计算 mods 哈希（sha1 + sha512）并构建 index.json ──
        QJsonArray filesArr;
        const int modTotal = mods.size();
        for (int i = 0; i < modTotal; ++i) {
            if (m_cancel.loadRelaxed()) { finish(false, tr("已取消")); return; }
            const auto& m = mods[i];
            QFile f(m.diskPath);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray data = f.readAll();
            f.close();
            const QString sha1 = QString::fromLatin1(
                QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
            const QString sha512 = QString::fromLatin1(
                QCryptographicHash::hash(data, QCryptographicHash::Sha512).toHex());
            QJsonObject fileObj;
            fileObj.insert(QStringLiteral("path"), QStringLiteral("mods/") + m.relPath);
            QJsonObject hashes;
            hashes.insert(QStringLiteral("sha1"), sha1);
            hashes.insert(QStringLiteral("sha512"), sha512);
            fileObj.insert(QStringLiteral("hashes"), hashes);
            filesArr.append(fileObj);
            setProgress(0.05 + 0.15 * (i + 1) / qMax(1, modTotal), tr("计算模组哈希 %1/%2").arg(i + 1).arg(modTotal));
        }

        QJsonObject index;
        index.insert(QStringLiteral("formatVersion"), 1);
        index.insert(QStringLiteral("game"), mcVersion);
        index.insert(QStringLiteral("versionId"), displayName);
        index.insert(QStringLiteral("name"), displayName);
        index.insert(QStringLiteral("summary"), QString());
        index.insert(QStringLiteral("files"), filesArr);
        if (!deps.isEmpty()) {
            QJsonObject depsObj;
            for (auto it = deps.begin(); it != deps.end(); ++it)
                depsObj.insert(it.key(), it.value());
            index.insert(QStringLiteral("dependencies"), depsObj);
        }
        const QByteArray indexJson = QJsonDocument(index).toJson(QJsonDocument::Indented);

        // ── 5. 打包 ──
        // 总文件数预估：mods + overrides（目录先递归计数）——先收集再算
        QStringList overrideFiles;
        qint64 overrideBytes = 0;
        for (const auto& o : overrides) {
            if (o.isDir) {
                QDirIterator it(o.diskPath, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    overrideFiles.append(it.next());
                    overrideBytes += QFileInfo(it.filePath()).size();
                }
            } else {
                overrideFiles.append(o.diskPath);
                overrideBytes += QFileInfo(o.diskPath).size();
            }
        }
        const int totalFiles = 1 + mods.size() + overrideFiles.size();
        int done = 0;

        ZipArchive zip;
        if (!zip.openForWrite(outPath)) {
            finish(false, zip.error());
            return;
        }

        // index.json
        if (!zip.addData(QStringLiteral("modrinth.index.json"), indexJson)) {
            finish(false, zip.error());
            return;
        }
        ++done;

        // mods
        for (const auto& m : mods) {
            if (m_cancel.loadRelaxed()) { zip.closeWrite(); QFile::remove(outPath); finish(false, tr("已取消")); return; }
            if (!zip.addFile(m.diskPath, QStringLiteral("mods/") + m.relPath)) {
                finish(false, zip.error());
                return;
            }
            ++done;
            setProgress(0.2 + 0.6 * done / qMax(1, totalFiles), tr("打包模组 %1/%2").arg(done).arg(totalFiles));
        }

        // overrides（目录整体 → overrides/ 前缀）
        for (const auto& f : overrideFiles) {
            if (m_cancel.loadRelaxed()) { zip.closeWrite(); QFile::remove(outPath); finish(false, tr("已取消")); return; }
            QString rel = QDir(gameDir).relativeFilePath(f);
            const QString entry = QStringLiteral("overrides/") + rel;
            if (!zip.addFile(f, entry)) {
                finish(false, zip.error());
                return;
            }
            ++done;
            setProgress(0.2 + 0.6 * done / qMax(1, totalFiles), tr("打包覆写文件 %1/%2").arg(done).arg(totalFiles));
        }

        if (!zip.closeWrite()) {
            QFile::remove(outPath);
            finish(false, zip.error());
            return;
        }

        setProgress(1.0, tr("导出完成"));
        qCInfo(logMod) << QStringLiteral("[整合包] 导出完成 %1 → %2 (%3 文件)").arg(displayName, outPath).arg(totalFiles);
        finish(true, QString());
    });
}

} // namespace ShadowLauncher
