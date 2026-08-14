// 验证山海经完成兜底 + 同 SHA1 预检查 key 修复（2026-08-14）：
// 场景：同 SHA1 两个任务（不同 savePath）→ m_preCheckQueued 不泄漏 → 正常完成不卡死
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <cstdio>
#include "core/asset_downloader.h"
#include "utils/logger.h"
#include "utils/hash_utils.h"

using namespace ShadowLauncher;

static int fail = 0;
static void check(bool cond, const char* name)
{
    fprintf(stderr, "  [%s] %s\n", cond ? "OK" : "FAIL", name);
    if (!cond) fail++;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());

    QByteArray contentA = "hello world asset A 0123456789";
    for (int i = 0; i < 12; ++i) contentA += contentA;  // ~300KB
    const QString sha1A = sha1Hex(contentA);

    // 简单服务器：每次连接返回 contentA
    QTcpServer server;
    QByteArray payload = contentA;
    server.listen(QHostAddress::LocalHost, 0);
    const int port = server.serverPort();
    QObject::connect(&server, &QTcpServer::newConnection, [&]() {
        QTcpSocket* s = server.nextPendingConnection();
        QByteArray head = "HTTP/1.1 200 OK\r\nContent-Length: "
                        + QByteArray::number(payload.size())
                        + "\r\nConnection: close\r\n\r\n";
        s->write(head + payload);
        s->flush();
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    });

    fprintf(stderr, "=== 山海经完成兜底 + 同SHA1预检查 key 测试 ===\n");

    // 两个任务：同 SHA1，不同 savePath
    const QString dirA = QStringLiteral("t_asset_a");
    const QString dirB = QStringLiteral("t_asset_b");
    QDir().mkpath(dirA);
    QDir().mkpath(dirB);

    AssetDownloader dl;
    QVector<AssetDownloader::AssetTask> tasks;
    AssetDownloader::AssetTask t1;
    t1.savePath = dirA + QStringLiteral("/asset.dat");
    t1.sha1 = sha1A;
    t1.size = contentA.size();
    t1.mirrors = { QStringLiteral("http://localhost:%1/a.dat").arg(port) };
    tasks.append(t1);

    AssetDownloader::AssetTask t2;
    t2.savePath = dirB + QStringLiteral("/asset.dat");
    t2.sha1 = sha1A;  // 同 SHA1
    t2.size = contentA.size();
    t2.mirrors = { QStringLiteral("http://localhost:%1/b.dat").arg(port) };
    tasks.append(t2);

    bool done = false;
    bool ok = false;
    QElapsedTimer timer;
    QObject::connect(&dl, &AssetDownloader::allFinished,
                     [&](bool success, int, const QStringList&) {
        done = true;
        ok = success;
    });

    timer.start();
    dl.startDownload(tasks, 4);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&dl, &AssetDownloader::allFinished, &loop, &QEventLoop::quit);
    timeout.start(10000);  // 10s 硬超时
    loop.exec();

    check(done, "allFinished 在 10s 内触发（修复前同SHA1计数泄漏会卡死超时）");
    check(timer.elapsed() < 10000, "10s 内完成");
    check(ok, "两个同 SHA1 任务均成功");

    // 清理
    QDir(dirA).removeRecursively();
    QDir(dirB).removeRecursively();
    server.close();

    fprintf(stderr, fail == 0 ? "\nALL PASS\n" : "\n%d FAIL\n", fail);
    return fail;
}
