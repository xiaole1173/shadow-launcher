#include "logger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QMutex>
#include <QTextStream>
#include <QtMessageHandler>
#include <cstdio>

namespace ShadowLauncher {

// ── Define module categories ──
Q_LOGGING_CATEGORY(logApp,      "Shadow.App")
Q_LOGGING_CATEGORY(logAccount,  "Shadow.Account")
Q_LOGGING_CATEGORY(logVersion,  "Shadow.Version")
Q_LOGGING_CATEGORY(logLaunch,   "Shadow.Launch")
Q_LOGGING_CATEGORY(logDownload, "Shadow.Download")
Q_LOGGING_CATEGORY(logMgr,      "Shadow.Mgr")

// ── Custom message handler ──

// Thread-safe guard for the custom handler
static QMutex g_logMutex;
static QFile* g_logFile = nullptr;
static QString g_logDir;
static QString g_currentLogDate;

/// Get today's log file path based on exeDir.
static QString logFilePath(const QString& exeDir)
{
    const QString date = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    return exeDir + QStringLiteral("/logs/shadow_launcher_") + date + QStringLiteral(".log");
}

/// Ensure logs directory exists.
static void ensureLogDir(const QString& exeDir)
{
    QDir dir(exeDir + QStringLiteral("/logs"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

/// Rotate log file when date changes.
static void rotateLogFile(const QString& exeDir)
{
    const QString today = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    if (g_currentLogDate == today && g_logFile && g_logFile->isOpen()) {
        return; // Same day, same file
    }

    // Close old file
    if (g_logFile) {
        if (g_logFile->isOpen()) g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }

    g_currentLogDate = today;
    ensureLogDir(exeDir);

    const QString path = logFilePath(exeDir);
    g_logFile = new QFile(path);
    if (g_logFile->open(QIODevice::Append | QIODevice::Text)) {
        // Write a separator for new session
        QTextStream stream(g_logFile);
        stream << Qt::endl;
        stream << QStringLiteral("=== Shadow Launcher Session ===") << Qt::endl;
        stream.flush();
    } else {
        delete g_logFile;
        g_logFile = nullptr;
    }
}

/// Custom Qt message handler: formatted timestamp + level + category + message.
static void shadowMessageHandler(QtMsgType type,
                                  const QMessageLogContext& ctx,
                                  const QString& msg)
{
    QMutexLocker locker(&g_logMutex);

    // ── Format ──
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

    QString levelStr;
    switch (type) {
    case QtDebugMsg:    levelStr = QStringLiteral("DEBUG"); break;
    case QtInfoMsg:     levelStr = QStringLiteral("INFO");  break;
    case QtWarningMsg:  levelStr = QStringLiteral("WARN");  break;
    case QtCriticalMsg: levelStr = QStringLiteral("ERROR"); break;
    case QtFatalMsg:    levelStr = QStringLiteral("FATAL"); break;
    }

    QString category = ctx.category ? QString::fromLatin1(ctx.category) : QStringLiteral("-");

    // Strip "Shadow." prefix for shorter module names
    QString module = category;
    if (category.startsWith(QStringLiteral("Shadow."))) {
        module = category.mid(7); // "App", "Account", "Version"...
    }

    // Remove leading "ShadowLauncher::" from message if present (from function names)
    QString cleanMsg = msg;
    if (cleanMsg.startsWith(QStringLiteral("ShadowLauncher::"))) {
        cleanMsg = cleanMsg.mid(16);
    }

    const QString formatted =
        QStringLiteral("[%1] [%2] [%3] %4")
            .arg(timestamp, levelStr, module, cleanMsg);

    // ── Rotate log file if needed ──
    rotateLogFile(g_logDir);

    // ── Write to file ──
    if (g_logFile && g_logFile->isOpen()) {
        QTextStream stream(g_logFile);
        stream << formatted << Qt::endl;
        stream.flush();
    }

    // ── Write to debug console ──
    // Use the original Qt logging facilities for console output
    // but with simplified format (no timestamp needed for debug console)
    {
        QByteArray utf8 = formatted.toUtf8();
        utf8.append('\n');
#ifdef Q_OS_WIN
        OutputDebugStringA(utf8.constData());
#else
        fwrite(utf8.constData(), 1, utf8.size(), stderr);
#endif
    }

    // ── Fatal ──
    if (type == QtFatalMsg) {
        if (g_logFile) {
            if (g_logFile->isOpen()) g_logFile->close();
            delete g_logFile;
            g_logFile = nullptr;
        }
        abort();
    }
}

// ============================================================
// Public API
// ============================================================

void initLogger(const QString& exeDir)
{
    installFileLogger(exeDir);
}

void installFileLogger(const QString& exeDir)
{
    QMutexLocker locker(&g_logMutex);

    // Store directory for rotation
    g_logDir = exeDir;

    // Ensure logs directory exists
    ensureLogDir(exeDir);

    // Open initial log file
    g_currentLogDate.clear(); // force rotation
    rotateLogFile(exeDir);

    // Install custom message handler
    qInstallMessageHandler(shadowMessageHandler);

    // Enable all our categories by default
    QLoggingCategory::setFilterRules(
        QStringLiteral("Shadow.*.debug=true\n"
                       "Shadow.*.info=true\n"
                       "Shadow.*.warning=true\n"
                       "Shadow.*.critical=true"));

    qCInfo(logApp) << "Logger initialized, log path:" << logFilePath(exeDir);
}

} // namespace ShadowLauncher
