// SPDX-License-Identifier: AGPL-3.0-or-later
// WebView2 smoke test — isolates WebView2Window from the full launcher.
// Loads bing.com, waits for loadFinished (60s timeout), saves a screenshot,
// exits 0 on success / 1 on env error / 2 on timeout.
#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <QDateTime>

#include "../src/utils/logger.h"
#include "../src/core/webview2_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    ShadowLauncher::initLogger(QCoreApplication::applicationDirPath());

    ShadowLauncher::WebView2Window w;
    w.setWindowTitle(QStringLiteral("WebView2 Smoke Test"));
    w.resize(480, 650);
    // Pin runtime version 59 (in use by working apps); try 72 if 59 fails.
    w.setBrowserExecutableFolder(
        QStringLiteral(R"(C:\Program Files (x86)\Microsoft\EdgeWebView\Application\151.0.4129.59)"));
    w.show();

    QElapsedTimer timer;
    timer.start();

    int result = 2; // default: timeout
    QObject::connect(&w, &ShadowLauncher::WebView2Window::environmentError,
                     [&](const QString& msg) {
        qInfo() << "[smoke] ENV ERROR:" << msg;
        result = 1;
        QTimer::singleShot(300, &app, &QCoreApplication::quit);
    });
    QObject::connect(&w, &ShadowLauncher::WebView2Window::loadFinished,
                     [&](bool ok) {
        qInfo() << "[smoke] LOAD FINISHED ok=" << ok << "elapsed=" << timer.elapsed() << "ms"
                << "url=" << w.currentUrl();
        w.grab().save(QStringLiteral("webview2_smoke.png"));
        qInfo() << "[smoke] screenshot saved";
        result = ok ? 0 : 1;
        QTimer::singleShot(300, &app, &QCoreApplication::quit);
    });

    qInfo() << "[smoke] navigating to bing.com...";
    w.loadUrl(QUrl(QStringLiteral("https://www.bing.com")));

    QTimer::singleShot(60000, &app, [&]() {
        qInfo() << "[smoke] TIMEOUT 60s url=" << w.currentUrl();
        w.grab().save(QStringLiteral("webview2_smoke.png"));
        result = 2;
        app.quit();
    });

    const int rc = app.exec();
    qInfo() << "[smoke] exit code:" << result << "(0=ok 1=err 2=timeout)";
    return result;
}
