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
#include <QSet>
#include <QSharedPointer>
#include "utils/lzma/LzmaDec.h"
#include "utils/logger.h"

// ── JAR manifest attribute injector (Java streaming, zero memory) ──
using namespace ShadowLauncher;
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
            qCInfo(logLoader) << "[ModLoader] Java manifest inject exitCode:" << exitCode
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
#include <private/qzipwriter_p.h>

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
    qCInfo(logLoader) << "[ModLoader] Forge from data (verify-only): MC" << mcVersion << "Forge" << forgeVersion;
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
    qCInfo(logLoader) << "[ModLoader] NeoForge from data (verify-only): MC" << mcVersion << "NeoForge" << neoVersion;
    neoStep2_verify(installerJar);
}

void ModLoaderInstaller::forgeContinueInstall()
{
    if (m_cachedJar.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] forgeContinueInstall but no cached jar";
        emit finished(false, "无缓存的安装程序");
        return;
    }
    qCInfo(logLoader) << "[ModLoader] Continuing Forge install";
    m_verifyOnly = false;
    m_running = true;
    forgeStep3_install(m_cachedJar);
}

void ModLoaderInstaller::neoForgeContinueInstall()
{
    if (m_cachedJar.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] neoForgeContinueInstall but no cached jar";
        emit finished(false, "无缓存的安装程序");
        return;
    }
    qCInfo(logLoader) << "[ModLoader] Continuing NeoForge install (build from installer)";
    m_verifyOnly = false;
    m_running = true;
    m_totalSteps = 3;  // buildVersion (step 3)
    neoForgeStep3_buildVersion(m_cachedJar);
}

void ModLoaderInstaller::installForge(const QString& mcVersion, const QString& forgeVersion,
                                       const QString& installName, const QString& expectedSha1) {
    qCInfo(logLoader) << "[ModLoader] installForge call — m_running before:" << m_running;
    if (m_running) return;
    m_running = true; m_cancelled = false; m_usedFallback = false;
    m_expectedForgeSha1 = expectedSha1;
    qCInfo(logLoader) << "[ModLoader] installForge started — m_running:" << m_running;
    m_mcVersion = mcVersion; m_loaderVersion = forgeVersion;
    m_installName = installName; m_loaderType = "forge";
    m_totalSteps = 3; m_currentStep = 0;
    qCInfo(logLoader) << "[ModLoader] Forge: MC" << mcVersion << "Forge" << forgeVersion;
    forgeStep1_downloadInstaller();
}

void ModLoaderInstaller::installFabric(const QString& mcVersion, const QString& fabricVersion,
                                        const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false;
    m_mcVersion = mcVersion; m_loaderVersion = fabricVersion;
    m_installName = installName; m_loaderType = "fabric";
    m_totalSteps = 2; m_currentStep = 0;  // download → write (no SHA1 for tiny profile JSON)
    qCInfo(logLoader) << "[ModLoader] Fabric: MC" << mcVersion << "Fabric" << fabricVersion;
    fabricStep1_downloadProfile();
}

void ModLoaderInstaller::installNeoForge(const QString& mcVersion, const QString& neoVersion,
                                          const QString& installName) {
    if (m_running) return;
    m_running = true; m_cancelled = false; m_usedFallback = false;
    m_mcVersion = mcVersion; m_loaderVersion = neoVersion;
    m_installName = installName; m_loaderType = "neoforge";
    m_totalSteps = 3; m_currentStep = 0;
    qCInfo(logLoader) << "[ModLoader] NeoForge: MC" << mcVersion << "NeoForge" << neoVersion;
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
    qCInfo(logLoader) << "[ModLoader] Optifine: MC" << mcVersion << "Opti" << optifineVersion
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
                qCInfo(logLoader) << "[ModLoader] BMCLAPI Optifine download failed, trying official...";
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
                qCInfo(logLoader) << "[ModLoader] BMCLAPI Optifine download failed, trying official...";
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
        qCWarning(logLoader) << "[ModLoader] OptiFine JAR invalid, preview:" << preview;
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
        qCInfo(logLoader) << "[ModLoader] OptiFine JAR entries (" << fileList.size() << "total, first" << maxLog << "):" << paths;
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
        qCInfo(logLoader) << "[ModLoader] OptiFine synthetic: using bmclType/bmclPatch" << m_optifineBmclType << m_optifineBmclPatch;
    } else {
        // Fallback: bmclType/bmclPatch not provided → derive from optifineVer
        qCInfo(logLoader) << "[ModLoader] OptiFine synthetic: bmclType/bmclPatch EMPTY, deriving from loaderVersion=" << m_loaderVersion;
        QString ver = m_loaderVersion;
        QString prefix = "OptiFine_" + m_mcVersion + "_";
        if (ver.startsWith(prefix)) ver = ver.mid(prefix.length());
        libSuffix = m_mcVersion + "_" + ver;
    }
    QString libName = "optifine:OptiFine:" + libSuffix;
    QString libPath = "optifine/OptiFine/" + libSuffix;
    QString libJar = "OptiFine-" + libSuffix + ".jar";

    qCInfo(logLoader) << "[ModLoader] OptiFine synthetic install: library =" << libName << libSuffix;

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

    QString baseJsonPath = m_gameDir + "/versions/" + m_mcVersion + "/" + m_mcVersion + ".json";
    QFile baseJsonFile(baseJsonPath);
    if (baseJsonFile.open(QIODevice::ReadOnly)) {
        // Start from base MC JSON, merge OptiFine on top
        QJsonDocument baseDoc = QJsonDocument::fromJson(baseJsonFile.readAll());
        baseJsonFile.close();
        versionJson = baseDoc.object();
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
    QString baseJarPath = m_gameDir + "/versions/" + m_mcVersion + "/" + m_mcVersion + ".jar";
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

    qCInfo(logLoader) << "[ModLoader] OptiFine synthetic install complete →" << versionId;
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
    qCInfo(logLoader) << "[ModLoader] OptiFine extracted" << copied << "bundled JARs";

    // Write version JSON
    QDir().mkpath(verDir);
    QFile jsonFile(verDir + "/" + versionId + ".json");
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(versionInfo);
        jsonFile.write(doc.toJson(QJsonDocument::Indented));
        jsonFile.close();
    }

    // Handle inheritsFrom
    QString inherits = versionInfo.value(QStringLiteral("inheritsFrom")).toString();
    if (!inherits.isEmpty() && inherits != versionId) {
        QString srcJar = m_gameDir + "/versions/" + inherits + "/" + inherits + ".jar";
        QString dstJar = verDir + "/" + versionId + ".jar";
        if (QFile::exists(srcJar) && !QFile::exists(dstJar)) QFile::copy(srcJar, dstJar);
    }

    qCInfo(logLoader) << "[ModLoader] OptiFine install (profile) complete →" << versionId;
    emit progressChanged(2, m_totalSteps, "OptiFine 安装完成");
    emit finished(true, QString());
    m_running = false;
}
// ── Fallback: run OptiFine installer via javaw (legacy / incompatible JAR) ──
void ModLoaderInstaller::runOptifineInstaller(const QByteArray& jarData) {
    qCInfo(logLoader) << "[ModLoader] OptiFine extract failed, falling back to javaw installer";
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

    qCInfo(logLoader) << "[ModLoader] Optifine install (isolated): javaw" << jargs;

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath, tempMcRoot, tempMcDir](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_cancelled || exitCode != 0) {
            cleanupTempMc(tempMcRoot);
            qCWarning(logLoader) << "[ModLoader] Optifine installer failed, exit:" << exitCode;
            emit finished(false, QString("OptiFine 安装程序运行失败（退出码: %1）").arg(exitCode));
            m_running = false;
            return;
        }
        collectForgeOutput(tempMcDir.absolutePath(), jarPath);
        cleanupTempMc(tempMcRoot);
        qCInfo(logLoader) << "[ModLoader] Optifine standalone install complete";
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

    qCInfo(logLoader) << "[ModLoader] Temp .minecraft created:" << tempMc;
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
        qCInfo(logLoader) << "[ModLoader] Copied version:" << d;
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
    qCInfo(logLoader) << "[ModLoader] Temp cleaned:" << tempDir;
}

void ModLoaderInstaller::cleanupAfterInstall(const QStringList& dirsToClean) {
    const int maxRetries = 3;
    const int retryDelayMs = 500;

    for (const QString& dirPath : dirsToClean) {
        QDir dir(dirPath);
        if (!dir.exists()) {
            qCInfo(logLoader) << "[ModLoader] Cleanup skip (not found):" << dirPath;
            continue;
        }

        bool ok = false;
        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            ok = dir.removeRecursively();
            if (ok) {
                qCInfo(logLoader) << "[ModLoader] Cleanup OK:" << dirPath
                         << (attempt > 1 ? QStringLiteral("(attempt %1)").arg(attempt) : QString());
                break;
            }
            // Check what's left
            QStringList leftovers = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            qCInfo(logLoader) << "[ModLoader] Cleanup FAIL (attempt" << attempt << "/" << maxRetries
                     << "):" << dirPath << "leftovers:" << leftovers;
            if (attempt < maxRetries) {
                QThread::msleep(retryDelayMs);
            }
        }

        if (!ok) {
            qCWarning(logLoader) << "[ModLoader] Cleanup GAVE UP after" << maxRetries
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
        qCInfo(logLoader) << "[ModLoader] BMCLAPI Forge download failed, trying official Maven...";
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
    emit progressChanged(2, m_totalSteps, "正在获取 Forge SHA1 校验值...");
    QString verArg = m_mcVersion + "-" + m_loaderVersion;

    if (m_usedFallback) {
        // Official source: fetch SHA1 from Maven
        QString sha1Url = QString("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/forge-%1-installer.jar.sha1").arg(verArg);
        downloadSmall(sha1Url, [this, actualSha1, jarData](bool ok, const QByteArray& sha1Data) {
            if (!ok || sha1Data.isEmpty()) {
                qCWarning(logLoader) << "[ModLoader] Cannot fetch official Forge SHA1, skip verification";
                emit verifyFinished(false);
                if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
                forgeStep3_install(jarData);
                return;
            }
            emit progressChanged(2, 2, "正在比对 Forge SHA1 校验值...");
            QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
            bool match = (actualSha1 == expectedSha1);
            qCInfo(logLoader) << "[ModLoader] Forge SHA1 (official) expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
            emit verifyFinished(match);
            if (!match) { emit finished(false, "Forge 安装程序校验失败（SHA1 不匹配）"); m_running = false; return; }
            if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
            forgeStep3_install(jarData);
        });
        return;
    }

    // BMCLAPI: use cached SHA1 from version list (avoids redundant network fetch)
    if (!m_expectedForgeSha1.isEmpty()) {
        emit progressChanged(2, m_totalSteps, QStringLiteral("正在比对 Forge SHA1 校验值..."));
        bool match = (actualSha1 == m_expectedForgeSha1);
        qCInfo(logLoader) << "[ModLoader] Forge SHA1 (cached) expected:" << m_expectedForgeSha1 << "actual:" << actualSha1 << "match:" << match;
        emit verifyFinished(match);
        if (!match) { emit finished(false, QStringLiteral("Forge 安装程序校验失败（SHA1 不匹配）")); m_running = false; return; }
        if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
        forgeStep3_install(jarData);
        return;
    }

    // Fallback: fetch SHA1 from Forge version list API
    QString url = QString("https://bmclapi2.bangbang93.com/forge/minecraft/%1").arg(m_mcVersion);

    downloadSmall(url, [this, jarData](bool ok, const QByteArray& forgeData) {
        if (!ok || forgeData.isEmpty()) {
            qCWarning(logLoader) << "[ModLoader] Cannot fetch SHA1, skip verification";
            emit verifyFinished(false);
            if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
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
            qCWarning(logLoader) << "[ModLoader] No SHA1 found for Forge" << m_loaderVersion;
        }

        qCInfo(logLoader) << "[ModLoader] Forge SHA1 expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
        emit verifyFinished(match);
        if (!match && !expectedSha1.isEmpty()) {
            emit finished(false, "Forge 安装程序校验失败（SHA1 不匹配）");
            m_running = false;
            return;
        }
        if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
        forgeStep3_install(jarData);
    });
}


// ═══════════════════════════════════════════════════════════════
// Forge install — extracts version.json from installer
// JAR, downloads all libraries via BMCLAPI mirrors, then writes
// the version config to gameDir. No java process needed.
// ═══════════════════════════════════════════════════════════════

void ModLoaderInstaller::forgeStep3_install(const QByteArray& jarData) {
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
    qCInfo(logLoader) << "[ModLoader] Extracted" << extractedCount << "jars from installer";

    // 3. Read install_profile.json → get version.json path
    QByteArray profileData = reader.fileData(QStringLiteral("install_profile.json"));
    if (profileData.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] No install_profile.json, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QJsonDocument profileDoc = QJsonDocument::fromJson(profileData);
    if (!profileDoc.isObject()) {
        qCWarning(logLoader) << "[ModLoader] Invalid install_profile.json, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QString versionJsonPath = profileDoc.object().value(QStringLiteral("json")).toString();
    if (versionJsonPath.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] No json path in install_profile, running installer directly";
        runInstallerProcess(jarData);
        return;
    }

    // 4. Read version.json → get game libraries
    QString actualPath = versionJsonPath.startsWith(QLatin1Char('/')) ? versionJsonPath.mid(1) : versionJsonPath;
    QByteArray versionData = reader.fileData(actualPath);
    if (versionData.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] No version.json in installer, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QJsonDocument versionDoc = QJsonDocument::fromJson(versionData);
    if (!versionDoc.isObject()) {
        qCWarning(logLoader) << "[ModLoader] Invalid version.json, running installer directly";
        runInstallerProcess(jarData);
        return;
    }
    QJsonArray libraries = versionDoc.object().value(QStringLiteral("libraries")).toArray();
    QJsonObject versionJson = versionDoc.object();

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

    qCInfo(logLoader) << "[ModLoader] Pre-downloading" << downloads.size() << "libs (skipped" << (libraries.size() - downloads.size()) << ")";

    if (downloads.isEmpty()) {
        forgeStep3_manualFinalize(jarData, versionJson);
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
            forgeStep3_manualFinalize(jarData, versionJson);
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
                // BMCLAPI failed → retry BMCLAPI, then Maven Central as last resort
                QString mavenUrl = QString(t.url);
                if (t.url.contains(QStringLiteral("bmclapi2.bangbang93.com"))) {
                    mavenUrl.replace(
                        QStringLiteral("bmclapi2.bangbang93.com"),
                        QStringLiteral("repo1.maven.org"));
                }
                downloadToFile(mavenUrl, t.path, [=](bool ok2, const QString& err2) {
                    if (!ok2)
                        qCWarning(logLoader) << "[ModLoader] Lib unavailable (installer will handle):" << t.name << err2;
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

void ModLoaderInstaller::forgeStep3_manualFinalize(const QByteArray& jarData, const QJsonObject& versionJson)
{
    emit stepProgress(3, 50);
    emit progressChanged(3, m_totalSteps, "正在提取 Forge 客户端文件...");

    // 1. Extract pre-merged client jar from installer
    QBuffer buffer;
    buffer.setData(jarData);
    if (!buffer.open(QIODevice::ReadOnly)) {
        qCWarning(logLoader) << "[ModLoader] Cannot open installer for manual finalize";
        runInstallerProcess(jarData);
        return;
    }
    QZipReader reader(&buffer);

    // Path in installer: maven/net/minecraftforge/forge/<ver>/forge-<ver>-client.jar
    QString clientJarPath = QStringLiteral("maven/net/minecraftforge/forge/%1/forge-%1-client.jar")
        .arg(m_loaderVersion);
    QByteArray clientJarBytes = reader.fileData(clientJarPath);
    reader.close();

    if (clientJarBytes.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] No pre-merged client jar in installer, falling back to JVM";
        runInstallerProcess(jarData);
        return;
    }

    // 2. Write client jar to version folder
    const QString verDir = m_gameDir + QStringLiteral("/versions/") + m_installName;
    QDir().mkpath(verDir);
    const QString jarDst = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
    QFile jf(jarDst);
    if (jf.open(QIODevice::WriteOnly)) {
        jf.write(clientJarBytes);
        jf.close();
        qCInfo(logLoader) << "[ModLoader] Extracted pre-merged client jar:" << clientJarBytes.size() << "bytes →" << jarDst;
    } else {
        qCWarning(logLoader) << "[ModLoader] Cannot write client jar, falling back to JVM";
        runInstallerProcess(jarData);
        return;
    }

    emit stepProgress(3, 75);
    emit progressChanged(3, m_totalSteps, "正在生成 Forge 版本配置...");

    // 3. Write version JSON (merged with vanilla MC)
    QJsonObject json = versionJson;
    json[QStringLiteral("id")] = m_installName;

    const QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
    const QString mcJsonPath = m_gameDir + QStringLiteral("/versions/%1/%1.json").arg(m_mcVersion);

    if (QFile::exists(mcJsonPath)) {
        QFile mcf(mcJsonPath);
        if (mcf.open(QIODevice::ReadOnly)) {
            QJsonObject mcObj = QJsonDocument::fromJson(mcf.readAll()).object();
            mcf.close();

            // Merge libraries: MC first, Forge after
            QJsonArray mcLibs = mcObj[QStringLiteral("libraries")].toArray();
            QJsonArray forgeLibs = json[QStringLiteral("libraries")].toArray();
            QJsonArray merged;
            for (const auto& v : mcLibs) merged.append(v);
            for (const auto& v : forgeLibs) merged.append(v);
            json[QStringLiteral("libraries")] = merged;

            // Merge game arguments
            QJsonArray mcGameArgs = mcObj[QStringLiteral("arguments")].toObject()
                [QStringLiteral("game")].toArray();
            QJsonObject forgeArgs = json[QStringLiteral("arguments")].toObject();
            QJsonArray forgeGameArgs = forgeArgs[QStringLiteral("game")].toArray();
            for (const auto& v : mcGameArgs) forgeGameArgs.append(v);
            forgeArgs[QStringLiteral("game")] = forgeGameArgs;
            json[QStringLiteral("arguments")] = forgeArgs;

            // Inherit fields from MC parent
            auto cpIfNeeded = [&](const QString& key) {
                QJsonValue v = json[key];
                if (v.isUndefined() || v.isNull()) json[key] = mcObj[key];
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
    }

    QFile out(jsonPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
        out.close();
        qCInfo(logLoader) << "[ModLoader] Forge version JSON written:" << jsonPath;
    }

    emit stepProgress(3, 100);
    emit progressChanged(3, m_totalSteps, "Forge 安装完成");
    emit finished(true, QString());
    m_running = false;
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
    qCInfo(logLoader) << "[ModLoader] Running Forge installer: java" << args;

    auto* proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, jarPath](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        QFile::remove(jarPath);

        QString stdoutStr = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString stderrStr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        qCInfo(logLoader) << "[ModLoader] Installer exitCode:" << exitCode
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
                    qCInfo(logLoader) << "[ModLoader] Copied client jar to" << jarPath;
                    copied = true;
                } else if (QFile::exists(universalJar) && QFile::copy(universalJar, jarPath)) {
                    qCInfo(logLoader) << "[ModLoader] Copied universal jar to" << jarPath;
                    copied = true;
                } else if (QFile::exists(explicitUniversalJar) && QFile::copy(explicitUniversalJar, jarPath)) {
                    qCInfo(logLoader) << "[ModLoader] Copied explicit-universal jar to" << jarPath;
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

            // Merge vanilla MC JSON into Forge JSON before deleting vanilla folder
            // Forge installer may create JSON with inheritsFrom referencing vanilla MC
            const QString forgeJsonPath = m_gameDir + QStringLiteral("/versions/") + m_installName
                + QStringLiteral("/") + m_installName + QStringLiteral(".json");
            const QString mcJsonPath = m_gameDir + QStringLiteral("/versions/%1/%1.json").arg(m_mcVersion);
            if (QFile::exists(forgeJsonPath) && QFile::exists(mcJsonPath)) {
                QFile fjf(forgeJsonPath);
                if (fjf.open(QIODevice::ReadOnly)) {
                    QJsonObject forgeObj = QJsonDocument::fromJson(fjf.readAll()).object();
                    fjf.close();
                    if (forgeObj.contains(QStringLiteral("inheritsFrom"))) {
                        QFile mcf(mcJsonPath);
                        if (mcf.open(QIODevice::ReadOnly)) {
                            QJsonObject mcObj = QJsonDocument::fromJson(mcf.readAll()).object();
                            mcf.close();
                            // Merge libraries: MC libs first, Forge libs after
                            QJsonArray mcLibs = mcObj[QStringLiteral("libraries")].toArray();
                            QJsonArray forgeLibs = forgeObj[QStringLiteral("libraries")].toArray();
                            QJsonArray merged;
                            for (const auto& v : mcLibs) merged.append(v);
                            for (const auto& v : forgeLibs) merged.append(v);
                            forgeObj[QStringLiteral("libraries")] = merged;
                            // Merge game arguments
                            QJsonObject mcArgs = mcObj[QStringLiteral("arguments")].toObject();
                            QJsonObject forgeArgs = forgeObj[QStringLiteral("arguments")].toObject();
                            QJsonArray mcGameArgs = mcArgs[QStringLiteral("game")].toArray();
                            QJsonArray forgeGameArgs = forgeArgs[QStringLiteral("game")].toArray();
                            for (const auto& v : mcGameArgs) forgeGameArgs.append(v);
                            forgeArgs[QStringLiteral("game")] = forgeGameArgs;
                            forgeObj[QStringLiteral("arguments")] = forgeArgs;
                            // Remove inheritsFrom
                            forgeObj.remove(QStringLiteral("inheritsFrom"));
                            // Copy inherited fields
                            auto cpIfNeeded = [&](const QString& key) {
                                QJsonValue v = forgeObj[key];
                                if (v.isUndefined() || v.isNull()) forgeObj[key] = mcObj[key];
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
                            QFile out(forgeJsonPath);
                            if (out.open(QIODevice::WriteOnly)) {
                                out.write(QJsonDocument(forgeObj).toJson(QJsonDocument::Indented));
                                out.close();
                            }
                            qCInfo(logLoader) << "[ModLoader] Forge JSON merged with vanilla MC libraries";
                        }
                    }
                }
            }

            // Vanilla MC cleanup deferred to VersionBackend (avoids race with other installs)
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
        qCInfo(logLoader) << "[ModLoader] BMCLAPI Fabric download failed, trying official...";
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

    qCInfo(logLoader) << "[ModLoader] Fabric library download:" << m_fabricLibTasks.size() << "files needed";
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

            qCInfo(logLoader) << "[ModLoader] Fabric lib" << taskIdx << "downloading" << url;
            QDir().mkpath(QFileInfo(taskSavePath).absolutePath());
            downloadToFile(url, taskSavePath, [this, taskIdx, taskSavePath, dlNext, tryMirror, idx](bool ok, const QString& err) {
                if (ok) {
                    qint64 fileSize = QFileInfo(taskSavePath).size();
                    qCInfo(logLoader) << "[ModLoader] Fabric lib" << taskIdx << "OK:" << fileSize << "bytes";
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
                    qCInfo(logLoader) << "[ModLoader] Fabric lib" << taskIdx << "mirror" << idx << "FAILED:" << err;
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
    qCInfo(logLoader) << "[ModLoader] Fabric finalize: writing version JSON (parallel mode)";
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

    // Merge vanilla MC JSON content into Fabric JSON (same pattern as NeoForge)
    // Fabric profile from meta.fabricmc.net uses inheritsFrom — vanilla libs/natives
    // must be merged before deleting the vanilla version folder
    if (json.contains(QStringLiteral("inheritsFrom"))) {
        const QString mcJsonPath = m_gameDir + QStringLiteral("/versions/%1/%1.json").arg(m_mcVersion);
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
                                    qCInfo(logLoader) << "[ModLoader] Fabric dedup: replaced" << mcName << "->" << name;
                                } else {
                                    qCInfo(logLoader) << "[ModLoader] Fabric dedup: kept" << mcName << "(newer than" << name << ")";
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
                qCInfo(logLoader) << "[ModLoader] Fabric JSON merged with vanilla MC libraries";
            }
        }
    }

    // Vanilla MC cleanup deferred to VersionBackend (avoids race with other installs)
    qCInfo(logLoader) << "[ModLoader] Fabric version JSON:" << jsonPath;
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
        qCInfo(logLoader) << "[ModLoader] BMCLAPI NeoForge download failed, trying official Maven...";
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

    emit progressChanged(2, m_totalSteps, "正在获取 NeoForge SHA1 校验值...");

    // NeoForge Maven provides .sha1 files (BMCLAPI does not mirror them)
    QString sha1Url = QString("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar.sha1")
                           .arg(m_loaderVersion);

    downloadSmall(sha1Url, [this, jarData](bool ok, const QByteArray& sha1Data) {
        if (!ok || sha1Data.isEmpty()) {
            qCWarning(logLoader) << "[ModLoader] Cannot fetch NeoForge SHA1, skip verification";
            emit verifyFinished(false);
            if (m_verifyOnly) { m_cachedJar = jarData; emit waitingForMC(); return; }
            neoForgeStep3_buildVersion(jarData);
            return;
        }

        QString expectedSha1 = QString::fromUtf8(sha1Data).trimmed();
        QString actualSha1 = computeSha1(jarData);
        bool match = (actualSha1 == expectedSha1);

        emit progressChanged(2, m_totalSteps, "正在比对 NeoForge SHA1 校验值...");

        qCInfo(logLoader) << "[ModLoader] NeoForge SHA1 expected:" << expectedSha1 << "actual:" << actualSha1 << "match:" << match;
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
    qCInfo(logLoader) << "[ModLoader] Extracted" << extractedCount << "jars from NeoForge installer";

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
            .replace(QStringLiteral("maven.neoforged.net/releases"),
                     QStringLiteral("bmclapi2.bangbang93.com/maven"));

        downloads.append({bmclUrl, localPath, lib.value(QStringLiteral("name")).toString()});
    }

    // Also ensure universal JAR is available (it's not in installer's version.json, but required at launch)
    {
        QString uniRel = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
        QString uniPath = libBase + QStringLiteral("/") + uniRel;
        if (!QFile::exists(uniPath)) {
            QString uniUrl = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
            qCInfo(logLoader) << "[ModLoader] NeoForge universal JAR not found, queueing download:" << uniRel;
            downloads.append({uniUrl, uniPath, QStringLiteral("neoforge-universal")});
        }
    }

    qCInfo(logLoader) << "[ModLoader] NeoForge: downloading" << downloads.size()
             << "libs (skipped" << (libraries.size() - downloads.size()) << ")";

    if (downloads.isEmpty()) {
        m_cachedJar = jarData;
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
            m_cachedJar = jarData;
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
                             QStringLiteral("maven.neoforged.net/releases"));
                downloadToFile(origUrl, t.path, [=](bool ok2, const QString& err2) {
                    if (!ok2)
                        qCWarning(logLoader) << "[ModLoader] NeoForge lib unavailable:" << t.name << err2;
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

    qCInfo(logLoader) << "[ModLoader] NeoForge version JSON written:" << jsonPath;

    // Step 4: Manual install — use cached jar data (from download-to-memory)
    neoManualFinalize(versionInfo);
}

// ── LZMA decompression via public-domain LZMA SDK ──
static QByteArray decompressLzma(const QByteArray& compressed)
{
    if (compressed.size() < 13)
        return {};  // LZMA header is 13 bytes minimum

    // LZMA header: byte 0 = properties, bytes 1-4 = dictSize (LE), bytes 5-12 = uncompSize (LE)
    Byte props[LZMA_PROPS_SIZE];
    props[0] = (Byte)compressed[0];
    UInt64 uncompSize = 0;
    for (int i = 0; i < 4; i++)
        props[1 + i] = (Byte)compressed[1 + i];
    for (int i = 0; i < 8; i++)
        uncompSize |= ((UInt64)(Byte)compressed[5 + i]) << (8 * i);

    size_t outLen = (size_t)uncompSize;
    size_t srcLen = (size_t)(compressed.size() - 13);

    // Fallback if uncompressed size is unknown (-1): allocate generously
    if (uncompSize == (UInt64)-1)
        outLen = (size_t)compressed.size() * 10;

    QByteArray result((int)outLen, Qt::Uninitialized);

    ELzmaStatus status;
    SRes res = LzmaDecode(
        reinterpret_cast<Byte*>(result.data()), &outLen,
        reinterpret_cast<const Byte*>(compressed.constData()) + 13, &srcLen,
        props, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &g_Alloc);

    if (res != SZ_OK) {
        qCWarning(logLoader) << "[ModLoader] LZMA decode failed, result=" << res;
        return {};
    }

    result.resize((int)outLen);
    return result;
}

void ModLoaderInstaller::neoManualFinalize(const QJsonObject& versionJson)
{
    // Step 3 (same step as "安装 NeoForge" in rebuildSteps)
    emit progressChanged(3, m_totalSteps, "正在提取 NeoForge 客户端文件...");
    emit stepProgress(3, 25);

    // 1. Use cached jar data (saved by neoForgeStep3_buildVersion)
    if (m_cachedJar.isEmpty()) {
        qCWarning(logLoader) << "[ModLoader] No cached NeoForge installer jar, cannot extract client";
        finished(true, QString());
        m_running = false;
        return;
    }
    QBuffer buf;
    buf.setData(m_cachedJar);
    if (!buf.open(QIODevice::ReadOnly)) {
        qCWarning(logLoader) << "[ModLoader] Cannot read installer buffer";
        finished(true, QString());
        m_running = false;
        return;
    }
    QZipReader reader(&buf);

    // Try new path first (26.x+ FancyModLoader — data/client.lzma), then old paths (21.x)
    QString clientJarInternal;
    QByteArray clientJarBytes;

    // Path 1: NeoForge 26.2+ — LZMA-compressed client in data/client.lzma
    {
        QByteArray lzma = reader.fileData(QStringLiteral("data/client.lzma"));
        if (!lzma.isEmpty()) {
            clientJarBytes = decompressLzma(lzma);
            if (clientJarBytes.isEmpty()) {
                qCWarning(logLoader) << "[ModLoader] LZMA decompression of client.lzma failed";
            } else {
                qCInfo(logLoader) << "[ModLoader] Decompressed client.lzma:" << clientJarBytes.size() << "bytes";
                // Merge in vanilla MC jar resources (assets, data, etc.) not present in patched jar
                const QString mcJarPath = m_gameDir + QStringLiteral("/versions/%1/%1.jar").arg(m_mcVersion);
                QFile mcFile(mcJarPath);
                if (mcFile.open(QIODevice::ReadOnly)) {
                    QByteArray vanillaBytes = mcFile.readAll();
                    mcFile.close();
                    if (!vanillaBytes.isEmpty()) {
                        QBuffer patchedBuf(&clientJarBytes);
                        QBuffer vanillaBuf(&vanillaBytes);
                        QZipReader patchedReader(&patchedBuf);
                        QZipReader vanillaReader(&vanillaBuf);
                        // Collect entries already in patched jar
                        QSet<QString> patchedEntries;
                        for (const auto& fi : patchedReader.fileInfoList())
                            patchedEntries.insert(fi.filePath);
                        // Build merged jar: patched entries + missing vanilla entries
                        QByteArray merged;
                        QBuffer mergedBuf(&merged);
                        QZipWriter writer(&mergedBuf);
                        // Copy patched entries first
                        for (const auto& fi : patchedReader.fileInfoList()) {
                            writer.addFile(fi.filePath, patchedReader.fileData(fi.filePath));
                        }
                        // Copy vanilla entries not in patched
                        for (const auto& fi : vanillaReader.fileInfoList()) {
                            if (!patchedEntries.contains(fi.filePath)) {
                                writer.addFile(fi.filePath, vanillaReader.fileData(fi.filePath));
                            }
                        }
                        patchedReader.close();
                        vanillaReader.close();
                        writer.close();
                        clientJarBytes = merged;
                        qCInfo(logLoader) << "[ModLoader] Merged vanilla resources:" << clientJarBytes.size() << "bytes";
                    }
                }
            }
        }
    }

    // Path 2: Old NeoForge (21.x) — pre-packaged maven JARs
    if (clientJarBytes.isEmpty()) {
        clientJarInternal = QStringLiteral("maven/net/neoforged/minecraft-client-patched/%1/minecraft-client-patched-%1.jar")
            .arg(m_loaderVersion);
        clientJarBytes = reader.fileData(clientJarInternal);
    }
    if (clientJarBytes.isEmpty()) {
        clientJarInternal = QStringLiteral("maven/net/neoforged/neoforge/%1/neoforge-%1-client.jar")
            .arg(m_loaderVersion);
        clientJarBytes = reader.fileData(clientJarInternal);
    }
    reader.close();

    emit stepProgress(3, 50);
    emit progressChanged(3, m_totalSteps, "正在写入 NeoForge 客户端文件...");

    // 2. Copy client jar to version folder
    const QString verDir = m_gameDir + QStringLiteral("/versions/") + m_installName;
    QDir().mkpath(verDir);
    const QString jarDst = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".jar");
    bool jarWritten = false;
    if (!clientJarBytes.isEmpty()) {
        QFile jf(jarDst);
        if (jf.open(QIODevice::WriteOnly)) {
            jf.write(clientJarBytes);
            jf.close();
            jarWritten = true;
            qCInfo(logLoader) << "[ModLoader] Extracted NeoForge client jar from installer:" << clientJarBytes.size() << "bytes →" << jarDst;
        }
    }

    // Fallback: copy from pre-extracted libraries (if neoForgeStep3_buildVersion extracted them)
    if (!jarWritten) {
        const QString clientJarLib = m_gameDir + QStringLiteral("/libraries/net/neoforged/neoforge/%1/neoforge-%1-client.jar")
            .arg(m_loaderVersion);
        if (QFile::exists(clientJarLib)) {
            if (QFile::exists(jarDst)) QFile::remove(jarDst);
            jarWritten = QFile::copy(clientJarLib, jarDst);
        }
    }
    if (!jarWritten) {
        const QString patchedLib = m_gameDir + QStringLiteral("/libraries/net/neoforged/minecraft-client-patched/%1/minecraft-client-patched-%1.jar")
            .arg(m_loaderVersion);
        if (QFile::exists(patchedLib)) {
            if (QFile::exists(jarDst)) QFile::remove(jarDst);
            jarWritten = QFile::copy(patchedLib, jarDst);
        }
    }
    if (!jarWritten) {
        qCWarning(logLoader) << "[ModLoader] No NeoForge client jar found in installer, trying vanilla MC jar";
        const QString mcJar = m_gameDir + QStringLiteral("/versions/%1/%1.jar").arg(m_mcVersion);
        if (QFile::exists(mcJar)) {
            if (QFile::exists(jarDst)) QFile::remove(jarDst);
            jarWritten = QFile::copy(mcJar, jarDst);
            if (jarWritten)
                qCInfo(logLoader) << "[ModLoader] Copied vanilla MC jar as NeoForge client:" << mcJar << "→" << jarDst;
            else
                qCWarning(logLoader) << "[ModLoader] Failed to copy vanilla MC jar:" << mcJar;
        } else {
            qCWarning(logLoader) << "[ModLoader] Vanilla MC jar not found at" << mcJar;
        }
    }
    if (!jarWritten) {
        qCWarning(logLoader) << "[ModLoader] NeoForge client jar FAILED to write, game may not launch";
    }

    emit stepProgress(3, 75);
    emit progressChanged(3, m_totalSteps, "正在生成 NeoForge 版本配置...");

    // 3. Write final version JSON (merged with vanilla MC)
    const QString jsonPath = verDir + QStringLiteral("/") + m_installName + QStringLiteral(".json");
    QJsonObject json = versionJson;
    json[QStringLiteral("id")] = m_installName;
    // Always set jar field so FML can locate the client JAR
    json[QStringLiteral("jar")] = m_installName;
    // Remove inheritsFrom since we are building a standalone merged JSON
    json.remove(QStringLiteral("inheritsFrom"));

    const QString mcJsonPath = m_gameDir + QStringLiteral("/versions/%1/%1.json").arg(m_mcVersion);
    if (QFile::exists(mcJsonPath)) {
        QFile mcf(mcJsonPath);
        if (mcf.open(QIODevice::ReadOnly)) {
            QJsonObject mcObj = QJsonDocument::fromJson(mcf.readAll()).object();
            mcf.close();

            QJsonArray mcLibs = mcObj[QStringLiteral("libraries")].toArray();
            QJsonArray neoLibs = json[QStringLiteral("libraries")].toArray();
            QJsonArray merged;
            for (const auto& v : mcLibs) merged.append(v);
            for (const auto& v : neoLibs) merged.append(v);
            json[QStringLiteral("libraries")] = merged;

            QJsonArray mcGameArgs = mcObj[QStringLiteral("arguments")].toObject()[QStringLiteral("game")].toArray();
            QJsonObject neoArgs = json[QStringLiteral("arguments")].toObject();
            QJsonArray neoGameArgs = neoArgs[QStringLiteral("game")].toArray();
            for (const auto& v : mcGameArgs) neoGameArgs.append(v);
            neoArgs[QStringLiteral("game")] = neoGameArgs;
            json[QStringLiteral("arguments")] = neoArgs;

            auto cp = [&](const QString& k) { if (json[k].isUndefined() || json[k].isNull()) json[k] = mcObj[k]; };
            cp(QStringLiteral("assetIndex"));
            cp(QStringLiteral("assets"));
            cp(QStringLiteral("minimumLauncherVersion"));
            cp(QStringLiteral("type"));
            cp(QStringLiteral("releaseTime"));
            cp(QStringLiteral("time"));
            cp(QStringLiteral("javaVersion"));
            cp(QStringLiteral("logging"));
            cp(QStringLiteral("complianceLevel"));
            cp(QStringLiteral("downloads"));
        }
    }

    // Add universal JAR as library
    QJsonArray libs = json[QStringLiteral("libraries")].toArray();
    QJsonObject uniLib;
    uniLib[QStringLiteral("name")] = QStringLiteral("net.neoforged:neoforge:%1").arg(m_loaderVersion);
    QJsonObject uniDl, uniArt;
    uniArt[QStringLiteral("path")] = QStringLiteral("net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
    uniArt[QStringLiteral("url")] = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-universal.jar").arg(m_loaderVersion);
    uniDl[QStringLiteral("artifact")] = uniArt;
    uniLib[QStringLiteral("downloads")] = uniDl;
    libs.prepend(uniLib);
    json[QStringLiteral("libraries")] = libs;

    QFile out(jsonPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
        out.close();
    }

    // 4. Inject Minecraft-Dists:client into jar manifest (NeoForge FML requirement)
    emit stepProgress(3, 90);
    emit progressChanged(3, m_totalSteps, "正在注入清单属性...");
    injectJarManifestAttributeAsync(jarDst,
        QStringLiteral("Minecraft-Dists"), QStringLiteral("client"),
        [this](bool ok) {
            if (!ok)
                qCWarning(logLoader) << "[ModLoader] WARNING: Failed to inject Minecraft-Dists:client";
            else
                qCInfo(logLoader) << "[ModLoader] Injected Minecraft-Dists:client into" << m_installName;

            emit stepProgress(3, 100);
            emit progressChanged(3, m_totalSteps, "NeoForge 安装完成");
            emit finished(true, QString());
            m_running = false;
        });
}

void ModLoaderInstaller::renameVersionFolder(const QString& oldName, const QString& newName)
{
    const QString vd = versionsDir();
    const QString oldDir = vd + QStringLiteral("/") + oldName;
    const QString newDir = vd + QStringLiteral("/") + newName;

    if (!QDir(oldDir).exists()) {
        qCInfo(logLoader) << "[ModLoader] renameVersionFolder: old not found, skip" << oldDir;
        return;
    }
    if (QDir(newDir).exists()) {
        qCInfo(logLoader) << "[ModLoader] renameVersionFolder: target exists, skip" << newDir;
        return;
    }

    // 1. Rename folder
    if (!QDir().rename(oldDir, newDir)) {
        qCWarning(logLoader) << "[ModLoader] renameVersionFolder: folder rename failed" << oldDir << "→" << newDir;
        return;
    }

    // 2. Rename JSON
    const QString oldJson = newDir + QStringLiteral("/") + oldName + QStringLiteral(".json");
    const QString newJson = newDir + QStringLiteral("/") + newName + QStringLiteral(".json");
    if (QFile::exists(oldJson) && !QFile::rename(oldJson, newJson)) {
        qCWarning(logLoader) << "[ModLoader] renameVersionFolder: json rename failed" << oldJson << "→" << newJson;
    }

    // 3. Rename JAR (if exists)
    const QString oldJar = newDir + QStringLiteral("/") + oldName + QStringLiteral(".jar");
    const QString newJar = newDir + QStringLiteral("/") + newName + QStringLiteral(".jar");
    if (QFile::exists(oldJar) && !QFile::rename(oldJar, newJar)) {
        qCWarning(logLoader) << "[ModLoader] renameVersionFolder: jar rename failed" << oldJar << "→" << newJar;
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

    qCInfo(logLoader) << "[ModLoader] Renamed version:" << oldName << "→" << newName;
}

