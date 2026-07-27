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
    if (!bmclType.isEmpty() && !bmclPatch.isEmpty()) {
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/%3").arg(mcVersion, bmclType, bmclPatch);
        filename = QString("OptiFine_%1_%2_%3.jar").arg(mcVersion, bmclType, bmclPatch);
    } else {
        filename = (optifineVersion.startsWith("OptiFine_") || optifineVersion.startsWith("preview_OptiFine_"))
            ? optifineVersion + ".jar"
            : QString("OptiFine_%1_%2.jar").arg(mcVersion, optifineVersion);
        url = QString("https://bmclapi2.bangbang93.com/optifine/%1/%2/download").arg(mcVersion, filename);
    }

    if (standalone) {
        // Standalone: run installer to create version JSON
        m_totalSteps = 2;
        m_currentStep = 1;
        emit progressChanged(1, m_totalSteps, "正在下载 OptiFine...");
        downloadToMemory(url, [this, filename](bool ok, const QByteArray& data) {
            if (!ok) {
                qCInfo(logLoader) << QStringLiteral("BMCLAPI OptiFine 下载失败，尝试官方源...");
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
                qCInfo(logLoader) << QStringLiteral("BMCLAPI OptiFine 下载失败，尝试官方源...");
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

    // 1. Copy JAR to libraries/
    QString libDir = m_gameDir + "/libraries/" + libPath;
    QDir().mkpath(libDir);
    QString libTarget = libDir + "/" + libJar;
    QFile libFile(libTarget);
    if (!libFile.open(QIODevice::WriteOnly)) {
        emit finished(false, "无法写入 OptiFine 库文件");
        m_running = false;
        return;
    }
    libFile.write(jarData);
    libFile.close();

    // 2. Create self-contained version JSON (no inheritsFrom → safe to delete base MC)
    QString versionId = m_installName;
    QJsonObject versionJson;

    // Resolve MC version directory (handles name mismatch like 26.2 vs 1.21.5)
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

    // Override/set id
    versionJson["id"] = versionId;
    // Remove inheritsFrom (self-contained, no parent dependency)
    versionJson.remove(QStringLiteral("inheritsFrom"));
    versionJson["type"] = QStringLiteral("release");

    // Add OptiFine library to the existing libraries array
    QJsonArray libraries = versionJson.value(QStringLiteral("libraries")).toArray();
    QJsonObject libObj;
    libObj["name"] = libName;
    libraries.append(libObj);
    versionJson["libraries"] = libraries;

    // 3. Copy base MC JAR to OptiFine version folder (self-contained, no inheritsFrom)
    QString baseJarPath;
    if (!mcOptifineDir.isEmpty())
        baseJarPath = mcOptifineDir + "/" + QDir(mcOptifineDir).dirName() + ".jar";
    QString verDir = m_gameDir + "/versions/" + versionId;
    QDir().mkpath(verDir);
    QString optiJarPath = verDir + "/" + versionId + ".jar";
    if (QFile::exists(baseJarPath) && !QFile::exists(optiJarPath)) {
        QFile::copy(baseJarPath, optiJarPath);
        versionJson["jar"] = versionId;  // use our own JAR, not base
    } else if (QFile::exists(optiJarPath)) {
        versionJson["jar"] = versionId;
    }

    // 4. Write to versions/
    QDir().mkpath(verDir);
    QFile jsonFile(verDir + "/" + versionId + ".json");
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit finished(false, "无法写入版本配置文件");
        m_running = false;
        return;
    }
    QJsonDocument doc(versionJson);
    jsonFile.write(doc.toJson(QJsonDocument::Indented));
    jsonFile.close();

    qCInfo(logLoader) << QStringLiteral("OptiFine 合成安装完成 → %1").arg(versionId);
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

    // Write version JSON
    QDir().mkpath(verDir);
    QFile jsonFile(verDir + "/" + versionId + ".json");
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(versionInfo);
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
    }

    // Handle inheritsFrom: resolve directory and copy JAR
    QString inherits = versionInfo.value(QStringLiteral("inheritsFrom")).toString();
    if (!inherits.isEmpty() && inherits != versionId) {
        QString parentDir = findVersionDir(inherits);
        if (!parentDir.isEmpty()) {
            QString srcJar = parentDir + "/" + QDir(parentDir).dirName() + ".jar";
            QString dstJar = verDir + "/" + versionId + ".jar";
            if (QFile::exists(srcJar) && !QFile::exists(dstJar)) QFile::copy(srcJar, dstJar);
        }
        // Remove inheritsFrom from written JSON (standalone now)
        QString jsonPath2 = verDir + "/" + versionId + ".json";
        QFile jf2(jsonPath2);
        if (jf2.open(QIODevice::ReadOnly)) {
            QJsonDocument jd2 = QJsonDocument::fromJson(jf2.readAll());
            jf2.close();
            if (jd2.isObject()) {
                QJsonObject jo2 = jd2.object();
                jo2.remove(QStringLiteral("inheritsFrom"));
                if (jf2.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    jf2.write(QJsonDocument(jo2).toJson(QJsonDocument::Indented));
                    jf2.close();
                }
            }
        }
    }

    qCInfo(logLoader) << QStringLiteral("OptiFine 安装完成（profile 模式）→ %1").arg(versionId);
    emit progressChanged(2, m_totalSteps, "OptiFine 安装完成");
    emit finished(true, QString());
    m_running = false;
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

    qCInfo(logLoader) << QStringLiteral("OptiFine 安装（隔离模式）: javaw %1").arg(jargs.join(QStringLiteral(" ")));

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

    proc->start("javaw", jargs);
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

    // Ordered fallback URLs: BMCLAPI branch→new→old → Official branch→new→old
    QStringList urlList;
    auto addUrl = [&](const QString& base, const QString& ver) {
        urlList.append(QString("%1/net/minecraftforge/forge/%2/forge-%2-installer.jar").arg(base, ver));
    };
    if (!vBranch.isEmpty()) { addUrl("https://bmclapi2.bangbang93.com/maven", vBranch); }
    addUrl("https://bmclapi2.bangbang93.com/maven", vNew);
    addUrl("https://bmclapi2.bangbang93.com/maven", vOld);
    if (!vBranch.isEmpty()) { addUrl("https://maven.minecraftforge.net", vBranch); }
    addUrl("https://maven.minecraftforge.net", vNew);
    addUrl("https://maven.minecraftforge.net", vOld);
    auto urls = std::make_shared<QStringList>(urlList);
    auto idx = std::make_shared<int>(0);
    auto tryNext = std::make_shared<std::function<void()>>();

    *tryNext = [this, urls, idx, tryNext]() {
        if (*idx >= urls->size()) {
            qCWarning(logLoader) << "Forge installer download failed after all fallbacks";
            emit finished(false, "Forge 安装程序下载失败（所有镜像源均不可用）");
            m_running = false;
            return;
        }
        QString url = (*urls)[(*idx)++];
        qCInfo(logLoader) << QStringLiteral("尝试下载 Forge installer [%1/%2]: %3").arg(*idx).arg(urls->size()).arg(url);
        downloadToMemory(url, [this, tryNext](bool ok, const QByteArray& data) {
            if (ok) {
                forgeStep2_verify(data);
                return;
            }
            qCInfo(logLoader) << "Forge 下载失败，尝试下一个源...";
            (*tryNext)();
        }, "forge-installer.jar");
    };

    (*tryNext)();
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

    // 1. Open installer JAR as ZIP (needed early for Maven version in install_profile.json)
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        emit finished(false, "无法打开安装程序文件");
        m_running = false;
        return;
    }
    QZipReader reader(&buffer);

    // 2. Extract Maven version from install_profile.json data section
    //    (e.g. "1.18.2-20220404.173914" from [net.minecraft:client:1.18.2-20220404.173914:mappings@txt])
    //    This is the definitive version Forge uses — NOT the MC version JSON "time" field.
    QString mavenVer = m_mcVersion;  // fallback: plain MC version, no timestamp
    {
        QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
        if (!profileData.isEmpty()) {
            QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
            if (!profileDoc.isNull()) {
                QJsonObject data = profileDoc.object().value(QStringLiteral("data")).toObject();
                // Try MOJMAPS (net.minecraft:client) first, then MAPPINGS (mcp_config)
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
                const QString savePath = m_gameDir
                    + QStringLiteral("/libraries/net/minecraft/client/") + mavenVer
                    + QStringLiteral("/client-") + mavenVer + QStringLiteral("-mappings.txt");
                if (!QFileInfo::exists(savePath)) {
                    qCInfo(logLoader) << QStringLiteral("下载缺失的 client_mappings: %1").arg(mavenVer);
                    QNetworkAccessManager nm;
                    QNetworkReply* r = nm.get(QNetworkRequest(QUrl(cmUrl)));
                    QEventLoop loop;
                    connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                    loop.exec();
                    if (r->error() == QNetworkReply::NoError) {
                        QByteArray data = r->readAll();
                        QDir().mkpath(QFileInfo(savePath).absolutePath());
                        QFile out(savePath);
                        if (out.open(QIODevice::WriteOnly)) {
                            out.write(data);
                            out.close();
                            qCInfo(logLoader) << QStringLiteral("client_mappings 已保存: %1 (%2 KB)")
                                .arg(savePath).arg(data.size() / 1024);
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
    qCInfo(logLoader) << QStringLiteral("已从安装程序解压 %1 个 JAR").arg(extractedCount);

    // 3. Read install_profile.json
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    if (profileData.isEmpty()) {
        qCWarning(logLoader) << QStringLiteral("未找到 install_profile.json，走 Bootstrapper");
        reader.close();
        runBootstrapperProcess(jarData);
        finalizeBootstrapperInstall();
        return;
    }
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
    if (!profileDoc.isObject()) {
        qCWarning(logLoader) << QStringLiteral("install_profile.json 无效，走 Bootstrapper");
        reader.close();
        runBootstrapperProcess(jarData);
        finalizeBootstrapperInstall();
        return;
    }
    QJsonObject profileObj = profileDoc.object();

    // LOG: install_profile.json spec
    int spec = profileObj.value(QStringLiteral("spec")).toInt(-1);
    int procCount = profileObj.value(QStringLiteral("processors")).toArray().size();
    bool hasInstall = profileObj.contains(QStringLiteral("install"));
    bool hasJson = profileObj.contains(QStringLiteral("json"));
    qCInfo(logLoader) << QStringLiteral("=== Forge install_profile 分析: spec=%1 processors=%2 install=%3 json=%4 ===")
        .arg(spec).arg(procCount).arg(hasInstall).arg(hasJson);

    // ── Three-way branch ──
    // Note: 主流启动器's ForgelikeInjector (forge-installer.jar / com.bangbang93.ForgeInstaller)
    // handles BOTH Forge AND NeoForge identically via the bootstrapper path.
    // We do the same: NeoForge falls through to Branch C / Bootstrapper.
    //
    // Branch A: has "install" → Legacy 2 (universal JAR + inheritsFrom)
    if (hasInstall) {
        reader.close();
        qCInfo(logLoader) << QStringLiteral("→ 走 Legacy 2（universal JAR + inheritsFrom）");
        installLegacy2(jarData, profileObj);
        return;
    }

    // Branch B: has "json" AND no processors AND spec <= 0 → Legacy 1 (maven/ + version JSON only)
    // IMPORTANT: if processors exist, Legacy 1 will skip them → missing client.lzma patches
    // → Forge can't transform Minecraft classes → vanilla MC boots instead
    if (hasJson && procCount == 0 && spec <= 0) {
        reader.close();
        qCInfo(logLoader) << QStringLiteral("→ 走 Legacy 1（maven/ + version.json 直接写入）");
        installLegacy1(jarData, profileObj);
        return;
    }

    // Branch C: has processors OR spec >= 1 → Bootstrapper
    // 主流启动器's ForgelikeInjector handles BOTH Forge and NeoForge identically.
    // No separate fork for NeoForge — the bootstrapper JAR (com.bangbang93.ForgeInstaller)
    // processes any Forgelike installer (Forge / NeoForge) the same way.
    reader.close();
    qCInfo(logLoader) << QStringLiteral("→ 走 Bootstrapper（Method A：Java 注入器）");
    runBootstrapperProcess(jarData);
    finalizeBootstrapperInstall();
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
    //    For Forge: patch install_profile.json (DOWNLOAD_MOJMAPS → --skipIfExists)
    //    For NeoForge: write as-is, bootstrapper handles everything
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (!isNeoForge) {
        // ── Forge: patch DOWNLOAD_MOJMAPS processor ──
        QBuffer buf;
        buf.setData(jarData);
        if (!buf.open(QIODevice::ReadOnly)) {
            emit finished(false, "无法打开安装程序");
            m_running = false; return;
        }
        QZipReader reader(&buf);
        QList<QZipReader::FileInfo> entries = reader.fileInfoList();

        QByteArray profData = reader.fileData(QStringLiteral("install_profile.json"));
        bool patched = false;
        if (!profData.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(profData);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonArray procs = root.value(QStringLiteral("processors")).toArray();
                for (int i = 0; i < procs.size(); ++i) {
                    QJsonObject proc = procs[i].toObject();
                    QJsonArray args = proc.value(QStringLiteral("args")).toArray();
                    for (const auto& a : args) {
                        if (a.toString() == QStringLiteral("DOWNLOAD_MOJMAPS")) {
                            bool hasSkip = false;
                            for (const auto& sa : args)
                                if (sa.toString() == QStringLiteral("--skipIfExists")) { hasSkip = true; break; }
                            if (!hasSkip) {
                                args.append(QStringLiteral("--skipIfExists"));
                                proc[QStringLiteral("args")] = args;
                                procs[i] = proc;
                                patched = true;
                                qCInfo(logLoader) << QStringLiteral("已为 processor[%1] 添加 --skipIfExists").arg(i);
                            }
                            break;
                        }
                    }
                }
                if (patched) {
                    root[QStringLiteral("processors")] = procs;
                    profData = QJsonDocument(root).toJson(QJsonDocument::Compact);
                }
            }
        }

        installerJarPath = tempDir + QStringLiteral("/forge-installer-") + m_installName + QStringLiteral(".jar");
        if (patched) {
            QZipWriter writer(installerJarPath);
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
            qCInfo(logLoader) << QStringLiteral("已写入修改后的安装程序（已剥离签名）: %1").arg(installerJarPath);
        } else {
            QFile f(installerJarPath);
            if (f.open(QIODevice::WriteOnly)) { f.write(jarData); f.close(); }
            qCInfo(logLoader) << QStringLiteral("已写入原始安装程序（无需修改）: %1").arg(installerJarPath);
        }
        reader.close();
    } else {
        // ── NeoForge: write JAR as-is (no patching, bootstrapper handles everything) ──
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

            // 1. Download version manifest
            QByteArray manifestData = downloadUrl(
                QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest.json"), 15000);
            if (manifestData.isEmpty()) {
                qCWarning(logLoader) << QStringLiteral("无法下载 Mojang version manifest");
            } else {
                QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestData);
                QJsonArray versions = manifestDoc.object().value(QStringLiteral("versions")).toArray();
                QString versionUrl;
                for (const auto& v : versions) {
                    if (v.toObject().value(QStringLiteral("id")).toString() == m_mcVersion) {
                        versionUrl = v.toObject().value(QStringLiteral("url")).toString();
                        break;
                    }
                }
                if (versionUrl.isEmpty()) {
                    qCWarning(logLoader) << QStringLiteral("版本 %1 不在 Mojang manifest 中").arg(m_mcVersion);
                } else {
                    // 2. Download version JSON
                    QByteArray versionData = downloadUrl(versionUrl, 15000);
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
                            // 3. Download raw ProGuard mapping file (up to 60s for ~12MB)
                            QByteArray rawMapping = downloadUrl(mappingsUrl, 60000);
                            if (rawMapping.isEmpty()) {
                                qCWarning(logLoader) << QStringLiteral("下载 mapping 文件失败");
                            } else {
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

                                // 5. Find srgutils JAR (downloaded in step 3d)
                                QStringList srgCandidates = {
                                    m_gameDir + QStringLiteral("/libraries/net/minecraftforge/srgutils/0.5.10/srgutils-0.5.10.jar"),
                                    m_gameDir + QStringLiteral("/libraries/net/minecraftforge/srgutils/0.5.6/srgutils-0.5.6.jar"),
                                    m_gameDir + QStringLiteral("/libraries/net/minecraftforge/srgutils/0.5.1/srgutils-0.5.1.jar"),
                                };
                                QString srgutilsJar;
                                for (const auto& c : srgCandidates) {
                                    if (QFile::exists(c)) { srgutilsJar = c; break; }
                                }

                                if (srgutilsJar.isEmpty()) {
                                    qCWarning(logLoader) << QStringLiteral("未找到 srgutils JAR");
                                } else {
                                    QString classDir = QDir::tempPath();
                                    QString javaSrc = classDir + QStringLiteral("/ProGuardToTSRG.java");
                                    QString classFile = classDir + QStringLiteral("/ProGuardToTSRG.class");

                                    // 6. Compile Java converter (always recompile to avoid Java version mismatch)
                                    {
                                        QFile sf(javaSrc);
                                        if (sf.open(QIODevice::WriteOnly)) {
                                            sf.write(QByteArray(
                                                "import java.io.*;\n"
                                                "import java.nio.file.*;\n"
                                                "import net.minecraftforge.srgutils.*;\n"
                                                "public class ProGuardToTSRG {\n"
                                                "    public static void main(String[] a) throws Exception {\n"
                                                "        if (a.length<2) { System.err.println(\"Usage: ...\"); System.exit(1); }\n"
                                                "        File i = new File(a[0]), o = new File(a[1]);\n"
                                                "        o.getParentFile().mkdirs();\n"
                                                "        try (InputStream is = new BufferedInputStream(new FileInputStream(i))) {\n"
                                                "            IMappingFile m = IMappingFile.load(is);\n"
                                                "            if (m == null) { System.err.println(\"Failed to load\"); System.exit(1); }\n"
                                                "            m.write(o.toPath(), IMappingFile.Format.TSRG, false);\n"
                                                "        }\n"
                                                "    }\n"
                                                "}\n"
                                            ));
                                            sf.close();
                                        }

                                        // Resolve javac (same Java as bootstrapper — not system PATH which may differ)
                                        QString javacPath = QStandardPaths::findExecutable(
                                            QStringLiteral("javac"),
                                            {QFileInfo(javaPath).absolutePath()});
                                        if (javacPath.isEmpty())
                                            javacPath = QStandardPaths::findExecutable(QStringLiteral("javac"));
                                        QProcess javacProc;
                                        QStringList javacArgs;
                                        javacArgs << QStringLiteral("-cp") << QDir::toNativeSeparators(srgutilsJar)
                                                  << QStringLiteral("-d") << QDir::toNativeSeparators(classDir)
                                                  << QDir::toNativeSeparators(javaSrc);
                                        javacProc.start(javacPath, javacArgs);
                                        javacProc.waitForFinished(30000);
                                        if (javacProc.exitCode() != 0) {
                                            qCWarning(logLoader) << QStringLiteral("javac 编译失败: ")
                                                + QString::fromUtf8(javacProc.readAllStandardError());
                                        }
                                    }

                                    // 7. Run converter: java ProGuardToTSRG raw.txt out.tsrg
                                    if (QFile::exists(classFile)) {
                                        QString outPath = tsrgDir + QStringLiteral("/client-")
                                            + m_mcVersion + QStringLiteral("-mappings.tsrg");
                                        QProcess runProc;
                                        runProc.start(javaPath,
                                            QStringList() << QStringLiteral("-cp")
                                                << (QDir::toNativeSeparators(srgutilsJar) + QStringLiteral(";") + QDir::toNativeSeparators(classDir))
                                                << QStringLiteral("ProGuardToTSRG")
                                                << QDir::toNativeSeparators(rawPath)
                                                << QDir::toNativeSeparators(outPath));
                                        if (runProc.waitForFinished(30000) && runProc.exitCode() == 0) {
                                            qCInfo(logLoader) << QStringLiteral("mappings -> TSRG 转换成功: ") + outPath;
                                        } else {
                                            qCWarning(logLoader) << QStringLiteral("mappings -> TSRG 转换失败: ")
                                                + QString::fromUtf8(runProc.readAllStandardError());
                                        }
                                    }
                                }

                                // 8. Clean up raw temp file
                                QFile::remove(rawPath);
                            }
                        }
                    }
                }
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

        QStringList launchArgs;
        // 主流启动器: extract BOTH forge-installer.jar (helper) and java-wrapper.jar (wrapper)
        QString wrapperJar = extractJavaWrapperPath();
        if (wrapperJar.isEmpty()) {
            qCWarning(logLoader) << QStringLiteral("无法释放 java-wrapper.jar，跳过 JavaWrapper");
            useWrapper = false;
        }
        if (useWrapper && !wrapperJar.isEmpty()) {
            // 主流启动器: -Doolloo.jlw.tmpdir=... -cp forge_installer.jar;installer.jar -jar java-wrapper.jar com.bangbang93.ForgeInstaller
            // java-wrapper.jar fixes CJK encoding on Windows (JDK-8272352)
            launchArgs << QStringLiteral("-Doolloo.jlw.tmpdir=%1").arg(QDir::toNativeSeparators(m_gameDir.trimmed()));
            launchArgs << QStringLiteral("-cp") << (bootstrapperJar + QStringLiteral(";") + installerJarPath);
            launchArgs << QStringLiteral("-jar") << wrapperJar;
            launchArgs << QStringLiteral("com.bangbang93.ForgeInstaller");
        } else {
            // 主流启动器 fallback: -cp forge_installer.jar;installer.jar com.bangbang93.ForgeInstaller
            // (no JavaWrapper; may have CJK issues on Windows with UTF-8 Beta enabled)
            launchArgs << QStringLiteral("-cp") << (bootstrapperJar + QStringLiteral(";") + installerJarPath);
            launchArgs << QStringLiteral("com.bangbang93.ForgeInstaller");
        }
        launchArgs << QDir::toNativeSeparators(m_gameDir);

        // 主流启动器: --add-exports cpw.mods.bootstraplauncher/... for Java 9+
        if (minJava >= 9) {
            launchArgs.prepend(QStringLiteral("cpw.mods.bootstraplauncher/cpw.mods.bootstraplauncher=ALL-UNNAMED"));
            launchArgs.prepend(QStringLiteral("--add-exports"));
        }

        qCInfo(logLoader) << QStringLiteral("运行 Bootstrapper（%1）: %2 %3")
            .arg(useWrapper ? QStringLiteral("JavaWrapper") : QStringLiteral("裸 Java"),
                 javaPath, launchArgs.join(QStringLiteral(" ")));

        emit progressChanged(3, m_totalSteps,
            QStringLiteral("正在通过 Java 运行 %1 安装器...").arg(loaderName));

        QProcess proc;
        proc.start(javaPath, launchArgs);
        if (!proc.waitForStarted(10000)) {
            if (useWrapper) {
                qCWarning(logLoader) << QStringLiteral("JavaWrapper 启动失败，将重试（裸 Java）");
                continue;
            }
            QFile::remove(installerJarPath);
            emit finished(false, QStringLiteral("无法启动 %1 安装器 Java 进程").arg(loaderName));
            m_running = false;
            return;
        }

        // Synchronous loop with progress keywords (matching 主流启动器's ForgelikeInjectorLine)
        QElapsedTimer elapsed;
        elapsed.start();
        const int pollMs = 100;
        const int timeoutMs = 180000;
        outputLines.clear();
        bool procFinished = false;

        while (!procFinished) {
            if (m_cancelled) {
                proc.kill();
                proc.waitForFinished(3000);
                QFile::remove(installerJarPath);
                emit finished(false, QStringLiteral("用户取消"));
                m_running = false;
                return;
            }
            if (elapsed.elapsed() > timeoutMs) {
                timedOut = true;
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
                        if (line == QStringLiteral("Extracting json"))
                            emit stepProgress(3, 27);
                        else if (line == QStringLiteral("Downloading libraries"))
                            emit stepProgress(3, 28);
                        else if (line.startsWith(QStringLiteral("File exists: Checksum validated.")))
                            qCInfo(logLoader) << line;
                        else if (line == QStringLiteral("Building Processors"))
                            emit stepProgress(3, 38);
                        else if (line == QStringLiteral("Task: DOWNLOAD_MOJMAPS"))
                            emit stepProgress(3, 40);
                        else if (line == QStringLiteral("Task: MERGE_MAPPING"))
                            emit stepProgress(3, 50);
                        else if (line.startsWith(QStringLiteral("Splitting: ")))
                            emit stepProgress(3, 55);
                        else if (line == QStringLiteral("Parameter Annotations"))
                            emit stepProgress(3, 60);
                        else if (line == QStringLiteral("Processing Complete") || line == QStringLiteral("log: null"))
                            emit stepProgress(3, 67);
                        else if (line == QStringLiteral("Sorting"))
                            emit stepProgress(3, 80);
                        else if (line == QStringLiteral("Remapping final jar"))
                            emit stepProgress(3, 85);
                        else if (line == QStringLiteral("Remapping jar... 50%"))
                            emit stepProgress(3, 90);
                        else if (line == QStringLiteral("Remapping jar... 100%"))
                            emit stepProgress(3, 95);
                        else if (line == QStringLiteral("Injecting profile"))
                            emit stepProgress(3, 98);
                        else
                            qCInfo(logLoader) << line;
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

        bootExitCode = proc.exitCode();

        // 主流启动器: check if output contains "true" in the last 5 lines
        bool hasTrue = outputLines.contains(QStringLiteral("true"));
        if (!hasTrue) {
            for (int i = qMax(0, outputLines.size() - 5); i < outputLines.size(); ++i) {
                if (outputLines[i] == QStringLiteral("true")) { hasTrue = true; break; }
            }
        }

        if (bootExitCode == 0 && hasTrue) {
            // Success — break out of retry loop
            break;
        }

        // 主流启动器: retry without JavaWrapper on failure
        if (useWrapper) {
            qCWarning(logLoader) << QStringLiteral("使用 JavaWrapper 安装 %1 失败（退出码=%2），将不使用 JavaWrapper 并重试")
                .arg(loaderName).arg(bootExitCode);
        }
    }

    // Cleanup installer JAR
    QFile::remove(installerJarPath);

    if (timedOut) {
        emit finished(false, QStringLiteral("%1 安装器超时，请检查 Java 配置后重试").arg(loaderName));
        m_running = false;
        return;
    }

    // Determine success (主流启动器: exitCode == 0 && (output has "true" OR last 5 lines contain "true"))
    bool success = (bootExitCode == 0);
    if (success) {
        bool hasTrue = outputLines.contains(QStringLiteral("true"));
        if (!hasTrue) {
            for (int i = qMax(0, outputLines.size() - 5); i < outputLines.size(); ++i) {
                if (outputLines[i] == QStringLiteral("true")) { hasTrue = true; break; }
            }
        }
        success = hasTrue;
    }

    if (success) {
        // ── Find new version folder by comparing with snapshot (matching 主流启动器's OldList approach) ──
        QStringList newVersions = QDir(versionsDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QStringList delta;
        for (const QString& v : newVersions) {
            if (!oldVersions.contains(v))
                delta.append(v);
        }

        // 主流启动器 logic:
        // Delta.Count == 1 → use new folder
        // Delta.Count > 1  → filter by non-empty + name contains "forge"
        // Delta.Count == 0 → predicted folder name
        QString foundSub;
        if (delta.size() == 1) {
            foundSub = delta[0];
            qCInfo(logLoader) << QStringLiteral("新增版本文件夹: %1").arg(foundSub);
        } else if (delta.size() > 1) {
            for (const QString& d : delta) {
                QDir dDir(versionsDir() + QStringLiteral("/") + d);
                if (dDir.exists() && dDir.entryList(QDir::Files).size() > 0) {
                    if (d.contains(QStringLiteral("forge"), Qt::CaseInsensitive)) {
                        foundSub = d;
                        qCInfo(logLoader) << QStringLiteral("选定新增版本文件夹（含 Forge 关键词）: %1").arg(foundSub);
                        break;
                    }
                    if (foundSub.isEmpty()) {
                        foundSub = d;
                        qCInfo(logLoader) << QStringLiteral("暂选新增版本文件夹: %1").arg(foundSub);
                    }
                }
            }
        }

        if (!foundSub.isEmpty()) {
            QString jsonPath = versionsDir() + QStringLiteral("/") + foundSub
                + QStringLiteral("/") + foundSub + QStringLiteral(".json");
            QString jarPathV = versionsDir() + QStringLiteral("/") + foundSub
                + QStringLiteral("/") + foundSub + QStringLiteral(".jar");

            if (foundSub != m_installName) {
                renameVersionFolder(foundSub, m_installName);
                jsonPath = versionsDir() + QStringLiteral("/") + m_installName
                    + QStringLiteral("/") + m_installName + QStringLiteral(".json");
                jarPathV = versionsDir() + QStringLiteral("/") + m_installName
                    + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
                qCInfo(logLoader) << QStringLiteral("已重命名版本文件夹: %1 → %2").arg(foundSub, m_installName);
            }

            // 主流启动器: step 5 just copies JSON (resolves inheritsFrom later via MergeJson).
            // We store the path so forgeStep3_install can do the flatten post-step.
            m_postJsonPath = jsonPath;
        } else {
            qCWarning(logLoader) << QStringLiteral("Bootstrapper 完成后未找到新增版本文件夹");
        }

        m_bootstrapperOk = true;
        // DON'T emit finished here — caller (forgeStep3_install) does the flatten step + emit.
        m_running = false;
    } else {
        m_bootstrapperOk = false;
        m_bootstrapperError = QStringLiteral("%1 安装器返回错误（退出码=%2）").arg(loaderName).arg(bootExitCode);
        // DON'T emit finished here either — caller handles it.
        m_running = false;
    }
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
                // NeoForge: skip this! The patched client is in the version folder JAR, not as a library.
                // Injecting net.minecraft:client adds the VANILLA client to classpath, which makes
                // NeoForge think the patched client is missing (it sees the unpatched vanilla one).
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

                // NeoForge: remove downloads.client (points to vanilla MC; launcher would redownload & overwrite patched)
        if (isNeo) {
            flattened.remove(QStringLiteral("downloads"));
            // Ensure net.neoforged:neoforge:{ver}:universal is in libraries (FMLLoader needs NeoForgeMod.class)
            // The bootstrapper's version.json might not include this entry; it's on disk but not in the JSON.
            QString nfName = QStringLiteral("net.neoforged:neoforge:%1:universal").arg(m_loaderVersion);
            QJsonArray nfLibs = flattened.value(QStringLiteral("libraries")).toArray();
            bool hasNf = false;
            for (const auto& lib : nfLibs) {
                if (lib.toObject().value(QStringLiteral("name")).toString() == nfName) {
                    hasNf = true; break;
                }
            }
            if (!hasNf) {
                QString nfPath = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
                QJsonObject artifact;
                artifact[QStringLiteral("path")] = nfPath;
                QJsonObject dlObj;
                dlObj[QStringLiteral("artifact")] = artifact;
                QJsonObject nfEntry;
                nfEntry[QStringLiteral("name")] = nfName;
                nfEntry[QStringLiteral("downloads")] = dlObj;
                nfLibs.append(nfEntry);
                flattened[QStringLiteral("libraries")] = nfLibs;
                qCInfo(logLoader) << QStringLiteral("已添加 NeoForge universal JAR 到 libraries: %1").arg(nfName);
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

        } // end if (!isNeo) for MC client injection
        // Copy the correct JAR to version folder (always overwrite in case of stale file)
        QString targetDir = versionsDir() + QStringLiteral("/") + m_installName;
        QString jarPathV = targetDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
        QFile::remove(jarPathV);
        bool jarCopied = false;
        if (!isNeo) {
            // Forge: -client.jar (installer-patched) or -universal.jar (loader)
            const QString clientJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                + QStringLiteral("/") + ver + QStringLiteral("/")
                + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-client.jar");
            const QString universalJar = m_gameDir + QStringLiteral("/libraries/") + loaderGroup
                + QStringLiteral("/") + ver + QStringLiteral("/")
                + filePrefix + QStringLiteral("-") + ver + QStringLiteral("-universal.jar");
            if (QFile::exists(clientJar) && QFile::copy(clientJar, jarPathV)) {
                qCInfo(logLoader) << QStringLiteral("已复制 Forge client JAR 到 %1").arg(jarPathV);
                jarCopied = true;
            } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPathV)) {
                qCInfo(logLoader) << QStringLiteral("已复制 Forge universal JAR 到 %1").arg(jarPathV);
                jarCopied = true;
            }
        } else {
            // NeoForge: copy the patched client from processor output
            // Processor: --output libraries/net/neoforged/minecraft-client-patched/{ver}/{name}.jar
            const QString patchedJar = m_gameDir
                + QStringLiteral("/libraries/net/neoforged/minecraft-client-patched/")
                + m_loaderVersion + QStringLiteral("/minecraft-client-patched-")
                + m_loaderVersion + QStringLiteral(".jar");
            if (QFile::exists(patchedJar) && QFile::copy(patchedJar, jarPathV)) {
                qCInfo(logLoader) << QStringLiteral("已复制 NeoForge patched client JAR 到 %1").arg(jarPathV);
                jarCopied = true;
            } else {
                qCWarning(logLoader) << QStringLiteral("未找到 NeoForge patched client JAR: %1").arg(patchedJar);
            }
        }
        if (!jarCopied) {
            qCWarning(logLoader) << QStringLiteral("无法复制版本 JAR 文件");
        }
    }

    emit finished(true, QString());
    m_running = false;
}
void ModLoaderInstaller::fabricStep1_downloadProfile() {
    m_currentStep = 1;
    emit progressChanged(1, m_totalSteps, "正在下载 Fabric 配置...");

    QString url = QString("https://bmclapi2.bangbang93.com/fabric-meta/v2/versions/loader/%1/%2/profile/json")
                      .arg(m_mcVersion, m_loaderVersion);

    downloadToMemory(url, [this](bool ok, const QByteArray& data) {
        if (ok) {
            
            fabricStep2_downloadLibraries(data);  // Step 2: download Fabric libraries
            return;
        }
        // Fallback to official Fabric meta
        qCInfo(logLoader) << QStringLiteral("BMCLAPI Fabric 下载失败，尝试官方源...");
        QString officialUrl = QString("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
                                   .arg(m_mcVersion, m_loaderVersion);
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) { emit finished(false, "Fabric 配置下载失败（BMCLAPI 和官方源均失败）"); m_running = false; return; }
            
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
    // 主流启动器: package name differs for 1.20.1 legacy (uses "forge" instead of "neoforge")
    // DlNeoForgeListEntry.UrlBase: PackageName = If(Inherit = "1.20.1", "forge", "neoforge")
    bool isLegacy = (m_mcVersion == QStringLiteral("1.20.1"));
    const QString pkg = isLegacy ? QStringLiteral("forge") : QStringLiteral("neoforge");
    const QString apiName = isLegacy ? QStringLiteral("1.20.1-%1").arg(ver) : ver;

    // BMCLAPI mirror (主流启动器: Url.Replace("maven.neoforged.net/releases", "bmclapi2.bangbang93.com/maven"))
    const QString bmclUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/%1/%2/%1-%2-installer.jar")
                                .arg(pkg, apiName);
    // Official Maven
    const QString officialUrl = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/%1/%2/%1-%2-installer.jar")
                                    .arg(pkg, apiName);

    downloadToMemory(bmclUrl, [this, officialUrl](bool ok, const QByteArray& data) {
        if (ok) {
            neoStep2_verify(data);
            return;
        }
        qCInfo(logLoader) << QStringLiteral("BMCLAPI NeoForge 下载失败，尝试官方 Maven...");
        downloadToMemory(officialUrl, [this](bool ok2, const QByteArray& data2) {
            if (!ok2) {
                emit finished(false, QStringLiteral("NeoForge 安装程序下载失败（BMCLAPI 和官方源均失败）"));
                m_running = false;
                return;
            }
            neoStep2_verify(data2);
        }, QStringLiteral("neoforge-installer.jar"));
    }, QStringLiteral("neoforge-installer.jar"));
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

