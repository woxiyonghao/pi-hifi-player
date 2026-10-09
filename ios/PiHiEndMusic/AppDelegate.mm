//
//  AppDelegate.mm
//  PiHiEndMusic
//
//  Created by MK-10 on 2026/9/30.
//

#import "AppDelegate.h"
#import "define/define.h"
#import "ipad/PadViewController.h"
#import "iphone/PhoneViewController.h"
#import <AVFoundation/AVFoundation.h>

namespace {

uint32_t gPreferredIOBufferFrames = 256;

dispatch_queue_t AudioSessionQueue() {
    static dispatch_queue_t queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("com.pihifi.audio-session", DISPATCH_QUEUE_SERIAL);
    });
    return queue;
}

void LogCurrentAudioRoute(AVAudioSession *session, NSString *reason) {
    AVAudioSessionPortDescription *output = session.currentRoute.outputs.firstObject;
    NSLog(@"[AVAudioSession] %@: output=%@, sampleRate=%.0f, ioBuffer=%.2fms",
          reason,
          output.portType ?: @"none",
          session.sampleRate,
          session.IOBufferDuration * 1000.0);
}

void KeepPadAudioSessionActive(NSString *reason) {
    if (!IS_IPAD && !IS_IPHONE) return;

    dispatch_async(AudioSessionQueue(), ^{
        AVAudioSession *session = [AVAudioSession sharedInstance];
        NSError *error = nil;
        BOOL activated = [session setActive:YES error:&error];
        if (!activated) {
            NSLog(@"[AVAudioSession] %@ activation error: %@", reason, error.localizedDescription);
        } else {
            LogCurrentAudioRoute(session, reason);
        }
    });
}

} // namespace

// iPad / iPhone 专用桥接：系统设置中的 64/256/512 表示 AVAudioSession 的首选
// 硬件 I/O 帧数，而不是 AudioQueue 每个应用层推流 buffer 的分配尺寸。
// 返回 true 只表示该 iOS 设置已在这里处理；蓝牙路由可能会采用自己的实际值。
extern "C" bool HifiPadSetPreferredIOBufferFrames(uint32_t frames) {
    if (!IS_IPAD && !IS_IPHONE) {
        return false;
    }

    AVAudioSession *session = [AVAudioSession sharedInstance];
    gPreferredIOBufferFrames = frames;
    const double sampleRate = session.sampleRate > 0.0 ? session.sampleRate : 48000.0;
    const NSTimeInterval duration = static_cast<NSTimeInterval>(frames) / sampleRate;
    NSError *error = nil;
    BOOL changed = [session setPreferredIOBufferDuration:duration error:&error];
    if (!changed) {
        NSLog(@"[AVAudioSession] setPreferredIOBufferDuration(%u) error: %@",
              frames, error.localizedDescription);
    }
    LogCurrentAudioRoute(session, [NSString stringWithFormat:@"preferred buffer=%u frames", frames]);
    return true;
}

// 在 iOS (iPad / iPhone) 的 A2DP 输出上返回实际采样率；其他设备与其他平台返回 0，
// 让核心继续沿用原来的解码和直通路径。
extern "C" uint32_t HifiPadGetBluetoothOutputSampleRate() {
    if (!IS_IPAD && !IS_IPHONE) {
        return 0;
    }

    AVAudioSession *session = [AVAudioSession sharedInstance];
    for (AVAudioSessionPortDescription *output in session.currentRoute.outputs) {
        if ([output.portType isEqualToString:AVAudioSessionPortBluetoothA2DP]) {
            return session.sampleRate > 0.0
                ? static_cast<uint32_t>(session.sampleRate + 0.5)
                : 48000;
        }
    }
    return 0;
}

@implementation AppDelegate

- (void)setupAudioSession {
    NSError *error = nil;
    AVAudioSession *session = [AVAudioSession sharedInstance];

    // Playback 本身就支持 A2DP。AllowBluetoothA2DP 只适用于支持输入的类别，
    // 与 Playback 组合会返回 OSStatus -50，并留下不稳定的旧会话配置。
    // 复刻 iPad 稳定做法：options 设为 0。
    AVAudioSessionCategoryOptions options = 0;
    BOOL categorySet = [session setCategory:AVAudioSessionCategoryPlayback
                                       mode:AVAudioSessionModeDefault
                                    options:options
                                      error:&error];
    if (!categorySet) {
        NSLog(@"[AVAudioSession] setCategory error: %@", error.localizedDescription);
    }

    if (IS_IPAD || IS_IPHONE) {
        HifiPadSetPreferredIOBufferFrames(gPreferredIOBufferFrames);
    }

    if (IS_IPAD || IS_IPHONE) {
        // 串行激活，避免锁屏/中断恢复与启动流程并发操作同一个 AudioSession。
        KeepPadAudioSessionActive(@"activated");
    } else {
        error = nil;
        [session setActive:YES error:&error];
        if (error) {
            NSLog(@"[AVAudioSession] setActive error: %@", error.localizedDescription);
        }
    }

    // 监听音频硬件路由变更 (例如 AirPods Pro 佩戴/摘下/重新连接)
    [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionRouteChangeNotification
                                                      object:nil
                                                       queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        NSLog(@"[AVAudioSession] 音频输出设备路由变更: %@", note.userInfo);
        AVAudioSession *currentSession = [AVAudioSession sharedInstance];
        if (IS_IPAD || IS_IPHONE) {
            // 不在路由通知里 setActive 或重提缓冲偏好；两者都会再次触发
            // A2DP 配置协商，形成 AirPods/扬声器来回跳转与边界爆音。
            LogCurrentAudioRoute(currentSession, @"route changed");
        } else {
            [currentSession setActive:YES error:nil];
        }
    }];

    // 监听音频中断 (例如电话呼入/系统强占恢复)
    [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioSessionInterruptionNotification
                                                      object:nil
                                                       queue:[NSOperationQueue mainQueue]
                                                  usingBlock:^(NSNotification * _Nonnull note) {
        NSDictionary *info = note.userInfo;
        AVAudioSessionInterruptionType type = (AVAudioSessionInterruptionType)[info[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
        if (type == AVAudioSessionInterruptionTypeEnded) {
            if (IS_IPAD || IS_IPHONE) {
                KeepPadAudioSessionActive(@"interruption ended");
            } else {
                [[AVAudioSession sharedInstance] setActive:YES error:nil];
            }
        }
    }];
}

- (void)setupRootWindow:(UIWindowScene *)windowScene {
    if (self.window) {
        if (windowScene && !self.window.windowScene) {
            self.window.windowScene = windowScene;
        }
        return;
    }

    if (windowScene) {
        self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
        if (IS_IPAD) {
            if (@available(iOS 16.0, *)) {
                UIWindowSceneGeometryPreferencesIOS *preferences = [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
                [windowScene requestGeometryUpdateWithPreferences:preferences errorHandler:nil];
            }
        }
    } else {
        self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    }

    if (IS_IPAD) {
        self.window.rootViewController = [[PadViewController alloc] init];
    } else {
        self.window.rootViewController = [[PhoneViewController alloc] init];
    }

    [self.window makeKeyAndVisible];
}

#pragma mark - UIApplicationDelegate (古法编程入口)

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    [self setupAudioSession];
    return YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    // 无 Scene 的兼容入口；后台音频模式已在 Info.plist 声明。
    KeepPadAudioSessionActive(@"application background");
}

- (UIInterfaceOrientationMask)application:(UIApplication *)application supportedInterfaceOrientationsForWindow:(UIWindow *)window {
    if (IS_IPAD) {
        return UIInterfaceOrientationMaskLandscape;
    }
    return UIInterfaceOrientationMaskAllButUpsideDown;
}

#pragma mark - UIWindowSceneDelegate (同体合一，满足 Apple SDK 强制检查，无需独立 SceneDelegate 文件)

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions {
    if ([scene isKindOfClass:[UIWindowScene class]]) {
        [self setupRootWindow:(UIWindowScene *)scene];
    }
}

- (void)sceneDidEnterBackground:(UIScene *)scene {
    // 锁屏会让 Scene 进入后台，但播放会话必须保持激活。
    KeepPadAudioSessionActive(@"scene background");
}

- (void)sceneWillEnterForeground:(UIScene *)scene {
    KeepPadAudioSessionActive(@"scene foreground");
}

@end
