// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
#include "mod_loader_installer.h"
#include "http_client.h"
#include <memory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QThread>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDebug>
#include <QBuffer>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QUrl>
#include <QSet>
#include <QSharedPointer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include "utils/lzma/LzmaDec.h"
#include "utils/logger.h"

#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <QMutex>
#include <QFutureWatcher>
#include <QtConcurrent>

using namespace ShadowLauncher;

// Forward declaration for LZMA decompression (defined below)

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

void ModLoaderInstaller::downloadToMemoryRace(const QStringList& urls,
                                                     std::function<void(bool ok, const QByteArray& data)> done,
                                                     const QString& fileNameHint) {
    if (urls.isEmpty()) { done(false, QByteArray()); return; }
    if (urls.size() == 1) {
        downloadToMemory(urls[0], done, fileNameHint);
        return;
    }

    // Shared race state: first success wins, all subsequent ignored
    auto won = std::make_shared<bool>(false);
    auto pending = std::make_shared<QAtomicInt>(urls.size());
    auto lastError = std::make_shared<QString>();
    auto errorMutex = std::make_shared<QMutex>();

    for (const QString& url : urls) {
        downloadToMemory(url,
            [this, url, won, pending, lastError, errorMutex, done](bool ok, const QByteArray& data) {
                if (*won) return;  // another download already won
                if (m_cancelled) { *won = true; done(false, QByteArray()); return; }

                if (ok) {
                    *won = true;
                    done(true, data);
                    return;
                }

                // Record error for this URL
                {
                    QMutexLocker locker(errorMutex.get());
                    if (lastError->isEmpty())
                        *lastError = url;
                    else
                        *lastError = *lastError + QStringLiteral("; ") + url;
                }

                // All failed?
                int remaining = pending->fetchAndAddRelaxed(-1) - 1;
                if (remaining <= 0) {
                    *won = true;
                    qCWarning(logLoader) << QStringLiteral("所有下载源均失败: %1").arg(*lastError);
                    done(false, QByteArray());
                }
            },
            fileNameHint);
    }
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
    qCInfo(logLoader) << QStringLiteral("Forge 验证数据 MC=%1 Forge=%2").arg(mcVersion, forgeVersion);
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
    qCInfo(logLoader) << QStringLiteral("NeoForge 验证数据 MC=%1 NeoForge=%2").arg(mcVersion, neoVersion);
    neoStep2_verify(installerJar);
}

void ModLoaderInstaller::forgeContinueInstall()
{
    if (m_cachedJar.isEmpty()) {
        qCWarning(logLoader) << QStringLiteral("forgeContinueInstall 调用但无缓存 JAR");
        emit finished(false, "无缓存的安装程序");
        return;
    }
    qCInfo(logLoader) << QStringLiteral("继续 Forge 安装流程");
    m_verifyOnly = false;
    m_running = true;
    forgeStep3_install(m_cachedJar);
}

void ModLoaderInstaller::neoForgeContinueInstall()
{
    if (m_cachedJar.isEmpty()) {
        qCWarning(logLoader) << QStringLiteral("neoForgeContinueInstall 调用但无缓存 JAR");
        emit finished(false, "无缓存的安装程序");
        return;
    }
    qCInfo(logLoader) << QStringLiteral("继续 NeoForge 安装流程（统一 Step 3）");
    m_verifyOnly = false;
    m_running = true;
    forgeStep3_install(m_cachedJar);
}

void ModLoaderInstaller::installForge(const QString& mcVersion, const QString& forgeVersion,
                                       const QString& installName, const QString& expectedSha1) {
    qCInfo(logLoader) << QStringLiteral("installForge 调用 — m_running 调用前=%1").arg(m_running);
    if (m_running) return;
    m_running = true; m_cancelled = false; 
    m_expectedForgeSha1 = expectedSha1;
    qCInfo(logLoader) << QStringLiteral("installForge 已启动 — m_running=%1").arg(m_running);
    m_mcVersion = mcVersion; m_loaderVersion = forgeVersion;
    m_installName = installName; m_loaderType = "forge";
    m_totalSteps = 3; m_currentStep = 0;
    qCInfo(logLoader) << QStringLiteral("Forge: MC=%1 Forge=%2").arg(mcVersion, forgeVersion);
    forgeStep1_downloadInstaller();
}

void ModLoaderInstaller::installFabric(const QString& mcVersion, const QString& fabricVersion,
                                        const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = fabricVersion;
    m_installName = installName; m_loaderType = "fabric";
    m_totalSteps = 2; m_currentStep = 0;  // download → write (no SHA1 for tiny profile JSON)
    qCInfo(logLoader) << QStringLiteral("Fabric: MC=%1 Fabric=%2").arg(mcVersion, fabricVersion);
    fabricStep1_downloadProfile();
}

void ModLoaderInstaller::installNeoForge(const QString& mcVersion, const QString& neoVersion,
                                          const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false; 
    m_mcVersion = mcVersion; m_loaderVersion = neoVersion;
    m_installName = installName; m_loaderType = "neoforge";
    m_totalSteps = 3; m_currentStep = 0;
    qCInfo(logLoader) << QStringLiteral("NeoForge: MC=%1 NeoForge=%2").arg(mcVersion, neoVersion);
    neoStep1_downloadInstaller();
}

void ModLoaderInstaller::installOptifine(const QString& mcVersion, const QString& optifineVersion,
                                          const QString& forgeVersion, const QString& installName,
                                          const QString& bmclType, const QString& bmclPatch) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = optifineVersion;
    m_installName = installName; m_loaderType = "optifine";
    m_optifineForgeVersion = forgeVersion;
    m_optifineBmclType = bmclType;
    m_optifineBmclPatch = bmclPatch;

    bool standalone = forgeVersion.isEmpty();
    qCInfo(logLoader) << QStringLiteral("OptiFine: MC=%1 OptiFine=%2").arg(mcVersion, optifineVersion)
             << (standalone ? "standalone" : "with Forge" + forgeVersion);

    QString filename;
    QString url;
    {
        QString t = bmclType;
        QString p = bmclPatch;
        if (t.isEmpty() || p.isEmpty()) {
            if (optifineVersion.startsWith(QStringLiteral("HD_U_"))) {
                t = QStringLiteral("HD_U");
                p = optifineVersion.mid(5);
            } else {
                t = QStringLiteral("HD_U");
                p = optifineVersion;
            }
        }
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, t, p);
        filename = QString("OptiFine_%1_%2_%3.jar").arg(mcVersion, t, p);
    }

    if (standalone) {
        // Standalone: run installer to create version JSON
        m_totalSteps = 2;
        m_currentStep = 1;
        emit progressChanged(1, m_totalSteps, "正在下载 OptiFine...");

        const QString bmclUrl = url;
        const QString offUrl = resolveOptifineOfficialUrl(filename);
        downloadToMemoryRace({bmclUrl, offUrl},
            [this, filename](bool ok, const QByteArray& data) {
                if (!ok) {
                    emit finished(false, "OptiFine 下载失败（BMCLAPI 和官方源均失败）");
                    m_running = false;
                    return;
                }
                qCInfo(logLoader) << QStringLiteral("OptiFine 下载成功，大小=%1 字节").arg(data.size());
                // Determine which source won (don't rely on m_optifineUseOfficial for race)
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

        // Use TrueRace: download from both BMCLAPI and official concurrently
        const QString bmclUrl = url;
        const QString offUrl = resolveOptifineOfficialUrl(filename);
        downloadToMemoryRace({bmclUrl, offUrl},
            [this, savePath, filename](bool ok, const QByteArray& data) {
                if (!ok) {
                    emit finished(false, "OptiFine 下载失败（BMCLAPI 和官方源均失败）");
                    m_running = false;
                    return;
                }
                QFile f(savePath);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(data);
                    f.close();
                    emit progressChanged(1, 1, "OptiFine 已安装 (mods/)");
                    emit finished(true, QString());
                } else {
                    emit finished(false, "OptiFine 写入失败: " + savePath);
                }
                m_running = false;
            });
    }
}

void ModLoaderInstaller::installOptifineFromJar(const QByteArray& jarData, const QString& mcVersion,
                                                 const QString& installName,
                                                 const QString& bmclType, const QString& bmclPatch) {
    // Called after JAR is already downloaded (by version_backend for merged install)
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion;
    m_installName = installName;
    // Extract OptiFine version from installName: "1.21.11-OptiFine_HD_U_J9" → "HD_U_J9"
    int ofIdx = installName.indexOf(QStringLiteral("OptiFine_"));
    m_loaderVersion = (ofIdx >= 0) ? installName.mid(ofIdx + 8) : mcVersion;
    m_loaderType = "optifine";
    m_optifineBmclType = bmclType;
    m_optifineBmclPatch = bmclPatch;
    m_totalSteps = 1;
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, tr("正在安装 OptiFine..."));
    QString filename = QString("OptiFine_%1_installer.jar").arg(mcVersion);
    optifineStep2_install(jarData, filename);
}

void ModLoaderInstaller::optifineStep2_install(const QByteArray& jarData, const QString& filename) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在安装 OptiFine...");

    Q_UNUSED(filename);

    // Validate: JAR must start with PK (ZIP magic bytes)
    if (jarData.size() < 4 || jarData[0] != 'P' || jarData[1] != 'K') {
        QString preview = QString::fromUtf8(jarData.left(qMin(500, jarData.size())));
        qCWarning(logLoader) << QStringLiteral("OptiFine JAR 文件无效，预览: %1").arg(preview);
        emit finished(false, QString("OptiFine JAR 文件无效（可能下载失败），前500字节: %1").arg(preview.left(200)));
        m_running = false;
        return;
    }

    // ── Step A: Extract from install_profile.json ──
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序文件");
        m_running = false;
        return;
    }
    QZipReader reader(&buffer);
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);

    // Diagnostic: log JAR contents
    {
        const auto& fileList = reader.fileInfoList();
        QStringList paths;
        int maxLog = qMin(50, (int)fileList.size());
        for (int i = 0; i < maxLog; i++) paths << fileList[i].filePath;
        qCInfo(logLoader) << QStringLiteral("OptiFine JAR 条目（共%1个，前%2个）: %3").arg(fileList.size()).arg(maxLog).arg(paths.join(QStringLiteral(", ")));
    }

    if (!profileData.isEmpty() && profileDoc.isObject()) {
        // Found install_profile.json — use it (reader stays open for extracting JARs)
        installOptifineFromProfile(profileDoc.object(), reader, jarData);
        // Reader and buffer cleaned up inside installOptifineFromProfile
        return;
    }

    reader.close();
    buffer.close();

    // ── Step B: Synthetic version JSON (no Java needed) ──
    // Derive library suffix: prefer bmclType+bmclPatch, fallback to optifine version string
    installOptifineSynthetic(jarData);
}

// Forward declaration of flattenVersionJson (defined later in the file)
static QJsonObject flattenVersionJson(const QString& gameDir, QJsonObject child);

// ── Synthetic version JSON construction (no JAR metadata needed) ──
void ModLoaderInstaller::installOptifineSynthetic(const QByteArray& jarData) {
    // Derive library suffix: prefer bmclType+bmclPatch, fallback to optifine version string
    QString libSuffix;
    if (!m_optifineBmclType.isEmpty() && !m_optifineBmclPatch.isEmpty()) {
        libSuffix = m_mcVersion + "_" + m_optifineBmclType + "_" + m_optifineBmclPatch;
        qCInfo(logLoader) << QStringLiteral("OptiFine 合成模式: 使用 bmclType/bmclPatch=%1/%2").arg(m_optifineBmclType, m_optifineBmclPatch);
    } else {
        // Fallback: bmclType/bmclPatch not provided → derive from optifineVer
        qCInfo(logLoader) << QStringLiteral("OptiFine 合成模式: bmclType/bmclPatch 为空，从 loaderVersion=%1 推导").arg(m_loaderVersion);
        QString ver = m_loaderVersion;
        QString prefix = "OptiFine_" + m_mcVersion + "_";
        if (ver.startsWith(prefix)) ver = ver.mid(prefix.length());
        libSuffix = m_mcVersion + "_" + ver;
    }
    QString libName = "optifine:OptiFine:" + libSuffix;
    QString libPath = "optifine/OptiFine/" + libSuffix;
    QString libJar = "OptiFine-" + libSuffix + ".jar";

    qCInfo(logLoader) << QStringLiteral("OptiFine 合成安装: library=%1 suffix=%2").arg(libName, libSuffix);

    // 1. Copy JAR to libraries/optifine/OptiFine/
    QString libDir = m_gameDir + "/libraries/" + libPath;
    QDir().mkpath(libDir);
    QString libTarget = libDir + "/" + libJar;
    {
        QFile libFile(libTarget);
        if (!libFile.open(QIODevice::WriteOnly)) {
            emit finished(false, "无法写入 OptiFine 库文件");
            m_running = false;
            return;
        }
        libFile.write(jarData);
        libFile.close();
    }

    // 2. Build version JSON — use inheritsFrom (follow 主流启动器 Path B)
    QString versionId = m_installName;
    QJsonObject versionJson;

    // Resolve MC version directory to inherit its JSON
    const QString mcOptifineDir = findVersionDir(m_mcVersion);
    if (!mcOptifineDir.isEmpty()) {
        QString baseJsonPath = mcOptifineDir + "/" + QDir(mcOptifineDir).dirName() + ".json";
        QFile baseJsonFile(baseJsonPath);
        if (baseJsonFile.open(QIODevice::ReadOnly)) {
            QJsonDocument baseDoc = QJsonDocument::fromJson(baseJsonFile.readAll());
            baseJsonFile.close();
            versionJson = baseDoc.object();
        }
    }

    // Set version id and type
    versionJson[QStringLiteral("id")] = versionId;
    versionJson[QStringLiteral("type")] = QStringLiteral("release");
    // Keep inheritsFrom — inherit MC version's libraries
    if (!versionJson.contains(QStringLiteral("inheritsFrom")))
        versionJson[QStringLiteral("inheritsFrom")] = m_mcVersion;

    // Set mainClass to LaunchWrapper (OptiFine needs tweaker loading)
    versionJson[QStringLiteral("mainClass")] = QStringLiteral("net.minecraft.launchwrapper.Launch");

    // Add --tweakClass optifine.OptiFineTweaker to launch arguments
    if (versionJson.contains(QStringLiteral("minecraftArguments"))) {
        // Old format: append to string
        QString args = versionJson[QStringLiteral("minecraftArguments")].toString();
        if (!args.contains(QStringLiteral("optifine.OptiFineTweaker")))
            args += QStringLiteral(" --tweakClass optifine.OptiFineTweaker");
        versionJson[QStringLiteral("minecraftArguments")] = args;
    } else {
        // New format: add to arguments.game array
        QJsonObject args = versionJson.value(QStringLiteral("arguments")).toObject();
        QJsonArray gameArgs = args.value(QStringLiteral("game")).toArray();
        // Check if already added
        bool hasTweak = false;
        for (const QJsonValue& gv : gameArgs) {
            if (gv.isString() && gv.toString() == QStringLiteral("optifine.OptiFineTweaker")) {
                hasTweak = true; break;
            }
        }
        if (!hasTweak) {
            gameArgs.append(QStringLiteral("--tweakClass"));
            gameArgs.append(QStringLiteral("optifine.OptiFineTweaker"));
        }
        args[QStringLiteral("game")] = gameArgs;
        versionJson[QStringLiteral("arguments")] = args;
    }

    // Add libraries: OptiFine + launchwrapper (主流启动器 Path B)
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    // OptiFine library
    QJsonObject ofLib;
    ofLib[QStringLiteral("name")] = libName;
    libraries.append(ofLib);
    // launchwrapper (needed for OptiFineTweaker)
    bool hasLw = false;
    for (const QJsonValue& lv : libraries) {
        if (lv.toObject().value(QStringLiteral("name")).toString().contains(QStringLiteral("launchwrapper")))
        { hasLw = true; break; }
    }
    if (!hasLw) {
        QJsonObject lwLib;
        lwLib[QStringLiteral("name")] = QStringLiteral("net.minecraft:launchwrapper:1.12");
        libraries.append(lwLib);
    }
    versionJson[QStringLiteral("libraries")] = libraries;

    // 3. Flatten to standalone (remove inheritsFrom, merge all parent libs + args)
    //    This allows the launcher to delete the base MC version (version isolation).
    {
        QJsonObject flattened = flattenVersionJson(m_gameDir, versionJson);
        if (flattened != versionJson) {
            versionJson = flattened;
            qCInfo(logLoader) << QStringLiteral("OptiFine JSON 已压平为独立版本（inheritsFrom 链已消解）");
        }
    }

    // 4. Copy base MC JAR to version folder
    QString baseJarPath;
    if (!mcOptifineDir.isEmpty())
        baseJarPath = mcOptifineDir + "/" + QDir(mcOptifineDir).dirName() + ".jar";
    QString verDir = m_gameDir + QStringLiteral("/versions/") + versionId;
    QDir().mkpath(verDir);
    QString optiJarPath = verDir + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    if (QFile::exists(baseJarPath) && !QFile::exists(optiJarPath))
        QFile::copy(baseJarPath, optiJarPath);

    // 5. Write version JSON
    QFile jsonFile(verDir + QStringLiteral("/") + versionId + QStringLiteral(".json"));
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit finished(false, "无法写入版本配置文件");
        m_running = false;
        return;
    }
    QJsonDocument doc(versionJson);
    jsonFile.write(doc.toJson(QJsonDocument::Indented));
    jsonFile.close();

    qCInfo(logLoader) << QStringLiteral("OptiFine 合成安装完成 → %1（独立版本，mainClass=LaunchWrapper, tweaker=已添加）")
        .arg(versionId);
    emit progressChanged(2, m_totalSteps, "OptiFine 安装完成");
    emit finished(true, QString());
    m_running = false;
}

// ── Extract from install_profile.json ──
void ModLoaderInstaller::installOptifineFromProfile(const QJsonObject& profile,
                                                      const QZipReader& reader,
                                                      const QByteArray& jarData) {
    QJsonObject versionInfo = profile.value(QStringLiteral("versionInfo")).toObject();
    if (versionInfo.isEmpty()) {
        installOptifineSynthetic(jarData);
        return;
    }

    QString versionId = versionInfo.value(QStringLiteral("id")).toString();
    if (versionId.isEmpty()) {
        versionId = profile.value(QStringLiteral("install")).toObject()
                       .value(QStringLiteral("version")).toString()
                       .replace(QLatin1Char(' '), QLatin1Char('-'));
    }
    if (versionId.isEmpty()) versionId = m_installName;

    QString verDir = m_gameDir + "/versions/" + versionId;
    QString libBase = m_gameDir + "/libraries";

    // Extract bundled JARs
    const auto& fileList = reader.fileInfoList();
    QMap<QString, QByteArray> bundledJars;
    for (const auto& info : fileList) {
        QString fp = info.filePath;
        if (!fp.endsWith(QStringLiteral(".jar"))) continue;
        if (fp.startsWith(QStringLiteral("META-INF/"))) continue;
        QByteArray bytes = reader.fileData(fp);
        if (!bytes.isEmpty()) bundledJars.insert(fp, bytes);
    }

    // Match to library paths
    QJsonArray libraries = versionInfo.value(QStringLiteral("libraries")).toArray();
    int copied = 0;
    for (const QJsonValue& lv : libraries) {
        QJsonObject lib = lv.toObject();
        QString path = lib.value(QStringLiteral("downloads")).toObject()
                           .value(QStringLiteral("artifact")).toObject()
                           .value(QStringLiteral("path")).toString();
        if (path.isEmpty()) continue;
        QString targetPath = libBase + "/" + path;
        if (QFile::exists(targetPath)) continue;
        for (auto it = bundledJars.begin(); it != bundledJars.end(); ++it) {
            if (targetPath.endsWith(it.key()) || it.key().endsWith(QFileInfo(targetPath).fileName())) {
                QDir().mkpath(QFileInfo(targetPath).absolutePath());
                QFile out(targetPath);
                if (out.open(QIODevice::WriteOnly)) { out.write(it.value()); out.close(); copied++; }
                bundledJars.erase(it);
                break;
            }
        }
    }

    // Place unmatched
    for (auto it = bundledJars.begin(); it != bundledJars.end(); ++it) {
        QString fn = QFileInfo(it.key()).fileName();
        QString destDir;
        if (fn.startsWith(QStringLiteral("launchwrapper"))) {
            QString ver = fn; ver.remove(QStringLiteral("launchwrapper-of-")).remove(QStringLiteral(".jar"));
            destDir = libBase + "/optifine/launchwrapper-of/" + ver + "/";
        } else {
            destDir = libBase + "/optifine/" + fn.remove(QStringLiteral(".jar")) + "/";
        }
        QString targetPath = destDir + fn;
        QDir().mkpath(destDir);
        QFile out(targetPath);
        if (out.open(QIODevice::WriteOnly)) { out.write(it.value()); out.close(); copied++; }
    }
    qCInfo(logLoader) << QStringLiteral("OptiFine 已解压 %1 个内嵌 JAR").arg(copied);

    // Handle inheritsFrom: resolve directory and copy JAR
    QString inherits = versionInfo.value(QStringLiteral("inheritsFrom")).toString();
    if (!inherits.isEmpty() && inherits != versionId) {
        QString parentDir = findVersionDir(inherits);
        if (!parentDir.isEmpty()) {
            QString srcJar = parentDir + "/" + QDir(parentDir).dirName() + ".jar";
            QString dstJar = verDir + "/" + versionId + ".jar";
            if (QFile::exists(srcJar) && !QFile::exists(dstJar)) QFile::copy(srcJar, dstJar);
        }
        // Flatten inheritsFrom chain: merge parent JSON into standalone version
        QJsonObject flattened = flattenVersionJson(m_gameDir, versionInfo);
        if (flattened != versionInfo) {
            versionInfo = flattened;
            qCInfo(logLoader) << QStringLiteral("OptiFine JSON 已压平为独立版本（profile 模式，inheritsFrom 链已消解）");
        }
    }

    // Write version JSON (already flattened if inheritsFrom was present)
    QDir().mkpath(verDir);
    QFile jsonFile(verDir + "/" + versionId + ".json");
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(versionInfo);
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
    }

    qCInfo(logLoader) << QStringLiteral("OptiFine 安装完成（profile 模式）→ %1").arg(versionId);
    emit progressChanged(2, m_totalSteps, "OptiFine 安装完成");
    emit finished(true, QString());
    m_running = false;
}
// ── Resolve official OptiFine download URL via adloadx (主流启动器-compatible flow) ──
QString ModLoaderInstaller::resolveOptifineOfficialUrl(const QString& filename) {
    // Step 1: fetch adloadx page to get the real download URL with session token
    const QString adloadUrl = QStringLiteral("https://optifine.net/adloadx?f=%1").arg(filename);
    QNetworkAccessManager nam;
    QUrl qurl(adloadUrl);
    QNetworkRequest req(qurl);
    req.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setTransferTimeout(15000);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    QString result;
    if (reply->error() == QNetworkReply::NoError && timer.isActive()) {
        timer.stop();
        QByteArray body = reply->readAll();
        QString html = QString::fromUtf8(body);
        // Extract download URL: "downloadx?f=...&x=..."
        QRegularExpression re(QStringLiteral("downloadx\\?f=[^\"'\\s]+"));
        auto m = re.match(html);
        if (m.hasMatch()) {
            result = QStringLiteral("https://optifine.net/") + m.captured();
            qCInfo(logLoader) << QStringLiteral("OptiFine 官方下载 URL（adloadx 解析）: %1").arg(result);
        } else {
            qCWarning(logLoader) << QStringLiteral("adloadx 页面中未找到 downloadx 链接");
        }
    } else {
        qCWarning(logLoader) << QStringLiteral("adloadx 请求失败: %1").arg(reply->errorString());
    }
    reply->deleteLater();
    return result;
}

// ── Fallback: run OptiFine installer via javaw (legacy / incompatible JAR) ──
void ModLoaderInstaller::runOptifineInstaller(const QByteArray& jarData) {
    qCInfo(logLoader) << QStringLiteral("OptiFine 解压失败，回退到 javaw 安装程序");
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

    // ── 检测所需 Java 版本（从 Installer.class 读取）──
    int requiredJavaMajor = 21;  // 默认下限
    {
        QBuffer buf;
        buf.setData(jarData);
        if (buf.open(QIODevice::ReadOnly)) {
            QZipReader zr(&buf);
            QByteArray installerClass = zr.fileData(QStringLiteral("optifine/Installer.class"));
            zr.close();
            buf.close();
            if (installerClass.size() >= 8) {
                // Class file magic: CA FE BA BE at offset 0-3
                // Minor version: offset 4-5, Major version: offset 6-7
                quint8 b0 = static_cast<quint8>(installerClass[0]);
                quint8 b1 = static_cast<quint8>(installerClass[1]);
                quint8 b2 = static_cast<quint8>(installerClass[2]);
                quint8 b3 = static_cast<quint8>(installerClass[3]);
                if (b0 == 0xCA && b1 == 0xFE && b2 == 0xBA && b3 == 0xBE) {
                    int classVersion = static_cast<quint8>(installerClass[6]) * 256
                                       + static_cast<quint8>(installerClass[7]);
                    if (classVersion >= 45) {
                        requiredJavaMajor = classVersion - 44;
                        qCInfo(logLoader) << QStringLiteral("OptiFine Installer.class 版本=%1 需要 Java %2")
                            .arg(classVersion).arg(requiredJavaMajor);
                    }
                }
            }
        }
    }

    // ── 查找合适的 Java ──
    QString javaExe = QStringLiteral("javaw");
    int foundJavaMajor = 0;
    auto checkJavaVersion = [&](const QString& exePath) -> int {
        QProcess jp;
        jp.start(exePath, {QStringLiteral("-version")});
        if (!jp.waitForFinished(5000)) return 0;
        QString output = QString::fromLocal8Bit(jp.readAllStandardError());
        if (output.isEmpty()) output = QString::fromLocal8Bit(jp.readAllStandardOutput());
        QRegularExpression jre(QStringLiteral("version\\s+\"([^\"]+)\""));
        auto m = jre.match(output);
        if (!m.hasMatch()) return 0;
        QString ver = m.captured(1);
        ver.replace(QLatin1Char('_'), QLatin1Char('.'));
        QStringList parts = ver.split(QLatin1Char('.'));
        if (parts.isEmpty()) return 0;
        int v = parts[0].toInt();
        if (v == 1 && parts.size() >= 2) v = parts[1].toInt();
        return v;
    };

    // Try javaw first, then java
    QStringList candidates = {QStringLiteral("javaw"), QStringLiteral("java")};
    // Also check JAVA_HOME
    QString javaHome = qEnvironmentVariable("JAVA_HOME");
    if (!javaHome.isEmpty()) {
        candidates.prepend(javaHome + QStringLiteral("/bin/javaw"));
        candidates.prepend(javaHome + QStringLiteral("/bin/java"));
    }

    for (const QString& cand : candidates) {
        int mv = checkJavaVersion(cand);
        if (mv >= requiredJavaMajor) {
            javaExe = cand;
            foundJavaMajor = mv;
            break;
        } else if (mv > 0) {
            qCInfo(logLoader) << QStringLiteral("Java %1 (%2) 版本过低，需要 %3")
                .arg(cand).arg(mv).arg(requiredJavaMajor);
        }
    }

    if (foundJavaMajor < requiredJavaMajor) {
        // setupTempMc 尚未调用，无需清理
        QString msg = QStringLiteral("未找到满足版本要求的 Java（需要 %1+）").arg(requiredJavaMajor);
        qCWarning(logLoader) << msg;
        emit finished(false, msg);
        m_running = false;
        return;
    }

    QDir tempMcDir = setupTempMc();
    QString tempMcRoot;
    {
        QString tmp = tempMcDir.absolutePath();
        tempMcRoot = tmp.endsWith("/.minecraft") ? tmp.left(tmp.length() - 11) : tmp;
    }

    QProcess* proc = new QProcess(this);
    QStringList jargs;
    jargs << "-jar" << jarPath << "--installClient"
          << QDir::toNativeSeparators(tempMcDir.absolutePath());

    qCInfo(logLoader) << QStringLiteral("OptiFine 安装（隔离模式）: %1 %2").arg(javaExe, jargs.join(QStringLiteral(" ")));

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath, tempMcRoot, tempMcDir](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_cancelled || exitCode != 0) {
            cleanupTempMc(tempMcRoot);
            qCWarning(logLoader) << QStringLiteral("OptiFine 安装程序运行失败，退出码=%1").arg(exitCode);
            emit finished(false, QString("OptiFine 安装程序运行失败（退出码: %1）").arg(exitCode));
            m_running = false;
            return;
        }
        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);
        qCInfo(logLoader) << QStringLiteral("OptiFine 独立安装完成");
        emit progressChanged(2, m_totalSteps, "OptiFine 安装完成");
        emit finished(true, QString());
        m_running = false;
    });

    proc->start(javaExe, jargs);
}

// ============================================================
// Temp .minecraft isolation
// ============================================================

QString ModLoaderInstaller::setupTempMc() {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + "/sl_temp_" + m_installName + "_"
                      + QString::number(QRandomGenerator::global()->generate() % 100000);
    QString tempMc = tempDir + "/.minecraft";
    QDir().mkpath(tempMc + "/versions/" + m_mcVersion);
    QDir().mkpath(tempMc + "/libraries");

    // Copy vanilla version files to temp
    // Resolve real MC version directory (name may differ from m_mcVersion)
    QString srcVerDir = findVersionDir(m_mcVersion);
    if (srcVerDir.isEmpty())
        srcVerDir = m_gameDir + "/versions/" + m_mcVersion;
    QString dstVerDir = tempMc + "/versions/" + m_mcVersion;
    QString srcId = QDir(srcVerDir).dirName();
    if (QFile::exists(srcVerDir + "/" + srcId + ".json")) {
        QFile::copy(srcVerDir + "/" + srcId + ".json",
                    dstVerDir + "/" + m_mcVersion + ".json");
    }
    if (QFile::exists(srcVerDir + "/" + srcId + ".jar")) {
        QFile::copy(srcVerDir + "/" + srcId + ".jar",
                    dstVerDir + "/" + m_mcVersion + ".jar");
    }

    // Create minimal launcher_profiles.json (Forge installer requirement)
    QFile lpj(tempMc + "/launcher_profiles.json");
    if (lpj.open(QIODevice::WriteOnly | QIODevice::Text)) {
        lpj.write("{\"profiles\":{},\"settings\":{},\"version\":3}");
        lpj.close();
    }

    qCInfo(logLoader) << QStringLiteral("临时 .minecraft 已创建: %1").arg(tempMc);
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
        qCInfo(logLoader) << QStringLiteral("已复制版本: %1").arg(d);
    }

    // Copy libraries from temp
    QString libSrc = tempMc + "/libraries";
    if (QDir(libSrc).exists()) {
        QStringList dirs2 = QDir(libSrc).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& d : dirs2) {
            copyRecursive(libSrc + "/" + d, m_gameDir + "/libraries/" + d);
        }
    }

    // Copy assets from temp (OptiFine installer may generate virtual assets)
    QString assetsSrc = tempMc + "/assets";
    if (QDir(assetsSrc).exists()) {
        copyRecursive(assetsSrc, m_gameDir + "/assets");
    }
    Q_UNUSED(jarPath);
}

void ModLoaderInstaller::copyRecursive(const QString& srcDir, const QString& dstDir) {
    QDir dir(srcDir);
    if (!dir.exists()) return;
    QDir().mkpath(dstDir);
    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        QString dst = dstDir + "/" + info.fileName();
        if (QFile::exists(dst)) QFile::remove(dst);
        QFile::copy(info.absoluteFilePath(), dst);
    }
    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        copyRecursive(info.absoluteFilePath(), dstDir + "/" + info.fileName());
    }
}

void ModLoaderInstaller::cleanupTempMc(const QString& tempDir) {
    QDir dir(tempDir);
    if (dir.exists()) dir.removeRecursively();
    qCInfo(logLoader) << QStringLiteral("临时目录已清理: %1").arg(tempDir);
}

void ModLoaderInstaller::cleanupAfterInstall(const QStringList& dirsToClean) {
    const int maxRetries = 3;
    const int retryDelayMs = 500;

    for (const QString& dirPath : dirsToClean) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            qCInfo(logLoader) << QStringLiteral("清理跳过（目录不存在）: %1").arg(dirPath);
            continue;
        }

        bool ok = false;
        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            ok = dir.removeRecursively();
            if (ok) {
                qCInfo(logLoader) << QStringLiteral("清理成功: %1").arg(dirPath)
                         << (attempt > 1 ? QStringLiteral("(attempt %1)").arg(attempt) : QString());
                break;
            }
            // Check what's left
            QStringList leftovers = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            qCInfo(logLoader) << QStringLiteral("清理失败(尝试%1/%2)").arg(attempt).arg(maxRetries)
                     << "):" << dirPath << "leftovers:" << leftovers;
            if (attempt < maxRetries) {
                QThread::msleep(retryDelayMs);
            }
        }

        if (!ok) {
            qCWarning(logLoader) << QStringLiteral("清理放弃 已达最大尝试次数=%1").arg(maxRetries)
                       << "attempts:" << dirPath;
        }
    }
}

void ModLoaderInstaller::forgeStep1_downloadInstaller() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Forge 安装程序...");

    QString mc = m_mcVersion, fv = m_loaderVersion;
    // Two Maven version formats: standard ({mc}-{forge}) and old ({mc}-{forge}-{mc} for 1.7.10/1.8.9)
    QString vNew = mc + "-" + fv;
    QString vOld = mc + "-" + fv + "-" + mc;
    // Branch-aware version if available (e.g. "1.10-12.18.0.2000-1.10.0", "1.7.2-10.12.2.1155-mc172")
    QString vBranch;
    if (!m_forgeBranch.isEmpty())
        vBranch = mc + "-" + fv + "-" + m_forgeBranch;

    // TrueRace: fire all source × version × category patterns concurrently
    // Categories: installer.jar (modern), universal.zip (MC 1.3.x-1.4.x), client.zip (MC 1.2.x)
    const QStringList bases = {
        QStringLiteral("https://bmclapi2.bangbang93.com/maven"),
        QStringLiteral("https://maven.minecraftforge.net")
    };
    const QStringList versions = {
        vBranch,  // may be empty
        vNew,
        vOld
    };
    const QStringList catExts = {
        QStringLiteral("installer.jar"),
        QStringLiteral("universal.zip"),
        QStringLiteral("client.zip")
    };

    QStringList urlList;
    for (const QString& base : bases) {
        for (const QString& ver : versions) {
            if (ver.isEmpty()) continue;
            for (const QString& catExt : catExts) {
                urlList.append(QStringLiteral("%1/net/minecraftforge/forge/%2/forge-%2-%3").arg(base, ver, catExt));
            }
        }
    }

    if (urlList.isEmpty()) {
        emit finished(false, "Forge 安装程序下载失败（无法构建下载地址）");
        m_running = false;
        return;
    }

    qCInfo(logLoader) << QStringLiteral("Forge 并行下载: %1 个地址").arg(urlList.size());
    for (const QString& u : urlList)
        qCInfo(logLoader) << QStringLiteral("  %1").arg(u);

    downloadToMemoryRace(urlList,
        [this](bool ok, const QByteArray& data) {
            if (!ok) {
                emit finished(false, "Forge 安装程序下载失败（所有镜像源均不可用）");
                m_running = false;
                return;
            }
            qCInfo(logLoader) << QStringLiteral("Forge 安装程序下载成功，大小=%1 字节").arg(data.size());
            forgeStep2_verify(data);
        },
        QStringLiteral("forge-installer.jar"));
}

void ModLoaderInstaller::forgeStep2_verify(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在校验 Forge 安装程序...");
    emit verifyStarted();

    QString actualSha1 = computeSha1(jarData);
    emit progressChanged(2, m_totalSteps, "正在获取 Forge SHA1 校验值...");
    QString verArg = m_mcVersion + "-" + m_loaderVersion;


    // BMCLAPI: use cached SHA1 from version list (avoids redundant network fetch)
    if (!m_expectedForgeSha1.isEmpty()) {
        emit progressChanged(2, m_totalSteps, QStringLiteral("正在比对 Forge SHA1 校验值..."));
        bool match = (actualSha1 == m_expectedForgeSha1);
        qCInfo(logLoader) << QStringLiteral("Forge SHA1（缓存）期望=%1 实际=%2 匹配=%3").arg(m_expectedForgeSha1, actualSha1, match ? QStringLiteral("是") : QStringLiteral("否"));
        emit verifyFinished(match);
        if (!match) { emit finished(false, QStringLiteral("Forge 安装程序校验失败（SHA1 不匹配）")); m_running = false; return; }
        if (m_verifyOnly) { m_cachedJar = jarData; m_running = false; emit waitingForMC(); return; }
        forgeStep3_install(jarData);
        return;
    }

    // Fallback: fetch SHA1 from Forge version list API
    QString url = QString("https://bmclapi2.bangbang93.com/forge/minecraft/%1").arg(m_mcVersion);

    downloadSmall(url, [this, jarData](bool ok, const QByteArray& forgeData) {
        if (!ok || forgeData.isEmpty()) {
            qCWarning(logLoader) << QStringLiteral("无法获取 SHA1，跳过校验");
            emit verifyFinished(false);
            if (m_verifyOnly) { m_cachedJar = jarData; m_running = false; emit waitingForMC(); return; }
            forgeStep3_install(jarData);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(forgeData);
        emit progressChanged(2, m_totalSteps, "正在比对 Forge SHA1 校验值...");
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
            qCWarning(logLoader) << QStringLiteral("未找到 Forge %1 的 SHA1 值").arg(m_loaderVersion);
        }

        qCInfo(logLoader) << QStringLiteral("Forge SHA1 期望=%1 实际=%2 匹配=%3").arg(expectedSha1, actualSha1, match ? QStringLiteral("是") : QStringLiteral("否"));
        emit verifyFinished(match);
        if (!match && !expectedSha1.isEmpty()) {
            emit finished(false, "Forge 安装程序校验失败（SHA1 不匹配）");
            m_running = false;
            return;
        }
        if (m_verifyOnly) { m_cachedJar = jarData; m_running = false; emit waitingForMC(); return; }
        forgeStep3_install(jarData);
    });
}


// ═══════════════════════════════════════════════════════════════


// ═══════════════════════════════════════════════════════════════
// Forge install — extracts version.json from installer
// JAR, downloads all libraries via BMCLAPI mirrors, then writes
// the version config to gameDir. No java process needed.
// ═══════════════════════════════════════════════════════════════

// ── Maven URL → BMCLAPI mirror helper ──
static QString mirrorMavenUrl(const QString& url) {
    if (url.isEmpty()) return url;
    // Priority: BMCLAPI mirror first
    QString result = url;
    result.replace(QStringLiteral("https://maven.minecraftforge.net"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
    result.replace(QStringLiteral("https://files.minecraftforge.net/maven"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
    result.replace(QStringLiteral("https://maven.neoforged.net/releases"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
    result.replace(QStringLiteral("https://libraries.minecraft.net"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/libraries"));
    result.replace(QStringLiteral("https://repo1.maven.org/maven2"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
    result.replace(QStringLiteral("https://repo.maven.apache.org/maven2"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
    result.replace(QStringLiteral("https://maven.fabricmc.net"),
                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
    return result;
}

void ModLoaderInstaller::forgeStep3_install(const QByteArray& jarData) {
    if (m_cancelled) return;

    // 0. Check if this is a genuine installer JAR or a self-contained universal/client zip
    //    For MC < 1.5 Forge (Leagcy 3), the file IS the game JAR, not an installer.
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序文件");
        m_running = false;
        return;
    }
    QZipReader reader(&buffer);
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    if (profileData.isEmpty()) {
        // No install_profile.json → not an installer JAR → universal/client zip (Legacy 3)
        qCInfo(logLoader) << QStringLiteral("未找到 install_profile.json，走 Legacy 3（自包含 JAR 直装）");
        reader.close();
        installLegacy3(jarData);
        return;
    }
    reader.close();

    // ── Here onward: genuine installer JAR with install_profile.json ──

    // Reopen for the real processing
    QBuffer buffer2;
    buffer2.setData(jarData);
    if (!buffer2.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序文件");
        m_running = false;
        return;
    }
    QZipReader reader2(&buffer2);

    // 2. Extract Maven version from install_profile.json data section
    //    (e.g. "1.18.2-20220404.173914" from [net.minecraft:client:1.18.2-20220404.173914:mappings@txt])
    QString mavenVer = m_mcVersion;
    {
        QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
        if (!profileDoc.isNull()) {
            QJsonObject data = profileDoc.object().value(QStringLiteral("data")).toObject();
            for (const QString& key : {QStringLiteral("MOJMAPS"), QStringLiteral("MAPPINGS")}) {
                QJsonObject entry = data.value(key).toObject();
                QString clientRef = entry.value(QStringLiteral("client")).toString();
                if (!clientRef.isEmpty()) {
                    QRegularExpression re(QStringLiteral("\\[[^:]+:[^:]+:([^:\\]]+):mappings@txt\\]"));
                    QRegularExpressionMatch match = re.match(clientRef);
                    if (match.hasMatch()) {
                        mavenVer = match.captured(1);
                        break;
                    }
                }
            }
        }
    }

    // 3. Ensure client_mappings is on disk before bootstrapper runs
    //    (Forge 1.19+ ChainMappings.process needs it for JAR remapping)
    {
        const QString vmPath = m_gameDir + QStringLiteral("/versions/") + m_mcVersion
            + QStringLiteral("/") + m_mcVersion + QStringLiteral(".json");
        QFile vf(vmPath);
        if (vf.open(QIODevice::ReadOnly)) {
            QJsonDocument vd = QJsonDocument::fromJson(vf.readAll());
            vf.close();
            QJsonObject cm = vd.object().value(QStringLiteral("downloads")).toObject()
                .value(QStringLiteral("client_mappings")).toObject();
            QString cmUrl = cm.value(QStringLiteral("url")).toString();
            if (!cmUrl.isEmpty()) {
                const QString mavenSavePath = m_gameDir
                    + QStringLiteral("/libraries/net/minecraft/client/") + mavenVer
                    + QStringLiteral("/client-") + mavenVer + QStringLiteral("-mappings.txt");
                // Also save to standard MC version path for Place 2 to find later
                const QString stdSavePath = m_gameDir
                    + QStringLiteral("/libraries/net/minecraft/client/") + m_mcVersion
                    + QStringLiteral("/client-") + m_mcVersion + QStringLiteral("-mappings.txt");
                if (!QFileInfo::exists(mavenSavePath) || (mavenVer != m_mcVersion && !QFileInfo::exists(stdSavePath))) {
                    qCInfo(logLoader) << QStringLiteral("下载缺失的 client_mappings: %1").arg(mavenVer);
                    QNetworkAccessManager nm;
                    QNetworkReply* r = nm.get(QNetworkRequest(QUrl(cmUrl)));
                    QEventLoop loop;
                    connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                    loop.exec();
                    if (r->error() == QNetworkReply::NoError) {
                        QByteArray data = r->readAll();
                        // Save to Maven path (install_profile data section path)
                        QDir().mkpath(QFileInfo(mavenSavePath).absolutePath());
                        {
                            QFile out(mavenSavePath);
                            if (out.open(QIODevice::WriteOnly)) {
                                out.write(data);
                                out.close();
                                qCInfo(logLoader) << QStringLiteral("client_mappings 已保存: %1 (%2 KB)")
                                    .arg(mavenSavePath).arg(data.size() / 1024);
                            }
                        }
                        // Also save to standard MC version path for TSRG converter
                        if (mavenVer != m_mcVersion) {
                            QDir().mkpath(QFileInfo(stdSavePath).absolutePath());
                            QFile out2(stdSavePath);
                            if (out2.open(QIODevice::WriteOnly)) {
                                out2.write(data);
                                out2.close();
                                qCInfo(logLoader) << QStringLiteral("client_mappings 已同步到标准路径: %1").arg(stdSavePath);
                            }
                        }
                    } else {
                        qCWarning(logLoader) << QStringLiteral("client_mappings 下载失败: %1").arg(r->errorString());
                    }
                }
            }
        }
    }

    m_currentStep = 3;
    emit progressChanged(3, m_totalSteps, QStringLiteral("正在准备安装..."));

    // 2. Extract bundled maven jars from installer to libraries/ (all paths benefit)
    QString libBase = m_gameDir + QStringLiteral("/libraries");
    int extractedCount = 0;
    const auto& fileList = reader2.fileInfoList();
    for (const auto& info : fileList) {
        QString fp = info.filePath;
        if (!fp.startsWith(QStringLiteral("maven/"))) continue;
        if (!fp.endsWith(QStringLiteral(".jar"))) continue;
        QString relPath = fp.mid(6);
        QString target = libBase + QStringLiteral("/") + relPath;
        if (QFile::exists(target)) continue;
        QByteArray jarBytes = reader2.fileData(fp);
        if (jarBytes.isEmpty()) continue;
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile jf(target);
        if (jf.open(QIODevice::WriteOnly)) { jf.write(jarBytes); jf.close(); extractedCount++; }
    }
    qCInfo(logLoader) << QStringLiteral("已从安装程序解压 %1 个 JAR").arg(extractedCount);

    // 3. Read install_profile.json (re-read for routing)
    QByteArray profileData2 = reader2.fileData(QStringLiteral("install_profile.json"));
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData2);
    if (!profileDoc.isObject()) {
        qCWarning(logLoader) << QStringLiteral("install_profile.json 无效，走 Bootstrapper");
        reader2.close();
        runBootstrapperProcess(jarData);
        return;
    }
    QJsonObject profileObj = profileDoc.object();

    int spec = profileObj.value(QStringLiteral("spec")).toInt(-1);
    int procCount = profileObj.value(QStringLiteral("processors")).toArray().size();
    bool hasInstall = profileObj.contains(QStringLiteral("install"));
    bool hasJson = profileObj.contains(QStringLiteral("json"));
    qCInfo(logLoader) << QStringLiteral("=== Forge install_profile 分析: spec=%1 processors=%2 install=%3 json=%4 ===")
        .arg(spec).arg(procCount).arg(hasInstall).arg(hasJson);

    // ── Four-way branch ──
    //
    // Branch A: has "install" → Legacy 2 (universal JAR + inheritsFrom)
    if (hasInstall) {
        reader2.close();
        qCInfo(logLoader) << QStringLiteral("→ 走 Legacy 2（universal JAR + inheritsFrom）");
        installLegacy2(jarData, profileObj);
        return;
    }

    // Branch B: has "json" AND no processors AND spec <= 0 → Legacy 1
    if (hasJson && procCount == 0 && spec <= 0) {
        reader2.close();
        qCInfo(logLoader) << QStringLiteral("→ 走 Legacy 1（maven/ + version.json 直接写入）");
        installLegacy1(jarData, profileObj);
        return;
    }

    // Branch C: has processors OR spec >= 1 → Bootstrapper
    reader2.close();
    qCInfo(logLoader) << QStringLiteral("→ 走 Bootstrapper（Method A：Java 注入器）");
    runBootstrapperProcess(jarData);
}

// ═══════════════════════════════════════════════════════════════
// Legacy 3: no install_profile.json → universal/client zip IS the game JAR
// For MC < 1.5 (Forge 3.x~6.x) where the "installer" is actually
// a complete forge-patched Minecraft client.
// The downloaded file IS the game JAR — copy it as-is and create
// a self-contained version JSON with inheritsFrom for libraries.
// ═══════════════════════════════════════════════════════════════
void ModLoaderInstaller::installLegacy3(const QByteArray& jarData) {
    emit progressChanged(3, m_totalSteps, QStringLiteral("安装旧版 Forge（Legacy 3：自包含 JAR）..."));

    // 1. Scan JAR contents to determine main class
    QBuffer buffer;
    buffer.setData(jarData);
    buffer.open(QIODevice::ReadOnly);
    QZipReader reader(&buffer);

    bool hasFMLRelauncher = false;
    bool hasLaunchwrapper = false;
    const auto& entries = reader.fileInfoList();
    for (const auto& e : entries) {
        if (e.filePath == QStringLiteral("cpw/mods/fml/relauncher/FMLRelauncher.class"))
            hasFMLRelauncher = true;
        if (e.filePath == QStringLiteral("net/minecraft/launchwrapper/Launch.class"))
            hasLaunchwrapper = true;
    }
    reader.close();

    QString mainClass;
    if (hasFMLRelauncher)
        mainClass = QStringLiteral("cpw.mods.fml.relauncher.FMLRelauncher");
    else if (hasLaunchwrapper)
        mainClass = QStringLiteral("net.minecraft.launchwrapper.Launch");
    else
        mainClass = QStringLiteral("net.minecraft.client.Minecraft");

    qCInfo(logLoader) << QStringLiteral("Legacy 3: mainClass=%1 (FML=%2, LaunchWrapper=%3)")
        .arg(mainClass).arg(hasFMLRelauncher).arg(hasLaunchwrapper);

    // 2. Create version directory
    const QString verDir = versionsDir() + QStringLiteral("/") + m_installName;
    QDir().mkpath(verDir);

    // 3. Copy JAR data to version folder (this IS the game JAR)
    const QString jarPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
    {
        QFile jf(jarPath);
        if (jf.open(QIODevice::WriteOnly)) {
            jf.write(jarData);
            jf.close();
            qCInfo(logLoader) << QStringLiteral("Legacy 3: 已写入版本 JAR %1 (%2 bytes)")
                .arg(jarPath).arg(jarData.size());
        } else {
            emit finished(false, QStringLiteral("Legacy 3: 无法写入版本 JAR"));
            m_running = false;
            return;
        }
    }

    // 4. Create version JSON with inheritsFrom (inherit MC's libraries)
    //    but override jar and mainClass to use our Forge-patched JAR.
    QJsonObject versionJson;
    versionJson[QStringLiteral("id")] = m_installName;
    versionJson[QStringLiteral("type")] = QStringLiteral("release");
    versionJson[QStringLiteral("mainClass")] = mainClass;
    versionJson[QStringLiteral("inheritsFrom")] = m_mcVersion;
    versionJson[QStringLiteral("jar")] = m_installName;  // use our JAR
    versionJson[QStringLiteral("minimumLauncherVersion")] = 4;
    versionJson[QStringLiteral("libraries")] = QJsonArray();  // all libraries from inheritsFrom

    // 5. Write version JSON
    const QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
    {
        QFile jf(jsonPath);
        if (jf.open(QIODevice::WriteOnly)) {
            jf.write(QJsonDocument(versionJson).toJson(QJsonDocument::Indented));
            jf.close();
            qCInfo(logLoader) << QStringLiteral("Legacy 3: 已写入版本 JSON %1").arg(jsonPath);
        } else {
            emit finished(false, QStringLiteral("Legacy 3: 无法写入版本 JSON"));
            m_running = false;
            return;
        }
    }

    qCInfo(logLoader) << QStringLiteral("Legacy 3 安装完成: %1（自包含 JAR，mainClass=%2）")
        .arg(m_installName, mainClass);
    emit finished(true, QString());
    m_running = false;
}

// ═══════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════
// Legacy 2: has "install" field → universal JAR + standalone JSON
// (Forge pre-1.13 style, e.g. 1.7.10, 1.12.2)
// Now flattens to standalone via flattenVersionJson → vanilla folder can be cleaned up
// ═══════════════════════════════════════════════════════════════
static QJsonObject flattenVersionJson(const QString& gameDir, QJsonObject child);
void ModLoaderInstaller::installLegacy2(const QByteArray& jarData, const QJsonObject& profile) {
    emit progressChanged(3, m_totalSteps, QStringLiteral("安装旧版 Forge（Legacy 2）..."));

    // Build Maven version with branch suffix
    const QString ver = m_mcVersion + QStringLiteral("-") + m_loaderVersion
        + (m_forgeBranch.isEmpty() ? QString() : QStringLiteral("-") + m_forgeBranch);
    const QString groupPath = QStringLiteral("net/minecraftforge/forge");
    const QString filePrefix = QStringLiteral("forge");

    // 1. Extract universal JAR to libraries/
    const QString jarDst = m_gameDir + QStringLiteral("/libraries/") + groupPath
        + QStringLiteral("/") + ver + QStringLiteral("/")
        + filePrefix + QStringLiteral("-") + ver + QStringLiteral(".jar");
    QDir().mkpath(QFileInfo(jarDst).absolutePath());

    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序");
        m_running = false; return;
    }
    QZipReader reader(&buffer);
    QByteArray universalBytes;
    QString installFilePath = profile.value(QStringLiteral("install")).toObject()
        .value(QStringLiteral("filePath")).toString();
    const QStringList candidates = {
        QStringLiteral("maven/%1/%2/%3-%2-universal.jar").arg(groupPath, ver, filePrefix),
        QStringLiteral("maven/%1/%2/%3-%2.jar").arg(groupPath, ver, filePrefix),
        installFilePath,
    };
    for (const auto& c : candidates) {
        if (c.isEmpty()) continue;
        universalBytes = reader.fileData(c);
        if (!universalBytes.isEmpty()) break;
    }
    reader.close();
    if (universalBytes.isEmpty()) {
        emit finished(false, "Legacy 2: 安装程序中未找到 universal JAR");
        m_running = false; return;
    }
    QFile jf(jarDst);
    if (jf.open(QIODevice::WriteOnly)) { jf.write(universalBytes); jf.close(); }

    // 2. Write version JSON with inheritsFrom
    QJsonObject vInfo = profile.value(QStringLiteral("versionInfo")).toObject();
    vInfo[QStringLiteral("id")] = m_installName;
    if (!vInfo.contains(QStringLiteral("inheritsFrom")))
        vInfo[QStringLiteral("inheritsFrom")] = m_mcVersion;
    if (!vInfo.contains(QStringLiteral("mainClass")))
        vInfo[QStringLiteral("mainClass")] = QStringLiteral("net.minecraft.launchwrapper.Launch");

    QString verDir = versionsDir() + QStringLiteral("/") + m_installName;
    QDir().mkpath(verDir);

    // Flatten to standalone: copy vanilla JAR + flatten inheritsFrom chain
    // (Same approach as Legacy 1 — makes forge folder self-contained, allows vanilla cleanup)
    {
        QString srcJar = versionsDir() + QStringLiteral("/") + m_mcVersion
            + QStringLiteral("/") + m_mcVersion + QStringLiteral(".jar");
        QString dstJar = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (!QFile::exists(dstJar) && QFile::exists(srcJar)) {
            QFile::copy(srcJar, dstJar);
        }
        QJsonObject flattened = flattenVersionJson(m_gameDir, vInfo);
        if (flattened != vInfo) {
            vInfo = flattened;
            qCInfo(logLoader) << QStringLiteral("Legacy 2 JSON 已压平为独立版本（inheritsFrom 链已消解）");
        }
    }

    // Pre-download forge-specific libraries (same as Legacy 1)
    {
        QJsonArray libs = vInfo.value(QStringLiteral("libraries")).toArray();
        if (!libs.isEmpty()) {
            QNetworkAccessManager* nam = HttpClient::instance().manager();
            int downloaded = 0;
            for (const auto& lv : libs) {
                if (!lv.isObject()) continue;
                QJsonObject libObj = lv.toObject();
                QString name = libObj.value(QStringLiteral("name")).toString();
                if (name.isEmpty()) continue;
                QStringList parts = name.split(QLatin1Char(':'));
                if (parts.size() < 3) continue;
                QString group = parts[0].replace(QLatin1Char('.'), QLatin1Char('/'));
                QString artifact = parts[1];
                QString version = parts[2];
                QString ext = QStringLiteral("jar");
                if (version.contains(QLatin1Char('@'))) {
                    int atIdx = version.indexOf(QLatin1Char('@'));
                    ext = version.mid(atIdx + 1);
                    version = version.left(atIdx);
                }
                QString libDir = m_gameDir + QStringLiteral("/libraries/") + group
                    + QStringLiteral("/") + artifact + QStringLiteral("/") + version;
                QString libFile = libDir + QStringLiteral("/") + artifact
                    + QStringLiteral("-") + version + QStringLiteral(".") + ext;
                if (QFile::exists(libFile)) continue;
                QString url = QStringLiteral("https://bmclapi2.bangbang93.com/maven/%1/%2/%3/%2-%3.%4")
                    .arg(group, artifact, version, ext);
                QDir().mkpath(libDir);
                QNetworkRequest req;
                req.setUrl(QUrl(url));
                QNetworkReply* reply = nam->get(req);
                QEventLoop loop;
                QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                QTimer timer;
                timer.setSingleShot(true);
                QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                timer.start(30000);
                loop.exec();
                if (reply->error() == QNetworkReply::NoError && timer.isActive()) {
                    timer.stop();
                    QFile f(libFile);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(reply->readAll());
                        f.close();
                        downloaded++;
                    }
                } else {
                    qCWarning(logLoader) << QStringLiteral("Legacy 2 库下载失败: %1 %2")
                        .arg(url, reply->errorString());
                    QFile::remove(libFile);
                }
                reply->deleteLater();
            }
            if (downloaded > 0)
                qCInfo(logLoader) << QStringLiteral("已为 Legacy 2 预下载 %1 个 Forge 库").arg(downloaded);
        }
    }

    QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
    QFile jf2(jsonPath);
    if (jf2.open(QIODevice::WriteOnly)) {
        jf2.write(QJsonDocument(vInfo).toJson(QJsonDocument::Indented));
        jf2.close();
    }

    qCInfo(logLoader) << QStringLiteral("Legacy 2 安装完成: %1（独立版本）").arg(m_installName);
    emit finished(true, QString());
    m_running = false;
}

// ═══════════════════════════════════════════════════════════════
// Legacy 1: has "json" field, no install, no processors
// → extract version.json from installer + flatten + download libs
// (Forge ~1.13-1.16.x, spec 0)
// ═══════════════════════════════════════════════════════════════
void ModLoaderInstaller::installLegacy1(const QByteArray& jarData, const QJsonObject& profile) {
    emit progressChanged(3, m_totalSteps, QStringLiteral("安装旧版 Forge（Legacy 1）..."));

    QString versionJsonPath = profile.value(QStringLiteral("json")).toString();
    if (versionJsonPath.isEmpty()) {
        emit finished(false, "Legacy 1: install_profile 中无 json 路径");
        m_running = false; return;
    }

    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序");
        m_running = false; return;
    }
    QZipReader reader(&buffer);

    // 1. Read version.json from installer JAR
    QString actualPath = versionJsonPath.startsWith(QLatin1Char('/'))
        ? versionJsonPath.mid(1) : versionJsonPath;
    QByteArray versionData = reader.fileData(actualPath);
    reader.close();
    if (versionData.isEmpty()) {
        emit finished(false, "安装程序中未找到 version.json");
        m_running = false; return;
    }
    QJsonDocument versionDoc = QJsonDocument::fromJson(versionData);
    if (!versionDoc.isObject()) {
        emit finished(false, "version.json 格式无效");
        m_running = false; return;
    }
    QJsonObject versionJson = versionDoc.object();

    // 2. Set correct id
    versionJson[QStringLiteral("id")] = m_installName;

    // 3. Create version directory
    QString verDir = versionsDir() + QStringLiteral("/") + m_installName;
    QDir().mkpath(verDir);

    // 4. Copy vanilla client.jar to version dir (needed after flatten)
    QString srcClientJar = versionsDir() + QStringLiteral("/") + m_mcVersion
        + QStringLiteral("/") + m_mcVersion + QStringLiteral(".jar");
    QString dstClientJar = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
    if (!QFile::exists(dstClientJar) && QFile::exists(srcClientJar)) {
        if (QFile::copy(srcClientJar, dstClientJar))
            qCInfo(logLoader) << QStringLiteral("已复制客户端 JAR: %1").arg(dstClientJar);
        else
            qCWarning(logLoader) << QStringLiteral("复制客户端 JAR 失败: %1").arg(srcClientJar);
    }

    // 5. Ensure inheritsFrom is set, then flatten into standalone
    if (!versionJson.contains(QStringLiteral("inheritsFrom")))
        versionJson[QStringLiteral("inheritsFrom")] = m_mcVersion;
    QJsonObject flattened = flattenVersionJson(m_gameDir, versionJson);
    if (flattened != versionJson) {
        versionJson = flattened;
        qCInfo(logLoader) << QStringLiteral("version.json 已压平为独立版本（inheritsFrom 链已消解）");
    }

    // 6. Pre-download Forge-specific libraries from BMCLAPI
    //    (similar to step 3d in runBootstrapperProcess)
    QJsonArray libs = versionJson.value(QStringLiteral("libraries")).toArray();
    if (!libs.isEmpty()) {
        QNetworkAccessManager* nam = HttpClient::instance().manager();
        int downloaded = 0;
        for (const auto& lv : libs) {
            if (!lv.isObject()) continue;
            QJsonObject libObj = lv.toObject();
            QString name = libObj.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) continue;
            // Parse Maven coordinate
            QStringList parts = name.split(QLatin1Char(':'));
            if (parts.size() < 3) continue;
            QString group = parts[0].replace(QLatin1Char('.'), QLatin1Char('/'));
            QString artifact = parts[1];
            QString version = parts[2];
            QString ext = QStringLiteral("jar");
            if (version.contains(QLatin1Char('@'))) {
                int atIdx = version.indexOf(QLatin1Char('@'));
                ext = version.mid(atIdx + 1);
                version = version.left(atIdx);
            }
            // Build local path
            QString libDir = m_gameDir + QStringLiteral("/libraries/") + group
                + QStringLiteral("/") + artifact + QStringLiteral("/") + version;
            QString libFile = libDir + QStringLiteral("/") + artifact
                + QStringLiteral("-") + version + QStringLiteral(".") + ext;
            if (QFile::exists(libFile)) continue;
            // Download from BMCLAPI mirror
            QString url = QStringLiteral("https://bmclapi2.bangbang93.com/maven/%1/%2/%3/%2-%3.%4")
                .arg(group, artifact, version, ext);
            QDir().mkpath(libDir);
            QNetworkRequest req;
            req.setUrl(QUrl(url));
            QNetworkReply* reply = nam->get(req);
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(30000);
            loop.exec();
            if (reply->error() == QNetworkReply::NoError && timer.isActive()) {
                timer.stop();
                QFile f(libFile);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(reply->readAll());
                    f.close();
                    downloaded++;
                }
            } else {
                qCWarning(logLoader) << QStringLiteral("Legacy 1 库下载失败: %1 %2")
                    .arg(url, reply->errorString());
                QFile::remove(libFile);
            }
            reply->deleteLater();
        }
        if (downloaded > 0)
            qCInfo(logLoader) << QStringLiteral("已为 Legacy 1 预下载 %1 个 Forge 库").arg(downloaded);
    }

    // 7. Write flattened version.json
    QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
    QFile jf(jsonPath);
    if (jf.open(QIODevice::WriteOnly)) {
        jf.write(QJsonDocument(versionJson).toJson(QJsonDocument::Indented));
        jf.close();
    }

    qCInfo(logLoader) << QStringLiteral("Legacy 1 安装完成: %1（独立版本，%2 个库已就绪）")
        .arg(m_installName).arg(libs.size());
    emit finished(true, QString());
    m_running = false;
}

// ═══════════════════════════════════════════════════════════════
// Method A: Bootstrapper (Java subprocess via forge-install-bootstrapper.jar)
// For: spec>=1 / has processors / NeoForge
// ═══════════════════════════════════════════════════════════════
QString ModLoaderInstaller::extractBootstrapperPath() {
    // Extract forge-installer.jar (helper, contains com.bangbang93.ForgeInstaller)
    QString dst = QDir::tempPath() + QStringLiteral("/forge-installer.jar");
    if (QFile::exists(dst)) return dst;
    QFile res(QStringLiteral(":/resources/tools/forge-installer.jar"));
    if (!res.open(QIODevice::ReadOnly)) {
        qCWarning(logLoader) << "无法打开嵌入式 forge-installer.jar 资源";
        return QString();
    }
    QByteArray data = res.readAll();
    res.close();
    QFile f(dst);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(data);
        f.close();
        qCInfo(logLoader) << QStringLiteral("已释放 forge-installer.jar 到 %1（%2 字节）").arg(dst).arg(data.size());
        return dst;
    }
    qCWarning(logLoader) << QStringLiteral("无法写入 forge-installer.jar 到 %1").arg(dst);
    return QString();
}

QString ModLoaderInstaller::extractJavaWrapperPath() {
    // Extract java-wrapper.jar (oolloo.jlw.Wrapper, fixes CJK encoding on Windows)
    QString dst = QDir::tempPath() + QStringLiteral("/java-wrapper.jar");
    if (QFile::exists(dst)) return dst;
    QFile res(QStringLiteral(":/resources/tools/java-wrapper.jar"));
    if (!res.open(QIODevice::ReadOnly)) {
        qCWarning(logLoader) << "无法打开嵌入式 java-wrapper.jar 资源";
        return QString();
    }
    QByteArray data = res.readAll();
    res.close();
    QFile f(dst);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(data);
        f.close();
        qCInfo(logLoader) << QStringLiteral("已释放 java-wrapper.jar 到 %1（%2 字节）").arg(dst).arg(data.size());
        return dst;
    }
    qCWarning(logLoader) << QStringLiteral("无法写入 java-wrapper.jar 到 %1").arg(dst);
    return QString();
}

// Forward declaration for Java version parsing used by findJavaPath and runBootstrapperProcess
static int parseJavaMajorVersion(const QString& output);
static int minJavaForMcInstaller(const QString& mcVersion);

QString ModLoaderInstaller::findJavaPath(int minVersion) {
    // 1. Try PATH
    QProcess proc;
    if (proc.start(QStringLiteral("java"), {QStringLiteral("-version")});
        proc.waitForFinished(5000) && proc.exitCode() == 0) {
        int major = parseJavaMajorVersion(QString::fromUtf8(proc.readAllStandardError()));
        if (major >= minVersion) {
            qCInfo(logLoader) << QStringLiteral("PATH 上找到 Java %1: java").arg(major);
            return QStringLiteral("java");
        }
    }

    // 2. Check auto-download cache first (from downloadAndExtractJava)
    {
        const QString cacheExe = QDir::currentPath() + QStringLiteral("/java_cache/%1/bin/java.exe").arg(minVersion);
        if (QFile::exists(cacheExe)) {
            QProcess cacheProc;
            cacheProc.start(cacheExe, {QStringLiteral("-version")});
            if (cacheProc.waitForFinished(5000) && cacheProc.exitCode() == 0) {
                int major = parseJavaMajorVersion(QString::fromUtf8(cacheProc.readAllStandardError()));
                if (major >= minVersion) {
                    qCInfo(logLoader) << QStringLiteral("在 java_cache 找到 Java %1: %2").arg(major).arg(cacheExe);
                    return cacheExe;
                }
            }
        }
    }

    // 3. Check common install dirs — return first match >= minVersion
    //    Track lower versions as fallback.
    QStringList candidates = {
        QStringLiteral("C:/Program Files/Java"),
        QStringLiteral("C:/Program Files/Eclipse Adoptium"),
        QStringLiteral("C:/Program Files/Adoptium"),
        QStringLiteral("C:/Program Files/Amazon Corretto"),
        QStringLiteral("C:/Program Files (x86)/Java"),
    };
    QString bestPath;
    int bestMajor = 0;
    for (const auto& dir : candidates) {
        QDir d(dir);
        if (!d.exists()) continue;
        for (const auto& sub : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString exe = dir + QStringLiteral("/") + sub + QStringLiteral("/bin/java.exe");
            if (!QFile::exists(exe)) continue;
            proc.start(exe, {QStringLiteral("-version")});
            if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
                int major = parseJavaMajorVersion(QString::fromUtf8(proc.readAllStandardError()));
                if (major >= minVersion) {
                    qCInfo(logLoader) << QStringLiteral("在 %1 找到 Java %2").arg(exe).arg(major);
                    return exe;
                }
                if (major > bestMajor) {
                    bestMajor = major;
                    bestPath = exe;
                }
            }
        }
    }

    // 3. Fallback if we have something (but below minimum)
    if (!bestPath.isEmpty()) {
        qCInfo(logLoader) << QStringLiteral("在 %1 找到 Java %2（要求 ≥%3）").arg(bestPath).arg(bestMajor).arg(minVersion);
        return bestPath;
    }

    qCWarning(logLoader) << QStringLiteral("未找到 Java ≥%1").arg(minVersion);
    return QString();
}

/** Auto-download Java from Tuna Adoptium mirror, extract ZIP to java_cache/{minVersion}/.
 *  Returns path to java.exe, or empty on failure. */
QString ModLoaderInstaller::downloadAndExtractJava(int minVersion) {
    const QString baseDir = QDir::currentPath() + QStringLiteral("/java_cache/");
    const QString javaDir = baseDir + QString::number(minVersion);
    const QString javaExe = javaDir + QStringLiteral("/bin/java.exe");

    // Already downloaded?
    if (QFile::exists(javaExe)) {
        qCInfo(logLoader) << QStringLiteral("Java %1 已存在于 java_cache: %2").arg(minVersion).arg(javaExe);
        return javaExe;
    }

    // Fetch directory listing from Tuna Adoptium mirror
    const QString mirrorBase = QStringLiteral("https://mirrors.tuna.tsinghua.edu.cn/Adoptium/%1/jdk/x64/windows/")
                                   .arg(minVersion);
    QNetworkAccessManager* nam = HttpClient::instance().manager();
    QNetworkRequest req;
    req.setUrl(QUrl(mirrorBase));
    req.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    QNetworkReply* reply = nam->get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError || !timer.isActive()) {
        qCWarning(logLoader) << QStringLiteral("获取 Java %1 文件列表失败: %2").arg(minVersion).arg(reply->errorString());
        reply->deleteLater();
        return {};
    }

    // Parse HTML for .zip file links
    QString html = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    static QRegularExpression zipRe(QLatin1String("<a href=\"([^\"]+\\.zip)\""));
    QStringList zipUrls;
    auto it = zipRe.globalMatch(html);
    while (it.hasNext()) {
        auto m = it.next();
        zipUrls.append(mirrorBase + m.captured(1));
    }
    if (zipUrls.isEmpty()) {
        qCWarning(logLoader) << QStringLiteral("Java %1 镜像中未找到 .zip 文件").arg(minVersion);
        return {};
    }

    // Download the latest .zip (last entry in alphabetical order)
    const QString zipUrl = zipUrls.last();
    const QString zipPath = baseDir + QStringLiteral("/jdk-%1.zip").arg(minVersion);
    QDir().mkpath(baseDir);

    qCInfo(logLoader) << QStringLiteral("正在下载 Java %1: %2").arg(minVersion).arg(zipUrl);
    emit progressChanged(3, m_totalSteps, QStringLiteral("正在下载 Java %1...").arg(minVersion));

    QNetworkRequest zipReq;
    zipReq.setUrl(QUrl(zipUrl));
    zipReq.setRawHeader("User-Agent", "ShadowLauncher/1.0");
    QNetworkReply* zipReply = nam->get(zipReq);
    QEventLoop zipLoop;
    QObject::connect(zipReply, &QNetworkReply::finished, &zipLoop, &QEventLoop::quit);
    QTimer zipTimer;
    zipTimer.setSingleShot(true);
    QObject::connect(&zipTimer, &QTimer::timeout, &zipLoop, &QEventLoop::quit);
    // Large file: 3 minute timeout
    zipTimer.start(180000);
    zipLoop.exec();

    if (zipReply->error() != QNetworkReply::NoError || !zipTimer.isActive()) {
        qCWarning(logLoader) << QStringLiteral("下载 Java %1 ZIP 失败: %2").arg(minVersion).arg(zipReply->errorString());
        zipReply->deleteLater();
        return {};
    }

    QByteArray zipData = zipReply->readAll();
    zipReply->deleteLater();

    // Extract ZIP using QZipReader
    qCInfo(logLoader) << QStringLiteral("正在解压 Java %1 (%2 MB)...").arg(minVersion).arg(zipData.size() / 1048576);
    emit progressChanged(3, m_totalSteps, QStringLiteral("正在解压 Java %1...").arg(minVersion));

    QBuffer buf;
    buf.setData(zipData);
    if (!buf.open(QIODevice::ReadOnly)) {
        QFile::remove(zipPath);
        return {};
    }
    {
        QZipReader reader(&buf);
        const QList<QZipReader::FileInfo> entries = reader.fileInfoList();
        int extracted = 0;
        for (const auto& entry : entries) {
            if (entry.isDir || entry.isSymLink) continue;
            QString outPath = javaDir + QStringLiteral("/") + entry.filePath;
            QDir().mkpath(QFileInfo(outPath).absolutePath());
            QFile out(outPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(reader.fileData(entry.filePath));
                out.close();
                extracted++;
            }
        }
        reader.close();
        qCInfo(logLoader) << QStringLiteral("Java %1 解压完成: %2 个文件").arg(minVersion).arg(extracted);
    }

    if (!QFile::exists(javaExe)) {
        qCWarning(logLoader) << QStringLiteral("Java %1 解压后未找到 java.exe").arg(minVersion);
        return {};
    }

    qCInfo(logLoader) << QStringLiteral("Java %1 已就绪: %2").arg(minVersion).arg(javaExe);
    return javaExe;
}

static int parseJavaMajorVersion(const QString& output) {
    QRegularExpression re(QStringLiteral("version\\s+\"([^\"]+)\""));
    auto m = re.match(output);
    if (!m.hasMatch()) return 0;
    QString ver = m.captured(1);
    ver.replace(QLatin1Char('_'), QLatin1Char('.'));
    QStringList parts = ver.split(QLatin1Char('.'));
    if (parts.isEmpty()) return 0;
    int major = parts[0].toInt();
    if (major == 1 && parts.size() > 1)
        major = parts[1].toInt();  // 1.8.0 → 8
    return major;
}

/**
 * Minimum Java version for the Forge/NeoForge installer bootstrapper,
 * based on MC version. MC 1.18+ Forge installers need Java 17+
 * for their post-processors (FART, srgutils).
 */
static int minJavaForMcInstaller(const QString& mcVersion) {
    QStringList parts = mcVersion.split(QLatin1Char('.'));
    if (parts.size() >= 2) {
        int major = parts[0].toInt();
        int minor = parts[1].toInt();
        // MC 1.18+ → Forge installer needs Java 17+
        if (major >= 2 || (major == 1 && minor >= 18))
            return 17;
    }
    return 8;
}


void ModLoaderInstaller::runBootstrapperProcess(const QByteArray& jarData) {
    const bool isNeoForge = (m_loaderType == QStringLiteral("neoforge"));
    emit progressChanged(3, m_totalSteps, isNeoForge
        ? QStringLiteral("正在通过 Bootstrapper 安装 NeoForge...")
        : QStringLiteral("正在通过 Bootstrapper 安装 Forge..."));

    // 1. Find Java — minimum version depends on MC version
    //    MC 1.18+ Forge installers need Java 17+ for their post-processors (FART, srgutils)
    int minJava = minJavaForMcInstaller(m_mcVersion);
    QString javaPath = findJavaPath(minJava);
    if (javaPath.isEmpty()) {
        qCInfo(logLoader) << QStringLiteral("未找到 Java %1+，尝试自动下载...").arg(minJava);
        emit progressChanged(3, m_totalSteps, QStringLiteral("未找到 Java %1+，正在自动下载...").arg(minJava));
        javaPath = downloadAndExtractJava(minJava);
        if (javaPath.isEmpty()) {
            emit finished(false, QStringLiteral("未找到 Java %1+，且自动下载失败。请先在「设置 → Java」中下载 Java。").arg(minJava));
            m_running = false;
            return;
        }
        qCInfo(logLoader) << QStringLiteral("自动下载/解压 Java 完成: %1").arg(javaPath);
    }

    // 2. Extract bootstrapper JAR (same bootstrapper for Forge and NeoForge, matching 主流启动器's ForgelikeInjector)
    QString bootstrapperJar = extractBootstrapperPath();
    if (bootstrapperJar.isEmpty()) {
        emit finished(false, "无法释放 Bootstrapper JAR");
        m_running = false;
        return;
    }

    QString installerJarPath;

    // 3. Write installer JAR to temp
    //    主流启动器 does NOT patch --skipIfExists on DOWNLOAD_MOJMAPS.
    //    The DOWNLOAD_MOJMAPS processor natively checks whether the .txt mapping exists
    //    and skips the download if it does (but still runs the TSRG conversion internally).
    //    Patching --skipIfExists was harmful: it made DOWNLOAD_MOJMAPS check for .tsrg (not .txt),
    //    skip even when .txt existed, and never produce .tsrg — crashing FART.
    //    We write the installer JAR as-is, matching 主流启动器's approach.
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (!isNeoForge) {
        // ── Forge: write installer JAR as-is (no patching, matching 主流启动器) ──
        QBuffer buf;
        buf.setData(jarData);
        if (!buf.open(QIODevice::ReadOnly)) {
            emit finished(false, "无法打开安装程序");
            m_running = false; return;
        }
        QZipReader reader(&buf);

        // Strip signature files (META-INF/*.SF, *.RSA, etc.) so Java doesn't reject the JAR
        QList<QZipReader::FileInfo> entries = reader.fileInfoList();
        QZipWriter writer(tempDir + QStringLiteral("/forge-installer-") + m_installName + QStringLiteral(".jar"));
        for (const auto& e : entries) {
            if (e.isDir) writer.addDirectory(e.filePath);
            else if (e.isSymLink) qCWarning(logLoader) << QStringLiteral("installer JAR 含符号链接，跳过: %1").arg(e.filePath);
            else {
                QString fp = e.filePath;
                if (fp.startsWith(QStringLiteral("META-INF/")) && (fp == QStringLiteral("META-INF/MANIFEST.MF")
                    || fp.endsWith(QStringLiteral(".SF")) || fp.endsWith(QStringLiteral(".RSA"))
                    || fp.endsWith(QStringLiteral(".DSA")) || fp.endsWith(QStringLiteral(".EC")))) continue;
                writer.addFile(fp, reader.fileData(fp));
            }
        }
        writer.close();
        reader.close();
        installerJarPath = tempDir + QStringLiteral("/forge-installer-") + m_installName + QStringLiteral(".jar");
        qCInfo(logLoader) << QStringLiteral("已写入安装程序（已剥离签名）: %1").arg(installerJarPath);
    } else {
        // ── NeoForge: write JAR as-is (no patching needed, bootstrapper handles everything) ──
        installerJarPath = tempDir + QStringLiteral("/neoforge-installer-") + m_installName + QStringLiteral(".jar");
        {
            QFile f(installerJarPath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(jarData);
                f.close();
            }
        }
        qCInfo(logLoader) << QStringLiteral("已写入 NeoForge 安装程序: %1").arg(installerJarPath);
    }

    // 3b. Ensure client JAR exists at Forge-installer-expected path
    // (Forge's FART processor and post-processors look for the client jar
    //  in libraries/net/minecraft/client/<version>/ — NOT in versions/.)
    QString clientVerDir = m_gameDir + QStringLiteral("/versions/") + m_mcVersion;
    QString clientJarPath = clientVerDir + QStringLiteral("/") + m_mcVersion + QStringLiteral(".jar");
    QString clientLibDir = m_gameDir + QStringLiteral("/libraries/net/minecraft/client/") + m_mcVersion;
    QString clientLibPath = clientLibDir + QStringLiteral("/client-") + m_mcVersion + QStringLiteral(".jar");
    if (QFile::exists(clientJarPath) && !QFile::exists(clientLibPath)) {
        QDir().mkpath(clientLibDir);
        if (QFile::copy(clientJarPath, clientLibPath))
            qCInfo(logLoader) << QStringLiteral("已复制客户端 JAR 到库目录: %1").arg(clientLibPath);
        else
            qCWarning(logLoader) << QStringLiteral("复制客户端 JAR 失败: %1 → %2").arg(clientJarPath, clientLibPath);
    }

    // 3d. Download install_profile libraries to Maven paths
    // The Forge installer's post-processors (DOWNLOAD_MOJMAPS, FART, binarypatcher)
    // need their JARs to be available locally. If they're not here, the installer
    // tries to download from official Maven which may be slow or blocked in China.
    // We pre-download using BMCLAPI mirror so the installer finds them and skips.
    {
        QBuffer buffer;
        buffer.setData(jarData);
        if (buffer.open(QIODevice::ReadOnly)) {
            QZipReader reader(&buffer);
            // 主流启动器: read BOTH install_profile.json AND version.json, merge them, extract libs
            QByteArray profData = reader.fileData(QStringLiteral("install_profile.json"));
            QByteArray verData = reader.fileData(QStringLiteral("version.json"));
            reader.close();
            if (!profData.isEmpty()) {
                QJsonObject merged = QJsonDocument::fromJson(profData).object();
                if (!verData.isEmpty()) {
                    QJsonObject verObj = QJsonDocument::fromJson(verData).object();
                    // Merge version.json fields into install_profile.json (主流启动器: Json.Merge(Json2))
                    for (auto it = verObj.begin(); it != verObj.end(); ++it) {
                        if (!merged.contains(it.key()))
                            merged[it.key()] = it.value();
                    }
                    // Merge libraries from both
                    QJsonArray profLibs = merged.value(QStringLiteral("libraries")).toArray();
                    QJsonArray verLibs = verObj.value(QStringLiteral("libraries")).toArray();
                    for (const auto& vl : verLibs) {
                        QString vlName = vl.toObject().value(QStringLiteral("name")).toString();
                        if (vlName.isEmpty()) continue;
                        bool found = false;
                        for (const auto& pl : profLibs) {
                            if (pl.toObject().value(QStringLiteral("name")).toString() == vlName) {
                                found = true; break;
                            }
                        }
                        if (!found)
                            profLibs.append(vl);
                    }
                    merged[QStringLiteral("libraries")] = profLibs;
                }
                QJsonArray libs = merged.value(QStringLiteral("libraries")).toArray();
                QNetworkAccessManager* nam = HttpClient::instance().manager();

                // Helper to mirror official Maven URL to BMCLAPI
                auto bmclapiMirror = [](const QString& officialUrl) -> QString {
                    QString result = officialUrl;
                    result.replace(QStringLiteral("https://maven.neoforged.net/releases"),
                                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
                    result.replace(QStringLiteral("https://files.minecraftforge.net/maven"),
                                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
                    result.replace(QStringLiteral("https://libraries.minecraft.net"),
                                   QStringLiteral("https://bmclapi2.bangbang93.com/libraries"));
                    result.replace(QStringLiteral("https://repo1.maven.org/maven2"),
                                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
                    result.replace(QStringLiteral("https://maven.minecraftforge.net"),
                                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
                    result.replace(QStringLiteral("https://repo.maven.apache.org/maven2"),
                                   QStringLiteral("https://bmclapi2.bangbang93.com/maven"));
                    return result;
                };

                for (const auto& lv : libs) {
                    // Determine download URL from library entry
                    QString officialUrl;
                    QString name;
                    QString classifier;

                    if (lv.isObject()) {
                        QJsonObject lo = lv.toObject();
                        name = lo.value(QStringLiteral("name")).toString();
                        // If the library object has an explicit download URL, use it directly
                        QJsonObject artifactUrl = lo.value(QStringLiteral("downloads")).toObject()
                                                    .value(QStringLiteral("artifact")).toObject();
                        if (!artifactUrl.isEmpty()) {
                            // Extract classifier from the URL path if present
                            QString urlStr = artifactUrl.value(QStringLiteral("url")).toString();
                            if (!urlStr.isEmpty()) {
                                officialUrl = urlStr;
                                // Derive classifier from filename like "neoforge-26.2.0.32-beta-universal.jar"
                                QString fileName = urlStr.mid(urlStr.lastIndexOf(QLatin1Char('/')) + 1);
                                QString bare = name.section(QLatin1Char(':'), 1, 1) + QStringLiteral("-")
                                             + name.section(QLatin1Char(':'), 2, 2);
                                if (fileName.startsWith(bare) && fileName.contains(QLatin1Char('-'))) {
                                    // Extra parts in filename beyond "artifact-version" indicate classifier
                                    QString middle = fileName.mid(bare.length());
                                    // middle can be "-classifier.ext" or just ".ext"
                                    if (middle.contains(QLatin1Char('.')) && middle.indexOf(QLatin1Char('.')) > 0) {
                                        classifier = middle.section(QLatin1Char('.'), 0, 0).mid(1); // strip leading -
                                    }
                                }
                            }
                        }
                    } else {
                        name = lv.toString();
                    }

                    if (name.isEmpty()) continue;

                    // Parse Maven coordinate: group:artifact:version[:classifier][@ext]
                    QStringList parts = name.split(QLatin1Char(':'));
                    if (parts.size() < 3) continue;
                    QString group = parts[0].replace(QLatin1Char('.'), QLatin1Char('/'));
                    QString artifactName = parts[1];
                    QString version = parts[2];
                    QString ext = QStringLiteral("jar");

                    // Handle version@ext notation
                    if (version.contains(QLatin1Char('@'))) {
                        int atIdx = version.indexOf(QLatin1Char('@'));
                        ext = version.mid(atIdx + 1);
                        version = version.left(atIdx);
                    }

                    // Handle classifier: Maven coordinate 4th field (group:artifact:version:classifier)
                    if (parts.size() >= 4 && classifier.isEmpty()) {
                        classifier = parts[3];
                    }

                    // Skip pre-download for the main loader artifact when:
                    // 1. It's a string-format library (no downloads.artifact.url) AND
                    // 2. It matches the NeoForge loader (purpose of 26.2.x installer)
                    // The main NeoForge JAR uses "universal" classifier not encoded in Maven coordinate string
                    // → our reconstructed URL would point to a non-existent file
                    // → bootstrapper handles this artifact correctly using its own resolver
                    if (officialUrl.isEmpty()
                        && m_loaderType == QStringLiteral("neoforge")
                        && group == QStringLiteral("net/neoforged")
                        && artifactName == QStringLiteral("neoforge")) {
                        qCInfo(logLoader) << QStringLiteral("跳过主 NeoForge loader 预下载（bootstrapper 自行处理）: %1").arg(name);
                        continue;
                    }

                    // Skip pre-download for Forge main artifacts when URL is empty (embedded in installer JAR).
                    // Matches 主流启动器's removal: "forge-{ver}.jar" and "forge-{ver}-client.jar" from the library list.
                    // These files are NOT published on Maven — the bootstrapper extracts them from the installer JAR.
                    if (officialUrl.isEmpty()
                        && m_loaderType == QStringLiteral("forge")
                        && group == QStringLiteral("net/minecraftforge")
                        && artifactName == QStringLiteral("forge")) {
                        qCInfo(logLoader) << QStringLiteral("跳过主 Forge artifact 预下载（bootstrapper 自行处理，URL 为空）: %1").arg(name);
                        continue;
                    }

                    // Build local path
                    QString libDir = m_gameDir + QStringLiteral("/libraries/") + group + QStringLiteral("/") + artifactName + QStringLiteral("/") + version;
                    QString classifierSuffix = classifier.isEmpty() ? QString() : (QStringLiteral("-") + classifier);
                    QString libFile = libDir + QStringLiteral("/") + artifactName + QStringLiteral("-") + version + classifierSuffix + QStringLiteral(".") + ext;
                    if (QFile::exists(libFile)) continue;

                    // Build download URLs
                    QStringList urls;
                    if (!officialUrl.isEmpty()) {
                        // Use the exact URL from install_profile.json (mirror to BMCLAPI too)
                        urls << bmclapiMirror(officialUrl);
                        urls << officialUrl;
                    } else {
                        // Reconstruct URL from Maven coordinate
                        QString fileName = artifactName + QStringLiteral("-") + version + classifierSuffix + QStringLiteral(".") + ext;
                        urls << QStringLiteral("https://bmclapi2.bangbang93.com/maven/%1/%2/%3/%4").arg(group, artifactName, version, fileName);
                        // Official NeoForge Maven
                        if (group.startsWith(QLatin1String("net/neoforged")))
                            urls << QStringLiteral("https://maven.neoforged.net/releases/%1/%2/%3/%4").arg(group, artifactName, version, fileName);
                        // Official Forge Maven
                        else if (group.startsWith(QLatin1String("net/minecraftforge")))
                            urls << QStringLiteral("https://files.minecraftforge.net/maven/%1/%2/%3/%4").arg(group, artifactName, version, fileName);
                    }

                    bool libOk = false;
                    for (const QString& url : urls) {
                        qCInfo(logLoader) << QStringLiteral("下载安装器库: %1").arg(url);
                        QDir().mkpath(libDir);
                        QNetworkRequest req;
                        req.setUrl(QUrl(url));
                        QNetworkReply* reply = nam->get(req);
                        QEventLoop loop;
                        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                        QTimer timer;
                        timer.setSingleShot(true);
                        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                        timer.start(30000);
                        loop.exec();
                        if (reply->error() == QNetworkReply::NoError && timer.isActive()) {
                            timer.stop();
                            QFile f(libFile);
                            if (f.open(QIODevice::WriteOnly)) {
                                f.write(reply->readAll());
                                f.close();
                                libOk = true;
                            }
                        } else {
                            qCWarning(logLoader) << QStringLiteral("安装器库下载失败: %1 %2").arg(url, reply->errorString());
                            QFile::remove(libFile);
                        }
                        reply->deleteLater();
                        if (libOk) break;
                    }
                    if (!libOk) {
                        qCWarning(logLoader) << QStringLiteral("安装器库下载失败（所有源均失败）: %1").arg(artifactName + QStringLiteral("-") + version);
                    }
                }
            }
        }
    }

    // 3e. Pre-download Mojang mapping file for FART processor (Forge only)
    // DOWNLOAD_MOJMAPS processor needs to download from Mojang servers (may be slow/unreliable in China).
    // We pre-download the raw ProGuard mapping from Mojang, convert to TSRG using
    // Java + srgutils, and save to the expected Maven path.
    // This way, if DOWNLOAD_MOJMAPS times out or fails, FART still finds its input file.
    if (!isNeoForge)
    {
        QString tsrgDir = m_gameDir + QStringLiteral("/libraries/net/minecraft/client/") + m_mcVersion;
        QString tsrgPath = tsrgDir + QStringLiteral("/client-") + m_mcVersion + QStringLiteral("-mappings.tsrg");
        if (!QFile::exists(tsrgPath)) {
            qCInfo(logLoader) << QStringLiteral("预下载 mappings.tsrg for %1").arg(m_mcVersion);
            QNetworkAccessManager* nam = HttpClient::instance().manager();

            // Helper: download URL with timeout
            auto downloadUrl = [&](const QString& url, int timeoutMs) -> QByteArray {
                QNetworkRequest req;
                req.setUrl(QUrl(url));
                QNetworkReply* reply = nam->get(req);
                QEventLoop loop;
                QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                QTimer timer;
                timer.setSingleShot(true);
                QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
                timer.start(timeoutMs);
                loop.exec();
                QByteArray data;
                if (reply->error() == QNetworkReply::NoError && timer.isActive()) {
                    timer.stop();
                    data = reply->readAll();
                }
                reply->deleteLater();
                return data;
            };

            // 1. Try to use existing .txt mapping from forgeStep3_install (Place 1) if available
            //    Path: libraries/net/minecraft/client/{m_mcVersion}/client-{m_mcVersion}-mappings.txt
            //    This avoids re-downloading the ~12MB file from Mojang.
            QString existingTxtPath = tsrgDir + QStringLiteral("/client-") + m_mcVersion + QStringLiteral("-mappings.txt");
            QByteArray rawMapping;
            bool mappingFromExisting = false;
            if (QFile::exists(existingTxtPath)) {
                qCInfo(logLoader) << QStringLiteral("使用已有的 client_mappings.txt: %1").arg(existingTxtPath);
                QFile ef(existingTxtPath);
                if (ef.open(QIODevice::ReadOnly)) {
                    rawMapping = ef.readAll();
                    ef.close();
                    if (!rawMapping.isEmpty()) {
                        mappingFromExisting = true;
                        qCInfo(logLoader) << QStringLiteral("已读取 %1 KB 的映射文件").arg(rawMapping.size() / 1024);
                    }
                }
            }

            if (!mappingFromExisting) {
                // Try BMCLAPI version manifest (fast from China) first, fallback to Mojang
                QStringList manifestUrls = {
                    QStringLiteral("https://bmclapi2.bangbang93.com/mc/game/version_manifest_v2.json"),
                    QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest.json"),
                };
                QByteArray manifestData;
                QString versionUrl;
                for (const auto& mu : manifestUrls) {
                    manifestData = downloadUrl(mu, 15000);
                    if (!manifestData.isEmpty()) {
                        QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestData);
                        QJsonArray versions = manifestDoc.object().value(QStringLiteral("versions")).toArray();
                        for (const auto& v : versions) {
                            if (v.toObject().value(QStringLiteral("id")).toString() == m_mcVersion) {
                                versionUrl = v.toObject().value(QStringLiteral("url")).toString();
                                break;
                            }
                        }
                        if (!versionUrl.isEmpty()) break;
                    }
                }
                if (versionUrl.isEmpty()) {
                    qCWarning(logLoader) << QStringLiteral("无法获取 %1 的版本信息（manifest 不可用）").arg(m_mcVersion);
                } else {
                    // 2. Download version JSON (try BMCLAPI mirror first)
                    QString versionJsonUrl = versionUrl;
                    // If the URL is from BMCLAPI manifest, it already points to BMCLAPI
                    QByteArray versionData = downloadUrl(versionJsonUrl, 15000);
                    if (versionData.isEmpty() && versionUrl.contains(QStringLiteral("launchermeta"))) {
                        // Try BMCLAPI version JSON directly
                        QString bmclVerUrl = QStringLiteral("https://bmclapi2.bangbang93.com/version/%1/json").arg(m_mcVersion);
                        qCInfo(logLoader) << QStringLiteral("尝试 BMCLAPI version JSON: %1").arg(bmclVerUrl);
                        versionData = downloadUrl(bmclVerUrl, 15000);
                    }

                    if (versionData.isEmpty()) {
                        qCWarning(logLoader) << QStringLiteral("无法下载 %1 version JSON").arg(m_mcVersion);
                    } else {
                        QJsonObject versionObj = QJsonDocument::fromJson(versionData).object();
                        QJsonObject clientMappings = versionObj.value(QStringLiteral("downloads")).toObject()
                            .value(QStringLiteral("client_mappings")).toObject();
                        QString mappingsUrl = clientMappings.value(QStringLiteral("url")).toString();
                        if (mappingsUrl.isEmpty()) {
                            qCInfo(logLoader) << QStringLiteral("版本 %1 没有 client_mappings").arg(m_mcVersion);
                        } else {
                            qCInfo(logLoader) << QStringLiteral("下载 client_mappings 文件（~12MB）...");
                            // 3. Download raw ProGuard mapping file (up to 90s for ~12MB)
                            rawMapping = downloadUrl(mappingsUrl, 90000);
                            if (rawMapping.isEmpty()) {
                                qCWarning(logLoader) << QStringLiteral("下载 mapping 文件失败（Mojang 可能不可达）");
                            }
                        }
                    }
                }
            }

            if (!rawMapping.isEmpty()) {
                // 4. Save raw mapping to temp file
                QString rawPath = QDir::tempPath()
                    + QStringLiteral("/forge_raw_mapping_") + m_mcVersion + QStringLiteral(".txt");
                {
                    QFile f(rawPath);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(rawMapping);
                        f.close();
                    }
                }

                // Also save a copy to the Maven path for Place 1 / future use
                {
                    QDir().mkpath(tsrgDir);
                    QFile sf(existingTxtPath);
                    if (!sf.exists() && sf.open(QIODevice::WriteOnly)) {
                        sf.write(rawMapping);
                        sf.close();
                        qCInfo(logLoader) << QStringLiteral("已缓存 client_mappings.txt: %1").arg(existingTxtPath);
                    }
                }

                // 5. Convert ProGuard .txt → TSRG .tsrg using srgutils (same library FART uses)
                //
                //    We cannot reliably produce TSRG in C++ — srgutils output format is complex
                //    (descriptors, ordering, edge cases). Instead, we use the srgutils JAR
                //    already downloaded to libraries by step 3d, via a pre-compiled wrapper class
                //    (ProGuardToTSRG.class) embedded as a Qt resource.
                //
                //    Flow: java -cp <srgutils.jar>;<converter_dir> ProGuardToTSRG <in.txt> <out.tsrg>
                {
                    QString outPath = tsrgDir + QStringLiteral("/client-")
                        + m_mcVersion + QStringLiteral("-mappings.tsrg");

                    // Find srgutils JAR by scanning libraries (matching the version FART uses)
                    // FART uses whichever srgutils is on its classpath;
                    // we scan for any version and prefer newest.
                    QString srgutilsJar;
                    {
                        QDirIterator it(m_gameDir + QStringLiteral("/libraries/net/minecraftforge/srgutils"),
                                        QDir::Dirs | QDir::NoDotAndDotDot);
                        QStringList versions;
                        while (it.hasNext()) {
                            QString verDir = it.next();
                            QString jarPath = verDir + QStringLiteral("/srgutils-")
                                + QDir(verDir).dirName() + QStringLiteral(".jar");
                            if (QFile::exists(jarPath))
                                versions.append(verDir);
                        }
                        if (!versions.isEmpty()) {
                            std::sort(versions.begin(), versions.end(), std::greater<QString>());
                            QString verDir = versions.first();
                            srgutilsJar = verDir + QStringLiteral("/srgutils-")
                                + QDir(verDir).dirName() + QStringLiteral(".jar");
                            qCInfo(logLoader) << QStringLiteral("找到 srgutils JAR: %1").arg(srgutilsJar);
                        }
                    }

                    if (srgutilsJar.isEmpty()) {
                        qCWarning(logLoader) << QStringLiteral("未找到 srgutils JAR，TSRG 转换将失败");
                    } else {
                        // Extract embedded converter class
                        QString converterDir = QDir::tempPath() + QStringLiteral("/sl_tsrg_converter");
                        QDir().mkpath(converterDir);
                        QString classPath = converterDir + QStringLiteral("/ProGuardToTSRG.class");
                        {
                            QFile res(QStringLiteral(":/resources/tools/ProGuardToTSRG.class"));
                            if (res.open(QIODevice::ReadOnly)) {
                                QByteArray classData = res.readAll();
                                res.close();
                                QFile cf(classPath);
                                if (cf.open(QIODevice::WriteOnly)) {
                                    cf.write(classData);
                                    cf.close();
                                }
                            }
                        }

                        if (!QFile::exists(classPath)) {
                            qCWarning(logLoader) << QStringLiteral("无法提取 ProGuardToTSRG.class");
                        } else {
                            qCInfo(logLoader) << QStringLiteral("运行 srgutils TSRG 转换: %1 → %2").arg(rawPath, outPath);
                            QProcess srgProc;
                            QString cp = srgutilsJar + QStringLiteral(";") + converterDir;
                            srgProc.start(javaPath,
                                QStringList() << QStringLiteral("-cp") << cp
                                    << QStringLiteral("ProGuardToTSRG")
                                    << QDir::toNativeSeparators(rawPath)
                                    << QDir::toNativeSeparators(outPath));
                            if (srgProc.waitForFinished(120000) && srgProc.exitCode() == 0) {
                                qint64 outSize = QFileInfo(outPath).size();
                                if (outSize > 1024) {
                                    qCInfo(logLoader) << QStringLiteral("srgutils TSRG 转换成功: %1 (%2 KB)")
                                        .arg(outPath).arg(outSize / 1024);
                                } else {
                                    qCWarning(logLoader) << QStringLiteral("TSRG 输出异常（仅 %1 字节）").arg(outSize);
                                    QFile::remove(outPath);
                                }
                            } else {
                                qCWarning(logLoader) << QStringLiteral("srgutils TSRG 转换失败: ")
                                    + QString::fromUtf8(srgProc.readAllStandardError());
                                QFile::remove(outPath);
                            }
                        }
                    }
                }

                // 8. Clean up raw temp file
                QFile::remove(rawPath);
            } else {
                qCInfo(logLoader) << QStringLiteral("映射文件不可用，将由 bootstrapper 的 DOWNLOAD_MOJMAPS 处理器下载");
            }
        } else {
            qCInfo(logLoader) << QStringLiteral("mappings.tsrg 已存在，跳过预下载");
        }
    }

    // 4. Ensure launcher_profiles.json exists
    QString profilesPath = m_gameDir + QStringLiteral("/launcher_profiles.json");
    if (!QFile::exists(profilesPath)) {
        QFile pf(profilesPath);
        if (pf.open(QIODevice::WriteOnly)) {
            pf.write(QStringLiteral("{\"profiles\":{}}").toUtf8());
            pf.close();
        }
    }

    // 5. Run bootstrapper (matching 主流启动器's ForgelikeInjector)
    QStringList args;
    // Note: 主流启动器 adds --add-exports cpw.mods.bootstraplauncher/... for Java 9+,
    // but on Java 25+ that module is removed from the JDK (it was inside bootstrapper JAR on classpath, not a system module).
    // The flag is unnecessary here — bootstrapper JAR is on -cp (unnamed module).
    args << QStringLiteral("-cp")
         << (bootstrapperJar + QStringLiteral(";") + installerJarPath)
         << QStringLiteral("com.bangbang93.ForgeInstaller")
         << QDir::toNativeSeparators(m_gameDir);

    qCInfo(logLoader) << QStringLiteral("运行 Bootstrapper: %1 %2").arg(javaPath, args.join(QStringLiteral(" ")));

    // 5. Take snapshot of versions/ before running bootstrapper (matching 主流启动器's OldList)
    QStringList oldVersions = QDir(versionsDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QString loaderName = isNeoForge ? QStringLiteral("NeoForge") : QStringLiteral("Forge");

    // 6. Run bootstrapper (matching 主流启动器's ForgelikeInjector).
    //    Retry once: 主流启动器 tries JavaWrapper first, falls back to bare Java.
    int retryAttempt = 0;
    const int maxRetries = 2;
    int bootExitCode = -1;
    QStringList outputLines;
    bool timedOut = false;

    for (retryAttempt = 0; retryAttempt < maxRetries; ++retryAttempt) {
        bool useWrapper = (retryAttempt == 0);
        if (m_cancelled) {
            QFile::remove(installerJarPath);
            emit finished(false, QStringLiteral("用户取消"));
            m_running = false;
            return;
        }

        emit progressChanged(3, m_totalSteps,
            QStringLiteral("正在通过 Java 运行 %1 安装器...").arg(loaderName));

        qCInfo(logLoader) << QStringLiteral("运行 Bootstrapper: %1").arg(javaPath);

        // ── Launch bootstrapper async with retry (QtConcurrent::run) ──
        if (m_bootstrapperWatcher && m_bootstrapperWatcher->isRunning()) {
            m_bootstrapperWatcher->waitForFinished();
        }
        if (!m_bootstrapperWatcher) {
            m_bootstrapperWatcher = new QFutureWatcher<BootstrapperResult>(this);
            connect(m_bootstrapperWatcher, &QFutureWatcher<BootstrapperResult>::finished,
                    this, &ModLoaderInstaller::onBootstrapperFinished);
        }

        QStringList oldVersionsSnapshot = oldVersions;
        QString versionsDirSnapshot = versionsDir();

        // Build both launch arg sets on main thread (uses member state)
        QString wrapperJar = extractJavaWrapperPath();
        bool hasWrapper = !wrapperJar.isEmpty();

        QStringList launchArgsWrapper, launchArgsRaw;
        if (hasWrapper) {
            launchArgsWrapper << QStringLiteral("-Doolloo.jlw.tmpdir=%1").arg(QDir::toNativeSeparators(m_gameDir.trimmed()));
            launchArgsWrapper << QStringLiteral("-cp") << (bootstrapperJar + QStringLiteral(";") + installerJarPath);
            launchArgsWrapper << QStringLiteral("-jar") << wrapperJar;
            launchArgsWrapper << QStringLiteral("com.bangbang93.ForgeInstaller");
        }
        launchArgsRaw << QStringLiteral("-cp") << (bootstrapperJar + QStringLiteral(";") + installerJarPath);
        launchArgsRaw << QStringLiteral("com.bangbang93.ForgeInstaller");

        const QString gameDirNative = QDir::toNativeSeparators(m_gameDir);
        launchArgsWrapper << gameDirNative;
        launchArgsRaw << gameDirNative;


        QFuture<BootstrapperResult> future = QtConcurrent::run(
            [javaPath, launchArgsWrapper, launchArgsRaw, hasWrapper,
             installerJarPath, loaderName, oldVersionsSnapshot,
             versionsDirSnapshot]() -> BootstrapperResult {
            // Attempt 1: with JavaWrapper (if available)
            if (hasWrapper) {
                BootstrapperResult r = runBootstrapperSync(
                    javaPath, launchArgsWrapper, installerJarPath, loaderName,
                    oldVersionsSnapshot, versionsDirSnapshot, 180000, nullptr);
                if (r.success) return r;
            }
            // Attempt 2: raw Java (fallback)
            return runBootstrapperSync(
                javaPath, launchArgsRaw, installerJarPath, loaderName,
                oldVersionsSnapshot, versionsDirSnapshot, 180000, nullptr);
        });

        m_bootstrapperWatcher->setFuture(future);
        return;  // Result handled in onBootstrapperFinished
    }

    // If we get here without returning, all retries failed
    QFile::remove(installerJarPath);
    emit finished(false, QStringLiteral("%1 安装器启动失败，请检查 Java 配置后重试").arg(loaderName));
    m_running = false;
    return;
}

// ═══════════════════════════════════════════════════════════════
// Async bootstrapper: run Java installer process off the main thread
// Returns BootstrapperResult struct with success/failure details.
// ═══════════════════════════════════════════════════════════════
ModLoaderInstaller::BootstrapperResult
ModLoaderInstaller::runBootstrapperSync(
    const QString& javaPath, const QStringList& launchArgs,
    const QString& installerJarPath, const QString& loaderName,
    const QStringList& oldVersions, const QString& versionsDirPath,
    int timeoutMs, std::function<void(int)> onStepProgress)
{
    BootstrapperResult result;
    result.installerJarPath = installerJarPath;
    result.loaderName = loaderName;
    result.oldVersions = oldVersions;

    QProcess proc;
    proc.start(javaPath, launchArgs);
    if (!proc.waitForStarted(10000)) {
        result.success = false;
        result.exitCode = -1;
        result.errorMsg = QStringLiteral("无法启动 %1 安装器 Java 进程").arg(loaderName);
        return result;
    }

    // Process output loop (runs on background thread — no UI blocking)
    QElapsedTimer elapsed;
    elapsed.start();
    const int pollMs = 100;
    QStringList outputLines;
    bool procFinished = false;

    while (!procFinished && !result.timedOut) {
        if (elapsed.elapsed() > timeoutMs) {
            result.timedOut = true;
            proc.kill();
            proc.waitForFinished(3000);
            break;
        }
        if (proc.waitForReadyRead(pollMs)) {
            QByteArray stdoutData = proc.readAllStandardOutput();
            if (!stdoutData.isEmpty()) {
                QStringList lines = QString::fromUtf8(stdoutData)
                    .split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
                for (const QString& line : lines) {
                    if (line.trimmed().isEmpty()) continue;
                    outputLines.append(line);
                    // 主流启动器 keyword → stepProgress mapping
                    if (onStepProgress) {
                        if (line == QStringLiteral("Extracting json"))
                            onStepProgress(27);
                        else if (line == QStringLiteral("Downloading libraries"))
                            onStepProgress(28);
                        else if (line == QStringLiteral("Building Processors"))
                            onStepProgress(38);
                        else if (line == QStringLiteral("Task: DOWNLOAD_MOJMAPS"))
                            onStepProgress(40);
                        else if (line == QStringLiteral("Task: MERGE_MAPPING"))
                            onStepProgress(50);
                        else if (line.startsWith(QStringLiteral("Splitting: ")))
                            onStepProgress(55);
                        else if (line == QStringLiteral("Parameter Annotations"))
                            onStepProgress(60);
                        else if (line == QStringLiteral("Processing Complete") || line == QStringLiteral("log: null"))
                            onStepProgress(67);
                        else if (line == QStringLiteral("Sorting"))
                            onStepProgress(80);
                        else if (line == QStringLiteral("Remapping final jar"))
                            onStepProgress(85);
                        else if (line == QStringLiteral("Remapping jar... 50%"))
                            onStepProgress(90);
                        else if (line == QStringLiteral("Remapping jar... 100%"))
                            onStepProgress(95);
                        else if (line == QStringLiteral("Injecting profile"))
                            onStepProgress(98);
                    }
                }
            }
        }
        QByteArray stderrData = proc.readAllStandardError();
        if (!stderrData.isEmpty()) {
            for (const auto& line : QString::fromUtf8(stderrData)
                .split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
                if (!line.trimmed().isEmpty())
                    qCInfo(logLoader) << QStringLiteral("[Bootstrapper:err] ") + line;
            }
        }
        if (proc.state() == QProcess::NotRunning) {
            procFinished = true;
            proc.waitForFinished(3000);
        }
    }

    result.exitCode = proc.exitCode();

    if (result.timedOut) {
        result.success = false;
        result.errorMsg = QStringLiteral("%1 安装器超时").arg(loaderName);
        return result;
    }

    // 主流启动器: check if output contains "true" in the last 5 lines
    bool hasTrue = outputLines.contains(QStringLiteral("true"));
    if (!hasTrue) {
        for (int i = qMax(0, outputLines.size() - 5); i < outputLines.size(); ++i) {
            if (outputLines[i] == QStringLiteral("true")) { hasTrue = true; break; }
        }
    }

    result.success = (result.exitCode == 0 && hasTrue);

    if (result.success) {
        // Find new version folder
        QStringList newVersions = QDir(versionsDirPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QStringList delta = newVersions;
        for (const QString& v : oldVersions) delta.removeAll(v);

        if (delta.size() == 1) {
            result.foundSub = delta[0];
        } else if (delta.size() > 1) {
            for (const QString& d : delta) {
                QDir dDir(versionsDirPath + QStringLiteral("/") + d);
                if (dDir.exists() && dDir.entryList(QDir::Files).size() > 0) {
                    if (d.contains(QStringLiteral("forge"), Qt::CaseInsensitive)) {
                        result.foundSub = d;
                        break;
                    }
                    if (result.foundSub.isEmpty()) result.foundSub = d;
                }
            }
        }
    } else {
        result.errorMsg = QStringLiteral("%1 安装器返回错误（退出码=%2）").arg(loaderName).arg(result.exitCode);
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════
// Handle async bootstrapper result — called on main thread via watcher
// ═══════════════════════════════════════════════════════════════
void ModLoaderInstaller::onBootstrapperFinished()
{
    if (!m_bootstrapperWatcher) return;
    BootstrapperResult result = m_bootstrapperWatcher->result();

    // Cleanup installer JAR
    QFile::remove(result.installerJarPath);

    if (result.timedOut) {
        emit finished(false, QStringLiteral("%1 安装器超时，请检查 Java 配置后重试").arg(result.loaderName));
        m_running = false;
        return;
    }

    if (result.success) {
        if (!result.foundSub.isEmpty()) {
            QString foundSub = result.foundSub;
            QString jsonPath = versionsDir() + QStringLiteral("/") + foundSub
                + QStringLiteral("/") + foundSub + QStringLiteral(".json");

            if (foundSub != m_installName) {
                renameVersionFolder(foundSub, m_installName);
                jsonPath = versionsDir() + QStringLiteral("/") + m_installName
                    + QStringLiteral("/") + m_installName + QStringLiteral(".json");
            }

            m_postJsonPath = jsonPath;
        } else {
            qCWarning(logLoader) << QStringLiteral("Bootstrapper 完成后未找到新增版本文件夹");
        }

        m_bootstrapperOk = true;
        m_running = false;
    } else {
        m_bootstrapperOk = false;
        m_bootstrapperError = result.errorMsg;
        m_running = false;
    }

    // Continue with JSON flatten + copy JAR
    finalizeBootstrapperInstall();
}

// ═══════════════════════════════════════════════════════════════
// Post-bootstrapper: flatten JSON + copy JAR (主流启动器's MergeJson equivalent)
// 主流启动器 does this as a SEPARATE step after the bootstrapper returns,
// NOT inside the bootstrapper itself.
// ═══════════════════════════════════════════════════════════════
void ModLoaderInstaller::finalizeBootstrapperInstall()
{
    if (!m_bootstrapperOk) {
        emit finished(false, m_bootstrapperError.isEmpty()
            ? QStringLiteral("Bootstrapper 安装失败") : m_bootstrapperError);
        m_running = false;
        return;
    }

    const bool isNeo = (m_loaderType == QStringLiteral("neoforge"));
    // 主流启动器: DlNeoForgeListEntry.UrlBase -> PackageName = If(Inherit = "1.20.1", "forge", "neoforge")
    bool isLegacy = (m_mcVersion == QStringLiteral("1.20.1"));
    const QString neoPkg = isLegacy ? QStringLiteral("forge") : QStringLiteral("neoforge");
    const QString ver = isNeo
        ? (isLegacy ? QStringLiteral("1.20.1-%1").arg(m_loaderVersion) : m_loaderVersion)
        : (m_mcVersion + QStringLiteral("-") + m_loaderVersion);
    const QString loaderGroup = isNeo
        ? (QStringLiteral("net/neoforged/") + neoPkg)
        : QStringLiteral("net/minecraftforge/forge");
    const QString filePrefix = isNeo ? neoPkg : QStringLiteral("forge");

    // Flatten version JSON (resolve inheritsFrom chain) — 主流启动器's MergeJson equivalent
    if (!m_postJsonPath.isEmpty()) {
        QFile jf(m_postJsonPath);
        if (jf.open(QIODevice::ReadOnly)) {
            QByteArray jdata = jf.readAll();
            jf.close();
            QJsonDocument jdoc = QJsonDocument::fromJson(jdata);
            if (jdoc.isObject()) {
                QJsonObject jObj = jdoc.object();
                QJsonObject flattened = flattenVersionJson(m_gameDir, jObj);

                bool inheritsLeft = flattened.contains(QStringLiteral("inheritsFrom"));
                if (inheritsLeft)
                    flattened.remove(QStringLiteral("inheritsFrom"));

                // Inject MC client as library if missing (for standalone version)
                // NeoForge: skip — client-26.2.jar on classpath interferes with
                // RequiredSystemFiles.areNeoForgeAndMinecraftSeparate(), causing
                // "The patched Minecraft jar is missing" error.
                if (!isNeo) {
                QJsonArray mergedLibs = flattened.value(QStringLiteral("libraries")).toArray();
                QString mcClientName = QStringLiteral("net.minecraft:client:") + m_mcVersion;
                bool hasMcClient = false;
                for (const auto& lib : mergedLibs) {
                    if (lib.toObject().value(QStringLiteral("name")).toString() == mcClientName) {
                        hasMcClient = true; break;
                    }
                }
                if (!hasMcClient) {
                    QJsonObject mcClient;
                    mcClient[QStringLiteral("name")] = mcClientName;
                    QJsonObject downloads;
                    QJsonObject artifact;
                    artifact[QStringLiteral("path")] = QStringLiteral("net/minecraft/client/%1/client-%1.jar").arg(m_mcVersion);
                    downloads[QStringLiteral("artifact")] = artifact;
                    mcClient[QStringLiteral("downloads")] = downloads;
                    mergedLibs.append(mcClient);
                    flattened[QStringLiteral("libraries")] = mergedLibs;
                }
                }

                if (jf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    jf.write(QJsonDocument(flattened).toJson(QJsonDocument::Indented));
                    jf.close();
                    if (inheritsLeft || !jObj.contains(QStringLiteral("inheritsFrom")) ||
                        jObj.value(QStringLiteral("inheritsFrom")).toString() != QString()) {
                        qCInfo(logLoader) << QStringLiteral("版本 JSON 已压平为独立版本，inheritsFrom 已消解");
                    }
                }
            }
        }

        // Forge: copy client/universal JAR to version folder (NeoForge uses vanilla MC client JAR instead)
        if (!isNeo) {
        QString targetDir = versionsDir() + QStringLiteral("/") + m_installName;
        QString jarPathV = targetDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (!QFile::exists(jarPathV)) {
            const QString clientJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                + QStringLiteral("/") + ver + QStringLiteral("/")
                + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-client.jar");
            const QString universalJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                + QStringLiteral("/") + ver + QStringLiteral("/")
                + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-universal.jar");

            if (QFile::exists(clientJar) && QFile::copy(clientJar, jarPathV)) {
                qCInfo(logLoader) << QStringLiteral("已复制 client JAR 到 %1").arg(jarPathV);
            } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPathV)) {
                qCInfo(logLoader) << QStringLiteral("已复制 universal JAR 到 %1").arg(jarPathV);
            }
        }
        }
    }

    // Copy vanilla MC client JAR to version folder (主流启动器's MergeJson FileUtils.Copy equivalent)
    // Needed by launcher for classpath; source: libraries/net/minecraft/client/{ver}/client-{ver}.jar
    {
        QString mcClientSrc = m_gameDir + QStringLiteral("/libraries/net/minecraft/client/") + m_mcVersion
            + QStringLiteral("/client-") + m_mcVersion + QStringLiteral(".jar");
        QString mcClientDst = versionsDir() + QStringLiteral("/") + m_installName
            + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        if (QFile::exists(mcClientSrc) && !QFile::exists(mcClientDst)) {
            QDir().mkpath(QFileInfo(mcClientDst).absolutePath());
            if (QFile::copy(mcClientSrc, mcClientDst))
                qCInfo(logLoader) << QStringLiteral("已复制原版客户端 JAR 到版本文件夹: %1").arg(mcClientDst);
            else
                qCWarning(logLoader) << QStringLiteral("复制原版客户端 JAR 失败: %1 -> %2").arg(mcClientSrc, mcClientDst);
        }
    }

    emit finished(true, QString());
    m_running = false;
}
void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Fabric 配置...");

    const QString bmclUrl = QStringLiteral("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/%1/%2/profile/json")
                                .arg(m_mcVersion, m_loaderVersion);
    const QString officialUrl = QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                                    .arg(m_mcVersion, m_loaderVersion);

    downloadToMemoryRace({bmclUrl, officialUrl},
        [this](bool ok, const QByteArray& data) {
            if (!ok) {
                emit finished(false, "Fabric 配置下载失败（BMCLAPI 和官方源均失败）");
                m_running = false;
                return;
            }
            qCInfo(logLoader) << QStringLiteral("Fabric 配置下载成功，大小=%1 字节").arg(data.size());
            fabricStep2_downloadLibraries(data);
        },
        QStringLiteral("fabric-profile.json"));
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

    for (const QJsonValue& v : libs) {
        QJsonObject lib = v.toObject();
        QString name = lib[QStringLiteral("name")].toString();
        QString relPath = mvnPath(name);
        if (relPath.isEmpty()) continue;

        FabricLibTask task;
        task.savePath = m_gameDir + QStringLiteral("/libraries/") + relPath;
        // Use BMCLAPI URL by default; fallback handled at download time
        task.url = QStringLiteral("https://bmclapi2.bangbang93.com/maven/") + relPath;
        m_fabricLibTasks.append(task);
    }

    qCInfo(logLoader) << QStringLiteral("Fabric 库下载: 需要 %1 个文件").arg(m_fabricLibTasks.size());
    m_fabricLibIndex = 0;
    m_fabricLibBytesDone = 0;
    m_fabricLibBytesTotal = 0;

    // Download sequentially (each Fabric lib is <2MB)
    // Capture task data by VALUE — async HTTP callback must not hold &task (dangling ref)
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

        // Capture by VALUE — safe across async callback
        int taskIdx = m_fabricLibIndex;
        QString taskUrl = m_fabricLibTasks[taskIdx].url;
        QString taskSavePath = m_fabricLibTasks[taskIdx].savePath;

        auto tryMirror = std::make_shared<std::function<void(int)>>();
        *tryMirror = [this, taskIdx, taskUrl, taskSavePath, dlNext, tryMirror, mirrors](int idx) {
            if (idx >= mirrors.size()) {
                emit finished(false, QStringLiteral("Fabric 依赖库下载失败:\n%1\n\n已尝试 %2 个镜像源")
                                   .arg(taskUrl, QString::number(mirrors.size())));
                m_running = false;
                return;
            }

            QString url = (idx == 0) ? taskUrl : taskUrl;
            url.replace(mirrors[0], mirrors[idx > 0 ? qMin(idx, mirrors.size()-1) : 0]);

            qCInfo(logLoader) << QStringLiteral("Fabric 库 #%1 正在下载 %2").arg(taskIdx).arg(url);
            QDir().mkpath(QFileInfo(taskSavePath).absolutePath());
            downloadToFile(url, taskSavePath, [this, taskIdx, taskSavePath, dlNext, tryMirror, idx](bool ok, const QString& err) {
                if (ok) {
                    qint64 fileSize = QFileInfo(taskSavePath).size();
                    qCInfo(logLoader) << QStringLiteral("Fabric 库 #%1 下载成功: %2 字节").arg(taskIdx).arg(fileSize);
                    m_fabricLibBytesDone += fileSize;
                    if (taskIdx < m_fabricLibTasks.size())
                        m_fabricLibTasks[taskIdx].downloaded = true;

                    int pct = (m_fabricLibTasks.size() > 0)
                        ? static_cast<int>((m_fabricLibIndex + 1) * 100 / m_fabricLibTasks.size())
                        : 100;
                    emit stepProgress(2, pct);
                    emit byteProgress(QFileInfo(taskSavePath).fileName(),
                                     static_cast<qint64>(m_fabricLibIndex + 1),
                                     static_cast<qint64>(m_fabricLibTasks.size()), 0);

                    m_fabricLibIndex++;
                    (*dlNext)();
                } else {
                    qCInfo(logLoader) << QStringLiteral("Fabric 库 #%1 镜像#%2 下载失败: %3").arg(taskIdx).arg(idx).arg(err);
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
    qCInfo(logLoader) << QStringLiteral("Fabric 收尾: 写入版本 JSON（并行模式）");
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
    QString mcVerDir = findVersionDir(m_mcVersion);
    QString targetJar = verDir + "/" + m_installName + ".jar";
    if (!mcVerDir.isEmpty()) {
        QString vanillaJar = mcVerDir + "/" + QDir(mcVerDir).dirName() + ".jar";
        if (QFile::exists(vanillaJar)) {
            if (QFile::exists(targetJar)) QFile::remove(targetJar);
            QFile::copy(vanillaJar, targetJar);
        }
    }

    // Merge vanilla MC JSON content into Fabric JSON (same pattern as NeoForge)
    // Fabric profile from meta.fabricmc.net uses inheritsFrom — vanilla libs/natives
    // must be merged before deleting the vanilla version folder
    if (json.contains(QStringLiteral("inheritsFrom"))) {
        QString mcJsonPath;
        if (!mcVerDir.isEmpty())
            mcJsonPath = mcVerDir + "/" + QDir(mcVerDir).dirName() + ".json";
        else
            mcJsonPath = m_gameDir + QStringLiteral("/versions/%1/%1.json").arg(m_mcVersion);  // fallback
        QFile mcf(mcJsonPath);
        if (mcf.open(QIODevice::ReadOnly)) {
            QJsonDocument mcDoc = QJsonDocument::fromJson(mcf.readAll());
            mcf.close();
            QJsonObject mcObj = mcDoc.object();
            // Re-read our JSON (fresh after write)
            QFile own(jsonPath);
            if (own.open(QIODevice::ReadOnly)) {
                QJsonObject ownObj = QJsonDocument::fromJson(own.readAll()).object();
                own.close();
                // Merge libraries: MC libs first, then Fabric libs.
                // Deduplicate: if both MC and Fabric carry the same library
                // (same group:artifact), keep the higher version to avoid
                // classpath conflicts (e.g. ASM 9.6 vs 9.10.1).
                QJsonArray mcLibs = mcObj[QStringLiteral("libraries")].toArray();
                QJsonArray fabLibs = ownObj[QStringLiteral("libraries")].toArray();
                QJsonArray merged;
                QSet<QString> seenGA;  // "group:artifact" keys already merged
                // Helper: extract "group:artifact" from "group:artifact:version"
                auto gaKey = [](const QString& name) -> QString {
                    int lastColon = name.lastIndexOf(QLatin1Char(':'));
                    return lastColon > 0 ? name.left(lastColon) : name;
                };
                // Helper: parse version string for numeric comparison
                auto versionWeight = [](const QString& ver) -> qint64 {
                    qint64 w = 0;
                    int shift = 48;
                    for (const auto& part : ver.split(QLatin1Char('.'))) {
                        bool ok = false;
                        int n = part.toInt(&ok);
                        if (ok) w |= (static_cast<qint64>(n & 0xFFFF) << shift);
                        shift -= 16;
                        if (shift < 0) break;
                    }
                    return w;
                };
                for (const auto& v : mcLibs) {
                    merged.append(v);
                    QJsonObject lib = v.toObject();
                    QString name = lib[QStringLiteral("name")].toString();
                    if (!name.isEmpty()) seenGA.insert(gaKey(name));
                }
                for (const auto& v : fabLibs) {
                    QJsonObject lib = v.toObject();
                    QString name = lib[QStringLiteral("name")].toString();
                    QString ga = gaKey(name);
                    if (seenGA.contains(ga)) {
                        // Already present from MC — compare versions, keep higher
                        QString fabVer = name.mid(ga.length() + 1);
                        // Find the MC entry with same GA and compare
                        bool replaced = false;
                        for (int mi = 0; mi < merged.size(); ++mi) {
                            QJsonObject mcLib = merged[mi].toObject();
                            QString mcName = mcLib[QStringLiteral("name")].toString();
                            if (gaKey(mcName) == ga) {
                                QString mcVer = mcName.mid(ga.length() + 1);
                                if (versionWeight(fabVer) > versionWeight(mcVer)) {
                                    merged[mi] = v;  // Replace with newer
                                    qCInfo(logLoader) << QStringLiteral("Fabric 去重: 已替换 %1 → %2").arg(mcName, name);
                                } else {
                                    qCInfo(logLoader) << QStringLiteral("Fabric 去重: 保留 %1（比 %2 新）").arg(mcName, name);
                                }
                                replaced = true;
                                break;
                            }
                        }
                        if (!replaced) {
                            merged.append(v);
                        }
                    } else {
                        merged.append(v);
                        seenGA.insert(ga);
                    }
                }
                ownObj[QStringLiteral("libraries")] = merged;
                // Merge game arguments
                QJsonObject mcArgs = mcObj[QStringLiteral("arguments")].toObject();
                QJsonObject ownArgs = ownObj[QStringLiteral("arguments")].toObject();
                QJsonArray mcGameArgs = mcArgs[QStringLiteral("game")].toArray();
                QJsonArray ownGameArgs = ownArgs[QStringLiteral("game")].toArray();
                for (const auto& v : mcGameArgs) ownGameArgs.append(v);
                ownArgs[QStringLiteral("game")] = ownGameArgs;
                ownObj[QStringLiteral("arguments")] = ownArgs;
                // Remove inheritsFrom — standalone JSON now
                ownObj.remove(QStringLiteral("inheritsFrom"));
                // Copy fields inherited from MC parent
                auto cpIfNeeded = [&](const QString& key) {
                    QJsonValue v = ownObj[key];
                    if (v.isUndefined() || v.isNull()) ownObj[key] = mcObj[key];
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
                // Write back merged JSON
                QFile out(jsonPath);
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(QJsonDocument(ownObj).toJson(QJsonDocument::Indented));
                    out.close();
                }
                qCInfo(logLoader) << QStringLiteral("Fabric JSON 已与 MC 原版 libraries 合并");
            }
        }
    }

    // Vanilla MC cleanup deferred to VersionBackend (avoids race with other installs)
    qCInfo(logLoader) << QStringLiteral("Fabric 版本 JSON: %1").arg(jsonPath);
    emit progressChanged(3, m_totalSteps, "Fabric 安装完成");
    emit finished(true, QString());
    m_running = false;
}

// ============================================================
// NeoForge — download, verify against official Maven .sha1
// ============================================================

void ModLoaderInstaller::neoStep1_downloadInstaller() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, QStringLiteral("正在下载 NeoForge 安装程序..."));

    const QString ver = m_loaderVersion;
    bool isLegacy = (m_mcVersion == QStringLiteral("1.20.1"));
    const QString pkg = isLegacy ? QStringLiteral("forge") : QStringLiteral("neoforge");
    const QString apiName = isLegacy ? QStringLiteral("1.20.1-%1").arg(ver) : ver;

    // TrueRace: BMCLAPI + Official concurrent
    const QString bmclUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/%1/%2/%1-%2-installer.jar")
                                .arg(pkg, apiName);
    const QString officialUrl = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/%1/%2/%1-%2-installer.jar")
                                    .arg(pkg, apiName);

    const QStringList urls = {bmclUrl, officialUrl};
    downloadToMemoryRace(urls,
        [this](bool ok, const QByteArray& data) {
            if (!ok) {
                emit finished(false, QStringLiteral("NeoForge 安装程序下载失败（BMCLAPI 和官方源均失败）"));
                m_running = false;
                return;
            }
            qCInfo(logLoader) << QStringLiteral("NeoForge 安装程序下载成功，大小=%1 字节").arg(data.size());
            neoStep2_verify(data);
        },
        QStringLiteral("neoforge-installer.jar"));
}

void ModLoaderInstaller::neoStep2_verify(const QByteArray& jarData) {
    m_currentStep = 2;
    emit progressChanged(2, m_totalSteps, "正在校验 NeoForge 安装程序...");
    emit verifyStarted();

    emit progressChanged(2, m_totalSteps, "正在获取 NeoForge SHA1 校验值...");

    // NeoForge Maven provides .sha1 files (BMCLAPI does not mirror them)
    // MC 1.20.1 uses legacy net/neoforged/forge package
    QString sha1Url;
    if (m_mcVersion == QStringLiteral("1.20.1")) {
        sha1Url = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/forge/1.20.1-%1/forge-1.20.1-%1-installer.jar.sha1")
                       .arg(m_loaderVersion);
    } else {
        sha1Url = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar.sha1")
                       .arg(m_loaderVersion);
    }

    downloadSmall(sha1Url, [this, jarData](bool ok, const QByteArray& sha1Data) {
        if (!ok || sha1Data.isEmpty()) {
            qCWarning(logLoader) << QStringLiteral("无法获取 NeoForge SHA1，跳过校验");
            emit verifyFinished(false);
            if (m_verifyOnly) { m_cachedJar = jarData; m_running = false; emit waitingForMC(); return; }
            forgeStep3_install(jarData);
            return;
        }

        QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
        QString actualSha1 = computeSha1(jarData);
        bool match = (actualSha1 == expectedSha1);

        emit progressChanged(2, m_totalSteps, "正在比对 NeoForge SHA1 校验值...");

        qCInfo(logLoader) << QStringLiteral("NeoForge SHA1 期望=%1 实际=%2 匹配=%3").arg(expectedSha1, actualSha1, match ? QStringLiteral("是") : QStringLiteral("否"));
        emit verifyFinished(match);

        if (!match) {
            emit finished(false, "NeoForge 安装程序校验失败（SHA1 不匹配）");
            m_running = false;
            return;
        }
        if (m_verifyOnly) { m_cachedJar = jarData; m_running = false; emit waitingForMC(); return; }
        forgeStep3_install(jarData);
    });
}

// ============================================================
// Post-install: rename version folder to user's chosen name
// ============================================================
// NeoForge — now handled by runBootstrapperProcess (主流启动器's ForgelikeInjector)
// The old ~870-line manual parser has been removed.
// ============================================================


void ModLoaderInstaller::installNeoForge(const QByteArray& jarData, const QJsonObject&)
{
    Q_UNUSED(jarData)
    // This path should never be reached — NeoForge now routes through
    // runBootstrapperProcess() which matches 主流启动器's ForgelikeInjector.
    qCWarning(logLoader) << QStringLiteral("installNeoForge(const QByteArray&) called unexpectedly — bootstrapper handles NeoForge");
    emit finished(false, QStringLiteral("内部错误：NeoForge 安装路径异常"));
    m_running = false;
}

void ModLoaderInstaller::renameVersionFolder(const QString& oldName, const QString& newName)
{
    const QString vd = versionsDir();
    const QString oldDir = vd + QStringLiteral("/") + oldName;
    const QString newDir = vd + QStringLiteral("/") + newName;

    if (!QDir(oldDir).exists()) {
        qCInfo(logLoader) << QStringLiteral("renameVersionFolder: 旧目录不存在，跳过 %1").arg(oldDir);
        return;
    }
    if (QDir(newDir).exists()) {
        qCInfo(logLoader) << QStringLiteral("renameVersionFolder: 目标已存在，跳过 %1").arg(newDir);
        return;
    }

    // 1. Rename folder
    if (!QDir().rename(oldDir, newDir)) {
        qCWarning(logLoader) << QStringLiteral("renameVersionFolder: 文件夹重命名失败 %1 → %2").arg(oldDir, newDir);
        return;
    }

    // 2. Rename JSON
    const QString oldJson = newDir + QStringLiteral("/") + oldName + QStringLiteral(".json");
    const QString newJson = newDir + QStringLiteral("/") + newName + QStringLiteral(".json");
    if (QFile::exists(oldJson) && !QFile::rename(oldJson, newJson)) {
        qCWarning(logLoader) << QStringLiteral("renameVersionFolder: JSON 重命名失败 %1 → %2").arg(oldJson, newJson);
    }

    // 3. Rename JAR (if exists)
    const QString oldJar = newDir + QStringLiteral("/") + oldName + QStringLiteral(".jar");
    const QString newJar = newDir + QStringLiteral("/") + newName + QStringLiteral(".jar");
    if (QFile::exists(oldJar) && !QFile::rename(oldJar, newJar)) {
        qCWarning(logLoader) << QStringLiteral("renameVersionFolder: JAR 重命名失败 %1 → %2").arg(oldJar, newJar);
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

    qCInfo(logLoader) << QStringLiteral("版本已重命名: %1 → %2").arg(oldName, newName);
}

// ============================================================
// Helper: Flatten version JSON — resolve inheritsFrom chain into standalone version
// ============================================================
static QJsonObject flattenVersionJson(const QString& gameDir, QJsonObject child) {
    // Walk inheritance chain and collect all version JSONs (child first)
    QList<QJsonObject> chain;
    chain.append(child);
    QString inherits = child.value(QStringLiteral("inheritsFrom")).toString();
    QStringList seen;
    while (!inherits.isEmpty() && !seen.contains(inherits)) {
        seen.append(inherits);
        QFile f(gameDir + QStringLiteral("/versions/") + inherits
                + QStringLiteral("/") + inherits + QStringLiteral(".json"));
        if (!f.open(QIODevice::ReadOnly)) break;
        QJsonObject parent = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        if (parent.isEmpty()) break;
        chain.append(parent);
        inherits = parent.value(QStringLiteral("inheritsFrom")).toString();
    }

    if (chain.size() <= 1)
        return child;  // Nothing to flatten

    // Merge fields from all parents into child
    // Libraries: merged & dedup by name (child first for priority)
    QJsonArray mergedLibs;
    // Map: lib name → index in mergedLibs (for merging cross-chain fields like `natives`)
    QMap<QString, int> libNameIndex;
    for (const auto& ver : chain) {
        QJsonArray libs = ver.value(QStringLiteral("libraries")).toArray();
        for (const auto& libVal : libs) {
            QJsonObject libObj = libVal.toObject();
            QString name = libObj.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) {
                mergedLibs.append(libVal);
                continue;
            }
            if (libNameIndex.contains(name)) {
                // Duplicate — merge missing fields from parent into existing entry
                // (child wins for most fields, but `natives` & `downloads.classifiers`
                //  may only exist in the parent version JSON — merge them in)
                int idx = libNameIndex[name];
                QJsonObject existing = mergedLibs[idx].toObject();
                bool changed = false;
                // Merge `natives` — Forge child version often omits this field
                if (!existing.contains(QStringLiteral("natives")) && libObj.contains(QStringLiteral("natives"))) {
                    existing[QStringLiteral("natives")] = libObj[QStringLiteral("natives")];
                    changed = true;
                }
                // Merge `downloads` — parent may have download URLs the child lacks
                if (!existing.contains(QStringLiteral("downloads")) && libObj.contains(QStringLiteral("downloads"))) {
                    existing[QStringLiteral("downloads")] = libObj[QStringLiteral("downloads")];
                    changed = true;
                } else if (existing.contains(QStringLiteral("downloads")) && libObj.contains(QStringLiteral("downloads"))) {
                    // Merge classifiers (natives artifacts) from parent into child
                    QJsonObject exDl = existing[QStringLiteral("downloads")].toObject();
                    QJsonObject parentDl = libObj[QStringLiteral("downloads")].toObject();
                    if (!exDl.contains(QStringLiteral("classifiers")) && parentDl.contains(QStringLiteral("classifiers"))) {
                        exDl[QStringLiteral("classifiers")] = parentDl[QStringLiteral("classifiers")];
                        existing[QStringLiteral("downloads")] = exDl;
                        changed = true;
                    }
                }
                if (changed)
                    mergedLibs[idx] = existing;
            } else {
                libNameIndex[name] = mergedLibs.size();
                mergedLibs.append(libVal);
            }
        }
    }
    child[QStringLiteral("libraries")] = mergedLibs;

    // arguments: walk parent → child, merging (not replacing)
    // Forge versions may use both `arguments` (object) and `minecraftArguments` (legacy string)
    QJsonObject mergedArgs;
    for (int i = chain.size() - 1; i >= 0; i--) {
        QJsonValue argsVal = chain[i].value(QStringLiteral("arguments"));
        if (argsVal.isObject()) {
            QJsonObject argsObj = argsVal.toObject();
            // game: parent first, child appends
            if (argsObj.contains(QStringLiteral("game"))) {
                QJsonArray arr = argsObj[QStringLiteral("game")].toArray();
                if (!mergedArgs.contains(QStringLiteral("game"))) {
                    mergedArgs[QStringLiteral("game")] = arr;
                } else {
                    QJsonArray existing = mergedArgs[QStringLiteral("game")].toArray();
                    for (const auto& v : arr) existing.append(v);
                    mergedArgs[QStringLiteral("game")] = existing;
                }
            }
            // jvm: parent first, child appends
            if (argsObj.contains(QStringLiteral("jvm"))) {
                QJsonArray arr = argsObj[QStringLiteral("jvm")].toArray();
                if (!mergedArgs.contains(QStringLiteral("jvm"))) {
                    mergedArgs[QStringLiteral("jvm")] = arr;
                } else {
                    QJsonArray existing = mergedArgs[QStringLiteral("jvm")].toArray();
                    for (const auto& v : arr) existing.append(v);
                    mergedArgs[QStringLiteral("jvm")] = existing;
                }
            }
        }
    }
    if (!mergedArgs.isEmpty())
        child[QStringLiteral("arguments")] = mergedArgs;

    // minecraftArguments: prefer child, fallback to deepest parent
    if (!child.contains(QStringLiteral("minecraftArguments"))) {
        for (int i = chain.size() - 1; i >= 0; i--) {
            if (chain[i].contains(QStringLiteral("minecraftArguments"))) {
                child[QStringLiteral("minecraftArguments")] = chain[i][QStringLiteral("minecraftArguments")];
                break;
            }
        }
    }

    // assetIndex: from deepest parent unless child specifies one
    if (!child.contains(QStringLiteral("assetIndex"))) {
        for (int i = chain.size() - 1; i >= 0; i--) {
            if (chain[i].contains(QStringLiteral("assetIndex"))) {
                child[QStringLiteral("assetIndex")] = chain[i][QStringLiteral("assetIndex")];
                break;
            }
        }
    }

    // javaVersion: from deepest parent unless child specifies one
    if (!child.contains(QStringLiteral("javaVersion"))) {
        for (int i = chain.size() - 1; i >= 0; i--) {
            if (chain[i].contains(QStringLiteral("javaVersion"))) {
                child[QStringLiteral("javaVersion")] = chain[i][QStringLiteral("javaVersion")];
                break;
            }
        }
    }

    // logging: from deepest parent unless child specifies one
    if (!child.contains(QStringLiteral("logging"))) {
        for (int i = chain.size() - 1; i >= 0; i--) {
            if (chain[i].contains(QStringLiteral("logging"))) {
                child[QStringLiteral("logging")] = chain[i][QStringLiteral("logging")];
                break;
            }
        }
    }

    // minimumLauncherVersion: from deepest parent unless child specifies one
    if (!child.contains(QStringLiteral("minimumLauncherVersion"))) {
        for (int i = chain.size() - 1; i >= 0; i--) {
            if (chain[i].contains(QStringLiteral("minimumLauncherVersion"))) {
                child[QStringLiteral("minimumLauncherVersion")] = chain[i][QStringLiteral("minimumLauncherVersion")];
                break;
            }
        }
    }

    // downloads: merge from deepest parent (client/server JAR URLs)
    // Forge child version.json typically omits downloads, relying on inheritance.
    if (!child.contains(QStringLiteral("downloads"))) {
        for (int i = chain.size() - 1; i >= 0; i--) {
            if (chain[i].contains(QStringLiteral("downloads"))) {
                child[QStringLiteral("downloads")] = chain[i][QStringLiteral("downloads")];
                break;
            }
        }
    }

    // Remove inheritsFrom — version is now standalone
    child.remove(QStringLiteral("inheritsFrom"));

    return child;
}

// ============================================================
// Helper: Resolve version ID to actual directory path
// ============================================================
QString ModLoaderInstaller::findVersionDir(const QString& versionId) const
{
    QString directPath = m_gameDir + QStringLiteral("/versions/") + versionId;
    if (QDir(directPath).exists())
        return directPath;
    // Scan all version directories for JSON whose "id" matches
    QDir vdir(m_gameDir + QStringLiteral("/versions"));
    for (const QString& dir : vdir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir sub(vdir.absoluteFilePath(dir));
        for (const QString& jf : sub.entryList({QStringLiteral("*.json")}, QDir::Files)) {
            QFile pf(sub.absoluteFilePath(jf));
            if (!pf.open(QIODevice::ReadOnly)) continue;
            QJsonDocument doc = QJsonDocument::fromJson(pf.readAll());
            pf.close();
            if (doc.isObject() && doc.object()[QStringLiteral("id")].toString() == versionId)
                return sub.absolutePath();
        }
    }
    return QString();
}

