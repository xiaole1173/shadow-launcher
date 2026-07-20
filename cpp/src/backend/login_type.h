// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>

namespace ShadowLauncher {

/// 第三方登录类型（per-version 设置）
/// 每个版本的登录方式可以独立选择，切换版本后自动恢复对应设置
struct LoginType {
    Q_GADGET
    Q_PROPERTY(int value MEMBER value CONSTANT)
public:
    enum Enum : int {
        Default = 0,         // 正版登录 或 离线登录（现有 UI）
        AuthlibInjector = 1, // Authlib-Injector / LittleSkin
        Nide8Auth = 2        // 统一通行证（Nide8Auth）
    };
    Q_ENUM(Enum)

    int value = Default;

    static QString toString(int type) {
        switch (type) {
            case Default:         return QStringLiteral("Default");
            case AuthlibInjector: return QStringLiteral("AuthlibInjector");
            case Nide8Auth:       return QStringLiteral("Nide8Auth");
            default:              return QStringLiteral("Unknown");
        }
    }
};

} // namespace ShadowLauncher
