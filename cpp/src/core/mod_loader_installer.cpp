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
    m_totalSteps = 3; m_currentStep = 0;
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
    // Find newly created version folder (Forge installer generates one)
    QDir verDir(tempMc + "/versions");
    QString foundJson;
    QStringList subDirs = verDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        if (subDir == m_mcVersion) continue;  // skip our copied vanilla
        QString jsonPath = verDir.absolutePath() + "/" + subDir + "/" + subDir + ".json";
        if (QFile::exists(jsonPath)) {
            foundJson = jsonPath;
            qDebug() << "[ModLoader] Found Forge output JSON:" << foundJson;
            break;
        }
    }

    // Write version JSON to real game dir
    QString targetVerDir = m_gameDir + "/versions/" + m_installName;
    QDir().mkpath(targetVerDir);
    QString targetJson = targetVerDir + "/" + m_installName + ".json";
    auto fixJsonId = [&](const QString& jsonPath) {
        QFile f(jsonPath);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) return;
        QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("id")).toString() == m_installName) return;
        obj[QStringLiteral("id")] = m_installName;
        qDebug() << "[ModLoader] Fixing JSON id →" << m_installName;
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    };

    if (!foundJson.isEmpty() && QFile::exists(foundJson)) {
        if (QFile::exists(targetJson)) QFile::remove(targetJson);
        QFile::copy(foundJson, targetJson);
        qDebug() << "[ModLoader] Forge JSON copied to" << targetJson;
        fixJsonId(targetJson);
    } else {
        qWarning() << "[ModLoader] No Forge output JSON found, trying fallback name";
        // Try the predicted folder name
        QString predictedJson = verDir.absolutePath() + "/" + m_installName + "/" + m_installName + ".json";
        if (QFile::exists(predictedJson)) {
            QFile::copy(predictedJson, targetJson);
            qDebug() << "[ModLoader] Forge JSON copied from predicted path:" << predictedJson;
            fixJsonId(targetJson);
        } else {
            qWarning() << "[ModLoader] Cannot find Forge output JSON at all";
        }
    }

    // Copy vanilla JAR
    QString vanillaJar = tempMc + "/versions/" + m_mcVersion + "/" + m_mcVersion + ".jar";
    QString targetJar = targetVerDir + "/" + m_installName + ".jar";
    if (QFile::exists(vanillaJar) && !QFile::exists(targetJar)) {
        QFile::copy(vanillaJar, targetJar);
    }

    // Copy libraries from temp to real game dir
    QString tempLibDir = tempMc + "/libraries";
    QString realLibDir = m_gameDir + "/libraries";
    if (QDir(tempLibDir).exists()) {
        QDir().mkpath(realLibDir);
        // Copy recursively
        auto copyDir = [](const QString& src, const QString& dst, auto& self) -> void {
            QDir(src).mkpath(dst);
            QDir d(src);
            for (const QFileInfo& info : d.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
                QString s = info.absoluteFilePath();
                QString t = dst + "/" + info.fileName();
                if (info.isDir()) {
                    self(s, t, self);
                } else {
                    if (!QFile::exists(t)) QFile::copy(s, t);
                }
            }
        };
        copyDir(tempLibDir, realLibDir, copyDir);
        qDebug() << "[ModLoader] Libraries copied from temp to real game dir";
    }

    // Cleanup temp installer JAR (the path passed via QProcess)
    if (!jarPath.isEmpty()) QFile::remove(jarPath);
}

void ModLoaderInstaller::cleanupTempMc(const QString& tempDir) {
    if (tempDir.isEmpty()) return;
    QDir d(tempDir);
    if (d.exists()) {
        d.removeRecursively();
        qDebug() << "[ModLoader] Temp .minecraft cleaned:" << tempDir;
    }
}

// ============================================================
// Forge
// ============================================================

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
                maybeRunForgeStep3(jarData);
                return;
            }
            QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
            bool match = (actualSha1 == expectedSha1);
            qDebug() << "[ModLoader] Forge SHA1 (official) expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
            emit verifyFinished(match);
            if (!match) { emit finished(false, "Forge 安装程序校验失败（SHA1 不匹配）"); m_running = false; return; }
            maybeRunForgeStep3(jarData);
        });
        return;
    }

    // BMCLAPI: fetch SHA1 from listing
    QString url = QString("https://bmclapi2.bangbang93.com/forge/minecraft/%1").arg(m_mcVersion);

    downloadSmall(url, [this, jarData](bool ok, const QByteArray& forgeData) {
        if (!ok || forgeData.isEmpty()) {
            qWarning() << "[ModLoader] Cannot fetch SHA1, skip verification";
            emit verifyFinished(false);
            maybeRunForgeStep3(jarData);
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
        maybeRunForgeStep3(jarData);
    });
}


void ModLoaderInstaller::maybeRunForgeStep3(const QByteArray& jarData) {
    if (m_pauseAfterVerify) {
        m_pausedJarData = jarData;
        qDebug() << "[ModLoader] Pausing after verify, waiting for MC to complete...";
        emit waitingForMC();
        return;
    }
    forgeStep3_prefetchAndRun(jarData);
}

void ModLoaderInstaller::continueAfterPause() {
    if (!m_pausedJarData.isEmpty()) {
        QByteArray data = m_pausedJarData;
        m_pausedJarData.clear();
        m_pauseAfterVerify = false;
        qDebug() << "[ModLoader] Resuming after MC ready, running installer...";
        forgeStep3_prefetchAndRun(data);
    }
}

// ═══════════════════════════════════════════════════════════════
// PCL-style library pre-population — downloads libs via BMCLAPI
// before the installer runs, eliminating slow Mojang CDN fetches
// ═══════════════════════════════════════════════════════════════

void ModLoaderInstaller::forgePrepopulateLibraries(
    const QByteArray& jarData, const QDir& tempMc,
    std::function<void(int done, int total)> progress,
    std::function<void(bool success, const QString& error)> done)
{
    if (m_cancelled) { done(false, "Cancelled"); return; }

    // 1. Open installer JAR as ZIP
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        qWarning() << "[ModLoader] Cannot open JAR buffer";
        done(false, "Cannot open JAR buffer");
        return;
    }
    QZipReader reader(&buffer);

    // 2. Extract install_profile.json
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    if (profileData.isEmpty()) {
        qWarning() << "[ModLoader] No install_profile.json in installer JAR, skipping prefetch";
        done(false, "No install_profile.json found");
        return;
    }

    // 3. Parse to get path to version.json
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
    if (!profileDoc.isObject()) {
        qWarning() << "[ModLoader] Invalid install_profile.json";
        done(false, "Invalid install_profile.json");
        return;
    }
    QString versionJsonPath = profileDoc.object().value(QStringLiteral("json")).toString();
    // Strip leading slash if present (ZIP entries usually don't have one)
    if (versionJsonPath.startsWith(QLatin1Char('/')))
        versionJsonPath = versionJsonPath.mid(1);

    // 4. Extract version.json
    QByteArray versionData = reader.fileData(versionJsonPath);
    if (versionData.isEmpty()) {
        // Retry with leading slash
        versionData = reader.fileData(QStringLiteral("/") + versionJsonPath);
        if (versionData.isEmpty()) {
            qWarning() << "[ModLoader] Cannot extract version.json:" << versionJsonPath;
            done(false, "Cannot extract version.json from installer JAR");
            return;
        }
    }

    // 5. Parse version.json → libraries array
    QJsonDocument versionDoc = QJsonDocument::fromJson(versionData);
    if (!versionDoc.isObject()) {
        qWarning() << "[ModLoader] Invalid version.json";
        done(false, "Invalid version.json");
        return;
    }
    QJsonObject versionObj = versionDoc.object();
    QJsonArray libraries = versionObj.value(QStringLiteral("libraries")).toArray();

    if (libraries.isEmpty()) {
        qWarning() << "[ModLoader] No libraries in version.json";
        done(false, "No libraries to prefetch");
        return;
    }

    // 6. Build download list: {url, localPath}
    QSharedPointer<QList<QPair<QString, QString>>> downloads(
        new QList<QPair<QString, QString>>());
    QString libBase = tempMc.absolutePath() + QStringLiteral("/libraries/");

    for (const QJsonValue& val : libraries) {
        QJsonObject lib = val.toObject();
        QJsonObject dlObj = lib.value(QStringLiteral("downloads")).toObject();
        QJsonObject artifact = dlObj.value(QStringLiteral("artifact")).toObject();
        if (artifact.isEmpty()) continue;

        QString relPath = artifact.value(QStringLiteral("path")).toString();
        if (relPath.isEmpty()) continue;

        // Build BMCLAPI Maven URL from the artifact URL
        QString originalUrl = artifact.value(QStringLiteral("url")).toString();
        QString bmclUrl;
        if (!originalUrl.isEmpty()) {
            // Replace the domain portion with BMCLAPI mirror
            int slashSlash = originalUrl.indexOf(QStringLiteral("://"));
            if (slashSlash > 0) {
                int pathStart = originalUrl.indexOf(QLatin1Char('/'), slashSlash + 3);
                if (pathStart > 0)
                    bmclUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven") + originalUrl.mid(pathStart);
            }
        }

        // Fallback: construct from Maven coordinate (groupId:artifactId:version)
        if (bmclUrl.isEmpty()) {
            QString name = lib.value(QStringLiteral("name")).toString();
            QStringList parts = name.split(QLatin1Char(':'));
            if (parts.size() >= 3) {
                QString groupPath = QString(parts[0]).replace(QLatin1Char('.'), QLatin1Char('/'));
                QString artifactId = parts[1];
                QString version = parts[2];
                bmclUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/%1/%2/%3/%2-%3.jar")
                              .arg(groupPath, artifactId, version);
            }
        }

        if (bmclUrl.isEmpty()) continue;

        QString localPath = libBase + relPath;
        downloads->append({bmclUrl, localPath});
    }

    if (downloads->isEmpty()) {
        qWarning() << "[ModLoader] No downloadable libraries after URL filtering";
        done(false, "No downloadable libraries");
        return;
    }

    qDebug() << "[ModLoader] Prefetching" << downloads->size() << "libraries via BMCLAPI...";

    // 7. Download in parallel (max 8 concurrent)
    QSharedPointer<int> remaining(new int(downloads->size()));
    QSharedPointer<int> completed(new int(0));
    const int MAX_CONCURRENT = 8;

    std::function<void()> processNext;
    processNext = [=, &processNext]() {
        if (m_cancelled || *remaining <= 0) {
            if (!m_cancelled)
                done(true, QString());
            return;
        }
        int idx = downloads->size() - *remaining;
        (*remaining)--;

        QString url = downloads->at(idx).first;
        QString localPath = downloads->at(idx).second;

        // Create parent directories
        QDir().mkpath(QFileInfo(localPath).absolutePath());

        downloadToFile(url, localPath, [=, &processNext](bool ok, const QString& err) {
            Q_UNUSED(err)
            if (!ok) {
                qWarning() << "[ModLoader] Prefetch failed:" << url;
                // Continue anyway — installer will download this lib itself
            }
            (*completed)++;
            if (completed && downloads->size() > 0)
                progress(*completed, downloads->size());
            processNext();
        });
    };

    // Start MAX_CONCURRENT downloads
    int initial = qMin(MAX_CONCURRENT, downloads->size());
    for (int i = 0; i < initial; i++) {
        processNext();
    }
}

void ModLoaderInstaller::neoForgePrepopulateLibraries(
    const QByteArray& jarData, const QDir& tempMc,
    std::function<void(int done, int total)> progress,
    std::function<void(bool success, const QString& error)> done)
{
    // NeoForge installer has the same format as Forge
    forgePrepopulateLibraries(jarData, tempMc, progress, done);
}

// ═══════════════════════════════════════════════════════════════
// Run the Forge/NeoForge installer process (extracted from old
// forgeStep3_runInstaller for reuse after library prefetch)
// ═══════════════════════════════════════════════════════════════

void ModLoaderInstaller::runForgeInstallerProcess(const QByteArray& jarData,
                                                   const QDir& tempMcDir,
                                                   const QString& tempMcRoot)
{
    if (m_cancelled) return;

    // Write installer JAR to temp
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

    QProcess* proc = new QProcess(this);
    QStringList args;
    args << "-jar" << jarPath << "--installClient" << tempMcDir.absolutePath();

    qDebug() << "[ModLoader] Forge install (isolated): java" << args << "→ tempMc:" << tempMcDir.absolutePath();

    // Reset library counter for installer sub-progress parsing
    m_forgeLibCount.storeRelaxed(0);

    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!err.isEmpty()) {
            qWarning().noquote() << "[ModLoader] Forge stderr:" << err;
            emit logMessage(QStringLiteral("Forge: ") + err.left(200));
        }
    });
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        while (proc->canReadLine()) {
            QString line = QString::fromUtf8(proc->readLine()).trimmed();
            if (line.isEmpty()) continue;
            qDebug().noquote() << "[ModLoader] Forge stdout:" << line;
            if (line.contains(QStringLiteral("Extracting"), Qt::CaseInsensitive) && line.contains(QStringLiteral("json"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 7 * 20 / 100) : 7);
            else if (line.contains(QStringLiteral("Downloading"), Qt::CaseInsensitive) && line.contains(QStringLiteral("librar"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 8 * 20 / 100) : 8);
            else if (line.contains(QStringLiteral("Considering library"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + qMin(18, 8 + m_forgeLibCount.fetchAndAddRelaxed(1)) * 20 / 100) : qMin(18, 8 + m_forgeLibCount.fetchAndAddRelaxed(1)));
            else if (line.contains(QStringLiteral("Building processors"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 18 * 20 / 100) : 18);
            else if (line.contains(QStringLiteral("DOWNLOAD_MOJMAPS"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 20 * 20 / 100) : 20);
            else if (line.contains(QStringLiteral("MERGE_MAPPING"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 30 * 20 / 100) : 30);
            else if (line.contains(QStringLiteral("Splitting"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 35 * 20 / 100) : 35);
            else if (line.contains(QStringLiteral("Processing"), Qt::CaseInsensitive) && line.contains(QStringLiteral("Complete"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 50 * 20 / 100) : 50);
            else if (line.contains(QStringLiteral("Remapping"), Qt::CaseInsensitive)) {
                if (line.contains(QStringLiteral("final"), Qt::CaseInsensitive))
                    emit stepProgress(3, m_prefetchDone ? (80 + 72 * 20 / 100) : 72);
                else {
                    QRegularExpression pctRe(QStringLiteral("(\\d+)\\s*%"));
                    auto m = pctRe.match(line);
                    if (m.hasMatch()) emit stepProgress(3, m_prefetchDone ? (80 + (72 + m.captured(1).toInt() / 10) * 20 / 100) : (72 + m.captured(1).toInt() / 10));
                }
            } else if (line.contains(QStringLiteral("Injecting"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 91 * 20 / 100) : 91);
        }
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath, tempMcRoot, tempMcDir](int exitCode, QProcess::ExitStatus) {
        QString stderrRemaining = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!stderrRemaining.isEmpty()) {
            qWarning().noquote() << "[ModLoader] Forge stderr (final):" << stderrRemaining;
        }
        proc->deleteLater();

        if (m_cancelled || exitCode != 0) {
            cleanupTempMc(tempMcRoot);
            QString msg = QString("Forge 安装程序运行失败（退出码: %1）: %2")
                .arg(exitCode).arg(stderrRemaining.isEmpty() ? QStringLiteral("未知错误") : stderrRemaining);
            qWarning() << "[ModLoader]" << msg;
            emit finished(false, msg);
            m_running = false;
            return;
        }

        // Copy installer output from temp to real game dir
        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);

        qDebug() << "[ModLoader] Forge install complete";
        emit progressChanged(3, m_totalSteps, "安装完成");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start("java", args);
}

void ModLoaderInstaller::runNeoForgeInstallerProcess(const QByteArray& jarData,
                                                      const QDir& tempMcDir,
                                                      const QString& tempMcRoot)
{
    if (m_cancelled) return;

    // Write installer JAR to temp
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/neoforge-installer-" + m_installName + ".jar";
    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "无法写入临时文件");
        m_running = false;
        return;
    }
    jarFile.write(jarData);
    jarFile.close();

    QProcess* proc = new QProcess(this);
    QStringList args;
    args << "-jar" << jarPath << "--installClient" << tempMcDir.absolutePath();

    qDebug() << "[ModLoader] NeoForge install (isolated): java" << args << "→ tempMc:" << tempMcDir.absolutePath();

    // Reset counter for stdout sub-progress parsing (same format as Forge)
    m_forgeLibCount.storeRelaxed(0);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        while (proc->canReadLine()) {
            QString line = QString::fromUtf8(proc->readLine()).trimmed();
            if (line.isEmpty()) continue;
            qDebug().noquote() << "[ModLoader] NeoForge stdout:" << line;
            if (line.contains(QStringLiteral("Extracting"), Qt::CaseInsensitive) && line.contains(QStringLiteral("json"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 7 * 20 / 100) : 7);
            else if (line.contains(QStringLiteral("Downloading"), Qt::CaseInsensitive) && line.contains(QStringLiteral("librar"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 8 * 20 / 100) : 8);
            else if (line.contains(QStringLiteral("Considering library"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + qMin(18, 8 + m_forgeLibCount.fetchAndAddRelaxed(1)) * 20 / 100) : qMin(18, 8 + m_forgeLibCount.fetchAndAddRelaxed(1)));
            else if (line.contains(QStringLiteral("Building processors"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 18 * 20 / 100) : 18);
            else if (line.contains(QStringLiteral("DOWNLOAD_MOJMAPS"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 20 * 20 / 100) : 20);
            else if (line.contains(QStringLiteral("MERGE_MAPPING"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 30 * 20 / 100) : 30);
            else if (line.contains(QStringLiteral("Splitting"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 35 * 20 / 100) : 35);
            else if (line.contains(QStringLiteral("Processing"), Qt::CaseInsensitive) && line.contains(QStringLiteral("Complete"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 50 * 20 / 100) : 50);
            else if (line.contains(QStringLiteral("Remapping"), Qt::CaseInsensitive)) {
                if (line.contains(QStringLiteral("final"), Qt::CaseInsensitive))
                    emit stepProgress(3, m_prefetchDone ? (80 + 72 * 20 / 100) : 72);
                else {
                    QRegularExpression pctRe(QStringLiteral("(\\d+)\\s*%"));
                    auto m = pctRe.match(line);
                    if (m.hasMatch()) emit stepProgress(3, m_prefetchDone ? (80 + (72 + m.captured(1).toInt() / 10) * 20 / 100) : (72 + m.captured(1).toInt() / 10));
                }
            } else if (line.contains(QStringLiteral("Injecting"), Qt::CaseInsensitive))
                emit stepProgress(3, m_prefetchDone ? (80 + 91 * 20 / 100) : 91);
        }
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath, tempMcRoot, tempMcDir](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_cancelled || exitCode != 0) {
            cleanupTempMc(tempMcRoot);
            qWarning() << "[ModLoader] NeoForge installer failed, exit:" << exitCode;
            emit finished(false, QString("安装程序运行失败（退出码: %1）").arg(exitCode));
            m_running = false;
            return;
        }

        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);

        qDebug() << "[ModLoader] NeoForge install complete";
        emit progressChanged(3, m_totalSteps, "安装完成");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start("java", args);
}

// ═══════════════════════════════════════════════════════════════
// Forge Step 3 (renamed): pre-populate libs via BMCLAPI, then
// run the installer on pre-populated temp directory
// ═══════════════════════════════════════════════════════════════

void ModLoaderInstaller::forgeStep3_prefetchAndRun(const QByteArray& jarData) {
    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "正在准备 Forge libraries...");

    // Set up temp .minecraft (same as before)
    QDir tempMcDir = setupTempMc();
    QString tempMcRoot;
    {
        QString tmp = tempMcDir.absolutePath();
        if (tmp.endsWith("/.minecraft"))
            tempMcRoot = tmp.left(tmp.length() - 11);  // strip "/.minecraft"
        else
            tempMcRoot = tmp;
    }

    // Pre-populate libraries via BMCLAPI mirrors (PCL-style)
    forgePrepopulateLibraries(jarData, tempMcDir,
        [this](int done, int total) {
            // Step 3 progress: 0 → 80% while downloading libs
            int pct = total > 0 ? (done * 80 / total) : 0;
            emit stepProgress(3, pct);
            // Update card description during prefetch
            if (done == 0)
                emit progressChanged(3, m_totalSteps, QStringLiteral("正在准备 Forge libraries..."));
        },
        [this, jarData, tempMcDir, tempMcRoot](bool success, const QString& error) {
            m_prefetchDone = success;  // enable stdout percentage offset
            if (!success)
                qWarning() << "[ModLoader] Library prefetch failed, falling back to installer:" << error;
            else
                emit progressChanged(3, m_totalSteps, "正在安装 Forge...");
            // Now run the installer on pre-populated temp .minecraft
            runForgeInstallerProcess(jarData, tempMcDir, tempMcRoot);
        }
    );
}

// ============================================================
// Fabric — download BMCLAPI, verify against official meta
// ============================================================

void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Fabric 配置...");

    QString url = QString("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/%1/%2/profile/json")
                      .arg(m_mcVersion, m_loaderVersion);

    downloadToMemory(url, [this](bool ok, const QByteArray& data) {
        if (ok) {
            m_usedFallback = false;
            fabricStep2_verify(data);
            return;
        }
        // Fallback to official Fabric meta
        qDebug() << "[ModLoader] BMCLAPI Fabric download failed, trying official...";
        QString officialUrl = QString("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                                   .arg(m_mcVersion, m_loaderVersion);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "Fabric 配置下载失败（BMCLAPI 和官方源均失败）"); m_running = false; return; }
            m_usedFallback = true;
            fabricStep2_verify(data2);
        }, "fabric-profile.json");
    }, "fabric-profile.json");
}

void ModLoaderInstaller::fabricStep2_verify(const QByteArray& bmclProfile) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在校验 Fabric 配置...");
    emit verifyStarted();

    // Fetch from official Fabric meta to compare
    QString officialUrl = QString("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                               .arg(m_mcVersion, m_loaderVersion);

    downloadSmall(officialUrl, [this, bmclProfile](bool ok, const QByteArray& officialData) {
        if (!ok || officialData.isEmpty()) {
            qWarning() << "[ModLoader] Cannot reach official Fabric meta, skip verification";
            emit verifyFinished(false);
            fabricStep3_writeVersion(bmclProfile);
            return;
        }

        // Compute SHA1 of both
        QString bmclSha1 = computeSha1(bmclProfile);
        QString officialSha1 = computeSha1(officialData);
        bool match = (bmclSha1 == officialSha1);

        qDebug() << "[ModLoader] Fabric profile SHA1 — BMCLAPI:" << bmclSha1
                 << "Official:" << officialSha1 << "match:" << match;
        emit verifyFinished(match);

        // If BMCLAPI matches official, use BMCLAPI (faster). Otherwise use official.
        fabricStep3_writeVersion(match ? bmclProfile : officialData);
    });
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
            neoStep3_prefetchAndRun(jarData);
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
        neoStep3_prefetchAndRun(jarData);
    });
}

void ModLoaderInstaller::neoStep3_prefetchAndRun(const QByteArray& jarData) {
    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "正在准备 NeoForge libraries...");

    // Set up temp .minecraft
    QDir tempMcDir = setupTempMc();
    QString tempMcRoot;
    {
        QString tmp = tempMcDir.absolutePath();
        tempMcRoot = tmp.endsWith("/.minecraft") ? tmp.left(tmp.length() - 11) : tmp;
    }

    // Pre-populate libraries via BMCLAPI mirrors
    neoForgePrepopulateLibraries(jarData, tempMcDir,
        [this](int done, int total) {
            int pct = total > 0 ? (done * 80 / total) : 0;
            emit stepProgress(3, pct);
            if (done == 0)
                emit progressChanged(3, m_totalSteps, QStringLiteral("正在准备 NeoForge libraries..."));
        },
        [this, jarData, tempMcDir, tempMcRoot](bool success, const QString& error) {
            m_prefetchDone = success;
            if (!success) {
                qWarning() << "[ModLoader] NeoForge library prefetch failed, falling back to installer:" << error;
            } else {
                emit progressChanged(3, m_totalSteps, QStringLiteral("正在安装 NeoForge..."));
            }
            runNeoForgeInstallerProcess(jarData, tempMcDir, tempMcRoot);
        }
    );
}
