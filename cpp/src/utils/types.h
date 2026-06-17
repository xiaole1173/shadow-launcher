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
// Download Task
// ============================================================

struct DownloadTask {
    QString name;          // Display name
    QString url;           // Download URL
    QString savePath;      // Local path to save
    QString sha1;          // Expected SHA1 (Minecraft uses SHA1)
    qint64 totalBytes = 0;
    qint64 downloadedBytes = 0;
    bool completed = false;
    bool failed = false;
    QString errorMsg;
    QStringList mirrors;   // Alternative download URLs (Modrinth/CDN fallback)
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
