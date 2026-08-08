// 验证 JavaRuntimeInstaller::installJavaAsync（启动自动安装 Java 的核心组件）：
// worker 下载/解压 + 主线程回调 + 进度信号。用法: JavaAutoInstallTest <major>
// 真实网络（Tuna Adoptium 镜像）。已存在的缓存会直接完成（不重新下载）。
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <cstdio>
#include "core/java_runtime_installer.h"
#include "utils/logger.h"

using namespace ShadowLauncher;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    const int major = argc > 1 ? QString::fromUtf8(argv[1]).toInt() : 8;

    JavaRuntimeInstaller inst;
    QElapsedTimer timer;
    timer.start();

    QObject::connect(&inst, &JavaRuntimeInstaller::downloadProgressChanged, [&]() {
        fprintf(stderr, "[prog] step=%d status=%s pct=%d bytes=%lld/%lld speed=%.2fMB/s elapsed=%lldms\n",
                inst.currentStep(), inst.statusText().toUtf8().constData(),
                inst.downloadPercent(), (long long)inst.downloadBytes(), (long long)inst.downloadTotal(),
                inst.downloadSpeedMBps(), (long long)timer.elapsed());
    });
    QObject::connect(&inst, &JavaRuntimeInstaller::logMessage, [](const QString& m) {
        fprintf(stderr, "[log] %s\n", m.toUtf8().constData());
    });

    fprintf(stderr, "=== START installJavaAsync major=%d ===\n", major);
    // 模拟启动流程：QTimer::singleShot 模拟“启动检查在等”
    QTimer::singleShot(0, [&]() {
        inst.installJavaAsync(major, QStringLiteral("jre"),
            [&](bool ok, const QString& err, const QString& exe) {
                fprintf(stderr, "=== DONE ok=%d err=%s exe=%s elapsed=%lldms mainThread=%d ===\n",
                        ok ? 1 : 0, err.toUtf8().constData(), exe.toUtf8().constData(),
                        (long long)timer.elapsed(), (int)QThread::currentThread()->isMainThread());
                QCoreApplication::exit(ok ? 0 : 1);
            });
    });

    QTimer::singleShot(300000, []() { fprintf(stderr, "=== TIMEOUT 300s\n"); QCoreApplication::exit(2); });
    return app.exec();
}
