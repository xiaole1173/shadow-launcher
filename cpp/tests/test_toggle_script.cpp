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
#include <private/qzipwriter_p.h>
#include <QBuffer>

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

    // ── 1b. disabled 后缀下仍能解析 JAR 内容（2026-08-08 修复）──
    {
        const QString gameDir = QStringLiteral("t_toggle2");
        QDir().mkpath(gameDir + QStringLiteral("/mods"));
        QFile::remove(gameDir + QStringLiteral("/mods/realmod.jar.disabled"));
        // 构造真实 zip：含 fabric.mod.json（QZipWriter，Qt6::GuiPrivate）
        {
            QBuffer zbuf;
            zbuf.open(QIODevice::WriteOnly);
            QZipWriter zw(&zbuf);
            zw.addFile(QStringLiteral("fabric.mod.json"),
                QJsonDocument(QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("realmod")},
                    {QStringLiteral("name"), QStringLiteral("Real Mod")},
                    {QStringLiteral("version"), QStringLiteral("1.2.3")},
                }).toJson());
            zw.close();
            QFile f(gameDir + QStringLiteral("/mods/realmod.jar.disabled"));
            f.open(QIODevice::WriteOnly); f.write(zbuf.data()); f.close();
        }
        LocalModManager lmm;
        lmm.setGameDir(gameDir);
        QVariantList scan = lmm.scanMods(QString());
        bool found = false;
        for (const QVariant& v : scan) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("fileName")).toString() != QStringLiteral("realmod.jar.disabled")) continue;
            found = true;
            // 关键：disabled 文件也必须解析出 modId/name/version（真实路径直接读 zip）
            bool idOk = m.value(QStringLiteral("modId")).toString() == QStringLiteral("realmod");
            bool nameOk = m.value(QStringLiteral("modName")).toString() == QStringLiteral("Real Mod");
            bool verOk = m.value(QStringLiteral("version")).toString() == QStringLiteral("1.2.3");
            bool disabledOk = m.value(QStringLiteral("enabled")).toBool() == false;
            fprintf(stderr, "[1b] disabled parse: id=%d name=%d ver=%d disabled=%d\n",
                    idOk ? 1 : 0, nameOk ? 1 : 0, verOk ? 1 : 0, disabledOk ? 1 : 0);
            if (!(idOk && nameOk && verOk && disabledOk)) fail++;
        }
        if (!found) { fprintf(stderr, "[1b] disabled mod not found in scan\n"); fail++; }
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

    // ── 2b. buildLaunchScript 全启动选项（2026-08-10）：pre/post 命令、GPU、优先级、窗口标题、token 脱敏 ──
    {
        const QString gameDir = QStringLiteral("t_script2");
        QDir().mkpath(gameDir + QStringLiteral("/versions/1.20.4"));
        QJsonObject vj;
        vj[QStringLiteral("id")] = QStringLiteral("1.20.4");
        vj[QStringLiteral("mainClass")] = QStringLiteral("net.minecraft.client.main.Main");
        vj[QStringLiteral("libraries")] = QJsonArray();
        QJsonObject argsObj;
        QJsonArray gameArr;
        gameArr.append(QStringLiteral("--username"));
        gameArr.append(QStringLiteral("${auth_player_name}"));
        gameArr.append(QStringLiteral("--accessToken"));
        gameArr.append(QStringLiteral("${auth_access_token}"));
        gameArr.append(QStringLiteral("--clientId"));
        gameArr.append(QStringLiteral("${clientid}"));
        argsObj[QStringLiteral("game")] = gameArr;
        // Fabric 26.x 风格 jvm 参数（等号后带空格，2026-08-10 实锤导出脚本启动失败根因）
        QJsonArray jvmArr;
        jvmArr.append(QStringLiteral("-DFabricMcEmu= net.minecraft.client.main.Main "));
        argsObj[QStringLiteral("jvm")] = jvmArr;
        vj[QStringLiteral("arguments")] = argsObj;
        QFile jf(gameDir + QStringLiteral("/versions/1.20.4/1.20.4.json"));
        jf.open(QIODevice::WriteOnly); jf.write(QJsonDocument(vj).toJson()); jf.close();

        Launcher launcher;
        launcher.setGameDir(gameDir);
        launcher.setVersionGameDir(gameDir);
        launcher.setAuthInfo(QStringLiteral("Alex"), QStringLiteral("00000000-0000-0000-0000-000000000002"),
                             QStringLiteral("SECRET_TOKEN_XYZ"), false);
        launcher.setAutoLangMode(0);
        launcher.setPreLaunchCommand(QStringLiteral("echo PRE_CMD"));
        launcher.setPostExitCommand(QStringLiteral("echo POST_CMD"));
        launcher.setProcessPriority(0);   // 高 → PS 包装 + AboveNormal
        launcher.setWindowTitleOverride(QStringLiteral("My Game Title"));
        const QString script = launcher.buildLaunchScript(
            QStringLiteral("1.20.4"), QStringLiteral("C:/java/bin/java.exe"),
            2048, QString(), QString(), true /* highPerfGpu */);

        // pre/post 命令
        const bool hasPre = script.contains(QStringLiteral("echo PRE_CMD"));
        const bool hasPost = script.contains(QStringLiteral("echo POST_CMD"));
        // GPU 块
        const bool hasGpuEnv = script.contains(QStringLiteral("SHIM_MCCOMPAT"));
        const bool hasGpuReg = script.contains(QStringLiteral("UserGpuPreferences"));
        // PS 包装 + 优先级 + 标题（解码 EncodedCommand）
        const QString psMarker = QStringLiteral("powershell -NoProfile -EncodedCommand ");
        const bool hasPs = script.contains(psMarker);
        bool psPrio = false, psTitle = false, psMcEmu = false, psEmptyArg = false;
        int psIdx = script.indexOf(psMarker);
        if (psIdx >= 0) {
            QString b64 = script.mid(psIdx + psMarker.length());
            int nl = b64.indexOf(QLatin1Char('\r'));
            if (nl >= 0) b64 = b64.left(nl);
            QByteArray enc = QByteArray::fromBase64(b64.toLatin1());
            QString psText;
            for (int i = 0; i + 1 < enc.size(); i += 2)
                psText += QChar(uchar(enc[i]) | (uchar(enc[i + 1]) << 8));
            psPrio = psText.contains(QStringLiteral("-PriorityClass AboveNormal"));
            psTitle = psText.contains(QStringLiteral("SetWindowText"))
                   && psText.contains(QStringLiteral("My Game Title"));
            // FabricMcEmu 空格合并：-DFabricMcEmu=net.minecraft...（不带 "McEmu= " 拆分）
            psMcEmu = psText.contains(QStringLiteral("-DFabricMcEmu=net.minecraft.client.main.Main"))
                   && !psText.contains(QStringLiteral("-DFabricMcEmu= net.minecraft"));
            // 空参数保留：--clientId 后跟 ""（否则后续参数错位）
            psEmptyArg = psText.contains(QStringLiteral("--clientId"))
                      && psText.contains(QStringLiteral("'\"\"'"));
        }
        // 低优先级路径（2=低 → BelowNormal 包装）
        Launcher launcherLow;
        launcherLow.setGameDir(gameDir);
        launcherLow.setVersionGameDir(gameDir);
        launcherLow.setAuthInfo(QStringLiteral("Alex"), QString(), QString(), false);
        launcherLow.setAutoLangMode(0);
        launcherLow.setProcessPriority(2);
        const QString scriptLow = launcherLow.buildLaunchScript(
            QStringLiteral("1.20.4"), QStringLiteral("C:/java/bin/java.exe"),
            2048, QString(), QString(), false);
        bool psLow = false;
        int psLowIdx = scriptLow.indexOf(psMarker);
        if (psLowIdx >= 0) {
            QString b64 = scriptLow.mid(psLowIdx + psMarker.length());
            int nl = b64.indexOf(QLatin1Char('\r'));
            if (nl >= 0) b64 = b64.left(nl);
            QByteArray enc = QByteArray::fromBase64(b64.toLatin1());
            QString psText;
            for (int i = 0; i + 1 < enc.size(); i += 2)
                psText += QChar(uchar(enc[i]) | (uchar(enc[i + 1]) << 8));
            psLow = psText.contains(QStringLiteral("-PriorityClass BelowNormal"));
        }
        // 默认路径（优先级中 + 无标题 → 直接 java 行）：FabricMcEmu 合并 + 空参数 "" 保留
        Launcher launcherMid;
        launcherMid.setGameDir(gameDir);
        launcherMid.setVersionGameDir(gameDir);
        launcherMid.setAuthInfo(QStringLiteral("Alex"), QStringLiteral("00000000-0000-0000-0000-000000000002"),
                                QStringLiteral("SECRET_TOKEN_XYZ"), true);
        launcherMid.setAutoLangMode(0);
        const QString scriptMid = launcherMid.buildLaunchScript(
            QStringLiteral("1.20.4"), QStringLiteral("C:/java/bin/java.exe"),
            2048, QString(), QString(), false);
        const bool midNoPs = !scriptMid.contains(psMarker);
        const bool midMcEmu = scriptMid.contains(QStringLiteral("-DFabricMcEmu=net.minecraft.client.main.Main"))
                           && !scriptMid.contains(QStringLiteral("-DFabricMcEmu= net.minecraft"));
        const bool midEmptyArg = scriptMid.contains(QStringLiteral("--clientId \"\""));
        // token 保留策略（2026-08-10）：有效 token 进脚本（正版会话/Realms 可用）+ 凭据提示
        // （时效验证/刷新在 ShadowBackend::exportLaunchScript，buildLaunchScript 不再脱敏）
        const bool midTokenKept = scriptMid.contains(QStringLiteral("SECRET_TOKEN_XYZ"));
        const bool midHint = scriptMid.contains(QStringLiteral("请勿分享"));
        // 离线路径（token 空）：buildArgs 以 0 占位（直接行 + 空 token）
        Launcher launcherOff;
        launcherOff.setGameDir(gameDir);
        launcherOff.setVersionGameDir(gameDir);
        launcherOff.setAuthInfo(QStringLiteral("Alex"), QString(), QString(), false);
        launcherOff.setAutoLangMode(0);
        const QString scriptOff = launcherOff.buildLaunchScript(
            QStringLiteral("1.20.4"), QStringLiteral("C:/java/bin/java.exe"),
            2048, QString(), QString(), false);
        const bool midOfflineToken = scriptOff.contains(QStringLiteral("--accessToken 0"));
        fprintf(stderr, "[2b] full options: pre=%d post=%d gpuEnv=%d gpuReg=%d ps=%d psPrio=%d psTitle=%d psLow=%d psMcEmu=%d psEmptyArg=%d midNoPs=%d midMcEmu=%d midEmptyArg=%d midTokenKept=%d midHint=%d midOfflineToken=%d\n",
                hasPre ? 1 : 0, hasPost ? 1 : 0, hasGpuEnv ? 1 : 0, hasGpuReg ? 1 : 0,
                hasPs ? 1 : 0, psPrio ? 1 : 0, psTitle ? 1 : 0, psLow ? 1 : 0,
                psMcEmu ? 1 : 0, psEmptyArg ? 1 : 0, midNoPs ? 1 : 0, midMcEmu ? 1 : 0, midEmptyArg ? 1 : 0,
                midTokenKept ? 1 : 0, midHint ? 1 : 0, midOfflineToken ? 1 : 0);
        if (!(hasPre && hasPost && hasGpuEnv && hasGpuReg && hasPs && psPrio && psTitle
              && psLow && psMcEmu && psEmptyArg && midNoPs && midMcEmu && midEmptyArg
              && midTokenKept && midHint && midOfflineToken)) fail++;
        QDir(gameDir).removeRecursively();
    }

    fprintf(stderr, "=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
