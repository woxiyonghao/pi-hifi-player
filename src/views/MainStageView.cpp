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

    // 2. 根据当前导航项智能分发子面板渲染
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

void MainStageView::renderScanMusicView(float x, float y, float w, float h, std::vector<Playlist>& playlists) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f); // 预留底部 BottomBar 胶囊空间

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, "本地发烧曲库自动检索", "支持 FLAC、WAV、DSD(DSF/DFF)、ALAC 等无损与母带规格");

    auto& scanner = MusicScanManager::getInstance();

    // 在卡片内部开启 ImGui 控件上下文
    ImGui::SetNextWindowPos(ImVec2(card_min.x + 20.0f, card_min.y + 70.0f));
    ImGui::SetNextWindowSize(ImVec2(card_max.x - card_min.x - 40.0f, card_max.y - card_min.y - 85.0f));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;
    if (ImGui::BeginChild("ScanMusicContent", ImVec2(0, 0), false, flags)) {
        // 路径输入框
        ImGui::AlignTextToFramePadding();
        ImGui::Text("扫描目录:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputText("##ScanPath", scan_path_buf_, sizeof(scan_path_buf_));

        ImGui::SameLine(0.0f, 15.0f);
        if (!scanner.isScanning()) {
            if (ImGui::Button(" 开始高速检索 ", ImVec2(130.0f, 0))) {
                scanner.startScan(scan_path_buf_);
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(235, 75, 75, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 95, 95, 230));
            if (ImGui::Button(" 中止扫描 ", ImVec2(110.0f, 0))) {
                scanner.cancelScan();
            }
            ImGui::PopStyleColor(2);
        }

        // 状态与统计栏
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ScanState state = scanner.getState();
        size_t count = scanner.getFoundCount();

        if (state == ScanState::Scanning) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "● 正在高速递归检索发烧母带... 已发现: %zu 首", count);
        } else if (state == ScanState::Completed) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "✔ 扫描完成！共索引发烧音频: %zu 首", count);
            if (count > 0 && !playlists.empty()) {
                ImGui::SameLine(0.0f, 20.0f);
                if (ImGui::Button(" 📥 导入全部扫描曲目至全部音乐 ", ImVec2(220.0f, 0))) {
                    auto scanned = scanner.getScannedTracks();
                    for (const auto& track : scanned) {
                        playlists[0].addTrack(track);
                    }
                }
            }
        } else if (state == ScanState::Cancelled) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "⚠ 扫描已被用户中止。已保留已发现曲目: %zu 首", count);
        } else if (state == ScanState::Failed) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "✕ 路径无效或无法访问，请检查目录权限！");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "状态: 待机就绪，点击上方按钮开启后台非阻塞扫描。");
        }

        // 扫描结果列表滚动区域
        ImGui::Spacing();
        auto scanned_tracks = scanner.getScannedTracks();
        if (!scanned_tracks.empty()) {
            ImGui::BeginChild("ScannedListScroll", ImVec2(0, 0), true);
            for (size_t i = 0; i < scanned_tracks.size(); ++i) {
                const auto& t = scanned_tracks[i];
                ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "%02zu.", i + 1);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", t.title.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 0.9f), "- %s", t.artist.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 0.9f), "[%s]", t.getFormatBadge().c_str());
            }
            ImGui::EndChild();
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
        : "空歌单";

    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawLiquidCard(dl, card_min, card_max, title.c_str(), subtitle.c_str());

    if (!target_playlist) return;

    auto& player = PlayerAdmin::getInstance();
    const auto& current_track = player.getCurrentTrack();

    ImGui::SetNextWindowPos(ImVec2(card_min.x + 20.0f, card_min.y + 75.0f));
    ImGui::SetNextWindowSize(ImVec2(card_max.x - card_min.x - 40.0f, card_max.y - card_min.y - 90.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove;
    if (ImGui::BeginChild("TrackListContent", ImVec2(0, 0), false, flags)) {
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
