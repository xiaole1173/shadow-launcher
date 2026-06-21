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
#include <QQuickWindow>
#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <QScreen>
#include <QPixmap>
#include <QWindow>
#include <memory>

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

    // ── SceneGraph optimizations (see scene-graph startup doc) ──
    // 1. Shader cache: avoid recompiling GLSL/SPIR-V every launch
    qputenv("QT_QUICK_SHADER_CACHE", qPrintable(QCoreApplication::applicationDirPath() + QStringLiteral("/shader_cache")));
    // 2. Fixed render backend: skip GPU enumeration delay
    qputenv("QSG_RHI_BACKEND", "opengl");

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
    engine.addImportPath(QStringLiteral("qrc:/ShadowLauncher/qml"));
    checkpoint(QStringLiteral("QML engine created"));

    // Expose backend and data directory to QML
    engine.rootContext()->setContextProperty("backend", backend);
    engine.rootContext()->setContextProperty("dataDir", dataDir);

    // ── Taskbar minimize: install native event filter ──
    TaskbarMinimizeFilter* taskbarFilter = new TaskbarMinimizeFilter();
    app.installNativeEventFilter(taskbarFilter);

    // Set target window + measure first frame render
    QElapsedTimer* pStartupTimer = &startupTimer;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [taskbarFilter, pStartupTimer](QObject* obj, const QUrl&) {
            QQuickWindow* win = qobject_cast<QQuickWindow*>(obj);
            if (!win) return;
            taskbarFilter->targetWindow = win;
            screenshotWindow = win;
            // Measure first frame timing breakdown
            QObject::connect(win, &QQuickWindow::beforeSynchronizing,
                win, [pStartupTimer]() {
                    static bool firstBeforeSync = true;
                    if (firstBeforeSync) {
                        firstBeforeSync = false;
                        qCInfo(logApp) << "[STARTUP +" << pStartupTimer->elapsed() << "ms] beforeSynchronizing (1st)";
                    }
                });
            QObject::connect(win, &QQuickWindow::afterSynchronizing,
                win, [pStartupTimer]() {
                    static bool firstAfterSync = true;
                    if (firstAfterSync) {
                        firstAfterSync = false;
                        qCInfo(logApp) << "[STARTUP +" << pStartupTimer->elapsed() << "ms] afterSynchronizing (1st)";
                    }
                });
            QObject::connect(win, &QQuickWindow::frameSwapped,
                win, [pStartupTimer]() {
                    qCInfo(logApp) << "[STARTUP +" << pStartupTimer->elapsed() << "ms] First frame rendered (GPU)";
                }, Qt::SingleShotConnection);
        }, Qt::QueuedConnection);

    // Load QML — dev mode (file://) when SHADOW_DEV=1, otherwise qrc precompiled
    bool devMode = qEnvironmentVariableIsSet("SHADOW_DEV");
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    engine.addImportPath(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml"));

    QUrl url;
    if (devMode) {
        QString devPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml/MainWindow.qml");
        if (QFile::exists(devPath)) {
            checkpoint(QStringLiteral("Loading QML (filesystem dev mode)..."));
            url = QUrl::fromLocalFile(devPath);
        } else {
            qCWarning(logApp) << "SHADOW_DEV set but" << devPath << "not found — falling back to qrc";
            url = QUrl(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml/MainWindow.qml"));
        }
    } else {
        checkpoint(QStringLiteral("Loading QML (precompiled qrc)..."));
        url = QUrl(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml/MainWindow.qml"));
    }
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
            backend->launch(autoVersion, true);
        });
    }

    // ── Auto-install test mode (trigger download click programmatically) ──
    int autoInstallIdx = args.indexOf(QStringLiteral("--auto-install"));
    if (autoInstallIdx >= 0 && autoInstallIdx + 1 < args.size()) {
        QString autoVersion = args[autoInstallIdx + 1];
        qCInfo(logApp) << "Auto-install test mode: version" << autoVersion;
        QTimer::singleShot(1500, backend, [backend, autoVersion]() {
            qCInfo(logApp) << "Auto-install: triggering install for" << autoVersion;
            backend->installVersion(autoVersion);
        });
    }

    // ── Test download mode: navigate to download page + auto-install ──
    int testDownloadIdx = args.indexOf(QStringLiteral("--test-download"));
    if (testDownloadIdx >= 0 && testDownloadIdx + 1 < args.size()) {
        QString testVersion = args[testDownloadIdx + 1];
        qCInfo(logApp) << "Test download mode: version" << testVersion;
        // Navigate to download page after QML is ready
        QTimer::singleShot(2500, backend, [&engine, backend, testVersion]() {
            qCInfo(logApp) << "Test-download: navigating to download page";
            if (!engine.rootObjects().isEmpty()) {
                engine.rootObjects().first()->setProperty("navListIndex", 1);
            }
            // Trigger install 800ms after navigation (allow page to render)
            QTimer::singleShot(800, backend, [backend, testVersion]() {
                qCInfo(logApp) << "Test-download: triggering install for" << testVersion;
                backend->installVersion(testVersion);
            });
        });
    }

    // ── Navigate + Screenshot test mode ──
    // Smart timing: wait for content-ready signal, not fixed delay
    // Usage: --screenshot <name> [--navigate <page>:<tab>]
    int ssIdx = args.indexOf(QStringLiteral("--screenshot"));
    if (ssIdx >= 0 && ssIdx + 1 < args.size()) {
        QString ssName = args[ssIdx + 1];
        QString ssPath = QCoreApplication::applicationDirPath() + "/" + ssName + ".png";
        qCInfo(logApp) << "Screenshot mode: saving to" << ssPath;

        // Parse --navigate argument
        int navIdx = args.indexOf(QStringLiteral("--navigate"));
        int targetPage = -1, targetTab = -1;
        if (navIdx >= 0 && navIdx + 1 < args.size()) {
            QString navArg = args[navIdx + 1];
            QStringList parts = navArg.split(':');
            if (parts.size() >= 1) {
                QString pageStr = parts[0].toLower();
                if (pageStr == "launch" || pageStr == "0") targetPage = 0;
                else if (pageStr == "download" || pageStr == "1") targetPage = 1;
                else if (pageStr == "settings" || pageStr == "2") targetPage = 2;
            }
            if (parts.size() >= 2) {
                QString tabStr = parts[1].toLower();
                if (tabStr == "mc" || tabStr == "0") targetTab = 0;
                else if (tabStr == "mod" || tabStr == "1") targetTab = 1;
                else if (tabStr == "shader" || tabStr == "2") targetTab = 2;
                else if (tabStr == "rp" || tabStr == "3") targetTab = 3;
            }
        }

        // Parse --detail argument (force-open RP detail page)
        int detailIdx = args.indexOf(QStringLiteral("--detail"));
        QString detailSlug;
        if (detailIdx >= 0 && detailIdx + 1 < args.size()) {
            detailSlug = args[detailIdx + 1];
            targetPage = 1;  // force download page
            if (targetTab < 0) targetTab = 3;  // default to RP tab
        }

        // Parse --detail-expand <major> (expand a group in detail page)
        int expandIdx = args.indexOf(QStringLiteral("--detail-expand"));
        QString expandMajor;
        if (expandIdx >= 0 && expandIdx + 1 < args.size()) {
            expandMajor = args[expandIdx + 1];
        }

        // Parse --toggle-pre-release <on|off>
        int toggleIdx = args.indexOf(QStringLiteral("--toggle-pre-release"));
        bool togglePreRelease = false, hasToggle = false;
        if (toggleIdx >= 0 && toggleIdx + 1 < args.size()) {
            togglePreRelease = (args[toggleIdx + 1].toLower() == QStringLiteral("on"));
            hasToggle = true;
            targetPage = 1; if (targetTab < 0) targetTab = 3;
        }

        // Parse --open-version-menu
        bool openVersionMenu = args.contains(QStringLiteral("--open-version-menu"));
        if (openVersionMenu) {
            targetPage = 1; if (targetTab < 0) targetTab = 3;
        }

        // Shared ready flag (heap-allocated so lambdas share ownership)
        auto ready = std::make_shared<bool>(false);
        auto doScreenshot = [ssPath, &app]() {
            // Use QQuickWindow::grabWindow() — renders window content off-screen
            // (QScreen::grabWindow captures screen compositor + overlapping windows on Windows)
            auto* qw = qobject_cast<QQuickWindow*>(screenshotWindow);
            if (qw && qw->isVisible()) {
                QImage img = qw->grabWindow();
                QPixmap pix = QPixmap::fromImage(img);
                if (pix.save(ssPath, "PNG")) {
                    qCInfo(logApp) << "Screenshot saved:" << ssPath
                                   << "size:" << pix.width() << "x" << pix.height();
                } else {
                    qCCritical(logApp) << "Screenshot save FAILED:" << ssPath;
                }
            } else {
                qCCritical(logApp) << "Screenshot: window not ready";
            }
            // Give async icon downloads 10s to finish before quitting
            qCInfo(logApp) << "Screenshot: waiting 10s for icon downloads before quit";
            QTimer::singleShot(10000, qApp, &QCoreApplication::quit);
        };

        if (targetPage >= 0) {
            qCInfo(logApp) << "Screenshot: navigating to page" << targetPage
                           << "tab" << targetTab;

            // Navigate after QML is ready
            QTimer::singleShot(1500, backend, [backend, targetPage, targetTab, 
                    detailSlug, expandMajor, togglePreRelease, hasToggle, openVersionMenu]() {
                emit backend->navigateToRequested(targetPage, targetTab);
                
                // --toggle-pre-release <on|off>
                if (hasToggle) {
                    QTimer::singleShot(500, backend, [backend, togglePreRelease]() {
                        qCInfo(logApp) << "Screenshot: setting pre-release toggle to" << togglePreRelease;
                        emit backend->setRpShowPreReleases(togglePreRelease);
                    });
                }
                
                // --open-version-menu
                if (openVersionMenu) {
                    QTimer::singleShot(1000, backend, [backend]() {
                        qCInfo(logApp) << "Screenshot: opening version dropdown";
                        emit backend->openRpVersionMenu();
                    });
                }
                
                // Open detail page if --detail flag was set
                if (!detailSlug.isEmpty()) {
                    QTimer::singleShot(2000, backend, [backend, detailSlug, expandMajor, targetTab]() {
                        qCInfo(logApp) << "Screenshot: opening detail page for" << detailSlug << "tab" << targetTab;
                        if (targetTab == 1)
                            emit backend->openModDetailRequested(detailSlug);
                        else if (targetTab == 2)
                            emit backend->openShaderDetailRequested(detailSlug);
                        else
                            emit backend->openRpDetailRequested(detailSlug);
                        // --detail-expand <major>
                        if (!expandMajor.isEmpty()) {
                            QTimer::singleShot(3500, backend, [backend, expandMajor]() {
                                qCInfo(logApp) << "Screenshot: expanding detail group" << expandMajor;
                                emit backend->expandRpDetailGroup(expandMajor);
                            });
                        }
                    });
                }
            });

            // Connect to content-ready signals based on target
            if (!detailSlug.isEmpty()) {
                // --detail mode: wait for detail page + expand to complete
                // QML engine init takes ~10s, total chain: navigate(1.5)+detail(2)+expand(3.5)+QML timer(0.5) ≈ 7.5s after first paint
                QTimer::singleShot(20000, &app, [ready]() {
                    *ready = true;
                    qCInfo(logApp) << "Screenshot: detail page ready";
                });
            } else if (openVersionMenu) {
                // --open-version-menu mode: wait for results then open menu
                QTimer::singleShot(9000, &app, [ready]() {
                    *ready = true;
                    qCInfo(logApp) << "Screenshot: version menu ready";
                });
            } else if (targetPage == 1 && targetTab == 3) {
                // RP tab: wait for search results
                QObject::connect(backend, &ShadowBackend::resourcepackSearchCompleted,
                    &app, [ready](const QVariantList&, int) {
                        *ready = true;
                        qCInfo(logApp) << "Screenshot: RP search completed, ready";
                    });
            } else if (targetPage == 1 && targetTab == 1) {
                // Mod tab: wait for mod search results
                QObject::connect(backend, &ShadowBackend::searchResultsReady,
                    &app, [ready](const QVariantList&) {
                        *ready = true;
                        qCInfo(logApp) << "Screenshot: Mod search completed, ready";
                    });
            } else {
                // Launch/Settings: ready after base render delay
                QTimer::singleShot(3000, &app, [ready]() {
                    *ready = true;
                });
            }

            // Poll until ready or timeout
            auto pollTimer = new QTimer(&app);
            auto maxTimer = new QTimer(&app);
            QObject::connect(pollTimer, &QTimer::timeout, &app, [ready, pollTimer, maxTimer, doScreenshot]() {
                if (*ready) {
                    qCInfo(logApp) << "Screenshot: content ready, capturing...";
                    pollTimer->stop();
                    maxTimer->stop();
                    // Small delay for final rendering
                    QTimer::singleShot(500, qApp, doScreenshot);
                }
            });
            QObject::connect(maxTimer, &QTimer::timeout, &app, [ready, pollTimer, doScreenshot]() {
                qCWarning(logApp) << "Screenshot: timeout waiting for content, capturing anyway";
                pollTimer->stop();
                *ready = true;  // force
                QTimer::singleShot(200, qApp, doScreenshot);
            });
            pollTimer->start(200);     // check every 200ms
            maxTimer->start(60000);    // 60s max timeout (icon download needs time)

        } else {
            // No navigation: take screenshot after QML render delay
            QTimer::singleShot(3000, &app, doScreenshot);
        }
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
