#include "views/CustomPlaylistView.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"
#include "tools/PlayerAdmin.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

void CustomPlaylistView::drawLiquidCard(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, const char* title, const char* subtitle) {
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

void CustomPlaylistView::render(uint64_t pid, std::vector<Playlist>& playlists, float x, float y, float w, float h) {
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
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, accent);

    if (ImGui::BeginChild("##TrackListContentChild", ImVec2(content_w, content_h), false, ImGuiWindowFlags_NoBackground)) {

        // 触控与鼠标拖拽平滑滚动：上下滑动时自然滚动歌单曲目列表
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = std::clamp(ImGui::GetIO().MouseDelta.y, -40.0f, 40.0f);
            if (drag_dy != 0.0f) {
                ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
            }
        }
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

                if ((clicked || play_click) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
                    player.playPlaylist(*target_playlist, i);
                }

                ImGui::PopID();
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    // 渲染添加歌曲模态弹窗
    if (show_add_music_modal_ && target_playlist) {
        renderAddMusicToPlaylistModal(target_playlist, playlists);
    }

    // 渲染删除播放列表二次确认模态弹窗
    if (show_delete_playlist_modal_ && target_playlist) {
        renderDeletePlaylistModal(target_playlist, playlists);
    }
}

void CustomPlaylistView::renderAddMusicToPlaylistModal(Playlist* target_playlist, std::vector<Playlist>& playlists) {
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

        // 右上角纯矢量 "✕" 关闭按钮
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

                    float row_w = ImGui::GetContentRegionAvail().x;
                    float row_h = 32.0f;
                    ImVec2 row_pos = ImGui::GetCursorScreenPos();
                    ImVec2 row_min(row_pos.x, row_pos.y);
                    ImVec2 row_max(row_pos.x + row_w, row_pos.y + row_h);

                    bool row_clicked = ImGui::InvisibleButton("##AddRowBtn", ImVec2(row_w, row_h));
                    bool row_hovered = ImGui::IsItemHovered();

                    if (row_clicked && !already_in) {
                        if (is_selected) {
                            selected_track_ids_to_add_.erase(sel_it);
                        } else {
                            selected_track_ids_to_add_.push_back(trk.id);
                        }
                    }

                    ImDrawList* cur_dl = ImGui::GetWindowDrawList();

                    // 背景高亮
                    if (is_selected) {
                        cur_dl->AddRectFilled(row_min, row_max, IM_COL32(r, g, b, 45), 6.0f);
                        cur_dl->AddRect(row_min, row_max, UIConfig::Color::GlassBorder, 6.0f, 0, 1.0f);
                    } else if (row_hovered && !already_in) {
                        cur_dl->AddRectFilled(row_min, row_max, IM_COL32(255, 255, 255, 14), 6.0f);
                        cur_dl->AddRect(row_min, row_max, IM_COL32(255, 255, 255, 20), 6.0f, 0, 1.0f);
                    }

                    // 左侧复选框勾选框
                    float box_size = 18.0f;
                    float box_x = row_pos.x + 8.0f;
                    float box_y = row_pos.y + (row_h - box_size) * 0.5f;

                    if (already_in) {
                        // 灰色已存在勾选框
                        cur_dl->AddRectFilled(ImVec2(box_x, box_y), ImVec2(box_x + box_size, box_y + box_size), IM_COL32(60, 70, 85, 160), 3.5f);
                        const ImVec2 pts[3] = {
                            ImVec2(box_x + 4.0f, box_y + 8.5f),
                            ImVec2(box_x + 7.0f, box_y + 12.2f),
                            ImVec2(box_x + 13.0f, box_y + 5.0f)
                        };
                        cur_dl->AddPolyline(pts, 3, IM_COL32(140, 155, 175, 200), ImDrawFlags_None, 1.8f);
                    } else if (is_selected) {
                        // 主题色激活勾选框
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

                        // 2. 规格徽标
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
                        right_x = badge_x - 12.0f;
                    }

                    // 左侧标题与艺术家 (紧跟勾选框，宽度自适应收敛)
                    float txt_left = box_x + box_size + 12.0f;
                    float txt_max_w = right_x - txt_left;

                    if (txt_max_w > 80.0f) {
                        float cur_x = txt_left;
                        float title_text_y = row_pos.y + 6.0f;

                        // 曲名
                        ImU32 title_color = already_in ? IM_COL32(140, 155, 175, 180) :
                                            (is_selected ? UIConfig::Color::TextActive : IM_COL32(225, 235, 245, 240));
                        cur_dl->AddText(ImVec2(cur_x, title_text_y), title_color, trk.title.c_str());
                        float title_w = ImGui::CalcTextSize(trk.title.c_str()).x;
                        cur_x += title_w + 10.0f;

                        // 艺术家
                        if (!trk.artist.empty() && cur_x < right_x - 40.0f) {
                            std::string art_txt = "- " + trk.artist;
                            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
                            cur_dl->AddText(ImVec2(cur_x, title_text_y + 1.0f), IM_COL32(120, 135, 160, 190), art_txt.c_str());
                            if (Fonts::Small) ImGui::PopFont();
                        }
                    }

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
        ImU32 conf_hov = has_selection ? IM_COL32(std::min(255u, r + 25), std::min(255u, g + 25), std::min(255u, b + 25), 255) : conf_bg;
        ImU32 conf_act = has_selection ? IM_COL32(std::max(0, static_cast<int>(r) - 25), std::max(0, static_cast<int>(g) - 25), std::max(0, static_cast<int>(b) - 25), 255) : conf_bg;

        ImGui::PushStyleColor(ImGuiCol_Button, conf_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, conf_hov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, conf_act);
        ImGui::PushStyleColor(ImGuiCol_Text, has_selection ? UIConfig::Color::TextActive : IM_COL32(180, 195, 215, 140));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

        if (ImGui::Button("确认添加", ImVec2(confirm_w, 30.0f)) && has_selection) {
            // 批量将选中的曲目添加进歌单
            for (uint64_t tid : selected_track_ids_to_add_) {
                for (const auto& trk : all_scanned) {
                    if (trk.id == tid) {
                        target_playlist->addTrack(trk);
                        break;
                    }
                }
            }

            // 持久化至 SQLite 数据库
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

void CustomPlaylistView::renderDeletePlaylistModal(Playlist* target_playlist, std::vector<Playlist>& playlists) {
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

                // 核心寻址逻辑：
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
