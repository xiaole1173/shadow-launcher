// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// 夸父（FileDownloader）单文件下载测试：用法 FDTest <url> [expectedSize] [outPath] [threads] [modpackMode]
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <cstdio>

#include "core/file_downloader.h"
#include "utils/logger.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());

    const QString url = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral("http://127.0.0.1:8801/slow_20mb.bin");
    const qint64 expectedSize = argc > 2 ? QString::fromUtf8(argv[2]).toLongLong() : 0;
    const QString outPath = argc > 3 ? QString::fromUtf8(argv[3]) : QStringLiteral("fd_test.bin");
    const int threads = argc > 4 ? QString::fromUtf8(argv[4]).toInt() : 16;
    const bool modpackMode = argc > 5 && QString::fromUtf8(argv[5]) == "1";
    QFile::remove(outPath);

    auto* fd = new ShadowDownloader::FileDownloader(&app);
    fd->setModpackMode(modpackMode);
    fd->setMaxThreads(threads);
    fd->addFile(outPath, QFileInfo(outPath).fileName(),
                QStringList() << url, expectedSize, QByteArray());

    QElapsedTimer timer;
    timer.start();

    QObject::connect(fd, &ShadowDownloader::FileDownloader::fileProgress, &app,
        [](const QString&, const QString&, qint64 received, qint64 total, const QString&) {
            static qint64 last = 0;
            if (received - last >= 5 * 1024 * 1024 || (total > 0 && received >= total)) {
                last = received;
                printf("  progress: %lld / %lld (%.0f%%)\n", received, total,
                       total > 0 ? 100.0 * received / total : 0.0);
                fflush(stdout);
            }
        });
    QObject::connect(fd, &ShadowDownloader::FileDownloader::fileFinished, &app,
        [&](const QString& path, bool ok) {
            const qint64 ms = timer.elapsed();
            const qint64 size = QFileInfo(path).size();
            printf("RESULT: ok=%d size=%.1fMB time=%.1fs avg=%.0f KB/s\n",
                   ok, size / 1024.0 / 1024.0, ms / 1000.0,
                   ms > 0 ? size * 1000.0 / 1024.0 / ms : 0.0);
            fflush(stdout);
            app.exit(ok ? 0 : 1);
        });
    QObject::connect(fd, &ShadowDownloader::FileDownloader::logMessage, &app,
        [](const QString& msg) { printf("  [FD] %s\n", msg.toUtf8().constData()); fflush(stdout); });

    QTimer::singleShot(240000, &app, []() {
        printf("TIMEOUT after 240s\n");
        fflush(stdout);
        QCoreApplication::exit(2);
    });

    fd->start();
    return app.exec();
}
