// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QImage>
#include <QString>
#include <QDir>
#include <QPainter>

// ── 头部裁剪：从完整 MC 皮肤裁剪/缩放出 128×128 头像 ──
// 移植自 AccountBackend::renderHead3D，作为独立工具函数让
// 正版登录（AccountBackend）和外置登录（YggdrasilSkinFetcher）共用
// 避免两份相同逻辑
//
// skinPath: 完整皮肤 PNG 路径
// 返回: _head.png 路径

inline QString renderHeadFromSkin(const QString& skinPath)
{
    QImage skin(skinPath);
    if (skin.isNull()) return {};

    constexpr int FACE_X=8, FACE_Y=8, FACE_W=8, FACE_H=8;
    constexpr int HAT_X=40, HAT_Y=8, HAT_W=8, HAT_H=8;
    constexpr int CANVAS = 128;
    constexpr int FACE_SZ = CANVAS * 3 / 4;   // 96
    constexpr int HAT_SZ  = CANVAS * 7 / 8;   // 112

    bool hasHat = (skin.width() >= 64 && skin.height() >= 32);

    // Face layer (same as renderHead3D)
    QImage face = skin.copy(FACE_X, FACE_Y, FACE_W, FACE_H);
    QImage faceScaled = face.scaled(FACE_SZ, FACE_SZ, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                            .convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QImage out(CANVAS, CANVAS, QImage::Format_ARGB32_Premultiplied);
    out.fill(0);

    int faceX = (CANVAS - FACE_SZ) / 2;
    int faceY = (CANVAS - FACE_SZ) / 2;
    for (int y = 0; y < FACE_SZ; ++y) {
        QRgb* dst = (QRgb*)out.scanLine(faceY + y) + faceX;
        const QRgb* src = (const QRgb*)faceScaled.constScanLine(y);
        for (int x = 0; x < FACE_SZ; ++x) dst[x] = src[x];
    }

    // Hat layer (same pixel-level copy as renderHead3D)
    if (hasHat) {
        QImage hatSrc = skin.copy(HAT_X, HAT_Y, HAT_W, HAT_H);
        QImage hat = hatSrc.scaled(HAT_SZ, HAT_SZ, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                          .convertToFormat(QImage::Format_ARGB32_Premultiplied);
        int hatX = (CANVAS - HAT_SZ) / 2;
        int hatY = (CANVAS - HAT_SZ) / 2;
        for (int y = 0; y < HAT_SZ; ++y) {
            QRgb* dst = (QRgb*)out.scanLine(hatY + y) + hatX;
            const QRgb* src = (const QRgb*)hat.constScanLine(y);
            for (int x = 0; x < HAT_SZ; ++x) {
                int a = qAlpha(src[x]);
                if (a >= 64) dst[x] = src[x];
            }
        }
    }

    QString headPath = skinPath.left(skinPath.length() - 4) + QStringLiteral("_head.png");
    if (out.save(headPath, "PNG"))
        return headPath;
    return {};
}
