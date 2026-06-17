#include "settings_backend.h"

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace ShadowLauncher {

// ============================================================
// Constants
// ============================================================

static const QString MINECRAFT_DIR =
    QDir::homePath() + QStringLiteral("/.shadow/minecraft");

static const char* COMMON_JAVA_DIRS[] = {
    "C:\\Program Files\\Java",
    "C:\\Program Files\\Eclipse Adoptium",
    "C:\\Program Files\\Microsoft",
    "C:\\Program Files\\Eclipse Foundation",
    "C:\\Program Files\\Amazon Corretto",
    "C:\\Program Files\\Azul",
    "C:\\Program Files\\Zulu",
    "C:\\Program Files\\BellSoft",
    "C:\\Program Files (x86)\\Java",
    "C:\\Program Files (x86)\\Eclipse Adoptium",
    "C:\\Program Files (x86)\\Microsoft",
    "C:\\Program Files (x86)\\Minecraft Launcher\\runtime",
    "C:\\XboxGames\\Minecraft Launcher\\Content\\runtime",
    "%USERPROFILE%\\scoop\\apps",
    "%LOCALAPPDATA%\\Programs",
};

// ============================================================
// Constructor
// ============================================================

SettingsBackend::SettingsBackend(QObject* parent)
    : QObject(parent)
    , m_minMemoryMB(512)
    , m_maxMemoryMB(2048)
    , m_closeAfterLaunch(false)
    , m_gameDir(MINECRAFT_DIR)
{
    QDir().mkpath(m_gameDir + QStringLiteral("/versions"));
    QDir().mkpath(m_gameDir + QStringLiteral("/libraries"));
    QDir().mkpath(m_gameDir + QStringLiteral("/assets"));

    // Load persisted settings
    loadSettings();

    // Auto-detect Java asynchronously (defer to event loop)
    // This avoids blocking the UI during startup
    QTimer::singleShot(100, this, &SettingsBackend::doAutoDetect);
}

// ============================================================
// Settings persistence (QSettings)
// ============================================================

void SettingsBackend::loadSettings()
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());

    m_javaPath = s.value(QStringLiteral("java/path"), QString()).toString();
    m_javaVersion = s.value(QStringLiteral("java/version"), QString()).toString();
    m_javaMajor = s.value(QStringLiteral("java/major"), 0).toInt();
    m_javaReady = (m_javaMajor > 0);

    m_minMemoryMB = s.value(QStringLiteral("memory/minMB"), 512).toInt();
    m_maxMemoryMB = s.value(QStringLiteral("memory/maxMB"), 2048).toInt();
    m_closeAfterLaunch = s.value(QStringLiteral("general/closeAfterLaunch"), false).toBool();
}

void SettingsBackend::saveSettings()
{
    QSettings s(QCoreApplication::organizationName(),
                QCoreApplication::applicationName());

    s.setValue(QStringLiteral("java/path"), m_javaPath);
    s.setValue(QStringLiteral("java/version"), m_javaVersion);
    s.setValue(QStringLiteral("java/major"), m_javaMajor);

    s.setValue(QStringLiteral("memory/minMB"), m_minMemoryMB);
    s.setValue(QStringLiteral("memory/maxMB"), m_maxMemoryMB);
    s.setValue(QStringLiteral("general/closeAfterLaunch"), m_closeAfterLaunch);
}

// ============================================================
// Async Java auto-detect (called via QTimer after constructor)
// ============================================================

void SettingsBackend::doAutoDetect()
{
    // Skip if we already have a valid Java from saved settings
    if (m_javaReady && QFileInfo::exists(m_javaPath)) {
        emit logMessage(QStringLiteral("Java 已配置: %1 (版本 %2)")
                            .arg(m_javaPath, m_javaVersion));
        return;
    }

    // Run auto-detection (will also populate cache lazily)
    emit logMessage(QStringLiteral("正在自动检测 Java..."));
    QString path = autoSelectJava();

    if (path.isEmpty()) {
        emit logMessage(QStringLiteral("⚠ 未找到 Java，请在设置中手动指定"));
    }

    saveSettings();
}

// ============================================================
// Properties
// ============================================================

void SettingsBackend::setJavaPath(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.isFile()) {
        emit logMessage(QStringLiteral("❌ 路径不存在: %1").arg(path));
        return;
    }
    JavaInfo info = getJavaInfo(path);
    if (info.major == 0) {
        emit logMessage(QStringLiteral("❌ 无法获取 Java 版本: %1").arg(path));
        return;
    }
    m_javaPath = info.path;
    m_javaVersion = info.version;
    m_javaMajor = info.major;

    if (!m_javaReady) {
        m_javaReady = true;
        emit javaReadyChanged();
    }

    saveSettings();
    emit logMessage(QStringLiteral("✅ Java 已设置: %1 (版本 %2)")
                        .arg(path, info.version));
    emit javaPathChanged();
}

// ============================================================
// Java Detection Slots
// ============================================================

const QVector<SettingsBackend::JavaInfo>& SettingsBackend::cachedJavaList()
{
    if (!m_javaCacheValid) {
        m_cachedJavaList = findAllJava();
        std::sort(m_cachedJavaList.begin(), m_cachedJavaList.end(),
                  [](const JavaInfo& a, const JavaInfo& b) {
                      return a.major > b.major;
                  });
        m_javaCacheValid = true;
    }
    return m_cachedJavaList;
}

QVariantList SettingsBackend::scanJavaInstallations()
{
    emit logMessage(QStringLiteral("正在扫描 Java 安装..."));
    const auto& results = cachedJavaList();

    QVariantList list;
    for (const auto& j : results) {
        list.append(QVariantMap{
            { QStringLiteral("path"), j.path },
            { QStringLiteral("version"), j.version },
            { QStringLiteral("major"), j.major }
        });
    }
    if (!list.isEmpty()) {
        emit logMessage(QStringLiteral("找到 %1 个 Java 安装，最新: Java %2 (%3)")
                            .arg(list.size())
                            .arg(results.first().major)
                            .arg(results.first().path));
    } else {
        emit logMessage(QStringLiteral("⚠ 未在系统中找到 Java 安装"));
    }
    return list;
}

QVariantList SettingsBackend::availableJavaList()
{
    const auto& results = cachedJavaList();
    QVariantList list;
    for (const auto& j : results) {
        list.append(QVariantMap{
            { QStringLiteral("path"), j.path },
            { QStringLiteral("version"), j.version },
            { QStringLiteral("major"), j.major }
        });
    }
    return list;
}

void SettingsBackend::selectJavaByIndex(int index)
{
    const auto& results = cachedJavaList();
    if (index < 0 || index >= results.size()) {
        emit logMessage(QStringLiteral("❌ Java 索引无效: %1").arg(index));
        return;
    }
    const auto& info = results[index];
    m_javaPath = info.path;
    m_javaVersion = info.version;
    m_javaMajor = info.major;
    m_javaReady = true;
    saveSettings();
    emit logMessage(QStringLiteral("✅ 已切换 Java %1: %2")
                        .arg(info.major).arg(info.path));
    emit javaPathChanged();
    emit javaReadyChanged();
}

QString SettingsBackend::autoSelectJava()
{
    const auto& results = cachedJavaList();
    const JavaInfo* best = nullptr;
    for (const auto& j : results) {
        if (j.major >= 17) { best = &j; break; }
    }
    if (!best && !results.isEmpty()) best = &results.first();
    if (best) {
        m_javaPath = best->path;
        m_javaVersion = best->version;
        m_javaMajor = best->major;
        m_javaReady = true;
        saveSettings();
        emit logMessage(QStringLiteral("✅ 已选择 Java %1: %2")
                            .arg(best->major).arg(best->path));
        emit javaPathChanged();
        emit javaReadyChanged();
        return best->path;
    }
    emit logMessage(QStringLiteral("⚠ 未找到合适的 Java，请手动指定"));
    return {};
}

QString SettingsBackend::detectJava() { return autoSelectJava(); }

QString SettingsBackend::openJavaFileDialog()
{
    // Use the QML engine's root object as parent to avoid modal issues
    QWidget* parentWidget = nullptr;
    QString path = QFileDialog::getOpenFileName(
        parentWidget, QStringLiteral("选择 Java 可执行文件"),
        QStringLiteral("C:\\Program Files\\Java"),
        QStringLiteral("Java 可执行文件 (java.exe javaw.exe);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        setJavaPath(path);
        return path;
    }
    return {};
}

QString SettingsBackend::browseJava() { return openJavaFileDialog(); }

// ============================================================
// Memory (Bug 6 fix: use int for QML compatibility)
// ============================================================

QVariantMap SettingsBackend::getMemoryStatus()
{
    QVariantMap map;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);
    if (GlobalMemoryStatusEx(&s)) {
        int totalMB = static_cast<int>(s.ullTotalPhys / (1024 * 1024));
        int availMB = static_cast<int>(s.ullAvailPhys / (1024 * 1024));
        map[QStringLiteral("total")]       = totalMB;
        map[QStringLiteral("available")]   = availMB;
        map[QStringLiteral("percent")]     = static_cast<int>(s.dwMemoryLoad);
        map[QStringLiteral("recommended")] = qBound(1024, availMB / 2, 8192);
        return map;
    }
#endif
    // Fallback: round-trip estimate
    int totalMB = 8192;
    int percent = 50;
    map[QStringLiteral("total")]       = totalMB;
    map[QStringLiteral("available")]   = totalMB / 2;
    map[QStringLiteral("percent")]     = percent;
    map[QStringLiteral("recommended")] = 2048;
    return map;
}

void SettingsBackend::setMinMemory(int mb)
{
    m_minMemoryMB = qBound(256, mb, m_maxMemoryMB);
    saveSettings();
    emit memorySettingsChanged();
}

void SettingsBackend::setMaxMemory(int mb)
{
    m_maxMemoryMB = qBound(m_minMemoryMB, mb, 32768);
    saveSettings();
    emit memorySettingsChanged();
}

// ============================================================
// General Settings
// ============================================================

void SettingsBackend::setCloseAfterLaunch(bool enabled)
{
    m_closeAfterLaunch = enabled;
    saveSettings();
    emit generalSettingsChanged();
}

// ============================================================
// Isolation (delegates to VersionManager later)
// ============================================================

void SettingsBackend::setIsolationEnabled(bool enabled)
{
    Q_UNUSED(enabled)
    emit logMessage(enabled
        ? QStringLiteral("版本隔离已开启 — 各版本拥有独立的存档/截图/配置")
        : QStringLiteral("版本隔离已关闭 — 所有版本共享游戏目录"));
    emit isolationChanged();
}

// ============================================================
// Path Operations
// ============================================================

void SettingsBackend::openGameDir()
{
    QDir().mkpath(MINECRAFT_DIR);
    QDesktopServices::openUrl(QUrl::fromLocalFile(MINECRAFT_DIR));
}

void SettingsBackend::openVersionDir(const QString& versionId)
{
    QString d = MINECRAFT_DIR + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(d);
    QDesktopServices::openUrl(QUrl::fromLocalFile(d));
}

void SettingsBackend::deleteVersion(const QString& versionId)
{
    QString verDir = MINECRAFT_DIR + QStringLiteral("/versions/") + versionId;
    QString ai = MINECRAFT_DIR + QStringLiteral("/assets/indexes/")
                 + versionId + QStringLiteral(".json");
    if (QDir(verDir).exists()) QDir(verDir).removeRecursively();
    if (QFileInfo::exists(ai)) QFile::remove(ai);
    emit logMessage(QStringLiteral("已删除版本: %1").arg(versionId));
}

void SettingsBackend::openPath(const QString& path)
{
    QFileInfo fi(QDir::cleanPath(path));
    QString target = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    QDir().mkpath(target);
    if (QFileInfo::exists(target))
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    else
        emit logMessage(QStringLiteral("路径不存在: %1").arg(path));
}

// ============================================================
// Private: findAllJava
// ============================================================

QVector<SettingsBackend::JavaInfo> SettingsBackend::findAllJava()
{
    QVector<JavaInfo> results;
    QSet<QString>     seenBinDirs;

    auto add = [&](const QString& p) { tryAddJavaResult(p, seenBinDirs, results); };

    // 1. JAVA_HOME
    QString javaHome = qEnvironmentVariable("JAVA_HOME");
    if (!javaHome.isEmpty()) add(findJavaInDir(javaHome));

    // 2. PATH ("where java")
    add(findJavaOnPath());

    // 3. Scan common directories (depth-limited)
    for (const char* raw : COMMON_JAVA_DIRS) {
        QString searchDir = QString::fromLocal8Bit(raw);
        // Expand %VAR% tokens
        static const QRegularExpression varRe(QStringLiteral("%([^%]+)%"));
        auto m = varRe.match(searchDir);
        if (m.hasMatch()) {
            QString val = qEnvironmentVariable(m.captured(1).toLocal8Bit());
            if (val.isEmpty()) continue;
            searchDir.replace(m.captured(0), val);
        }
        QDir d(searchDir);
        if (!d.exists()) continue;

        QDirIterator it(searchDir, QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString fp = it.filePath();
            int depth = fp.count(QRegularExpression(QStringLiteral("[\\\\/]")))
                        - searchDir.count(QRegularExpression(QStringLiteral("[\\\\/]")));
            if (depth > 4) continue;
            add(findJavaInDir(fp));
        }
        add(findJavaInDir(searchDir));
    }

    // 4. Windows Registry
#ifdef Q_OS_WIN
    const wchar_t* regPaths[] = {
        L"SOFTWARE\\JavaSoft\\JDK",
        L"SOFTWARE\\JavaSoft\\Java Runtime Environment",
        L"SOFTWARE\\Eclipse Adoptium\\JDK",
        L"SOFTWARE\\Eclipse Foundation\\JDK",
        L"SOFTWARE\\Microsoft\\JDK",
    };
    for (const auto* rp : regPaths) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, rp, 0,
                          KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
            continue;
        DWORD idx = 0;
        wchar_t name[256];
        DWORD nameLen = 256;
        while (RegEnumKeyExW(hKey, idx, name, &nameLen,
                             nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            HKEY hSub;
            if (RegOpenKeyExW(hKey, name, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                wchar_t home[512];
                DWORD sz = sizeof(home), type;
                if (RegQueryValueExW(hSub, L"JavaHome", nullptr, &type,
                                     reinterpret_cast<BYTE*>(home), &sz) == ERROR_SUCCESS
                    && type == REG_SZ) {
                    add(findJavaInDir(QString::fromWCharArray(home)));
                }
                RegCloseKey(hSub);
            }
            ++idx;
            nameLen = 256;
        }
        RegCloseKey(hKey);
    }
#endif

    return results;
}

// ============================================================
// Private Helpers
// ============================================================

QString SettingsBackend::findJavaInDir(const QString& dirPath)
{
    static const char* cands[] = { "bin/java.exe", "bin/javaw.exe", "java.exe", "javaw.exe" };
    for (const char* c : cands) {
        QString p = QDir::cleanPath(dirPath + QLatin1Char('/') + QString::fromLatin1(c));
        if (QFileInfo::exists(p)) return p;
    }
    return {};
}

QString SettingsBackend::findJavaOnPath()
{
    QProcess proc;
    proc.start(QStringLiteral("where"), QStringList() << QStringLiteral("java"));
    proc.waitForFinished(5000);
    if (proc.exitCode() == 0) {
        QStringList lines = QString::fromLocal8Bit(
            proc.readAllStandardOutput()).split(QRegularExpression(QStringLiteral("[\r\n]")),
                                                Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            QString p = QDir::cleanPath(lines.first().trimmed());
            if (QFileInfo::exists(p)) return p;
        }
    }
    return {};
}

bool SettingsBackend::tryAddJavaResult(const QString& exePath,
                                        QSet<QString>& seenBinDirs,
                                        QVector<JavaInfo>& out)
{
    if (exePath.isEmpty()) return false;
    QFileInfo fi(exePath);
    if (!fi.isFile()) return false;
    QString binDir = QDir::cleanPath(fi.absolutePath()).toLower();
    if (seenBinDirs.contains(binDir)) return false;
    JavaInfo info = getJavaInfo(exePath);
    if (info.major == 0) return false;
    seenBinDirs.insert(binDir);
    out.append(info);
    return true;
}

SettingsBackend::JavaInfo SettingsBackend::getJavaInfo(const QString& exePath)
{
    JavaInfo info;
    info.path = exePath;
    QProcess proc;
    proc.start(exePath, QStringList() << QStringLiteral("-version"));
    proc.waitForFinished(10000);
    QString output = QString::fromLocal8Bit(proc.readAllStandardError());
    if (output.isEmpty()) output = QString::fromLocal8Bit(proc.readAllStandardOutput());
    if (output.isEmpty()) return info;

    QRegularExpression re(QStringLiteral("version\\s+\"([^\"]+)\""));
    auto match = re.match(output);
    if (!match.hasMatch()) {
        QRegularExpression re2(QStringLiteral("version\\s+(\\S+)"));
        match = re2.match(output);
    }
    if (match.hasMatch()) {
        info.version = match.captured(1);
        info.major = parseMajorVersion(info.version);
    }
    return info;
}

int SettingsBackend::parseMajorVersion(const QString& versionStr)
{
    QString n = versionStr;
    n.replace(QLatin1Char('_'), QLatin1Char('.'));
    QStringList parts = n.split(QLatin1Char('.'));
    if (parts.isEmpty()) return 0;
    if (parts.size() >= 2 && parts[0] == QStringLiteral("1")) {
        bool ok; int v = parts[1].toInt(&ok); return ok ? v : 0;
    }
    bool ok; int v = parts[0].toInt(&ok); return ok ? v : 0;
}

} // namespace ShadowLauncher
