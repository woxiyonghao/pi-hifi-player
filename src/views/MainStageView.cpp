#include "views/MainStageView.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "widgets/DrawUtils.hpp"
#include "tools/MusicDatabase.hpp"
#include "themes/ThemeManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <random>

MainStageView::MainStageView() {
}

void MainStageView::renderScanMusicView(float x, float y, float w, float h, std::vector<Playlist>& playlists) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    // 绘制卡片边框背景
    drawLiquidCard(dl, card_min, card_max, nullptr, nullptr);

    // 将内容委托给独立的 ScanMusicWidget 渲染！
    scan_widget_.render(dl, card_min, card_max, playlists);
}

void MainStageView::drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle) {
    // 渲染顶级深空高密度毛玻璃卡片
    GlassCardRenderer::drawCard(dl, p_min, p_max, UIConfig::Layout::ContainerRounding, "main_stage");

    // 绘制标题
    if (title) {
        ImVec2 title_pos = ImVec2(p_min.x + 20.0f, p_min.y + 16.0f);
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        dl->AddText(title_pos, UIConfig::Color::TextActive, title);
        if (Fonts::Medium) ImGui::PopFont();
    }

    // 绘制副标题 / 描述
    if (subtitle) {
        ImVec2 sub_pos = ImVec2(p_min.x + 20.0f, p_min.y + 44.0f);
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(sub_pos, UIConfig::Color::TextMuted, subtitle);
        if (Fonts::Small) ImGui::PopFont();
    }
}

void MainStageView::render(SidebarTab current_tab, 
                           uint64_t selected_playlist_id, 
                           std::vector<Playlist>& playlists,
                           float stage_x, float stage_y, float stage_w, float stage_h) {
    // 创建主舞台专属透明顶层无边框窗口 (严格限制高度不重叠底部 BottomBar，消除事件劫持)
    ImGui::SetNextWindowPos(ImVec2(stage_x, stage_y));
    float safe_stage_h = std::min(stage_h, 524.0f); // BottomBar 位于 y = 536，卡片底位于 y = 514
    ImGui::SetNextWindowSize(ImVec2(stage_w, safe_stage_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar 
                           | ImGuiWindowFlags_NoResize 
                           | ImGuiWindowFlags_NoMove 
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoBackground
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##MainStageMasterRoot", nullptr, flags)) {
        // 根据当前导航项智能分发子面板渲染
        switch (current_tab) {
            case SidebarTab::ScanMusic:
                renderScanMusicView(stage_x, stage_y, stage_w, stage_h, playlists);
                break;
            case SidebarTab::Equalizer:
                renderEqualizerView(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::DACSettings:
                renderDACSettingsView(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::ThemeSettings:
                renderThemeSettingsView(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::SystemSettings:
                renderSystemSettingsView(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::AllMusic:
                renderAllMusicView(stage_x, stage_y, stage_w, stage_h, playlists);
                break;
            case SidebarTab::CustomPlaylist:
                renderPlaylistView(selected_playlist_id, playlists, stage_x, stage_y, stage_w, stage_h);
                break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void MainStageView::renderAllMusicView(float x, float y, float w, float h, const std::vector<Playlist>& playlists) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    auto tracks = MusicScanManager::getInstance().getScannedTracks();
    if (tracks.empty()) {
        for (const auto& pl : playlists) {
            for (const auto& t : pl.getTracks()) {
                tracks.push_back(t);
            }
        }
    }

    size_t total_count = tracks.size();
    uint32_t total_dur_sec = 0;
    for (const auto& t : tracks) {
        total_dur_sec += t.duration_sec;
    }

    std::string title = "所有音乐";
    std::string subtitle = total_count > 0 
        ? ("发烧曲库全景树状视图 · 共 " + std::to_string(total_count) + " 首 · 总时长: " + 
           std::to_string(total_dur_sec / 60) + " 分钟")
        : "本地发烧曲库树列表";

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, title.c_str(), subtitle.c_str());

    float content_x = card_min.x + 20.0f;
    float content_y = card_min.y + 72.0f;
    float content_w = card_max.x - card_min.x - 40.0f;
    float content_h = card_max.y - card_min.y - 84.0f;

    if (total_count == 0) {
        ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
        if (ImGui::BeginChild("##AllMusicEmptyChild", ImVec2(content_w, content_h), false, ImGuiWindowFlags_NoBackground)) {
            ImGui::Dummy(ImVec2(0.0f, 40.0f));
            if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
            ImGui::SetCursorPosX((content_w - 380.0f) * 0.5f);
            ImGui::TextColored(ImVec4(0.9f, 0.92f, 0.96f, 1.0f), "本地发烧曲库暂无音乐");
            if (Fonts::Regular) ImGui::PopFont();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImGui::SetCursorPosX((content_w - 380.0f) * 0.5f);
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "请前往「扫描音乐」检索本地音乐文件，或将歌曲导入播放列表。");
            if (Fonts::Small) ImGui::PopFont();

            ImGui::Dummy(ImVec2(0.0f, 18.0f));
            ImGui::SetCursorPosX((content_w - 130.0f) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button, UIConfig::Color::Accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 65, 95, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(230, 30, 60, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            if (ImGui::Button("前往扫描音乐", ImVec2(130.0f, 32.0f))) {
                if (on_navigate_tab_) {
                    on_navigate_tab_(SidebarTab::ScanMusic);
                }
            }
            if (Fonts::Small) ImGui::PopFont();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();
        return;
    }

    auto& player = PlayerAdmin::getInstance();
    const auto& current_track = player.getCurrentTrack();

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 1. 顶层工具栏：模式切换 (去除展开全部、折叠全部、随机播放三个按钮，严格满足截图二要求)
    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
    if (ImGui::BeginChild("##AllMusicToolbar", ImVec2(content_w, 32.0f), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {

        // 模式切换胶囊按钮：样式与色值与 Sidebar / BottomBar 纯正液态玻璃主题色完全对齐 (满足截图三要求)
        auto renderModeBtn = [this, r, g, b](const char* label, int mode) {
            bool is_act = (all_music_view_mode_ == mode);
            ImU32 btn_bg = is_act ? IM_COL32(r, g, b, 70) : IM_COL32(28, 34, 46, 170);
            ImU32 btn_hov = is_act ? IM_COL32(r, g, b, 95) : IM_COL32(42, 50, 68, 210);
            ImU32 btn_act = IM_COL32(r, g, b, 120);
            ImU32 btn_border = is_act ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 25);
            ImU32 text_col = is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal;

            ImGui::PushStyleColor(ImGuiCol_Button, btn_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_hov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_act);
            ImGui::PushStyleColor(ImGuiCol_Text, text_col);
            ImGui::PushStyleColor(ImGuiCol_Border, btn_border);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 4.0f));

            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            if (ImGui::Button(label, ImVec2(0.0f, 26.0f))) {
                all_music_view_mode_ = mode;
            }
            if (Fonts::Small) ImGui::PopFont();

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(5);
        };

        renderModeBtn("按音频格式", 0);
        ImGui::SameLine(0.0f, 8.0f);
        renderModeBtn("按艺术家/专辑", 1);
        ImGui::SameLine(0.0f, 8.0f);
        renderModeBtn("按存储目录", 2);
    }
    ImGui::EndChild();

    // 2. 树状列表主滚动视口 (去除垂直滚动条，满足截图一要求)
    float tree_y = content_y + 36.0f;
    float tree_h = content_h - 36.0f;
    ImGui::SetCursorScreenPos(ImVec2(content_x, tree_y));

    // 隐藏滚动条，完全杜绝截图一的粗灰滚动条，同时支持鼠标滚轮与触控屏自然滑动
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    if (ImGui::BeginChild("##AllMusicTreeScroll", ImVec2(content_w, tree_h), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) {

        float dt = ImGui::GetIO().DeltaTime;

        // 通用曲目行渲染器 (字阶适中精致，满足截图四要求；高亮与主题色对齐，满足截图三要求)
        auto renderTrackRow = [&](const Track& track, size_t row_idx, const std::vector<Track>& queue_context,
                                  size_t index_in_queue, float indent) {
            bool is_current = current_track.has_value() && (current_track->id == track.id || current_track->file_path == track.file_path);
            bool is_playing = is_current && player.isPlaying();

            ImGui::PushID(static_cast<int>(track.id * 1000 + row_idx));

            float avail_w = ImGui::GetContentRegionAvail().x;
            float row_h = 30.0f; // 紧凑优雅行高
            ImVec2 row_pos = ImGui::GetCursorScreenPos();
            ImVec2 row_min(row_pos.x + indent, row_pos.y);
            ImVec2 row_max(row_pos.x + avail_w, row_pos.y + row_h);

            bool clicked = ImGui::InvisibleButton("##TrackBtn", ImVec2(avail_w, row_h));
            bool hovered = ImGui::IsItemHovered();

            ImDrawList* cur_dl = ImGui::GetWindowDrawList();

            // 背景悬停与在播液态玻璃主题高亮底纹 (对齐 Theme 色，不突兀)
            if (is_current) {
                cur_dl->AddRectFilled(row_min, row_max, IM_COL32(r, g, b, 50), 6.0f);
                cur_dl->AddRectFilled(row_min, row_max, UIConfig::Color::GlassActive, 6.0f);
                cur_dl->AddRect(row_min, row_max, UIConfig::Color::GlassBorder, 6.0f, 0, 1.0f);
            } else if (hovered) {
                cur_dl->AddRectFilled(row_min, row_max, UIConfig::Color::GlassHover, 6.0f);
                cur_dl->AddRect(row_min, row_max, IM_COL32(255, 255, 255, 25), 6.0f, 0, 1.0f);
            }

            float left_x = row_min.x + 8.0f;
            float text_y = row_pos.y + 6.0f;

            // 状态角标与律动频谱动画
            if (is_playing) {
                float t = static_cast<float>(ImGui::GetTime());
                float b1 = 3.5f + 5.0f * std::abs(std::sin(t * 6.0f));
                float b2 = 2.5f + 6.5f * std::abs(std::sin(t * 7.5f + 1.2f));
                float b3 = 4.0f + 6.0f * std::abs(std::sin(t * 5.2f + 2.5f));
                float cy = row_pos.y + row_h * 0.5f;

                cur_dl->AddLine(ImVec2(left_x, cy + b1 * 0.5f), ImVec2(left_x, cy - b1 * 0.5f), accent, 2.0f);
                cur_dl->AddLine(ImVec2(left_x + 4.5f, cy + b2 * 0.5f), ImVec2(left_x + 4.5f, cy - b2 * 0.5f), accent, 2.0f);
                cur_dl->AddLine(ImVec2(left_x + 9.0f, cy + b3 * 0.5f), ImVec2(left_x + 9.0f, cy - b3 * 0.5f), accent, 2.0f);
                left_x += 18.0f;
            } else if (is_current) {
                // 暂停状态下显示发光主题角标
                cur_dl->AddText(ImVec2(left_x, text_y), accent, "▶");
                left_x += 16.0f;
            } else {
                char num_buf[16];
                std::snprintf(num_buf, sizeof(num_buf), "%02zu.", row_idx + 1);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                cur_dl->AddText(ImVec2(left_x, text_y + 1.0f), IM_COL32(140, 155, 175, 200), num_buf);
                if (Fonts::Small) ImGui::PopFont();
                left_x += 24.0f;
            }

            // 曲目标题 (适中字阶，播放中保持白色纯净优雅)
            ImU32 title_color = (is_current || hovered) ? UIConfig::Color::TextActive : IM_COL32(215, 225, 238, 230);
            cur_dl->AddText(ImVec2(left_x, text_y), title_color, track.title.c_str());

            float title_w = ImGui::CalcTextSize(track.title.c_str()).x;
            left_x += title_w + 10.0f;

            // 艺术家 (Small 字号)
            if (!track.artist.empty()) {
                std::string art = "- " + track.artist;
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                cur_dl->AddText(ImVec2(left_x, text_y + 1.0f), UIConfig::Color::TextMuted, art.c_str());
                if (Fonts::Small) ImGui::PopFont();
            }

            // 右侧区域：播放按钮 + 时长 + 格式徽标
            float right_x = row_max.x - 8.0f;

            float btn_w = 42.0f;
            float btn_h = 20.0f;
            float btn_x = right_x - btn_w;
            float btn_y = row_pos.y + 5.0f;
            bool btn_hov = ImGui::IsMouseHoveringRect(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h));
            bool btn_click = btn_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            cur_dl->AddRectFilled(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h),
                                  btn_hov ? IM_COL32(r, g, b, 90) : IM_COL32(40, 48, 66, 170), 4.0f);
            cur_dl->AddRect(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h),
                            btn_hov ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 20), 4.0f, 0, 1.0f);
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            cur_dl->AddText(ImVec2(btn_x + 8.0f, btn_y + 2.0f), IM_COL32(255, 255, 255, 255), "播放");
            if (Fonts::Small) ImGui::PopFont();

            right_x = btn_x - 12.0f;

            // 时长
            if (track.duration_sec > 0) {
                char dur_buf[32];
                std::snprintf(dur_buf, sizeof(dur_buf), "%02u:%02u", track.duration_sec / 60, track.duration_sec % 60);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                float dur_w = ImGui::CalcTextSize(dur_buf).x;
                right_x -= dur_w;
                cur_dl->AddText(ImVec2(right_x, text_y + 1.0f), IM_COL32(150, 165, 185, 200), dur_buf);
                if (Fonts::Small) ImGui::PopFont();
                right_x -= 12.0f;
            }

            // 音频规格徽标 (精致紧凑)
            std::string badge = track.getFormatBadge();
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            float badge_txt_w = ImGui::CalcTextSize(badge.c_str()).x;
            float badge_w = badge_txt_w + 10.0f;
            float badge_h = 17.0f;
            float badge_x = right_x - badge_w;
            float badge_y = row_pos.y + 6.5f;

            cur_dl->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(18, 24, 34, 210), 3.0f);
            cur_dl->AddRect(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(50, 70, 95, 160), 3.0f, 0, 1.0f);
            cur_dl->AddText(ImVec2(badge_x + 5.0f, badge_y + 1.0f), IM_COL32(65, 190, 255, 230), badge.c_str());
            if (Fonts::Small) ImGui::PopFont();

            if (clicked || btn_click) {
                player.playTracks(queue_context, index_in_queue);
            }

            ImGui::PopID();
        };

        // 通用树节点 Header 渲染器 (圆润矢量倒角 + 180度平滑旋转展开动效，满足截图五要求；Regular 适度字阶，满足截图四要求)
        auto renderTreeNodeHeader = [&](const std::string& key, const char* node_title, const char* badge,
                                        size_t count, const std::vector<Track>& group_tracks, float indent) -> bool {
            bool is_open = tree_expanded_.find(key) == tree_expanded_.end() ? true : tree_expanded_[key];

            // 180° 旋转阻尼平滑插值计算 (1.0f = 展开向下, 0.0f = 折叠向上)
            float target_t = is_open ? 1.0f : 0.0f;
            if (tree_anim_t_.find(key) == tree_anim_t_.end()) {
                tree_anim_t_[key] = target_t;
            }
            float& cur_t = tree_anim_t_[key];
            cur_t += (target_t - cur_t) * std::clamp(dt * 13.0f, 0.0f, 1.0f);
            if (std::abs(target_t - cur_t) < 0.005f) {
                cur_t = target_t;
            }

            ImGui::PushID(key.c_str());

            float avail_w = ImGui::GetContentRegionAvail().x;
            float node_h = 32.0f; // 精致紧凑高度
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 node_min(pos.x + indent, pos.y);
            ImVec2 node_max(pos.x + avail_w, pos.y + node_h);

            bool clicked = ImGui::InvisibleButton("##NodeBtn", ImVec2(avail_w, node_h));
            bool hovered = ImGui::IsItemHovered();

            if (clicked) {
                is_open = !is_open;
                tree_expanded_[key] = is_open;
            }

            ImDrawList* cur_dl = ImGui::GetWindowDrawList();

            ImU32 bg_col = is_open ? IM_COL32(30, 36, 52, 190) : (hovered ? IM_COL32(36, 44, 62, 170) : IM_COL32(22, 28, 40, 150));
            cur_dl->AddRectFilled(node_min, node_max, bg_col, 7.0f);
            cur_dl->AddRect(node_min, node_max, is_open ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 25), 7.0f, 0, 1.0f);

            // =========================================================================
            // 绘制纯矢量圆角三角形，并进行 180° 旋转平滑动画 (完美彻底解决截图五痛点)
            // cur_t = 1.0f (展开态): 角度 = +PI/2 (指向正下方 ▼)
            // cur_t = 0.0f (收起态): 角度 = -PI/2 (指向正上方 ▲, 旋转整整 180 度)
            // =========================================================================
            float angle = -1.5707963f + cur_t * 3.14159265f;
            ImVec2 tri_center(node_min.x + 14.0f, pos.y + node_h * 0.5f);
            const float tri_r = 4.2f;

            ImVec2 tp0(tri_center.x + tri_r * std::cos(angle), tri_center.y + tri_r * std::sin(angle));
            ImVec2 tp1(tri_center.x + tri_r * std::cos(angle + 2.0943951f), tri_center.y + tri_r * std::sin(angle + 2.0943951f));
            ImVec2 tp2(tri_center.x + tri_r * std::cos(angle - 2.0943951f), tri_center.y + tri_r * std::sin(angle - 2.0943951f));

            ImU32 tri_col = is_open ? accent : IM_COL32(180, 195, 215, 210);
            DrawRoundedTriangle(cur_dl, tp0, tp1, tp2, 1.2f, tri_col);

            float left_x = node_min.x + 26.0f;
            float text_y = pos.y + 7.5f;

            // 格式徽标 (主题色匹配，满足截图三；Small 字阶，满足截图四)
            if (badge && badge[0] != '\0') {
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                float b_w = ImGui::CalcTextSize(badge).x + 8.0f;
                float b_h = 17.0f;
                float b_y = pos.y + (node_h - b_h) * 0.5f;
                cur_dl->AddRectFilled(ImVec2(left_x, b_y), ImVec2(left_x + b_w, b_y + b_h), IM_COL32(r, g, b, 45), 3.0f);
                cur_dl->AddRect(ImVec2(left_x, b_y), ImVec2(left_x + b_w, b_y + b_h), IM_COL32(r, g, b, 120), 3.0f, 0, 1.0f);
                cur_dl->AddText(ImVec2(left_x + 4.0f, b_y + 1.0f), IM_COL32(r, g, b, 240), badge);
                if (Fonts::Small) ImGui::PopFont();
                left_x += b_w + 8.0f;
            }

            // 标题 (使用 Regular 15px 替代原 Medium 20px，字体更加匀称精致，彻底解决截图四文字过大问题)
            cur_dl->AddText(ImVec2(left_x, text_y), UIConfig::Color::TextActive, node_title);

            // 右侧曲目统计与播放全部
            float right_x = node_max.x - 8.0f;

            float play_btn_w = 58.0f;
            float play_btn_h = 22.0f;
            float play_btn_x = right_x - play_btn_w;
            float play_btn_y = pos.y + (node_h - play_btn_h) * 0.5f;
            bool play_hov = ImGui::IsMouseHoveringRect(ImVec2(play_btn_x, play_btn_y), ImVec2(play_btn_x + play_btn_w, play_btn_y + play_btn_h));
            bool play_click = play_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            cur_dl->AddRectFilled(ImVec2(play_btn_x, play_btn_y), ImVec2(play_btn_x + play_btn_w, play_btn_y + play_btn_h),
                                  play_hov ? IM_COL32(r, g, b, 90) : IM_COL32(45, 54, 74, 180), 4.0f);
            cur_dl->AddRect(ImVec2(play_btn_x, play_btn_y), ImVec2(play_btn_x + play_btn_w, play_btn_y + play_btn_h),
                            play_hov ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 25), 4.0f, 0, 1.0f);
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            cur_dl->AddText(ImVec2(play_btn_x + 6.0f, play_btn_y + 3.0f), IM_COL32(255, 255, 255, 255), "播放全部");
            if (Fonts::Small) ImGui::PopFont();

            if (play_click && !group_tracks.empty()) {
                player.playTracks(group_tracks, 0);
            }

            right_x = play_btn_x - 12.0f;

            std::string count_str = std::to_string(count) + " 首曲目";
            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            float count_w = ImGui::CalcTextSize(count_str.c_str()).x;
            right_x -= count_w;
            cur_dl->AddText(ImVec2(right_x, text_y + 1.0f), UIConfig::Color::TextMuted, count_str.c_str());
            if (Fonts::Small) ImGui::PopFont();

            ImGui::Dummy(ImVec2(0.0f, 3.0f));
            ImGui::PopID();

            return is_open;
        };

        // 根据当前的树分类模式渲染树形结构
        if (all_music_view_mode_ == 0) {
            // Mode 0: 按音频格式分类
            struct FormatGroup {
                std::string key;
                std::string badge;
                std::string name;
                std::vector<Track> group_tracks;
            };

            std::vector<FormatGroup> groups = {
                { "dsf", "DSD", "DSD 原生母带 (DSF / DFF)", {} },
                { "flac", "FLAC", "FLAC 高解析无损母带", {} },
                { "wav", "WAV", "WAV 广播级线性母带", {} },
                { "alac", "ALAC", "Apple Lossless (ALAC)", {} },
                { "mp3", "MP3", "MP3 经典流行音频", {} },
                { "other", "RAW", "其它发烧音轨", {} }
            };

            for (const auto& t : tracks) {
                if (t.format == AudioFormat::DSD_DSF || t.format == AudioFormat::DSD_DFF) {
                    groups[0].group_tracks.push_back(t);
                } else if (t.format == AudioFormat::FLAC) {
                    groups[1].group_tracks.push_back(t);
                } else if (t.format == AudioFormat::WAV) {
                    groups[2].group_tracks.push_back(t);
                } else if (t.format == AudioFormat::ALAC) {
                    groups[3].group_tracks.push_back(t);
                } else if (t.format == AudioFormat::MP3) {
                    groups[4].group_tracks.push_back(t);
                } else {
                    groups[5].group_tracks.push_back(t);
                }
            }

            for (const auto& g : groups) {
                if (g.group_tracks.empty()) continue;
                bool open = renderTreeNodeHeader(g.key, g.name.c_str(), g.badge.c_str(), g.group_tracks.size(), g.group_tracks, 0.0f);
                if (open) {
                    for (size_t i = 0; i < g.group_tracks.size(); ++i) {
                        renderTrackRow(g.group_tracks[i], i, g.group_tracks, i, 20.0f);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }

        } else if (all_music_view_mode_ == 1) {
            // Mode 1: 按艺术家/专辑双层树
            std::map<std::string, std::map<std::string, std::vector<Track>>> art_alb_map;
            for (const auto& t : tracks) {
                std::string art = t.artist.empty() ? "未知艺术家" : t.artist;
                std::string alb = t.album.empty() ? "本地精选单曲" : t.album;
                art_alb_map[art][alb].push_back(t);
            }

            for (const auto& [artist_name, albums] : art_alb_map) {
                std::vector<Track> all_artist_tracks;
                for (const auto& [alb, alb_tracks] : albums) {
                    all_artist_tracks.insert(all_artist_tracks.end(), alb_tracks.begin(), alb_tracks.end());
                }

                std::string art_key = "art_" + artist_name;
                bool art_open = renderTreeNodeHeader(art_key, artist_name.c_str(), "歌手", all_artist_tracks.size(), all_artist_tracks, 0.0f);
                if (art_open) {
                    for (const auto& [album_name, alb_tracks] : albums) {
                        std::string alb_key = "alb_" + artist_name + "_" + album_name;
                        bool alb_open = renderTreeNodeHeader(alb_key, album_name.c_str(), "专辑", alb_tracks.size(), alb_tracks, 16.0f);
                        if (alb_open) {
                            for (size_t i = 0; i < alb_tracks.size(); ++i) {
                                renderTrackRow(alb_tracks[i], i, alb_tracks, i, 32.0f);
                                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                            }
                        }
                        ImGui::Dummy(ImVec2(0.0f, 3.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }

        } else {
            // Mode 2: 按存储目录结构
            std::map<std::string, std::vector<Track>> dir_map;
            for (const auto& t : tracks) {
                std::filesystem::path fp(t.file_path);
                std::string dir_name = fp.has_parent_path() ? fp.parent_path().filename().string() : "根目录";
                if (dir_name.empty()) dir_name = "本地曲库";
                dir_map[dir_name].push_back(t);
            }

            for (const auto& [dir_name, dir_tracks] : dir_map) {
                std::string dir_key = "dir_" + dir_name;
                bool dir_open = renderTreeNodeHeader(dir_key, dir_name.c_str(), "目录", dir_tracks.size(), dir_tracks, 0.0f);
                if (dir_open) {
                    for (size_t i = 0; i < dir_tracks.size(); ++i) {
                        renderTrackRow(dir_tracks[i], i, dir_tracks, i, 20.0f);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(); // Pop ScrollbarSize
}

void MainStageView::renderPlaylistView(uint64_t pid, std::vector<Playlist>& playlists, float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    Playlist* target_playlist = nullptr;
    for (auto& pl : playlists) {
        if (pl.getId() == pid) {
            target_playlist = &pl;
            break;
        }
    }
    if (!target_playlist && !playlists.empty()) {
        target_playlist = &playlists[0];
    }

    std::string title = target_playlist ? target_playlist->getName() : "发烧歌单";
    std::string subtitle = target_playlist 
        ? ("包含曲目: " + std::to_string(target_playlist->getTrackCount()) + " 首 · 总时长: " + 
           std::to_string(target_playlist->getTotalDurationSec() / 60) + " 分钟")
        : "暂无歌单";

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, title.c_str(), subtitle.c_str());

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 右上角工具按钮栏 (从右边排序是：删除播放列表，添加歌曲)
    if (target_playlist) {
        // 1. 最右侧：删除播放列表按钮 (具有微红警示色悬停底纹)
        float del_btn_w = 96.0f;
        float del_btn_h = 28.0f;
        float del_btn_x = card_max.x - 20.0f - del_btn_w;
        float del_btn_y = card_min.y + 18.0f;

        ImGui::SetCursorScreenPos(ImVec2(del_btn_x, del_btn_y));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(38, 44, 58, 170));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 45, 55, 170));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(230, 30, 45, 210));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 230, 245, 220));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 25));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        if (ImGui::Button("删除播放列表", ImVec2(del_btn_w, del_btn_h))) {
            show_delete_playlist_modal_ = true;
        }
        if (Fonts::Small) ImGui::PopFont();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);

        // 2. 左侧：添加歌曲按钮 (经典液态玻璃主题色)
        float add_btn_w = 88.0f;
        float add_btn_h = 28.0f;
        float add_btn_x = del_btn_x - 10.0f - add_btn_w;
        float add_btn_y = card_min.y + 18.0f;

        ImGui::SetCursorScreenPos(ImVec2(add_btn_x, add_btn_y));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(r, g, b, 60));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(r, g, b, 95));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(r, g, b, 130));
        ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextActive);
        ImGui::PushStyleColor(ImGuiCol_Border, UIConfig::Color::GlassBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        if (ImGui::Button("添加歌曲", ImVec2(add_btn_w, add_btn_h))) {
            show_add_music_modal_ = true;
            selected_track_ids_to_add_.clear();
            add_music_search_buf_[0] = '\0';
        }
        if (Fonts::Small) ImGui::PopFont();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
    }

    float content_x = card_min.x + 20.0f;
    float content_y = card_min.y + 75.0f;
    float content_w = card_max.x - card_min.x - 40.0f;
    float content_h = card_max.y - card_min.y - 90.0f;

    if (!target_playlist) {
        ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
        if (ImGui::BeginChild("##TrackListContentChildEmpty", ImVec2(content_w, content_h), false, ImGuiWindowFlags_NoBackground)) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "当前暂无歌单。点击左侧边栏「+ 添加播放列表」即可创建您的专属歌单。");
        }
        ImGui::EndChild();
        return;
    }

    auto& player = PlayerAdmin::getInstance();
    const auto& current_track = player.getCurrentTrack();

    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::BeginChild("##TrackListContentChild", ImVec2(content_w, content_h), false, child_flags)) {
        const auto& tracks = target_playlist->getTracks();
        if (tracks.empty()) {
            // 空状态：右上角已常驻「+ 添加歌曲」，中央保持纯净发烧通透
        } else {
            // 渲染歌单内曲目行，包含高亮、律动频谱、播放与移除按钮
            for (size_t i = 0; i < tracks.size(); ++i) {
                const auto& track = tracks[i];
                bool is_current = current_track.has_value() && (current_track->id == track.id || current_track->file_path == track.file_path);
                bool is_playing = is_current && player.isPlaying();

                ImGui::PushID(static_cast<int>(track.id * 1000 + i));

                float avail_w = ImGui::GetContentRegionAvail().x;
                float row_h = 32.0f;
                ImVec2 row_pos = ImGui::GetCursorScreenPos();
                ImVec2 row_min(row_pos.x, row_pos.y);
                ImVec2 row_max(row_pos.x + avail_w, row_pos.y + row_h);

                bool clicked = ImGui::InvisibleButton("##PlTrackBtn", ImVec2(avail_w, row_h));
                bool hovered = ImGui::IsItemHovered();

                ImDrawList* cur_dl = ImGui::GetWindowDrawList();

                if (is_current) {
                    cur_dl->AddRectFilled(row_min, row_max, IM_COL32(r, g, b, 50), 6.0f);
                    cur_dl->AddRectFilled(row_min, row_max, UIConfig::Color::GlassActive, 6.0f);
                    cur_dl->AddRect(row_min, row_max, UIConfig::Color::GlassBorder, 6.0f, 0, 1.0f);
                } else if (hovered) {
                    cur_dl->AddRectFilled(row_min, row_max, UIConfig::Color::GlassHover, 6.0f);
                    cur_dl->AddRect(row_min, row_max, IM_COL32(255, 255, 255, 25), 6.0f, 0, 1.0f);
                }

                float left_x = row_min.x + 8.0f;
                float text_y = row_pos.y + 7.0f;

                // 播放动效或序号
                if (is_playing) {
                    float t = static_cast<float>(ImGui::GetTime());
                    float b1 = 3.5f + 5.0f * std::abs(std::sin(t * 6.0f));
                    float b2 = 2.5f + 6.5f * std::abs(std::sin(t * 7.5f + 1.2f));
                    float b3 = 4.0f + 6.0f * std::abs(std::sin(t * 5.2f + 2.5f));
                    float cy = row_pos.y + row_h * 0.5f;

                    cur_dl->AddLine(ImVec2(left_x, cy + b1 * 0.5f), ImVec2(left_x, cy - b1 * 0.5f), accent, 2.0f);
                    cur_dl->AddLine(ImVec2(left_x + 4.5f, cy + b2 * 0.5f), ImVec2(left_x + 4.5f, cy - b2 * 0.5f), accent, 2.0f);
                    cur_dl->AddLine(ImVec2(left_x + 9.0f, cy + b3 * 0.5f), ImVec2(left_x + 9.0f, cy - b3 * 0.5f), accent, 2.0f);
                    left_x += 18.0f;
                } else if (is_current) {
                    cur_dl->AddText(ImVec2(left_x, text_y), accent, "▶");
                    left_x += 16.0f;
                } else {
                    char num_buf[16];
                    std::snprintf(num_buf, sizeof(num_buf), "%02zu.", i + 1);
                    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                    cur_dl->AddText(ImVec2(left_x, text_y + 1.0f), IM_COL32(140, 155, 175, 200), num_buf);
                    if (Fonts::Small) ImGui::PopFont();
                    left_x += 24.0f;
                }

                // 标题
                ImU32 title_color = (is_current || hovered) ? UIConfig::Color::TextActive : IM_COL32(215, 225, 238, 230);
                cur_dl->AddText(ImVec2(left_x, text_y), title_color, track.title.c_str());

                float title_w = ImGui::CalcTextSize(track.title.c_str()).x;
                left_x += title_w + 10.0f;

                // 艺术家
                if (!track.artist.empty()) {
                    std::string art = "- " + track.artist;
                    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                    cur_dl->AddText(ImVec2(left_x, text_y + 1.0f), UIConfig::Color::TextMuted, art.c_str());
                    if (Fonts::Small) ImGui::PopFont();
                }

                // 右侧按钮栏：移除 + 播放 + 时长 + 格式徽标
                float right_x = row_max.x - 8.0f;

                // 1. 移除按钮
                float del_w = 42.0f;
                float del_h = 20.0f;
                float del_x = right_x - del_w;
                float del_y = row_pos.y + 6.0f;
                bool del_hov = ImGui::IsMouseHoveringRect(ImVec2(del_x, del_y), ImVec2(del_x + del_w, del_y + del_h));
                bool del_click = del_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

                cur_dl->AddRectFilled(ImVec2(del_x, del_y), ImVec2(del_x + del_w, del_y + del_h),
                                      del_hov ? IM_COL32(200, 45, 55, 150) : IM_COL32(40, 48, 66, 170), 4.0f);
                cur_dl->AddRect(ImVec2(del_x, del_y), ImVec2(del_x + del_w, del_y + del_h),
                                del_hov ? IM_COL32(255, 70, 80, 200) : IM_COL32(255, 255, 255, 20), 4.0f, 0, 1.0f);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                cur_dl->AddText(ImVec2(del_x + 8.0f, del_y + 2.0f), del_hov ? IM_COL32(255, 120, 130, 255) : IM_COL32(180, 195, 215, 220), "移除");
                if (Fonts::Small) ImGui::PopFont();

                right_x = del_x - 8.0f;

                // 2. 播放按钮
                float play_w = 42.0f;
                float play_h = 20.0f;
                float play_x = right_x - play_w;
                float play_y = row_pos.y + 6.0f;
                bool play_hov = ImGui::IsMouseHoveringRect(ImVec2(play_x, play_y), ImVec2(play_x + play_w, play_y + play_h));
                bool play_click = play_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

                cur_dl->AddRectFilled(ImVec2(play_x, play_y), ImVec2(play_x + play_w, play_y + play_h),
                                      play_hov ? IM_COL32(r, g, b, 90) : IM_COL32(40, 48, 66, 170), 4.0f);
                cur_dl->AddRect(ImVec2(play_x, play_y), ImVec2(play_x + play_w, play_y + play_h),
                                play_hov ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 20), 4.0f, 0, 1.0f);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                cur_dl->AddText(ImVec2(play_x + 8.0f, play_y + 2.0f), IM_COL32(255, 255, 255, 255), "播放");
                if (Fonts::Small) ImGui::PopFont();

                right_x = play_x - 12.0f;

                // 3. 时长
                if (track.duration_sec > 0) {
                    char dur_buf[32];
                    std::snprintf(dur_buf, sizeof(dur_buf), "%02u:%02u", track.duration_sec / 60, track.duration_sec % 60);
                    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                    float dur_w = ImGui::CalcTextSize(dur_buf).x;
                    right_x -= dur_w;
                    cur_dl->AddText(ImVec2(right_x, text_y + 1.0f), IM_COL32(150, 165, 185, 200), dur_buf);
                    if (Fonts::Small) ImGui::PopFont();
                    right_x -= 12.0f;
                }

                // 4. 音频规格徽标
                std::string badge = track.getFormatBadge();
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                float badge_txt_w = ImGui::CalcTextSize(badge.c_str()).x;
                float badge_w = badge_txt_w + 10.0f;
                float badge_h = 17.0f;
                float badge_x = right_x - badge_w;
                float badge_y = row_pos.y + 7.5f;

                cur_dl->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(18, 24, 34, 210), 3.0f);
                cur_dl->AddRect(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(50, 70, 95, 160), 3.0f, 0, 1.0f);
                cur_dl->AddText(ImVec2(badge_x + 5.0f, badge_y + 1.0f), IM_COL32(65, 190, 255, 230), badge.c_str());
                if (Fonts::Small) ImGui::PopFont();

                if (del_click) {
                    target_playlist->removeTrack(i);
                    MusicDatabase::getInstance().savePlaylists(playlists);
                    ImGui::PopID();
                    break;
                }

                if (clicked || play_click) {
                    player.playPlaylist(*target_playlist, i);
                }

                ImGui::PopID();
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(); // Pop ScrollbarSize

    // 渲染添加歌曲模态弹窗
    if (show_add_music_modal_ && target_playlist) {
        renderAddMusicToPlaylistModal(target_playlist, playlists);
    }

    // 渲染删除播放列表二次确认模态弹窗
    if (show_delete_playlist_modal_ && target_playlist) {
        renderDeletePlaylistModal(target_playlist, playlists);
    }
}

void MainStageView::renderAddMusicToPlaylistModal(Playlist* target_playlist, std::vector<Playlist>& playlists) {
    if (!target_playlist) return;

    ImGuiIO& io = ImGui::GetIO();
    float screen_w = io.DisplaySize.x;
    float screen_h = io.DisplaySize.y;

    // 1. 全透明交互遮罩 (拦截底层鼠标点击，绝不添加发黑灰蒙层，底层视觉 100% 通透保留)
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags backdrop_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##AddMusicModalBackdrop", nullptr, backdrop_flags)) {
        ImGui::InvisibleButton("##AddMusicBackdropClickBlocker", io.DisplaySize);
        if (ImGui::IsItemClicked()) {
            show_add_music_modal_ = false;
            selected_track_ids_to_add_.clear();
        }
    }
    ImGui::End();

    // 2. 居中模态卡片尺寸与排版 (660px × 480px，留足右侧音频格式与时长空间)
    const float modal_w = 660.0f;
    const float modal_h = 480.0f;
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

    if (ImGui::Begin("##AddMusicModalDialog", nullptr, flags)) {
        // 绘制发烧级纯正毛玻璃卡片底板与柔和漫射阴影
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetWindowPos();
        ImVec2 p_max(p_min.x + modal_w, p_min.y + modal_h);

        dl->AddRectFilled(ImVec2(p_min.x - 2.0f, p_min.y + 4.0f),
                          ImVec2(p_max.x + 2.0f, p_max.y + 14.0f),
                          IM_COL32(0, 0, 0, 120), 18.0f);
        GlassCardRenderer::drawFrosted(dl, p_min, p_max, 16.0f);

        const ImU32 accent = UIConfig::Color::Accent;
        const uint32_t r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
        const uint32_t g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
        const uint32_t b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

        // 顶层 Header: 标题与目标歌单
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "添加歌曲到歌单");
        if (Fonts::Medium) ImGui::PopFont();

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        std::string tag_str = "「" + target_playlist->getName() + "」";
        ImGui::TextColored(ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, 0.95f), "%s", tag_str.c_str());
        if (Fonts::Small) ImGui::PopFont();

        // 右上角纯矢量 "✕" 关闭按钮 (彻底消除字库缺失导致的 "?" 乱码)
        float close_btn_size = 26.0f;
        float close_x = modal_w - 24.0f - close_btn_size;
        float close_y = 18.0f;
        ImGui::SetCursorPos(ImVec2(close_x, close_y));

        ImVec2 close_screen_pos = ImGui::GetCursorScreenPos();
        bool close_clicked = ImGui::InvisibleButton("##CloseModalBtn", ImVec2(close_btn_size, close_btn_size));
        bool close_hovered = ImGui::IsItemHovered();
        bool close_active = ImGui::IsItemActive();

        ImU32 close_bg = close_active ? IM_COL32(255, 255, 255, 40) : (close_hovered ? IM_COL32(255, 255, 255, 22) : IM_COL32(0, 0, 0, 0));
        dl->AddCircleFilled(ImVec2(close_screen_pos.x + close_btn_size * 0.5f, close_screen_pos.y + close_btn_size * 0.5f),
                            close_btn_size * 0.5f, close_bg);

        ImVec2 cross_center(close_screen_pos.x + close_btn_size * 0.5f, close_screen_pos.y + close_btn_size * 0.5f);
        float cr = 4.8f;
        ImU32 cross_col = close_hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 195, 215, 200);
        dl->AddLine(ImVec2(cross_center.x - cr, cross_center.y - cr), ImVec2(cross_center.x + cr, cross_center.y + cr), cross_col, 1.8f);
        dl->AddLine(ImVec2(cross_center.x + cr, cross_center.y - cr), ImVec2(cross_center.x - cr, cross_center.y + cr), cross_col, 1.8f);

        if (close_clicked) {
            show_add_music_modal_ = false;
            selected_track_ids_to_add_.clear();
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // 搜索输入框
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(32, 38, 52, 220));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 48, 65, 230));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(45, 54, 75, 240));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, UIConfig::Color::GlassBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 7.0f));

        ImGui::SetNextItemWidth(modal_w - 48.0f);
        ImGui::InputTextWithHint("##AddMusicSearchInput", "搜索曲名、艺术家、专辑或格式...",
                                 add_music_search_buf_, sizeof(add_music_search_buf_));

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 获取全部曲目与筛选
        std::vector<Track> all_scanned = MusicScanManager::getInstance().getScannedTracks();
        if (all_scanned.empty()) {
            all_scanned = MusicDatabase::getInstance().loadScannedTracks();
        }

        std::string search_query = add_music_search_buf_;
        std::transform(search_query.begin(), search_query.end(), search_query.begin(), ::tolower);

        std::vector<Track> filtered_tracks;
        for (const auto& t : all_scanned) {
            if (search_query.empty()) {
                filtered_tracks.push_back(t);
                continue;
            }
            std::string t_title = t.title;
            std::string t_artist = t.artist;
            std::string t_album = t.album;
            std::string t_badge = t.getFormatBadge();
            std::transform(t_title.begin(), t_title.end(), t_title.begin(), ::tolower);
            std::transform(t_artist.begin(), t_artist.end(), t_artist.begin(), ::tolower);
            std::transform(t_album.begin(), t_album.end(), t_album.begin(), ::tolower);
            std::transform(t_badge.begin(), t_badge.end(), t_badge.begin(), ::tolower);

            if (t_title.find(search_query) != std::string::npos ||
                t_artist.find(search_query) != std::string::npos ||
                t_album.find(search_query) != std::string::npos ||
                t_badge.find(search_query) != std::string::npos) {
                filtered_tracks.push_back(t);
            }
        }

        // 中间曲目列表区域 (无粗滚动条，采用内嵌 Padding 避免边缘裁剪)
        float list_w = modal_w - 48.0f;
        float list_h = modal_h - 180.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 4.0f));
        if (ImGui::BeginChild("##AddMusicTrackListScroll", ImVec2(list_w, list_h), true,
                              ImGuiWindowFlags_NoScrollbar)) {

            if (all_scanned.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 40.0f));
                ImGui::SetCursorPosX((list_w - 280.0f) * 0.5f);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.85f), "曲库暂无扫描曲目，请先前往「扫描音乐」");
                if (Fonts::Small) ImGui::PopFont();

                ImGui::Dummy(ImVec2(0.0f, 12.0f));
                ImGui::SetCursorPosX((list_w - 120.0f) * 0.5f);
                if (ImGui::Button("前往扫描音乐", ImVec2(120.0f, 32.0f))) {
                    if (on_navigate_tab_) {
                        on_navigate_tab_(SidebarTab::ScanMusic);
                    }
                    show_add_music_modal_ = false;
                }
            } else if (filtered_tracks.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 50.0f));
                ImGui::SetCursorPosX((list_w - 200.0f) * 0.5f);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.85f), "未找到符合条件的音频曲目");
                if (Fonts::Small) ImGui::PopFont();
            } else {
                for (size_t i = 0; i < filtered_tracks.size(); ++i) {
                    const auto& trk = filtered_tracks[i];

                    // 检查是否已经在歌单中
                    bool already_in = false;
                    for (const auto& pt : target_playlist->getTracks()) {
                        if (pt.id == trk.id || pt.file_path == trk.file_path) {
                            already_in = true;
                            break;
                        }
                    }

                    // 检查是否在当前选集
                    auto sel_it = std::find(selected_track_ids_to_add_.begin(), selected_track_ids_to_add_.end(), trk.id);
                    bool is_selected = (sel_it != selected_track_ids_to_add_.end());

                    ImGui::PushID(static_cast<int>(trk.id * 1000 + i));

                    float avail_w = ImGui::GetContentRegionAvail().x;
                    float row_h = 32.0f;
                    ImVec2 row_pos = ImGui::GetCursorScreenPos();
                    ImVec2 row_min = row_pos;
                    ImVec2 row_max(row_pos.x + avail_w, row_pos.y + row_h);

                    bool row_clicked = ImGui::InvisibleButton("##RowBtn", ImVec2(avail_w, row_h));
                    bool row_hovered = ImGui::IsItemHovered();

                    if (row_clicked && !already_in) {
                        if (is_selected) {
                            selected_track_ids_to_add_.erase(sel_it);
                        } else {
                            selected_track_ids_to_add_.push_back(trk.id);
                        }
                    }

                    ImDrawList* cur_dl = ImGui::GetWindowDrawList();

                    // 背景绘制
                    if (already_in) {
                        cur_dl->AddRectFilled(row_min, row_max, IM_COL32(25, 30, 42, 120), 5.0f);
                    } else if (is_selected) {
                        cur_dl->AddRectFilled(row_min, row_max, IM_COL32(r, g, b, 50), 5.0f);
                        cur_dl->AddRect(row_min, row_max, UIConfig::Color::GlassBorder, 5.0f, 0, 1.0f);
                    } else if (row_hovered) {
                        cur_dl->AddRectFilled(row_min, row_max, UIConfig::Color::GlassHover, 5.0f);
                    }

                    // 左侧复选框绘制 (纯矢量防锯齿绘制，彻底杜绝缺失字形导致的 "?" 乱码)
                    float box_size = 17.0f;
                    float box_x = row_min.x + 8.0f;
                    float box_y = row_pos.y + (row_h - box_size) * 0.5f;

                    if (already_in) {
                        // 已添加图标 (灰色背景 + 矢量勾选)
                        cur_dl->AddRectFilled(ImVec2(box_x, box_y), ImVec2(box_x + box_size, box_y + box_size), IM_COL32(50, 60, 75, 180), 3.5f);
                        const ImVec2 pts[3] = {
                            ImVec2(box_x + 4.0f, box_y + 8.5f),
                            ImVec2(box_x + 7.0f, box_y + 12.2f),
                            ImVec2(box_x + 13.0f, box_y + 5.0f)
                        };
                        cur_dl->AddPolyline(pts, 3, IM_COL32(160, 175, 195, 220), ImDrawFlags_None, 1.8f);
                    } else if (is_selected) {
                        // 选中图标 (主题色高亮底板 + 纯白矢量对勾)
                        cur_dl->AddRectFilled(ImVec2(box_x, box_y), ImVec2(box_x + box_size, box_y + box_size), accent, 3.5f);
                        const ImVec2 pts[3] = {
                            ImVec2(box_x + 4.0f, box_y + 8.5f),
                            ImVec2(box_x + 7.0f, box_y + 12.2f),
                            ImVec2(box_x + 13.0f, box_y + 5.0f)
                        };
                        cur_dl->AddPolyline(pts, 3, IM_COL32(255, 255, 255, 255), ImDrawFlags_None, 2.0f);
                    } else {
                        // 未选中框
                        cur_dl->AddRect(ImVec2(box_x, box_y), ImVec2(box_x + box_size, box_y + box_size), IM_COL32(100, 115, 135, 180), 3.5f, 0, 1.2f);
                    }

                    // 右侧：已在歌单标记 或 规格 + 时长 (优先从右侧倒排布局，确保右侧间距充足绝不截断)
                    float right_x = row_max.x - 10.0f;
                    if (already_in) {
                        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                        const char* exist_tag = "已在歌单";
                        float exist_w = ImGui::CalcTextSize(exist_tag).x + 12.0f;
                        float exist_h = 20.0f;
                        float exist_x = right_x - exist_w;
                        float exist_y = row_pos.y + (row_h - exist_h) * 0.5f;

                        cur_dl->AddRectFilled(ImVec2(exist_x, exist_y), ImVec2(exist_x + exist_w, exist_y + exist_h), IM_COL32(35, 42, 56, 190), 3.5f);
                        cur_dl->AddRect(ImVec2(exist_x, exist_y), ImVec2(exist_x + exist_w, exist_y + exist_h), IM_COL32(70, 85, 110, 160), 3.5f, 0, 1.0f);
                        cur_dl->AddText(ImVec2(exist_x + 6.0f, exist_y + 2.0f), IM_COL32(140, 155, 175, 220), exist_tag);
                        if (Fonts::Small) ImGui::PopFont();
                        right_x = exist_x - 10.0f;
                    } else {
                        // 1. 时长
                        if (trk.duration_sec > 0) {
                            char dur_buf[32];
                            std::snprintf(dur_buf, sizeof(dur_buf), "%02u:%02u", trk.duration_sec / 60, trk.duration_sec % 60);
                            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                            float dur_w = ImGui::CalcTextSize(dur_buf).x;
                            right_x -= dur_w;
                            cur_dl->AddText(ImVec2(right_x, row_pos.y + 7.0f), IM_COL32(150, 165, 185, 200), dur_buf);
                            if (Fonts::Small) ImGui::PopFont();
                            right_x -= 12.0f;
                        }

                        // 2. 规格徽标 (赋予充足宽度与内边距，文字与边框完整保留)
                        std::string badge = trk.getFormatBadge();
                        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                        float badge_txt_w = ImGui::CalcTextSize(badge.c_str()).x;
                        float badge_w = badge_txt_w + 12.0f;
                        float badge_h = 18.0f;
                        float badge_x = right_x - badge_w;
                        float badge_y = row_pos.y + (row_h - badge_h) * 0.5f;

                        cur_dl->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(18, 24, 34, 210), 3.5f);
                        cur_dl->AddRect(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(50, 70, 95, 160), 3.5f, 0, 1.0f);
                        cur_dl->AddText(ImVec2(badge_x + 6.0f, badge_y + 1.5f), IM_COL32(65, 190, 255, 230), badge.c_str());
                        if (Fonts::Small) ImGui::PopFont();
                        right_x = badge_x - 10.0f;
                    }

                    // 3. 曲名与艺术家 (安全裁剪矩形，防止超长曲名覆盖右侧徽标)
                    float text_x = box_x + box_size + 12.0f;
                    float text_y = row_pos.y + 7.0f;

                    cur_dl->PushClipRect(ImVec2(text_x, row_min.y), ImVec2(right_x - 6.0f, row_max.y), true);

                    ImU32 title_col = already_in ? IM_COL32(130, 145, 165, 180) :
                                      (is_selected ? UIConfig::Color::TextActive : IM_COL32(215, 225, 238, 230));
                    cur_dl->AddText(ImVec2(text_x, text_y), title_col, trk.title.c_str());

                    float title_w = ImGui::CalcTextSize(trk.title.c_str()).x;
                    text_x += title_w + 10.0f;

                    if (!trk.artist.empty()) {
                        std::string art = "- " + trk.artist;
                        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                        cur_dl->AddText(ImVec2(text_x, text_y + 1.0f), UIConfig::Color::TextMuted, art.c_str());
                        if (Fonts::Small) ImGui::PopFont();
                    }

                    cur_dl->PopClipRect();

                    ImGui::PopID();
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2); // Pop ScrollbarSize and WindowPadding

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // 底部工具与操作栏
        // 计算当前可添加曲目总数
        size_t available_candidates = 0;
        for (const auto& trk : filtered_tracks) {
            bool already_in = false;
            for (const auto& pt : target_playlist->getTracks()) {
                if (pt.id == trk.id || pt.file_path == trk.file_path) {
                    already_in = true;
                    break;
                }
            }
            if (!already_in) available_candidates++;
        }

        // 左侧：全选/反选 + 已选计数
        bool all_avail_selected = (available_candidates > 0) && (selected_track_ids_to_add_.size() >= available_candidates);

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 48, 64, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 62, 82, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 78, 102, 250));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);

        const char* sel_all_text = all_avail_selected ? "取消全选" : "全选全部";
        if (ImGui::Button(sel_all_text, ImVec2(80.0f, 30.0f))) {
            if (all_avail_selected) {
                selected_track_ids_to_add_.clear();
            } else {
                selected_track_ids_to_add_.clear();
                for (const auto& trk : filtered_tracks) {
                    bool already_in = false;
                    for (const auto& pt : target_playlist->getTracks()) {
                        if (pt.id == trk.id || pt.file_path == trk.file_path) {
                            already_in = true;
                            break;
                        }
                    }
                    if (!already_in) {
                        selected_track_ids_to_add_.push_back(trk.id);
                    }
                }
            }
        }

        ImGui::SameLine(0.0f, 14.0f);
        std::string sel_summary = "已选 " + std::to_string(selected_track_ids_to_add_.size()) + " 首曲目";
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0f);
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.85f), "%s", sel_summary.c_str());

        if (Fonts::Small) ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // 右侧：取消 + 确认添加
        float btn_w = 90.0f;
        float confirm_w = 100.0f;
        ImGui::SameLine(modal_w - 24.0f - btn_w - 10.0f - confirm_w);

        // 取消按钮
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 46, 60, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 60, 78, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 75, 96, 250));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        if (ImGui::Button("取消", ImVec2(btn_w, 30.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_add_music_modal_ = false;
            selected_track_ids_to_add_.clear();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // 确认添加按钮
        ImGui::SameLine(0.0f, 10.0f);
        bool has_selection = !selected_track_ids_to_add_.empty();
        ImU32 conf_bg = has_selection ? accent : IM_COL32(r, g, b, 70);
        ImU32 conf_hov = has_selection ? IM_COL32(std::min<uint32_t>(255u, r + 30u), std::min<uint32_t>(255u, g + 30u), std::min<uint32_t>(255u, b + 30u), 255) : conf_bg;

        ImGui::PushStyleColor(ImGuiCol_Button, conf_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, conf_hov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(r, g, b, 200));
        ImGui::PushStyleColor(ImGuiCol_Text, has_selection ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 160));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

        if (ImGui::Button("确认添加", ImVec2(confirm_w, 30.0f)) && has_selection) {
            for (uint64_t tid : selected_track_ids_to_add_) {
                auto it = std::find_if(all_scanned.begin(), all_scanned.end(), [tid](const Track& trk) {
                    return trk.id == tid;
                });
                if (it != all_scanned.end()) {
                    target_playlist->addTrack(*it);
                }
            }
            // 立即持久化至 SQLite 数据库
            MusicDatabase::getInstance().savePlaylists(playlists);

            show_add_music_modal_ = false;
            selected_track_ids_to_add_.clear();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void MainStageView::renderDeletePlaylistModal(Playlist* target_playlist, std::vector<Playlist>& playlists) {
    if (!target_playlist) return;

    ImGuiIO& io = ImGui::GetIO();
    float screen_w = io.DisplaySize.x;
    float screen_h = io.DisplaySize.y;

    // 1. 全透明交互遮罩 (拦截底层鼠标点击，绝不发黑遮挡)
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags backdrop_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##DeletePlaylistModalBackdrop", nullptr, backdrop_flags)) {
        ImGui::InvisibleButton("##DeletePlaylistBackdropClickBlocker", io.DisplaySize);
        if (ImGui::IsItemClicked()) {
            show_delete_playlist_modal_ = false;
        }
    }
    ImGui::End();

    // 2. 居中模态卡片尺寸与排版 (400px × 210px)
    const float modal_w = 400.0f;
    const float modal_h = 210.0f;
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 22.0f));

    if (ImGui::Begin("##DeletePlaylistModalDialog", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetWindowPos();
        ImVec2 p_max(p_min.x + modal_w, p_min.y + modal_h);

        dl->AddRectFilled(ImVec2(p_min.x - 2.0f, p_min.y + 4.0f),
                          ImVec2(p_max.x + 2.0f, p_max.y + 14.0f),
                          IM_COL32(0, 0, 0, 120), 18.0f);
        GlassCardRenderer::drawFrosted(dl, p_min, p_max, 16.0f);

        // 标题
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "删除播放列表");
        if (Fonts::Medium) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 提示说明文案
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        std::string confirm_text = "确定要删除播放列表「" + target_playlist->getName() + "」吗？";
        ImGui::TextColored(ImVec4(0.92f, 0.94f, 0.98f, 0.95f), "%s", confirm_text.c_str());
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.85f), "删除后该歌单将从侧边栏移除，本地磁盘音频源文件不受影响。");
        if (Fonts::Small) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        // 底部按钮栏：取消 / 确认删除
        const float btn_w = 96.0f;
        const float btn_h = 32.0f;
        ImGui::SetCursorPosX(modal_w - 24.0f - btn_w * 2.0f - 12.0f);

        // 1. 取消按钮
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 46, 60, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(52, 60, 78, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 75, 96, 250));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        if (ImGui::Button("取消", ImVec2(btn_w, btn_h)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            show_delete_playlist_modal_ = false;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // 2. 确认删除按钮 (红色警示色)
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 45, 55, 210));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(230, 40, 55, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 30, 45, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

        if (ImGui::Button("确认删除", ImVec2(btn_w, btn_h))) {
            size_t del_index = playlists.size();
            for (size_t i = 0; i < playlists.size(); ++i) {
                if (playlists[i].getId() == target_playlist->getId()) {
                    del_index = i;
                    break;
                }
            }

            if (del_index < playlists.size()) {
                SidebarTab next_tab = SidebarTab::AllMusic;
                uint64_t next_pl_id = 0;

                // 核心寻址逻辑 (严格符合用户指示)：
                // 1. 如果下一个是播放列表，则选中它 (del_index + 1 < playlists.size())
                if (del_index + 1 < playlists.size()) {
                    next_tab = SidebarTab::CustomPlaylist;
                    next_pl_id = playlists[del_index + 1].getId();
                }
                // 2. 如果是添加按钮，则往上寻找 (del_index > 0)
                else if (del_index > 0) {
                    next_tab = SidebarTab::CustomPlaylist;
                    next_pl_id = playlists[del_index - 1].getId();
                }
                // 3. 往上没有的话，那就是全部音乐了
                else {
                    next_tab = SidebarTab::AllMusic;
                    next_pl_id = 0;
                }

                // 从列表中删除
                playlists.erase(playlists.begin() + del_index);
                // 立即持久化至 SQLite 数据库
                MusicDatabase::getInstance().savePlaylists(playlists);

                // 通知主控制器切换激活项
                if (on_select_playlist_) {
                    on_select_playlist_(next_tab, next_pl_id);
                }
            }

            show_delete_playlist_modal_ = false;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void MainStageView::renderEqualizerView(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, "图形均衡器", "10 段专业频段精调 · 纯净硬件直通");

    // 委托给独立专业 EQ 调音组件渲染
    eq_view_.render(dl, card_min, card_max);
}

void MainStageView::renderDACSettingsView(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, "AK4191EQ + AK4499EX 旗舰平衡解码前级设置", "AKM Velvet Sound 数字滤波滚降特性与飞秒双时钟同步管理");
}

void MainStageView::renderThemeSettingsView(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 绘制主舞台大底板卡片
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "main_stage");

    auto& tm = ThemeManager::getInstance();
    ThemeId cur_theme = tm.getCurrentTheme();

    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // ==============================================================================
    // 1. 顶部 Header 栏：左侧主标题「主题」(无 subtitle)，右侧「恢复名机预设」按钮
    // ==============================================================================
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "主题");
    if (Fonts::Medium) ImGui::PopFont();

    // 右上角：「恢复名机预设」按钮
    float rst_w = 110.0f;
    float rst_h = 28.0f;
    float rst_x = card_max.x - 20.0f - rst_w;
    float rst_y = card_min.y + 15.0f;

    ImGui::SetCursorScreenPos(ImVec2(rst_x, rst_y));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 48, 64, 180));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(55, 66, 88, 220));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70, 84, 110, 250));
    ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextNormal);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    if (ImGui::Button("恢复名机预设", ImVec2(rst_w, rst_h))) {
        tm.setTheme(ThemeId::ModernCrimson);
        tm.setBackgroundVisualMode(BackgroundVisualMode::LEDSpectrum);
    }

    if (Fonts::Small) ImGui::PopFont();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // 辅助毛玻璃板块卡片绘制闭包
    auto drawSectionFrostedCard = [&](ImVec2 p0, ImVec2 p1, float rounding = 10.0f) {
        // 1. 深空磨砂底板
        dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), rounding);
        // 2. 漫射高光层
        dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 8), rounding);
        // 3. 顶部微光棱线
        dl->AddLine(ImVec2(p0.x + rounding, p0.y + 0.5f), ImVec2(p1.x - rounding, p0.y + 0.5f),
                    IM_COL32(255, 255, 255, 45), 1.0f);
        // 4. 1px 微光边框
        dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 28), rounding, 0, 1.0f);
    };

    float sec_x0 = card_min.x + 20.0f;
    float sec_x1 = card_max.x - 20.0f;
    float sec_w = sec_x1 - sec_x0;

    // ==============================================================================
    // 2. 板块一：预设 (标题改为「预设」，无 subtitle，带独立毛玻璃容器与 Options)
    // ==============================================================================
    float sec1_y0 = card_min.y + 52.0f;
    float sec1_h = 154.0f;
    float sec1_y1 = sec1_y0 + sec1_h;
    drawSectionFrostedCard(ImVec2(sec_x0, sec1_y0), ImVec2(sec_x1, sec1_y1));

    // [Header] 仅标题「预设」，去除 subtitle
    float s1_head_y = sec1_y0 + 10.0f;
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(sec_x0 + 16.0f, s1_head_y), UIConfig::Color::TextActive, "预设");
    if (Fonts::Regular) ImGui::PopFont();



    // [Options] 4 张名机卡片 (2 行 × 2 列)
    const auto& presets = tm.getAllPresets();
    float c_inner_w = sec_w - 32.0f;
    float col_gap = 12.0f;
    float c_w = (c_inner_w - col_gap) * 0.5f;
    float c_h = 50.0f;
    float row_gap = 8.0f;

    for (size_t i = 0; i < presets.size() && i < 4; ++i) {
        const auto& p = presets[i];
        int row = static_cast<int>(i / 2);
        int col = static_cast<int>(i % 2);

        float cx0 = sec_x0 + 16.0f + col * (c_w + col_gap);
        float cy0 = sec1_y0 + 36.0f + row * (c_h + row_gap);
        float cx1 = cx0 + c_w;
        float cy1 = cy0 + c_h;

        ImVec2 c_min(cx0, cy0);
        ImVec2 c_max(cx1, cy1);

        bool is_current = (cur_theme == p.id);

        std::string btn_id = "##ThemeCard_" + std::to_string(static_cast<int>(p.id));
        ImGui::SetCursorScreenPos(c_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(c_w, c_h));

        bool is_hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            tm.setTheme(p.id);
        }

        const ImU32 pr = (p.accent_color >> IM_COL32_R_SHIFT) & 0xFF;
        const ImU32 pg = (p.accent_color >> IM_COL32_G_SHIFT) & 0xFF;
        const ImU32 pb = (p.accent_color >> IM_COL32_B_SHIFT) & 0xFF;

        ImU32 bg_col = is_current ? IM_COL32(pr, pg, pb, 35) :
                       (is_hovered ? IM_COL32(255, 255, 255, 18) : IM_COL32(255, 255, 255, 8));
        dl->AddRectFilled(c_min, c_max, bg_col, 8.0f);

        ImU32 border_col = is_current ? p.accent_color :
                           (is_hovered ? IM_COL32(pr, pg, pb, 160) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(c_min, c_max, border_col, 8.0f, 0, is_current ? 1.6f : 1.0f);

        // 主题色标点
        float dot_cx = cx0 + 16.0f;
        float dot_cy = cy0 + c_h * 0.5f;
        dl->AddCircleFilled(ImVec2(dot_cx, dot_cy), 6.5f, p.accent_color);
        if (is_current) {
            dl->AddCircle(ImVec2(dot_cx, dot_cy), 10.5f, p.accent_color, 24, 1.4f);
        }

        // 主题名称与风格
        float label_x = dot_cx + 16.0f;
        if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
        dl->AddText(ImVec2(label_x, cy0 + 8.0f), is_current ? UIConfig::Color::TextActive : IM_COL32(220, 230, 245, 230), p.name.c_str());
        if (Fonts::Regular) ImGui::PopFont();

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(label_x, cy0 + 28.0f), UIConfig::Color::TextMuted, p.sound_style.c_str());

        // 右侧色块与状态标签
        float chip_w = 13.0f;
        float chip_h = 18.0f;
        float chip_r = 3.0f;
        float chip_y = cy0 + (c_h - chip_h) * 0.5f;
        float right_pos = cx1 - 12.0f;

        if (is_current) {
            const char* act_tag = "已激活";
            float act_w = ImGui::CalcTextSize(act_tag).x + 10.0f;
            float act_x = right_pos - act_w;
            float act_y = cy0 + (c_h - 18.0f) * 0.5f;

            dl->AddRectFilled(ImVec2(act_x, act_y), ImVec2(act_x + act_w, act_y + 18.0f), IM_COL32(pr, pg, pb, 60), 4.0f);
            dl->AddRect(ImVec2(act_x, act_y), ImVec2(act_x + act_w, act_y + 18.0f), p.accent_color, 4.0f, 0, 1.0f);
            dl->AddText(ImVec2(act_x + 5.0f, act_y + 1.0f), UIConfig::Color::TextActive, act_tag);
            right_pos = act_x - 8.0f;
        } else if (is_hovered) {
            const char* hov_tag = "启用";
            float hov_w = ImGui::CalcTextSize(hov_tag).x + 10.0f;
            float hov_x = right_pos - hov_w;
            float hov_y = cy0 + (c_h - 18.0f) * 0.5f;

            dl->AddRectFilled(ImVec2(hov_x, hov_y), ImVec2(hov_x + hov_w, hov_y + 18.0f), IM_COL32(255, 255, 255, 22), 4.0f);
            dl->AddRect(ImVec2(hov_x, hov_y), ImVec2(hov_x + hov_w, hov_y + 18.0f), IM_COL32(255, 255, 255, 60), 4.0f, 0, 1.0f);
            dl->AddText(ImVec2(hov_x + 5.0f, hov_y + 1.0f), UIConfig::Color::TextActive, hov_tag);
            right_pos = hov_x - 8.0f;
        }

        // 3 颗调色代表色条
        float chip3_x = right_pos - chip_w;
        dl->AddRectFilled(ImVec2(chip3_x, chip_y), ImVec2(chip3_x + chip_w, chip_y + chip_h), p.peak_color, chip_r);
        dl->AddRect(ImVec2(chip3_x, chip_y), ImVec2(chip3_x + chip_w, chip_y + chip_h), IM_COL32(255, 255, 255, 40), chip_r, 0, 1.0f);

        float chip2_x = chip3_x - chip_w - 4.0f;
        dl->AddRectFilled(ImVec2(chip2_x, chip_y), ImVec2(chip2_x + chip_w, chip_y + chip_h), p.lit_color, chip_r);
        dl->AddRect(ImVec2(chip2_x, chip_y), ImVec2(chip2_x + chip_w, chip_y + chip_h), IM_COL32(255, 255, 255, 40), chip_r, 0, 1.0f);

        float chip1_x = chip2_x - chip_w - 4.0f;
        dl->AddRectFilled(ImVec2(chip1_x, chip_y), ImVec2(chip1_x + chip_w, chip_y + chip_h), p.accent_color, chip_r);
        dl->AddRect(ImVec2(chip1_x, chip_y), ImVec2(chip1_x + chip_w, chip_y + chip_h), IM_COL32(255, 255, 255, 40), chip_r, 0, 1.0f);

        if (Fonts::Small) ImGui::PopFont();
    }

    // ==============================================================================
    // 3. 板块二：背景律动 (标题改为「背景律动」，无 subtitle，带独立毛玻璃容器与 Options)
    // ==============================================================================
    float sec2_y0 = sec1_y1 + 8.0f;
    float sec2_h = 72.0f;
    float sec2_y1 = sec2_y0 + sec2_h;
    drawSectionFrostedCard(ImVec2(sec_x0, sec2_y0), ImVec2(sec_x1, sec2_y1));

    // [Header] 仅标题「背景律动」，去除 subtitle
    float s2_head_y = sec2_y0 + 10.0f;
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(sec_x0 + 16.0f, s2_head_y), UIConfig::Color::TextActive, "背景律动");
    if (Fonts::Regular) ImGui::PopFont();

    // [Options] 4 个动效风格按钮 (含金嗓子功放液晶双表头显示)
    auto cur_bg_mode = tm.getBackgroundVisualMode();
    const char* mode_labels[4] = {
        "48列全景 LED 频谱",
        "麦景图动圈大表头",
        "金嗓子功放液晶显示",
        "极简纯黑发烧机架"
    };

    float mode_btn_gap = 10.0f;
    float mode_btn_w = (c_inner_w - mode_btn_gap * 3.0f) / 4.0f;
    float mode_btn_h = 28.0f;
    float mode_btn_y = sec2_y0 + 34.0f;

    for (int m = 0; m < 4; ++m) {
        float mx0 = sec_x0 + 16.0f + m * (mode_btn_w + mode_btn_gap);
        ImVec2 m_min(mx0, mode_btn_y);
        ImVec2 m_max(mx0 + mode_btn_w, mode_btn_y + mode_btn_h);

        bool is_mode_act = (static_cast<int>(cur_bg_mode) == m);
        std::string m_id = "##BgModeBtn_" + std::to_string(m);
        ImGui::SetCursorScreenPos(m_min);
        ImGui::InvisibleButton(m_id.c_str(), ImVec2(mode_btn_w, mode_btn_h));

        bool m_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            tm.setBackgroundVisualMode(static_cast<BackgroundVisualMode>(m));
        }

        ImU32 m_bg = is_mode_act ? IM_COL32(r, g, b, 70) :
                     (m_hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(m_min, m_max, m_bg, 7.0f);

        ImU32 m_border = is_mode_act ? accent :
                         (m_hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(m_min, m_max, m_border, 7.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(mode_labels[m]);
        float txt_x = mx0 + (mode_btn_w - txt_sz.x) * 0.5f;
        float txt_y = mode_btn_y + (mode_btn_h - txt_sz.y) * 0.5f;
        dl->AddText(ImVec2(txt_x, txt_y), is_mode_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, mode_labels[m]);
        if (Fonts::Small) ImGui::PopFont();
    }

    // ==============================================================================
    // 4. 板块三：自由调色 (标题改为「自由调色」，无 subtitle，对齐图二 macOS 风格色轮与调色台)
    // ==============================================================================
    float sec3_y0 = sec2_y1 + 8.0f;
    float sec3_h = 186.0f;
    float sec3_y1 = sec3_y0 + sec3_h;
    drawSectionFrostedCard(ImVec2(sec_x0, sec3_y0), ImVec2(sec_x1, sec3_y1));

    // [Header] 仅标题「自由调色」，去除 subtitle
    float s3_head_y = sec3_y0 + 10.0f;
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(sec_x0 + 16.0f, s3_head_y), UIConfig::Color::TextActive, "自由调色");
    if (Fonts::Regular) ImGui::PopFont();

    // Header 右侧当前色彩 HEX 与 RGB 实时信息药丸
    char hex_buf[32];
    std::snprintf(hex_buf, sizeof(hex_buf), "#%02X%02X%02X  ·  RGB(%u, %u, %u)", r, g, b, r, g, b);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    float hex_w = ImGui::CalcTextSize(hex_buf).x + 20.0f;
    float hex_h = 22.0f;
    float hex_x = sec_x1 - 16.0f - hex_w;
    float hex_y = s3_head_y - 2.0f;

    dl->AddRectFilled(ImVec2(hex_x, hex_y), ImVec2(hex_x + hex_w, hex_y + hex_h), IM_COL32(r, g, b, 45), 11.0f);
    dl->AddRect(ImVec2(hex_x, hex_y), ImVec2(hex_x + hex_w, hex_y + hex_h), IM_COL32(r, g, b, 140), 11.0f, 0, 1.0f);
    dl->AddText(ImVec2(hex_x + 10.0f, hex_y + 3.5f), UIConfig::Color::TextActive, hex_buf);
    if (Fonts::Small) ImGui::PopFont();

    // --------------------------------------------------------------------------
    // [Options] 左侧：Apple/macOS 风格全彩渐变色轮 (Hue-Saturation Color Wheel)
    // --------------------------------------------------------------------------
    ImVec4 custom_v4 = tm.getCustomColor();
    float cur_r = custom_v4.x;
    float cur_g = custom_v4.y;
    float cur_b = custom_v4.z;

    float cur_h = 0.0f, cur_s = 0.0f, cur_v = 1.0f;
    ImGui::ColorConvertRGBtoHSV(cur_r, cur_g, cur_b, cur_h, cur_s, cur_v);

    float wheel_radius = 65.0f;
    ImVec2 wheel_center(sec_x0 + 16.0f + wheel_radius + 4.0f, sec3_y0 + 38.0f + wheel_radius);

    // 1. GPU Triangle Fan 绘制平滑连续真色彩轮 (圆心为灰白，圆周为饱和纯色)
    const int num_wheel_segments = 64;
    dl->PrimReserve(num_wheel_segments * 3, num_wheel_segments + 1);

    ImDrawIdx center_idx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    ImVec2 uv = ImGui::GetIO().Fonts->TexUvWhitePixel;
    // 中心顶点色：饱和度为 0 的明度对应基色
    float cr0, cg0, cb0;
    ImGui::ColorConvertHSVtoRGB(0.0f, 0.0f, cur_v, cr0, cg0, cb0);
    dl->PrimWriteVtx(wheel_center, uv, IM_COL32(static_cast<int>(cr0 * 255.0f), static_cast<int>(cg0 * 255.0f), static_cast<int>(cb0 * 255.0f), 255));

    constexpr float kPi = 3.14159265358979323846f;
    for (int s = 0; s < num_wheel_segments; ++s) {
        float a = (static_cast<float>(s) / static_cast<float>(num_wheel_segments)) * 2.0f * kPi;
        float h_edge = static_cast<float>(s) / static_cast<float>(num_wheel_segments);
        float er, eg, eb;
        ImGui::ColorConvertHSVtoRGB(h_edge, 1.0f, cur_v, er, eg, eb);
        ImVec2 p_edge(wheel_center.x + std::cos(a) * wheel_radius, wheel_center.y + std::sin(a) * wheel_radius);
        dl->PrimWriteVtx(p_edge, uv, IM_COL32(static_cast<int>(er * 255.0f), static_cast<int>(eg * 255.0f), static_cast<int>(eb * 255.0f), 255));
    }

    for (int s = 0; s < num_wheel_segments; ++s) {
        dl->PrimWriteIdx(center_idx);
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + s));
        dl->PrimWriteIdx(static_cast<ImDrawIdx>(center_idx + 1 + ((s + 1) % num_wheel_segments)));
    }

    // 色轮精工微光外圈
    dl->AddCircle(wheel_center, wheel_radius, IM_COL32(255, 255, 255, 60), num_wheel_segments, 1.2f);

    // 2. 色轮交互：点击/拖拽动态拾取 Hue 与 Saturation
    ImGui::SetCursorScreenPos(ImVec2(wheel_center.x - wheel_radius, wheel_center.y - wheel_radius));
    ImGui::InvisibleButton("##MacColorWheelArea", ImVec2(wheel_radius * 2.0f, wheel_radius * 2.0f));

    if (ImGui::IsItemActive()) {
        ImVec2 m = ImGui::GetIO().MousePos;
        float dx = m.x - wheel_center.x;
        float dy = m.y - wheel_center.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        float new_s = std::clamp(dist / wheel_radius, 0.0f, 1.0f);
        float angle = std::atan2(dy, dx);
        float new_h = std::fmod((angle / (2.0f * kPi) + 1.0f), 1.0f);

        float new_r, new_g, new_b;
        ImGui::ColorConvertHSVtoRGB(new_h, new_s, cur_v, new_r, new_g, new_b);
        tm.setCustomColor(ImVec4(new_r, new_g, new_b, 1.0f));
    }

    // 3. 绘制类似图二的取色准星 (Crosshair Reticle)
    float ret_angle = cur_h * 2.0f * kPi;
    float ret_dist = cur_s * wheel_radius;
    ImVec2 ret_pos(wheel_center.x + std::cos(ret_angle) * ret_dist,
                   wheel_center.y + std::sin(ret_angle) * ret_dist);

    dl->AddCircle(ret_pos, 7.0f, IM_COL32(0, 0, 0, 220), 16, 2.2f);
    dl->AddCircle(ret_pos, 6.0f, IM_COL32(255, 255, 255, 255), 16, 1.5f);
    dl->AddCircleFilled(ret_pos, 3.0f, IM_COL32(r, g, b, 255), 16);

    // --------------------------------------------------------------------------
    // [Options] 右侧调色中枢：明度滑条 + 经典色彩数值栏 + 经典发烧色格矩阵 (对齐图二)
    // --------------------------------------------------------------------------
    float right_panel_x = wheel_center.x + wheel_radius + 24.0f;
    float right_panel_w = sec_x1 - 16.0f - right_panel_x;

    // 1. 横向明度渐变滑条 (Value Slider - 类似图二色轮下方滑块)
    float slider_y = sec3_y0 + 38.0f;
    float slider_h = 16.0f;
    ImVec2 slider_min(right_panel_x, slider_y + 16.0f);
    ImVec2 slider_max(right_panel_x + right_panel_w, slider_y + 16.0f + slider_h);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(right_panel_x, slider_y), UIConfig::Color::TextMuted, "明度 / 亮度 (Brightness)");
    char v_percent_buf[16];
    std::snprintf(v_percent_buf, sizeof(v_percent_buf), "%d%%", static_cast<int>(std::round(cur_v * 100.0f)));
    float v_txt_w = ImGui::CalcTextSize(v_percent_buf).x;
    dl->AddText(ImVec2(slider_max.x - v_txt_w, slider_y), UIConfig::Color::TextActive, v_percent_buf);
    if (Fonts::Small) ImGui::PopFont();

    // 绘制明度渐变条 (从纯黑到当前 Hue/Sat 的最大饱和纯色，采用 Triangle Strip 完美全圆角胶囊光栅化)
    float max_r, max_g, max_b;
    ImGui::ColorConvertHSVtoRGB(cur_h, cur_s, 1.0f, max_r, max_g, max_b);

    float track_r = slider_h * 0.5f;
    float cy = slider_min.y + track_r;
    float cx_left = slider_min.x + track_r;
    float cx_right = slider_max.x - track_r;

    // 硬件级顶点 Triangle Strip 绘制 100% 顺滑全圆角胶囊横向渐变条 (彻底去除直角毛刺，呈现圆润质感)
    const int num_slices = 64;
    dl->PrimReserve(num_slices * 6, (num_slices + 1) * 2);

    ImDrawIdx base_vtx = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
    ImVec2 uv_white = ImGui::GetIO().Fonts->TexUvWhitePixel;

    for (int k = 0; k <= num_slices; ++k) {
        float t = static_cast<float>(k) / static_cast<float>(num_slices);
        float x = slider_min.x + t * right_panel_w;

        float dy = track_r;
        if (x < cx_left) {
            float dx = cx_left - x;
            dy = std::sqrt(std::max(0.0f, track_r * track_r - dx * dx));
        } else if (x > cx_right) {
            float dx = x - cx_right;
            dy = std::sqrt(std::max(0.0f, track_r * track_r - dx * dx));
        }

        ImVec2 v_top(x, cy - dy);
        ImVec2 v_bot(x, cy + dy);

        uint32_t c_r = static_cast<uint32_t>(std::clamp(max_r * t * 255.0f, 0.0f, 255.0f));
        uint32_t c_g = static_cast<uint32_t>(std::clamp(max_g * t * 255.0f, 0.0f, 255.0f));
        uint32_t c_b = static_cast<uint32_t>(std::clamp(max_b * t * 255.0f, 0.0f, 255.0f));
        ImU32 col_t = IM_COL32(c_r, c_g, c_b, 255);

        dl->PrimWriteVtx(v_top, uv_white, col_t);
        dl->PrimWriteVtx(v_bot, uv_white, col_t);
    }

    for (int k = 0; k < num_slices; ++k) {
        ImDrawIdx i0 = static_cast<ImDrawIdx>(base_vtx + k * 2);
        ImDrawIdx i1 = static_cast<ImDrawIdx>(base_vtx + k * 2 + 1);
        ImDrawIdx i2 = static_cast<ImDrawIdx>(base_vtx + (k + 1) * 2);
        ImDrawIdx i3 = static_cast<ImDrawIdx>(base_vtx + (k + 1) * 2 + 1);

        dl->PrimWriteIdx(i0);
        dl->PrimWriteIdx(i1);
        dl->PrimWriteIdx(i3);

        dl->PrimWriteIdx(i0);
        dl->PrimWriteIdx(i3);
        dl->PrimWriteIdx(i2);
    }

    // 绘制胶囊外圈 1px 高光边框
    dl->AddRect(slider_min, slider_max, IM_COL32(255, 255, 255, 50), track_r, 0, 1.0f);

    // 明度游标尺寸与极简全圆角胶囊设计 (对齐 macOS 与苹果风格)
    float thumb_w = 14.0f;
    float thumb_h = slider_h + 6.0f; // 稍高于轨道，呈现立体质感
    float thumb_r = thumb_w * 0.5f;   // 100% 全圆角胶囊
    float thumb_travel = right_panel_w - thumb_w;
    float thumb_x = slider_min.x + thumb_w * 0.5f + cur_v * thumb_travel;

    // 明度滑条交互
    ImGui::SetCursorScreenPos(ImVec2(slider_min.x, cy - thumb_h * 0.5f));
    ImGui::InvisibleButton("##MacBrightnessSlider", ImVec2(right_panel_w, thumb_h));
    if (ImGui::IsItemActive()) {
        float mx = ImGui::GetIO().MousePos.x;
        float new_v = std::clamp((mx - (slider_min.x + thumb_w * 0.5f)) / thumb_travel, 0.05f, 1.0f);
        float new_r, new_g, new_b;
        ImGui::ColorConvertHSVtoRGB(cur_h, cur_s, new_v, new_r, new_g, new_b);
        tm.setCustomColor(ImVec4(new_r, new_g, new_b, 1.0f));
    }

    // 绘制极简圆滑胶囊手柄 (全圆角 pill 造型，配备微投影与精工倒角)
    ImVec2 t0(thumb_x - thumb_r, cy - thumb_h * 0.5f);
    ImVec2 t1(thumb_x + thumb_r, cy + thumb_h * 0.5f);

    // 1. 柔和环境微投影
    dl->AddRectFilled(ImVec2(t0.x, t0.y + 1.5f), ImVec2(t1.x, t1.y + 2.5f), IM_COL32(0, 0, 0, 90), thumb_r);
    // 2. 润白实体胶囊手柄
    dl->AddRectFilled(t0, t1, IM_COL32(255, 255, 255, 255), thumb_r);
    // 3. 内部高光层
    dl->AddRect(ImVec2(t0.x + 0.5f, t0.y + 0.5f), ImVec2(t1.x - 0.5f, t1.y - 0.5f), IM_COL32(255, 255, 255, 200), thumb_r - 0.5f, 0, 1.0f);
    // 4. 金属微光轮廓线
    dl->AddRect(t0, t1, IM_COL32(0, 0, 0, 110), thumb_r, 0, 1.0f);

    // 2. 颜色预览大块与三行格式编码 (完全对齐图二的 ff2e8c / hsl / rgb)
    float info_y = slider_max.y + 12.0f;

    // 当前色实时预览大色块 (类似图二左下角大方块)
    float swatch_sz = 34.0f;
    ImVec2 sw_min(right_panel_x, info_y);
    ImVec2 sw_max(right_panel_x + swatch_sz, info_y + swatch_sz);
    dl->AddRectFilled(sw_min, sw_max, IM_COL32(r, g, b, 255), 6.0f);
    dl->AddRect(sw_min, sw_max, IM_COL32(255, 255, 255, 120), 6.0f, 0, 1.2f);

    // 旁边三列标签胶囊：HEX、RGB、HSL (类似图二的三个可复制字段)
    float box_x = right_panel_x + swatch_sz + 10.0f;
    float box_w = (right_panel_w - swatch_sz - 10.0f - 16.0f) / 3.0f;
    float box_h = 34.0f;

    auto drawCodeBox = [&](float bx, const char* label, const char* val) {
        ImVec2 b_min(bx, info_y);
        ImVec2 b_max(bx + box_w, info_y + box_h);
        dl->AddRectFilled(b_min, b_max, IM_COL32(255, 255, 255, 10), 6.0f);
        dl->AddRect(b_min, b_max, IM_COL32(255, 255, 255, 24), 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(bx + 8.0f, info_y + 3.0f), UIConfig::Color::TextMuted, label);
        dl->AddText(ImVec2(bx + 8.0f, info_y + 16.0f), UIConfig::Color::TextActive, val);
        if (Fonts::Small) ImGui::PopFont();
    };

    char hex_str[16];
    std::snprintf(hex_str, sizeof(hex_str), "#%02x%02x%02x", r, g, b);
    drawCodeBox(box_x, "HEX", hex_str);

    char rgb_str[24];
    std::snprintf(rgb_str, sizeof(rgb_str), "%u, %u, %u", r, g, b);
    drawCodeBox(box_x + box_w + 8.0f, "RGB", rgb_str);

    char hsl_str[24];
    std::snprintf(hsl_str, sizeof(hsl_str), "%d°, %d%%, %d%%",
                  static_cast<int>(std::round(cur_h * 360.0f)),
                  static_cast<int>(std::round(cur_s * 100.0f)),
                  static_cast<int>(std::round(cur_v * 100.0f)));
    drawCodeBox(box_x + (box_w + 8.0f) * 2.0f, "HSL", hsl_str);

    // 3. 经典发烧色格矩阵 (对齐图二底部的调色板小格矩阵)
    float grid_y = info_y + box_h + 10.0f;
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(right_panel_x, grid_y + 2.0f), UIConfig::Color::TextMuted, "经典名机色格：");
    if (Fonts::Small) ImGui::PopFont();

    static const struct {
        const char* name;
        ImVec4 col;
    } mac_swatches[] = {
        {"现代深空", ImVec4(0.98f, 0.18f, 0.28f, 1.0f)}, // Apple Music 玫红
        {"麦景图蓝", ImVec4(0.00f, 0.71f, 0.94f, 1.0f)}, // 麦景图冰蓝
        {"金嗓子金", ImVec4(0.92f, 0.70f, 0.03f, 1.0f)}, // 金嗓子香槟金
        {"复古琥珀", ImVec4(0.98f, 0.45f, 0.09f, 1.0f)}, // 模拟开盘机
        {"索尼黑金", ImVec4(0.90f, 0.73f, 0.35f, 1.0f)}, // 索尼金砖
        {"英国之宝", ImVec4(0.01f, 0.52f, 0.78f, 1.0f)}, // Meridian
        {"翡翠纯翠", ImVec4(0.06f, 0.73f, 0.51f, 1.0f)}, // 极光翠
        {"赛博极光", ImVec4(0.55f, 0.36f, 0.96f, 1.0f)}, // Cyber Violet
        {"经典朱砂", ImVec4(0.86f, 0.15f, 0.15f, 1.0f)}, // Ruby
        {"银月纯白", ImVec4(0.89f, 0.91f, 0.94f, 1.0f)}  // Silver White
    };

    float sw_start_x = right_panel_x + 95.0f;
    float sw_gap = 7.0f;
    float sw_w = 32.0f;
    float sw_h = 20.0f;

    for (size_t s = 0; s < 10; ++s) {
        float sx0 = sw_start_x + s * (sw_w + sw_gap);
        if (sx0 + sw_w > sec_x1 - 16.0f) break; // 安全边界防溢出

        ImVec2 s_min(sx0, grid_y);
        ImVec2 s_max(sx0 + sw_w, grid_y + sw_h);

        std::string s_id = "##MacSwatch_" + std::to_string(s);
        ImGui::SetCursorScreenPos(s_min);
        ImGui::InvisibleButton(s_id.c_str(), ImVec2(sw_w, sw_h));

        bool s_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            tm.setCustomColor(mac_swatches[s].col);
        }

        uint32_t sr = static_cast<uint32_t>(mac_swatches[s].col.x * 255.0f);
        uint32_t sg = static_cast<uint32_t>(mac_swatches[s].col.y * 255.0f);
        uint32_t sb = static_cast<uint32_t>(mac_swatches[s].col.z * 255.0f);
        ImU32 s_col = IM_COL32(sr, sg, sb, 255);

        dl->AddRectFilled(s_min, s_max, s_col, 4.0f);
        dl->AddRect(s_min, s_max, s_hov ? IM_COL32(255, 255, 255, 220) : IM_COL32(0, 0, 0, 60), 4.0f, 0, s_hov ? 1.5f : 1.0f);

        if (s_hov) {
            dl->AddRect(ImVec2(s_min.x - 1.5f, s_min.y - 1.5f), ImVec2(s_max.x + 1.5f, s_max.y + 1.5f), IM_COL32(255, 255, 255, 120), 5.5f, 0, 1.0f);
        }
    }
}

void MainStageView::renderSystemSettingsView(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, "系统状态与树莓派硬件中枢", "ARMv8.2-A Cortex-A76 · KMS/DRM 无桌面直启 · 0dB 静音运行");
}
