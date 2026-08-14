// NeoForge 1.20.6 log4j-slf4j 绑定去重测试（2026-08-14 修复验证）
// 复现：NeoForge 20.6.139 的 version JSON 同时列出
//   log4j-slf4j2-impl:2.19.0@jar（AMN 错误为 org.apache.logging.log4j.slf4j）
//   log4j-slf4j2-impl:2.22.1（module-info 正确为 slf4j2.impl）
// --add-modules ALL-MODULE-PATH 同时加载 → 包导出冲突 → ResolutionException
// 修复：buildClasspath 对同一 artifact 的多个 slf4j 绑定只保留最高版本
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
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

// 构造 NeoForge 风格 JSON：含两个 log4j-slf4j2-impl 版本 + 假 jar 文件
static QString makeNeoforgeJson(const QString& gameDir)
{
    QDir().mkpath(gameDir + QStringLiteral("/versions/1.20.6-neoforge-test"));
    QDir().mkpath(gameDir + QStringLiteral("/libraries/org/apache/logging/log4j/log4j-slf4j2-impl/2.19.0"));
    QDir().mkpath(gameDir + QStringLiteral("/libraries/org/apache/logging/log4j/log4j-slf4j2-impl/2.22.1"));
    QDir().mkpath(gameDir + QStringLiteral("/libraries/cpw/mods/bootstraplauncher/2.0.2"));
    QDir().mkpath(gameDir + QStringLiteral("/libraries/org/slf4j/slf4j-api/2.0.9"));

    // 假 jar 文件（内容无关，只需存在）
    {
        QFile j1(gameDir + QStringLiteral("/libraries/org/apache/logging/log4j/log4j-slf4j2-impl/2.19.0/log4j-slf4j2-impl-2.19.0.jar"));
        j1.open(QIODevice::WriteOnly); j1.write("x"); j1.close();
        QFile j2(gameDir + QStringLiteral("/libraries/org/apache/logging/log4j/log4j-slf4j2-impl/2.22.1/log4j-slf4j2-impl-2.22.1.jar"));
        j2.open(QIODevice::WriteOnly); j2.write("x"); j2.close();
        QFile jb(gameDir + QStringLiteral("/libraries/cpw/mods/bootstraplauncher/2.0.2/bootstraplauncher-2.0.2.jar"));
        jb.open(QIODevice::WriteOnly); jb.write("x"); jb.close();
        QFile ja(gameDir + QStringLiteral("/libraries/org/slf4j/slf4j-api/2.0.9/slf4j-api-2.0.9.jar"));
        ja.open(QIODevice::WriteOnly); ja.write("x"); ja.close();
    }

    QJsonObject vj;
    vj[QStringLiteral("id")] = QStringLiteral("1.20.6-neoforge-test");
    vj[QStringLiteral("mainClass")] = QStringLiteral("cpw.mods.bootstraplauncher.BootstrapLauncher");

    QJsonArray libs;
    // NeoForge 条目（2.19.0@jar，AMN 缺陷版）
    QJsonObject l1;
    l1[QStringLiteral("name")] = QStringLiteral("org.apache.logging.log4j:log4j-slf4j2-impl:2.19.0@jar");
    QJsonObject d1, a1;
    a1[QStringLiteral("path")] = QStringLiteral("org/apache/logging/log4j/log4j-slf4j2-impl/2.19.0/log4j-slf4j2-impl-2.19.0.jar");
    d1[QStringLiteral("artifact")] = a1;
    l1[QStringLiteral("downloads")] = d1;
    libs.append(l1);
    // MC 父条目（2.22.1，module-info 正确版）
    QJsonObject l2;
    l2[QStringLiteral("name")] = QStringLiteral("org.apache.logging.log4j:log4j-slf4j2-impl:2.22.1");
    QJsonObject d2, a2;
    a2[QStringLiteral("path")] = QStringLiteral("org/apache/logging/log4j/log4j-slf4j2-impl/2.22.1/log4j-slf4j2-impl-2.22.1.jar");
    d2[QStringLiteral("artifact")] = a2;
    l2[QStringLiteral("downloads")] = d2;
    libs.append(l2);
    // 一个普通库（确保正常逻辑不受影响）
    QJsonObject l3;
    l3[QStringLiteral("name")] = QStringLiteral("org.slf4j:slf4j-api:2.0.9");
    QJsonObject d3, a3;
    a3[QStringLiteral("path")] = QStringLiteral("org/slf4j/slf4j-api/2.0.9/slf4j-api-2.0.9.jar");
    d3[QStringLiteral("artifact")] = a3;
    l3[QStringLiteral("downloads")] = d3;
    libs.append(l3);

    vj[QStringLiteral("libraries")] = libs;

    // arguments.jvm 带 -p module path + --add-modules（NeoForge 特征）
    QJsonObject argsObj;
    QJsonArray jvmArr;
    QJsonObject pObj;
    QJsonArray pVal;
    pVal.append(QStringLiteral("-p"));
    pVal.append(QStringLiteral("${library_directory}/cpw/mods/bootstraplauncher/2.0.2/bootstraplauncher-2.0.2.jar"));
    jvmArr.append(pVal);
    jvmArr.append(QStringLiteral("--add-modules"));
    jvmArr.append(QStringLiteral("ALL-MODULE-PATH"));
    argsObj[QStringLiteral("jvm")] = jvmArr;
    QJsonArray gameArr;
    gameArr.append(QStringLiteral("--username"));
    gameArr.append(QStringLiteral("${auth_player_name}"));
    argsObj[QStringLiteral("game")] = gameArr;
    vj[QStringLiteral("arguments")] = argsObj;

    const QString jp = gameDir + QStringLiteral("/versions/1.20.6-neoforge-test/1.20.6-neoforge-test.json");
    QFile jf(jp);
    jf.open(QIODevice::WriteOnly); jf.write(QJsonDocument(vj).toJson()); jf.close();
    return jp;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QString gameDir = QDir::temp().filePath(QStringLiteral("slf4j_dedup_test_") + QString::number(qApp->applicationPid()));
    QDir().mkpath(gameDir);
    makeNeoforgeJson(gameDir);

    fprintf(stderr, "=== NeoForge slf4j 绑定去重测试 ===\n");

    // 用假 java 避免真实 JVM 探测
    QString fakeJava = QDir::temp().filePath(QStringLiteral("fakejava.bat"));
    {
        QFile jf(fakeJava);
        jf.open(QIODevice::WriteOnly);
        jf.write("@echo off\r\n");
        jf.write("echo openjdk version \"21.0.1\" 2023-10-17\r\n");
        jf.write("echo 64-Bit\r\n");
        jf.close();
    }

    Launcher l;
    l.setGameDir(gameDir);
    QString script = l.buildLaunchScript(QStringLiteral("1.20.6-neoforge-test"),
                                         fakeJava, 4096, QString(), QString(), false);

    // 断言 1：classpath 只含一个 log4j-slf4j2-impl（2.22.1）
    int count19 = script.count(QStringLiteral("log4j-slf4j2-impl-2.19.0"));
    int count22 = script.count(QStringLiteral("log4j-slf4j2-impl-2.22.1"));
    fprintf(stderr, "  2.19.0 出现 %d 次, 2.22.1 出现 %d 次\n", count19, count22);
    check(count19 == 0, "log4j-slf4j2-impl 2.19.0 已从 classpath 排除（AMN 冲突版）");
    check(count22 == 1, "log4j-slf4j2-impl 2.22.1 保留（module-info 正确版）");

    // 断言 2：slf4j-api 正常保留
    fprintf(stderr, "  script 含 slf4j-api: %d\n", script.contains(QStringLiteral("slf4j-api")));
    fprintf(stderr, "  script 前 400 字:\n  %s\n",
            script.left(400).toUtf8().constData());
    check(script.contains(QStringLiteral("slf4j-api-2.0.9")), "slf4j-api 正常保留");

    // 断言 3：普通库不受影响
    check(script.contains(QStringLiteral("ALL-MODULE-PATH")), "--add-modules ALL-MODULE-PATH 保留");

    // 清理
    QDir(gameDir).removeRecursively();
    QFile::remove(fakeJava);

    fprintf(stderr, fail == 0 ? "\nALL PASS\n" : "\n%d FAIL\n", fail);
    return fail;
}
