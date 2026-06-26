#include "mod_loader_installer.h"
#include "http_client.h"
#include <memory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QThread>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDebug>
#include <QBuffer>
#include <QSharedPointer>

// ── JAR manifest attribute injector (Java streaming, zero memory) ──
namespace {
void injectJarManifestAttributeAsync(const QString& jarPath,
                                  const QString& key, const QString& value,
                                  std::function<void(bool)> callback) {
    // Use Java single-file source execution (Java 11+)
    // JarInputStream/JarOutputStream streaming — O(1) memory, no tools needed
    const QString tmpJava = QDir::tempPath() + QStringLiteral("/inject_mf_%1.java")
        .arg(QRandomGenerator::global()->generate());
    {
        QFile jf(tmpJava);
        if (!jf.open(QIODevice::WriteOnly)) { callback(false); return; }
        jf.write(QStringLiteral(
            "import java.io.*;import java.util.jar.*;import java.util.zip.*;\n"
            "public class _IM {\n"
            " public static void main(String[]a)throws Exception{\n"
            "  String p=a[0],k=a[1],v=a[2],t=p+\".tmp\";\n"
            "  Manifest m;\n"
            "  try(JarInputStream ji=new JarInputStream(new FileInputStream(p))){\n"
            "   m=ji.getManifest();if(m==null)m=new Manifest();\n"
            "   if(m.getMainAttributes().getValue(k)==null)\n"
            "    m.getMainAttributes().putValue(k,v);\n"
            "   try(JarOutputStream jo=new JarOutputStream(new FileOutputStream(t),m)){\n"
            "    JarEntry e;\n"
            "    while((e=ji.getNextJarEntry())!=null){\n"
            "     if(e.getName().equals(\"META-INF/MANIFEST.MF\"))continue;\n"
            "     jo.putNextEntry(e);ji.transferTo(jo);jo.closeEntry();\n"
            "    }\n"
            "   }\n"
            "  }\n"
            "  new File(p).delete();new File(t).renameTo(new File(p));\n"
            " }}\n").toUtf8());
        jf.close();
    }
    QProcess* proc = new QProcess();
    QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        [proc, tmpJava, callback](int exitCode, QProcess::ExitStatus) {
            QFile::remove(tmpJava);
            qDebug() << "[ModLoader] Java manifest inject exitCode:" << exitCode
                     << "stderr:" << proc->readAllStandardError();
            callback(exitCode == 0);
            proc->deleteLater();
        });
    proc->start(QStringLiteral("java"), { QDir::toNativeSeparators(tmpJava),
        jarPath, key, value });
    // No timeout — process will be cleaned up on callback
}
} // anonymous namespace
#include <private/qzipreader_p.h>

using namespace ShadowLauncher;

ModLoaderInstaller::ModLoaderInstaller(QObject* parent) : QObject(parent) {}
ModLoaderInstaller::~ModLoaderInstaller() = default;

void ModLoaderInstaller::cancel() { m_cancelled = true; m_running = false; }

QString ModLoaderInstaller::computeSha1(const QByteArray& data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

void ModLoaderInstaller::emitByteProgress(const QString& name, qint64 received, qint64 total) {
    qint64 speed = 0;
    if (!m_speedTimer.isValid()) {
        m_speedTimer.start();
        m_bytesLast = received;
    } else {
        qint64 elapsed = m_speedTimer.elapsed();
        if (elapsed >= 500) {
            speed = (elapsed > 0) ? ((received - m_bytesLast) * 1000 / elapsed) : 0;
            m_speedTimer.restart();
            m_bytesLast = received;
        }
    }
    emit byteProgress(name, received, total, speed);
}

// ============================================================
// Download helpers — use HttpClient for all I/O
// ============================================================

void ModLoaderInstaller::downloadToFile(const QString& url, const QString& savePath,
                                         std::function<void(bool ok, const QString& error)> done) {
    if (m_cancelled) { done(false, "Cancelled"); return; }
    QString fileName = savePath.section('/', -1);
    HttpClient::instance().downloadWithFallback(url, savePath,
        [this, fileName](qint64 received, qint64 total) {
            if (m_cancelled) return;
            emitByteProgress(fileName, received, total);
        },
        [this, done](bool ok, const QString& error) {
            if (m_cancelled) { done(false, "Cancelled"); return; }
            done(ok, error);
        });
}

void ModLoaderInstaller::downloadToMemory(const QString& url,
                                           std::function<void(bool ok, const QByteArray& data)> done,
                                           const QString& fileNameHint) {
    if (m_cancelled) { done(false, QByteArray()); return; }
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + "/sl_ml_" + m_installName + "_"
                       + QString::number(QRandomGenerator::global()->generate() % 100000);
    downloadToFile(url, tempPath, [this, tempPath, done, fileNameHint](bool ok, const QString& error) {
        if (!ok) { done(false, QByteArray()); QFile::remove(tempPath); return; }
        QFile f(tempPath);
        if (!f.open(QIODevice::ReadOnly)) { done(false, QByteArray()); QFile::remove(tempPath); return; }
        QByteArray data = f.readAll();
        f.close();
        QFile::remove(tempPath);
        done(true, data);
    });
}

void ModLoaderInstaller::downloadSmall(const QString& url,
                                        std::function<void(bool ok, const QByteArray& data)> done) {
    // For small JSON files — use HttpClient::get (fast, no temp file)
    if (m_cancelled) { done(false, QByteArray()); return; }
    HttpClient::instance().get(url, [this, done](int status, const QByteArray& body) {
        if (m_cancelled) { done(false, QByteArray()); return; }
        done(status == 200, body);
    }, [this, done](const QString&) {
        if (m_cancelled) return;
        done(false, QByteArray());
    });
}

// ============================================================
// Public API
// ============================================================

void ModLoaderInstaller::installForgeFromData(const QByteArray& installerJar, const QString& mcVersion,
                                         const QString& forgeVersion, const QString& installName)
{
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = forgeVersion;
    m_installName = installName; m_loaderType = "forge";
    m_totalSteps = 2; m_currentStep = 0;  // verify + install (download skipped)
    m_verifyOnly = true;
    qDebug() << "[ModLoader] Forge from data (verify-only): MC" << mcVersion << "Forge" << forgeVersion;
    forgeStep2_verify(installerJar);
}

void ModLoaderInstaller::installNeoForgeFromData(const QByteArray& installerJar, const QString& mcVersion,
                                         const QString& neoVersion, const QString& installName)
{
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = neoVersion;
    m_installName = installName; m_loaderType = "neoforge";
    m_totalSteps = 2; m_currentStep = 0;  // verify + install (download skipped)
    m_verifyOnly = true;
    qDebug() << "[ModLoader] NeoForge from data (verify-only): MC" << mcVersion << "NeoForge" << neoVersion;
    neoStep2_verify(installerJar);
}

void ModLoaderInstaller::forgeContinueInstall()
{
    if (m_cachedJar.isEmpty()) {
        qWarning() << "[ModLoader] forgeContinueInstall but no cached jar";
        emit finished(false, "无缓存的安装程序");
        return;
    }
    qDebug() << "[ModLoader] Continuing Forge PCL install";
    m_verifyOnly = false;
    m_running = true;
    forgeStep3_PCLinstall(m_cachedJar);
}

void ModLoaderInstaller::neoForgeContinueInstall()
{
    if (m_cachedJar.isEmpty()) {
        qWarning() << "[ModLoader] neoForgeContinueInstall but no cached jar";
        emit finished(false, "无缓存的安装程序");
        return;
    }
    qDebug() << "[ModLoader] Continuing NeoForge install (build from installer)";
    m_verifyOnly = false;
    m_running = true;
    neoForgeStep3_buildVersion(m_cachedJar);
}

void ModLoaderInstaller::installForge(const QString& mcVersion, const QString& forgeVersion,
                                       const QString& installName) {
    qDebug() << "[ModLoader] installForge call — m_running before:" << m_running;
    if (m_running) return;
    m_running = true; m_cancelled = false; m_usedFallback = false;
    qDebug() << "[ModLoader] installForge started — m_running:" << m_running;
    m_mcVersion = mcVersion; m_loaderVersion = forgeVersion;
    m_installName = installName; m_loaderType = "forge";
    m_totalSteps = 3; m_currentStep = 0;
    qDebug() << "[ModLoader] Forge: MC" << mcVersion << "Forge" << forgeVersion;
    forgeStep1_downloadInstaller();
}

void ModLoaderInstaller::installFabric(const QString& mcVersion, const QString& fabricVersion,
                                        const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = fabricVersion;
    m_installName = installName; m_loaderType = "fabric";
    m_totalSteps = 2; m_currentStep = 0;  // download → write (no SHA1 for tiny profile JSON)
    qDebug() << "[ModLoader] Fabric: MC" << mcVersion << "Fabric" << fabricVersion;
    fabricStep1_downloadProfile();
}

void ModLoaderInstaller::installNeoForge(const QString& mcVersion, const QString& neoVersion,
                                          const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false; m_usedFallback = false;
    m_mcVersion = mcVersion; m_loaderVersion = neoVersion;
    m_installName = installName; m_loaderType = "neoforge";
    m_totalSteps = 3; m_currentStep = 0;
    qDebug() << "[ModLoader] NeoForge: MC" << mcVersion << "NeoForge" << neoVersion;
    neoStep1_downloadInstaller();
}

void ModLoaderInstaller::installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                          const QString& forgeVersion, const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = optifineVersion;
    m_installName = installName; m_loaderType = "optifine";
    m_optifineForgeVersion = forgeVersion;

    bool standalone = forgeVersion.isEmpty();
    qDebug() << "[ModLoader] Optifine: MC" << mcVersion << "Opti" << optifineVersion
             << (standalone ? "standalone" : "with Forge" + forgeVersion);

    QString filename = QString("OptiFine_%1_%2.jar").arg(mcVersion, optifineVersion);
    QString url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/download").arg(mcVersion, filename);

    if (standalone) {
        // Standalone: run installer to create version JSON
        m_totalSteps = 2;
        m_currentStep = 1;
        emit progressChanged(1, m_totalSteps, "正在下载 OptiFine...");
        downloadToMemory(url, [this, filename](bool ok, const QByteArray& data) {
            if (!ok) {
                qDebug() << "[ModLoader] BMCLAPI Optifine download failed, trying official...";
                m_optifineUseOfficial = true;
                // Fallback: download from official site
                QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
                downloadToMemory(offUrl, [this, filename](bool ok2, const QByteArray& data2) {
                    if (!ok2) {
                        emit finished(false, "OptiFine 下载失败（BMCLAPI 和官方源均失败）");
                        m_running = false;
                        return;
                    }
                    optifineStep2_install(data2, filename);
                });
                return;
            }
            m_optifineUseOfficial = false;
            optifineStep2_install(data, filename);
        });
    } else {
        // With Forge/NeoForge: just put JAR in mods/ (respects version isolation)
        m_totalSteps = 1;
        QString md = modsDir();
        QDir().mkpath(md);
        QString savePath = md + "/" + filename;
        m_currentStep = 1;
        emit progressChanged(1, 1, "正在下载 OptiFine...");
        downloadToFile(url, savePath, [this, filename, savePath](bool ok, const QString& error) {
            if (!ok) {
                qDebug() << "[ModLoader] BMCLAPI Optifine download failed, trying official...";
                QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
                downloadToFile(offUrl, savePath, [this](bool ok2, const QString& err2) {
                    if (!ok2) {
                        emit finished(false, err2.isEmpty() ? "OptiFine 下载失败（BMCLAPI 和官方源均失败）" : err2);
                        m_running = false;
                        return;
                    }
                    emit progressChanged(1, 1, "OptiFine 已安装 (mods/)");
                    emit finished(true, QString());
                    m_running = false;
                });
                return;
            }
            emit progressChanged(1, 1, "OptiFine 已安装 (mods/)");
            emit finished(true, QString());
            m_running = false;
        });
    }
}

void ModLoaderInstaller::optifineStep2_install(const QByteArray& jarData, const QString& filename) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在运行 OptiFine 安装程序...");

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/optifine-" + m_installName + ".jar";
    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "无法写入 OptiFine 临时文件");
        m_running = false;
        return;
    }
    jarFile.write(jarData);
    jarFile.close();

    // PCL-style temp .minecraft isolation
    QDir tempMcDir = setupTempMc();
    QString tempMcRoot;
    {
        QString tmp = tempMcDir.absolutePath();
        tempMcRoot = tmp.endsWith("/.minecraft") ? tmp.left(tmp.length() - 11) : tmp;
    }

    QProcess* proc = new QProcess(this);
    QStringList args;
    args << "-jar" << jarPath << "--installClient" << tempMcDir.absolutePath();

    qDebug() << "[ModLoader] Optifine install (isolated): java" << args << "→ tempMc:" << tempMcDir.absolutePath();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath, tempMcRoot, tempMcDir](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_cancelled || exitCode != 0) {
            cleanupTempMc(tempMcRoot);
            qWarning() << "[ModLoader] Optifine installer failed, exit:" << exitCode;
            emit finished(false, QString("OptiFine 安装程序运行失败（退出码: %1）").arg(exitCode));
            m_running = false;
            return;
        }

        // Copy installer output from temp to real game dir
        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);

        qDebug() << "[ModLoader] Optifine standalone install complete";
        emit progressChanged(2, m_totalSteps, "OptiFine 安装完成");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start("java", args);
}

// ============================================================
// Temp .minecraft isolation (PCL-style)
// ============================================================

QString ModLoaderInstaller::setupTempMc() {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/sl_temp_" + m_installName + "_"
                      + QString::number(QRandomGenerator::global()->generate() % 100000);
    QString tempMc = tempDir + "/.minecraft";
    QDir().mkpath(tempMc + "/versions/" + m_mcVersion);
    QDir().mkpath(tempMc + "/libraries");

    // Copy vanilla version files to temp
    QString srcVerDir = m_gameDir + "/versions/" + m_mcVersion;
    QString dstVerDir = tempMc + "/versions/" + m_mcVersion;
    if (QFile::exists(srcVerDir + "/" + m_mcVersion + ".json")) {
        QFile::copy(srcVerDir + "/" + m_mcVersion + ".json",
                    dstVerDir + "/" + m_mcVersion + ".json");
    }
    if (QFile::exists(srcVerDir + "/" + m_mcVersion + ".jar")) {
        QFile::copy(srcVerDir + "/" + m_mcVersion + ".jar",
                    dstVerDir + "/" + m_mcVersion + ".jar");
    }

    // Create minimal launcher_profiles.json (Forge installer requirement)
    QFile lpj(tempMc + "/launcher_profiles.json");
    if (lpj.open(QIODevice::WriteOnly | QIODevice::Text)) {
        lpj.write("{\"profiles\":{},\"settings\":{},\"version\":3}");
        lpj.close();
    }

    qDebug() << "[ModLoader] Temp .minecraft created:" << tempMc;
    return tempMc;
}

void ModLoaderInstaller::collectForgeOutput(const QString& tempMc, const QString& jarPath) {
    // Copy version JSON and jar from temp to game dir
    QString versionsSrc = tempMc + "/versions";
    if (!QDir(versionsSrc).exists()) return;

    QStringList dirs = QDir(versionsSrc).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& d : dirs) {
        if (d == m_mcVersion) continue;
        QString srcDir = versionsSrc + "/" + d;
        QString dstDir = m_gameDir + "/versions/" + d;
        QDir().mkpath(dstDir);
        QStringList files = QDir(srcDir).entryList(QDir::Files);
        for (const QString& f : files) {
            QString src = srcDir + "/" + f;
            QString dst = dstDir + "/" + f;
            if (QFile::exists(dst)) QFile::remove(dst);
            QFile::copy(src, dst);
        }
        qDebug() << "[ModLoader] Copied version:" << d;
    }

    // Copy libraries from temp
    QString libSrc = tempMc + "/libraries";
    if (QDir(libSrc).exists()) {
        QStringList dirs2 = QDir(libSrc).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& d : dirs2) {
            copyRecursive(libSrc + "/" + d, m_gameDir + "/libraries/" + d);
        }
    }
    Q_UNUSED(jarPath);
}

void ModLoaderInstaller::copyRecursive(const QString& srcDir, const QString& dstDir) {
    QDir dir(srcDir);
    if (!dir.exists()) return;
    QDir().mkpath(dstDir);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        QString dst = dstDir + "/" + info.fileName();
        if (!QFile::exists(dst))
            QFile::copy(info.absoluteFilePath(), dst);
    }
    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        copyRecursive(info.absoluteFilePath(), dstDir + "/" + info.fileName());
    }
}

void ModLoaderInstaller::cleanupTempMc(const QString& tempDir) {
    QDir dir(tempDir);
    if (dir.exists()) dir.removeRecursively();
    qDebug() << "[ModLoader] Temp cleaned:" << tempDir;
}

void ModLoaderInstaller::cleanupAfterInstall(const QStringList& dirsToClean) {
    const int maxRetries = 3;
    const int retryDelayMs = 500;

    for (const QString& dirPath : dirsToClean) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            qDebug() << "[ModLoader] Cleanup skip (not found):" << dirPath;
            continue;
        }

        bool ok = false;
        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            ok = dir.removeRecursively();
            if (ok) {
                qDebug() << "[ModLoader] Cleanup OK:" << dirPath
                         << (attempt > 1 ? QStringLiteral("(attempt %1)").arg(attempt) : QString());
                break;
            }
            // Check what's left
            QStringList leftovers = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            qDebug() << "[ModLoader] Cleanup FAIL (attempt" << attempt << "/" << maxRetries
                     << "):" << dirPath << "leftovers:" << leftovers;
            if (attempt < maxRetries) {
                QThread::msleep(retryDelayMs);
            }
        }

        if (!ok) {
            qWarning() << "[ModLoader] Cleanup GAVE UP after" << maxRetries
                       << "attempts:" << dirPath;
        }
    }
}

void ModLoaderInstaller::forgeStep1_downloadInstaller() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Forge 安装程序...");

    QString verArg = m_mcVersion + "-" + m_loaderVersion;
    QString bmclUrl = QString("https://bmclapi2.bangbang93.com/forge/download/%1/installer").arg(verArg);

    downloadToMemory(bmclUrl, [this, verArg](bool ok, const QByteArray& data) {
        if (ok) {
            m_usedFallback = false;
            forgeStep2_verify(data);
            return;
        }
        // Fallback to official Forge Maven
        qDebug() << "[ModLoader] BMCLAPI Forge download failed, trying official Maven...";
        QString officialUrl = QString("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/forge-%1-installer.jar").arg(verArg);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "Forge 安装程序下载失败（BMCLAPI 和官方源均失败）"); m_running = false; return; }
            m_usedFallback = true;
            forgeStep2_verify(data2);
        }, "forge-installer.jar");
    }, "forge-installer.jar");
}

void ModLoaderInstaller::forgeStep2_verify(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在校验 Forge 安装程序...");
    emit verifyStarted();

    QString actualSha1 = computeSha1(jarData);
    QString verArg = m_mcVersion + "-" + m_loaderVersion;

    if (m_usedFallback) {
        // Official source: fetch SHA1 from Maven
        QString sha1Url = QString("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/forge-%1-installer.jar.sha1").arg(verArg);
        downloadSmall(sha1Url, [this, actualSha1, jarData](bool ok, const QByteArray& sha1Data) {
            if (!ok || sha1Data.isEmpty()) {
                qWarning() << "[ModLoader] Cannot fetch official Forge SHA1, skip verification";
                emit verifyFinished(false);
                if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
                forgeStep3_PCLinstall(jarData);
                return;
            }
            QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
            bool match = (actualSha1 == expectedSha1);
            qDebug() << "[ModLoader] Forge SHA1 (official) expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
            emit verifyFinished(match);
            if (!match) { emit finished(false, "Forge 安装程序校验失败（SHA1 不匹配）"); m_running = false; return; }
            if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
            forgeStep3_PCLinstall(jarData);
        });
        return;
    }

    // BMCLAPI: fetch SHA1 from listing
    QString url = QString("https://bmclapi2.bangbang93.com/forge/minecraft/%1").arg(m_mcVersion);

    downloadSmall(url, [this, jarData](bool ok, const QByteArray& forgeData) {
        if (!ok || forgeData.isEmpty()) {
            qWarning() << "[ModLoader] Cannot fetch SHA1, skip verification";
            emit verifyFinished(false);
            if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
            forgeStep3_PCLinstall(jarData);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(forgeData);
        QString expectedSha1;
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const QJsonValue& v : arr) {
                QJsonObject obj = v.toObject();
                if (obj.value("version").toString() != m_loaderVersion) continue;
                QJsonArray files = obj.value("files").toArray();
                for (const QJsonValue& fv : files) {
                    QJsonObject f = fv.toObject();
                    if (f.value("category").toString() == "installer") {
                        expectedSha1 = f.value("hash").toString();
                        break;
                    }
                }
                break;
            }
        }

        QString actualSha1 = computeSha1(jarData);
        bool match = !expectedSha1.isEmpty() && (actualSha1 == expectedSha1);

        if (expectedSha1.isEmpty()) {
            qWarning() << "[ModLoader] No SHA1 found for Forge" << m_loaderVersion;
        }

        qDebug() << "[ModLoader] Forge SHA1 expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
        emit verifyFinished(match);
        if (!match && !expectedSha1.isEmpty()) {
            emit finished(false, "Forge 安装程序校验失败（SHA1 不匹配）");
            m_running = false;
            return;
        }
        if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
        forgeStep3_PCLinstall(jarData);
    });
}


// ═══════════════════════════════════════════════════════════════
// PCL-style Forge install — extracts version.json from installer
// JAR, downloads all libraries via BMCLAPI mirrors, then writes
// the version config to gameDir. No java process needed.
// ═══════════════════════════════════════════════════════════════

void ModLoaderInstaller::forgeStep3_PCLinstall(const QByteArray& jarData) {
    if (m_cancelled) return;

    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "正在准备 Forge libraries...");

    // 1. Open installer JAR as ZIP
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序文件");
        m_running = false;
        return;
    }
    QZipReader reader(&buffer);

    // 2. Extract bundled maven jars from installer to gameDir/libraries/
    QString libBase = m_gameDir + QStringLiteral("/libraries");
    int extractedCount = 0;
    const auto& fileList = reader.fileInfoList();
    for (const auto& info : fileList) {
        QString fp = info.filePath;
        if (!fp.startsWith(QStringLiteral("maven/"))) continue;
        if (!fp.endsWith(QStringLiteral(".jar"))) continue;
        QString relPath = fp.mid(6);
        QString target = libBase + QStringLiteral("/") + relPath;
        if (QFile::exists(target)) continue;
        QByteArray jarBytes = reader.fileData(fp);
        if (jarBytes.isEmpty()) continue;
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile jf(target);
        if (jf.open(QIODevice::WriteOnly)) { jf.write(jarBytes); jf.close(); extractedCount++; }
    }
    qDebug() << "[ModLoader] Extracted" << extractedCount << "jars from installer";

    // 3. Read install_profile.json → get version.json path
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    if (profileData.isEmpty()) {
        qWarning() << "[ModLoader] No install_profile.json, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
    if (!profileDoc.isObject()) {
        qWarning() << "[ModLoader] Invalid install_profile.json, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QString versionJsonPath = profileDoc.object().value(QStringLiteral("json")).toString();
    if (versionJsonPath.isEmpty()) {
        qWarning() << "[ModLoader] No json path in install_profile, running installer directly";
        runInstallerProcess(jarData);
        return;
    }

    // 4. Read version.json → get game libraries
    QString actualPath = versionJsonPath.startsWith(QLatin1Char('/')) ? versionJsonPath.mid(1) : versionJsonPath;
    QByteArray versionData = reader.fileData(actualPath);
    if (versionData.isEmpty()) {
        qWarning() << "[ModLoader] No version.json in installer, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QJsonDocument versionDoc = QJsonDocument::fromJson(versionData);
    if (!versionDoc.isObject()) {
        qWarning() << "[ModLoader] Invalid version.json, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QJsonArray libraries = versionDoc.object().value(QStringLiteral("libraries")).toArray();

    // 5. Build download list: skip empty URLs, skip already-extracted files
    struct DlItem { QString url; QString path; QString name; };
    QList<DlItem> downloads;
    for (const QJsonValue& v : libraries) {
        QJsonObject lib = v.toObject();
        QJsonObject downloadsObj = lib.value(QStringLiteral("downloads")).toObject();
        QJsonObject artifact = downloadsObj.value(QStringLiteral("artifact")).toObject();
        QString url = artifact.value(QStringLiteral("url")).toString();
        if (url.isEmpty()) continue;  // client.jar etc — installer generates

        QString pathStr = artifact.value(QStringLiteral("path")).toString();
        if (pathStr.isEmpty()) continue;
        QString localPath = libBase + QStringLiteral("/") + pathStr;
        if (QFile::exists(localPath)) continue;  // already extracted from maven/

        // Convert official URL → BMCLAPI mirror
        QString bmclUrl = QString(url).replace(
            QStringLiteral("maven.minecraftforge.net"),
            QStringLiteral("bmclapi2.bangbang93.com/maven"));
        bmclUrl.replace(
            QStringLiteral("libraries.minecraft.net"),
            QStringLiteral("bmclapi2.bangbang93.com/libraries"));

        downloads.append({bmclUrl, localPath, lib.value(QStringLiteral("name")).toString()});
    }

    qDebug() << "[ModLoader] Pre-downloading" << downloads.size() << "libs (skipped" << (libraries.size() - downloads.size()) << ")";

    if (downloads.isEmpty()) {
        runInstallerProcess(jarData);
        return;
    }

    // 6. Concurrent download (max 8), BMCLAPI → Maven fallback
    //    Failed downloads logged but do NOT abort — installer handles the rest
    QSharedPointer<int> remaining(new int(downloads.size()));
    QSharedPointer<int> completed(new int(0));
    QSharedPointer<bool> doneCalled(new bool(false));
    const int MAX_CONCURRENT = 8;

    auto processNext = QSharedPointer<std::function<void()>>::create();
    *processNext = [=]() {
        if (*completed >= downloads.size()) {
            if (*doneCalled) return;
            *doneCalled = true;
            // Always run installer — it fills any gaps
            runInstallerProcess(jarData);
            return;
        }
        if (*remaining <= 0) return;  // No more to start, waiting for inflight

        int idx = downloads.size() - *remaining;
        (*remaining)--;
        const DlItem& t = downloads.at(idx);
        QDir().mkpath(QFileInfo(t.path).absolutePath());

        downloadToFile(t.url, t.path, [=](bool ok, const QString& err) {
            if (ok) {
                (*completed)++;
                int pct = downloads.size() > 0 ? ((*completed) * 100 / downloads.size()) : 100;
                emit stepProgress(3, pct);
                (*processNext)();
            } else {
                // BMCLAPI failed → try Maven Central
                QString mavenUrl = QString(t.url).replace(
                    QStringLiteral("bmclapi2.bangbang93.com"),
                    QStringLiteral("repo1.maven.org"));
                downloadToFile(mavenUrl, t.path, [=](bool ok2, const QString& err2) {
                    if (!ok2)
                        qWarning() << "[ModLoader] Lib unavailable (installer will handle):" << t.name << err2;
                    (*completed)++;
                    int pct = downloads.size() > 0 ? ((*completed) * 100 / downloads.size()) : 100;
                    emit stepProgress(3, pct);
                    (*processNext)();
                });
            }
        });
    };

    int initial = qMin(MAX_CONCURRENT, downloads.size());
    for (int i = 0; i < initial; i++)
        (*processNext)();
}

void ModLoaderInstaller::runInstallerProcess(const QByteArray& jarData) {
    emit progressChanged(3, m_totalSteps, "正在安装 Forge...");

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/forge-installer-" + m_installName + ".jar";
    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "无法写入临时文件");
        m_running = false;
        return;
    }
    jarFile.write(jarData);
    jarFile.close();

    // Forge installer requires launcher_profiles.json in game dir
    QString profilesPath = m_gameDir + QStringLiteral("/launcher_profiles.json");
    if (!QFile::exists(profilesPath)) {
        QFile pf(profilesPath);
        if (pf.open(QIODevice::WriteOnly)) {
            pf.write(QStringLiteral("{\"profiles\":{}}").toUtf8());
            pf.close();
        }
    }

    QStringList args;
    args << QStringLiteral("-Xmx512M")
         << QStringLiteral("-Djava.net.preferIPv4Stack=true")
         << QStringLiteral("-jar") << jarPath
         << QStringLiteral("--installClient") << m_gameDir;
    qDebug() << "[ModLoader] Running Forge installer: java" << args;

    auto* proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        QFile::remove(jarPath);

        QString stdoutStr = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString stderrStr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        qDebug() << "[ModLoader] Installer exitCode:" << exitCode
                 << "stdout:" << stdoutStr << "stderr:" << stderrStr;

        if (exitCode == 0) {
            // Forge/NeoForge (spec:1) stores client jar in libraries/.
            // Find the version folder the installer actually created and copy jar there.
            const QString libVer = m_loaderType == QStringLiteral("neoforge")
                ? m_loaderVersion
                : (m_mcVersion + QStringLiteral("-") + m_loaderVersion);
            const QString loaderGroup = m_loaderType == QStringLiteral("neoforge")
                ? QStringLiteral("net/neoforged/neoforge")
                : QStringLiteral("net/minecraftforge/forge");
            const QString filePrefix = m_loaderType == QStringLiteral("neoforge")
                ? QStringLiteral("neoforge")
                : QStringLiteral("forge");
            const QString clientJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                + QStringLiteral("/") + libVer + QStringLiteral("/")
                + filePrefix + QStringLiteral("-") + libVer + QStringLiteral("-client.jar");

            // Find version folders that have .json but no .jar
            const QDir vDir(versionsDir());
            const QStringList subDirs = vDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
            for (const QString& sub : subDirs) {
                const QString jsonPath = versionsDir() + QStringLiteral("/") + sub
                    + QStringLiteral("/") + sub + QStringLiteral(".json");
                const QString jarPath  = versionsDir() + QStringLiteral("/") + sub
                    + QStringLiteral("/") + sub + QStringLiteral(".jar");
                if (!QFile::exists(jsonPath) || QFile::exists(jarPath)) continue;

                // Try client jar first (spec:1 with :client classifier)
                // Fallback to universal jar (spec:1 without classifier, or older spec:0)
                const QString universalJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                    + QStringLiteral("/") + libVer + QStringLiteral("/")
                    + filePrefix + QStringLiteral("-") + libVer + QStringLiteral(".jar");
                // NeoForge universal jar uses explicit -universal classifier
                const QString explicitUniversalJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                    + QStringLiteral("/") + libVer + QStringLiteral("/")
                    + filePrefix + QStringLiteral("-") + libVer + QStringLiteral("-universal.jar");

                bool copied = false;
                if (QFile::exists(clientJar) && QFile::copy(clientJar, jarPath)) {
                    qDebug() << "[ModLoader] Copied client jar to" << jarPath;
                    copied = true;
                } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPath)) {
                    qDebug() << "[ModLoader] Copied universal jar to" << jarPath;
                    copied = true;
                } else if (QFile::exists(explicitUniversalJar) && QFile::copy(explicitUniversalJar, jarPath)) {
                    qDebug() << "[ModLoader] Copied explicit-universal jar to" << jarPath;
                    copied = true;
                }
                if (copied) break;
                // Otherwise installer already handled it (spec:0 / processors)
            }

            // If the installer's default folder name differs from the user's
            // custom/auto-generated installName, rename to match.
            QString defaultName;
            if (m_loaderType == QStringLiteral("neoforge")) {
                defaultName = QStringLiteral("neoforge-") + m_loaderVersion;
            } else if (m_loaderType == QStringLiteral("forge")) {
                defaultName = m_mcVersion + QStringLiteral("-forge-") + m_loaderVersion;
            }
            if (!defaultName.isEmpty() && m_installName != defaultName) {
                renameVersionFolder(defaultName, m_installName);
            }

            emit finished(true, QString());
        } else {
            QString detail = (stdoutStr + QStringLiteral(" ") + stderrStr).trimmed();
            if (detail.isEmpty()) detail = tr("无输出");
            emit finished(false, QString("Forge 安装程序运行失败（退出码: %1）\n%2").arg(exitCode).arg(detail));
        }
        m_running = false;
    });
    proc->start(QStringLiteral("java"), args);
}

void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Fabric 配置...");

    QString url = QString("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/%1/%2/profile/json")
                      .arg(m_mcVersion, m_loaderVersion);

    downloadToMemory(url, [this](bool ok, const QByteArray& data) {
        if (ok) {
            m_usedFallback = false;
            fabricStep2_downloadLibraries(data);  // Step 2: download Fabric libraries
            return;
        }
        // Fallback to official Fabric meta
        qDebug() << "[ModLoader] BMCLAPI Fabric download failed, trying official...";
        QString officialUrl = QString("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                                   .arg(m_mcVersion, m_loaderVersion);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "Fabric 配置下载失败（BMCLAPI 和官方源均失败）"); m_running = false; return; }
            m_usedFallback = true;
            fabricStep2_downloadLibraries(data2);
        }, "fabric-profile.json");
    }, "fabric-profile.json");
}

void ModLoaderInstaller::fabricStep2_downloadLibraries(const QByteArray& profileData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在下载 Fabric 依赖库...");

    // Parse profile JSON to extract library list
    QJsonDocument doc = QJsonDocument::fromJson(profileData);
    if (doc.isNull()) {
        emit finished(false, "Fabric 配置 JSON 格式无效");
        m_running = false;
        return;
    }

    QJsonArray libs = doc.object()[QStringLiteral("libraries")].toArray();

    // Build task list: derive download URL & save path for each library
    m_fabricLibTasks.clear();
    m_fabricLibBytesDone = 0;
    m_fabricLibBytesTotal = 0;

    // Helper: Maven "group:artifact:version" → relative path
    auto mvnPath = [](const QString& name) -> QString {
        QStringList p = name.split(QLatin1Char(':'));
        if (p.size() < 3) return {};
        return p[0].replace(QLatin1Char('.'), QLatin1Char('/')) + QLatin1Char('/')
             + p[1] + QLatin1Char('/') + p[2] + QLatin1Char('/')
             + p[1] + QLatin1Char('-') + p[2] + QStringLiteral(".jar");
    };

    // Mirror list: BMCLAPI first, official Fabric Maven as fallback
    const QStringList mirrors = {
        QStringLiteral("https://bmclapi2.bangbang93.com/maven/"),
        QStringLiteral("https://maven.fabricmc.net/")
    };

    QString libsDir = m_gameDir + QStringLiteral("/libraries");

    for (const QJsonValue& v : libs) {
        QJsonObject lib = v.toObject();
        QString name = lib[QStringLiteral("name")].toString();
        QString relPath = mvnPath(name);
        if (relPath.isEmpty()) continue;

        QString savePath = libsDir + QLatin1Char('/') + relPath;
        if (QFileInfo::exists(savePath)) continue;  // already cached

        FabricLibTask task;
        task.savePath = savePath;
        // Use BMCLAPI URL by default; fallback handled at download time
        task.url = QStringLiteral("https://bmclapi2.bangbang93.com/maven/") + relPath;
        m_fabricLibTasks.append(task);
    }

    if (m_fabricLibTasks.isEmpty()) {
        qDebug() << "[ModLoader] Fabric libraries are all cached, skipping download step";
        emit progressChanged(2, m_totalSteps, "Fabric 依赖库已缓存");
        if (m_parallelMode) { m_fabricProfileData = profileData; emit waitingForMC(); }
        else fabricStep3_writeVersion(profileData);
        return;
    }

    qDebug() << "[ModLoader] Fabric library download:" << m_fabricLibTasks.size() << "files needed";
    m_fabricLibIndex = 0;
    m_fabricLibBytesDone = 0;
    m_fabricLibBytesTotal = 0;

    // Download sequentially (each Fabric lib is <2MB, sequential avoids concurrency complexity)
    // Use std::function + shared_ptr to allow recursive self-reference in C++ lambdas
    auto dlNext = std::make_shared<std::function<void()>>();
    *dlNext = [this, profileData, mirrors, dlNext]() {
        if (m_cancelled) {
            emit finished(false, "用户取消");
            m_running = false;
            return;
        }
        if (m_fabricLibIndex >= m_fabricLibTasks.size()) {
            emit progressChanged(2, m_totalSteps, "Fabric 依赖库下载完成");
            if (m_parallelMode) { m_fabricProfileData = profileData; emit waitingForMC(); }
            else fabricStep3_writeVersion(profileData);
            return;
        }

        auto& task = m_fabricLibTasks[m_fabricLibIndex];

        auto tryMirror = std::make_shared<std::function<void(int)>>();
        *tryMirror = [this, &task, dlNext, tryMirror, profileData, mirrors](int idx) {
            if (idx >= mirrors.size()) {
                emit finished(false, QStringLiteral("Fabric 依赖库下载失败:\n%1\n\n已尝试 %2 个镜像源")
                                   .arg(task.url, QString::number(mirrors.size())));
                m_running = false;
                return;
            }

            QString url = task.url;
            if (idx > 0) url.replace(mirrors[0], mirrors[idx]);

            QDir().mkpath(QFileInfo(task.savePath).absolutePath());
            downloadToFile(url, task.savePath, [this, &task, dlNext, tryMirror, idx](bool ok, const QString& err) {
                if (ok) {
                    m_fabricLibBytesDone += task.size ? task.size : QFileInfo(task.savePath).size();
                    task.downloaded = true;
                    
                    // Progress by task count
                    int pct = static_cast<int>(m_fabricLibBytesDone * 100 / qMax<qint64>(m_fabricLibTasks.size(), 1));
                    emit stepProgress(2, pct);
                    emit byteProgress(QFileInfo(task.savePath).fileName(), 
                                     static_cast<qint64>(m_fabricLibIndex + 1),
                                     static_cast<qint64>(m_fabricLibTasks.size()), 0);

                    m_fabricLibIndex++;
                    (*dlNext)();
                } else {
                    qDebug() << "[ModLoader] Fabric lib mirror" << idx << "failed:" << err;
                    (*tryMirror)(idx + 1);
                }
            });
        };

        (*tryMirror)(0);
    };

    (*dlNext)();
}

void ModLoaderInstaller::fabricFinalize() {
    if (m_fabricProfileData.isEmpty()) {
        emit finished(false, "Fabric 配置数据丢失");
        m_running = false;
        return;
    }
    qDebug() << "[ModLoader] Fabric finalize: writing version JSON (parallel mode)";
    fabricStep3_writeVersion(m_fabricProfileData);
}

void ModLoaderInstaller::fabricStep3_writeVersion(const QByteArray& profileData) {
    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "正在创建版本配置...");

    QJsonDocument doc = QJsonDocument::fromJson(profileData);
    if (doc.isNull()) { emit finished(false, "Fabric 配置 JSON 格式无效"); m_running = false; return; }

    QJsonObject json = doc.object();
    json["id"] = m_installName;

    QString versionsPath = m_gameDir + "/versions";
    QString verDir = versionsPath + "/" + m_installName;
    QDir().mkpath(verDir);

    QString jsonPath = verDir + "/" + m_installName + ".json";
    QFile f(jsonPath);
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    f.close();

    // Copy vanilla jar
    QString vanillaJar = versionsPath + "/" + m_mcVersion + "/" + m_mcVersion + ".jar";
    QString targetJar = verDir + "/" + m_installName + ".jar";
    if (QFile::exists(vanillaJar)) {
        if (QFile::exists(targetJar)) QFile::remove(targetJar);
        QFile::copy(vanillaJar, targetJar);
    }

    qDebug() << "[ModLoader] Fabric version JSON:" << jsonPath;
    emit progressChanged(3, m_totalSteps, "Fabric 安装完成");
    emit finished(true, QString());
    m_running = false;
}

// ============================================================
// NeoForge — download, verify against official Maven .sha1
// ============================================================

void ModLoaderInstaller::neoStep1_downloadInstaller() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 NeoForge 安装程序...");

    QString ver = m_loaderVersion;
    // BMCLAPI Maven mirror (same path as official Maven)
    QString bmclUrl = QString("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(ver);

    downloadToMemory(bmclUrl, [this, ver](bool ok, const QByteArray& data) {
        if (ok) {
            m_usedFallback = false;
            neoStep2_verify(data);
            return;
        }
        qDebug() << "[ModLoader] BMCLAPI NeoForge download failed, trying official Maven...";
        QString officialUrl = QString("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(ver);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "NeoForge 安装程序下载失败（BMCLAPI 和官方源均失败）"); m_running = false; return; }
            m_usedFallback = true;
            neoStep2_verify(data2);
        }, "neoforge-installer.jar");
    }, "neoforge-installer.jar");
}

void ModLoaderInstaller::neoStep2_verify(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在校验 NeoForge 安装程序...");
    emit verifyStarted();

    // Official NeoForge Maven provides .sha1 files
    QString sha1Url = QString("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar.sha1")
                           .arg(m_loaderVersion);

    downloadSmall(sha1Url, [this, jarData](bool ok, const QByteArray& sha1Data) {
        if (!ok || sha1Data.isEmpty()) {
            qWarning() << "[ModLoader] Cannot fetch NeoForge SHA1, skip verification";
            emit verifyFinished(false);
            if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
            neoForgeStep3_buildVersion(jarData);
            return;
        }

        QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
        QString actualSha1 = computeSha1(jarData);
        bool match = (actualSha1 == expectedSha1);

        qDebug() << "[ModLoader] NeoForge SHA1 expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
        emit verifyFinished(match);

        if (!match) {
            emit finished(false, "NeoForge 安装程序校验失败（SHA1 不匹配）");
            m_running = false;
            return;
        }
        if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
        neoForgeStep3_buildVersion(jarData);
    });
}

// ============================================================
// Post-install: rename version folder to user's chosen name
// ============================================================

// ============================================================
// NeoForge — extract version from installer, download libs, write JSON
// ============================================================

void ModLoaderInstaller::neoForgeStep3_buildVersion(const QByteArray& jarData)
{
    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "正在准备 NeoForge 运行库...");

    // 1. Open installer JAR as ZIP
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序文件");
        m_running = false;
        return;
    }
    QZipReader reader(&buffer);

    // 2. Extract bundled maven jars from installer to libraries/
    //    AND save installer JAR itself for later binary patching
    QString libBase = m_gameDir + QStringLiteral("/libraries");
    int extractedCount = 0;

    // Save installer JAR to maven path for runInstallerProcess later
    QString installerPath = QStringLiteral("%1/net/neoforged/neoforge/%2/neoforge-%2-installer.jar")
        .arg(libBase, m_loaderVersion);
    if (!QFile::exists(installerPath)) {
        QDir().mkpath(QFileInfo(installerPath).absolutePath());
        QFile jf(installerPath);
        if (jf.open(QIODevice::WriteOnly)) { jf.write(jarData); jf.close(); }
        qDebug() << "[ModLoader] Saved NeoForge installer to" << installerPath;
    }

    const auto& fileList = reader.fileInfoList();
    for (const auto& info : fileList) {
        QString fp = info.filePath;
        if (!fp.startsWith(QStringLiteral("maven/"))) continue;
        if (!fp.endsWith(QStringLiteral(".jar"))) continue;
        QString relPath = fp.mid(6);
        QString target = libBase + QStringLiteral("/") + relPath;
        if (QFile::exists(target)) continue;
        QByteArray jarBytes = reader.fileData(fp);
        if (jarBytes.isEmpty()) continue;
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile jf(target);
        if (jf.open(QIODevice::WriteOnly)) { jf.write(jarBytes); jf.close(); extractedCount++; }
    }
    qDebug() << "[ModLoader] Extracted" << extractedCount << "jars from NeoForge installer";

    // 3. Read install_profile.json -> get version.json path
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    if (profileData.isEmpty()) {
        reader.close();
        emit finished(false, "NeoForge 安装程序格式无效（无 install_profile.json）");
        m_running = false;
        return;
    }
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
    if (!profileDoc.isObject()) {
        reader.close();
        emit finished(false, "NeoForge 安装程序 JSON 格式无效");
        m_running = false;
        return;
    }
    QJsonObject profile = profileDoc.object();
    QString versionJsonPath = profile.value(QStringLiteral("json")).toString();
    if (versionJsonPath.isEmpty()) {
        reader.close();
        emit finished(false, "NeoForge profile 中缺少 json 字段");
        m_running = false;
        return;
    }

    // 4. Read version.json (referenced by install_profile.json's "json" key)
    QString actualPath = versionJsonPath.startsWith(QLatin1Char('/'))
        ? versionJsonPath.mid(1) : versionJsonPath;
    QByteArray versionData = reader.fileData(actualPath);
    reader.close();

    if (versionData.isEmpty()) {
        emit finished(false, "NeoForge 安装程序找不到 version.json");
        m_running = false;
        return;
    }
    QJsonDocument versionDoc = QJsonDocument::fromJson(versionData);
    if (!versionDoc.isObject()) {
        emit finished(false, "NeoForge version.json 格式无效");
        m_running = false;
        return;
    }
    QJsonObject versionJson = versionDoc.object();

    // 5. Build download list for missing libraries
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    struct DlItem { QString url; QString path; QString name; };
    QList<DlItem> downloads;
    for (const QJsonValue& v : libraries) {
        QJsonObject lib = v.toObject();
        QJsonObject artifact = lib.value(QStringLiteral("downloads")).toObject()
            .value(QStringLiteral("artifact")).toObject();
        QString url = artifact.value(QStringLiteral("url")).toString();
        if (url.isEmpty()) continue;
        QString pathStr = artifact.value(QStringLiteral("path")).toString();
        if (pathStr.isEmpty()) continue;
        QString localPath = libBase + QStringLiteral("/") + pathStr;
        if (QFile::exists(localPath)) continue;

        // Convert to BMCLAPI mirror first
        QString bmclUrl = QString(url)
            .replace(QStringLiteral("libraries.minecraft.net"),
                     QStringLiteral("bmclapi2.bangbang93.com/libraries"))
            .replace(QStringLiteral("maven.neoforged.net"),
                     QStringLiteral("bmclapi2.bangbang93.com/maven"));

        downloads.append({bmclUrl, localPath, lib.value(QStringLiteral("name")).toString()});
    }
    qDebug() << "[ModLoader] NeoForge: downloading" << downloads.size()
             << "libs (skipped" << (libraries.size() - downloads.size()) << ")";

    if (downloads.isEmpty()) {
        writeNeoForgeVersion(versionJson);
        return;
    }

    // 6. Concurrent download (max 8) -> write version when done
    QSharedPointer<int> remaining(new int(downloads.size()));
    QSharedPointer<int> completed(new int(0));
    QSharedPointer<bool> doneCalled(new bool(false));
    const int MAX_CONCURRENT = 8;

    auto processNext = QSharedPointer<std::function<void()>>::create();
    *processNext = [=]() {
        if (*completed >= downloads.size()) {
            if (*doneCalled) return;
            *doneCalled = true;
            writeNeoForgeVersion(versionJson);
            return;
        }
        if (*remaining <= 0) return;

        int idx = downloads.size() - *remaining;
        (*remaining)--;
        const DlItem& t = downloads.at(idx);
        QDir().mkpath(QFileInfo(t.path).absolutePath());

        downloadToFile(t.url, t.path, [=](bool ok, const QString& err) {
            if (ok) {
                (*completed)++;
                int pct = downloads.size() > 0 ? ((*completed) * 100 / downloads.size()) : 100;
                emit stepProgress(3, pct);
                (*processNext)();
            } else {
                // BMCLAPI failed -> try original source
                QString origUrl = QString(t.url)
                    .replace(QStringLiteral("bmclapi2.bangbang93.com/libraries"),
                             QStringLiteral("libraries.minecraft.net"))
                    .replace(QStringLiteral("bmclapi2.bangbang93.com/maven"),
                             QStringLiteral("maven.neoforged.net"));
                downloadToFile(origUrl, t.path, [=](bool ok2, const QString& err2) {
                    if (!ok2)
                        qWarning() << "[ModLoader] NeoForge lib unavailable:" << t.name << err2;
                    (*completed)++;
                    int pct = downloads.size() > 0 ? ((*completed) * 100 / downloads.size()) : 100;
                    emit stepProgress(3, pct);
                    (*processNext)();
                });
            }
        });
    };

    int initial = qMin(MAX_CONCURRENT, downloads.size());
    for (int i = 0; i < initial; i++)
        (*processNext)();
}

void ModLoaderInstaller::writeNeoForgeVersion(const QJsonObject& versionInfo)
{
    QJsonObject json = versionInfo;
    json[QStringLiteral("id")] = m_installName;

    QString versionsPath = m_gameDir + QStringLiteral("/versions");
    QString verDir = versionsPath + QStringLiteral("/") + m_installName;
    QDir().mkpath(verDir);

    QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
    QFile f(jsonPath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
        f.close();
    }

    qDebug() << "[ModLoader] NeoForge version JSON written:" << jsonPath;

    // Step 4: Run installer for binary patching only (all libs already downloaded)
    m_currentStep = 4;
    emit progressChanged(4, m_totalSteps, tr("正在安装 NeoForge（二进制补丁）..."));

    QString installerPath = m_gameDir + QStringLiteral("/libraries/net/neoforged/neoforge/%1/neoforge-%1-installer.jar")
        .arg(m_loaderVersion);

    if (!QFile::exists(installerPath)) {
        qWarning() << "[ModLoader] NeoForge installer not found, falling back to vanilla JAR copy";
        QString vanillaJar = versionsPath + QStringLiteral("/") + m_mcVersion
            + QStringLiteral("/") + m_mcVersion + QStringLiteral(".jar");
        QString targetJar = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (QFile::exists(vanillaJar)) {
            if (QFile::exists(targetJar)) QFile::remove(targetJar);
            QFile::copy(vanillaJar, targetJar);
        }
        emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装完成");
        emit finished(true, QString());
        m_running = false;
        return;
    }

    // Run installer with --installClient (libraries already cached → binary patching only, ~5s)
    QStringList args;
    args << QStringLiteral("-Xmx512M")
         << QStringLiteral("-Djava.net.preferIPv4Stack=true")
         << QStringLiteral("-jar") << QDir::toNativeSeparators(installerPath)
         << QStringLiteral("--installClient") << QDir::toNativeSeparators(m_gameDir);
    qDebug() << "[ModLoader] Running NeoForge installer:" << args;

    auto* proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        QString stdoutStr = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString stderrStr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        qDebug() << "[ModLoader] NeoForge installer exitCode:" << exitCode
                 << "stdout:" << (stdoutStr.isEmpty() ? "(empty)" : stdoutStr.left(500));
        if (!stderrStr.isEmpty())
            qDebug() << "[ModLoader] NeoForge installer stderr:" << stderrStr.left(500);

        // Post-install: use INSTALLER's output, not our partial JSON from install_profile.json
        //  1. Copy installer-generated version JSON (complete libraries + JVM args)
        //  2. Copy patched client JAR (from minecraft-client-patched)
        //  3. Delete installer's auto-named version folder
        //  4. Delete vanilla MC JAR (keep JSON for inheritsFrom, hide from version list)

        const QString verDir = m_gameDir + QStringLiteral("/versions") + QStringLiteral("/") + m_installName;
        const QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
        const QString jarPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");

        // Copy installer's COMPLETE JSON → overwrite our partial one
        const QString installedVerName = m_gameDir
            + QStringLiteral("/versions/neoforge-%1").arg(m_loaderVersion);
        const QString installedJson = installedVerName + QStringLiteral("/neoforge-%1.json").arg(m_loaderVersion);
        if (QFile::exists(installedJson)) {
            // Read installer JSON, change id to user's name
            QFile ijf(installedJson);
            if (ijf.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(ijf.readAll());
                ijf.close();
                QJsonObject obj = doc.object();
                obj[QStringLiteral("id")] = m_installName;

                // Merge parent MC version content → standalone JSON (no inheritsFrom needed)
                const QString mcJsonPath = m_gameDir + QStringLiteral("/versions/%1/%1.json").arg(m_mcVersion);
                QFile mcf(mcJsonPath);
                if (mcf.open(QIODevice::ReadOnly)) {
                    QJsonDocument mcDoc = QJsonDocument::fromJson(mcf.readAll());
                    mcf.close();
                    QJsonObject mcObj = mcDoc.object();

                    // Merge libraries: prepend MC libs (NeoForge libs already present)
                    QJsonArray mcLibs = mcObj[QStringLiteral("libraries")].toArray();
                    QJsonArray allLibs = obj[QStringLiteral("libraries")].toArray();
                    for (const QJsonValue& v : mcLibs)
                        allLibs.append(v);
                    obj[QStringLiteral("libraries")] = allLibs;

                    // Merge game arguments (MC base + NeoForge additions)
                    QJsonObject mcArgs = mcObj[QStringLiteral("arguments")].toObject();
                    QJsonObject objArgs = obj[QStringLiteral("arguments")].toObject();
                    QJsonArray mcGameArgs = mcArgs[QStringLiteral("game")].toArray();
                    QJsonArray objGameArgs = objArgs[QStringLiteral("game")].toArray();
                    for (const QJsonValue& v : mcGameArgs)
                        objGameArgs.append(v);
                    objArgs[QStringLiteral("game")] = objGameArgs;
                    obj[QStringLiteral("arguments")] = objArgs;

                    // Inherit fields that the launcher reads from the parent
                    // Remove inheritsFrom (standalone now) — must be absent, not null
                    obj.remove(QStringLiteral("inheritsFrom"));
                    // Copy null/missing fields from MC parent
                    auto cpIfNeeded = [&](const QString& key) {
                        QJsonValue v = obj[key];
                        if (v.isUndefined() || v.isNull())
                            obj[key] = mcObj[key];
                    };
                    cpIfNeeded(QStringLiteral("assetIndex"));
                    cpIfNeeded(QStringLiteral("assets"));
                    cpIfNeeded(QStringLiteral("minimumLauncherVersion"));
                    cpIfNeeded(QStringLiteral("type"));
                    cpIfNeeded(QStringLiteral("releaseTime"));
                    cpIfNeeded(QStringLiteral("time"));
                    cpIfNeeded(QStringLiteral("javaVersion"));
                    cpIfNeeded(QStringLiteral("logging"));
                    cpIfNeeded(QStringLiteral("complianceLevel"));
                    cpIfNeeded(QStringLiteral("downloads"));
                }

                // Add universal JAR as library (contains NeoForgeMod.class etc.)
                // The installer doesn't include it in libraries — it's on the module path
                // but our launcher builds classpath from libraries, so we must add it
                QJsonArray libs = obj[QStringLiteral("libraries")].toArray();
                QJsonObject uniLib;
                uniLib[QStringLiteral("name")] = QStringLiteral("net.neoforged:neoforge:%1").arg(m_loaderVersion);
                QJsonObject uniDl;
                QJsonObject uniArt;
                uniArt[QStringLiteral("path")] = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
                uniArt[QStringLiteral("url")] = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
                uniDl[QStringLiteral("artifact")] = uniArt;
                uniLib[QStringLiteral("downloads")] = uniDl;
                libs.prepend(uniLib);
                obj[QStringLiteral("libraries")] = libs;

                QDir().mkpath(QFileInfo(jsonPath).absolutePath());
                QFile jf(jsonPath);
                if (jf.open(QIODevice::WriteOnly)) {
                    jf.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
                    jf.close();
                    qDebug() << "[ModLoader] Copied installer-generated JSON →" << jsonPath;
                }
            }
        }

        // Copy patched client JAR
        bool jarCopied = false;
        // New path (26.x+ FancyModLoader)
        const QString patchedJarNew = m_gameDir
            + QStringLiteral("/libraries/net/neoforged/minecraft-client-patched/%1/minecraft-client-patched-%1.jar")
                .arg(m_loaderVersion);
        if (!jarCopied && QFile::exists(patchedJarNew)) {
            if (QFile::exists(jarPath)) QFile::remove(jarPath);
            jarCopied = QFile::copy(patchedJarNew, jarPath);
            qDebug() << "[ModLoader] Copied patched client:" << patchedJarNew;
        }
        // Old path (21.x)
        const QString clientJar = m_gameDir
            + QStringLiteral("/libraries/net/neoforged/neoforge/%1/neoforge-%1-client.jar")
                .arg(m_loaderVersion);
        if (!jarCopied && QFile::exists(clientJar)) {
            if (QFile::exists(jarPath)) QFile::remove(jarPath);
            jarCopied = QFile::copy(clientJar, jarPath);
        }

        // Inject Minecraft-Dists:client into version JAR manifest (FML requires it)
        if (jarCopied && QFile::exists(jarPath)) {
            emit progressChanged(m_currentStep, m_totalSteps, "正在注入清单属性...");
            injectJarManifestAttributeAsync(jarPath,
                QStringLiteral("Minecraft-Dists"), QStringLiteral("client"),
                [this, installedVerName, exitCode](bool ok) {
                    if (!ok)
                        qDebug() << "[ModLoader] WARNING: Failed to inject Minecraft-Dists";
                    else
                        qDebug() << "[ModLoader] Minecraft-Dists:client injected (async Java)";

                    // Unified cleanup
                    cleanupAfterInstall({
                        installedVerName,
                        m_gameDir + QStringLiteral("/versions/") + m_mcVersion
                    });

                    if (exitCode == 0) {
                        emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装完成");
                        emit finished(true, QString());
                    } else {
                        emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装失败");
                        emit finished(false, tr("NeoForge 安装程序失败 (exitCode=%1)").arg(exitCode));
                    }
                    m_running = false;
                });
            return; // rest runs in callback above
        }

        // Unified cleanup
        cleanupAfterInstall({
            installedVerName,
            m_gameDir + QStringLiteral("/versions/") + m_mcVersion
        });

        if (exitCode == 0) {
            emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装完成");
            emit finished(true, QString());
        } else {
            emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装失败");
            emit finished(false, tr("NeoForge 安装程序失败 (exitCode=%1)").arg(exitCode));
        }
        m_running = false;
    });
    proc->start(QStringLiteral("java"), args);
}

void ModLoaderInstaller::renameVersionFolder(const QString& oldName, const QString& newName)
{
    const QString vd = versionsDir();
    const QString oldDir = vd + QStringLiteral("/") + oldName;
    const QString newDir = vd + QStringLiteral("/") + newName;

    if (!QDir(oldDir).exists()) {
        qDebug() << "[ModLoader] renameVersionFolder: old not found, skip" << oldDir;
        return;
    }
    if (QDir(newDir).exists()) {
        qDebug() << "[ModLoader] renameVersionFolder: target exists, skip" << newDir;
        return;
    }

    // 1. Rename folder
    if (!QDir().rename(oldDir, newDir)) {
        qWarning() << "[ModLoader] renameVersionFolder: folder rename failed" << oldDir << "→" << newDir;
        return;
    }

    // 2. Rename JSON
    const QString oldJson = newDir + QStringLiteral("/") + oldName + QStringLiteral(".json");
    const QString newJson = newDir + QStringLiteral("/") + newName + QStringLiteral(".json");
    if (QFile::exists(oldJson) && !QFile::rename(oldJson, newJson)) {
        qWarning() << "[ModLoader] renameVersionFolder: json rename failed" << oldJson << "→" << newJson;
    }

    // 3. Rename JAR (if exists)
    const QString oldJar = newDir + QStringLiteral("/") + oldName + QStringLiteral(".jar");
    const QString newJar = newDir + QStringLiteral("/") + newName + QStringLiteral(".jar");
    if (QFile::exists(oldJar) && !QFile::rename(oldJar, newJar)) {
        qWarning() << "[ModLoader] renameVersionFolder: jar rename failed" << oldJar << "→" << newJar;
    }

    // 4. Update JSON "id" field
    QFile f(newJson);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            obj[QStringLiteral("id")] = newName;
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(QJsonDocument(obj).toJson());
                f.close();
            }
        }
    }

    qDebug() << "[ModLoader] Renamed version:" << oldName << "→" << newName;
}

