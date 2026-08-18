// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// ────────────────────────────────────────────────────────────────
// 外部 .minecraft 文件夹「选择 / 识别 / 读取」后端（QML 接口预留，2026-08-18）
//
// 目的：兼容 PCL2 / HMCL / 官方启动器生成的 .minecraft，正确处理「版本隔离」
// 与「非版本隔离」两种形态，且绝不破坏导入目录的内部结构与功能。
//
// 设计：
//   - probe(dir)：只读分析选中目录（有效性/布局形态/来源/版本清单/每版本游戏目录）。
//   - applyAsGameDir()：把启动器活动游戏目录切换到该外部目录（尊重原布局，
//     进入外部只读模式：不写版本隔离配置、不做结构改动）；通过信号交由
//     ShadowBackend 完成子后端接线。
//   - revertToDefault()：切回启动器自带 .minecraft（退出外部模式）。
//
// 本类不直接持有 AppBackend/VersionBackend 等依赖；应用/回退通过信号委托给
// ShadowBackend（它拥有全部子后端与装配逻辑），保持本类可独立测试。
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QFutureWatcher>
#include "../core/minecraft_layout.h"

namespace ShadowLauncher {

class MinecraftFolderBackend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentDir READ currentDir NOTIFY folderChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY analysisChanged)
    Q_PROPERTY(int layoutId READ layoutId NOTIFY analysisChanged)
    Q_PROPERTY(QString layoutName READ layoutName NOTIFY analysisChanged)
    Q_PROPERTY(QString launcherName READ launcherName NOTIFY analysisChanged)
    Q_PROPERTY(int versionCount READ versionCount NOTIFY analysisChanged)
    Q_PROPERTY(bool rootHasGameData READ rootHasGameData NOTIFY analysisChanged)
    Q_PROPERTY(int isolatedVersionCount READ isolatedVersionCount NOTIFY analysisChanged)
    Q_PROPERTY(bool foreignActive READ isForeignActive NOTIFY foreignChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)

public:
    explicit MinecraftFolderBackend(QObject* parent = nullptr);

    // ── 读取当前分析结果 ──
    QString currentDir() const { return m_info.root; }
    bool isValid() const { return m_info.valid; }
    int layoutId() const { return static_cast<int>(m_info.layout); }
    QString layoutName() const;
    QString launcherName() const;
    int versionCount() const { return m_info.totalVersionCount; }
    bool rootHasGameData() const { return m_info.rootHasGameData; }
    int isolatedVersionCount() const { return m_info.isolatedVersionCount; }
    bool isForeignActive() const { return m_applied; }
    QStringList notes() const { return m_info.notes; }

    // ── QML 接口 ──
    /// 选择并识别目录（只读，不切换活动目录）。返回是否识别为有效 .minecraft。
    Q_INVOKABLE bool probe(const QString& dir);
    /// 清空当前分析
    Q_INVOKABLE void clear();
    /// 完整分析结果（QML 模型用）
    Q_INVOKABLE QVariantMap result() const;
    /// 每版本详情列表：{id, jsonPath, jarPath, gameDir, isolated, hasJson, modsCount}
    Q_INVOKABLE QVariantList versionInfos() const;
    /// 解析某版本的实际游戏数据目录（布局感知）
    Q_INVOKABLE QString versionGameDir(const QString& versionId) const;
    Q_INVOKABLE QString versionJsonPath(const QString& versionId) const;
    Q_INVOKABLE QString versionJarPath(const QString& versionId) const;
    Q_INVOKABLE bool versionHasJson(const QString& versionId) const;
    /// 某版本 mods 目录下 .jar 数量（不存在返回 0）
    Q_INVOKABLE int versionModsCount(const QString& versionId) const;
    /// 应用为启动器活动游戏目录（进入外部只读模式）。要求已成功 probe。
    Q_INVOKABLE bool applyAsGameDir();
    /// 切回启动器自带 .minecraft（退出外部模式）
    Q_INVOKABLE void revertToDefault();

    // ── ShadowBackend 读取有效隔离形态（UI 开关显示）──
    /// 外部模式下，有效隔离 = 布局为 Isolated
    bool effectiveIsolation() const;

    // ════════════════════════════════════════════════════════════
    // 游戏文件夹注册表（2026-08-18 QML 阶段）
    // ════════════════════════════════════════════════════════════
    // 命名文件：{folder}/config/shadow_folder.json —— 每个 .minecraft 内独立存放，
    // 记录用户命名与基本信息（来源等）。格式自定。
    // 注册表：{dataDir}/game_folders.json —— 仅存导入的路径列表（有序）与活动路径。
    // 删除条目不删文件夹：removeGameFolder 只从注册表移除。
    void setDataDir(const QString& dir) { m_dataDir = dir; }
    QString dataDir() const { return m_dataDir; }

    /// 异步刷新全部文件夹条目（注册表 + 逐目录探测）→ 完成后发 foldersReady/foldersChanged。
    /// 工作线程执行，不阻塞主进程。
    Q_INVOKABLE void refreshFolders();
    /// 当前缓存的文件夹列表（主线程读）。每项：{path,name,isDefault,exists,active,
    /// layoutName,launcherName,versionCount}
    QVariantList folders() const { return m_folders; }
    /// 活动（当前使用）文件夹路径
    Q_INVOKABLE QString activeFolderPath() const;
    /// 本启动器固有文件夹（exe 同目录 .minecraft）
    Q_INVOKABLE QString defaultFolderPath() const;
    /// 添加导入文件夹（写命名文件 + 注册表，随后异步刷新）。name 为空则取目录名。
    Q_INVOKABLE bool addGameFolder(const QString& path, const QString& name);
    /// 重命名（更新命名文件，随后异步刷新）
    Q_INVOKABLE bool renameGameFolder(const QString& path, const QString& newName);
    /// 从注册表移除条目（不删除文件夹）；若移除的是当前活动外部目录则回退默认
    Q_INVOKABLE bool removeGameFolder(const QString& path);
    /// 切换活动游戏目录到指定文件夹（探测 → 应用；布局尊重原目录）
    Q_INVOKABLE bool setActiveFolder(const QString& path);
    /// 原生目录选择对话框，返回选中的路径（空串=取消）
    Q_INVOKABLE QString pickFolderDialog();

signals:
    void folderChanged();
    void analysisChanged();
    /// 应用请求：root + layout(int) → ShadowBackend 完成子后端接线
    void applyRequested(const QString& root, int layout);
    /// 回退到默认目录请求
    void revertRequested();
    void foreignChanged();
    /// 文件夹列表刷新完成（异步）
    void foldersReady(const QVariantList& folders);
    void foldersChanged();

private:
    QString resolveGameDirFor(const QString& versionId) const;
    QString findJsonInDir(const QString& verDir) const;
    QString findJarInDir(const QString& verDir) const;

    // ── 注册表辅助 ──
    QString registryPath() const;
    QStringList readRegistryPaths() const;          // 已导入路径（有序）
    void writeRegistry(const QStringList& paths) const;
    static QString readNameFile(const QString& folder);        // 读命名文件→名称（空=无）
    static bool writeNameFile(const QString& folder, const QString& name);
    static QVariantMap buildFolderEntry(const QString& path, const QString& name,
                                        bool isDefault, const QString& currentActive);
    static QVariantList collectFolders(const QStringList& paths,
                                       const QString& defaultPath, const QString& currentActive);

    MinecraftFolderInfo m_info;
    bool m_applied = false;   // 是否已应用为活动目录
    QString m_dataDir;        // 注册表存放目录（ShadowBackend 注入 m_app->dataDir()）
    QVariantList m_folders;   // 主线程缓存（refreshFolders 完成后更新）
    QFutureWatcher<QVariantList>* m_folderWatcher = nullptr;
};

} // namespace ShadowLauncher
