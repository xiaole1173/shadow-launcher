#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include <functional>

namespace ShadowLauncher {

class ResourceFetchEngine;

/// ─────────────────────────────────────────────────────────────
/// CurseForge API 源适配器（走司南引擎 getJson + x-api-key 头）
///
/// classId：6=Mods  12=ResourcePacks  6552=Shaders  4471=Modpacks
/// 数据模型：toUnified() 产出与 Modrinth parseSearchResponseItems
/// 完全同构的 QVariantMap → QML 显示零改动，卡片 source="CurseForge"
/// ─────────────────────────────────────────────────────────────
class CfApi : public QObject {
    Q_OBJECT
public:
    explicit CfApi(ResourceFetchEngine* engine, QObject* parent = nullptr);

    using JsonFail = std::function<void(const QString& error)>;
    using SearchCb = std::function<void(const QVariantList& items, int total)>;

    /// 搜索（index/limit 分页，按下载量降序）；categoryId<=0 不限制
    void search(int classId, const QString& query, int categoryId,
                const QString& gameVersion, const QString& loader,
                int index, int limit, SearchCb done, JsonFail fail);

    /// 文件列表 → Modrinth 等价版本结构（composite key "mcVer|loader"）
    /// 供详情页复用现有 onModVersionsPartial 渲染
    void fetchFilesAsVersions(const QString& modId, const QString& gameVersion,
                              const QString& loader,
                              std::function<void(const QStringList& versions,
                                                 const QVariantMap& details)> done,
                              JsonFail fail);

    /// CF 搜索条目 → 统一结果模型（slug=数字id，source=CurseForge）
    static QVariantMap toUnified(const QJsonObject& mod);

    /// 分类静态表（classId → [{id, name}]，从 API 实测提取，避免启动慢请求）
    static QVariantList categories(int classId);
    /// 分类显示名（"CF·Adventure and RPG"）
    static QString categoryLabel(int classId, int categoryId);

    /// 加载器 → CF modLoaderType（forge=1 fabric=4 quilt=5 neoforge=6 liteloader=3）
    static int loaderTypeFor(const QString& loader);
    static QString loaderNameFor(int type);

    /// 分类值判断："cf:422" → CF 分类 422
    static bool isCfCategory(const QString& category);
    static int cfCategoryId(const QString& category);

private:
    QString apiKey() const;
    ResourceFetchEngine* m_engine;
    mutable QString m_apiKey;
    mutable bool m_keyTried = false;
};

} // namespace ShadowLauncher
