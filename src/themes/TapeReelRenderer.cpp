#include "themes/TapeReelRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

TapeReelRenderer::TapeReelRenderer() = default;

void TapeReelRenderer::drawReel(ImDrawList* dl, ImVec2 center, float radius, float tape_radius, float angle, bool is_left) {
    (void)is_left;
    constexpr float kPi = 3.14159265358979323846f;

    // 1. 底层磁带盘身阴影
    dl->AddCircleFilled(ImVec2(center.x + 2.0f, center.y + 4.0f), radius, IM_COL32(0, 0, 0, 110), 64);

    // 2. 金属轮盘外圈实体底板 (深色磨砂底)
    dl->AddCircleFilled(center, radius, IM_COL32(26, 30, 38, 255), 64);

    // 3. 磁带卷盘实体层 (深褐色高密度氧化铁磁带，带同心圆走带纹理)
    if (tape_radius > 35.0f) {
        dl->AddCircleFilled(center, tape_radius, IM_COL32(42, 28, 20, 255), 64);
        dl->AddCircle(center, tape_radius, IM_COL32(75, 48, 32, 220), 64, 1.5f);
        // 同心圆走带微纹理
        dl->AddCircle(center, (tape_radius + 35.0f) * 0.5f, IM_COL32(32, 20, 14, 120), 48, 1.0f);
        dl->AddCircle(center, (tape_radius + 35.0f) * 0.75f, IM_COL32(50, 32, 22, 100), 48, 1.0f);
    }

    // 4. 金属外法兰盘镂空外圈 (铝合金拉丝盘面)
    dl->AddCircle(center, radius, IM_COL32(180, 190, 205, 230), 64, 2.5f);
    dl->AddCircle(center, radius - 4.0f, IM_COL32(100, 110, 125, 160), 64, 1.0f);

    // 5. 经典 3 孔镂空 NAB 轮毂 (随着 angle 旋转)
    float hole_dist = radius * 0.58f;
    float hole_radius = radius * 0.22f;

    for (int k = 0; k < 3; ++k) {
        float a = angle + static_cast<float>(k) * (2.0f * kPi / 3.0f);
        ImVec2 h_center(center.x + std::cos(a) * hole_dist, center.y + std::sin(a) * hole_dist);

        // 镂空大圆孔 (透视到内部深色机身与磁带底层)
        dl->AddCircleFilled(h_center, hole_radius, IM_COL32(16, 19, 24, 255), 32);
        // 铝合金边缘倒角高光反光线
        dl->AddCircle(h_center, hole_radius, IM_COL32(210, 220, 235, 200), 32, 1.5f);
        dl->AddCircle(h_center, hole_radius - 1.5f, IM_COL32(80, 90, 105, 120), 32, 1.0f);
    }

    // 6. 中央 NAB 金属锁扣与轴心盖 (银色/金色精密同心环)
    const ImU32 accent = UIConfig::Color::Accent;
    dl->AddCircleFilled(center, 34.0f, IM_COL32(38, 44, 56, 255), 32);
    dl->AddCircle(center, 34.0f, IM_COL32(160, 175, 195, 240), 32, 2.0f);
    dl->AddCircleFilled(center, 22.0f, IM_COL32(18, 22, 28, 255), 24);
    dl->AddCircle(center, 22.0f, accent, 24, 1.6f);

    // 三芒星锁定卡齿
    for (int k = 0; k < 3; ++k) {
        float a = angle + static_cast<float>(k) * (2.0f * kPi / 3.0f);
        ImVec2 p_in(center.x + std::cos(a) * 8.0f, center.y + std::sin(a) * 8.0f);
        ImVec2 p_out(center.x + std::cos(a) * 20.0f, center.y + std::sin(a) * 20.0f);
        dl->AddLine(p_in, p_out, IM_COL32(230, 235, 245, 230), 2.2f);
    }

    // 中心紧固螺钉
    dl->AddCircleFilled(center, 6.0f, IM_COL32(220, 225, 235, 255), 16);
}

void TapeReelRenderer::drawHeadBlock(ImDrawList* dl, ImVec2 center, float width, float height) {
    ImVec2 p0(center.x - width * 0.5f, center.y - height * 0.5f);
    ImVec2 p1(center.x + width * 0.5f, center.y + height * 0.5f);

    // 磁头基座屏蔽罩
    dl->AddRectFilled(p0, p1, IM_COL32(22, 26, 34, 240), 6.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 30), 6.0f, 0, 1.0f);

    // 三组精密模拟磁头 (消音磁头、录音磁头、放音磁头)
    float head_gap = width / 4.0f;
    for (int h = 1; h <= 3; ++h) {
        float hx = p0.x + static_cast<float>(h) * head_gap;
        ImVec2 h_min(hx - 12.0f, center.y - 14.0f);
        ImVec2 h_max(hx + 12.0f, center.y + 14.0f);

        // 坡莫合金金属屏蔽外壳
        dl->AddRectFilled(h_min, h_max, IM_COL32(50, 58, 72, 255), 3.0f);
        dl->AddRect(h_min, h_max, IM_COL32(190, 205, 225, 200), 3.0f, 0, 1.2f);
        // 磁头工作微隙 (Gap line)
        dl->AddLine(ImVec2(hx, center.y - 12.0f), ImVec2(hx, center.y + 12.0f), IM_COL32(20, 24, 30, 255), 1.5f);
    }
}

void TapeReelRenderer::render(float screen_w, float screen_h, bool is_playing, float elapsed_sec) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    // 机械角速度平滑加减速 (惯性起停)
    float target_speed = is_playing ? 1.45f : 0.0f;
    current_speed_ += (target_speed - current_speed_) * (is_playing ? 3.0f : 1.8f) * dt;
    reel_angle_ += current_speed_ * dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空复古发烧机架面板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    // 2. 双开盘带轮几何布局 (左右对称，上部留白，下部展示走带磁头)
    float reel_radius = std::min(screen_w * 0.17f, 138.0f);
    float reel_y = screen_h * 0.44f;
    float reel_lx = screen_w * 0.28f;
    float reel_rx = screen_w * 0.72f;

    // 磁带随播放进度微量转移：左盘供带减少 (115px -> 45px)，右盘收带增多 (45px -> 115px)
    float progress = std::clamp(elapsed_sec / 300.0f, 0.0f, 1.0f);
    float tape_r_l = 115.0f - progress * 65.0f;
    float tape_r_r = 50.0f + progress * 65.0f;

    // 3. 渲染穿梭磁带连接线 (Taut magnetic tape path)
    ImVec2 tape_l_pt(reel_lx + 45.0f, screen_h * 0.74f);
    ImVec2 tape_r_pt(reel_rx - 45.0f, screen_h * 0.74f);

    // 走带暗褐反光实带
    dl->AddLine(ImVec2(reel_lx + 20.0f, reel_y + reel_radius * 0.7f), tape_l_pt, IM_COL32(45, 30, 22, 230), 4.0f);
    dl->AddLine(tape_l_pt, tape_r_pt, IM_COL32(55, 36, 26, 240), 4.5f);
    dl->AddLine(tape_r_pt, ImVec2(reel_rx - 20.0f, reel_y + reel_radius * 0.7f), IM_COL32(45, 30, 22, 230), 4.0f);

    // 磁带表面微高光
    dl->AddLine(tape_l_pt, tape_r_pt, IM_COL32(110, 80, 60, 100), 1.0f);

    // 4. 左右对称导带轮 (Guide Rollers)
    dl->AddCircleFilled(tape_l_pt, 10.0f, IM_COL32(160, 175, 195, 255), 24);
    dl->AddCircle(tape_l_pt, 10.0f, IM_COL32(60, 70, 85, 255), 24, 2.0f);
    dl->AddCircleFilled(tape_l_pt, 3.5f, IM_COL32(20, 24, 30, 255), 12);

    dl->AddCircleFilled(tape_r_pt, 10.0f, IM_COL32(160, 175, 195, 255), 24);
    dl->AddCircle(tape_r_pt, 10.0f, IM_COL32(60, 70, 85, 255), 24, 2.0f);
    dl->AddCircleFilled(tape_r_pt, 3.5f, IM_COL32(20, 24, 30, 255), 12);

    // 5. 中央高保真磁头组 (Head Block)
    drawHeadBlock(dl, ImVec2(screen_w * 0.5f, screen_h * 0.74f), 130.0f, 32.0f);

    // 6. 渲染左供带盘与右收带盘 (带平滑转速与三孔镂空)
    drawReel(dl, ImVec2(reel_lx, reel_y), reel_radius, tape_r_l, reel_angle_, true);
    drawReel(dl, ImVec2(reel_rx, reel_y), reel_radius, tape_r_r, reel_angle_ * 1.08f, false);

    // 7. 机械数字时间计数器
    char counter_buf[32];
    int mins = static_cast<int>(elapsed_sec) / 60;
    int secs = static_cast<int>(elapsed_sec) % 60;
    std::snprintf(counter_buf, sizeof(counter_buf), "[ %02d : %02d ]", mins, secs);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 cnt_sz = ImGui::CalcTextSize(counter_buf);
    float cnt_x = screen_w * 0.5f - cnt_sz.x * 0.5f;
    float cnt_y = reel_y - 20.0f;

    // 计数器深色微框
    dl->AddRectFilled(ImVec2(cnt_x - 12.0f, cnt_y - 4.0f), ImVec2(cnt_x + cnt_sz.x + 12.0f, cnt_y + cnt_sz.y + 4.0f), IM_COL32(18, 22, 28, 230), 4.0f);
    dl->AddRect(ImVec2(cnt_x - 12.0f, cnt_y - 4.0f), ImVec2(cnt_x + cnt_sz.x + 12.0f, cnt_y + cnt_sz.y + 4.0f), IM_COL32(255, 255, 255, 30), 4.0f, 0, 1.0f);
    dl->AddText(ImVec2(cnt_x, cnt_y), UIConfig::Color::Accent, counter_buf);
    if (Fonts::Regular) ImGui::PopFont();

    // 8. 底部经典开盘机铭牌
    const char* footer_left = "Studer Revox Master Tape Deck · 15 IPS Direct-Drive Dual Reel";
    const char* footer_right = "2-TRACK MASTER TAPE · NAB / CCIR PURE ANALOG HEAD";
    const ImU32 badge_col = UIConfig::Color::Accent;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), badge_col, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), badge_col, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
