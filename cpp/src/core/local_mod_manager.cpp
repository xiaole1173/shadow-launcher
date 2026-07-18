// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 褰?/ Shadow
#include "local_mod_manager.h"
#include "utils/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QRegularExpression>
#include "utils/logger.h"
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

#include <private/qzipreader_p.h>

namespace ShadowLauncher {

LocalModManager::LocalModManager(QObject* parent) : QObject(parent)
{
    // Icon cache in launcher data directory
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataDir.isEmpty())
        dataDir = QCoreApplication::applicationDirPath();
    m_iconCacheDir = dataDir + QStringLiteral("/cache/mod_icons");
}

void LocalModManager::setGameDir(const QString& dir) { m_gameDir = dir; }
QString LocalModManager::gameDir() const { return m_gameDir; }

QString LocalModManager::iconCacheDir() const { return m_iconCacheDir; }

QString LocalModManager::modsDir(const QString& versionId) const
{
    if (versionId.isEmpty()) return m_gameDir + QStringLiteral("/mods");

    // Isolated: .minecraft/versions/<id>/game/mods
    QString isolatedDir = m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game/mods");
    if (QDir(isolatedDir).exists() || QDir(m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game")).exists())
        return isolatedDir;

    // Non-isolated: .minecraft/mods (shared)
    return m_gameDir + QStringLiteral("/mods");
}

// 鈹€鈹€ Public API 鈹€鈹€

QVariantList LocalModManager::scanMods(const QString& versionId)
{
    QVariantList result;
    QString dir = modsDir(versionId);

    // Also check the non-isolated mods folder (root .minecraft/mods)
    QStringList searchDirs;
    searchDirs << dir;
    QString rootMods = m_gameDir + QStringLiteral("/mods");
    if (rootMods != dir && QDir(rootMods).exists())
        searchDirs << rootMods;

    QDir cacheDir(m_iconCacheDir);
    if (!cacheDir.exists()) cacheDir.mkpath(QStringLiteral("."));

    for (const QString& d : searchDirs) {
        QDir modDir(d);
        if (!modDir.exists()) continue;

        const QFileInfoList files = modDir.entryInfoList(
            {QStringLiteral("*.jar"), QStringLiteral("*.JAR")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            LocalModEntry entry = parseJar(fi.absoluteFilePath());
            if (!entry.valid) {
                // Minimal entry from filename
                entry.fileName = fi.fileName();
                entry.modName = fi.completeBaseName();
                entry.fileSize = fi.size();
                entry.fileSizeText = formatFileSize(fi.size());
                entry.valid = true; // still show it
            }
            result.append(entryToMap(entry));
        }
        break; // Only scan the first existing dir (version-isolated takes priority)
    }

    return result;
}

bool LocalModManager::importMod(const QString& filePath, const QString& versionId)
{
    QFileInfo fi(filePath);
    qCInfo(logMgr) << QStringLiteral("[本地Mod] 开始导入 源=%1 版本=%2").arg(filePath, versionId);
    if (!fi.exists() || !fi.suffix().compare(QStringLiteral("jar"), Qt::CaseInsensitive) == 0) {
        emit importFinished(fi.fileName(), false, QStringLiteral("涓嶆槸鏈夋晥鐨?Mod JAR 鏂囦欢"));
        return false;
    }

    QString dstDir = modsDir(versionId);
    QDir().mkpath(dstDir);
    QString dstPath = dstDir + QStringLiteral("/") + fi.fileName();

    // If already exists, append a counter
    if (QFile::exists(dstPath)) {
        int c = 1;
        QString base = fi.completeBaseName();
        while (QFile::exists(dstDir + QStringLiteral("/") + base + QStringLiteral(" (%1).jar").arg(c)))
            c++;
        dstPath = dstDir + QStringLiteral("/") + base + QStringLiteral(" (%1).jar").arg(c);
    }

    if (QFile::copy(filePath, dstPath)) {
        QString name = QFileInfo(dstPath).fileName();
        qCInfo(logMgr) << QStringLiteral("[本地Mod] 导入成功 文件=%1 目标=%2").arg(name, dstPath);
        emit modsChanged(versionId);
        emit importFinished(name, true, QString());
        return true;
    } else {
        emit importFinished(fi.fileName(), false, QStringLiteral("澶嶅埗鏂囦欢澶辫触"));
        return false;
    }
}

bool LocalModManager::deleteMod(const QString& fileName, const QString& versionId)
{
    QString dir = modsDir(versionId);
    QString path = dir + QStringLiteral("/") + fileName;
    if (!QFile::exists(path)) {
        // Try root mods
        path = m_gameDir + QStringLiteral("/mods/") + fileName;
    }
    if (QFile::exists(path)) {
        if (QFile::remove(path)) {
            qCInfo(logMgr) << QStringLiteral("[本地Mod] 删除成功 文件=%1").arg(fileName);
            emit modsChanged(versionId);
            return true;
        }
    }
    return false;
}

bool LocalModManager::openModsFolder(const QString& versionId)
{
    QString dir = modsDir(versionId);
    QDir().mkpath(dir);
    qCInfo(logMgr) << QStringLiteral("[本地Mod] 打开文件夹 路径=%1").arg(dir);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

// 鈹€鈹€ JAR Parsing 鈹€鈹€

LocalModEntry LocalModManager::parseJar(const QString& jarPath)
{
    LocalModEntry entry;
    entry.fileName = QFileInfo(jarPath).fileName();
    entry.fileSize = QFileInfo(jarPath).size();
    entry.fileSizeText = formatFileSize(entry.fileSize);
    entry.modName = QFileInfo(jarPath).completeBaseName();
    entry.loader = QStringLiteral("unknown");

    QFile f(jarPath);
    if (!f.open(QIODevice::ReadOnly)) return entry;

    QByteArray jarData = f.readAll();
    f.close();

    QBuffer buf(&jarData);
    if (!buf.open(QIODevice::ReadOnly)) return entry;
    QZipReader reader(&buf);

    // 1. Try fabric.mod.json
    QByteArray fabricData = reader.fileData(QStringLiteral("fabric.mod.json"));
    if (!fabricData.isEmpty()) {
        reader.close();
        return parseFabricJson(fabricData, jarPath, entry.fileName, entry.fileSize);
    }

    // 2. Try neoforge.mods.toml (NeoForge 20.4+)
    QByteArray nfToml = reader.fileData(QStringLiteral("META-INF/neoforge.mods.toml"));
    if (!nfToml.isEmpty()) {
        entry = parseNeoForgeToml(nfToml, jarPath, entry.fileName, entry.fileSize);
        if (entry.modId.isEmpty()) entry.modId = entry.modName.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]")), QStringLiteral("_"));
        entry.loader = QStringLiteral("neoforge");
        reader.close();
        return entry;
    }

    // 3. Try mods.toml (Forge 1.13+)
    QByteArray fToml = reader.fileData(QStringLiteral("META-INF/mods.toml"));
    if (!fToml.isEmpty()) {
        entry = parseNeoForgeToml(fToml, jarPath, entry.fileName, entry.fileSize);
        if (entry.modId.isEmpty()) entry.modId = entry.modName.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]")), QStringLiteral("_"));
        entry.loader = QStringLiteral("forge");
        reader.close();
        return entry;
    }

    // 4. Try mcmod.info (legacy Forge)
    QByteArray mcmodData = reader.fileData(QStringLiteral("mcmod.info"));
    if (!mcmodData.isEmpty()) {
        reader.close();
        return parseForgeMcmodInfo(mcmodData, jarPath, entry.fileName, entry.fileSize);
    }

    // 5. Try MANIFEST.MF for bare minimum
    QByteArray mf = reader.fileData(QStringLiteral("META-INF/MANIFEST.MF"));
    if (!mf.isEmpty()) {
        QString mfStr = QString::fromUtf8(mf);
        QRegularExpression nameRe(QStringLiteral("^Implementation-Title:\\s*(.+)$"), QRegularExpression::MultilineOption);
        QRegularExpression verRe(QStringLiteral("^Implementation-Version:\\s*(.+)$"), QRegularExpression::MultilineOption);
        auto m1 = nameRe.match(mfStr);
        auto m2 = verRe.match(mfStr);
        if (m1.hasMatch()) entry.modName = m1.captured(1).trimmed();
        if (m2.hasMatch()) entry.version = m2.captured(1).trimmed();
    }

    // Try to find any icon in the JAR (common paths)
    QStringList iconCandidates = {
        QStringLiteral("pack.png"),
        QStringLiteral("logo.png"),
        QStringLiteral("icon.png"),
    };
    for (const QString& ic : iconCandidates) {
        QByteArray iconData = reader.fileData(ic);
        if (!iconData.isEmpty()) {
            entry.iconPath = extractJarIcon(jarData, ic, entry.modId);
            break;
        }
    }

    reader.close();
    entry.valid = true;
    return entry;
}

LocalModEntry LocalModManager::parseFabricJson(const QByteArray& data, const QString& jarPath, const QString& fileName, qint64 fileSize)
{
    LocalModEntry entry;
    entry.fileName = fileName;
    entry.fileSize = fileSize;
    entry.fileSizeText = formatFileSize(fileSize);
    entry.loader = QStringLiteral("fabric");

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        entry.valid = true;
        return entry;
    }

    QJsonObject obj = doc.object();
    entry.modId = obj.value(QStringLiteral("id")).toString();
    entry.modName = obj.value(QStringLiteral("name")).toString();
    entry.version = obj.value(QStringLiteral("version")).toString();
    entry.description = obj.value(QStringLiteral("description")).toString();

    QJsonArray authorsArr = obj.value(QStringLiteral("authors")).toArray();
    for (const auto& a : authorsArr) {
        if (a.isObject()) {
            QJsonObject ao = a.toObject();
            entry.authors.append(ao.value(QStringLiteral("name")).toString());
        } else if (a.isString()) {
            entry.authors.append(a.toString());
        }
    }

    // Icon: load jar from jarPath, extract icon file
    QString iconInternal = obj.value(QStringLiteral("icon")).toString();
    if (!iconInternal.isEmpty() && !entry.modId.isEmpty()) {
        QFile jf(jarPath);
        if (jf.open(QIODevice::ReadOnly)) {
            QByteArray jarData = jf.readAll();
            jf.close();
            entry.iconPath = extractJarIcon(jarData, iconInternal, entry.modId);
        }
    }

    if (entry.modName.isEmpty()) entry.modName = entry.modId;
    entry.valid = true;
    return entry;
}

LocalModEntry LocalModManager::parseNeoForgeToml(const QByteArray& data, const QString& jarPath, const QString& fileName, qint64 fileSize)
{
    LocalModEntry entry;
    entry.fileName = fileName;
    entry.fileSize = fileSize;
    entry.fileSizeText = formatFileSize(fileSize);

    // Simple TOML parser 鈥?just extract [[mods]] section fields
    QString text = QString::fromUtf8(data);

    QRegularExpression modIdRe(QStringLiteral("modId\\s*=\\s*\"([^\"]+)\""));
    QRegularExpression displayRe(QStringLiteral("displayName\\s*=\\s*\"([^\"]+)\""));
    QRegularExpression verRe(QStringLiteral("version\\s*=\\s*\"([^\"]+)\""));
    QRegularExpression descRe(QStringLiteral("description\\s*=\\s*\"([^\"]+)\""));
    QRegularExpression logoRe(QStringLiteral("logoFile\\s*=\\s*\"([^\"]+)\""));
    QRegularExpression authorRe(QStringLiteral("authors\\s*=\\s*\"([^\"]+)\""));

    auto m1 = modIdRe.match(text);
    if (m1.hasMatch()) entry.modId = m1.captured(1);
    auto m2 = displayRe.match(text);
    if (m2.hasMatch()) entry.modName = m2.captured(1);
    auto m3 = verRe.match(text);
    if (m3.hasMatch()) entry.version = m3.captured(1);
    auto m4 = descRe.match(text);
    if (m4.hasMatch()) entry.description = m4.captured(1);
    auto m5 = logoRe.match(text);
    if (m5.hasMatch() && !entry.modId.isEmpty()) {
        QFile jf(jarPath);
        if (jf.open(QIODevice::ReadOnly)) {
            QByteArray jarData = jf.readAll();
            jf.close();
            entry.iconPath = extractJarIcon(jarData, m5.captured(1), entry.modId);
        }
    }
    auto m6 = authorRe.match(text);
    if (m6.hasMatch()) entry.authors << m6.captured(1);

    if (entry.modName.isEmpty()) entry.modName = entry.modId;
    entry.valid = true;
    return entry;
}

LocalModEntry LocalModManager::parseForgeMcmodInfo(const QByteArray& data, const QString& jarPath, const QString& fileName, qint64 fileSize)
{
    LocalModEntry entry;
    entry.fileName = fileName;
    entry.fileSize = fileSize;
    entry.fileSizeText = formatFileSize(fileSize);
    entry.loader = QStringLiteral("forge");

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);

    auto extractFromObj = [&](const QJsonObject& obj) {
        if (entry.modId.isEmpty()) entry.modId = obj.value(QStringLiteral("modid")).toString();
        if (entry.modName.isEmpty()) entry.modName = obj.value(QStringLiteral("name")).toString();
        if (entry.version.isEmpty()) entry.version = obj.value(QStringLiteral("version")).toString();
        if (entry.description.isEmpty()) entry.description = obj.value(QStringLiteral("description")).toString();

        QString logo = obj.value(QStringLiteral("logoFile")).toString();
        if (!logo.isEmpty() && !entry.modId.isEmpty()) {
            QFile jf(jarPath);
            if (jf.open(QIODevice::ReadOnly)) {
                QByteArray jarData = jf.readAll();
                jf.close();
                entry.iconPath = extractJarIcon(jarData, logo, entry.modId);
            }
        }

        QJsonArray authorsArr = obj.value(QStringLiteral("authorList")).toArray();
        for (const auto& a : authorsArr) {
            if (a.isString()) entry.authors.append(a.toString());
        }
        if (entry.authors.isEmpty()) {
            QString author = obj.value(QStringLiteral("authors")).toString();
            if (!author.isEmpty()) {
                // Could be comma-separated or JSON array
                if (author.startsWith('[')) {
                    QJsonDocument ad = QJsonDocument::fromJson(author.toUtf8());
                    if (ad.isArray()) {
                        for (const auto& x : ad.array())
                            if (x.isString()) entry.authors.append(x.toString());
                    }
                } else {
                    for (const QString& s : author.split(','))
                        entry.authors.append(s.trimmed());
                }
            }
        }
    };

    if (doc.isArray()) {
        for (const auto& v : doc.array()) {
            if (v.isObject()) { extractFromObj(v.toObject()); break; }
        }
    } else if (doc.isObject()) {
        extractFromObj(doc.object());
    }

    if (entry.modName.isEmpty()) entry.modName = entry.modId;
    entry.valid = true;
    return entry;
}

QString LocalModManager::extractJarIcon(const QByteArray& jarData, const QString& iconInternalPath, const QString& modId)
{
    if (iconInternalPath.isEmpty() || modId.isEmpty()) return {};

    QDir().mkpath(m_iconCacheDir);
    QString cachePath = m_iconCacheDir + QStringLiteral("/") + modId + QStringLiteral(".png");

    // Return cached if exists
    if (QFile::exists(cachePath)) {
        qCInfo(logMgr) << QStringLiteral("[图标提取] 缓存命中 modId=%1").arg(modId);
        return cachePath;
    }

    // Extract from JAR data
    QBuffer buf;
    buf.setData(jarData);
    if (!buf.open(QIODevice::ReadOnly)) return {};
    QZipReader reader(&buf);
    QByteArray iconData = reader.fileData(iconInternalPath);
    reader.close();

    qCInfo(logMgr) << QStringLiteral("[图标提取] 从JAR解析 iconPath=%1 modId=%2").arg(iconInternalPath, modId);

    if (iconData.isEmpty()) {
        // Try common alternatives with a fresh reader
        buf.close();
        buf.setData(jarData);
        buf.open(QIODevice::ReadOnly);
        {
            QZipReader altReader(&buf);
            QStringList altPaths = {
                QStringLiteral("assets/") + modId + QStringLiteral("/icon.png"),
                QStringLiteral("assets/") + modId + QStringLiteral("/logo.png"),
                QStringLiteral("pack.png"),
            };
            for (const QString& alt : altPaths) {
                iconData = altReader.fileData(alt);
                if (!iconData.isEmpty()) break;
            }
        }
    }

    if (iconData.isEmpty()) return {};

    // Resize to 64x64 for cache efficiency
    QImage img;
    img.loadFromData(iconData);
    if (img.isNull()) return {};

    if (img.width() > 64 || img.height() > 64)
        img = img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    img.save(cachePath, "PNG");
    return cachePath;
}

QString LocalModManager::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
}

QVariantMap LocalModManager::entryToMap(const LocalModEntry& e) const
{
    QVariantMap m;
    m[QStringLiteral("fileName")]    = e.fileName;
    m[QStringLiteral("modName")]     = e.modName;
    m[QStringLiteral("modId")]       = e.modId;
    m[QStringLiteral("version")]     = e.version;
    m[QStringLiteral("description")] = e.description;
    m[QStringLiteral("iconPath")]    = e.iconPath.isEmpty()
        ? QString() : QUrl::fromLocalFile(e.iconPath).toString();
    m[QStringLiteral("authors")]     = QVariant(e.authors);
    m[QStringLiteral("loader")]      = e.loader;
    m[QStringLiteral("fileSize")]    = e.fileSize;
    m[QStringLiteral("fileSizeText")]= e.fileSizeText;
    return m;
}

// ═══════════════════════════════════════════════════════════════
//  Resource Pack Operations
// ═══════════════════════════════════════════════════════════════

QString LocalModManager::resourcePacksDir(const QString& versionId) const
{
    // Isolated path: .minecraft/versions/<id>/game/resourcepacks
    QString isolatedDir = m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game/resourcepacks");
    if (QDir(m_gameDir + QStringLiteral("/versions/") + versionId + QStringLiteral("/game")).exists())
        return isolatedDir;
    // Fallback: .minecraft/resourcepacks (shared)
    return m_gameDir + QStringLiteral("/resourcepacks");
}

QVariantList LocalModManager::scanResourcePacks(const QString& versionId)
{
    QVariantList result;
    QString rpDir = resourcePacksDir(versionId);
    QDir dir(rpDir);
    if (!dir.exists()) return result;

    QDir cacheDir(m_iconCacheDir);
    if (!cacheDir.exists()) cacheDir.mkpath(QStringLiteral("."));

    const QFileInfoList files = dir.entryInfoList(
        {QStringLiteral("*.zip"), QStringLiteral("*.ZIP")}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        LocalResourcePackEntry entry = parseResourcePackZip(fi.absoluteFilePath());
        if (!entry.valid) {
            // Minimal entry from filename
            entry.fileName = fi.fileName();
            entry.displayName = fi.completeBaseName();
            entry.fileSize = fi.size();
            entry.fileSizeText = formatFileSize(fi.size());
        }
        // Override fileSize from disk (trust disk more than ZIP parse)
        if (entry.fileSize == 0) {
            entry.fileSize = fi.size();
            entry.fileSizeText = formatFileSize(fi.size());
        }
        result.append(rpEntryToMap(entry));
    }
    return result;
}

bool LocalModManager::deleteResourcePack(const QString& fileName, const QString& versionId)
{
    QString dir = resourcePacksDir(versionId);
    QString path = dir + QStringLiteral("/") + fileName;
    if (!QFile::exists(path)) return false;
    return QFile::remove(path);
}

LocalResourcePackEntry LocalModManager::parseResourcePackZip(const QString& zipPath)
{
    LocalResourcePackEntry entry;
    QFileInfo fi(zipPath);
    entry.fileName = fi.fileName();
    entry.fileSize = fi.size();
    entry.fileSizeText = formatFileSize(fi.size());

    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) return entry;
    QByteArray zipData = file.readAll();
    file.close();

    QBuffer buf;
    buf.setData(zipData);
    if (!buf.open(QIODevice::ReadOnly)) return entry;

    QZipReader reader(&buf);

    // ── Parse pack.mcmeta ──
    QByteArray mcmetaData = reader.fileData(QStringLiteral("pack.mcmeta"));
    if (!mcmetaData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(mcmetaData);
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            QJsonObject pack = root.value(QStringLiteral("pack")).toObject();
            if (!pack.isEmpty()) {
                entry.packFormat = pack.value(QStringLiteral("pack_format")).toInt(0);
                entry.versionLabel = packFormatToVersionLabel(entry.packFormat);

                // description: can be string or object {"text": "..."} or {"translate": "..."}
                QJsonValue descVal = pack.value(QStringLiteral("description"));
                if (descVal.isString()) {
                    entry.description = stripColorCodes(descVal.toString());
                } else if (descVal.isObject()) {
                    QJsonObject descObj = descVal.toObject();
                    if (descObj.contains(QStringLiteral("text")))
                        entry.description = stripColorCodes(descObj.value(QStringLiteral("text")).toString());
                    else if (descObj.contains(QStringLiteral("translate")))
                        entry.description = stripColorCodes(descObj.value(QStringLiteral("translate")).toString());
                }

                // displayName from filename, description from pack.mcmeta
                entry.displayName = fi.completeBaseName();
                // description already set above (keep it as pack.mcmeta description)
            }
        }
        entry.valid = true;
    } else {
        // No pack.mcmeta — still valid (some zip-based packs)
        entry.displayName = fi.completeBaseName();
        entry.valid = true;
    }

    // ── Extract pack.png ──
    QString packName = fi.completeBaseName();
    entry.iconPath = extractPackPng(zipData, packName);

    reader.close();
    return entry;
}

QString LocalModManager::extractPackPng(const QByteArray& zipData, const QString& packName)
{
    qCInfo(logMgr) << QStringLiteral("[资源包PNG] 开始提取 pack=%1").arg(packName);
    if (packName.isEmpty()) return {};

    QDir().mkpath(m_iconCacheDir);
    QString cachePath = m_iconCacheDir + QStringLiteral("/rp_") + packName + QStringLiteral(".png");

    // Return cached if exists
    if (QFile::exists(cachePath)) {
        qCInfo(logMgr) << QStringLiteral("[资源包PNG] 缓存命中 pack=%1").arg(packName);
        return cachePath;
    }

    QBuffer buf;
    buf.setData(zipData);
    if (!buf.open(QIODevice::ReadOnly)) return {};

    QZipReader reader(&buf);
    QByteArray pngData = reader.fileData(QStringLiteral("pack.png"));
    reader.close();

    if (pngData.isEmpty()) return {};

    // Resize to 64x64
    QImage img;
    if (!img.loadFromData(pngData)) return {};

    if (img.width() > 64 || img.height() > 64)
        img = img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    img.save(cachePath, "PNG");
    return cachePath;
}

QString LocalModManager::packFormatToVersionLabel(int packFormat)
{
    if (packFormat <= 0) return {};
    // Minecraft wiki: Pack_format values per version
    struct { int fmt; const char* label; } map[] = {
        {1, "格式 1 · 1.6.1–1.8.9"},
        {2, "格式 2 · 1.9–1.10.2"},
        {3, "格式 3 · 1.11–1.12.2"},
        {4, "格式 4 · 1.13–1.14.4"},
        {5, "格式 5 · 1.15–1.16.1"},
        {6, "格式 6 · 1.16.2–1.16.5"},
        {7, "格式 7 · 1.17–1.17.1"},
        {8, "格式 8 · 1.18–1.18.2"},
        {9, "格式 9 · 1.19–1.19.2"},
        {12, "格式 12 · 1.19.3"},
        {13, "格式 13 · 1.19.4"},
        {15, "格式 15 · 1.20–1.20.1"},
        {18, "格式 18 · 1.20.2"},
        {22, "格式 22 · 1.20.3–1.20.4"},
        {34, "格式 34 · 1.20.5–1.21.1"},
        {42, "格式 42 · 1.21.2+"},
        {0, nullptr}
    };
    for (int i = 0; map[i].label; i++) {
        if (packFormat <= map[i].fmt) return QString::fromUtf8(map[i].label);
        // check if there's a next entry and we're between
        if (map[i+1].label && packFormat < map[i+1].fmt)
            return QString::fromUtf8(map[i].label);
    }
    // Unknown higher format
    return QStringLiteral("格式 %1").arg(packFormat);
}

QString LocalModManager::stripColorCodes(const QString& text)
{
    // Strip Minecraft color/format codes: §[0-9a-fk-or] and &[0-9a-fk-or]
    QString result;
    result.reserve(text.size());
    bool skip = false;
    for (const QChar& ch : text) {
        if (skip) { skip = false; continue; }
        if ((ch == QChar(0x00A7) || ch == QLatin1Char('&')) && result.size() < text.size() - 1) {
            skip = true;
            continue;
        }
        result.append(ch);
    }
    return result;
}

QVariantMap LocalModManager::rpEntryToMap(const LocalResourcePackEntry& e)
{
    QVariantMap m;
    m[QStringLiteral("name")]         = e.displayName;
    m[QStringLiteral("fileName")]     = e.fileName;
    m[QStringLiteral("version")]      = e.packFormat > 0 ? QString::number(e.packFormat) : QString();
    m[QStringLiteral("versionLabel")] = e.versionLabel;
    m[QStringLiteral("iconPath")]     = e.iconPath.isEmpty()
        ? QString() : QUrl::fromLocalFile(e.iconPath).toString();
    m[QStringLiteral("fileSize")]     = e.fileSize;
    m[QStringLiteral("fileSizeText")] = e.fileSizeText;

    // Split description: first line -> versionText (green), rest -> authorText
    // Strip leading "■" / "▪" / "·" / "•" / "∙" / "・" bullet prefix (direct char cmp)
    auto stripBullet = [](const QString& line) -> QString {
        if (line.isEmpty()) return line;
        const auto u = line[0].unicode();
        // U+25A0 ■, U+25AA ▪, U+00B7 ·, U+2022 •, U+2219 ∙, U+30FB ・
        if (u == 0x25A0 || u == 0x25AA || u == 0x00B7 || u == 0x2022 || u == 0x2219 || u == 0x30FB) {
            int skip = 1;
            while (skip < line.size() && line[skip].isSpace()) skip++;
            return line.mid(skip);
        }
        return line;
    };
    auto stripBullets = [&](const QString& text) {
        const auto lines = text.split(QStringLiteral("\n"));
        QStringList out; out.reserve(lines.size());
        for (const auto& l : lines) out.append(stripBullet(l));
        return out.join(QStringLiteral("\n"));
    };

    const QString& desc = e.description;
    const int nl = desc.indexOf(QStringLiteral("\n"));
    QString versionText, authorText;
    if (nl >= 0) {
        versionText = stripBullet(desc.left(nl));
        authorText = stripBullets(desc.mid(nl + 1));
    } else {
        versionText = stripBullet(desc);
    }
    m[QStringLiteral("versionText")] = versionText;
    // Format "By XXX" → "作者：XXX"
    static const QRegularExpression byRe(QStringLiteral("^By\\s+"));
    authorText.replace(byRe, QStringLiteral("\u4F5C\u8005\uFF1A"));
    m[QStringLiteral("authorText")] = authorText;

    return m;
}

} // namespace ShadowLauncher
