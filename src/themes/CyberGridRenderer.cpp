#include "themes/CyberGridRenderer.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

CyberGridRenderer::CyberGridRenderer() = default;

void CyberGridRenderer::render(float screen_w, float screen_h, bool is_playing, const float* spectrum_levels, int num_levels) {
    float current_time = static_cast<float>(ImGui::GetTime());
    float dt = (last_time_ > 0.0f) ? std::clamp(current_time - last_time_, 0.001f, 0.05f) : 0.016f;
    last_time_ = current_time;

    // 平滑频段能量
    int n_bands = std::min(num_levels, 16);
    float total_energy = 0.0f;
    for (int i = 0; i < n_bands; ++i) {
        float target = (is_playing && spectrum_levels) ? spectrum_levels[i] : 0.0f;
        smooth_levels_[i] += (target - smooth_levels_[i]) * (is_playing ? 14.0f : 5.0f) * dt;
        total_energy += smooth_levels_[i];
    }
    float avg_energy = (n_bands > 0) ? (total_energy / n_bands) : 0.0f;

    anim_time_ += (0.95f + avg_energy * 2.2f) * dt;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // 1. 深空黑底板
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(screen_w, screen_h), UIConfig::Color::MainStageBg);

    const ImU32 accent = UIConfig::Color::Accent;
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 2. 3D 透视参数配置
    constexpr int cols = 36;
    constexpr int rows = 22;

    float center_x = screen_w * 0.5f;
    float horizon_y = screen_h * 0.38f;
    float fov = 380.0f;
    float cam_y = -190.0f; // 相机位于网格上方俯视

    float z_near = 120.0f;
    float z_far = 760.0f;
    float x_span = 720.0f;

    // 顶点缓存 (cols × rows)
    struct GridVtx {
        ImVec2 pt;
        float depth_t; // 0.0 (最远) ~ 1.0 (最近)
        float wave_height;
        bool valid;
    };

    std::vector<GridVtx> grid(cols * rows);

    for (int r = 0; r < rows; ++r) {
        float norm_z = static_cast<float>(r) / static_cast<float>(rows - 1);
        float z = z_far - norm_z * (z_far - z_near); // 从远到近
        float depth_t = (z_far - z) / (z_far - z_near);

        for (int c = 0; c < cols; ++c) {
            float norm_x = static_cast<float>(c) / static_cast<float>(cols - 1); // 0.0 ~ 1.0
            float centered_x = (norm_x - 0.5f) * 2.0f; // -1.0 ~ +1.0
            float world_x = centered_x * (x_span * 0.5f);

            // 对应频段能量映射
            float band_pos = norm_x * static_cast<float>(std::max(1, n_bands - 1));
            int b0 = static_cast<int>(band_pos);
            int b1 = std::min(b0 + 1, n_bands - 1);
            float f = band_pos - static_cast<float>(b0);
            float band_energy = smooth_levels_[b0] * (1.0f - f) + smooth_levels_[b1] * f;

            // 地形起伏正弦波动计算 (多谐波流动波浪)
            float wave = std::sin(centered_x * 4.2f + anim_time_ * 1.5f)
                       * std::cos(norm_z * 5.5f - anim_time_ * 2.2f) * 0.65f
                       + std::sin(centered_x * 8.5f - anim_time_ * 3.0f + norm_z * 3.0f) * 0.35f;

            // 高度由待机微动 + 音乐能量强烈调制
            float wave_height = wave * (12.0f + band_energy * 95.0f);
            float world_y = wave_height;

            // 3D 透视投影
            float scale = fov / z;
            float px = center_x + world_x * scale;
            float py = horizon_y + (world_y - cam_y) * scale;

            int idx = r * cols + c;
            grid[idx].pt = ImVec2(px, py);
            grid[idx].depth_t = depth_t;
            grid[idx].wave_height = wave_height;
            grid[idx].valid = (z > 10.0f);
        }
    }

    // 3. 渲染纵深透视连线 (横向网线与纵向深谷线)
    // 纵向线 (从远到近流向观察者)
    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows - 1; ++r) {
            int i0 = r * cols + c;
            int i1 = (r + 1) * cols + c;
            if (!grid[i0].valid || !grid[i1].valid) continue;

            float avg_depth = (grid[i0].depth_t + grid[i1].depth_t) * 0.5f;
            // 距离雾化透明度 (远处淡隐入黑色深空)
            int alpha = static_cast<int>(std::clamp(avg_depth * avg_depth * 180.0f, 0.0f, 255.0f));
            if (alpha <= 2) continue;

            ImU32 col_line = IM_COL32(ar, ag, ab, alpha);
            float line_w = 0.8f + avg_depth * 1.2f;
            dl->AddLine(grid[i0].pt, grid[i1].pt, col_line, line_w);
        }
    }

    // 横向线
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            int i0 = r * cols + c;
            int i1 = r * cols + (c + 1);
            if (!grid[i0].valid || !grid[i1].valid) continue;

            float avg_depth = grid[i0].depth_t;
            int alpha = static_cast<int>(std::clamp(avg_depth * avg_depth * 140.0f, 0.0f, 255.0f));
            if (alpha <= 2) continue;

            ImU32 col_line = IM_COL32(ar, ag, ab, alpha);
            float line_w = 0.7f + avg_depth * 1.0f;
            dl->AddLine(grid[i0].pt, grid[i1].pt, col_line, line_w);
        }
    }

    // 4. 渲染发光粒子节点 (Glowing Particle Nodes)
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int idx = r * cols + c;
            if (!grid[idx].valid) continue;

            float d = grid[idx].depth_t;
            if (d < 0.15f) continue; // 远处极小微粒子省略，保留空间纵深

            float dot_radius = 1.0f + d * 2.5f;
            int dot_alpha = static_cast<int>(std::clamp(d * 240.0f, 0.0f, 255.0f));

            // 山峰高点粒子聚核高亮白光
            if (grid[idx].wave_height > 18.0f) {
                dl->AddCircleFilled(grid[idx].pt, dot_radius * 1.4f, IM_COL32(255, 255, 255, dot_alpha), 8);
                dl->AddCircle(grid[idx].pt, dot_radius * 2.2f, IM_COL32(ar, ag, ab, dot_alpha / 2), 8, 1.0f);
            } else {
                dl->AddCircleFilled(grid[idx].pt, dot_radius, IM_COL32(ar, ag, ab, dot_alpha), 8);
            }
        }
    }

    // 5. 底部铭牌
    const char* footer_left = "3D Cyberpunk Particle Grid · Perspective Terrain Modulation";
    const char* footer_right = "SPATIAL MESH DYNAMICS & DEPTH FOG";

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(24.0f, screen_h - 32.0f), accent, footer_left);
    ImVec2 badge_sz = ImGui::CalcTextSize(footer_right);
    dl->AddText(ImVec2(screen_w - badge_sz.x - 24.0f, screen_h - 32.0f), accent, footer_right);
    if (Fonts::Small) ImGui::PopFont();
}
