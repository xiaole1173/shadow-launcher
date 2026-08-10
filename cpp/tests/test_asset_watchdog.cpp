// 验证山海经 in-flight 挂起看门狗（2026-08-10）：
// 挂起源 30s 无数据 → watchdog abort → 换正常源 → 下载成功（不再永久卡 99%）
#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QElapsedTimer>
#include <cstdio>
#include "core/asset_downloader.h"
#include "utils/logger.h"
#include "utils/hash_utils.h"

using namespace ShadowLauncher;

// 挂起源：接受连接 → 发 200 头 + 一半数据 → 挂起 60s（不关闭、不发送）
class StallingServer : public QTcpServer {
public:
    QByteArray payload;
    int connections = 0;
protected:
    void incomingConnection(qintptr socketDescriptor) override {
        connections++;
        QTcpSocket* s = new QTcpSocket(this);
        s->setSocketDescriptor(socketDescriptor);
        QByteArray head = "HTTP/1.1 200 OK\r\nContent-Length: "
                        + QByteArray::number(payload.size())
                        + "\r\nConnection: close\r\n\r\n";
        s->write(head + payload.left(payload.size() / 2));
        s->flush();
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    int fail = 0;

    // 数据（~500KB）
    QByteArray data = "hello asset watchdog! 0123456789";
    for (int i = 0; i < 15; ++i) data += data;
    const QString sha1 = sha1Hex(data);

    // 正常源：一次性发完整文件
    QTcpServer normal;
    QByteArray payload = data;
    normal.listen(QHostAddress::LocalHost, 0);
    const int nport = normal.serverPort();
    QObject::connect(&normal, &QTcpServer::newConnection, [&]() {
        QTcpSocket* s = normal.nextPendingConnection();
        QByteArray head = "HTTP/1.1 200 OK\r\nContent-Length: "
                        + QByteArray::number(payload.size())
                        + "\r\nConnection: close\r\n\r\n";
        s->write(head + payload);
        s->flush();
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    });

    // 挂起源
    StallingServer stall;
    stall.payload = data;
    stall.listen(QHostAddress::LocalHost, 0);
    const int sport = stall.serverPort();

    const QString savePath = QStringLiteral("t_watchdog/asset.bin");
    QDir().mkpath(QStringLiteral("t_watchdog"));
    QFile::remove(savePath);

    AssetDownloader dl;
    QVector<AssetDownloader::AssetTask> tasks;
    AssetDownloader::AssetTask t;
    t.savePath = savePath;
    t.sha1 = sha1;
    t.size = data.size();
    // 挂起源(127.0.0.1) 与 正常源(localhost) 用不同 host：避免同 host 失败被 hostCanAccept 误伤
    t.mirrors = { QStringLiteral("http://127.0.0.1:%1/stall.bin").arg(sport),
                  QStringLiteral("http://localhost:%1/ok.bin").arg(nport) };
    tasks.append(t);

    QElapsedTimer timer;
    timer.start();
    bool done = false;
    bool ok = false;
    int failed = -1;
    QObject::QObject::connect(&dl, &AssetDownloader::allFinished,
                     [&](bool success, int failedCount, const QStringList&) {
        done = true;
        ok = success;
        failed = failedCount;
        fprintf(stderr, "[watchdog] allFinished success=%d failed=%d elapsed=%lldms\n",
                success ? 1 : 0, failedCount, (long long)timer.elapsed());
    });

    dl.startDownload(tasks, 4);

    // 等完成（看门狗 30s + 换源下载，最多 60s）——用 exec 标准事件循环驱动
    QEventLoop loop;
    QObject::connect(&dl, &AssetDownloader::allFinished, &loop, &QEventLoop::quit);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(60000);
    loop.exec();
    timeout.stop();

    // 校验文件
    QFile f(savePath);
    bool fileOk = false;
    if (f.open(QIODevice::ReadOnly)) {
        fileOk = (sha1Hex(f.readAll()) == sha1);
        f.close();
    }
    fprintf(stderr, "[watchdog] result: done=%d ok=%d failed=%d fileOk=%d stallConnections=%d\n",
            done ? 1 : 0, ok ? 1 : 0, failed, fileOk ? 1 : 0, stall.connections);
    if (!(done && ok && failed == 0 && fileOk && stall.connections >= 1)) fail++;

    QDir(QStringLiteral("t_watchdog")).removeRecursively();
    fprintf(stderr, "=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
