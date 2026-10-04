#include "views/MainStageView.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include <algorithm>

MainStageView::MainStageView() {
    settings_view_.setOnNavigateTab([this](int tab_id) {
        if (on_navigate_tab_) {
            on_navigate_tab_(static_cast<SidebarTab>(tab_id));
        }
    });
    all_music_view_.setOnNavigateTab([this](SidebarTab tab) {
        if (on_navigate_tab_) {
            on_navigate_tab_(tab);
        }
    });
    custom_playlist_view_.setOnNavigateTab([this](SidebarTab tab) {
        if (on_navigate_tab_) {
            on_navigate_tab_(tab);
        }
    });
    custom_playlist_view_.setOnSelectPlaylist([this](SidebarTab tab, uint64_t pl_id) {
        if (on_select_playlist_) {
            on_select_playlist_(tab, pl_id);
        }
    });
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

void MainStageView::render(SidebarTab current_tab, 
                           uint64_t selected_playlist_id, 
                           std::vector<Playlist>& playlists,
                           float stage_x, float stage_y, float stage_w, float stage_h) {
    // 创建主舞台专属透明顶层无边框窗口 (动态避让底部 BottomBar 胶囊高 48px + 边距 16px + 缓冲 12px = 76px)
    ImGui::SetNextWindowPos(ImVec2(stage_x, stage_y));
    float safe_stage_h = std::max(stage_h - 76.0f, 300.0f);
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
            case SidebarTab::MSEBTuning:
                magic_tuning_view_.render(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::DACSettings:
                dac_view_.render(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::ThemeSettings:
                theme_setting_view_.render(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::SystemSettings:
                settings_view_.render(stage_x, stage_y, stage_w, stage_h);
                break;
            case SidebarTab::AllMusic:
                all_music_view_.render(stage_x, stage_y, stage_w, stage_h, playlists);
                break;
            case SidebarTab::CustomPlaylist:
                custom_playlist_view_.render(selected_playlist_id, playlists, stage_x, stage_y, stage_w, stage_h);
                break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
