// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

// ── Module categories (short, readable names) ──
Q_LOGGING_CATEGORY(logApp,      "Shadow.App")
Q_LOGGING_CATEGORY(logAccount,  "Shadow.Account")
Q_LOGGING_CATEGORY(logVersion,  "Shadow.Version")
Q_LOGGING_CATEGORY(logLaunch,   "Shadow.Launch")
Q_LOGGING_CATEGORY(logDownload, "Shadow.Download")
Q_LOGGING_CATEGORY(logMod,      "Shadow.Mod")       // 模组/光影/资源包管理
Q_LOGGING_CATEGORY(logLoader,   "Shadow.Loader")    // Forge/Fabric/NeoForge 安装
Q_LOGGING_CATEGORY(logJava,     "Shadow.Java")      // Java 运行时检测
Q_LOGGING_CATEGORY(logNet,      "Shadow.Net")       // 联机网络
Q_LOGGING_CATEGORY(logUI,       "Shadow.UI")        // 用户界面操作
// Keep legacy 'logMgr' for backward compat — maps to same "Shadow.Mod"
Q_LOGGING_CATEGORY(logMgr,      "Shadow.Mod")

// ── Custom message handler ──

static QRecursiveMutex g_logMutex;
static QFile* g_logFile = nullptr;
static QString g_logDir;
static QString g_currentLogDate;

static QString logFilePath(const QString& exeDir)
{
    const QString date = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    return exeDir + QStringLiteral("/logs/shadow_launcher_") + date + QStringLiteral(".log");
}

static void ensureLogDir(const QString& exeDir)
{
    QDir dir(exeDir + QStringLiteral("/logs"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

static void rotateLogFile(const QString& exeDir)
{
    const QString today = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
    if (g_currentLogDate == today && g_logFile && g_logFile->isOpen())
        return;

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
        QTextStream stream(g_logFile);
        stream.setEncoding(QStringConverter::Utf8);
        stream << QStringLiteral("——  Shadow Launcher  ——") << Qt::endl;
        stream.flush();
    } else {
        delete g_logFile;
        g_logFile = nullptr;
    }
}

static void shadowMessageHandler(QtMsgType type,
                                  const QMessageLogContext& ctx,
                                  const QString& msg)
{
    // ── Production: skip debug messages entirely ──
    if (type == QtDebugMsg) return;

    // ── Filter QML engine runtime noise (binding loops, non-existent properties) ──
    if (type == QtWarningMsg) {
        QString cat = ctx.category ? QString::fromLatin1(ctx.category) : QString();
        if (cat == QStringLiteral("qml") || cat == QStringLiteral("default"))
            return;
    }

    // ── SHADOW_DISABLE_FILE_LOG: skip file I/O entirely (perf testing) ──
    static int disableFileLog = -1;
    if (disableFileLog < 0)
        disableFileLog = qEnvironmentVariableIsSet("SHADOW_DISABLE_FILE_LOG") ? 1 : 0;

    QMutexLocker locker(&g_logMutex);

    // ── Log format: "HH:mm:ss.zzz L [Module] Message" ──
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));

    QString levelStr;
    switch (type) {
    case QtInfoMsg:     levelStr = QStringLiteral("I"); break;
    case QtWarningMsg:  levelStr = QStringLiteral("W"); break;
    case QtCriticalMsg: levelStr = QStringLiteral("E"); break;
    case QtFatalMsg:    levelStr = QStringLiteral("E"); break;
    default:            levelStr = QStringLiteral("?"); break;
    }

    QString module = ctx.category ? QString::fromLatin1(ctx.category) : QStringLiteral("-");
    // Strip "Shadow." prefix for cleaner display: "Shadow.App" → "App"
    if (module.startsWith(QStringLiteral("Shadow.")))
        module = module.mid(7);

    // Strip class name prefix from messages that leak via Qt internals
    QString cleanMsg = msg;
    if (cleanMsg.startsWith(QStringLiteral("ShadowLauncher::")))
        cleanMsg = cleanMsg.mid(16);

    const QString formatted =
        QStringLiteral("%1 %2 [%3] %4")
            .arg(timestamp, levelStr, module, cleanMsg);

    // ── Write to file (skip if SHADOW_DISABLE_FILE_LOG=1) ──
    if (!disableFileLog) {
        rotateLogFile(g_logDir);
        if (g_logFile && g_logFile->isOpen()) {
            QTextStream stream(g_logFile);
            stream.setEncoding(QStringConverter::Utf8);
            stream << formatted << Qt::endl;
            stream.flush();
        }
    }

    // ── Write to debug console (skip if SHADOW_DISABLE_FILE_LOG=1) ──
    if (!disableFileLog) {
        QByteArray utf8 = formatted.toUtf8();
        utf8.append('\n');
#ifdef Q_OS_WIN
        OutputDebugStringA(utf8.constData());
#else
        fwrite(utf8.constData(), 1, utf8.size(), stderr);
#endif
    }

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

    g_logDir = exeDir;
    ensureLogDir(exeDir);

    g_currentLogDate.clear();
    rotateLogFile(exeDir);

    qInstallMessageHandler(shadowMessageHandler);

    // Enable all categories at Info level; disable Debug
    QLoggingCategory::setFilterRules(
        QStringLiteral(
            "Shadow.*.debug=false\n"
            "Shadow.*.info=true\n"
            "qml.debug=false\n"
            "qml.info=false\n"
            "default.debug=false\n"));

    qCInfo(logApp) << QStringLiteral("日志系统已就绪");
}

bool suppressUrlLog()
{
    static int cached = -1;
    if (cached < 0) {
        cached = qEnvironmentVariableIsSet("SHADOW_SUPPRESS_URL_LOG") ? 1 : 0;
        if (cached == 1)
            qCInfo(logApp) << QStringLiteral("[SUPPRESS] URL日志已禁用 (SHADOW_SUPPRESS_URL_LOG=1)");
    }
    return cached == 1;
}

} // namespace ShadowLauncher
