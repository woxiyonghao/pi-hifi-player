#pragma once

#include "imgui.h"

// ==============================================================================
// 全局 UI 视觉与样式配置中心 (UI Configuration Center)
// 集中管理全局颜色板、字阶大小、控件尺寸、动效与内边距
// ==============================================================================
namespace UIConfig {

    // ==========================================================================
    // 1. 发烧调色板 (Colors)
    // ==========================================================================
    namespace Color {
        // [主题强调色]：用于高亮、激活图标、选中指示等 (默认 Apple Music 标志性玫红)
        inline ImU32 Accent           = IM_COL32(250, 45, 72, 255);   // #FA2D48

        // [背景与底板]
        inline ImU32 WindowBg         = IM_COL32(18, 22, 28, 240);    // 侧边栏深邃半透明暗色底
        inline ImU32 MainStageBg      = IM_COL32(10, 14, 20, 255);    // 右侧主舞台基底深空黑

        // [Xcode 风格液态玻璃卡片容器 (上下两大独立包裹容器)]
        inline ImU32 ContainerBg      = IM_COL32(255, 255, 255, 14);  // Xcode 搜索栏同款微光磨砂底色 (Alpha: 14)
        inline ImU32 ContainerBorder  = IM_COL32(255, 255, 255, 30);  // 容器平滑微光 1px 折射圆角边框 (Alpha: 30)

        // [选中项发光液态玻璃胶囊 (Liquid Glass Active Indicator)]
        inline ImU32 GlassActiveTint  = IM_COL32(250, 45, 72, 60);    // 激活胶囊主题色流体润色底 (随 Accent 色调联动)
        inline ImU32 GlassActive      = IM_COL32(255, 255, 255, 30);  // 激活胶囊通透磨砂底板 (Alpha: 30)
        inline ImU32 GlassHover       = IM_COL32(255, 255, 255, 18);  // 悬停项微亮玻璃底板 (Alpha: 18)
        inline ImU32 GlassBorder      = IM_COL32(255, 255, 255, 50);  // 胶囊平滑 1px 微光折射圆角边框 (Alpha: 50)

        // [文字色彩层次]
        inline ImU32 TextActive       = IM_COL32(255, 255, 255, 255); // 激活文字 (纯白 100%)
        inline ImU32 TextNormal       = IM_COL32(200, 210, 225, 220); // 普通菜单文字 (柔和浅灰)
        inline ImU32 TextMuted        = IM_COL32(130, 140, 155, 255); // 次级小标题文字 (Apple 次级浅灰)

        // [图标色彩]
        inline ImU32 IconNormal       = IM_COL32(180, 188, 200, 255); // 未激活图标颜色

        // [DAC 硬件连接指示灯]
        inline ImU32 DacConnected     = IM_COL32(52, 199, 89, 255);   // 绿灯：Bit-Perfect 已连接直通
        inline ImU32 DacDisconnected  = IM_COL32(140, 145, 155, 255); // 灰灯：未连接
    }

    // ==========================================================================
    // 2. 控件尺寸与几何排版 (Layout & Dimensions)
    // ==========================================================================
    namespace Layout {
        inline float SidebarWidth     = 230.0f; // 侧边栏固定总宽度 (px)
        inline float ScreenHeight     = 600.0f; // 屏幕物理总高度 (px)

        // 容器边距与间隙 (根据要求：左右间隔 16px，上下间隔 16px)
        inline float ContainerMarginX = 16.0f;  // 容器左右外边距 (px)
        inline float ContainerMarginY = 16.0f;  // 容器上下外边距 (px)
        inline float ContainerGap     = 16.0f;  // 上下两容器垂直间距 (px)
        inline float ContainerRounding = 10.0f; // 容器圆角半径 (px)

        // 菜单胶囊按钮参数
        inline float NavItemHeight    = 34.0f;  // 按钮触控舒适高度 (px)
        inline float NavItemRounding  = 8.0f;   // 按钮圆角半径 (px)

        // 底部固定 DAC 容器高度
        inline float DacCardHeight    = 42.0f;  // DAC 容器高度 (px)
    }

    // ==========================================================================
    // 3. 多字阶矢量字号 (Typography Font Sizes)
    // ==========================================================================
    namespace FontSize {
        inline float Small            = 12.0f;  // 小字阶：小标题、格式徽标、时间戳
        inline float Regular          = 15.0f;  // 默认字阶：菜单项、歌单列表、普通正文
        inline float Medium           = 20.0f;  // 中字阶：次级标题、专辑名
        inline float Large            = 28.0f;  // 大字阶：正在播放巨型曲目名
    }

    // ==========================================================================
    // 4. 动效与发光参数 (Animations & Glow)
    // ==========================================================================
    namespace Animation {
        inline bool  EnableSliding    = true;   // 是否开启液态玻璃滑动动画
        inline float SlideSpeed       = 12.0f;  // 滑动阻尼弹簧速度 (建议 10.0~14.0，兼具丝滑流畅与响应感)
        inline bool  EnableGlow       = true;   // 是否开启液态玻璃边缘环境辉光 (Bloom Glow)
        inline float GlowIntensity    = 1.0f;   // 辉光强度系数 (0.5~2.0)
    }
} // namespace UIConfig
