// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QString>
#include <QLocale>

namespace ShadowLauncher {

namespace mc_language {

// ── Mapping ──

/// Map QLocale to Minecraft language code (e.g. "zh_cn", "zh_hk", "en_us")
/// Differentiates Chinese regions via QLocale::country().
QString localeToMinecraftLang(const QLocale& locale);

/// Map IP region code (ISO 3166-1 alpha-2, e.g. "CN", "HK", "TW") to Minecraft lang code
QString regionToMinecraftLang(const QString& regionCode);

// ── options.txt I/O ──

/// Write lang:X line to options.txt inside versionGameDir.
/// Reads existing options.txt, replaces or appends the "lang:" line.
/// @return true if write succeeded
bool writeOptionsTxt(const QString& versionGameDir, const QString& mcLangCode);

} // namespace mc_language

} // namespace ShadowLauncher
