// Shadow Launcher — C++ Entry Point
// Phase 1: Minimal QML bootstrap (no backend yet)

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>

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

    // QML engine
    QQmlApplicationEngine engine;

    // Expose data directory to QML
    engine.rootContext()->setContextProperty("dataDir", dataDir);

    // Load main QML
    const QUrl url(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml/Phase1Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
