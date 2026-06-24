#include "mod_loader_installer.h"
#include "http_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDebug>
#include <QBuffer>
#include <QSharedPointer>
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
    HttpClient::instance().download(url, savePath,
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

                bool copied = false;
                if (QFile::exists(clientJar) && QFile::copy(clientJar, jarPath)) {
                    qDebug() << "[ModLoader] Copied client jar to" << jarPath;
                    copied = true;
                } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPath)) {
                    qDebug() << "[ModLoader] Copied universal jar to" << jarPath;
                    copied = true;
                }
                if (copied) break;
                // Otherwise installer already handled it (spec:0 / processors)
            }
            emit finished(true, QString());
        } else {
            QString detail = (stdoutStr + QStringLiteral(" ") + stderrStr).trimmed();
            if (detail.isEmpty()) detail = QStringLiteral("无输出");
            emit finished(false, QString("Forge 安装程序运行失败（退出码: %1）\n%2").arg(exitCode).arg(detail));
        }
        m_running = false;
    });
    proc->start(QStringLiteral("java"), args);
}

void ModLoaderInstaller::neoForgeStep3_PCLinstall(const QByteArray& jarData) {
    // NeoForge uses the same PCL install logic as Forge
    // (NeoForge installer JARs use install_profile.json format)
    // Reuse the Forge implementation
    forgeStep3_PCLinstall(jarData);
}


void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Fabric 配置...");

    QString url = QString("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/%1/%2/profile/json")
                      .arg(m_mcVersion, m_loaderVersion);

    downloadToMemory(url, [this](bool ok, const QByteArray& data) {
        if (ok) {
            m_usedFallback = false;
            fabricStep3_writeVersion(data);  // Skip SHA1 — profile JSON is ~2KB, no verification needed
            return;
        }
        // Fallback to official Fabric meta
        qDebug() << "[ModLoader] BMCLAPI Fabric download failed, trying official...";
        QString officialUrl = QString("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                                   .arg(m_mcVersion, m_loaderVersion);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "Fabric 配置下载失败（BMCLAPI 和官方源均失败）"); m_running = false; return; }
            m_usedFallback = true;
            fabricStep3_writeVersion(data2);
        }, "fabric-profile.json");
    }, "fabric-profile.json");
}

void ModLoaderInstaller::fabricStep3_writeVersion(const QByteArray& profileData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在创建版本配置...");

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
    emit progressChanged(2, m_totalSteps, "Fabric 安装完成");
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
        QString officialUrl = QString("https://maven.neoforged.net/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(ver);
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
    QString sha1Url = QString("https://maven.neoforged.net/net/neoforged/neoforge/%1/neoforge-%1-installer.jar.sha1")
                           .arg(m_loaderVersion);

    downloadSmall(sha1Url, [this, jarData](bool ok, const QByteArray& sha1Data) {
        if (!ok || sha1Data.isEmpty()) {
            qWarning() << "[ModLoader] Cannot fetch NeoForge SHA1, skip verification";
            emit verifyFinished(false);
            neoForgeStep3_PCLinstall(jarData);
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
        neoForgeStep3_PCLinstall(jarData);
    });
}

