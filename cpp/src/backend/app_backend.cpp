#include "app_backend.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ShadowLauncher {

// ────────────────────────────────────────────────────────────
// Constructor
// ────────────────────────────────────────────────────────────

AppBackend::AppBackend(QObject *parent)
    : QObject(parent)
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/shadow");
    m_gameDir = m_dataDir + QStringLiteral("/minecraft");

    // Ensure data directories exist
    QDir().mkpath(m_dataDir);
    QDir().mkpath(m_gameDir);
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
