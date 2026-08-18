// 游戏文件夹注册表验证（2026-08-18 QML 阶段）
// 覆盖：命名文件（config/shadow_folder.json）、注册表（game_folders.json）、
// 添加/重命名/删除条目（不删文件夹）、活动切换（applyRequested 信号）、
// 固有文件夹不可删、异步刷新（foldersChanged）。
// 用法: GameFolderTest
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include "backend/minecraft_folder_backend.h"

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

QString readJsonName(const QString& file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    return doc.object().value(QStringLiteral("name")).toString();
}

bool waitFolders(MinecraftFolderBackend& b, int timeoutMs = 3000)
{
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(&b, &MinecraftFolderBackend::foldersChanged,
                     &loop, &QEventLoop::quit);
    loop.exec();
    return !b.folders().isEmpty();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int fail = 0;

#define CHECK(cond, msg) \
    do { \
        const bool _ok = (cond); \
        fprintf(stderr, "  [%s] %s\n", _ok ? "PASS" : "FAIL", msg); \
        if (!_ok) ++fail; \
    } while (0)

    const QString base = QStringLiteral("t_gfolder");
    const QString dataDir = base + QStringLiteral("/data");
    mk(dataDir);

    // 构造一个有效的外部 .minecraft（PCL2 隔离风格）
    const QString pcl2 = base + QStringLiteral("/pcl2");
    mk(pcl2 + "/versions/v1/mods");
    mk(pcl2 + "/versions/v1/PCL");
    writeFile(pcl2 + "/versions/v1/v1.json");
    writeFile(pcl2 + "/versions/v1/v1.jar");
    writeFile(pcl2 + "/versions/v1/options.txt");
    writeFile(pcl2 + "/PCL.ini");

    MinecraftFolderBackend b;
    b.setDataDir(dataDir);

    fprintf(stderr, "\n[1] 初始刷新（仅固有文件夹）\n");
    b.refreshFolders();
    CHECK(waitFolders(b), "foldersChanged 触发");
    CHECK(b.folders().size() == 1, "仅 1 条（固有）");
    CHECK(b.folders()[0].toMap()["isDefault"].toBool(), "第一项=固有");
    CHECK(b.folders()[0].toMap()["name"].toString() == "默认游戏目录", "固有默认名");
    CHECK(b.activeFolderPath() == b.defaultFolderPath(), "活动=默认");

    fprintf(stderr, "\n[2] 添加文件夹（写命名文件+注册表）\n");
    CHECK(b.addGameFolder(pcl2, "我的PCL2"), "addGameFolder 成功");
    CHECK(waitFolders(b), "刷新完成");
    CHECK(b.folders().size() == 2, "2 条（固有+导入）");
    const QString nameFile = pcl2 + "/config/shadow_folder.json";
    CHECK(QFile::exists(nameFile), "命名文件已写入");
    CHECK(readJsonName(nameFile) == "我的PCL2", "命名文件内容正确");
    const auto imp = b.folders()[1].toMap();
    CHECK(imp["name"].toString() == "我的PCL2", "列表名称=用户命名");
    CHECK(imp["path"].toString() == pcl2, "路径正确");
    CHECK(imp["launcherName"].toString() == "PCL2", "来源识别=PCL2");
    CHECK(imp["versionCount"].toInt() == 1, "版本数=1");

    fprintf(stderr, "\n[3] 重命名\n");
    CHECK(b.renameGameFolder(pcl2, "RLCraft 高配"), "renameGameFolder 成功");
    CHECK(waitFolders(b), "刷新完成");
    CHECK(readJsonName(nameFile) == "RLCraft 高配", "命名文件已更新");
    CHECK(b.folders()[1].toMap()["name"].toString() == "RLCraft 高配", "列表名称已更新");

    fprintf(stderr, "\n[4] 切换活动（applyRequested 信号 + 活动标记）\n");
    bool applied = false; int appliedLayout = -1;
    QObject::connect(&b, &MinecraftFolderBackend::applyRequested,
                     [&](const QString& root, int layout) {
        applied = true; appliedLayout = layout;
    });
    CHECK(b.setActiveFolder(pcl2), "setActiveFolder 成功");
    CHECK(applied, "applyRequested 已发出");
    CHECK(QDir::cleanPath(b.activeFolderPath()) == QDir::cleanPath(pcl2), "活动路径=导入文件夹");
    b.refreshFolders();
    CHECK(waitFolders(b), "刷新完成");
    CHECK(b.folders()[0].toMap()["active"].toBool() == false, "固有非活动");
    CHECK(b.folders()[1].toMap()["active"].toBool() == true, "导入项=活动(使用中)");

    fprintf(stderr, "\n[5] 删除活动条目（回退默认 + 不删文件夹）\n");
    bool reverted = false;
    QObject::connect(&b, &MinecraftFolderBackend::revertRequested,
                     [&]() { reverted = true; });
    CHECK(b.removeGameFolder(pcl2), "removeGameFolder 成功");
    CHECK(reverted, "已发回退请求");
    CHECK(QDir(pcl2).exists(), "文件夹本体未被删除");
    CHECK(b.activeFolderPath() == b.defaultFolderPath(), "活动回退默认");
    b.refreshFolders();
    CHECK(waitFolders(b), "刷新完成");
    CHECK(b.folders().size() == 1, "条目已移除（仅剩固有）");

    fprintf(stderr, "\n[6] 固有文件夹不可删除\n");
    CHECK(b.removeGameFolder(b.defaultFolderPath()) == false, "拒绝删除固有");
    b.refreshFolders();
    CHECK(waitFolders(b), "刷新完成");
    CHECK(b.folders().size() == 1, "固有仍在");

    fprintf(stderr, "\n[7] 持久化（重新构造实例读取注册表）\n");
    {
        MinecraftFolderBackend b2;
        b2.setDataDir(dataDir);
        b2.addGameFolder(pcl2, "PCL2 本体");
        CHECK(waitFolders(b2), "刷新完成");
        CHECK(b2.folders().size() == 2, "重启后仍识别导入项");
        CHECK(b2.folders()[1].toMap()["name"].toString() == "PCL2 本体", "命名持久化");
    }

#undef CHECK

    QDir(base).removeRecursively();
    fprintf(stderr, "\n=== %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
