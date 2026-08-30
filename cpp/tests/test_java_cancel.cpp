// 验证 Java 自动安装取消语义（2026-08-08）：
//   a) 已缓存（=装好了）时 installJavaAsync 直接完成，cancelInstall 不误删
//   b) 不存在版本 → 获取列表失败 → failJob 清理残留（zip/目录）
// 真实网络（Tuna）。用法: JavaCancelTest <cachedMajor> <fakeMajor>
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <cstdio>
#include "core/java_runtime_installer.h"
#include "utils/logger.h"

using namespace ShadowLauncher;

static int fail = 0;
static int phase = 0;   // 0=cached, 1=fake

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    const int cachedMajor = argc > 1 ? QString::fromUtf8(argv[1]).toInt() : 25;
    const int fakeMajor = argc > 2 ? QString::fromUtf8(argv[2]).toInt() : 99;

    JavaRuntimeInstaller inst;
    QElapsedTimer timer;
    timer.start();

    QObject::connect(&inst, &JavaRuntimeInstaller::logMessage, [](const QString& m) {
        fprintf(stderr, "[log] %s\n", m.toUtf8().constData());
    });

    // ── 场景 a：已缓存 → 直接完成，cancelInstall 不误删 ──
    fprintf(stderr, "=== TEST a: cached Java %d ===\n", cachedMajor);
    phase = 0;
    // ── 2026-08-18：jdk（JRE 缺 jdk.crypto.ec；缓存命中要求真 JDK）──
    inst.installJavaAsync(cachedMajor, QStringLiteral("jdk"),
        [&](bool ok, const QString& err, const QString& exe) {
            fprintf(stderr, "[a] onDone ok=%d err=%s exe=%s\n", ok ? 1 : 0,
                    err.toUtf8().constData(), exe.toUtf8().constData());
            const bool dirKept = QDir(QCoreApplication::applicationDirPath()
                + QStringLiteral("/java_cache/%1").arg(cachedMajor)).exists();
            fprintf(stderr, "[a] dirKept=%d\n", dirKept ? 1 : 0);
            if (!ok || !dirKept) fail++;
            // 场景 a 完成 → 测场景 b
            phase = 1;
            fprintf(stderr, "=== TEST b: fake Java %d (获取列表失败 → 清理) ===\n", fakeMajor);
            inst.installJavaAsync(fakeMajor, QStringLiteral("jdk"),
                [&](bool ok2, const QString& err2, const QString&) {
                    fprintf(stderr, "[b] onDone ok=%d err=%s\n", ok2 ? 1 : 0, err2.toUtf8().constData());
                    const bool zipGone = !QFile::exists(QCoreApplication::applicationDirPath()
                        + QStringLiteral("/java_cache/.tmp-jdk-%1.zip").arg(fakeMajor));
                    const bool dirGone = !QDir(QCoreApplication::applicationDirPath()
                        + QStringLiteral("/java_cache/%1").arg(fakeMajor)).exists();
                    fprintf(stderr, "[b] zipCleaned=%d dirCleaned=%d\n", zipGone ? 1 : 0, dirGone ? 1 : 0);
                    if (ok2 || !zipGone || !dirGone) fail++;
                    fprintf(stderr, "=== %s (elapsed %lldms) ===\n", fail ? "FAIL" : "PASS", (long long)timer.elapsed());
                    QCoreApplication::exit(fail ? 1 : 0);
                });
        });

    QTimer::singleShot(120000, []() { fprintf(stderr, "=== TIMEOUT 120s\n"); QCoreApplication::exit(2); });
    return app.exec();
}
