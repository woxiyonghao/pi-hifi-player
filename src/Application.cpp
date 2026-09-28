#include "Application.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

#if defined(__APPLE__)
#include <OpenGL/gl3.h> // macOS 使用原生 OpenGL 3.2 Core
#else
#include <SDL2/SDL_opengles2.h> // 树莓派 5 使用 OpenGL ES 2.0
#endif

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::init() {
    if (!initSDL()) return false;
    if (!initOpenGL()) return false;
    if (!initWindow()) return false;
    if (!initImGui()) return false;
    initData();

    running_ = true;
    std::cout << "[Application] 纯音数播图形与事件中枢初始化完毕，锁定 60fps 原生垂直同步。" << std::endl;
    return true;
}

bool Application::initSDL() {
    // 强制开启 SDL2 触摸转鼠标模拟提示，适配各类触控屏
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::cerr << "[SDL] 初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }
    return true;
}

bool Application::initOpenGL() {
#if defined(__APPLE__)
    glsl_version_ = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    glsl_version_ = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // 双缓冲防撕裂，24位深度
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    return true;
}

bool Application::initWindow() {
    // 物理拟合微雪 7 寸 QLED 纯平触控屏 (1024×600)
    window_ = SDL_CreateWindow(
        "PiHifiPlayer - McIntosh Dual VU Meter (1024x600)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window_) {
        std::cerr << "[Window] 窗口创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "[GL] 上下文创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // 锁定 V-Sync
    return true;
}

bool Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // 纯音设备免写配置盘
    ImGui::StyleColorsDark();

    // 字体字阶初始化
    Fonts::initialize(io);

    // 绑定 SDL2 与 OpenGL3 后端
    if (!ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_)) return false;
    if (!ImGui_ImplOpenGL3_Init(glsl_version_)) return false;

    return true;
}

void Application::initData() {
    // 初始化发烧测试歌单 (Mock 数据，后续可由 MusicScanManager 替换)
    {
        Playlist p1(101, "蔡琴经典试音");
        p1.addTrack(Track{1, "渡口", "蔡琴", "民歌蔡琴", "/music/dukou.flac",
                          AudioFormat::FLAC, 192000, 24, 215});
        p1.addTrack(Track{2, "被遗忘的时光", "蔡琴", "民歌蔡琴", "/music/shiguang.flac",
                          AudioFormat::FLAC, 192000, 24, 180});

        Playlist p2(102, "交响大动态母带");
        p2.addTrack(Track{3, "1812序曲", "柴可夫斯基", "Mercury", "/music/1812.dsf",
                          AudioFormat::DSD_DSF, 2822400, 1, 940});

        Playlist p3(103, "爵士黑胶典藏");

        playlists_.push_back(p1);
        playlists_.push_back(p2);
        playlists_.push_back(p3);
    }

    // 侧边栏默认高亮首个歌单，并绑定新建歌单交互
    sidebar_.setSelectedPlaylistId(101);
    sidebar_.setOnCreatePlaylist([this]() {
        uint64_t next_id = playlists_.empty() ? 101 : (playlists_.back().getId() + 1);
        std::string name = "新发烧歌单 " + std::to_string(next_id - 100);
        playlists_.emplace_back(next_id, name);
        sidebar_.setSelectedPlaylistId(next_id);
    });
}

void Application::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        // 原生多点触摸 (TouchScreen) 转 ImGui 鼠标交互事件 (针对微雪触控屏)
        ImGuiIO& io = ImGui::GetIO();
        if (event.type == SDL_FINGERDOWN) {
            float x = event.tfinger.x * 1024.0f;
            float y = event.tfinger.y * 600.0f;
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(x, y);
            io.AddMouseButtonEvent(0, true);
        } else if (event.type == SDL_FINGERUP) {
            float x = event.tfinger.x * 1024.0f;
            float y = event.tfinger.y * 600.0f;
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(x, y);
            io.AddMouseButtonEvent(0, false);
        } else if (event.type == SDL_FINGERMOTION) {
            float x = event.tfinger.x * 1024.0f;
            float y = event.tfinger.y * 600.0f;
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(x, y);
        }

        if (event.type == SDL_QUIT) {
            running_ = false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            running_ = false;
        }
    }
}

void Application::update(float dt) {
    // 推进播放器时间轴心跳
    PlayerAdmin::getInstance().update(dt);

    // 动圈表头节拍激励步进
    sim_time_ += 0.035f;
}

void Application::renderBackground(float screen_w, float screen_h) {
    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();

    // 1. 铺设整个 App 的基准发烧底色 (完全对齐原设计的区域与色值规范)
    // 左侧侧边栏暗色基底 (0 ~ 230)
    bg_dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(UIConfig::Layout::SidebarWidth, screen_h), UIConfig::Color::WindowBg);
    // 右侧主舞台深空基底 (230 ~ screen_w)
    bg_dl->AddRectFilled(ImVec2(UIConfig::Layout::SidebarWidth, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    // 2. 如果未播放，保持现在的颜色 (0 个方块，0 动效，完全纯净)
    auto& player = PlayerAdmin::getInstance();
    if (!player.isPlaying()) {
        return;
    }

    // 3. 如果播放，在整个 App 的底，渲染音乐动效 (LED 矩阵频谱律动)
    // 获取 12 频段实时音频幅度
    float levels12[12] = {0.0f};
    player.getSpectrumLevels(levels12, 12);

    // 几何排版参数：左右对齐发烧容器外边距 (16px ~ 1008px)
    const float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    const float total_w = screen_w - margin_x * 2.0f;          // 992.0f
    const int num_cols = 48;                                   // 48 列超宽音轨点阵
    const float gap_x = 4.0f;                                  // 列间距
    const float col_w = (total_w - (num_cols - 1) * gap_x) / num_cols; // ~16.75px
    
    const int num_rows = 69;                                   // 69 行分段 LED (全屏满屏高度贯通)
    const float seg_h = 6.0f;                                  // 每个方块高度
    const float gap_y = 2.5f;                                  // 方块纵向间距
    const float seg_round = 1.2f;                              // 圆角微弧度
    const float bot_y = screen_h - 8.0f;                       // 距底部屏幕边缘 8px 起振 (最高行达 y = 8px)

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 常规点亮方块色 (半透明玫红，柔和穿透毛玻璃卡片，通透而不遮挡前景文字)
    const ImU32 lit_color = IM_COL32(r, g, b, 140);
    // 柱顶峰值高亮点 (Peak indicator, 强化动态层次感)
    const ImU32 peak_color = IM_COL32(std::min(255u, r + 20), std::min(255u, g + 60), std::min(255u, b + 60), 220);
    // 播放时隐约的点阵暗底 (极低透明度，凸显专业仪器质感)
    const ImU32 unlit_color = IM_COL32(255, 255, 255, 5);

    // 全屏全景氛围微辉光 (随着整体低频能量呼吸涌动)
    float bass_energy = (levels12[0] + levels12[1] + levels12[2]) / 3.0f;
    int glow_alpha = static_cast<int>(bass_energy * 32.0f);
    if (glow_alpha > 0) {
        bg_dl->AddRectFilledMultiColor(
            ImVec2(margin_x, 0.0f),
            ImVec2(screen_w - margin_x, screen_h),
            IM_COL32(r, g, b, 0),
            IM_COL32(r, g, b, 0),
            IM_COL32(r, g, b, glow_alpha),
            IM_COL32(r, g, b, glow_alpha)
        );
    }

    // 平滑插值绘制 48 列分段 LED 矩阵
    for (int c = 0; c < num_cols; ++c) {
        float x0 = margin_x + c * (col_w + gap_x);
        float x1 = x0 + col_w;

        // 平滑余弦插值获取当前列的连续频段能量
        float norm_x = static_cast<float>(c) / static_cast<float>(num_cols - 1);
        float pos = norm_x * 11.0f;
        int idx0 = static_cast<int>(pos);
        int idx1 = std::min(idx0 + 1, 11);
        float frac = pos - static_cast<float>(idx0);
        float smooth_t = (1.0f - std::cos(frac * 3.14159265f)) * 0.5f;
        float level = levels12[idx0] * (1.0f - smooth_t) + levels12[idx1] * smooth_t;

        // 大动态激荡曲线：确保高频与低频爆发时能满屏激荡涌动 (贯穿整个 600px 屏幕)
        float dynamic_level = std::clamp(std::pow(level, 0.65f) * 1.35f, 0.0f, 1.0f);

        int active_count = static_cast<int>(std::round(dynamic_level * num_rows));
        active_count = std::clamp(active_count, 0, num_rows);

        for (int r_idx = 0; r_idx < num_rows; ++r_idx) {
            float y1 = bot_y - r_idx * (seg_h + gap_y);
            float y0 = y1 - seg_h;

            bool is_lit = (r_idx < active_count);
            bool is_peak = (r_idx == active_count - 1 && active_count > 0);

            if (is_lit) {
                ImU32 col = is_peak ? peak_color : lit_color;
                bg_dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col, seg_round);
            } else if ((unlit_color & IM_COL32_A_MASK) != 0) {
                bg_dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), unlit_color, seg_round);
            }
        }
    }
}

void Application::render() {
    // 1. 开启 ImGui 帧缓冲
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int display_w = 0, display_h = 0;
    SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
    const float screen_w = (display_w > 0) ? static_cast<float>(display_w) : 1024.0f;
    const float screen_h = (display_h > 0) ? static_cast<float>(display_h) : 600.0f;

    // [全景底层背景与音乐律动动效] (位于所有窗口最底层)
    renderBackground(screen_w, screen_h);

    // 2. 调度发烧 UI 三驾马车布局渲染
    // [左侧] 导航与歌单侧边栏 (230 × 600)
    sidebar_.render(playlists_, 230.0f, screen_h);

    // [右上方] 中央主舞台区域 (794 × 600)
    main_stage_.render(sidebar_.getCurrentTab(), sidebar_.getSelectedPlaylistId(), playlists_, 
                       230.0f, 0.0f, screen_w - 230.0f, screen_h);

    // [底部] 播放控制胶囊栏 (1024 × 600 屏幕下部)
    bottom_bar_.render(screen_w, screen_h);

    // 3. 提交绘图并进行 OpenGL 光栅化清屏
    ImGui::Render();
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.015f, 0.03f, 0.06f, 1.0f); // 经典麦景图深蓝黑底
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

int Application::run() {
    Uint64 last_time = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running_) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>((now - last_time) / freq);
        last_time = now;

        pollEvents();
        update(dt);
        render();
    }

    return 0;
}

void Application::shutdown() {
    if (gl_context_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    std::cout << "[Application] 系统已优雅安全退出。" << std::endl;
}
