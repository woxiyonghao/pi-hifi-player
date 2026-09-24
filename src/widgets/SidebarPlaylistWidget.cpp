#include "widgets/SidebarPlaylistWidget.hpp"
#include "Font.hpp"
#include "UIConfig.hpp"
#include <string>

// 通透平滑的液态玻璃微光材质
static void DrawLiquidGlass(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, ImU32 fill_color) {
    dl->AddRectFilled(p_min, p_max, fill_color, rounding);
    dl->AddRect(p_min, p_max, UIConfig::Color::GlassBorder, rounding, 0, 1.0f);
}

void SidebarPlaylistWidget::drawPlaylistIcon(ImDrawList* dl, ImVec2 center, PlaylistIcon icon, ImU32 color) {
    switch (icon) {
        case PlaylistIcon::Music: {
            // 发烧双八分连音符 (符头 + 符杆 + 符梁)
            ImVec2 n1(center.x - 3.0f, center.y + 3.0f);
            ImVec2 n2(center.x + 3.0f, center.y + 1.0f);
            dl->AddCircleFilled(n1, 2.2f, color);
            dl->AddCircleFilled(n2, 2.2f, color);
            dl->AddLine(ImVec2(n1.x + 1.5f, n1.y), ImVec2(n1.x + 1.5f, center.y - 5.0f), color, 1.2f);
            dl->AddLine(ImVec2(n2.x + 1.5f, n2.y), ImVec2(n2.x + 1.5f, center.y - 7.0f), color, 1.2f);
            dl->AddLine(ImVec2(n1.x + 1.5f, center.y - 5.0f), ImVec2(n2.x + 1.5f, center.y - 7.0f), color, 1.6f);
            break;
        }
        case PlaylistIcon::Playlist: {
            // 三道等宽水平条目线
            for (int i = -1; i <= 1; ++i) {
                float y = center.y + i * 4.0f;
                dl->AddLine(ImVec2(center.x - 6.0f, y), ImVec2(center.x + 6.0f, y), color, 1.4f);
            }
            break;
        }
        case PlaylistIcon::Add: {
            // 正交等长十字加号
            dl->AddLine(ImVec2(center.x - 5.0f, center.y), ImVec2(center.x + 5.0f, center.y), color, 1.5f);
            dl->AddLine(ImVec2(center.x, center.y - 5.0f), ImVec2(center.x, center.y + 5.0f), color, 1.5f);
            break;
        }
    }
}

void SidebarPlaylistWidget::drawHeader(const char* title) {
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, UIConfig::Color::TextMuted);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);

    if (Fonts::Small) {
        ImGui::PushFont(Fonts::Small);
    }
    ImGui::TextUnformatted(title);
    if (Fonts::Small) {
        ImGui::PopFont();
    }

    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

bool SidebarPlaylistWidget::drawPlaylistItem(PlaylistIcon icon, const char* label, bool is_selected, PlaylistIndicatorTarget& out_target) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = UIConfig::Layout::NavItemHeight;
    float rounding = UIConfig::Layout::NavItemRounding;

    std::string btn_id = std::string("##pl_") + label;
    bool clicked = ImGui::InvisibleButton(btn_id.c_str(), ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();

    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + width, pos.y + height);

    if (is_selected) {
        out_target.x = p_min.x;
        out_target.y = pos.y;
        out_target.width = width;
        out_target.height = height;
    } else if (hovered) {
        DrawLiquidGlass(dl, p_min, p_max, rounding, UIConfig::Color::GlassHover);
    }

    ImU32 icon_col = is_selected ? UIConfig::Color::TextActive : UIConfig::Color::IconNormal;
    ImU32 text_col = is_selected ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal;

    drawPlaylistIcon(dl, ImVec2(pos.x + 18.0f, pos.y + height * 0.5f), icon, icon_col);
    dl->AddText(ImVec2(pos.x + 36.0f, pos.y + 8.0f), text_col, label);

    ImGui::Dummy(ImVec2(0.0f, 1.0f));
    return clicked;
}

std::optional<PlaylistIndicatorTarget> SidebarPlaylistWidget::render(
    const std::vector<Playlist>& playlists,
    SidebarTab& current_tab,
    uint64_t& selected_playlist_id
) {
    drawHeader("播放列表");

    std::optional<PlaylistIndicatorTarget> active_target = std::nullopt;

    // 1. 首项：所有音乐
    {
        bool is_all_music = (current_tab == SidebarTab::AllMusic);
        PlaylistIndicatorTarget target{};
        if (drawPlaylistItem(PlaylistIcon::Music, "所有音乐", is_all_music, target)) {
            current_tab = SidebarTab::AllMusic;
        }
        if (is_all_music) {
            active_target = target;
        }
    }

    // 2. 自定义歌单实体列表
    for (const auto& playlist : playlists) {
        bool is_sel = (current_tab == SidebarTab::CustomPlaylist && selected_playlist_id == playlist.getId());
        PlaylistIndicatorTarget target{};
        if (drawPlaylistItem(PlaylistIcon::Playlist, playlist.getName().c_str(), is_sel, target)) {
            current_tab = SidebarTab::CustomPlaylist;
            selected_playlist_id = playlist.getId();
        }
        if (is_sel) {
            active_target = target;
        }
    }

    // 3. 添加播放列表按钮
    {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        PlaylistIndicatorTarget dummy_target{};
        if (drawPlaylistItem(PlaylistIcon::Add, "添加播放列表", false, dummy_target)) {
            if (on_create_playlist_) {
                on_create_playlist_();
            }
        }
    }

    return active_target;
}