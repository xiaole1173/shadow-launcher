// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#pragma once

#include <QObject>
#include <QString>

namespace ShadowLauncher {

class AppBackend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(QString gameDir READ gameDir NOTIFY gameDirChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool devMode READ devMode NOTIFY devModeChanged)

public:
    explicit AppBackend(QObject *parent = nullptr);

    QString appVersion() const { return QStringLiteral(SHADOW_DISPLAY_VERSION); }
    QString appName() const { return QStringLiteral("Shadow Launcher"); }
    static constexpr const char* AGREEMENT_VERSION = "1.0";
    QString dataDir() const { return m_dataDir; }
    QString gameDir() const { return m_gameDir; }
    QString theme() const { return m_theme; }
    QString jvmArgs() const { return m_jvmArgs; }
    void setJvmArgs(const QString& args) { m_jvmArgs = args; }
    QString gameArgs() const { return m_gameArgs; }
    void setGameArgs(const QString& args) { m_gameArgs = args; }
    bool highPerfGpu() const { return m_highPerfGpu; }
    void setHighPerfGpu(bool v) { m_highPerfGpu = v; }
    bool devMode() const { return m_devMode; }

    void setTheme(const QString &theme);
    void setGameDir(const QString &dir);

    Q_INVOKABLE QString resolvePath(const QString &relativePath) const;
    Q_INVOKABLE void setDevMode(bool enabled);

signals:
    void themeChanged();
    void gameDirChanged();
    void devModeChanged();
    void logMessage(const QString &msg);

private:
    QString m_dataDir;
    QString m_gameDir;
    QString m_theme = QStringLiteral("dark");
    QString m_jvmArgs;
    QString m_gameArgs;
    bool m_highPerfGpu = false;
    bool m_devMode = false;
};

} // namespace ShadowLauncher
