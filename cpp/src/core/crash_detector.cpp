// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "crash_detector.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QDateTime>
#include <private/qzipwriter_p.h>

#include "../utils/logger.h"

namespace ShadowLauncher {

// ============================================================
// CrashReport → QVariantMap
// ============================================================

QVariantMap CrashReport::toVariantMap() const
{
    QVariantMap report;
    report[QStringLiteral("type")] = type;
    report[QStringLiteral("reason")] = reason;
    report[QStringLiteral("description")] = description;
    report[QStringLiteral("suspectedMods")] = suspectedMods;
    report[QStringLiteral("filePath")] = filePath;
    report[QStringLiteral("timestamp")] = timestamp;
    report[QStringLiteral("isValid")] = isValid;
    report[QStringLiteral("suggestions")] = suggestions;
    report[QStringLiteral("matchedRules")] = matchedRules;
    report[QStringLiteral("collectedLogs")] = collectedLogs;
    report[QStringLiteral("jvmOutput")] = jvmOutput;
    report[QStringLiteral("reportFilePath")] = reportFilePath;
    report[QStringLiteral("exportDir")] = exportDir;
    report[QStringLiteral("reportTooLong")] = reportTooLong;
    return report;
}

// ============================================================
// Constructor
// ============================================================

CrashDetector::CrashDetector(QObject* parent)
    : QObject(parent)
{
}

// ============================================================
// Rule engine (ported from 主流启动器 CrashReportAnalyzer)
// ============================================================

QList<CrashRule> CrashDetector::rules()
{
    // NOTE: regexes are QRegularExpression (PCRE) syntax, compatible with
    // the Java patterns used in 主流启动器's CrashReportAnalyzer.
    static const QList<CrashRule> kRules = {
        { QStringLiteral("OPENJ9"),
          QStringLiteral(R"((Open J9 is not supported|OpenJ9 is incompatible|\.J9VMInternals\.))"),
          {},
          tr("检测到您正在使用 OpenJ9 虚拟机。Minecraft 与 OpenJ9 不兼容，请在启动设置中更换为 Oracle/微软 OpenJDK 或 Temurin 等 HotSpot 虚拟机。"),
          tr("OpenJ9 虚拟机不兼容") },

        { QStringLiteral("NEED_JDK11"),
          QStringLiteral(R"((no such method: sun\.misc\.Unsafe\.defineAnonymousClass|UnsupportedClassVersionError: icyllis|The requested compatibility level JAVA_11 could not be set))"),
          {},
          tr("此游戏需要 Java 11 或更高版本。请在 Java 设置中选择已安装的 Java 11+ 运行时。"),
          tr("需要 Java 11") },

        { QStringLiteral("TOO_OLD_JAVA"),
          QStringLiteral(R"(java\.lang\.UnsupportedClassVersionError: .*? version (\d+)\.0)"),
          { QStringLiteral("expected") },
          tr("Java 版本过低：游戏/模组需要更高版本的 Java。请安装与游戏版本匹配的 Java（1.17+ 需要 Java 17/21）。"),
          tr("Java 版本过低") },

        { QStringLiteral("JVM_32BIT"),
          QStringLiteral(R"((Could not reserve enough space for .*?KB object heap|The specified size exceeds the maximum representable size|Invalid maximum heap size))"),
          {},
          tr("您使用的是 32 位 Java 或分配的内存过大。请安装 64 位 Java，或将分配内存调低（建议 4GB 以下）。"),
          tr("32 位 Java / 堆内存过大") },

        { QStringLiteral("GL_OPERATION_FAILURE"),
          QStringLiteral(R"((1282: Invalid operation|Maybe try a lower resolution resourcepack\?))"),
          {},
          tr("显卡 OpenGL 操作失败，可能是模组/光影包渲染问题。尝试关闭光影、更换资源包分辨率，或更新显卡驱动。"),
          tr("OpenGL 操作失败") },

        { QStringLiteral("OPENGL_NOT_SUPPORTED"),
          QStringLiteral(R"(The driver does not appear to support OpenGL)"),
          {},
          tr("显卡驱动不支持 OpenGL。请更新显卡驱动；虚拟机/远程桌面环境请启用 3D 加速。"),
          tr("不支持 OpenGL") },

        { QStringLiteral("GRAPHICS_DRIVER"),
          QStringLiteral(R"((Pixel format not accelerated|GLX: Failed to create context: GLXBadFBConfig|Couldn't set pixel format|org\.lwjgl\.LWJGLException|EXCEPTION_ACCESS_VIOLATION(.|\n|\r)+# C {2}\[(ig|atio|nvoglv)))"),
          {},
          tr("显卡驱动异常（显示驱动崩溃）。请更新显卡驱动；若使用核显/独显切换，请为 Java 指定高性能显卡。"),
          tr("显卡驱动异常") },

        { QStringLiteral("OUT_OF_MEMORY"),
          QStringLiteral(R"((java\.lang\.OutOfMemoryError|The system is out of physical RAM or swap space|Out of Memory Error|Error occurred during initialization of VM(?:\r?\n)Too small maximum heap))"),
          {},
          tr("内存不足！请在启动设置中增大内存分配，或关闭其他占用内存的程序后重试。"),
          tr("内存不足") },

        { QStringLiteral("MEMORY_EXCEEDED"),
          QStringLiteral(R"(There is insufficient memory for the Java Runtime Environment to continue)"),
          {},
          tr("系统物理内存不足（JVM 无法继续运行）。请关闭其他程序，或在启动设置中调低分配内存。"),
          tr("系统内存不足") },

        { QStringLiteral("RESOLUTION_TOO_HIGH"),
          QStringLiteral(R"(Maybe try a (lower resolution|lowerresolution) (resourcepack|texturepack)\?)"),
          {},
          tr("资源包分辨率过高。请尝试使用更低分辨率的资源包/光影包。"),
          tr("资源包分辨率过高") },

        { QStringLiteral("JDK_9"),
          QStringLiteral(R"(java\.lang\.ClassCastException: (java\.base/jdk|class jdk))"),
          {},
          tr("游戏（或 Forge 旧版本）不兼容高版本 Java。1.12.2 及以下版本建议使用 Java 8。"),
          tr("Java 版本过高") },

        { QStringLiteral("FILE_CHANGED"),
          QStringLiteral(R"(java\.lang\.SecurityException: SHA1 digest error for (.*?)|signer information does not match)"),
          { QStringLiteral("file") },
          tr("游戏核心文件被修改或校验失败。请使用版本管理中的「校验/修复」功能修复游戏文件。"),
          tr("游戏文件被修改") },

        { QStringLiteral("NO_SUCH_METHOD_ERROR"),
          QStringLiteral(R"(java\.lang\.NoSuchMethodError: (.*?))"),
          { QStringLiteral("class") },
          tr("模组/核心注入失败（缺少方法），通常是模组与游戏版本或加载器不兼容。请检查崩溃堆栈中的模组并更新/移除它。"),
          tr("模组注入失败 (NoSuchMethodError)") },

        { QStringLiteral("NO_CLASS_DEF_FOUND_ERROR"),
          QStringLiteral(R"(java\.lang\.NoClassDefFoundError: (.*?))"),
          { QStringLiteral("class") },
          tr("缺少类定义，通常因模组缺失/版本不匹配或安装不完整。请重装对应模组或使用「修复」功能。"),
          tr("缺少类定义 (NoClassDefFoundError)") },

        { QStringLiteral("ILLEGAL_ACCESS_ERROR"),
          QStringLiteral(R"(java\.lang\.IllegalAccessError: tried to access class (.*?) from class (.*?))"),
          { QStringLiteral("class") },
          tr("模组非法访问内部类，多为模组与游戏版本不兼容。请更新或移除相关模组。"),
          tr("非法访问错误") },

        { QStringLiteral("DUPLICATED_MOD"),
          QStringLiteral(R"(Found a duplicate mod (.*?) at (.*?))"),
          { QStringLiteral("name"), QStringLiteral("path") },
          tr("检测到重复模组！请在 mods 文件夹中删除重复的模组文件（同一模组出现两份）。"),
          tr("重复模组") },

        { QStringLiteral("MOD_RESOLUTION"),
          QStringLiteral(R"(ModResolutionException: (.*?))"),
          { QStringLiteral("reason") },
          tr("模组依赖解析失败（Fabric）。请检查缺失或冲突的模组依赖，或更新相关模组。"),
          tr("模组依赖解析失败") },

        { QStringLiteral("FORGEMOD_RESOLUTION"),
          QStringLiteral(R"(Missing or unsupported mandatory dependencies:(.*?))"),
          { QStringLiteral("reason") },
          tr("Forge 模组缺少必需依赖。请安装报告中列出的前置模组。"),
          tr("Forge 缺少依赖") },

        { QStringLiteral("MOD_RESOLUTION_CONFLICT"),
          QStringLiteral(R"(ModResolutionException: Found conflicting mods: (.*?) conflicts with (.*?))"),
          { QStringLiteral("sourcemod"), QStringLiteral("destmod") },
          tr("模组冲突！请移除冲突的模组之一（见报告中的模组名称）。"),
          tr("模组冲突") },

        { QStringLiteral("MOD_RESOLUTION_MISSING"),
          QStringLiteral(R"(ModResolutionException: Could not find required mod: (.*?) requires (.*?))"),
          { QStringLiteral("sourcemod"), QStringLiteral("destmod") },
          tr("缺少必需模组：请安装报告中所列的依赖模组。"),
          tr("缺少必需模组") },

        { QStringLiteral("MOD_RESOLUTION_MISSING_MINECRAFT"),
          QStringLiteral(R"(ModResolutionException: Could not find required mod: (.*?) requires \{minecraft @ (.*?)\})"),
          { QStringLiteral("mod"), QStringLiteral("version") },
          tr("模组要求的 Minecraft 版本与当前版本不匹配。请安装模组支持的 Minecraft 版本。"),
          tr("模组与 MC 版本不匹配") },

        { QStringLiteral("LOADING_CRASHED_FORGE"),
          QStringLiteral(R"(LoaderExceptionModCrash: Caught exception from (.*?) \((.*?)\))"),
          { QStringLiteral("name"), QStringLiteral("id") },
          tr("Forge 加载模组时崩溃。请移除报告中提到的模组，或更新它到兼容版本。"),
          tr("Forge 加载模组崩溃") },

        { QStringLiteral("BOOTSTRAP_FAILED"),
          QStringLiteral(R"(Failed to create mod instance\. ModID: (.*?),)"),
          { QStringLiteral("id") },
          tr("模组实例创建失败。请移除或更新对应模组。"),
          tr("模组实例创建失败") },

        { QStringLiteral("LOADING_CRASHED_FABRIC"),
          QStringLiteral(R"(Could not execute entrypoint stage '(.*?)' due to errors, provided by '(.*?)'!)"),
          { QStringLiteral("id") },
          tr("Fabric 加载模组入口失败。请移除报告中提到的模组，或更新它到兼容版本。"),
          tr("Fabric 加载模组崩溃") },

        { QStringLiteral("FABRIC_VERSION_0_12"),
          QStringLiteral(R"(java\.lang\.NoClassDefFoundError: org/spongepowered/asm/mixin/transformer/FabricMixinTransformerProxy)"),
          {},
          tr("Fabric Loader 版本过旧。请更新 Fabric Loader 到最新版本。"),
          tr("Fabric Loader 过旧") },

        { QStringLiteral("MODLAUNCHER_8"),
          QStringLiteral(R"(java\.lang\.NoSuchMethodError: ('void sun\.security\.util\.ManifestEntryVerifier|sun\.security\.util\.ManifestEntryVerifier\.<init>))"),
          {},
          tr("Forge 与 Java 版本不兼容（已知 JDK 问题）。1.16+Forge 请使用 Java 8/17 的对应版本，或更换 Java 发行版。"),
          tr("Forge 与 Java 不兼容") },

        { QStringLiteral("CONFIG"),
          QStringLiteral(R"(Failed loading config file (.*?) of type (.*?) for modid (.*?))"),
          { QStringLiteral("file"), QStringLiteral("id") },
          tr("模组配置文件加载失败。请删除对应模组的配置文件（config 目录）后重试。"),
          tr("配置文件加载失败") },

        { QStringLiteral("FABRIC_WARNINGS"),
          QStringLiteral(R"((Warnings were found!|Incompatible mod set!|Incompatible mods found!)(.*?)[\n\r]+(.*?)\[)"),
          { QStringLiteral("reason") },
          tr("Fabric 检测到不兼容模组组合。请查看日志中的警告详情并移除冲突模组。"),
          tr("Fabric 模组不兼容警告") },

        { QStringLiteral("ENTITY"),
          QStringLiteral(R"(Entity Type: (.*?)[\w\W\n\r]*?Entity's Exact location: (.*?))"),
          { QStringLiteral("type"), QStringLiteral("location") },
          tr("游戏在加载实体时崩溃。可能是实体相关模组的问题，请更新/移除相关模组（如实体渲染类模组）。"),
          tr("实体加载崩溃") },

        { QStringLiteral("BLOCK"),
          QStringLiteral(R"(Block: (.*?)[\w\W\n\r]*?Block location: (.*?))"),
          { QStringLiteral("type"), QStringLiteral("location") },
          tr("游戏在加载方块模型时崩溃。可能是方块/世界生成相关模组的问题，请更新/移除相关模组。"),
          tr("方块加载崩溃") },

        { QStringLiteral("UNSATISFIED_LINK_ERROR"),
          QStringLiteral(R"(java\.lang\.UnsatisfiedLinkError: Failed to locate library: (.*?))"),
          { QStringLiteral("name") },
          tr("找不到本地库文件，通常是模组安装不完整或缺少依赖。请重新安装相关模组。"),
          tr("缺少本地库") },

        { QStringLiteral("OPTIFINE_IS_NOT_COMPATIBLE_WITH_FORGE"),
          QStringLiteral(R"((java\.lang\.NoSuchMethodError: 'java\.lang\.Class sun\.misc\.Unsafe\.defineAnonymousClass|java\.lang\.NoSuchMethodError: 'void net\.minecraft\.client\.renderer\.texture\.SpriteContents|TRANSFORMER/net\.optifine/net\.optifine\.reflect\.Reflector\.<clinit>))"),
          {},
          tr("OptiFine 与当前 Forge 版本不兼容！请更换 OptiFine 版本，或移除 OptiFine 后使用其他光影方案（如 Iris/Sodium）。"),
          tr("OptiFine 与 Forge 不兼容") },

        { QStringLiteral("MOD_FILES_ARE_DECOMPRESSED"),
          QStringLiteral(R"((The directories below appear to be extracted jar files\. Fix this before you continue|Extracted mod jars found, loading will NOT continue))"),
          {},
          tr("发现被解压的模组文件夹！模组不应以解压文件夹形式放入 mods 目录。请删除这些文件夹，只保留 .jar 文件。"),
          tr("模组被解压") },

        { QStringLiteral("TOO_MANY_MODS_LEAD_TO_EXCEEDING_THE_ID_LIMIT"),
          QStringLiteral(R"(maximum id range exceeded)"),
          {},
          tr("模组数量过多导致 ID 超限（常见于 1.12.2 及以下）。请移除部分模组，或安装 ID 修复模组。"),
          tr("模组 ID 超限") },

        { QStringLiteral("MODMIXIN_FAILURE"),
          QStringLiteral(R"((MixinApplyError|Mixin prepare failed |Mixin apply failed |mixin\.injection\.throwables\.|\.mixins\.json\] FAILED during \)))"),
          {},
          tr("模组 Mixin 注入失败，通常为模组与游戏/加载器版本不兼容。请更新或移除相关模组。"),
          tr("Mixin 注入失败") },

        { QStringLiteral("MIXIN_APPLY_MOD_FAILED"),
          QStringLiteral(R"(Mixin apply for mod (.*?) failed)"),
          { QStringLiteral("id") },
          tr("模组 Mixin 应用失败。请更新或移除该模组。"),
          tr("Mixin 应用失败") },

        { QStringLiteral("FORGE_ERROR"),
          QStringLiteral(R"(An exception was thrown, the game will display an error screen and halt\.(?:\r?\n)*(.*?))"),
          { QStringLiteral("reason") },
          tr("Forge 抛出异常导致游戏停止。请查看崩溃报告中的具体堆栈信息。"),
          tr("Forge 运行时异常") },

        { QStringLiteral("FORGE_REPEAT_INSTALLATION"),
          QStringLiteral(R"(MultipleArgumentsForOptionException: Found multiple arguments for option)"),
          {},
          tr("Forge 重复安装（检测到多个加载参数）。请使用版本管理删除该版本后重新安装 Forge。"),
          tr("Forge 重复安装") },

        { QStringLiteral("OPTIFINE_REPEAT_INSTALLATION"),
          QStringLiteral(R"(ResolutionException: Module optifine reads another module named optifine)"),
          {},
          tr("OptiFine 重复安装（mods 文件夹和自动安装同时存在）。请删除 mods 文件夹中的 OptiFine，或卸载自动安装的 OptiFine。"),
          tr("OptiFine 重复安装") },

        { QStringLiteral("JAVA_VERSION_IS_TOO_HIGH"),
          QStringLiteral(R"((Unable to make protected final java\.lang\.Class java\.lang\.ClassLoader\.defineClass|Unsupported class file major version|because module java\.base does not export|java\.lang\.ClassNotFoundException: jdk\.nashorn|Exception in thread "main" java\.lang\.NullPointerException: Cannot read the array length because "urls" is null))"),
          {},
          tr("Java 版本过高！此游戏（或加载器）不支持当前 Java。1.12.2 及以下请使用 Java 8，1.16-1.17 建议 Java 8/17，1.18+ 使用 Java 17/21。"),
          tr("Java 版本过高") },

        { QStringLiteral("INSTALL_MIXINBOOTSTRAP"),
          QStringLiteral(R"(java\.lang\.ClassNotFoundException: org\.spongepowered\.asm\.launch\.MixinTweaker)"),
          {},
          tr("缺少 Mixin 引导类，Forge 安装可能不完整。请重新安装 Forge。"),
          tr("Forge 安装不完整") },

        { QStringLiteral("MOD_NAME"),
          QStringLiteral(R"(Invalid module name: '' is not a Java identifier)"),
          {},
          tr("模组文件名非法（纯中文或特殊字符文件名导致 JPMS 模块名无效）。请将 mods 文件夹中的模组重命名为英文+版本号的格式。"),
          tr("模组文件名非法") },

        { QStringLiteral("INCOMPLETE_FORGE_INSTALLATION"),
          QStringLiteral(R"((java\.io\.UncheckedIOException: java\.io\.IOException: Invalid paths argument|Failed to find Minecraft resource version (.*?) at (.*?)forge-(.*?)-client\.jar|Cannot find launch target fmlclient|Could not find net/minecraft/client/Minecraft\.class))"),
          {},
          tr("Forge 安装不完整（缺少核心库）。请删除该版本后重新安装 Forge。"),
          tr("Forge 安装不完整") },

        { QStringLiteral("NIGHT_CONFIG_FIXES"),
          QStringLiteral(R"(com\.electronwill\.nightconfig\.core\.io\.ParsingException: Not enough data available)"),
          {},
          tr("模组配置文件损坏。请删除 config 目录中对应模组的配置文件后重试（或安装 NightConfigFixes 模组）。"),
          tr("配置文件损坏") },

        { QStringLiteral("SHADERS_MOD"),
          QStringLiteral(R"(java\.lang\.RuntimeException: Shaders Mod detected\. Please remove it, OptiFine has built-in support for shaders\.)"),
          {},
          tr("检测到 Shaders Mod 与 OptiFine 冲突。请移除 Shaders Mod（OptiFine 已内置光影支持）。"),
          tr("Shaders Mod 冲突") },

        { QStringLiteral("MOD_FOREST_OPTIFINE"),
          QStringLiteral(R"(Error occurred applying transform of coremod META-INF/asm/multipart\.js function render)"),
          {},
          tr("模组与 OptiFine 不兼容（Forestry 等模组核心变换失败）。请更换模组版本或移除 OptiFine。"),
          tr("模组与 OptiFine 不兼容") },

        { QStringLiteral("PERFORMANT_FOREST_OPTIFINE"),
          QStringLiteral(R"(Critical injection failure: Redirector OnisOnLadder)"),
          {},
          tr("Performant 模组与 OptiFine 不兼容。请移除 Performant 或 OptiFine 之一。"),
          tr("Performant 与 OptiFine 冲突") },

        { QStringLiteral("TWILIGHT_FOREST_OPTIFINE"),
          QStringLiteral(R"(java\.lang\.IllegalArgumentException: (.*?) outside of image bounds (.*?))"),
          {},
          tr("暮色森林模组与 OptiFine 在 1.16 不兼容。请更新暮色森林或移除 OptiFine。"),
          tr("暮色森林与 OptiFine 冲突") },

        { QStringLiteral("JADE_FOREST_OPTIFINE"),
          QStringLiteral(R"(Critical injection failure: LVT in net/minecraft/client/renderer/GameRenderer)"),
          {},
          tr("Jade 模组与 OptiFine 在 1.20+ 不兼容。请更新 Jade 或移除 OptiFine。"),
          tr("Jade 与 OptiFine 冲突") },

        { QStringLiteral("NEOFORGE_FOREST_OPTIFINE"),
          QStringLiteral(R"(cpw\.mods\.modlauncher\.InvalidLauncherSetupException: Invalid Services found OptiFine)"),
          {},
          tr("NeoForge 与 OptiFine 不兼容。请移除 OptiFine，或使用 Iris 等替代光影方案。"),
          tr("NeoForge 与 OptiFine 冲突") },

        { QStringLiteral("RTSS_FOREST_SODIUM"),
          QStringLiteral(R"(RivaTuner Statistics Server \(RTSS\) is not compatible with Sodium)"),
          {},
          tr("检测到 RTSS（微星小飞机/显卡超频工具）与 Sodium 不兼容。请关闭 RTSS 或卸载后重试。"),
          tr("RTSS 与 Sodium 冲突") },
    };
    return kRules;
}

QStringList CrashDetector::matchRules(const QString& logText)
{
    QStringList matched;
    for (const CrashRule& rule : rules()) {
        QRegularExpression re(rule.pattern,
                              QRegularExpression::CaseInsensitiveOption |
                              QRegularExpression::MultilineOption |
                              QRegularExpression::DotMatchesEverythingOption);
        if (!re.isValid()) {
            qCWarning(logLaunch) << "[崩溃分析] 无效规则:" << rule.id << re.errorString();
            continue;
        }
        if (re.match(logText).hasMatch())
            matched.append(rule.id);
    }
    return matched;
}

QStringList CrashDetector::suggestionsForRules(const QStringList& matchedRuleIds)
{
    QStringList suggestions;
    for (const CrashRule& rule : rules()) {
        if (matchedRuleIds.contains(rule.id) && !rule.suggestion.isEmpty())
            suggestions.append(rule.suggestion);
    }
    return suggestions;
}

// ============================================================
// Stack-trace keyword analysis (主流启动器-style)
// ============================================================

QStringList CrashDetector::findKeywordsFromCrashReport(const QString& crashReport)
{
    // Blacklist of common MC/Java/loader package keywords — same spirit as
    // 主流启动器's PACKAGE_KEYWORD_BLACK_LIST. Any stack-trace package token that
    // is NOT in this list is likely a mod.
    static const QSet<QString> blacklist = {
        // minecraft
        QStringLiteral("net"), QStringLiteral("minecraft"), QStringLiteral("item"), QStringLiteral("setup"),
        QStringLiteral("block"), QStringLiteral("assist"), QStringLiteral("optifine"), QStringLiteral("player"),
        QStringLiteral("unimi"), QStringLiteral("fastutil"), QStringLiteral("tileentity"), QStringLiteral("events"),
        QStringLiteral("common"), QStringLiteral("blockentity"), QStringLiteral("client"), QStringLiteral("entity"),
        QStringLiteral("mojang"), QStringLiteral("main"), QStringLiteral("gui"), QStringLiteral("world"),
        QStringLiteral("server"), QStringLiteral("dedicated"), QStringLiteral("map"), QStringLiteral("dsi"),
        QStringLiteral("renderer"), QStringLiteral("chunk"), QStringLiteral("model"), QStringLiteral("loading"),
        QStringLiteral("color"), QStringLiteral("pipeline"), QStringLiteral("inventory"), QStringLiteral("launcher"),
        QStringLiteral("physics"), QStringLiteral("particle"), QStringLiteral("gen"), QStringLiteral("registry"),
        QStringLiteral("worldgen"), QStringLiteral("texture"), QStringLiteral("biomes"), QStringLiteral("biome"),
        QStringLiteral("monster"), QStringLiteral("passive"), QStringLiteral("ai"), QStringLiteral("integrated"),
        QStringLiteral("tile"), QStringLiteral("state"), QStringLiteral("play"), QStringLiteral("override"),
        QStringLiteral("transformers"), QStringLiteral("structure"), QStringLiteral("nbt"), QStringLiteral("pathfinding"),
        QStringLiteral("audio"), QStringLiteral("entities"), QStringLiteral("items"), QStringLiteral("renderers"),
        QStringLiteral("storage"), QStringLiteral("universal"), QStringLiteral("oshi"), QStringLiteral("platform"),
        // java
        QStringLiteral("java"), QStringLiteral("lang"), QStringLiteral("util"), QStringLiteral("nio"),
        QStringLiteral("io"), QStringLiteral("sun"), QStringLiteral("reflect"), QStringLiteral("zip"),
        QStringLiteral("jar"), QStringLiteral("jdk"), QStringLiteral("nashorn"), QStringLiteral("scripts"),
        QStringLiteral("runtime"), QStringLiteral("internal"),
        // generic titles
        QStringLiteral("mods"), QStringLiteral("mod"), QStringLiteral("impl"), QStringLiteral("org"),
        QStringLiteral("com"), QStringLiteral("cn"), QStringLiteral("cc"), QStringLiteral("jp"),
        QStringLiteral("core"), QStringLiteral("config"), QStringLiteral("registries"), QStringLiteral("lib"),
        QStringLiteral("ruby"), QStringLiteral("mc"), QStringLiteral("codec"), QStringLiteral("recipe"),
        QStringLiteral("channel"), QStringLiteral("embedded"), QStringLiteral("done"), QStringLiteral("netty"),
        QStringLiteral("network"), QStringLiteral("load"), QStringLiteral("github"), QStringLiteral("handler"),
        QStringLiteral("content"), QStringLiteral("feature"), QStringLiteral("file"), QStringLiteral("machine"),
        QStringLiteral("shader"), QStringLiteral("general"), QStringLiteral("helper"), QStringLiteral("init"),
        QStringLiteral("library"), QStringLiteral("api"), QStringLiteral("integration"), QStringLiteral("engine"),
        QStringLiteral("preload"), QStringLiteral("preinit"),
        // forge
        QStringLiteral("fml"), QStringLiteral("minecraftforge"), QStringLiteral("forge"), QStringLiteral("cpw"),
        QStringLiteral("modlauncher"), QStringLiteral("launchwrapper"), QStringLiteral("objectweb"),
        QStringLiteral("asm"), QStringLiteral("event"), QStringLiteral("eventhandler"), QStringLiteral("handshake"),
        QStringLiteral("modapi"), QStringLiteral("kcauldron"),
        // fabric
        QStringLiteral("fabricmc"), QStringLiteral("loader"), QStringLiteral("game"), QStringLiteral("knot"),
        QStringLiteral("launch"), QStringLiteral("mixin"),
    };

    QStringList result;
    static const QRegularExpression stackTraceRe(
        QStringLiteral(R"(Description: (.*?)[\n\r]+(?<stacktrace>[\w\W\n\r]+)A detailed walkthrough of the error)"));
    static const QRegularExpression frameRe(QStringLiteral(R"(at (.*?)\((.*?)\))"));
    static const QRegularExpression moduleRe(QStringLiteral(R"(\{(.*?)\})"));

    auto addTokens = [&result](const QStringList& tokens) {
        for (const QString& tok : tokens) {
            if (tok.isEmpty() || blacklist.contains(tok))
                continue;
            if (!result.contains(tok))
                result.append(tok);
        }
    };

    // Extract stack-trace region:
    //  1. Between "Description:" and "A detailed walkthrough" (some report formats)
    //  2. After "A detailed walkthrough of the error:" (standard MC format)
    QString stacktrace;
    QRegularExpressionMatch sm = stackTraceRe.match(crashReport);
    if (sm.hasMatch())
        stacktrace = sm.captured(QStringLiteral("stacktrace"));

    int walkIdx = crashReport.indexOf(QLatin1String("A detailed walkthrough"));
    if (walkIdx >= 0) {
        QString after = crashReport.mid(walkIdx);
        // take at most ~250 lines after the walkthrough marker
        const QStringList afterLines = after.split(QLatin1Char('\n'));
        QStringList limited;
        for (int i = 0; i < afterLines.size() && i < 250; i++)
            limited.append(afterLines[i]);
        stacktrace += QStringLiteral("\n") + limited.join(QLatin1Char('\n'));
    }

    if (stacktrace.isEmpty())
        return result;

    const QStringList lines = stacktrace.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        QRegularExpressionMatch fm = frameRe.match(line);
        if (fm.hasMatch()) {
            const QStringList method = fm.captured(1).split(QLatin1Char('.'));
            // Take package tokens, skipping the last two (class.method)
            for (int i = 0; i < method.size() - 2; i++)
                addTokens({ method[i] });
            // Module tokens (e.g. {xf:ModName:...})
            QRegularExpressionMatch mm = moduleRe.match(fm.captured(2));
            if (mm.hasMatch()) {
                for (const QString& module : mm.captured(1).split(QLatin1Char(','))) {
                    const QStringList split = module.split(QLatin1Char(':'));
                    if (split.size() >= 2 && split[0] == QLatin1String("xf"))
                        addTokens({ split[1] });
                }
            }
        }
    }
    return result;
}

// ============================================================
// Log collection
// ============================================================

QStringList CrashDetector::collectLogFiles(const QString& gameDir)
{
    QStringList files;
    QSet<QString> seen;

    auto addFile = [&files, &seen](const QString& path) {
        QFileInfo info(path);
        if (info.exists() && info.size() > 0 && !seen.contains(path)) {
            seen.insert(path);
            files.append(path);
        }
    };

    // 1. crash-reports/*.txt (Minecraft crash reports)
    QDir crashDir(gameDir + QStringLiteral("/crash-reports"));
    if (crashDir.exists()) {
        for (const QFileInfo& fi : crashDir.entryInfoList({ QStringLiteral("crash-*.txt") },
                                                          QDir::Files | QDir::NoDotAndDotDot, QDir::Time))
            addFile(fi.absoluteFilePath());
    }

    // 2. hs_err_pid*.log in game dir root
    QDir rootDir(gameDir);
    for (const QFileInfo& fi : rootDir.entryInfoList({ QStringLiteral("hs_err_pid*.log") },
                                                     QDir::Files | QDir::NoDotAndDotDot, QDir::Time))
        addFile(fi.absoluteFilePath());

    // 3. logs/latest.log + logs/debug.log
    addFile(gameDir + QStringLiteral("/logs/latest.log"));
    addFile(gameDir + QStringLiteral("/logs/debug.log"));

    return files;
}

// ============================================================
// scanLatestCrash (legacy quick scan)
// ============================================================

CrashReport CrashDetector::scanLatestCrash(const QString& gameDir)
{
    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] 开始扫描 目录=%1").arg(gameDir);

    {
        QDir crashDir(gameDir + QStringLiteral("/crash-reports"));
        if (crashDir.exists()) {
            auto entries = crashDir.entryInfoList({ QStringLiteral("crash-*.txt") },
                                                  QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
            if (!entries.isEmpty()) {
                CrashReport r = parseMinecraftCrash(entries.first().absoluteFilePath());
                if (r.isValid) return r;
            }
        }
    }

    {
        QDir rootDir(gameDir);
        auto entries = rootDir.entryInfoList({ QStringLiteral("hs_err_pid*.log") },
                                             QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
        if (!entries.isEmpty()) {
            CrashReport r = parseJvmCrash(entries.first().absoluteFilePath());
            if (r.isValid) return r;
        }
    }

    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] 未找到崩溃报告");
    return CrashReport{};
}

// ============================================================
// Full analysis (v2)
// ============================================================

CrashReport CrashDetector::analyzeCrash(const QString& gameDir,
                                        const QStringList& latestOutput,
                                        const QString& launcherLogPath)
{
    CrashReport r;
    qCInfo(logLaunch) << QStringLiteral("[崩溃分析] 开始完整分析 目录=%1").arg(gameDir);

    // ── 1. Collect logs ──
    r.collectedLogs = collectLogFiles(gameDir);
    if (!launcherLogPath.isEmpty() && QFileInfo::exists(launcherLogPath))
        r.collectedLogs.append(launcherLogPath);

    // ── 2. Parse the primary crash source (newest crash report / hs_err) ──
    CrashReport parsed;
    {
        QDir crashDir(gameDir + QStringLiteral("/crash-reports"));
        if (crashDir.exists()) {
            auto entries = crashDir.entryInfoList({ QStringLiteral("crash-*.txt") },
                                                  QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
            if (!entries.isEmpty())
                parsed = parseMinecraftCrash(entries.first().absoluteFilePath());
        }
        if (!parsed.isValid) {
            QDir rootDir(gameDir);
            auto entries = rootDir.entryInfoList({ QStringLiteral("hs_err_pid*.log") },
                                                 QDir::Files | QDir::NoDotAndDotDot, QDir::Time);
            if (!entries.isEmpty())
                parsed = parseJvmCrash(entries.first().absoluteFilePath());
        }
        if (!parsed.isValid) {
            // No crash report file — try latest.log
            QFileInfo latestLog(gameDir + QStringLiteral("/logs/latest.log"));
            if (latestLog.exists()) {
                QFile f(latestLog.absoluteFilePath());
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&f);
                    const QString content = in.readAll();
                    f.close();
                    parsed.type = QStringLiteral("log");
                    parsed.filePath = latestLog.absoluteFilePath();
                    parsed.timestamp = latestLog.lastModified();
                    // Find last ERROR/FATAL line as reason
                    static const QRegularExpression errRe(
                        QStringLiteral(R"((ERROR|FATAL)\].*?)"),
                        QRegularExpression::MultilineOption);
                    QStringList reasons;
                    auto it = errRe.globalMatch(content);
                    while (it.hasNext() && reasons.size() < 5)
                        reasons.append(it.next().captured(0).trimmed());
                    parsed.reason = reasons.isEmpty() ? tr("游戏异常退出（日志中未找到明确错误）")
                                                      : reasons.join(QStringLiteral(" | "));
                    parsed.description = tr("游戏进程异常退出，已从日志中提取错误信息");
                    parsed.isValid = true;
                }
            }
        }
    }

    if (parsed.isValid) {
        r.type = parsed.type;
        r.reason = parsed.reason;
        r.description = parsed.description;
        r.filePath = parsed.filePath;
        r.timestamp = parsed.timestamp;
        r.suspectedMods = parsed.suspectedMods;
    } else if (!latestOutput.isEmpty()) {
        // 无崩溃报告/latest.log，但有游戏进程输出（同主流启动器：规则引擎直接跑进程输出）。
        // 典型场景：Java 不匹配/版本过高等启动即退出，唯一诊断源就是 JVM stdout/stderr。
        const QString raw = latestOutput.join(QLatin1Char('\n'));
        r.type = QStringLiteral("log");
        r.reason = tr("游戏进程异常退出（未找到崩溃报告，依据 JVM 输出分析）");
        r.description = tr("游戏在启动阶段退出。以下基于游戏进程最后输出进行规则分析。");
        r.isValid = true;
        r.jvmOutput = latestOutput;
        // 提取最后一条 ERROR/异常行作为具体原因
        static const QRegularExpression errRe(
            QStringLiteral(R"((?m)^.*(ERROR|FATAL|Exception|Error).*$)"));
        QStringList errLines;
        auto it = errRe.globalMatch(raw);
        while (it.hasNext() && errLines.size() < 8)
            errLines.append(it.next().captured(0).trimmed());
        if (!errLines.isEmpty())
            r.reason = errLines.last();
    } else {
        r.isValid = false;
        r.reason = tr("未找到崩溃报告");
        r.description = tr("未在游戏目录中找到崩溃报告或错误日志，可能是进程被强制结束或崩溃前未写入报告。");
        return r;
    }
    r.isValid = true;

    // ── 3. Build analysis text = crash report + latest.log + process output ──
    QString analysisText;
    auto appendFileContent = [&analysisText](const QString& path, const QString& header) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            analysisText += QStringLiteral("\n\n===== %1: %2 =====\n")
                                .arg(header, QFileInfo(path).fileName());
            analysisText += QString::fromUtf8(f.readAll());
            f.close();
        }
    };

    if (!r.filePath.isEmpty())
        appendFileContent(r.filePath, QStringLiteral("崩溃报告"));
    appendFileContent(gameDir + QStringLiteral("/logs/latest.log"), QStringLiteral("游戏日志"));
    appendFileContent(gameDir + QStringLiteral("/logs/debug.log"), QStringLiteral("调试日志"));

    if (!latestOutput.isEmpty()) {
        analysisText += QStringLiteral("\n\n===== 游戏进程最后输出 =====\n");
        analysisText += latestOutput.join(QLatin1Char('\n'));
        r.jvmOutput = latestOutput;
    }
    r.analysisText = analysisText;

    // ── 4. Run rule engine ──
    r.matchedRules = matchRules(analysisText);
    if (!r.matchedRules.isEmpty())
        r.suggestions = suggestionsForRules(r.matchedRules);

    // ── 5. Stack-trace keyword analysis (suspected mods) ──
    if (r.type == QLatin1String("minecraft")) {
        QString crashContent;
        QFile f(r.filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            crashContent = QString::fromUtf8(f.readAll());
            f.close();
        }
        if (!crashContent.isEmpty()) {
            for (const QString& kw : findKeywordsFromCrashReport(crashContent)) {
                if (!r.suspectedMods.contains(kw))
                    r.suspectedMods.append(kw);
            }
        }
    }

    // ── 6. Export logs + write report if too long ──
    const int kInlineLimit = 6000;  // chars — beyond this, write to file
    const bool hasSuggestions = !r.suggestions.isEmpty();
    const bool hasMods = !r.suspectedMods.isEmpty();
    r.reportTooLong = r.analysisText.size() > kInlineLimit || hasSuggestions || hasMods;

    if (r.reportTooLong) {
        QString base = gameDir + QStringLiteral("/crash-analysis");
        QDir().mkpath(base);
        QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        r.exportDir = base + QStringLiteral("/") + ts;
        QDir().mkpath(r.exportDir);

        // Export collected logs
        for (const QString& src : r.collectedLogs) {
            QFileInfo si(src);
            if (!si.exists()) continue;
            QString dst = r.exportDir + QStringLiteral("/") + si.fileName();
            // avoid name collision with report file
            if (si.fileName() == QLatin1String("analysis-report.md")) {
                dst = r.exportDir + QStringLiteral("/source-") + si.fileName();
            }
            QFile::copy(src, dst);
        }

        // Write analysis report
        r.reportFilePath = r.exportDir + QStringLiteral("/analysis-report.md");
        writeReport(r, r.reportFilePath);
    }

    qCInfo(logLaunch) << QStringLiteral("[崩溃分析] 完成 原因=%1 命中规则=%2 嫌疑模组=%3")
                             .arg(r.reason)
                             .arg(r.matchedRules.size())
                             .arg(r.suspectedMods.size());
    return r;
}

// ============================================================
// Export logs
// ============================================================

QString CrashDetector::exportLogs(const QString& gameDir,
                                  const QString& exportZipPath,
                                  const QString& launcherLogPath)
{
    if (exportZipPath.isEmpty())
        return {};

    // 确保目标目录存在（zip 文件所在目录）
    QFileInfo zipInfo(exportZipPath);
    if (!QDir().mkpath(zipInfo.absolutePath())) {
        qCWarning(logLaunch) << "[崩溃分析] 创建导出目录失败:" << zipInfo.absolutePath();
        return {};
    }

    // 收集日志文件
    const QStringList logs = collectLogFiles(gameDir);
    if (logs.isEmpty() && launcherLogPath.isEmpty()) {
        qCWarning(logLaunch) << "[崩溃分析] 没有可导出的日志";
        return {};
    }

    QZipWriter zip(exportZipPath);
    zip.setCompressionPolicy(QZipWriter::AlwaysCompress);
    int added = 0;

    for (const QString& src : logs) {
        QFile f(src);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray data = f.readAll();
        f.close();
        if (data.isEmpty())
            continue;
        // 顶层目录打平（仅文件名），避免路径嵌套
        zip.addFile(QFileInfo(src).fileName(), data);
        added++;
    }

    if (!launcherLogPath.isEmpty() && QFileInfo::exists(launcherLogPath)) {
        QFile f(launcherLogPath);
        if (f.open(QIODevice::ReadOnly)) {
            zip.addFile(QStringLiteral("launcher-log.txt"), f.readAll());
            f.close();
            added++;
        }
    }

    zip.close();
    if (added == 0 || !QFile::exists(exportZipPath)) {
        QFile::remove(exportZipPath);
        return {};
    }

    qCInfo(logLaunch) << "[崩溃分析] 日志已打包 文件=" << exportZipPath << "条目数=" << added;
    return exportZipPath;
}

// ============================================================
// Report writer
// ============================================================

bool CrashDetector::writeReport(const CrashReport& r, const QString& reportFilePath)
{
    QFile f(reportFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "# Shadow Launcher 崩溃分析报告\n\n";
    out << "- 生成时间: " << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
    out << "- 崩溃类型: " << (r.type == QLatin1String("jvm") ? QStringLiteral("JVM") :
                              r.type == QLatin1String("minecraft") ? QStringLiteral("Minecraft") :
                              QStringLiteral("游戏日志")) << "\n";
    out << "- 原因: " << r.reason << "\n";
    out << "- 崩溃报告: " << (r.filePath.isEmpty() ? QStringLiteral("无") : r.filePath) << "\n\n";

    if (!r.description.isEmpty()) {
        out << "## 描述\n\n" << r.description << "\n\n";
    }

    if (!r.matchedRules.isEmpty()) {
        out << "## 命中的分析规则\n\n";
        for (const CrashRule& rule : rules()) {
            if (r.matchedRules.contains(rule.id)) {
                out << "### " << rule.title << "\n\n";
                out << rule.suggestion << "\n\n";
            }
        }
    }

    if (!r.suspectedMods.isEmpty()) {
        out << "## 疑似相关模组\n\n";
        for (const QString& mod : r.suspectedMods)
            out << "- " << mod << "\n";
        out << "\n";
    }

    if (!r.analysisText.isEmpty()) {
        out << "## 完整日志\n\n```text\n";
        out << r.analysisText;
        out << "\n```\n";
    }

    f.close();
    return true;
}

// ============================================================
// Legacy parsers (kept from v1)
// ============================================================

CrashReport CrashDetector::parseJvmCrash(const QString& filePath)
{
    CrashReport r;
    r.type = QStringLiteral("jvm");
    r.filePath = filePath;
    r.timestamp = QFileInfo(filePath).lastModified();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return r;

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    f.close();

    for (int i = 0; i < lines.size() && i < 20; ++i) {
        const QString& line = lines[i];

        if (line.startsWith(QLatin1String("#"))) {
            static const QRegularExpression sigRe(
                QStringLiteral(R"(#\s+(SIG\w+|EXCEPTION_\w+)\s*\()"),
                QRegularExpression::CaseInsensitiveOption);
            auto m = sigRe.match(line);
            if (m.hasMatch()) {
                r.reason = m.captured(1);
                r.description = tr("JVM 崩溃: %1").arg(r.reason);
                break;
            }

            static const QRegularExpression internalRe(
                QStringLiteral(R"(#\s+Internal\s+Error\s*\(([^)]+)\))"),
                QRegularExpression::CaseInsensitiveOption);
            m = internalRe.match(line);
            if (m.hasMatch()) {
                r.reason = m.captured(1).trimmed();
                r.description = tr("JVM 内部错误: %1").arg(r.reason);
                break;
            }
        }
    }

    if (r.reason.isEmpty()) {
        r.reason = QStringLiteral("Unknown JVM crash");
        r.description = tr("JVM 异常崩溃，详见崩溃报告");
    }

    r.suspectedMods = extractSuspectedMods(lines);
    r.isValid = true;

    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] JVM崩溃解析完成 原因=%1").arg(r.reason);
    return r;
}

CrashReport CrashDetector::parseMinecraftCrash(const QString& filePath)
{
    CrashReport r;
    r.type = QStringLiteral("minecraft");
    r.filePath = filePath;
    r.timestamp = QFileInfo(filePath).lastModified();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return r;

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    f.close();

    QString description;
    QString reason;

    for (const QString& line : lines) {
        if (line.startsWith(QLatin1String("Description: "))) {
            reason = line.mid(13).trimmed();
        }

        if (line.startsWith(QLatin1String("// ")) || line.startsWith(QLatin1String("//"))) {
            QString descLine = line.mid(3).trimmed();
            if (!descLine.isEmpty() && descLine != QLatin1String("Minecraft Crash Report ----")
                && !descLine.startsWith(QLatin1String("Description:"))) {
                if (description.isEmpty())
                    description = descLine;
                else if (description.length() < 200)
                    description += QStringLiteral(" | ") + descLine;
            }
        }

        if (line.startsWith(QLatin1String("A detailed walkthrough")) ||
            line.startsWith(QLatin1String("-- Head --"))) {
            break;
        }
    }

    if (reason.isEmpty()) {
        reason = description.isEmpty()
                     ? QStringLiteral("Unknown Minecraft crash")
                     : description;
    }

    if (description.isEmpty()) {
        description = reason;
    }

    r.reason = reason;
    r.description = description.left(200);
    r.suspectedMods = extractSuspectedMods(lines);
    r.isValid = true;

    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] MC崩溃解析完成 原因=%1").arg(r.reason);
    return r;
}

QStringList CrashDetector::extractSuspectedMods(const QStringList& lines)
{
    QStringList mods;
    bool readingMods = false;

    for (const QString& line : lines) {
        if (line.contains(QLatin1String("Mod List:")) ||
            line.contains(QLatin1String("-- Mod List --")) ||
            line.contains(QLatin1String("-- Mods --")) ||
            line.contains(QLatin1String("-- All Mods --"))) {
            readingMods = true;
            continue;
        }

        if (readingMods) {
            if (line.trimmed().isEmpty() || line.startsWith(QLatin1String("-- "))) {
                readingMods = false;
                break;
            }

            if (line.contains(QLatin1String("|"))) {
                QStringList parts = line.split(QLatin1Char('|'), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QString modName = parts[0].trimmed();
                    if (!modName.isEmpty() && modName != QLatin1String("Name")
                        && modName != QLatin1String("State") && modName != QLatin1String("LCH"))
                        mods.append(modName);
                }
            }
        }

        if (readingMods) continue;

        static const QRegularExpression modRe(QStringLiteral(R"(\[([\w-]+(?:-[\d.]+)?\.jar)\])"));
        auto it = modRe.globalMatch(line);
        while (it.hasNext()) {
            auto m = it.next();
            QString jarName = m.captured(1);
            jarName.remove(QRegularExpression(QStringLiteral(R"(-\d+[.\d]*.*)")));
            if (!mods.contains(jarName))
                mods.append(jarName);
        }
    }

    return mods;
}

} // namespace ShadowLauncher
