// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <future>

#include "../utils/types.h"

namespace ShadowLauncher {

class VersionManager : public QObject {
    Q_OBJECT

public:
    explicit VersionManager(QObject* parent = nullptr);
    ~VersionManager() override;

    // ── 配置 ──
    void setDataDir(const QString& path) { m_dataDir = path; }
    QString dataDir() const { return m_dataDir; }

    void setGameDir(const QString& path) { m_gameDir = path; }
    QString gameDir() const { return m_gameDir; }

    // ── 版本获取 ──
    /// 触发异步获取版本列表（先读缓存，缓存无或过期才请求网络）
    /// 完成后发射 versionsReady
    void fetchVersions();

    /// 同步获取版本列表（阻塞当前线程，用 QEventLoop）
    /// 优先使用已缓存结果，否则网络请求
    QVector<McVersion> fetchVersionsSync();

    /// 返回已加载的版本列表（不触发网络请求）
    QVector<McVersion> cachedVersions() const { return m_versions; }
    bool hasVersions() const { return !m_versions.isEmpty(); }

    // ── 过滤 ──
    QVector<McVersion> getByType(const QString& type) const;
    McVersion getLatest(const QString& type) const;

    // ── 安装状态 ──
    /// 检查版本是否已安装（{gameDir}/versions/{versionId}/{versionId}.json 存在）
    bool isInstalled(const QString& versionId) const;

    /// 获取版本安装路径
    QString getVersionPath(const QString& versionId) const;

    // ── 缓存 ──
    /// 从缓存文件加载（成功返回 true）
    bool loadFromCache();

    /// 保存当前版本列表到缓存文件
    void saveToCache() const;

    /// 缓存文件路径
    QString cacheFilePath() const;

    /// 网络获取（无缓存时，内部使用）
    void doNetworkFetch();

signals:
    /// 版本列表就绪（数量可能为 0，表示没有可用版本）
    void versionsReady(const QVector<McVersion>& versions);

    /// 网络请求出错
    void fetchError(const QString& error);

private:
    /// 解析 Mojang version_manifest.json
    QVector<McVersion> parseManifest(const QByteArray& rawJson) const;

    // ── 成员 ──
    QNetworkAccessManager* m_nam = nullptr;
    QVector<McVersion> m_versions;
    QString m_dataDir;
    QString m_gameDir;
    std::future<void> m_cacheLoadFuture;
};

} // namespace ShadowLauncher
