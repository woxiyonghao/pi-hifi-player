#include "views/MainStageView.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
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
    float content_y = card_min.y + 75.0f;
    float content_w = card_max.x - card_min.x - 40.0f;
    float content_h = card_max.y - card_min.y - 90.0f;

    if (total_count == 0) {
        ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
        if (ImGui::BeginChild("##AllMusicEmptyChild", ImVec2(content_w, content_h), false, ImGuiWindowFlags_NoBackground)) {
            ImGui::Dummy(ImVec2(0.0f, 40.0f));
            if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
            ImGui::SetCursorPosX((content_w - 380.0f) * 0.5f);
            ImGui::TextColored(ImVec4(0.9f, 0.92f, 0.96f, 1.0f), "本地发烧曲库暂无音乐");
            if (Fonts::Medium) ImGui::PopFont();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::SetCursorPosX((content_w - 380.0f) * 0.5f);
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "请前往「扫描音乐」检索本地音乐文件，或将歌曲导入播放列表。");

            ImGui::Dummy(ImVec2(0.0f, 20.0f));
            ImGui::SetCursorPosX((content_w - 140.0f) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button, UIConfig::Color::Accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 65, 95, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(230, 30, 60, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            if (ImGui::Button("前往扫描音乐", ImVec2(140.0f, 36.0f))) {
                if (on_navigate_tab_) {
                    on_navigate_tab_(SidebarTab::ScanMusic);
                }
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::EndChild();
        return;
    }

    auto& player = PlayerAdmin::getInstance();
    const auto& current_track = player.getCurrentTrack();

    // 1. 顶层工具栏：模式切换与快捷操作
    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
    if (ImGui::BeginChild("##AllMusicToolbar", ImVec2(content_w, 34.0f), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {
        auto renderModeBtn = [this](const char* label, int mode) {
            bool is_act = (all_music_view_mode_ == mode);
            ImGui::PushStyleColor(ImGuiCol_Button, is_act ? UIConfig::Color::Accent : IM_COL32(35, 42, 60, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, is_act ? UIConfig::Color::Accent : IM_COL32(48, 58, 80, 220));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIConfig::Color::Accent);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            if (ImGui::Button(label, ImVec2(104.0f, 28.0f))) {
                all_music_view_mode_ = mode;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        };

        renderModeBtn("按音频格式", 0);
        ImGui::SameLine(0.0f, 8.0f);
        renderModeBtn("按艺术家/专辑", 1);
        ImGui::SameLine(0.0f, 8.0f);
        renderModeBtn("按存储目录", 2);

        // 右侧快捷操作按钮
        float right_w = 260.0f;
        if (content_w > 600.0f) {
            ImGui::SameLine(content_w - right_w, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(35, 42, 60, 160));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(48, 58, 80, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(58, 70, 95, 230));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            if (ImGui::Button("展开全部", ImVec2(76.0f, 28.0f))) {
                for (auto& [k, v] : tree_expanded_) v = true;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("折叠全部", ImVec2(76.0f, 28.0f))) {
                for (auto& [k, v] : tree_expanded_) v = false;
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("随机播放", ImVec2(86.0f, 28.0f))) {
                std::vector<Track> shuffled = tracks;
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(shuffled.begin(), shuffled.end(), g);
                PlayerAdmin::getInstance().playTracks(shuffled, 0);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::EndChild();

    // 2. 树状列表主滚动视口
    float tree_y = content_y + 40.0f;
    float tree_h = content_h - 40.0f;
    ImGui::SetCursorScreenPos(ImVec2(content_x, tree_y));
    if (ImGui::BeginChild("##AllMusicTreeScroll", ImVec2(content_w, tree_h), false, ImGuiWindowFlags_NoBackground)) {

        // 通用曲目行渲染器
        auto renderTrackRow = [&](const Track& track, size_t row_idx, const std::vector<Track>& queue_context,
                                  size_t index_in_queue, float indent) {
            bool is_current = current_track.has_value() && (current_track->id == track.id || current_track->file_path == track.file_path);
            bool is_playing = is_current && player.isPlaying();

            ImGui::PushID(static_cast<int>(track.id * 1000 + row_idx));

            float avail_w = ImGui::GetContentRegionAvail().x;
            float row_h = 34.0f;
            ImVec2 row_pos = ImGui::GetCursorScreenPos();
            ImVec2 row_min(row_pos.x + indent, row_pos.y);
            ImVec2 row_max(row_pos.x + avail_w, row_pos.y + row_h);

            bool clicked = ImGui::InvisibleButton("##TrackBtn", ImVec2(avail_w, row_h));
            bool hovered = ImGui::IsItemHovered();

            ImDrawList* cur_dl = ImGui::GetWindowDrawList();

            // 背景悬停与激活发烧玻璃底纹
            if (is_current) {
                cur_dl->AddRectFilled(row_min, row_max, IM_COL32(250, 45, 72, 38), 6.0f);
                cur_dl->AddRect(row_min, row_max, IM_COL32(250, 45, 72, 90), 6.0f, 0, 1.0f);
            } else if (hovered) {
                cur_dl->AddRectFilled(row_min, row_max, UIConfig::Color::GlassHover, 6.0f);
                cur_dl->AddRect(row_min, row_max, UIConfig::Color::GlassBorder, 6.0f, 0, 1.0f);
            }

            float left_x = row_min.x + 8.0f;
            float text_y = row_pos.y + 8.0f;

            // 状态角标与律动频谱动画
            if (is_playing) {
                float t = static_cast<float>(ImGui::GetTime());
                float b1 = 4.0f + 6.0f * std::abs(std::sin(t * 6.0f));
                float b2 = 3.0f + 8.0f * std::abs(std::sin(t * 7.5f + 1.2f));
                float b3 = 5.0f + 7.0f * std::abs(std::sin(t * 5.2f + 2.5f));
                float cy = row_pos.y + row_h * 0.5f;

                cur_dl->AddLine(ImVec2(left_x, cy + b1 * 0.5f), ImVec2(left_x, cy - b1 * 0.5f), UIConfig::Color::Accent, 2.0f);
                cur_dl->AddLine(ImVec2(left_x + 5.0f, cy + b2 * 0.5f), ImVec2(left_x + 5.0f, cy - b2 * 0.5f), UIConfig::Color::Accent, 2.0f);
                cur_dl->AddLine(ImVec2(left_x + 10.0f, cy + b3 * 0.5f), ImVec2(left_x + 10.0f, cy - b3 * 0.5f), UIConfig::Color::Accent, 2.0f);
                left_x += 20.0f;
            } else if (is_current) {
                cur_dl->AddText(ImVec2(left_x, text_y), UIConfig::Color::Accent, "▶");
                left_x += 18.0f;
            } else {
                char num_buf[16];
                std::snprintf(num_buf, sizeof(num_buf), "%02zu.", row_idx + 1);
                cur_dl->AddText(ImVec2(left_x, text_y), IM_COL32(150, 165, 185, 200), num_buf);
                left_x += 28.0f;
            }

            // 曲目标题
            ImU32 title_color = is_current ? UIConfig::Color::Accent : (hovered ? UIConfig::Color::TextActive : IM_COL32(230, 235, 245, 255));
            cur_dl->AddText(ImVec2(left_x, text_y), title_color, track.title.c_str());

            float title_w = ImGui::CalcTextSize(track.title.c_str()).x;
            left_x += title_w + 10.0f;

            // 艺术家
            if (!track.artist.empty()) {
                std::string art = "- " + track.artist;
                cur_dl->AddText(ImVec2(left_x, text_y), UIConfig::Color::TextMuted, art.c_str());
            }

            // 右侧区域：播放按钮 + 时长 + 格式徽标
            float right_x = row_max.x - 10.0f;

            float btn_w = 46.0f;
            float btn_h = 22.0f;
            float btn_x = right_x - btn_w;
            float btn_y = row_pos.y + 6.0f;
            bool btn_hov = ImGui::IsMouseHoveringRect(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h));
            bool btn_click = btn_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            cur_dl->AddRectFilled(ImVec2(btn_x, btn_y), ImVec2(btn_x + btn_w, btn_y + btn_h),
                                  btn_hov ? UIConfig::Color::Accent : IM_COL32(45, 55, 75, 180), 4.0f);
            cur_dl->AddText(ImVec2(btn_x + 9.0f, btn_y + 3.0f), IM_COL32(255, 255, 255, 255), "播放");

            right_x = btn_x - 14.0f;

            // 时长
            if (track.duration_sec > 0) {
                char dur_buf[32];
                std::snprintf(dur_buf, sizeof(dur_buf), "%02u:%02u", track.duration_sec / 60, track.duration_sec % 60);
                float dur_w = ImGui::CalcTextSize(dur_buf).x;
                right_x -= dur_w;
                cur_dl->AddText(ImVec2(right_x, text_y), IM_COL32(160, 175, 195, 220), dur_buf);
                right_x -= 14.0f;
            }

            // 音频规格徽标
            std::string badge = track.getFormatBadge();
            float badge_txt_w = ImGui::CalcTextSize(badge.c_str()).x;
            float badge_w = badge_txt_w + 12.0f;
            float badge_h = 19.0f;
            float badge_x = right_x - badge_w;
            float badge_y = row_pos.y + 7.0f;

            cur_dl->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(20, 26, 38, 220), 4.0f);
            cur_dl->AddRect(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + badge_h), IM_COL32(60, 80, 110, 180), 4.0f, 0, 1.0f);
            cur_dl->AddText(ImVec2(badge_x + 6.0f, badge_y + 2.0f), IM_COL32(65, 190, 255, 240), badge.c_str());

            if (clicked || btn_click) {
                player.playTracks(queue_context, index_in_queue);
            }

            ImGui::PopID();
        };

        // 通用树节点 Header 渲染器
        auto renderTreeNodeHeader = [&](const std::string& key, const char* node_title, const char* badge,
                                        size_t count, const std::vector<Track>& group_tracks, float indent) -> bool {
            bool is_open = tree_expanded_.find(key) == tree_expanded_.end() ? true : tree_expanded_[key];

            ImGui::PushID(key.c_str());

            float avail_w = ImGui::GetContentRegionAvail().x;
            float node_h = 38.0f;
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

            ImU32 bg_col = is_open ? IM_COL32(32, 40, 58, 200) : (hovered ? IM_COL32(38, 48, 68, 180) : IM_COL32(24, 30, 44, 160));
            cur_dl->AddRectFilled(node_min, node_max, bg_col, 8.0f);
            cur_dl->AddRect(node_min, node_max, is_open ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 25), 8.0f, 0, 1.0f);

            float left_x = node_min.x + 12.0f;
            float text_y = pos.y + 10.0f;

            // 折叠指示箭头
            const char* arrow = is_open ? "▼" : "▶";
            cur_dl->AddText(ImVec2(left_x, text_y), UIConfig::Color::Accent, arrow);
            left_x += 18.0f;

            // 徽标
            if (badge && badge[0] != '\0') {
                float b_w = ImGui::CalcTextSize(badge).x + 10.0f;
                cur_dl->AddRectFilled(ImVec2(left_x, pos.y + 9.0f), ImVec2(left_x + b_w, pos.y + 28.0f), IM_COL32(250, 45, 72, 45), 4.0f);
                cur_dl->AddRect(ImVec2(left_x, pos.y + 9.0f), ImVec2(left_x + b_w, pos.y + 28.0f), UIConfig::Color::Accent, 4.0f, 0, 1.0f);
                cur_dl->AddText(ImVec2(left_x + 5.0f, pos.y + 11.0f), UIConfig::Color::Accent, badge);
                left_x += b_w + 10.0f;
            }

            // 标题
            if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
            cur_dl->AddText(ImVec2(left_x, text_y - 2.0f), UIConfig::Color::TextActive, node_title);
            if (Fonts::Medium) ImGui::PopFont();

            // 右侧曲目统计与播放全部
            float right_x = node_max.x - 12.0f;

            float play_btn_w = 64.0f;
            float play_btn_h = 24.0f;
            float play_btn_x = right_x - play_btn_w;
            float play_btn_y = pos.y + 7.0f;
            bool play_hov = ImGui::IsMouseHoveringRect(ImVec2(play_btn_x, play_btn_y), ImVec2(play_btn_x + play_btn_w, play_btn_y + play_btn_h));
            bool play_click = play_hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            cur_dl->AddRectFilled(ImVec2(play_btn_x, play_btn_y), ImVec2(play_btn_x + play_btn_w, play_btn_y + play_btn_h),
                                  play_hov ? UIConfig::Color::Accent : IM_COL32(50, 60, 85, 200), 4.0f);
            cur_dl->AddText(ImVec2(play_btn_x + 8.0f, play_btn_y + 4.0f), IM_COL32(255, 255, 255, 255), "播放全部");

            if (play_click && !group_tracks.empty()) {
                player.playTracks(group_tracks, 0);
            }

            right_x = play_btn_x - 14.0f;

            std::string count_str = std::to_string(count) + " 首曲目";
            float count_w = ImGui::CalcTextSize(count_str.c_str()).x;
            right_x -= count_w;
            cur_dl->AddText(ImVec2(right_x, text_y), UIConfig::Color::TextMuted, count_str.c_str());

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
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
                        renderTrackRow(g.group_tracks[i], i, g.group_tracks, i, 22.0f);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
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
                        bool alb_open = renderTreeNodeHeader(alb_key, album_name.c_str(), "专辑", alb_tracks.size(), alb_tracks, 18.0f);
                        if (alb_open) {
                            for (size_t i = 0; i < alb_tracks.size(); ++i) {
                                renderTrackRow(alb_tracks[i], i, alb_tracks, i, 36.0f);
                                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                            }
                        }
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
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
                        renderTrackRow(dir_tracks[i], i, dir_tracks, i, 22.0f);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
            }
        }
    }
    ImGui::EndChild();
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
    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoBackground;
    if (ImGui::BeginChild("##TrackListContentChild", ImVec2(content_w, content_h), false, child_flags)) {
        const auto& tracks = target_playlist->getTracks();
        if (tracks.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "当前歌单暂无曲目，请前往「扫描音乐」检索并导入本地发烧曲目。");
        } else {
            for (size_t i = 0; i < tracks.size(); ++i) {
                const auto& track = tracks[i];
                bool is_current = current_track.has_value() && current_track->id == track.id;

                ImGui::PushID(static_cast<int>(i));
                
                // 选定高亮色
                if (is_current) {
                    ImGui::TextColored(ImVec4(0.98f, 0.18f, 0.28f, 1.0f), "▶ %02zu. %s", i + 1, track.title.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.9f, 0.93f, 0.98f, 1.0f), "  %02zu. %s", i + 1, track.title.c_str());
                }

                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 0.8f), "- %s", track.artist.c_str());

                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 0.9f), "[%s]", track.getFormatBadge().c_str());

                // 双击或点击整行启动播放
                ImGui::SameLine();
                if (ImGui::SmallButton("播放")) {
                    player.playPlaylist(*target_playlist, i);
                }

                ImGui::PopID();
                ImGui::Spacing();
            }
        }
    }
    ImGui::EndChild();
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
    drawLiquidCard(dl, card_min, card_max, "视觉主题与表头风格 (Themes)", "麦景图湖蓝深空 · 金嗓子香槟暖金 · 复古琥珀卡座");
}

void MainStageView::renderSystemSettingsView(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, "系统状态与树莓派硬件中枢", "ARMv8.2-A Cortex-A76 · KMS/DRM 无桌面直启 · 0dB 静音运行");
}
