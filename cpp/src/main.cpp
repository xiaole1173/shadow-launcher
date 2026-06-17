// Shadow Launcher — C++ Entry Point
// Phase 4: Unified backend + QML UI port

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QWindow>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "backend/shadow_backend.h"

using namespace ShadowLauncher;

// ── Native event filter: handle taskbar minimize for frameless window ──
class TaskbarMinimizeFilter : public QAbstractNativeEventFilter {
public:
    QWindow* targetWindow = nullptr;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) override {
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
            MSG* msg = static_cast<MSG*>(message);
            if (msg->message == WM_SYSCOMMAND && (msg->wParam & 0xFFF0) == SC_MINIMIZE) {
                if (targetWindow) {
                    targetWindow->showMinimized();
                    return true;
                }
            }
        }
#endif
        Q_UNUSED(eventType)
        return false;
    }
};

int main(int argc, char *argv[])
{
    // Qt attribute setup
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    app.setApplicationName("Shadow Launcher");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ShadowTeam");
    app.setOrganizationDomain("shadowteam.dev");

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

    // ── Taskbar minimize: install native event filter ──
    TaskbarMinimizeFilter* taskbarFilter = new TaskbarMinimizeFilter();
    app.installNativeEventFilter(taskbarFilter);

    // Set target window once QML creates it
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [taskbarFilter](QObject* obj, const QUrl&) {
            if (QWindow* win = qobject_cast<QWindow*>(obj)) {
                taskbarFilter->targetWindow = win;
            }
        }, Qt::QueuedConnection);

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

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
