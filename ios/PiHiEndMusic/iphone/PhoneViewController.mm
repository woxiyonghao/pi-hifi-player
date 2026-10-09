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
#import "views/EQConfigView.hpp"
#import <memory>
#import <string>
#import <cmath>

namespace {

void ConfigureBackgroundReadableFiles(NSString *documentsDirectory) {
    if (documentsDirectory.length == 0) return;

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSDictionary<NSFileAttributeKey, id> *attributes = @{
        NSFileProtectionKey: NSFileProtectionCompleteUntilFirstUserAuthentication
    };

    NSError *rootError = nil;
    if (![fileManager setAttributes:attributes ofItemAtPath:documentsDirectory error:&rootError]) {
        NSLog(@"[BackgroundAudio] Documents protection update failed: %@", rootError.localizedDescription);
    }

    NSDirectoryEnumerator<NSString *> *enumerator = [fileManager enumeratorAtPath:documentsDirectory];
    for (NSString *relativePath in enumerator) {
        NSString *fullPath = [documentsDirectory stringByAppendingPathComponent:relativePath];
        NSError *itemError = nil;
        if (![fileManager setAttributes:attributes ofItemAtPath:fullPath error:&itemError]) {
            NSLog(@"[BackgroundAudio] protection update failed for %@: %@",
                  relativePath, itemError.localizedDescription);
        }
    }
}

} // namespace

@interface PhoneViewController () <MTKViewDelegate> {
    std::unique_ptr<HifiPadRenderer> _renderer;
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

    // 0.1 准备 iOS App 沙盒环境路径 (确保 Documents/hifi_player 目录存在并配置给 C++ 核心引擎)
    NSString *docsDir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString *configDir = [docsDir stringByAppendingPathComponent:@"hifi_player"];
    [[NSFileManager defaultManager] createDirectoryAtPath:configDir withIntermediateDirectories:YES attributes:nil error:nil];
    ConfigureBackgroundReadableFiles(docsDir);
    AppConfig::Path::setConfigDir([configDir UTF8String]);
    AppConfig::Path::setMusicDir([docsDir UTF8String]);

    // 0.2 针对 iPhone 紧凑横屏优化全局布局尺寸与边距，彻底杜绝左侧菜单截断与滚动
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

    // 3. 初始化发烧音频全功能横屏数播渲染器
    _renderer = std::make_unique<HifiPadRenderer>();
    _renderer->init();
}

- (void)dealloc {
    _renderer.reset();
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
    if (@available(iOS 16.0, *)) {
        UIWindowSceneGeometryPreferencesIOS *geometryPreferences = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
        [self.view.window.windowScene requestGeometryUpdateWithPreferences:geometryPreferences errorHandler:nil];
        [self setNeedsUpdateOfSupportedInterfaceOrientations];
    }
}

- (BOOL)shouldAutorotate {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeRight;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
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

    // 全功能横屏发烧数播模式
    if (_renderer) {
        UIEdgeInsets insets = UIEdgeInsetsZero;
        if (@available(iOS 11.0, *)) {
            insets = view.safeAreaInsets;
        }
        _renderer->setSafeArea(insets.left, insets.top, insets.right, insets.bottom);
        _renderer->render(screen_w, screen_h);
    }

    ImGui::Render();
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);
    [renderEncoder endEncoding];

    [commandBuffer presentDrawable:view.currentDrawable];
    [commandBuffer commit];
}

#pragma mark - 触控交互适配 (Touch Events & Smooth Scrolling)

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_renderer) _renderer->resetIdle();
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
    if (_renderer) _renderer->resetIdle();
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
    if (_renderer) _renderer->resetIdle();
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
