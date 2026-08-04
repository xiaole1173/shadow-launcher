// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVariantMap>

namespace ShadowLauncher {

/// Result of scanning for a crash report
struct CrashReport {
    QString type;           // "jvm" or "minecraft" or "log"
    QString reason;         // Short crash reason (e.g. "java.lang.OutOfMemoryError")
    QString description;    // 1-2 line human-readable summary
    QStringList suspectedMods;  // Mod names extracted from crash report
    QString filePath;       // Absolute path to the crash report file
    QDateTime timestamp;    // When the crash occurred
    bool isValid = false;   // false if no crash report found

    // ── Full analysis (v2) ──
    QStringList suggestions;     // Human-readable fix suggestions (from matched rules)
    QStringList matchedRules;    // Rule ids that matched
    QStringList collectedLogs;   // All log files collected for this analysis
    QString analysisText;        // Full analysis text (for report document)
    QString reportFilePath;      // Path of generated report document ("" if inline only)
    QString exportDir;           // Directory where logs were exported ("" if not exported)
    bool reportTooLong = false;  // true → report written to file, dialog shows summary only

    QVariantMap toVariantMap() const;

    Q_GADGET
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString reason MEMBER reason)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(QStringList suspectedMods MEMBER suspectedMods)
    Q_PROPERTY(QString filePath MEMBER filePath)
    Q_PROPERTY(QDateTime timestamp MEMBER timestamp)
    Q_PROPERTY(bool isValid MEMBER isValid)
    Q_PROPERTY(QStringList suggestions MEMBER suggestions)
    Q_PROPERTY(QStringList matchedRules MEMBER matchedRules)
    Q_PROPERTY(QStringList collectedLogs MEMBER collectedLogs)
    Q_PROPERTY(QString reportFilePath MEMBER reportFilePath)
    Q_PROPERTY(QString exportDir MEMBER exportDir)
    Q_PROPERTY(bool reportTooLong MEMBER reportTooLong)
};

/// A single crash-analysis rule (ported from 主流启动器 CrashReportAnalyzer)
struct CrashRule {
    QString id;            // Rule identifier
    QString pattern;       // Regex pattern (QRegularExpression syntax)
    QStringList captures;  // Named capture groups to extract
    QString suggestion;    // Human-readable fix suggestion (Chinese)
    QString title;         // Short human-readable rule name (Chinese)
};

class CrashDetector : public QObject {
    Q_OBJECT

public:
    explicit CrashDetector(QObject* parent = nullptr);

    /// Scan for the latest crash report in the game directory.
    /// Returns a CrashReport with isValid=true if found.
    CrashReport scanLatestCrash(const QString& gameDir);

    /// Full crash analysis (v2):
    ///   1. Collect all relevant logs (crash reports + hs_err + latest.log + debug.log)
    ///   2. Run the rule engine over concatenated log text
    ///   3. Extract stack-trace keywords → suspected mods
    ///   4. Build suggestions from matched rules
    ///   5. Export collected logs + write report document if analysis is too long
    /// latestOutput: last N lines captured from the game process (optional)
    CrashReport analyzeCrash(const QString& gameDir,
                             const QStringList& latestOutput = {},
                             const QString& launcherLogPath = {});

    /// Export all relevant logs (crash reports, hs_err, latest.log, debug.log,
    /// launcher log) into exportDir. Returns the export dir, or "" on failure.
    QString exportLogs(const QString& gameDir,
                       const QString& exportDir,
                       const QString& launcherLogPath = {});

    /// Write a full analysis report document (markdown) to reportFilePath.
    /// Returns true on success.
    bool writeReport(const CrashReport& r, const QString& reportFilePath);

    /// The rule engine (static, reusable).
    static QList<CrashRule> rules();
    /// Run all rules against logText, return matched rule ids.
    static QStringList matchRules(const QString& logText);
    /// Extract stack-trace keywords from a crash report (主流启动器-style blacklist filter).
    static QStringList findKeywordsFromCrashReport(const QString& crashReport);

private:
    /// Parse a hs_err_pid*.log (JVM crash) file
    CrashReport parseJvmCrash(const QString& filePath);
    /// Parse a crash-reports/crash-*.txt (Minecraft crash) file
    CrashReport parseMinecraftCrash(const QString& filePath);
    /// Extract suspected mod names from crash report lines
    QStringList extractSuspectedMods(const QStringList& lines);
    /// Collect all log files relevant to a crash into a sorted list (newest first)
    QStringList collectLogFiles(const QString& gameDir);
    /// Build a suggestion list from matched rule ids
    static QStringList suggestionsForRules(const QStringList& matchedRuleIds);
};

} // namespace ShadowLauncher
