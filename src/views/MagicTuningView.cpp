#include "views/MagicTuningView.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "public/AppConfig.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/PlayerAdmin.hpp"
#include "themes/ThemeManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>

const std::array<MagicTuningView::ParamDef, MagicTuningView::NUM_PARAMS>& MagicTuningView::getParamDefs() {
    static const std::array<ParamDef, NUM_PARAMS> kDefs = {{
        { "声音冷暖", "Sound Temperature", "冷暖音色倾斜", "偏冷通透", "偏暖醇厚" },
        { "低音下潜", "Bass Extension",    "极低频下潜深度", "紧致轻盈",   "深沉澎湃" },
        { "低音质感", "Bass Texture",      "低频速度与弹性", "速度迅捷",   "蓬松弹性" },
        { "音符厚度", "Note Thickness",    "基频饱满厚实度", "纤细轻盈",   "扎实饱满" },
        { "人声位置", "Vocal Distance",    "人声结像远近感", "舞台后缩",   "贴耳靠前" },
        { "女声甜度", "Female Overtones",  "高频泛音润泽度", "自然平直",   "润泽甜美" },
        { "齿音消除", "Sibilance Control", "柔化唇齿毛刺音", "柔化去刺",   "原生锋芒" },
        { "冲激响应", "Impulse Response",  "瞬态与打击力度", "柔和松弛",   "硬朗凌厉" },
        { "空气感",   "Air & Treble",     "极高频空间泛音", "凝聚内敛",   "空灵弥漫" },
        { "声场重塑", "Soundstage Width",  "现场全景环绕感", "紧凑聚焦",   "宏大宽广" }
    }};
    return kDefs;
}

static MagicTuningView* s_magic_instance = nullptr;

MagicTuningView* MagicTuningView::getInstance() {
    return s_magic_instance;
}

MagicTuningView::MagicTuningView() {
    s_magic_instance = this;
    loadConfig();
}

MagicTuningView::~MagicTuningView() {
    if (s_magic_instance == this) {
        s_magic_instance = nullptr;
    }
}

void MagicTuningView::setEnabled(bool enabled) {
    if (is_enabled_ != enabled) {
        is_enabled_ = enabled;
        syncToAudioEngine();
        saveConfig();
    }
}

void MagicTuningView::setParamValue(size_t index, float val) {
    if (index < NUM_PARAMS) {
        float clamped = std::clamp(val, -10.0f, 10.0f);
        if (std::abs(clamped - params_[index]) > 0.01f) {
            params_[index] = clamped;
            syncToAudioEngine();
            saveConfig();
        }
    }
}

float MagicTuningView::getParamValue(size_t index) const {
    if (index < NUM_PARAMS) return params_[index];
    return 0.0f;
}

void MagicTuningView::resetAll() {
    params_.fill(0.0f);
    syncToAudioEngine();
    saveConfig();
}

void MagicTuningView::resetParam(size_t index) {
    if (index < NUM_PARAMS) {
        params_[index] = 0.0f;
        syncToAudioEngine();
        saveConfig();
    }
}

void MagicTuningView::syncToAudioEngine() {
    auto& player = PlayerAdmin::getInstance();

    if (!is_enabled_) {
        // 旁路时恢复纯平坦无音染
        player.setEqBands({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
        return;
    }

    // 10 频段映射表: 31Hz, 62Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz
    std::array<float, 10> gains = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // 1. 声音冷暖 (Sound Temperature): 以 1kHz 为支点全局频谱倾斜
    float temp = params_[0];
    gains[0] += temp * 0.35f;
    gains[1] += temp * 0.30f;
    gains[2] += temp * 0.20f;
    gains[3] += temp * 0.10f;
    gains[4] += temp * 0.05f;
    gains[5] -= temp * 0.05f;
    gains[6] -= temp * 0.10f;
    gains[7] -= temp * 0.20f;
    gains[8] -= temp * 0.30f;
    gains[9] -= temp * 0.35f;

    // 2. 低音下潜 (Bass Extension): 31Hz & 62Hz 极低频
    float bass_ext = params_[1];
    gains[0] += bass_ext * 0.65f;
    gains[1] += bass_ext * 0.40f;

    // 3. 低音质感 (Bass Texture): 125Hz 弹跳感与阻尼
    float bass_tex = params_[2];
    gains[2] += bass_tex * 0.50f;

    // 4. 音符厚度 (Note Thickness): 250Hz & 500Hz 基音体量
    float thickness = params_[3];
    gains[3] += thickness * 0.45f;
    gains[4] += thickness * 0.35f;

    // 5. 人声位置 (Vocal Distance): 1kHz & 2kHz 结像远近
    float vocal = params_[4];
    gains[5] += vocal * 0.55f;
    gains[6] += vocal * 0.45f;

    // 6. 女声甜度 (Female Overtones): 4kHz 甜美泛音
    float female = params_[5];
    gains[7] += female * 0.55f;

    // 7. 齿音消除 (Sibilance Control): 8kHz 柔化去刺 (负值为去齿音，正值为强化唇齿音)
    float sib = params_[6];
    gains[8] += sib * 0.50f;

    // 8. 冲激响应 (Impulse Response): 250Hz & 4kHz 瞬态冲击
    float impulse = params_[7];
    gains[3] += impulse * 0.20f;
    gains[7] += impulse * 0.25f;

    // 9. 空气感 (Air & Treble): 16kHz 极高频泛音
    float air = params_[8];
    gains[9] += air * 0.65f;

    // 10. 声场开阔度 (Soundstage Width): 边缘微调
    float stage = params_[9];
    gains[0] += stage * 0.15f;
    gains[5] -= stage * 0.20f;
    gains[9] += stage * 0.35f;

    // 约束在标准发烧 EQ [-12.0dB, +12.0dB] 安全范围，防止数字削波破音
    for (int i = 0; i < 10; ++i) {
        gains[i] = std::clamp(gains[i], -12.0f, 12.0f);
    }

    player.setEqEnabled(true);
    player.setEqBands(gains);
}

void MagicTuningView::saveConfig() {
    std::string config_dir = AppConfig::Path::getConfigDir();
    std::error_code ec;
    if (!std::filesystem::exists(config_dir, ec)) {
        std::filesystem::create_directories(config_dir, ec);
    }
    std::string file_path = config_dir + "/mseb_settings.ini";
    std::ofstream ofs(file_path);
    if (!ofs.is_open()) return;

    ofs << "[MSEB]\n";
    ofs << "enabled=" << (is_enabled_ ? 1 : 0) << "\n";
    for (size_t i = 0; i < NUM_PARAMS; ++i) {
        ofs << "param_" << i << "=" << params_[i] << "\n";
    }
}

void MagicTuningView::loadConfig() {
    std::string file_path = AppConfig::Path::getConfigDir() + "/mseb_settings.ini";
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        params_.fill(0.0f);
        is_enabled_ = true;
        return;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);

        if (key == "enabled") {
            try { is_enabled_ = (std::stoi(val) != 0); } catch (...) {}
        } else if (key.rfind("param_", 0) == 0) {
            try {
                size_t p_idx = std::stoul(key.substr(6));
                if (p_idx < NUM_PARAMS) {
                    params_[p_idx] = std::clamp(std::stof(val), -10.0f, 10.0f);
                }
            } catch (...) {}
        }
    }
}

bool MagicTuningView::renderHorizontalSlider(ImDrawList* dl, ImVec2 track_min, ImVec2 track_max,
                                             float& val, float min_val, float max_val, const char* str_id) {
    float track_w = track_max.x - track_min.x;
    float track_h = track_max.y - track_min.y;
    float track_y = track_min.y + track_h * 0.5f;

    ImGui::SetCursorScreenPos(track_min);
    ImGui::InvisibleButton(str_id, ImVec2(track_w, track_h));

    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();
    bool changed = false;

    if (is_active) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float norm = std::clamp((mouse_x - track_min.x) / track_w, 0.0f, 1.0f);
        float new_val = min_val + norm * (max_val - min_val);
        // 原点吸附机制 (在 0 附近自动吸附至绝对零点)
        if (std::abs(new_val) < 0.25f) new_val = 0.0f;
        if (std::abs(new_val - val) > 0.02f) {
            val = new_val;
            changed = true;
        }
    } else if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float norm = std::clamp((mouse_x - track_min.x) / track_w, 0.0f, 1.0f);
        float new_val = min_val + norm * (max_val - min_val);
        if (std::abs(new_val) < 0.25f) new_val = 0.0f;
        val = new_val;
        changed = true;
    }

    // 双击快速复位
    if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        val = 0.0f;
        changed = true;
    }

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float norm = (val - min_val) / (max_val - min_val);
    norm = std::clamp(norm, 0.0f, 1.0f);

    float track_cx = track_min.x + track_w * 0.5f;
    float thumb_cx = track_min.x + norm * track_w;

    // 1. 底层暗色轨道线 (截图1风格深炭灰)
    dl->AddLine(ImVec2(track_min.x, track_y), ImVec2(track_max.x, track_y),
                IM_COL32(38, 45, 60, 220), 3.5f);

    // 2. 中心 0.0 原点刻度微标记
    dl->AddLine(ImVec2(track_cx, track_y - 5.5f), ImVec2(track_cx, track_y + 5.5f),
                IM_COL32(110, 125, 150, 180), 1.5f);

    // 3. 动态发光填充轨 (由中心向滑块延伸，呈现截图 1 的主题色高亮条)
    if (std::abs(val) > 0.05f) {
        float x_start = std::min(track_cx, thumb_cx);
        float x_end = std::max(track_cx, thumb_cx);
        dl->AddLine(ImVec2(x_start, track_y), ImVec2(x_end, track_y),
                    IM_COL32(ar, ag, ab, 65), 7.0f);
        dl->AddLine(ImVec2(x_start, track_y), ImVec2(x_end, track_y),
                    accent, 3.5f);
    }

    // 4. 截图 1 风格的椭圆胶囊药丸滑块旋钮 (Pill Thumb)
    float thumb_w = 26.0f;
    float thumb_h = 14.0f;
    ImVec2 thumb_min(thumb_cx - thumb_w * 0.5f, track_y - thumb_h * 0.5f);
    ImVec2 thumb_max(thumb_cx + thumb_w * 0.5f, track_y + thumb_h * 0.5f);

    // 滑块光晕外轮廓 (悬浮或拖拽时高亮)
    if (is_active || is_hovered || std::abs(val) > 0.1f) {
        dl->AddRect(ImVec2(thumb_min.x - 2.0f, thumb_min.y - 2.0f),
                    ImVec2(thumb_max.x + 2.0f, thumb_max.y + 2.0f),
                    IM_COL32(ar, ag, ab, is_active ? 110 : (is_hovered ? 75 : 45)),
                    8.5f, 0, 2.0f);
    }

    // 滑块主体填充
    ImU32 thumb_bg = (is_active || is_hovered) ? IM_COL32(46, 56, 78, 255) : IM_COL32(28, 35, 48, 250);
    dl->AddRectFilled(thumb_min, thumb_max, thumb_bg, 7.0f);

    // 滑块边框 (主题色或金属边)
    ImU32 thumb_border = (is_active || is_hovered || std::abs(val) > 0.1f) ? accent : IM_COL32(85, 100, 125, 180);
    dl->AddRect(thumb_min, thumb_max, thumb_border, 7.0f, 0, 1.4f);

    // 滑块上半部分微光反光
    dl->AddRectFilled(thumb_min, ImVec2(thumb_max.x, track_y),
                      IM_COL32(255, 255, 255, 25), 7.0f, ImDrawFlags_RoundCornersTop);

    // 滑块中心手柄横纹
    dl->AddLine(ImVec2(thumb_cx, track_y - 3.5f), ImVec2(thumb_cx, track_y + 3.5f),
                IM_COL32(255, 255, 255, 80), 1.2f);

    return changed;
}

float MagicTuningView::renderTuningGroupCard(ImDrawList* dl, size_t idx, float x, float y, float w) {
    const auto& def = getParamDefs()[idx];
    float card_h = 76.0f;
    ImVec2 card_min(x, y);
    ImVec2 card_max(x + w, y + card_h);

    // ==============================================================================
    // [底板材质] 严格遵照截图 2 的暗色磨砂微光玻璃卡片样色
    // ==============================================================================
    dl->AddRectFilled(card_min, card_max, IM_COL32(20, 24, 34, 180), 8.0f);
    dl->AddRect(card_min, card_max, IM_COL32(255, 255, 255, 18), 8.0f, 0, 1.0f);

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float cur_val = params_[idx];
    bool is_non_zero = (std::abs(cur_val) > 0.05f);

    // ==============================================================================
    // 1. --header (标题与状态徽标，符合用户“--header -----slider”布局要求)
    // ==============================================================================
    float head_y = y + 10.0f;
    float head_x = x + 16.0f;

    // [中文标题]
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(head_x, head_y), is_non_zero ? UIConfig::Color::TextActive : IM_COL32(220, 230, 245, 230), def.name_zh);
    float zh_w = ImGui::CalcTextSize(def.name_zh).x;
    if (Fonts::Regular) ImGui::PopFont();

    // [英文名称标签]
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(head_x + zh_w + 8.0f, head_y + 2.0f), IM_COL32(ar, ag, ab, 200), def.name_en);
    float en_w = ImGui::CalcTextSize(def.name_en).x;

    // [小描述文本]
    float desc_x = head_x + zh_w + en_w + 16.0f;
    dl->AddText(ImVec2(desc_x, head_y + 2.0f), UIConfig::Color::TextMuted, def.description);

    // [Header 右侧复位/状态徽章药丸]
    char status_buf[32];
    if (!is_non_zero) {
        std::snprintf(status_buf, sizeof(status_buf), "0.0 平衡");
    } else if (cur_val > 0.0f) {
        std::snprintf(status_buf, sizeof(status_buf), "+%.1f %s", cur_val, def.right_label);
    } else {
        std::snprintf(status_buf, sizeof(status_buf), "%.1f %s", cur_val, def.left_label);
    }

    ImVec2 badge_sz = ImGui::CalcTextSize(status_buf);
    float badge_w = badge_sz.x + 16.0f;
    float badge_h = 20.0f;
    float badge_x = x + w - badge_w - 16.0f;
    float badge_y = head_y - 1.0f;

    ImVec2 b_min(badge_x, badge_y);
    ImVec2 b_max(badge_x + badge_w, badge_y + badge_h);

    std::string reset_btn_id = "##ResetPill_" + std::to_string(idx);
    ImGui::SetCursorScreenPos(b_min);
    ImGui::InvisibleButton(reset_btn_id.c_str(), ImVec2(badge_w, badge_h));
    bool badge_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        resetParam(idx);
    }

    ImU32 b_bg = is_non_zero ? IM_COL32(ar, ag, ab, badge_hov ? 80 : 45) :
                 (badge_hov ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12));
    dl->AddRectFilled(b_min, b_max, b_bg, 4.0f);
    dl->AddRect(b_min, b_max, is_non_zero ? accent : IM_COL32(255, 255, 255, 30), 4.0f, 0, 1.0f);

    const char* show_txt = badge_hov ? "点击复位" : status_buf;
    ImVec2 st_sz = ImGui::CalcTextSize(show_txt);
    float txt_x = badge_x + (badge_w - st_sz.x) * 0.5f;
    float txt_y = badge_y + (badge_h - st_sz.y) * 0.5f;
    dl->AddText(ImVec2(txt_x, txt_y), is_non_zero ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted, show_txt);
    if (Fonts::Small) ImGui::PopFont();

    // ==============================================================================
    // 2. -----slider (左侧特性标签 + 中间高精度滑轨 + 右侧特性标签 + 数值)
    // ==============================================================================
    float slider_row_y = y + 42.0f;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    // [左端标签]
    ImU32 left_col = (cur_val < -0.1f) ? accent : UIConfig::Color::TextMuted;
    float tag_left_x = x + 20.0f;
    dl->AddText(ImVec2(tag_left_x, slider_row_y + 3.0f), left_col, def.left_label);
    float left_label_w = ImGui::CalcTextSize(def.left_label).x;

    // [右端数值与右侧标签]
    char val_buf[16];
    if (std::abs(cur_val) < 0.05f) {
        std::snprintf(val_buf, sizeof(val_buf), "0.0");
    } else if (cur_val > 0.0f) {
        std::snprintf(val_buf, sizeof(val_buf), "+%.1f", cur_val);
    } else {
        std::snprintf(val_buf, sizeof(val_buf), "%.1f", cur_val);
    }

    ImVec2 val_sz = ImGui::CalcTextSize(val_buf);
    float val_x = x + w - val_sz.x - 20.0f;
    ImU32 val_col = is_non_zero ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted;
    dl->AddText(ImVec2(val_x, slider_row_y + 3.0f), val_col, val_buf);

    ImU32 right_col = (cur_val > 0.1f) ? accent : UIConfig::Color::TextMuted;
    float right_label_w = ImGui::CalcTextSize(def.right_label).x;
    float tag_right_x = val_x - right_label_w - 18.0f;
    dl->AddText(ImVec2(tag_right_x, slider_row_y + 3.0f), right_col, def.right_label);

    if (Fonts::Small) ImGui::PopFont();

    // [中间高精度水平滑轨]
    float track_x0 = tag_left_x + left_label_w + 18.0f;
    float track_x1 = tag_right_x - 18.0f;
    float track_h = 24.0f;

    std::string slider_id = "##MagicSlider_" + std::to_string(idx);
    float v = params_[idx];
    if (renderHorizontalSlider(dl, ImVec2(track_x0, slider_row_y), ImVec2(track_x1, slider_row_y + track_h),
                               v, -10.0f, +10.0f, slider_id.c_str())) {
        params_[idx] = v;
        syncToAudioEngine();
        saveConfig();
    }

    return card_h;
}

void MagicTuningView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX;
    float margin_y = UIConfig::Layout::ContainerMarginY;
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - UIConfig::Layout::BottomBarOffset);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 顶级深空高密度毛玻璃大底板
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "magic_tuning_stage");

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const uint32_t ar = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const uint32_t ag = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const uint32_t ab = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 2. 页面顶层标题与操作栏 (一键复位 & 总开关，无副标题)
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 14.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "调音魔棒");
    if (Fonts::Medium) ImGui::PopFont();

    // 顶部右侧「调音总开关」与「一键复位」
    float top_btn_h = 24.0f;
    float top_btn_y = card_min.y + 14.0f;

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);

    // 按钮 1: 一键全部复位
    const char* reset_all_txt = "一键全复位";
    float r_btn_w = ImGui::CalcTextSize(reset_all_txt).x + 18.0f;
    float r_btn_x = card_max.x - 20.0f - r_btn_w;
    ImVec2 r_min(r_btn_x, top_btn_y);
    ImVec2 r_max(r_btn_x + r_btn_w, top_btn_y + top_btn_h);

    ImGui::SetCursorScreenPos(r_min);
    ImGui::InvisibleButton("##ResetAllMSEB", ImVec2(r_btn_w, top_btn_h));
    bool r_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        resetAll();
    }
    dl->AddRectFilled(r_min, r_max, r_hov ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12), 4.0f);
    dl->AddRect(r_min, r_max, r_hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 25), 4.0f, 0, 1.0f);
    dl->AddText(ImVec2(r_btn_x + 9.0f, top_btn_y + 4.0f), UIConfig::Color::TextNormal, reset_all_txt);

    // 按钮 2: 调音总开关
    const char* toggle_txt = is_enabled_ ? "魔棒调音 · 已开启" : "魔棒调音 · 已旁路";
    float t_btn_w = ImGui::CalcTextSize(toggle_txt).x + 20.0f;
    float t_btn_x = r_btn_x - 12.0f - t_btn_w;
    ImVec2 t_min(t_btn_x, top_btn_y);
    ImVec2 t_max(t_btn_x + t_btn_w, top_btn_y + top_btn_h);

    ImGui::SetCursorScreenPos(t_min);
    ImGui::InvisibleButton("##ToggleMSEB", ImVec2(t_btn_w, top_btn_h));
    bool t_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        setEnabled(!is_enabled_);
    }

    ImU32 t_bg = is_enabled_ ? IM_COL32(ar, ag, ab, t_hov ? 80 : 50) :
                               (t_hov ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 10));
    dl->AddRectFilled(t_min, t_max, t_bg, 4.0f);
    dl->AddRect(t_min, t_max, is_enabled_ ? accent : IM_COL32(255, 255, 255, 30), 4.0f, 0, 1.0f);
    dl->AddText(ImVec2(t_btn_x + 10.0f, top_btn_y + 4.0f), is_enabled_ ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted, toggle_txt);

    if (Fonts::Small) ImGui::PopFont();

    // 3. 独立平滑滚动区域：去除右侧生硬滚动条 (NoScrollbar)，支持全屏鼠标滚轮自由滚动
    float scroll_y0 = card_min.y + 48.0f;
    float scroll_h = card_max.y - scroll_y0 - 10.0f;
    float scroll_w = card_max.x - card_min.x - 32.0f;

    ImGui::SetCursorScreenPos(ImVec2(card_min.x + 16.0f, scroll_y0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(0, 0, 0, 0));

    if (ImGui::BeginChild("##MagicTuningScroll", ImVec2(scroll_w, scroll_h), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        float cur_card_y = ImGui::GetCursorScreenPos().y;

        for (size_t i = 0; i < NUM_PARAMS; ++i) {
            float ch = renderTuningGroupCard(cdl, i, card_min.x + 16.0f, cur_card_y, scroll_w);
            cur_card_y += ch + 8.0f;
        }

        ImGui::SetCursorScreenPos(ImVec2(card_min.x + 16.0f, cur_card_y));
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}
