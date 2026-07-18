cmake_minimum_required(VERSION 3.16)
project(cef_helper LANGUAGES CXX)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/cef")

add_executable(cef_helper
    main.cpp
    helper_app.cpp
    helper_client.cpp
)

target_include_directories(cef_helper PRIVATE ${CEF_ROOT})
target_compile_definitions(cef_helper PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN NDEBUG)
target_compile_features(cef_helper PRIVATE cxx_std_20)
set_target_properties(cef_helper PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded"
)

# Use cefsimple's verified libcef_dll_wrapper + libcef.lib
target_link_libraries(cef_helper PRIVATE
    "${CEF_ROOT}/build_test/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib"
    "${CEF_ROOT}/Release/libcef.lib"
)

set_target_properties(cef_helper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../build/Release"
)
