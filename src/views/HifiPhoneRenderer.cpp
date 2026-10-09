#include "views/HifiPhoneRenderer.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include <algorithm>
#include <cmath>

HifiPhoneRenderer::HifiPhoneRenderer() {}

void HifiPhoneRenderer::init() {
    cached_tracks_ = MusicDatabase::getInstance().loadScannedTracks();
    cached_playlists_ = MusicDatabase::getInstance().loadPlaylists();
    data_loaded_ = true;
}

void HifiPhoneRenderer::render(float screen_w, float screen_h) {
    if (!data_loaded_) {
        init();
    }

    // 全屏底色：深邃暗黑
    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();
    if (bg_dl) {
        bg_dl->AddRectFilled(ImVec2(0, 0), ImVec2(screen_w, screen_h), IM_COL32(8, 10, 14, 255));
    }

    // 全屏 Now Playing 沉浸大页优先渲染
    if (show_now_playing_) {
        renderNowPlayingOverlay(screen_w, screen_h);
        return;
    }

    // 适配 iPhone 常见安全区域 (顶部刘海/灵动岛约 44pt，底部 Home Bar 约 34pt)
    float top_inset = (screen_h > 700.0f) ? 44.0f : 20.0f;
    float bottom_inset = (screen_h > 700.0f) ? 34.0f : 10.0f;

    // 布局垂直切分：
    // 1. Header (44pt)
    float header_h = 44.0f;
    renderHeader(screen_w, top_inset);

    // 2. 底部 TabBar (50pt)
    float tabbar_h = 50.0f;
    float tabbar_y = screen_h - tabbar_h - bottom_inset;

    // 3. 常驻 Mini Player (54pt，紧贴在 TabBar 上方)
    float mini_h = 54.0f;
    float mini_y = tabbar_y - mini_h - 8.0f;
    renderMiniPlayer(screen_w, mini_y);
    renderBottomTabBar(screen_w, tabbar_y, tabbar_h);

    // 4. 中间主内容区 (从 Header 下方到 Mini Player 上方)
    float content_y = top_inset + header_h + 6.0f;
    float content_h = mini_y - content_y - 6.0f;
    if (content_h > 100.0f) {
        renderMainContent(screen_w, content_y, content_h);
    }
}

void HifiPhoneRenderer::renderHeader(float screen_w, float top_inset) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float y = top_inset + 6.0f;
    const char* tab_title = "PiHiEnd";
    switch (current_tab_) {
        case PhoneTab::Library:  tab_title = "发烧曲库"; break;
        case PhoneTab::Tuning:   tab_title = "发烧调音"; break;
        case PhoneTab::Hardware: tab_title = "硬件与服务"; break;
        case PhoneTab::Visual:   tab_title = "发烧动效大屏"; break;
    }

    if (Fonts::Large) ImGui::PushFont(Fonts::Large);
    dl->AddText(ImVec2(16.0f, y), UIConfig::Color::TextActive, tab_title);
    if (Fonts::Large) ImGui::PopFont();

    // 右上角：一键切回“数播模式”胶囊按钮
    float btn_w = 88.0f;
    float btn_h = 28.0f;
    float btn_x = screen_w - btn_w - 16.0f;
    ImVec2 b_min(btn_x, y);
    ImVec2 b_max(btn_x + btn_w, y + btn_h);

    ImGui::SetCursorScreenPos(b_min);
    if (ImGui::InvisibleButton("##HeaderSwitchToStreamerBtn", ImVec2(btn_w, btn_h))) {
        if (on_switch_to_streamer_) {
            on_switch_to_streamer_();
        }
    }
    bool hov = ImGui::IsItemHovered();
    ImU32 bg_col = hov ? IM_COL32(250, 45, 72, 80) : IM_COL32(255, 255, 255, 22);
    dl->AddRectFilled(b_min, b_max, bg_col, 14.0f);
    dl->AddRect(b_min, b_max, hov ? UIConfig::Color::Accent : UIConfig::Color::GlassBorder, 14.0f, 0, 1.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 t_sz = ImGui::CalcTextSize("切为数播");
    dl->AddText(ImVec2(btn_x + (btn_w - t_sz.x) * 0.5f, y + (btn_h - t_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "切为数播");
    if (Fonts::Small) ImGui::PopFont();
}

void HifiPhoneRenderer::renderMainContent(float screen_w, float content_y, float content_h) {
    float x = 12.0f;
    float w = screen_w - 24.0f;

    switch (current_tab_) {
        case PhoneTab::Library:
            renderLibraryTab(x, content_y, w, content_h);
            break;
        case PhoneTab::Tuning:
            renderTuningTab(x, content_y, w, content_h);
            break;
        case PhoneTab::Hardware:
            renderHardwareTab(x, content_y, w, content_h);
            break;
        case PhoneTab::Visual:
            renderVisualTab(x, content_y, w, content_h);
            break;
    }
}

// ==============================================================================
// 1. 曲库页面 (移动端双行卡片列表)
// ==============================================================================
void HifiPhoneRenderer::renderLibraryTab(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 card_min(x, y);
    ImVec2 card_max(x + w, y + h);
    GlassCardRenderer::drawCard(dl, card_min, card_max, 14.0f, "phone_library");

    // 头部：曲目总数与重新扫描
    float pad = 12.0f;
    char count_str[64];
    std::snprintf(count_str, sizeof(count_str), "全部曲目 (%zu 首)", cached_tracks_.size());

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(x + pad, y + pad), UIConfig::Color::TextActive, count_str);
    if (Fonts::Medium) ImGui::PopFont();

    // 刷新/扫歌按钮
    float scan_btn_w = 64.0f;
    float scan_btn_h = 26.0f;
    float scan_btn_x = x + w - scan_btn_w - pad;
    float scan_btn_y = y + pad - 2.0f;
    ImGui::SetCursorScreenPos(ImVec2(scan_btn_x, scan_btn_y));
    if (ImGui::InvisibleButton("##PhoneScanBtn", ImVec2(scan_btn_w, scan_btn_h))) {
        MusicScanManager::getInstance().startScan();
        cached_tracks_ = MusicDatabase::getInstance().loadScannedTracks();
    }
    bool hov_scan = ImGui::IsItemHovered();
    dl->AddRectFilled(ImVec2(scan_btn_x, scan_btn_y), ImVec2(scan_btn_x + scan_btn_w, scan_btn_y + scan_btn_h),
                      hov_scan ? IM_COL32(250, 45, 72, 70) : IM_COL32(255, 255, 255, 18), 13.0f);
    dl->AddRect(ImVec2(scan_btn_x, scan_btn_y), ImVec2(scan_btn_x + scan_btn_w, scan_btn_y + scan_btn_h),
                UIConfig::Color::GlassBorder, 13.0f, 0, 1.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 sb_sz = ImGui::CalcTextSize("扫描");
    dl->AddText(ImVec2(scan_btn_x + (scan_btn_w - sb_sz.x) * 0.5f, scan_btn_y + (scan_btn_h - sb_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "扫描");
    if (Fonts::Small) ImGui::PopFont();

    // 歌曲滚动列表容器
    float list_y = y + pad + 32.0f;
    float list_h = h - (list_y - y) - pad;
    if (list_h <= 50.0f) return;

    ImGui::SetCursorScreenPos(ImVec2(x + 4.0f, list_y));
    ImGuiWindowFlags list_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    ImGui::BeginChild("##PhoneTrackListChild", ImVec2(w - 8.0f, list_h), false, list_flags);

    auto& player = PlayerAdmin::getInstance();
    auto cur_opt = player.getCurrentTrack();
    uint64_t cur_id = cur_opt.has_value() ? cur_opt->id : 0;

    const float item_h = 52.0f;
    for (size_t i = 0; i < cached_tracks_.size(); ++i) {
        const auto& track = cached_tracks_[i];
        bool is_cur = (track.id == cur_id);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1(p0.x + w - 8.0f, p0.y + item_h);

        std::string btn_id = "##phone_track_" + std::to_string(track.id);
        if (ImGui::InvisibleButton(btn_id.c_str(), ImVec2(w - 8.0f, item_h))) {
            player.playTracks(cached_tracks_, i);
        }
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();

        if (is_cur) {
            dl->AddRectFilled(p0, p1, IM_COL32(250, 45, 72, 50), 8.0f);
            dl->AddRect(p0, p1, IM_COL32(250, 45, 72, 180), 8.0f, 0, 1.0f);
        } else if (hov || act) {
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 18), 8.0f);
        }

        // 第一行：曲目名称
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImU32 name_col = is_cur ? UIConfig::Color::Accent : UIConfig::Color::TextActive;
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 6.0f), name_col, track.title.c_str());
        if (Fonts::Medium) ImGui::PopFont();

        // 第二行：艺术家 + 母带音质规格
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        char meta_str[128];
        int sec = track.duration_sec;
        std::snprintf(meta_str, sizeof(meta_str), "%s  ·  %02d:%02d",
                      track.artist.empty() ? "未知艺术家" : track.artist.c_str(), sec / 60, sec % 60);
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 30.0f), UIConfig::Color::TextMuted, meta_str);
        if (Fonts::Small) ImGui::PopFont();
    }

    ImGui::EndChild();
}

// ==============================================================================
// 2. 调音页面 (10段EQ + MSEB魔棒)
// ==============================================================================
void HifiPhoneRenderer::renderTuningTab(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 card_min(x, y);
    ImVec2 card_max(x + w, y + h);
    GlassCardRenderer::drawCard(dl, card_min, card_max, 14.0f, "phone_tuning");

    // 顶部子Tab切换：[10段EQ] | [MSEB魔棒]
    float switch_w = 200.0f;
    float switch_h = 32.0f;
    float switch_x = x + (w - switch_w) * 0.5f;
    float switch_y = y + 10.0f;
    dl->AddRectFilled(ImVec2(switch_x, switch_y), ImVec2(switch_x + switch_w, switch_y + switch_h),
                      IM_COL32(255, 255, 255, 14), 16.0f);

    float half_w = switch_w * 0.5f;
    // EQ 按钮
    ImGui::SetCursorScreenPos(ImVec2(switch_x, switch_y));
    if (ImGui::InvisibleButton("##SubTabEQ", ImVec2(half_w, switch_h))) {
        tuning_subtab_ = 0;
    }
    if (tuning_subtab_ == 0) {
        dl->AddRectFilled(ImVec2(switch_x, switch_y), ImVec2(switch_x + half_w, switch_y + switch_h),
                          UIConfig::Color::Accent, 16.0f);
    }
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 eq_sz = ImGui::CalcTextSize("10段图形EQ");
    dl->AddText(ImVec2(switch_x + (half_w - eq_sz.x) * 0.5f, switch_y + (switch_h - eq_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "10段图形EQ");
    if (Fonts::Small) ImGui::PopFont();

    // MSEB 按钮
    ImGui::SetCursorScreenPos(ImVec2(switch_x + half_w, switch_y));
    if (ImGui::InvisibleButton("##SubTabMSEB", ImVec2(half_w, switch_h))) {
        tuning_subtab_ = 1;
    }
    if (tuning_subtab_ == 1) {
        dl->AddRectFilled(ImVec2(switch_x + half_w, switch_y), ImVec2(switch_x + switch_w, switch_y + switch_h),
                          UIConfig::Color::Accent, 16.0f);
    }
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 mseb_sz = ImGui::CalcTextSize("调音魔棒 MSEB");
    dl->AddText(ImVec2(switch_x + half_w + (half_w - mseb_sz.x) * 0.5f, switch_y + (switch_h - mseb_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "调音魔棒 MSEB");
    if (Fonts::Small) ImGui::PopFont();

    // 调音内容区
    float tune_content_y = switch_y + switch_h + 10.0f;
    float tune_content_h = h - (tune_content_y - y) - 8.0f;

    if (tuning_subtab_ == 0) {
        eq_view_.render(dl, ImVec2(x + 6.0f, tune_content_y), ImVec2(x + w - 6.0f, tune_content_y + tune_content_h));
    } else {
        magic_tuning_view_.render(x + 6.0f, tune_content_y, w - 12.0f, tune_content_h);
    }
}

// ==============================================================================
// 3. 硬件与服务页面 (DAC + Web服务)
// ==============================================================================
void HifiPhoneRenderer::renderHardwareTab(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 card_min(x, y);
    ImVec2 card_max(x + w, y + h);
    GlassCardRenderer::drawCard(dl, card_min, card_max, 14.0f, "phone_hardware");

    float pad = 16.0f;
    float cur_y = y + pad;

    // 标题
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(x + pad, cur_y), UIConfig::Color::TextActive, "硬件解码与无线服务");
    if (Fonts::Medium) ImGui::PopFont();
    cur_y += 36.0f;

    // Web 服务卡片
    bool is_web_running = WebService::getInstance().isRunning();
    ImVec2 web_min(x + pad, cur_y);
    ImVec2 web_max(x + w - pad, cur_y + 88.0f);
    dl->AddRectFilled(web_min, web_max, IM_COL32(255, 255, 255, 14), 10.0f);
    dl->AddRect(web_min, web_max, UIConfig::Color::GlassBorder, 10.0f, 0, 1.0f);

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(web_min.x + 12.0f, web_min.y + 12.0f), UIConfig::Color::TextActive, "Web 远程控制服务");
    if (Fonts::Medium) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    std::string web_url = is_web_running ? ("http://局域网IP:" + std::to_string(WebService::getInstance().getPort())) : "服务已关闭";
    dl->AddText(ImVec2(web_min.x + 12.0f, web_min.y + 40.0f),
                is_web_running ? IM_COL32(76, 217, 100, 255) : UIConfig::Color::TextMuted, web_url.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 开关按钮
    float tog_w = 64.0f;
    float tog_h = 32.0f;
    float tog_x = web_max.x - tog_w - 12.0f;
    float tog_y = web_min.y + (88.0f - tog_h) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(tog_x, tog_y));
    if (ImGui::InvisibleButton("##PhoneWebTogBtn", ImVec2(tog_w, tog_h))) {
        if (is_web_running) {
            WebService::getInstance().stop();
        } else {
            WebService::getInstance().start(8080);
        }
    }
    bool hov_tog = ImGui::IsItemHovered();
    ImU32 tog_col = hov_tog ? (is_web_running ? IM_COL32(250, 45, 72, 100) : IM_COL32(255, 255, 255, 30)) : (is_web_running ? UIConfig::Color::Accent : IM_COL32(255, 255, 255, 22));
    dl->AddRectFilled(ImVec2(tog_x, tog_y), ImVec2(tog_x + tog_w, tog_y + tog_h), tog_col, 16.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    const char* tog_txt = is_web_running ? "开启" : "关闭";
    ImVec2 tt_sz = ImGui::CalcTextSize(tog_txt);
    dl->AddText(ImVec2(tog_x + (tog_w - tt_sz.x) * 0.5f, tog_y + (tog_h - tt_sz.y) * 0.5f),
                UIConfig::Color::TextActive, tog_txt);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += 104.0f;

    // DAC 硬件参数委托给 DACSettingView 渲染
    float dac_h = h - (cur_y - y) - pad;
    if (dac_h > 100.0f) {
        dac_view_.render(x + pad, cur_y, w - pad * 2.0f, dac_h);
    }
}

// ==============================================================================
// 4. 发烧动效大屏 (金嗓子 / 开盘机 / VU表头)
// ==============================================================================
void HifiPhoneRenderer::renderVisualTab(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 card_min(x, y);
    ImVec2 card_max(x + w, y + h);
    GlassCardRenderer::drawCard(dl, card_min, card_max, 14.0f, "phone_visual");

    auto& player = PlayerAdmin::getInstance();
    bool is_playing = player.isPlaying();
    float levels12[12] = {0.0f};
    float l = 0.0f, r = 0.0f;
    if (is_playing) {
        player.getSpectrumLevels(levels12, 12);
        l = std::clamp((levels12[0] + levels12[1] + levels12[2]) * 0.35f, 0.0f, 1.0f);
        r = std::clamp((levels12[2] + levels12[3] + levels12[4]) * 0.35f, 0.0f, 1.0f);
    }

    auto bg_mode = ThemeManager::getInstance().getBackgroundVisualMode();
    if (bg_mode == BackgroundVisualMode::Accuphase) {
        accuphase_renderer_.setTheme(static_cast<int>(ThemeManager::getInstance().getCurrentTheme()));
        accuphase_renderer_.render(w, h, l, r);
    } else if (bg_mode == BackgroundVisualMode::TapeReel) {
        tape_renderer_.render(w, h, is_playing, idle_timer_);
    } else if (bg_mode == BackgroundVisualMode::VUMeter) {
        vu_renderer_.render(w, h, l, r);
    } else {
        siri_renderer_.render(w, h, is_playing, l, r);
    }
}

// ==============================================================================
// 5. 常驻底部 Mini Player
// ==============================================================================
void HifiPhoneRenderer::renderMiniPlayer(float screen_w, float bottom_y) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float margin = 12.0f;
    float h = 54.0f;
    ImVec2 p_min(margin, bottom_y);
    ImVec2 p_max(screen_w - margin, bottom_y + h);

    // 毛玻璃胶囊底板
    GlassCardRenderer::drawCard(dl, p_min, p_max, 27.0f, "mini_player");

    auto& player = PlayerAdmin::getInstance();
    auto cur_t = player.getCurrentTrack();

    // 点击左半部整块区域：向上滑出全屏播放大页
    float touch_area_w = (screen_w - margin * 2.0f) - 100.0f;
    ImGui::SetCursorScreenPos(p_min);
    if (ImGui::InvisibleButton("##OpenNowPlayingOverlayBtn", ImVec2(touch_area_w, h))) {
        show_now_playing_ = true;
    }

    // 左侧小光点 / 状态指示
    float dot_x = p_min.x + 20.0f;
    float dot_y = p_min.y + h * 0.5f;
    dl->AddCircleFilled(ImVec2(dot_x, dot_y), 5.0f, player.isPlaying() ? UIConfig::Color::Accent : UIConfig::Color::TextMuted);

    // 歌名与歌手
    const char* t_str = cur_t.has_value() ? cur_t->title.c_str() : "未在播放";
    const char* a_str = cur_t.has_value() ? cur_t->artist.c_str() : "点击选择曲目";

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(dot_x + 14.0f, p_min.y + 8.0f), UIConfig::Color::TextActive, t_str);
    if (Fonts::Medium) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(dot_x + 14.0f, p_min.y + 30.0f), UIConfig::Color::TextMuted, a_str);
    if (Fonts::Small) ImGui::PopFont();

    // 右侧播放/暂停与下一曲按键
    float btn_sz = 34.0f;
    float play_btn_x = p_max.x - 88.0f;
    float play_btn_y = p_min.y + (h - btn_sz) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(play_btn_x, play_btn_y));
    if (ImGui::InvisibleButton("##MiniPlayPauseBtn", ImVec2(btn_sz, btn_sz))) {
        player.togglePlayPause();
    }
    bool hov_p = ImGui::IsItemHovered();
    dl->AddCircleFilled(ImVec2(play_btn_x + btn_sz * 0.5f, play_btn_y + btn_sz * 0.5f),
                        btn_sz * 0.5f, hov_p ? IM_COL32(250, 45, 72, 80) : UIConfig::Color::Accent);

    // 下一曲
    float next_btn_x = p_max.x - 44.0f;
    ImGui::SetCursorScreenPos(ImVec2(next_btn_x, play_btn_y));
    if (ImGui::InvisibleButton("##MiniNextBtn", ImVec2(btn_sz, btn_sz))) {
        player.next();
    }
    bool hov_n = ImGui::IsItemHovered();
    dl->AddCircleFilled(ImVec2(next_btn_x + btn_sz * 0.5f, play_btn_y + btn_sz * 0.5f),
                        btn_sz * 0.5f, hov_n ? IM_COL32(255, 255, 255, 30) : IM_COL32(255, 255, 255, 14));
}

// ==============================================================================
// 6. 底部 TabBar (4个大图标触控区)
// ==============================================================================
void HifiPhoneRenderer::renderBottomTabBar(float screen_w, float tabbar_y, float tabbar_h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, tabbar_y), ImVec2(screen_w, tabbar_y + tabbar_h), IM_COL32(10, 12, 16, 230));
    dl->AddLine(ImVec2(0, tabbar_y), ImVec2(screen_w, tabbar_y), UIConfig::Color::GlassBorder, 1.0f);

    const char* tabs[4] = {"曲库", "调音", "硬件", "大屏"};
    PhoneTab tab_enums[4] = {PhoneTab::Library, PhoneTab::Tuning, PhoneTab::Hardware, PhoneTab::Visual};

    float tab_w = screen_w / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float tx0 = i * tab_w;
        ImGui::SetCursorScreenPos(ImVec2(tx0, tabbar_y));
        std::string tid = "##PhoneTabBtn_" + std::to_string(i);
        if (ImGui::InvisibleButton(tid.c_str(), ImVec2(tab_w, tabbar_h))) {
            current_tab_ = tab_enums[i];
        }
        bool active = (current_tab_ == tab_enums[i]);

        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImVec2 t_sz = ImGui::CalcTextSize(tabs[i]);
        ImU32 col = active ? UIConfig::Color::Accent : UIConfig::Color::TextMuted;
        dl->AddText(ImVec2(tx0 + (tab_w - t_sz.x) * 0.5f, tabbar_y + (tabbar_h - t_sz.y) * 0.5f), col, tabs[i]);
        if (Fonts::Medium) ImGui::PopFont();
    }
}

// ==============================================================================
// 7. 全屏沉浸式 Now Playing 播放详情大页
// ==============================================================================
void HifiPhoneRenderer::renderNowPlayingOverlay(float screen_w, float screen_h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // 纯黑沉浸背景
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(screen_w, screen_h), IM_COL32(5, 7, 10, 255));

    // 顶部收起按钮 (向下箭头 / "收起")
    float top_y = 50.0f;
    float back_btn_w = 60.0f;
    float back_btn_h = 30.0f;
    ImGui::SetCursorScreenPos(ImVec2(16.0f, top_y));
    if (ImGui::InvisibleButton("##CloseNowPlayingOverlayBtn", ImVec2(back_btn_w, back_btn_h))) {
        show_now_playing_ = false;
    }
    dl->AddRectFilled(ImVec2(16.0f, top_y), ImVec2(16.0f + back_btn_w, top_y + back_btn_h),
                      IM_COL32(255, 255, 255, 18), 15.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(30.0f, top_y + 6.0f), UIConfig::Color::TextActive, "收起");
    if (Fonts::Small) ImGui::PopFont();

    auto& player = PlayerAdmin::getInstance();
    auto cur_t = player.getCurrentTrack();

    // 上半部 (45% 高度)：发烧表头视觉动效
    float vis_y = top_y + 44.0f;
    float vis_h = screen_h * 0.38f;
    ImVec2 vis_min(16.0f, vis_y);
    ImVec2 vis_max(screen_w - 16.0f, vis_y + vis_h);
    GlassCardRenderer::drawCard(dl, vis_min, vis_max, 14.0f, "np_visual");

    // 运行表头动效
    float levels12[12] = {0.0f};
    float l = 0.0f, r = 0.0f;
    if (player.isPlaying()) {
        player.getSpectrumLevels(levels12, 12);
        l = std::clamp((levels12[0] + levels12[1]) * 0.4f, 0.0f, 1.0f);
        r = std::clamp((levels12[2] + levels12[3]) * 0.4f, 0.0f, 1.0f);
    }
    accuphase_renderer_.render(screen_w - 32.0f, vis_h, l, r);

    // 中部：曲目信息与母带音质徽章
    float info_y = vis_y + vis_h + 24.0f;
    const char* t_title = cur_t.has_value() ? cur_t->title.c_str() : "未在播放音频";
    const char* t_artist = cur_t.has_value() ? cur_t->artist.c_str() : "PiHiEnd 高保真播放器";

    if (Fonts::Large) ImGui::PushFont(Fonts::Large);
    dl->AddText(ImVec2(24.0f, info_y), UIConfig::Color::TextActive, t_title);
    if (Fonts::Large) ImGui::PopFont();

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(24.0f, info_y + 34.0f), UIConfig::Color::TextMuted, t_artist);
    if (Fonts::Medium) ImGui::PopFont();

    // 进度条与时间轴
    float prog_y = info_y + 80.0f;
    float bar_margin = 24.0f;
    float bar_w = screen_w - bar_margin * 2.0f;
    double cur_time = player.getCurrentTimeSec();
    double dur_time = player.getDurationSec();
    float progress = std::clamp(player.getProgress(), 0.0f, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(bar_margin, prog_y - 10.0f));
    if (ImGui::InvisibleButton("##NowPlayingSeekBtn", ImVec2(bar_w, 24.0f))) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float p = std::clamp((mouse_x - bar_margin) / bar_w, 0.0f, 1.0f);
        player.seek(static_cast<double>(p) * dur_time);
    }

    dl->AddRectFilled(ImVec2(bar_margin, prog_y), ImVec2(bar_margin + bar_w, prog_y + 4.0f),
                      IM_COL32(255, 255, 255, 30), 2.0f);
    dl->AddRectFilled(ImVec2(bar_margin, prog_y), ImVec2(bar_margin + bar_w * progress, prog_y + 4.0f),
                      UIConfig::Color::Accent, 2.0f);

    // 时间标字
    char time_str[32];
    int c_sec = static_cast<int>(cur_time);
    int d_sec = static_cast<int>(dur_time);
    std::snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d", c_sec / 60, c_sec % 60, d_sec / 60, d_sec % 60);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(bar_margin, prog_y + 10.0f), UIConfig::Color::TextMuted, time_str);
    if (Fonts::Small) ImGui::PopFont();

    // 下部：核心控制按键 (上一曲 | 播放/暂停 | 下一曲)
    float ctrl_y = prog_y + 54.0f;
    float play_btn_sz = 64.0f;
    float play_x = (screen_w - play_btn_sz) * 0.5f;

    // 上一曲
    float prev_x = play_x - 70.0f;
    ImGui::SetCursorScreenPos(ImVec2(prev_x, ctrl_y + 12.0f));
    if (ImGui::InvisibleButton("##NPPrevBtn", ImVec2(40.0f, 40.0f))) {
        player.previous();
    }
    dl->AddCircleFilled(ImVec2(prev_x + 20.0f, ctrl_y + 32.0f), 20.0f, IM_COL32(255, 255, 255, 20));

    // 播放/暂停 (大号居中圆圈)
    ImGui::SetCursorScreenPos(ImVec2(play_x, ctrl_y));
    if (ImGui::InvisibleButton("##NPPlayPauseBtn", ImVec2(play_btn_sz, play_btn_sz))) {
        player.togglePlayPause();
    }
    dl->AddCircleFilled(ImVec2(play_x + play_btn_sz * 0.5f, ctrl_y + play_btn_sz * 0.5f),
                        play_btn_sz * 0.5f, UIConfig::Color::Accent);

    // 下一曲
    float next_x = play_x + play_btn_sz + 30.0f;
    ImGui::SetCursorScreenPos(ImVec2(next_x, ctrl_y + 12.0f));
    if (ImGui::InvisibleButton("##NPNextBtn", ImVec2(40.0f, 40.0f))) {
        player.next();
    }
    dl->AddCircleFilled(ImVec2(next_x + 20.0f, ctrl_y + 32.0f), 20.0f, IM_COL32(255, 255, 255, 20));
}
