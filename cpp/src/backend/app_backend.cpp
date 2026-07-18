// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "app_backend.h"
#include "../utils/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>

namespace ShadowLauncher {

// ────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────

AppBackend::AppBackend(QObject *parent)
    : QObject(parent)
{
    // Data dir: keep in AppData for launcher settings/cache
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/shadow");

    // Game dir: create .minecraft next to the launcher EXE
    QString exeDir = QCoreApplication::applicationDirPath();
    m_gameDir = exeDir + QStringLiteral("/.minecraft");

    // Defer directory creation to event loop — avoid blocking construction
    QTimer::singleShot(0, this, [this]() {
        QDir().mkpath(m_dataDir);
        QDir().mkpath(m_gameDir);
        QDir().mkpath(m_gameDir + QStringLiteral("/versions"));
        QDir().mkpath(m_gameDir + QStringLiteral("/libraries"));
        QDir().mkpath(m_gameDir + QStringLiteral("/assets"));
        QDir().mkpath(m_gameDir + QStringLiteral("/mods"));
        qCInfo(logApp) << QStringLiteral("应用模块已初始化") << QStringLiteral(" 数据目录:") << m_dataDir
                       << QStringLiteral(" 游戏目录:") << m_gameDir;
        emit logMessage(tr("游戏目录: %1").arg(m_gameDir));
    });
}

// ────────────────────────────────────────────────────────────
// Theme
// ────────────────────────────────────────────────────────────

void AppBackend::setTheme(const QString &theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        emit themeChanged();
        emit logMessage(QStringLiteral("Theme changed to: ") + theme);
    }
}

// ────────────────────────────────────────────────────────────
// Game Directory
// ────────────────────────────────────────────────────────────

void AppBackend::setGameDir(const QString &dir)
{
    if (m_gameDir != dir) {
        m_gameDir = dir;
        emit gameDirChanged();
        emit logMessage(QStringLiteral("Game directory changed to: ") + dir);
    }
}

// ────────────────────────────────────────────────────────────
// Path Resolution
// ────────────────────────────────────────────────────────────

QString AppBackend::resolvePath(const QString &relativePath) const
{
    return QDir(m_gameDir).absoluteFilePath(relativePath);
}

// ────────────────────────────────────────────────────────────
// Dev Mode
// ────────────────────────────────────────────────────────────

void AppBackend::setDevMode(bool enabled)
{
    if (m_devMode != enabled) {
        m_devMode = enabled;
        emit devModeChanged();
        emit logMessage(
            enabled ? QStringLiteral("Dev mode enabled")
                    : QStringLiteral("Dev mode disabled"));
    }
}

} // namespace ShadowLauncher
