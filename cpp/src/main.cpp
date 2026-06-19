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
#include <QScreen>
#include <QPixmap>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "utils/logger.h"
#include "backend/shadow_backend.h"
#include "core/http_client.h"

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

// Global for screenshot mode
static QWindow* screenshotWindow = nullptr;

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

    // Modrinth API: uniquely-identifying User-Agent header (mandatory)
    HttpClient::instance().setUserAgent(
        QStringLiteral("xiaole1173/ShadowLauncher/1.0.0"
                       " (https://github.com/xiaole1173/shadow-launcher)"));

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
                screenshotWindow = win;
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

    // ── Auto-launch test mode ──
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

    // ── Screenshot test mode ──
    int ssIdx = args.indexOf(QStringLiteral("--screenshot"));
    if (ssIdx >= 0 && ssIdx + 1 < args.size()) {
        QString ssName = args[ssIdx + 1];
        QString ssPath = QCoreApplication::applicationDirPath() + "/" + ssName + ".png";
        qCInfo(logApp) << "Screenshot mode: saving to" << ssPath;
        QTimer::singleShot(3000, &app, [ssPath]() {
            if (screenshotWindow && screenshotWindow->isVisible()) {
                QScreen* screen = QGuiApplication::primaryScreen();
                QPixmap pix = screen->grabWindow(screenshotWindow->winId());
                if (pix.save(ssPath, "PNG")) {
                    qCInfo(logApp) << "Screenshot saved:" << ssPath
                                   << "size:" << pix.width() << "x" << pix.height();
                } else {
                    qCCritical(logApp) << "Screenshot save FAILED:" << ssPath;
                }
            } else {
                qCCritical(logApp) << "Screenshot: window not ready";
            }
            qApp->quit();
        });
    }

    // ── Auto-download test mode (for testing download + fallback) ──
    int dlIdx = args.indexOf(QStringLiteral("--auto-download"));
    if (dlIdx >= 0 && dlIdx + 1 < args.size()) {
        QString dlVersion = args[dlIdx + 1];
        int sourceIdx = (dlIdx + 2 < args.size())
            ? args[dlIdx + 2].toInt()
            : 0;  // default: BMCLAPI
        qCInfo(logApp) << "Auto-download test mode: version" << dlVersion
                       << "sourceIndex" << sourceIdx;
        QTimer::singleShot(5000, backend, [backend, dlVersion, sourceIdx]() {
            qCInfo(logApp) << "Auto-download: triggering installVersion for" << dlVersion;
            backend->installVersion(dlVersion, sourceIdx);
        });
    }

    return app.exec();
}
