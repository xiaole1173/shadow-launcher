#pragma once
#include <QString>
#include <QByteArray>

// ══════════════════════════════════════════════════════════════════
// engine_identity.h — 下载引擎身份注册表（命名 + 用途标注）
//
// 命名体系：上古神话（2026-08-01 定案）
//   VersionDownloader   → 盘古    版本安装管线（开天辟地，从零组装世界）
//   FileDownloader      → 夸父    通用批量文件下载（夸父逐日，极速追击）
//   AssetDownloader     → 山海经  游戏资源专项下载（万物图鉴，收录万宝）
//   ModDownloadEngine   → 精卫    批量模组小文件下载（衔微石填海，持恒投递）
//   ModpackDownloader   → 女娲    整合包编排（抟土造人，捏合成完整世界）
//   HttpClient          → 驿道    HTTP 传输底座（天下驿道，信息干线）
//
// 日志规范（防止"名字好看但看不出用途"的花架子）：
//   1. 所有日志前缀统一为 [<雅名>]，一眼可辨是哪个引擎在干活（2026-08-02 精简：去掉冗长的 [引擎·] 包装）
//   2. 每个引擎启动时打印一次"身份卡"（engineBanner）：名称 + 用途说明
// ══════════════════════════════════════════════════════════════════

struct EngineIdentity {
    const char* id;      // 拼音代号（ASCII，代码内使用 / 日志 grep）
    const char* name;    // 中文雅名（日志/UI 显示）
    const char* purpose; // 用途说明（身份卡日志用）
};

inline const EngineIdentity kEngineIdentities[] = {
    { "pangu",   "盘古",   "Minecraft 版本安装管线：编排版本 JSON、库文件与 assets 下载，从零组装出可启动版本" },
    { "kuafu",   "夸父",   "通用批量文件下载引擎：并行分块加速、连接池、主机健康、限速、缓存命中与 SHA1 校验" },
    { "shanhai", "山海经", "游戏资源（assets）专项下载：异步 SHA1 预检、异步 DNS、多镜像降级，资源宝库" },
    { "jingwei", "精卫",   "批量模组小文件下载：多源自降级、SHA1/大小校验、EMA 网速统计，海量微石批量投递" },
    { "nuwa",    "女娲",   "整合包编排：manifest 解析、CF/Modrinth API、文件清单与落盘路径、覆盖备份钩子" },
    { "yidao",   "驿道",   "HTTP 传输底座：全引擎共用的唯一网络通道（Qt6::Network）" },
    { "sinan",   "司南",   "资源拉取引擎：统一调度搜索 API 与图标拉取，三层缓存 + 本地缩略图 + 严格并发控制，适配 Modrinth/CurseForge/整合包多源" },
};

inline EngineIdentity engineIdentity(const char* id)
{
    for (const auto& e : kEngineIdentities) {
        if (qstrcmp(e.id, id) == 0)
            return e;
    }
    return { id, id, "未知引擎" };
}

/// 日志前缀，如 "[盘古] "（2026-08-02 精简：原为 [盘古]）
inline QString engineTag(const char* id)
{
    return QStringLiteral("[%1] ").arg(QString::fromUtf8(engineIdentity(id).name));
}

/// 身份卡：引擎启动时打印，标注名称与用途
inline QString engineBanner(const char* id)
{
    const auto e = engineIdentity(id);
    return QStringLiteral("[%1] 身份｜用途：%2")
        .arg(QString::fromUtf8(e.name), QString::fromUtf8(e.purpose));
}
