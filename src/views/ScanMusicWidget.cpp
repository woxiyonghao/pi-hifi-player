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

void ScanMusicWidget::drawSearchIcon(ImDrawList* dl, ImVec2 center, float radius, float offset_x, float offset_y, bool is_scanning) {
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

    // 扫描态特有增强动效：镜片内部雷达探照波与旋转光针
    if (is_scanning) {
        // 雷达扩散脉冲波
        float ping_r = std::fmod(anim_timer_ * 28.0f, lens_r * 0.75f);
        int ping_alpha = static_cast<int>((1.0f - (ping_r / (lens_r * 0.75f))) * 160.0f);
        dl->AddCircle(lens_c, ping_r, IM_COL32(250, 45, 72, ping_alpha), 24, 1.2f);

        // 旋转扫描光线 (雷达声纳指针)
        float sweep_ang = anim_timer_ * 5.5f;
        ImVec2 sweep_tip(lens_c.x + std::cos(sweep_ang) * (lens_r * 0.72f),
                         lens_c.y + std::sin(sweep_ang) * (lens_r * 0.72f));
        dl->AddLine(lens_c, sweep_tip, UIConfig::Color::Accent, 1.8f);
    }

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
        renderScanningState(dl, p_min, p_max, center);
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

namespace {

// 绘制真·GPU硬件双线性顶点插值平滑太阳径向渐变光球 (0 色阶、0 硬边、完全平滑消隐)
void drawSmoothSolarSphere(ImDrawList* dl, ImVec2 center, float pulse) {
    constexpr int NUM_SEGS = 40;
    const float angle_step = 6.283185307f / (float)NUM_SEGS;

    // 预计算单位圆顶点 (0.90f 配合整体画面的微透视轻微压扁)
    ImVec2 unit_circle[NUM_SEGS];
    for (int i = 0; i < NUM_SEGS; ++i) {
        float ang = (float)i * angle_step;
        unit_circle[i] = ImVec2(std::cos(ang), std::sin(ang) * 0.90f);
    }

    struct GradientStop {
        float r;
        ImU32 col;
    };

    // 渐变停靠点阶梯：由内向外连续过渡，边缘平滑渐隐至 0 Alpha，完美复刻真实太阳光晕
    const GradientStop stops[] = {
        { 0.0f,  IM_COL32(255, 255, 255, 255) }, // 极热中心
        { 3.5f,  IM_COL32(255, 255, 255, 255) }, // 白炽核
        { 7.0f,  IM_COL32(255, 160, 180, 245) }, // 高温暖白-浅粉过渡
        { 11.5f, IM_COL32(250, 45, 72, 215) },  // 核心色球层 (主题玫瑰红)
        { 16.5f, IM_COL32(250, 45, 72, 110) },  // 辐射过渡层
        { 22.0f, IM_COL32(250, 45, 72, 0) }    // 边缘完全融入背景的 0 Alpha 消隐层
    };
    constexpr int NUM_STOPS = sizeof(stops) / sizeof(stops[0]);

    // 1. 中心圆盘扇区 (0.0 到 stops[1].r)
    dl->PrimReserve(NUM_SEGS * 3, NUM_SEGS + 1);
    ImDrawIdx center_idx = (ImDrawIdx)dl->_VtxCurrentIdx;
    dl->PrimWriteVtx(center, ImVec2(0.5f, 0.5f), stops[0].col);
    float r1 = stops[1].r * pulse;
    for (int i = 0; i < NUM_SEGS; ++i) {
        ImVec2 p(center.x + unit_circle[i].x * r1, center.y + unit_circle[i].y * r1);
        dl->PrimWriteVtx(p, ImVec2(0.5f, 0.5f), stops[1].col);

        dl->PrimWriteIdx(center_idx);
        dl->PrimWriteIdx((ImDrawIdx)(center_idx + 1 + i));
        dl->PrimWriteIdx((ImDrawIdx)(center_idx + 1 + ((i + 1) % NUM_SEGS)));
    }

    // 2. 连续平滑环带 (GPU 硬件顶点色双线性连续插值，无任何分块色带感)
    for (int s = 1; s < NUM_STOPS - 1; ++s) {
        float inner_r = stops[s].r * pulse;
        float outer_r = stops[s + 1].r * pulse;
        ImU32 inner_col = stops[s].col;
        ImU32 outer_col = stops[s + 1].col;

        dl->PrimReserve(NUM_SEGS * 6, NUM_SEGS * 2);
        ImDrawIdx base_idx = (ImDrawIdx)dl->_VtxCurrentIdx;

        for (int i = 0; i < NUM_SEGS; ++i) {
            ImVec2 p_in(center.x + unit_circle[i].x * inner_r, center.y + unit_circle[i].y * inner_r);
            dl->PrimWriteVtx(p_in, ImVec2(0.5f, 0.5f), inner_col);
        }
        for (int i = 0; i < NUM_SEGS; ++i) {
            ImVec2 p_out(center.x + unit_circle[i].x * outer_r, center.y + unit_circle[i].y * outer_r);
            dl->PrimWriteVtx(p_out, ImVec2(0.5f, 0.5f), outer_col);
        }

        for (int i = 0; i < NUM_SEGS; ++i) {
            int next_i = (i + 1) % NUM_SEGS;
            ImDrawIdx in0 = (ImDrawIdx)(base_idx + i);
            ImDrawIdx in1 = (ImDrawIdx)(base_idx + next_i);
            ImDrawIdx out0 = (ImDrawIdx)(base_idx + NUM_SEGS + i);
            ImDrawIdx out1 = (ImDrawIdx)(base_idx + NUM_SEGS + next_i);

            // 构成四边形的 2 个三角形
            dl->PrimWriteIdx(in0);
            dl->PrimWriteIdx(in1);
            dl->PrimWriteIdx(out1);

            dl->PrimWriteIdx(in0);
            dl->PrimWriteIdx(out1);
            dl->PrimWriteIdx(out0);
        }
    }
}

} // anonymous namespace

void ScanMusicWidget::drawLaserWarpAnimation(ImDrawList* dl, ImVec2 emitter_pos, ImVec2 p_min, ImVec2 p_max) {
    // 裁剪在卡片矩形内部，防止满屏激光与粒子溢出主舞台卡片
    dl->PushClipRect(p_min, p_max, true);

    // 计算从发射中心到卡片最远顶角的物理距离，确保 100% 满屏无死角穿透至四角与边缘
    float max_dist = std::hypot(
        std::max(emitter_pos.x - p_min.x, p_max.x - emitter_pos.x),
        std::max(emitter_pos.y - p_min.y, p_max.y - emitter_pos.y)
    ) + 40.0f;

    // =========================================================================
    // 1. 密集发丝级径向星芒激光流 (360 束，严格保持原本的直线曲速跃迁动画路线)
    // =========================================================================
    constexpr int NUM_RAYS = 360;
    for (int i = 0; i < NUM_RAYS; ++i) {
        // 黄金分割角 (137.5°) 均匀环形铺展
        float angle = i * 2.3999632f;

        // 确定性随机相位与多频流动速度
        float phase = std::fmod((float)(i * 17 + 31) * 0.0137f, 1.0f);
        float speed = 0.18f + std::fmod((float)(i * 7) * 0.019f, 0.12f);
        float progress = std::fmod(anim_timer_ * speed + phase, 1.0f);

        // 连续长光束延伸 (原本的直尺径向发射轨迹)
        float r_start = 18.0f + progress * 50.0f;
        float r_end = std::min(max_dist, r_start + 80.0f + (progress * progress) * (max_dist - 80.0f));

        float x1 = emitter_pos.x + std::cos(angle) * r_start;
        float y1 = emitter_pos.y + std::sin(angle) * (r_start * 0.90f);
        float x2 = emitter_pos.x + std::cos(angle) * r_end;
        float y2 = emitter_pos.y + std::sin(angle) * (r_end * 0.90f);

        // 多层通透微光：大部分为细腻幽光 (Alpha 25~120)，少数高亮主光束 (Alpha 180~220)
        int alpha = static_cast<int>(25.0f + progress * 95.0f);
        if (i % 8 == 0) {
            alpha = std::min(225, alpha + 90);
        }

        // 1.0px 发丝级细腻线条
        dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(250, 45, 72, alpha), 1.0f);

        // 高亮主光束核心叠加入射白炽微光
        if (alpha > 150) {
            ImVec2 mid(x1 + (x2 - x1) * 0.5f, y1 + (y2 - y1) * 0.5f);
            dl->AddLine(mid, ImVec2(x2, y2), IM_COL32(255, 210, 225, static_cast<int>(alpha * 0.65f)), 1.0f);
        }
    }

    // =========================================================================
    // 2. 漫天星尘与闪烁十字星芒粒子系统 (240 颗，严格保持原本的径向飞跃路线)
    // =========================================================================
    constexpr int NUM_PARTICLES = 240;
    for (int j = 0; j < NUM_PARTICLES; ++j) {
        float p_angle = (float)(j * 2.3999632f) + std::fmod((float)(j * 13) * 0.021f, 0.25f);
        float p_phase = std::fmod((float)(j * 29 + 11) * 0.0173f, 1.0f);
        float p_speed = 0.16f + std::fmod((float)(j * 11) * 0.013f, 0.10f);
        float p_prog = std::fmod(anim_timer_ * p_speed + p_phase, 1.0f);

        float p_curve = p_prog * p_prog; // 二次加速，呈现远小近大的空间透视
        float p_r = 16.0f + p_curve * (max_dist - 16.0f);

        float px = emitter_pos.x + std::cos(p_angle) * p_r;
        float py = emitter_pos.y + std::sin(p_angle) * (p_r * 0.90f);

        // 渐入渐出透明度包络
        float fade = (p_prog < 0.12f) ? (p_prog / 0.12f) : (1.0f - p_curve * 0.65f);
        int p_alpha = std::clamp(static_cast<int>(fade * 255.0f), 0, 255);
        float p_size = 0.8f + p_curve * 2.2f;

        int p_type = j % 12;
        if (p_type < 7) {
            // (1) 普通深空微光星尘：细微点缀
            dl->AddCircleFilled(ImVec2(px, py), p_size, IM_COL32(255, 195, 210, p_alpha), 10);
        } else if (p_type < 10) {
            // (2) 带有主题色柔和光晕的恒星粒子：双层微发光
            float halo_r = p_size * 2.6f;
            dl->AddCircleFilled(ImVec2(px, py), halo_r, IM_COL32(250, 45, 72, static_cast<int>(p_alpha * 0.35f)), 16);
            dl->AddCircleFilled(ImVec2(px, py), p_size, IM_COL32(255, 255, 255, p_alpha), 12);
        } else {
            // (3) 星球大战原版同款 4 芒十字衍射星芒 (Cross Star Flares)
            float flare_len = 4.0f + p_curve * 10.0f;
            dl->AddCircleFilled(ImVec2(px, py), p_size * 2.0f, IM_COL32(250, 45, 72, static_cast<int>(p_alpha * 0.45f)), 16);
            dl->AddLine(ImVec2(px - flare_len, py), ImVec2(px + flare_len, py),
                        IM_COL32(255, 235, 245, static_cast<int>(p_alpha * 0.85f)), 1.0f);
            dl->AddLine(ImVec2(px, py - flare_len), ImVec2(px, py + flare_len),
                        IM_COL32(255, 235, 245, static_cast<int>(p_alpha * 0.85f)), 1.0f);
            dl->AddCircleFilled(ImVec2(px, py), 1.5f, IM_COL32(255, 255, 255, 255), 8);
        }
    }

    // =========================================================================
    // 3. 核心太阳光球 (纯 GPU 硬件顶点色连续插值渐变，0 色阶硬边，与深空自然融合)
    // =========================================================================
    float pulse = 1.0f + 0.03f * std::sin(anim_timer_ * 2.5f);
    drawSmoothSolarSphere(dl, emitter_pos, pulse);

    dl->PopClipRect();
}

void ScanMusicWidget::renderScanningState(ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImVec2 center) {
    // 根据用户明确指示：搜索中除了背景动画，其他的全部不要（包括放大镜、状态文字、计数、终止按钮），连终止也不需要
    // 呈现完全纯净、满屏通透沉浸的太阳日冕等离子背景动画
    drawLaserWarpAnimation(dl, center, p_min, p_max);
}

void ScanMusicWidget::renderCompletedState(ImDrawList* dl, 
                                          [[maybe_unused]] ImVec2 p_min, 
                                          [[maybe_unused]] ImVec2 p_max, 
                                          std::vector<Playlist>& playlists) {
    // 居中呈现扫描完成仪表盘，避免页面突兀黑屏空白
    ImVec2 center(
        (p_min.x + p_max.x) * 0.5f,
        (p_min.y + p_max.y) * 0.5f
    );

    auto& scanner = MusicScanManager::getInstance();
    auto scanned_tracks = scanner.getScannedTracks();
    size_t total_found = scanned_tracks.size();

    // 1. 成功徽标圆圈 (绿色微光质感)
    ImVec2 badge_c(center.x, center.y - 70.0f);
    dl->AddCircleFilled(badge_c, 32.0f, IM_COL32(52, 199, 89, 40), 32);
    dl->AddCircle(badge_c, 32.0f, IM_COL32(52, 199, 89, 200), 32, 2.0f);
    dl->AddText(ImVec2(badge_c.x - 9.0f, badge_c.y - 12.0f), IM_COL32(52, 199, 89, 255), "OK");

    // 2. 标题文字
    const char* title = "本地发烧曲库检索完成！";
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    ImVec2 t_sz = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(center.x - t_sz.x * 0.5f, center.y - 20.0f), UIConfig::Color::TextActive, title);
    if (Fonts::Medium) ImGui::PopFont();

    // 3. 统计副标题
    std::string summary = "共索引高规格音频: " + std::to_string(total_found) + " 首 · 支持 DSD / FLAC / WAV";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 s_sz = ImGui::CalcTextSize(summary.c_str());
    dl->AddText(ImVec2(center.x - s_sz.x * 0.5f, center.y + 16.0f), UIConfig::Color::TextNormal, summary.c_str());
    if (Fonts::Regular) ImGui::PopFont();

    // 4. 操作按钮 1：「导入曲库」
    const float btn_w = 150.0f;
    const float btn_h = 36.0f;
    const float btn_r = btn_h * 0.5f;

    ImVec2 b1_p0(center.x - btn_w - 12.0f, center.y + 60.0f);
    ImVec2 b1_p1(b1_p0.x + btn_w, b1_p0.y + btn_h);
    bool b1_hovered = ImGui::IsMouseHoveringRect(b1_p0, b1_p1);
    bool b1_clicked = b1_hovered && ImGui::IsMouseClicked(0);

    ImU32 b1_bg = b1_hovered ? UIConfig::Color::Accent : IM_COL32(250, 45, 72, 85);
    dl->AddRectFilled(b1_p0, b1_p1, b1_bg, btn_r);
    dl->AddRect(b1_p0, b1_p1, UIConfig::Color::GlassBorder, btn_r, 0, 1.0f);

    const char* b1_label = "📥 导入全部音频";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 b1_sz = ImGui::CalcTextSize(b1_label);
    dl->AddText(ImVec2(b1_p0.x + (btn_w - b1_sz.x) * 0.5f, b1_p0.y + (btn_h - b1_sz.y) * 0.5f),
                UIConfig::Color::TextActive, b1_label);
    if (Fonts::Regular) ImGui::PopFont();

    if (b1_clicked && !playlists.empty()) {
        for (const auto& track : scanned_tracks) {
            playlists[0].addTrack(track);
        }
    }

    // 操作按钮 2：「重新检索」
    ImVec2 b2_p0(center.x + 12.0f, center.y + 60.0f);
    ImVec2 b2_p1(b2_p0.x + btn_w, b2_p0.y + btn_h);
    bool b2_hovered = ImGui::IsMouseHoveringRect(b2_p0, b2_p1);
    bool b2_clicked = b2_hovered && ImGui::IsMouseClicked(0);

    ImU32 b2_bg = b2_hovered ? IM_COL32(255, 255, 255, 30) : IM_COL32(255, 255, 255, 14);
    dl->AddRectFilled(b2_p0, b2_p1, b2_bg, btn_r);
    dl->AddRect(b2_p0, b2_p1, UIConfig::Color::GlassBorder, btn_r, 0, 1.0f);

    const char* b2_label = "🔄 重新全盘检索";
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    ImVec2 b2_sz = ImGui::CalcTextSize(b2_label);
    dl->AddText(ImVec2(b2_p0.x + (btn_w - b2_sz.x) * 0.5f, b2_p0.y + (btn_h - b2_sz.y) * 0.5f),
                UIConfig::Color::TextNormal, b2_label);
    if (Fonts::Regular) ImGui::PopFont();

    if (b2_clicked) {
        scanner.clear(); // 重置为 Idle 态，方便反复测试待机与扫描动效
    }
}