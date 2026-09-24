#include "Application.hpp"
#include "public/Font.hpp"
#include <iostream>
#include <cmath>

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

void Application::render() {
    // 1. 开启 ImGui 帧缓冲
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // 2. 调度发烧 UI 三驾马车布局渲染
    // [左侧] 导航与歌单侧边栏 (230 × 600)
    sidebar_.render(playlists_, 230.0f, 600.0f);

    // [右上方] 中央主舞台区域 (794 × 600)
    main_stage_.render(sidebar_.getCurrentTab(), sidebar_.getSelectedPlaylistId(), playlists_, 
                       230.0f, 0.0f, 794.0f, 600.0f);

    // [底部] 播放控制胶囊栏 (1024 × 600 屏幕下部)
    bottom_bar_.render(1024.0f, 600.0f);

    // 3. 提交绘图并进行 OpenGL 光栅化清屏
    ImGui::Render();
    int display_w = 0, display_h = 0;
    SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
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
