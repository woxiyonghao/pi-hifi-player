//
//  PadViewController.mm
//  PiHiEndMusic
//
//  Created by MK-10 on 2026/9/30.
//

#import "PadViewController.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "imgui.h"
#import "imgui_impl_metal.h"
#import "HifiPadRenderer.hpp"
#import "public/AppConfig.hpp"
#import <memory>

namespace {

void ConfigureBackgroundReadableFiles(NSString *documentsDirectory) {
    if (documentsDirectory.length == 0) return;

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSDictionary<NSFileAttributeKey, id> *attributes = @{
        NSFileProtectionKey: NSFileProtectionCompleteUntilFirstUserAuthentication
    };

    // 音乐与数据库在首次解锁后保持可读，确保锁屏期间解码线程不会把
    // “文件暂不可读”误判成 EOF。目录属性也会作为后续传入文件的默认值。
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

@interface PadViewController () <MTKViewDelegate> {
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

@implementation PadViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];

    // 0. 明确锁定当前运行架构为 iPad
    Platform::setOverride(PlatformType::IPad);

    // 0.1 准备 iOS App 沙盒环境路径 (确保 Documents/hifi_player 目录存在并配置给 C++ 核心引擎)
    NSString *docsDir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString *configDir = [docsDir stringByAppendingPathComponent:@"hifi_player"];
    [[NSFileManager defaultManager] createDirectoryAtPath:configDir withIntermediateDirectories:YES attributes:nil error:nil];
    ConfigureBackgroundReadableFiles(docsDir);
    AppConfig::Path::setConfigDir([configDir UTF8String]);
    AppConfig::Path::setMusicDir([docsDir UTF8String]);

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

    // 2. 初始化 Dear ImGui 核心与 Metal 后端 (防重复初始化保护)
    if (ImGui::GetCurrentContext() == nullptr) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();

        Fonts::initialize(io);
        ImGui_ImplMetal_Init(self.device);
    } else {
        ImGuiIO& io = ImGui::GetIO();
        if (io.BackendRendererUserData == nullptr) {
            ImGui_ImplMetal_Init(self.device);
        }
    }

    // 3. 初始化 C++ 发烧音频核心界面中枢
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

    if (_renderer) {
        _renderer->render(view.bounds.size.width, view.bounds.size.height);
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
    self.isScrolling = NO;
    self.isItemDrag = NO;
    self.scrollVelocityY = 0.0f; // 立即中止惯性滚动

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
            // 判定主要位移方向：纵向显著 -> 判定为上下滑动手势
            if (fabs(dy) > fabs(dx) * 0.7f) {
                self.isScrolling = YES;
                // 立即释放鼠标左键，取消被点选按钮或列表项的激活态，防止误触发点击
                io.AddMouseButtonEvent(0, false);
            } else {
                // 横向显著 -> 判定为滑块拖动 (如音量条、进度条)
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
        // 映射为标准 ImGui 滚轮事件 (1:1 像素跟手平滑滚动)
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

    if (self.isScrolling) {
        // 限幅惯性滑动初速度，防止超速飞出
        if (self.scrollVelocityY > 2800.0f) self.scrollVelocityY = 2800.0f;
        if (self.scrollVelocityY < -2800.0f) self.scrollVelocityY = -2800.0f;
        if (fabsf(self.scrollVelocityY) < 120.0f) self.scrollVelocityY = 0.0f;

        io.AddMousePosEvent(pt.x, pt.y);
        self.isScrolling = NO;
    } else {
        // 普通点按 (Tap) 或横向滑块拖动结束
        io.AddMousePosEvent(pt.x, pt.y);
        io.AddMouseButtonEvent(0, false);
    }
    self.isItemDrag = NO;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(0, false);
    self.isScrolling = NO;
    self.isItemDrag = NO;
    self.scrollVelocityY = 0.0f;
}

@end
