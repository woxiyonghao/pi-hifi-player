#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include "types/MusicModel.hpp"
#include "views/SidebarView.hpp"
#include "views/MainStageView.hpp"
#include "views/BottomBarView.hpp"
#include "themes/VUMeterRenderer.hpp"
#include "tools/PlayerAdmin.hpp"

// ==============================================================================
// 纯音数播全局应用程序运行容器 (Application)
// 统一管控 SDL2 底层视窗、OpenGL 上下文、ImGui 胶水层与 60fps 帧心跳
// ==============================================================================
class Application {
public:
    Application();
    ~Application();

    // 禁用拷贝与移动
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // 初始化硬件窗口、图形后端与全局组件 (返回成功/失败)
    bool init();

    // 启动 60fps 主事件心跳驱动循环 (阻塞直至用户退出)
    int run();

    // 退出清理资源
    void shutdown();

private:
    bool initSDL();
    bool initOpenGL();
    bool initWindow();
    bool initImGui();
    void initData();

    void pollEvents();
    void update(float dt);
    void render();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    const char* glsl_version_ = "#version 150";

    bool running_ = false;
    float sim_time_ = 0.0f;

    // 数据源与状态模型
    std::vector<Playlist> playlists_;

    // 发烧 UI 界面核心三驾马车与主题引擎
    SidebarView sidebar_;
    MainStageView main_stage_;
    BottomBarView bottom_bar_;
    VUMeterRenderer vu_renderer_;
};
