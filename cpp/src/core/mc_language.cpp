// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "mc_language.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

namespace ShadowLauncher {
namespace mc_language {

QString localeToMinecraftLang(const QLocale& locale)
{
    QLocale::Language lang = locale.language();
    QLocale::Country country = locale.country();

    switch (lang) {
    case QLocale::Chinese:
        switch (country) {
        case QLocale::China:        return QStringLiteral("zh_cn");
        case QLocale::HongKong:     return QStringLiteral("zh_hk");
        case QLocale::Macau:        return QStringLiteral("zh_hk");  // Traditional Chinese, HK variant
        case QLocale::Taiwan:       return QStringLiteral("zh_tw");
        default:                    return QStringLiteral("zh_cn");  // fallback
        }
    case QLocale::Japanese:
        return QStringLiteral("ja_jp");
    case QLocale::Korean:
        return QStringLiteral("ko_kr");
    default:
        return QStringLiteral("en_us");
    }
}

QString regionToMinecraftLang(const QString& regionCode)
{
    const QString upper = regionCode.toUpper().trimmed();

    // Chinese-speaking regions
    if (upper == QStringLiteral("CN"))  return QStringLiteral("zh_cn");
    if (upper == QStringLiteral("HK"))  return QStringLiteral("zh_hk");
    if (upper == QStringLiteral("MO"))  return QStringLiteral("zh_hk");
    if (upper == QStringLiteral("TW"))  return QStringLiteral("zh_tw");

    // Other commonly detected regions
    if (upper == QStringLiteral("JP"))  return QStringLiteral("ja_jp");
    if (upper == QStringLiteral("KR"))  return QStringLiteral("ko_kr");

    // Extend with more languages as needed:
    // FR→fr_fr, DE→de_de, ES→es_es, PT→pt_br, RU→ru_ru, IT→it_it, etc.

    return QStringLiteral("en_us");
}

bool writeOptionsTxt(const QString& versionGameDir, const QString& mcLangCode)
{
    if (versionGameDir.isEmpty() || mcLangCode.isEmpty()) return false;

    const QString optionsPath = versionGameDir + QStringLiteral("/options.txt");
    const QString langLine = QStringLiteral("lang:") + mcLangCode;

    // Read existing options.txt
    QStringList lines;
    bool foundLang = false;

    QFile file(optionsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.startsWith(QStringLiteral("lang:"))) {
                lines.append(langLine);
                foundLang = true;
            } else if (!line.isEmpty()) {
                lines.append(line);
            }
        }
        file.close();
    }

    if (!foundLang) {
        lines.append(langLine);
    }

    // Write back
    QDir().mkpath(versionGameDir);
    QFile outFile(optionsPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&outFile);
        for (const QString& line : lines) {
            out << line << QStringLiteral("\n");
        }
        outFile.close();
        qDebug().noquote() << "[mc_language] options.txt written, lang:" << mcLangCode
                           << "path:" << optionsPath;
        return true;
    }

    qWarning().noquote() << "[mc_language] Failed to write options.txt at:" << optionsPath;
    return false;
}

} // namespace mc_language
} // namespace ShadowLauncher
