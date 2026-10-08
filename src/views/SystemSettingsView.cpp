#include "views/SystemSettingsView.hpp"
#include "public/UIConfig.hpp"
#include "public/Platform.hpp"
#include "public/Font.hpp"
#include "widgets/GlassCardRenderer.hpp"
#include "tools/MusicDatabase.hpp"
#include "themes/ThemeManager.hpp"
#include "tools/UpdateManager.hpp"
#include "audio_engine/AudioEngine.hpp"
#include "tools/PlayerAdmin.hpp"
#include "tools/NetworkTool.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

SystemSettingsView::SystemSettingsView() {
    loadSettings();
}

void SystemSettingsView::applyHardwareBufferSize(int mode) {
    uint32_t frames = 256;
    if (mode == 0) frames = 64;
    else if (mode == 1) frames = 256;
    else if (mode == 2) frames = 512;
    audio_engine::AudioEngine::getInstance().setHardwareBufferSize(frames);
}

void SystemSettingsView::applyFadeDuration(int mode) {
    float sec = 0.5f;
    switch (mode) {
        case 0: sec = 0.0f; break; // 关闭
        case 1: sec = 0.3f; break; // 极速
        case 2: sec = 0.5f; break; // 发烧标准 (默认)
        case 3: sec = 1.0f; break; // 悠扬慢淡
        default: sec = 0.5f; break;
    }
    PlayerAdmin::getInstance().setFadeDuration(sec);
}

void SystemSettingsView::applyCpuGovernor(int mode) {
#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    const char* gov = (mode == 0) ? "performance" : "schedutil";
    for (int cpu = 0; cpu < 8; ++cpu) {
        char path[128];
        std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpu);
        FILE* fp = std::fopen(path, "w");
        if (fp) {
            std::fputs(gov, fp);
            std::fclose(fp);
        }
    }
#else
    (void)mode;
#endif
}

void SystemSettingsView::applyScreenBrightness(float brightness) {
#if defined(__linux__) && !defined(HIFI_PLATFORM_MAC)
    int val = std::clamp(static_cast<int>(brightness * 255.0f), 10, 255);
    FILE* fp = std::fopen("/sys/class/backlight/rpi_backlight/brightness", "w");
    if (!fp) {
        fp = std::fopen("/sys/class/backlight/10-0045/brightness", "w");
    }
    if (fp) {
        std::fprintf(fp, "%d\n", val);
        std::fclose(fp);
    }
#else
    (void)brightness;
#endif
}

void SystemSettingsView::loadSettings() {
    auto& db = MusicDatabase::getInstance();

    std::string s_sr = db.getSetting("setting_sample_rate", "0");
    std::string s_dsd = db.getSetting("setting_dsd_mode", "0");
    std::string s_buf = db.getSetting("setting_buffer_size", "1");
    std::string s_fade = db.getSetting("setting_fade_duration_mode", "2");
    std::string s_cpu = db.getSetting("setting_cpu_governor", "0");
    std::string s_br = db.getSetting("setting_brightness", "0.85");
    std::string s_to = db.getSetting("setting_screen_timeout", "0");
    std::string s_idle = db.getSetting("setting_idle_fullscreen", "4");

    try {
        sample_rate_mode_ = std::clamp(std::stoi(s_sr), 0, 2);
        dsd_mode_ = std::clamp(std::stoi(s_dsd), 0, 2);
        buffer_size_mode_ = std::clamp(std::stoi(s_buf), 0, 2);
        fade_duration_mode_ = std::clamp(std::stoi(s_fade), 0, 3);
        cpu_governor_ = std::clamp(std::stoi(s_cpu), 0, 1);
        screen_brightness_ = std::clamp(std::stof(s_br), 0.1f, 1.0f);
        screen_timeout_mode_ = std::clamp(std::stoi(s_to), 0, 3);
        idle_fullscreen_mode_ = std::clamp(std::stoi(s_idle), 0, 4);
    } catch (...) {
        sample_rate_mode_ = 0;
        dsd_mode_ = 0;
        buffer_size_mode_ = 1;
        fade_duration_mode_ = 2;
        cpu_governor_ = 0;
        screen_brightness_ = 0.85f;
        screen_timeout_mode_ = 0;
        idle_fullscreen_mode_ = 4;
    }

    applyHardwareBufferSize(buffer_size_mode_);
    applyFadeDuration(fade_duration_mode_);
    applyCpuGovernor(cpu_governor_);
    applyScreenBrightness(screen_brightness_);
}

void SystemSettingsView::saveSettings() {
    auto& db = MusicDatabase::getInstance();
    db.setSetting("setting_sample_rate", std::to_string(sample_rate_mode_));
    db.setSetting("setting_dsd_mode", std::to_string(dsd_mode_));
    db.setSetting("setting_buffer_size", std::to_string(buffer_size_mode_));
    db.setSetting("setting_fade_duration_mode", std::to_string(fade_duration_mode_));
    db.setSetting("setting_cpu_governor", std::to_string(cpu_governor_));
    db.setSetting("setting_brightness", std::to_string(screen_brightness_));
    db.setSetting("setting_screen_timeout", std::to_string(screen_timeout_mode_));
    db.setSetting("setting_idle_fullscreen", std::to_string(idle_fullscreen_mode_));
}

void SystemSettingsView::render(float x, float y, float w, float h) {
    float margin_x = UIConfig::Layout::ContainerMarginX; // 16.0f
    float margin_y = UIConfig::Layout::ContainerMarginY; // 16.0f
    ImVec2 card_min(x + margin_x, y + margin_y);
    ImVec2 card_max(x + w - margin_x, y + h - 86.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 渲染顶级深空高密度毛玻璃主卡片
    GlassCardRenderer::drawCard(dl, card_min, card_max, UIConfig::Layout::ContainerRounding, "main_stage");

    // 2. 绘制标题「系统设置」 (右上角无任何标签，完全对齐用户指令)
    ImVec2 title_pos(card_min.x + 20.0f, card_min.y + 16.0f);
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(title_pos, UIConfig::Color::TextActive, "系统设置");
    if (Fonts::Medium) ImGui::PopFont();

    // 3. 开启独立滚动子区域 (覆盖四大发烧设置板块，支持滑轮、触控拖拽与半透明纤细滚动条)
    float content_x = card_min.x + 16.0f;
    float content_y = card_min.y + 54.0f;
    float content_w = card_max.x - card_min.x - 32.0f;
    float content_h = card_max.y - content_y - 12.0f;

    ImU32 accent = ThemeManager::getInstance().getAccentColor();

    ImGui::SetCursorScreenPos(ImVec2(content_x, content_y));

    // 精美半透明纤细滚动条样式
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 255, 255, 45));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 255, 255, 90));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, accent);

    if (ImGui::BeginChild("##SystemSettingsScroll", ImVec2(content_w, content_h), false,
                          ImGuiWindowFlags_NoBackground)) {

        // 支持触控屏与鼠标在背景区域直接拖拽平滑滚动
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
            float drag_dy = std::clamp(ImGui::GetIO().MouseDelta.y, -40.0f, 40.0f);
            if (drag_dy != 0.0f) {
                ImGui::SetScrollY(ImGui::GetScrollY() - drag_dy);
            }
        }

        ImDrawList* child_dl = ImGui::GetWindowDrawList();
        float section_w = ImGui::GetContentRegionAvail().x; // 扣除滚动条后的可用内容宽度

        // 板块一：音频重放与时钟引擎 (182px, 含采样率/DSD/缓冲深度/切歌淡入淡出)
        ImVec2 p_audio = ImGui::GetCursorScreenPos();
        renderAudioSection(child_dl, p_audio.x, p_audio.y, section_w);
        ImGui::SetCursorScreenPos(ImVec2(p_audio.x, p_audio.y + 182.0f));
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 板块二：硬件性能与显示控制 (182px, 含空余时间显示全屏)
        ImVec2 p_hw = ImGui::GetCursorScreenPos();
        renderHardwareSection(child_dl, p_hw.x, p_hw.y, section_w);
        ImGui::SetCursorScreenPos(ImVec2(p_hw.x, p_hw.y + 182.0f));
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // 仅在非 iPad / 非 iOS 平台 (如树莓派/Linux) 呈现底层固件更新与系统维护网络调试面板
        if (!Platform::isIPad() && !Platform::isIOS()) {
            // 板块三：固件与在线更新 (OTA)
            ImVec2 p_update = ImGui::GetCursorScreenPos();
            float update_card_h = renderUpdateSection(child_dl, p_update.x, p_update.y, section_w);
            ImGui::SetCursorScreenPos(ImVec2(p_update.x, p_update.y + update_card_h));
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            // 板块四：系统维护、网络调试与电源管控
            ImVec2 p_power = ImGui::GetCursorScreenPos();
            float power_card_h = renderPowerSection(child_dl, p_power.x, p_power.y, section_w);
            ImGui::SetCursorScreenPos(ImVec2(p_power.x, p_power.y + power_card_h));
            ImGui::Dummy(ImVec2(0.0f, 16.0f)); // 底部缓冲留白
        } else {
            ImGui::Dummy(ImVec2(0.0f, 16.0f)); // 底部缓冲留白
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    if (confirm_action_ != ConfirmAction::None) {
        renderPowerConfirmModal();
    }
}

void SystemSettingsView::renderAudioSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 182.0f;
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
        const float max_btn_w = 200.0f;
        float btn_w = std::min((w - 156.0f - btn_gap * (count - 1)) / static_cast<float>(count), max_btn_w);
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
                if (std::strcmp(id_prefix, "BufMode") == 0) {
                    applyHardwareBufferSize(current_val);
                } else if (std::strcmp(id_prefix, "FadeMode") == 0) {
                    applyFadeDuration(current_val);
                }
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

    // 4. 切歌平滑过渡 (淡入淡出)
    const char* fade_opts[] = { "关闭 (直接切歌)", "0.3 秒 (极速)", "0.5 秒 (发烧标准)", "1.0 秒 (悠扬慢淡)" };
    renderButtonGroup(y0 + 144.0f, "切歌过渡效果", fade_opts, 4, fade_duration_mode_, "FadeMode");
}

void SystemSettingsView::renderHardwareSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 182.0f;
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
            applyCpuGovernor(cpu_governor_);
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
        applyScreenBrightness(screen_brightness_);
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
    const float max_to_w = 140.0f;
    float to_w = std::min((w - 156.0f - to_gap * 3.0f) / 4.0f, max_to_w);

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

    // 4. 空余时间显示全屏 (默认 15 秒)
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 148.0f), UIConfig::Color::TextMuted, "空余时间显示全屏");
    if (Fonts::Small) ImGui::PopFont();

    const char* idle_opts[] = { "15 秒", "30 秒", "1 分钟", "5 分钟", "永不" };
    float idle_gap = 8.0f;
    const float max_idle_btn_w = 120.0f;
    float idle_btn_w = std::min((w - 156.0f - idle_gap * 4.0f) / 5.0f, max_idle_btn_w);

    for (int i = 0; i < 5; ++i) {
        float bx0 = btn_start_x + i * (idle_btn_w + idle_gap);
        ImVec2 b_min(bx0, y0 + 144.0f);
        ImVec2 b_max(bx0 + idle_btn_w, y0 + 144.0f + btn_h);

        bool is_act = (idle_fullscreen_mode_ == i);
        std::string btn_id = "##IdleFullscreen_" + std::to_string(i);

        ImGui::SetCursorScreenPos(b_min);
        ImGui::InvisibleButton(btn_id.c_str(), ImVec2(idle_btn_w, btn_h));

        bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            idle_fullscreen_mode_ = i;
            saveSettings();
            if (on_idle_fullscreen_changed_) {
                on_idle_fullscreen_changed_(getIdleFullscreenSeconds());
            }
        }

        ImU32 bg = is_act ? IM_COL32(r, g, b, 70) :
                   (hov ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
        dl->AddRectFilled(b_min, b_max, bg, 6.0f);
        ImU32 border = is_act ? accent : (hov ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 24));
        dl->AddRect(b_min, b_max, border, 6.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 txt_sz = ImGui::CalcTextSize(idle_opts[i]);
        dl->AddText(ImVec2(bx0 + (idle_btn_w - txt_sz.x) * 0.5f, y0 + 144.0f + (btn_h - txt_sz.y) * 0.5f),
                    is_act ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal, idle_opts[i]);
        if (Fonts::Small) ImGui::PopFont();
    }
}

float SystemSettingsView::renderUpdateSection(ImDrawList* dl, float x0, float y0, float w) {
    auto& um = UpdateManager::getInstance();
    UpdateStatus st = um.getStatus();

    bool has_logs = (st == UpdateStatus::UpdateAvailable && !um.getUpdateLog().empty());
    float h = has_logs ? 148.0f : 100.0f;
    if (st == UpdateStatus::UpdateFailed && !um.getErrorMessage().empty()) {
        h = 130.0f;
    }

    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "固件与在线更新 (OTA Update)");
    if (Fonts::Regular) ImGui::PopFont();

    // 状态与版本信息文本
    std::string ver_info = "当前固件：" + um.getCurrentCommitHash() + " (" + um.getCurrentCommitDate() +
                           ") · 分支: " + um.getCurrentBranch();
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 34.0f), UIConfig::Color::TextMuted, ver_info.c_str());

    float btn_h = 26.0f;
    float btn_y = y0 + 58.0f;

    ImU32 accent = ThemeManager::getInstance().getAccentColor();
    const ImU32 r = (accent >> IM_COL32_R_SHIFT) & 0xFF;
    const ImU32 g = (accent >> IM_COL32_G_SHIFT) & 0xFF;
    const ImU32 b = (accent >> IM_COL32_B_SHIFT) & 0xFF;

    // 1. 检查更新按钮
    float chk_btn_w = 110.0f;
    ImVec2 chk_min(x0 + 16.0f, btn_y);
    ImVec2 chk_max(chk_min.x + chk_btn_w, btn_y + btn_h);
    ImGui::SetCursorScreenPos(chk_min);

    bool is_busy = (st == UpdateStatus::Checking || st == UpdateStatus::Updating);
    if (!is_busy) {
        ImGui::InvisibleButton("##CheckUpdateBtn", ImVec2(chk_btn_w, btn_h));
        bool chk_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            um.checkForUpdatesAsync();
        }
        dl->AddRectFilled(chk_min, chk_max, chk_hov ? IM_COL32(r, g, b, 70) : IM_COL32(255, 255, 255, 16), 6.0f);
        dl->AddRect(chk_min, chk_max, chk_hov ? accent : IM_COL32(255, 255, 255, 30), 6.0f, 0, 1.0f);
        ImVec2 sz = ImGui::CalcTextSize("检查更新");
        dl->AddText(ImVec2(chk_min.x + (chk_btn_w - sz.x) * 0.5f, btn_y + (btn_h - sz.y) * 0.5f),
                    UIConfig::Color::TextActive, "检查更新");
    } else {
        dl->AddRectFilled(chk_min, chk_max, IM_COL32(255, 255, 255, 10), 6.0f);
        dl->AddRect(chk_min, chk_max, IM_COL32(255, 255, 255, 20), 6.0f, 0, 1.0f);
        ImVec2 sz = ImGui::CalcTextSize(st == UpdateStatus::Checking ? "检测中..." : "编译中...");
        dl->AddText(ImVec2(chk_min.x + (chk_btn_w - sz.x) * 0.5f, btn_y + (btn_h - sz.y) * 0.5f),
                    UIConfig::Color::TextMuted, st == UpdateStatus::Checking ? "检测中..." : "编译中...");
    }

    // 2. 状态标签或动作按钮 (使用精细硬件状态呼吸灯 + 纯净 CJK 文本，杜绝 Emoji 字体缺失导致 '?' 乱码)
    float status_x = chk_max.x + 14.0f;
    float dot_radius = 3.5f;
    float dot_cy = btn_y + btn_h * 0.5f;
    float text_x = status_x + 14.0f;

    if (st == UpdateStatus::Idle) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(148, 163, 184, 200));
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), UIConfig::Color::TextMuted, "点击左侧按钮向远端仓库请求检测最新固件");
    } else if (st == UpdateStatus::Checking) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(250, 204, 21, 240));
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), IM_COL32(250, 204, 21, 240), "正在连接远端仓库 (git fetch)...");
    } else if (st == UpdateStatus::UpToDate) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(52, 211, 153, 240));
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), IM_COL32(52, 211, 153, 240), "当前固件已是最新版本 (与远端保持同步)");
    } else if (st == UpdateStatus::UpdateAvailable) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(251, 146, 60, 240));
        std::string notice = "发现 " + std::to_string(um.getNewCommitCount()) + " 个新提交 (最新: " + um.getRemoteCommitHash() + ")";
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), IM_COL32(251, 146, 60, 240), notice.c_str());

        // 立即更新按钮
        float upd_btn_w = 160.0f;
        float upd_btn_x = text_x + ImGui::CalcTextSize(notice.c_str()).x + 16.0f;
        if (upd_btn_x + upd_btn_w > x0 + w - 16.0f) {
            upd_btn_x = x0 + w - 16.0f - upd_btn_w;
        }
        ImVec2 upd_min(upd_btn_x, btn_y);
        ImVec2 upd_max(upd_min.x + upd_btn_w, btn_y + btn_h);
        ImGui::SetCursorScreenPos(upd_min);
        ImGui::InvisibleButton("##ExecuteUpdateBtn", ImVec2(upd_btn_w, btn_h));
        bool upd_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            um.executeUpdateAsync();
        }
        dl->AddRectFilled(upd_min, upd_max, upd_hov ? IM_COL32(16, 185, 129, 90) : IM_COL32(16, 185, 129, 50), 6.0f);
        dl->AddRect(upd_min, upd_max, IM_COL32(16, 185, 129, upd_hov ? 240 : 150), 6.0f, 0, 1.0f);
        ImVec2 upd_sz = ImGui::CalcTextSize("一键在线更新并编译");
        dl->AddText(ImVec2(upd_min.x + (upd_btn_w - upd_sz.x) * 0.5f, btn_y + (btn_h - upd_sz.y) * 0.5f),
                    IM_COL32(255, 255, 255, 240), "一键在线更新并编译");

        if (has_logs) {
            float log_y = btn_y + btn_h + 8.0f;
            std::string first_log = um.getUpdateLog();
            dl->AddText(ImVec2(x0 + 16.0f, log_y), IM_COL32(203, 213, 225, 220), first_log.c_str());
        }
    } else if (st == UpdateStatus::Updating) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(96, 165, 250, 240));
        std::string prog_msg = um.getProgressMessage();
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), IM_COL32(96, 165, 250, 240), prog_msg.c_str());
    } else if (st == UpdateStatus::UpdateSuccess) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(52, 211, 153, 240));
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), IM_COL32(52, 211, 153, 240), "固件重编译成功！");

        float rst_btn_w = 110.0f;
        float rst_btn_x = text_x + 140.0f;
        ImVec2 rst_min(rst_btn_x, btn_y);
        ImVec2 rst_max(rst_min.x + rst_btn_w, btn_y + btn_h);
        ImGui::SetCursorScreenPos(rst_min);
        ImGui::InvisibleButton("##RestartServiceBtn", ImVec2(rst_btn_w, btn_h));
        bool rst_hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            um.restartApplication();
        }
        dl->AddRectFilled(rst_min, rst_max, rst_hov ? IM_COL32(59, 130, 246, 80) : IM_COL32(59, 130, 246, 40), 6.0f);
        dl->AddRect(rst_min, rst_max, IM_COL32(59, 130, 246, 180), 6.0f, 0, 1.0f);
        ImVec2 rst_sz = ImGui::CalcTextSize("立即重启生效");
        dl->AddText(ImVec2(rst_min.x + (rst_btn_w - rst_sz.x) * 0.5f, btn_y + (btn_h - rst_sz.y) * 0.5f),
                    IM_COL32(255, 255, 255, 240), "立即重启生效");
    } else if (st == UpdateStatus::UpdateFailed) {
        dl->AddCircleFilled(ImVec2(status_x + dot_radius, dot_cy), dot_radius, IM_COL32(239, 68, 68, 240));
        std::string err = "[更新失败] " + um.getErrorMessage();
        if (err.length() > 60) err = err.substr(0, 57) + "...";
        dl->AddText(ImVec2(text_x, btn_y + 5.0f), IM_COL32(239, 68, 68, 240), err.c_str());
    }

    if (Fonts::Small) ImGui::PopFont();
    return h;
}

float SystemSettingsView::renderPowerSection(ImDrawList* dl, float x0, float y0, float w) {
    float h = 142.0f;
    ImVec2 p0(x0, y0);
    ImVec2 p1(x0 + w, y0 + h);

    dl->AddRectFilled(p0, p1, IM_COL32(20, 26, 36, 175), 10.0f);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 20), 10.0f, 0, 1.0f);
    dl->AddLine(ImVec2(p0.x + 10.0f, p0.y), ImVec2(p1.x - 10.0f, p0.y), IM_COL32(255, 255, 255, 38), 1.0f);

    if (Fonts::Regular) ImGui::PushFont(Fonts::Regular);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 10.0f), UIConfig::Color::TextActive, "系统维护、网络调试与电源管控");
    if (Fonts::Regular) ImGui::PopFont();

    NetworkInfo net = NetworkTool::getNetworkInfo();

    // 1. 固件版本
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 34.0f), UIConfig::Color::TextMuted,
                "固件版本：PiHiFi Player OS v1.1.0 (ARMv8.2-A / 60fps Native Pure C++20)");

    // 2. 实时网络与调试 IP
    std::string ip_str = net.is_connected ? (net.ip + " (" + net.interface_name + ")") : "未连接网络";
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 54.0f), UIConfig::Color::TextMuted, "局域网 IP：");
    dl->AddText(ImVec2(x0 + 84.0f, y0 + 54.0f), net.is_connected ? IM_COL32(52, 211, 153, 255) : IM_COL32(248, 113, 113, 255), ip_str.c_str());

    if (!net.mac.empty() && net.mac != "--") {
        std::string mac_str = " · MAC: " + net.mac;
        ImVec2 ip_sz = ImGui::CalcTextSize(ip_str.c_str());
        dl->AddText(ImVec2(x0 + 88.0f + ip_sz.x, y0 + 54.0f), UIConfig::Color::TextMuted, mac_str.c_str());
    }

    // 3. Wi-Fi 连接信息与刷新按钮
    dl->AddText(ImVec2(x0 + 16.0f, y0 + 74.0f), UIConfig::Color::TextMuted, "Wi-Fi 状态：");
    std::string wifi_str = net.wifi_ssid + " (" + net.wifi_signal + ")";
    dl->AddText(ImVec2(x0 + 84.0f, y0 + 74.0f), net.is_connected ? IM_COL32(56, 189, 248, 255) : UIConfig::Color::TextMuted, wifi_str.c_str());

    // 专属「刷新网络」独立大按钮 (靠右排布，面积充裕，适合 7 寸触控屏单指点击)
    float ref_btn_w = 92.0f;
    float ref_btn_h = 30.0f;
    float ref_btn_x = x0 + w - ref_btn_w - 18.0f;
    float ref_btn_y = y0 + 52.0f;

    ImGui::SetCursorScreenPos(ImVec2(ref_btn_x, ref_btn_y));
    ImGui::InvisibleButton("##RefreshNetBtn", ImVec2(ref_btn_w, ref_btn_h));
    bool ref_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        NetworkTool::refreshAsync();
        network_refresh_feedback_timer_ = 1.5f; // 显示 1.5 秒绿色刷新反馈
    }

    if (network_refresh_feedback_timer_ > 0.0f) {
        network_refresh_feedback_timer_ -= ImGui::GetIO().DeltaTime;
    }

    bool is_just_refreshed = (network_refresh_feedback_timer_ > 0.0f);
    const char* ref_label = is_just_refreshed ? "已刷新" : "刷新网络";

    ImU32 btn_bg = is_just_refreshed ? IM_COL32(52, 211, 153, 50) : (ref_hov ? IM_COL32(255, 255, 255, 40) : IM_COL32(255, 255, 255, 20));
    ImU32 btn_border = is_just_refreshed ? IM_COL32(52, 211, 153, 180) : (ref_hov ? IM_COL32(255, 255, 255, 120) : IM_COL32(255, 255, 255, 45));
    ImU32 btn_text_col = is_just_refreshed ? IM_COL32(52, 211, 153, 255) : (ref_hov ? UIConfig::Color::TextActive : UIConfig::Color::TextNormal);

    dl->AddRectFilled(ImVec2(ref_btn_x, ref_btn_y), ImVec2(ref_btn_x + ref_btn_w, ref_btn_y + ref_btn_h), btn_bg, 6.0f);
    dl->AddRect(ImVec2(ref_btn_x, ref_btn_y), ImVec2(ref_btn_x + ref_btn_w, ref_btn_y + ref_btn_h), btn_border, 6.0f, 0, 1.0f);
    ImVec2 ref_sz = ImGui::CalcTextSize(ref_label);
    dl->AddText(ImVec2(ref_btn_x + (ref_btn_w - ref_sz.x) * 0.5f, ref_btn_y + (ref_btn_h - ref_sz.y) * 0.5f), btn_text_col, ref_label);

    if (Fonts::Small) ImGui::PopFont();

    float btn_w = 140.0f;
    float btn_h = 28.0f;
    float btn_y = y0 + 102.0f;

    // 重启系统
    ImVec2 rb_min(x0 + 16.0f, btn_y);
    ImVec2 rb_max(x0 + 16.0f + btn_w, btn_y + btn_h);
    ImGui::SetCursorScreenPos(rb_min);
    ImGui::InvisibleButton("##RebootBtn", ImVec2(btn_w, btn_h));
    bool rb_hov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        confirm_action_ = ConfirmAction::Reboot;
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
        confirm_action_ = ConfirmAction::Shutdown;
    }
    dl->AddRectFilled(sd_min, sd_max, sd_hov ? IM_COL32(239, 68, 68, 60) : IM_COL32(239, 68, 68, 25), 6.0f);
    dl->AddRect(sd_min, sd_max, IM_COL32(239, 68, 68, sd_hov ? 220 : 110), 6.0f, 0, 1.0f);
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 sd_sz = ImGui::CalcTextSize("安全关机");
    dl->AddText(ImVec2(sd_min.x + (btn_w - sd_sz.x) * 0.5f, btn_y + (btn_h - sd_sz.y) * 0.5f),
                IM_COL32(254, 202, 202, 240), "安全关机");
    if (Fonts::Small) ImGui::PopFont();

    if (!power_status_msg_.empty()) {
        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(x0 + 36.0f + btn_w * 2.0f, btn_y + 6.0f), IM_COL32(245, 158, 11, 255), power_status_msg_.c_str());
        if (Fonts::Small) ImGui::PopFont();
    }

    return h;
}

void SystemSettingsView::renderPowerConfirmModal() {
    ImGuiIO& io = ImGui::GetIO();
    float screen_w = io.DisplaySize.x;
    float screen_h = io.DisplaySize.y;

    // 1. 全透明交互遮罩
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGuiWindowFlags backdrop_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##PowerConfirmModalBackdrop", nullptr, backdrop_flags)) {
        ImGui::InvisibleButton("##PowerModalBackdropBlocker", io.DisplaySize);
        if (ImGui::IsItemClicked()) {
            confirm_action_ = ConfirmAction::None;
        }
    }
    ImGui::End();

    // 2. 居中模态卡片尺寸
    const float modal_w = 400.0f;
    const float modal_h = 190.0f;
    const float modal_x = (screen_w - modal_w) * 0.5f;
    const float modal_y = (screen_h - modal_h) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(modal_x, modal_y));
    ImGui::SetNextWindowSize(ImVec2(modal_w, modal_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));

    if (ImGui::Begin("##PowerConfirmModalDialog", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetWindowPos();
        ImVec2 p_max(p_min.x + modal_w, p_min.y + modal_h);

        dl->AddRectFilled(ImVec2(p_min.x - 2.0f, p_min.y + 4.0f),
                          ImVec2(p_max.x + 2.0f, p_max.y + 14.0f),
                          IM_COL32(0, 0, 0, 140), 18.0f);
        GlassCardRenderer::drawFrosted(dl, p_min, p_max, 16.0f);

        bool is_reboot = (confirm_action_ == ConfirmAction::Reboot);
        const char* title = is_reboot ? "确认重启数播系统" : "确认安全关机";
        const char* desc1 = is_reboot ? "确定要安全重启树莓派吗？" : "确定要安全关闭树莓派吗？";
        const char* desc2 = is_reboot ? "系统将安全落盘所有曲库与设置数据后重新引导。" : "系统将安全卸载磁盘。等待绿色指示灯熄灭后方可拔电！";

        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        ImU32 title_col = is_reboot ? IM_COL32(234, 179, 8, 255) : IM_COL32(239, 68, 68, 255);
        ImGui::TextColored(ImColor(title_col).Value, "%s", title);
        if (Fonts::Medium) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.95f, 1.0f), "%s", desc1);
        ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "%s", desc2);
        if (Fonts::Small) ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        float btn_w = 120.0f;
        float btn_h = 32.0f;
        float spacing = 20.0f;
        float start_x = (modal_w - (btn_w * 2.0f + spacing)) * 0.5f;

        ImGui::SetCursorPos(ImVec2(start_x, modal_h - btn_h - 20.0f));
        if (ImGui::Button("取消", ImVec2(btn_w, btn_h))) {
            confirm_action_ = ConfirmAction::None;
        }

        ImGui::SameLine(0.0f, spacing);
        ImGui::PushStyleColor(ImGuiCol_Button, is_reboot ? IM_COL32(202, 138, 4, 180) : IM_COL32(220, 38, 38, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, is_reboot ? IM_COL32(234, 179, 8, 220) : IM_COL32(239, 68, 68, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, is_reboot ? IM_COL32(161, 98, 7, 255) : IM_COL32(185, 28, 28, 255));

        const char* confirm_label = is_reboot ? "确认重启" : "确认关机";
        if (ImGui::Button(confirm_label, ImVec2(btn_w, btn_h))) {
            if (is_reboot) {
                power_status_msg_ = "正在执行重启指令...";
                confirm_action_ = ConfirmAction::None;
#if !defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE
                system("sudo reboot || reboot &");
#endif
            } else {
                power_status_msg_ = "正在执行关机指令，请待绿灯熄灭后拔电...";
                confirm_action_ = ConfirmAction::None;
#if !defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE
                system("sudo poweroff || shutdown -h now &");
#endif
            }
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
