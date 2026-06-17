// Shadow Launcher — C++ Entry Point
// Phase 4: Unified backend + QML UI port

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>

#include "backend/shadow_backend.h"

using namespace ShadowLauncher;

int main(int argc, char *argv[])
{
    // Qt attribute setup
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    app.setApplicationName("Shadow Launcher");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ShadowTeam");

    // Data directory
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // Create unified backend — owns all 7 sub-backends
    ShadowBackend* backend = new ShadowBackend(&app);

    // QML engine
    QQmlApplicationEngine engine;

    // Expose backend and data directory to QML
    engine.rootContext()->setContextProperty("backend", backend);
    engine.rootContext()->setContextProperty("dataDir", dataDir);

    // Load main QML (filesystem for packaged, fallback to qrc for dev)
    QString qmlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml/MainWindow.qml");
    QUrl url;
    if (QFile::exists(qmlPath)) {
        url = QUrl::fromLocalFile(qmlPath);
    } else {
        url = QUrl(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml/MainWindow.qml"));
    }
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
