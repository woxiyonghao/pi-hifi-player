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
    platform_ = Platform::current();
    std::cout << "[Application] 运行平台: " << Platform::displayName()
              << " (" << Platform::name() << ")，目标刷新率: "
              << Platform::getTargetFps() << " FPS" << std::endl;
    return true;
}

bool Application::initSDL() {
    // 禁用 SDL2 默认单指触摸模拟鼠标事件，由原生多指触控引擎统一全权调度 (杜绝双指坐标交叉震荡)
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

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
    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    const char* title = "PiHifiPlayer - High Fidelity Music Player";
    if (Platform::isRaspberryPi()) {
        // 树莓派微雪 7 寸屏锁定纯平专机标题
        title = "PiHifiPlayer - High Fidelity Music Player (1024x600)";
    } else {
        // Mac / Windows / 桌面环境支持窗口自由调节与拖拽拉伸
        window_flags |= SDL_WINDOW_RESIZABLE;
    }

    // 默认创建发烧标准比例窗口 (1024×600)
    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 600,
        window_flags
    );

    if (!window_) {
        std::cerr << "[Window] 窗口创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!Platform::isRaspberryPi()) {
        // 设置桌面环境下的安全最小窗口尺寸，保证发烧控件与歌单在紧凑视口下完整可用
        SDL_SetWindowMinimumSize(window_, 960, 540);
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "[GL] 上下文创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    if (Platform::isRaspberryPi()) {
        // 树莓派触控屏环境隐藏系统硬件鼠标光标
        SDL_ShowCursor(SDL_DISABLE);
        // 树莓派 KMSDRM 下解除硬件垂直同步锁等待，配合上层精确 30 FPS 限制器休眠让出 CPU 算力
        SDL_GL_SetSwapInterval(0);
    } else {
        SDL_GL_SetSwapInterval(1); // Mac / iOS 等锁定 60fps 原生垂直同步
    }
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

    // 绑定空余时间全屏设置变动回调
    main_stage_.setOnIdleFullscreenChanged([this](float /*secs*/) {
        resetIdle();
    });

    // 绑定底部 DAC 卡片交互：点击直达 DAC 设置视图
    sidebar_.setOnDacClick([this]() {
        sidebar_.setCurrentTab(SidebarTab::DACSettings);
        MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(SidebarTab::DACSettings)));
        MusicDatabase::getInstance().setSetting("sidebar_playlist_id", "0");
    });

    // 初始化同步 DAC 硬件连接状态与当前芯片名
    sidebar_.setDacConnected(main_stage_.getDacView().isDacConnected(), main_stage_.getDacView().getCurrentChipName());
}

void Application::resetIdle() {
    idle_timer_ = 0.0f;
    is_fullscreen_idle_ = false;
}

void Application::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        // 原生多点触摸 (TouchScreen) 转 ImGui 交互与平滑手势拖拽滚动 (自适应窗口调节与微雪触控屏)
        ImGuiIO& io = ImGui::GetIO();
        int cur_w = 1024, cur_h = 600;
        if (window_) {
            SDL_GetWindowSize(window_, &cur_w, &cur_h);
        }
        float touch_w = (cur_w > 0) ? static_cast<float>(cur_w) : 1024.0f;
        float touch_h = (cur_h > 0) ? static_cast<float>(cur_h) : 600.0f;

        if (event.type == SDL_FINGERDOWN) {
            float x = event.tfinger.x * touch_w;
            float y = event.tfinger.y * touch_h;
            SDL_FingerID fid = event.tfinger.fingerId;

            // 维护当前活跃触控点
            auto it = std::find_if(active_fingers_.begin(), active_fingers_.end(),
                                   [fid](const TouchFinger& f) { return f.id == fid; });
            if (it == active_fingers_.end()) {
                active_fingers_.push_back({fid, x, y, x, y});
            } else {
                it->x = x; it->y = y; it->last_x = x; it->last_y = y;
            }

            resetIdle();

            if (active_fingers_.size() == 1) {
                // 单指初次触屏：准备常规轻触点击或单指拖拽
                touch_accum_dy_ = 0.0f;
                is_touch_scrolling_ = false;
                touch_scroll_velocity_ = 0.0f;

                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, true);
            } else {
                // 多指触屏 (如双指滚动或多指手势)：
                // 1. 立即释放鼠标按下状态，防止意外触发任何按键或曲目误点击
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                io.AddMouseButtonEvent(0, false);
                // 2. 进入多指平滑滚动准备态
                is_touch_scrolling_ = true;
                touch_scroll_velocity_ = 0.0f;
                // 重置所有活跃手指的 last 坐标，彻底阻断跨手指差值震荡
                for (auto& f : active_fingers_) {
                    f.last_x = f.x;
                    f.last_y = f.y;
                }
            }
        } else if (event.type == SDL_FINGERUP) {
            float x = event.tfinger.x * touch_w;
            float y = event.tfinger.y * touch_h;
            SDL_FingerID fid = event.tfinger.fingerId;

            auto it = std::find_if(active_fingers_.begin(), active_fingers_.end(),
                                   [fid](const TouchFinger& f) { return f.id == fid; });
            if (it != active_fingers_.end()) {
                active_fingers_.erase(it);
            }

            resetIdle();

            if (active_fingers_.empty()) {
                // 所有手指均已离开屏幕
                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, false);
                is_touch_scrolling_ = false;
            } else {
                // 仍有其它手指留在屏幕上 (例如两指抬起了一指)
                // 重置剩余手指的 last 坐标，保证抬指瞬间无任何跳变 delta
                for (auto& f : active_fingers_) {
                    f.last_x = f.x;
                    f.last_y = f.y;
                }
            }
        } else if (event.type == SDL_FINGERMOTION) {
            float x = event.tfinger.x * touch_w;
            float y = event.tfinger.y * touch_h;
            SDL_FingerID fid = event.tfinger.fingerId;

            auto it = std::find_if(active_fingers_.begin(), active_fingers_.end(),
                                   [fid](const TouchFinger& f) { return f.id == fid; });
            if (it == active_fingers_.end()) {
                active_fingers_.push_back({fid, x, y, x, y});
                it = active_fingers_.end() - 1;
            }

            // 核心关键：仅与本手指自身历史坐标计算差值，绝对不与其它手指发生交叉差值计算！
            float finger_dy = y - it->last_y;
            it->x = x;
            it->y = y;
            it->last_x = x;
            it->last_y = y;

            // 单帧防突变安全钳位
            finger_dy = std::clamp(finger_dy, -50.0f, 50.0f);

            resetIdle();

            if (active_fingers_.size() == 1) {
                // 单指模式：常规追踪
                touch_accum_dy_ += std::abs(finger_dy);

                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                io.AddMousePosEvent(x, y);

                // 单指位移超过 8px 时识别为滚动，取消点击高亮
                if (!is_touch_scrolling_ && touch_accum_dy_ > 8.0f) {
                    is_touch_scrolling_ = true;
                    io.AddMouseButtonEvent(0, false);
                }

                if (is_touch_scrolling_) {
                    float wheel_delta = finger_dy / 24.0f;
                    io.AddMouseWheelEvent(0.0f, wheel_delta);
                    touch_scroll_velocity_ = finger_dy * 0.45f;
                }
            } else {
                // 多指模式 (两指及以上双指平滑滚动)：
                // 1. 确保鼠标保持释放状态
                io.AddMouseButtonEvent(0, false);
                is_touch_scrolling_ = true;

                // 2. 双指/多指位移平滑融合：均分各手指贡献，手感与单指一样轻柔稳定
                float weight = 1.0f / static_cast<float>(active_fingers_.size());
                float smooth_dy = finger_dy * weight;
                float wheel_delta = smooth_dy / 24.0f;
                io.AddMouseWheelEvent(0.0f, wheel_delta);
                touch_scroll_velocity_ = smooth_dy * 0.45f;

                // 3. 虚拟指针平滑定位于所有活跃手指的几何重心
                float avg_x = 0.0f, avg_y = 0.0f;
                for (const auto& f : active_fingers_) {
                    avg_x += f.x;
                    avg_y += f.y;
                }
                avg_x /= active_fingers_.size();
                avg_y /= active_fingers_.size();

                io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                io.AddMousePosEvent(avg_x, avg_y);
            }
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_LEAVE ||
                event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                active_fingers_.clear();
                is_touch_scrolling_ = false;
            }
            resetIdle();
        } else if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
                   event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL) {
            resetIdle();
        } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            resetIdle();
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
    // 触控手势离屏动量平滑惯性滚动 (基于真实 dt 进行阻尼衰减，30fps 与 60fps 下手感完全一致)
    if (!is_touch_scrolling_ && std::abs(touch_scroll_velocity_) > 0.4f) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent(0.0f, touch_scroll_velocity_ / 24.0f);
        float damping = std::pow(0.88f, dt * 60.0f);
        touch_scroll_velocity_ *= damping;
        if (std::abs(touch_scroll_velocity_) < 0.4f) {
            touch_scroll_velocity_ = 0.0f;
        }
    }

    // 推进播放器时间轴心跳
    PlayerAdmin::getInstance().update(dt);

    // 动圈表头节拍激励步进 (真实时间驱动，30fps/60fps 摆动频率恒定)
    sim_time_ += dt * 2.1f;

    // 空余时间全屏检测与四角动画插值
    float idle_timeout = main_stage_.getIdleFullscreenSeconds();
    if (idle_timeout > 0.0f) {
        // 如果有模态弹窗正在显示，不触发屏保
        if (!show_create_playlist_modal_) {
            idle_timer_ += dt;
            if (idle_timer_ >= idle_timeout) {
                is_fullscreen_idle_ = true;
            }
        } else {
            idle_timer_ = 0.0f;
        }
    } else {
        is_fullscreen_idle_ = false;
        idle_timer_ = 0.0f;
    }

    // 平滑插值动画 anim_progress_ (0.0f 正常展开 <-> 1.0f 四角移出全屏沉浸)
    float target_progress = is_fullscreen_idle_ ? 1.0f : 0.0f;
    float anim_speed = 2.5f; // 过渡耗时约 0.4s
    if (anim_progress_ < target_progress) {
        anim_progress_ = std::min(anim_progress_ + dt * anim_speed, 1.0f);
    } else if (anim_progress_ > target_progress) {
        anim_progress_ = std::max(anim_progress_ - dt * anim_speed, 0.0f);
    }
}

void Application::renderBackground(float screen_w, float screen_h) {
    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();

    // 1. 铺设整个 App 的基准发烧底色 (完全对齐原设计的区域与色值规范)
    if (anim_progress_ >= 0.999f) {
        // 完全全屏屏保状态：底板 100% 满屏无缝铺满，杜绝任何分界线与色块
        bg_dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);
    } else {
        float ease_t = anim_progress_ < 0.5f ? 4.0f * anim_progress_ * anim_progress_ * anim_progress_
                                             : 1.0f - std::pow(-2.0f * anim_progress_ + 2.0f, 3.0f) * 0.5f;
        float sidebar_bg_x = UIConfig::Layout::SidebarWidth * (1.0f - ease_t);

        if (sidebar_bg_x > 0.5f) {
            // 左侧侧边栏暗色基底 (0 ~ sidebar_bg_x)
            bg_dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(sidebar_bg_x, screen_h), UIConfig::Color::WindowBg);
        }
        // 右侧主舞台深空基底 (sidebar_bg_x ~ screen_w)
        bg_dl->AddRectFilled(ImVec2(sidebar_bg_x, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);
    }

    auto bg_mode = ThemeManager::getInstance().getBackgroundVisualMode();
    if (bg_mode == BackgroundVisualMode::PureBlack) {
        return;
    }

    auto& player = PlayerAdmin::getInstance();
    bool is_playing = player.isPlaying();
    float levels12[12] = {0.0f};
    float l = 0.0f;
    float r = 0.0f;
    if (is_playing) {
        player.getSpectrumLevels(levels12, 12);
        l = std::clamp((levels12[0] + levels12[1] + levels12[2] + levels12[3] + levels12[4]) * 0.28f, 0.0f, 1.0f);
        r = std::clamp((levels12[2] + levels12[3] + levels12[4] + levels12[5] + levels12[6]) * 0.28f, 0.0f, 1.0f);
    }

    if (bg_mode == BackgroundVisualMode::Accuphase) {
        accuphase_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        accuphase_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        accuphase_renderer_.render(screen_w, screen_h, l, r);
        return;
    }

    if (bg_mode == BackgroundVisualMode::VUMeter) {
        vu_renderer_.setTheme(ThemeManager::getInstance().getMeterTheme());
        vu_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        vu_renderer_.render(screen_w, screen_h, l, r);
        return;
    }

    if (bg_mode == BackgroundVisualMode::TapeReel) {
        tape_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        tape_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        tape_renderer_.render(screen_w, screen_h, is_playing, player.getProgress());
        return;
    }

    if (bg_mode == BackgroundVisualMode::SiriWaveform) {
        siri_wave_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        siri_wave_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        siri_wave_renderer_.render(screen_w, screen_h, is_playing, l, r);
        return;
    }

    if (bg_mode == BackgroundVisualMode::SiriOrb) {
        siri_orb_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        siri_orb_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        siri_orb_renderer_.render(screen_w, screen_h, is_playing, l, r);
        return;
    }

    if (bg_mode == BackgroundVisualMode::FloatingBubbles) {
        bubbles_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        bubbles_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        bubbles_renderer_.render(screen_w, screen_h, is_playing, levels12);
        return;
    }

    if (bg_mode == BackgroundVisualMode::NeonWaveform) {
        neon_wave_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        neon_wave_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        neon_wave_renderer_.render(screen_w, screen_h, is_playing, levels12, 12);
        return;
    }

    if (bg_mode == BackgroundVisualMode::CyberGrid) {
        cyber_grid_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        cyber_grid_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        cyber_grid_renderer_.render(screen_w, screen_h, is_playing, levels12, 12);
        return;
    }

    if (bg_mode == BackgroundVisualMode::GlassClock) {
        glass_clock_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        glass_clock_renderer_.setCustomColor(ThemeManager::getInstance().getCustomColor());
        glass_clock_renderer_.render(screen_w, screen_h, is_playing, l, r);
        return;
    }

    // 几何排版参数：左右对齐发烧容器外边距 (16px ~ 1008px)
    const float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    const float total_w = screen_w - margin_x * 2.0f;          // 992.0f
    const int num_cols = 48;                                   // 48 列超宽音轨点阵
    const float gap_x = 4.0f;                                  // 列间距
    const float col_w = (total_w - (num_cols - 1) * gap_x) / num_cols; // ~16.75px
    
    const float seg_h = 6.0f;                                  // 每个方块高度
    const float gap_y = 2.5f;                                  // 方块纵向间距
    const int num_rows = std::clamp(static_cast<int>((screen_h - 16.0f) / (seg_h + gap_y)), 20, 200); // 随窗口高度自适应满屏行数
    const float seg_round = 0.0f;                              // 0.0f 纯净直角点阵 (大幅降低 GLES 顶点负载，呈现硬朗经典机皇发烧质感)
    const float bot_y = screen_h - 8.0f;                       // 距底部屏幕边缘 8px 起振 (最高行达 y = 8px)

    const ImU32 unlit_color = ThemeManager::getInstance().getSpectrumUnlitColor();

    // 2. 如果未播放且为 LED 频谱模式，保持通透深空暗黑基底，避免在屏幕上生成静态点阵方块伪影
    if (!is_playing) {
        return;
    }

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 cr = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 cg = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 cb = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 常规点亮方块色与顶峰指示色 (完全随主题联动)
    const ImU32 lit_color = ThemeManager::getInstance().getSpectrumLitColor();
    const ImU32 peak_color = ThemeManager::getInstance().getSpectrumPeakColor();

    // 全屏全景氛围微辉光 (随着整体低频能量呼吸涌动)
    float bass_energy = (levels12[0] + levels12[1] + levels12[2]) / 3.0f;
    int glow_alpha = static_cast<int>(bass_energy * 32.0f);
    if (glow_alpha > 0) {
        bg_dl->AddRectFilledMultiColor(
            ImVec2(margin_x, 0.0f),
            ImVec2(screen_w - margin_x, screen_h),
            IM_COL32(cr, cg, cb, 0),
            IM_COL32(cr, cg, cb, 0),
            IM_COL32(cr, cg, cb, glow_alpha),
            IM_COL32(cr, cg, cb, glow_alpha)
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

        // 真实声学动态曲线：去除过载过冲，保留真实的高低频落差与音乐跳动层次
        float dynamic_level = std::clamp(std::pow(level, 0.85f) * 0.90f, 0.0f, 1.0f);

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
    ImGuiIO& io = ImGui::GetIO();
    // 使用 ImGui 逻辑尺寸作为全屏排版依据 (适配 Retina 高分屏与窗口自由缩放)
    const float screen_w = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : ((display_w > 0) ? static_cast<float>(display_w) : 1024.0f);
    const float screen_h = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : ((display_h > 0) ? static_cast<float>(display_h) : 600.0f);

    // [全景底层背景与音乐律动动效] (位于所有窗口最底层)
    renderBackground(screen_w, screen_h);

    // 计算四角屏保动效缓动因子 (0.0f 正常显现 ~ 1.0f 移出至四角)
    float ease_t = anim_progress_ < 0.5f ? 4.0f * anim_progress_ * anim_progress_ * anim_progress_
                                         : 1.0f - std::pow(-2.0f * anim_progress_ + 2.0f, 3.0f) * 0.5f;

    // 只有在未完全移出屏幕时才渲染 4 大板块
    if (anim_progress_ < 0.999f) {
        // 四角移出偏移计算：完全移出大屏视口，确保无边缘色块残留
        float top_nav_dx = -(230.0f + 60.0f) * ease_t;
        float top_nav_dy = -(screen_h * 0.5f + 60.0f) * ease_t;

        float dac_dx = -(230.0f + 60.0f) * ease_t;
        float dac_dy = (screen_h * 0.5f + 60.0f) * ease_t;

        float main_dx = (screen_w + 50.0f) * ease_t;
        float main_dy = -(screen_h * 0.5f + 50.0f) * ease_t;

        float bottom_dx = (screen_w + 50.0f) * ease_t;
        float bottom_dy = (screen_h * 0.35f + 80.0f) * ease_t;

        // 2. 调度发烧 UI 三驾马车布局渲染
        // 动态同步当前选中的 DAC 芯片状态与显示名称
        sidebar_.setDacConnected(main_stage_.getDacView().isDacConnected(), main_stage_.getDacView().getCurrentChipName());

        // [左侧] 导航与歌单侧边栏 (向左上方移出) & DAC 卡片 (向左下方移出)
        sidebar_.render(playlists_, 230.0f, screen_h, top_nav_dx, top_nav_dy, dac_dx, dac_dy);

        // [右上方] 中央主舞台区域 (向右上方移出)
        main_stage_.render(sidebar_.getCurrentTab(), sidebar_.getSelectedPlaylistId(), playlists_, 
                           230.0f + main_dx, 0.0f + main_dy, screen_w - 230.0f, screen_h);

        // [底部] 播放控制胶囊栏 (向右下方移出)
        bottom_bar_.render(screen_w, screen_h, bottom_dx, bottom_dy);
    }

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


int Application::run() {
    Uint64 last_time = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    // 核心帧率策略：除树莓派锁定 30fps 控温降载外，其他平台 (Mac/iOS/Android/Windows) 统一 60fps
    const double target_frame_time = Platform::getTargetFrameTime();

    while (running_) {
        Uint64 frame_start = SDL_GetPerformanceCounter();
        float dt = static_cast<float>((frame_start - last_time) / freq);
        if (dt > 0.1f) dt = 0.1f; // 限制 dt 最大步长，防止长时间系统卡顿导致的物理插值突变
        last_time = frame_start;

        pollEvents();
        update(dt);
        render();

        // 精确帧率限制器：休眠多余时间，彻底解放 CPU 占用与降低发热
        Uint64 frame_end = SDL_GetPerformanceCounter();
        double elapsed_sec = static_cast<double>(frame_end - frame_start) / freq;
        if (elapsed_sec < target_frame_time) {
            double sleep_ms = (target_frame_time - elapsed_sec) * 1000.0;
            if (sleep_ms >= 1.0) {
                SDL_Delay(static_cast<Uint32>(sleep_ms));
            }
        }
    }

    // 退出前确保持久化最后的侧边栏选中项
    MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(sidebar_.getCurrentTab())));
    MusicDatabase::getInstance().setSetting("sidebar_playlist_id", std::to_string(sidebar_.getSelectedPlaylistId()));

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
