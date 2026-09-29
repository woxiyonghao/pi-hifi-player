#include "views/DACSettingView.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/MusicDatabase.hpp"
#include "themes/ThemeManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

DACSettingView::DACSettingView() {
    loadSettings();
}

void DACSettingView::setCurrentDacType(DACType type) {
    if (current_dac_type_ != type) {
        current_dac_type_ = type;
        saveSettings();
        if (on_dac_changed_) {
            on_dac_changed_(current_dac_type_, getCurrentDacName());
        }
    }
}

void DACSettingView::loadSettings() {
    auto& db = MusicDatabase::getInstance();

    // 1. 读取当前选定的 DAC 硬件类型
    std::string s_type = db.getSetting("setting_dac_hardware_type", "0");
    try {
        int t = std::stoi(s_type);
        current_dac_type_ = static_cast<DACType>(std::clamp(t, 0, 3));
    } catch (...) {
        current_dac_type_ = DACType::ES9038PRO;
    }

    // 2. ESS ES9038PRO 参数
    std::string s_pcm_f = db.getSetting("setting_dac_pcm_filter", "2");
    std::string s_dsd_by = db.getSetting("setting_dac_dsd_bypass", "0");
    std::string s_dsd_cu = db.getSetting("setting_dac_dsd_cutoff", "0");
    std::string s_pcm_dp = db.getSetting("setting_dac_pcm_dpll", "0");
    std::string s_dsd_dp = db.getSetting("setting_dac_dsd_dpll", "0");
    std::string s_thd = db.getSetting("setting_dac_thd_comp", "0");
    std::string s_mono = db.getSetting("setting_dac_mono_mode", "0");
    std::string s_out = db.getSetting("setting_dac_output_mode", "0");
    std::string s_pha = db.getSetting("setting_dac_phase", "0");

    try {
        pcm_filter_mode_ = std::clamp(std::stoi(s_pcm_f), 0, 6);
        dsd_bypass_mode_ = std::clamp(std::stoi(s_dsd_by), 0, 1);
        dsd_filter_cutoff_ = std::clamp(std::stoi(s_dsd_cu), 0, 3);
        pcm_dpll_band_ = std::clamp(std::stoi(s_pcm_dp), 0, 2);
        dsd_dpll_band_ = std::clamp(std::stoi(s_dsd_dp), 0, 2);
        thd_comp_mode_ = std::clamp(std::stoi(s_thd), 0, 2);
        channel_mode_ = std::clamp(std::stoi(s_mono), 0, 1);
        output_level_mode_ = std::clamp(std::stoi(s_out), 0, 1);
        phase_invert_ = std::clamp(std::stoi(s_pha), 0, 1);
    } catch (...) {
        pcm_filter_mode_ = 2;
        dsd_bypass_mode_ = 0;
        dsd_filter_cutoff_ = 0;
        pcm_dpll_band_ = 0;
        dsd_dpll_band_ = 0;
        thd_comp_mode_ = 0;
        channel_mode_ = 0;
        output_level_mode_ = 0;
        phase_invert_ = 0;
    }

    // 3. AKM AK4499EX 参数
    try {
        ak_filter_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_ak_filter", "2")), 0, 5);
        ak_sound_color_ = std::clamp(std::stoi(db.getSetting("setting_dac_ak_sound_color", "0")), 0, 3);
        ak_dsd_bypass_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_ak_dsd_bypass", "0")), 0, 1);
        ak_dsd_filter_cutoff_ = std::clamp(std::stoi(db.getSetting("setting_dac_ak_dsd_cutoff", "0")), 0, 2);
        ak_output_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_ak_output_mode", "0")), 0, 1);
        ak_phase_invert_ = std::clamp(std::stoi(db.getSetting("setting_dac_ak_phase", "0")), 0, 1);
    } catch (...) {
        ak_filter_mode_ = 2;
        ak_sound_color_ = 0;
        ak_dsd_bypass_mode_ = 0;
        ak_dsd_filter_cutoff_ = 0;
        ak_output_mode_ = 0;
        ak_phase_invert_ = 0;
    }

    // 4. Cirrus CS43198 参数
    try {
        cs_filter_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_filter", "4")), 0, 4);
        cs_dsd_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_dsd_mode", "0")), 0, 1);
        cs_headphone_gain_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_gain", "0")), 0, 2);
        cs_output_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_cs_output_mode", "0")), 0, 1);
    } catch (...) {
        cs_filter_mode_ = 4;
        cs_dsd_mode_ = 0;
        cs_headphone_gain_ = 0;
        cs_output_mode_ = 0;
    }

    // 5. 标准通用 I2S / 平台声卡参数
    try {
        gen_bit_perfect_ = std::clamp(std::stoi(db.getSetting("setting_dac_gen_bit_perfect", "0")), 0, 1);
        gen_bit_depth_ = std::clamp(std::stoi(db.getSetting("setting_dac_gen_bit_depth", "0")), 0, 3);
        gen_output_mode_ = std::clamp(std::stoi(db.getSetting("setting_dac_gen_output_mode", "0")), 0, 1);
        gen_phase_invert_ = std::clamp(std::stoi(db.getSetting("setting_dac_gen_phase", "0")), 0, 1);
    } catch (...) {
        gen_bit_perfect_ = 0;
        gen_bit_depth_ = 0;
        gen_output_mode_ = 0;
        gen_phase_invert_ = 0;
    }
}

void DACSettingView::saveSettings() {
    auto& db = MusicDatabase::getInstance();
    db.setSetting("setting_dac_hardware_type", std::to_string(static_cast<int>(current_dac_type_)));

    // ESS ES9038
    db.setSetting("setting_dac_pcm_filter", std::to_string(pcm_filter_mode_));
    db.setSetting("setting_dac_dsd_bypass", std::to_string(dsd_bypass_mode_));
    db.setSetting("setting_dac_dsd_cutoff", std::to_string(dsd_filter_cutoff_));
    db.setSetting("setting_dac_pcm_dpll", std::to_string(pcm_dpll_band_));
    db.setSetting("setting_dac_dsd_dpll", std::to_string(dsd_dpll_band_));
    db.setSetting("setting_dac_thd_comp", std::to_string(thd_comp_mode_));
    db.setSetting("setting_dac_mono_mode", std::to_string(channel_mode_));
    db.setSetting("setting_dac_output_mode", std::to_string(output_level_mode_));
    db.setSetting("setting_dac_phase", std::to_string(phase_invert_));

    // AKM AK4499EX
    db.setSetting("setting_dac_ak_filter", std::to_string(ak_filter_mode_));
    db.setSetting("setting_dac_ak_sound_color", std::to_string(ak_sound_color_));
    db.setSetting("setting_dac_ak_dsd_bypass", std::to_string(ak_dsd_bypass_mode_));
    db.setSetting("setting_dac_ak_dsd_cutoff", std::to_string(ak_dsd_filter_cutoff_));
    db.setSetting("setting_dac_ak_output_mode", std::to_string(ak_output_mode_));
    db.setSetting("setting_dac_ak_phase", std::to_string(ak_phase_invert_));

    // Cirrus CS43198
    db.setSetting("setting_dac_cs_filter", std::to_string(cs_filter_mode_));
    db.setSetting("setting_dac_cs_dsd_mode", std::to_string(cs_dsd_mode_));
    db.setSetting("setting_dac_cs_gain", std::to_string(cs_headphone_gain_));
    db.setSetting("setting_dac_cs_output_mode", std::to_string(cs_output_mode_));

    // 通用 I2S
    db.setSetting("setting_dac_gen_bit_perfect", std::to_string(gen_bit_perfect_));
    db.setSetting("setting_dac_gen_bit_depth", std::to_string(gen_bit_depth_));
    db.setSetting("setting_dac_gen_output_mode", std::to_string(gen_output_mode_));
    db.setSetting("setting_dac_gen_phase", std::to_string(gen_phase_invert_));
}

void DACSettingView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 渲染顶级深空高密度毛玻璃大底板
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "main_stage");

    // 2. 标题「DAC 硬件设置与前级调音」
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 14.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "DAC 硬件设置与前级调音");
    if (Fonts::Medium) ImGui::PopFont();

    // 3. 硬件芯片型号切换选择器 (Top Section)
    float selector_y = card_min.y + 44.0f;
    float content_w = card_max.x - card_min.x - 32.0f;
    renderHardwareSelector(dl, card_min.x + 16.0f, selector_y, content_w);

    // 4. 开启独立滚动子区域 (支持手势拖拽滑动与纤细滚动条)
    float scroll_y = selector_y + 70.0f;
    float scroll_h = card_max.y - scroll_y - 12.0f;

    ImU32 accent = ThemeManager::getInstance().getAccentColor();

    ImGui::SetCursorScreenPos(ImVec2(card_min.x + 16.0f, scroll_y));

    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, accent);

    if (ImGui::BeginChild("##DACDynamicSettingsScroll", ImVec2(content_w, scroll_h), false,
                          ImGuiWindowFlags_NoBackground)) {

        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = ImGui::GetIO().MouseDelta.y;
            ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
        }

        ImDrawList* child_dl = ImGui::GetWindowDrawList();
        float section_w = ImGui::GetContentRegionAvail().x;
        ImVec2 p_cursor = ImGui::GetCursorScreenPos();

        // 根据当前选定的硬件 DAC 架构，动态分发渲染专属面板
        switch (current_dac_type_) {
            case DACType::ES9038PRO:
                renderES9038View(child_dl, p_cursor.x, p_cursor.y, section_w);
                break;
            case DACType::AK4499EX:
                renderAK4499EXView(child_dl, p_cursor.x, p_cursor.y, section_w);
                break;
            case DACType::CS43198:
                renderCS43198View(child_dl, p_cursor.x, p_cursor.y, section_w);
                break;
            case DACType::GENERIC_I2S:
            default:
                renderGenericI2SView(child_dl, p_cursor.x, p_cursor.y, section_w);
                break;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

// ==============================================================================
// 硬件芯片切换器与规格胶囊
// ==============================================================================
void DACSettingView::renderHardwareSelector(ImDrawList* dl, float x0, float y0, float w) {
    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const auto& profiles = getSupportedDACProfiles();
    int count = static_cast<int>(profiles.size());
    float gap = 8.0f;
    float btn_w = (w - gap * (count - 1)) / count;
    float btn_h = 28.0f;

    for (int i = 0; i < count; ++i) {
        float bx = x0 + i * (btn_w + gap);
        ImVec2 b_min(bx, y0);
        ImVec2 b_max(bx + btn_w, y0 + btn_h);

        bool is_act = (current_dac_type_ == profiles[i].type);
        std::string btn_id = "##DacTypeBtn_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            setCurrentDacType(profiles[i].type);
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 75) :
                   (hov ? IM_COL32(255, 255, 255, 24) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 60) : IM_COL32(255, 255, 255, 22));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 t_sz = ImGui::CalcTextSize(profiles[i].model_name.c_str());
        dl->AddText(ImVec2(bx + (btn_w - t_sz.x) * 0.5f, y0 + (btn_h - t_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal,
                    profiles[i].model_name.c_str());
        if (Fonts::Small) ImGui::PopFont();
    }

    // 绘制当前选定芯片的架构规格徽章
    const auto& cur_info = getDACProfileInfo(current_dac_type_);
    float badge_y = y0 + btn_h + 8.0f;
    float badge_h = 24.0f;
    ImVec2 badge_min(x0, badge_y);
    ImVec2 badge_max(x0 + w, badge_y + badge_h);

    dl->AddRectFilled(badge_min, badge_max, IM_COL32(10, 16, 26, 160), 4.0f);
    dl->AddRect(badge_min, badge_max, IM_COL32(255, 255, 255, 16), 4.0f, 0, 1.0f);

    std::string meta_str = "硬件架构: " + cur_info.manufacturer + " · " + cur_info.architecture +
                           "  [最高支持 PCM " + cur_info.max_pcm + " | " + cur_info.max_dsd + "]";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 10.0f, badge_y + (badge_h - ImGui::GetFontSize()) * 0.5f),
                IM_COL32(148, 163, 184, 255), meta_str.c_str());
    if (Fonts::Small) ImGui::PopFont();
}

// ==============================================================================
// 1. ESS ES9038PRO 专属渲染面板
// ==============================================================================
void DACSettingView::renderES9038View(ImDrawList* dl, float x0, float y0, float w) {
    ImVec2 p_pcm(x0, y0);
    renderPCMFilterSection(dl, p_pcm.x, p_pcm.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_pcm.x, p_pcm.y + 150.0f));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImVec2 p_dsd = ImGui::GetCursorScreenPos();
    renderDSDFilterSection(dl, p_dsd.x, p_dsd.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_dsd.x, p_dsd.y + 110.0f));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImVec2 p_dpll = ImGui::GetCursorScreenPos();
    renderDPLLSection(dl, p_dpll.x, p_dpll.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_dpll.x, p_dpll.y + 110.0f));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImVec2 p_thd = ImGui::GetCursorScreenPos();
    renderHarmonicsAndOutputSection(dl, p_thd.x, p_thd.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_thd.x, p_thd.y + 182.0f));
    ImGui::Dummy(ImVec2(0.0f, 16.0f));
}

void DACSettingView::renderPCMFilterSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 150.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "ESS PCM 硬件数字滤波器 (Hardware FIR Filter)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    const char* row1_labels[] = { "快速线性", "慢速线性", "快速最小 (推荐)", "慢速最小" };
    const int row1_indices[] = { 0, 1, 2, 3 };

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    float btn_w1 = (row_w - gap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        int filter_idx = row1_indices[i];
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 38.0f + btn_h);

        bool is_act = (pcm_filter_mode_ == filter_idx);
        std::string btn_id = "##PCMFir_" + std::to_string(filter_idx);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            pcm_filter_mode_ = filter_idx;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(row1_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, row1_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    const char* row2_labels[] = { "变迹滤波 (Apodizing)", "砖墙滤波 (Brickwall)", "混合滤波 (Hybrid)" };
    const int row2_indices[] = { 4, 5, 6 };
    float btn_w2 = (row_w - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        int filter_idx = row2_indices[i];
        float bx0 = start_x + i * (btn_w2 + gap);
        ImVec2 b_min(bx0, y0 + 72.0f);
        ImVec2 b_max(bx0 + btn_w2, y0 + 72.0f + btn_h);

        bool is_act = (pcm_filter_mode_ == filter_idx);
        std::string btn_id = "##PCMFir_" + std::to_string(filter_idx);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w2, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            pcm_filter_mode_ = filter_idx;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(row2_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w2 - txt_sz.x) * 0.5f, y0 + 72.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, row2_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    const char* descs[] = {
        "快速滚降线性相位：频响平坦，声场工整对称，标准监听级重播表现。",
        "慢速滚降线性相位：高频过渡平滑温和，乐器泛音自然舒展。",
        "快速滚降最小相位：无预振铃(Pre-ringing)，瞬态结像干脆，现代流行与人声首选推荐。",
        "慢速滚降最小相位：微弱后振铃，暖甜耐听，极佳的模拟黑胶乐感。",
        "变迹滤波 (Apodizing)：消除高频数码采样阶梯伪影，声底通透纯净。",
        "砖墙滤波 (Brickwall)：极限陡峭滚降，杜绝任何超声波混叠镜频镜像。",
        "混合滤波 (Hybrid)：兼顾线性相位平坦度与最小相位瞬态打击感。"
    };
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 112.0f), IM_COL32(148, 163, 184, 255), descs[pcm_filter_mode_]);
    if (Fonts::Small) ImGui::PopFont();
}

void DACSettingView::renderDSDFilterSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 110.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "ESS DSD 模拟低通滤波与直通 (DSD Direct & Reconstruction)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* bp_labels[] = { "Direct 1-Bit 直通 (最高纯度)", "FIR 模拟低通滤波" };
    float bp_w = (row_w * 0.45f - gap) * 0.5f;
    for (int i = 0; i < 2; ++i) {
        float bx0 = start_x + i * (bp_w + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + bp_w, y0 + 38.0f + btn_h);

        bool is_act = (dsd_bypass_mode_ == i);
        std::string btn_id = "##DSDBypass_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(bp_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            dsd_bypass_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(bp_labels[i]);
        dl->AddText(ImVec2(bx0 + (bp_w - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, bp_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    const char* cut_labels[] = { "47.7kHz (标准)", "50kHz", "60kHz", "70kHz (高带宽)" };
    float cut_start_x = start_x + row_w * 0.45f + gap;
    float cut_avail_w = row_w * 0.55f - gap;
    float cut_w = (cut_avail_w - gap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float bx0 = cut_start_x + i * (cut_w + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + cut_w, y0 + 38.0f + btn_h);

        bool is_act = (dsd_filter_cutoff_ == i);
        std::string btn_id = "##DSDCutoff_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(cut_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            dsd_filter_cutoff_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(cut_labels[i]);
        dl->AddText(ImVec2(bx0 + (cut_w - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, cut_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    const char* dsd_tip = (dsd_bypass_mode_ == 0)
        ? "当前处于 1-Bit 纯物理直通，绕过所有数字运算与滤波，呈现最真实的 DSD 模拟原音。"
        : "开启模拟滤波可有效抑制高频量子化噪声，推荐在搭配宽频带甲类放大器时选用。";
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 78.0f), IM_COL32(148, 163, 184, 255), dsd_tip);
    if (Fonts::Small) ImGui::PopFont();
}

void DACSettingView::renderDPLLSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 110.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "ESS 专利 DPLL 飞秒抖动消除器 (Jitter Eliminator Bandwidth)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* dpll_labels[] = { "极窄带 (超低抖动)", "标准平衡", "宽带 (抗时钟失锁)" };
    float col_w = (row_w - gap) * 0.5f;
    float btn_w = (col_w - gap * 2.0f) / 3.0f;

    for (int i = 0; i < 3; ++i) {
        float bx0 = start_x + i * (btn_w + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w, y0 + 38.0f + btn_h);

        bool is_act = (pcm_dpll_band_ == i);
        std::string btn_id = "##PCMDPLL_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            pcm_dpll_band_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(dpll_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, dpll_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    float dsd_x0 = start_x + col_w + gap;
    for (int i = 0; i < 3; ++i) {
        float bx0 = dsd_x0 + i * (btn_w + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w, y0 + 38.0f + btn_h);

        bool is_act = (dsd_dpll_band_ == i);
        std::string btn_id = "##DSDDPLL_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            dsd_dpll_band_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(dpll_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, dpll_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 78.0f), IM_COL32(148, 163, 184, 255),
                "左侧为 PCM DPLL 带宽，右侧为 DSD DPLL 带宽。在优质晶振环境下选择极窄带可获得最佳结像凝聚力。");
    if (Fonts::Small) ImGui::PopFont();
}

void DACSettingView::renderHarmonicsAndOutputSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 182.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "THD 谐波补偿与双芯片并联架构 (THD Compensation & Architecture)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* thd_labels[] = { "纯净超低失真 (< -122dB)", "二次偶次谐波增强 (模拟胆味)", "关闭谐波补偿" };
    float btn_w1 = (row_w - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 38.0f + btn_h);

        bool is_act = (thd_comp_mode_ == i);
        std::string btn_id = "##THDComp_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            thd_comp_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(thd_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, thd_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    const char* row2_labels[] = {
        (channel_mode_ == 0) ? "双芯片 8-Ch 单声道并联 (140dB SNR)" : "全平衡双声道立体声",
        (output_level_mode_ == 0) ? "纯后级固定 Line-Out (4.2V)" : "模拟前级 Pre-Out (音量可调)",
        (phase_invert_ == 0) ? "绝对正相 (0° In-Phase)" : "相位极性反转 (180° Inverted)"
    };

    float btn_w2 = (row_w - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        float bx0 = start_x + i * (btn_w2 + gap);
        ImVec2 b_min(bx0, y0 + 72.0f);
        ImVec2 b_max(bx0 + btn_w2, y0 + 72.0f + btn_h);

        bool is_act = (i == 0 && channel_mode_ == 0) || (i == 1 && output_level_mode_ == 0) || (i == 2 && phase_invert_ == 0);
        std::string btn_id = "##ArchRow_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w2, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f)) {
            if (i == 0) channel_mode_ = 1 - channel_mode_;
            else if (i == 1) output_level_mode_ = 1 - output_level_mode_;
            else if (i == 2) phase_invert_ = 1 - phase_invert_;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(row2_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w2 - txt_sz.x) * 0.5f, y0 + 72.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, row2_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 112.0f), IM_COL32(148, 163, 184, 255),
                "左右独立双 ES9038PRO 采用每片 8 通道单声道物理并联方案，实现高达 140dB 的极限动态范围。");
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 130.0f), IM_COL32(148, 163, 184, 255),
                "谐波补偿可针对模拟音频链路非线性进行抵消，二次谐波增强可为现代高解析录音注入温润黑胶质感。");
    if (Fonts::Small) ImGui::PopFont();
}

// ==============================================================================
// 2. AKM AK4499EX 专属渲染面板
// ==============================================================================
void DACSettingView::renderAK4499EXView(ImDrawList* dl, float x0, float y0, float w) {
    ImVec2 p_velvet(x0, y0);
    renderAKMVelvetFilterSection(dl, p_velvet.x, p_velvet.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_velvet.x, p_velvet.y + 140.0f));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImVec2 p_color = ImGui::GetCursorScreenPos();
    renderAKMSoundColorSection(dl, p_color.x, p_color.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_color.x, p_color.y + 110.0f));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImVec2 p_dsd = ImGui::GetCursorScreenPos();
    renderAKMDSDAndOutputSection(dl, p_dsd.x, p_dsd.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_dsd.x, p_dsd.y + 140.0f));
    ImGui::Dummy(ImVec2(0.0f, 16.0f));
}

void DACSettingView::renderAKMVelvetFilterSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 140.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "AKM Velvet Sound 滤波曲线 (Digital Filter Types)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    // 6 种经典 AKM 滤波
    const char* ak_labels[] = {
        "锐减 (Sharp Roll-Off)", "缓减 (Slow Roll-Off)", "短延迟锐减 (Short Delay Sharp)",
        "短延迟缓减 (Short Delay Slow)", "超缓减 (Super Slow Roll-Off)", "低色散短延迟 (Low Dispersion)"
    };

    float btn_w = (row_w - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 6; ++i) {
        int row = i / 3;
        int col = i % 3;
        float bx0 = start_x + col * (btn_w + gap);
        float by0 = y0 + 38.0f + row * (btn_h + 8.0f);
        ImVec2 b_min(bx0, by0);
        ImVec2 b_max(bx0 + btn_w, by0 + btn_h);

        bool is_act = (ak_filter_mode_ == i);
        std::string btn_id = "##AKFilter_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            ak_filter_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(ak_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, by0 + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, ak_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 110.0f), IM_COL32(148, 163, 184, 255),
                "AKM 专利 Velvet Sound 架构：短延迟模式可最大限度抑制声学回声，低色散模式实现极高的空间定位感。");
    if (Fonts::Small) ImGui::PopFont();
}

void DACSettingView::renderAKMSoundColorSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 110.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "AKM Sound Color 模拟风格调节 (Hardware Sound Color Modes)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* colors[] = { "Sound 1 (默认自然)", "Sound 2 (温暖饱满)", "Sound 3 (极速通透)", "Sound 4 (醇厚模拟)" };
    float btn_w = (row_w - gap * 3.0f) / 4.0f;

    for (int i = 0; i < 4; ++i) {
        float bx0 = start_x + i * (btn_w + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w, y0 + 38.0f + btn_h);

        bool is_act = (ak_sound_color_ == i);
        std::string btn_id = "##AKColor_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            ak_sound_color_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(colors[i]);
        dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, colors[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 78.0f), IM_COL32(148, 163, 184, 255),
                "通过 AK4499EX 内部专有数字时钟拓扑与偏置电流微调，在纯硬件级别塑造温润、通透或大动态模拟声底。");
    if (Fonts::Small) ImGui::PopFont();
}

void DACSettingView::renderAKMDSDAndOutputSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 140.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "DSD Direct 原生直通与模拟输出配置 (DSD & Output Architecture)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* dsd_modes[] = { "DSD Direct (纯物理直通)", "Normal Filter (模拟滤波)" };
    float btn_w1 = (row_w - gap) * 0.5f;
    for (int i = 0; i < 2; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 38.0f + btn_h);

        bool is_act = (ak_dsd_bypass_mode_ == i);
        std::string btn_id = "##AKDsdMode_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            ak_dsd_bypass_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(dsd_modes[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, dsd_modes[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    const char* out_labels[] = {
        (ak_output_mode_ == 0) ? "固定电平 Line-Out (4.0V RMS)" : "模拟前级 Pre-Out (音量可调)",
        (ak_phase_invert_ == 0) ? "绝对正相 (0° In-Phase)" : "相位极性反转 (180° Inverted)"
    };
    for (int i = 0; i < 2; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 72.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 72.0f + btn_h);

        bool is_act = (i == 0 && ak_output_mode_ == 0) || (i == 1 && ak_phase_invert_ == 0);
        std::string btn_id = "##AKOutRow_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            if (i == 0) ak_output_mode_ = 1 - ak_output_mode_;
            else if (i == 1) ak_phase_invert_ = 1 - ak_phase_invert_;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(out_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 72.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, out_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 110.0f), IM_COL32(148, 163, 184, 255),
                "AK4191 与 AK4499EX 纯物理独立分工，DSD 直通跳过调制器，提供母带级无染色还原。");
    if (Fonts::Small) ImGui::PopFont();
}

// ==============================================================================
// 3. Cirrus CS43198 专属渲染面板
// ==============================================================================
void DACSettingView::renderCS43198View(ImDrawList* dl, float x0, float y0, float w) {
    ImVec2 p_filt(x0, y0);
    renderCSFiltersAndNOSSection(dl, p_filt.x, p_filt.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_filt.x, p_filt.y + 140.0f));
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImVec2 p_gain = ImGui::GetCursorScreenPos();
    renderCSGainAndOutputSection(dl, p_gain.x, p_gain.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_gain.x, p_gain.y + 140.0f));
    ImGui::Dummy(ImVec2(0.0f, 16.0f));
}

void DACSettingView::renderCSFiltersAndNOSSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 140.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "Cirrus MasterHIFI 滤波与 NOS 模式 (Digital Filter & NOS Mode)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* cs_labels[] = {
        "快速线性 (Fast Linear)", "慢速线性 (Slow Linear)", "快速最小 (Fast Min)",
        "慢速最小 (Slow Min)", "★ NOS 无过采样模式 (Non-Oversampling)"
    };

    // 第一行 3 个按钮
    float btn_w1 = (row_w - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 38.0f + btn_h);

        bool is_act = (cs_filter_mode_ == i);
        std::string btn_id = "##CSFilter_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            cs_filter_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(cs_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, cs_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    // 第二行 2 个按钮
    float btn_w2 = (row_w - gap) * 0.5f;
    for (int i = 3; i < 5; ++i) {
        float bx0 = start_x + (i - 3) * (btn_w2 + gap);
        ImVec2 b_min(bx0, y0 + 72.0f);
        ImVec2 b_max(bx0 + btn_w2, y0 + 72.0f + btn_h);

        bool is_act = (cs_filter_mode_ == i);
        std::string btn_id = "##CSFilter_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w2, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            cs_filter_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(cs_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w2 - txt_sz.x) * 0.5f, y0 + 72.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, cs_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 110.0f), IM_COL32(148, 163, 184, 255),
                "NOS (Non-Oversampling) 模式完全关闭数字插值滤波，消除一切时域振铃，带来极致温润通透的模拟黑胶乐感。");
    if (Fonts::Small) ImGui::PopFont();
}

void DACSettingView::renderCSGainAndOutputSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 140.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "阻抗自适应耳放与输出配置 (Impedance Sensing & Output)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    const char* gains[] = { "自适应阻抗检测 (Auto)", "低阻高敏耳塞模式", "高阻抗监听大耳 (High Gain)" };
    float btn_w1 = (row_w - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 38.0f + btn_h);

        bool is_act = (cs_headphone_gain__ == i);
        std::string btn_id = "##CSGain_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            cs_headphone_gain_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(gains[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, gains[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    const char* dsd_cs_labels[] = { "Direct DSD (1-Bit 物理直通)", "Filtered DSD (模拟低通)" };
    float btn_w2 = (row_w - gap) * 0.5f;
    for (int i = 0; i < 2; ++i) {
        float bx0 = start_x + i * (btn_w2 + gap);
        ImVec2 b_min(bx0, y0 + 72.0f);
        ImVec2 b_max(bx0 + btn_w2, y0 + 72.0f + btn_h);

        bool is_act = (cs_dsd_mode_ == i);
        std::string btn_id = "##CSDsd_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w2, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            cs_dsd_mode_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(dsd_cs_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w2 - txt_sz.x) * 0.5f, y0 + 72.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, dsd_cs_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 110.0f), IM_COL32(148, 163, 184, 255),
                "集成高性能 Class-H 耳机驱动器，支持 600Ω 阻抗检测并输出高达 2V RMS 强劲驱动力。");
    if (Fonts::Small) ImGui::PopFont();
}

// ==============================================================================
// 4. 标准通用 I2S / 平台声卡专属渲染面板
// ==============================================================================
void DACSettingView::renderGenericI2SView(ImDrawList* dl, float x0, float y0, float w) {
    ImVec2 p_gen(x0, y0);
    renderGenericAudioSection(dl, p_gen.x, p_gen.y, w);
    ImGui::SetCursorScreenPos(ImVec2(p_gen.x, p_gen.y + 180.0f));
    ImGui::Dummy(ImVec2(0.0f, 16.0f));
}

void DACSettingView::renderGenericAudioSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 180.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "标准音频数据流与直通配置 (Standard Audio Stream & Bit-Perfect)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    // 第一行：Bit-Perfect 源码独占直通
    const char* bp_labels[] = { "开启 Bit-Perfect 源码独占直通 (推荐)", "共享系统混音器 (非独占模式)" };
    float btn_w1 = (row_w - gap) * 0.5f;
    for (int i = 0; i < 2; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 38.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 38.0f + btn_h);

        bool is_act = (gen_bit_perfect_ == i);
        std::string btn_id = "##GenBp_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            gen_bit_perfect_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(bp_labels[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 38.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, bp_labels[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    // 第二行：目标音频位深
    const char* depths[] = { "自适应原始精度 (Auto Native)", "16-Bit (CD 标准)", "24-Bit (Hi-Res 高解析)", "32-Bit Float (发烧浮点)" };
    float btn_w2 = (row_w - gap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float bx0 = start_x + i * (btn_w2 + gap);
        ImVec2 b_min(bx0, y0 + 72.0f);
        ImVec2 b_max(bx0 + btn_w2, y0 + 72.0f + btn_h);

        bool is_act = (gen_bit_depth_ == i);
        std::string btn_id = "##GenDepth_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w2, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            gen_bit_depth_ = i;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(depths[i]);
        dl->AddText(ImVec2(bx0 + (btn_w2 - txt_sz.x) * 0.5f, y0 + 72.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, depths[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    // 第三行：输出模式与相位
    const char* gen_opts[] = {
        (gen_output_mode_ == 0) ? "纯后级固定 Line-Out" : "模拟前级 Pre-Out (音量可调)",
        (gen_phase_invert_ == 0) ? "绝对正相 (0° In-Phase)" : "相位极性反转 (180° Inverted)"
    };
    for (int i = 0; i < 2; ++i) {
        float bx0 = start_x + i * (btn_w1 + gap);
        ImVec2 b_min(bx0, y0 + 106.0f);
        ImVec2 b_max(bx0 + btn_w1, y0 + 106.0f + btn_h);

        bool is_act = (i == 0 && gen_output_mode_ == 0) || (i == 1 && gen_phase_invert_ == 0);
        std::string btn_id = "##GenOut_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w1, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            if (i == 0) gen_output_mode_ = 1 - gen_output_mode_;
            else if (i == 1) gen_phase_invert_ = 1 - gen_phase_invert_;
            saveSettings();
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(gen_opts[i]);
        dl->AddText(ImVec2(bx0 + (btn_w1 - txt_sz.x) * 0.5f, y0 + 106.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, gen_opts[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 144.0f), IM_COL32(148, 163, 184, 255),
                "适配普通 I2S 声卡（如 PCM5102/MAX98357A）或 Mac 平台内置声卡，支持点对点无重采样直出。");
    if (Fonts::Small) ImGui::PopFont();
}
