#include "views/MainStageView.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include <algorithm>
#include <cstdio>

MainStageView::MainStageView() {
    // 树莓派真机默认扫描路径适配
#if defined(HIFI_PLATFORM_RPI)
    std::snprintf(scan_path_buf_, sizeof(scan_path_buf_), "/home/pi/Music");
#else
    std::snprintf(scan_path_buf_, sizeof(scan_path_buf_), "/Users/mk10/Music");
#endif
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
    // 液态玻璃微光磨砂底板与 1px 折射边框
    dl->AddRectFilled(p_min, p_max, UIConfig::Color::ContainerBg, UIConfig::Layout::ContainerRounding);
    dl->AddRect(p_min, p_max, UIConfig::Color::ContainerBorder, UIConfig::Layout::ContainerRounding, 0, 1.0f);

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
    // 1. 铺设右侧主舞台深空暗色基底
    ImDrawList* bg_dl = ImGui::GetBackgroundDrawList();
    bg_dl->AddRectFilled(ImVec2(stage_x, stage_y), ImVec2(stage_x + stage_w, stage_y + stage_h), 
                         UIConfig::Color::MainStageBg);

    // 2. 创建主舞台专属透明顶层无边框窗口 (严格限制高度不重叠底部 BottomBar，消除事件劫持)
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
            case SidebarTab::CustomPlaylist:
                renderPlaylistView(selected_playlist_id, playlists, stage_x, stage_y, stage_w, stage_h);
                break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
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
        : "空歌单";

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, title.c_str(), subtitle.c_str());

    if (!target_playlist) return;

    auto& player = PlayerAdmin::getInstance();
    const auto& current_track = player.getCurrentTrack();

    float content_x = card_min.x + 20.0f;
    float content_y = card_min.y + 75.0f;
    float content_w = card_max.x - card_min.x - 40.0f;
    float content_h = card_max.y - card_min.y - 90.0f;

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
    drawLiquidCard(dl, card_min, card_max, "10段发烧级图形均衡器 (Graphic Equalizer)", "采用 RBJ Audio EQ 二阶 IIR 滤波算法 · 支持硬件 Direct 直通");
}

void MainStageView::renderDACSettingsView(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, "ES9038PRO 旗舰平衡解码前级设置", "硬件数字滤波滚降特性与飞秒双时钟同步管理");
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
