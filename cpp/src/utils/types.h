// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QVector>
#include <optional>

namespace ShadowLauncher {

// ============================================================
// Minecraft Version
// ============================================================

struct McVersion {
    QString id;           // e.g. "1.21.4"
    QString type;         // "release" | "snapshot" | "old_beta" | "old_alpha"
    QString url;          // Manifest URL from Mojang API
    QDateTime releaseTime;
    bool installed = false;
    QString installPath;

    static McVersion fromJson(const QJsonObject& obj);
    QJsonObject toJson() const;
};

// ============================================================
// Account (offline + online)
// ============================================================

struct Account {
    QString username;
    QString uuid;          // For offline: generated; for online: from Mojang
    QString accessToken;   // Online only
    QString skinUrl;       // NameMC or Mojang skin URL
    bool isOnline = false;

    QJsonObject toJson() const;
    static Account fromJson(const QJsonObject& obj);
};

// ============================================================
// Download Policy
// ============================================================

enum class DownloadSourcePolicy : int {
    PreferMirror   = 0,  // BMCLAPI primary, fall back to Mojang
    PreferOfficial = 1,  // Mojang primary, fall back to BMCLAPI
    AutoSwitch     = 2   // Mojang primary, auto-switch to BMCLAPI if slow
};

struct DownloadConfig {
    int maxWorkers = 64;
    double speedLimitMB = -1;
    DownloadSourcePolicy fileSource = DownloadSourcePolicy::PreferOfficial;
    DownloadSourcePolicy listSource = DownloadSourcePolicy::PreferOfficial;
};

// ============================================================
// Download Task
// ============================================================

struct DownloadTask {
    QString name;          // Display name
    QString url;           // Primary download URL
    QString savePath;      // Local path to save
    QString sha1;          // Expected SHA1 (Minecraft uses SHA1)
    qint64 totalBytes = 0;
    qint64 downloadedBytes = 0;
    bool completed = false;
    bool failed = false;
    QString errorMsg;
    QStringList mirrors;   // Alternative download URLs (Modrinth/CDN fallback)

    // --- Chunked segmented download fields ---
    int chunkIndex = 0;    // Which chunk this task represents (0-based)
    int chunkTotal = 1;    // Total chunks for this file (=1 means single-chunk)
    qint64 rangeStart = 0; // Byte range start for this chunk
    qint64 rangeEnd = -1;  // Byte range end (inclusive, -1 = to end of file)
};

// ============================================================
// Modrinth Resource
// ============================================================

struct ModrinthResource {
    QString id;
    QString title;
    QString description;
    QString iconUrl;
    QString projectType;   // "mod" | "shader" | "resourcepack"
    QString versionId;
    QString fileName;
    QString downloadUrl;
    qint64 size = 0;
    bool installed = false;
};

// ============================================================
// App Settings
// ============================================================

struct AppSettings {
    // Java
    QString javaPath;
    int minMemoryMB = 2048;
    int maxMemoryMB = 4096;
    QString jvmArgs;  // custom JVM arguments (space-separated)

    // Game
    QString gameDir;
    bool fullscreen = false;
    int windowWidth = 854;
    int windowHeight = 480;

    // Login
    bool rememberAccount = true;
    QString lastUsername;

    // UI
    QString language = "zh_CN";
    QString theme = "dark";

    QJsonObject toJson() const;
    static AppSettings fromJson(const QJsonObject& obj);
};

} // namespace ShadowLauncher
