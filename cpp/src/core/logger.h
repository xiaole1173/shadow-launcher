// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once
#include <QMutex>
#include <QString>

namespace ShadowLauncher {

class Logger {
public:
    static void init(const QString& dataDir);
    static void info(const QString& msg);
    static void warn(const QString& msg);
    static void error(const QString& msg);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger() = default;

    static Logger& instance();
    void writeLog(const QString& level, const QString& msg);
    QString logFilePath() const;
    void ensureDir();

    QMutex m_mutex;
    QString m_dataDir;
    bool m_initialized = false;
};

} // namespace ShadowLauncher
