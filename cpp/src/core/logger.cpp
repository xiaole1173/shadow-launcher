// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "logger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QTextStream>

namespace ShadowLauncher {

// ============================================================
// Singleton accessor (thread-safe via C++11 static init)
// ============================================================

Logger& Logger::instance()
{
    static Logger inst;
    return inst;
}

// ============================================================
// Public API
// ============================================================

void Logger::init(const QString& dataDir)
{
    Logger& inst = instance();
    QMutexLocker locker(&inst.m_mutex);

    if (inst.m_initialized)
        return;

    inst.m_dataDir = dataDir;
    inst.ensureDir();
    inst.m_initialized = true;

    qDebug().noquote()
        << "[Logger] Initialized, log dir:" << (inst.m_dataDir + "/logs");
}

void Logger::info(const QString& msg)
{
    Logger& inst = instance();
    QMutexLocker locker(&inst.m_mutex);

    if (!inst.m_initialized) {
        qDebug().noquote() << "[Logger:uninit] [INFO]" << msg;
        return;
    }
    inst.writeLog(QStringLiteral("INFO"), msg);
}

void Logger::warn(const QString& msg)
{
    Logger& inst = instance();
    QMutexLocker locker(&inst.m_mutex);

    if (!inst.m_initialized) {
        qWarning().noquote() << "[Logger:uninit] [WARN]" << msg;
        return;
    }
    inst.writeLog(QStringLiteral("WARN"), msg);
}

void Logger::error(const QString& msg)
{
    Logger& inst = instance();
    QMutexLocker locker(&inst.m_mutex);

    if (!inst.m_initialized) {
        qCritical().noquote() << "[Logger:uninit] [ERROR]" << msg;
        return;
    }
    inst.writeLog(QStringLiteral("ERROR"), msg);
}

// ============================================================
// Internal helpers
// ============================================================

void Logger::ensureDir()
{
    const QString logDir = m_dataDir + QStringLiteral("/logs");
    QDir dir;
    if (!dir.mkpath(logDir)) {
        qWarning().noquote()
            << "[Logger] Failed to create log directory:" << logDir;
    }
}

QString Logger::logFilePath() const
{
    const QString date = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    return m_dataDir + QStringLiteral("/logs/shadow_") + date + QStringLiteral(".log");
}

void Logger::writeLog(const QString& level, const QString& msg)
{
    // Build formatted log line
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString line =
        QStringLiteral("[%1] [%2] %3").arg(timestamp, level, msg);

    // Ensure directory exists (handle manual deletion at runtime)
    ensureDir();

    // Write to file (append mode, close immediately)
    QFile file(logFilePath());
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << line << Qt::endl;
        stream.flush();
        file.close();
    }

    // Mirror to console via Qt logging facilities
    if (level == QStringLiteral("INFO")) {
        qDebug().noquote() << line;
    } else if (level == QStringLiteral("WARN")) {
        qWarning().noquote() << line;
    } else {
        qCritical().noquote() << line;
    }
}

} // namespace ShadowLauncher
