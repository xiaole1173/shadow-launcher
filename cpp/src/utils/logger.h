// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow
#pragma once

#include <QLoggingCategory>
#include <QString>

/// Declare per-module QLoggingCategory for structured logging.
/// Usage: qCDebug(logApp) << "message";
///        qCInfo(logApp)  << "message";
///        qCWarning(logApp) << "message";
///        qCCritical(logApp) << "message";

namespace ShadowLauncher {

// ── Module categories ──
Q_DECLARE_LOGGING_CATEGORY(logApp)
Q_DECLARE_LOGGING_CATEGORY(logAccount)
Q_DECLARE_LOGGING_CATEGORY(logVersion)
Q_DECLARE_LOGGING_CATEGORY(logLaunch)
Q_DECLARE_LOGGING_CATEGORY(logDownload)
Q_DECLARE_LOGGING_CATEGORY(logMgr)

/// One-time logger initialisation. Must be called once at process start
/// before any qCDebug/qCInfo/qCWarning/qCCritical calls.
/// Creates: {exeDir}/logs/shadow_launcher_{yyyy-MM-dd}.log
void initLogger(const QString& exeDir);

/// Install a Qt message handler that writes to both the log file and the
/// debug console (qDebug/qWarning/qCritical). Called by initLogger().
void installFileLogger(const QString& exeDir);

} // namespace ShadowLauncher
