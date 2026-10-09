#include "HifiPadRenderer.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

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

HifiPadRenderer::HifiPadRenderer() {
    sidebar_.setOnCreatePlaylist([this]() {
        show_create_playlist_modal_ = true;
        create_playlist_focus_needed_ = true;
        new_playlist_name_buf_[0] = '\0';
    });
    sidebar_.setOnDacClick([this]() {
        sidebar_.setCurrentTab(SidebarTab::DACSettings);
    });
    main_stage_.setOnNavigateTab([this](SidebarTab tab) {
        sidebar_.setCurrentTab(tab);
    });
    main_stage_.setOnSelectPlaylist([this](SidebarTab tab, uint64_t pl_id) {
        if (tab == SidebarTab::CustomPlaylist) {
            sidebar_.setSelectedPlaylistId(pl_id);
        } else {
            sidebar_.setCurrentTab(tab);
        }
    });
    main_stage_.setOnIdleFullscreenChanged([this](float) {
        idle_timer_ = 0.0f;
        is_fullscreen_idle_ = false;
    });
}

void HifiPadRenderer::init() {
    last_frame_time_ = std::chrono::steady_clock::now();
    MusicDatabase::getInstance().init();
    ThemeManager::getInstance().init();
    MusicScanManager::getInstance().loadFromDatabase();
    playlists_ = MusicDatabase::getInstance().loadPlaylists();

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
            last_saved_tab_ = sidebar_.getCurrentTab();
            last_saved_playlist_id_ = sidebar_.getSelectedPlaylistId();
        } catch (...) {
            sidebar_.setCurrentTab(SidebarTab::AllMusic);
        }
    } else {
        sidebar_.setCurrentTab(SidebarTab::AllMusic);
    }
}

void HifiPadRenderer::render(float screen_w, float screen_h) {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;
    PlayerAdmin::getInstance().update(dt);

    idle_timer_ += dt;
    if (idle_timer_ >= 15.0f && PlayerAdmin::getInstance().isPlaying()) {
        is_fullscreen_idle_ = true;
    }
    float anim_speed = 3.2f;
    if (is_fullscreen_idle_) {
        anim_progress_ = std::min(1.0f, anim_progress_ + anim_speed * dt);
    } else {
        anim_progress_ = std::max(0.0f, anim_progress_ - anim_speed * dt);
    }

    renderBackground(screen_w, screen_h);

    float ease_t = anim_progress_ < 0.5f ? 4.0f * anim_progress_ * anim_progress_ * anim_progress_
                                         : 1.0f - std::pow(-2.0f * anim_progress_ + 2.0f, 3.0f) * 0.5f;

    if (anim_progress_ < 0.999f) {
        float sidebar_w = (screen_w < 900.0f) ? std::min(220.0f, std::max(160.0f, screen_w * 0.28f)) : 260.0f;
        float top_nav_dx = -sidebar_w * ease_t;
        float top_nav_dy = -150.0f * ease_t;
        float dac_dx = -sidebar_w * ease_t;
        float dac_dy = 120.0f * ease_t;
        float main_dx = (screen_w - sidebar_w) * ease_t;
        float main_dy = -150.0f * ease_t;
        float bottom_dx = screen_w * ease_t;
        float bottom_dy = 120.0f * ease_t;

        sidebar_.setDacConnected(true, main_stage_.getDacView().getCurrentChipName());
        sidebar_.render(playlists_, sidebar_w, screen_h, top_nav_dx, top_nav_dy, dac_dx, dac_dy);
        main_stage_.render(sidebar_.getCurrentTab(), sidebar_.getSelectedPlaylistId(), playlists_, 
                           sidebar_w + main_dx, 0.0f + main_dy, screen_w - sidebar_w, screen_h);
        bottom_bar_.render(screen_w, screen_h, bottom_dx, bottom_dy);
    }

    if (show_create_playlist_modal_) {
        renderCreatePlaylistModal(screen_w, screen_h);
    }

    SidebarTab cur_tab = sidebar_.getCurrentTab();
    uint64_t cur_pl_id = sidebar_.getSelectedPlaylistId();
    if (cur_tab != last_saved_tab_ || cur_pl_id != last_saved_playlist_id_) {
        last_saved_tab_ = cur_tab;
        last_saved_playlist_id_ = cur_pl_id;
        MusicDatabase::getInstance().setSetting("sidebar_tab", std::to_string(static_cast<int>(cur_tab)));
        MusicDatabase::getInstance().setSetting("sidebar_playlist_id", std::to_string(cur_pl_id));
    }
}

void HifiPadRenderer::renderBackground(float screen_w, float screen_h) {
    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();
    if (!bg_dl) return;

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

    if (!is_playing) {
        return;
    }

    // LED 频谱模式
    const float margin_x = UIConfig::Layout::ContainerMarginX;
    const float total_w = screen_w - margin_x * 2.0f;
    const int num_cols = 48;
    const float gap_x = 4.0f;
    const float col_w = (total_w - (num_cols - 1) * gap_x) / num_cols;
    const int num_rows = 69;
    const float seg_h = 6.0f;
    const float gap_y = 2.5f;
    const float seg_round = 1.2f;
    const float bot_y = screen_h - 8.0f;

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 cr = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 cg = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 cb = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const ImU32 lit_color = ThemeManager::getInstance().getSpectrumLitColor();
    const ImU32 peak_color = ThemeManager::getInstance().getSpectrumPeakColor();
    const ImU32 unlit_color = ThemeManager::getInstance().getSpectrumUnlitColor();

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

    for (int c = 0; c < num_cols; ++c) {
        float x0 = margin_x + c * (col_w + gap_x);
        float x1 = x0 + col_w;
        float norm_x = static_cast<float>(c) / static_cast<float>(num_cols - 1);
        float pos = norm_x * 11.0f;
        int idx0 = static_cast<int>(pos);
        int idx1 = std::min(idx0 + 1, 11);
        float frac = pos - static_cast<float>(idx0);
        float smooth_t = (1.0f - std::cos(frac * 3.14159265f)) * 0.5f;
        float level = levels12[idx0] * (1.0f - smooth_t) + levels12[idx1] * smooth_t;
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

void HifiPadRenderer::renderCreatePlaylistModal(float screen_w, float screen_h) {
    ImGuiIO& io = ImGui::GetIO();

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
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetWindowPos();
        ImVec2 p_max(p_min.x + modal_w, p_min.y + modal_h);

        dl->AddRectFilled(ImVec2(p_min.x - 2.0f, p_min.y + 4.0f),
                          ImVec2(p_max.x + 2.0f, p_max.y + 14.0f),
                          IM_COL32(0, 0, 0, 110), 18.0f);
        GlassCardRenderer::drawFrosted(dl, p_min, p_max, 16.0f);

        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "新建播放列表");
        if (Fonts::Medium) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        size_t utf8_len = getUtf8Length(new_playlist_name_buf_);
        if (utf8_len > 8) {
            std::string truncated = truncateUtf8(new_playlist_name_buf_, 8);
            std::snprintf(new_playlist_name_buf_, sizeof(new_playlist_name_buf_), "%s", truncated.c_str());
            utf8_len = 8;
        }

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

        utf8_len = getUtf8Length(new_playlist_name_buf_);
        if (utf8_len > 8) {
            std::string truncated = truncateUtf8(new_playlist_name_buf_, 8);
            std::snprintf(new_playlist_name_buf_, sizeof(new_playlist_name_buf_), "%s", truncated.c_str());
            utf8_len = 8;
        }

        std::string count_str = std::to_string(utf8_len) + " / 8 字";
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        float count_w = ImGui::CalcTextSize(count_str.c_str()).x;
        ImGui::SetCursorPosX(modal_w - 24.0f - count_w);
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.8f), "%s", count_str.c_str());
        if (Fonts::Small) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        const float btn_w = 96.0f;
        const float btn_h = 34.0f;
        ImGui::SetCursorPosX(modal_w - 24.0f - btn_w * 2.0f - 12.0f);

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
