// 外部 .minecraft 布局识别 / 游戏目录解析验证（2026-08-18）
// 覆盖：版本隔离（PCL2/HMCL 散文件 + Shadow game/ 子目录）、非隔离（共享）、
// 混合；来源启动器识别；外部只读模式（不写配置/不迁移）；真实 PCL2/HMCL 目录探测。
// 用法: MinecraftLayoutTest
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <cstdio>
#include "core/minecraft_layout.h"
#include "core/version_isolation.h"

using namespace ShadowLauncher;

namespace {

void writeFile(const QString& path, const QString& content = QStringLiteral("x"))
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) { f.write(content.toUtf8()); f.close(); }
}

void mk(const QString& path)
{
    QDir().mkpath(path);
}

// resolve 返回平台原生分隔符；比较前统一为 '/'（QDir::cleanPath）
QString norm(const QString& s)
{
    return QDir::cleanPath(s);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int fail = 0;
    const QString base = QStringLiteral("t_layout");

#define CHECK(cond, msg) \
    do { \
        const bool _ok = (cond); \
        fprintf(stderr, "  [%s] %s\n", _ok ? "PASS" : "FAIL", msg); \
        if (!_ok) ++fail; \
    } while (0)

    // ════════════════════════════════════════════════════════════
    // 1) PCL2 风格版本隔离目录
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/pcl2");
        mk(root + "/versions/vA"); mk(root + "/versions/vB");
        mk(root + "/versions/vA/mods"); mk(root + "/versions/vA/config");
        mk(root + "/versions/vA/PCL");
        writeFile(root + "/versions/vA/vA.json");
        writeFile(root + "/versions/vA/vA.jar");
        writeFile(root + "/versions/vA/options.txt");
        writeFile(root + "/versions/vB/vB.json");
        writeFile(root + "/versions/vB/vB.jar");
        writeFile(root + "/PCL.ini");

        const auto info = probeMinecraftFolder(root);
        fprintf(stderr, "\n[1] PCL2 版本隔离\n");
        CHECK(info.valid, "valid");
        CHECK(info.layout == MinecraftLayout::Isolated, "布局=版本隔离");
        CHECK(info.launcher == LauncherSource::Pcl2, "来源=PCL2");
        CHECK(info.totalVersionCount == 2, "版本数=2");
        CHECK(info.isolatedVersionCount == 1, "自带数据版本=1(vA)");
        CHECK(info.rootHasGameData == false, "根目录无数据");
        // 解析：vA 有数据→vA；vB 空但在隔离布局→vB
        CHECK(norm(resolveVersionGameDir(root, "vA", info.layout)).endsWith("/versions/vA"), "vA→版本目录");
        CHECK(norm(resolveVersionGameDir(root, "vB", info.layout)).endsWith("/versions/vB"), "vB(空)→版本目录");
    }

    // ════════════════════════════════════════════════════════════
    // 2) HMCL 风格版本隔离（.hmcl 标记）
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/hmcl");
        mk(root + "/versions/26.2-Forge/.hmcl/config");
        mk(root + "/versions/26.2-Forge/mods");
        writeFile(root + "/versions/26.2-Forge/26.2-Forge.json");
        writeFile(root + "/versions/26.2-Forge/26.2-Forge.jar");
        writeFile(root + "/launcher_profiles.json");

        const auto info = probeMinecraftFolder(root);
        fprintf(stderr, "\n[2] HMCL 版本隔离\n");
        CHECK(info.valid, "valid");
        CHECK(info.layout == MinecraftLayout::Isolated, "布局=版本隔离");
        CHECK(info.launcher == LauncherSource::Hmcl, "来源=HMCL");
        CHECK(info.totalVersionCount == 1, "版本数=1");
        CHECK(norm(resolveVersionGameDir(root, "26.2-Forge", info.layout))
                  .endsWith("/versions/26.2-Forge"), "版本→版本目录");
    }

    // ════════════════════════════════════════════════════════════
    // 3) 非隔离（共享）：根目录有数据
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/shared");
        mk(root + "/versions/vC");
        mk(root + "/saves"); mk(root + "/mods");
        writeFile(root + "/versions/vC/vC.json");
        writeFile(root + "/versions/vC/vC.jar");
        writeFile(root + "/options.txt");
        writeFile(root + "/launcher_profiles.json");

        const auto info = probeMinecraftFolder(root);
        fprintf(stderr, "\n[3] 非隔离（共享）\n");
        CHECK(info.valid, "valid");
        CHECK(info.layout == MinecraftLayout::Shared, "布局=共享(非隔离)");
        CHECK(info.launcher == LauncherSource::Official, "来源=官方(launcher_profiles)");
        CHECK(info.rootHasGameData, "根目录有数据");
        CHECK(info.isolatedVersionCount == 0, "无版本自带数据");
        // vC 目录无数据 → 共享根目录
        CHECK(norm(resolveVersionGameDir(root, "vC", info.layout)) == QStringLiteral("t_layout/shared"),
              "vC(空)→共享根目录");
    }

    // ════════════════════════════════════════════════════════════
    // 4) 混合：根有数据 + 一个版本自带数据
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/mixed");
        mk(root + "/versions/vD"); mk(root + "/versions/vE");
        mk(root + "/versions/vD/saves");
        writeFile(root + "/versions/vD/vD.json");
        writeFile(root + "/versions/vD/vD.jar");
        writeFile(root + "/versions/vE/vE.json");
        writeFile(root + "/versions/vE/vE.jar");
        writeFile(root + "/options.txt");

        const auto info = probeMinecraftFolder(root);
        fprintf(stderr, "\n[4] 混合布局\n");
        CHECK(info.valid, "valid");
        CHECK(info.layout == MinecraftLayout::Mixed, "布局=混合");
        CHECK(info.isolatedVersionCount == 1, "自带数据版本=1(vD)");
        CHECK(norm(resolveVersionGameDir(root, "vD", info.layout)).endsWith("/versions/vD"), "vD→版本目录");
        // vE 空 + 混合布局 → 共享根目录（保守侧）
        CHECK(norm(resolveVersionGameDir(root, "vE", info.layout)) == QStringLiteral("t_layout/mixed"),
              "vE(空,混合)→根(保守)");
    }

    // ════════════════════════════════════════════════════════════
    // 5) Shadow 标准 game/ 子目录（隔离）
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/shadow");
        mk(root + "/versions/vF/game");
        mk(root + "/config");
        writeFile(root + "/versions/vF/vF.json");
        writeFile(root + "/versions/vF/vF.jar");
        writeFile(root + "/versions/vF/game/options.txt");
        writeFile(root + "/config/version_isolation.json");

        const auto info = probeMinecraftFolder(root);
        fprintf(stderr, "\n[5] Shadow 标准 game/ 子目录\n");
        CHECK(info.valid, "valid");
        CHECK(info.launcher == LauncherSource::Shadow, "来源=Shadow");
        CHECK(info.layout == MinecraftLayout::Isolated, "布局=版本隔离");
        CHECK(norm(resolveVersionGameDir(root, "vF", info.layout)).endsWith("/versions/vF/game"),
              "vF→game/ 子目录");
    }

    // ════════════════════════════════════════════════════════════
    // 6) 无效目录
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/notmc");
        mk(root);
        const auto info = probeMinecraftFolder(root);
        fprintf(stderr, "\n[6] 无效目录\n");
        CHECK(!info.valid, "valid=false");
        CHECK(!info.notes.isEmpty(), "有备注");
    }

    // ════════════════════════════════════════════════════════════
    // 7) VersionIsolation 外部只读模式
    // ════════════════════════════════════════════════════════════
    {
        const QString root = base + QStringLiteral("/pcl2");
        VersionIsolation iso;
        iso.setGameDir(root);
        iso.setFolderLayout(MinecraftLayout::Isolated);
        const QString dA = iso.getVersionGameDir("vA");
        const QString dB = iso.getVersionGameDir("vB");
        fprintf(stderr, "\n[7] VersionIsolation 外部模式\n");
        CHECK(norm(dA).endsWith("/versions/vA"), "外部模式 vA→版本目录");
        CHECK(norm(dB).endsWith("/versions/vB"), "外部模式 vB→版本目录");
        // 外部模式禁止写配置/迁移
        CHECK(iso.migrateToIsolated("vB") == false, "外部模式拒绝迁移");
        CHECK(!QFile::exists(root + "/config/version_isolation.json"), "未写版本隔离配置");
        CHECK(!QFile::exists(root + "/versions/vB/game"), "未创建 game/ 目录");
        // 回退后恢复传统行为
        iso.setFolderLayout(MinecraftLayout::Unknown);
        iso.setEnabled(true);
        CHECK(norm(iso.getVersionGameDir("vB")).endsWith("/versions/vB"), "回退后传统行为");
    }

    // ════════════════════════════════════════════════════════════
    // 8) 真实 PCL2 / HMCL 目录探测（存在才跑，只读）
    // ════════════════════════════════════════════════════════════
    {
        const QStringList real = {
            QStringLiteral("D:/Minecraft/.minecraft"),
            QStringLiteral("C:/Users/蔡朝彬/Downloads/.minecraft"),
        };
        fprintf(stderr, "\n[8] 真实目录探测（存在才跑）\n");
        for (const QString& p : real) {
            if (!QDir(p).exists()) {
                fprintf(stderr, "  (跳过 %s)\n", p.toUtf8().constData());
                continue;
            }
            const auto info = probeMinecraftFolder(p);
            fprintf(stderr, "  %s\n", p.toUtf8().constData());
            fprintf(stderr, "    valid=%d layout=%s launcher=%s versions=%d isolated=%d rootData=%d\n",
                    info.valid ? 1 : 0,
                    layoutDisplayName(info.layout).toUtf8().constData(),
                    launcherDisplayName(info.launcher).toUtf8().constData(),
                    info.totalVersionCount, info.isolatedVersionCount,
                    info.rootHasGameData ? 1 : 0);
            for (const QString& note : info.notes)
                fprintf(stderr, "    · %s\n", note.toUtf8().constData());
            // 打印每个版本解析出的游戏目录
            for (const QString& id : info.versionIds) {
                const QString gd = resolveVersionGameDir(p, id, info.layout);
                fprintf(stderr, "    - %s → %s\n", id.toUtf8().constData(),
                        gd.toUtf8().constData());
            }
            // 真实目录应识别为版本隔离（当前两个样本均为隔离形态）
            CHECK(info.valid, ("真实目录有效: " + p).toUtf8().constData());
            CHECK(info.layout == MinecraftLayout::Isolated,
                  ("真实目录=版本隔离: " + p).toUtf8().constData());
        }
    }

#undef CHECK

    QDir(base).removeRecursively();
    fprintf(stderr, "\n=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
