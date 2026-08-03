// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "modpack_common.h"

#include <QRegularExpression>

namespace ShadowLauncher {

bool parseLoaderId(const QString& rawId, QString* loaderType, QString* loaderVersion, QString* error)
{
    if (loaderType) loaderType->clear();
    if (loaderVersion) loaderVersion->clear();
    if (error) error->clear();

    QString id = rawId.trimmed();
    if (id.isEmpty()) return false;

    const QString lower = id.toLower();

    // 主流启动器：过老整合包使用 "forge-1.7.10-recommended" 这类占位 id，无法安装
    if (lower.contains(QStringLiteral("recommended"))) {
        if (error) *error = QStringLiteral("整合包版本过老（加载器标识为占位符 %1），已不支持安装").arg(rawId);
        return false;
    }

    struct LoaderPrefix { QString prefix; QString type; };
    static const LoaderPrefix kPrefixes[] = {
        {QStringLiteral("neoforge-"), QStringLiteral("neoforge")},
        {QStringLiteral("fabric-loader-"), QStringLiteral("fabric")},
        {QStringLiteral("fabric-"), QStringLiteral("fabric")},
        {QStringLiteral("forge-"), QStringLiteral("forge")},
        {QStringLiteral("quilt-loader-"), QStringLiteral("quilt")},
        {QStringLiteral("quilt-"), QStringLiteral("quilt")},
    };

    for (const LoaderPrefix& lp : kPrefixes) {
        if (lower.startsWith(lp.prefix)) {
            const QString ver = id.mid(lp.prefix.size()).trimmed();
            if (ver.isEmpty()) {
                if (error) *error = QStringLiteral("加载器标识缺少版本号: %1").arg(rawId);
                return false;
            }
            if (loaderType) *loaderType = lp.type;
            if (loaderVersion) *loaderVersion = ver;
            return true;
        }
    }

    if (error) *error = QStringLiteral("不支持的加载器标识: %1").arg(rawId);
    return false;
}

QString sanitizeVersionName(const QString& raw)
{
    QString name = raw.simplified();
    // 版本名允许中文等 Unicode 字符（旧实现把非 ASCII 全剥掉 → 中文版本名变成一串下划线）；
    // 仅转换空白与 Windows 文件名非法字符为下划线（主流启动器 ValidateFolderName 同思路放宽版）
    name.replace(QRegularExpression(QStringLiteral("[\\s<>:\"/\\\\|?*\\x00-\\x1f]")), QStringLiteral("_"));
    while (name.contains(QStringLiteral("__")))
        name.replace(QStringLiteral("__"), QStringLiteral("_"));
    // Windows 不允许名称以点结尾
    while (name.endsWith(QLatin1Char('.')))
        name.chop(1);
    // 规避 Windows 保留设备名（CON/PRN/AUX/NUL/COM1-9/LPT1-9），加后缀避免建目录失败
    static const QStringList reserved = {
        QStringLiteral("con"), QStringLiteral("prn"), QStringLiteral("aux"), QStringLiteral("nul"),
        QStringLiteral("com1"), QStringLiteral("com2"), QStringLiteral("com3"), QStringLiteral("com4"),
        QStringLiteral("com5"), QStringLiteral("com6"), QStringLiteral("com7"), QStringLiteral("com8"),
        QStringLiteral("com9"), QStringLiteral("lpt1"), QStringLiteral("lpt2"), QStringLiteral("lpt3"),
        QStringLiteral("lpt4"), QStringLiteral("lpt5"), QStringLiteral("lpt6"), QStringLiteral("lpt7"),
        QStringLiteral("lpt8"), QStringLiteral("lpt9")};
    if (reserved.contains(name.toLower()))
        name += QStringLiteral("_pack");
    if (name.isEmpty())
        name = QStringLiteral("Modpack");
    return name;
}

QString sanitizeRelPath(const QString& entryName)
{
    QString name = entryName;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // 拒绝绝对路径 / 盘符开头
    if (name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\')))
        return {};
    if (name.length() >= 2 && name[1] == QLatin1Char(':') && name[0].isLetter())
        return {};

    const QStringList parts = name.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList safe;
    for (const QString& part : parts) {
        if (part == QLatin1String("..") || part == QLatin1String("."))
            return {};  // 路径穿越直接拒绝（主流启动器 同策略：越界即失败）
        for (const QChar c : part) {
            if (c == QLatin1Char(':') || c == QLatin1Char('*') || c == QLatin1Char('?')
                || c == QLatin1Char('"') || c == QLatin1Char('<') || c == QLatin1Char('>')
                || c == QLatin1Char('|')) {
                return {};
            }
        }
        safe.append(part);
    }
    return safe.join(QLatin1Char('/'));
}

} // namespace ShadowLauncher
