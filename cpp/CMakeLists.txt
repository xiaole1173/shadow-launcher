cmake_minimum_required(VERSION 3.21)
project(ShadowLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)  # Qt default
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ============================================================
# Dependencies
# ============================================================

# Qt 6.8 LTS
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")
find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    QuickLayouts
    Network
    Widgets
    WebEngineQuick
    WebEngineWidgets
    LinguistTools
)

# nlohmann/json (header-only — manually downloaded)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE src/third_party/nlohmann)

# cpr (libcurl C++ wrapper) — TODO: install via vcpkg or manual
# For Phase 1 build test, use Qt Network as fallback
# Uncomment when cpr is available:
# include(FetchContent)
# FetchContent_Declare(cpr ...) 
# FetchContent_MakeAvailable(cpr)
# target_link_libraries(... cpr::cpr)

# ============================================================
# Sources
# ============================================================

set(SOURCES
    src/main.cpp

    # Utilities (Phase 4 — structured logging)
    src/utils/logger.cpp

    # Core services (Phase 2 — added progressively)
    src/core/http_client.cpp
    src/core/logger.cpp
    src/core/version_manager.cpp
    src/core/downloader.cpp
    src/core/parallel_downloader.cpp
    src/core/version_downloader.cpp
    src/core/launcher.cpp
    src/core/mod_manager.cpp
    src/core/microsoft_auth.cpp
    src/core/version_isolation.cpp
    src/core/mod_loader_installer.cpp
    src/core/download_coordinator.cpp
    src/core/crash_detector.cpp
    # Backend (Phase 3 — added progressively)
    src/backend/app_backend.cpp
    src/backend/settings_backend.cpp
    src/backend/account_backend.cpp
    src/backend/version_backend.cpp
    src/backend/launch_backend.cpp
    src/backend/resource_backend.cpp
    src/backend/check_backend.cpp

    # Unified backend wrapper (Phase 4)
    src/backend/shadow_backend.cpp
)

set(HEADERS
    src/utils/types.h
    src/utils/logger.h
    src/core/http_client.h
    src/core/logger.h
    src/core/version_manager.h
    src/core/downloader.h
    src/core/parallel_downloader.h
    src/core/download_coordinator.h
    src/core/version_downloader.h
    src/core/launcher.h
    src/core/mod_manager.h
    src/core/version_isolation.h
    src/core/crash_detector.h
    src/backend/app_backend.h
    src/backend/settings_backend.h
    src/backend/account_backend.h
    src/backend/version_backend.h
    src/backend/launch_backend.h
    src/backend/resource_backend.h
    src/backend/check_backend.h

    # Unified backend wrapper (Phase 4)
    src/backend/shadow_backend.h
)

# ============================================================
# Target
# ============================================================

# ── App icon (.ico must be next to .rc during rc compilation) ──
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico"
    "${CMAKE_CURRENT_BINARY_DIR}/app.ico"
    COPYONLY
)

qt_add_executable(ShadowLauncher ${SOURCES} ${HEADERS} resources/app.rc)

# QML: bundled in qrc for memory-mapped loading + runtime caching
# Dev mode: set SHADOW_DEV=1 to load from filesystem for hot-reload
qt6_add_resources(ShadowLauncher "qml_resources"
    PREFIX "/qt/qml/ShadowLauncher"
    FILES
        qml/AnimationTokens.qml
        qml/ConfirmDialog.qml
        qml/CrashDialog.qml
        qml/DetailInfoCard.qml
        qml/DetailVersionCard.qml
        qml/DownloadPage.qml
        qml/DownloadProgressPage.qml
        qml/HomePage.qml
        qml/InstallConfigOverlay.qml
        qml/InstallPage.qml
        qml/KillButton.qml
        qml/LaunchOverlay.qml
        qml/MainWindow.qml
        qml/MinecraftHead2D.qml
        qml/MinecraftHead3D.qml
        qml/ModLoaderCard.qml
        qml/ModDetailPage.qml
        qml/ResourcePackDetailPage.qml
        qml/SettingsGeneralPage.qml
        qml/SettingsJavaPage.qml
        qml/SettingsMemoryPage.qml
        qml/SettingsPage.qml
        qml/ShadowButton.qml
        qml/SplashWindow.qml
        qml/SubPageOverlays.qml
        qml/ToastManager.qml
        qml/VersionSelectOverlay.qml
        qml/VersionSelectPage.qml
        qml/VersionSettingsOverlay.qml
        qml/VersionSettingsPage.qml
)
target_include_directories(ShadowLauncher PRIVATE src)

# ── Lucide icon resources (shared prefix with QML for cross-mode path compatibility) ──
qt6_add_resources(ShadowLauncher "icon_resources"
    PREFIX "/qt/qml/ShadowLauncher/icons"
    FILES
        icons/lucide/home.svg
        icons/lucide/download.svg
        icons/lucide/settings.svg
        icons/lucide/package.svg
        icons/lucide/x-circle.svg
        icons/lucide/minus.svg
        icons/lucide/x.svg
        icons/lucide/shield-check.svg
        icons/lucide/key.svg
        icons/lucide/user.svg
        icons/lucide/chevron-down.svg
        icons/lucide/log-out.svg
        icons/lucide/play.svg
        icons/lucide/list.svg
        icons/lucide/wrench.svg
        icons/lucide/box.svg
        icons/lucide/puzzle.svg
        icons/lucide/sparkles.svg
        icons/lucide/palette.svg
        icons/lucide/check-circle.svg
        icons/lucide/flask-conical.svg
        icons/lucide/landmark.svg
        icons/lucide/refresh-cw.svg
        icons/lucide/globe.svg
        icons/lucide/search.svg
        icons/lucide/plug-zap.svg
        icons/lucide/gamepad-2.svg
        icons/lucide/folder-tree.svg
        icons/lucide/monitor.svg
        icons/lucide/scroll-text.svg
        icons/lucide/info.svg
        icons/lucide/tag.svg
        icons/lucide/zap.svg
        icons/lucide/ruler.svg
        icons/lucide/arrow-left.svg
        icons/lucide/hammer.svg
        icons/lucide/eye.svg
        icons/lucide/package-plus.svg
        icons/lucide/star.svg
        icons/lucide/pen.svg
        icons/lucide/alert-triangle.svg
        icons/lucide/terminal.svg
        icons/lucide/cpu.svg
        icons/lucide/door-open.svg
        icons/lucide/folder-open.svg
        icons/lucide/bot.svg
        icons/lucide/bar-chart-3.svg
        icons/lucide/layout-panel-top.svg
        icons/lucide/languages.svg
        icons/lucide/rotate-ccw.svg
        icons/lucide/folder.svg
        icons/lucide/folder-plus.svg
        icons/lucide/clipboard-list.svg
        icons/lucide/rocket.svg
        icons/lucide/map.svg
        icons/lucide/camera.svg
        icons/lucide/file-text.svg
        icons/lucide/file.svg
        icons/lucide/alert-octagon.svg
        icons/lucide/clipboard-copy.svg
        icons/lucide/folder-search.svg
        icons/lucide/trash-2.svg
        icons/lucide/broom.svg
        icons/lucide/file-search.svg
        icons/lucide/copy.svg
        icons/lucide/folder-sync.svg
        icons/lucide/skip-forward.svg
        icons/lucide/log-in.svg
        icons/lucide/check.svg
        icons/lucide/construction.svg
        icons/lucide/target.svg
        icons/lucide/sliders-horizontal.svg
        icons/lucide/image.svg
        icons/lucide/sun.svg
)

# ── Multi-language translations ──
file(GLOB_RECURSE I18N_QML_SOURCES "${CMAKE_SOURCE_DIR}/qml/*.qml")
file(GLOB_RECURSE I18N_CPP_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h")
qt_add_translations(ShadowLauncher
    TS_FILES
        i18n/shadow_zh_CN.ts
        i18n/shadow_zh_HK.ts
        i18n/shadow_zh_TW.ts
    SOURCES
        ${I18N_QML_SOURCES}
        ${I18N_CPP_SOURCES}
    RESOURCE_PREFIX /i18n
)

# ── libwebp (static, for webp→PNG icon decoding) ──
set(WEBP_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/libwebp")
target_include_directories(ShadowLauncher PRIVATE "${WEBP_ROOT}/include")
target_link_directories(ShadowLauncher PRIVATE "${WEBP_ROOT}/lib")

# Define WEBP_STATIC_LINK for static lib (needed on Windows)
target_compile_definitions(ShadowLauncher PRIVATE WEBP_STATIC_LINK)

target_link_libraries(ShadowLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
    Qt6::Network
    Qt6::Widgets
    Qt6::WebEngineQuick
    Qt6::WebEngineWidgets
    nlohmann_json
    libwebp
)

# cpr::cpr — added when libcurl is installed

# Windows: hide console window
if(WIN32)
    set_target_properties(ShadowLauncher PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    # Auto-deploy Qt DLLs after each build
    add_custom_command(TARGET ShadowLauncher POST_BUILD
        COMMAND "${CMAKE_PREFIX_PATH}/bin/windeployqt.exe"
            "$<TARGET_FILE:ShadowLauncher>"
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            --no-translations --no-compiler-runtime
        COMMENT "Deploying Qt runtime (QML imports from ${CMAKE_SOURCE_DIR}/qml)..."
    )
    # Copy QML source files to output dir for dev-mode hot-swap (no rebuild for QML-only changes)
    add_custom_command(TARGET ShadowLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/qml"
            "$<TARGET_FILE_DIR:ShadowLauncher>/qml"
        COMMENT "Copying QML sources for dev-mode..."
    )
    # Copy icon SVGs into qml dir for dev-mode (same relative path works in both modes)
    add_custom_command(TARGET ShadowLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/icons"
            "$<TARGET_FILE_DIR:ShadowLauncher>/qml/icons"
        COMMENT "Copying icon SVGs for dev-mode..."
    )
endif()

# ============================================================
# Install
# ============================================================

install(TARGETS ShadowLauncher
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
