// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

namespace ShadowLauncher {

/// Yggdrasil 外置登录皮肤获取器（PCL 方案）
/// 通过 sessionserver API 获取角色纹理，下载皮肤 PNG 到本地缓存
class YggdrasilSkinFetcher : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString skinPath READ skinPath NOTIFY skinChanged)
    Q_PROPERTY(int skinVariant READ skinVariant NOTIFY skinChanged)

public:
    explicit YggdrasilSkinFetcher(QObject *parent = nullptr);

    QString skinPath() const { return m_skinPath; }
    int skinVariant() const { return m_skinVariant; }

    /// 开始获取皮肤：GET {apiRoot}/sessionserver/session/minecraft/profile/{uuid}
    /// 流程如 PCL McSkinGetAddress → McSkinDownload
    Q_INVOKABLE void fetchSkin(const QString &apiRoot, const QString &uuid);

    /// 缓存目录（供外部构造头像 URL 使用）
    static QString cacheDir();

signals:
    void skinChanged();
    void skinReady();
    void skinFailed(const QString &error);

private slots:
    void onProfileReply();
    void onDownloadReply();

private:
    QString m_skinPath;
    int m_skinVariant = 0;  // 0=classic/steve, 1=slim/alex
    QNetworkAccessManager *m_nam = nullptr;
    QString m_pendingUuid;   // 下载中暂存 uuid，用于写缓存文件名
    int m_pendingVariant = 0;
};

} // namespace ShadowLauncher
