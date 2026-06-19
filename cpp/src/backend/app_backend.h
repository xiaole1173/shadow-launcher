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

    QString appVersion() const { return QStringLiteral("1.0.0"); }
    QString appName() const { return QStringLiteral("Shadow Launcher"); }
    QString dataDir() const { return m_dataDir; }
    QString gameDir() const { return m_gameDir; }
    QString theme() const { return m_theme; }
    QString jvmArgs() const { return m_jvmArgs; }
    void setJvmArgs(const QString& args) { m_jvmArgs = args; }
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
    bool m_devMode = false;
};

} // namespace ShadowLauncher
