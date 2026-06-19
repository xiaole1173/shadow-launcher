#include "resource_backend.h"
#include "core/mod_manager.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

namespace ShadowLauncher {

// ============================================================
// Constructor / Destructor
// ============================================================

ResourceBackend::ResourceBackend(QObject* parent)
    : QObject(parent)
    , m_modMgr(new ModManager(this))
{
    connect(m_modMgr, &ModManager::searchCompleted,
            this, &ResourceBackend::onSearchCompleted);
    connect(m_modMgr, &ModManager::downloadProgress,
            this, &ResourceBackend::onDownloadProgress);
    connect(m_modMgr, &ModManager::downloadFinished,
            this, &ResourceBackend::onDownloadFinished);
    connect(m_modMgr, &ModManager::resourcepackSearchCompleted,
            this, &ResourceBackend::onResourcepackSearchCompleted);
    connect(m_modMgr, &ModManager::resourcepackDownloadFinished,
            this, &ResourceBackend::onResourcepackDownloadFinished);
    connect(m_modMgr, &ModManager::resourcepackVersionsLoaded,
            this, &ResourceBackend::onResourcepackVersionsLoaded);
    connect(m_modMgr, &ModManager::resourcepackVersionsPartial,
            this, &ResourceBackend::resourcepackVersionsPartial);
    connect(m_modMgr, &ModManager::resourcepackVersionsProgress,
            this, &ResourceBackend::resourcepackVersionsProgress);
    connect(m_modMgr, &ModManager::modVersionsLoaded,
            this, &ResourceBackend::onModVersionsLoaded);
    connect(m_modMgr, &ModManager::modVersionsPartial,
            this, &ResourceBackend::modVersionsPartial);
    connect(m_modMgr, &ModManager::modVersionsProgress,
            this, &ResourceBackend::modVersionsProgress);
    connect(m_modMgr, &ModManager::shaderVersionsLoaded,
            this, &ResourceBackend::onShaderVersionsLoaded);
    connect(m_modMgr, &ModManager::shaderVersionsPartial,
            this, &ResourceBackend::shaderVersionsPartial);
    connect(m_modMgr, &ModManager::shaderVersionsProgress,
            this, &ResourceBackend::shaderVersionsProgress);
    connect(m_modMgr, &ModManager::logMessage,
            this, &ResourceBackend::logMessage);
}

ResourceBackend::~ResourceBackend() = default;

// ============================================================
// Public Slots — Popular Mods / Shaders (offline data)
// ============================================================

QVariantList ResourceBackend::getPopularMods(const QString& loader)
{
    const auto mods = ModManager::getPopularMods(loader);
    QVariantList result;
    for (auto it = mods.cbegin(); it != mods.cend(); ++it) {
        const QJsonObject& info = it.value();
        QVariantMap entry;
        entry[QStringLiteral("slug")]     = it.key();
        entry[QStringLiteral("title")]    = info[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]     = info[QStringLiteral("desc")].toString();
        entry[QStringLiteral("icon")]     = info[QStringLiteral("icon")].toString();
        entry[QStringLiteral("category")] = info[QStringLiteral("category")].toString();
        result.append(entry);
    }
    emit logMessage(QStringLiteral("加载 %1 个 %2 推荐Mod")
                        .arg(result.size())
                        .arg(loader.toUpper()));
    return result;
}

QVariantList ResourceBackend::getShaderList()
{
    const auto shaders = ModManager::getShaderList();
    QVariantList result;
    for (auto it = shaders.cbegin(); it != shaders.cend(); ++it) {
        const QJsonObject& info = it.value();
        QVariantMap entry;
        entry[QStringLiteral("slug")]      = it.key();
        entry[QStringLiteral("title")]     = info[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]      = info[QStringLiteral("desc")].toString();
        entry[QStringLiteral("icon")]      = info[QStringLiteral("icon")].toString();
        entry[QStringLiteral("downloads")] = 0;
        result.append(entry);
    }
    emit logMessage(QStringLiteral("加载 %1 个光影包").arg(result.size()));
    return result;
}

// ============================================================
// Public Slots — Online Search
// ============================================================

void ResourceBackend::searchMods(const QString& query, const QString& loader)
{
    QStringList loaders;
    if (!loader.isEmpty())
        loaders << loader;

    emit logMessage(QStringLiteral("🔍 正在搜索Mod: %1 (%2)...").arg(query, loader));
    // Use dedicated search with project_type:mod facet
    m_modMgr->searchModrinthProjects(query, {QStringLiteral("project_type:mod")}, {}, loaders, 0, 20,
                             QStringLiteral("relevance"));
}

void ResourceBackend::searchShaders(const QString& query, const QString& gameVersion)
{
    QStringList versions;
    if (!gameVersion.isEmpty())
        versions << gameVersion;

    emit logMessage(QStringLiteral("🔍 正在搜索光影: %1...").arg(query));
    m_modMgr->searchModrinthProjects(query, {QStringLiteral("project_type:shader")}, versions, {}, 0, 20,
                             QStringLiteral("downloads"));
}

// ============================================================
// Public Slots — Download
// ============================================================

void ResourceBackend::downloadMod(const QString& slug, const QString& gameVersion, const QString& minecraftDir)
{
    if (m_downloading) {
        emit logMessage(QStringLiteral("已有下载任务进行中"));
        return;
    }

    m_downloading = true;
    emit downloadStateChanged();

    QString destDir = minecraftDir.isEmpty() ? m_minecraftDir : minecraftDir;
    emit logMessage(QStringLiteral("正在查找 %1 MC%2 的最新版本...").arg(slug, gameVersion));
    m_modMgr->downloadMod(slug, gameVersion, QStringLiteral("fabric"), destDir,
                          QStringLiteral("mods"));
}

void ResourceBackend::downloadShader(const QString& slug, const QString& gameVersion, const QString& minecraftDir)
{
    if (m_downloading) {
        emit logMessage(QStringLiteral("已有下载任务进行中"));
        return;
    }

    m_downloading = true;
    emit downloadStateChanged();

    QString destDir = minecraftDir.isEmpty() ? m_minecraftDir : minecraftDir;
    emit logMessage(QStringLiteral("正在查找 %1 MC%2 的最新版本...").arg(slug, gameVersion));
    m_modMgr->downloadShader(slug, gameVersion, destDir);
}

void ResourceBackend::searchResourcepacks(const QString& query, const QString& gameVersion, int offset, const QStringList& categories)
{
    emit logMessage(QStringLiteral("[MODRINTH] 搜索资源包: q=%1 gv=%2 offset=%3 cats=%4").arg(query, gameVersion).arg(offset).arg(categories.join(QChar(','))));
    m_modMgr->searchResourcepacks(query, gameVersion.isEmpty()
                                  ? QStringList{} : QStringList{gameVersion},
                                  categories, offset);
}

void ResourceBackend::downloadResourcepack(const QString& slug, const QString& gameVersion, const QString& minecraftDir)
{
    if (m_downloading) {
        emit logMessage(QStringLiteral("⚠️ 已有下载任务进行中"));
        return;
    }

    m_downloading = true;
    emit downloadStateChanged();

    emit logMessage(QStringLiteral("🔍 正在查找资源包 %1 ...").arg(slug));
    QString destDir = minecraftDir.isEmpty() ? m_minecraftDir : minecraftDir;
    m_modMgr->downloadResourcepack(slug, gameVersion, destDir);
}

void ResourceBackend::fetchResourcepackVersions(const QStringList& slugs)
{
    m_modMgr->fetchResourcepackVersions(slugs);
}

void ResourceBackend::fetchModVersions(const QStringList& slugs)
{
    m_modMgr->fetchModVersions(slugs);
}

void ResourceBackend::fetchShaderVersions(const QStringList& slugs)
{
    m_modMgr->fetchShaderVersions(slugs);
}

void ResourceBackend::cancelDownload()
{
    m_modMgr->cancel();
}

// ============================================================
// Private Slots — ModManager signal forwarding
// ============================================================

void ResourceBackend::onSearchCompleted(const QJsonArray& results, int /*totalHits*/)
{
    QVariantList list;
    for (const QJsonValue& item : results) {
        const QJsonObject obj = item.toObject();
        QVariantMap entry;
        entry[QStringLiteral("slug")]      = obj[QStringLiteral("slug")].toString();
        entry[QStringLiteral("title")]     = obj[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]      = obj[QStringLiteral("description")].toString();
        entry[QStringLiteral("icon")]      = obj[QStringLiteral("iconUrl")].toString();
        entry[QStringLiteral("downloads")] = obj[QStringLiteral("downloads")].toInt();
        list.append(entry);
    }

    emit logMessage(QStringLiteral("找到 %1 个结果").arg(list.size()));
    emit searchResultsReady(list);
}

void ResourceBackend::onDownloadProgress(const QString& name, qint64 received, qint64 total)
{
    m_dlProgress = static_cast<int>(received);
    m_dlTotal    = static_cast<int>(total);
    m_dlFile     = name;
    emit downloadProgressChanged(m_dlProgress, m_dlTotal, m_dlFile);
}

void ResourceBackend::onDownloadFinished(const QString& slug, bool success,
                                         const QString& filePath)
{
    m_downloading = false;
    m_dlProgress  = 0;
    m_dlTotal     = 0;
    m_dlFile.clear();
    emit downloadStateChanged();

    if (success) {
        emit logMessage(QStringLiteral("✅ %1 下载完成").arg(slug));
    } else {
        emit logMessage(QStringLiteral("❌ %1 下载失败，请检查版本兼容性").arg(slug));
    }

    emit downloadFinished(slug, success, filePath);
}

void ResourceBackend::onResourcepackSearchCompleted(const QJsonArray& results, int totalHits)
{
    QVariantList list;
    for (const QJsonValue& item : results) {
        const QJsonObject obj = item.toObject();
        QVariantMap entry;
        entry[QStringLiteral("slug")]      = obj[QStringLiteral("slug")].toString();
        entry[QStringLiteral("title")]     = obj[QStringLiteral("title")].toString();
        entry[QStringLiteral("desc")]      = obj[QStringLiteral("description")].toString();
        entry[QStringLiteral("icon")]      = obj[QStringLiteral("iconUrl")].toString();
        entry[QStringLiteral("downloads")] = obj[QStringLiteral("downloads")].toInt();
        entry[QStringLiteral("author")]    = obj[QStringLiteral("author")].toString();
        entry[QStringLiteral("updated")]   = obj[QStringLiteral("updated")].toString();
        entry[QStringLiteral("source")]    = QStringLiteral("Modrinth");
        // Categories as QVariant string list
        QJsonArray cats = obj[QStringLiteral("categories")].toArray();
        QStringList catList;
        for (const QJsonValue& catVal : cats) {
            catList.append(catVal.toString());
        }
        entry[QStringLiteral("categories")] = catList;

        // Features (split by parseSearchResponse)
        QJsonArray feats = obj[QStringLiteral("features")].toArray();
        QStringList featList;
        for (const QJsonValue& fv : feats) {
            featList.append(fv.toString());
        }
        entry[QStringLiteral("features")] = featList;

        // Resolutions (split by parseSearchResponse)
        QJsonArray resos = obj[QStringLiteral("resolutions")].toArray();
        QStringList resList;
        for (const QJsonValue& rv : resos) {
            resList.append(rv.toString());
        }
        entry[QStringLiteral("resolutions")] = resList;
        list.append(entry);
    }
    emit logMessage(QStringLiteral("找到 %1 个资源包").arg(list.size()));
    emit resourcepackSearchCompleted(list, totalHits);
}

void ResourceBackend::onResourcepackDownloadFinished(const QString& slug, bool success,
                                                      const QString& filePath)
{
    m_downloading = false;
    m_dlProgress  = 0;
    m_dlTotal     = 0;
    m_dlFile.clear();
    emit downloadStateChanged();
    emit resourcepackDownloadFinished(slug, success, filePath);
}

void ResourceBackend::onResourcepackVersionsLoaded(const QVariantMap& slugToVersions)
{
    emit resourcepackVersionsLoaded(slugToVersions);
}

void ResourceBackend::onModVersionsLoaded(const QVariantMap& slugToVersions)
{
    emit modVersionsLoaded(slugToVersions);
}

void ResourceBackend::onShaderVersionsLoaded(const QVariantMap& slugToVersions)
{
    emit shaderVersionsLoaded(slugToVersions);
}

} // namespace ShadowLauncher
