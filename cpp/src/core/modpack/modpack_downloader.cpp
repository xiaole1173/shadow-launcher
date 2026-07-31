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
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QtConcurrent>

#include "../http_client.h"
#include "../../utils/logger.h"

using namespace ShadowLauncher;

namespace {

// ════════════════════════════════════════════════════════════════
// 官方 / MCIM 镜像端点（文档：https://mod.mcimirror.top/docs）
//   镜像优先，官方兜底。CF 鉴权头原样携带（镜像兼容 x-api-key）。
// ════════════════════════════════════════════════════════════════
constexpr const char* kCfOfficialBase  = "https://api.curseforge.com/v1";
constexpr const char* kCfMirrorBase    = "https://mod.mcimirror.top/curseforge/v1";
constexpr const char* kCfMirrorFileCdn = "https://mod.mcimirror.top/files";   // /files/{fid1}/{fid2}/{name}
constexpr const char* kMrOfficialBase  = "https://api.modrinth.com/v2";
constexpr const char* kMrMirrorBase    = "https://mod.mcimirror.top/modrinth/v2";
constexpr const char* kMrOfficialCdn   = "https://cdn.modrinth.com/data";
constexpr const char* kMrMirrorData    = "https://mod.mcimirror.top/data";   // /data/{pid}/versions/{vid}/{name}

constexpr int kCfBatchSize = 50;          // CF API 单次 fileIds 上限
constexpr int kMirrorTimeoutMs = 15000;   // 镜像优先，超时快速降级
constexpr int kOfficialTimeoutMs = 30000; // 官方兜底给足时间（本机到 CF CloudFront TLS 建连 ~10s）

// 请求超时必须显式设在请求属性上（HttpClient 的 getRaw/post 会强制覆写为
// manager 默认 10s；请求属性优先于 manager 默认，故直接走 manager()）。
// 另：官方 CloudFront / MCIM 镜像对 QNAM 的 HTTP/2 连接处理不稳定
// （实测 RemoteHostClosedError / 挂起超时），统一禁用 H2 走 HTTP/1.1。
QNetworkRequest makeRequest(const QString& url, const QString& apiKey, int timeoutMs)
{
    QNetworkRequest req{ QUrl(url) };
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Content-Type", "application/json");
    if (!apiKey.isEmpty())
        req.setRawHeader("x-api-key", apiKey.toUtf8());
    req.setTransferTimeout(timeoutMs);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    return req;
}

// 降级判定：连接失败/超时(status=0)、429 限流、5xx 服务错误 → 换官方重试；
// 其余 4xx（400/401/403/404）与镜像同源语义，降级无意义，直接透传。
bool shouldFallbackToOfficial(int status)
{
    return status == 0 || status == 429 || status >= 500;
}

// 从官方 Modrinth CDN URL 提取 (projectId, versionId, fileName)，用于构造镜像数据路由。
// 返回 false 表示非标准 URL（无法镜像，原样下载）。
bool parseMrCdnUrl(const QString& url, QString* projectId, QString* versionId, QString* fileName)
{
    static const QRegularExpression re(
        QStringLiteral("https://cdn\\.modrinth\\.com/data/([^/]+)/versions/([^/]+)/(.+)$"));
    const QRegularExpressionMatch m = re.match(url);
    if (!m.hasMatch()) return false;
    if (projectId) *projectId = m.captured(1);
    if (versionId) *versionId = m.captured(2);
    if (fileName) *fileName = m.captured(3);
    return true;
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

// ── 镜像优先 + 官方兜底请求（异步链式）──

void ModpackDownloader::apiWithFallback(bool isPost,
                                        const QString& mirrorUrl, const QString& officialUrl,
                                        const QByteArray& body, const QString& apiKey,
                                        const std::function<void(int, const QByteArray&)>& cb)
{
    if (m_cancelled) return;

    // 递归链式请求：send 放入堆上（shared_ptr），避免异步回调时栈帧已销毁
    auto send = std::make_shared<std::function<void(int)>>();
    *send = [this, isPost, mirrorUrl, officialUrl, body, apiKey, cb, send](int stage) {
        const QString url = (stage == 0) ? mirrorUrl : officialUrl;
        const int timeout = (stage == 0) ? kMirrorTimeoutMs : kOfficialTimeoutMs;

        QNetworkReply* reply = isPost
            ? m_http->manager()->post(makeRequest(url, apiKey, timeout), body)
            : m_http->manager()->get(makeRequest(url, apiKey, timeout));
        m_inflight.append(reply);

        connect(reply, &QNetworkReply::finished, this,
                [this, reply, stage, cb, send, mirrorUrl, officialUrl, isPost, body, apiKey]() {
            m_inflight.removeAll(reply);
            reply->deleteLater();
            if (m_cancelled) return;

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
            const QByteArray resp = reply->readAll();
            const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
            if (ok) {
                cb(status, resp);
                return;
            }
            if (stage == 0 && shouldFallbackToOfficial(status)) {
                // 镜像超时/429/5xx/连接失败：静默丢弃，自动换官方重试一次
                qCWarning(logMod).noquote()
                    << QStringLiteral("[modpack] 镜像请求失败，降级官方: %1 status=%2 %3")
                           .arg(mirrorUrl).arg(status).arg(reply->errorString());
                (*send)(1);
                return;
            }
            cb(status, resp);  // 官方也失败 / 非降级类错误 → 透传
        });
    };
    (*send)(0);
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
            m_items[i].savePath = m_targetDir + QLatin1Char('/') + rf.relPath;
            // Modrinth 下载源链：镜像数据路由优先，官方 CDN 兜底，其余 downloads 依次追加
            QString pid, vid, fname;
            if (!rf.downloadUrl.isEmpty()
                && parseMrCdnUrl(rf.downloadUrl, &pid, &vid, &fname)) {
                m_items[i].urls.append(QLatin1String(kMrMirrorData) + QLatin1Char('/')
                                       + pid + QLatin1String("/versions/") + vid + QLatin1Char('/') + fname);
                m_items[i].urls.append(rf.downloadUrl);
            } else if (!rf.downloadUrl.isEmpty()) {
                m_items[i].urls.append(rf.downloadUrl);
            }
            for (const QString& u : rf.fallbackUrls)
                m_items[i].urls.append(u);
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
            if (m_items[i].urls.isEmpty()) {
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

    // 镜像优先（/curseforge/v1/mods/files），超时/429/5xx 自动降级官方
    apiWithFallback(true,
                    QLatin1String(kCfMirrorBase) + QStringLiteral("/mods/files"),
                    QLatin1String(kCfOfficialBase) + QStringLiteral("/mods/files"),
                    body, m_apiKey,
                    [this, startIndex](int status, const QByteArray& respBody) {
        if (m_cancelled) return;
        if (status >= 200 && status < 300) {
            onResolveBatchDone(startIndex, status, respBody);
            return;
        }
        if (status == 401) {
            onResolveBatchFailed(startIndex,
                tr("CurseForge API 密钥无效（401），请检查密钥配置"));
            return;
        }
        if (status == 403) {
            onResolveBatchFailed(startIndex,
                tr("CurseForge API 拒绝访问（403）：密钥权限不足 / IP 风控 / 配额耗尽"));
            return;
        }
        onResolveBatchFailed(startIndex, tr("CurseForge API 请求失败（HTTP %1）").arg(status));
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

        // ── 下载源链：镜像 CDN 分片优先，官方签名直链兜底 ──
        // 镜像: https://mod.mcimirror.top/files/{fid/1000}/{fid%1000}/{fileName}
        const QString officialUrl = o.value(QStringLiteral("downloadUrl")).toString();
        const QString encName = QString::fromUtf8(
            QUrl::toPercentEncoding(rf.fileName, "/", " "));
        m_items[i].urls.append(QStringLiteral("%1/%2/%3/%4")
            .arg(QLatin1String(kCfMirrorFileCdn))
            .arg(rf.fileId / 1000).arg(rf.fileId % 1000).arg(encName));
        if (!officialUrl.isEmpty())
            m_items[i].urls.append(officialUrl);

        if (officialUrl.isEmpty()) {
            // 官方直链缺失：走 download-url 接口补解析（镜像优先），稍后统一处理
            m_downloadUrlPending.append(i);
        }
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
        // 所有批次解析完成：若有缺失直链的条目，先补解析 download-url，再进入下载
        if (!m_downloadUrlPending.isEmpty()) {
            emit statusChanged(tr("正在补全下载地址…"));
            resolveDownloadUrls();
        } else {
            m_resolveDone = true;
            emit statusChanged(tr("开始下载模组…"));
            scheduleNext();
        }
    }
}

// ── download-url 缺失条目补解析（镜像优先，并发 3 路）──

void ModpackDownloader::resolveDownloadUrls()
{
    if (m_cancelled || m_downloadUrlPending.isEmpty()) {
        m_resolveDone = true;
        emit statusChanged(tr("开始下载模组…"));
        scheduleNext();
        return;
    }

    // 并发 3 路：取前 3 个未开始的条目各发一个请求
    int started = 0;
    for (int i = 0; i < m_downloadUrlPending.size(); ++i) {
        const int idx = m_downloadUrlPending[i];
        if (m_items[idx].inFlight) continue;
        if (started >= 3) break;
        m_items[idx].inFlight = true;
        started++;
        startDownloadUrlResolve(idx);
    }
}

void ModpackDownloader::startDownloadUrlResolve(int idx)
{
    const ModpackRemoteFile& rf = m_files->at(idx);
    const QString mirror = QStringLiteral("%1/mods/%2/files/%3/download-url")
        .arg(QLatin1String(kCfMirrorBase)).arg(rf.projectId).arg(rf.fileId);
    const QString official = QStringLiteral("%1/mods/%2/files/%3/download-url")
        .arg(QLatin1String(kCfOfficialBase)).arg(rf.projectId).arg(rf.fileId);

    apiWithFallback(false, mirror, official, QByteArray(), m_apiKey,
                    [this, idx](int status, const QByteArray& body) {
        m_items[idx].inFlight = false;
        if (m_cancelled) return;

        bool ok = false;
        if (status >= 200 && status < 300) {
            QJsonParseError perr;
            const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
            const QJsonObject data = (perr.error == QJsonParseError::NoError)
                ? doc.object().value(QStringLiteral("data")).toObject() : QJsonObject();
            const QString realUrl = data.value(QStringLiteral("downloadUrl")).toString();
            if (!realUrl.isEmpty()) {
                m_items[idx].urls.append(realUrl);  // 镜像 CDN 分片已在前，官方直链追加兜底
                ok = true;
            }
        }
        if (!ok) {
            m_items[idx].finished = true;
            m_items[idx].error = tr("无法获取下载地址（HTTP %1）").arg(status);
            m_failed++;
            m_completed++;
            if (idx < m_files->size()) {
                m_files->operator[](idx).status = QStringLiteral("fail");
                m_files->operator[](idx).error = m_items[idx].error;
            }
            emit logLine(tr("⚠ %1 无法获取下载地址").arg(m_items[idx].fileName));
        }

        m_downloadUrlPending.removeAll(idx);
        if (!m_downloadUrlPending.isEmpty()) {
            resolveDownloadUrls();  // 补位继续
        } else {
            m_resolveDone = true;
            emit statusChanged(tr("开始下载模组…"));
            scheduleNext();
        }
    });
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
            if (m_items[i].urls.isEmpty()) continue;
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

    // 临时文件与最终文件同目录，成功后改名（原子落盘）
    it.tmpPath = it.savePath + QStringLiteral(".part");
    QFileInfo fi(it.savePath);
    QDir().mkpath(fi.absolutePath());
    QFile::remove(it.tmpPath);

    const QString url = (it.urlIdx < it.urls.size()) ? it.urls[it.urlIdx] : QString();
    if (url.isEmpty()) {
        it.inFlight = false;
        finalizeItem(idx, false, tr("无可用下载地址"));
        return;
    }

    const QString sourceTag = it.urlIdx == 0 ? QString() : tr("（备用源 %1/%2）").arg(it.urlIdx + 1).arg(it.urls.size());
    emit statusChanged(tr("正在下载 %1%2").arg(it.fileName.isEmpty() ? url : it.fileName, sourceTag));
    emit logLine(tr("下载 %1%2\n  源: %3")
        .arg(it.fileName.isEmpty() ? url : it.fileName, sourceTag, url));

    // 自实现下载（不依赖 downloadWithReply）：可禁用 HTTP/2（官方 CDN / 镜像
    // 对 QNAM H2 连接不稳定，实测 Connection closed / 挂起超时），并给足超时。
    QNetworkRequest req{ QUrl(url) };
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setTransferTimeout(60000);
    QNetworkReply* reply = m_http->manager()->get(req);
    m_inflight.append(reply);

    auto* file = new QFile(it.tmpPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reply->abort();
        reply->deleteLater();
        m_inflight.removeAll(reply);
        delete file;
        it.inFlight = false;
        finalizeItem(idx, false, tr("无法创建临时文件: %1").arg(it.tmpPath));
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        const QByteArray chunk = reply->readAll();
        if (file->write(chunk) != chunk.size())
            reply->abort();  // 磁盘空间不足等：中断，走失败路径
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, idx](qint64 received, qint64 total) {
        emit fileProgress(idx, m_items[idx].fileName, received, total);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, idx]() {
        m_inflight.removeAll(reply);
        file->close();
        delete file;
        reply->deleteLater();

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

        bool ok = reply->error() == QNetworkReply::NoError;
        QString err = reply->errorString();
        if (!ok) {
            const int st = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
            if (st >= 400)
                err = tr("HTTP %1 %2").arg(st).arg(err);
        }
        onItemDone(idx, ok, err);
    });

    m_activeSlots++;
}

void ModpackDownloader::onItemDone(int idx, bool ok, const QString& err)
{
    if (ok) {
        verifyItem(idx);
        return;
    }
    // 下载失败：切换到备用源（镜像→官方→…），全部用尽才判失败
    if (m_items[idx].urlIdx + 1 < m_items[idx].urls.size()) {
        m_items[idx].urlIdx++;
        qCWarning(logMod) << "[modpack] 下载失败，切换源重试:" << m_items[idx].fileName << err;
        emit logLine(tr("下载失败，切换备用源: %1 (%2)").arg(m_items[idx].fileName, err));
        QFile::remove(m_items[idx].tmpPath);
        scheduleNext();  // 重排会重新拾取该条目（inFlight=false, finished=false）
        return;
    }
    finalizeItem(idx, false, err.isEmpty() ? tr("下载失败（所有源均不可用）") : err);
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
    if (!ok && m_items[idx].urlIdx + 1 < m_items[idx].urls.size()) {
        // 校验失败（镜像源内容损坏等）：删掉临时文件，换备用源重下一遍（主流启动器 对损坏文件同样重试）
        m_items[idx].urlIdx++;
        qCWarning(logMod) << "[modpack] 校验失败，切换源重试:" << m_items[idx].fileName << err;
        emit logLine(tr("文件校验失败，切换备用源: %1 (%2)").arg(m_items[idx].fileName, err));
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
