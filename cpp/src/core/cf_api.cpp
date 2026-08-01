// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — CurseForge API 源适配器
#include "cf_api.h"
#include "resource_fetch_engine.h"
#include "cf_key_crypto.h"
#include "../utils/logger.h"

#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDateTime>

#if __has_include("cf_api_key_local.h")
#include "cf_api_key_local.h"
#endif

namespace ShadowLauncher {

namespace {
// CF API 端点：镜像优先（MCIM /curseforge/v1/，免 key，files 端点 186ms 远快于官方 6.4s）
// 官方 api.curseforge.com 作为镜像失败（超时/429/5xx）时的降级源（需 x-api-key）
const QString kCfMirrorBase = QStringLiteral("https://mod.mcimirror.top/curseforge/v1");
const QString kCfOfficialBase = QStringLiteral("https://api.curseforge.com/v1");
} // namespace

namespace {

// ── CF 分类静态表（2026-08-01 API 实测提取；classId=6/12/6552/4471）──
struct CfCategory { int classId; int id; const char* name; };
const CfCategory kCfCategories[] = {
    // Mods (6)
    {6, 422, "Adventure and RPG"}, {6, 412, "Technology"}, {6, 419, "Magic"},
    {6, 406, "World Gen"}, {6, 434, "Armor, Tools, and Weapons"}, {6, 420, "Storage"},
    {6, 421, "API and Library"}, {6, 423, "Map and Information"}, {6, 436, "Food"},
    {6, 6814, "Performance"}, {6, 6821, "Bug Fixes"}, {6, 424, "Cosmetic"},
    {6, 425, "Miscellaneous"}, {6, 435, "Server Utility"}, {6, 426, "Addons"},
    {6, 5191, "Utility & QoL"}, {6, 4558, "Redstone"}, {6, 4906, "MCreator"},
    {6, 4671, "Twitch Integration"}, {6, 8937, "ModJam 2025"}, {6, 5299, "Education"},
    {6, 9026, "CreativeMode"}, {6, 10775, "Horror"},
    // Resource Packs (12)
    {12, 393, "16x"}, {12, 394, "32x"}, {12, 395, "64x"}, {12, 396, "128x"},
    {12, 397, "256x"}, {12, 398, "512x and Higher"}, {12, 400, "Photo Realistic"},
    {12, 403, "Traditional"}, {12, 5244, "Font Packs"}, {12, 4465, "Mod Support"},
    {12, 402, "Medieval"}, {12, 5193, "Data Packs"}, {12, 404, "Animated"},
    {12, 401, "Modern"}, {12, 399, "Steampunk"}, {12, 8939, "ModJam 2025"},
    {12, 405, "Miscellaneous"},
    // Shaders (6552)
    {6552, 6553, "Realistic"}, {6552, 6554, "Fantasy"}, {6552, 6555, "Vanilla"},
    // Modpacks (4471) — 整合包 tab 预留
    {4471, 4482, "Extra Large"}, {4471, 4481, "Small / Light"}, {4471, 4483, "Combat / PvP"},
    {4471, 4474, "Sci-Fi"}, {4471, 4475, "Adventure and RPG"}, {4471, 4487, "FTB Official Pack"},
    {4471, 4478, "Quests"}, {4471, 4472, "Tech"}, {4471, 4736, "Skyblock"},
    {4471, 4480, "Map Based"}, {4471, 7418, "Horror"}, {4471, 4484, "Multiplayer"},
    {4471, 4477, "Mini Game"}, {4471, 4473, "Magic"}, {4471, 5128, "Vanilla+"},
    {4471, 4479, "Hardcore"}, {4471, 4476, "Exploration"}, {4471, 9243, "Expert"},
    {4471, 10683, "RLCraft"},
};

bool isMcVersion(const QString& s)
{
    static const QRegularExpression re(QStringLiteral(R"(^\d+\.\d+(\.\d+)?$)"));
    return re.match(s).hasMatch();
}

bool isLoaderName(const QString& s)
{
    return s == QLatin1String("Forge") || s == QLatin1String("Fabric")
        || s == QLatin1String("Quilt") || s == QLatin1String("NeoForge")
        || s == QLatin1String("LiteLoader") || s == QLatin1String("Rift")
        || s == QLatin1String("Cauldron");
}

} // namespace

CfApi::CfApi(ResourceFetchEngine* engine, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
}

QString CfApi::apiKey() const
{
    if (!m_keyTried) {
        m_keyTried = true;
#if defined(SHADOW_CF_ENC_IKM_HEX)
        m_apiKey = CfKeyCrypto::decryptEmbeddedCfKey(
            SHADOW_CF_ENC_IKM_HEX, SHADOW_CF_ENC_SALT_HEX,
            SHADOW_CF_ENC_NONCE_HEX, SHADOW_CF_ENC_CIPHER_HEX, SHADOW_CF_ENC_TAG_HEX);
#endif
        if (m_apiKey.isEmpty())
            qCWarning(logDownload) << QStringLiteral("[CF] 未能解密内嵌 API Key（无 cf_api_key_local.h）");
    }
    return m_apiKey;
}

// 镜像优先请求：镜像失败（非 2xx/网络错误）→ 官方带 key 重试一次
void CfApi::getJsonWithFallback(const QString& mirrorUrl, const QString& officialUrl,
                                bool cacheable, ResourceFetchEngine::JsonDone done,
                                ResourceFetchEngine::JsonFail fail)
{
    if (!m_engine) { if (fail) fail(QStringLiteral("无司南引擎")); return; }
    auto officialReq = [this, officialUrl, cacheable, done, fail]() {
        ResourceFetchEngine::JsonHeaders h;
        const QString key = apiKey();
        if (!key.isEmpty())
            h.insert(QStringLiteral("x-api-key"), key);
        m_engine->getJson(officialUrl, cacheable, done, fail, h);
    };
    m_engine->getJson(mirrorUrl, cacheable,
        [officialReq, done, fail](int status, const QByteArray& body) {
            if (status >= 200 && status < 300) {
                if (done) done(status, body);
            } else {
                qCWarning(logDownload) << QStringLiteral("[CF] 镜像请求失败(status=%1)，降级官方").arg(status);
                officialReq();
            }
        },
        [officialReq, fail](const QString& err) {
            qCWarning(logDownload) << QStringLiteral("[CF] 镜像网络错误，降级官方: %1").arg(err);
            officialReq();
        });
}

void CfApi::search(int classId, const QString& query, int categoryId,
                   const QString& gameVersion, const QString& loader,
                   int index, int limit, SearchCb done, JsonFail fail)
{
    if (!m_engine) { if (fail) fail(QStringLiteral("无司南引擎")); return; }

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("gameId"), QStringLiteral("432"));
    params.addQueryItem(QStringLiteral("classId"), QString::number(classId));
    if (!query.isEmpty())
        params.addQueryItem(QStringLiteral("searchFilter"), query);
    if (categoryId > 0)
        params.addQueryItem(QStringLiteral("categoryId"), QString::number(categoryId));
    if (!gameVersion.isEmpty())
        params.addQueryItem(QStringLiteral("gameVersion"), gameVersion);
    const int lt = loaderTypeFor(loader);
    if (lt > 0)
        params.addQueryItem(QStringLiteral("modLoaderType"), QString::number(lt));
    params.addQueryItem(QStringLiteral("sortField"), QStringLiteral("6")); // 下载量
    params.addQueryItem(QStringLiteral("sortOrder"), QStringLiteral("desc"));
    params.addQueryItem(QStringLiteral("index"), QString::number(index));
    params.addQueryItem(QStringLiteral("pageSize"), QString::number(limit));
    const QString qs = params.toString(QUrl::FullyEncoded);
    const QString mirrorUrl = kCfMirrorBase + QStringLiteral("/mods/search?") + qs;
    const QString officialUrl = kCfOfficialBase + QStringLiteral("/mods/search?") + qs;

    getJsonWithFallback(mirrorUrl, officialUrl, true,
        [done](int status, const QByteArray& body) {
            if (status != 200) { if (done) done({}, 0); return; }
            QJsonDocument doc = QJsonDocument::fromJson(body);
            QJsonArray arr = doc.object().value(QStringLiteral("data")).toArray();
            const int total = doc.object().value(QStringLiteral("pagination"))
                                  .toObject().value(QStringLiteral("totalCount")).toInt();
            QVariantList items;
            items.reserve(arr.size());
            for (const QJsonValue& v : arr)
                items.append(toUnified(v.toObject()));
            if (done) done(items, total);
        },
        fail);
}

void CfApi::fetchFilesAsVersions(const QString& modId, const QString& gameVersion,
                                 const QString& loader,
                                 std::function<void(const QStringList&, const QVariantMap&)> done,
                                 JsonFail fail)
{
    if (!m_engine) { if (fail) fail(QStringLiteral("无司南引擎")); return; }

    QUrlQuery params;
    if (!gameVersion.isEmpty())
        params.addQueryItem(QStringLiteral("gameVersion"), gameVersion);
    const int lt = loaderTypeFor(loader);
    if (lt > 0)
        params.addQueryItem(QStringLiteral("modLoaderType"), QString::number(lt));
    params.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("50"));
    const QString qs = params.toString(QUrl::FullyEncoded);
    const QString mirrorUrl = kCfMirrorBase + QStringLiteral("/mods/%1/files?").arg(modId) + qs;
    const QString officialUrl = kCfOfficialBase + QStringLiteral("/mods/%1/files?").arg(modId) + qs;

    getJsonWithFallback(mirrorUrl, officialUrl, false,
        [done, gameVersion](int status, const QByteArray& body) {
            if (status != 200) { if (done) done({}, {}); return; }
            QJsonDocument doc = QJsonDocument::fromJson(body);
            const QJsonArray files = doc.object().value(QStringLiteral("data")).toArray();

            // 按 "mcVer|loader" 分组，每组取最新文件 → Modrinth 等价 version 结构
            QVariantMap detailMap;
            QStringList compositeVersions;
            for (const QJsonValue& fv : files) {
                QJsonObject f = fv.toObject();
                QString mcVer, loaderName;
                const QJsonArray gvs = f.value(QStringLiteral("gameVersions")).toArray();
                for (const QJsonValue& gv : gvs) {
                    const QString s = gv.toString();
                    if (isMcVersion(s)) mcVer = s;
                    else if (isLoaderName(s)) loaderName = s;
                }
                if (mcVer.isEmpty()) mcVer = gameVersion;
                if (loaderName.isEmpty()) loaderName = QStringLiteral("Any");
                const QString keyStr = mcVer + QLatin1Char('|') + loaderName.toLower();

                QVariantMap fileEntry;
                fileEntry.insert(QStringLiteral("url"), f.value(QStringLiteral("downloadUrl")).toString());
                fileEntry.insert(QStringLiteral("filename"), f.value(QStringLiteral("fileName")).toString());
                fileEntry.insert(QStringLiteral("size"), (qlonglong)f.value(QStringLiteral("fileLength")).toDouble());
                fileEntry.insert(QStringLiteral("date_published"), f.value(QStringLiteral("fileDate")).toString());
                fileEntry.insert(QStringLiteral("version_type"), f.value(QStringLiteral("releaseType")).toString());
                fileEntry.insert(QStringLiteral("version_number"), f.value(QStringLiteral("displayName")).toString());
                // CF hashes: algo 1=sha1, 2=md5 → 与 Modrinth 的 sha1 字段对齐（下载完整性校验）
                const QJsonArray hashes = f.value(QStringLiteral("hashes")).toArray();
                for (const QJsonValue& hv : hashes) {
                    const QJsonObject ho = hv.toObject();
                    if (ho.value(QStringLiteral("algo")).toInt() == 1)
                        fileEntry.insert(QStringLiteral("sha1"), ho.value(QStringLiteral("value")).toString());
                }
                // CF 依赖：relationType 2=required, 3=optional → 与 Modrinth dependency_type 对齐
                QVariantList depList;
                const QJsonArray deps = f.value(QStringLiteral("dependencies")).toArray();
                for (const QJsonValue& dv : deps) {
                    const QJsonObject dObj = dv.toObject();
                    QVariantMap dep;
                    dep.insert(QStringLiteral("project_id"), QString::number((qlonglong)dObj.value(QStringLiteral("modId")).toDouble()));
                    const int rel = dObj.value(QStringLiteral("relationType")).toInt();
                    dep.insert(QStringLiteral("dependency_type"),
                               rel == 3 ? QStringLiteral("optional") : QStringLiteral("required"));
                    depList.append(dep);
                }
                fileEntry.insert(QStringLiteral("dependencies"), depList);

                if (!detailMap.contains(keyStr)) {
                    QVariantMap d;
                    d.insert(QStringLiteral("game_version"), mcVer);
                    d.insert(QStringLiteral("game_versions"), QVariantList{mcVer});
                    d.insert(QStringLiteral("loaders"), QVariantList{loaderName.toLower()});
                    d.insert(QStringLiteral("files"), QVariantList{fileEntry});
                    // 顶层便捷字段（与 Modrinth 详情结构对齐，QML 直接读取）
                    d.insert(QStringLiteral("version_number"), fileEntry.value(QStringLiteral("version_number")));
                    d.insert(QStringLiteral("date_published"), fileEntry.value(QStringLiteral("date_published")));
                    d.insert(QStringLiteral("downloads"), 0);
                    d.insert(QStringLiteral("url"), fileEntry.value(QStringLiteral("url")));
                    d.insert(QStringLiteral("filename"), fileEntry.value(QStringLiteral("filename")));
                    d.insert(QStringLiteral("size"), fileEntry.value(QStringLiteral("size")));
                    d.insert(QStringLiteral("sha1"), fileEntry.value(QStringLiteral("sha1")));
                    detailMap.insert(keyStr, d);
                    compositeVersions.append(keyStr);
                } else {
                    // 同组已有文件：保留第一个（API 默认最新在前）
                }
            }
            // 版本倒序（新版在前，与 Modrinth 体验一致）
            std::sort(compositeVersions.begin(), compositeVersions.end(),
                      [](const QString& a, const QString& b) {
                          auto parse = [](const QString& v) -> std::tuple<int,int,int> {
                              const QString ver = v.section(QLatin1Char('|'), 0, 0);
                              const QStringList pts = ver.split(QLatin1Char('.'));
                              return { pts.size() > 0 ? pts[0].toInt() : 0,
                                       pts.size() > 1 ? pts[1].toInt() : 0,
                                       pts.size() > 2 ? pts[2].toInt() : 0 };
                          };
                          return parse(a) > parse(b);
                      });
            if (done) done(compositeVersions, detailMap);
        },
        fail);
}

QVariantMap CfApi::toUnified(const QJsonObject& mod)
{
    QVariantMap m;
    m.insert(QStringLiteral("slug"), QString::number((qlonglong)mod.value(QStringLiteral("id")).toDouble()));
    m.insert(QStringLiteral("title"), mod.value(QStringLiteral("name")).toString());
    m.insert(QStringLiteral("desc"), mod.value(QStringLiteral("summary")).toString());
    m.insert(QStringLiteral("icon"), mod.value(QStringLiteral("logo"))
                                        .toObject().value(QStringLiteral("thumbnailUrl")).toString());
    m.insert(QStringLiteral("downloads"), mod.value(QStringLiteral("downloadCount")).toDouble());
    m.insert(QStringLiteral("dateModified"), mod.value(QStringLiteral("dateModified")).toString());

    // 版本/加载器从 latestFiles 提取（gameVersions 混着版本/加载器/环境）
    QStringList vers, loaders;
    const QJsonArray files = mod.value(QStringLiteral("latestFiles")).toArray();
    for (const QJsonValue& fv : files) {
        const QJsonArray gvs = fv.toObject().value(QStringLiteral("gameVersions")).toArray();
        for (const QJsonValue& gv : gvs) {
            const QString s = gv.toString();
            if (isMcVersion(s)) vers.append(s);
            else if (isLoaderName(s)) loaders.append(s);
        }
    }
    vers.removeDuplicates();
    loaders.removeDuplicates();
    m.insert(QStringLiteral("versions"), vers.join(QLatin1Char(',')));
    m.insert(QStringLiteral("loader"), loaders.join(QLatin1Char(',')));
    m.insert(QStringLiteral("loadersList"), loaders);
    m.insert(QStringLiteral("clientSide"), QString());
    m.insert(QStringLiteral("source"), QStringLiteral("CurseForge"));
    return m;
}

QVariantList CfApi::categories(int classId)
{
    QVariantList out;
    for (const auto& c : kCfCategories) {
        if (c.classId == classId) {
            QVariantMap m;
            m.insert(QStringLiteral("id"), c.id);
            m.insert(QStringLiteral("name"), QString::fromUtf8(c.name));
            m.insert(QStringLiteral("value"), QStringLiteral("cf:%1").arg(c.id));
            out.append(m);
        }
    }
    return out;
}

QString CfApi::categoryLabel(int classId, int categoryId)
{
    for (const auto& c : kCfCategories) {
        if (c.classId == classId && c.id == categoryId)
            return QStringLiteral("CF·%1").arg(QString::fromUtf8(c.name));
    }
    return QString();
}

int CfApi::loaderTypeFor(const QString& loader)
{
    const QString l = loader.toLower();
    if (l == QLatin1String("forge")) return 1;
    if (l == QLatin1String("liteloader")) return 3;
    if (l == QLatin1String("fabric")) return 4;
    if (l == QLatin1String("quilt")) return 5;
    if (l == QLatin1String("neoforge")) return 6;
    return 0;
}

QString CfApi::loaderNameFor(int type)
{
    switch (type) {
    case 1: return QStringLiteral("forge");
    case 3: return QStringLiteral("liteloader");
    case 4: return QStringLiteral("fabric");
    case 5: return QStringLiteral("quilt");
    case 6: return QStringLiteral("neoforge");
    default: return QString();
    }
}

bool CfApi::isCfCategory(const QString& category)
{
    return category.startsWith(QStringLiteral("cf:"));
}

int CfApi::cfCategoryId(const QString& category)
{
    if (!isCfCategory(category)) return 0;
    return category.mid(3).toInt();
}

} // namespace ShadowLauncher
