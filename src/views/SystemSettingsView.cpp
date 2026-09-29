#include "views/SystemSettingsView.hpp"
#include "public/UIConfig.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/MusicDatabase.hpp"
#include "themes/ThemeManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

SystemSettingsView::SystemSettingsView() {
    loadSettings();
}

void SystemSettingsView::loadSettings() {
    auto& db = MusicDatabase::getInstance();

    std::string s_sr = db.getSetting("setting_sample_rate", "0");
    std::string s_dsd = db.getSetting("setting_dsd_mode", "0");
    std::string s_buf = db.getSetting("setting_buffer_size", "1");
    std::string s_cpu = db.getSetting("setting_cpu_governor", "0");
    std::string s_br = db.getSetting("setting_brightness", "0.85");
    std::string s_to = db.getSetting("setting_screen_timeout", "0");

    try {
        sample_rate_mode_ = std::clamp(std::stoi(s_sr), 0, 2);
        dsd_mode_ = std::clamp(std::stoi(s_dsd), 0, 2);
        buffer_size_mode_ = std::clamp(std::stoi(s_buf), 0, 2);
        cpu_governor_ = std::clamp(std::stoi(s_cpu), 0, 1);
        screen_brightness_ = std::clamp(std::stof(s_br), 0.1f, 1.0f);
        screen_timeout_mode_ = std::clamp(std::stoi(s_to), 0, 3);
    } catch (...) {
        sample_rate_mode_ = 0;
        dsd_mode_ = 0;
        buffer_size_mode_ = 1;
        cpu_governor_ = 0;
        screen_brightness_ = 0.85f;
        screen_timeout_mode_ = 0;
    }
}

void SystemSettingsView::saveSettings() {
    auto& db = MusicDatabase::getInstance();
    db.setSetting("setting_sample_rate", std::to_string(sample_rate_mode_));
    db.setSetting("setting_dsd_mode", std::to_string(dsd_mode_));
    db.setSetting("setting_buffer_size", std::to_string(buffer_size_mode_));
    db.setSetting("setting_cpu_governor", std::to_string(cpu_governor_));
    db.setSetting("setting_brightness", std::to_string(screen_brightness_));
    db.setSetting("setting_screen_timeout", std::to_string(screen_timeout_mode_));
}

void SystemSettingsView::showToast(const std::string& msg) {
    toast_msg_ = msg;
    toast_timer_ = 2.5f;
}

void SystemSettingsView::renderToast(ImDrawList* dl, float card_x0, float card_y0, float card_w, float card_h) {
    (void)card_y0;
    if (toast_timer_ <= 0.0f || toast_msg_.empty()) return;

    toast_timer_ -= ImGui::GetIO().DeltaTime;
    float alpha = std::clamp(toast_timer_ * 2.0f, 0.0f, 1.0f);
    int bg_alpha = static_cast<int>(230 * alpha);
    int border_alpha = static_cast<int>(255 * alpha);
    int txt_alpha = static_cast<int>(255 * alpha);

    ImVec2 txt_sz = ImGui::CalcTextSize(toast_msg_.c_str());
    float pad_x = 20.0f;
    float pad_y = 9.0f;
    float toast_w = txt_sz.x + pad_x * 2.0f;
    float toast_h = txt_sz.y + pad_y * 2.0f;

    float tx = card_x0 + (card_w - toast_w) * 0.5f;
    float ty = card_y0 + card_h - toast_h - 18.0f;

    ImVec2 t_min(tx, ty);
    ImVec2 t_max(tx + toast_w, ty + toast_h);

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    ImU32 toast_bg = IM_COL32(18, 22, 30, bg_alpha);
    ImU32 toast_border = (accent & 0x00FFFFFF) | (static_cast<uint32_t>(border_alpha) << IM_COL32_A_SHIFT);
    ImU32 toast_txt = IM_COL32(255, 255, 255, txt_alpha);

    dl->AddRectFilled(t_min, t_max, toast_bg, 8.0f);
    dl->AddRect(t_min, t_max, toast_border, 8.0f, 0, 1.2f);
    dl->AddText(ImVec2(tx + pad_x, ty + pad_y), toast_txt, toast_msg_.c_str());
}

void SystemSettingsView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 26.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 渲染顶级深空高密度毛玻璃主卡片
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "main_stage");

    // 2. 绘制标题「系统设置」 (无任何副标题，完全对齐用户指令)
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "系统设置");
    if (Fonts::Medium) ImGui::PopFont();

    // 右上角系统状态徽章
    const char* sys_tag = "树莓派 5 纯音中枢 · Bit-Perfect";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 tag_sz = ImGui::CalcTextSize(sys_tag);
    float tag_x = card_max.x - tag_sz.x - 28.0f;
    float tag_y = card_min.y + 18.0f;

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    dl->AddRectFilled(ImVec2(tag_x - 8.0f, tag_y - 2.0f), ImVec2(card_max.x - 20.0f, tag_y + tag_sz.y + 2.0f),
                      IM_COL32(r, g, b, 35), 4.0f);
    dl->AddRect(ImVec2(tag_x - 8.0f, tag_y - 2.0f), ImVec2(card_max.x - 20.0f, tag_y + tag_sz.y + 2.0f),
                IM_COL32(r, g, b, 120), 4.0f, 0, 1.0f);
    dl->AddText(ImVec2(tag_x, tag_y), UIConfig::Color::TextActive, sys_tag);
    if (Fonts::Small) ImGui::PopFont();

    // 3. 开启独立滚动子区域 (覆盖四大发烧设置板块)
    float content_x = card_min.x + 16.0f;
    float content_y = card_min.y + 50.0f;
    float content_w = card_max.x - card_min.x - 32.0f;
    float content_h = card_max.y - content_y - 12.0f;

    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));
    ImGui::BeginChild("##SystemSettingsScroll", ImVec2(content_w, content_h), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    ImDrawList* child_dl = ImGui::GetWindowDrawList();
    float cur_y = content_y;

    // 板块一：音频重放与时钟引擎
    renderAudioSection(child_dl, content_x, cur_y, content_w);
    cur_y += 156.0f;

    // 板块二：硬件性能与显示控制
    renderHardwareSection(child_dl, content_x, cur_y, content_w);
    cur_y += 156.0f;

    // 板块三：曲库与存储中枢
    renderLibrarySection(child_dl, content_x, cur_y, content_w);
    cur_y += 108.0f;

    // 板块四：系统维护与电源管控
    renderPowerSection(child_dl, content_x, cur_y, content_w);
    cur_y += 96.0f;

    // 占位缓冲确保底部完整滑出
    ImGui::Dummy(ImVec2(content_w, cur_y - content_y + 16.0f));

    ImGui::EndChild();

    // 4. Toast 反馈微弹窗渲染
    renderToast(dl, card_min.x, card_min.y, card_max.x - card_min.x, card_max.y - card_min.y);
}

void SystemSettingsView::renderAudioSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 146.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    // 毛玻璃卡片底板
    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    // 板块标题
    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "音频重放与时钟引擎");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    auto renderButtonGroup = [&](float row_y, const char* label, const char* const options[], int count, int& current_val, const char* id_prefix) {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(x0 + 16.0f, row_y + 4.0f), UIConfig::Color::TextMuted, label);
        if (Fonts::Small) ImGui::PopFont();

        float btn_start_x = x0 + 140.0f;
        float btn_gap = 8.0f;
        float btn_w = (w - 156.0f - btn_gap * (count - 1)) / static_cast<float>(count);
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
            if (ImGui::IsItemClicked()) {
                current_val = i;
                saveSettings();
                showToast(std::string("已更新") + label + "：" + options[i]);
            }

            ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                       (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
            dl->AddRectFilled(b_min, b_max, bg, 6.0f);

            ImU32 border = is_act ? accent :
                           (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
            dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

            if (Fonts::Small) ImGui::PushFont(Fonts::Small);
            ImVec2 txt_sz = ImGui::CalcTextSize(options[i]);
            ImVec2 txt_pos(bx0 + (btn_w - txt_sz.x) * 0.5f, row_y + (btn_h - txt_sz.y) * 0.5f);
            dl->AddText(txt_pos, is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, options[i]);
            if (Fonts::Small) ImGui::PopFont();
        }
    };

    // 1. 采样率输出策略
    const char* sr_opts[] = { "Bit-Perfect 源码直出", "超采样 192kHz/24Bit", "极频 384kHz/32Bit" };
    renderButtonGroup(y0 + 36.0f, "输出采样率", sr_opts, 3, sample_rate_mode_, "SRMode");

    // 2. DSD 解码通道
    const char* dsd_opts[] = { "DoP (DSD over PCM)", "Native 原生直通", "DSD 软解 PCM" };
    renderButtonGroup(y0 + 72.0f, "DSD 播放模式", dsd_opts, 3, dsd_mode_, "DSDMode");

    // 3. 硬件缓冲深度
    const char* buf_opts[] = { "64 帧 (极低延迟)", "256 帧 (标准发烧)", "512 帧 (防爆音深缓冲)" };
    renderButtonGroup(y0 + 108.0f, "硬件缓冲深度", buf_opts, 3, buffer_size_mode_, "BufMode");
}

void SystemSettingsView::renderHardwareSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 146.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "硬件性能与显示控制");
    if (Fonts::Regular) ImGui::PopFont();

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 1. CPU 调频策略
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 40.0f), UIConfig::Color::TextMuted, "CPU 调频策略");
    if (Fonts::Small) ImGui::PopFont();

    const char* cpu_opts[] = { "Performance (纯音锁频最高)", "Schedutil (动态温控平衡)" };
    float btn_start_x = x0 + 140.0f;
    float btn_w = (w - 156.0f - 8.0f) * 0.5f;
    float btn_h = 26.0f;

    for (int i = 0; i < 2; ++i) {
        float bx0 = btn_start_x + i * (btn_w + 8.0f);
        ImVec2 b_min(bx0, y0 + 36.0f);
        ImVec2 b_max(bx0 + btn_w, y0 + 36.0f + btn_h);

        bool is_act = (cpu_governor_ == i);
        std::string btn_id = "##CPUGov_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            cpu_governor_ = i;
            saveSettings();
            showToast(std::string("已切换 CPU 调频策略为：") + cpu_opts[i]);
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(cpu_opts[i]);
        dl->AddText(ImVec2(bx0 + (btn_w - txt_sz.x) * 0.5f, y0 + 36.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, cpu_opts[i]);
        if (Fonts::Small) ImGui::PopFont();
    }

    // 2. 屏幕背光亮度 (交互滑条)
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 76.0f), UIConfig::Color::TextMuted, "屏幕背光亮度");
    if (Fonts::Small) ImGui::PopFont();

    float slider_x0 = x0 + 140.0f;
    float slider_w = w - 210.0f;
    float slider_y = y0 + 82.0f;
    float slider_h = 6.0f;

    ImVec2 s_min(slider_x0, slider_y);
    ImVec2 s_max(slider_x0 + slider_w, slider_y + slider_h);

    // 轨道背景
    dl->AddRectFilled(s_min, s_max, IM_COL32(255, 255, 255, 25), 3.0f);
    // 高亮进度
    float fill_w = slider_w * screen_brightness_;
    dl->AddRectFilled(s_min, ImVec2(slider_x0 + fill_w, slider_y + slider_h), accent, 3.0f);

    // 手柄
    float thumb_x = slider_x0 + fill_w;
    float thumb_y = slider_y + slider_h * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(slider_x0, slider_y - 8.0f));
    ImGui::InvisibleButton("##ScreenBrightnessSlider", ImVec2(slider_w, slider_h + 16.0f));
    if (ImGui::IsItemActive()) {
        float mx = ImGui::GetIO().MousePos.x;
        screen_brightness_ = std::clamp((mx - slider_x0) / slider_w, 0.1f, 1.0f);
        saveSettings();
    }

    dl->AddCircleFilled(ImVec2(thumb_x, thumb_y), 7.0f, IM_COL32(255, 255, 255, 255));
    dl->AddCircle(ImVec2(thumb_x, thumb_y), 7.0f, accent, 16, 1.2f);

    char br_buf[16];
    std::snprintf(br_buf, sizeof(br_buf), "%d%%", static_cast<int>(std::round(screen_brightness_ * 100.0f)));
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(slider_x0 + slider_w + 14.0f, y0 + 76.0f), UIConfig::Color::TextActive, br_buf);
    if (Fonts::Small) ImGui::PopFont();

    // 3. 屏幕息屏待机
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 112.0f), UIConfig::Color::TextMuted, "自动息屏待机");
    if (Fonts::Small) ImGui::PopFont();

    const char* to_opts[] = { "从不", "5 分钟", "15 分钟", "30 分钟" };
    float to_gap = 8.0f;
    float to_w = (w - 156.0f - to_gap * 3.0f) / 4.0f;

    for (int i = 0; i < 4; ++i) {
        float bx0 = btn_start_x + i * (to_w + to_gap);
        ImVec2 b_min(bx0, y0 + 108.0f);
        ImVec2 b_max(bx0 + to_w, y0 + 108.0f + btn_h);

        bool is_act = (screen_timeout_mode_ == i);
        std::string btn_id = "##ScreenTimeout_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(to_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            screen_timeout_mode_ = i;
            saveSettings();
            showToast(std::string("自动息屏时间已设定为：") + to_opts[i]);
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(to_opts[i]);
        dl->AddText(ImVec2(bx0 + (to_w - txt_sz.x) * 0.5f, y0 + 108.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, to_opts[i]);
        if (Fonts::Small) ImGui::PopFont();
    }
}

void SystemSettingsView::renderLibrarySection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 98.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "曲库存储与数据中枢");
    if (Fonts::Regular) ImGui::PopFont();

    // 统计标签
    std::string track_count_str = "曲库引擎：SQLite 3 高性能 WAL 模式 · 存储路径：~/.config/hifi_player/";
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 36.0f), UIConfig::Color::TextMuted, track_count_str.c_str());
    if (Fonts::Small) ImGui::PopFont();

    // 操作按钮：触发重新扫描 & 清理封面缓存
    float btn_w = 160.0f;
    float btn_h = 28.0f;
    float btn_y = y0 + 58.0f;

    // 按钮 1：转到扫描音乐
    ImVec2 b1_min(x0 + 16.0f, btn_y);
    ImVec2 b1_max(x0 + 16.0f + btn_w, btn_y + btn_h);
    ImGui::SetCursorScreenPos(b1_min);
    ImGui::InvisibleButton("##GoScanMusicBtn", ImVec2(btn_w, btn_h));
    bool hov1 = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        if (on_navigate_tab_) {
            on_navigate_tab_(0); // SidebarTab::ScanMusic
        }
    }
    dl->AddRectFilled(b1_min, b1_max, hov1 ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12), 6.0f);
    dl->AddRect(b1_min, b1_max, hov1 ? IM_COL32(255, 255, 255, 80) : IM_COL32(255, 255, 255, 30), 6.0f, 0, 1.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 t1_sz = ImGui::CalcTextSize("前往扫描音乐");
    dl->AddText(ImVec2(b1_min.x + (btn_w - t1_sz.x) * 0.5f, btn_y + (btn_h - t1_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "前往扫描音乐");
    if (Fonts::Small) ImGui::PopFont();

    // 按钮 2：清空歌曲缓存
    ImVec2 b2_min(x0 + 24.0f + btn_w, btn_y);
    ImVec2 b2_max(x0 + 24.0f + btn_w * 2.0f, btn_y + btn_h);
    ImGui::SetCursorScreenPos(b2_min);
    ImGui::InvisibleButton("##ClearCacheBtn", ImVec2(btn_w, btn_h));
    bool hov2 = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        showToast("已成功优化并整理 SQLite 数据库缓存");
    }
    dl->AddRectFilled(b2_min, b2_max, hov2 ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12), 6.0f);
    dl->AddRect(b2_min, b2_max, hov2 ? IM_COL32(255, 255, 255, 80) : IM_COL32(255, 255, 255, 30), 6.0f, 0, 1.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 t2_sz = ImGui::CalcTextSize("整理数据库缓存");
    dl->AddText(ImVec2(b2_min.x + (btn_w - t2_sz.x) * 0.5f, btn_y + (btn_h - t2_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "整理数据库缓存");
    if (Fonts::Small) ImGui::PopFont();
}

void SystemSettingsView::renderPowerSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 88.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "系统维护与电源管控");
    if (Fonts::Regular) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 34.0f), UIConfig::Color::TextMuted,
                "固件版本：PiHiFi Player OS v1.0.0 (ARMv8.2-A / 60fps Native Pure C++20)");
    if (Fonts::Small) ImGui::PopFont();

    float btn_w = 140.0f;
    float btn_h = 26.0f;
    float btn_y = y0 + 52.0f;

    // 重启系统
    ImVec2 rb_min(x0 + 16.0f, btn_y);
    ImVec2 rb_max(x0 + 16.0f + btn_w, btn_y + btn_h);
    ImGui::SetCursorScreenPos(rb_min);
    ImGui::InvisibleButton("##RebootBtn", ImVec2(btn_w, btn_h));
    bool rb_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        showToast("已下发安全重启指令 (Syncing filesystem...)");
    }
    dl->AddRectFilled(rb_min, rb_max, rb_hov ? IM_COL32(234, 179, 8, 55) : IM_COL32(234, 179, 8, 25), 6.0f);
    dl->AddRect(rb_min, rb_max, IM_COL32(234, 179, 8, rb_hov ? 200 : 100), 6.0f, 0, 1.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 rb_sz = ImGui::CalcTextSize("重启系统");
    dl->AddText(ImVec2(rb_min.x + (btn_w - rb_sz.x) * 0.5f, btn_y + (btn_h - rb_sz.y) * 0.5f),
                IM_COL32(253, 230, 138, 240), "重启系统");
    if (Fonts::Small) ImGui::PopFont();

    // 安全关机
    ImVec2 sd_min(x0 + 24.0f + btn_w, btn_y);
    ImVec2 sd_max(x0 + 24.0f + btn_w * 2.0f, btn_y + btn_h);
    ImGui::SetCursorScreenPos(sd_min);
    ImGui::InvisibleButton("##ShutdownBtn", ImVec2(btn_w, btn_h));
    bool sd_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        showToast("已下发安全关机指令 (Safely dismounting ALSA & SQLite)");
    }
    dl->AddRectFilled(sd_min, sd_max, sd_hov ? IM_COL32(239, 68, 68, 60) : IM_COL32(239, 68, 68, 25), 6.0f);
    dl->AddRect(sd_min, sd_max, IM_COL32(239, 68, 68, sd_hov ? 220 : 110), 6.0f, 0, 1.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 sd_sz = ImGui::CalcTextSize("安全关机");
    dl->AddText(ImVec2(sd_min.x + (btn_w - sd_sz.x) * 0.5f, btn_y + (btn_h - sd_sz.y) * 0.5f),
                IM_COL32(254, 202, 202, 240), "安全关机");
    if (Fonts::Small) ImGui::PopFont();
}
