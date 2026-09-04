// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_exporter.cpp — 整合包导出（完全对齐主流启动器实现 PageInstanceExport）。
//
// 全部磁盘 IO / 哈希 / 网络查询 / 压缩在 QtConcurrent worker 线程执行，
// 进度与结果经 invokeMethod 回主线程（跨线程写成员有数据竞争，必须回投）。
// ZipArchive 单工作线程串行调用（miniz 内部持 FILE* 状态）。

#include "modpack_exporter.h"

#include "zip_archive.h"
#include "../../utils/logger.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QSet>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>
#include <QUrl>
#include <QtConcurrent>

#include <memory>

namespace ShadowLauncher {

ModpackExporter::ModpackExporter(QObject* parent) : QObject(parent) {}

void ModpackExporter::setGameDir(const QString& dir)
{
    m_gameDir = dir;
    // CF API Key 自动加载（与 ModpackImporter 同款）：环境变量 → {gameDir}/config/cf_api_key.json
    if (m_cfApiKey.isEmpty()) {
        QString key = qEnvironmentVariable("SHADOW_CF_API_KEY");
        if (!key.isEmpty()) {
            m_cfApiKey = key;
            return;
        }
        if (!dir.isEmpty()) {
            const QString path = dir + QStringLiteral("/config/cf_api_key.json");
            QFile f(path);
            if (f.exists() && f.open(QIODevice::ReadOnly)) {
                const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
                m_cfApiKey = obj.value(QStringLiteral("apiKey")).toString();
            }
        }
    }
}

void ModpackExporter::setProgress(qreal p, const QString& text)
{
    // 后台线程调用 → 回主线程更新（跨线程写成员有数据竞争）
    QMetaObject::invokeMethod(this, [this, p, text]() {
        m_progress = p;
        m_statusText = text;
        emit progressChanged();
    });
}

bool ModpackExporter::waitLookupDecision(int platform, const QString& detail)
{
    // worker 线程内：发信号让主线程 QML 弹窗（QueuedConnection 自动排队），
    // 轮询等待 continueAfterLookupFailure 写入结果（主流启动器 弹窗询问语义）。
    m_lookupContinue.storeRelaxed(-1);
    emit lookupFailed(platform, detail);
    // ── 兜底（2026-08-30）：确认框异常（弹窗丢失/接线错误）时最多等 90s，
    // 超时自动继续（未查到直装），避免 worker 无限死等导致导出永久卡住。──
    QElapsedTimer guard;
    guard.start();
    while (m_lookupContinue.loadRelaxed() < 0) {
        if (m_cancel.loadRelaxed()) return false;
        if (guard.elapsed() > 90000) {
            qCWarning(logMod) << "[导出] 联网失败确认超时（90s），自动继续（未查到直装）";
            return true;
        }
        QThread::msleep(50);
    }
    return m_lookupContinue.loadRelaxed() == 1;
}

void ModpackExporter::cancel()
{
    m_cancel.storeRelaxed(1);
    m_lookupContinue.storeRelaxed(1);   // 取消等待中的用户确认
}

void ModpackExporter::continueAfterLookupFailure(bool cont)
{
    m_lookupContinue.storeRelaxed(cont ? 1 : 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// 版本内容根：隔离子目录 game/ > 版本内目录（versions/{id}/ 直接含游戏文件，如
// 26.2-forge-65.1.0 的 mods/config/saves 就在版本文件夹内）> 共享 .minecraft 根
static QString versionContentRoot(const QString& gameDir, const QString& versionId)
{
    const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
    if (QDir(versionDir + QStringLiteral("/game")).exists())
        return versionDir + QStringLiteral("/game");
    const QDir vd(versionDir);
    if (vd.exists()) {
        const bool hasGameContent =
            vd.exists(QStringLiteral("mods")) || vd.exists(QStringLiteral("config"))
            || vd.exists(QStringLiteral("saves")) || vd.exists(QStringLiteral("resourcepacks"))
            || vd.exists(QStringLiteral("shaderpacks")) || vd.exists(QStringLiteral("options.txt"))
            || vd.exists(QStringLiteral("logs"));
        if (hasGameContent) return versionDir;
    }
    return gameDir;
}

// 目录存在且非空（选项可见性“有内容才出现”语义）
static bool dirHasContent(const QString& dir)
{
    const QDir d(dir);
    return d.exists() && !d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

// 资源包/光影子项黑名单（同主流启动器 SubOptionBlackList：UI 不列 + 收集时兜底排除）
static const QStringList kSubBlacklist = {
    QStringLiteral("Quark Programmer Art.zip"),
    QStringLiteral("+ EuphoriaPatches_"),
    QStringLiteral("PCL2 Skin.zip")};

QVariantList ModpackExporter::listSaves(const QString& versionId) const
{
    // 版本内容根（隔离子目录 game/ > 版本内目录 > 共享根）
    const QString savesRoot = versionContentRoot(m_gameDir, versionId) + QStringLiteral("/saves");
    QVariantList out;
    const QDir savesDir(savesRoot);
    if (savesDir.exists()) {
        // 同主流启动器 ReloadSubOptions：按最后修改时间倒序（最新在前）
        const auto infos = savesDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const auto& fi : infos) {
            QVariantMap m;
            m.insert(QStringLiteral("name"), fi.fileName());
            m.insert(QStringLiteral("modified"),
                     fi.lastModified().toString(QStringLiteral("yyyy/MM/dd HH:mm")));
            out.append(m);
        }
    }
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// 导出选项表（完全对齐主流启动器实现 PageInstanceExport.xaml 的 ExportOption 集合）
// 隐私敏感项（个人信息/地图/JEI/EMI/帕秋莉/服务器列表）默认不勾选
// ═════════════════════════════════════════════════════════════════════════════

const QList<ModpackExporter::ExportOptionDef>& ModpackExporter::optionDefs()
{
    static const QList<ExportOptionDef> defs = {
        // —— 基础 ——
        {"options", "游戏本体设置", "按键、音量、视频设置等",
         {"options.txt", "configureddefaults/"}, true, false, false, false,
         {"options.txt", "configureddefaults/"}, {}},
        {"personal", "游戏本体个人信息", "命令历史、已保存的快捷栏（默认不导出）",
         {"hotbar.nbt", "command_history.txt"}, false, false, false, false,
         {"hotbar.nbt", "command_history.txt"}, {}},
        {"optifine", "OptiFine 设置", "",
         {"optionsof.txt", "optionsshaders.txt"}, true, false, true, false,
         {"optionsof.txt", "optionsshaders.txt"}, {}},
        // —— Mod 及其子项（主流启动器 子项面板绑定父选项勾选）——
        {"mod", "模组 (mods/)", "模组本体，含 coremods/lib（原版隐藏）",
         {"mods/", "coremods/", "lib/", "!mods/*.disabled", "!mods/*.old", "!mods/.connector/"},
         true, true, false, false, {"mods/", "coremods/", "lib/"}, {}},
        {"mod-disabled", "已禁用的 Mod", "打包 .disabled/.old 文件（默认排除）",
         {"mods/*.disabled", "mods/*.old"}, false, true, false, false,
         {"mods/*.disabled", "mods/*.old"}, QStringLiteral("mod")},
        {"packdata", "整合包重要数据", "脚本、内置资源包、数据包等",
         {"hotai/", "bansoukou/", "addons/", "multiblocked/", "modpack-update-checker/",
          "global_packs/", "global_resource_packs/", "global_data_packs/", "optional_data_packs/",
          "moonlight-global-datapacks/", "maps/", "icon.png", "mods-resourcepacks/", "matmos/",
          "resource_assorts/", "resource_assorts.json", "patchouli_books/", "datapacks/",
          "kubejs*/", "!kubejs*/probe/", "!kubejs*/exported/", "!kubejs*/jsconfig.json", "!kubejs*/README.txt",
          "openloader/", "worldshape/", "resources/", "scripts/", "structures/", "fontfiles/",
          "oresources/", "packmenu/", "craftpresence/", "pointblanks/", "template*/",
          "!template*/playerdata/", "!template*/stats/"},
         true, true, false, false,
         {"global_packs/", "datapacks/", "kubejs*/", "scripts/", "resources/", "openloader/",
          "maps/", "icon.png", "hotai/"}, QStringLiteral("mod")},
        {"config", "Mod 设置 (config/)", "模组配置文件（排除账号/隐私文件）",
         {"config/", "!config/jei/world/", "!config/worldedit/", "config/worldedit/worldedit.properties",
          "!config/spark/", "config/spark/config.json", "defaultconfigs/", "journeymap/config/",
          "journeymap/server/", "TrashSlotSaveState.json", "customfov.txt", "gg.essential.mod/",
          "!essential/", "!essential/*/", "!essential/*.jar*", "!essential/screenshot-checksum-caches.json",
          "!essential/microsoft_accounts.json", "paragliderSettings.nbt", "local/client_config.json",
          "local/ftbl.json", "local/client/sidebar_buttons.json", "local/client/ftbutilities.cfg",
          "local/client/ftblib.cfg", "local/client/xencraft.cfg", "liteloader.properties",
          "default_reference.xml", "CustomSkinLoader/CustomSkinLoader.json"},
         true, true, false, false, {"config/", "defaultconfigs/"}, QStringLiteral("mod")},
        {"tacz", "TaCZ 枪包", "Timeless and Classics Guns 模组的枪包数据",
         {"tacz/", "config/tacz/custom/"}, true, true, false, false,
         {"tacz/", "config/tacz/custom/"}, QStringLiteral("mod")},
        {"immersive", "已上传的沉浸画", "immersive_paintings 目录",
         {"immersive_paintings/"}, true, true, false, false,
         {"immersive_paintings/"}, QStringLiteral("mod")},
        {"mapdata", "已绘制的地图", "地图类 Mod 的世界/服务器地图、路标点（默认不导出）",
         {"journeymap/data/", "xaero/", "XaeroWaypoints/", "XaeroWorldMap/"}, false, true, false, false,
         {"journeymap/data/", "xaero/"}, QStringLiteral("mod")},
        {"jei", "JEI 个人信息", "物品收藏夹等（默认不导出）",
         {"config/jei/world/"}, false, true, false, false,
         {"config/jei/world/"}, QStringLiteral("mod")},
        {"emi", "EMI 个人信息", "物品收藏夹、默认配方、合成历史（默认不导出）",
         {"emi.json"}, false, true, false, false,
         {"emi.json"}, QStringLiteral("mod")},
        {"patchouli", "帕秋莉手册个人信息", "教程书已读记录、书签（默认不导出）",
         {"patchouli_data.json"}, false, true, false, false,
         {"patchouli_data.json"}, QStringLiteral("mod")},
        // —— 资源/光影 ——
        {"resourcepacks", "资源包 (resourcepacks/)", "纹理包、材质包",
         {"resourcepacks/", "texturepacks/"}, true, false, false, false, {"resourcepacks/", "texturepacks/"}, {}},
        {"shaderpacks", "光影包 (shaderpacks/)", "需 Mod 或 OptiFine（原版隐藏）",
         {"shaderpacks/"}, true, false, false, true, {"shaderpacks/"}, {}},
        // —— 附加（默认不导出）——
        {"screenshots", "截图", "screenshots/ 目录（默认不导出）",
         {"screenshots/"}, false, false, false, false, {"screenshots/"}, {}},
        {"schematics", "导出的结构", "schematics 文件（默认不导出）",
         {"schematics/"}, false, false, false, false, {"schematics/"}, {}},
        {"replay", "录像回放", "Replay Mod 的录像文件（默认不导出）",
         {"replay_recordings/", "replay_videos/"}, false, true, false, false,
         {"replay_recordings/", "replay_videos/"}, {}},
        {"saves", "单机游戏存档", "世界/地图（按存档子项勾选）",
         {"saves/"}, false, false, false, false, {"saves/"}, {}},
        {"license", "协议", "Licence 文件",
         {"LICEN*"}, true, false, false, false, {"LICEN*"}, {}},
        {"servers", "多人游戏服务器列表", "servers.dat（默认不导出）",
         {"servers.dat"}, false, false, false, false, {"servers.dat"}, {}},
    };
    return defs;
}

// ═════════════════════════════════════════════════════════════════════════════
// 主流启动器 Like 通配匹配（* ? [] 字符集；大小写不敏感；目录规则以 \ 结尾=前缀匹配）
// ═════════════════════════════════════════════════════════════════════════════

static bool likeMatchImpl(const QString& p, int pi, const QString& t, int ti)
{
    while (pi < p.size()) {
        const QChar pc = p.at(pi);
        if (pc == QLatin1Char('*')) {
            // 连续 * 合并
            while (pi + 1 < p.size() && p.at(pi + 1) == QLatin1Char('*')) ++pi;
            if (pi + 1 == p.size()) return true;    // 尾部 * 匹配一切
            for (int k = ti; k <= t.size(); ++k)
                if (likeMatchImpl(p, pi + 1, t, k)) return true;
            return false;
        }
        if (ti >= t.size()) return false;
        const QChar tc = t.at(ti);
        if (pc == QLatin1Char('?')) {
            ++pi; ++ti;
            continue;
        }
        if (pc == QLatin1Char('[')) {
            // 字符集 [abc] / [a-z]，支持 ^ 取反
            int j = pi + 1;
            bool negate = false;
            if (j < p.size() && p.at(j) == QLatin1Char('^')) { negate = true; ++j; }
            bool matched = false;
            while (j < p.size() && p.at(j) != QLatin1Char(']')) {
                if (j + 2 < p.size() && p.at(j + 1) == QLatin1Char('-') && p.at(j + 2) != QLatin1Char(']')) {
                    if (tc >= p.at(j) && tc <= p.at(j + 2)) matched = true;
                    j += 3;
                } else {
                    if (tc == p.at(j)) matched = true;
                    ++j;
                }
            }
            if (j >= p.size()) return false;    // 未闭合的 [ 不匹配
            if (matched == negate) return false;
            pi = j + 1; ++ti;
            continue;
        }
        if (pc.toLower() != tc.toLower()) return false;
        ++pi; ++ti;
    }
    return ti >= t.size();
}

/// 规则匹配：支持 \ 或 / 结尾的目录前缀规则（主流启动器 StandardizeLines 加 * 语义）与 ! 反转（由调用方处理）
static bool likeMatch(const QString& rawPattern, const QString& text)
{
    QString pat = rawPattern;
    if (pat.endsWith(QLatin1Char('\\')) || pat.endsWith(QLatin1Char('/')))
        pat += QLatin1Char('*');
    return likeMatchImpl(pat, 0, text, 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// 导出上下文（动态渲染选项，同主流启动器 ShowRules）
// ═════════════════════════════════════════════════════════════════════════════

QVariantMap ModpackExporter::exportContext(const QString& versionId) const
{
    QVariantMap ctx;
    const QString versionDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
    const bool versionExists = QFileInfo::exists(jsonPath);
    ctx.insert(QStringLiteral("versionExists"), versionExists);
    // 版本内容根：隔离子目录 game/ > 版本内目录 > 共享根
    const QString contentRoot = versionContentRoot(m_gameDir, versionId);

    // 模组加载器 / OptiFine（读版本 JSON libraries，主流启动器 Modable/HasOptiFine 同款）
    bool modable = false;
    bool hasOptiFine = false;
    if (versionExists) {
        QFile f(jsonPath);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
            const QJsonArray libs = root.value(QStringLiteral("libraries")).toArray();
            for (const auto& lv : libs) {
                const QString name = lv.toObject().value(QStringLiteral("name")).toString();
                // Forge 家族：主库 net.minecraftforge:forge: 或附属库（eventbus/modlauncher/
                // coremods 等）——整合包导入的 JSON 常无主库只有家族库，必须能识别
                if (name.startsWith(QStringLiteral("net.minecraftforge:"))
                    || name.startsWith(QStringLiteral("net.neoforged:"))
                    || name.contains(QStringLiteral("fabric-loader"))
                    || name.contains(QStringLiteral("quilt-loader")))
                    modable = true;
                if (name.contains(QStringLiteral("optifine"), Qt::CaseInsensitive))
                    hasOptiFine = true;
            }
        }
    }
    ctx.insert(QStringLiteral("modable"), modable);
    ctx.insert(QStringLiteral("hasOptiFine"), hasOptiFine);

    ctx.insert(QStringLiteral("hasMods"), dirHasContent(contentRoot + QStringLiteral("/mods")));
    ctx.insert(QStringLiteral("hasConfig"), dirHasContent(contentRoot + QStringLiteral("/config")));
    ctx.insert(QStringLiteral("hasShaderpacks"), dirHasContent(contentRoot + QStringLiteral("/shaderpacks")));
    ctx.insert(QStringLiteral("hasResourcepacks"), dirHasContent(contentRoot + QStringLiteral("/resourcepacks")));
    ctx.insert(QStringLiteral("hasSaves"), dirHasContent(contentRoot + QStringLiteral("/saves")));
    ctx.insert(QStringLiteral("hasScreenshots"), dirHasContent(contentRoot + QStringLiteral("/screenshots")));
    ctx.insert(QStringLiteral("hasServersDat"), QFileInfo::exists(contentRoot + QStringLiteral("/servers.dat")));

    // Java 可用性（java_cache 有任一 JRE）——同主流启动器 RefreshJavaInfo：无 Java 时隐藏/禁用
    const QDir javaRoot(QCoreApplication::applicationDirPath() + QStringLiteral("/java_cache"));
    ctx.insert(QStringLiteral("javaAvailable"),
               javaRoot.exists() && !javaRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());

    // 选项可见性（同主流启动器 RefreshAllOptionsUI）：Require* 门槛 + ShowRules 匹配实际内容
    QVariantList optList;
    const auto& defs = optionDefs();
    for (const auto& d : defs) {
        bool visible = true;
        if (d.requireModLoader && !modable) visible = false;
        if (d.requireOptiFine && !hasOptiFine) visible = false;
        if (d.requireModLoaderOrOptiFine && !modable && !hasOptiFine) visible = false;
        if (visible && !d.showRules.isEmpty()) {
            // ShowRules 判定（主流启动器 RefreshAllOptionsUI 语义：前两级别举 + 三级精确检查）
            //   无 / 文件规则 → 精确文件存在（含 * 通配 → 根目录 Like 匹配）
            //   路径规则 → 目录段数：1 段=一级目录条目存在（空目录也显示，同主流启动器）；
            //               ≥2 段=精确目录存在且非空 / 文件存在（没装对应 Mod 就不出现）
            visible = false;
            auto matchShowRule = [&](const QString& rule) {
                const int slash = rule.indexOf(QLatin1Char('/'));
                if (slash < 0) {
                    if (rule.contains(QLatin1Char('*')) || rule.contains(QLatin1Char('?'))) {
                        const QDir root(contentRoot);
                        if (!root.exists()) return false;
                        const auto files = root.entryList(QDir::Files, QDir::Name);
                        for (const auto& f : files)
                            if (likeMatch(rule, f)) return true;
                        return false;
                    }
                    return QFileInfo::exists(contentRoot + QLatin1Char('/') + rule);
                }
                const QString dirPart = rule.left(rule.lastIndexOf(QLatin1Char('/')) + 1);
                const int segs = dirPart.count(QLatin1Char('/'));
                if (segs <= 1) {
                    QString top = dirPart;
                    if (top.endsWith(QLatin1Char('/'))) top.chop(1);
                    if (top.isEmpty()) return false;
                    if (top.contains(QLatin1Char('*')) || top.contains(QLatin1Char('?'))) {
                        const QDir root(contentRoot);
                        if (!root.exists()) return false;
                        const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                        for (const auto& d : dirs)
                            if (likeMatch(top, d) && dirHasContent(root.filePath(d))) return true;
                        return false;
                    }
                    // 一级目录：有内容才显示（用户语义“有得导才出现”；空目录不产生选项）
                    return dirHasContent(contentRoot + QLatin1Char('/') + top);
                }
                // 二级及以上：精确检查（主流启动器 三级精确 IsValidDirectory / 文件存在）
                const QString path = contentRoot + QLatin1Char('/') + rule;
                const QFileInfo fi(path);
                if (rule.endsWith(QLatin1Char('/')))
                    return fi.isDir()
                        && !QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
                if (rule.contains(QLatin1Char('*')) || rule.contains(QLatin1Char('?')))
                    return QDir(contentRoot + QLatin1Char('/') + dirPart).exists();   // 通配文件：目录存在即可
                return fi.isFile();
            };
            for (const auto& r : d.showRules) {
                if (matchShowRule(r)) { visible = true; break; }
            }
        }
        if (!visible) continue;
        QVariantMap om;
        om.insert(QStringLiteral("id"), d.id);
        om.insert(QStringLiteral("title"), d.title);
        om.insert(QStringLiteral("description"), d.description);
        om.insert(QStringLiteral("defaultChecked"), d.defaultChecked);
        om.insert(QStringLiteral("privacy"), !d.defaultChecked);
        om.insert(QStringLiteral("parent"), d.parent);
        optList.append(om);
    }
    ctx.insert(QStringLiteral("options"), optList);

    // 资源包/光影子项（同主流启动器 ReloadSubOptions：zip/rar + 文件夹，按修改时间倒序，
    // 黑名单过滤，texturepacks 合并；空目录剔除）
    auto subItems = [](const QString& dirA, const QString& dirB) {
        QVariantList out;
        auto collectDir = [&out](const QString& dir) {
            if (dir.isEmpty()) return;   // QDir("") 会解析为 CWD（启动器根目录），必须拦
            const QDir d(dir);
            if (!d.exists()) return;
            const auto files = d.entryInfoList(QStringList() << QStringLiteral("*.zip") << QStringLiteral("*.rar"),
                                               QDir::Files, QDir::Name);
            for (const auto& fi : files) {
                bool black = false;
                for (const auto& b : kSubBlacklist)
                    if (fi.fileName().contains(b)) { black = true; break; }
                if (black) continue;
                QVariantMap m;
                m.insert(QStringLiteral("name"), fi.fileName());
                m.insert(QStringLiteral("type"), QStringLiteral("file"));
                out.append(m);
            }
            const auto dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);   // 时间倒序
            for (const auto& di : dirs) {
                if (di.fileName().startsWith(QLatin1Char('.'))) continue;
                bool black = false;
                for (const auto& b : kSubBlacklist)
                    if (di.fileName().contains(b)) { black = true; break; }
                if (black) continue;
                // 空目录不列（同主流启动器 IsValidDirectory）
                QDirIterator it(di.absoluteFilePath(), QDir::NoDotAndDotDot | QDir::AllEntries);
                if (!it.hasNext()) continue;
                QVariantMap m;
                m.insert(QStringLiteral("name"), di.fileName());
                m.insert(QStringLiteral("type"), QStringLiteral("dir"));
                out.append(m);
            }
        };
        collectDir(dirA);
        collectDir(dirB);
        return out;
    };
    ctx.insert(QStringLiteral("rpItems"),
               subItems(contentRoot + QStringLiteral("/resourcepacks"),
                        contentRoot + QStringLiteral("/texturepacks")));
    ctx.insert(QStringLiteral("shaderItems"),
               subItems(contentRoot + QStringLiteral("/shaderpacks"), QString()));
    return ctx;
}

// ═════════════════════════════════════════════════════════════════════════════
// CurseForge fingerprint：MurmurHash2 32 位，种子 1，计算前剔除 \t\n\r 空格
// （主流启动器 LocalResourceFile.CurseForgeHash 同款，CF 官方指纹算法）
// ═════════════════════════════════════════════════════════════════════════════

static quint32 cfMurmurHash2(const QByteArray& raw)
{
    QByteArray d;
    d.reserve(raw.size());
    for (char b : raw) {
        if (b == 9 || b == 10 || b == 13 || b == 32) continue;
        d.append(b);
    }
    const int len = d.size();
    const auto* p = reinterpret_cast<const unsigned char*>(d.constData());
    quint32 h = 1u ^ static_cast<quint32>(len);   // seed = 1
    const quint32 m = 0x5BD1E995u;
    int i = 0;
    while (i + 4 <= len) {
        quint32 k = p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | (p[i + 3] << 24);
        i += 4;
        k *= m; k ^= k >> 24; k *= m;
        h *= m; h ^= k;
    }
    switch (len - i) {
    case 3: h ^= p[i] | (p[i + 1] << 8); h ^= static_cast<quint32>(p[i + 2]) << 16; h *= m; break;
    case 2: h ^= p[i] | (p[i + 1] << 8); h *= m; break;
    case 1: h ^= p[i]; h *= m; break;
    }
    h ^= h >> 13; h *= m; h ^= h >> 15;
    return h;
}

// CF downloadUrl 域名变体展开（主流启动器 ResourceVersion.ParseCurseForgeDownloadUrls 同款）
static QStringList expandCfDownloadUrls(const QString& url)
{
    QStringList out;
    QString v1 = url;
    out << v1.replace(QStringLiteral("-service.overwolf.wtf"), QStringLiteral(".forgecdn.net"))
              .replace(QStringLiteral("://edge."), QStringLiteral("://mediafilez."))
              .replace(QStringLiteral("://media."), QStringLiteral("://mediafilez."));
    QString v2 = url;
    out << v2.replace(QStringLiteral("://edge."), QStringLiteral("://mediafilez."))
              .replace(QStringLiteral("://media."), QStringLiteral("://mediafilez."));
    QString v3 = url;
    out << v3.replace(QStringLiteral("-service.overwolf.wtf"), QStringLiteral(".forgecdn.net"));
    QString v4 = url;
    out << v4.replace(QStringLiteral("://media."), QStringLiteral("://edge."));
    out << url;
    out.removeDuplicates();
    return out;
}

// 同步 POST（worker 线程内 QNAM + QEventLoop，超时 20s），成功返回响应体
static QByteArray postJson(const QUrl& url, const QByteArray& body,
                           const QHash<QByteArray, QByteArray>& headers = {})
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    req.setRawHeader("Content-Type", "application/json");
    for (auto it = headers.begin(); it != headers.end(); ++it)
        req.setRawHeader(it.key(), it.value());
    req.setTransferTimeout(20000);

    QNetworkReply* reply = nam.post(req, body);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, [&]() { timedOut = true; loop.quit(); });
    timeout.start(20000);
    loop.exec();

    // 用 timedOut 标志判断：旧逻辑 `!timeout.isActive()` 在正常完成时恒 false → 误判失败
    const bool ok = !timedOut && (reply->error() == QNetworkReply::NoError);
    const QByteArray data = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();
    return data;
}

// ═════════════════════════════════════════════════════════════════════════════
// 导出主流程
// ═════════════════════════════════════════════════════════════════════════════

void ModpackExporter::exportVersion(const QString& versionId, const QString& displayName,
                                    const QString& packVersion, const QVariantList& checkedOptions,
                                    const QVariantList& selectedSaves,
                                    bool modrinthUploadMode, bool hostedAssetsOnly,
                                    bool includeJava, int format,
                                    const QString& outPath, const QVariantList& extraFiles,
                                    const QVariantList& rulesOverride)
{
    if (m_busy) return;
    if (versionId.isEmpty() || outPath.isEmpty()) {
        qCInfo(logMod) << QStringLiteral("[整合包] 导出参数不完整 versionId=%1 outPath=%2").arg(versionId, outPath);
        emit finished(false, outPath, tr("参数不完整"));
        return;
    }
    m_busy = true;
    m_cancel.storeRelaxed(0);
    m_lookupContinue.storeRelaxed(1);   // 重置联网失败确认状态
    setProgress(0.0, tr("准备导出..."));
    emit busyChanged();
    qCInfo(logMod) << QStringLiteral("[整合包] 导出开始 %1 → %2").arg(displayName, outPath);

    const QString gameDir = m_gameDir;
    const QString cfKey = m_cfApiKey;
    const bool cfFormat = (format == 1);

    QtConcurrent::run([this, gameDir, cfKey, cfFormat,
                       versionId, displayName, packVersion,
                       checkedOptions, selectedSaves,
                       modrinthUploadMode, hostedAssetsOnly, includeJava, outPath, extraFiles,
                       rulesOverride]() {
        auto finish = [this, outPath](bool ok, const QString& err) {
            qCInfo(logMod) << QStringLiteral("[整合包] 导出结束 %1 %2 %3")
                                  .arg(ok ? QStringLiteral("成功") : QStringLiteral("失败"), outPath, err);
            const QString out = outPath;
            QMetaObject::invokeMethod(this, [this, ok, out, err]() {
                m_busy = false;
                emit busyChanged();
                emit finished(ok, out, err);
            });
        };

        // ── 1. 解析版本 JSON：MC 版本 + 加载器依赖 ──
        const QString versionDir = gameDir + QStringLiteral("/versions/") + versionId;
        const QString jsonPath = versionDir + QStringLiteral("/") + versionId + QStringLiteral(".json");
        if (!QFileInfo::exists(jsonPath)) {
            finish(false, tr("版本 %1 不存在或已损坏").arg(versionId));
            return;
        }
        QString mcVersion;
        QMap<QString, QString> deps;
        {
            QFile f(jsonPath);
            if (f.open(QIODevice::ReadOnly)) {
                const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
                // MC 版本：继承的原版优先（旧版 Forge JSON 有 inheritsFrom），
                // 否则从安装名去掉加载器后缀（26.2-forge-65.1.0 → 26.2）
                mcVersion = root.value(QStringLiteral("id")).toString();
                const QString inherited = root.value(QStringLiteral("inheritsFrom")).toString();
                if (!inherited.isEmpty()) {
                    mcVersion = inherited;
                } else {
                    for (const char* marker : {"-forge-", "-neoforge-", "-fabric-loader-", "-quilt-loader-"}) {
                        const int idx = mcVersion.indexOf(QLatin1String(marker));
                        if (idx > 0) { mcVersion = mcVersion.left(idx); break; }
                    }
                }
                const QJsonArray libs = root.value(QStringLiteral("libraries")).toArray();
                for (const auto& lv : libs) {
                    const QString name = lv.toObject().value(QStringLiteral("name")).toString();
                    // group:artifact:version[:classifier]——版本段是第 3 段
                    // （forge 库名形如 net.minecraftforge:forge:26.2-65.1.0:universal，
                    //  取最后一段会拿到 classifier 而非版本号）
                    const QStringList parts = name.split(QLatin1Char(':'));
                    if (parts.size() < 3) continue;
                    QString ver = parts.at(2);
                    // forge/neoforge 版本段可能是 "26.2-65.1.0"（mcVer-forgeVer 完整 ID）——
                    // mrpack/CF 依赖规范用纯 forge 版本（如 47.4.6），去掉 MC 前缀
                    if (name.startsWith(QStringLiteral("net.minecraftforge:forge:"))
                        || name.startsWith(QStringLiteral("net.neoforged:neoforge:"))) {
                        const int dash = ver.indexOf(QLatin1Char('-'));
                        if (dash > 0 && ver.left(dash) == mcVersion)
                            ver = ver.mid(dash + 1);
                    }
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

        // ── 2. 规则列表（同主流启动器 GetAllRules）──
        QSet<QString> checked;
        for (const auto& v : checkedOptions) checked.insert(v.toString());
        QStringList rules;
        if (!rulesOverride.isEmpty()) {
            // 配置读取的规则覆盖模式（主流启动器 RulesOverrides：手工规则整体生效，忽略选项勾选）
            for (const auto& v : rulesOverride) {
                QString r = v.toString();
                if (r.isEmpty()) continue;
                rules.append(r.replace(QLatin1Char('\\'), QLatin1Char('/')));
            }
        } else {
            const auto& defs = optionDefs();
            for (const auto& d : defs)
                if (checked.contains(d.id))
                    for (const auto& r : d.rules) rules.append(r);
            // 存档子项：勾选 saves 时用精确子项规则替换整目录规则
            if (checked.contains(QStringLiteral("saves"))) {
                rules.removeAll(QStringLiteral("saves/"));
                QStringList wantSaves;
                for (const auto& v : selectedSaves) wantSaves.append(v.toString());
                for (const auto& s : wantSaves)
                    rules.append(QStringLiteral("saves/") + s + QStringLiteral("/"));
            }
            // 资源包/光影子项（QML 传 id:name 文件 / id:dir:name 文件夹）：
            // 有勾选子项时用精确规则替换整目录规则（同主流启动器 ReloadSubOptions）
            auto applySubItemRules = [&](const QString& optId, const QStringList& dirRules) {
                QStringList subs;
                const QString prefix = optId + QLatin1Char(':');
                for (const auto& v : checkedOptions) {
                    const QString s = v.toString();
                    if (!s.startsWith(prefix)) continue;
                    QString rest = s.mid(prefix.size());
                    if (rest.startsWith(QStringLiteral("dir:")))
                        subs.append(optId + QStringLiteral("/") + rest.mid(4) + QStringLiteral("/*"));
                    else
                        subs.append(optId + QStringLiteral("/") + rest);
                }
                if (subs.isEmpty()) return;
                for (const auto& r : dirRules) rules.removeAll(r);
                for (const auto& s : subs) rules.append(s);
            };
            applySubItemRules(QStringLiteral("resourcepacks"),
                              {QStringLiteral("resourcepacks/"), QStringLiteral("texturepacks/")});
            applySubItemRules(QStringLiteral("shaderpacks"),
                              {QStringLiteral("shaderpacks/")});
        }
        // 全局排除（同主流启动器：日志/临时/启动器配置文件不进包）——覆盖模式同样生效
        rules << QStringLiteral("!*.log") << QStringLiteral("!*.dat_old")
              << QStringLiteral("!*.BakaCoreInfo") << QStringLiteral("!hmclversion.cfg")
              << QStringLiteral("!log4j2.xml");
        // mods jar 收集开关：普通模式看勾选；覆盖模式从规则反推（含 mods/ 正向目录规则即收集）
        bool includeMods = false, includeDisabled = false;
        if (rulesOverride.isEmpty()) {
            includeMods = checked.contains(QStringLiteral("mod"));
            includeDisabled = checked.contains(QStringLiteral("mod-disabled"));
        } else {
            for (const auto& r : rules) {
                const bool neg = r.startsWith(QLatin1Char('!'));
                const QString rr = neg ? r.mid(1) : r;
                if (rr == QStringLiteral("mods/")) { if (!neg) includeMods = true; else includeMods = false; }
                if (!neg && (rr.contains(QStringLiteral("*.disabled")) || rr.contains(QStringLiteral("*.old"))))
                    includeDisabled = true;
            }
        }
        // 版本内容根（隔离子目录 game/ > 版本内目录 > 共享根）
        const QString contentRoot = versionContentRoot(gameDir, versionId);

        // ── 3. 收集哈希对象（主流启动器 CheckHostedAssets 语义：mods/packs/resource 路径下的
        //     压缩包类文件尝试在线匹配）：mods/ 下 *.jar/*.zip/*.rar（.disabled/.old 仅当
        //     勾选“已禁用的 Mod”）+ resourcepacks/ 下 *.zip/*.rar
        struct ModFile {
            QString diskPath;
            QString relPath;          // 相对版本目录（mods/xxx.jar 或 resourcepacks/xxx.zip）
            QByteArray sha1;
            QByteArray sha512;
            quint32 cfHash = 0;
            qint64 size = 0;
            QStringList downloads;    // 在线下载 URL 列表
            int cfProjectId = 0;
            int cfFileId = 0;
            bool hosted = false;      // 找到至少一个在线来源
        };
        QList<ModFile> mods;
        auto collectMods = [&](const QString& baseDir, const QString& relPrefix) {
            const QDir d(baseDir);
            if (!d.exists()) return;
            QDirIterator it(d.absolutePath(),
                            QStringList() << QStringLiteral("*.jar") << QStringLiteral("*.zip")
                                          << QStringLiteral("*.rar") << QStringLiteral("*.disabled")
                                          << QStringLiteral("*.old"),
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString p = it.next();
                const QString lower = p.toLower();
                if (!includeDisabled && (lower.endsWith(QStringLiteral(".disabled"))
                                          || lower.endsWith(QStringLiteral(".old"))))
                    continue;
                if (p.contains(QStringLiteral("/.connector/"), Qt::CaseInsensitive)) continue;
                ModFile mf;
                mf.diskPath = p;
                mf.relPath = relPrefix + QStringLiteral("/") + d.relativeFilePath(p);
                mf.size = QFileInfo(p).size();
                mods.append(mf);
            }
        };
        if (includeMods)
            collectMods(contentRoot + QStringLiteral("/mods"), QStringLiteral("mods"));
        // resourcepacks 的 zip 哈希（主流启动器 packs/resource 语义）——遵守子项精确勾选：
        // 有子项勾选时只哈希勾选项，否则未勾选的 zip 会泄漏进 files[]/实体直装
        if (checked.contains(QStringLiteral("resourcepacks"))) {
            QStringList rpSubs;
            const QString rpPrefix = QStringLiteral("resourcepacks:");
            for (const auto& v : checkedOptions) {
                const QString s = v.toString();
                if (s.startsWith(rpPrefix)) rpSubs.append(s.mid(rpPrefix.size()));
            }
            if (rpSubs.isEmpty()) {
                collectMods(contentRoot + QStringLiteral("/resourcepacks"), QStringLiteral("resourcepacks"));
            } else {
                for (const auto& sub : rpSubs) {
                    const QString name = sub.startsWith(QStringLiteral("dir:")) ? sub.mid(4) : sub;
                    const QString base = contentRoot + QStringLiteral("/resourcepacks/") + name;
                    const QFileInfo fi(base);
                    if (fi.isFile()) {
                        ModFile mf;
                        mf.diskPath = base;
                        mf.relPath = QStringLiteral("resourcepacks/") + name;
                        mf.size = fi.size();
                        mods.append(mf);
                    } else if (fi.isDir()) {
                        collectMods(base, QStringLiteral("resourcepacks/") + name);
                    }
                }
            }
        }

        // ── 4. overrides 文件清单（规则驱动，同主流启动器 SearchFolder：
        //     遍历版本目录，Like 匹配规则；! 反选；跳过 assets/versions/libraries 与垃圾目录）──
        struct OvFile { QString diskPath; QString relPath; };
        QList<OvFile> ovFiles;
        // 主流启动器 黑名单：structureCacheV1/.fabric/.git/avatar-cache/cosmetic-cache；顶层跳过版本公共目录
        static const QStringList kSkipDirs = {
            QStringLiteral("structureCacheV1"), QStringLiteral(".fabric"), QStringLiteral(".git"),
            QStringLiteral("avatar-cache"), QStringLiteral("cosmetic-cache"),
            QStringLiteral("assets"), QStringLiteral("versions"), QStringLiteral("libraries")};
        auto shouldKeep = [&](const QString& rel) {
            bool keep = false;
            for (const auto& r : rules) {
                const bool neg = r.startsWith(QLatin1Char('!'));
                const QString pat = neg ? r.mid(1) : r;
                if (likeMatch(pat, rel)) keep = !neg;
            }
            return keep;
        };
        std::function<void(const QString&, const QString&)> scanDir;
        scanDir = [&](const QString& absDir, const QString& relPrefix) {
            QDir d(absDir);
            const auto subDirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto& sd : subDirs) {
                if (kSkipDirs.contains(sd)) continue;   // 跳过垃圾目录
                scanDir(d.filePath(sd), relPrefix.isEmpty() ? sd : relPrefix + QStringLiteral("/") + sd);
            }
            const auto files = d.entryList(QDir::Files, QDir::Name);
            for (const auto& fn : files) {
                const QString rel = relPrefix.isEmpty() ? fn : relPrefix + QStringLiteral("/") + fn;
                // mods/ 与 resourcepacks/ 下的压缩包类文件走哈希流程，避免与 overrides 重复打包
                const QString lower = rel.toLower();
                const bool isArchive = lower.endsWith(QStringLiteral(".jar"))
                    || lower.endsWith(QStringLiteral(".zip")) || lower.endsWith(QStringLiteral(".rar"))
                    || lower.endsWith(QStringLiteral(".disabled")) || lower.endsWith(QStringLiteral(".old"));
                if (isArchive && (rel.startsWith(QStringLiteral("mods/"))
                                  || rel.startsWith(QStringLiteral("resourcepacks/"))))
                    continue;
                if (!shouldKeep(rel)) continue;
                // 子项黑名单兜底（主流启动器 SubOptionBlackList：这些文件永不打包）
                bool black = false;
                for (const auto& b : kSubBlacklist)
                    if (rel.contains(b)) { black = true; break; }
                if (black) continue;
                ovFiles.append({d.filePath(fn), rel});
            }
        };
        scanDir(contentRoot, QString());

        // 追加内容（主流启动器 GetExtraFileLines：文件夹→包根/名，文件→包根）
        for (const auto& ef : extraFiles) {
            const QString p = ef.toString();
            if (p.isEmpty()) continue;
            const QFileInfo fi(p);
            if (fi.isDir()) {
                std::function<void(const QString&, const QString&)> addDirRec;
                addDirRec = [&](const QString& absDir, const QString& relPrefix) {
                    QDir d(absDir);
                    const auto subDirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                    for (const auto& sd : subDirs)
                        addDirRec(d.filePath(sd), relPrefix + QStringLiteral("/") + sd);
                    const auto files = d.entryList(QDir::Files, QDir::Name);
                    for (const auto& fn : files)
                        ovFiles.append({d.filePath(fn), relPrefix + QStringLiteral("/") + fn});
                };
                addDirRec(p, fi.fileName());
            } else if (fi.isFile()) {
                ovFiles.append({p, fi.fileName()});
            }
        }
        setProgress(0.06, tr("收集导出内容完成（%1 个覆写文件）").arg(ovFiles.size()));

        // ── 3b. IncludeJava：打包便携 Java 运行时（java_cache 中匹配 major 的 JRE，同主流启动器）──
        if (includeJava) {
            // 从版本 JSON 读需要的 Java major（缺省 8）
            int needMajor = 8;
            {
                QFile f(jsonPath);
                if (f.open(QIODevice::ReadOnly)) {
                    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
                    needMajor = root.value(QStringLiteral("javaVersion")).toObject()
                                    .value(QStringLiteral("majorVersion")).toInt(8);
                }
            }
            const QDir javaRoot(QCoreApplication::applicationDirPath() + QStringLiteral("/java_cache"));
            if (javaRoot.exists()) {
                QString javaDir;
                const auto majors = javaRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for (const auto& m : majors) {
                    if (m == QString::number(needMajor)) { javaDir = m; break; }
                }
                if (javaDir.isEmpty() && !majors.isEmpty())
                    javaDir = majors.first();   // 无精确匹配取第一个可用
                if (!javaDir.isEmpty()) {
                    // 复制 java_cache/{dir} → overrides/java/{dir}（局部递归，避免与扫描互扰）
                    std::function<void(const QString&, const QString&)> addJavaRec;
                    addJavaRec = [&](const QString& absDir, const QString& relPrefix) {
                        QDir d(absDir);
                        const auto subDirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                        for (const auto& sd : subDirs)
                            addJavaRec(d.filePath(sd), relPrefix + QStringLiteral("/") + sd);
                        const auto files = d.entryList(QDir::Files, QDir::Name);
                        for (const auto& fn : files)
                            ovFiles.append({d.filePath(fn), relPrefix + QStringLiteral("/") + fn});
                    };
                    addJavaRec(javaRoot.filePath(javaDir), QStringLiteral("java/") + javaDir);
                    qCInfo(logMod) << QStringLiteral("[导出] 打包 Java %1 → overrides/java/%1").arg(javaDir);
                }
            }
        }

        // ── 4. 计算模组双哈希（SHA1 + SHA512 + CF MurmurHash2）──
        const int modTotal = mods.size();
        for (int i = 0; i < modTotal; ++i) {
            if (m_cancel.loadRelaxed()) { finish(false, tr("已取消")); return; }
            auto& m = mods[i];
            QFile f(m.diskPath);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray data = f.readAll();
            f.close();
            m.sha1 = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
            m.sha512 = QCryptographicHash::hash(data, QCryptographicHash::Sha512);
            m.cfHash = cfMurmurHash2(data);
            setProgress(0.06 + 0.14 * (i + 1) / qMax(1, modTotal),
                        tr("计算模组哈希 %1/%2").arg(i + 1).arg(modTotal));
        }

        // ── 5. 双平台查询在线来源（同主流启动器；hostedAssetsOnly 时跳过全部联网）──
        int modrinthHits = 0, cfHits = 0;
        if (!mods.isEmpty() && !hostedAssetsOnly) {
            // 5a. Modrinth：批量 sha1 查询（分块 500/批，大整合包防 API 上限）
            setProgress(0.2, tr("查询 Modrinth 在线来源..."));
            bool modrinthFailed = false;
            constexpr int kMrBatch = 500;
            for (int b = 0; b < mods.size() && !modrinthFailed; b += kMrBatch) {
                QJsonArray shaArr;
                const int end = qMin(b + kMrBatch, mods.size());
                for (int i = b; i < end; ++i)
                    shaArr.append(QString::fromLatin1(mods[i].sha1.toHex()));
                QJsonObject bodyObj;
                bodyObj.insert(QStringLiteral("hashes"), shaArr);
                bodyObj.insert(QStringLiteral("algorithm"), QStringLiteral("sha1"));
                const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);
                const QByteArray resp = postJson(
                    QUrl(QStringLiteral("https://api.modrinth.com/v2/version_files")), body);
                if (resp.isEmpty()) { modrinthFailed = true; break; }
                const QJsonObject root = QJsonDocument::fromJson(resp).object();
                for (int i = b; i < end; ++i) {
                    auto& m = mods[i];
                    const QString shaHex = QString::fromLatin1(m.sha1.toHex());
                    const QJsonObject entry = root.value(shaHex).toObject();
                    if (entry.isEmpty()) continue;
                    const QJsonArray files = entry.value(QStringLiteral("files")).toArray();
                    if (files.isEmpty()) continue;
                    const QJsonObject f0 = files.first().toObject();
                    if (f0.value(QStringLiteral("hashes")).toObject().value(QStringLiteral("sha1")).toString() != shaHex)
                        continue;   // 主流启动器：校验返回 sha1 与请求一致
                    const QString url = f0.value(QStringLiteral("url")).toString();
                    if (!url.isEmpty()) {
                        m.downloads.append(url);
                        m.hosted = true;
                        modrinthHits++;
                    }
                }
            }
            if (modrinthFailed) {
                // 查询失败 → 主流启动器 弹窗询问是否继续（未查到文件将直接打包）
                qCWarning(logMod) << "[导出] Modrinth 查询失败";
                if (!waitLookupDecision(0, tr("Modrinth 在线来源查询失败，无法获取信息的文件将直接打包。是否继续？"))) {
                    finish(false, tr("已取消"));
                    return;
                }
            }
            setProgress(0.36, tr("Modrinth 查询完成（命中 %1）").arg(modrinthHits));

            // 5b. CurseForge：批量 fingerprint 查询（ModrinthUploadMode 跳过）
            if (!modrinthUploadMode) {
                if (cfKey.isEmpty()) {
                    // 主流启动器 内置 CF Key；我们未配置 → 提示后继续（同失败弹窗语义）
                    qCWarning(logMod) << "[导出] 未配置 CurseForge API Key";
                    if (!waitLookupDecision(1, tr("未配置 CurseForge API Key，CurseForge 在线来源不可用。是否继续？"))) {
                        finish(false, tr("已取消"));
                        return;
                    }
                } else {
                    // 分块 500 fingerprints/批（CF API 批量上限，大整合包防截断）
                    setProgress(0.38, tr("查询 CurseForge 在线来源..."));
                    bool cfFailed = false;
                    constexpr int kCfBatch = 500;
                    for (int b = 0; b < mods.size() && !cfFailed; b += kCfBatch) {
                        QJsonArray fpArr;
                        const int end = qMin(b + kCfBatch, mods.size());
                        for (int i = b; i < end; ++i)
                            fpArr.append(static_cast<double>(mods[i].cfHash));
                        QJsonObject cfBodyObj;
                        cfBodyObj.insert(QStringLiteral("fingerprints"), fpArr);
                        const QByteArray cfBody = QJsonDocument(cfBodyObj).toJson(QJsonDocument::Compact);
                        const QByteArray cfResp = postJson(
                            QUrl(QStringLiteral("https://api.curseforge.com/v1/fingerprints/432/")), cfBody,
                            {{"x-api-key", cfKey.toUtf8()}});
                        if (cfResp.isEmpty()) { cfFailed = true; break; }
                        const QJsonObject data = QJsonDocument::fromJson(cfResp).object()
                                                     .value(QStringLiteral("data")).toObject();
                        const QJsonArray matches = data.value(QStringLiteral("exactMatches")).toArray();
                        for (const auto& mv : matches) {
                            const QJsonObject match = mv.toObject();
                            const quint32 fp = static_cast<quint32>(
                                match.value(QStringLiteral("fileFingerprint")).toDouble());
                            const QJsonObject file = match.value(QStringLiteral("file")).toObject();
                            const QString dlUrl = file.value(QStringLiteral("downloadUrl")).toString();
                            if (dlUrl.isEmpty()) continue;
                            for (int i = b; i < end; ++i) {
                                auto& m = mods[i];
                                if (m.cfHash != fp) continue;
                                m.cfProjectId = match.value(QStringLiteral("projectId")).toInt();
                                m.cfFileId = match.value(QStringLiteral("id")).toInt();
                                // 域名变体展开（同主流启动器）
                                const auto urls = expandCfDownloadUrls(dlUrl);
                                for (const auto& u : urls)
                                    if (!m.downloads.contains(u))
                                        m.downloads.append(u);
                                m.hosted = true;
                                cfHits++;
                                break;
                            }
                        }
                    }
                    if (cfFailed) {
                        qCWarning(logMod) << "[导出] CurseForge 查询失败";
                        if (!waitLookupDecision(1, tr("CurseForge 在线来源查询失败，无法获取信息的文件将直接打包。是否继续？"))) {
                            finish(false, tr("已取消"));
                            return;
                        }
                    }
                }
                setProgress(0.52, tr("CurseForge 查询完成（命中 %1）").arg(cfHits));
            }
        } else if (!mods.isEmpty()) {
            setProgress(0.5, tr("仅打包包内资源，跳过联网查询"));
        }

        if (m_cancel.loadRelaxed()) { finish(false, tr("已取消")); return; }

        // ── 6. 构建清单 + 打包 ──
        // hosted（有在线来源）→ files[]/manifest 引用，不打包实体；
        // 非 hosted → overrides/mods/ 实体直装（同主流启动器：ModFile.File.Delete() 语义）
        const int hostedCount = mods.size() - [&]() {
            int n = 0;
            for (const auto& m : mods) if (!m.hosted) n++;
            return n;
        }();

        // 实体文件总数（打包进度用）：非 hosted mods + overrides
        qint64 totalBytes = 0;
        int totalFiles = 1;   // 清单文件
        for (const auto& m : mods) if (!m.hosted) { totalFiles++; totalBytes += m.size; }
        for (const auto& o : ovFiles) { totalFiles++; totalBytes += QFileInfo(o.diskPath).size(); }

        ZipArchive zip;
        if (!zip.openForWrite(outPath)) {
            finish(false, zip.error());
            return;
        }
        int done = 0;

        // 6a. 清单（Modrinth index.json / CurseForge manifest.json）
        // summary：版本描述（主流启动器 McVersion.Info 同款：MC 版本 + 加载器）
        QString summary = QStringLiteral("Minecraft ") + mcVersion;
        for (auto it = deps.begin(); it != deps.end(); ++it)
            summary += QStringLiteral(" + %1 %2").arg(it.key(), it.value());
        if (cfFormat) {
            QJsonObject manifest;
            manifest.insert(QStringLiteral("manifestType"), QStringLiteral("minecraftModpack"));
            manifest.insert(QStringLiteral("manifestVersion"), 1);
            manifest.insert(QStringLiteral("name"), displayName);
            manifest.insert(QStringLiteral("version"), packVersion.isEmpty() ? QStringLiteral("1.0.0") : packVersion);
            manifest.insert(QStringLiteral("author"), QString());
            QJsonArray filesArr;
            for (const auto& m : mods) {
                if (!m.hosted || m.cfFileId <= 0) continue;
                QJsonObject f;
                f.insert(QStringLiteral("projectID"), m.cfProjectId);
                f.insert(QStringLiteral("fileID"), m.cfFileId);
                f.insert(QStringLiteral("required"), true);
                filesArr.append(f);
            }
            manifest.insert(QStringLiteral("files"), filesArr);
            manifest.insert(QStringLiteral("overrides"), QStringLiteral("overrides"));
            QJsonObject mc;
            mc.insert(QStringLiteral("version"), mcVersion);
            QJsonArray loaders;
            for (auto it = deps.begin(); it != deps.end(); ++it) {
                QJsonObject l;
                l.insert(QStringLiteral("id"), it.key() + QStringLiteral("-") + it.value());
                l.insert(QStringLiteral("primary"), loaders.isEmpty());
                loaders.append(l);
            }
            mc.insert(QStringLiteral("modLoaders"), loaders);
            manifest.insert(QStringLiteral("minecraft"), mc);
            if (!zip.addData(QStringLiteral("manifest.json"),
                             QJsonDocument(manifest).toJson(QJsonDocument::Indented))) {
                zip.closeWrite();
                QFile::remove(outPath);
                finish(false, zip.error());
                return;
            }
        } else {
            QJsonObject index;
            index.insert(QStringLiteral("formatVersion"), 1);
            // mrpack 规范 + 主流启动器：game 固定 "minecraft"（非版本号）、versionId=打包版本号
            index.insert(QStringLiteral("game"), QStringLiteral("minecraft"));
            index.insert(QStringLiteral("versionId"),
                         packVersion.isEmpty() ? displayName : packVersion);
            index.insert(QStringLiteral("name"), displayName);
            index.insert(QStringLiteral("summary"), summary);
            QJsonArray filesArr;
            for (const auto& m : mods) {
                if (!m.hosted) continue;
                QJsonObject f;
                f.insert(QStringLiteral("path"), m.relPath);   // 已含前缀（mods/xxx.jar 或 resourcepacks/xxx.zip）
                QJsonObject hashes;
                hashes.insert(QStringLiteral("sha1"), QString::fromLatin1(m.sha1.toHex()));
                hashes.insert(QStringLiteral("sha512"), QString::fromLatin1(m.sha512.toHex()));
                f.insert(QStringLiteral("hashes"), hashes);
                // downloads：URL 列表，非 Modrinth 优先排序（同主流启动器 OrderBy：
                // “不优先选择 Modrinth”，Modrinth 链接排后，避免默认从 Modrinth 拉取）
                QJsonArray dlArr;
                QStringList sorted = m.downloads;
                std::stable_sort(sorted.begin(), sorted.end(),
                                 [](const QString& a, const QString& b) {
                                     return !a.contains(QStringLiteral("modrinth.com"))
                                         && b.contains(QStringLiteral("modrinth.com"));
                                 });
                for (const auto& u : sorted) dlArr.append(u);
                f.insert(QStringLiteral("downloads"), dlArr);
                f.insert(QStringLiteral("fileSize"), m.size);
                filesArr.append(f);
            }
            index.insert(QStringLiteral("files"), filesArr);
            QJsonObject depsObj;
            depsObj.insert(QStringLiteral("minecraft"), mcVersion);
            for (auto it = deps.begin(); it != deps.end(); ++it)
                depsObj.insert(it.key(), it.value());
            index.insert(QStringLiteral("dependencies"), depsObj);
            if (!zip.addData(QStringLiteral("modrinth.index.json"),
                             QJsonDocument(index).toJson(QJsonDocument::Indented))) {
                zip.closeWrite();
                QFile::remove(outPath);
                finish(false, zip.error());
                return;
            }
        }
        ++done;
        setProgress(0.55, tr("生成压缩包..."));

        // 打包进度节流（100ms）：几千文件的包逐文件 setProgress 会信号风暴卡主线程
        QElapsedTimer packTick;
        packTick.start();
        auto throttledPackProgress = [&](const QString& text) {
            if (packTick.elapsed() < 100) return;
            packTick.restart();
            setProgress(0.55 + 0.45 * done / qMax(1, totalFiles), text);
        };

        // 6b. 非 hosted mods → overrides/mods/ 实体
        for (const auto& m : mods) {
            if (m.hosted) continue;
            if (m_cancel.loadRelaxed()) { zip.closeWrite(); QFile::remove(outPath); finish(false, tr("已取消")); return; }
            if (!zip.addFile(m.diskPath, QStringLiteral("overrides/") + m.relPath)) {
                zip.closeWrite();
                QFile::remove(outPath);
                finish(false, zip.error());
                return;
            }
            ++done;
            throttledPackProgress(tr("打包模组 %1/%2").arg(done).arg(totalFiles));
        }

        // 6c. overrides 文件
        for (const auto& o : ovFiles) {
            if (m_cancel.loadRelaxed()) { zip.closeWrite(); QFile::remove(outPath); finish(false, tr("已取消")); return; }
            if (!zip.addFile(o.diskPath, QStringLiteral("overrides/") + o.relPath)) {
                zip.closeWrite();
                QFile::remove(outPath);
                finish(false, zip.error());
                return;
            }
            ++done;
            throttledPackProgress(tr("打包覆写文件 %1/%2").arg(done).arg(totalFiles));
        }

        if (!zip.closeWrite()) {
            QFile::remove(outPath);
            finish(false, zip.error());
            return;
        }

        const int localCount = mods.size() - hostedCount;
        const QString note = modrinthUploadMode
            ? tr("（%1 个模组未在 Modrinth 托管，已直接打包）").arg(localCount)
            : tr("（%1 个模组已联网托管，%2 个直接打包）").arg(hostedCount).arg(localCount);
        setProgress(1.0, tr("导出完成 %1").arg(note));
        qCInfo(logMod) << QStringLiteral("[整合包] 导出完成 %1 → %2 (mods=%3 hosted=%4 local=%5)")
            .arg(displayName, outPath).arg(mods.size()).arg(hostedCount).arg(localCount);
        finish(true, note);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// 导出配置保存/读取（主流启动器 export_config.txt 语义：ini 段 + 规则段 + 追加内容段）
// 配置文件为 UTF-8 文本，规则段支持手工编辑（读取后 options 为空 → 用规则段整体覆盖）
// ═════════════════════════════════════════════════════════════════════════════

static const QString kCfgSep = QStringLiteral("==============================================================");

bool ModpackExporter::saveExportConfig(const QString& path, const QVariantMap& cfg) const
{
    QStringList lines;
    lines << QStringLiteral("Name:") + cfg.value(QStringLiteral("name")).toString()
          << QStringLiteral("Version:") + cfg.value(QStringLiteral("version")).toString()
          << QStringLiteral("IncludeJava:") + QString(cfg.value(QStringLiteral("includeJava")).toBool() ? "True" : "False")
          << QStringLiteral("DontCheckHostedAssets:") + QString(cfg.value(QStringLiteral("hostedAssetsOnly")).toBool() ? "True" : "False")
          << QStringLiteral("ModrinthUploadMode:") + QString(cfg.value(QStringLiteral("modrinthUploadMode")).toBool() ? "True" : "False")
          << QStringLiteral("Format:") + QString::number(cfg.value(QStringLiteral("format")).toInt())
          << QStringLiteral("PackPath:") + cfg.value(QStringLiteral("packPath")).toString()
          << QStringLiteral("UncheckedOptions:") + [&]() {
                 QStringList un;
                 const auto unchecked = cfg.value(QStringLiteral("unchecked")).toList();
                 for (const auto& u : unchecked) un.append(u.toString());
                 return un.join(QLatin1Char(','));
             }()
          << QString()
          << kCfgSep
          << QStringLiteral("# 导出的规则（勾选的选项）——可按 主流启动器语法手工编辑：! 反转、* ? [] 通配、\\ 结尾=目录")
          << QStringLiteral("# 读取时若下方为空则按此规则整体生效（忽略界面勾选）");
    const auto checked = cfg.value(QStringLiteral("options")).toList();
    for (const auto& c : checked) {
        const QString id = c.toString();
        for (const auto& d : optionDefs()) {
            if (d.id != id) continue;
            lines << QStringLiteral("# ") + d.title;
            for (const auto& r : d.rules)
                lines << QString(r).replace(QLatin1Char('/'), QLatin1Char('\\'));
            lines << QString();
            break;
        }
    }
    lines << QStringLiteral("# 全局排除")
          << QStringLiteral("!*.log") << QStringLiteral("!*.dat_old")
          << QStringLiteral("!*.BakaCoreInfo") << QStringLiteral("!hmclversion.cfg")
          << QStringLiteral("!log4j2.xml")
          << QString()
          << kCfgSep
          << QStringLiteral("# 追加内容：完整绝对路径，每行一个；以 \\ 结尾=文件夹（复制到包根同名目录）");
    const auto extras = cfg.value(QStringLiteral("extraFiles")).toList();
    for (const auto& e : extras)
        lines << e.toString();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    f.write(lines.join(QLatin1Char('\n')).toUtf8());
    f.close();
    return true;
}

QVariantMap ModpackExporter::loadExportConfig(const QString& path) const
{
    QVariantMap cfg;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return cfg;
    const QStringList raw = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    f.close();

    QStringList segments;
    QString cur;
    for (const auto& line : raw) {
        const QString t = line.trimmed();
        if (t == kCfgSep) { segments.append(cur); cur.clear(); continue; }
        cur += line + QLatin1Char('\n');
    }
    segments.append(cur);

    // ini 段
    if (segments.size() > 0) {
        for (const auto& l : segments[0].split(QLatin1Char('\n'))) {
            const QString t = l.trimmed();
            if (t.isEmpty() || t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char('='))) continue;
            const int idx = t.indexOf(QLatin1Char(':'));
            if (idx <= 0) continue;
            const QString key = t.left(idx);
            const QString val = t.mid(idx + 1).trimmed();
            if (key == QLatin1String("Name")) cfg.insert(QStringLiteral("name"), val);
            else if (key == QLatin1String("Version")) cfg.insert(QStringLiteral("version"), val);
            else if (key == QLatin1String("IncludeJava")) cfg.insert(QStringLiteral("includeJava"), val.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0);
            else if (key == QLatin1String("DontCheckHostedAssets")) cfg.insert(QStringLiteral("hostedAssetsOnly"), val.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0);
            else if (key == QLatin1String("ModrinthUploadMode")) cfg.insert(QStringLiteral("modrinthUploadMode"), val.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0);
            else if (key == QLatin1String("Format")) cfg.insert(QStringLiteral("format"), val.toInt());
            else if (key == QLatin1String("PackPath")) cfg.insert(QStringLiteral("packPath"), val);
            else if (key == QLatin1String("UncheckedOptions")) {
                QVariantList un;
                const auto parts = val.split(QLatin1Char(','), Qt::SkipEmptyParts);
                for (const auto& p : parts) un.append(p.trimmed());
                cfg.insert(QStringLiteral("unchecked"), un);
            }
        }
    }

    // 规则段 → 尝试反推勾选选项；rawRules 原样保留（QML 侧检测到自定义规则时走覆盖模式）
    QStringList ruleLines;
    if (segments.size() > 1) {
        for (const auto& l : segments[1].split(QLatin1Char('\n'))) {
            const QString t = l.trimmed();
            if (t.isEmpty() || t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char('='))) continue;
            ruleLines.append(t);
        }
    }
    QVariantList checked;
    const auto& defs = optionDefs();
    for (const auto& d : defs) {
        bool has = false, hasNeg = false;
        for (const auto& r : d.rules) {
            const QString rr = QString(r).replace(QLatin1Char('/'), QLatin1Char('\\'));
            if (ruleLines.contains(rr)) has = true;
            if (ruleLines.contains(QLatin1Char('!') + rr)) hasNeg = true;
        }
        if (has && !hasNeg) checked.append(d.id);
    }
    cfg.insert(QStringLiteral("options"), checked);
    // 规则覆盖模式：反推的选项无法覆盖所有规则行（用户手工编辑过）→ 整体作为覆盖规则
    QVariantList rawRules;
    bool allCovered = !ruleLines.isEmpty();
    for (const auto& rl : ruleLines) {
        bool covered = rl.startsWith(QLatin1Char('!'));   // 全局排除行视为已覆盖
        if (!covered) {
            const QString rr = QString(rl).replace(QLatin1Char('\\'), QLatin1Char('/'));
            for (const auto& d : defs) {
                if (d.rules.contains(rr)) { covered = true; break; }
            }
        }
        if (!covered) allCovered = false;
        rawRules.append(rl);
    }
    if (!allCovered)
        cfg.insert(QStringLiteral("rawRules"), rawRules);
    // 追加内容段
    QVariantList extras;
    if (segments.size() > 2) {
        for (const auto& l : segments[2].split(QLatin1Char('\n'))) {
            const QString t = l.trimmed();
            if (t.isEmpty() || t.startsWith(QLatin1Char('#')) || t.startsWith(QLatin1Char('='))) continue;
            extras.append(t);
        }
    }
    cfg.insert(QStringLiteral("extraFiles"), extras);
    return cfg;
}

} // namespace ShadowLauncher
