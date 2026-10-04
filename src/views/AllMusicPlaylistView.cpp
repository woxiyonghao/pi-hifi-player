#include "views/AllMusicPlaylistView.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "widgets/DrawUtils.hpp"
#include "tools/MusicScanManager.hpp"
#include "tools/PlayerAdmin.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>

void AllMusicPlaylistView::drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle) {
    GlassCardRenderer::drawCard(dl, p_min, p_max, UIConfig::Layout::ContainerRounding, "main_stage");

    if (title) {
        ImVec2 title_pos = ImVec2(p_min.x + 20.0f, p_min.y + 16.0f);
        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        dl->AddText(title_pos, UIConfig::Color::TextActive, title);
        if (Fonts::Medium) ImGui::PopFont();
    }

    if (subtitle) {
        ImVec2 sub_pos = ImVec2(p_min.x + 20.0f, p_min.y + 44.0f);
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(sub_pos, UIConfig::Color::TextMuted, subtitle);
        if (Fonts::Small) ImGui::PopFont();
    }
}

void AllMusicPlaylistView::rebuildCache(const std::vector<Track>& tracks) {
    last_tracks_count_ = tracks.size();
    last_first_track_id_ = tracks.empty() ? 0 : tracks.front().id;

    // 1. 按格式分类
    cached_format_groups_ = {
        { "dsf", "DSD", "DSD 原生母带 (DSF / DFF)", {} },
        { "flac", "FLAC", "FLAC 高解析无损母带", {} },
        { "wav", "WAV", "WAV 广播级线性母带", {} },
        { "alac", "ALAC", "Apple Lossless (ALAC)", {} },
        { "mp3", "MP3", "MP3 经典流行音频", {} },
        { "other", "RAW", "其它发烧音轨", {} }
    };
    for (const auto& t : tracks) {
        if (t.format == AudioFormat::DSD_DSF || t.format == AudioFormat::DSD_DFF) {
            cached_format_groups_[0].group_tracks.push_back(t);
        } else if (t.format == AudioFormat::FLAC) {
            cached_format_groups_[1].group_tracks.push_back(t);
        } else if (t.format == AudioFormat::WAV) {
            cached_format_groups_[2].group_tracks.push_back(t);
        } else if (t.format == AudioFormat::ALAC) {
            cached_format_groups_[3].group_tracks.push_back(t);
        } else if (t.format == AudioFormat::MP3) {
            cached_format_groups_[4].group_tracks.push_back(t);
        } else {
            cached_format_groups_[5].group_tracks.push_back(t);
        }
    }

    // 2. 按艺术家/专辑分类
    cached_artist_groups_.clear();
    std::map<std::string, std::map<std::string, std::vector<Track>>> art_alb_map;
    for (const auto& t : tracks) {
        std::string art = t.artist.empty() ? "未知艺术家" : t.artist;
        std::string alb = t.album.empty() ? "本地精选单曲" : t.album;
        art_alb_map[art][alb].push_back(t);
    }
    for (auto& [art_name, albums] : art_alb_map) {
        ArtistGroup ag;
        ag.artist_name = art_name;
        for (auto& [alb_name, alb_tracks] : albums) {
            ag.all_tracks.insert(ag.all_tracks.end(), alb_tracks.begin(), alb_tracks.end());
            ag.albums.push_back({alb_name, std::move(alb_tracks)});
        }
        cached_artist_groups_.push_back(std::move(ag));
    }

    // 3. 按存储目录分类
    cached_dir_groups_.clear();
    std::map<std::string, std::vector<Track>> dir_map;
    for (const auto& t : tracks) {
        std::filesystem::path fp(t.file_path);
        std::string dir_name = fp.has_parent_path() ? fp.parent_path().filename().string() : "根目录";
        if (dir_name.empty()) dir_name = "本地曲库";
        dir_map[dir_name].push_back(t);
    }
    for (auto& [dir_name, dir_tracks] : dir_map) {
        cached_dir_groups_.push_back({dir_name, std::move(dir_tracks)});
    }
}

void AllMusicPlaylistView::render(float x, float y, float w, float h, const std::vector<Playlist>& playlists) {
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

    uint64_t cur_first_id = tracks.empty() ? 0 : tracks.front().id;
    if (tracks.size() != last_tracks_count_ || cur_first_id != last_first_track_id_) {
        rebuildCache(tracks);
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

    // 1. 顶层工具栏：模式切换
    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
    if (ImGui::BeginChild("##AllMusicToolbar", ImVec2(content_w, 32.0f), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {

        // 模式切换胶囊按钮：样式与色值与 Sidebar / BottomBar 纯正液态玻璃主题色完全对齐
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

    // 2. 树状列表主滚动视口
    float tree_y = content_y + 36.0f;
    float tree_h = content_h - 36.0f;
    ImGui::SetCursorScreenPos(ImVec2(content_x, tree_y));

    // 开启精美半透明纤细滚动条样式，支持鼠标滚轮与触控屏手势拖拽
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, accent);

    if (ImGui::BeginChild("##AllMusicTreeScroll", ImVec2(content_w, tree_h), false,
                          ImGuiWindowFlags_NoBackground)) {

        // 触控与鼠标拖拽平滑滚动：上下滑动时自然滚动曲目列表
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = std::clamp(ImGui::GetIO().MouseDelta.y, -40.0f, 40.0f);
            if (drag_dy != 0.0f) {
                ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
            }
        }

        float dt = ImGui::GetIO().DeltaTime;

        // 视口边界计算：用于视口裁剪 (Viewport Culling)，屏幕外的行跳过全部几何与文本提交
        float win_y = ImGui::GetWindowPos().y;
        float win_h = ImGui::GetWindowSize().y;

        // 通用曲目行渲染器
        auto renderTrackRow = [&](const Track& track, size_t row_idx, const std::vector<Track>& queue_context,
                                  size_t index_in_queue, float indent) {
            float avail_w = ImGui::GetContentRegionAvail().x;
            float row_h = 30.0f; // 紧凑优雅行高
            ImVec2 row_pos = ImGui::GetCursorScreenPos();
            ImVec2 row_min(row_pos.x + indent, row_pos.y);
            ImVec2 row_max(row_pos.x + avail_w, row_pos.y + row_h);

            // 视口裁剪：在可见窗口上下各预留 40px 缓冲区，超出视口直接 Dummy 占位跳过渲染
            if (row_pos.y + row_h < win_y - 40.0f || row_pos.y > win_y + win_h + 40.0f) {
                ImGui::Dummy(ImVec2(avail_w, row_h));
                return;
            }

            bool is_current = current_track.has_value() && (current_track->id == track.id || current_track->file_path == track.file_path);
            bool is_playing = is_current && player.isPlaying();

            ImGui::PushID(static_cast<int>(track.id * 1000 + row_idx));

            bool clicked = ImGui::InvisibleButton("##TrackBtn", ImVec2(avail_w, row_h));
            bool hovered = ImGui::IsItemHovered();

            ImDrawList* cur_dl = ImGui::GetWindowDrawList();

            // 背景悬停与在播液态玻璃主题高亮底纹
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
                char num_buf[32];
                std::snprintf(num_buf, sizeof(num_buf), "%02zu.", row_idx + 1);
                if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                cur_dl->AddText(ImVec2(left_x, text_y + 1.0f), IM_COL32(140, 155, 175, 200), num_buf);
                if (Fonts::Small) ImGui::PopFont();
                left_x += 24.0f;
            }

            // 曲目标题
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

            // 统一由整行按键响应点击，防抖且防拖拽误触
            if (clicked && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
                if (!is_playing) {
                    player.playTracks(queue_context, index_in_queue);
                }
            }

            ImGui::PopID();
        };

        // 通用树节点 Header 渲染器 (圆润矢量倒角 + 180度平滑旋转展开动效)
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

            if (clicked && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
                is_open = !is_open;
                tree_expanded_[key] = is_open;
            }

            ImDrawList* cur_dl = ImGui::GetWindowDrawList();

            ImU32 bg_col = is_open ? IM_COL32(30, 36, 52, 190) : (hovered ? IM_COL32(36, 44, 62, 170) : IM_COL32(22, 28, 40, 150));
            cur_dl->AddRectFilled(node_min, node_max, bg_col, 7.0f);
            cur_dl->AddRect(node_min, node_max, is_open ? UIConfig::Color::GlassBorder : IM_COL32(255, 255, 255, 25), 7.0f, 0, 1.0f);

            // 绘制纯矢量圆角三角形，并进行 180° 旋转平滑动画
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

            // 格式徽标
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

            // 标题
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

        // 直接读取已缓存的分组数据进行高效率渲染，零重复内存分配
        if (all_music_view_mode_ == 0) {
            // Mode 0: 按音频格式分类
            for (const auto& g : cached_format_groups_) {
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
            for (const auto& ag : cached_artist_groups_) {
                std::string art_key = "art_" + ag.artist_name;
                bool art_open = renderTreeNodeHeader(art_key, ag.artist_name.c_str(), "歌手", ag.all_tracks.size(), ag.all_tracks, 0.0f);
                if (art_open) {
                    for (const auto& alb : ag.albums) {
                        std::string alb_key = "alb_" + ag.artist_name + "_" + alb.album_name;
                        bool alb_open = renderTreeNodeHeader(alb_key, alb.album_name.c_str(), "专辑", alb.tracks.size(), alb.tracks, 16.0f);
                        if (alb_open) {
                            for (size_t i = 0; i < alb.tracks.size(); ++i) {
                                renderTrackRow(alb.tracks[i], i, alb.tracks, i, 32.0f);
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
            for (const auto& dg : cached_dir_groups_) {
                std::string dir_key = "dir_" + dg.dir_name;
                bool dir_open = renderTreeNodeHeader(dir_key, dg.dir_name.c_str(), "目录", dg.tracks.size(), dg.tracks, 0.0f);
                if (dir_open) {
                    for (size_t i = 0; i < dg.tracks.size(); ++i) {
                        renderTrackRow(dg.tracks[i], i, dg.tracks, i, 20.0f);
                        ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    }
                }
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}
