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

    // 获取全局当前主题强调色，用于阳极氧化铝盘面与法兰微光润色
    const ImU32 accent = UIConfig::Color::Accent;
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 1. 底层大盘多阶柔和漫射阴影 (消除硬边缘)
    dl->AddCircleFilled(ImVec2(center.x + 3.0f, center.y + 6.0f), radius + 2.0f, IM_COL32(0, 0, 0, 75), 64);
    dl->AddCircleFilled(ImVec2(center.x + 1.0f, center.y + 3.0f), radius, IM_COL32(0, 0, 0, 95), 64);

    // 2. 金属轮盘外圈实体底板 (深空微光底，融入 12% 主题色调)
    uint32_t disk_r = static_cast<uint32_t>(ar * 0.12f + 16.0f);
    uint32_t disk_g = static_cast<uint32_t>(ag * 0.12f + 19.0f);
    uint32_t disk_b = static_cast<uint32_t>(ab * 0.12f + 25.0f);
    dl->AddCircleFilled(center, radius, IM_COL32(disk_r, disk_g, disk_b, 255), 64);

    // 3. 磁带卷盘实体层 (高密度发烧磁带，同心微纹理与柔和边缘)
    if (tape_radius > 36.0f) {
        // 磁带主体：深棕黑底带，微带主题暖光折射
        uint32_t tape_r = static_cast<uint32_t>(std::clamp(ar * 0.18f + 28.0f, 0.0f, 255.0f));
        uint32_t tape_g = static_cast<uint32_t>(std::clamp(ag * 0.15f + 22.0f, 0.0f, 255.0f));
        uint32_t tape_b = static_cast<uint32_t>(std::clamp(ab * 0.15f + 18.0f, 0.0f, 255.0f));
        dl->AddCircleFilled(center, tape_radius, IM_COL32(tape_r, tape_g, tape_b, 255), 64);

        // 磁带外层缠绕同心圆微光柔化圈
        dl->AddCircle(center, tape_radius, IM_COL32(tape_r + 30, tape_g + 20, tape_b + 15, 140), 64, 1.2f);
        dl->AddCircle(center, (tape_radius + 36.0f) * 0.5f, IM_COL32(tape_r + 15, tape_g + 10, tape_b + 8, 80), 48, 0.8f);
        dl->AddCircle(center, (tape_radius + 36.0f) * 0.75f, IM_COL32(tape_r + 20, tape_g + 15, tape_b + 12, 60), 48, 0.8f);
    }

    // 4. 金属外法兰盘倒角光晕 (柔和渐变铝合金边缘，无生硬实线)
    uint32_t rim_hi_r = static_cast<uint32_t>(std::clamp(ar * 0.45f + 140.0f, 0.0f, 255.0f));
    uint32_t rim_hi_g = static_cast<uint32_t>(std::clamp(ag * 0.45f + 140.0f, 0.0f, 255.0f));
    uint32_t rim_hi_b = static_cast<uint32_t>(std::clamp(ab * 0.45f + 150.0f, 0.0f, 255.0f));

    // 柔化外圈与倒角弧光
    dl->AddCircle(center, radius, IM_COL32(rim_hi_r, rim_hi_g, rim_hi_b, 160), 64, 1.4f);
    dl->AddCircle(center, radius - 1.5f, IM_COL32(255, 255, 255, 45), 64, 0.8f);
    dl->AddCircle(center, radius - 4.5f, IM_COL32(ar, ag, ab, 70), 64, 1.0f);

    // 5. 经典 3 孔镂空 NAB 轮毂 (铝合金内倾角与柔和阴影，随 angle 顺滑旋转)
    float hole_dist = radius * 0.58f;
    float hole_radius = radius * 0.22f;

    for (int k = 0; k < 3; ++k) {
        float a = angle + static_cast<float>(k) * (2.0f * kPi / 3.0f);
        ImVec2 h_center(center.x + std::cos(a) * hole_dist, center.y + std::sin(a) * hole_dist);

        // 镂空大圆孔内阴影
        dl->AddCircleFilled(h_center, hole_radius, IM_COL32(10, 13, 18, 250), 36);

        // 镂空孔内边缘柔和倒角反光圈 (结合主题色发光，消除生硬单线)
        dl->AddCircle(h_center, hole_radius, IM_COL32(rim_hi_r, rim_hi_g, rim_hi_b, 150), 36, 1.2f);
        dl->AddCircle(h_center, hole_radius - 1.2f, IM_COL32(ar, ag, ab, 60), 36, 0.8f);
    }

    // 6. 中央 NAB 金属锁扣与轴心盖 (纯正阳极氧化铝拉丝同心环)
    dl->AddCircleFilled(center, 36.0f, IM_COL32(22, 26, 34, 240), 36);
    dl->AddCircle(center, 36.0f, IM_COL32(rim_hi_r, rim_hi_g, rim_hi_b, 180), 36, 1.4f);

    // 主题色微光外环
    dl->AddCircleFilled(center, 24.0f, IM_COL32(ar / 3, ag / 3, ab / 3, 200), 28);
    dl->AddCircle(center, 24.0f, IM_COL32(ar, ag, ab, 190), 28, 1.4f);

    // 三芒星锁定卡齿 (柔和微高光金属棒)
    for (int k = 0; k < 3; ++k) {
        float a = angle + static_cast<float>(k) * (2.0f * kPi / 3.0f);
        ImVec2 p_in(center.x + std::cos(a) * 7.0f, center.y + std::sin(a) * 7.0f);
        ImVec2 p_out(center.x + std::cos(a) * 22.0f, center.y + std::sin(a) * 22.0f);
        dl->AddLine(p_in, p_out, IM_COL32(rim_hi_r, rim_hi_g, rim_hi_b, 190), 1.6f);
    }

    // 中心精密不锈钢紧固轴心
    dl->AddCircleFilled(center, 6.5f, IM_COL32(235, 240, 250, 240), 20);
    dl->AddCircle(center, 6.5f, IM_COL32(ar, ag, ab, 160), 20, 1.0f);
    dl->AddCircleFilled(center, 2.0f, IM_COL32(10, 12, 16, 255), 12);
}

void TapeReelRenderer::drawHeadBlock(ImDrawList* dl, ImVec2 center, float width, float height) {
    ImVec2 p0(center.x - width * 0.5f, center.y - height * 0.5f);
    ImVec2 p1(center.x + width * 0.5f, center.y + height * 0.5f);

    const ImU32 accent = UIConfig::Color::Accent;

    // 磁头基座屏蔽罩底板 (带微倒角与轻柔漫射)
    dl->AddRectFilled(p0, p1, IM_COL32(18, 22, 30, 230), 6.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 24), 6.0f, 0, 1.0f);

    // 三组精密模拟磁头 (消音磁头、录音磁头、放音磁头)
    float head_gap = width / 4.0f;
    for (int h = 1; h <= 3; ++h) {
        float hx = p0.x + static_cast<float>(h) * head_gap;
        ImVec2 h_min(hx - 12.0f, center.y - 13.0f);
        ImVec2 h_max(hx + 12.0f, center.y + 13.0f);

        // 坡莫合金金属屏蔽外壳与微光主题镶边
        dl->AddRectFilled(h_min, h_max, IM_COL32(36, 42, 54, 255), 3.0f);
        dl->AddRect(h_min, h_max, IM_COL32(200, 210, 230, 140), 3.0f, 0, 1.0f);
        // 磁头工作微隙 (Gap line)
        dl->AddLine(ImVec2(hx, center.y - 11.0f), ImVec2(hx, center.y + 11.0f), accent, 1.2f);
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

    // 1. 深空复古发烧机架底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    const ImU32 accent = UIConfig::Color::Accent;
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 2. 双开盘带轮几何排版
    float reel_radius = std::min(screen_w * 0.17f, 138.0f);
    float reel_y = screen_h * 0.44f;
    float reel_lx = screen_w * 0.28f;
    float reel_rx = screen_w * 0.72f;

    // 磁带随播放进度平滑转移：左盘供带减少 (115px -> 45px)，右盘收带增多 (45px -> 115px)
    float progress = std::clamp(elapsed_sec / 300.0f, 0.0f, 1.0f);
    float tape_r_l = 115.0f - progress * 65.0f;
    float tape_r_r = 50.0f + progress * 65.0f;

    // 3. 渲染走带路径 (磁带连接线，增加柔和渐隐与微光)
    ImVec2 tape_l_pt(reel_lx + 45.0f, screen_h * 0.74f);
    ImVec2 tape_r_pt(reel_rx - 45.0f, screen_h * 0.74f);

    // 走带暗褐色反光实带
    dl->AddLine(ImVec2(reel_lx + 20.0f, reel_y + reel_radius * 0.7f), tape_l_pt, IM_COL32(38, 28, 24, 210), 3.5f);
    dl->AddLine(tape_l_pt, tape_r_pt, IM_COL32(46, 32, 26, 230), 4.0f);
    dl->AddLine(tape_r_pt, ImVec2(reel_rx - 20.0f, reel_y + reel_radius * 0.7f), IM_COL32(38, 28, 24, 210), 3.5f);

    // 磁带微反光亮线 (融入主题微光)
    dl->AddLine(tape_l_pt, tape_r_pt, IM_COL32(ar, ag, ab, 70), 1.0f);

    // 4. 左右对称导带轮 (Guide Rollers)
    dl->AddCircleFilled(tape_l_pt, 10.0f, IM_COL32(140, 155, 175, 230), 24);
    dl->AddCircle(tape_l_pt, 10.0f, IM_COL32(ar, ag, ab, 160), 24, 1.2f);
    dl->AddCircleFilled(tape_l_pt, 3.5f, IM_COL32(ar, ag, ab, 220), 12);

    dl->AddCircleFilled(tape_r_pt, 10.0f, IM_COL32(140, 155, 175, 230), 24);
    dl->AddCircle(tape_r_pt, 10.0f, IM_COL32(ar, ag, ab, 160), 24, 1.2f);
    dl->AddCircleFilled(tape_r_pt, 3.5f, IM_COL32(ar, ag, ab, 220), 12);

    // 5. 中央高保真磁头组 (Head Block)
    drawHeadBlock(dl, ImVec2(screen_w * 0.5f, screen_h * 0.74f), 130.0f, 32.0f);

    // 6. 渲染左供带盘与右收带盘 (带平滑转速与主题色阳极氧化质感)
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

    // 计数器柔和圆角卡片底板
    dl->AddRectFilled(ImVec2(cnt_x - 12.0f, cnt_y - 4.0f), ImVec2(cnt_x + cnt_sz.x + 12.0f, cnt_y + cnt_sz.y + 4.0f),
                      IM_COL32(16, 20, 26, 220), 5.0f);
    dl->AddRect(ImVec2(cnt_x - 12.0f, cnt_y - 4.0f), ImVec2(cnt_x + cnt_sz.x + 12.0f, cnt_y + cnt_sz.y + 4.0f),
                IM_COL32(ar, ag, ab, 90), 5.0f, 0, 1.0f);
    dl->AddText(ImVec2(cnt_x, cnt_y), accent, counter_buf);
    if (Fonts::Regular) ImGui::PopFont();

    // 8. 底部经典开盘机铭牌
    const char* footer_left = "Studer Revox Master Tape Deck · Direct-Drive Dual Reel System";
    const char* footer_right = "NAB / CCIR MASTER TAPE · PURE ANALOG REPRODUCTION";

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), accent, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), accent, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
