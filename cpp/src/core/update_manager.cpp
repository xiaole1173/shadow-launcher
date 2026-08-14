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

// ── 2026-08-15：自定义更新服务器（latest.json，字段兼容 Gitee release 格式）──
void UpdateManager::setUpdateApiUrl(const QString& url)
{
    m_apiUrl = url;
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
    obj["last_silent_check"] = m_lastSilentCheck;

    QFile f(stateDir() + "state.json");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        qCWarning(logApp) << "[UpdateManager] 保存状态文件失败 error=" << f.errorString();
    }
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

    // 从 state.json 读取上次检查时间戳
    {
        QFile f(stateDir() + "state.json");
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                m_lastSilentCheck = static_cast<qint64>(
                    doc.object().value("last_silent_check").toDouble());
            }
        }
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSilentCheck < kSilentCheckIntervalMs) {
        qint64 remainingH = (kSilentCheckIntervalMs - (now - m_lastSilentCheck)) / 3600000;
        qint64 remainingM = ((kSilentCheckIntervalMs - (now - m_lastSilentCheck)) % 3600000) / 60000;
        qCInfo(logApp) << "[UpdateManager] 跳过静默检查(距上次不足"
                       << (kSilentCheckIntervalMs / 3600000) << "小时)，剩余"
                       << remainingH << "h" << remainingM << "m";
        return;
    }

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

    // 标记检查时间
    if (!m_userInitiated)
        m_lastSilentCheck = QDateTime::currentMSecsSinceEpoch();

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

            // ── 2026-08-15：同版本更新已就绪/在途/暂停 → 不重复下载 ──
            // 用户实测：每点一次"检查更新"都重新下载已下载好的包（Ready 状态被
            // 覆盖重下）。此处拦截：版本一致且已有下载状态 → 提示并直接结束检查。
            if (!m_downloadVersion.isEmpty() && m_downloadVersion == tagName
                && (m_state == Ready || m_state == Paused || m_state == Downloading)) {
                qCInfo(logApp) << "[UpdateManager] 同版本更新已就绪/在途，跳过下载"
                               << "tag=" << tagName << "state=" << static_cast<int>(m_state);
                if (m_userInitiated) {
                    if (m_state == Ready)
                        emit toastMessage(tr("更新已就绪，重启后自动安装"));
                    else if (m_state == Downloading)
                        emit toastMessage(tr("更新正在下载中，请稍候"));
                    else
                        emit toastMessage(tr("更新下载已暂停，重启后自动续传"));
                }
                emit checkCompleted(true, tagName);
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
        // ── 2026-08-15：force_full 也设置 full_sha256（原实现直接 return，
        //    不设置 m_expectedSha256 → 全量包不校验，或残留上一次的 exe_sha256
        //    导致校验必然失败删包）──
        m_expectedSha256 = compat.value("full_sha256").toString();
        if (!m_expectedSha256.isEmpty())
            qCInfo(logApp) << "[UpdateManager] SHA256 期望=" << m_expectedSha256;
        else
            qCWarning(logApp) << "[UpdateManager] compat.json 未提供 full_sha256，跳过校验";
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
    } else {
        // 全量更新：优先选 .zip/.7z 完整包（2026-08-11 起发布 .zip；不依赖 assets 顺序）
        for (const QJsonValue& a : assets) {
            const QString name = a["name"].toString();
            if (name.endsWith(QStringLiteral(".zip")) || name.endsWith(QStringLiteral(".7z"))) {
                asset = a.toObject();
                break;
            }
        }
    }
    // ── 2026-08-15：找不到匹配资产不再回退 assets.first()（原回退可能把 .zip
    //    当增量 exe 下载 → SHA256 用 exe_sha256 校验必然失败 → 更新中断）──
    if (asset.isEmpty()) {
        qCWarning(logApp) << "[UpdateManager] 未找到匹配的更新资产 forceFull=" << forceFull;
        if (m_userInitiated)
            emit toastMessage(forceFull ? tr("未找到全量更新包，请稍后重试")
                                        : tr("未找到增量更新文件，请稍后重试"));
        setState(Idle);
        return;
    }

    QString downloadUrl = asset.value("browser_download_url").toString();
    QString fileName    = asset.value("name").toString();
    qint64  fileSize    = static_cast<qint64>(asset.value("size").toDouble());
    if (fileSize <= 0) fileSize = ((fileName.endsWith(".7z") || fileName.endsWith(".zip")) ? 60 : 10) * 1024 * 1024;

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
            // ── 2026-08-15：m_lastStateSaved 为成员（原 static 局部变量跨下载
            //    残留：第二次下载 received < 旧 lastSaved 时断点永不保存）──
            if (received - m_lastStateSaved > 512 * 1024
                || (total > 0 && received >= total)) {
                m_lastStateSaved = received;
                saveState();
            }
        },
        [this](bool ok, const QString& error) {
            // 驿道 v2：句柄不再暴露 QNetworkReply 属性；状态码已包含在 error 文本（HTTP xxx）
            m_activeReply = nullptr;
            onDownloadFinished(ok, error, 0);
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
        // ── 2026-08-15：文件打不开 = 损坏/被占用，不能跳过校验（原实现直接
        //    放行 → 损坏文件进入 Ready 被安装）──
        if (!verifyFile.open(QIODevice::ReadOnly)) {
            qCWarning(logApp) << "[UpdateManager] SHA256 校验文件无法打开:"
                              << m_downloadPath << verifyFile.errorString();
            QFile::remove(m_downloadPath);
            setState(Idle);
            if (m_userInitiated)
                emit toastMessage(tr("下载文件无法读取，已删除，请重新更新"));
            emit downloadFailed(tr("verify file unreadable"));
            return;
        }
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
