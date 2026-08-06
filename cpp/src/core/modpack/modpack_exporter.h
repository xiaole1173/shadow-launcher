// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// modpack_exporter.h — 整合包导出（完全对齐主流启动器实现 PageInstanceExport，2026-08-05）。
//
// 流程（同主流启动器三阶段）：
//   1. 收集：按导出规则收集 mods + overrides（config/saves 子项/资源包/光影/选项）
//   2. 双平台查询：本地 mod 双哈希（Modrinth=SHA1、CurseForge=MurmurHash2 去空白种子1），
//      并行批量查询 Modrinth v2/version_files + CurseForge v1/fingerprints/432，
//      每个文件收集所有在线下载 URL（CF 域名五变体展开）
//   3. 生成：Modrinth .mrpack（files[] 引用 + 未托管实体进 overrides/mods/）或
//      CurseForge .zip（manifest.json files[] 引用 + 未托管实体进 overrides/mods/）
//
// ModrinthUploadMode：仅查 Modrinth（跳过 CF），用于上传 Modrinth 场景；
// 查询失败降级：单平台失败继续（该平台无结果），全失败则全部实体直装。
// 全部在 QtConcurrent worker 线程执行，进度/结果 invokeMethod 回主线程，UI 零阻塞。

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QAtomicInteger>

namespace ShadowLauncher {

class ModpackExporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)

public:
    explicit ModpackExporter(QObject* parent = nullptr);

    void setGameDir(const QString& dir);
    void setCurseForgeApiKey(const QString& key) { m_cfApiKey = key; }

    bool isBusy() const { return m_busy; }
    qreal progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }

    /// 导出选项定义（同主流启动器 ExportOption：规则驱动，!反选，\结尾=目录）
    struct ExportOptionDef {
        QString id;
        QString title;
        QString description;
        QStringList rules;          // 导出规则（空=选项恒不导出内容，仅作分组）
        bool defaultChecked = true;
        bool requireModLoader = false;          // 需 Mod 加载器（Forge/Fabric/NeoForge/Quilt）
        bool requireOptiFine = false;           // 需 OptiFine
        bool requireModLoaderOrOptiFine = false;// 两者其一
        QStringList showRules;      // 可见性规则（为空=恒可见；匹配版本目录内容才显示，主流启动器 ShowRules）
    };
    /// 全部选项定义（静态表）
    static const QList<ExportOptionDef>& optionDefs();

    /// 列出版本下所有存档名（saves/ 子目录）及修改时间，供导出内容列表勾选
    Q_INVOKABLE QVariantList listSaves(const QString& versionId) const;

    /// 导出上下文：按版本实际情况返回可导出内容（供导出界面动态渲染，同主流启动器 ShowRules）
    ///   versionExists/modable/hasOptiFine/hasMods/hasConfig/hasShaderpacks/
    ///   hasResourcepacks/hasSaves/hasScreenshots/hasServersDat/javaAvailable
    ///   options: [{id,title,description,visible,defaultChecked}]（按实际可见性过滤）
    ///   rpItems/shaderItems: 资源包/光影目录下 zip/rar/文件夹子项
    Q_INVOKABLE QVariantMap exportContext(const QString& versionId) const;

    /// 保存/读取导出配置（主流启动器 export_config.txt 语义：ini 段 + 规则段 + 追加内容段）
    /// 配置字段：name/version/includeJava/hostedAssetsOnly/modrinthUploadMode/
    ///           format/options(勾选 id 列表)/extraFiles(追加内容绝对路径)/packPath
    Q_INVOKABLE bool saveExportConfig(const QString& path, const QVariantMap& cfg) const;
    Q_INVOKABLE QVariantMap loadExportConfig(const QString& path) const;

    /// 导出已安装版本为整合包（完全对齐主流启动器实现 PageInstanceExport，规则驱动）。
    ///   format: 0=Modrinth(.mrpack) 1=CurseForge(.zip)
    ///   checkedOptions: 勾选的选项 id 列表（未列出的按默认值）
    ///   selectedSaves: 勾选的存档名列表
    ///   modrinthUploadMode: 仅查 Modrinth（跳过 CurseForge）
    ///   hostedAssetsOnly: 仅打包包内资源（跳过全部联网查询，主流启动器 CheckAdvancedInclude）
    ///   includeJava: 打包便携 Java 运行时（java_cache 中匹配的 JRE）
    ///   extraFiles: 追加内容绝对路径列表（\结尾=文件夹，复制到包根）
    Q_INVOKABLE void exportVersion(const QString& versionId, const QString& displayName,
                                   const QString& packVersion, const QVariantList& checkedOptions,
                                   const QVariantList& selectedSaves,
                                   bool modrinthUploadMode, bool hostedAssetsOnly,
                                   bool includeJava, int format, const QString& outPath,
                                   const QVariantList& extraFiles = {});
    Q_INVOKABLE void cancel();
    /// 联网查询失败后用户选择：true=继续导出（未查到文件直装）false=取消
    Q_INVOKABLE void continueAfterLookupFailure(bool cont);

signals:
    void busyChanged();
    void progressChanged();
    void finished(bool success, const QString& outPath, const QString& error);
    /// 联网查询失败（主流启动器 弹窗询问是否继续）：platform 0=Modrinth 1=CurseForge 2=全部
    void lookupFailed(int platform, const QString& detail);

private:
    void setProgress(qreal p, const QString& text);
    /// worker 线程内：emit lookupFailed 并轮询等待用户选择（主流启动器 弹窗语义）
    bool waitLookupDecision(int platform, const QString& detail);

    QString m_gameDir;
    QString m_cfApiKey;
    QAtomicInteger<int> m_cancel{0};
    QAtomicInteger<int> m_lookupContinue{1};   // 联网失败后用户选择（1=继续 0=取消），worker 等待
    bool m_busy = false;
    qreal m_progress = 0.0;
    QString m_statusText;
};

} // namespace ShadowLauncher
