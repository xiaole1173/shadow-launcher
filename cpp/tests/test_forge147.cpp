// Forge 旧版安装集成测试（Legacy 2/3 统一路线验证）
// 用法: Forge147Test <mcVersion> <forgeVersion> [gameDir]
#include <QCoreApplication>
#include <QTimer>
#include <QDir>
#include <cstdio>

#include "core/mod_loader_installer.h"
#include "utils/logger.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());

    const QString mc = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral("1.5.2");
    const QString fv = argc > 2 ? QString::fromUtf8(argv[2]) : QStringLiteral("7.8.1.738");
    const QString gameDir = argc > 3 ? QString::fromUtf8(argv[3]) : QStringLiteral("forge_old_test_dir");
    QDir().mkpath(gameDir);

    auto* ml = new ShadowLauncher::ModLoaderInstaller(&app);
    ml->setGameDir(gameDir);

    QObject::connect(ml, &ShadowLauncher::ModLoaderInstaller::finished,
        [ml](bool ok, const QString& err) {
            fprintf(stderr, "=== FINISHED ok=%d err=%s\n", ok ? 1 : 0, err.toUtf8().constData());
            ml->deleteLater();
            QCoreApplication::exit(ok ? 0 : 1);
        });
    QObject::connect(ml, &ShadowLauncher::ModLoaderInstaller::logMessage,
        [](const QString& msg) {
            fprintf(stderr, "[ML] %s\n", msg.toUtf8().constData());
        });
    QObject::connect(ml, &ShadowLauncher::ModLoaderInstaller::progressChanged,
        [](int step, int total, const QString& status) {
            fprintf(stderr, "[ML] progress %d/%d %s\n", step, total, status.toUtf8().constData());
        });
    QObject::connect(ml, &ShadowLauncher::ModLoaderInstaller::waitingForMC,
        [ml]() {
            fprintf(stderr, "[ML] waitingForMC → forgeContinueInstall\n");
            ml->forgeContinueInstall();
        });

    fprintf(stderr, "=== START installForge mc=%s fv=%s gameDir=%s\n",
           mc.toUtf8().constData(), fv.toUtf8().constData(), gameDir.toUtf8().constData());
    ml->installForge(mc, fv, mc + QStringLiteral("-forge-") + fv, QString());

    QTimer::singleShot(180000, []() {
        fprintf(stderr, "=== TIMEOUT 180s\n");
        QCoreApplication::exit(2);
    });

    return app.exec();
}
