// SPDX-License-Identifier: AGPL-3.0-or-later
// 整合包导出测试：ModpackExportTest <gameDir> <versionId> [outPath] [format]
// 导出后校验包结构（mrpack: modrinth.index.json + files[]/overrides；
// zip: manifest.json + overrides），并用 ZipArchive 读回验证可解析。
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <cstdio>

#include "core/modpack/modpack_exporter.h"
#include "core/modpack/zip_archive.h"
#include "utils/logger.h"

using namespace ShadowLauncher;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());

    if (argc < 3) {
        printf("usage: ModpackExportTest <gameDir> <versionId> [outPath] [format=0]\n");
        return 2;
    }
    const QString gameDir = QString::fromUtf8(argv[1]);
    const QString versionId = QString::fromUtf8(argv[2]);
    const int format = argc > 4 ? QString::fromUtf8(argv[4]).toInt() : 0;
    const QString suffix = format == 1 ? QStringLiteral(".zip") : QStringLiteral(".mrpack");
    const QString outPath = argc > 3 ? QString::fromUtf8(argv[3])
                                     : gameDir + QStringLiteral("/_export_test_") + versionId + suffix;
    QFile::remove(outPath);

    auto* exporter = new ModpackExporter(&app);
    exporter->setGameDir(gameDir);

    // 测试自动继续联网失败（真实 UI 有 ConfirmDialog 询问；测试直接降级直装）
    QObject::connect(exporter, &ModpackExporter::lookupFailed, &app,
        [exporter](int, const QString&) { exporter->continueAfterLookupFailure(true); });

    QObject::connect(exporter, &ModpackExporter::finished, &app,
        [&](bool ok, const QString& out, const QString& err) {
            if (!ok) {
                printf("RESULT: fail err=%s\n", err.toUtf8().constData());
                app.exit(1);
                return;
            }
            ZipArchive zip;
            if (!zip.open(out)) {
                printf("RESULT: fail 无法打开导出包: %s\n", zip.error().toUtf8().constData());
                app.exit(1);
                return;
            }
            const QStringList entries = zip.listEntries();
            int modsInOverrides = 0, overridesOther = 0, hostedInManifest = 0;
            QString game, deps;
            if (format == 1) {
                const QByteArray mf = zip.readEntry("manifest.json", 4 * 1024 * 1024);
                const QJsonObject manifest = QJsonDocument::fromJson(mf).object();
                game = manifest.value("minecraft").toObject().value("version").toString();
                hostedInManifest = manifest.value("files").toArray().size();
            } else {
                const QByteArray idx = zip.readEntry("modrinth.index.json", 4 * 1024 * 1024);
                const QJsonObject index = QJsonDocument::fromJson(idx).object();
                game = index.value("game").toString();
                deps = QString::fromLatin1(QJsonDocument(
                    index.value("dependencies").toObject()).toJson(QJsonDocument::Compact));
                hostedInManifest = index.value("files").toArray().size();
            }
            for (const auto& e : entries) {
                if (e.startsWith("overrides/mods/")) modsInOverrides++;
                else if (e.startsWith("overrides/")) overridesOther++;
            }
            printf("RESULT: ok fmt=%d game=%s deps=%s hostedInManifest=%d modsInOverrides=%d overridesOther=%d size=%.2fMB path=%s\n",
                   format, game.toUtf8().constData(), deps.toUtf8().constData(),
                   hostedInManifest, modsInOverrides, overridesOther,
                   QFileInfo(out).size() / 1048576.0, out.toUtf8().constData());
            zip.close();
            app.exit(0);
        });

    // 勾选全部存档测试 saves 子项路径
    QVariantList saves;
    const QDir savesDir(gameDir + QStringLiteral("/saves"));
    if (savesDir.exists()) {
        const auto list = savesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const auto& s : list) saves.append(s);
    }
    // 勾选选项：与导出界面默认一致（游戏本体设置/协议/资源包/光影/存档/Mod 设置等）
    QVariantList checked;
    const auto& defs = ModpackExporter::optionDefs();
    for (const auto& d : defs) {
        if (d.defaultChecked) checked.append(d.id);
    }
    checked.append(QStringLiteral("saves"));
    exporter->exportVersion(versionId, versionId + "-export-test", QStringLiteral("1.0.0"),
                            checked, saves, true /*modrinthOnly: 跳过 CF*/, true /*hostedAssetsOnly: 纯本地规则收集测试*/,
                            false, format, outPath);
    return app.exec();
}
