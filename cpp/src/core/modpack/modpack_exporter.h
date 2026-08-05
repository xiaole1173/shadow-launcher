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

    /// 列出版本下所有存档名（saves/ 子目录），供导出内容列表勾选（同步快操作）
    Q_INVOKABLE QStringList listSaves(const QString& versionId) const;

    /// 导出已安装版本为整合包（完全对齐主流启动器实现 PageInstanceExport）。
    ///   format: 0=Modrinth(.mrpack) 1=CurseForge(.zip)
    ///   selectedSaves: 勾选的存档名列表
    ///   modrinthUploadMode: 仅查 Modrinth（跳过 CurseForge）
    ///   hostedAssetsOnly: 仅打包包内资源（跳过全部联网查询，主流启动器 CheckAdvancedInclude）
    ///   includeJava: 打包便携 Java 运行时（java_cache 中匹配的 JRE）
    Q_INVOKABLE void exportVersion(const QString& versionId, const QString& displayName,
                                   const QString& packVersion, bool includeConfig,
                                   const QVariantList& selectedSaves,
                                   bool includeResourcepacks, bool includeShaderpacks,
                                   bool modrinthUploadMode, bool hostedAssetsOnly,
                                   bool includeJava, int format, const QString& outPath);
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
