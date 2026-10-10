#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <memory>
#include <string>
#include <cmath>
#include <chrono>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "views/HifiPadRenderer.hpp"
#include "views/EQConfigView.hpp"
#include "tools/PlayerAdmin.hpp"
#include "tools/MusicDatabase.hpp"
#include "tools/MusicScanManager.hpp"
#include "themes/ThemeManager.hpp"
#include "public/AppConfig.hpp"
#include "public/Platform.hpp"
#include "public/Font.hpp"
#include "public/UIConfig.hpp"

#define LOG_TAG "PiHifiBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

struct TouchState {
    float start_x = 0.0f;
    float start_y = 0.0f;
    float last_x = 0.0f;
    float last_y = 0.0f;
    int64_t last_time_ms = 0;
    bool is_scrolling = false;
    bool is_item_drag = false;
    float scroll_velocity_y = 0.0f;
};

std::unique_ptr<HifiPadRenderer> g_renderer;
TouchState g_touch;
int g_physical_w = 0;
int g_physical_h = 0;
float g_density = 1.0f;
float g_logical_w = 0.0f;
float g_logical_h = 0.0f;

bool g_is_initialized = false;
std::chrono::steady_clock::time_point g_last_frame_time;

} // namespace

extern "C" {

JNIEXPORT void JNICALL
Java_com_pihifi_player_HifiGLSurfaceView_nativeInit(
    JNIEnv* env, jobject /*thiz*/, jstring configDir, jstring musicDir) {
    if (g_is_initialized) return;

    // 1. 明确锁定平台为 Android (共享手机紧凑型发烧布局与逻辑)
    Platform::setOverride(PlatformType::Android);

    // 2. 配置沙盒数据目录与曲库路径
    const char* c_config = env->GetStringUTFChars(configDir, nullptr);
    const char* c_music = env->GetStringUTFChars(musicDir, nullptr);
    AppConfig::Path::setConfigDir(c_config);
    AppConfig::Path::setMusicDir(c_music);
    LOGI("沙盒配置目录: %s, 曲库默认目录: %s", c_config, c_music);
    env->ReleaseStringUTFChars(configDir, c_config);
    env->ReleaseStringUTFChars(musicDir, c_music);

    // 3. 布局参数专属优化 (针对移动端横屏进行空间释放，彻底消除拥挤感)
    UIConfig::Layout::NavItemHeight = 24.0f;
    UIConfig::Layout::DacCardHeight = 30.0f;
    UIConfig::Layout::ContainerMarginX = 8.0f;
    UIConfig::Layout::ContainerMarginY = 8.0f;
    UIConfig::Layout::ContainerGap = 6.0f;
    UIConfig::Layout::SidebarWidth  = 180.0f;
    UIConfig::Layout::BottomBarOffset = 62.0f; // 底部播放栏 48px + 边距 8px + 间距 6px = 62px (释放 24px 垂直净空间)

    g_last_frame_time = std::chrono::steady_clock::now();
    g_is_initialized = true;
    LOGI("PiHifiCore 原生核心环境初始化就绪");
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_HifiGLSurfaceView_nativeSurfaceCreated(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    LOGI("GLSurfaceView Surface 创建，初始化 ImGui & GLES3 管线");

    if (ImGui::GetCurrentContext() == nullptr) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // 移动端禁用 ini 窗口位置缓存
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        Fonts::initialize(io);
        ImGui_ImplOpenGL3_Init("#version 300 es");
    } else {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        if (io.BackendRendererUserData == nullptr) {
            ImGui_ImplOpenGL3_Init("#version 300 es");
        }
    }

    // 初始化全功能发烧数播渲染器
    g_renderer = std::make_unique<HifiPadRenderer>();
    g_renderer->init();
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_HifiGLSurfaceView_nativeSurfaceChanged(
    JNIEnv* /*env*/, jobject /*thiz*/, jint width, jint height, jfloat density) {
    g_physical_w = width;
    g_physical_h = height;
    g_density = (density > 0.1f) ? density : 1.0f;
    g_logical_w = static_cast<float>(width) / g_density;
    g_logical_h = static_cast<float>(height) / g_density;

    LOGI("Surface 尺寸: %d x %d (物理像素), 逻辑尺寸: %.1f x %.1f (dp), 屏幕缩放比(density): %.3f",
         width, height, g_logical_w, g_logical_h, g_density);
    glViewport(0, 0, width, height);
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_HifiGLSurfaceView_nativeSetSafeArea(
    JNIEnv* /*env*/, jobject /*thiz*/, jfloat left, jfloat top, jfloat right, jfloat bottom) {
    if (g_renderer) {
        float d = (g_density > 0.1f) ? g_density : 1.0f;
        g_renderer->setSafeArea(left / d, top / d, right / d, bottom / d);
    }
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_HifiGLSurfaceView_nativeDrawFrame(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    if (!g_renderer || g_logical_w <= 0.0f || g_logical_h <= 0.0f) return;

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - g_last_frame_time).count();
    g_last_frame_time = now;
    if (dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;

    // 驱动 PlayerAdmin 播放器心跳与进度更新
    PlayerAdmin::getInstance().update(dt);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(g_logical_w, g_logical_h);
    io.DisplayFramebufferScale = ImVec2(g_density, g_density);
    io.DeltaTime = dt;

    // 动量惯性滚动衰减 (与 iOS Native Fling Inertia 体验 1:1 保持完全一致)
    if (!g_touch.is_scrolling && std::abs(g_touch.scroll_velocity_y) > 10.0f) {
        float step_dt = 1.0f / 60.0f;
        float dy = g_touch.scroll_velocity_y * step_dt;
        io.AddMouseWheelEvent(0.0f, dy / 65.0f);
        g_touch.scroll_velocity_y *= 0.92f;
        if (std::abs(g_touch.scroll_velocity_y) < 10.0f) {
            g_touch.scroll_velocity_y = 0.0f;
        }
    }

    ImVec4 clear_color = ThemeManager::getInstance().getClearColor();
    glClearColor(clear_color.x, clear_color.y, clear_color.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // 渲染发烧数播主界面 (使用逻辑 DP 尺寸，比例与 iPhone 完全一致)
    g_renderer->render(g_logical_w, g_logical_h);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_HifiGLSurfaceView_nativeOnTouchEvent(
    JNIEnv* /*env*/, jobject /*thiz*/, jint action, jfloat x, jfloat y, jlong eventTimeMs) {
    if (g_renderer) g_renderer->resetIdle();
    ImGuiIO& io = ImGui::GetIO();

    float d = (g_density > 0.1f) ? g_density : 1.0f;
    float lx = x / d;
    float ly = y / d;

    switch (action) {
        case 0: { // ACTION_DOWN
            g_touch.start_x = lx;
            g_touch.start_y = ly;
            g_touch.last_x = lx;
            g_touch.last_y = ly;
            g_touch.last_time_ms = eventTimeMs;
            g_touch.scroll_velocity_y = 0.0f;

            // 若手指触控命中 EQ 推子滑块区域，锁定为拖拽态，严禁误判为列表滚动
            if (EQConfigView::isSliderTouch(lx, ly)) {
                g_touch.is_item_drag = true;
                g_touch.is_scrolling = false;
            } else {
                g_touch.is_scrolling = false;
                g_touch.is_item_drag = false;
            }

            io.AddMousePosEvent(lx, ly);
            io.AddMouseButtonEvent(0, true);
            break;
        }
        case 2: { // ACTION_MOVE
            float dx = lx - g_touch.start_x;
            float dy = ly - g_touch.start_y;
            float total_dist = std::sqrt(dx * dx + dy * dy);

            if (!g_touch.is_scrolling && !g_touch.is_item_drag) {
                if (total_dist > 7.0f) {
                    if (EQConfigView::isSliderTouch(g_touch.start_x, g_touch.start_y) ||
                        EQConfigView::isSliderTouch(lx, ly) ||
                        EQConfigView::isAnySliderActive()) {
                        g_touch.is_item_drag = true;
                        g_touch.is_scrolling = false;
                    } else if (std::abs(dy) > std::abs(dx) * 0.7f) {
                        g_touch.is_scrolling = true;
                        io.AddMouseButtonEvent(0, false);
                    } else {
                        g_touch.is_item_drag = true;
                    }
                }
            }

            if (g_touch.is_scrolling) {
                float delta_y = ly - g_touch.last_y;
                int64_t dt_ms = eventTimeMs - g_touch.last_time_ms;
                if (dt_ms > 1) {
                    float instant_velocity = (delta_y / static_cast<float>(dt_ms)) * 1000.0f;
                    g_touch.scroll_velocity_y = g_touch.scroll_velocity_y * 0.25f + instant_velocity * 0.75f;
                }

                io.AddMousePosEvent(lx, ly);
                io.AddMouseWheelEvent(0.0f, delta_y / 65.0f);
            } else {
                io.AddMousePosEvent(lx, ly);
            }

            g_touch.last_x = lx;
            g_touch.last_y = ly;
            g_touch.last_time_ms = eventTimeMs;
            break;
        }
        case 1:   // ACTION_UP
        case 3: { // ACTION_CANCEL
            EQConfigView::setSliderActive(false);

            if (g_touch.is_scrolling) {
                if (g_touch.scroll_velocity_y > 2800.0f) g_touch.scroll_velocity_y = 2800.0f;
                if (g_touch.scroll_velocity_y < -2800.0f) g_touch.scroll_velocity_y = -2800.0f;
                if (std::abs(g_touch.scroll_velocity_y) < 120.0f) g_touch.scroll_velocity_y = 0.0f;

                io.AddMousePosEvent(lx, ly);
                g_touch.is_scrolling = false;
            } else {
                io.AddMousePosEvent(lx, ly);
                io.AddMouseButtonEvent(0, false);
            }
            g_touch.is_item_drag = false;
            break;
        }
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// 系统音频播控与前台服务 JNI 交互接口
// -----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeTogglePlayPause(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    PlayerAdmin::getInstance().togglePlayPause();
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeNextTrack(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    PlayerAdmin::getInstance().next();
}

JNIEXPORT void JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativePrevTrack(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    PlayerAdmin::getInstance().previous();
}

JNIEXPORT jboolean JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeIsPlaying(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    return PlayerAdmin::getInstance().isPlaying() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeGetCurrentTrackTitle(
    JNIEnv* env, jobject /*thiz*/) {
    const auto& cur = PlayerAdmin::getInstance().getCurrentTrack();
    if (cur.has_value() && !cur->title.empty()) {
        return env->NewStringUTF(cur->title.c_str());
    }
    return env->NewStringUTF("PiHiEndMusic");
}

JNIEXPORT jstring JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeGetCurrentTrackArtist(
    JNIEnv* env, jobject /*thiz*/) {
    const auto& cur = PlayerAdmin::getInstance().getCurrentTrack();
    if (cur.has_value() && !cur->artist.empty()) {
        return env->NewStringUTF(cur->artist.c_str());
    }
    return env->NewStringUTF("HiFi Master Quality");
}

JNIEXPORT jlong JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeGetCurrentPositionMs(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    return static_cast<jlong>(PlayerAdmin::getInstance().getCurrentTimeSec() * 1000.0);
}

JNIEXPORT jlong JNICALL
Java_com_pihifi_player_AudioPlaybackService_nativeGetDurationMs(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    return static_cast<jlong>(PlayerAdmin::getInstance().getDurationSec() * 1000.0);
}

} // extern "C"
