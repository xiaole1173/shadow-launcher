// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QCryptographicHash>
#include <QByteArray>
#include <QString>

/// 统一文件哈希校验入口（2026-08-02：消除各下载引擎/安装器重复的 SHA1 hex 实现）
namespace ShadowLauncher {

/// SHA1 hex digest（小写十六进制字符串）
inline QString sha1Hex(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

} // namespace ShadowLauncher
