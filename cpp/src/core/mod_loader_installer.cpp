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
#include <QDataStream>
#include <private/qzipreader_p.h>

// ── CRC-32 for ZIP ──
namespace {
static const quint32 kCrc32Table[256] = {
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
    0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
    0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
    0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C7B,0x58684CED,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
    0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
    0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
    0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
    0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8BEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
    0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB30A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
    0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xD8BBDE25,0x41B23DEB,0x36B56B7D,
    0xAFB0B63B,0xD8B7EDAD,0x41BEFC17,0x36B9DC81,0xA8DD490A,0xDFDA799C,0x46D32826,0x31D418B0,
    0xABEAB321,0xDCEDC0B7,0x45E4D10D,0x32E3E19B,0xAC865E78,0xDB816EEE,0x4289DF54,0x358EF5C2,
};

quint32 crc32(const QByteArray& data) {
    quint32 c = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); ++i)
        c = kCrc32Table[(c ^ quint8(data[i])) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

// Raw deflate from qCompress (strip Qt+zlib wrappers)
QByteArray rawDeflate(const QByteArray& data) {
    QByteArray c = qCompress(data);
    if (c.size() < 10) return data; // fallback to store
    return c.mid(6, c.size() - 10);
}

QByteArray rawInflate(const QByteArray& data, quint32 uncompSize) {
    // Re-wrap with zlib header+trailer for qUncompress
    QByteArray wrapped;
    wrapped.append(char(0x78));  // CMF
    wrapped.append(char(0x9C));  // FLG (default compression)
    wrapped.append(data);
    // Adler-32 (can be zero — qUncompress skips verification)
    wrapped.append(char(0)); wrapped.append(char(0));
    wrapped.append(char(0)); wrapped.append(char(0));
    QByteArray result = qUncompress(wrapped);
    if (result.isEmpty() && uncompSize > 0) {
        // Try as raw stored
        return data;
    }
    return result.isEmpty() ? data : result;
}

// ── Pure C++ JAR manifest attribute injector (no external tools) ──
bool injectJarManifestAttribute(const QString& jarPath,
                                  const QString& key, const QString& value) {
    const QString tmpPath = jarPath + QStringLiteral(".tmp");
    QZipReader reader(jarPath);
    if (!reader.isReadable()) return false;

    // Collect entries
    struct Entry {
        QString path;
        QByteArray data;
        bool isDir = false;
    };
    QList<Entry> entries;
    const QList<QZipReader::FileInfo> infos = reader.fileInfoList();
    for (const QZipReader::FileInfo& info : infos) {
        Entry e;
        e.path = info.filePath;
        e.isDir = info.isDir;
        if (!e.isDir)
            e.data = reader.fileData(info.filePath);
        entries.append(e);
    }
    reader.close();

    // Modify MANIFEST.MF
    bool modified = false;
    for (auto& e : entries) {
        if (e.path == QStringLiteral("META-INF/MANIFEST.MF")) {
            QByteArray mf = e.data;
            if (!mf.contains(key.toUtf8())) {
                QByteArrayList lines = mf.split('\n');
                QByteArrayList out;
                for (const auto& line : lines) {
                    if (!line.startsWith(key.toUtf8()))
                        out.append(line);
                }
                QByteArrayList result;
                for (const auto& line : out) {
                    result.append(line);
                    if (line.startsWith("Main-Class:")) {
                        QByteArray attr = key.toUtf8() + ": " + value.toUtf8();
                        result.append(attr);
                    }
                }
                e.data = result.join(QByteArrayLiteral("\r\n"));
                modified = true;
            }
            break;
        }
    }
    if (!modified) { QFile::remove(tmpPath); return true; } // already has attr

    // Write new ZIP
    QFile outFile(tmpPath);
    if (!outFile.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&outFile);
    ds.setByteOrder(QDataStream::LittleEndian);

    struct CDF {
        QString name;
        quint32 crc = 0;
        quint32 compSize = 0;
        quint32 uncompSize = 0;
        quint32 lhOffset = 0;
        quint16 method = 0;
    };
    QList<CDF> cdEntries;
    quint32 offset = 0;

    for (auto& e : entries) {
        QByteArray nameBytes = e.path.toUtf8();
        QByteArray storeData = e.isDir ? QByteArray() : e.data;
        QByteArray compData;
        quint16 method;
        if (e.isDir) {
            method = 0; // store
            compData = storeData;
        } else {
            QByteArray deflated = rawDeflate(storeData);
            if (!deflated.isEmpty() && deflated.size() < storeData.size()) {
                method = 8; // deflate
                compData = deflated;
            } else {
                method = 0; // store
                compData = storeData;
            }
        }
        quint32 crc = e.isDir ? 0 : crc32(storeData);
        quint32 compSz = compData.size();
        quint32 uncompSz = storeData.size();

        // Local file header
        ds << quint32(0x04034b50);  // signature
        ds << quint16(20);          // version needed
        ds << quint16(0);           // flags
        ds << method;
        ds << quint16(0);           // mod time
        ds << quint16(0);           // mod date
        ds << crc;
        ds << compSz;
        ds << uncompSz;
        ds << quint16(nameBytes.size());
        ds << quint16(0);           // extra field
        ds.writeRawData(nameBytes.constData(), nameBytes.size());
        if (!compData.isEmpty())
            ds.writeRawData(compData.constData(), compData.size());

        cdEntries.append({e.path, crc, compSz, uncompSz, offset, method});
        offset += 30 + nameBytes.size();
        if (!compData.isEmpty()) offset += compData.size();
    }

    // Central directory
    quint32 cdOffset = offset;
    for (const auto& cd : cdEntries) {
        QByteArray nameBytes = cd.name.toUtf8();
        ds << quint32(0x02014b50);  // signature
        ds << quint16(20);          // version made by
        ds << quint16(20);          // version needed
        ds << quint16(0);           // flags
        ds << cd.method;
        ds << quint16(0);           // mod time
        ds << quint16(0);           // mod date
        ds << cd.crc;
        ds << cd.compSize;
        ds << cd.uncompSize;
        ds << quint16(nameBytes.size());
        ds << quint16(0);           // extra
        ds << quint16(0);           // comment
        ds << quint16(0);           // disk start
        ds << quint16(0);           // internal attrs
        ds << quint32(0);           // external attrs
        ds << cd.lhOffset;
        ds.writeRawData(nameBytes.constData(), nameBytes.size());
    }
    quint32 cdSize = outFile.pos() - cdOffset;

    // End of central directory
    ds << quint32(0x06054b50);  // signature
    ds << quint16(0);           // disk number
    ds << quint16(0);           // start disk
    ds << quint16(cdEntries.size());
    ds << quint16(cdEntries.size());
    ds << cdSize;
    ds << cdOffset;
    ds << quint16(0);           // comment

    outFile.close();

    // Replace original
    if (!QFile::remove(jarPath)) return false;
    if (!QFile::rename(tmpPath, jarPath)) return false;
    return true;
}
} // anonymous namespace

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
            if (detail.isEmpty()) detail = QStringLiteral("无输出");
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
    emit progressChanged(4, m_totalSteps, QStringLiteral("正在安装 NeoForge（二进制补丁）..."));

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
            if (!injectJarManifestAttribute(jarPath,
                    QStringLiteral("Minecraft-Dists"), QStringLiteral("client"))) {
                qDebug() << "[ModLoader] WARNING: Failed to inject Minecraft-Dists into JAR";
            } else {
                qDebug() << "[ModLoader] Minecraft-Dists:client injected (pure C++)";
            }
        }

        // Delete installer's auto-named version folder
        QDir instDir(installedVerName);
        if (instDir.exists()) instDir.removeRecursively();

        // Delete vanilla MC version folder (merged into standalone JSON above)
        const QString vanillaVer = m_gameDir + QStringLiteral("/versions/") + m_mcVersion;
        QDir(vanillaVer).removeRecursively();

        if (exitCode == 0) {
            emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装完成");
            emit finished(true, QString());
        } else {
            emit progressChanged(m_currentStep, m_totalSteps, "NeoForge 安装失败");
            emit finished(false, QStringLiteral("NeoForge 安装程序失败 (exitCode=%1)").arg(exitCode));
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

