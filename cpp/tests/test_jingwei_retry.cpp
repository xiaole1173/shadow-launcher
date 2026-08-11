// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
//
// test_jingwei_retry.cpp — 精卫（ModDownloadEngine）补位重试回归测试（2026-08-12）：
//
// 背景 bug（内测/实测：导入整合包模组上百个时"一旦出现失败就成片失败，取消重导才可能成功"）：
//   ① 任务层重试结果被丢弃：ModpackDownloader::onEngineFileFinished 的 first-only 守卫 +
//      findIndexBySavePath 的 !finished 过滤 → 精卫补位重试成功后同一文件再发 fileFinished
//      被忽略 → 文件实际已下好仍计失败 → failed==total 误判整包导入失败（本测试第 2 节验证）。
//   ② 全局 5 轮重试预算被一波集中失败耗尽：晚失败的文件零重试（本测试第 1 节验证每文件预算）。
//
// 第 1 节（引擎级）：本地 HTTP 服务器按路径控制失败次数——
//   /ok/*     恒 200；/f2/* 前 2 次 503 后 200；/f4/* 前 4 次 503 后 200；/always/* 恒 503。
//   断言：ok 1 次请求成功；f2 各 3 次请求成功；f4 恰 5 次请求（预算上限内）成功；
//         always 恰 5 次请求后永久失败；引擎 completedFiles==4 failedFiles==1。
//   旧实现（全局轮次闸门）在此场景也能过，但每文件请求次数断言保证了预算语义。
//
// 第 2 节（任务层）：ModpackDownloader + 2 个 f2 文件 —— 两文件首轮全失败后被重试成功，
//   断言最终 rf.status=="done"、最终 queueProgress failed==0（旧实现此处必 FAIL：
//   重试成功被丢弃 → 状态停在 fail）。
//
// 构建：cmake -S . -B build-test -DSHADOW_BUILD_MODPACK_SELFTEST=ON
// 运行：build-test/Release/JingweiRetryTest.exe

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QHash>
#include <QElapsedTimer>
#include <cstdio>
#include <cstring>

#include "core/modpack/mod_download_engine.h"
#include "core/modpack/modpack_downloader.h"
#include "core/modpack/modpack_common.h"
#include "core/http_client.h"
#include "utils/logger.h"

using namespace ShadowLauncher;
using namespace ShadowDownloader;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg) \
    do { \
        ++g_checks; \
        if (cond) { fprintf(stderr, "  [PASS] %s\n", msg); } \
        else { ++g_failures; fprintf(stderr, "  [FAIL] %s\n", msg); } \
    } while (0)

// ── 可控失败次数的本地 HTTP 服务器 ──
// /ok/* → 200；/always/* → 恒 503；/fN/* → 前 N 次 503，之后 200（按 path 计数）
class RetryServer : public QTcpServer {
public:
    QByteArray payload;
    QHash<QString, int> requests;   // path → 请求次数

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        QTcpSocket* s = new QTcpSocket(this);
        s->setSocketDescriptor(socketDescriptor);
        QObject::connect(s, &QTcpSocket::readyRead, this, [this, s]() {
            static QHash<QTcpSocket*, QByteArray> bufs;
            bufs[s].append(s->readAll());
            QByteArray& buf = bufs[s];
            int nl = buf.indexOf('\n');
            if (nl < 0) return;
            const QByteArray reqLine = buf.left(nl).trimmed();
            bufs.remove(s);

            const QByteArray path = reqLine.split(' ').value(1);
            requests[path] = requests.value(path) + 1;
            const int n = requests[path];

            bool fail = false;
            if (path.startsWith("/always/")) fail = true;
            else if (path.startsWith("/f")) {
                // /f2/xxx → 前 2 次 503；/f4/xxx → 前 4 次 503
                const int lim = QString::fromLatin1(path).mid(2).section('/', 0, 0).toInt();
                if (n <= lim) fail = true;
            }
            // 发响应（QNAM 侧 identity 编码，纯字节直发）
            QByteArray resp;
            if (fail) {
                resp = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            } else {
                resp = "HTTP/1.1 200 OK\r\nContent-Length: "
                     + QByteArray::number(payload.size())
                     + "\r\nConnection: close\r\n\r\n" + payload;
            }
            s->write(resp);
            s->flush();
        });
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    }
};

static QString makePayload()
{
    QByteArray d = "jingwei-retry-test-payload-0123456789-";
    for (int i = 0; i < 12; ++i) d += d;
    return QString::fromLatin1(d);
}

// ════════════════════════════════════════════════════════════════
// 第 1 节：引擎级 — 每文件重试预算 + 完整复位 + 全部文件收敛
// ════════════════════════════════════════════════════════════════
static void testEngineRetry(QCoreApplication& app)
{
    fprintf(stderr, "=== 第 1 节 引擎级：每文件重试预算（/f2 /f4 /always /ok）===\n");

    RetryServer srv;
    const QByteArray payload = makePayload().toLatin1();
    srv.payload = payload;
    if (!srv.listen(QHostAddress::LocalHost, 0)) {
        CHECK(false, "server listen");
        return;
    }
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(srv.serverPort());

    const QString dir = QStringLiteral("t_jingwei/out");
    QDir().mkpath(dir);
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    ModDownloadEngine eng;
    eng.setMaxThreads(3);
    eng.setRetryGapMs(0);   // 测试加速：重试不加间隔

    auto add = [&](const QString& name, const QString& path) {
        eng.addFile(dir + QLatin1Char('/') + name, name,
                    { base + path }, payload.size());
    };
    add(QStringLiteral("ok1.jar"), QStringLiteral("/ok/ok1.jar"));
    add(QStringLiteral("f2a.jar"), QStringLiteral("/f2/f2a.jar"));
    add(QStringLiteral("f2b.jar"), QStringLiteral("/f2/f2b.jar"));
    add(QStringLiteral("f4.jar"),  QStringLiteral("/f4/f4.jar"));
    add(QStringLiteral("always.jar"), QStringLiteral("/always/always.jar"));

    int finishedOk = 0, finishedFail = 0;
    bool allDone = false;
    QObject::connect(&eng, &ModDownloadEngine::fileFinished, &app,
                     [&](const QString&, bool ok) { ok ? finishedOk++ : finishedFail++; });
    QObject::connect(&eng, &ModDownloadEngine::allFinished, &app,
                     [&]() { allDone = true; });

    eng.start();

    QEventLoop loop;
    QObject::connect(&eng, &ModDownloadEngine::allFinished, &loop, [&]() { loop.quit(); });
    QTimer::singleShot(30000, &loop, []() {
        fprintf(stderr, "  [FAIL] 引擎 30s 超时未结束\n");
        ++g_failures;
    });
    QElapsedTimer t; t.start();
    loop.exec();

    const int ms = static_cast<int>(t.elapsed());
    fprintf(stderr, "  耗时 %dms  completed=%d failed=%d\n",
            ms, eng.completedFiles(), eng.failedFiles());

    CHECK(allDone, "引擎 allFinished 触发");
    CHECK(eng.completedFiles() == 4, "completedFiles==4（ok1/f2a/f2b/f4）");
    CHECK(eng.failedFiles() == 1, "failedFiles==1（always）");
    // fileFinished 只在 finishItem 时发（sourceFailed 就地换源不发）：
    // 成功 4 次（ok1/f2a/f2b/f4）；失败 9 次（f2a×1 + f2b×1 + f4×2 + always×5）——
    // 每轮内多次源尝试合并为一次终态事件。
    CHECK(finishedOk == 4 && finishedFail == 9, "fileFinished 13 次：4 成功 9 失败（含重试轮）");

    // 每文件请求次数（验证预算语义：f4 恰 5 次=预算上限，always 5 次后放弃）
    CHECK(srv.requests.value("/ok/ok1.jar") == 1, "/ok/ok1.jar 请求 1 次");
    CHECK(srv.requests.value("/f2/f2a.jar") == 3, "/f2/f2a.jar 请求 3 次（2 败+1 成）");
    CHECK(srv.requests.value("/f2/f2b.jar") == 3, "/f2/f2b.jar 请求 3 次（2 败+1 成）");
    CHECK(srv.requests.value("/f4/f4.jar") == 5, "/f4/f4.jar 请求 5 次（预算上限内成功）");
    CHECK(srv.requests.value("/always/always.jar") == 10, "/always/always.jar 5 轮×2（含回绕）后放弃");

    // 落盘校验
    QFileInfo fiOk(dir + "/ok1.jar"), fiF2a(dir + "/f2a.jar"), fiF4(dir + "/f4.jar");
    CHECK(fiOk.exists() && fiOk.size() == payload.size(), "ok1.jar 落盘且大小正确");
    CHECK(fiF2a.exists() && fiF2a.size() == payload.size(), "f2a.jar 落盘且大小正确（重试成功）");
    CHECK(fiF4.exists() && fiF4.size() == payload.size(), "f4.jar 落盘且大小正确（第 5 次尝试成功）");
    CHECK(!QFileInfo::exists(dir + "/always.jar"), "always.jar 不存在（5 次全败）");
}

// ════════════════════════════════════════════════════════════════
// 第 2 节：任务层 — 重试成功对 ModpackDownloader 可见（旧实现必 FAIL）
// ════════════════════════════════════════════════════════════════
static void testTaskLayerRetryVisibility(QCoreApplication& app)
{
    fprintf(stderr, "=== 第 2 节 任务层：补位重试成功对 ModpackDownloader 可见 ===\n");

    RetryServer srv;
    const QByteArray payload = makePayload().toLatin1();
    srv.payload = payload;
    if (!srv.listen(QHostAddress::LocalHost, 0)) {
        CHECK(false, "server listen");
        return;
    }
    const QString base = QStringLiteral("http://127.0.0.1:%1").arg(srv.serverPort());

    const QString dir = QStringLiteral("t_jingwei/task");
    QDir().mkpath(dir);
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    QList<ModpackRemoteFile> files;
    for (int i = 0; i < 2; ++i) {
        ModpackRemoteFile rf;
        rf.source = QStringLiteral("modrinth");
        rf.fileName = QStringLiteral("mod%1.jar").arg(i);
        rf.relPath = QStringLiteral("mods/mod%1.jar").arg(i);
        rf.downloadUrl = base + QStringLiteral("/f2/mod%1.jar").arg(i);
        rf.size = payload.size();
        rf.required = true;
        files.append(rf);
    }

    ModpackDownloader dl;
    dl.setTargetDir(dir);
    dl.setFiles(&files);

    int fileFinOk = 0, fileFinFail = 0;
    int lastQueueFailed = -1;
    bool allDone = false, cancelled = false;
    QObject::connect(&dl, &ModpackDownloader::fileFinished, &app,
                     [&](int, bool ok, const QString&) { ok ? fileFinOk++ : fileFinFail++; });
    QObject::connect(&dl, &ModpackDownloader::queueProgress, &app,
                     [&](int, int, int failed) { lastQueueFailed = failed; });
    QObject::connect(&dl, &ModpackDownloader::allFinished, &app,
                     [&](bool c) { allDone = true; cancelled = c; });

    dl.start(false);

    QEventLoop loop;
    QObject::connect(&dl, &ModpackDownloader::allFinished, &loop, [&]() { loop.quit(); });
    QTimer::singleShot(40000, &loop, []() {
        fprintf(stderr, "  [FAIL] 任务层 40s 超时未结束（重试间隔 5s 预留）\n");
        ++g_failures;
    });
    QElapsedTimer t; t.start();
    loop.exec();   // allFinished → quit

    const int ms = static_cast<int>(t.elapsed());
    fprintf(stderr, "  耗时 %dms（含 5s 重试间隔） fileFinished ok=%d fail=%d\n",
            ms, fileFinOk, fileFinFail);

    CHECK(allDone && !cancelled, "allFinished(false) 正常完成");
    CHECK(files[0].status == QStringLiteral("done"), "mod0 最终状态 done（重试成功可见）");
    CHECK(files[1].status == QStringLiteral("done"), "mod1 最终状态 done（重试成功可见）");
    CHECK(fileFinOk == 2 && fileFinFail == 2, "fileFinished 4 次：首轮 2 失败 + 重试 2 成功");
    CHECK(lastQueueFailed == 0, "最终 queueProgress failed==0");
    CHECK(QFileInfo::exists(dir + "/mods/mod0.jar")
          && QFileInfo::exists(dir + "/mods/mod1.jar"), "两文件落盘");
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    fprintf(stderr, "=== 精卫补位重试回归（2026-08-12）===\n");

    testEngineRetry(app);
    testTaskLayerRetryVisibility(app);

    fprintf(stderr, "=== 结果：%d/%d 通过 ===\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
