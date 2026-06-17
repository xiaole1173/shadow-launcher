#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QTimer>

namespace ShadowLauncher {

class SettingsBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString javaPath READ javaPath WRITE setJavaPath NOTIFY javaPathChanged)
    Q_PROPERTY(QString javaVersion READ javaVersion NOTIFY javaPathChanged)
    Q_PROPERTY(int javaMajor READ javaMajor NOTIFY javaPathChanged)
    Q_PROPERTY(int minMemoryMB READ minMemoryMB NOTIFY memorySettingsChanged)
    Q_PROPERTY(int maxMemoryMB READ maxMemoryMB NOTIFY memorySettingsChanged)
    Q_PROPERTY(bool closeAfterLaunch READ closeAfterLaunch NOTIFY generalSettingsChanged)
    Q_PROPERTY(bool javaReady READ isJavaReady NOTIFY javaPathChanged)

public:
    explicit SettingsBackend(QObject* parent = nullptr);

    QString javaPath() const { return m_javaPath; }
    QString javaVersion() const { return m_javaVersion; }
    int javaMajor() const { return m_javaMajor; }
    int minMemoryMB() const { return m_minMemoryMB; }
    int maxMemoryMB() const { return m_maxMemoryMB; }
    bool closeAfterLaunch() const { return m_closeAfterLaunch; }
    bool isJavaReady() const { return m_javaReady; }

    void setJavaPath(const QString& path);

    // ---- Slots ----
    Q_INVOKABLE QVariantList scanJavaInstallations();
    Q_INVOKABLE QString autoSelectJava();
    Q_INVOKABLE QString detectJava();         // QML alias
    Q_INVOKABLE QVariantMap getMemoryStatus();
    Q_INVOKABLE void setMinMemory(int mb);
    Q_INVOKABLE void setMaxMemory(int mb);
    Q_INVOKABLE QVariantList availableJavaList();
    Q_INVOKABLE void selectJavaByIndex(int index);
    Q_INVOKABLE QString openJavaFileDialog();
    Q_INVOKABLE QString browseJava();          // QML alias
    Q_INVOKABLE void setIsolationEnabled(bool enabled);
    Q_INVOKABLE void openGameDir();
    Q_INVOKABLE void openVersionDir(const QString& versionId);
    Q_INVOKABLE void deleteVersion(const QString& versionId);
    Q_INVOKABLE void openPath(const QString& path);
    Q_INVOKABLE void setCloseAfterLaunch(bool enabled);

signals:
    void javaPathChanged();
    void javaReadyChanged();
    void memorySettingsChanged();
    void generalSettingsChanged();
    void isolationChanged();
    void logMessage(const QString& msg);

private:
    struct JavaInfo {
        QString path;
        QString version;
        int major = 0;
    };

    void loadSettings();
    void saveSettings();
    void doAutoDetect();
    const QVector<JavaInfo>& cachedJavaList();

    QVector<JavaInfo> findAllJava();
    JavaInfo getJavaInfo(const QString& exePath);
    int parseMajorVersion(const QString& versionStr);
    bool tryAddJavaResult(const QString& exePath, QSet<QString>& seenBinDirs,
                          QVector<JavaInfo>& out);
    QString findJavaInDir(const QString& dirPath);
    QString findJavaOnPath();

    QString m_javaPath;
    QString m_javaVersion;
    int m_javaMajor = 0;
    int m_minMemoryMB = 512;
    int m_maxMemoryMB = 2048;
    bool m_closeAfterLaunch = false;
    bool m_javaReady = false;
    QString m_gameDir;

    // Cache for Java scan results (expensive operation)
    QVector<JavaInfo> m_cachedJavaList;
    bool m_javaCacheValid = false;
};

} // namespace ShadowLauncher
