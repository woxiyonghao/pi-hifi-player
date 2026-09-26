cmake_minimum_required(VERSION 3.20)
project(PiHifiPlayer VERSION 1.0.0 LANGUAGES C CXX)

# 1.强制使用 Moderin C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. 导出编译命令数据库 (供 VSCode / Clangd / Cursor 智能跳转和补全)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 3. 严格编译器警告等级
add_compile_options(-Wall -Wextra -Wpedantic)

# 4. 平台差异化依赖与优化配置
if(APPLE)
    message(STATUS ">> [HiFi Build] 当前检测为 macOS 开发调试平台")
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)
    add_compile_definitions(HIFI_PLATFORM_MAC=1 GL_SILENCE_DEPRECATION=1)
elseif(UNIX AND NOT APPLE)
    message(STATUS ">> [HiFi Build] 当前检测为 Linux / 树莓派 5 生产部署平台")
     # 针对树莓派 5 Cortex-A76 架构开启极端性能优化
    add_compile_options(-O3 -pipe -march=armv8.2-a+crypto -mcpu=cortex-a76)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(GLESV2 REQUIRED glesv2)
    # 后续关卡按需启用外设驱动:
    # pkg_check_modules(ALSA REQUIRED alsa)
    # pkg_check_modules(GPIOD REQUIRED libgpiod)

    add_compile_definitions(HIFI_PLATFORM_RPI=1)
endif()

# 5. 源码收集
# Dear ImGui 核心与 SDL2 + OpenGL3 后端实现
set(
    IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

# 收集项目自身所有 C++ 源码
file(GLOB_RECURSE APP_SOURCES "src/*.cpp")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/themes
    ${CMAKE_CURRENT_SOURCE_DIR}/include/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${SDL2_INCLUDE_DIRS}
)

# 8. 链接依赖库
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SDL2_LIBRARIES}
        OpenGL::GL
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        ${SDL2_LIBRARIES}
        ${GLESV2_LIBRARIES}
    )
endif()
