cmake_minimum_required(VERSION 3.20)
project(PiHifiPlayer VERSION 1.1.0 LANGUAGES C CXX)

# 1.强制使用 Moderin C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. 导出编译命令数据库 (供 VSCode / Clangd / Cursor 智能跳转和补全)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 3. 严格编译器警告等级
add_compile_options(-Wall -Wextra -Wpedantic)

# 4. 平台差异化依赖与优化配置
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    # 在 Linux/树莓派 等环境下若未安装 libsqlite3-dev，自动使用系统已有的 libsqlite3.so.0 动态库 + 内置头文件
    find_library(SQLITE3_LIB NAMES sqlite3 sqlite3.so.0 libsqlite3.so.0 libsqlite3.so PATHS /usr/lib /usr/lib/aarch64-linux-gnu /usr/local/lib)
    if(SQLITE3_LIB)
        add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            IMPORTED_LOCATION "${SQLITE3_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/include"
        )
        set(SQLite3_FOUND TRUE)
        message(STATUS ">> [SQLite3] 成功定位系统运行时动态库: ${SQLITE3_LIB} (使用内置头文件)")
    else()
        message(FATAL_ERROR "Could NOT find SQLite3 library")
    endif()
elseif(NOT TARGET SQLite3::SQLite3 AND TARGET SQLite::SQLite3)
    add_library(SQLite3::SQLite3 INTERFACE IMPORTED)
    target_link_libraries(SQLite3::SQLite3 INTERFACE SQLite::SQLite3)
endif()
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum OS X version")
    endif()
    message(STATUS ">> [HiFi Build] 当前检测为 macOS 开发调试平台 (Target macOS ${CMAKE_OSX_DEPLOYMENT_TARGET})")
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)
    add_compile_definitions(HIFI_PLATFORM_MAC=1 GL_SILENCE_DEPRECATION=1)
elseif(UNIX AND NOT APPLE)
    message(STATUS ">> [HiFi Build] 当前检测为 Linux / 树莓派 5 生产部署平台")
     # 针对树莓派 5 Cortex-A76 架构开启极端性能优化
    add_compile_options(-O3 -pipe -mcpu=cortex-a76+crypto)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(GLESV2 REQUIRED glesv2)
    # 后续关卡按需启用外设驱动:
    # pkg_check_modules(ALSA REQUIRED alsa)
    # pkg_check_modules(GPIOD REQUIRED libgpiod)

    add_compile_definitions(HIFI_PLATFORM_RPI=1)
endif()

# 5. 源码收集
# 引入专有独立的发烧音频核心 SDK
add_subdirectory(audio_engine)

# Dear ImGui 核心与 SDL2 + OpenGL3 后端实现
set(
    IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
        src/views/EQConfigView.cpp
        include/views/EQConfigView.hpp
)

# 收集项目自身所有 C++ 源码
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# 6. 生成目标可执行程序 (产物将命名为 PiHifiPlayer)
add_executable(${PROJECT_NAME} ${APP_SOURCES} ${IMGUI_SOURCES})

# 7. 头文件包含路径 (使用现代 CMake 的 target 级别作用域)
target_include_directories(
    ${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/public
    ${CMAKE_CURRENT_SOURCE_DIR}/include/types
    ${CMAKE_CURRENT_SOURCE_DIR}/include/views
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/include/themes
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${SDL2_INCLUDE_DIRS}
)

# 8. 链接依赖库
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        audio_engine
        SQLite3::SQLite3
        ${SDL2_LIBRARIES}
        OpenGL::GL
        "-framework CoreText"
    )

    # 9. macOS DMG 独立安装包一键打包 Target (cmake --build build --target dmg)
    add_custom_target(dmg
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package_mac_dmg.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "打包生成独立 macOS DMG 安装镜像 (PiHifiPlayer.dmg)..."
        USES_TERMINAL
    )

    # 10. 树莓派 5 一键部署与运行 Target (可在 CLion 中直接点击 ▶️ 运行)
    add_custom_target(deploy_pi
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy_pi.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "🚀 [HiFi Deploy] 正在一键同步、编译并启动到树莓派 5..."
        USES_TERMINAL
    )

    # 11. iOS iPhone 模拟器一键运行 Target (可在 CLion 中直接点击 ▶️ 运行)
    add_custom_target(ios_simulator_iphone
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_ios.sh iphone
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "📱 [iOS] 正在编译并在 iPhone 模拟器启动 PiHiEndMusic..."
        USES_TERMINAL
    )

    # 12. iOS iPad 模拟器一键运行 Target (可在 CLion 中直接点击 ▶️ 运行)
    add_custom_target(ios_simulator_ipad
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_ios.sh ipad
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "📱 [iOS] 正在编译并在 iPad 模拟器启动 PiHiEndMusic..."
        USES_TERMINAL
    )

    # 13. iOS 项目完整构建 Target
    add_custom_target(ios_build
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_ios.sh build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "📱 [iOS] 正在编译 iOS 核心库与 App..."
        USES_TERMINAL
    )

    # 14. iOS iPhone 物理真机一键运行 Target (可在 CLion 中直接点击 ▶️ 运行)
    add_custom_target(ios_device_iphone
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_ios.sh device
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "📲 [iOS] 正在编译并部署到物理 iPhone 真机..."
        USES_TERMINAL
    )

    # 15. Android AVD 模拟器一键运行 Target (可在 CLion 中直接点击 ▶️ 运行)
    add_custom_target(android_emulator
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_android.sh emulator
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "🤖 [Android] 正在编译并在 AVD 模拟器启动 PiHiEndMusic..."
        USES_TERMINAL
    )

    # 16. Android 物理真机一键运行 Target (可在 CLion 中直接点击 ▶️ 运行)
    add_custom_target(android_device
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_android.sh device
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "📲 [Android] 正在编译并部署到物理 Android 真机..."
        USES_TERMINAL
    )

    # 17. Android APK 完整构建 Target
    add_custom_target(android_build
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_android.sh build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "📦 [Android] 正在编译 Android Debug APK..."
        USES_TERMINAL
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        audio_engine
        SQLite3::SQLite3
        pthread
        ${SDL2_LIBRARIES}
        ${GLESV2_LIBRARIES}
    )
endif()


