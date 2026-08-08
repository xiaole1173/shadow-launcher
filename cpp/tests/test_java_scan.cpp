// 验证 JavaRuntimeInstaller::scanSystemJavas 能检测到 MC 官方 runtime 的 Java
// （%APPDATA%/.minecraft/runtime/jre-legacy 等，2026-08-08 修复）
// scanSystemJavas 是异步的：等 systemJavaScanFinished 信号后检查结果。
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>
#include "core/java_runtime_installer.h"
#include "utils/logger.h"

using namespace ShadowLauncher;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());

    JavaRuntimeInstaller inst;
    bool foundMcRuntime = false;
    bool done = false;

    QObject::connect(&inst, &JavaRuntimeInstaller::systemJavaScanFinished, [&]() {
        const QVariantList javas = inst.detectedSystemJavas();
        fprintf(stderr, "=== 检测到 %d 个 Java ===\n", javas.size());
        for (const QVariant& v : javas) {
            const QVariantMap m = v.toMap();
            const QString path = m.value(QStringLiteral("path")).toString();
            fprintf(stderr, "  Java %d %s: %s\n",
                    m.value(QStringLiteral("major")).toInt(),
                    m.value(QStringLiteral("isJdk")).toBool() ? "JDK" : "JRE",
                    path.toUtf8().constData());
            if (path.contains(QStringLiteral(".minecraft/runtime"), Qt::CaseInsensitive))
                foundMcRuntime = true;
        }
        fprintf(stderr, "=== mcRuntimeFound=%d ===\n", foundMcRuntime ? 1 : 0);
        done = true;
        QCoreApplication::exit(foundMcRuntime ? 0 : 1);
    });

    fprintf(stderr, "=== 开始扫描 ===\n");
    inst.scanSystemJavas();

    QTimer::singleShot(60000, []() { fprintf(stderr, "=== TIMEOUT 60s ===\n"); QCoreApplication::exit(2); });
    return app.exec();
}
