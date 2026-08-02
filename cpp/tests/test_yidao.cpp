// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// 驿道 v2 集成测试：实测分片下载（用法: YidaoTest <url> [expectedSize] [outPath]）
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QFileInfo>
#include <cstdio>

#include "core/http_client.h"
#include "utils/logger.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ShadowLauncher::installFileLogger(QCoreApplication::applicationDirPath());

    QString url = argc > 1
        ? QString::fromUtf8(argv[1])
        : QStringLiteral("https://cdn.modrinth.com/data/4BV47HRn/versions/bdRIjRgs/"
                         "Better%20MC%20%5BFORGE%5D%201.20.1%201.20.1%20v59.mrpack");
    qint64 expectedSize = argc > 2 ? QString::fromUtf8(argv[2]).toLongLong() : 0;
    QString outPath = argc > 3 ? QString::fromUtf8(argv[3]) : QStringLiteral("yidao_test.bin");
    QFile::remove(outPath);

    QElapsedTimer timer;
    timer.start();

    const bool abortTest = QString::fromUtf8(argc > 4 ? argv[4] : "") == QStringLiteral("abort");

    ShadowLauncher::HttpClient::DownloadHandle* handle = nullptr;
    if (abortTest) {
        // 取消路径测试：1s 后 abort，期望 done(false, 已取消/网络错误) 且不崩
        handle = ShadowLauncher::HttpClient::instance().downloadWithReply(
            url, outPath,
            [](qint64 received, qint64 total) {
                if (received % (4 * 1024 * 1024) < 65536)
                    printf("  progress: %lld / %lld\n", received, total);
            },
            [&](bool ok, const QString& err) {
                const qint64 ms = timer.elapsed();
                printf("ABORT-RESULT: ok=%d err=%s time=%.1fs\n",
                       ok, err.toUtf8().constData(), ms / 1000.0);
                fflush(stdout);
                app.exit(0);
            });
        QTimer::singleShot(1000, &app, [handle]() {
            printf("  -> aborting\n");
            fflush(stdout);
            if (handle) handle->abort();
        });
    } else {
        ShadowLauncher::HttpClient::instance().download(
            url, outPath,
            [](qint64 received, qint64 total) {
                static qint64 last = 0;
                if (received - last >= 5 * 1024 * 1024 || (total > 0 && received >= total)) {
                    last = received;
                    printf("  progress: %lld / %lld (%.0f%%)\n",
                           received, total,
                           total > 0 ? 100.0 * received / total : 0.0);
                    fflush(stdout);
                }
            },
            [&](bool ok, const QString& err) {
                const qint64 ms = timer.elapsed();
                const qint64 size = QFileInfo(outPath).size();
                const double mb = size / 1024.0 / 1024.0;
                printf("RESULT: ok=%d err=%s size=%.1fMB time=%.1fs avg=%.0f KB/s\n",
                       ok, err.toUtf8().constData(), mb, ms / 1000.0,
                       ms > 0 ? size * 1000.0 / 1024.0 / ms : 0.0);
                fflush(stdout);
                app.exit(ok ? 0 : 1);
            },
            expectedSize, {});
    }

    QTimer::singleShot(240000, &app, []() {
        printf("TIMEOUT after 240s\n");
        fflush(stdout);
        QCoreApplication::exit(2);
    });

    return app.exec();
}
