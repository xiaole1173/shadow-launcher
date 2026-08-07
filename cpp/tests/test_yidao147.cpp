// 验证驿道下载 universal.zip（1.4.7 merged fallback 目标）→ 文件完整 + ZIP 魔数
// 用法: Yidao147Test（下载 bmclapi 1.4.7 universal.zip 到 yidao147.zip 并校验）
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <cstdio>
#include "core/http_client.h"
#include "utils/logger.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    const QString url = QStringLiteral("https://bmclapi2.bangbang93.com/maven/net/minecraftforge/forge/1.4.7-6.6.2.534/forge-1.4.7-6.6.2.534-universal.zip");
    const QString path = QStringLiteral("yidao147.zip");
    QFile::remove(path);
    fprintf(stderr, "=== 驿道下载 1.4.7 universal.zip\n");
    ShadowLauncher::HttpClient::DownloadHandle* h = ShadowLauncher::HttpClient::instance().downloadWithReply(
        url, path,
        [](qint64 recv, qint64 total) {
            if (total > 0) fprintf(stderr, "progress %lld/%lld\n", (long long)recv, (long long)total);
        },
        [&app, path](bool ok, const QString& err) {
            fprintf(stderr, "=== done ok=%d err=%s\n", ok ? 1 : 0, err.toUtf8().constData());
            if (ok) {
                QFile f(path);
                bool zipOk = false;
                if (f.open(QIODevice::ReadOnly)) {
                    QByteArray head = f.read(4);
                    zipOk = head.size() == 4 && static_cast<quint8>(head[0]) == 'P' && static_cast<quint8>(head[1]) == 'K';
                    fprintf(stderr, "size=%lld PK魔数=%s\n", (long long)f.size(), zipOk ? "OK" : "BAD");
                    f.close();
                }
                QCoreApplication::exit(zipOk ? 0 : 1);
            } else {
                QCoreApplication::exit(2);
            }
        });
    if (!h) { fprintf(stderr, "handle null\n"); return 3; }
    QTimer::singleShot(120000, []() { fprintf(stderr, "=== TIMEOUT\n"); QCoreApplication::exit(4); });
    return app.exec();
}
