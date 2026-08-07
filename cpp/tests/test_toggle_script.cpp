// 验证 setModEnabled（.jar ↔ .jar.disabled 重命名）+ buildLaunchScript 生成
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <cstdio>
#include "core/local_mod_manager.h"
#include "core/launcher.h"
#include "utils/logger.h"

using namespace ShadowLauncher;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());
    int fail = 0;

    // ── 1. setModEnabled ──
    {
        const QString gameDir = QStringLiteral("t_toggle");
        QDir().mkpath(gameDir + QStringLiteral("/mods"));
        QFile::remove(gameDir + QStringLiteral("/mods/foo.jar"));
        QFile::remove(gameDir + QStringLiteral("/mods/foo.jar.disabled"));
        QFile f(gameDir + QStringLiteral("/mods/foo.jar"));
        f.open(QIODevice::WriteOnly); f.write("jar"); f.close();

        LocalModManager lmm;
        lmm.setGameDir(gameDir);
        // 禁用
        bool ok1 = lmm.setModEnabled(QStringLiteral("foo.jar"), QString(), false);
        bool disabledExists = QFile::exists(gameDir + QStringLiteral("/mods/foo.jar.disabled"));
        // 扫描应标记 enabled=false
        QVariantList scan = lmm.scanMods(QString());
        bool scanDisabled = false;
        for (const QVariant& v : scan) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("fileName")).toString() == QStringLiteral("foo.jar.disabled"))
                scanDisabled = (m.value(QStringLiteral("enabled")).toBool() == false);
        }
        // 启用
        bool ok2 = lmm.setModEnabled(QStringLiteral("foo.jar.disabled"), QString(), true);
        bool enabledExists = QFile::exists(gameDir + QStringLiteral("/mods/foo.jar"));
        fprintf(stderr, "[1] setModEnabled: disable=%d disabledFile=%d scanDisabled=%d enable=%d enabledFile=%d\n",
                ok1 ? 1 : 0, disabledExists ? 1 : 0, scanDisabled ? 1 : 0, ok2 ? 1 : 0, enabledExists ? 1 : 0);
        if (!(ok1 && disabledExists && scanDisabled && ok2 && enabledExists)) fail++;
        QDir(gameDir + QStringLiteral("/mods")).removeRecursively();
    }

    // ── 2. buildLaunchScript ──
    {
        const QString gameDir = QStringLiteral("t_script");
        QDir().mkpath(gameDir + QStringLiteral("/versions/1.12.2"));
        QJsonObject vj;
        vj[QStringLiteral("id")] = QStringLiteral("1.12.2");
        vj[QStringLiteral("mainClass")] = QStringLiteral("net.minecraft.client.main.Main");
        vj[QStringLiteral("libraries")] = QJsonArray();
        QJsonObject argsObj;
        QJsonArray gameArr;
        gameArr.append(QStringLiteral("--username"));
        gameArr.append(QStringLiteral("${auth_player_name}"));
        gameArr.append(QStringLiteral("--gameDir"));
        gameArr.append(QStringLiteral("${game_directory}"));
        argsObj[QStringLiteral("game")] = gameArr;
        vj[QStringLiteral("arguments")] = argsObj;
        QFile jf(gameDir + QStringLiteral("/versions/1.12.2/1.12.2.json"));
        jf.open(QIODevice::WriteOnly); jf.write(QJsonDocument(vj).toJson()); jf.close();

        Launcher launcher;
        launcher.setGameDir(gameDir);
        launcher.setVersionGameDir(gameDir);
        launcher.setAuthInfo(QStringLiteral("Steve"), QStringLiteral("00000000-0000-0000-0000-000000000001"),
                             QStringLiteral("tok"), false);
        launcher.setAutoLangMode(1);   // system locale → ensureOptionsTxt 应写 lang 行
        const QString script = launcher.buildLaunchScript(
            QStringLiteral("1.12.2"), QStringLiteral("C:/java/bin/java.exe"),
            2048, QStringLiteral("-Xmx2G"), QStringLiteral("--demo"), false);
        const bool hasEcho = script.contains(QStringLiteral("@echo off"));
        const bool hasCmd = script.contains(QStringLiteral("java.exe"));
        const bool hasMain = script.contains(QStringLiteral("net.minecraft.client.main.Main"));
        const bool hasUser = script.contains(QStringLiteral("Steve"));
        // --gameDir 后有值（t_script 路径出现）
        const bool gdOk = script.contains(QStringLiteral("t_script"));
        // 语言选项写入 options.txt（ensureOptionsTxt 生效）
        QFile opt(gameDir + QStringLiteral("/options.txt"));
        bool langOk = false;
        if (opt.open(QIODevice::ReadOnly)) {
            const QString optText = QString::fromUtf8(opt.readAll());
            langOk = optText.contains(QStringLiteral("lang:"));
            opt.close();
        }
        fprintf(stderr, "[2] buildLaunchScript: len=%d echo=%d cmd=%d main=%d user=%d gameDirFilled=%d lang=%d\n",
                script.size(), hasEcho ? 1 : 0, hasCmd ? 1 : 0, hasMain ? 1 : 0,
                hasUser ? 1 : 0, gdOk ? 1 : 0, langOk ? 1 : 0);
        if (!(hasEcho && hasCmd && hasMain && hasUser && gdOk && langOk)) fail++;
        QDir(gameDir).removeRecursively();
    }

    fprintf(stderr, "=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
