#include "mod_loader_installer.h"
#include "http_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QProcess>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDebug>

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
        emit progressChanged(1, m_totalSteps, "Downloading Optifine...");
        downloadToMemory(url, [this, filename](bool ok, const QByteArray& data) {
            if (!ok) {
                qDebug() << "[ModLoader] BMCLAPI Optifine download failed, trying official...";
                m_optifineUseOfficial = true;
                // Fallback: download from official site
                QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
                downloadToMemory(offUrl, [this, filename](bool ok2, const QByteArray& data2) {
                    if (!ok2) {
                        emit finished(false, "Optifine download failed (BMCLAPI + official both failed)");
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
        // With Forge: just put JAR in mods/
        m_totalSteps = 1;
        QDir().mkpath(m_gameDir + "/mods");
        QString savePath = m_gameDir + "/mods/" + filename;
        m_currentStep = 1;
        emit progressChanged(1, 1, "Downloading Optifine...");
        downloadToFile(url, savePath, [this, filename, savePath](bool ok, const QString& error) {
            if (!ok) {
                qDebug() << "[ModLoader] BMCLAPI Optifine download failed, trying official...";
                QString offUrl = QString("https://optifine.net/downloadx?f=%1").arg(filename);
                downloadToFile(offUrl, savePath, [this](bool ok2, const QString& err2) {
                    if (!ok2) {
                        emit finished(false, err2.isEmpty() ? "Optifine download failed (BMCLAPI + official both failed)" : err2);
                        m_running = false;
                        return;
                    }
                    emit progressChanged(1, 1, "Optifine installed (mods/)");
                    emit finished(true, QString());
                    m_running = false;
                });
                return;
            }
            emit progressChanged(1, 1, "Optifine installed (mods/)");
            emit finished(true, QString());
            m_running = false;
        });
    }
}

void ModLoaderInstaller::optifineStep2_install(const QByteArray& jarData, const QString& filename) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "Running Optifine installer...");

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/optifine-" + m_installName + ".jar";
    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "Cannot write Optifine JAR");
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
            emit finished(false, QString("Optifine installer failed (exit %1)").arg(exitCode));
            m_running = false;
            return;
        }

        // Copy installer output from temp to real game dir
        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);

        qDebug() << "[ModLoader] Optifine standalone install complete";
        emit progressChanged(2, m_totalSteps, "Optifine installed");
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
    if (!foundJson.isEmpty() && QFile::exists(foundJson)) {
        if (QFile::exists(targetJson)) QFile::remove(targetJson);
        QFile::copy(foundJson, targetJson);
        qDebug() << "[ModLoader] Forge JSON copied to" << targetJson;
    } else {
        qWarning() << "[ModLoader] No Forge output JSON found, trying fallback name";
        // Try the predicted folder name
        QString predictedJson = verDir.absolutePath() + "/" + m_installName + "/" + m_installName + ".json";
        if (QFile::exists(predictedJson)) {
            QFile::copy(predictedJson, targetJson);
            qDebug() << "[ModLoader] Forge JSON copied from predicted path:" << predictedJson;
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
    emit progressChanged(1, m_totalSteps, "Downloading Forge installer...");

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
            if (!ok2) { emit finished(false, "Forge installer download failed (BMCLAPI + official both failed)"); m_running = false; return; }
            m_usedFallback = true;
            forgeStep2_verify(data2);
        }, "forge-installer.jar");
    }, "forge-installer.jar");
}

void ModLoaderInstaller::forgeStep2_verify(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "Verifying Forge installer...");
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
            if (!match) { emit finished(false, "Forge installer SHA1 mismatch"); m_running = false; return; }
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
            emit finished(false, "Forge installer SHA1 mismatch");
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
    forgeStep3_runInstaller(jarData);
}

void ModLoaderInstaller::continueAfterPause() {
    if (!m_pausedJarData.isEmpty()) {
        QByteArray data = m_pausedJarData;
        m_pausedJarData.clear();
        m_pauseAfterVerify = false;
        qDebug() << "[ModLoader] Resuming after MC ready, running installer...";
        forgeStep3_runInstaller(data);
    }
}
void ModLoaderInstaller::forgeStep3_runInstaller(const QByteArray& jarData) {
    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "Installing Forge...");

    // Write installer JAR to temp
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/forge-installer-" + m_installName + ".jar";
    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "Cannot write temp JAR");
        m_running = false;
        return;
    }
    jarFile.write(jarData);
    jarFile.close();

    // PCL-style: create temp .minecraft, run installer there, copy results out
    QDir tempMcDir = setupTempMc();
    QString tempMcRoot;  // parent of .minecraft for cleanup
    {
        QString tmp = tempMcDir.absolutePath();
        if (tmp.endsWith("/.minecraft"))
            tempMcRoot = tmp.left(tmp.length() - 11);  // strip "/.minecraft"
        else
            tempMcRoot = tmp;
    }

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
            // PCL-style ForgeLikeInjectorLine: parse key milestones for sub-progress
            if (line.contains(QStringLiteral("Extracting"), Qt::CaseInsensitive) && line.contains(QStringLiteral("json"), Qt::CaseInsensitive))
                emit stepProgress(3, 7);
            else if (line.contains(QStringLiteral("Downloading"), Qt::CaseInsensitive) && line.contains(QStringLiteral("librar"), Qt::CaseInsensitive))
                emit stepProgress(3, 8);
            else if (line.contains(QStringLiteral("Considering library"), Qt::CaseInsensitive))
                emit stepProgress(3, qMin(18, 8 + m_forgeLibCount.fetchAndAddRelaxed(1)));
            else if (line.contains(QStringLiteral("Building processors"), Qt::CaseInsensitive))
                emit stepProgress(3, 18);
            else if (line.contains(QStringLiteral("DOWNLOAD_MOJMAPS"), Qt::CaseInsensitive))
                emit stepProgress(3, 20);
            else if (line.contains(QStringLiteral("MERGE_MAPPING"), Qt::CaseInsensitive))
                emit stepProgress(3, 30);
            else if (line.contains(QStringLiteral("Splitting"), Qt::CaseInsensitive))
                emit stepProgress(3, 35);
            else if (line.contains(QStringLiteral("Processing"), Qt::CaseInsensitive) && line.contains(QStringLiteral("Complete"), Qt::CaseInsensitive))
                emit stepProgress(3, 50);
            else if (line.contains(QStringLiteral("Remapping"), Qt::CaseInsensitive)) {
                if (line.contains(QStringLiteral("final"), Qt::CaseInsensitive))
                    emit stepProgress(3, 72);
                else {
                    QRegularExpression pctRe(QStringLiteral("(\\d+)\\s*%"));
                    auto m = pctRe.match(line);
                    if (m.hasMatch()) emit stepProgress(3, 72 + m.captured(1).toInt() / 10);
                }
            } else if (line.contains(QStringLiteral("Injecting"), Qt::CaseInsensitive))
                emit stepProgress(3, 91);
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
            QString msg = QString("Forge installer failed (exit %1): %2")
                .arg(exitCode).arg(stderrRemaining.isEmpty() ? QStringLiteral("unknown error") : stderrRemaining);
            qWarning() << "[ModLoader]" << msg;
            emit finished(false, msg);
            m_running = false;
            return;
        }

        // Copy installer output from temp to real game dir
        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);

        qDebug() << "[ModLoader] Forge install complete";
        emit progressChanged(3, m_totalSteps, "Install complete");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start("java", args);
}

// ============================================================
// Fabric — download BMCLAPI, verify against official meta
// ============================================================

void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "Downloading Fabric profile...");

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
            if (!ok2) { emit finished(false, "Fabric profile download failed (BMCLAPI + official both failed)"); m_running = false; return; }
            m_usedFallback = true;
            fabricStep2_verify(data2);
        }, "fabric-profile.json");
    }, "fabric-profile.json");
}

void ModLoaderInstaller::fabricStep2_verify(const QByteArray& bmclProfile) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "Verifying Fabric profile...");
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
    emit progressChanged(3, m_totalSteps, "Creating version config...");

    QJsonDocument doc = QJsonDocument::fromJson(profileData);
    if (doc.isNull()) { emit finished(false, "Invalid Fabric profile JSON"); m_running = false; return; }

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
    emit progressChanged(3, m_totalSteps, "Fabric install complete");
    emit finished(true, QString());
    m_running = false;
}

// ============================================================
// NeoForge — download, verify against official Maven .sha1
// ============================================================

void ModLoaderInstaller::neoStep1_downloadInstaller() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "Downloading NeoForge installer...");

    QString ver = m_loaderVersion;
    QString bmclUrl = QString("https://bmclapi2.bangbang93.com/forge/download/%1/installer").arg(ver);

    downloadToMemory(bmclUrl, [this, ver](bool ok, const QByteArray& data) {
        if (ok) {
            m_usedFallback = false;
            neoStep2_verify(data);
            return;
        }
        qDebug() << "[ModLoader] BMCLAPI NeoForge download failed, trying official Maven...";
        QString officialUrl = QString("https://maven.neoforged.net/net/neoforged/neoforge/%1/neoforge-%1-installer.jar").arg(ver);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "NeoForge installer download failed (BMCLAPI + official both failed)"); m_running = false; return; }
            m_usedFallback = true;
            neoStep2_verify(data2);
        }, "neoforge-installer.jar");
    }, "neoforge-installer.jar");
}

void ModLoaderInstaller::neoStep2_verify(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "Verifying NeoForge installer...");
    emit verifyStarted();

    // Official NeoForge Maven provides .sha1 files
    QString sha1Url = QString("https://maven.neoforged.net/net/neoforged/neoforge/%1/neoforge-%1-installer.jar.sha1")
                           .arg(m_loaderVersion);

    downloadSmall(sha1Url, [this, jarData](bool ok, const QByteArray& sha1Data) {
        if (!ok || sha1Data.isEmpty()) {
            qWarning() << "[ModLoader] Cannot fetch NeoForge SHA1, skip verification";
            emit verifyFinished(false);
            neoStep3_runInstaller(jarData);
            return;
        }

        QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
        QString actualSha1 = computeSha1(jarData);
        bool match = (actualSha1 == expectedSha1);

        qDebug() << "[ModLoader] NeoForge SHA1 expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
        emit verifyFinished(match);

        if (!match) {
            emit finished(false, "NeoForge installer SHA1 mismatch");
            m_running = false;
            return;
        }
        neoStep3_runInstaller(jarData);
    });
}

void ModLoaderInstaller::neoStep3_runInstaller(const QByteArray& jarData) {
    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, "Running NeoForge installer...");

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString jarPath = tempDir + "/neoforge-installer-" + m_installName + ".jar";
    QFile jarFile(jarPath);
    if (!jarFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "Cannot write temp JAR");
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

    qDebug() << "[ModLoader] NeoForge install (isolated): java" << args << "→ tempMc:" << tempMcDir.absolutePath();

    // Reset counter and parse stdout for sub-progress (same format as Forge)
    m_forgeLibCount.storeRelaxed(0);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        while (proc->canReadLine()) {
            QString line = QString::fromUtf8(proc->readLine()).trimmed();
            if (line.isEmpty()) continue;
            qDebug().noquote() << "[ModLoader] NeoForge stdout:" << line;
            if (line.contains(QStringLiteral("Extracting"), Qt::CaseInsensitive) && line.contains(QStringLiteral("json"), Qt::CaseInsensitive))
                emit stepProgress(3, 7);
            else if (line.contains(QStringLiteral("Downloading"), Qt::CaseInsensitive) && line.contains(QStringLiteral("librar"), Qt::CaseInsensitive))
                emit stepProgress(3, 8);
            else if (line.contains(QStringLiteral("Considering library"), Qt::CaseInsensitive))
                emit stepProgress(3, qMin(18, 8 + m_forgeLibCount.fetchAndAddRelaxed(1)));
            else if (line.contains(QStringLiteral("Building processors"), Qt::CaseInsensitive))
                emit stepProgress(3, 18);
            else if (line.contains(QStringLiteral("DOWNLOAD_MOJMAPS"), Qt::CaseInsensitive))
                emit stepProgress(3, 20);
            else if (line.contains(QStringLiteral("MERGE_MAPPING"), Qt::CaseInsensitive))
                emit stepProgress(3, 30);
            else if (line.contains(QStringLiteral("Splitting"), Qt::CaseInsensitive))
                emit stepProgress(3, 35);
            else if (line.contains(QStringLiteral("Processing"), Qt::CaseInsensitive) && line.contains(QStringLiteral("Complete"), Qt::CaseInsensitive))
                emit stepProgress(3, 50);
            else if (line.contains(QStringLiteral("Remapping"), Qt::CaseInsensitive)) {
                if (line.contains(QStringLiteral("final"), Qt::CaseInsensitive))
                    emit stepProgress(3, 72);
                else {
                    QRegularExpression pctRe(QStringLiteral("(\\d+)\\s*%"));
                    auto m = pctRe.match(line);
                    if (m.hasMatch()) emit stepProgress(3, 72 + m.captured(1).toInt() / 10);
                }
            } else if (line.contains(QStringLiteral("Injecting"), Qt::CaseInsensitive))
                emit stepProgress(3, 91);
        }
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath, tempMcRoot, tempMcDir](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_cancelled || exitCode != 0) {
            cleanupTempMc(tempMcRoot);
            qWarning() << "[ModLoader] NeoForge installer failed, exit:" << exitCode;
            emit finished(false, QString("Installer failed (exit %1)").arg(exitCode));
            m_running = false;
            return;
        }

        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);

        qDebug() << "[ModLoader] NeoForge install complete";
        emit progressChanged(3, m_totalSteps, "Install complete");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start("java", args);
}
