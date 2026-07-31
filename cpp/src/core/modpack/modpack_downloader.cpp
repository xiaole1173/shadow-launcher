// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "modpack_downloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QtConcurrent>

#include "../http_client.h"
#include "../../utils/logger.h"

using namespace ShadowLauncher;

namespace {

constexpr const char* kCfFilesApi = "https://api.curseforge.com/v1/mods/files";
constexpr const char* kCfDownloadUrlApi = "https://api.curseforge.com/v1/mods/%1/files/%2/download-url";
constexpr int kCfBatchSize = 50;   // CF API 单次 fileIds 上限
constexpr int kCfApiTimeoutMs = 30000;  // 本机到 CF CloudFront 的 TLS 建连可能 >10s，
                                        // 故绕过 HttpClient 的 10s 默认超时（请求属性覆盖 manager 默认）

QNetworkRequest makeCfRequest(const QString& url, const QString& apiKey)
{
    QNetworkRequest req{ QUrl(url) };
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Content-Type", "application/json");
    if (!apiKey.isEmpty())
        req.setRawHeader("x-api-key", apiKey.toUtf8());
    req.setTransferTimeout(kCfApiTimeoutMs);
    return req;
}

// 直接走 HttpClient 的共享 QNAM（公开 accessor），请求属性超时优先于 manager 默认值
QNetworkReply* cfGet(HttpClient* http, const QString& url, const QString& apiKey)
{
    return http->manager()->get(makeCfRequest(url, apiKey));
}

QNetworkReply* cfPost(HttpClient* http, const QString& url, const QByteArray& body, const QString& apiKey)
{
    return http->manager()->post(makeCfRequest(url, apiKey), body);
}

} // namespace

// ── 工具：工作线程流式 SHA1 ──

QByteArray ModpackDownloader::fileSha1(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    QByteArray buf;
    buf.resize(1024 * 1024);
    qint64 r = 0;
    while ((r = f.read(buf.data(), buf.size())) > 0) {
        hash.addData(buf.constData(), r);
    }
    f.close();
    return hash.result().toHex();
}

// ── 工具：CF 资源类型判定（主流启动器：modules 含 META-INF/mcmod.info 或 .jar → mods；
//    pack.mcmeta → resourcepacks；否则 shaderpacks；modules 为空 → mods）──

QString ModpackDownloader::classifyCategory(const QString& fileName, const QJsonArray& modules)
{
    QStringList names;
    for (const QJsonValue& v : modules) {
        if (v.isString()) names.append(v.toString());
    }
    const QString lowerFile = fileName.toLower();
    if (names.contains(QStringLiteral("META-INF"))
        || names.contains(QStringLiteral("mcmod.info"))
        || lowerFile.endsWith(QLatin1String(".jar")))
        return QStringLiteral("mods");
    if (names.contains(QStringLiteral("pack.mcmeta")))
        return QStringLiteral("resourcepacks");
    if (!modules.isEmpty())
        return QStringLiteral("shaderpacks");
    return QStringLiteral("mods");
}

// ── 启动 ──

ModpackDownloader::ModpackDownloader(QObject* parent)
    : QObject(parent)
    , m_http(&HttpClient::instance())
{
}

void ModpackDownloader::start(bool includeOptional)
{
    if (m_running || !m_files) return;
    m_running = true;
    m_cancelled = false;
    m_includeOptional = includeOptional;
    m_total = m_files->size();
    m_completed = 0;
    m_failed = 0;
    m_skipped = 0;
    m_resolveDone = false;
    m_items.clear();
    m_inflight.clear();

    m_items.resize(m_total);
    for (int i = 0; i < m_total; ++i) {
        m_items[i].index = i;
        const ModpackRemoteFile& rf = m_files->at(i);
        m_items[i].fileName = rf.fileName;
        if (rf.source == QLatin1String("modrinth")) {
            m_items[i].url = rf.downloadUrl;
            m_items[i].savePath = m_targetDir + QLatin1Char('/') + rf.relPath;
        }
    }

    // 第一阶段：CF 文件批量解析下载地址（Modrinth 直接可用）
    QList<int> cfIndexes;
    for (int i = 0; i < m_total; ++i) {
        const ModpackRemoteFile& rf = m_files->at(i);
        if (!rf.required && !m_includeOptional) {
            m_items[i].finished = true;
            m_items[i].error = QStringLiteral("可选文件已跳过");
            m_skipped++;
            m_completed++;
            if (i < m_files->size()) {
                m_files->operator[](i).status = QStringLiteral("skipped");
                m_files->operator[](i).error = m_items[i].error;
            }
            continue;
        }
        if (rf.source == QLatin1String("curseforge")) {
            cfIndexes.append(i);
        } else {
            if (m_items[i].url.isEmpty()) {
                // Modrinth 条目没有可用下载地址（主流启动器 同样会下载失败）
                m_items[i].finished = true;
                m_items[i].error = QStringLiteral("无可用下载地址");
                m_failed++;
                m_completed++;
                if (i < m_files->size()) {
                    m_files->operator[](i).status = QStringLiteral("fail");
                    m_files->operator[](i).error = m_items[i].error;
                }
            }
        }
    }

    if (!cfIndexes.isEmpty()) {
        emit statusChanged(tr("正在解析 CurseForge 下载地址…"));
        emit logLine(tr("解析 %1 个 CurseForge 文件下载地址").arg(cfIndexes.size()));
        resolveBatch(0);
    } else {
        m_resolveDone = true;
        emit statusChanged(tr("开始下载模组…"));
        scheduleNext();
    }
}

// ── CF 地址解析（50 个/批）──

void ModpackDownloader::resolveBatch(int startIndex)
{
    if (m_cancelled) return;

    QList<int> cfIndexes;
    for (int i = startIndex; i < m_total && cfIndexes.size() < kCfBatchSize; ++i) {
        if (m_files->at(i).source == QLatin1String("curseforge"))
            cfIndexes.append(i);
    }
    if (cfIndexes.isEmpty()) {
        m_resolveDone = true;
        scheduleNext();
        return;
    }

    QJsonArray idArr;
    for (int i : cfIndexes)
        idArr.append(m_files->at(i).fileId);

    QJsonObject bodyObj;
    bodyObj[QStringLiteral("fileIds")] = idArr;
    const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = cfPost(m_http, QLatin1String(kCfFilesApi), body, m_apiKey);
    m_inflight.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, startIndex, cfIndexes]() {
        m_inflight.removeAll(reply);
        reply->deleteLater();

        if (m_cancelled) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
        const QByteArray respBody = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && status == 200) {
            onResolveBatchDone(startIndex, status, respBody);
        } else {
            // 429 / 5xx / 网络错误：重试一次
            qCWarning(logMod) << "[modpack][CF] 批量解析失败 status=" << status
                              << reply->errorString() << "cfIndexes=" << cfIndexes.size();
            if (status == 401 || status == 403) {
                onResolveBatchFailed(startIndex,
                    status == 401
                        ? tr("CurseForge API 密钥无效（401），请检查密钥配置")
                        : tr("CurseForge API 拒绝访问（403）：密钥权限不足 / IP 风控 / 配额耗尽"));
                return;
            }
            onResolveBatchFailed(startIndex, tr("CurseForge API 请求失败（HTTP %1）").arg(status));
        }
    });
}

void ModpackDownloader::onResolveBatchDone(int startIndex, int status, const QByteArray& body)
{
    Q_UNUSED(status);
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    const QJsonArray data = (perr.error == QJsonParseError::NoError)
        ? doc.object().value(QStringLiteral("data")).toArray() : QJsonArray();

    // fileId → 响应条目
    QMap<int, QJsonObject> byId;
    for (const QJsonValue& v : data) {
        const QJsonObject o = v.toObject();
        byId.insert(o.value(QStringLiteral("id")).toInt(-1), o);
    }

    for (int i = startIndex; i < m_total; ++i) {
        ModpackRemoteFile& rf = m_files->operator[](i);
        if (rf.source != QLatin1String("curseforge")) continue;

        const QJsonObject o = byId.value(rf.fileId);
        if (o.isEmpty()) {
            // 文件已被原作者删除（主流启动器 会弹窗提示缺失，后端改为记录 + 跳过）
            m_items[i].finished = true;
            m_items[i].error = QStringLiteral("文件已被删除（fileId=%1）").arg(rf.fileId);
            m_failed++;
            m_completed++;
            rf.status = QStringLiteral("fail");
            rf.error = m_items[i].error;
            emit logLine(tr("⚠ CurseForge 文件缺失（可能已被作者删除）: %1").arg(rf.fileId));
            continue;
        }

        rf.displayName = o.value(QStringLiteral("displayName")).toString(rf.displayName);
        rf.fileName = o.value(QStringLiteral("fileName")).toString();
        rf.size = static_cast<qint64>(o.value(QStringLiteral("fileLength")).toDouble(0));
        const QString category = classifyCategory(rf.fileName, o.value(QStringLiteral("modules")).toArray());
        rf.category = category;

        // 落盘路径：{分类目录}/{文件名}（主流启动器 同策略）
        const QString rel = category + QLatin1Char('/') + rf.fileName;
        const QString safeRel = sanitizeRelPath(rel);
        if (safeRel.isEmpty()) {
            m_items[i].finished = true;
            m_items[i].error = QStringLiteral("文件名为非法路径: %1").arg(rf.fileName);
            m_failed++;
            m_completed++;
            rf.status = QStringLiteral("fail");
            rf.error = m_items[i].error;
            continue;
        }
        m_items[i].savePath = m_targetDir + QLatin1Char('/') + safeRel;
        m_items[i].fileName = rf.fileName;

        QString url = o.value(QStringLiteral("downloadUrl")).toString();
        if (url.isEmpty()) {
            // 官方博客提供的备用下载地址接口
            url = QString::fromLatin1(kCfDownloadUrlApi).arg(rf.projectId).arg(rf.fileId);
        }
        m_items[i].url = url;
    }

    // 是否还有下一批
    int next = startIndex + kCfBatchSize;
    bool hasMore = false;
    for (int i = next; i < m_total; ++i) {
        if (m_files->at(i).source == QLatin1String("curseforge")) {
            hasMore = true;
            break;
        }
    }
    if (hasMore) {
        resolveBatch(next);
    } else {
        m_resolveDone = true;
        emit statusChanged(tr("开始下载模组…"));
        scheduleNext();
    }
}

void ModpackDownloader::onResolveBatchFailed(int startIndex, const QString& err)
{
    Q_UNUSED(startIndex);
    // 地址解析阶段硬失败：所有未完成条目标记失败
    m_cancelled = true;
    for (int i = 0; i < m_total; ++i) {
        if (!m_items[i].finished && !m_items[i].inFlight) {
            m_items[i].finished = true;
            m_items[i].error = err;
            m_failed++;
            m_completed++;
            if (i < m_files->size()) {
                m_files->operator[](i).status = QStringLiteral("fail");
                m_files->operator[](i).error = err;
            }
        }
    }
    emit logLine(tr("❌ %1").arg(err));
    emit allFinished(false);
    m_running = false;
}

// ── 下载调度 ──

void ModpackDownloader::scheduleNext()
{
    if (m_cancelled) return;
    while (m_activeSlots < kMaxConcurrent) {
        // 找下一个未开始、有地址、未完成的条目
        int next = -1;
        for (int i = 0; i < m_total; ++i) {
            if (m_items[i].finished || m_items[i].inFlight) continue;
            if (m_items[i].url.isEmpty()) continue;
            next = i;
            break;
        }
        if (next < 0) break;
        startItem(next);
    }
    finishIfAllDone();
}

void ModpackDownloader::startItem(int idx)
{
    DlItem& it = m_items[idx];
    it.inFlight = true;
    it.attempt++;

    // 临时文件与最终文件同目录，成功后改名（原子落盘）
    it.tmpPath = it.savePath + QStringLiteral(".part");
    QFileInfo fi(it.savePath);
    QDir().mkpath(fi.absolutePath());
    QFile::remove(it.tmpPath);

    emit statusChanged(tr("正在下载 %1").arg(it.fileName.isEmpty() ? it.url : it.fileName));

    QNetworkReply* reply = m_http->downloadWithReply(
        it.url, it.tmpPath,
        [this, idx](qint64 received, qint64 total) {
            emit fileProgress(idx, m_items[idx].fileName, received, total);
        },
        [this, idx](bool ok, const QString& err) {
            m_items[idx].inFlight = false;
            m_activeSlots--;
            if (m_cancelled) {
                // 取消路径：不逐文件报错，统一收尾
                if (!m_items[idx].finished) {
                    m_items[idx].finished = true;
                    m_skipped++;
                    m_completed++;
                }
                finishIfAllDone();
                return;
            }
            onItemDone(idx, ok, err);
        });
    m_activeSlots++;
    m_inflight.append(reply);
}

void ModpackDownloader::onItemDone(int idx, bool ok, const QString& err)
{
    if (ok) {
        verifyItem(idx);
        return;
    }
    // 网络失败：重试一次（规格：单次失败重试）
    if (m_items[idx].attempt < 1) {
        qCWarning(logMod) << "[modpack] 下载失败，重试一次:" << m_items[idx].fileName << err;
        emit logLine(tr("下载失败，重试: %1 (%2)").arg(m_items[idx].fileName, err));
        QFile::remove(m_items[idx].tmpPath);
        scheduleNext();  // 重排会重新拾取该条目（inFlight=false, finished=false）
        return;
    }
    finalizeItem(idx, false, err.isEmpty() ? tr("下载失败") : err);
}

// ── 哈希/大小校验（QtConcurrent 工作线程，避免阻塞主线程）──

void ModpackDownloader::verifyItem(int idx)
{
    struct VerifyResult { bool ok; QString error; };
    const DlItem& it = m_items[idx];

    auto* watcher = new QFutureWatcher<VerifyResult>(this);
    const QByteArray expectSha1 = (idx < m_files->size()) ? m_files->at(idx).sha1 : QByteArray();
    const qint64 expectSize = (idx < m_files->size()) ? m_files->at(idx).size : 0;
    const QString tmpPath = it.tmpPath;

    watcher->setFuture(QtConcurrent::run([tmpPath, expectSha1, expectSize]() -> VerifyResult {
        QFileInfo fi(tmpPath);
        if (!fi.exists()) return {false, QStringLiteral("临时文件不存在")};
        if (expectSize > 0 && fi.size() != expectSize)
            return {false, QStringLiteral("大小不匹配（期望 %1，实际 %2）").arg(expectSize).arg(fi.size())};
        if (!expectSha1.isEmpty()) {
            const QByteArray actual = fileSha1(tmpPath);
            if (actual.isEmpty())
                return {false, QStringLiteral("文件读取失败")};
            if (actual != expectSha1)
                return {false, QStringLiteral("SHA1 不匹配（期望 %1，实际 %2）")
                            .arg(QString::fromLatin1(expectSha1), QString::fromLatin1(actual))};
        }
        return {true, QString()};
    }));

    connect(watcher, &QFutureWatcher<VerifyResult>::finished, this,
            [this, watcher, idx]() {
        const VerifyResult r = watcher->result();
        watcher->deleteLater();
        onVerifyDone(idx, r.ok, r.error);
    });
}

void ModpackDownloader::onVerifyDone(int idx, bool ok, const QString& err)
{
    if (!ok && m_items[idx].attempt < 1) {
        // 校验失败：删掉临时文件重下一遍（主流启动器 对损坏文件同样重试）
        qCWarning(logMod) << "[modpack] 校验失败，重试一次:" << m_items[idx].fileName << err;
        emit logLine(tr("文件校验失败，重试: %1 (%2)").arg(m_items[idx].fileName, err));
        QFile::remove(m_items[idx].tmpPath);
        scheduleNext();
        return;
    }
    finalizeItem(idx, ok, err);
}

void ModpackDownloader::finalizeItem(int idx, bool ok, const QString& err)
{
    DlItem& it = m_items[idx];
    if (it.finished) return;
    it.finished = true;

    if (ok) {
        if (QFileInfo::exists(it.savePath)) {
            if (m_overwriteHook) m_overwriteHook(it.savePath);  // 先备份旧文件
            QFile::remove(it.savePath);
        }
        if (!QFile::rename(it.tmpPath, it.savePath)) {
            QFile::remove(it.tmpPath);
            ok = false;
            it.error = tr("临时文件改名失败");
        }
    } else {
        it.error = err;
        QFile::remove(it.tmpPath);
    }

    if (ok) {
        m_completed++;
        if (idx < m_files->size()) {
            m_files->operator[](idx).status = QStringLiteral("done");
            m_files->operator[](idx).error.clear();
        }
        emit fileFinished(idx, true, {});
    } else {
        m_failed++;
        if (idx < m_files->size()) {
            m_files->operator[](idx).status = QStringLiteral("fail");
            m_files->operator[](idx).error = it.error;
        }
        emit fileFinished(idx, false, it.error);
        emit logLine(tr("⚠ %1 下载失败: %2").arg(it.fileName, it.error));
    }

    emit queueProgress(m_completed, m_total, m_failed);
    scheduleNext();
}

void ModpackDownloader::finishIfAllDone()
{
    if (!m_running) return;
    if (m_completed + m_failed >= m_total && m_activeSlots == 0) {
        m_running = false;
        emit queueProgress(m_completed, m_total, m_failed);
        emit allFinished(m_cancelled);
    }
}

// ── 取消 ──

void ModpackDownloader::cancel()
{
    if (!m_running) return;
    m_cancelled = true;

    // 中止所有在途请求
    for (const QPointer<QNetworkReply>& rp : m_inflight) {
        if (rp) m_http->abortDownload(rp);
    }
    m_inflight.clear();

    // 未开始的条目标记为跳过
    for (int i = 0; i < m_total; ++i) {
        if (!m_items[i].finished && !m_items[i].inFlight) {
            m_items[i].finished = true;
            m_items[i].error = QStringLiteral("已取消");
            m_skipped++;
            m_completed++;
            if (i < m_files->size()) {
                m_files->operator[](i).status = QStringLiteral("skipped");
                m_files->operator[](i).error = QStringLiteral("已取消");
            }
        }
    }
    cleanupTmp();
    m_running = false;
    emit statusChanged(tr("已取消下载"));
    emit allFinished(true);
}

void ModpackDownloader::cleanupTmp()
{
    for (const DlItem& it : m_items) {
        if (!it.tmpPath.isEmpty())
            QFile::remove(it.tmpPath);
    }
}
