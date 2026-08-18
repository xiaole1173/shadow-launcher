// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "minecraft_folder_backend.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QtConcurrent>

namespace ShadowLauncher {

namespace {

QString launcherSourceKey(LauncherSource source)
{
    switch (source) {
    case LauncherSource::Shadow:   return QStringLiteral("shadow");
    case LauncherSource::Pcl2:     return QStringLiteral("pcl2");
    case LauncherSource::Hmcl:     return QStringLiteral("hmcl");
    case LauncherSource::Official: return QStringLiteral("official");
    case LauncherSource::Unknown:
    default:                       return QStringLiteral("unknown");
    }
}

} // namespace

MinecraftFolderBackend::MinecraftFolderBackend(QObject* parent)
    : QObject(parent)
{
}

// ════════════════════════════════════════════════════════════════
// 选择 / 识别
// ════════════════════════════════════════════════════════════════

bool MinecraftFolderBackend::probe(const QString& dir)
{
    const MinecraftFolderInfo fresh = probeMinecraftFolder(dir);
    m_info = fresh;
    emit folderChanged();
    emit analysisChanged();
    return m_info.valid;
}

void MinecraftFolderBackend::clear()
{
    if (m_info.valid || !m_info.root.isEmpty()) {
        m_info = MinecraftFolderInfo();
        emit folderChanged();
        emit analysisChanged();
    }
}

QVariantMap MinecraftFolderBackend::result() const
{
    QVariantMap map;
    map[QStringLiteral("root")] = m_info.root;
    map[QStringLiteral("valid")] = m_info.valid;
    map[QStringLiteral("layout")] = static_cast<int>(m_info.layout);
    map[QStringLiteral("layoutName")] = layoutDisplayName(m_info.layout);
    map[QStringLiteral("launcher")] = launcherDisplayName(m_info.launcher);
    map[QStringLiteral("versionCount")] = m_info.totalVersionCount;
    map[QStringLiteral("rootHasGameData")] = m_info.rootHasGameData;
    map[QStringLiteral("isolatedVersionCount")] = m_info.isolatedVersionCount;
    map[QStringLiteral("notes")] = m_info.notes;
    return map;
}

QString MinecraftFolderBackend::layoutName() const
{
    return layoutDisplayName(m_info.layout);
}

QString MinecraftFolderBackend::launcherName() const
{
    return launcherDisplayName(m_info.launcher);
}

// ════════════════════════════════════════════════════════════════
// 读取（每版本）
// ════════════════════════════════════════════════════════════════

QVariantList MinecraftFolderBackend::versionInfos() const
{
    QVariantList out;
    if (!m_info.valid) return out;

    for (const QString& id : m_info.versionIds) {
        const QString verDir = m_info.root + QStringLiteral("/versions/") + id;
        const QString jsonPath = findJsonInDir(verDir);
        const QString jarPath = findJarInDir(verDir);
        const QString gdir = resolveGameDirFor(id);

        QVariantMap vm;
        vm[QStringLiteral("id")] = id;
        vm[QStringLiteral("jsonPath")] = jsonPath;
        vm[QStringLiteral("jarPath")] = jarPath;
        vm[QStringLiteral("gameDir")] = gdir;
        vm[QStringLiteral("hasJson")] = !jsonPath.isEmpty();
        vm[QStringLiteral("isolated")] = hasVersionGameData(verDir);
        vm[QStringLiteral("modsCount")] = versionModsCount(id);
        out.append(vm);
    }
    return out;
}

QString MinecraftFolderBackend::versionGameDir(const QString& versionId) const
{
    return resolveGameDirFor(versionId);
}

QString MinecraftFolderBackend::versionJsonPath(const QString& versionId) const
{
    return findJsonInDir(m_info.root + QStringLiteral("/versions/") + versionId);
}

QString MinecraftFolderBackend::versionJarPath(const QString& versionId) const
{
    return findJarInDir(m_info.root + QStringLiteral("/versions/") + versionId);
}

bool MinecraftFolderBackend::versionHasJson(const QString& versionId) const
{
    return !findJsonInDir(m_info.root + QStringLiteral("/versions/") + versionId).isEmpty();
}

int MinecraftFolderBackend::versionModsCount(const QString& versionId) const
{
    if (!m_info.valid) return 0;
    const QDir modsDir(resolveGameDirFor(versionId) + QStringLiteral("/mods"));
    if (!modsDir.exists()) return 0;

    int count = 0;
    const QFileInfoList files = modsDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& f : files) {
        if (f.fileName().endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive))
            ++count;
    }
    return count;
}

// ════════════════════════════════════════════════════════════════
// 应用 / 回退
// ════════════════════════════════════════════════════════════════

bool MinecraftFolderBackend::applyAsGameDir()
{
    if (!m_info.valid || m_info.root.isEmpty()) return false;
    if (m_applied && !m_info.root.isEmpty()) {
        // 已应用同一目录：幂等，仍发一次信号确保后端状态一致
        emit applyRequested(m_info.root, static_cast<int>(m_info.layout));
        return true;
    }
    m_applied = true;
    emit foreignChanged();
    emit applyRequested(m_info.root, static_cast<int>(m_info.layout));
    return true;
}

void MinecraftFolderBackend::revertToDefault()
{
    if (m_applied) {
        m_applied = false;
        emit foreignChanged();
    }
    emit revertRequested();
}

bool MinecraftFolderBackend::effectiveIsolation() const
{
    if (!m_applied) return false;
    return m_info.layout == MinecraftLayout::Isolated;
}

// ════════════════════════════════════════════════════════════════
// 私有
// ════════════════════════════════════════════════════════════════

QString MinecraftFolderBackend::resolveGameDirFor(const QString& versionId) const
{
    if (!m_info.valid) return {};
    return resolveVersionGameDir(m_info.root, versionId, m_info.layout);
}

QString MinecraftFolderBackend::findJsonInDir(const QString& verDir) const
{
    if (verDir.isEmpty()) return {};
    QDir dir(verDir);
    if (!dir.exists()) return {};

    // 目录名匹配优先（标准命名）
    const QFileInfo exact(dir.absolutePath() + QStringLiteral("/")
                          + QDir(verDir).dirName() + QStringLiteral(".json"));
    if (exact.exists()) return exact.absoluteFilePath();

    // 否则取第一个 .json（兼容目录名≠文件名，如 RLCraft/RLCraft.json 已覆盖，
    // 以及整合包目录名与 json 名不一致的情况）
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& f : files) {
        if (f.fileName().endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
            return f.absoluteFilePath();
    }
    return {};
}

QString MinecraftFolderBackend::findJarInDir(const QString& verDir) const
{
    if (verDir.isEmpty()) return {};
    QDir dir(verDir);
    if (!dir.exists()) return {};

    const QFileInfo exact(dir.absolutePath() + QStringLiteral("/")
                          + QDir(verDir).dirName() + QStringLiteral(".jar"));
    if (exact.exists()) return exact.absoluteFilePath();

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& f : files) {
        if (f.fileName().endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive))
            return f.absoluteFilePath();
    }
    return {};
}

// ════════════════════════════════════════════════════════════════
// 游戏文件夹注册表
// ════════════════════════════════════════════════════════════════

QString MinecraftFolderBackend::defaultFolderPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/.minecraft");
}

QString MinecraftFolderBackend::registryPath() const
{
    return m_dataDir + QStringLiteral("/game_folders.json");
}

QStringList MinecraftFolderBackend::readRegistryPaths() const
{
    QFile f(registryPath());
    if (!f.open(QIODevice::ReadOnly)) return {};

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};

    QStringList out;
    const QJsonArray arr = doc.object().value(QStringLiteral("folders")).toArray();
    for (const auto& v : arr) {
        if (v.isObject())
            out.append(v.toObject().value(QStringLiteral("path")).toString());
    }
    return out;
}

void MinecraftFolderBackend::writeRegistry(const QStringList& paths) const
{
    if (m_dataDir.isEmpty()) return;
    QDir().mkpath(m_dataDir);

    QJsonObject root;
    QJsonArray arr;
    for (const QString& p : paths) {
        QJsonObject o;
        o[QStringLiteral("path")] = p;
        arr.append(o);
    }
    root[QStringLiteral("folders")] = arr;

    QFile f(registryPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }
}

QString MinecraftFolderBackend::readNameFile(const QString& folder)
{
    QFile f(folder + QStringLiteral("/config/shadow_folder.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object().value(QStringLiteral("name")).toString();
}

bool MinecraftFolderBackend::writeNameFile(const QString& folder, const QString& name)
{
    if (folder.isEmpty()) return false;
    QDir().mkpath(folder + QStringLiteral("/config"));

    QJsonObject root;
    root[QStringLiteral("name")] = name;
    root[QStringLiteral("addedAt")] =
        QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QStringLiteral("source")] =
        launcherSourceKey(probeMinecraftFolder(folder).launcher);

    QFile f(folder + QStringLiteral("/config/shadow_folder.json"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

QVariantMap MinecraftFolderBackend::buildFolderEntry(const QString& path,
                                                     const QString& name,
                                                     bool isDefault,
                                                     const QString& currentActive)
{
    const QString clean = QDir::cleanPath(path);
    const bool exists = QDir(clean).exists();

    QVariantMap m;
    m[QStringLiteral("path")] = clean;
    m[QStringLiteral("isDefault")] = isDefault;
    m[QStringLiteral("exists")] = exists;
    m[QStringLiteral("active")] = (QDir::cleanPath(currentActive) == clean);

    const QString trimmed = name.trimmed();
    m[QStringLiteral("name")] = !trimmed.isEmpty()
        ? trimmed
        : (isDefault ? QStringLiteral("默认游戏目录") : QDir(clean).dirName());

    if (exists) {
        const auto info = probeMinecraftFolder(clean);
        m[QStringLiteral("layoutName")] = layoutDisplayName(info.layout);
        // 固有文件夹=本启动器自带目录，直接标记 Shadow（2026-08-18：
        // 全新默认目录可能没有 config/version_isolation.json——该文件只在切换
        // 隔离开关时才写入，只剩 launcher_profiles.json 会被误判为官方启动器）
        m[QStringLiteral("launcherName")] = isDefault
            ? QStringLiteral("Shadow Launcher")
            : launcherDisplayName(info.launcher);
        m[QStringLiteral("versionCount")] = info.totalVersionCount;
    } else {
        m[QStringLiteral("layoutName")] = QStringLiteral("—");
        m[QStringLiteral("launcherName")] = QStringLiteral("—");
        m[QStringLiteral("versionCount")] = 0;
    }
    return m;
}

QVariantList MinecraftFolderBackend::collectFolders(const QStringList& paths,
                                                    const QString& defaultPath,
                                                    const QString& currentActive)
{
    QVariantList out;
    out.append(buildFolderEntry(defaultPath, readNameFile(defaultPath), true, currentActive));
    const QString defClean = QDir::cleanPath(defaultPath);
    for (const QString& p : paths) {
        const QString clean = QDir::cleanPath(p);
        if (clean.isEmpty()) continue;
        if (clean == defClean) continue;  // 固有文件夹去重
        out.append(buildFolderEntry(clean, readNameFile(clean), false, currentActive));
    }
    return out;
}

void MinecraftFolderBackend::refreshFolders()
{
    const QString defaultPath = defaultFolderPath();
    const QStringList paths = readRegistryPaths();
    // 当前实际使用的目录（活动标记以此为准，避免与持久化状态不一致）
    const QString currentActive = (m_applied && !m_info.root.isEmpty())
        ? m_info.root : defaultPath;

    if (!m_folderWatcher) {
        m_folderWatcher = new QFutureWatcher<QVariantList>(this);
        connect(m_folderWatcher, &QFutureWatcher<QVariantList>::finished,
                this, [this]() {
            m_folders = m_folderWatcher->result();
            emit foldersReady(m_folders);
            emit foldersChanged();
        });
    }
    // 工作线程探测（纯字符串/纯逻辑，跨线程安全），主线程收到结果后发信号
    m_folderWatcher->setFuture(QtConcurrent::run([paths, defaultPath, currentActive]() {
        return collectFolders(paths, defaultPath, currentActive);
    }));
}

QString MinecraftFolderBackend::activeFolderPath() const
{
    return (m_applied && !m_info.root.isEmpty()) ? m_info.root : defaultFolderPath();
}

bool MinecraftFolderBackend::addGameFolder(const QString& path, const QString& name)
{
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty() || !QDir(clean).exists()) return false;

    QStringList paths = readRegistryPaths();
    if (!paths.contains(clean)) paths.append(clean);

    writeNameFile(clean, name.trimmed().isEmpty() ? QDir(clean).dirName() : name.trimmed());
    writeRegistry(paths);
    refreshFolders();
    return true;
}

bool MinecraftFolderBackend::renameGameFolder(const QString& path, const QString& newName)
{
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) return false;
    const bool ok = writeNameFile(clean, newName.trimmed());
    refreshFolders();
    return ok;
}

bool MinecraftFolderBackend::removeGameFolder(const QString& path)
{
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) return false;
    if (clean == QDir::cleanPath(defaultFolderPath())) return false;  // 固有文件夹不可移除

    QStringList paths = readRegistryPaths();
    if (!paths.removeAll(clean)) return false;  // 不在注册表

    const bool wasActive = m_applied && QDir::cleanPath(m_info.root) == clean;
    writeRegistry(paths);
    if (wasActive) {
        m_applied = false;
        emit foreignChanged();
        emit revertRequested();  // 移除活动外部目录 → 回退默认
    }
    refreshFolders();
    return true;
}

bool MinecraftFolderBackend::setActiveFolder(const QString& path)
{
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty()) return false;

    if (m_info.root != clean) probe(clean);  // 复用缓存，避免重复扫描
    if (!m_info.valid) return false;

    m_applied = true;
    emit foreignChanged();
    emit applyRequested(clean, static_cast<int>(m_info.layout));
    return true;
}

QString MinecraftFolderBackend::pickFolderDialog()
{
    const QString start = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QFileDialog::getExistingDirectory(
        nullptr, tr("选择 Minecraft 游戏文件夹"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

} // namespace ShadowLauncher
