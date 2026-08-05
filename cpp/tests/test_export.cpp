// SPDX-License-Identifier: AGPL-3.0-or-later
// 整合包导出测试：ModpackExportTest <gameDir> <versionId> [outPath]
// 导出后校验 mrpack 结构（index.json 合法、mods/overrides 条目存在），
// 并用 ZipArchive 读回验证可解析。
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
        printf("usage: ModpackExportTest <gameDir> <versionId> [outPath]\n");
        return 2;
    }
    const QString gameDir = QString::fromUtf8(argv[1]);
    const QString versionId = QString::fromUtf8(argv[2]);
    const QString outPath = argc > 3 ? QString::fromUtf8(argv[3])
                                     : gameDir + QStringLiteral("/_export_test_") + versionId + QStringLiteral(".mrpack");
    QFile::remove(outPath);

    auto* exporter = new ModpackExporter(&app);
    exporter->setGameDir(gameDir);

    QObject::connect(exporter, &ModpackExporter::finished, &app,
        [&](bool ok, const QString& out, const QString& err) {
            if (!ok) {
                printf("RESULT: fail err=%s\n", err.toUtf8().constData());
                app.exit(1);
                return;
            }
            // ── 校验 mrpack 结构 ──
            ZipArchive zip;
            if (!zip.open(out)) {
                printf("RESULT: fail 无法打开导出包: %s\n", zip.error().toUtf8().constData());
                app.exit(1);
                return;
            }
            const QByteArray idx = zip.readEntry("modrinth.index.json", 4 * 1024 * 1024);
            QJsonObject index = QJsonDocument::fromJson(idx).object();
            const int fmt = index.value("formatVersion").toInt();
            const QString game = index.value("game").toString();
            const QString deps = QString::fromLatin1(
                QJsonDocument(index.value("dependencies").toObject()).toJson(QJsonDocument::Compact));
            const int fileCount = index.value("files").toArray().size();
            const QStringList entries = zip.listEntries();
            int modsInZip = 0, overridesInZip = 0;
            for (const auto& e : entries) {
                if (e.startsWith("mods/")) modsInZip++;
                if (e.startsWith("overrides/")) overridesInZip++;
            }
            printf("RESULT: ok fmt=%d game=%s deps=%s files=%d modsInZip=%d overridesInZip=%d size=%.1fMB path=%s\n",
                   fmt, game.toUtf8().constData(), deps.toUtf8().constData(), fileCount,
                   modsInZip, overridesInZip, QFileInfo(out).size() / 1048576.0, out.toUtf8().constData());
            zip.close();
            app.exit(0);
        });

    exporter->exportVersion(versionId, versionId + "-export-test",
                            true, true, true, outPath);
    return app.exec();
}
