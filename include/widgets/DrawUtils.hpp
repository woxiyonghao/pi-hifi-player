#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <utility>

// 纯原生 GPU 矢量圆角三角形/多边形 (Apple 质感丝滑倒角，彻底告别尖锐毛刺)
inline void DrawRoundedTriangle(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImVec2 p2, float radius, ImU32 col) {
    if (radius <= 0.1f) {
        dl->AddTriangleFilled(p0, p1, p2, col);
        return;
    }
    // 强制顺时针绕向 (CW winding order)：确保 ImGui 外抗锯齿羽化完美向外展开
    float cross = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (cross < 0.0f) {
        std::swap(p1, p2);
    }
    const ImVec2 pts[3] = { p0, p1, p2 };
    for (int i = 0; i < 3; ++i) {
        ImVec2 prev_p = pts[(i + 2) % 3];
        ImVec2 curr_p = pts[i];
        ImVec2 next_p = pts[(i + 1) % 3];
        float v1x = prev_p.x - curr_p.x;
        float v1y = prev_p.y - curr_p.y;
        float v2x = next_p.x - curr_p.x;
        float v2y = next_p.y - curr_p.y;
        float len1 = std::sqrt(v1x * v1x + v1y * v1y);
        float len2 = std::sqrt(v2x * v2x + v2y * v2y);
        if (len1 < 1e-4f || len2 < 1e-4f) {
            continue;
        }
        float u1x = v1x / len1, u1y = v1y / len1;
        float u2x = v2x / len2, u2y = v2y / len2;
        float dot = std::clamp(u1x * u2x + u1y * u2y, -1.0f, 1.0f);
        float half_angle = std::acos(dot) * 0.5f;
        float d = (half_angle > 1e-3f) ? (radius / std::tan(half_angle)) : 0.0f;
        d = std::min(d, std::min(len1, len2) * 0.46f);
        ImVec2 t_in(curr_p.x + u1x * d, curr_p.y + u1y * d);
        ImVec2 t_out(curr_p.x + u2x * d, curr_p.y + u2y * d);
        dl->PathLineTo(t_in);
        dl->PathBezierQuadraticCurveTo(curr_p, t_out, 4);
    }
    dl->PathFillConvex(col);
}