#include "views/ScanMusicWidget.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"
#include <cmath>
#include <cstdio>

ScanMusicWidget::ScanMusicWidget() {
#if defined(HIFI_PLATFORM_RPI)
    std::snprintf(default_scan_path_, sizeof(default_scan_path_), "/home/pi/Music");
#else
    std::snprintf(default_scan_path_, sizeof(default_scan_path_), "/Users/mk10/Music");
#endif
}

void ScanMusicWidget::drawSearchIcon(ImDrawList* dl, ImVec2 center, float radius, float offset_x, float offset_y) {
    // 叠加 2D 上下左右平滑巡游偏移量
    ImVec2 pos(center.x + offset_x, center.y + offset_y);

    // 放大镜透镜中心稍微偏左上方，确保 45° 手柄朝向右下方自然平衡
    ImVec2 lens_c(pos.x - radius * 0.2f, pos.y - radius * 0.2f);
    float lens_r = radius * 0.62f;

    // 1. 镜片半透明微光底板 (通透液态玻璃质感)
    dl->AddCircleFilled(lens_c, lens_r - 2.0f, IM_COL32(255, 255, 255, 12), 48);

    // 2. 外部主镜框 (主题色玫瑰红 + 柔和外发光光圈)
    dl->AddCircle(lens_c, lens_r + 2.5f, IM_COL32(250, 45, 72, 50), 48, 2.0f); // 柔和外发光
    dl->AddCircle(lens_c, lens_r, UIConfig::Color::Accent, 48, 3.5f);            // 玫瑰红金属镜圈

    // 3. 镜片弧光反射 (左上圆弧高光，呈现晶莹剔透感)
    dl->PathArcTo(lens_c, lens_r - 6.0f, -2.4f, -0.9f, 16);
    dl->PathStroke(IM_COL32(255, 255, 255, 140), 0, 2.0f);

    // 4. 镜内探索引导小圆 (渲染主题色玫瑰红 + 柔和内发光)
    dl->AddCircleFilled(lens_c, lens_r * 0.38f, IM_COL32(250, 45, 72, 35), 32);
    dl->AddCircle(lens_c, lens_r * 0.38f, UIConfig::Color::Accent, 32, 2.0f);

    // 5. 45度斜向手柄 (指向右下方，圆润手感)
    const float cos45 = 0.7071f;
    const float sin45 = 0.7071f;
    ImVec2 h_start(lens_c.x + lens_r * cos45, lens_c.y + lens_r * sin45);
    ImVec2 h_end(h_start.x + radius * 0.62f, h_start.y + radius * 0.62f);

    // 手柄本体
    dl->AddLine(h_start, h_end, UIConfig::Color::Accent, 5.5f);
    dl->AddCircleFilled(h_end, 2.75f, UIConfig::Color::Accent, 16); // 手柄末端平滑半圆

    // 手柄背脊微光高光线
    dl->AddLine(ImVec2(h_start.x + 1.2f, h_start.y + 1.2f),
                ImVec2(h_end.x - 2.5f, h_end.y - 2.5f),
                IM_COL32(255, 255, 255, 90), 1.8f);
}

void ScanMusicWidget::render(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, std::vector<Playlist>& playlists) {
    anim_timer_ += 0.035f; // 约 60fps 时间步长推进

    // 计算卡片中心点
    ImVec2 center(
        (p_min.x + p_max.x) * 0.5f,
        (p_min.y + p_max.y) * 0.5f
    );

    ScanState state = MusicScanManager::getInstance().getState();

    // 状态机分发
    if (state == ScanState::Scanning) {
        renderScanningState(dl, center);
    } else if (state == ScanState::Completed) {
        renderCompletedState(dl, p_min, p_max, playlists);
    } else {
        // Idle 或 Cancelled/Failed 状态均呈现样式 1 (待机态)
        renderIdleState(dl, center, playlists);
    }
}

void ScanMusicWidget::renderIdleState(ImDrawList* dl, ImVec2 center, [[maybe_unused]] std::vector<Playlist>& playlists) {
    // 1. 2D 上下左右平滑巡游动效：利用不同频率的正弦/余弦实现柔和的多维空间漫游漂浮
    float offset_x = std::cos(anim_timer_ * 1.5f) * 12.0f;
    float offset_y = std::sin(anim_timer_ * 2.2f) * 9.0f;

    // 绘制大号 🔍 矢量放大镜 (半径 48px，位置在中心偏上，消除中间文本留出呼吸空间)
    ImVec2 icon_center(center.x, center.y - 35.0f);
    drawSearchIcon(dl, icon_center, 48.0f, offset_x, offset_y);

    // 2. 居中发烧级胶囊扫描按钮
    const float btn_w = 190.0f;
    const float btn_h = 42.0f;
    const float btn_rounding = btn_h * 0.5f; // 21px 纯半圆胶囊

    ImVec2 btn_p0(center.x - btn_w * 0.5f, center.y + 60.0f);
    ImVec2 btn_p1(btn_p0.x + btn_w, btn_p0.y + btn_h);

    // 判定鼠标悬停与点击状态
    bool is_hovered = ImGui::IsMouseHoveringRect(btn_p0, btn_p1);
    bool is_clicked = is_hovered && ImGui::IsMouseClicked(0);

    // 按钮渲染层次结构：
    // Blur 态：具有清晰半透明 Alpha 质感，无外发光，不抢视觉重心
    // Onhover 态：完全对齐左侧边栏「扫描音乐」主题色胶囊，环境辉光绽放 + 1px 折射微光边
    if (is_hovered) {
        // ---------------- Hover 态 (onhover) ----------------
        // 1. 发光辉光层 (双层微光扩散)
        dl->AddRectFilled(ImVec2(btn_p0.x - 5.0f, btn_p0.y - 5.0f), 
                          ImVec2(btn_p1.x + 5.0f, btn_p1.y + 5.0f), 
                          IM_COL32(250, 45, 72, 30), btn_rounding + 4.0f);
        dl->AddRectFilled(ImVec2(btn_p0.x - 2.5f, btn_p0.y - 2.5f), 
                          ImVec2(btn_p1.x + 2.5f, btn_p1.y + 2.5f), 
                          IM_COL32(250, 45, 72, 60), btn_rounding + 2.0f);

        // 2. 严格对齐左侧边栏选中的主题色液态玻璃配方 (对齐后与侧边栏完全一致)
        dl->AddRectFilled(btn_p0, btn_p1, IM_COL32(250, 45, 72, 85), btn_rounding);
        dl->AddRectFilled(btn_p0, btn_p1, UIConfig::Color::GlassActive, btn_rounding); // 30 Alpha 磨砂白

        // 3. 1px 微光折射圆角边框
        dl->AddRect(btn_p0, btn_p1, UIConfig::Color::GlassBorder, btn_rounding, 0, 1.0f); // 50 Alpha 边框
    } else {
        // ---------------- Blur 态 (未悬停) ----------------
        // 1. 无外发光晕，消除光晕带来的膨胀感与过亮感
        // 2. 带有轻盈 Alpha 的半透明玫瑰红底板 + 微量通透层，清晰透出暗色背景
        dl->AddRectFilled(btn_p0, btn_p1, IM_COL32(250, 45, 72, 65), btn_rounding);
        dl->AddRectFilled(btn_p0, btn_p1, IM_COL32(255, 255, 255, 12), btn_rounding);

        // 3. 极细柔和边缘轮廓
        dl->AddRect(btn_p0, btn_p1, IM_COL32(255, 255, 255, 35), btn_rounding, 0, 1.0f);
    }

    ImU32 text_col = is_hovered 
        ? UIConfig::Color::TextActive        // 悬停时：纯白 100%
        : IM_COL32(215, 222, 235, 210);      // 未悬停时：柔和浅白 (带适度透感)

    // 按钮内部居中文本
    const char* btn_label = "全盘检索";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 label_sz = ImGui::CalcTextSize(btn_label);
    dl->AddText(ImVec2(btn_p0.x + (btn_w - label_sz.x) * 0.5f, btn_p0.y + (btn_h - label_sz.y) * 0.5f),
                text_col, btn_label);
    if (Fonts::Regular) ImGui::PopFont();

    // 点击启动异步扫描，状态将自动切入 Scanning！
    if (is_clicked) {
        MusicScanManager::getInstance().startScan(default_scan_path_);
    }
}

void ScanMusicWidget::renderScanningState([[maybe_unused]] ImDrawList* dl, [[maybe_unused]] ImVec2 center) {
    // 【下一步待实现】：扫描中，Icon 上下左右模拟滚动 + 底部扫描动态能量波
}

void ScanMusicWidget::renderCompletedState([[maybe_unused]] ImDrawList* dl, 
                                          [[maybe_unused]] ImVec2 p_min, 
                                          [[maybe_unused]] ImVec2 p_max, 
                                          [[maybe_unused]] std::vector<Playlist>& playlists) {
    // 【下一步待实现】：扫描完成，呈现统计卡片与入库按钮
}