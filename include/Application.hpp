#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include "types/MusicModel.hpp"
#include "views/SidebarView.hpp"
#include "views/MainStageView.hpp"
#include "views/BottomBarView.hpp"
#include "themes/VUMeterRenderer.hpp"
#include "themes/AccuphaseMeterRenderer.hpp"
#include "themes/TapeReelRenderer.hpp"
#include "themes/SiriWaveformRenderer.hpp"
#include "themes/SiriOrbRenderer.hpp"
#include "themes/AudioBubblesRenderer.hpp"
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
    void renderBackground(float screen_w, float screen_h);
    void renderCreatePlaylistModal(float screen_w, float screen_h);
    void resetIdle();

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    const char* glsl_version_ = "#version 150";

    bool running_ = false;
    float sim_time_ = 0.0f;

    // 数据源与状态模型
    std::vector<Playlist> playlists_;

    // 新建播放列表模态弹窗状态
    bool show_create_playlist_modal_ = false;
    bool create_playlist_focus_needed_ = false;
    char new_playlist_name_buf_[64] = "";

    // 发烧 UI 界面核心三驾马车与主题引擎
    SidebarView sidebar_;
    MainStageView main_stage_;
    BottomBarView bottom_bar_;
    VUMeterRenderer vu_renderer_;
    AccuphaseMeterRenderer accuphase_renderer_;
    TapeReelRenderer tape_renderer_;
    SiriWaveformRenderer siri_wave_renderer_;
    SiriOrbRenderer siri_orb_renderer_;
    AudioBubblesRenderer bubbles_renderer_;

    // 侧边栏持久化状态追踪
    SidebarTab last_saved_tab_ = SidebarTab::AllMusic;
    uint64_t last_saved_playlist_id_ = 0;

    // 空余时间全屏屏保与四角动画状态
    float idle_timer_ = 0.0f;
    bool is_fullscreen_idle_ = false;
    float anim_progress_ = 0.0f; // 0.0f (完全移入展出) ~ 1.0f (完全移出至四角)
};
