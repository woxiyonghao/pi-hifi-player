#include <iostream>
#include <SDL2/SDL.h>

#if defined (__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <SDL2/SDL_opengles2.h>
#endif

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// C++20 [[maybe_unused]] 属性：明确告诉编译器这两个形参未在此处使用，消除编译器严格告警
int main([[maybe_unused]] int argc,[[maybe_unused]] char* argv[]) {
    std::cout << "[PiHifiPlayer] 启动发烧级纯音数播系统 (Modern C++20)..." << std::endl;

    // 1. 初始化 SDL2 底层子系统 (视频渲染、高精定时器与输入事件)
    if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS)){
        std::cerr << "SDL_Init 失败: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    // 2. 配置 OpenGL 上下文 (macOS 使用 OpenGL 3.2 Core 规范，树莓派后续使用 ES 2.0)
#if defined (__APPLE__)
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // 3. 创建 1024x600 纯平窗口 (1:1 像素拟合微雪 7 寸 QLED 屏)
    SDL_Window *window = SDL_CreateWindow(
        "PiHifiPlayer - 1024x600 Desktop Simulation", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        1024, 600, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window){
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // 开启 V-Sync 垂直同步，稳稳锁定 60 FPS

    // 4. 初始化 Dear ImGui 界面上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // 纯音嵌入式设备，禁用 imgui.ini 磁盘配置文件写入
    ImGui::StyleColorsDark();

    // 5. 绑定 SDL2 平台胶水层与 OpenGL3 渲染后端
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);
    std::cout << "[Ready] 渲染管线就绪，进入 60fps 主循环。按 ESC 退出。" << std::endl;

    // 6. 主事件与渲染循环 (60fps 心跳)
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (SDL_QUIT == event.type){
                running = false;
            }
            if (SDL_KEYDOWN == event.type && SDLK_ESCAPE == event.key.keysym.sym){
                running = false;
            }
        }

        // 启动 ImGui 新帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 关卡 1 起步验证卡片
        ImGui::SetNextWindowPos(ImVec2(320,220),ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(384,160),ImGuiCond_Always);
        ImGui::Begin("关卡 1: 起步点亮", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "PiHifiPlayer C++20 Core");
        ImGui::Separator();
        ImGui::Text("微雪 7.0寸 QLED 纯平屏: 1024 x 600");
        ImGui::Text("当前渲染刷新率: %.1f FPS", io.Framerate);
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "[√] 基础工程与 ImGui 循环点亮成功!");
        ImGui::End();

        // 后台缓冲清屏与绘制 (经典麦景图夜空深蓝黑底色: #050a14)
        ImGui::Render();
        int display_w,display_h;
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.02f, 0.04f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window); // 交换前后缓冲区 (V-Sync 节拍器)
    }

    // 7. 退出清理资源
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "[PiHifiPlayer] 系统已优雅安全退出。" << std::endl;
    return 0;
}