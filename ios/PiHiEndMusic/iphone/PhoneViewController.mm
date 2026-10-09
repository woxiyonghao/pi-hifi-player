//
//  PhoneViewController.mm
//  PiHiEndMusic
//
//  Created by MK-10 on 2026/9/30.
//

#import "PhoneViewController.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "imgui.h"
#import "imgui_impl_metal.h"
#import "../ipad/HifiPadRenderer.hpp"
#import "public/AppConfig.hpp"
#import "public/Platform.hpp"
#import "public/Font.hpp"
#import "public/UIConfig.hpp"
#import "tools/MusicDatabase.hpp"
#import "tools/PlayerAdmin.hpp"
#import "services/WebService.hpp"
#import "views/HifiPhoneRenderer.hpp"
#import "views/EQConfigView.hpp"
#import "widgets/GlassCardRenderer.hpp"
#import <memory>
#import <string>
#import <cmath>

@interface PhoneViewController () <MTKViewDelegate> {
    std::unique_ptr<HifiPadRenderer> _pad_renderer;
    std::unique_ptr<HifiPhoneRenderer> _phone_renderer;
    int _currentMode; // 0: 未设置, 1: 数播横屏模式, 2: 随身主端模式
}

@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) MTKView *mtkView;

@property (nonatomic, assign) CGPoint touchStartPos;
@property (nonatomic, assign) CGPoint lastTouchPos;
@property (nonatomic, assign) NSTimeInterval lastTouchTime;
@property (nonatomic, assign) BOOL isScrolling;
@property (nonatomic, assign) BOOL isItemDrag;
@property (nonatomic, assign) float scrollVelocityY;

@end

@implementation PhoneViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];

    // 0. 明确锁定当前运行架构为 iPhone
    Platform::setOverride(PlatformType::IPhone);

    // 0.1 准备 iOS App 沙盒环境路径
    NSString *docsDir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString *configDir = [docsDir stringByAppendingPathComponent:@"hifi_player"];
    [[NSFileManager defaultManager] createDirectoryAtPath:configDir withIntermediateDirectories:YES attributes:nil error:nil];
    AppConfig::Path::setConfigDir([configDir UTF8String]);
    AppConfig::Path::setMusicDir([docsDir UTF8String]);

    // 0.2 每次打开 App 均主动询问用户进入哪一个运行形态 (0: 引导选择弹窗, 1: 数播横屏模式, 2: 随身主端模式)
    _currentMode = 0;

    // 0.3 针对 iPhone 紧凑屏幕优化全局布局尺寸与边距，彻底杜绝左侧菜单截断与滚动
    UIConfig::Layout::NavItemHeight = 25.0f;
    UIConfig::Layout::DacCardHeight = 32.0f;
    UIConfig::Layout::ContainerMarginX = 8.0f;
    UIConfig::Layout::ContainerMarginY = 8.0f;
    UIConfig::Layout::ContainerGap = 6.0f;
    UIConfig::Layout::SidebarWidth  = 180.0f;

    // 1. 初始化 Metal 绘图引擎
    self.device = MTLCreateSystemDefaultDevice();
    self.commandQueue = [self.device newCommandQueue];

    self.mtkView = [[MTKView alloc] initWithFrame:self.view.bounds device:self.device];
    self.mtkView.delegate = self;
    self.mtkView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.mtkView.preferredFramesPerSecond = 60;
    self.mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.mtkView.depthStencilPixelFormat = MTLPixelFormatInvalid;
    [self.view addSubview:self.mtkView];

    // 2. 初始化 Dear ImGui 核心与 Metal 后端
    if (ImGui::GetCurrentContext() == nullptr) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // 移动端禁用 ini 窗口位置缓存，防止出现 Debug 浮窗
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        Fonts::initialize(io);
        ImGui_ImplMetal_Init(self.device);
    } else {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        if (io.BackendRendererUserData == nullptr) {
            ImGui_ImplMetal_Init(self.device);
        }
    }

    // 3. 初始化发烧音频横屏渲染器 (数播模式复用)
    _pad_renderer = std::make_unique<HifiPadRenderer>();
    _pad_renderer->init();

    // 4. 初始化发烧音频移动端竖屏渲染器 (主端模式使用)
    _phone_renderer = std::make_unique<HifiPhoneRenderer>();
    _phone_renderer->init();
    _phone_renderer->setOnSwitchToStreamer([self]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self switchToMode:1];
        });
    });
}

- (void)dealloc {
    _pad_renderer.reset();
    _phone_renderer.reset();
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplMetal_Shutdown();
        ImGui::DestroyContext();
    }
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    self.mtkView.frame = self.view.bounds;
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    if (_currentMode == 1) {
        if (@available(iOS 16.0, *)) {
            UIWindowSceneGeometryPreferencesIOS *geometryPreferences = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
            [self.view.window.windowScene requestGeometryUpdateWithPreferences:geometryPreferences errorHandler:nil];
            [self setNeedsUpdateOfSupportedInterfaceOrientations];
        }
    }
}

- (BOOL)shouldAutorotate {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    if (_currentMode == 1) {
        return UIInterfaceOrientationMaskLandscape;
    }
    // 引导选择模式 (0) 或随身主端模式 (2)：均为垂直竖屏
    return UIInterfaceOrientationMaskPortrait;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    if (_currentMode == 1) {
        return UIInterfaceOrientationLandscapeRight;
    }
    return UIInterfaceOrientationPortrait;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
}

- (void)switchToMode:(int)mode {
    _currentMode = mode;
    MusicDatabase::getInstance().setSetting("setting_mobile_app_role", std::to_string(mode));

    if (mode == 1) {
        WebService::getInstance().start(8088);
        if (@available(iOS 16.0, *)) {
            UIWindowSceneGeometryPreferencesIOS *geom = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
            [self.view.window.windowScene requestGeometryUpdateWithPreferences:geom errorHandler:nil];
        }
    } else if (mode == 2) {
        if (@available(iOS 16.0, *)) {
            UIWindowSceneGeometryPreferencesIOS *geom = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskPortrait];
            [self.view.window.windowScene requestGeometryUpdateWithPreferences:geom errorHandler:nil];
        }
    }
    [self setNeedsUpdateOfSupportedInterfaceOrientations];
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
}

- (void)drawInMTKView:(MTKView *)view {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(view.bounds.size.width, view.bounds.size.height);
    io.DisplayFramebufferScale = ImVec2(view.contentScaleFactor, view.contentScaleFactor);

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    MTLRenderPassDescriptor *renderPassDesc = view.currentRenderPassDescriptor;
    if (!renderPassDesc) return;

    ImVec4 clearColor = ThemeManager::getInstance().getClearColor();
    renderPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(clearColor.x, clearColor.y, clearColor.z, 1.0);
    renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;

    // 动量惯性滚动衰减 (iOS Native Fling Inertia)
    if (!self.isScrolling && fabsf(self.scrollVelocityY) > 10.0f) {
        float dt = 1.0f / 60.0f;
        float dy = self.scrollVelocityY * dt;
        io.AddMouseWheelEvent(0.0f, dy / 65.0f);
        self.scrollVelocityY *= 0.92f;
        if (fabsf(self.scrollVelocityY) < 10.0f) {
            self.scrollVelocityY = 0.0f;
        }
    }

    // 帧渲染
    ImGui_ImplMetal_NewFrame(renderPassDesc);
    ImGui::NewFrame();

    float screen_w = view.bounds.size.width;
    float screen_h = view.bounds.size.height;

    if (_currentMode == 0) {
        // [引导选择弹窗] 首次启动未选择角色形态
        [self renderRoleSelectionModal:screen_w height:screen_h];
    } else if (_currentMode == 1) {
        // [数播模式] 强制横屏，运行 iPad 缩小自适应版
        if (_pad_renderer) {
            UIEdgeInsets insets = UIEdgeInsetsZero;
            if (@available(iOS 11.0, *)) {
                insets = view.safeAreaInsets;
            }
            _pad_renderer->setSafeArea(insets.left, insets.top, insets.right, insets.bottom);
            _pad_renderer->render(screen_w, screen_h);
        }
        // 浮动模式切换按钮 (右上角切换胶囊)
        [self renderModeSwitchButton:screen_w height:screen_h];
    } else {
        // [主端模式] 竖屏 Dear ImGui 移动端重构版
        if (_phone_renderer) {
            _phone_renderer->render(screen_w, screen_h);
        }
    }

    ImGui::Render();
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);
    [renderEncoder endEncoding];

    [commandBuffer presentDrawable:view.currentDrawable];
    [commandBuffer commit];
}

#pragma mark - 内部 ImGui 视图渲染

- (void)renderRoleSelectionModal:(float)screen_w height:(float)screen_h {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screen_w, screen_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##PhoneRoleSelectionModalRoot", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 磨砂全屏暗色遮罩
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(screen_w, screen_h), IM_COL32(0, 0, 0, 200));

        float card_w = std::min(screen_w - 32.0f, 380.0f);
        float card_h = 360.0f;
        float card_x0 = (screen_w - card_w) * 0.5f;
        float card_y0 = (screen_h - card_h) * 0.5f;
        ImVec2 c_min(card_x0, card_y0);
        ImVec2 c_max(card_x0 + card_w, card_y0 + card_h);

        GlassCardRenderer::drawCard(dl, c_min, c_max, 16.0f, "role_modal");

        // 标题与副标题
        if (Fonts::Large) ImGui::PushFont(Fonts::Large);
        ImVec2 t_sz = ImGui::CalcTextSize("PiHiEnd 发烧音频中枢");
        dl->AddText(ImVec2(card_x0 + (card_w - t_sz.x) * 0.5f, card_y0 + 20.0f),
                    UIConfig::Color::TextActive, "PiHiEnd 发烧音频中枢");
        if (Fonts::Large) ImGui::PopFont();

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        const char* sub_txt = "请选择当前设备运行形态 (设置中可随时切换)";
        ImVec2 s_sz = ImGui::CalcTextSize(sub_txt);
        dl->AddText(ImVec2(card_x0 + (card_w - s_sz.x) * 0.5f, card_y0 + 52.0f),
                    UIConfig::Color::TextMuted, sub_txt);
        if (Fonts::Small) ImGui::PopFont();

        // 选项 1：数播模式卡片
        float btn_w = card_w - 32.0f;
        float btn_h = 96.0f;
        float btn1_y = card_y0 + 82.0f;
        ImVec2 b1_min(card_x0 + 16.0f, btn1_y);
        ImVec2 b1_max(card_x0 + 16.0f + btn_w, btn1_y + btn_h);

        ImGui::SetCursorScreenPos(b1_min);
        if (ImGui::InvisibleButton("##SelectStreamerRoleBtn", ImVec2(btn_w, btn_h))) {
            [self switchToMode:1];
        }
        bool hov1 = ImGui::IsItemHovered();
        ImU32 b1_bg = hov1 ? IM_COL32(250, 45, 72, 60) : IM_COL32(255, 255, 255, 18);
        ImU32 b1_bd = hov1 ? UIConfig::Color::Accent : UIConfig::Color::GlassBorder;
        dl->AddRectFilled(b1_min, b1_max, b1_bg, 12.0f);
        dl->AddRect(b1_min, b1_max, b1_bd, 12.0f, 0, 1.2f);

        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        dl->AddText(ImVec2(b1_min.x + 16.0f, b1_min.y + 14.0f), UIConfig::Color::TextActive, "发烧数播模式 (Streamer)");
        if (Fonts::Medium) ImGui::PopFont();

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(b1_min.x + 16.0f, b1_min.y + 42.0f), UIConfig::Color::TextNormal,
                    "耳放解码直连 · 绚丽发烧表头 · 强制横屏");
        dl->AddText(ImVec2(b1_min.x + 16.0f, b1_min.y + 64.0f), UIConfig::Color::TextMuted,
                    "自动开启Web遥控 · 适合桌面独立转盘");
        if (Fonts::Small) ImGui::PopFont();

        // 选项 2：便携随身主端模式卡片
        float btn2_y = btn1_y + btn_h + 14.0f;
        ImVec2 b2_min(card_x0 + 16.0f, btn2_y);
        ImVec2 b2_max(card_x0 + 16.0f + btn_w, btn2_y + btn_h);

        ImGui::SetCursorScreenPos(b2_min);
        if (ImGui::InvisibleButton("##SelectPlayerRoleBtn", ImVec2(btn_w, btn_h))) {
            [self switchToMode:2];
        }
        bool hov2 = ImGui::IsItemHovered();
        ImU32 b2_bg = hov2 ? IM_COL32(250, 45, 72, 60) : IM_COL32(255, 255, 255, 18);
        ImU32 b2_bd = hov2 ? UIConfig::Color::Accent : UIConfig::Color::GlassBorder;
        dl->AddRectFilled(b2_min, b2_max, b2_bg, 12.0f);
        dl->AddRect(b2_min, b2_max, b2_bd, 12.0f, 0, 1.2f);

        if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
        dl->AddText(ImVec2(b2_min.x + 16.0f, b2_min.y + 14.0f), UIConfig::Color::TextActive, "便携主端模式 (Player)");
        if (Fonts::Medium) ImGui::PopFont();

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        dl->AddText(ImVec2(b2_min.x + 16.0f, b2_min.y + 42.0f), UIConfig::Color::TextNormal,
                    "单手竖屏握持 · 随身海量高解析曲库");
        dl->AddText(ImVec2(b2_min.x + 16.0f, b2_min.y + 64.0f), UIConfig::Color::TextMuted,
                    "10段发烧EQ & 调音魔棒 · 随身聆听");
        if (Fonts::Small) ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

- (void)renderModeSwitchButton:(float)screen_w height:(float)screen_h {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(screen_w, screen_h));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##PhoneModeSwitchButtonRoot", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        UIEdgeInsets insets = UIEdgeInsetsZero;
        if (@available(iOS 11.0, *)) {
            insets = self.view.safeAreaInsets;
        }

        // 放置在左侧边栏顶部右侧空白处，完全避让右侧主舞台所有页面控件 (调音魔棒一键复位/设置/歌单等)
        float sidebar_w = UIConfig::Layout::SidebarWidth;
        float btn_w = 64.0f;
        float btn_h = 22.0f;
        float btn_x = insets.left + sidebar_w - UIConfig::Layout::ContainerMarginX - btn_w - 4.0f;
        float btn_y = insets.top + UIConfig::Layout::ContainerMarginY + 4.0f;
        ImVec2 b_min(btn_x, btn_y);
        ImVec2 b_max(btn_x + btn_w, btn_y + btn_h);

        ImGui::SetCursorScreenPos(b_min);
        if (ImGui::InvisibleButton("##TopSwitchRoleBtn", ImVec2(btn_w, btn_h))) {
            [self switchToMode:2];
        }
        bool hov = ImGui::IsItemHovered();
        ImU32 fill = hov ? IM_COL32(250, 45, 72, 80) : IM_COL32(255, 255, 255, 26);
        dl->AddRectFilled(b_min, b_max, fill, 11.0f);
        dl->AddRect(b_min, b_max, hov ? UIConfig::Color::Accent : UIConfig::Color::GlassBorder, 11.0f, 0, 1.0f);

        if (Fonts::Small) ImGui::PushFont(Fonts::Small);
        ImVec2 t_sz = ImGui::CalcTextSize("切换竖屏");
        dl->AddText(ImVec2(btn_x + (btn_w - t_sz.x) * 0.5f, btn_y + (btn_h - t_sz.y) * 0.5f),
                    UIConfig::Color::TextActive, "切换竖屏");
        if (Fonts::Small) ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

- (void)renderMobilePlayerView:(float)screen_w height:(float)screen_h {
    // 竖屏主端模式基础骨架 (顶部状态栏 + 当前在播中枢 + 切换数播按钮)
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    // 背景深邃暗黑磨砂
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(screen_w, screen_h), IM_COL32(8, 10, 14, 255));

    // 顶部 Header
    float header_y = 44.0f; // 避让安全区
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    dl->AddText(ImVec2(20.0f, header_y + 10.0f), UIConfig::Color::TextActive, "PiHiEnd 便携随身播放器");
    if (Fonts::Medium) ImGui::PopFont();

    // 右上角一键切回数播模式胶囊
    float btn_w = 96.0f;
    float btn_h = 30.0f;
    float btn_x = screen_w - btn_w - 20.0f;
    float btn_y = header_y + 6.0f;
    ImVec2 b_min(btn_x, btn_y);
    ImVec2 b_max(btn_x + btn_w, btn_y + btn_h);

    ImGui::SetCursorScreenPos(b_min);
    if (ImGui::InvisibleButton("##SwitchToStreamerModeBtn", ImVec2(btn_w, btn_h))) {
        [self switchToMode:1];
    }
    bool hov = ImGui::IsItemHovered();
    dl->AddRectFilled(b_min, b_max, hov ? IM_COL32(250, 45, 72, 80) : IM_COL32(255, 255, 255, 26), 15.0f);
    dl->AddRect(b_min, b_max, hov ? UIConfig::Color::Accent : UIConfig::Color::GlassBorder, 15.0f, 0, 1.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 t_sz = ImGui::CalcTextSize("切换为数播");
    dl->AddText(ImVec2(btn_x + (btn_w - t_sz.x) * 0.5f, btn_y + (btn_h - t_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "切换为数播");
    if (Fonts::Small) ImGui::PopFont();

    // 当前在播中枢卡片
    auto& player = PlayerAdmin::getInstance();
    auto cur_t = player.getCurrentTrack();

    float card_margin = 16.0f;
    float card_y = header_y + 54.0f;
    float card_h = 240.0f;
    ImVec2 c_min(card_margin, card_y);
    ImVec2 c_max(screen_w - card_margin, card_y + card_h);
    GlassCardRenderer::drawCard(dl, c_min, c_max, 14.0f, "phone_now_playing");

    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    const char* title_str = cur_t.has_value() ? cur_t->title.c_str() : "未在播放音频";
    dl->AddText(ImVec2(card_margin + 20.0f, card_y + 20.0f), UIConfig::Color::TextActive, title_str);
    if (Fonts::Medium) ImGui::PopFont();

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    const char* artist_str = cur_t.has_value() ? cur_t->artist.c_str() : "点击播放控制开始聆听";
    dl->AddText(ImVec2(card_margin + 20.0f, card_y + 48.0f), UIConfig::Color::TextMuted, artist_str);
    if (Fonts::Small) ImGui::PopFont();

    // 控制按键
    float ctrl_y = card_y + 110.0f;
    float btn_play_w = 120.0f;
    float btn_play_h = 44.0f;
    ImVec2 p_min(card_margin + 20.0f, ctrl_y);
    ImVec2 p_max(card_margin + 20.0f + btn_play_w, ctrl_y + btn_play_h);

    ImGui::SetCursorScreenPos(p_min);
    if (ImGui::InvisibleButton("##PhonePlayPauseBtn", ImVec2(btn_play_w, btn_play_h))) {
        player.togglePlayPause();
    }
    bool hov_play = ImGui::IsItemHovered();
    dl->AddRectFilled(p_min, p_max, hov_play ? IM_COL32(250, 45, 72, 100) : UIConfig::Color::Accent, 22.0f);

    const char* state_str = player.isPlaying() ? "暂停" : "播放";
    if (Fonts::Medium) ImGui::PushFont(Fonts::Medium);
    ImVec2 st_sz = ImGui::CalcTextSize(state_str);
    dl->AddText(ImVec2(p_min.x + (btn_play_w - st_sz.x) * 0.5f, p_min.y + (btn_play_h - st_sz.y) * 0.5f),
                IM_COL32(255, 255, 255, 255), state_str);
    if (Fonts::Medium) ImGui::PopFont();

    // 下一曲按钮
    float next_x = p_max.x + 16.0f;
    ImVec2 n_min(next_x, ctrl_y);
    ImVec2 n_max(next_x + 80.0f, ctrl_y + btn_play_h);
    ImGui::SetCursorScreenPos(n_min);
    if (ImGui::InvisibleButton("##PhoneNextBtn", ImVec2(80.0f, btn_play_h))) {
        player.next();
    }
    bool hov_next = ImGui::IsItemHovered();
    dl->AddRectFilled(n_min, n_max, hov_next ? IM_COL32(255, 255, 255, 36) : IM_COL32(255, 255, 255, 18), 22.0f);
    dl->AddRect(n_min, n_max, UIConfig::Color::GlassBorder, 22.0f, 0, 1.0f);

    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    ImVec2 n_sz = ImGui::CalcTextSize("下一曲");
    dl->AddText(ImVec2(n_min.x + (80.0f - n_sz.x) * 0.5f, n_min.y + (btn_play_h - n_sz.y) * 0.5f),
                UIConfig::Color::TextActive, "下一曲");
    if (Fonts::Small) ImGui::PopFont();

    // 提示信息
    float hint_y = card_y + card_h + 30.0f;
    if (Fonts::Small) ImGui::PushFont(Fonts::Small);
    dl->AddText(ImVec2(20.0f, hint_y), UIConfig::Color::TextMuted,
                "💡 提示：点击右上角「切换为数播」可进入全功能横屏数播模式，\n享受完整金嗓子/开盘机表头动效与Web远程控制。");
    if (Fonts::Small) ImGui::PopFont();
}

#pragma mark - 触控交互适配 (Touch Events & Smooth Scrolling)

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_pad_renderer) _pad_renderer->resetIdle();
    if (_phone_renderer) _phone_renderer->resetIdle();
    UITouch *touch = [touches anyObject];
    CGPoint pt = [touch locationInView:self.mtkView];

    self.touchStartPos = pt;
    self.lastTouchPos = pt;
    self.lastTouchTime = [NSDate timeIntervalSinceReferenceDate];
    self.scrollVelocityY = 0.0f;

    // 若手指触控命中 EQ 推子滑块区域，立即锁定为组件拖拽态，杜绝误判为纵向列表滚动而释放鼠标按键
    if (EQConfigView::isSliderTouch((float)pt.x, (float)pt.y)) {
        self.isItemDrag = YES;
        self.isScrolling = NO;
    } else {
        self.isScrolling = NO;
        self.isItemDrag = NO;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(pt.x, pt.y);
    io.AddMouseButtonEvent(0, true);
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_pad_renderer) _pad_renderer->resetIdle();
    if (_phone_renderer) _phone_renderer->resetIdle();
    UITouch *touch = [touches anyObject];
    CGPoint pt = [touch locationInView:self.mtkView];
    NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    ImGuiIO& io = ImGui::GetIO();

    CGFloat dx = pt.x - self.touchStartPos.x;
    CGFloat dy = pt.y - self.touchStartPos.y;
    CGFloat totalDist = sqrt(dx * dx + dy * dy);

    if (!self.isScrolling && !self.isItemDrag) {
        if (totalDist > 7.0f) {
            // 若滑块处于激活状态或触控起始点/当前点位于推子区域，严禁触发列表滚动，严禁释放鼠标左键！
            if (EQConfigView::isSliderTouch((float)self.touchStartPos.x, (float)self.touchStartPos.y) ||
                EQConfigView::isSliderTouch((float)pt.x, (float)pt.y) ||
                EQConfigView::isAnySliderActive()) {
                self.isItemDrag = YES;
                self.isScrolling = NO;
            } else if (fabs(dy) > fabs(dx) * 0.7f) {
                self.isScrolling = YES;
                io.AddMouseButtonEvent(0, false);
            } else {
                self.isItemDrag = YES;
            }
        }
    }

    if (self.isScrolling) {
        CGFloat deltaY = pt.y - self.lastTouchPos.y;
        NSTimeInterval dt = now - self.lastTouchTime;
        if (dt > 0.001) {
            float instantVelocity = (float)(deltaY / dt);
            self.scrollVelocityY = self.scrollVelocityY * 0.25f + instantVelocity * 0.75f;
        }

        io.AddMousePosEvent(pt.x, pt.y);
        io.AddMouseWheelEvent(0.0f, (float)(deltaY / 65.0f));
    } else {
        io.AddMousePosEvent(pt.x, pt.y);
    }

    self.lastTouchPos = pt;
    self.lastTouchTime = now;
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_pad_renderer) _pad_renderer->resetIdle();
    if (_phone_renderer) _phone_renderer->resetIdle();
    UITouch *touch = [touches anyObject];
    CGPoint pt = [touch locationInView:self.mtkView];
    ImGuiIO& io = ImGui::GetIO();

    EQConfigView::setSliderActive(false);

    if (self.isScrolling) {
        if (self.scrollVelocityY > 2800.0f) self.scrollVelocityY = 2800.0f;
        if (self.scrollVelocityY < -2800.0f) self.scrollVelocityY = -2800.0f;
        if (fabsf(self.scrollVelocityY) < 120.0f) self.scrollVelocityY = 0.0f;

        io.AddMousePosEvent(pt.x, pt.y);
        self.isScrolling = NO;
    } else {
        io.AddMousePosEvent(pt.x, pt.y);
        io.AddMouseButtonEvent(0, false);
    }
    self.isItemDrag = NO;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    EQConfigView::setSliderActive(false);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(0, false);
    self.isScrolling = NO;
    self.isItemDrag = NO;
    self.scrollVelocityY = 0.0f;
}

@end
