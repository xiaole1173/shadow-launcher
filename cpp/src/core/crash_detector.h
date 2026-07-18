// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

namespace ShadowLauncher {

/// Result of scanning for a crash report
struct CrashReport {
    QString type;           // "jvm" or "minecraft"
    QString reason;         // Short crash reason (e.g. "java.lang.OutOfMemoryError")
    QString description;    // 1-2 line human-readable summary
    QStringList suspectedMods;  // Mod names extracted from crash report
    QString filePath;       // Absolute path to the crash report file
    QDateTime timestamp;    // When the crash occurred
    bool isValid = false;   // false if no crash report found

    Q_GADGET
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString reason MEMBER reason)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(QStringList suspectedMods MEMBER suspectedMods)
    Q_PROPERTY(QString filePath MEMBER filePath)
    Q_PROPERTY(QDateTime timestamp MEMBER timestamp)
    Q_PROPERTY(bool isValid MEMBER isValid)
};

class CrashDetector : public QObject {
    Q_OBJECT

public:
    explicit CrashDetector(QObject* parent = nullptr);

    /// Scan for the latest crash report in the game directory.
    /// Returns a CrashReport with isValid=true if found.
    CrashReport scanLatestCrash(const QString& gameDir);

private:
    /// Parse a hs_err_pid*.log (JVM crash) file
    CrashReport parseJvmCrash(const QString& filePath);
    /// Parse a crash-reports/crash-*.txt (Minecraft crash) file
    CrashReport parseMinecraftCrash(const QString& filePath);
    /// Extract suspected mod names from crash report lines
    QStringList extractSuspectedMods(const QStringList& lines);
};

} // namespace ShadowLauncher
