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

#include "utils/logger.h"
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
                ShowWindow(msg->hwnd, SW_MINIMIZE);
                return true;
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

    // ── Initialise structured logging ──
    initLogger(QCoreApplication::applicationDirPath());
    qCInfo(logApp) << "=== Shadow Launcher v" << app.applicationVersion() << "starting ==="
                   << "PID:" << QCoreApplication::applicationPid();

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

    // Load main QML from filesystem
    QString qmlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml/MainWindow.qml");
    QUrl url;
    if (QFile::exists(qmlPath)) {
        url = QUrl::fromLocalFile(qmlPath);
        qCInfo(logApp) << "QML loaded from filesystem:" << qmlPath;
    } else {
        url = QUrl(QStringLiteral("qrc:/ShadowLauncher/qml/MainWindow.qml"));
        qCInfo(logApp) << "QML loaded from resources";
    }
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCCritical(logApp) << "Failed to load any QML root objects — exiting";
        return -1;
    }

    qCInfo(logApp) << "Event loop started";
    return app.exec();
}
