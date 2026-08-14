// 用真实 1.20.6-neoforge JSON 验证 slf4j 去重（指向用户真实游戏目录）
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <cstdio>
#include "core/launcher.h"
#include "utils/logger.h"

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

    // 真实游戏目录（用户提供）
    const QString gameDir = QStringLiteral("C:/Users/蔡朝彬/Downloads/ShadowLauncher_v1.0.0 (1)/.minecraft");
    const QString versionId = QStringLiteral("1.20.6-neoforge-20.6.139");

    // 假 Java（避免依赖真实 JVM 探测；脚本组装不依赖 Java 版本对 slf4j 的影响）
    QString fakeJava = QDir::temp().filePath(QStringLiteral("fakejava21.bat"));
    {
        QFile jf(fakeJava);
        jf.open(QIODevice::WriteOnly);
        jf.write("@echo off\r\n");
        jf.write("echo openjdk version \"21.0.1\" 2023-10-17\r\n");
        jf.write("echo 64-Bit\r\n");
        jf.close();
    }

    fprintf(stderr, "=== 真实 1.20.6-neoforge slf4j 去重验证 ===\n");

    Launcher l;
    l.setGameDir(gameDir);
    QString script = l.buildLaunchScript(versionId, fakeJava, 4096, QString(), QString(), false);

    int count19 = script.count(QStringLiteral("log4j-slf4j2-impl-2.19.0"));
    int count22 = script.count(QStringLiteral("log4j-slf4j2-impl-2.22.1"));
    fprintf(stderr, "  2.19.0 出现 %d 次, 2.22.1 出现 %d 次\n", count19, count22);
    check(count19 == 0, "真实 JSON：2.19.0（AMN 冲突版）已排除");
    check(count22 == 1, "真实 JSON：2.22.1（正确版）保留");

    // module-path 参数（-p）应保留完整（含 securejarhandler 等核心模块）
    int pIdx = script.indexOf(QStringLiteral("-p"));
    QString modulePathSeg = pIdx >= 0 ? script.mid(pIdx, 500) : QString();
    fprintf(stderr, "  -p 段(前 250): %s\n", modulePathSeg.left(250).toUtf8().constData());
    check(pIdx >= 0 && modulePathSeg.contains(QStringLiteral("securejarhandler")),
          "-p module-path 参数完整保留");
    // module-path 上不应有 slf4j 绑定
    check(!modulePathSeg.contains(QStringLiteral("slf4j")),
          "module-path 不含 slf4j 绑定（slf4j 走 classpath）");

    QFile::remove(fakeJava);
    fprintf(stderr, fail == 0 ? "\nALL PASS\n" : "\n%d FAIL\n", fail);
    return fail;
}
