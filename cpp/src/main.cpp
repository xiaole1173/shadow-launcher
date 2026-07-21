// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Shadow Launcher — C++ Entry Point
// Phase 4: Unified backend + QML UI port

#include <QApplication>
#include <QWidget>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QJsonDocument>

// 3D geometry
#include <QJsonObject>
#include <QProcess>
#include <QWindow>
#include <QQuickWindow>
#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <QScreen>
#include <QPixmap>
#include <QWindow>
#include <QSurfaceFormat>
#include <QTranslator>
#include <QSettings>
#include <memory>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include "utils/logger.h"
#include "backend/shadow_backend.h"
#include "multiplayer/elevated_session.h"
#include "core/http_client.h"
#include "core/screenshot_server.h"

// ── Remove CEF references ──

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
    // ── Pending update from previous session? ──
    // MUST run before QApplication — no QML window should flash.
    {
        OutputDebugStringA("[PreInit] 检查待安装更新...\n");
        wchar_t exePathW[MAX_PATH];
        GetModuleFileNameW(nullptr, exePathW, MAX_PATH);
        std::wstring exePath(exePathW);
        std::wstring appDirW = exePath.substr(0, exePath.find_last_of(L"\\/"));

        std::wstring lockPath = appDirW + L"\\_update\\.install_lock";
        std::wstring statePath = appDirW + L"\\_update\\state.json";

        bool lockExists = GetFileAttributesW(lockPath.c_str()) != INVALID_FILE_ATTRIBUTES;
        bool stateExists = GetFileAttributesW(statePath.c_str()) != INVALID_FILE_ATTRIBUTES;

        // If lock exists but no state.json → SLUpdater completed, cleanup
        if (lockExists && !stateExists) {
            OutputDebugStringA("[PreInit] 安装锁存在但无待安装更新，清理锁\n");
            DeleteFileW(lockPath.c_str());
            lockExists = false;
        }
        // If lock exists with state → SLUpdater crashed mid-install, retry
        if (lockExists) {
            OutputDebugStringA("[PreInit] 安装锁存在，上次安装可能中断，重试\n");
            DeleteFileW(lockPath.c_str());
            lockExists = false;
        }
        HANDLE hFile = CreateFileW(statePath.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            OutputDebugStringA("[PreInit] 无待安装更新\n");
        } else {
            DWORD size = GetFileSize(hFile, nullptr);
            if (size > 0 && size < 4096) {
                std::string json(size, '\0');
                DWORD read = 0;
                ReadFile(hFile, &json[0], size, &read, nullptr);
                CloseHandle(hFile);

                // Quick JSON parse: find "state":N
                auto findInt = [&](const char* key) -> int {
                    std::string k = std::string("\"") + key + "\":";
                    auto pos = json.find(k);
                    if (pos == std::string::npos) return -1;
                    pos += k.size();
                    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
                    return atoi(&json[pos]);
                };

                int stateVal = findInt("state");

                // If already running the target version, clean up stale state
                bool alreadyCurrent = false;
                if (stateVal == 5) {
                    auto pos = json.find("\"download_version\":\"");
                    if (pos != std::string::npos) {
                        pos += 20;
                        auto end = json.find('"', pos);
                        std::string dlVer = json.substr(pos, end - pos);
                        if (dlVer == SHADOW_DISPLAY_VERSION) {
                            OutputDebugStringA("[PreInit] 已是目标版本，清除过期状态\n");
                            DeleteFileW(statePath.c_str());
                            DeleteFileW(lockPath.c_str());
                            std::wstring wildcard = appDirW + L"\\_update\\*";
                            WIN32_FIND_DATAW fd;
                            HANDLE hFind = FindFirstFileW(wildcard.c_str(), &fd);
                            if (hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    if (wcscmp(fd.cFileName, L"state.json") != 0) {
                                        DeleteFileW((appDirW + L"\\_update\\" + fd.cFileName).c_str());
                                    }
                                } while (FindNextFileW(hFind, &fd));
                                FindClose(hFind);
                            }
                            alreadyCurrent = true;
                        }
                    }
                }

                if (alreadyCurrent) {
                    // skip — already on this version
                } else if (stateVal == -1) {
                    // Corrupted — clean up
                    OutputDebugStringA("[PreInit] state.json 损坏，清除\n");
                    DeleteFileW(statePath.c_str());
                    // Also delete any download files
                    std::wstring wildcard = appDirW + L"\\_update\\*";
                    WIN32_FIND_DATAW fd;
                    HANDLE hFind = FindFirstFileW(wildcard.c_str(), &fd);
                    if (hFind != INVALID_HANDLE_VALUE) {
                        do {
                            if (wcscmp(fd.cFileName, L"state.json") != 0) {
                                std::wstring fp = appDirW + L"\\_update\\" + fd.cFileName;
                                DeleteFileW(fp.c_str());
                            }
                        } while (FindNextFileW(hFind, &fd));
                        FindClose(hFind);
                    }
                } else if (stateVal == 5) { // Ready
                    auto pos = json.find("\"download_path\":\"");
                    if (pos != std::string::npos) {
                        pos += 18;
                        auto end = json.find('"', pos);
                        std::string newFile = json.substr(pos, end - pos);
                        std::wstring newFileW(newFile.begin(), newFile.end());

                        // Extract release notes BEFORE SLUpdater deletes state.json
                        auto rnPos = json.find("\"release_notes\":\"");
                        if (rnPos != std::string::npos) {
                            rnPos += 17;
                            std::string notes;
                            while (rnPos < json.size()) {
                                if (json[rnPos] == '\\' && rnPos + 1 < json.size()) {
                                    char c = json[rnPos + 1];
                                    if (c == 'n') { notes += '\n'; rnPos += 2; continue; }
                                    if (c == 'r') { rnPos += 2; continue; }
                                    if (c == '"') { notes += '"'; rnPos += 2; continue; }
                                    if (c == '\\') { notes += '\\'; rnPos += 2; continue; }
                                }
                                if (json[rnPos] == '"') break;
                                notes += json[rnPos];
                                rnPos++;
                            }
                            if (!notes.empty()) {
                                // Escape notes for JSON (must be done after unescaping from state.json)
                                auto jsonEscape = [](const std::string& s) -> std::string {
                                    std::string out;
                                    out.reserve(s.size() + 32);
                                    for (char c : s) {
                                        switch (c) {
                                            case '"':  out += "\\\""; break;
                                            case '\\': out += "\\\\"; break;
                                            case '\n': out += "\\n";  break;
                                            case '\r': out += "\\r";  break;
                                            case '\t': out += "\\t";  break;
                                            default:   out += c;
                                        }
                                    }
                                    return out;
                                };
                                // Write to changelog_to_show.json
                                std::wstring clPath = appDirW + L"\\_update\\changelog_to_show.json";
                                HANDLE hCl = CreateFileW(clPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                                if (hCl != INVALID_HANDLE_VALUE) {
                                    std::string wrapped = "{\"version\":\"" + std::string(SHADOW_DISPLAY_VERSION)
                                                         + "\",\"notes\":\"" + jsonEscape(notes) + "\"}";
                                    DWORD written;
                                    WriteFile(hCl, wrapped.data(), (DWORD)wrapped.size(), &written, nullptr);
                                    CloseHandle(hCl);
                                    OutputDebugStringA("[PreInit] 已保存发布公告\n");
                                }
                            }
                        }

                        std::wstring updaterPath = appDirW + L"\\SLUpdater.exe";
                        if (GetFileAttributesW(updaterPath.c_str()) != INVALID_FILE_ATTRIBUTES &&
                            GetFileAttributesW(newFileW.c_str()) != INVALID_FILE_ATTRIBUTES) {
                            // Create lock file to prevent concurrent installs
                            CreateDirectoryW((appDirW + L"\\_update").c_str(), nullptr);
                            HANDLE hLock = CreateFileW(lockPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                            if (hLock != INVALID_HANDLE_VALUE) CloseHandle(hLock);

                            OutputDebugStringA("[PreInit] 发现待安装更新，启动 SLUpdater\n");
                            DWORD pid = GetCurrentProcessId();
                            wchar_t cmdLine[2048];
                            swprintf_s(cmdLine, L"\"%s\" %lu \"%s\" \"%s\"",
                                       updaterPath.c_str(), pid, exePathW, newFileW.c_str());

                            STARTUPINFOW si = { sizeof(si) };
                            PROCESS_INFORMATION pi = {};
                            si.dwFlags = STARTF_USESHOWWINDOW;
                            si.wShowWindow = SW_HIDE;
                            CreateProcessW(nullptr, cmdLine, nullptr, nullptr,
                                           FALSE, 0, nullptr, nullptr, &si, &pi);
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                            return 0; // Updated, exit immediately
                        } else {
                            OutputDebugStringA("[PreInit] 缺少 SLUpdater 或更新文件，跳过\n");
                        }
                    }
                }
            } else {
                CloseHandle(hFile);
            }
        }
    }

    auto checkpoint = [&](const QString& stage) { qCInfo(logApp) << stage; };

    // Qt attribute setup
    checkpoint(QStringLiteral("Setting Qt attributes..."));
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // ── Transparent frameless window: require alpha channel ──
    // Without this, Window { color: "transparent" } may flash white corners
    // on certain GPU/driver combos (Intel, some NVidia).
    {
        QSurfaceFormat fmt;
        fmt.setAlphaBufferSize(8);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    QApplication app(argc, argv);
    checkpoint(QStringLiteral("QApplication constructed"));

    // ── Parse elevated session args ──
    {
        QStringList args = app.arguments();
        int elevIdx = args.indexOf(QStringLiteral("--elevated"));
        int cfgIdx = args.indexOf(QStringLiteral("--elevate-config"));
        if (elevIdx >= 0 && cfgIdx >= 0 && cfgIdx + 1 < args.size()) {
            QString configPath = args[cfgIdx + 1];
            auto data = ElevatedSession::loadAndDelete(configPath);
            if (!data.networkName.isEmpty()) {
                ElevatedSession::setActive(true);
                ElevatedSession::setCurrentSession(data);
                qCInfo(logApp) << QStringLiteral("[提权] 已加载会话配置 房间码=%1").arg(data.roomCode);
            }
        }
    }

    app.setApplicationName("Shadow Launcher");
    app.setApplicationVersion(SHADOW_DISPLAY_VERSION);
    app.setOrganizationName("ShadowTeam");
    app.setOrganizationDomain("shadowteam.dev");

    // ── Load saved language ──
    QSettings settings;
    QString lang = settings.value("general/languageIndex", 0).toInt() == 1 ? "zh_HK"
                 : settings.value("general/languageIndex", 0).toInt() == 2 ? "zh_TW"
                 : "zh_CN";
    auto* translator = new QTranslator(&app);
    if (lang != QStringLiteral("zh_CN") && translator->load(QStringLiteral(":/i18n/shadow_%1").arg(lang))) {
        app.installTranslator(translator);
        qCInfo(logApp) << QStringLiteral("语言已加载 lang=%1").arg(lang);
    } else {
        qCInfo(logApp) << QStringLiteral("使用默认语言");
    }

    // ── Initialise structured logging ──
    initLogger(QCoreApplication::applicationDirPath());
    checkpoint(QStringLiteral("Logger initialized"));
    qCInfo(logApp) << QStringLiteral("=== Shadow Launcher v%1 启动 ===").arg(app.applicationVersion())
                   << "PID:" << QCoreApplication::applicationPid();

    // Data directory
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    checkpoint(QStringLiteral("Data directory created"));

    // Create unified backend — owns all 7 sub-backends

    // Modrinth API: uniquely-identifying User-Agent header (mandatory)
    HttpClient::instance().setUserAgent(
        QStringLiteral("YOUR_APP/1.0.0"
                       " (https://github.com/xiaole1173/shadow-launcher)"));

    ShadowBackend* backend = new ShadowBackend(&app);
    checkpoint(QStringLiteral("ShadowBackend constructed"));

    // QML engine
    QQmlApplicationEngine engine;

    // Clear QML disk cache to avoid stale compiled QML from older builds
    engine.clearComponentCache();

    // Add exe dir to import path so module can find qmldir
    engine.addImportPath(QCoreApplication::applicationDirPath());
    if (!qEnvironmentVariableIsSet("SHADOW_DEV")) {
        engine.addImportPath(QStringLiteral("qrc:/ShadowLauncher/qml"));
    }
    checkpoint(QStringLiteral("QML engine created"));

    // 注册自定义 3D 几何体

    // Expose backend and data directory to QML
    engine.rootContext()->setContextProperty("backend", backend);
    engine.rootContext()->setContextProperty("dataDir", dataDir);

    // Pass engine reference for language hot-switch
    backend->setEngine(&engine);

    // ── Screenshot Server (Debug/Dev only; NDEBUG stubs it out) ──
    ScreenshotServer* ssServer = new ScreenshotServer(&engine, &app);

    // ── Taskbar minimize: install native event filter ──
    TaskbarMinimizeFilter* taskbarFilter = new TaskbarMinimizeFilter();
    app.installNativeEventFilter(taskbarFilter);

    // Set target window + measure first frame render
    QElapsedTimer* pStartupTimer = &startupTimer;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [taskbarFilter, pStartupTimer, ssServer](QObject* obj, const QUrl&) {
            QQuickWindow* win = qobject_cast<QQuickWindow*>(obj);
            if (!win) return;

            // ── Force transparent window background (prevents white corner artifacts) ──
            win->setColor(Qt::transparent);

            taskbarFilter->targetWindow = win;
            screenshotWindow = win;
            if (ssServer) ssServer->setWindow(win);

            // ── Frameless window taskbar fix: add WS_SYSMENU|WS_MINIMIZEBOX ──
            // Without these, Windows Shell ignores taskbar minimize requests
            HWND hwnd = (HWND)win->winId();
            SetWindowLongPtrW(hwnd, GWL_STYLE,
                GetWindowLongPtrW(hwnd, GWL_STYLE) | WS_SYSMENU | WS_MINIMIZEBOX);
            // Measure first frame render
            QObject::connect(win, &QQuickWindow::frameSwapped,
                win, [pStartupTimer]() {
                    qCInfo(logApp) << QStringLiteral("首帧渲染完成");
                }, Qt::SingleShotConnection);
        }, Qt::QueuedConnection);

    // Load QML — dev mode (file://) when SHADOW_DEV=1, otherwise qrc precompiled
    bool devMode = qEnvironmentVariableIsSet("SHADOW_DEV");
    // Filesystem import path: always added, but dev mode prioritizes it
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    // QRC import path: only in release mode (dev mode forces filesystem resolution)
    if (!devMode) {
        engine.addImportPath(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml"));
    } else {
        // Dev mode: add Qt's QML path so built-in modules (QtQuick3D etc.) resolve plugins
            engine.addImportPath(QStringLiteral("YOUR_QT_QML_PATH"));
    }

    QUrl url;
    if (devMode) {
        QString devPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml/MainWindow.qml");
        if (QFile::exists(devPath)) {
            checkpoint(QStringLiteral("Loading QML (filesystem dev mode)..."));
            url = QUrl::fromLocalFile(devPath);
            qCInfo(logApp) << QStringLiteral("[DEV MODE] 从文件系统加载MainWindow 路径=%1").arg(devPath);
            qCInfo(logApp) << QStringLiteral("[DEV MODE] 全部QML从文件系统加载 (QRC导入路径已禁用)");
        } else {
            qCWarning(logApp) << QStringLiteral("SHADOW_DEV已设置但路径不存在 路径=%1 回退到QRC").arg(devPath);
            url = QUrl(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml/MainWindow.qml"));
        }
    } else {
        checkpoint(QStringLiteral("Loading QML (precompiled qrc)..."));
        url = QUrl(QStringLiteral("qrc:/qt/qml/ShadowLauncher/qml/MainWindow.qml"));
    }

    // ── Beta key gate ──
    // Skipped in dev mode or with SHADOW_SKIP_BETA env var
    bool loadedBetaDialog = false;
    if (!qEnvironmentVariableIsSet("SHADOW_SKIP_BETA")) {
        checkpoint(QStringLiteral("Checking beta key..."));
        QString savedKey = ShadowBackend::loadBetaKey();

        if (savedKey.isEmpty() || !ShadowBackend::validateBetaKey(savedKey)) {
            if (!savedKey.isEmpty()) {
                qCWarning(logApp) << QStringLiteral("[Beta] 保存的密钥无效 显示对话框");
            }

            // Load BetaKeyDialog; betaVerified signal will reload MainWindow
            QUrl betaUrl;
            if (devMode) {
                QString devPath = QCoreApplication::applicationDirPath() + "/qml/BetaKeyDialog.qml";
                betaUrl = QFile::exists(devPath) ? QUrl::fromLocalFile(devPath)
                                                  : QUrl("qrc:/qt/qml/ShadowLauncher/qml/BetaKeyDialog.qml");
            } else {
                betaUrl = QUrl("qrc:/qt/qml/ShadowLauncher/qml/BetaKeyDialog.qml");
            }
            checkpoint(QStringLiteral("Loading beta key dialog..."));
            engine.load(betaUrl);
            checkpoint(QStringLiteral("Waiting for beta key input..."));

            // On successful verification, reload MainWindow
            QObject::connect(backend, &ShadowBackend::betaVerified, &app, [&engine, url]() {
                qCInfo(logApp) << QStringLiteral("[Beta] 密钥验证通过 加载MainWindow");
                engine.load(url);
            });

            loadedBetaDialog = true;
        } else {
            qCInfo(logApp) << QStringLiteral("[Beta] 密钥有效 继续启动");
        }
    }

    if (!loadedBetaDialog) {
        engine.load(url);
    }
    checkpoint(QStringLiteral("QML engine.load() completed"));

    // Only check root objects if MainWindow was loaded (not beta dialog)
    if (!loadedBetaDialog) {
        if (engine.rootObjects().isEmpty()) {
            qCCritical(logApp) << QStringLiteral("无法加载任何QML根对象 退出");
            return -1;
        }
        checkpoint(QStringLiteral("Root objects verified"));
    }

    qCInfo(logApp) << QStringLiteral("事件循环已启动")
                   << "— total startup:" << startupTimer.elapsed() << "ms";

    // ── Restore elevated session (multiplayer handoff) ──
    if (ElevatedSession::isActive()) {
        auto session = ElevatedSession::currentSession();
        QTimer::singleShot(1500, backend, [backend, &engine, session]() {
            // Navigate to multiplayer page (index 2)
            auto objs = engine.rootObjects();
            if (!objs.isEmpty())
                objs.first()->setProperty("navListIndex", 2);

            if (session.networkName.isEmpty())
                return;

            if (session.role == QStringLiteral("guest")) {
                QMetaObject::invokeMethod(backend->multiplayer(), "restoreGuestSession",
                    Q_ARG(QString, session.networkName),
                    Q_ARG(QString, session.networkKey),
                    Q_ARG(QString, session.roomCode));
            } else {
                QMetaObject::invokeMethod(backend->multiplayer(), "restoreHostSession",
                    Q_ARG(QString, session.networkName),
                    Q_ARG(QString, session.networkKey),
                    Q_ARG(QString, session.roomCode),
                    Q_ARG(unsigned short, session.mcPort),
                    Q_ARG(QString, session.hostname));
            }
        });
    }

    // ── Start screenshot server ──
    {
        quint16 ssPort = static_cast<quint16>(qEnvironmentVariableIntValue("SHADOW_SS_PORT"));
        if (ssPort == 0) ssPort = 9999;
        ssServer->start(ssPort);
    }

    // ── Auto-launch test mode ──
    QStringList args = app.arguments();
    int autoIdx = args.indexOf(QStringLiteral("--auto-launch"));
    if (autoIdx >= 0 && autoIdx + 1 < args.size()) {
        QString autoVersion = args[autoIdx + 1];
        qCInfo(logApp) << QStringLiteral("[自动测试] 自动启动模式 版本=%1").arg(autoVersion);
        QTimer::singleShot(2000, backend, [backend, autoVersion]() {
            qCInfo(logApp) << QStringLiteral("[自动测试] 触发启动 版本=%1").arg(autoVersion);
            backend->launch(autoVersion, true);
        });
    }

    // ── Auto-install test mode (trigger download click programmatically) ──
    int autoInstallIdx = args.indexOf(QStringLiteral("--auto-install"));
    if (autoInstallIdx >= 0 && autoInstallIdx + 1 < args.size()) {
        QString autoVersion = args[autoInstallIdx + 1];
        qCInfo(logApp) << QStringLiteral("[自动测试] 自动安装模式 版本=%1").arg(autoVersion);
        QTimer::singleShot(1500, backend, [backend, autoVersion]() {
            qCInfo(logApp) << QStringLiteral("[自动测试] 触发安装 版本=%1").arg(autoVersion);
            backend->installVersion(autoVersion);
        });
    }

    // ── Test download mode: navigate to download page + auto-install ──
    int testDownloadIdx = args.indexOf(QStringLiteral("--test-download"));
    if (testDownloadIdx >= 0 && testDownloadIdx + 1 < args.size()) {
        QString testVersion = args[testDownloadIdx + 1];
        qCInfo(logApp) << QStringLiteral("[自动测试] 下载测试模式 版本=%1").arg(testVersion);
        // Navigate to download page after QML is ready
        QTimer::singleShot(2500, backend, [&engine, backend, testVersion]() {
            qCInfo(logApp) << QStringLiteral("[自动测试] 跳转到下载页面");
            if (!engine.rootObjects().isEmpty()) {
                engine.rootObjects().first()->setProperty("navListIndex", 1);
            }
            // Trigger install 800ms after navigation (allow page to render)
            QTimer::singleShot(800, backend, [backend, testVersion]() {
                qCInfo(logApp) << QStringLiteral("[自动测试] 触发下载安装 版本=%1").arg(testVersion);
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
        qCInfo(logApp) << QStringLiteral("[截图] 截图模式 保存路径=%1").arg(ssPath);

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
                    qCInfo(logApp) << QStringLiteral("[截图] 截图已保存 路径=%1").arg(ssPath)
                                   << "size:" << pix.width() << "x" << pix.height();
                } else {
                    qCCritical(logApp) << QStringLiteral("[截图] 截图保存失败 路径=%1").arg(ssPath);
                }
            } else {
                qCCritical(logApp) << QStringLiteral("[截图] 窗口未就绪");
            }
            // Give async icon downloads 10s to finish before quitting
            qCInfo(logApp) << QStringLiteral("[截图] 等待10秒图标下载后退出");
            QTimer::singleShot(10000, qApp, &QCoreApplication::quit);
        };

        if (targetPage >= 0) {
            qCInfo(logApp) << QStringLiteral("[截图] 导航到页面=%1").arg(targetPage)
                           << "tab" << targetTab;

            // Navigate after QML is ready
            QTimer::singleShot(1500, backend, [backend, targetPage, targetTab, 
                    detailSlug, expandMajor, togglePreRelease, hasToggle, openVersionMenu]() {
                emit backend->navigateToRequested(targetPage, targetTab);
                
                // --toggle-pre-release <on|off>
                if (hasToggle) {
                    QTimer::singleShot(500, backend, [backend, togglePreRelease]() {
                        qCInfo(logApp) << QStringLiteral("[截图] 切换预发布开关=%1").arg(togglePreRelease);
                        emit backend->setRpShowPreReleases(togglePreRelease);
                    });
                }
                
                // --open-version-menu
                if (openVersionMenu) {
                    QTimer::singleShot(1000, backend, [backend]() {
                        qCInfo(logApp) << QStringLiteral("[截图] 打开版本下拉菜单");
                        emit backend->openRpVersionMenu();
                    });
                }
                
                // Open detail page if --detail flag was set
                if (!detailSlug.isEmpty()) {
                    QTimer::singleShot(2000, backend, [backend, detailSlug, expandMajor, targetTab]() {
                        qCInfo(logApp) << QStringLiteral("[截图] 打开详情页 slug=%1 tab=%2").arg(detailSlug, targetTab);
                        if (targetTab == 1)
                            emit backend->openModDetailRequested(detailSlug);
                        else if (targetTab == 2)
                            emit backend->openShaderDetailRequested(detailSlug);
                        else
                            emit backend->openRpDetailRequested(detailSlug);
                        // --detail-expand <major>
                        if (!expandMajor.isEmpty()) {
                            QTimer::singleShot(3500, backend, [backend, expandMajor]() {
                                qCInfo(logApp) << QStringLiteral("[截图] 展开详情分组 version=%1").arg(expandMajor);
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
                    qCInfo(logApp) << QStringLiteral("[截图] 详情页就绪");
                });
            } else if (openVersionMenu) {
                // --open-version-menu mode: wait for results then open menu
                QTimer::singleShot(9000, &app, [ready]() {
                    *ready = true;
                    qCInfo(logApp) << QStringLiteral("[截图] 版本菜单就绪");
                });
            } else if (targetPage == 1 && targetTab == 3) {
                // RP tab: wait for search results
                QObject::connect(backend, &ShadowBackend::resourcepackSearchCompleted,
                    &app, [ready](const QVariantList&, int) {
                        *ready = true;
                        qCInfo(logApp) << QStringLiteral("[截图] 资源包搜索完成 就绪");
                    });
            } else if (targetPage == 1 && targetTab == 1) {
                // Mod tab: wait for mod search results
                QObject::connect(backend, &ShadowBackend::searchResultsReady,
                    &app, [ready](const QVariantList&) {
                        *ready = true;
                        qCInfo(logApp) << QStringLiteral("[截图] Mod搜索完成 就绪");
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
                    qCInfo(logApp) << QStringLiteral("[截图] 内容就绪 开始截图");
                    pollTimer->stop();
                    maxTimer->stop();
                    // Small delay for final rendering
                    QTimer::singleShot(500, qApp, doScreenshot);
                }
            });
            QObject::connect(maxTimer, &QTimer::timeout, &app, [ready, pollTimer, doScreenshot]() {
                qCWarning(logApp) << QStringLiteral("[截图] 等待超时 强制截图");
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

    // ── Auto-install-loader test mode ──
    int autoLoaderIdx = args.indexOf(QStringLiteral("--auto-install-loader"));
    if (autoLoaderIdx >= 0 && autoLoaderIdx + 4 < args.size()) {
        QString loaderMcVer = args[autoLoaderIdx + 1];
        QString loaderType = args[autoLoaderIdx + 2];
        QString loaderVer = args[autoLoaderIdx + 3];
        QString loaderName = args[autoLoaderIdx + 4];
        qCInfo(logApp) << QStringLiteral("[自动测试] 安装加载器 MC=%1").arg(loaderMcVer)
                       << loaderType << loaderVer << ">" << loaderName;
        QTimer::singleShot(2000, backend, [backend, loaderMcVer, loaderType, loaderVer, loaderName]() {
            qCInfo(logApp) << QStringLiteral("[自动测试] 先安装MC 版本=%1").arg(loaderMcVer);
            backend->installVersion(loaderMcVer);
            // Chain loader install after MC finishes
            QObject::connect(backend, &ShadowBackend::installComplete,
                backend, [backend, loaderMcVer, loaderType, loaderVer, loaderName](const QString& completedName) {
                    // Only chain if the completed install is the MC version we're waiting for
                    if (completedName != loaderMcVer) return;
                    qCInfo(logApp) << QStringLiteral("[自动测试] MC安装完成 安装加载器=%1").arg(loaderType);
                    QTimer::singleShot(1000, backend, [backend, loaderMcVer, loaderType, loaderVer, loaderName]() {
                        backend->installModLoader(loaderMcVer, loaderType, loaderVer, loaderName);
                    });
                });
        });
    }
    int dlIdx = args.indexOf(QStringLiteral("--auto-download"));
    if (dlIdx >= 0 && dlIdx + 1 < args.size()) {
        QString dlVersion = args[dlIdx + 1];
        qCInfo(logApp) << QStringLiteral("[自动测试] 自动下载模式 版本=%1").arg(dlVersion);
        QTimer::singleShot(5000, backend, [backend, dlVersion]() {
            qCInfo(logApp) << QStringLiteral("[自动测试] 触发下载安装 版本=%1").arg(dlVersion);
            backend->installVersion(dlVersion);
        });
    }

    return app.exec();
}
