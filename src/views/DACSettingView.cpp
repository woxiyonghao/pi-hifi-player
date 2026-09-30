#include "views/DACSettingView.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/MusicDatabase.hpp"
#include "themes/ThemeManager.hpp"
#include "audio_engine/AudioEngine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

DACSettingView::DACSettingView() {
    loadSettings();
}

std::string DACSettingView::getCurrentChipName() const {
    switch (selected_chip_) {
        case 0: return "Apple Direct";
        case 1: return "Dual ES9038PRO";
        case 2: return "AK4499EX Velvet";
        case 3: return "CS43198 Master";
        case 4: return "R-2R Discrete";
        case 5: return "BD34301EKV";
        default: return "Apple Direct";
    }
}

void DACSettingView::setSelectedChip(int index) {
    int clamped = std::clamp(index, 0, 5);
    if (selected_chip_ != clamped) {
        selected_chip_ = clamped;
        saveSettings();
        if (on_dac_changed_) {
            on_dac_changed_(getCurrentDacType(), getCurrentDacName());
        }
    }
}

void DACSettingView::loadSettings() {
    auto& db = MusicDatabase::getInstance();

    // 0. 读取选中的芯片架构 (兼容 setting_dac_selected_chip 与 setting_dac_hardware_type)
    std::string s_chip = db.getSetting("setting_dac_selected_chip", "");
    if (s_chip.empty()) {
        s_chip = db.getSetting("setting_dac_hardware_type", "0");
    }
    try {
        selected_chip_ = std::clamp(std::stoi(s_chip), 0, 5);
    } catch (...) {
        selected_chip_ = 0;
    }

    // 1. Apple Direct 设置
    try {
        apple_exclusive_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_apple_exclusive", "0")), 0, 1);
        apple_sample_rate_ = std::clamp(std::stoi(db.getSetting("setting_dac_apple_sample_rate", "0")), 0, 2);
        apple_headphone_drive_ = std::clamp(std::stoi(db.getSetting("setting_dac_apple_drive", "0")), 0, 2);
        apple_bit_depth_ = std::clamp(std::stoi(db.getSetting("setting_dac_apple_bit_depth", "0")), 0, 1);
    } catch (...) {}
    audio_engine::AudioEngine::getInstance().setExclusiveMode(apple_exclusive_mode_ == 0);

    // 2. ESS Sabre 设置
    try {
        pcm_filter_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_pcm_filter", "0")), 0, 6);
        dsd_bypass_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_dsd_bypass", "0")), 0, 1);
        dsd_filter_cutoff_ = std::clamp(std::stoi(db.getSetting("setting_dac_dsd_cutoff", "0")), 0, 3);
        pcm_dpll_band_ = std::clamp(std::stoi(db.getSetting("setting_dac_pcm_dpll", "0")), 0, 2);
        dsd_dpll_band_ = std::clamp(std::stoi(db.getSetting("setting_dac_dsd_dpll", "0")), 0, 2);
        thd_comp_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_thd_comp", "0")), 0, 2);
        channel_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_mono_mode", "0")), 0, 1);
        output_level_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_output_mode", "0")), 0, 1);
        phase_invert_ = std::clamp(std::stoi(db.getSetting("setting_dac_phase", "0")), 0, 1);
    } catch (...) {}

    // 3. AKM Velvet Sound 设置
    try {
        akm_filter_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_akm_filter", "0")), 0, 5);
        akm_sound_color_ = std::clamp(std::stoi(db.getSetting("setting_dac_akm_color", "0")), 0, 3);
        akm_dsd_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_akm_dsd", "0")), 0, 1);
        akm_exdf_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_akm_exdf", "0")), 0, 1);
    } catch (...) {}

    // 4. Cirrus Logic 设置
    try {
        cs_filter_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_filter", "0")), 0, 4);
        cs_dsd_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_dsd", "0")), 0, 1);
        cs_drive_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_drive", "0")), 0, 1);
        cs_impedance_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_impedance", "0")), 0, 1);
    } catch (...) {}

    // 5. R-2R 纯电阻网络设置
    try {
        r2r_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_r2r_mode", "0")), 0, 2);
        r2r_dsd_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_r2r_dsd", "0")), 0, 1);
        r2r_clock_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_r2r_clock", "0")), 0, 1);
        r2r_phase_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_r2r_phase", "0")), 0, 1);
    } catch (...) {}

    // 6. ROHM 罗姆设置
    try {
        rohm_filter_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_rohm_filter", "0")), 0, 1);
        rohm_modulator_clock_ = std::clamp(std::stoi(db.getSetting("setting_dac_rohm_clock", "0")), 0, 2);
        rohm_dsd_path_ = std::clamp(std::stoi(db.getSetting("setting_dac_rohm_dsd", "0")), 0, 1);
    } catch (...) {}
}

void DACSettingView::saveSettings() {
    auto& db = MusicDatabase::getInstance();
    db.setSetting("setting_dac_selected_chip", std::to_string(selected_chip_));
    db.setSetting("setting_dac_hardware_type", std::to_string(selected_chip_));

    // Apple
    db.setSetting("setting_dac_apple_exclusive", std::to_string(apple_exclusive_mode_));
    db.setSetting("setting_dac_apple_sample_rate", std::to_string(apple_sample_rate_));
    db.setSetting("setting_dac_apple_drive", std::to_string(apple_headphone_drive_));
    db.setSetting("setting_dac_apple_bit_depth", std::to_string(apple_bit_depth_));
    
    bool exclusive = (apple_exclusive_mode_ == 0);
    audio_engine::AudioEngine::getInstance().setExclusiveMode(exclusive);
    if (exclusive) {
        uint32_t target_sr = 0;
        if (apple_sample_rate_ == 1) target_sr = 96000;
        else if (apple_sample_rate_ == 2) target_sr = 192000;
        else target_sr = audio_engine::AudioEngine::getInstance().getCurrentSpec().sample_rate;
        if (target_sr > 0) {
            audio_engine::AudioEngine::getInstance().applyHardwareSampleRate(target_sr);
        }
    }

    // ESS
    db.setSetting("setting_dac_pcm_filter", std::to_string(pcm_filter_mode_));
    db.setSetting("setting_dac_dsd_bypass", std::to_string(dsd_bypass_mode_));
    db.setSetting("setting_dac_dsd_cutoff", std::to_string(dsd_filter_cutoff_));
    db.setSetting("setting_dac_pcm_dpll", std::to_string(pcm_dpll_band_));
    db.setSetting("setting_dac_dsd_dpll", std::to_string(dsd_dpll_band_));
    db.setSetting("setting_dac_thd_comp", std::to_string(thd_comp_mode_));
    db.setSetting("setting_dac_mono_mode", std::to_string(channel_mode_));
    db.setSetting("setting_dac_output_mode", std::to_string(output_level_mode_));
    db.setSetting("setting_dac_phase", std::to_string(phase_invert_));

    // AKM
    db.setSetting("setting_dac_akm_filter", std::to_string(akm_filter_mode_));
    db.setSetting("setting_dac_akm_color", std::to_string(akm_sound_color_));
    db.setSetting("setting_dac_akm_dsd", std::to_string(akm_dsd_mode_));
    db.setSetting("setting_dac_akm_exdf", std::to_string(akm_exdf_mode_));

    // Cirrus
    db.setSetting("setting_dac_cs_filter", std::to_string(cs_filter_mode_));
    db.setSetting("setting_dac_cs_dsd", std::to_string(cs_dsd_mode_));
    db.setSetting("setting_dac_cs_drive", std::to_string(cs_drive_mode_));
    db.setSetting("setting_dac_cs_impedance", std::to_string(cs_impedance_mode_));

    // R-2R
    db.setSetting("setting_dac_r2r_mode", std::to_string(r2r_mode_));
    db.setSetting("setting_dac_r2r_dsd", std::to_string(r2r_dsd_mode_));
    db.setSetting("setting_dac_r2r_clock", std::to_string(r2r_clock_mode_));
    db.setSetting("setting_dac_r2r_phase", std::to_string(r2r_phase_mode_));

    // ROHM
    db.setSetting("setting_dac_rohm_filter", std::to_string(rohm_filter_mode_));
    db.setSetting("setting_dac_rohm_clock", std::to_string(rohm_modulator_clock_));
    db.setSetting("setting_dac_rohm_dsd", std::to_string(rohm_dsd_path_));
}

void DACSettingView::renderOptionRow(ImDrawList* dl, float x0, float row_y, float w,
                                     const char* label, const char* const options[], int count,
                                     int& current_val, const char* id_prefix) {
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, row_y + 4.0f), UIConfig::Color::TextMuted, label);
    if (Fonts::Small) ImGui::PopFont();

    float btn_start_x = x0 + 116.0f;
    float btn_gap = 8.0f;
    float total_avail_w = w - 116.0f - 16.0f;
    float btn_w = (total_avail_w - btn_gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    float btn_h = 26.0f;

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    for (int i = 0; i < count; ++i) {
        float bx0 = btn_start_x + static_cast<float>(i) * (btn_w + btn_gap);
        ImVec2 b_min(bx0, row_y);
        ImVec2 b_max(bx0 + btn_w, row_y + btn_h);

        bool is_act = (current_val == i);
        std::string btn_id = std::string("##") + id_prefix + "_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            current_val = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        // 文本精准测量与内边距保护，绝不溢出选项框
        dl->PushClipRect(ImVec2(b_min.x + 2.0f, b_min.y), ImVec2(b_max.x - 2.0f, b_max.y), true);
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(options[i]);
        float txt_x = bx0 + (btn_w - txt_sz.x) * 0.5f;
        if (txt_x < bx0 + 3.0f) txt_x = bx0 + 3.0f;
        float txt_y = row_y + (btn_h - txt_sz.y) * 0.5f;

        dl->AddText(ImVec2(txt_x, txt_y),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal,
                    options[i]);
        if (Fonts::Small) ImGui::PopFont();
        dl->PopClipRect();
    }
}

void DACSettingView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 渲染顶级深空高密度毛玻璃大底板
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "main_stage");

    // 2. 标题「DAC 设置」(严格无副标题，右上角无徽标，对齐用户指令)
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 14.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "DAC 设置");
    if (Fonts::Medium) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 3. 顶部芯片架构选项卡分段选择器 (Segmented Tab Bar)
    float chip_tab_y = card_min.y + 44.0f;
    float chip_tab_x = card_min.x + 16.0f;
    float chip_tab_w = card_max.x - card_min.x - 32.0f;
    float chip_tab_h = 30.0f;

    static const struct {
        const char* label;
        const char* subtitle;
    } chip_tabs[6] = {
        { "Apple 直通", "MacBook 硬件直通" },
        { "ES9038PRO", "ESS Sabre 旗舰并联" },
        { "AK4499EX", "AKM 旭化成 Velvet" },
        { "CS43198", "Cirrus Logic Master" },
        { "R-2R 纯电阻", "分立电阻 NOS/OS" },
        { "ROHM 34301", "MUS-IC BD34301" }
    };

    float tab_gap = 6.0f;
    float tab_w = (chip_tab_w - tab_gap * 5.0f) / 6.0f;

    for (int c = 0; c < 6; ++c) {
        float tx0 = chip_tab_x + static_cast<float>(c) * (tab_w + tab_gap);
        ImVec2 t_min(tx0, chip_tab_y);
        ImVec2 t_max(tx0 + tab_w, chip_tab_y + chip_tab_h);

        bool is_cur = (selected_chip_ == c);
        std::string tab_id = "##ChipTab_" + std::to_string(c);

        ImGui::SetCursorScreenPos(t_min);
        ImGui::InvisibleButton(tab_id.c_str(), ImVec2(tab_w, chip_tab_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            setSelectedChip(c);
        }

        ImU32 tab_bg = is_cur ? IM_COL32(r, g, b, 70) :
                       (hov ? IM_COL32(255, 255, 255, 20) : IM_COL32(255, 255, 255, 8));
        dl->AddRectFilled(t_min, t_max, tab_bg, 7.0f);
        ImU32 tab_border = is_cur ? accent : (hov ? IM_COL32(255, 255, 255, 60) : IM_COL32(255, 255, 255, 22));
        dl->AddRect(t_min, t_max, tab_border, 7.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(chip_tabs[c].label);
        float txt_x = tx0 + (tab_w - txt_sz.x) * 0.5f;
        float txt_y = chip_tab_y + (chip_tab_h - txt_sz.y) * 0.5f;
        dl->AddText(ImVec2(txt_x, txt_y),
                    is_cur ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal,
                    chip_tabs[c].label);
        if (Fonts::Small) ImGui::PopFont();
    }

    // 4. 开启独立滚动子区域 (支持鼠标滑轮、触控屏手指拖拽滑动与 6px 纤细半透明滚动条)
    float content_x = card_min.x + 16.0f;
    float content_y = chip_tab_y + chip_tab_h + 10.0f;
    float content_w = chip_tab_w;
    float content_h = card_max.y - content_y - 12.0f;

    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, accent);

    if (ImGui::BeginChild("##DACSettingsScroll", ImVec2(content_w, content_h), false,
                          ImGuiWindowFlags_NoBackground)) {

        // 触控与鼠标在背景区平滑拖拽滚动
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = ImGui::GetIO().MouseDelta.y;
            ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
        }

        ImDrawList* child_dl = ImGui::GetWindowDrawList();
        float section_w = ImGui::GetContentRegionAvail().x;
        float cur_y = ImGui::GetCursorScreenPos().y;
        float x0 = ImGui::GetCursorScreenPos().x;

        // 渲染通用声卡硬件输出与独占流状态条 (所有芯片架构均共享底层声卡硬件控制)
        renderHardwareStatusBar(child_dl, x0, cur_y, section_w);

        // 根据顶部选中的芯片架构，渲染专属的发烧设置面板
        switch (selected_chip_) {
            case 0:
                renderAppleDirectSettings(child_dl, x0, cur_y, section_w);
                break;
            case 1:
                renderESSSabreSettings(child_dl, x0, cur_y, section_w);
                break;
            case 2:
                renderAKMVelvetSettings(child_dl, x0, cur_y, section_w);
                break;
            case 3:
                renderCirrusSettings(child_dl, x0, cur_y, section_w);
                break;
            case 4:
                renderR2RSettings(child_dl, x0, cur_y, section_w);
                break;
            case 5:
                renderROHMSettings(child_dl, x0, cur_y, section_w);
                break;
        }

        ImGui::Dummy(ImVec2(0.0f, 16.0f));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

// ==============================================================================
// 全局声卡硬件输出与独占流状态条 (所有芯片架构通用共享)
// ==============================================================================
void DACSettingView::renderHardwareStatusBar(ImDrawList* dl, float x0, float& cur_y, float w) {
    float h = 42.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h);

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    dl->AddRectFilled(p0, p1, IM_COL32(18, 24, 34, 190), 8.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 22), 8.0f, 0, 1.0f);

    std::string dev_name = audio_engine::AudioEngine::getInstance().getActiveHardwareDeviceName();
    bool is_bt = audio_engine::AudioEngine::getInstance().isCurrentDeviceBluetooth();
    bool is_real_hog = audio_engine::AudioEngine::getInstance().isRealHogActive();
    bool is_exclusive_pref = (apple_exclusive_mode_ == 0);
    uint32_t cur_sr = audio_engine::AudioEngine::getInstance().getActiveHardwareSampleRate();

    // 右侧独占开关药丸按钮 (支持随时切换，与全局各芯片设置严格双向同步)
    float btn_w = 110.0f;
    float btn_h = 24.0f;
    float btn_x = p1.x - btn_w - 12.0f;
    float btn_y = p0.y + (h - btn_h) * 0.5f;
    ImVec2 bp0(btn_x, btn_y);
    ImVec2 bp1(btn_x + btn_w, btn_y + btn_h);

    ImGui::SetCursorScreenPos(bp0);
    if (ImGui::InvisibleButton("##GlobalHogExclusiveToggle", ImVec2(btn_w, btn_h))) {
        apple_exclusive_mode_ = (apple_exclusive_mode_ == 0 ? 1 : 0);
        saveSettings();
    }
    bool btn_hov = ImGui::IsItemHovered();

    // 状态呼吸指示灯与颜色判定
    ImVec2 dot_center(p0.x + 16.0f, p0.y + h * 0.5f);
    ImU32 dot_col = IM_COL32(160, 160, 160, 200);
    std::string info_text;
    ImU32 info_text_col = UIConfig::Color::TextNormal;

    if (is_bt) {
        // 蓝牙模式：天蓝色标识，明确告知用户走 0dB 源码直通
        dot_col = IM_COL32(60, 195, 255, 255);
        dl->AddCircle(dot_center, 6.5f, IM_COL32(60, 195, 255, 80), 0, 1.5f);
        info_text = "声卡硬件: " + dev_name + "  |  " + (is_exclusive_pref ? "蓝牙 0dB 源码直通 (系统共享)" : "系统混音共享") + "  |  " + std::to_string(cur_sr / 1000) + " kHz";
        info_text_col = is_exclusive_pref ? IM_COL32(60, 195, 255, 255) : UIConfig::Color::TextNormal;
    } else if (is_real_hog) {
        // 物理有线声卡 / USB DAC 且已成功占用 Hog 锁
        dot_col = IM_COL32(40, 205, 120, 255);
        dl->AddCircle(dot_center, 6.5f, IM_COL32(40, 205, 120, 80), 0, 1.5f);
        info_text = "声卡硬件: " + dev_name + "  |  Hog Mode 硬件已独占 (Bit-Perfect)  |  " + std::to_string(cur_sr / 1000) + " kHz";
        info_text_col = UIConfig::Color::TextActive;
    } else if (is_exclusive_pref) {
        // 独占已开启，处于起播即独占待命状态
        dot_col = IM_COL32(40, 205, 120, 200);
        info_text = "声卡硬件: " + dev_name + "  |  独占模式就绪 (起播锁定)  |  " + std::to_string(cur_sr / 1000) + " kHz";
        info_text_col = UIConfig::Color::TextActive;
    } else {
        // 共享混音模式
        dot_col = IM_COL32(160, 160, 160, 200);
        info_text = "声卡硬件: " + dev_name + "  |  系统混音共享模式  |  " + std::to_string(cur_sr / 1000) + " kHz";
        info_text_col = UIConfig::Color::TextMuted;
    }

    dl->AddCircleFilled(dot_center, 4.0f, dot_col);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(dot_center.x + 12.0f, p0.y + (h - 14.0f) * 0.5f),
                info_text_col, info_text.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 绘制独占切换按钮
    const char* btn_text = is_exclusive_pref ? "独占: 已开启" : "独占: 已关闭";
    ImU32 btn_bg = is_exclusive_pref ? IM_COL32(r, g, b, 70) :
                   (btn_hov ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12));
    ImU32 btn_border = is_exclusive_pref ? accent :
                       (btn_hov ? IM_COL32(255, 255, 255, 60) : IM_COL32(255, 255, 255, 30));
    ImU32 btn_text_col = is_exclusive_pref ? UIConfig::Color::TextActive : UIConfig::Color::TextMuted;

    dl->AddRectFilled(bp0, bp1, btn_bg, 5.0f);
    dl->AddRect(bp0, bp1, btn_border, 5.0f, 0, 1.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 bsz = ImGui::CalcTextSize(btn_text);
    dl->AddText(ImVec2(btn_x + (btn_w - bsz.x) * 0.5f, btn_y + (btn_h - bsz.y) * 0.5f),
                btn_text_col, btn_text);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h + 10.0f;
}

// ==============================================================================
// 0. Apple Direct (MacBook Pro M1 Pro 硬件直通)
// ==============================================================================
void DACSettingView::renderAppleDirectSettings(ImDrawList* dl, float x0, float& cur_y, float w) {
    bool is_bt = audio_engine::AudioEngine::getInstance().isCurrentDeviceBluetooth();
    float h1 = is_bt ? 134.0f : 114.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h1);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "MacBook Pro 硬件直通通道 (Apple CoreAudio Bit-Perfect)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* excl_opts[] = { "Bit-Perfect 独占流", "系统混音共享" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "硬件独占流", excl_opts, 2, apple_exclusive_mode_, "AppleExcl");

    const char* rate_opts[] = { "原生跟随母带 (44.1k-192k)", "锁定 96kHz", "锁定 192kHz" };
    renderOptionRow(dl, x0, cur_y + 72.0f, w, "采样率追踪", rate_opts, 3, apple_sample_rate_, "AppleRate");

    if (is_bt) {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(x0 + 16.0f, cur_y + 110.0f),
                    IM_COL32(60, 195, 255, 230),
                    "已连接蓝牙设备，音频走系统 AAC 无线协议，已自动回退至 0dB 共享直通，确保稳定发声。");
        if (Fonts::Small) ImGui::PopFont();
    }

    cur_y += h1 + 10.0f;

    // 板块二：耳机阻抗侦测与位深
    float h2 = 146.0f;
    ImVec2 q0(x0, cur_y);
    ImVec2 q1(x0 + w, cur_y + h2);

    dl->AddRectFilled(q0, q1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(q0, q1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(q0.x + 10.0f, q0.y), ImVec2(q1.x - 10.0f, q0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "高阻抗耳机智能驱动与数据精度 (Impedance & Bit Depth)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* drive_opts[] = { "智能检测 (1.25V~3Vrms)", "强制高输出 (3.0Vrms)", "标准输出 (1.25Vrms)" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "阻抗驱动力", drive_opts, 3, apple_headphone_drive_, "AppleDrive");

    const char* depth_opts[] = { "32-bit Float 浮点直通", "24-bit 整数定点" };
    renderOptionRow(dl, x0, cur_y + 72.0f, w, "数据位深", depth_opts, 2, apple_bit_depth_, "AppleDepth");

    const char* desc = "Apple Direct 说明: MacBook Pro 硬件直通，Bit-Perfect 独占流绕过系统混音，原生 0 损耗输出。";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 112.0f), UIConfig::Color::TextMuted, desc);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h2;
}

// ==============================================================================
// 1. ESS Sabre (ES9038PRO / ES9039PRO 并联架构)
// ==============================================================================
void DACSettingView::renderESSSabreSettings(ImDrawList* dl, float x0, float& cur_y, float w) {
    // 1. PCM FIR 硬件滤波
    float h1 = 150.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h1);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "ESS Sabre ES9038PRO 硬件 FIR 滤波特性 (8通道双并联 HyperStream II)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* fir_r1[] = { "快速最小 (推荐)", "慢速最小", "快速线性", "慢速线性" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "滤波特性 (1)", fir_r1, 4, pcm_filter_mode_, "ESSFIR1");

    int mode_r2 = pcm_filter_mode_ - 4;
    const char* fir_r2[] = { "变迹滤波", "砖墙滤波", "混合滤波" };
    if (mode_r2 < 0) mode_r2 = -1;
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "滤波特性 (2)", fir_r2, 3, mode_r2, "ESSFIR2");
    if (mode_r2 >= 0) {
        pcm_filter_mode_ = mode_r2 + 4;
    }

    const char* fir_descs[] = {
        "快速最小: 消除前置振铃，低频冲击力与厚实人声极佳 (现代流媒体发烧推荐)",
        "慢速最小: 瞬态衰减平滑，极低相位延迟，模拟韵味醇厚",
        "快速线性: 频响开阔平坦，全频段高解析监听与宏大声场",
        "慢速线性: 消除高频突兀，古典弦乐与大编制交响温润自然",
        "变迹滤波: 彻底滤除早期母带残留高频混叠与杂散伪影",
        "砖墙滤波: 超陡峭截止，彻底隔绝带外高频噪声，理论指标极致",
        "混合滤波: 融汇线性与最小相位优势，声场宽阔通透"
    };
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 112.0f), UIConfig::Color::TextMuted,
                fir_descs[std::clamp(pcm_filter_mode_, 0, 6)]);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h1 + 10.0f;

    // 2. DSD 滤波与 DPLL
    float h2 = 146.0f;
    ImVec2 q0(x0, cur_y);
    ImVec2 q1(x0 + w, cur_y + h2);

    dl->AddRectFilled(q0, q1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(q0, q1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(q0.x + 10.0f, q0.y), ImVec2(q1.x - 10.0f, q0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "DSD 模拟低通滤波与 DPLL 飞秒抖动消除器 (DSD & DPLL)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* dsd_by_opts[] = { "Direct 1-Bit 直通", "FIR 模拟滤波" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "DSD 数据通路", dsd_by_opts, 2, dsd_bypass_mode_, "ESSDSDBypass");

    const char* dsd_cu_opts[] = { "47.7kHz", "50kHz", "60kHz", "70kHz" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "DSD 截止频率", dsd_cu_opts, 4, dsd_filter_cutoff_, "ESSDSDCut");

    const char* dpll_opts[] = { "极窄带 (超低抖动)", "标准平衡 (推荐)", "宽带 (抗失锁)" };
    renderOptionRow(dl, x0, cur_y + 104.0f, w, "DPLL 时钟锁定", dpll_opts, 3, pcm_dpll_band_, "ESSCkDPLL");

    cur_y += h2 + 10.0f;

    // 3. THD 谐波补偿与输出模式
    float h3 = 146.0f;
    ImVec2 r0(x0, cur_y);
    ImVec2 r1(x0 + w, cur_y + h3);

    dl->AddRectFilled(r0, r1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(r0, r1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(r0.x + 10.0f, r0.y), ImVec2(r1.x - 10.0f, r0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "THD 谐波补偿与双芯片并联架构 (THD & Parallel Architecture)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* thd_opts[] = { "超低失真 (< -122dB)", "二次谐波 (胆味)", "关闭补偿" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "THD 谐波补偿", thd_opts, 3, thd_comp_mode_, "ESSTHD");

    const char* arch_opts[] = { "双芯片 8-Ch 并联 (140dB)", "双芯片立体声平衡" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "并联解码架构", arch_opts, 2, channel_mode_, "ESSArch");

    const char* phase_opts[] = { "绝对正相 (0°)", "极性反转 (180°)" };
    renderOptionRow(dl, x0, cur_y + 104.0f, w, "输出绝对相位", phase_opts, 2, phase_invert_, "ESSPhase");

    cur_y += h3;
}

// ==============================================================================
// 2. AKM 旭化成 (AK4499EX + AK4191EQ Velvet Sound Verita)
// ==============================================================================
void DACSettingView::renderAKMVelvetSettings(ImDrawList* dl, float x0, float& cur_y, float w) {
    float h1 = 150.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h1);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "AKM Velvet Sound 6 种数字滤波器 (AK4191EQ 独立数字处理)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* akm_r1[] = { "短延时锐滚降", "短延时慢滚降", "锐滚降" };
    int f1 = (akm_filter_mode_ < 3) ? akm_filter_mode_ : -1;
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "滤波特性 (1)", akm_r1, 3, f1, "AKMF1");
    if (f1 >= 0) akm_filter_mode_ = f1;

    const char* akm_r2[] = { "慢滚降", "超低群延迟", "低色散短延时" };
    int f2 = (akm_filter_mode_ >= 3) ? (akm_filter_mode_ - 3) : -1;
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "滤波特性 (2)", akm_r2, 3, f2, "AKMF2");
    if (f2 >= 0) akm_filter_mode_ = f2 + 3;

    const char* akm_descs[] = {
        "短延时锐滚降: 声音结像深厚温润，人声饱满耐听，AKM 旗舰代表音色",
        "短延时慢滚降: 极富宽松空气感，微弱残响还原逼真，适合爵士与流行",
        "锐滚降: 超高解析力与动态对比，全频段精准监听",
        "慢滚降: 纯模拟温厚声底，彻底消除数码生硬毛刺感",
        "超低群延迟: 极高冲击力响应，低频瞬态饱满凝聚",
        "低色散短延时: AKM 独家全频段无色散相位对齐技术"
    };
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 112.0f), UIConfig::Color::TextMuted,
                akm_descs[std::clamp(akm_filter_mode_, 0, 5)]);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h1 + 10.0f;

    // 板块二：Sound Color 与 DSD 直通
    float h2 = 146.0f;
    ImVec2 q0(x0, cur_y);
    ImVec2 q1(x0 + w, cur_y + h2);

    dl->AddRectFilled(q0, q1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(q0, q1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(q0.x + 10.0f, q0.y), ImVec2(q1.x - 10.0f, q0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "Sound Color 声音风格与数字模拟完全分离架构 (Sound Style & EXDF)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* color_opts[] = { "风格1 (自然)", "风格2 (通透)", "风格3 (宽厚)", "风格4 (监听)" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "声音色彩风格", color_opts, 4, akm_sound_color_, "AKMColor");

    const char* dsd_opts[] = { "Direct 模式 (旁路调制)", "Normal 滤波模式" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "DSD 直通模式", dsd_opts, 2, akm_dsd_mode_, "AKMDSD");

    const char* exdf_opts[] = { "外部超采样直通 (EXDF)", "内部 AK4191EQ 处理" };
    renderOptionRow(dl, x0, cur_y + 104.0f, w, "数字滤波通路", exdf_opts, 2, akm_exdf_mode_, "AKMEXDF");

    cur_y += h2;
}

// ==============================================================================
// 3. Cirrus Logic (CS43198 / CS43131 MasterHIFI)
// ==============================================================================
void DACSettingView::renderCirrusSettings(ImDrawList* dl, float x0, float& cur_y, float w) {
    float h1 = 150.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h1);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "MasterHIFI 5 种滤波特性与 NOS 模式 (CS43198 Filter Modes)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* cs_r1[] = { "快速线性", "慢速线性", "快速最小" };
    int f1 = (cs_filter_mode_ < 3) ? cs_filter_mode_ : -1;
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "滤波特性 (1)", cs_r1, 3, f1, "CSF1");
    if (f1 >= 0) cs_filter_mode_ = f1;

    const char* cs_r2[] = { "慢速最小", "NOS 无过采样" };
    int f2 = (cs_filter_mode_ >= 3) ? (cs_filter_mode_ - 3) : -1;
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "滤波特性 (2)", cs_r2, 2, f2, "CSF2");
    if (f2 >= 0) cs_filter_mode_ = f2 + 3;

    const char* cs_descs[] = {
        "快速线性: 动态平坦，通透解析，标准高保真回放",
        "慢速线性: 高频滚降柔和，空气感出众，极佳耐听度",
        "快速最小: 人声甜美温润，低音弹性好，无前置振铃",
        "慢速最小: 自然模拟声学衰减，声音温厚纯净",
        "NOS (Non-Oversampling): 旁路数字插值滤波，阶梯原生方波再现"
    };
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 112.0f), UIConfig::Color::TextMuted,
                cs_descs[std::clamp(cs_filter_mode_, 0, 4)]);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h1 + 10.0f;

    // 板块二：Direct DSD 与输出驱动
    float h2 = 146.0f;
    ImVec2 q0(x0, cur_y);
    ImVec2 q1(x0 + w, cur_y + h2);

    dl->AddRectFilled(q0, q1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(q0, q1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(q0.x + 10.0f, q0.y), ImVec2(q1.x - 10.0f, q0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "Direct DSD 原生模拟与模拟放大架构 (Analog & Driver)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* dsd_opts[] = { "Direct DSD (直接转换)", "DoP 硬件解调" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "DSD 解码模式", dsd_opts, 2, cs_dsd_mode_, "CSDSD");

    const char* drive_opts[] = { "高推力伪差分 (2Vrms)", "标准单端 (1Vrms)" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "模拟驱动模式", drive_opts, 2, cs_drive_mode_, "CSDrive");

    const char* imp_opts[] = { "自适应匹配 (16Ω~600Ω)", "高阻监听优先" };
    renderOptionRow(dl, x0, cur_y + 104.0f, w, "阻抗探测模式", imp_opts, 2, cs_impedance_mode_, "CSImp");

    cur_y += h2;
}

// ==============================================================================
// 4. R-2R 纯分立电阻网络 (NOS / OS 架构)
// ==============================================================================
void DACSettingView::renderR2RSettings(ImDrawList* dl, float x0, float& cur_y, float w) {
    float h1 = 146.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h1);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "分立梯形电阻架构模式 (R-2R Ladder Mode & 1-Bit DSD)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* r2r_opts[] = { "NOS 纯无过采样 (方波原生)", "OS 线性相位超采样", "OS 最小相位超采样" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "过采样模式", r2r_opts, 3, r2r_mode_, "R2RMode");

    const char* dsd_opts[] = { "独立 1-Bit 电阻网络转换", "转换为 24-bit 阶梯解码" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "DSD 网络模式", dsd_opts, 2, r2r_dsd_mode_, "R2RDSD");

    const char* desc = "R-2R 说明: NOS 纯无过采样零数字滤波，方波瞬态真实，还原纯模拟黑胶听感。";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 112.0f), UIConfig::Color::TextMuted, desc);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h1 + 10.0f;

    // 板块二：FIFO 重整与极性
    float h2 = 114.0f;
    ImVec2 q0(x0, cur_y);
    ImVec2 q1(x0 + w, cur_y + h2);

    dl->AddRectFilled(q0, q1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(q0, q1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(q0.x + 10.0f, q0.y), ImVec2(q1.x - 10.0f, q0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "本地飞秒时钟 FIFO 缓存与绝对相位 (Clock FIFO Reclocking)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* clk_opts[] = { "飞秒 FIFO 本地时钟重整", "直接跟随输入源时钟" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "时钟重整机制", clk_opts, 2, r2r_clock_mode_, "R2RClock");

    const char* phase_opts[] = { "绝对正相 (0°)", "极性反转 (180°)" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "模拟信号极性", phase_opts, 2, r2r_phase_mode_, "R2RPhase");

    cur_y += h2;
}

// ==============================================================================
// 5. ROHM 罗姆 (MUS-IC BD34301EKV 旗舰)
// ==============================================================================
void DACSettingView::renderROHMSettings(ImDrawList* dl, float x0, float& cur_y, float w) {
    float h1 = 146.0f;
    ImVec2 p0(x0, cur_y);
    ImVec2 p1(x0 + w, cur_y + h1);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "ROHM MUS-IC 声音微调滤波与调制器时钟 (Sound Tuning & Modulator)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* f_opts[] = { "Sharp Roll-Off (高解析宏大声场)", "Slow Roll-Off (浓郁宽松自然感)" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "声音微调滤波", f_opts, 2, rohm_filter_mode_, "ROHMF");

    const char* clk_opts[] = { "智能倍频自适应", "锁定 64x fs", "锁定 128x fs" };
    renderOptionRow(dl, x0, cur_y + 70.0f, w, "调制器采样率", clk_opts, 3, rohm_modulator_clock_, "ROHMClk");

    const char* desc = "ROHM 说明: 日本罗姆旗舰 MUS-IC，力士 D-10X 旗舰唱机核心，具备天然宽松乐感。";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 112.0f), UIConfig::Color::TextMuted, desc);
    if (Fonts::Small) ImGui::PopFont();

    cur_y += h1 + 10.0f;

    // 板块二：DSD 纯模拟通路
    float h2 = 82.0f;
    ImVec2 q0(x0, cur_y);
    ImVec2 q1(x0 + w, cur_y + h2);

    dl->AddRectFilled(q0, q1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(q0, q1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(q0.x + 10.0f, q0.y), ImVec2(q1.x - 10.0f, q0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, cur_y + 10.0f), UIConfig::Color::TextActive,
                "DSD 纯模拟直接旁路通道 (Direct DSD Path)");
    if (Fonts::Regular) ImGui::PopFont();

    const char* dsd_opts[] = { "Direct Path 纯模拟通道", "标准多级滤波处理" };
    renderOptionRow(dl, x0, cur_y + 36.0f, w, "DSD 处理通道", dsd_opts, 2, rohm_dsd_path_, "ROHMPath");

    cur_y += h2;
}
