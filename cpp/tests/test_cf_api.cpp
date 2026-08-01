// CF 适配器自测：搜索统一模型 + 文件→版本转换 + 分类表
#include <QCoreApplication>
#include <QTimer>
#include <QDir>
#include <cstdio>
#include "core/resource_fetch_engine.h"
#include "core/cf_api.h"

using namespace ShadowLauncher;

static int g_fail = 0;
static void check(bool ok, const char* name)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++g_fail;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ResourceFetchEngine engine(QDir::tempPath() + "/sl_cf_test_" + QString::number(QCoreApplication::applicationPid()));
    CfApi cf(&engine);

    // 1. 分类静态表
    QVariantList modCats = CfApi::categories(6);
    check(modCats.size() >= 20, "CF Mods 分类表 >= 20");
    check(CfApi::isCfCategory("cf:412"), "isCfCategory(cf:412)");
    check(CfApi::cfCategoryId("cf:412") == 412, "cfCategoryId 解析");
    check(!CfApi::isCfCategory("adventure"), "Modrinth 分类非 cf");
    check(CfApi::loaderTypeFor("fabric") == 4, "loader fabric→4");
    check(CfApi::loaderTypeFor("neoforge") == 6, "loader neoforge→6");

    // 2. 搜索（真实网络）
    cf.search(6, "jei", 0, "1.20.1", "forge", 0, 5,
        [&](const QVariantList& items, int total) {
            check(!items.isEmpty(), "CF 搜索返回结果");
            check(total > 0, "CF total > 0");
            if (!items.isEmpty()) {
                const QVariantMap m = items.first().toMap();
                check(!m.value("slug").toString().isEmpty(), "CF slug(modId)");
                check(!m.value("title").toString().isEmpty(), "CF title");
                check(m.value("source").toString() == "CurseForge", "CF source 标签");
                check(!m.value("icon").toString().isEmpty(), "CF icon URL");
                check(m.value("downloads").toDouble() > 0, "CF downloads");
                std::printf("[TEST] %s | %s | 下载=%lld | icon=%s\n",
                            m.value("slug").toString().toUtf8().constData(),
                            m.value("title").toString().toUtf8().constData(),
                            (long long)m.value("downloads").toDouble(),
                            m.value("icon").toString().left(60).toUtf8().constData());
                // 3. 文件 → 版本（用第一个结果的 modId）
                const QString modId = m.value("slug").toString();
                cf.fetchFilesAsVersions(modId, "1.20.1", "forge",
                    [&](const QStringList& versions, const QVariantMap& details) {
                        check(!versions.isEmpty(), "CF 版本列表非空");
                        if (!versions.isEmpty()) {
                            const QString key = versions.first();
                            const QVariantMap d = details.value(key).toMap();
                            check(d.value("game_version").toString() == "1.20.1", "版本 key 含 1.20.1");
                            const QVariantList files = d.value("files").toList();
                            check(!files.isEmpty(), "版本有文件");
                            if (!files.isEmpty()) {
                                const QVariantMap f = files.first().toMap();
                                check(!f.value("url").toString().isEmpty(), "文件有 downloadUrl");
                                std::printf("[TEST] 版本=%s 文件=%s url=%s\n",
                                            key.toUtf8().constData(),
                                            f.value("filename").toString().toUtf8().constData(),
                                            f.value("url").toString().left(70).toUtf8().constData());
                            }
                        }
                        std::printf(g_fail == 0 ? "\nALL PASS\n" : "\n%d FAILURES\n", g_fail);
                        app.exit(g_fail == 0 ? 0 : 1);
                    },
                    [&](const QString& e) {
                        check(false, "CF 文件请求失败");
                        std::printf("err: %s\n", e.toUtf8().constData());
                        app.exit(1);
                    });
            } else {
                std::printf("\nFAILURES\n");
                app.exit(1);
            }
        },
        [&](const QString& err) {
            check(false, "CF 搜索请求失败");
            std::printf("err: %s\n", err.toUtf8().constData());
            app.exit(1);
        });

    QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    return app.exec();
}
