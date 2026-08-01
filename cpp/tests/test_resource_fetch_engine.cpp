// SPDX-License-Identifier: AGPL-3.0-or-later
// 资源拉取引擎（司南）自测：
//   1. getJson 搜索 → 第二次同 URL 应缓存命中（毫秒级）
//   2. prefetchIcons → iconReady 信号 → 原图 + 88px 缩略图落盘
//   3. iconLocalPath 命中返回本地路径
#include <QCoreApplication>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <cstdio>

#include "core/resource_fetch_engine.h"

using namespace ShadowLauncher;

static int g_failures = 0;
static void check(bool ok, const char* name)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++g_failures;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    const QString root = QDir::tempPath() + "/sl_fetch_test_" + QString::number(QCoreApplication::applicationPid());
    ResourceFetchEngine engine(root);

    const QString searchUrl =
        QStringLiteral("https://mod.mcimirror.top/modrinth/v2/search?query=jei&limit=3&offset=0&index=relevance");
    const QString iconUrl =
        QStringLiteral("https://mod.mcimirror.top/data/AANobbMI/icon.png"); // JEI icon via mirror

    // ── Step 1: search with cache ──
    engine.getJson(searchUrl, true,
        [&](int status, const QByteArray& body) {
            check(status == 200 && !body.isEmpty(), "搜索请求 200 且有数据");
            std::printf("[TEST] search: %d bytes\n", (int)body.size());

            // ── Step 2: second call must hit cache (fast) ──
            QElapsedTimer t;
            t.start();
            engine.getJson(searchUrl, true,
                [&](int status2, const QByteArray& body2) {
                    const qint64 ms = t.elapsed();
                    check(status2 == 200 && body2 == body, "缓存命中数据一致");
                    check(ms < 100, "缓存命中 <100ms");
                    std::printf("[TEST] cached hit: %lld ms, %d bytes\n", (long long)ms, (int)body2.size());

                    // ── Step 3: icon prefetch → iconReady ──
                    bool readySeen = false;
                    QObject::connect(&engine, &ResourceFetchEngine::iconReady,
                        [&](const QString& url, const QString& localPath) {
                            readySeen = true;
                            check(url == iconUrl, "iconReady 回传正确 URL");
                            check(QFileInfo::exists(QUrl(localPath).toLocalFile()), "iconReady 本地文件存在");
                            const QString local = QUrl(localPath).toLocalFile();
                            std::printf("[TEST] icon ready: %s (%lld bytes)\n",
                                        local.toUtf8().constData(),
                                        (long long)QFileInfo(local).size());

                            // 缩略图文件也应存在
                            const QString thumb = root + "/thumbs/" +
                                QString::fromLatin1(QCryptographicHash::hash(iconUrl.toUtf8(),
                                    QCryptographicHash::Sha1).toHex()).left(16) + "_88.png";
                            check(QFileInfo::exists(thumb), "88px 缩略图已生成");

                            // ── Step 4: iconLocalPath 命中 ──
                            const QString p = engine.iconLocalPath(iconUrl);
                            check(!p.isEmpty() && QFileInfo::exists(QUrl(p).toLocalFile()),
                                  "iconLocalPath 命中本地缓存");

                            std::printf(g_failures == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", g_failures);
                            app.exit(g_failures == 0 ? 0 : 1);
                        });
                    engine.prefetchIcons({iconUrl});
                    // 5s 兜底
                    QTimer::singleShot(5000, [&]() {
                        if (!readySeen) {
                            check(false, "图标下载超时未收到 iconReady");
                            std::printf("\nTIMEOUT (镜像可能慢，非逻辑错误)\n");
                            app.exit(2);
                        }
                    });
                },
                [&](const QString& e) {
                    check(false, "缓存命中请求失败");
                    std::printf("err: %s\n", e.toUtf8().constData());
                    app.exit(1);
                });
        },
        [&](const QString& err) {
            check(false, "搜索请求失败");
            std::printf("search err: %s\n", err.toUtf8().constData());
            app.exit(1);
        });

    QTimer::singleShot(20000, &app, &QCoreApplication::quit); // 20s 总兜底
    return app.exec();
}
