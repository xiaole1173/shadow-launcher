// Java 下载速度测试 - Adoptium API 稳定链接
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QElapsedTimer>
#include <iostream>

#include "core/file_downloader.h"

using namespace ShadowDownloader;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    struct JavaInfo { QString name; QString url; qint64 size; };

    // Tsinghua TUNA Adoptium mirror (direct, no redirect, supports Range)
    QList<JavaInfo> javas = {
        {"Java 8",  "https://mirrors.tuna.tsinghua.edu.cn/Adoptium/8/jdk/x64/windows/OpenJDK8U-jdk_x64_windows_hotspot_8u492b09.zip",  106462632},
        {"Java 21", "https://mirrors.tuna.tsinghua.edu.cn/Adoptium/21/jdk/x64/windows/OpenJDK21U-jdk_x64_windows_hotspot_21.0.11_10.zip", 205073954},
        {"Java 25", "https://mirrors.tuna.tsinghua.edu.cn/Adoptium/25/jdk/x64/windows/OpenJDK25U-jdk_x64_windows_hotspot_25.0.3_9.zip", 141131903},
    };

    // Test all three
    const auto& java = javas[2]; // Start with Java 25 (smallest)

    QString outDir = QCoreApplication::applicationDirPath() + "/java_dl_test";
    QDir().mkpath(outDir);

    auto* dl = new FileDownloader(&app);
    dl->setMaxThreads(64);

    QElapsedTimer totalTimer;
    totalTimer.start();

    QObject::connect(dl, &FileDownloader::progressChanged,
        [dl](int done, int total, qint64 dlBytes, qint64 totalBytes) {
            double mb = dlBytes / (1024.0 * 1024.0);
            double speed = dl->currentSpeedMBps();
            int threads = dl->activeThreads();
            if (totalBytes > 0) {
                int pct = (int)(dlBytes * 100 / totalBytes);
                std::cout << "\r[" << pct << "%] " << QString::number(mb,'f',1).toStdString() << "MB"
                          << " | " << QString::number(speed,'f',1).toStdString() << " MB/s"
                          << " | " << threads << "t" << std::flush;
            } else {
                std::cout << "\r[?%] " << QString::number(mb,'f',1).toStdString() << "MB"
                          << " | " << QString::number(speed,'f',1).toStdString() << " MB/s"
                          << " | " << threads << "t" << std::flush;
            }
        });

    QObject::connect(dl, &FileDownloader::fileFinished,
        [&totalTimer](const QString& path, bool ok) {
            double secs = totalTimer.elapsed() / 1000.0;
            if (ok)
                std::cout << "\nOK " << path.toStdString() << " (" << secs << "s)" << std::endl;
            else
                std::cout << "\nFAIL " << path.toStdString() << std::endl;
        });

    QObject::connect(dl, &FileDownloader::allFinished, [&app, dl, &totalTimer]() {
        double secs = totalTimer.elapsed() / 1000.0;
        qint64 totalMB = dl->totalBytes() / (1024*1024);
        double avgMBps = totalMB / secs;
        std::cout << "=== Done: " << dl->completedFiles() << "/" << dl->totalFiles()
                  << " | " << secs << "s"
                  << " | avg " << QString::number(avgMBps,'f',1).toStdString() << " MB/s" << std::endl;
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    });

    QObject::connect(dl, &FileDownloader::logMessage,
        [](const QString& msg) { std::cout << "\n[LOG] " << msg.toStdString() << std::endl; });

    QString safeName = java.name;
    safeName.replace(" ", "_");
    QString outPath = outDir + "/" + safeName + ".zip";
    QFile::remove(outPath);

    dl->addFile(outPath, java.name, QStringList{java.url}, java.size, QByteArray());
    dl->start();

    std::cout << "Downloading " << java.name.toStdString()
              << " | threads=" << dl->maxThreads() << std::endl;

    return app.exec();
}
