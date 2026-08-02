// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173

#include "modpack_downloader.h"
#include "../engine_identity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QRegularExpression>
#include <QDateTime>
#include <QTimer>
#include <QPointer>

#include "../http_client.h"
#include "mod_download_engine.h"
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
                    << QStringLiteral("[女娲] 镜像请求失败，降级官方: %1 status=%2 %3")
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
    qCInfo(logMod) << engineBanner("nuwa");
    emit logLine(engineBanner("nuwa"));
    m_running = true;
    m_cancelled = false;
    m_includeOptional = includeOptional;
    m_total = m_files->size();
    m_failed = 0;
    m_skippedCount = 0;
    m_items.clear();
    m_inflight.clear();
    m_preExisting.clear();
    m_lastQueueEmitMs = 0;
    m_lastFileProgMs = 0;
    m_lastEngineError.clear();

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
                                       + pid + QStringLiteral("/versions/") + vid + QLatin1Char('/') + fname);
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
            ++m_skippedCount;
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
                ++m_failed;
                if (i < m_files->size()) {
                    m_files->operator[](i).status = QStringLiteral("fail");
                    m_files->operator[](i).error = m_items[i].error;
                }
            }
        }
    }

    if (!cfIndexes.isEmpty()) {
        // 确有此包含 CurseForge 文件，才需要 CF API Key；未配置时提示一次（便于排查 401）
        if (m_apiKey.isEmpty()) {
            qCInfo(logMod) << "[女娲] 检测到 CurseForge 文件但未配置 CF API Key，"
                           << "镜像源可能可用；若官方源返回 401/403 请配置"
                           << "(环境变量 SHADOW_CF_API_KEY 或 config/cf_api_key.json)";
        }
        emit statusChanged(tr("正在解析 CurseForge 下载地址…"));
        emit logLine(tr("解析 %1 个 CurseForge 文件下载地址").arg(cfIndexes.size()));
        resolveBatch(0);
    } else {
        emit statusChanged(tr("开始下载模组…"));
        startEngineDownloads();
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
        startEngineDownloads();
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

    // ── 镜像元数据缺失检测（并非真的被删）──
    // 实测：mod.mcimirror.top 对部分较新/更新的 fileId 无数据（混合批次静默忽略、
    // 单独查询返回 404），而官方 API 存在（如 6914376 BDHill Reforge…）。
    // 旧逻辑直接判"文件已被删除"→ 每次导入稳定误报 N 个失败（主流启动器 却全部正常）。
    // 修复：镜像缺失 → 官方 API 补查（带 x-api-key）→ 仍缺失才算真删除。
    QList<int> missingIdx;
    int processed = 0;
    for (int i = startIndex; i < m_total && processed < kCfBatchSize; ++i) {
        ModpackRemoteFile& rf = m_files->operator[](i);
        if (rf.source != QLatin1String("curseforge")) continue;
        ++processed;
        if (byId.value(rf.fileId).isEmpty()) missingIdx.append(i);
    }
    if (!missingIdx.isEmpty() && !m_apiKey.isEmpty()) {
        QJsonArray idArr;
        for (int i : missingIdx) idArr.append(m_files->at(i).fileId);
        QJsonObject b;
        b[QStringLiteral("fileIds")] = idArr;
        const QByteArray body2 = QJsonDocument(b).toJson(QJsonDocument::Compact);
        emit logLine(tr("镜像缺少 %1 个 CF 文件元数据，转官方 API 补查…").arg(missingIdx.size()));
        // ⚠ 不走 apiWithFallback：其 stage0 绑定镜像短超时（15s），官方 API 响应慢
        // （CF CloudFront TLS 建连 ~10s）会被误杀成 Operation canceled → 补查永远失败。
        // 直接发官方请求，用官方 30s 超时；失败重试一次后仍失败才透传。
        const QString officialUrl = QLatin1String(kCfOfficialBase) + QStringLiteral("/mods/files");
        auto sendOfficial = std::make_shared<std::function<void(int)>>();
        *sendOfficial = [this, officialUrl, body2, startIndex, byId, missingIdx, sendOfficial](int attempt) {
            if (m_cancelled) return;
            QNetworkReply* reply = m_http->manager()->post(
                makeRequest(officialUrl, m_apiKey, kOfficialTimeoutMs), body2);
            m_inflight.append(reply);
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, startIndex, byId, missingIdx, sendOfficial, attempt]() {
                m_inflight.removeAll(reply);
                reply->deleteLater();
                if (m_cancelled) return;
                const int st = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
                const QByteArray rb = reply->readAll();
                if (st >= 200 && st < 300) {
                    QMap<int, QJsonObject> merged = byId;
                    QJsonParseError e2;
                    const QJsonDocument d2 = QJsonDocument::fromJson(rb, &e2);
                    if (e2.error == QJsonParseError::NoError) {
                        for (const QJsonValue& v : d2.object().value(QStringLiteral("data")).toArray()) {
                            const QJsonObject o = v.toObject();
                            merged.insert(o.value(QStringLiteral("id")).toInt(-1), o);
                        }
                    }
                    // 官方补查才有的 fileId：镜像未同步 → 镜像分片 URL 必 404，
                    // 下载直接走官方直链/官方 download-url 补解析，避免精卫先试镜像白等超时
                    QSet<int> officialOnly;
                    for (int i : missingIdx) {
                        const int fid = m_files->at(i).fileId;
                        if (!byId.contains(fid) && merged.contains(fid)) officialOnly.insert(fid);
                    }
                    processResolvedBatch(startIndex, merged, officialOnly);
                    return;
                }
                if (attempt == 0) {
                    qCWarning(logMod) << QStringLiteral("[女娲] 官方补查失败(HTTP %1) 重试一次").arg(st);
                    (*sendOfficial)(1);
                    return;
                }
                qCWarning(logMod) << QStringLiteral("[女娲] 官方补查失败(HTTP %1)，按删除处理").arg(st);
                processResolvedBatch(startIndex, byId);
            });
        };
        (*sendOfficial)(0);
        return;
    }
    processResolvedBatch(startIndex, byId);
}

void ModpackDownloader::processResolvedBatch(int startIndex, const QMap<int, QJsonObject>& byId,
                                              const QSet<int>& officialOnly)
{
    // ⚠ 只处理本批次请求过的 CF 条目（与 resolveBatch 收集逻辑一致：
    // 自 startIndex 起前 kCfBatchSize 个 CF 条目）。
    m_officialOnlyFileIds.unite(officialOnly);
    int processed = 0;
    for (int i = startIndex; i < m_total && processed < kCfBatchSize; ++i) {
        ModpackRemoteFile& rf = m_files->operator[](i);
        if (rf.source != QLatin1String("curseforge")) continue;
        ++processed;

        const QJsonObject o = byId.value(rf.fileId);
        if (o.isEmpty()) {
            // 镜像 + 官方补查都缺失 → 文件才真是被原作者删除（主流启动器 会弹窗提示缺失，后端改为记录 + 跳过）
            m_items[i].finished = true;
            m_items[i].error = QStringLiteral("文件已被删除（fileId=%1）").arg(rf.fileId);
            ++m_failed;
            rf.status = QStringLiteral("fail");
            rf.error = m_items[i].error;
            emit logLine(tr("⚠ CurseForge 文件缺失（镜像与官方均查不到）: %1").arg(rf.fileId));
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
            ++m_failed;
            rf.status = QStringLiteral("fail");
            rf.error = m_items[i].error;
            continue;
        }
        m_items[i].savePath = m_targetDir + QLatin1Char('/') + safeRel;
        m_items[i].fileName = rf.fileName;

        // ── 下载源链：镜像 CDN 分片优先，官方签名直链兜底 ──
        // 镜像: https://mod.mcimirror.top/files/{fid/1000}/{fid%1000}/{fileName}
        // ⚠ officialOnly（官方补查救回）：镜像未同步该文件 → 镜像分片 URL 必 404，
        // 不拼接（否则精卫先试镜像白等超时才切官方，严重拖慢整体下载）。
        const QString officialUrl = o.value(QStringLiteral("downloadUrl")).toString();
        const bool isOfficialOnly = officialOnly.contains(rf.fileId);
        if (!isOfficialOnly) {
            const QString encName = QString::fromUtf8(
                QUrl::toPercentEncoding(rf.fileName, "/", " "));
            m_items[i].urls.append(QStringLiteral("%1/%2/%3/%4")
                .arg(QLatin1String(kCfMirrorFileCdn))
                .arg(rf.fileId / 1000).arg(rf.fileId % 1000).arg(encName));
        }
        if (!officialUrl.isEmpty())
            m_items[i].urls.append(officialUrl);

        if (officialUrl.isEmpty()) {
            // 官方直链缺失：走 download-url 接口补解析（officialOnly 直接官方接口），稍后统一处理
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
        // 所有批次解析完成：若有缺失直链的条目，先补解析 download-url，再进入引擎下载
        if (!m_downloadUrlPending.isEmpty()) {
            emit statusChanged(tr("正在补全下载地址…"));
            resolveDownloadUrls();
        } else {
            emit statusChanged(tr("开始下载模组…"));
            startEngineDownloads();
        }
    }
}

// ── download-url 缺失条目补解析（镜像优先，并发 3 路）──

void ModpackDownloader::resolveDownloadUrls()
{
    if (m_cancelled || m_downloadUrlPending.isEmpty()) {
        emit statusChanged(tr("开始下载模组…"));
        startEngineDownloads();
        return;
    }

    // 并发 3 路：取前 3 个未开始的条目各发一个请求
    int started = 0;
    for (int i = 0; i < m_downloadUrlPending.size(); ++i) {
        const int idx = m_downloadUrlPending[i];
        if (m_items[idx].finished) continue;
        if (started >= kConcurrentStartup) break;
        m_items[idx].finished = true;   // 临时占用标记，回调里复位（防止重复发起）
        started++;
        startDownloadUrlResolve(idx);
    }
}

void ModpackDownloader::startDownloadUrlResolve(int idx)
{
    const ModpackRemoteFile& rf = m_files->at(idx);

    // ── officialOnly（官方补查救回）：镜像未同步该文件，镜像 download-url 接口必 404
    // （404 不在降级条件里 → 会白等一轮）→ 直接官方接口（30s 官方超时 + 重试一次）。
    if (m_officialOnlyFileIds.contains(rf.fileId)) {
        const QString official = QStringLiteral("%1/mods/%2/files/%3/download-url")
            .arg(QLatin1String(kCfOfficialBase)).arg(rf.projectId).arg(rf.fileId);
        auto send = std::make_shared<std::function<void(int)>>();
        *send = [this, official, idx, send](int attempt) {
            if (m_cancelled) return;
            QNetworkReply* reply = m_http->manager()->get(
                makeRequest(official, m_apiKey, kOfficialTimeoutMs));
            m_inflight.append(reply);
            connect(reply, &QNetworkReply::finished, this,
                    [this, reply, idx, send, attempt]() {
                m_inflight.removeAll(reply);
                reply->deleteLater();
                if (m_cancelled) return;
                const int st = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(0);
                const QByteArray body = reply->readAll();
                bool ok = false;
                if (st >= 200 && st < 300) {
                    QJsonParseError perr;
                    const QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
                    const QJsonObject data = (perr.error == QJsonParseError::NoError)
                        ? doc.object().value(QStringLiteral("data")).toObject() : QJsonObject();
                    const QString realUrl = data.value(QStringLiteral("downloadUrl")).toString();
                    if (!realUrl.isEmpty()) { m_items[idx].urls.append(realUrl); ok = true; }
                }
                if (!ok && attempt == 0) { (*send)(1); return; }
                finishDownloadUrlResolve(idx, ok, st);
            });
        };
        (*send)(0);
        return;
    }

    const QString mirror = QStringLiteral("%1/mods/%2/files/%3/download-url")
        .arg(QLatin1String(kCfMirrorBase)).arg(rf.projectId).arg(rf.fileId);
    const QString official = QStringLiteral("%1/mods/%2/files/%3/download-url")
        .arg(QLatin1String(kCfOfficialBase)).arg(rf.projectId).arg(rf.fileId);

    apiWithFallback(false, mirror, official, QByteArray(), m_apiKey,
                    [this, idx](int status, const QByteArray& body) {
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
        finishDownloadUrlResolve(idx, ok, status);
    });
}

void ModpackDownloader::finishDownloadUrlResolve(int idx, bool ok, int status)
{
    if (m_cancelled) return;
    m_items[idx].finished = false;   // 复位占用标记
    if (!ok) {
        // ── 官方直链拿不到时的处理 ──
        // 镜像 CDN 分片 URL 在 processResolvedBatch 已拼接进 urls（纯 fileId 拼接，
        // 不依赖 download-url 接口）。旧逻辑直接判失败 → 明明有镜像分片却废弃 →
        // 误报"无法获取下载地址"（官方批量接口本就不返回 downloadUrl）。
        // 修复：有分片 URL 就交给引擎尝试（分片缺失时引擎自动换源官方 edge，带 key），
        // 只有连分片 URL 都没有才真判失败。
        if (!m_items[idx].urls.isEmpty()) {
            m_items[idx].finished = false;
            emit logLine(tr("⚠ %1 官方直链缺失，改用镜像分片尝试").arg(m_items[idx].fileName));
        } else {
            m_items[idx].finished = true;
            m_items[idx].error = tr("无法获取下载地址（HTTP %1）").arg(status);
            ++m_failed;
            if (idx < m_files->size()) {
                m_files->operator[](idx).status = QStringLiteral("fail");
                m_files->operator[](idx).error = m_items[idx].error;
            }
            emit logLine(tr("⚠ %1 无法获取下载地址").arg(m_items[idx].fileName));
        }
    }

    m_downloadUrlPending.removeAll(idx);
    if (!m_downloadUrlPending.isEmpty()) {
        resolveDownloadUrls();  // 补位继续
    } else {
        emit statusChanged(tr("开始下载模组…"));
        startEngineDownloads();
    }
}

void ModpackDownloader::onResolveBatchFailed(int startIndex, const QString& err)
{
    Q_UNUSED(startIndex);
    // 地址解析阶段硬失败：所有未完成条目标记失败
    m_cancelled = true;
    for (int i = 0; i < m_total; ++i) {
        if (!m_items[i].finished) {
            m_items[i].finished = true;
            m_items[i].error = err;
            ++m_failed;
            if (i < m_files->size()) {
                m_files->operator[](i).status = QStringLiteral("fail");
                m_files->operator[](i).error = err;
            }
        }
    }
    emit logLine(tr("❌ %1").arg(err));
    m_running = false;
    emit allFinished(false);
}

// ════════════════════════════════════════════════════════════════
// 引擎适配层 — ShadowDownloader::ModDownloadEngine（模组小文件专属）
// ════════════════════════════════════════════════════════════════

void ModpackDownloader::startEngineDownloads()
{
    if (m_cancelled) return;

    // 每次任务新建引擎实例（引擎无清空队列 API，不复用；
    // 实例级状态与 MC 下载引擎完全隔离，互不干扰）
    m_fd = new ShadowDownloader::ModDownloadEngine(this);
    m_fd->setMaxThreads(12);       // 模组专项：全局 12 并发（主流启动器 默认 9 同量级）——
    // 实测 24 路并发对 MCIM 镜像过于激进：高峰期镜像限流饿死部分连接 →
    // 30s 无数据超时 → 分片失败 → 大文件报废；12 路温和稳定且峰值仍可达 3MB/s+
    m_fd->setMirrorRateLimitMs(100);   // MCIM/BMCLAPI 镜像限频（PCL 同款：每启一线程 Sleep(100)）
    m_fd->setCfApiKey(m_apiKey);       // 官方 edge CDN 下载认证（7/16 起强制）

    for (int i = 0; i < m_total; ++i) {
        DlItem& it = m_items[i];
        if (it.finished) continue;   // 解析失败 / 可选跳过已标记
        if (it.urls.isEmpty()) {
            // 无可用地址（双保险，正常情况下解析阶段已处理）
            it.finished = true;
            it.error = tr("无可用下载地址");
            ++m_failed;
            if (i < m_files->size()) {
                m_files->operator[](i).status = QStringLiteral("fail");
                m_files->operator[](i).error = it.error;
            }
            continue;
        }
        // 覆盖处理：备份旧文件并删除 → 引擎永远写全新文件（引擎无覆盖钩子，
        // 且 isNoSplit 直接写最终路径，会截断覆盖旧文件，必须先清场）
        if (QFileInfo::exists(it.savePath)) {
            if (m_overwriteHook) m_overwriteHook(it.savePath);
            QFile::remove(it.savePath);
            m_preExisting.insert(it.savePath);
        }
        QDir().mkpath(QFileInfo(it.savePath).absolutePath());
        const ModpackRemoteFile& rf = m_files->at(i);
        m_fd->addFile(it.savePath, it.fileName, it.urls, rf.size, rf.sha1);
    }

    if (m_fd->totalFiles() == 0) {
        // 空队列（全部解析失败/跳过）：直接收尾，不启动引擎
        m_fd->deleteLater();
        m_fd = nullptr;
        m_running = false;
        emit queueProgress(m_skippedCount, m_total, m_failed);
        emit allFinished(m_cancelled);
        return;
    }

    // ── 引擎信号 → 任务层信号桥接 ──
    connect(m_fd, &ShadowDownloader::ModDownloadEngine::progressChanged,
            this, &ModpackDownloader::onEngineProgress);
    connect(m_fd, &ShadowDownloader::ModDownloadEngine::fileProgress,
            this, &ModpackDownloader::onEngineFileProgress);
    connect(m_fd, &ShadowDownloader::ModDownloadEngine::fileFinished,
            this, &ModpackDownloader::onEngineFileFinished);
    connect(m_fd, &ShadowDownloader::ModDownloadEngine::allFinished,
            this, &ModpackDownloader::onEngineAllFinished);
    connect(m_fd, &ShadowDownloader::ModDownloadEngine::logMessage,
            this, [this](const QString& msg) {
        emit logLine(msg);
        // 捕获引擎失败/校验详情（用于 fileFinished(false) 的错误文案透传，
        // 消灭界面「详见日志」模糊提示）
        if (msg.contains(QStringLiteral("失败"))
            || msg.contains(QStringLiteral("校验"))
            || msg.contains(QStringLiteral("SHA1"))) {
            m_lastEngineError = msg;
        }
    });

    m_fd->start();
}

double ModpackDownloader::currentSpeedMBps() const
{
    return m_fd ? m_fd->currentSpeedMBps() : 0.0;
}

int ModpackDownloader::findIndexBySavePath(const QString& path) const{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].savePath == path && !m_items[i].finished) return i;
    }
    return -1;
}

void ModpackDownloader::onEngineProgress(int completed, int total, qint64 bytes, qint64 allBytes)
{
    Q_UNUSED(bytes); Q_UNUSED(allBytes);
    // 引擎 50ms 粒度上报，桥接限频 200ms（任务层卡片更新不需要更频）
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastQueueEmitMs < 200 && completed < total) return;
    m_lastQueueEmitMs = now;
    emit queueProgress(m_skippedCount + completed, m_total,
                       m_failed + (m_fd ? m_fd->failedFiles() : 0));
}

void ModpackDownloader::onEngineFileProgress(const QString& url, const QString& fileName,
                                             qint64 received, qint64 total)
{
    Q_UNUSED(url);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFileProgMs < 200) return;
    m_lastFileProgMs = now;

    int idx = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].fileName == fileName && !m_items[i].finished) { idx = i; break; }
    }
    if (idx < 0) return;
    emit fileProgress(idx, fileName, received, total);
}

void ModpackDownloader::onEngineFileFinished(const QString& localPath, bool success)
{
    if (m_cancelled) return;   // 取消后引擎 abort 回调忽略（统一走 allFinished(true)）
    const int idx = findIndexBySavePath(localPath);
    if (idx < 0 || idx >= m_files->size()) return;
    DlItem& it = m_items[idx];
    if (it.finished) return;
    it.finished = true;
    it.ok = success;

    ModpackRemoteFile& rf = m_files->operator[](idx);
    if (success) {
        rf.status = QStringLiteral("done");
        rf.error.clear();
        // 新建文件登记回滚（覆盖场景已在 addFile 前备份，不重复登记）
        if (!m_preExisting.contains(localPath) && m_createdHook)
            m_createdHook(localPath);
        emit fileFinished(idx, true, {});
    } else {
        rf.status = QStringLiteral("fail");
        // 失败详情透传：优先用引擎最近一条失败/校验日志（如「SHA1校验失败: xx」
        // 「请求失败 URL=... 错误=超时」），无缓存才用笼统文案
        it.error = m_lastEngineError.isEmpty()
            ? tr("下载失败（详见日志）") : m_lastEngineError;
        m_lastEngineError.clear();
        rf.error = it.error;
        QFile::remove(localPath);   // 清理引擎残留的半截文件（isNoSplit 直接写最终路径）
        emit fileFinished(idx, false, it.error);
        emit logLine(tr("⚠ %1 下载失败: %2").arg(it.fileName, it.error));
    }
    emit queueProgress(m_skippedCount + (m_fd ? m_fd->completedFiles() : 0), m_total,
                       m_failed + (m_fd ? m_fd->failedFiles() : 0));
}

void ModpackDownloader::onEngineAllFinished()
{
    // wasRunning 区分：正常完成路径 emit 收尾；取消后引擎若补发 allFinished
    // （所有文件恰好都已启动并 abort 完）则不重复 emit，只做清理。
    const bool wasRunning = m_running;
    m_running = false;
    if (wasRunning) {
        emit queueProgress(m_skippedCount + (m_fd ? m_fd->completedFiles() : 0), m_total,
                           m_failed + (m_fd ? m_fd->failedFiles() : 0));
        emit allFinished(m_cancelled);
    }
    if (m_fd) {
        // 析构会触发 cancel() 并 emit「下载已取消」噪音 → 先断开全部信号再释放
        m_fd->disconnect();
        m_fd->deleteLater();
        m_fd = nullptr;
    }
}

// ── 取消 ──

void ModpackDownloader::cancel()
{
    if (!m_running) return;
    m_cancelled = true;

    // 中止 CF API 在途请求
    for (const QPointer<QNetworkReply>& rp : m_inflight) {
        if (rp) m_http->abortDownload(rp);
    }
    m_inflight.clear();

    // 中止引擎（abort 所有在途分片请求）
    if (m_fd) m_fd->cancel();

    // 未完成的条目标记为跳过 + 清理可能残留的半截文件
    // （引擎 isNoSplit 直接写最终路径，abort 中断会留半截；onEngineFileFinished
    //   被 m_cancelled 守卫拦截不走正常清理路径，必须在此补删）
    for (int i = 0; i < m_total; ++i) {
        if (!m_items[i].finished) {
            if (!m_items[i].savePath.isEmpty())
                QFile::remove(m_items[i].savePath);
            m_items[i].finished = true;
            m_items[i].error = QStringLiteral("已取消");
            ++m_skippedCount;
            if (i < m_files->size()) {
                m_files->operator[](i).status = QStringLiteral("skipped");
                m_files->operator[](i).error = QStringLiteral("已取消");
            }
        }
    }
    m_running = false;
    emit statusChanged(tr("已取消下载"));
    emit allFinished(true);
    // ⚠ 引擎 worker 异步 abort 中：不能立即 disconnect/deleteLater——
    //   立即析构会触发 ~ModDownloadEngine（abort 在途请求）——阻塞主线程（UI 冻结），
    //   且 cancel 后未启动文件永不计数、引擎 allFinished 不会来。
    //   延后 2s 清理（abort 必然已完成、worker 已退出 → 析构不阻塞）。
    if (m_fd) {
        QPointer<ModpackDownloader> self(this);
        QTimer::singleShot(kCleanupDelayMs, self, [self]() {
            if (!self) return;
            if (self->m_fd) {
                self->m_fd->disconnect();
                self->m_fd->deleteLater();
                self->m_fd = nullptr;
            }
        });
    }
}
