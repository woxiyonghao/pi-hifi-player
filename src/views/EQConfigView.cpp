#include "views/EQConfigView.hpp"
#include "public/AppConfig.hpp"
#include "public/Font.hpp"
#include "public/Platform.hpp"
#include "public/UIConfig.hpp"
#include "tools/PlayerAdmin.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

const float EQConfigView::BAND_FREQUENCIES[EQConfigView::NUM_BANDS] = {
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

const char* const EQConfigView::BAND_LABELS[EQConfigView::NUM_BANDS] = {
    "31Hz", "62Hz", "125Hz", "250Hz", "500Hz", "1kHz", "2kHz", "4kHz", "8kHz", "16kHz"
};

EQConfigView::EQConfigView() {
    // 预装 8 套发烧经典调音预设
    presets_ = {
        { "原音直通", { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
        { "流行音乐", { 2.5f, 1.5f, 0.0f, -1.0f, -1.5f, 1.0f, 2.5f, 3.5f, 2.5f, 1.5f } },
        { "摇滚现场", { 4.5f, 3.5f, 2.0f, 0.0f, -1.0f, 1.5f, 3.5f, 4.0f, 4.5f, 4.0f } },
        { "经典人声", { -2.0f, -1.0f, 0.0f, 2.0f, 3.5f, 4.0f, 3.0f, 1.5f, 0.0f, -1.0f } },
        { "古典交响", { 3.5f, 3.0f, 2.0f, 1.0f, -1.0f, 0.0f, 1.5f, 2.5f, 3.5f, 3.5f } },
        { "震撼低音", { 6.0f, 5.0f, 3.5f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
        { "通透高音", { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.5f, 4.5f, 6.0f, 6.0f } },
        { "自定义",   { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f } }
    };

    // 从磁盘持久化加载上次保存的 EQ 配置与自定义微调数值
    loadConfig();
}

// 静态推子交互状态与区域缓存 (供移动端触控手势感知，杜绝垂直推子被误判为列表滚动而释放按键)
static bool s_is_any_slider_active = false;
static uint64_t s_slider_render_frame_count = 0;
static float s_slider_area_min_x = 0.0f;
static float s_slider_area_min_y = 0.0f;
static float s_slider_area_max_x = 0.0f;
static float s_slider_area_max_y = 0.0f;

bool EQConfigView::isSliderTouch(float x, float y) {
    if (s_is_any_slider_active) return true;
    if (ImGui::GetCurrentContext() == nullptr) return false;
    int cur_frame = ImGui::GetFrameCount();
    if (static_cast<uint64_t>(cur_frame) > s_slider_render_frame_count + 1) {
        return false;
    }
    return (x >= s_slider_area_min_x && x <= s_slider_area_max_x &&
            y >= s_slider_area_min_y && y <= s_slider_area_max_y);
}

bool EQConfigView::isAnySliderActive() {
    return s_is_any_slider_active;
}

void EQConfigView::setSliderActive(bool active) {
    s_is_any_slider_active = active;
}

void EQConfigView::setEnabled(bool enabled) {
    is_enabled_ = enabled;
    syncToAudioEngine();
    saveConfig();
}

void EQConfigView::setBandGain(size_t index, float gain_db) {
    if (index < NUM_BANDS) {
        band_gains_[index] = std::clamp(gain_db, -12.0f, 12.0f);
        current_preset_idx_ = static_cast<int>(presets_.size() - 1); // 自动标记为自定义
        presets_[current_preset_idx_].gains = band_gains_;
        // 用户调节推子时，自动开启 EQ 激活态，确保曲线立即点亮发光
        if (!is_enabled_) {
            is_enabled_ = true;
        }
        syncToAudioEngine();
        saveConfig();
    }
}

float EQConfigView::getBandGain(size_t index) const {
    if (index < NUM_BANDS) {
        return band_gains_[index];
    }
    return 0.0f;
}

void EQConfigView::applyPreset(size_t preset_index) {
    if (preset_index < presets_.size()) {
        current_preset_idx_ = static_cast<int>(preset_index);
        band_gains_ = presets_[preset_index].gains;
        // 选择预设方案自动激活 EQ（若选择第 0 项原音直通则转为直通）
        if (preset_index == 0) {
            is_enabled_ = false;
        } else {
            is_enabled_ = true;
        }
        syncToAudioEngine();
        saveConfig();
    }
}

void EQConfigView::resetToFlat() {
    applyPreset(0);
    presets_.back().gains.fill(0.0f);
    is_enabled_ = false; // 复原重置自动转为 Direct 直通
    saveConfig();
}

void EQConfigView::syncToAudioEngine() {
    auto& player = PlayerAdmin::getInstance();
    player.setEqEnabled(is_enabled_);
    player.setEqBands(band_gains_);
}

void EQConfigView::saveConfig() {
    std::string config_dir = AppConfig::Path::getConfigDir();
    std::error_code ec;
    if (!std::filesystem::exists(config_dir, ec)) {
        std::filesystem::create_directories(config_dir, ec);
    }
    std::string file_path = config_dir + "/eq_settings.ini";
    std::ofstream ofs(file_path);
    if (!ofs.is_open()) return;

    ofs << "[EQ]\n";
    ofs << "enabled=" << (is_enabled_ ? 1 : 0) << "\n";
    ofs << "preset=" << current_preset_idx_ << "\n";

    // 保存自定义微调增益
    ofs << "custom_gains=";
    const auto& custom_g = presets_.back().gains;
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        ofs << custom_g[i] << (i + 1 < NUM_BANDS ? "," : "\n");
    }

    // 保存当前各频段实时增益
    ofs << "active_gains=";
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        ofs << band_gains_[i] << (i + 1 < NUM_BANDS ? "," : "\n");
    }
}

void EQConfigView::loadConfig() {
    std::string file_path = AppConfig::Path::getConfigDir() + "/eq_settings.ini";
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        // 首启默认初始化
        auto& player = PlayerAdmin::getInstance();
        is_enabled_ = player.isEqEnabled();
        band_gains_ = player.getEqBands();
        return;
    }

    std::string line;
    bool found_active = false;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        if (key == "enabled") {
            try { is_enabled_ = (std::stoi(val) != 0); } catch (...) {}
        } else if (key == "preset") {
            try {
                int p = std::stoi(val);
                if (p >= 0 && static_cast<size_t>(p) < presets_.size()) {
                    current_preset_idx_ = p;
                }
            } catch (...) {}
        } else if (key == "custom_gains") {
            std::stringstream ss(val);
            std::string item;
            size_t idx = 0;
            while (std::getline(ss, item, ',') && idx < NUM_BANDS) {
                try {
                    presets_.back().gains[idx] = std::stof(item);
                } catch (...) {}
                ++idx;
            }
        } else if (key == "active_gains") {
            std::stringstream ss(val);
            std::string item;
            size_t idx = 0;
            while (std::getline(ss, item, ',') && idx < NUM_BANDS) {
                try {
                    band_gains_[idx] = std::stof(item);
                } catch (...) {}
                ++idx;
            }
            found_active = true;
        }
    }

    if (current_preset_idx_ >= 0 && static_cast<size_t>(current_preset_idx_) < presets_.size() - 1) {
        band_gains_ = presets_[current_preset_idx_].gains;
    } else if (found_active) {
        presets_.back().gains = band_gains_;
    } else {
        band_gains_ = presets_.back().gains;
    }

    syncToAudioEngine();
}

// ==============================================================================
// 1. 顶部操作区 (EQ 开关 / Direct 直通与重置归零)
// ==============================================================================
void EQConfigView::renderTopActions(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max) {
    const bool is_iphone = Platform::isIPhone();
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const float top_y = is_iphone ? (card_min.y + 8.0f) : (card_min.y + 16.0f);
    const float btn_h = is_iphone ? 24.0f : 30.0f;

    // 1. 右侧 EQ 直通/启用胶囊开关
    const float toggle_w = is_iphone ? 98.0f : 120.0f;
    float toggle_x0 = card_max.x - 20.0f - toggle_w;
    ImVec2 toggle_min(toggle_x0, top_y);
    ImVec2 toggle_max(toggle_x0 + toggle_w, top_y + btn_h);

    ImGui::SetCursorScreenPos(toggle_min);
    bool clicked_toggle = ImGui::InvisibleButton("##eq_toggle_btn", ImVec2(toggle_w, btn_h));
    bool hov_toggle = ImGui::IsItemHovered();
    bool act_toggle = ImGui::IsItemActive();

    if (clicked_toggle) {
        setEnabled(!is_enabled_);
    }

    float radius = is_iphone ? 12.0f : 15.0f;
    float dot_x = is_iphone ? (toggle_min.x + 12.0f) : (toggle_min.x + 16.0f);
    float text_x = is_iphone ? (toggle_min.x + 22.0f) : (toggle_min.x + 28.0f);

    if (is_enabled_) {
        // 开启态：主题色流光胶囊底板 + 1px 折射边框
        ImU32 fill_col = (hov_toggle || act_toggle) ? IM_COL32(r, g, b, 75) : IM_COL32(r, g, b, 50);
        dl->AddRectFilled(toggle_min, toggle_max, fill_col, radius);
        dl->AddRect(toggle_min, toggle_max, IM_COL32(r, g, b, 200), radius, 0, 1.0f);

        // 绿色状态圆点
        dl->AddCircleFilled(ImVec2(dot_x, top_y + btn_h * 0.5f), is_iphone ? 3.0f : 4.0f, IM_COL32(76, 217, 100, 255));
        dl->AddCircleFilled(ImVec2(dot_x, top_y + btn_h * 0.5f), is_iphone ? 5.5f : 7.0f, IM_COL32(76, 217, 100, 50));

        // 文字
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 text_sz = ImGui::CalcTextSize("EQ 已激活");
        dl->AddText(ImVec2(text_x, top_y + (btn_h - text_sz.y) * 0.5f),
                    UIConfig::Color::TextActive, "EQ 已激活");
        if (Fonts::Small) ImGui::PopFont();
    } else {
        // 直通态 (Direct 100% 纯净源码直出)
        ImU32 fill_col = (hov_toggle || act_toggle) ? IM_COL32(255, 255, 255, 28) : IM_COL32(255, 255, 255, 14);
        dl->AddRectFilled(toggle_min, toggle_max, fill_col, radius);
        dl->AddRect(toggle_min, toggle_max, UIConfig::Color::GlassBorder, radius, 0, 1.0f);

        // 灰色状态圆点
        dl->AddCircle(ImVec2(dot_x, top_y + btn_h * 0.5f), is_iphone ? 3.0f : 4.0f, UIConfig::Color::TextMuted, 16, 1.2f);

        // 文字
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 text_sz = ImGui::CalcTextSize("Direct 直通");
        dl->AddText(ImVec2(text_x, top_y + (btn_h - text_sz.y) * 0.5f),
                    UIConfig::Color::TextMuted, "Direct 直通");
        if (Fonts::Small) ImGui::PopFont();
    }

    // 2. 复原重置按钮
    const float reset_w = is_iphone ? 46.0f : 58.0f;
    float reset_x0 = toggle_x0 - (is_iphone ? 8.0f : 10.0f) - reset_w;
    ImVec2 reset_min(reset_x0, top_y);
    ImVec2 reset_max(reset_x0 + reset_w, top_y + btn_h);

    ImGui::SetCursorScreenPos(reset_min);
    bool clicked_reset = ImGui::InvisibleButton("##eq_reset_btn", ImVec2(reset_w, btn_h));
    bool hov_reset = ImGui::IsItemHovered();
    bool act_reset = ImGui::IsItemActive();

    if (clicked_reset) {
        resetToFlat();
    }

    ImU32 reset_fill = (hov_reset || act_reset) ? IM_COL32(255, 255, 255, 26) : IM_COL32(255, 255, 255, 12);
    dl->AddRectFilled(reset_min, reset_max, reset_fill, radius);
    dl->AddRect(reset_min, reset_max, (hov_reset || act_reset) ? IM_COL32(r, g, b, 160) : UIConfig::Color::GlassBorder,
                radius, 0, 1.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 r_sz = ImGui::CalcTextSize("复原");
    dl->AddText(ImVec2(reset_min.x + (reset_w - r_sz.x) * 0.5f, top_y + (btn_h - r_sz.y) * 0.5f),
                (hov_reset || act_reset) ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted, "复原");
    if (Fonts::Small) ImGui::PopFont();
}

// ==============================================================================
// 2. 预设方案水平胶囊栏 (Preset Chips)
// ==============================================================================
void EQConfigView::renderPresetChips(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max) {
    const bool is_iphone = Platform::isIPhone();
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const float start_x = card_min.x + 20.0f;
    const float total_w = card_max.x - card_min.x - 40.0f;
    const float card_total_h = card_max.y - card_min.y;
    const float chip_y = is_iphone ? (card_min.y + (card_total_h > 400.0f ? 35.0f : 32.0f)) : (card_min.y + 70.0f);
    const float chip_h = is_iphone ? 22.0f : 26.0f;
    const float chip_round = is_iphone ? 11.0f : 13.0f;
    const size_t count = presets_.size();

    const float gap = is_iphone ? 4.0f : 7.0f;
    const float max_chip_w = is_iphone ? 90.0f : 115.0f;
    const float chip_w = std::min((total_w - gap * (count - 1)) / static_cast<float>(count), max_chip_w);

    static const char* const SHORT_PRESET_NAMES[8] = {
        "直通", "流行", "摇滚", "人声", "古典", "低音", "高音", "自定"
    };
    const bool use_short_name = (is_iphone && total_w < 380.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    for (size_t i = 0; i < count; ++i) {
        float x0 = start_x + i * (chip_w + gap);
        ImVec2 p0(x0, chip_y);
        ImVec2 p1(x0 + chip_w, chip_y + chip_h);

        ImGui::SetCursorScreenPos(p0);
        std::string btn_id = "##eq_chip_" + std::to_string(i);
        bool clicked = ImGui::InvisibleButton(btn_id.c_str(), ImVec2(chip_w, chip_h));
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();

        if (clicked) {
            applyPreset(i);
        }

        bool is_selected = (current_preset_idx_ == static_cast<int>(i));

        if (is_selected) {
            // 选中胶囊：主题色流体发光
            dl->AddRectFilled(p0, p1, IM_COL32(r, g, b, 65), chip_round);
            dl->AddRect(p0, p1, IM_COL32(r, g, b, 210), chip_round, 0, 1.0f);
            if (UIConfig::Animation::EnableGlow) {
                dl->AddRect(ImVec2(p0.x - 1.5f, p0.y - 1.5f), ImVec2(p1.x + 1.5f, p1.y + 1.5f),
                            IM_COL32(r, g, b, 45), chip_round + 1.5f, 0, 1.5f);
            }
        } else {
            // 普通胶囊：轻盈微透毛玻璃
            ImU32 fill_col = (hov || act) ? IM_COL32(255, 255, 255, 24) : IM_COL32(255, 255, 255, 12);
            dl->AddRectFilled(p0, p1, fill_col, chip_round);
            dl->AddRect(p0, p1, (hov || act) ? IM_COL32(r, g, b, 120) : UIConfig::Color::GlassBorder,
                        chip_round, 0, 1.0f);
        }

        ImU32 text_col = is_selected ? UIConfig::Color::TextActive :
                         (hov ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted);
        const char* disp_name = (use_short_name && i < 8) ? SHORT_PRESET_NAMES[i] : presets_[i].name.c_str();
        ImVec2 txt_sz = ImGui::CalcTextSize(disp_name);
        ImVec2 txt_pos(x0 + (chip_w - txt_sz.x) * 0.5f, chip_y + (chip_h - txt_sz.y) * 0.5f);
        dl->AddText(txt_pos, text_col, disp_name);
    }

    if (Fonts::Small) ImGui::PopFont();
}

// ==============================================================================
// 3. 动态频响拟合贝塞尔响应曲线视窗 (Curve Canvas)
// ==============================================================================
void EQConfigView::renderCurveCanvas(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max) {
    const bool is_iphone = Platform::isIPhone();
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const float canvas_x0 = card_min.x + 20.0f;
    const float canvas_x1 = card_max.x - 20.0f;
    const float card_total_h = card_max.y - card_min.y;

    // 动态黄金比例计算曲线视窗高度：
    // iPhone 竖屏 (card_total_h > 400px): ~92px；iPhone 横屏 (card_total_h < 350px): ~72px；普通平台: 138-240px
    const float canvas_h = is_iphone ? std::clamp(card_total_h * (card_total_h > 400.0f ? 0.17f : 0.25f), 72.0f, 96.0f)
                                     : std::clamp(card_total_h * 0.28f, 138.0f, 240.0f);
    const float y0 = is_iphone ? (card_min.y + (card_total_h > 400.0f ? 62.0f : 58.0f)) : (card_min.y + 104.0f);
    const float y1 = y0 + canvas_h;
    const float h = canvas_h;
    const float y_mid = (y0 + y1) * 0.5f;
    const float max_dev = h * 0.38f;

    // 1. 视窗容器毛玻璃底板
    dl->AddRectFilled(ImVec2(canvas_x0, y0), ImVec2(canvas_x1, y1), IM_COL32(12, 16, 24, 215), 8.0f);
    dl->AddRect(ImVec2(canvas_x0, y0), ImVec2(canvas_x1, y1), UIConfig::Color::GlassBorder, 8.0f, 0, 1.0f);

    // 左侧微型 dB 刻度标字
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    float db_txt_x = canvas_x0 + (is_iphone ? 4.0f : 8.0f);
    dl->AddText(ImVec2(db_txt_x, y_mid - max_dev - (is_iphone ? 5.0f : 6.0f)), IM_COL32(160, 175, 195, 150), "+12");
    dl->AddText(ImVec2(db_txt_x, y_mid - (is_iphone ? 5.0f : 6.0f)), IM_COL32(160, 175, 195, 190), " 0dB");
    dl->AddText(ImVec2(db_txt_x, y_mid + max_dev - (is_iphone ? 5.0f : 6.0f)), IM_COL32(160, 175, 195, 150), "-12");
    if (Fonts::Small) ImGui::PopFont();

    // 2. 计算 10 个频段的中心采样点坐标
    const float indent_x = 12.0f;
    const float x0 = canvas_x0 + indent_x;
    const float x1 = canvas_x1 - indent_x;
    const float total_w = x1 - x0;
    const float col_w = total_w / static_cast<float>(NUM_BANDS);
    ImVec2 pts[NUM_BANDS];

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        float cx = x0 + (static_cast<float>(i) + 0.5f) * col_w;
        // 始终呈现当前各频段增益真实频响，彻底杜绝调节后由于直通状态“不渲染曲线”的概率性断层
        float gain = band_gains_[i];
        float cy = y_mid - (gain / 12.0f) * max_dev;
        pts[i] = ImVec2(cx, cy);
    }

    // 3. 刻度基准参考线 (+12dB, 0dB, -12dB)
    const ImU32 col_grid = IM_COL32(255, 255, 255, 18);
    dl->AddLine(ImVec2(pts[0].x, y_mid - max_dev), ImVec2(pts[NUM_BANDS - 1].x, y_mid - max_dev), col_grid, 1.0f);
    dl->AddLine(ImVec2(pts[0].x, y_mid), ImVec2(pts[NUM_BANDS - 1].x, y_mid), IM_COL32(255, 255, 255, 38), 1.0f);
    dl->AddLine(ImVec2(pts[0].x, y_mid + max_dev), ImVec2(pts[NUM_BANDS - 1].x, y_mid + max_dev), col_grid, 1.0f);

    // 4. Catmull-Rom 三次样条插值生成高精细连续平滑频响曲线
    constexpr int SUBDIV = 8;
    std::vector<ImVec2> curve_pts;
    curve_pts.reserve((NUM_BANDS - 1) * SUBDIV + 1);

    for (size_t i = 0; i < NUM_BANDS - 1; ++i) {
        ImVec2 p0 = (i == 0) ? pts[0] : pts[i - 1];
        ImVec2 p1 = pts[i];
        ImVec2 p2 = pts[i + 1];
        ImVec2 p3 = (i + 2 < NUM_BANDS) ? pts[i + 2] : p2;

        for (int step = 0; step < SUBDIV; ++step) {
            float t = static_cast<float>(step) / static_cast<float>(SUBDIV);
            float t2 = t * t;
            float t3 = t2 * t;

            float x = 0.5f * ((2.0f * p1.x) +
                              (-p0.x + p2.x) * t +
                              (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                              (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
            float y = 0.5f * ((2.0f * p1.y) +
                              (-p0.y + p2.y) * t +
                              (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                              (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
            y = std::clamp(y, y0 + 2.0f, y1 - 2.0f);
            curve_pts.push_back(ImVec2(x, y));
        }
    }
    curve_pts.push_back(pts[NUM_BANDS - 1]);

    dl->PushClipRect(ImVec2(canvas_x0, y0), ImVec2(canvas_x1, y1), true);

    // 5. 曲线下方发光半透渐变填充 (Gradient Fill under curve)
    if (is_enabled_) {
        for (size_t i = 0; i < curve_pts.size() - 1; ++i) {
            ImVec2 c0 = curve_pts[i];
            ImVec2 c1 = curve_pts[i + 1];
            ImVec2 b0(c0.x, y_mid);
            ImVec2 b1(c1.x, y_mid);
            dl->AddQuadFilled(c0, c1, b1, b0, IM_COL32(r, g, b, 32));
        }
    }

    // 6. 曲线主体矢量高光描边 (带高斯微光光晕)
    ImU32 stroke_col = is_enabled_ ? accent : IM_COL32(160, 175, 195, 140);
    if (is_enabled_ && UIConfig::Animation::EnableGlow) {
        for (size_t i = 0; i < curve_pts.size() - 1; ++i) {
            dl->AddLine(curve_pts[i], curve_pts[i + 1], IM_COL32(r, g, b, 70), 4.5f);
        }
    }
    for (size_t i = 0; i < curve_pts.size() - 1; ++i) {
        dl->AddLine(curve_pts[i], curve_pts[i + 1], stroke_col, is_enabled_ ? 2.5f : 1.8f);
    }

    // 7. 10 个频段频点发光圆环节点
    for (size_t i = 0; i < NUM_BANDS; ++i) {
        if (is_enabled_) {
            dl->AddCircleFilled(pts[i], 4.5f, IM_COL32(r, g, b, 90));
            dl->AddCircleFilled(pts[i], 3.0f, accent);
            dl->AddCircleFilled(pts[i], 1.5f, IM_COL32(255, 255, 255, 255));
        } else {
            dl->AddCircleFilled(pts[i], 2.5f, IM_COL32(160, 175, 195, 160));
        }
    }

    dl->PopClipRect();
}

// ==============================================================================
// 4. 10 段发烧级图形推子控制区 (Fader Sliders)
// ==============================================================================
void EQConfigView::renderSliders(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max) {
    const bool is_iphone = Platform::isIPhone();
    const ImU32 accent = UIConfig::Color::Accent;
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const float indent_x = 12.0f;
    const float start_x = card_min.x + 20.0f + indent_x;
    const float total_w = card_max.x - card_min.x - 40.0f - indent_x * 2.0f;
    const float col_w = total_w / static_cast<float>(NUM_BANDS);

    const float card_total_h = card_max.y - card_min.y;
    const float canvas_h = is_iphone ? std::clamp(card_total_h * (card_total_h > 400.0f ? 0.17f : 0.25f), 72.0f, 96.0f)
                                     : std::clamp(card_total_h * 0.28f, 138.0f, 240.0f);
    const float curve_y0 = is_iphone ? (card_min.y + (card_total_h > 400.0f ? 62.0f : 58.0f)) : (card_min.y + 104.0f);
    const float y0 = is_iphone ? (curve_y0 + canvas_h + (card_total_h > 400.0f ? 8.0f : 6.0f))
                               : (card_min.y + 104.0f + canvas_h + 12.0f);
    const float y1 = is_iphone ? (card_max.y - 4.0f) : (card_max.y - 12.0f);

    const float label_top_h = is_iphone ? 14.0f : 24.0f;
    const float label_bot_h = is_iphone ? 14.0f : 24.0f;

    const float rail_top = is_iphone ? (y0 + label_top_h + 4.0f) : (y0 + label_top_h + 8.0f);
    const float rail_bot = is_iphone ? (y1 - label_bot_h - 4.0f) : (y1 - label_bot_h - 8.0f);
    const float rail_h = rail_bot - rail_top;
    const float rail_mid = (rail_top + rail_bot) * 0.5f;

    bool any_active = false;

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        float cx = start_x + (static_cast<float>(i) + 0.5f) * col_w;
        float gain = band_gains_[i];

        // ---------------------------------------------------------------------
        // 1. 推子全高舒适交互热区
        // ---------------------------------------------------------------------
        const float hit_w = is_iphone ? (col_w - 2.0f) : (col_w - 6.0f);
        const float hit_pad_y = is_iphone ? 12.0f : 10.0f;
        ImVec2 hit_min(cx - hit_w * 0.5f, rail_top - hit_pad_y);
        ImVec2 hit_max(cx + hit_w * 0.5f, rail_bot + hit_pad_y);

        ImGui::SetCursorScreenPos(hit_min);
        std::string slider_id = "##eq_fader_" + std::to_string(i);
        ImGui::InvisibleButton(slider_id.c_str(), ImVec2(hit_w, (rail_bot - rail_top) + hit_pad_y * 2.0f));

        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();
        if (act) {
            any_active = true;
        }

        // 鼠标点击或手指触控滑动实时调节 (无论往上拉还是往下拉均精准 1:1 跟手响应)
        if (act && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float mouse_y = ImGui::GetIO().MousePos.y;
            float norm = std::clamp((rail_mid - mouse_y) / (rail_h * 0.5f), -1.0f, 1.0f);
            float new_gain = std::round(norm * 12.0f * 10.0f) * 0.1f; // 0.1dB 晶振级细分
            setBandGain(i, new_gain);
            gain = band_gains_[i];
        }

        // 桌面端滚轮微调 (±0.5 dB，移动端禁用防止误触冲突)
        if (!is_iphone && hov && ImGui::GetIO().MouseWheel != 0.0f) {
            float new_gain = std::clamp(gain + ImGui::GetIO().MouseWheel * 0.5f, -12.0f, 12.0f);
            new_gain = std::round(new_gain * 10.0f) * 0.1f;
            setBandGain(i, new_gain);
            gain = band_gains_[i];
        }

        // 双击快速复位该频段为 0.0 dB
        if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            setBandGain(i, 0.0f);
            gain = 0.0f;
        }

        // ---------------------------------------------------------------------
        // 2. 顶部实时 dB 数值显示
        // ---------------------------------------------------------------------
        char db_buf[16];
        if (gain > 0.05f) {
            std::snprintf(db_buf, sizeof(db_buf), "+%.1f", gain);
        } else if (gain < -0.05f) {
            std::snprintf(db_buf, sizeof(db_buf), "%.1f", gain);
        } else {
            std::snprintf(db_buf, sizeof(db_buf), "0.0");
        }

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 db_sz = ImGui::CalcTextSize(db_buf);
        ImVec2 db_pos(cx - db_sz.x * 0.5f, is_iphone ? (y0 + 2.0f) : (y0 + 6.0f));

        ImU32 db_col = UIConfig::Color::TextMuted;
        if (is_enabled_) {
            if (act || hov) {
                db_col = accent;
            } else if (std::abs(gain) > 0.05f) {
                db_col = UIConfig::Color::TextActive;
            }
        }
        dl->AddText(db_pos, db_col, db_buf);
        if (Fonts::Small) ImGui::PopFont();

        // ---------------------------------------------------------------------
        // 3. 垂直推子轨道 (Vertical Rail Slot)
        // ---------------------------------------------------------------------
        const float track_w = 4.0f;
        ImVec2 tr_p0(cx - track_w * 0.5f, rail_top);
        ImVec2 tr_p1(cx + track_w * 0.5f, rail_bot);

        // 轨道暗槽
        dl->AddRectFilled(tr_p0, tr_p1, IM_COL32(255, 255, 255, 22), 2.0f);

        // 0 dB 中心基准参考线横向短刻度
        dl->AddLine(ImVec2(cx - 7.0f, rail_mid), ImVec2(cx + 7.0f, rail_mid),
                    IM_COL32(255, 255, 255, 55), 1.0f);

        // 计算当前滑块 Thumb 物理 Y 坐标
        float thumb_norm = std::clamp(gain / 12.0f, -1.0f, 1.0f);
        float thumb_y = rail_mid - thumb_norm * (rail_h * 0.5f);

        // ---------------------------------------------------------------------
        // 4. 动态发光填充柱 (从 0dB 向上/向下激发)
        // ---------------------------------------------------------------------
        if (is_enabled_ && std::abs(gain) > 0.05f) {
            float y_fill0 = std::min(rail_mid, thumb_y);
            float y_fill1 = std::max(rail_mid, thumb_y);

            // 主题色微光柱
            dl->AddRectFilled(ImVec2(cx - 2.0f, y_fill0), ImVec2(cx + 2.0f, y_fill1),
                              (hov || act) ? accent : IM_COL32(r, g, b, 200), 2.0f);

            if (UIConfig::Animation::EnableGlow) {
                dl->AddRectFilled(ImVec2(cx - 3.5f, y_fill0), ImVec2(cx + 3.5f, y_fill1),
                                  IM_COL32(r, g, b, 45), 3.0f);
            }
        }

        // ---------------------------------------------------------------------
        // 5. 推子物理手柄 (Tactile Slider Thumb)
        // ---------------------------------------------------------------------
        const float thumb_w = is_iphone ? ((hov || act) ? 24.0f : 22.0f) : ((hov || act) ? 30.0f : 28.0f);
        const float thumb_h = is_iphone ? ((hov || act) ? 14.0f : 12.0f) : ((hov || act) ? 14.0f : 12.0f);
        const float r_thumb = thumb_h * 0.5f; // 纯圆润平滑胶囊

        ImVec2 th_min(cx - thumb_w * 0.5f, thumb_y - thumb_h * 0.5f);
        ImVec2 th_max(cx + thumb_w * 0.5f, thumb_y + thumb_h * 0.5f);

        // 1. 软弥散环境投射阴影
        dl->AddRectFilled(ImVec2(th_min.x - 1.0f, th_min.y + 1.5f),
                          ImVec2(th_max.x + 1.0f, th_max.y + 3.5f),
                          IM_COL32(0, 0, 0, 75), r_thumb + 1.0f);

        // 2. 悬停/激活时主题色柔光 Bloom 环绕外发光
        if (hov || act) {
            dl->AddRectFilled(ImVec2(th_min.x - 3.0f, th_min.y - 3.0f),
                              ImVec2(th_max.x + 3.0f, th_max.y + 3.0f),
                              IM_COL32(r, g, b, 65), r_thumb + 3.0f);
        }

        // 3. 手柄主体
        ImU32 th_bg = (act || hov) ? accent : IM_COL32(32, 38, 50, 235);
        dl->AddRectFilled(th_min, th_max, th_bg, r_thumb);

        // 4. 上半部通透镜面漫反射微光 (Glass Sheen)
        ImU32 sheen_col = (act || hov) ? IM_COL32(255, 255, 255, 55) : IM_COL32(255, 255, 255, 22);
        dl->AddRectFilled(th_min, ImVec2(th_max.x, thumb_y), sheen_col, r_thumb);

        // ---------------------------------------------------------------------
        // 6. 底部频段标签 (e.g. 31Hz, 1kHz, 16kHz；紧凑宽度自适应为 31, 62, 125 防止字体重叠)
        // ---------------------------------------------------------------------
        static const char* const SHORT_BAND_LABELS[10] = {
            "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
        };
        const char* disp_freq = (is_iphone && col_w < 38.0f) ? SHORT_BAND_LABELS[i] : BAND_LABELS[i];

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 freq_sz = ImGui::CalcTextSize(disp_freq);
        ImVec2 freq_pos(cx - freq_sz.x * 0.5f, is_iphone ? (rail_bot + 4.0f) : (rail_bot + 12.0f));

        ImU32 freq_col = (hov || act) ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted;
        dl->AddText(freq_pos, freq_col, disp_freq);
        if (Fonts::Small) ImGui::PopFont();
    }

    // 记录推子交互状态与物理几何包围盒 (供触控手势防误判)
    s_is_any_slider_active = any_active;
    s_slider_area_min_x = start_x - 14.0f;
    s_slider_area_max_x = start_x + total_w + 14.0f;
    s_slider_area_min_y = rail_top - 20.0f;
    s_slider_area_max_y = rail_bot + 20.0f;
    if (ImGui::GetCurrentContext() != nullptr) {
        s_slider_render_frame_count = static_cast<uint64_t>(ImGui::GetFrameCount());
    }
}

// ==============================================================================
// 5. 顶层主渲染分发
// ==============================================================================
void EQConfigView::render(ImDrawList* dl, ImVec2 card_min, ImVec2 card_max) {
    // 1. 顶部操作区 (Direct 直通/启用切换、归零重置)
    renderTopActions(dl, card_min, card_max);

    // 2. 预设方案水平胶囊栏
    renderPresetChips(dl, card_min, card_max);

    // 3. 动态频响拟合贝塞尔响应曲线视窗
    renderCurveCanvas(dl, card_min, card_max);

    // 4. 10 段发烧级图形推子控制区
    renderSliders(dl, card_min, card_max);
}
