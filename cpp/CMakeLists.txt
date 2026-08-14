cmake_minimum_required(VERSION 3.21)
project(ShadowLauncher VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)  # Qt default
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ============================================================
# 构建选项
# ============================================================

# 内测密钥闸门（Beta gate）：
#   ON  = 内测版：启动先弹 BetaKeyDialog 密钥验证窗口（无保存密钥时）
#   OFF = 公测版：直接加载 MainWindow，跳过内测验证（默认）
# 加回内测：cmake 重新配置 -DSHADOW_ENABLE_BETA_GATE=ON 即可，相关代码/资源全部保留。
option(SHADOW_ENABLE_BETA_GATE "Enable beta key gate (internal testing)" OFF)

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
    Concurrent
    Network
    Widgets
    LinguistTools
)

# nlohmann/json (header-only 鈥?manually downloaded)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE src/third_party/nlohmann)

# cpr (libcurl C++ wrapper) 鈥?TODO: install via vcpkg or manual
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
    src/main_release.cpp

    # Utilities (Phase 4 鈥?structured logging)
    src/utils/logger.cpp
    src/utils/temp_tracker.cpp
    src/utils/token_crypto.cpp

    # Core services (Phase 2 鈥?added progressively)
    src/core/http_client.cpp
    src/core/resource_fetch_engine.cpp
    src/core/cf_api.cpp
    src/core/logger.cpp
    src/core/version_manager.cpp
    src/core/file_downloader.cpp
    src/core/asset_downloader.cpp
    src/core/version_downloader.cpp
    src/core/launcher.cpp
    src/core/mc_language.cpp
    src/core/geoip_service.cpp
    src/core/mod_manager.cpp
    src/core/icon_cache.cpp
    src/core/microsoft_auth.cpp
    src/core/yggdrasil_auth.cpp
    src/core/version_isolation.cpp
    src/core/mod_loader_installer.cpp
    src/core/java_runtime_installer.cpp
    src/core/local_mod_manager.cpp
    src/core/modpack_importer.cpp
    src/core/modpack/modpack_common.cpp
    src/core/modpack/zip_archive.cpp
    src/core/modpack/modpack_parser.cpp
    src/core/modpack/modpack_downloader.cpp
    src/core/modpack/mod_download_engine.cpp
    src/core/modpack/modpack_install_task.cpp
    src/core/modpack/modpack_exporter.cpp
    src/core/update_manager.cpp
    thirdparty/miniz/miniz.c
    src/utils/lzma/LzmaDec.c
    src/core/crash_detector.cpp
    src/core/screenshot_server.cpp
    src/core/step_node.cpp
    src/core/step_pipeline.cpp
    # Backend (Phase 3 鈥?added progressively)
    src/backend/app_backend.cpp
    src/backend/settings_backend.cpp
    src/backend/yggdrasil_backend.cpp
    src/backend/yggdrasil_skin_fetcher.cpp
    src/backend/account_backend.cpp
    src/backend/version_backend.cpp
    src/session/download_session.cpp
    src/backend/launch_backend.cpp
    src/backend/resource_backend.cpp
    src/backend/stats_backend.cpp
    src/backend/java_backend.cpp
    src/backend/check_backend.cpp
    src/backend/userdata_backend.cpp
    src/backend/debug_logger.cpp

    # Unified backend wrapper (Phase 4)
    src/backend/shadow_backend.cpp
)

set(HEADERS
    src/utils/types.h
    src/utils/logger.h
    src/utils/temp_tracker.h
    src/core/http_client.h
    src/core/resource_fetch_engine.h
    src/core/cf_api.h
    src/core/logger.h
    src/core/version_manager.h
    src/core/file_downloader.h
    src/core/asset_downloader.h
    src/core/version_downloader.h
    src/core/step_node.h
    src/core/step_pipeline.h
    src/core/launcher.h
    src/core/mc_language.h
    src/core/geoip_service.h
    src/core/mod_manager.h
    src/core/local_mod_manager.h
    src/core/modpack_importer.h
    src/core/modpack/modpack_common.h
    src/core/modpack/zip_archive.h
    src/core/modpack/modpack_parser.h
    src/core/modpack/modpack_downloader.h
    src/core/modpack/mod_download_engine.h
    src/core/modpack/modpack_install_task.h
    src/core/modpack/modpack_exporter.h
    src/core/update_manager.h
    src/core/version_isolation.h
    src/core/yggdrasil_auth.h
    src/core/crash_detector.h
    src/core/screenshot_server.h
    src/backend/app_backend.h
    src/backend/settings_backend.h
    src/backend/yggdrasil_backend.h
    src/backend/yggdrasil_skin_fetcher.h
    src/backend/account_backend.h
    src/backend/version_backend.h
    src/backend/launch_backend.h
    src/backend/resource_backend.h
    src/backend/stats_backend.h
    src/backend/check_backend.h
    src/backend/userdata_backend.h
    src/backend/debug_logger.h

    # Unified backend wrapper (Phase 4)
    src/backend/shadow_backend.h

    src/multiplayer/room_code.cpp
    src/multiplayer/room_code.h
    src/multiplayer/scaffolding_protocol.cpp
    src/multiplayer/scaffolding_protocol.h
    src/multiplayer/connection_guard.cpp
    src/multiplayer/connection_guard.h
    src/multiplayer/relay_crypto.cpp
    src/multiplayer/relay_crypto.h
    src/multiplayer/encrypted_addr.h
    src/multiplayer/easytier_process.cpp
    src/multiplayer/easytier_process.h
    src/multiplayer/multiplayer_manager.cpp
    src/multiplayer/multiplayer_manager.h
    src/multiplayer/elevated_session.cpp
    src/multiplayer/elevated_session.h
    src/multiplayer/mc_scanner.cpp
    src/multiplayer/mc_scanner.h
    src/multiplayer/port_request.cpp
    src/multiplayer/port_request.h
)

# Encrypted blob fragments (scrambled, split across compilation units)
# Real files exist only on the developer's machine (gitignored).
# CI/third-party builds use the stub with zero placeholders.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/multiplayer/encrypted_frag_1.cpp")
    list(APPEND SOURCES
        src/multiplayer/encrypted_frag_1.cpp
        src/multiplayer/encrypted_frag_2.cpp
        src/multiplayer/encrypted_frag_3.cpp
        src/multiplayer/encrypted_frag_4.cpp
        src/multiplayer/encrypted_frag_5.cpp
    )
else()
    list(APPEND SOURCES src/multiplayer/encrypted_frag_stub.cpp)
endif()

# ============================================================
# Target
# ============================================================

# 鈹€鈹€ App icon (.ico must be next to .rc during rc compilation) 鈹€鈹€
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico"
    "${CMAKE_CURRENT_BINARY_DIR}/app.ico"
    COPYONLY
)

qt_add_executable(ShadowLauncher ${SOURCES} ${HEADERS} resources/app.rc)
add_dependencies(ShadowLauncher SLUpdater)

# QML: bundled in qrc for memory-mapped loading + runtime caching
# Dev mode: set SHADOW_DEV=1 to load from filesystem for hot-reload
qt6_add_resources(ShadowLauncher "qml_resources"
    PREFIX "/qt/qml/ShadowLauncher"
    FILES
        qml/shaders/blur.frag.qsb
        qml/AgreementOverlay.qml
        qml/AnimationTokens.qml
        qml/BackgroundCropOverlay.qml
        qml/ConfirmDialog.qml
        qml/CrashDialog.qml
        qml/DetailInfoCard.qml
        qml/DetailVersionCard.qml
        qml/DownloadPage.qml
        qml/ExpandableGroupCard.qml
    qml/ExportModpackSection.qml
        qml/HomePage.qml
        qml/InlineToast.qml
        qml/InstallConfigOverlay.qml
        qml/InstallPage.qml
        qml/KillButton.qml
        qml/LaunchOverlay.qml
        qml/ToastStyleWarning.qml
        qml/ToastStyleSuccess.qml
        qml/MainWindow.qml
        qml/BetaKeyDialog.qml
        qml/MinecraftHead2D.qml
        qml/ModLoaderCard.qml
        qml/ModDetailPage.qml
        qml/ModpackDetailPage.qml
        qml/MultiplayerPage.qml
        qml/MultiplayerStateDot.qml
        qml/MultiplayerStateIndicator.qml
        qml/MultiplayerPlayerCard.qml
        qml/MultiplayerRoomCodeCard.qml
        qml/MultiplayerNetworkPanel.qml
        qml/MultiplayerHelpPanel.qml
        qml/ResourcePackDetailPage.qml
        qml/SettingsGeneralPage.qml
        qml/SettingsJavaPage.qml
        qml/ShaderDetailPage.qml
        qml/StatsPage.qml
        qml/SettingsMemoryPage.qml
        qml/SettingsPage.qml
        qml/ShadowButton.qml
        qml/ShadowSwitch.qml
        qml/SplashWindow.qml
        qml/SubPageOverlays.qml
        qml/ToastManager.qml
        qml/VersionSelectOverlay.qml
        qml/VersionSelectPage.qml
        qml/VersionSettingsOverlay.qml
        qml/VersionSettingsPage.qml
        qml/VersionMemorySection.qml
        qml/VersionLaunchSection.qml
        qml/ModpackImportOverlay.qml
        qml/ModpackInfoPanel.qml
        qml/StyleTokens.qml
        qml/JavaPage.qml
        qml/SettingsMemorySection.qml
        qml/DebugPanel.qml
        qml/DebugWindow.qml
        qml/BackButton.qml
        qml/DownloadCard.qml
        qml/DownloadCardTagRow.qml
        qml/DownloadQueueCard.qml
        qml/DataPackDetailPage.qml
        qml/DownloadQueuePanel.qml
        qml/FilterCard.qml
        qml/LoadStatus.qml
        qml/PaginationFooter.qml
        qml/ProfileSelectPopup.qml
        qml/RefreshButton.qml
        qml/SearchBox.qml
        qml/SelectionPopup.qml
        qml/GenericPopup.qml
        qml/ShadowDropdown.qml
        qml/LabeledDropdown.qml
        qml/ShadowIconButton.qml
        qml/VersionCard.qml
        qml/InputBox.qml
        qml/InstallProgressPage.qml
        qml/LoadingSpinner.qml
        qml/SettingsExperimentalPage.qml
        qml/animation_tokens.json
        qml/qmldir
)
target_include_directories(ShadowLauncher PRIVATE src)

# 鈹€鈹€ Lucide icon resources (must mirror dev-mode copy: icons copied into qml/icons/) 鈹€鈹€
qt6_add_resources(ShadowLauncher "icon_resources"
    PREFIX "/qt/qml/ShadowLauncher/qml"
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
        icons/lucide/download-cloud.svg

        # ── Version type icons (Blocks) ──
        icons/blocks/Anvil.png
        icons/blocks/CobbleStone.png
        icons/blocks/CommandBlock.png
        icons/blocks/Egg.png
        icons/blocks/Fabric.png
        icons/blocks/GoldBlock.png
        icons/blocks/Grass.png
        icons/blocks/GrassPath.png
        icons/blocks/NeoForge.png
        icons/blocks/OptiFabric.png
        icons/blocks/RedstoneBlock.png
        icons/blocks/RedstoneLampOff.png
        icons/blocks/RedstoneLampOn.png

        # ── Release type icons ──
        icons/types/Alpha.png
        icons/types/Beta.png
        icons/types/Release.png
)

# 鈹€鈹€ Agreement HTML resources (embedded for offline access) 鈹€鈹€
qt6_add_resources(ShadowLauncher "agreement_resources"
    PREFIX "/qt/qml/ShadowLauncher/agreements"
    FILES
        qml/agreements/beta_agreement.html
        qml/agreements/terms_of_service.html
        qml/agreements/privacy_policy.html
)

# ── Bootstrap tools (embedded installer helper JARs) ──
qt6_add_resources(ShadowLauncher "tools_resources"
    PREFIX "/"
    FILES
        resources/tools/forge-installer.jar
        resources/tools/java-wrapper.jar
        resources/tools/ProGuardToTSRG.class
)

# Cape textures (Wardrobe)
qt6_add_resources(ShadowLauncher "cape_resources"
    PREFIX "/qt/qml/ShadowLauncher/resources/capes"
    FILES
        resources/capes/15th_anniversary.png
        resources/capes/builder.png
        resources/capes/cherry_blossom.png
        resources/capes/common.png
        resources/capes/copper.png
        resources/capes/followers.png
        resources/capes/founders.png
        resources/capes/home.png
        resources/capes/mcc_15th_year.png
        resources/capes/menace.png
        resources/capes/migrator.png
        resources/capes/minecon_2011.png
        resources/capes/minecon_2012.png
        resources/capes/minecon_2013.png
        resources/capes/minecon_2015.png
        resources/capes/minecon_2016.png
        resources/capes/minecraft_experience.png
        resources/capes/mojang.png
        resources/capes/mojang_studios.png
        resources/capes/pan.png
        resources/capes/purple_heart.png
        resources/capes/realms_mapmaker.png
        resources/capes/translator_chinese.png
        resources/capes/vanilla.png
        resources/capes/yearn.png
        resources/capes/zombie_horse.png
)


# 鈹€鈹€ Multi-language translations 鈹€鈹€
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

# 鈹€鈹€ libwebp (static, for webp鈫扨NG icon decoding) 鈹€鈹€
set(WEBP_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/libwebp")
target_include_directories(ShadowLauncher PRIVATE "${WEBP_ROOT}/include")
target_link_directories(ShadowLauncher PRIVATE "${WEBP_ROOT}/lib")

# 鈹€鈹€ miniz (public domain, ZIP 解压 — 整合包导入用) 鈹€鈹€
target_include_directories(ShadowLauncher PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/miniz")

# Define WEBP_STATIC_LINK for static lib (needed on Windows)
target_compile_definitions(ShadowLauncher PRIVATE
    WEBP_STATIC_LINK
    SHADOW_DISPLAY_VERSION="v1.0.0"
    SHADOW_QT_VERSION="${Qt6_VERSION}"
    SHADOW_RESOURCE_EPOCH=1
)

# Beta gate 开关：ON 时定义 SHADOW_ENABLE_BETA_GATE=1（main_release 启用内测密钥闸门）
if(SHADOW_ENABLE_BETA_GATE)
    target_compile_definitions(ShadowLauncher PRIVATE SHADOW_ENABLE_BETA_GATE=1)
endif()

target_link_libraries(ShadowLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::CorePrivate
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
    Qt6::Concurrent
    Qt6::Network
    Qt6::Widgets
    bcrypt
    nlohmann_json
    libwebp
    bcrypt
    crypt32
)

# cpr::cpr 鈥?added when libcurl is installed

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
    add_custom_command(TARGET ShadowLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/qml/agreements"
            "$<TARGET_FILE_DIR:ShadowLauncher>/qml/agreements"
        COMMENT "Copying agreement HTMLs for dev-mode..."
    )
endif()

# ============================================================
# Modpack 模块自测（可选目标，不参与主程序构建）
#   cmake -S . -B build-test -DSHADOW_BUILD_MODPACK_SELFTEST=ON
# ============================================================
option(SHADOW_BUILD_MODPACK_SELFTEST "Build modpack self-test executable" OFF)

if(SHADOW_BUILD_MODPACK_SELFTEST)
    add_executable(ModpackSelfTest
        tests/test_modpack.cpp
        tests/test_zip_helper.cpp
        src/core/modpack/modpack_common.cpp
        src/core/modpack/zip_archive.cpp
        src/core/modpack/modpack_parser.cpp
        src/core/modpack/modpack_downloader.cpp
        src/core/modpack/mod_download_engine.cpp
        src/core/file_downloader.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
        thirdparty/miniz/miniz.c
    )
    target_include_directories(ModpackSelfTest PRIVATE src thirdparty/miniz)
    target_link_libraries(ModpackSelfTest PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)

    # 精卫补位重试回归（2026-08-12）：每文件重试预算 + 任务层重试结果可见性
    # 用法：JingweiRetryTest.exe（本地 HTTP 服务器，无需外网）
    add_executable(JingweiRetryTest
        tests/test_jingwei_retry.cpp
        src/core/modpack/modpack_common.cpp
        src/core/modpack/modpack_downloader.cpp
        src/core/modpack/mod_download_engine.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(JingweiRetryTest PRIVATE src)
    target_link_libraries(JingweiRetryTest PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)

    # CF API Key 加密互操作测试（验证 Python 工具链生成的密文可被 C++ 解密）
    add_executable(CfCryptoInteropTest
        tests/test_cf_crypto_interop.cpp
    )
    target_include_directories(CfCryptoInteropTest PRIVATE src)
    target_link_libraries(CfCryptoInteropTest PRIVATE Qt6::Core bcrypt)

    # 驿道 v2 集成测试：实测分片下载（用法: YidaoTest <url> [expectedSize] [outPath]）
    add_executable(YidaoTest
        tests/test_yidao.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(YidaoTest PRIVATE src)
    target_link_libraries(YidaoTest PRIVATE Qt6::Core Qt6::Network)

    # 夸父（FileDownloader）单文件下载测试
    add_executable(FDTest
        tests/test_fd.cpp
        src/core/file_downloader.cpp
        src/utils/logger.cpp
    )

    # 整合包导出测试（临时）：ModpackExportTest <gameDir> <versionId> [outPath]
    add_executable(ModpackExportTest
        tests/test_export.cpp
        src/core/modpack/modpack_exporter.cpp
        src/core/modpack/modpack_parser.cpp
        src/core/modpack/zip_archive.cpp
        src/core/modpack/modpack_common.cpp
        src/utils/logger.cpp
        thirdparty/miniz/miniz.c
    )
    target_include_directories(ModpackExportTest PRIVATE src thirdparty/miniz)
    target_link_libraries(ModpackExportTest PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)
    target_include_directories(FDTest PRIVATE src)
    target_link_libraries(FDTest PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)

    # CF 适配器自测：搜索统一模型 + 文件→版本转换 + 分类表
    add_executable(CfApiTest
        tests/test_cf_api.cpp
        src/core/cf_api.cpp
        src/core/resource_fetch_engine.cpp
        src/utils/logger.cpp
    )
    target_include_directories(CfApiTest PRIVATE src "${WEBP_ROOT}/include")
    target_link_directories(CfApiTest PRIVATE "${WEBP_ROOT}/lib")
    target_compile_definitions(CfApiTest PRIVATE WEBP_STATIC_LINK)
    target_link_libraries(CfApiTest PRIVATE Qt6::Core Qt6::Network Qt6::Gui libwebp bcrypt)

    # 资源拉取引擎（司南）自测：搜索缓存命中 + 图标下载 + 缩略图生成
    add_executable(ResourceFetchEngineTest
        tests/test_resource_fetch_engine.cpp
        src/core/resource_fetch_engine.cpp
        src/utils/logger.cpp
    )
    target_include_directories(ResourceFetchEngineTest PRIVATE src "${WEBP_ROOT}/include")
    target_link_directories(ResourceFetchEngineTest PRIVATE "${WEBP_ROOT}/lib")
    target_compile_definitions(ResourceFetchEngineTest PRIVATE WEBP_STATIC_LINK)
    target_link_libraries(ResourceFetchEngineTest PRIVATE Qt6::Core Qt6::Network Qt6::Gui libwebp)

    # 图像解码诊断（webp 支持验证）
    add_executable(DiagImage tests/diag_image.cpp)
    target_include_directories(DiagImage PRIVATE "${WEBP_ROOT}/include")
    target_link_directories(DiagImage PRIVATE "${WEBP_ROOT}/lib")
    target_compile_definitions(DiagImage PRIVATE WEBP_STATIC_LINK)
    target_link_libraries(DiagImage PRIVATE Qt6::Gui Qt6::Core libwebp)

    # Forge 旧版安装集成测试（Legacy 2/3 统一路线）：真实网络跑 installForge
    add_executable(Forge147Test
        tests/test_forge147.cpp
        src/core/mod_loader_installer.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(Forge147Test PRIVATE src)
    target_link_libraries(Forge147Test PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate)

    # 驿道下载 1.4.7 universal.zip（merged fallback 目标）完整性验证
    add_executable(Yidao147Test
        tests/test_yidao147.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(Yidao147Test PRIVATE src)
    target_link_libraries(Yidao147Test PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)

    # Mod 启禁用 + 启动脚本导出验证
    add_executable(ToggleScriptTest
        tests/test_toggle_script.cpp
        src/core/local_mod_manager.cpp
        src/core/launcher.cpp
        src/core/mc_language.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(ToggleScriptTest PRIVATE src)
    target_link_libraries(ToggleScriptTest PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent Qt6::Gui Qt6::GuiPrivate)

    # 山海经 in-flight 挂起看门狗验证（2026-08-10）：挂起源 30s 无数据 → abort 换源 → 成功
    add_executable(AssetWatchdogTest
        tests/test_asset_watchdog.cpp
        src/core/asset_downloader.cpp
        src/utils/logger.cpp
    )
    target_include_directories(AssetWatchdogTest PRIVATE src)
    target_link_libraries(AssetWatchdogTest PRIVATE Qt6::Core Qt6::Network Qt6::Concurrent)

    # 版本隔离 game 目录布局感知验证（散文件→版本根 / game非空→game）
    add_executable(IsoGdTest
        tests/test_isogd.cpp
        src/core/version_isolation.cpp
        src/utils/logger.cpp
    )
    target_include_directories(IsoGdTest PRIVATE src)
    target_link_libraries(IsoGdTest PRIVATE Qt6::Core)

    # 启动自动安装 Java 组件验证（真实网络 Tuna，worker 下载/解压 + 主线程回调）
    # 用法: JavaAutoInstallTest <major>（默认 8；已缓存直接完成）
    add_executable(JavaAutoInstallTest
        tests/test_java_autoinstall.cpp
        src/core/java_runtime_installer.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(JavaAutoInstallTest PRIVATE src)
    target_link_libraries(JavaAutoInstallTest PRIVATE Qt6::Core Qt6::Network Qt6::Gui Qt6::GuiPrivate)

    # Java 需求规则验证（1.16.5-→8 / 1.17→16 / 1.18-1.20.4→17 / 1.20.5-1.21→21 / 22+→25）
    add_executable(JavaRulesTest
        tests/test_java_rules.cpp
        src/utils/logger.cpp
    )
    target_include_directories(JavaRulesTest PRIVATE src)
    target_link_libraries(JavaRulesTest PRIVATE Qt6::Core)

    # Java 自动安装取消语义验证（已缓存不误删 / 失败清理残留）
    add_executable(JavaCancelTest
        tests/test_java_cancel.cpp
        src/core/java_runtime_installer.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(JavaCancelTest PRIVATE src)
    target_link_libraries(JavaCancelTest PRIVATE Qt6::Core Qt6::Network Qt6::Gui Qt6::GuiPrivate)

    # Java 列表扫描验证（MC 官方 runtime 路径可达性）
    add_executable(JavaScanTest
        tests/test_java_scan.cpp
        src/core/java_runtime_installer.cpp
        src/core/http_client.cpp
        src/utils/logger.cpp
    )
    target_include_directories(JavaScanTest PRIVATE src)
    target_link_libraries(JavaScanTest PRIVATE Qt6::Core Qt6::Network Qt6::Gui Qt6::GuiPrivate)

    # 启动细节工程验证（2026-08-08 低垂果实批）：GC 四档 / 全屏 / 自动进服新旧语法 /
    # 设置导入导出往返 / launcher_profiles 结构 / 脚本冒烟
    add_executable(LaunchDetailTest
        tests/test_launch_detail.cpp
        src/core/launcher.cpp
        src/core/mc_language.cpp
        src/backend/settings_backend.cpp
        src/core/version_isolation.cpp
        src/utils/logger.cpp
    )
    target_include_directories(LaunchDetailTest PRIVATE src)
    target_link_libraries(LaunchDetailTest PRIVATE Qt6::Core Qt6::Network Qt6::Gui Qt6::Widgets Qt6::GuiPrivate)
endif()

# ============================================================
# Install
# ============================================================

install(TARGETS ShadowLauncher
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# SLUpdater — standalone self-update helper (no Qt dependency)
# ============================================================
add_executable(SLUpdater
    update/SLUpdater.cpp
)
set_target_properties(SLUpdater PROPERTIES
    WIN32_EXECUTABLE TRUE
    AUTOMOC OFF
)
target_compile_options(SLUpdater PRIVATE /utf-8)
target_link_libraries(SLUpdater PRIVATE
    shell32
)
