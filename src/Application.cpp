#include "Application.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"
#include "themes/ThemeManager.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <filesystem>

#if defined(__APPLE__)
#include <OpenGL/gl3.h> // macOS 使用原生 OpenGL 3.2 Core
#else
#include <SDL2/SDL_opengles2.h> // 树莓派 5 使用 OpenGL ES 2.0
#endif

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

namespace {

// 计算 UTF-8 字符数 (支持中英文、数字、标点，1 个汉字计数为 1 个字)
inline size_t getUtf8Length(const char* s) {
    if (!s) return 0;
    size_t length = 0;
    while (*s) {
        unsigned char c = static_cast<unsigned char>(*s);
        if (c < 0x80) s += 1;
        else if ((c >> 5) == 0x06) s += 2;
        else if ((c >> 4) == 0x0E) s += 3;
        else if ((c >> 3) == 0x1E) s += 4;
        else s += 1;
        length++;
    }
    return length;
}

// 截断至最多 max_chars 个 UTF-8 字符，防止多字节字符截断撕裂乱码
inline std::string truncateUtf8(const std::string& str, size_t max_chars) {
    size_t length = 0;
    size_t byte_index = 0;
    while (byte_index < str.length() && length < max_chars) {
        unsigned char c = static_cast<unsigned char>(str[byte_index]);
        if (c < 0x80) byte_index += 1;
        else if ((c >> 5) == 0x06) byte_index += 2;
        else if ((c >> 4) == 0x0E) byte_index += 3;
        else if ((c >> 3) == 0x1E) byte_index += 4;
        else byte_index += 1;
        length++;
    }
    return str.substr(0, byte_index);
}

// 跨平台 BMP 贴图加载器 (支持 macOS、Linux 与树莓派 5)
GLuint loadTextureFromBMP(const std::string& filename) {
    std::vector<std::string> search_paths = {
        filename,
        "../" + filename,
        "../../" + filename
    };
    char* base_path = SDL_GetBasePath();
    if (base_path) {
        search_paths.push_back(std::string(base_path) + filename);
        search_paths.push_back(std::string(base_path) + "../" + filename);
        SDL_free(base_path);
    }

    std::string found_path;
    for (const auto& p : search_paths) {
        if (std::filesystem::exists(p)) {
            found_path = p;
            break;
        }
    }

    if (found_path.empty()) {
        std::cerr << "[Texture] 无法定位图片文件: " << filename << std::endl;
        return 0;
    }

    SDL_Surface* surf = SDL_LoadBMP(found_path.c_str());
    if (!surf) {
        std::cerr << "[Texture] SDL_LoadBMP 载入失败: " << SDL_GetError() << std::endl;
        return 0;
    }

    SDL_Surface* formatted = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!formatted) {
        std::cerr << "[Texture] SDL_ConvertSurfaceFormat 转换失败: " << SDL_GetError() << std::endl;
        return 0;
    }

    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, formatted->w, formatted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, formatted->pixels);

    SDL_FreeSurface(formatted);
    std::cout << "[Texture] 成功载入发烧原机贴图: " << found_path << " (ID: " << tex_id << ")" << std::endl;
    return tex_id;
}

} // namespace

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
        "PiHifiPlayer - High Fidelity Music Player (1024x600)",
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
    // 1. 初始化发烧 SQLite 数据库
    MusicDatabase::getInstance().init();

    // 1.5 初始化并恢复视觉主题系统 (全景联动)
    ThemeManager::getInstance().init();

    // 2. 从数据库恢复已扫描的本地发烧曲库
    MusicScanManager::getInstance().loadFromDatabase();

    // 3. 从数据库恢复持久化的播放列表与其中曲目
    playlists_ = MusicDatabase::getInstance().loadPlaylists();

    // 4. 从数据库恢复上次退出时的侧边栏选中项与歌单状态
    std::string saved_tab_str = MusicDatabase::getInstance().getSetting("sidebar_tab", "");
    std::string saved_pl_id_str = MusicDatabase::getInstance().getSetting("sidebar_playlist_id", "0");

    if (!saved_tab_str.empty()) {
        try {
            int tab_val = std::stoi(saved_tab_str);
            SidebarTab tab = static_cast<SidebarTab>(tab_val);
            uint64_t pl_id = std::stoull(saved_pl_id_str);

            if (tab == SidebarTab::CustomPlaylist) {
                bool found = false;
                for (const auto& pl : playlists_) {
                    if (pl.getId() == pl_id) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    sidebar_.setSelectedPlaylistId(pl_id);
                } else if (!playlists_.empty()) {
                    sidebar_.setSelectedPlaylistId(playlists_[0].getId());
                } else {
                    sidebar_.setCurrentTab(SidebarTab::AllMusic);
                }
            } else {
                sidebar_.setCurrentTab(tab);
            }
        } catch (...) {
            sidebar_.setCurrentTab(SidebarTab::AllMusic);
        }
    } else {
        sidebar_.setCurrentTab(SidebarTab::AllMusic);
    }

    last_saved_tab_ = sidebar_.getCurrentTab();
    last_saved_playlist_id_ = sidebar_.getSelectedPlaylistId();

    // 绑定添加播放列表交互：弹出最多 8 字的输入确认模态框
    sidebar_.setOnCreatePlaylist([this]() {
        show_create_playlist_modal_ = true;
        create_playlist_focus_needed_ = true;
        new_playlist_name_buf_[0] = '\0';
    });

    // 绑定主舞台 Tab 快速跳转回调
    main_stage_.setOnNavigateTab([this](SidebarTab tab) {
        sidebar_.setCurrentTab(tab);
        MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(tab)));
        MusicDatabase::getInstance().setSetting("sidebar_playlist_id", "0");
    });

    // 绑定主舞台歌单选择/删除后的路由回调
    main_stage_.setOnSelectPlaylist([this](SidebarTab tab, uint64_t pl_id) {
        if (tab == SidebarTab::CustomPlaylist) {
            sidebar_.setSelectedPlaylistId(pl_id);
        } else {
            sidebar_.setCurrentTab(tab);
        }
        MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(sidebar_.getCurrentTab())));
        MusicDatabase::getInstance().setSetting("sidebar_playlist_id", std::to_string(sidebar_.getSelectedPlaylistId()));
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

    auto bg_mode = ThemeManager::getInstance().getBackgroundVisualMode();
    if (bg_mode == BackgroundVisualMode::PureBlack) {
        return;
    }

    auto& player = PlayerAdmin::getInstance();

    if (bg_mode == BackgroundVisualMode::Accuphase) {
        float l = 0.0f;
        float r = 0.0f;
        if (player.isPlaying()) {
            float levels12[12] = {0.0f};
            player.getSpectrumLevels(levels12, 12);
            l = std::clamp((levels12[0] + levels12[1] + levels12[2] + levels12[3] + levels12[4]) * 0.28f, 0.0f, 1.0f);
            r = std::clamp((levels12[2] + levels12[3] + levels12[4] + levels12[5] + levels12[6]) * 0.28f, 0.0f, 1.0f);
        }
        renderAccuphaseBackground(screen_w, screen_h, l, r);
        return;
    }

    if (bg_mode == BackgroundVisualMode::VUMeter) {
        float l = 0.0f;
        float r = 0.0f;
        if (player.isPlaying()) {
            float levels12[12] = {0.0f};
            player.getSpectrumLevels(levels12, 12);
            // 低频/中高频能量估算左右声道电平
            l = std::clamp((levels12[0] + levels12[1] + levels12[2] + levels12[3] + levels12[4]) * 0.28f, 0.0f, 1.0f);
            r = std::clamp((levels12[2] + levels12[3] + levels12[4] + levels12[5] + levels12[6]) * 0.28f, 0.0f, 1.0f);
        }
        vu_renderer_.setTheme(ThemeManager::getInstance().getMeterTheme());
        vu_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        vu_renderer_.render(screen_w, screen_h, l, r);
        return;
    }

    // 2. 如果未播放且为 LED 频谱模式，保持现在的颜色 (0 个方块，0 动效，完全纯净)
    if (!player.isPlaying()) {
        return;
    }

    // 3. 如果播放且为 LED 频谱模式，在整个 App 的底，渲染音乐动效 (48列 LED 矩阵频谱律动)
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

    // 常规点亮方块色与顶峰指示色 (完全随主题联动)
    const ImU32 lit_color = ThemeManager::getInstance().getSpectrumLitColor();
    const ImU32 peak_color = ThemeManager::getInstance().getSpectrumPeakColor();
    const ImU32 unlit_color = ThemeManager::getInstance().getSpectrumUnlitColor();

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

    // [顶层模态弹窗] 新建播放列表对话框
    if (show_create_playlist_modal_) {
        renderCreatePlaylistModal(screen_w, screen_h);
    }

    // 侧边栏选中项发生变动时立即持久化
    SidebarTab cur_tab = sidebar_.getCurrentTab();
    uint64_t cur_pl_id = sidebar_.getSelectedPlaylistId();
    if (cur_tab != last_saved_tab_ || cur_pl_id != last_saved_playlist_id_) {
        last_saved_tab_ = cur_tab;
        last_saved_playlist_id_ = cur_pl_id;
        MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(cur_tab)));
        MusicDatabase::getInstance().setSetting("sidebar_playlist_id", std::to_string(cur_pl_id));
    }

    // 3. 提交绘图并进行 OpenGL 光栅化清屏
    ImGui::Render();
    glViewport(0, 0, display_w, display_h);
    ImVec4 clear_col = ThemeManager::getInstance().getClearColor();
    glClearColor(clear_col.x, clear_col.y, clear_col.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);
}

void Application::renderCreatePlaylistModal(float screen_w, float screen_h) {
    ImGuiIO& io = ImGui::GetIO();

    // 1. 全透明交互遮罩 (拦截底层鼠标点击，绝不添加发黑灰蒙层，底层视觉 100% 通透保留)
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags backdrop_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##CreatePlaylistModalBackdrop", nullptr, backdrop_flags)) {
        ImGui::InvisibleButton("##CreatePlaylistBackdropClickBlocker", io.DisplaySize);
        if (ImGui::IsItemClicked()) {
            show_create_playlist_modal_ = false;
            new_playlist_name_buf_[0] = '\0';
        }
    }
    ImGui::End();

    // 2. 居中模态卡片尺寸与排版
    const float modal_w = 380.0f;
    const float modal_h = 200.0f;
    const float modal_x = (screen_w - modal_w) * 0.5f;
    const float modal_y = (screen_h - modal_h) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(modal_x, modal_y));
    ImGui::SetNextWindowSize(ImVec2(modal_w, modal_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));

    if (ImGui::Begin("##CreatePlaylistModalDialog", nullptr, flags)) {
        // 绘制发烧级纯正毛玻璃卡片底板与柔和漫射阴影
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetWindowPos();
        ImVec2 p_max(p_min.x + modal_w, p_min.y + modal_h);

        dl->AddRectFilled(ImVec2(p_min.x - 2.0f, p_min.y + 4.0f),
                          ImVec2(p_max.x + 2.0f, p_max.y + 14.0f),
                          IM_COL32(0, 0, 0, 110), 18.0f);
        GlassCardRenderer::drawFrosted(dl, p_min, p_max, 16.0f);

        // 标题
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "新建播放列表");
        if (Fonts::Medium) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // 预判字符数限制并截断至最多 8 个字
        size_t utf8_len = getUtf8Length(new_playlist_name_buf_);
        if (utf8_len > 8) {
            std::string truncated = truncateUtf8(new_playlist_name_buf_, 8);
            std::snprintf(new_playlist_name_buf_, sizeof(new_playlist_name_buf_), "%s", truncated.c_str());
            utf8_len = 8;
        }

        // 输入框样式
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(32, 38, 52, 220));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 48, 65, 230));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(45, 54, 75, 240));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, UIConfig::Color::GlassBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));

        if (create_playlist_focus_needed_) {
            ImGui::SetKeyboardFocusHere();
            create_playlist_focus_needed_ = false;
        }

        ImGui::SetNextItemWidth(modal_w - 48.0f);
        bool enter_pressed = ImGui::InputTextWithHint("##playlist_name_input", "输入播放列表名称 (最多8个字)",
                                                     new_playlist_name_buf_, sizeof(new_playlist_name_buf_),
                                                     ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        // 重新获取输入后字数并执行硬限制截断
        utf8_len = getUtf8Length(new_playlist_name_buf_);
        if (utf8_len > 8) {
            std::string truncated = truncateUtf8(new_playlist_name_buf_, 8);
            std::snprintf(new_playlist_name_buf_, sizeof(new_playlist_name_buf_), "%s", truncated.c_str());
            utf8_len = 8;
        }

        // 字数提示
        std::string count_str = std::to_string(utf8_len) + " / 8 字";
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        float count_w = ImGui::CalcTextSize(count_str.c_str()).x;
        ImGui::SetCursorPosX(modal_w - 24.0f - count_w);
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.8f), "%s", count_str.c_str());
        if (Fonts::Small) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        // 底部按钮栏：取消 / 确认
        const float btn_w = 96.0f;
        const float btn_h = 34.0f;
        ImGui::SetCursorPosX(modal_w - 24.0f - btn_w * 2.0f - 12.0f);

        // 1. 取消按钮
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 46, 60, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 60, 78, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 75, 96, 250));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        if (ImGui::Button("取消", ImVec2(btn_w, btn_h)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_create_playlist_modal_ = false;
            new_playlist_name_buf_[0] = '\0';
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 12.0f);

        // 2. 确认按钮
        std::string trimmed_name(new_playlist_name_buf_);
        while (!trimmed_name.empty() && (trimmed_name.front() == ' ' || trimmed_name.front() == '\t')) trimmed_name.erase(trimmed_name.begin());
        while (!trimmed_name.empty() && (trimmed_name.back() == ' ' || trimmed_name.back() == '\t')) trimmed_name.pop_back();
        bool can_confirm = !trimmed_name.empty() && (getUtf8Length(trimmed_name.c_str()) <= 8);

        if (!can_confirm) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, UIConfig::Color::Accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 65, 95, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(230, 30, 60, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button("确认", ImVec2(btn_w, btn_h)) || (can_confirm && enter_pressed)) {
            uint64_t next_id = playlists_.empty() ? 101 : (playlists_.back().getId() + 1);
            playlists_.emplace_back(next_id, trimmed_name);
            sidebar_.setSelectedPlaylistId(next_id);

            // 立即持久化至 SQLite 数据库
            MusicDatabase::getInstance().savePlaylists(playlists_);

            show_create_playlist_modal_ = false;
            new_playlist_name_buf_[0] = '\0';
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (!can_confirm) {
            ImGui::EndDisabled();
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void Application::renderAccuphaseBackground(float screen_w, float screen_h, float raw_level_l, float raw_level_r) {
    if (accuphase_tex_id_ == 0) {
        accuphase_tex_id_ = loadTextureFromBMP("assets/themes/accuphase_bg.bmp");
    }

    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();

    // 1. 绘制金嗓子 E-260 1024x600 原机全景贴图
    if (accuphase_tex_id_ != 0) {
        bg_dl->AddImage(
            static_cast<ImTextureID>(accuphase_tex_id_),
            ImVec2(0.0f, 0.0f),
            ImVec2(screen_w, screen_h)
        );
    } else {
        bg_dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), IM_COL32(14, 16, 20, 255));
    }

    // 2. 金嗓子原机左右双表头动圈物理模拟 (Attack ~12ms, Decay ~280ms)
    // 刻度弧度范围: -40dB 对应约 -125度 (-2.18 rad), +3dB 对应约 -55度 (-0.96 rad), 摆幅 1.22 rad
    constexpr float kAccMinAngle = -2.18f;
    constexpr float kAccSweep = 1.22f;

    float norm_l = std::pow(std::clamp(raw_level_l, 0.0f, 1.0f), 0.85f);
    float norm_r = std::pow(std::clamp(raw_level_r, 0.0f, 1.0f), 0.85f);

    float target_l = kAccMinAngle + norm_l * kAccSweep;
    float target_r = kAccMinAngle + norm_r * kAccSweep;

    if (target_l > acc_needle_l_) {
        acc_needle_l_ += (target_l - acc_needle_l_) * 0.35f;
    } else {
        acc_needle_l_ += (target_l - acc_needle_l_) * 0.07f;
    }

    if (target_r > acc_needle_r_) {
        acc_needle_r_ += (target_r - acc_needle_r_) * 0.35f;
    } else {
        acc_needle_r_ += (target_r - acc_needle_r_) * 0.07f;
    }

    // 3. 动态表头背光微光晕与跳动金属细针
    const float needle_len = 90.0f;
    ImVec2 pivot_l(196.5f, 345.5f);
    ImVec2 pivot_r(753.5f, 345.5f);

    // 左声道表盘
    bg_dl->PushClipRect(ImVec2(84.0f, 230.0f), ImVec2(365.0f, 360.0f), true);
    if (raw_level_l > 0.02f) {
        int glow_a = static_cast<int>(40.0f * (0.6f + 0.4f * norm_l));
        bg_dl->AddCircleFilled(ImVec2(196.5f, 295.0f), 55.0f, IM_COL32(255, 230, 160, glow_a));
    }
    ImVec2 tip_l(pivot_l.x + std::cos(acc_needle_l_) * needle_len, pivot_l.y + std::sin(acc_needle_l_) * needle_len);
    bg_dl->AddLine(pivot_l, tip_l, IM_COL32(255, 240, 200, 40), 3.5f);
    bg_dl->AddLine(pivot_l, tip_l, IM_COL32(20, 18, 16, 245), 2.0f);
    bg_dl->AddCircleFilled(pivot_l, 5.0f, IM_COL32(28, 25, 22, 255));
    bg_dl->PopClipRect();

    // 右声道表盘
    bg_dl->PushClipRect(ImVec2(645.0f, 230.0f), ImVec2(926.0f, 360.0f), true);
    if (raw_level_r > 0.02f) {
        int glow_a = static_cast<int>(40.0f * (0.6f + 0.4f * norm_r));
        bg_dl->AddCircleFilled(ImVec2(753.5f, 295.0f), 55.0f, IM_COL32(255, 230, 160, glow_a));
    }
    ImVec2 tip_r(pivot_r.x + std::cos(acc_needle_r_) * needle_len, pivot_r.y + std::sin(acc_needle_r_) * needle_len);
    bg_dl->AddLine(pivot_r, tip_r, IM_COL32(255, 240, 200, 40), 3.5f);
    bg_dl->AddLine(pivot_r, tip_r, IM_COL32(20, 18, 16, 245), 2.0f);
    bg_dl->AddCircleFilled(pivot_r, 5.0f, IM_COL32(28, 25, 22, 255));
    bg_dl->PopClipRect();

    // 4. 标志性翡翠绿 Accuphase 徽标呼吸微光 (机皇灵魂)
    float logo_pulse = (std::sin(static_cast<float>(ImGui::GetTime()) * 1.5f) + 1.0f) * 0.5f;
    int logo_a = static_cast<int>(14.0f + 20.0f * logo_pulse);
    bg_dl->AddRectFilled(ImVec2(440.0f, 245.0f), ImVec2(584.0f, 292.0f), IM_COL32(0, 235, 150, logo_a), 10.0f);

    // 5. 经典红光 7 段数码管音量读数 (-24 dB / -- dB)
    auto& player = PlayerAdmin::getInstance();
    float vol = player.getVolume();
    int db_atten = (vol > 0.01f) ? static_cast<int>(std::round((1.0f - vol) * -50.0f)) : -99;
    char db_buf[16];
    if (db_atten <= -99) {
        std::snprintf(db_buf, sizeof(db_buf), " -- ");
    } else {
        std::snprintf(db_buf, sizeof(db_buf), "-%02d", std::abs(db_atten));
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    bg_dl->AddText(ImVec2(495.0f, 340.0f), IM_COL32(255, 45, 45, 240), db_buf);
    if (Fonts::Small) ImGui::PopFont();
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

    // 退出前确保持久化最后的侧边栏选中项
    MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(sidebar_.getCurrentTab())));
    MusicDatabase::getInstance().setSetting("sidebar_playlist_id", std::to_string(sidebar_.getSelectedPlaylistId()));

    return 0;
}

void Application::shutdown() {
    if (accuphase_tex_id_ != 0) {
        glDeleteTextures(1, &accuphase_tex_id_);
        accuphase_tex_id_ = 0;
    }

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
