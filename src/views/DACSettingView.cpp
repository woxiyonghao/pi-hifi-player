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

void DACSettingView::loadSettings() {
    auto& db = MusicDatabase::getInstance();
    std::string s_pcm_f = db.getSetting("setting_dac_pcm_filter", "2"); // 默认：快速最小相位
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
}

void DACSettingView::saveSettings() {
    auto& db = MusicDatabase::getInstance();
    db.setSetting("setting_dac_pcm_filter", std::to_string(pcm_filter_mode_));
    db.setSetting("setting_dac_dsd_bypass", std::to_string(dsd_bypass_mode_));
    db.setSetting("setting_dac_dsd_cutoff", std::to_string(dsd_filter_cutoff_));
    db.setSetting("setting_dac_pcm_dpll", std::to_string(pcm_dpll_band_));
    db.setSetting("setting_dac_dsd_dpll", std::to_string(dsd_dpll_band_));
    db.setSetting("setting_dac_thd_comp", std::to_string(thd_comp_mode_));
    db.setSetting("setting_dac_mono_mode", std::to_string(channel_mode_));
    db.setSetting("setting_dac_output_mode", std::to_string(output_level_mode_));
    db.setSetting("setting_dac_phase", std::to_string(phase_invert_));
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
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "DAC 设置");
    if (Fonts::Medium) ImGui::PopFont();

    // 3. 开启独立滚动子区域 (支持鼠标滑轮、触控屏手指拖拽滑动与 6px 纤细半透明滚动条)
    float content_x = card_min.x + 16.0f;
    float content_y = card_min.y + 54.0f;
    float content_w = card_max.x - card_min.x - 32.0f;
    float content_h = card_max.y - content_y - 12.0f;

    ImU32 accent = ThemeManager::getInstance().getAccentColor();

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

        // 板块一：PCM 硬件数字滤波器滚降特性 (150px)
        ImVec2 p_pcm = ImGui::GetCursorScreenPos();
        renderPCMFilterSection(child_dl, p_pcm.x, p_pcm.y, section_w);
        ImGui::SetCursorScreenPos(ImVec2(p_pcm.x, p_pcm.y + 150.0f));
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 板块二：DSD 模拟低通滤波与直通 (110px)
        ImVec2 p_dsd = ImGui::GetCursorScreenPos();
        renderDSDFilterSection(child_dl, p_dsd.x, p_dsd.y, section_w);
        ImGui::SetCursorScreenPos(ImVec2(p_dsd.x, p_dsd.y + 110.0f));
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 板块三：ESS 专利 DPLL 飞秒抖动消除器 (110px)
        ImVec2 p_dpll = ImGui::GetCursorScreenPos();
        renderDPLLSection(child_dl, p_dpll.x, p_dpll.y, section_w);
        ImGui::SetCursorScreenPos(ImVec2(p_dpll.x, p_dpll.y + 110.0f));
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 板块四：THD 谐波补偿与双芯片并联架构 (182px)
        ImVec2 p_thd = ImGui::GetCursorScreenPos();
        renderHarmonicsAndOutputSection(child_dl, p_thd.x, p_thd.y, section_w);
        ImGui::SetCursorScreenPos(ImVec2(p_thd.x, p_thd.y + 182.0f));
        ImGui::Dummy(ImVec2(0.0f, 16.0f));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void DACSettingView::renderPCMFilterSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 150.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "PCM 硬件数字滤波器滚降特性 (Hardware FIR Filter)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 7 种滤波预设紧凑易读名称 (严格不溢出)
    const char* row1_labels[] = { "快速线性", "慢速线性", "快速最小 (推荐)", "慢速最小" };
    const int row1_indices[] = { 0, 1, 2, 3 };

    float start_x = x0 + 16.0f;
    float row_w = w - 32.0f;
    float gap = 8.0f;
    float btn_h = 26.0f;

    // 第一行 (4 个按钮)
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

    // 第二行 (3 个按钮)
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

    // 动态发烧听感与冲激响应说明
    const char* desc = "";
    switch (pcm_filter_mode_) {
        case 0: desc = "快速线性 (Fast Linear): 频响平坦开阔，瞬态强劲，声场纵深与细节解析力极高"; break;
        case 1: desc = "慢速线性 (Slow Linear): 极低振铃效应，高频温润衰减自然，适合管弦乐与古典录音"; break;
        case 2: desc = "快速最小 (Fast Minimum, 推荐): 完全消除前置振铃，极佳低频瞬态与饱满人声厚度"; break;
        case 3: desc = "慢速最小 (Slow Minimum): 瞬态响应干净利落，音色模拟柔和，无数字化生硬感"; break;
        case 4: desc = "变迹滤波 (Apodizing): 消除母带录音前期残存高频伪影与混叠，还原本真纯净母带"; break;
        case 5: desc = "砖墙滤波 (Brickwall): 彻底阻断带外噪声，高频截止陡峭，保证绝对理论信噪比"; break;
        case 6: desc = "混合滤波 (Hybrid): 融合线性与最小相位优势，兼具极低群延迟与开阔声场立体感"; break;
        default: desc = ""; break;
    }

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(start_x, y0 + 112.0f), UIConfig::Color::TextMuted, desc);
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
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "DSD 模拟低通滤波与直通 (DSD Reconstruction Filter)");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    auto renderButtonGroup = [&](float row_y, const char* label, const char* const options[], int count, int& current_val, const char* id_prefix) {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(x0 + 16.0f, row_y + 4.0f), UIConfig::Color::TextMuted, label);
        if (Fonts::Small) ImGui::PopFont();

        float btn_start_x = x0 + 124.0f;
        float btn_gap = 8.0f;
        float btn_w = (w - 140.0f - btn_gap * (count - 1)) / static_cast<float>(count);
        float btn_h = 26.0f;

        for (int i = 0; i < count; ++i) {
            float bx0 = btn_start_x + i * (btn_w + btn_gap);
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

            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImVec2 txt_sz = ImGui::CalcTextSize(options[i]);
            dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, row_y + (btn_h - txt_sz.y) * 0.5f),
                        is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, options[i]);
            if (Fonts::Small) ImGui::PopFont();
        }
    };

    // 1. DSD 通路模式 (精炼文案，消除溢出)
    const char* dsd_bypass_opts[] = { "Direct 1-Bit 直通", "FIR 模拟滤波" };
    renderButtonGroup(y0 + 36.0f, "DSD 数据通路", dsd_bypass_opts, 2, dsd_bypass_mode_, "DSDBypass");

    // 2. DSD 截止频率 (精炼文案)
    const char* dsd_cutoff_opts[] = { "47.7kHz (标准)", "50kHz (平缓)", "60kHz (通透)", "70kHz (高解析)" };
    renderButtonGroup(y0 + 72.0f, "DSD 截止频率", dsd_cutoff_opts, 4, dsd_filter_cutoff_, "DSDCutoff");
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

    auto renderButtonGroup = [&](float row_y, const char* label, const char* const options[], int count, int& current_val, const char* id_prefix) {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(x0 + 16.0f, row_y + 4.0f), UIConfig::Color::TextMuted, label);
        if (Fonts::Small) ImGui::PopFont();

        float btn_start_x = x0 + 124.0f;
        float btn_gap = 8.0f;
        float btn_w = (w - 140.0f - btn_gap * (count - 1)) / static_cast<float>(count);
        float btn_h = 26.0f;

        for (int i = 0; i < count; ++i) {
            float bx0 = btn_start_x + i * (btn_w + btn_gap);
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

            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImVec2 txt_sz = ImGui::CalcTextSize(options[i]);
            dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, row_y + (btn_h - txt_sz.y) * 0.5f),
                        is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, options[i]);
            if (Fonts::Small) ImGui::PopFont();
        }
    };

    // 1. PCM DPLL 带宽 (精准适配按钮宽度，绝无溢出)
    const char* pcm_dpll_opts[] = { "极窄带 (超低抖动)", "标准平衡 (推荐)", "宽带 (抗时钟失锁)" };
    renderButtonGroup(y0 + 36.0f, "PCM 抖动抑制", pcm_dpll_opts, 3, pcm_dpll_band_, "PCMDPLL");

    // 2. DSD DPLL 带宽 (精准适配按钮宽度，绝无溢出)
    const char* dsd_dpll_opts[] = { "极窄带 (超低抖动)", "标准平衡 (推荐)", "宽带 (强容错防爆音)" };
    renderButtonGroup(y0 + 72.0f, "DSD 抖动抑制", dsd_dpll_opts, 3, dsd_dpll_band_, "DSDDPLL");
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

    auto renderButtonGroup = [&](float row_y, const char* label, const char* const options[], int count, int& current_val, const char* id_prefix) {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(x0 + 16.0f, row_y + 4.0f), UIConfig::Color::TextMuted, label);
        if (Fonts::Small) ImGui::PopFont();

        float btn_start_x = x0 + 124.0f;
        float btn_gap = 8.0f;
        float btn_w = (w - 140.0f - btn_gap * (count - 1)) / static_cast<float>(count);
        float btn_h = 26.0f;

        for (int i = 0; i < count; ++i) {
            float bx0 = btn_start_x + i * (btn_w + btn_gap);
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

            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImVec2 txt_sz = ImGui::CalcTextSize(options[i]);
            dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, row_y + (btn_h - txt_sz.y) * 0.5f),
                        is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, options[i]);
            if (Fonts::Small) ImGui::PopFont();
        }
    };

    // 1. THD 谐波补偿策略 (精炼文案，消除溢出)
    const char* thd_opts[] = { "超低失真 (< -122dB)", "二次谐波 (模拟胆味)", "关闭补偿" };
    renderButtonGroup(y0 + 36.0f, "THD 谐波补偿", thd_opts, 3, thd_comp_mode_, "THDComp");

    // 2. 双芯片工作模式 (精炼文案，消除溢出)
    const char* mono_opts[] = { "双芯片 8-Ch 并联 (140dB SNR)", "双芯片立体声平衡" };
    renderButtonGroup(y0 + 72.0f, "解码芯片架构", mono_opts, 2, channel_mode_, "MonoArch");

    // 3. 模拟输出模式 (精炼文案，消除溢出)
    const char* out_opts[] = { "固定后级 (Line-Out 4.2V)", "可调前级 (Pre-Out)" };
    renderButtonGroup(y0 + 108.0f, "模拟输出模式", out_opts, 2, output_level_mode_, "OutMode");

    // 4. 信号绝对相位 (精炼文案，消除溢出)
    const char* phase_opts[] = { "绝对正相 (0°)", "极性反转 (180°)" };
    renderButtonGroup(y0 + 144.0f, "输出绝对相位", phase_opts, 2, phase_invert_, "PhaseInvert");
}
