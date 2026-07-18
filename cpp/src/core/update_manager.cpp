// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 Shadow
#include "update_manager.h"
#include "http_client.h"
#include "utils/logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QCryptographicHash>

namespace ShadowLauncher {

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent)
{
    QTimer::singleShot(0, this, [this]() { m_initPhase = false; });
}

QString UpdateManager::stateDir() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/_update/");
}

void UpdateManager::setRepo(const QString& owner, const QString& repo)
{
    m_apiUrl = QStringLiteral("https://gitee.com/api/v5/repos/%1/%2/releases/latest")
                   .arg(owner, repo);
}

// ── State persistence ──

static QVersionNumber parseTag(const QString& tag)
{
    QString cleaned = tag;
    if (cleaned.startsWith(QLatin1Char('v')) || cleaned.startsWith(QLatin1Char('V')))
        cleaned = cleaned.mid(1);
    int dashIdx = cleaned.indexOf(QLatin1Char('-'));
    return QVersionNumber::fromString(dashIdx >= 0 ? cleaned.left(dashIdx) : cleaned);
}

void UpdateManager::saveState()
{
    if (m_initPhase) return;
    QDir().mkpath(stateDir());

    QJsonObject obj;
    obj["state"] = static_cast<int>(m_state);
    obj["download_version"] = m_downloadVersion;
    obj["download_url"] = m_downloadUrl;
    obj["download_path"] = m_downloadPath;
    obj["download_file_name"] = m_downloadFileName;
    obj["download_total"] = m_downloadTotal;
    obj["download_received"] = m_downloadReceived;
    obj["release_notes"] = m_releaseNotes;

    QFile f(stateDir() + "state.json");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        qCWarning(logApp) << "[UpdateManager] 保存状态文件失败 path="
                          << f.fileName() << "error=" << f.errorString();
    }
}

bool UpdateManager::hasPendingReady() const
{
    // If install lock exists, SLUpdater is running or crashed
    if (QFileInfo::exists(stateDir() + ".install_lock")) {
        qCInfo(logApp) << "[UpdateManager] 安装锁存在，跳过安装检查";
        return false;
    }

    QFile f(stateDir() + "state.json");
    if (!f.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(logApp) << "[UpdateManager] state.json 损坏，自动清除"
                          << "error:" << err.errorString();
        f.close();
        QFile::remove(stateDir() + "state.json");
        return false;
    }

    QJsonObject obj = doc.object();
    int s = obj.value("state").toInt(-1);
    if (s != Ready) return false;

    QString path = obj.value("download_path").toString();
    QString ver = obj.value("download_version").toString();
    qCInfo(logApp) << "[UpdateManager] 发现待安装更新 version=" << ver << "file=" << path;
    return QFileInfo::exists(path);
}

void UpdateManager::resumePausedDownload()
{
    QFile f(stateDir() + "state.json");
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(logApp) << "[UpdateManager] state.json 损坏(恢复断点时)，自动清除"
                          << "error:" << err.errorString();
        f.close();
        QFile::remove(stateDir() + "state.json");
        // Also clean up any partial download
        QDir d(stateDir());
        for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            if (fi.fileName() != "state.json")
                QFile::remove(fi.absoluteFilePath());
        }
        return;
    }

    QJsonObject obj = doc.object();
    int s = obj.value("state").toInt(-1);
    if (s != Paused) {
        qCInfo(logApp) << "[UpdateManager] 无待续传下载 state=" << s;
        return;
    }

    m_downloadVersion  = obj.value("download_version").toString();
    m_downloadUrl      = obj.value("download_url").toString();
    m_downloadPath     = obj.value("download_path").toString();
    m_downloadFileName = obj.value("download_file_name").toString();
    m_downloadTotal    = static_cast<qint64>(obj.value("download_total").toDouble());
    m_downloadReceived = static_cast<qint64>(obj.value("download_received").toDouble());

    qCInfo(logApp) << "[UpdateManager] 恢复断点下载:"
                   << m_downloadVersion << "已下载:" << m_downloadReceived;

    QFileInfo fi(m_downloadPath);
    if (!fi.exists() || fi.size() < m_downloadReceived) {
        qCWarning(logApp) << "[UpdateManager] 断点文件不匹配，重新下载";
        QFile::remove(m_downloadPath);
        m_downloadReceived = 0;
        setState(Idle);
        saveState();
        checkSilent();
        return;
    }

    m_userInitiated = false;
    resumeDownload(m_downloadReceived);
}

// ── Actions ──

void UpdateManager::setState(State s)
{
    if (m_state == s) return;
    qCInfo(logApp) << "[UpdateManager] 状态变更" << static_cast<int>(m_state)
                   << "->" << static_cast<int>(s);
    m_state = s;
    emit stateChanged(s);
    saveState();
}

void UpdateManager::checkSilent()
{
    if (isBusy()) return;
    qCInfo(logApp) << "[UpdateManager] 启动后自动检查更新";
    m_userInitiated = false;
    doCheck();
}

void UpdateManager::checkUserInitiated()
{
    if (isBusy()) {
        emit toastMessage(tr("正在检查中，请稍后..."));
        return;
    }
    qCInfo(logApp) << "[UpdateManager] 用户手动检查更新";
    m_userInitiated = true;
    doCheck();
}

void UpdateManager::doCheck()
{
    if (m_apiUrl.isEmpty()) {
        if (m_userInitiated) emit toastMessage(tr("更新地址未配置"));
        emit checkCompleted(false, QString());
        return;
    }

    setState(Checking);
    qCInfo(logApp) << "[UpdateManager] 检查更新... 当前版本:" << m_currentVersion;

    QVersionNumber current = parseTag(m_currentVersion);

    HttpClient::instance().get(m_apiUrl,
        [this, current](int status, const QByteArray& body) {
            if (status != 200 || body.isEmpty()) {
                qCWarning(logApp) << "[UpdateManager] API 失败 HTTP" << status;
                if (m_userInitiated)
                    emit toastMessage(tr("检查更新失败 (HTTP %1)").arg(status));
                setState(Idle);
                emit checkCompleted(false, QString());
                return;
            }

            QJsonObject release = QJsonDocument::fromJson(body).object();
            m_releaseCache = release;
            m_releaseNotes = release.value("body").toString();

            QString tagName = release.value("tag_name").toString();
            if (tagName.isEmpty()) {
                if (m_userInitiated) emit toastMessage(tr("未找到版本信息"));
                setState(Idle);
                emit checkCompleted(false, QString());
                return;
            }

            QVersionNumber server = parseTag(tagName);
            qCInfo(logApp) << "[UpdateManager] 服务器版本:" << tagName
                           << "需要更新:" << (QVersionNumber::compare(server, current) > 0);

            if (QVersionNumber::compare(server, current) <= 0) {
                if (m_userInitiated) emit toastMessage(tr("已是最新版本"));
                setState(Idle);
                emit checkCompleted(false, QString());
                return;
            }

            // New version — fetch compat.json to decide full vs. exe
            QJsonArray assets = release.value("assets").toArray();
            QString compatUrl;
            for (const QJsonValue& a : assets) {
                if (a["name"].toString().contains("compat.json")) {
                    compatUrl = a["browser_download_url"].toString();
                    break;
                }
            }

            auto startFullDl = [this, tagName]() {
                m_downloadVersion = tagName;
                if (m_userInitiated)
                    emit toastMessage(tr("发现新版本 %1，正在后台下载...").arg(tagName));
                pickAssetAndDownload(true);
                emit checkCompleted(true, tagName);
            };

            if (!compatUrl.isEmpty()) {
                qCInfo(logApp) << "[UpdateManager] 获取 compat.json:" << compatUrl;
                HttpClient::instance().get(compatUrl,
                    [this, tagName, startFullDl](int cStatus, const QByteArray& cBody) {
                        if (cStatus == 200) {
                            onCompatJsonReady(cBody);
                            emit checkCompleted(true, tagName);
                        } else {
                            qCWarning(logApp) << "[UpdateManager] compat.json HTTP" << cStatus;
                            startFullDl();
                        }
                    },
                    [startFullDl](const QString&) { startFullDl(); }
                );
            } else {
                qCInfo(logApp) << "[UpdateManager] 无 compat.json，全量下载";
                startFullDl();
            }
        },
        [this](const QString& error) {
            qCWarning(logApp) << "[UpdateManager] 网络错误:" << error;
            if (m_userInitiated)
                emit toastMessage(tr("网络连接失败，请稍后重试"));
            setState(Idle);
            emit checkCompleted(false, QString());
        }
    );
}

void UpdateManager::onCompatJsonReady(const QByteArray& body)
{
    QJsonObject compat = QJsonDocument::fromJson(body).object();

    qCInfo(logApp) << "[UpdateManager] compat.json 解析成功"
                   << "qt=" << compat.value("qt_version").toString()
                   << "epoch=" << compat.value("resource_epoch").toInt()
                   << "mode=" << compat.value("update_mode").toString();

    // Emergency override
    if (compat.value("update_mode").toString() == QStringLiteral("force_full")) {
        QString reason = compat.value("force_reason").toString();
        qCInfo(logApp) << "[UpdateManager] force_full:" << reason;
        pickAssetAndDownload(true);
        return;
    }

    bool needFull = false;

    QString serverQt = compat.value("qt_version").toString();
    if (!serverQt.isEmpty() && serverQt != m_qtVersion) {
        qCInfo(logApp) << "[UpdateManager] Qt 版本不匹配:" << m_qtVersion << "->" << serverQt;
        needFull = true;
    }

    int serverEpoch = compat.value("resource_epoch").toInt(0);
    if (serverEpoch > m_resourceEpoch) {
        qCInfo(logApp) << "[UpdateManager] 资源 epoch 变更:" << m_resourceEpoch << "->" << serverEpoch;
        needFull = true;
    }

    qCInfo(logApp) << "[UpdateManager] compat 判定: needFull=" << needFull;

    // Extract expected SHA256 for post-download verification
    m_expectedSha256 = compat.value("exe_sha256").toString();
    if (needFull)
        m_expectedSha256 = compat.value("full_sha256").toString();

    if (!m_expectedSha256.isEmpty())
        qCInfo(logApp) << "[UpdateManager] SHA256 期望=" << m_expectedSha256;
    else
        qCWarning(logApp) << "[UpdateManager] compat.json 未提供 SHA256，跳过校验";

    pickAssetAndDownload(needFull);
}

void UpdateManager::pickAssetAndDownload(bool forceFull)
{
    QJsonArray assets = m_releaseCache.value("assets").toArray();
    if (assets.isEmpty()) {
        qCWarning(logApp) << "[UpdateManager] Release 无资产文件";
        setState(Idle);
        return;
    }

    QJsonObject asset;
    if (!forceFull) {
        for (const QJsonValue& a : assets) {
            if (a["name"].toString().contains("ShadowLauncher.exe")) {
                asset = a.toObject();
                break;
            }
        }
    }
    if (asset.isEmpty()) asset = assets.first().toObject();

    QString downloadUrl = asset.value("browser_download_url").toString();
    QString fileName    = asset.value("name").toString();
    qint64  fileSize    = static_cast<qint64>(asset.value("size").toDouble());
    if (fileSize <= 0) fileSize = (fileName.endsWith(".7z") ? 50 : 10) * 1024 * 1024;

    qCInfo(logApp) << "[UpdateManager] 选择:" << fileName
                   << (fileSize / 1024 / 1024) << "MB" << "forceFull=" << forceFull;

    // Check disk space
    QString newPath = stateDir() + fileName;
    QString parentDir = QFileInfo(newPath).absolutePath();
    QStorageInfo storage(parentDir);
    if (storage.isValid() && storage.bytesAvailable() >= 0) {
        qint64 availableMB = storage.bytesAvailable() / (1024 * 1024);
        qint64 neededMB = fileSize / (1024 * 1024) + 50; // +50MB safety margin
        if (availableMB < neededMB) {
            qCWarning(logApp) << "[UpdateManager] 磁盘空间不足 需要=" << neededMB
                              << "MB 可用=" << availableMB << "MB";
            if (m_userInitiated)
                emit toastMessage(tr("磁盘空间不足，需要 %1 MB，可用 %2 MB")
                                      .arg(neededMB).arg(availableMB));
            setState(Idle);
            return;
        }
    }
    bool canResume = false;
    qint64 existingSize = 0;

    if (m_state == Paused && m_downloadPath == newPath) {
        QFileInfo fi(newPath);
        if (fi.exists() && fi.size() > 0 && fi.size() <= fileSize) {
            canResume = true;
            existingSize = fi.size();
        } else {
            QFile::remove(newPath);
        }
    }

    m_downloadUrl      = downloadUrl;
    m_downloadPath     = newPath;
    m_downloadFileName = fileName;
    m_downloadTotal    = fileSize;

    setState(Available);

    if (canResume) {
        resumeDownload(existingSize);
    } else {
        QFile::remove(newPath);
        m_downloadReceived = 0;
        startDownload();
    }
}

void UpdateManager::startDownload()
{
    resumeDownload(0);
}

void UpdateManager::resumeDownload(qint64 resumeFrom)
{
    if (m_downloadUrl.isEmpty()) {
        qCWarning(logApp) << "[UpdateManager] 下载地址为空，无法开始下载";
        return;
    }

    setState(Downloading);
    m_downloadReceived = resumeFrom;

    qCInfo(logApp) << "[UpdateManager] 开始下载:" << m_downloadUrl
                   << "断点:" << resumeFrom;

    m_activeReply = HttpClient::instance().downloadWithReply(
        m_downloadUrl, m_downloadPath,
        [this](qint64 received, qint64 total) {
            m_downloadReceived = received;
            m_downloadTotal = total;
            emit downloadProgress(received, total);
            static qint64 lastSaved = 0;
            if (received - lastSaved > 512 * 1024 || received >= total) {
                lastSaved = received;
                saveState();
            }
        },
        [this](bool ok, const QString& error) {
            int httpStatus = 0;
            if (m_activeReply) {
                httpStatus = m_activeReply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute).toInt();
            }
            m_activeReply = nullptr;
            onDownloadFinished(ok, error, httpStatus);
        },
        resumeFrom
    );
}

void UpdateManager::onDownloadFinished(bool ok, const QString& error, int httpStatus)
{
    if (!ok) {
        if (httpStatus > 0)
            qCWarning(logApp) << "[UpdateManager] 下载失败 HTTP" << httpStatus << "error:" << error;
        else
            qCWarning(logApp) << "[UpdateManager] 下载失败:" << error;
        setState(Paused);
        saveState();
        if (m_userInitiated)
            emit toastMessage(tr("下载失败: %1").arg(error));
        emit downloadFailed(error);
        return;
    }

    // SHA256 integrity check
    if (!m_expectedSha256.isEmpty()) {
        QFile verifyFile(m_downloadPath);
        if (verifyFile.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(&verifyFile);
            QString actual = hash.result().toHex().toLower();
            verifyFile.close();
            if (actual != m_expectedSha256.toLower()) {
                qCWarning(logApp) << "[UpdateManager] SHA256 校验失败"
                                  << "期望=" << m_expectedSha256
                                  << "实际=" << actual;
                QFile::remove(m_downloadPath);
                setState(Idle);
                if (m_userInitiated)
                    emit toastMessage(tr("下载文件校验失败，文件可能已损坏"));
                emit downloadFailed(tr("SHA256 mismatch"));
                return;
            }
            qCInfo(logApp) << "[UpdateManager] SHA256 校验通过";
        }
    }

    qCInfo(logApp) << "[UpdateManager] 下载完成 => Ready";

    // Clean up stale files from previous versions
    QDir d(stateDir());
    QStringList keep = { "state.json", m_downloadFileName };
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (!keep.contains(fi.fileName())) {
            qCInfo(logApp) << "[UpdateManager] 清理旧文件:" << fi.fileName();
            QFile::remove(fi.absoluteFilePath());
        }
    }

    setState(Ready);
    saveState();

    emit downloadComplete();
    emit updateAvailableForInstall(m_downloadVersion);
    if (m_userInitiated)
        emit toastMessage(tr("更新已就绪，下次启动时自动安装"));
}

} // namespace ShadowLauncher
