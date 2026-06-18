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
#include <QElapsedTimer>
#include <QTimer>
#include <QThread>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "utils/logger.h"
#include "backend/shadow_backend.h"

using namespace ShadowLauncher;

// ── Native event filter: handle taskbar minimize/restore for frameless window ──
class TaskbarMinimizeFilter : public QAbstractNativeEventFilter {
public:
    QWindow* targetWindow = nullptr;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) override {
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
            MSG* msg = static_cast<MSG*>(message);

            // ── Minimize: click title bar minimize button or custom window menu ──
            if (msg->message == WM_SYSCOMMAND && (msg->wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(msg->hwnd, SW_MINIMIZE);
                return true;
            }

            // ── Also handle system restore (taskbar thumbnail preview context menu) ──
            if (msg->message == WM_SYSCOMMAND && (msg->wParam & 0xFFF0) == SC_RESTORE) {
                if (targetWindow) {
                    targetWindow->showNormal();
                    targetWindow->raise();
                    targetWindow->requestActivate();
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
    QElapsedTimer startupTimer;
    startupTimer.start();
    auto checkpoint = [&](const QString& stage) {
        qCInfo(logApp) << "[STARTUP +" << startupTimer.elapsed() << "ms] " << stage
                       << "(thread:" << QThread::currentThreadId() << ")";
    };

    // Qt attribute setup
    checkpoint(QStringLiteral("Setting Qt attributes..."));
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);
    checkpoint(QStringLiteral("QGuiApplication constructed"));

    app.setApplicationName("Shadow Launcher");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ShadowTeam");
    app.setOrganizationDomain("shadowteam.dev");

    // ── Initialise structured logging ──
    initLogger(QCoreApplication::applicationDirPath());
    checkpoint(QStringLiteral("Logger initialized"));
    qCInfo(logApp) << "=== Shadow Launcher v" << app.applicationVersion() << "starting ==="
                   << "PID:" << QCoreApplication::applicationPid();

    // Data directory
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    checkpoint(QStringLiteral("Data directory created"));

    // Create unified backend — owns all 7 sub-backends
    ShadowBackend* backend = new ShadowBackend(&app);
    checkpoint(QStringLiteral("ShadowBackend constructed"));

    // QML engine
    QQmlApplicationEngine engine;
    // Add exe dir to import path so module can find qmldir
    engine.addImportPath(QCoreApplication::applicationDirPath());
    checkpoint(QStringLiteral("QML engine created"));

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

    // Load main QML from embedded resources (precompiled via qt_add_qml_module)
    checkpoint(QStringLiteral("Loading QML (precompiled qrc)..."));
    QUrl url(QStringLiteral("qrc:/ShadowLauncher/qml/MainWindow.qml"));
    engine.load(url);
    checkpoint(QStringLiteral("QML engine.load() completed"));

    if (engine.rootObjects().isEmpty()) {
        qCCritical(logApp) << "Failed to load any QML root objects — exiting";
        return -1;
    }
    checkpoint(QStringLiteral("Root objects verified"));

    qCInfo(logApp) << "Event loop started"
                   << "— total startup:" << startupTimer.elapsed() << "ms";

    // --auto-launch <version> command-line test mode
    QStringList args = app.arguments();
    int autoIdx = args.indexOf(QStringLiteral("--auto-launch"));
    if (autoIdx >= 0 && autoIdx + 1 < args.size()) {
        QString autoVersion = args[autoIdx + 1];
        qCInfo(logApp) << "Auto-launch test mode: version" << autoVersion;
        QTimer::singleShot(2000, backend, [backend, autoVersion]() {
            qCInfo(logApp) << "Auto-launch: triggering launch for" << autoVersion;
            backend->launch(autoVersion);
        });
    }

    return app.exec();
}
