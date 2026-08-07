// 验证 VersionIsolation::getVersionGameDir 布局感知（散文件→版本根 / game非空→game）
// 用法: IsoGdTest
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <cstdio>
#include "core/version_isolation.h"

using namespace ShadowLauncher;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int fail = 0;
    const QString gameDir = QStringLiteral("t_iso");
    QDir().mkpath(gameDir + QStringLiteral("/versions/v1"));
    QDir().mkpath(gameDir + QStringLiteral("/versions/v2/game"));

    // v1: 散文件布局（版本根有 jar，无 game/ 或 game 空）
    QFile f(gameDir + QStringLiteral("/versions/v1/v1.jar"));
    f.open(QIODevice::WriteOnly); f.write("x"); f.close();
    // v2: game/ 有内容
    QFile g(gameDir + QStringLiteral("/versions/v2/game/options.txt"));
    g.open(QIODevice::WriteOnly); g.write("lang:en_us"); g.close();

    VersionIsolation iso;
    iso.setGameDir(gameDir);
    iso.setEnabled(true);

    const QString d1 = iso.getVersionGameDir(QStringLiteral("v1"));
    const QString d2 = iso.getVersionGameDir(QStringLiteral("v2"));
    fprintf(stderr, "v1(散文件) → %s\n", d1.toUtf8().constData());
    fprintf(stderr, "v2(game)   → %s\n", d2.toUtf8().constData());
    const bool ok1 = d1.endsWith(QStringLiteral("/versions/v1"));
    const bool ok2 = d2.endsWith(QStringLiteral("/versions/v2/game"));
    fprintf(stderr, "v1=版本根:%d v2=game:%d\n", ok1 ? 1 : 0, ok2 ? 1 : 0);
    if (!(ok1 && ok2)) fail++;

    // v3: 不存在 → 返回版本根（不创建 game）
    const QString d3 = iso.getVersionGameDir(QStringLiteral("v3"));
    const bool noGameCreated = !QDir().exists(gameDir + QStringLiteral("/versions/v3/game"));
    fprintf(stderr, "v3(不存在) → %s (game 未创建:%d)\n", d3.toUtf8().constData(), noGameCreated ? 1 : 0);
    if (!(d3.endsWith(QStringLiteral("/versions/v3")) && noGameCreated)) fail++;

    QDir(gameDir).removeRecursively();
    fprintf(stderr, "=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
