//
//  PhoneViewController.h
//  PiHiEndMusic
//
//  Created by MK-10 on 2026/9/30.
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface PhoneViewController : UIViewController

// 动态切换模式并应用屏幕方向 (1: 数播横屏模式, 2: 主端竖屏模式)
- (void)switchToMode:(int)mode;

@end

NS_ASSUME_NONNULL_END
