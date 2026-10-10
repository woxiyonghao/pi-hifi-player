cmake_minimum_required(VERSION 3.20)
project(PiHifiCore LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

# 启用 LLVM libc++ C++20 stop_token 与 jthread 特性
add_compile_definitions(_LIBCPP_ENABLE_EXPERIMENTAL=1)

# 子模块：音频核心
add_subdirectory("${ROOT_DIR}/audio_engine" "${CMAKE_CURRENT_BINARY_DIR}/audio_engine")

# 引入 Dear ImGui 核心与 GLES3 后端
set(IMGUI_SOURCES
    "${ROOT_DIR}/third_party/imgui/imgui.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui_draw.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui_tables.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui_widgets.cpp"
    "${ROOT_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp"
)

# 收集项目除了 main.cpp 和 Application.cpp 之外的所有源码
file(GLOB_RECURSE ALL_APP_SOURCES CONFIGURE_DEPENDS "${ROOT_DIR}/src/*.cpp")
list(FILTER ALL_APP_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER ALL_APP_SOURCES EXCLUDE REGEX ".*/Application\\.cpp$")

# 包含 SQLite3 独立自包含源码
set(SQLITE3_SOURCES
    "${ROOT_DIR}/third_party/sqlite3/src/sqlite3.c"
)

add_library(PiHifiCore SHARED
    AndroidAppBridge.cpp
    ${IMGUI_SOURCES}
    ${SQLITE3_SOURCES}
    ${ALL_APP_SOURCES}
)

target_include_directories(PiHifiCore PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ROOT_DIR}/include"
    "${ROOT_DIR}/include/public"
    "${ROOT_DIR}/include/types"
    "${ROOT_DIR}/include/views"
    "${ROOT_DIR}/include/tools"
    "${ROOT_DIR}/include/services"
    "${ROOT_DIR}/include/themes"
    "${ROOT_DIR}/include/widgets"
    "${ROOT_DIR}/third_party/imgui"
    "${ROOT_DIR}/third_party/imgui/backends"
    "${ROOT_DIR}/third_party/sqlite3/include"
    "${ROOT_DIR}/audio_engine/include"
)

target_compile_definitions(PiHifiCore PRIVATE
    HIFI_PLATFORM_ANDROID=1
    IMGUI_IMPL_OPENGL_ES3=1
)

target_link_libraries(PiHifiCore PRIVATE
    audio_engine
    GLESv3
    EGL
    android
    log
)
