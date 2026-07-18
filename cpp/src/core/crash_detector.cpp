// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "crash_detector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

#include "../utils/logger.h"

namespace ShadowLauncher {

CrashDetector::CrashDetector(QObject* parent)
    : QObject(parent)
{
}

CrashReport CrashDetector::scanLatestCrash(const QString& gameDir)
{
    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] 开始扫描 目录=%1").arg(gameDir);

    // ── Check crash-reports/ folder for Minecraft crash reports ──
    {
        QDir crashDir(gameDir + QStringLiteral("/crash-reports"));
        if (crashDir.exists()) {
            QStringList filters;
            filters << QStringLiteral("crash-*.txt");
            auto entries = crashDir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot, QDir::Time);

            if (!entries.isEmpty()) {
                // Take the newest crash report
                const QFileInfo& newest = entries.first();
                qCInfo(logLaunch) << QStringLiteral("[崩溃检测] 找到MC崩溃报告 文件=%1").arg(newest.fileName());
                CrashReport r = parseMinecraftCrash(newest.absoluteFilePath());
                if (r.isValid) return r;
            }
        }
    }

    // ── Check game dir root for hs_err_pid*.log (JVM crash) ──
    {
        QDir rootDir(gameDir);
        QStringList filters;
        filters << QStringLiteral("hs_err_pid*.log");
        auto entries = rootDir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot, QDir::Time);

        if (!entries.isEmpty()) {
            const QFileInfo& newest = entries.first();
            qCInfo(logLaunch) << QStringLiteral("[崩溃检测] 找到JVM崩溃报告 文件=%1").arg(newest.fileName());
            CrashReport r = parseJvmCrash(newest.absoluteFilePath());
            if (r.isValid) return r;
        }
    }

    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] 未找到崩溃报告");
    return CrashReport{};
}

CrashReport CrashDetector::parseJvmCrash(const QString& filePath)
{
    CrashReport r;
    r.type = QStringLiteral("jvm");
    r.filePath = filePath;
    r.timestamp = QFileInfo(filePath).lastModified();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return r;

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    f.close();

    // Parse JVM crash report — it has a well-defined format
    // First line: "# A fatal error has been detected by the Java Runtime Environment:"
    // Second line: "#  EXCEPTION_ACCESS_VIOLATION (0xc0000005) at pc=..."
    // Third line: "# Internal Error (...)"
    // Or: "#  SIGSEGV (0xb) at pc=..."
    for (int i = 0; i < lines.size() && i < 20; ++i) {
        const QString& line = lines[i];

        if (line.startsWith(QLatin1String("#"))) {
            // Extract exception type
            static const QRegularExpression sigRe(
                QStringLiteral(R"(#\s+(SIG\w+|EXCEPTION_\w+)\s*\()"),
                QRegularExpression::CaseInsensitiveOption);
            auto m = sigRe.match(line);
            if (m.hasMatch()) {
                r.reason = m.captured(1);
                r.description = tr("JVM 崩溃: %1").arg(r.reason);
                break;
            }

            // "Internal Error" type
            static const QRegularExpression internalRe(
                QStringLiteral(R"(#\s+Internal\s+Error\s*\(([^)]+)\))"),
                QRegularExpression::CaseInsensitiveOption);
            m = internalRe.match(line);
            if (m.hasMatch()) {
                r.reason = m.captured(1).trimmed();
                r.description = tr("JVM 内部错误: %1").arg(r.reason);
                break;
            }
        }
    }

    if (r.reason.isEmpty()) {
        r.reason = QStringLiteral("Unknown JVM crash");
        r.description = tr("JVM 异常崩溃，详见崩溃报告");
    }

    r.suspectedMods = extractSuspectedMods(lines);
    r.isValid = true;

    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] JVM崩溃解析完成 原因=%1").arg(r.reason);
    return r;
}

CrashReport CrashDetector::parseMinecraftCrash(const QString& filePath)
{
    CrashReport r;
    r.type = QStringLiteral("minecraft");
    r.filePath = filePath;
    r.timestamp = QFileInfo(filePath).lastModified();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return r;

    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    f.close();

    // Minecraft crash report format:
    // ---- Minecraft Crash Report ----
    // // Description line(s)
    // Time: YYYY-MM-DD HH:MM:SS
    // Description: <reason>
    //
    // A detailed walkthrough of the error...
    // ...
    // -- Mod List: --
    // | Name | Version | ... |

    QString description;
    QString reason;

    for (const QString& line : lines) {
        // Extract Description line
        if (line.startsWith(QLatin1String("Description: "))) {
            reason = line.mid(13).trimmed();
        }

        // Extract the // description comment lines
        if (line.startsWith(QLatin1String("// ")) || line.startsWith(QLatin1String("//"))) {
 QString descLine = line.mid(3).trimmed();
            if (!descLine.isEmpty() && descLine != QLatin1String("Minecraft Crash Report ----")
                && !descLine.startsWith(QLatin1String("Description:"))) {
                if (description.isEmpty())
                    description = descLine;
                else if (description.length() < 200)
                    description += QStringLiteral(" | ") + descLine;
            }
        }

        // Stop at stack trace
        if (line.startsWith(QLatin1String("A detailed walkthrough")) ||
            line.startsWith(QLatin1String("-- Head --"))) {
            break;
        }
    }

    if (reason.isEmpty()) {
        reason = description.isEmpty()
                     ? QStringLiteral("Unknown Minecraft crash")
                     : description;
    }

    if (description.isEmpty()) {
        description = reason;
    }

    r.reason = reason;
    r.description = description.left(200);  // Truncate for display
    r.suspectedMods = extractSuspectedMods(lines);
    r.isValid = true;

    qCInfo(logLaunch) << QStringLiteral("[崩溃检测] MC崩溃解析完成 原因=%1").arg(r.reason);
    return r;
}

QStringList CrashDetector::extractSuspectedMods(const QStringList& lines)
{
    QStringList mods;
    bool readingMods = false;

    for (const QString& line : lines) {
        // Minecraft crash reports have "-- Mods --" or "Mod List:" section
        if (line.contains(QLatin1String("Mod List:")) ||
            line.contains(QLatin1String("-- Mods --")) ||
            line.contains(QLatin1String("-- All Mods --"))) {
            readingMods = true;
            continue;
        }

        if (readingMods) {
            // Mod list ends at empty line or next section
            if (line.trimmed().isEmpty() || line.startsWith(QLatin1String("-- "))) {
                readingMods = false;
                break;
            }

            // Table format: "| ModName | Version | ... |"
            if (line.contains(QLatin1String("|"))) {
                QStringList parts = line.split(QLatin1Char('|'), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QString modName = parts[0].trimmed();
                    if (!modName.isEmpty() && modName != QLatin1String("Name")
                        && modName != QLatin1String("State") && modName != QLatin1String("LCH"))
                        mods.append(modName);
                }
            }
        }

        // JVM crash reports have "Java frames:" after which mod names appear
        if (readingMods) continue;  // Already in mod section

        // If no mod table found, search for mod-like patterns in stack trace
        static const QRegularExpression modRe(QStringLiteral(R"(\[([\w-]+(?:-[\d.]+)?\.jar)\])"));
        auto it = modRe.globalMatch(line);
        while (it.hasNext()) {
            auto m = it.next();
        QString jarName = m.captured(1);
            // Extract mod name from jar filename
            jarName.remove(QRegularExpression(QStringLiteral(R"(-\d+[.\d]*.*)")));  // Remove version suffix
            if (!mods.contains(jarName))
                mods.append(jarName);
        }
    }

    return mods;
}

} // namespace ShadowLauncher
