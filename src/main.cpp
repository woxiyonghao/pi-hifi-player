#include <iostream>
#include <cmath> // 提供 std::sin, std::abs 用于音频波形模拟计算
#include <SDL2/SDL.h>
#include <vector>

// 平台差异化渲染头文件适配
#if defined(__APPLE__)
#include <OpenGL/gl3.h> // macOS 平台采用系统原生的 OpenGL 3.2 Core 接口
#else
#include <SDL2/SDL_opengles2.h> // 树莓派 5 Linux 平台后续采用嵌入式 OpenGL ES 2.0
#endif

// Dear ImGui 核心与跨平台平台胶水层
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

// 引入我们刚刚自主封装的麦景图动圈双表头渲染引擎
#include "VUMeterRenderer.hpp"
#include "SidebarView.hpp"
#include "MusicModel.hpp"
#include "Font.hpp"

// C++20 [[maybe_unused]] 属性：明确告知编译器形参未直接使用，消除强警告
int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    std::cout << "[PiHifiPlayer] 启动发烧级纯音数播系统 (Modern C++20)..." << std::endl;

    // =========================================================================
    // 1. 初始化 SDL2 底层子系统 (视频渲染、高精度定时器与输入事件总线)
    // =========================================================================
    if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS))
    {
        std::cerr << "SDL_Init 失败: " << SDL_GetError() << std::endl;
        return -1;
    }

    // =========================================================================
    // 2. 配置 OpenGL 上下文属性 (按平台自动选择 Core Profile 或 ES)
    // =========================================================================
#if defined(__APPLE__)
    const char *glsl_version = "#version 150"; // macOS 使用 GLSL 1.50
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    const char *glsl_version = "#version 100"; // 树莓派后续使用 GLSL ES 1.00
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // 开启前后双缓冲 (防止画面撕裂)，配置 24 位深度缓冲
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // =========================================================================
    // 3. 创建 1024x600 纯平窗口 (1:1 像素物理拟合微雪 7 寸 QLED 发烧电容屏)
    // =========================================================================
    SDL_Window *window = SDL_CreateWindow(
        "PiHifiPlayer - McIntosh Dual VU Meter (1024x600)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // 激活 OpenGL 渲染上下文并开启垂直同步 (锁定 60/75 FPS 原生刷新率)
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    // =========================================================================
    // 4. 初始化 Dear ImGui 界面上下文
    // =========================================================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; // 纯音数播嵌入式设备，免写 imgui.ini 磁盘配置
    ImGui::StyleColorsDark();
    // 统一初始化字阶系统 (Small 12px, Regular 15px, Medium 20px, Large 28px)
    Fonts::initialize(io);

    // =========================================================================
    // 5. 绑定 SDL2 窗口系统与 OpenGL3 渲染后端
    // =========================================================================
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 实例化独立麦景图表头渲染器组件
    VUMeterRenderer vu_renderer;
    // 初始化测试歌单实体列表 (Model 数据源)
    std::vector<Playlist> playlists;
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
        playlists.push_back(p1);
        playlists.push_back(p2);
        playlists.push_back(p3);
    }
    // 实例化侧边栏组件并绑定新建歌单事件
    SidebarView sidebar;
    sidebar.setSelectedPlaylistId(101); // 默认高亮首个歌单
    sidebar.setOnCreatePlaylist([&playlists, &sidebar]()
                                {
        uint64_t next_id = playlists.empty() ? 101 : (playlists.back().getId() + 1);
        std::string name = "新发烧歌单 " + std::to_string(next_id - 100);
        playlists.emplace_back(next_id, name);
        sidebar.setSelectedPlaylistId(next_id); });
    float sim_time = 0.0f; // 动圈正弦波模拟激励源的时间步长

    std::cout << "[Ready] 麦景图动圈渲染管线就绪，进入 60fps 主循环。按 ESC 退出。" << std::endl;

    // =========================================================================
    // 6. 主事件与 60fps 动圈渲染心跳循环
    // =========================================================================
    bool running = true;
    while (running) {
        // [6.1] 捕获并分发操作系统输入事件 (键盘、鼠标、窗口关闭)
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (SDL_QUIT == event.type)
                running = false;
            if (SDL_KEYDOWN == event.type && SDLK_ESCAPE == event.key.keysym.sym)
                running = false;
        }

        // [6.2] 模拟左右声道音频能量 (正弦节拍基底 + 随机瞬态爆发，后续接入真实 PCM 解码流)
        sim_time += 0.035f;
        [[maybe_unused]]  float level_l = std::abs(std::sin(sim_time * 1.5f)) * 0.75f + (rand() % 100 / 1000.0f);
        [[maybe_unused]]  float level_r = std::abs(std::sin(sim_time * 1.8f)) * 0.70f + (rand() % 100 / 1000.0f);

        // [6.3] 开启 ImGui 帧缓冲
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // [6.4] 核心渲染调用：满屏绘制经典麦景图湖蓝动圈双表头与物理阻尼指针
        // vu_renderer.render(1024, 600, level_l, level_r);
        sidebar.render(playlists, 230.0f, 600.0f);
         // 右侧主舞台临时展示 (用于验证左侧点击与 Model 数据联动)
        {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            dl->AddRectFilled(ImVec2(230.0f, 0.0f), ImVec2(1024.0f, 600.0f), IM_COL32(10, 14, 20, 255));
            std::string stage_title = "当前视图: ";
            switch (sidebar.getCurrentTab()) {
                case SidebarTab::ScanMusic:      stage_title += "[扫描音乐 - 正在检索 SD卡/SSD 音频文件]"; break;
                case SidebarTab::Equalizer:      stage_title += "[10段专业图形 EQ 均衡器]"; break;
                case SidebarTab::DACSettings:    stage_title += "[ES9038PRO DAC 硬件滤波配置]"; break;
                case SidebarTab::ThemeSettings:  stage_title += "[主题与自选色设置]"; break;
                case SidebarTab::SystemSettings: stage_title += "[系统与硬件配置]"; break;
                case SidebarTab::AllMusic:       stage_title += "[所有音乐曲库]"; break;
                case SidebarTab::CustomPlaylist: {
                    uint64_t pid = sidebar.getSelectedPlaylistId();
                    for (const auto& pl : playlists) {
                        if (pl.getId() == pid) {
                            stage_title += "[播放列表] -> " + pl.getName() + 
                                           " (包含曲目: " + std::to_string(pl.getTrackCount()) + " 首)";
                            
                            // 在右侧列出曲目与格式徽章
                            float y_offset = 90.0f;
                            for (size_t i = 0; i < pl.getTracks().size(); ++i) {
                                const auto& t = pl.getTracks()[i];
                                std::string track_info = std::to_string(i + 1) + ". " + t.title + 
                                                         " - " + t.artist + "  [" + t.getFormatBadge() + "]";
                                dl->AddText(ImVec2(270.0f, y_offset), IM_COL32(170, 185, 205, 220), track_info.c_str());
                                y_offset += 26.0f;
                            }
                            break;
                        }
                    }
                    break;
                }
            }
            dl->AddText(ImVec2(260.0f, 40.0f), IM_COL32(230, 240, 255, 255), stage_title.c_str());
        }

        // [6.5] 提交渲染指令，进行后台缓冲清屏 (经典麦景图夜空深蓝黑底色: #040810)
        ImGui::Render();
        int display_w, display_h;
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.015f, 0.03f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // [6.6] 光栅化绘图数据并交换前后双缓冲区 (由 V-Sync 硬件节拍器驱动换帧)
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // =========================================================================
    // 7. 退出清理资源 (对称反向销毁，绝不泄漏一字节内存与显存句柄)
    // =========================================================================
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "[PiHifiPlayer] 系统已优雅安全退出。" << std::endl;
    return 0;
}