// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "modpack_parser.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "zip_archive.h"
#include "../../utils/logger.h"

namespace ShadowLauncher {

namespace {

constexpr qint64 kManifestMaxBytes = 16 * 1024 * 1024;  // 清单文件不可能更大

QString jsonToString(const QJsonValue& v)
{
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble());
    return v.toVariant().toString();
}

// ── CurseForge manifest.json 解析（主流启动器 InstallPackCurseForge）──

bool parseCurseForge(const QString& zipPath, ZipArchive& zip, ModpackMeta& meta, QString& error)
{
    const QByteArray raw = zip.readEntry(QStringLiteral("manifest.json"), kManifestMaxBytes);
    if (raw.isEmpty()) {
        error = QStringLiteral("压缩包内缺少 manifest.json 或清单为空");
        return false;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QStringLiteral("manifest.json 不是合法 JSON: %1").arg(perr.errorString());
        return false;
    }
    const QJsonObject root = doc.object();

    meta.format = ModpackFormat::CurseForge;
    meta.name = root.value(QStringLiteral("name")).toString();
    meta.versionId = jsonToString(root.value(QStringLiteral("version")));
    if (meta.versionId.isEmpty()) meta.versionId = QStringLiteral("1.0");

    // minecraft.version — 缺失即拒绝（主流启动器 同文案）
    const QJsonObject mc = root.value(QStringLiteral("minecraft")).toObject();
    meta.mcVersion = mc.value(QStringLiteral("version")).toString();
    if (meta.mcVersion.isEmpty()) {
        error = QStringLiteral("CurseForge 整合包未提供 Minecraft 版本信息");
        return false;
    }

    // 加载器：优先 primary 项（主流启动器 遍历 modLoaders，取匹配前缀）
    const QJsonArray loaders = mc.value(QStringLiteral("modLoaders")).toArray();
    int primaryIdx = -1;
    for (int i = 0; i < loaders.size(); ++i) {
        if (loaders.at(i).toObject().value(QStringLiteral("primary")).toBool(false)) {
            primaryIdx = i;
            break;
        }
    }
    if (primaryIdx < 0 && !loaders.isEmpty()) primaryIdx = 0;

    if (primaryIdx >= 0) {
        const QString loaderId = loaders.at(primaryIdx).toObject().value(QStringLiteral("id")).toString();
        QString lErr;
        if (!parseLoaderId(loaderId, &meta.loaderType, &meta.loaderVersion, &lErr)) {
            qCWarning(logMod) << "[modpack][CF]" << lErr;
            // 主流启动器 NotifyIncompatibleLoader：用户可选择“不安装并继续”。后端无交互，
            // 记录告警并继续（纯原版兜底），最终结果中带警告。
            meta.loaderType.clear();
            meta.loaderVersion.clear();
            meta.summary = lErr;  // 借 summary 携带警告（最终界面提示）
        }
    }

    // 文件列表：缺 projectID/fileID 的条目跳过（主流启动器 Hint + Continue For）
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    int idx = 0;
    for (const QJsonValue& val : files) {
        const QJsonObject f = val.toObject();
        const int projectId = f.value(QStringLiteral("projectID")).toInt(-1);
        const int fileId = f.value(QStringLiteral("fileID")).toInt(-1);
        if (projectId <= 0 || fileId <= 0) {
            qCWarning(logMod) << "[modpack][CF] 跳过缺少必要信息的条目:" << QString::fromUtf8(QJsonDocument(f).toJson(QJsonDocument::Compact));
            continue;
        }
        ModpackRemoteFile rf;
        rf.source = QStringLiteral("curseforge");
        rf.projectId = projectId;
        rf.fileId = fileId;
        rf.required = f.value(QStringLiteral("required")).toBool(true);
        rf.index = idx++;
        meta.files.append(rf);
    }

    // overrides 目录（主流启动器 #5613：支持自定义 overrides 字段；"." 表示整包根）
    QString overrides = root.value(QStringLiteral("overrides")).toString(QStringLiteral("overrides"));
    if (overrides == QStringLiteral(".") || overrides == QStringLiteral("./"))
        overrides.clear();
    meta.overrideDirs.append(overrides);

    if (meta.name.isEmpty())
        meta.name = QFileInfo(zipPath).completeBaseName();
    return true;
}

// ── Modrinth modrinth.index.json 解析（主流启动器 InstallPackModrinth）──

bool parseModrinth(const QString& zipPath, ZipArchive& zip, ModpackMeta& meta, QString& error)
{
    const QByteArray raw = zip.readEntry(QStringLiteral("modrinth.index.json"), kManifestMaxBytes);
    if (raw.isEmpty()) {
        error = QStringLiteral("压缩包内缺少 modrinth.index.json 或清单为空");
        return false;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QStringLiteral("modrinth.index.json 不是合法 JSON: %1").arg(perr.errorString());
        return false;
    }
    const QJsonObject root = doc.object();

    meta.format = ModpackFormat::Modrinth;
    meta.name = root.value(QStringLiteral("name")).toString();
    meta.versionId = root.value(QStringLiteral("versionId")).toString();
    meta.summary = root.value(QStringLiteral("summary")).toString();

    // dependencies：minecraft 必须；加载器识别 forge/neoforge/fabric-loader/quilt-loader
    const QJsonObject deps = root.value(QStringLiteral("dependencies")).toObject();
    meta.mcVersion = deps.value(QStringLiteral("minecraft")).toString();
    if (meta.mcVersion.isEmpty()) {
        error = QStringLiteral("Modrinth 整合包未提供 Minecraft 版本信息");
        return false;
    }

    static const char* kLoaderDeps[] = {"forge", "neoforge", "fabric-loader", "quilt-loader"};
    QString depKey;
    for (const char* k : kLoaderDeps) {
        if (deps.contains(QLatin1String(k))) {
            depKey = QLatin1String(k);
            break;
        }
    }
    if (!depKey.isEmpty()) {
        const QString ver = deps.value(depKey).toString();
        QString lErr;
        if (!parseLoaderId(depKey + QLatin1Char('-') + ver, &meta.loaderType, &meta.loaderVersion, &lErr)) {
            qCWarning(logMod) << "[modpack][MR]" << lErr;
            meta.loaderType.clear();
            meta.loaderVersion.clear();
            if (!meta.summary.isEmpty()) meta.summary += QStringLiteral("；");
            meta.summary += lErr;
        }
    }

    // 文件列表：env.client == unsupported 跳过；optional 标记非必需；路径越界直接失败（主流启动器）
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    int idx = 0;
    for (const QJsonValue& val : files) {
        const QJsonObject f = val.toObject();
        const QString path = f.value(QStringLiteral("path")).toString();

        // 路径安全：必须在版本目录内（主流启动器 全路径校验）
        const QString safePath = sanitizeRelPath(path);
        if (safePath.isEmpty()) {
            error = QStringLiteral("整合包文件路径非法（可能试图越界写入）: %1").arg(path);
            return false;
        }

        // env.client：required / optional / unsupported（主流启动器 Select Case）
        const QJsonObject env = f.value(QStringLiteral("env")).toObject();
        const QString clientEnv = env.value(QStringLiteral("client")).toString();
        if (clientEnv == QStringLiteral("unsupported")) {
            qCInfo(logMod) << "[modpack][MR] 跳过客户端不支持的资源:" << path;
            continue;
        }

        ModpackRemoteFile rf;
        rf.source = QStringLiteral("modrinth");
        rf.relPath = safePath;
        rf.fileName = QFileInfo(safePath).fileName();
        rf.displayName = rf.fileName;
        rf.required = (clientEnv != QStringLiteral("optional"));
        rf.index = idx++;

        const QJsonArray downloads = f.value(QStringLiteral("downloads")).toArray();
        for (int d = 0; d < downloads.size(); ++d) {
            const QString u = downloads.at(d).toString();
            if (u.isEmpty()) continue;
            if (d == 0) rf.downloadUrl = u;
            else rf.fallbackUrls.append(u);
        }

        rf.size = static_cast<qint64>(f.value(QStringLiteral("fileSize")).toDouble(0));
        const QJsonObject hashes = f.value(QStringLiteral("hashes")).toObject();
        const QString sha1 = hashes.value(QStringLiteral("sha1")).toString();
        if (!sha1.isEmpty()) rf.sha1 = sha1.toLatin1();

        meta.files.append(rf);
    }

    // 覆写目录：overrides + client-overrides（主流启动器 两个都拷）
    meta.overrideDirs.append(QStringLiteral("overrides"));
    meta.overrideDirs.append(QStringLiteral("client-overrides"));

    if (meta.name.isEmpty())
        meta.name = QFileInfo(zipPath).completeBaseName();
    return true;
}

} // namespace

// ── 格式探测 ──

ModpackFormat ModpackParser::detectFormat(const QString& zipPath)
{
    ZipArchive zip;
    if (!zip.open(zipPath)) {
        qCWarning(logMod) << "[modpack] 无法打开压缩包:" << zipPath << zip.error();
        return ModpackFormat::Unknown;
    }

    if (zip.hasEntry(QStringLiteral("modrinth.index.json")))
        return ModpackFormat::Modrinth;

    if (zip.hasEntry(QStringLiteral("manifest.json")))
        return ModpackFormat::CurseForge;

    qCWarning(logMod) << "[modpack] 未识别到任何整合包清单（modrinth.index.json / manifest.json）";
    return ModpackFormat::Unknown;
}

// ── 解析 ──

bool ModpackParser::parse(const QString& zipPath, ModpackFormat format,
                          ModpackMeta& meta, QString& error)
{
    meta = ModpackMeta{};
    error.clear();

    ZipArchive zip;
    if (!zip.open(zipPath)) {
        error = zip.error();
        return false;
    }

    bool ok = false;
    if (format == ModpackFormat::CurseForge) {
        ok = parseCurseForge(zipPath, zip, meta, error);
    } else if (format == ModpackFormat::Modrinth) {
        ok = parseModrinth(zipPath, zip, meta, error);
    } else {
        error = QStringLiteral("未知整合包格式");
        return false;
    }
    if (!ok) return false;

    // 目标版本名（版本文件夹名 = 版本 id）
    if (meta.name.isEmpty()) meta.name = QStringLiteral("Modpack");
    qCInfo(logMod) << "[modpack] 解析完成:" << meta.name
                   << "MC=" << meta.mcVersion
                   << "loader=" << meta.loaderType << meta.loaderVersion
                   << "files=" << meta.files.size()
                   << "overrides=" << meta.overrideDirs.join(QLatin1Char(','));
    return true;
}

} // namespace ShadowLauncher
