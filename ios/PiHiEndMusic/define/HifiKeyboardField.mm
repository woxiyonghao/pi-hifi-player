#import "HifiKeyboardField.h"
#import "imgui.h"

@implementation HifiKeyboardField

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.delegate = self;
        self.autocorrectionType = UITextAutocorrectionTypeNo;
        self.autocapitalizationType = UITextAutocapitalizationTypeNone;
        self.spellCheckingType = UITextSpellCheckingTypeNo;
        self.keyboardAppearance = UIKeyboardAppearanceDark;
        self.returnKeyType = UIReturnKeyDone;
        self.alpha = 0.01;
        [self addTarget:self action:@selector(onTextChanged:) forControlEvents:UIControlEventEditingChanged];
    }
    return self;
}

- (BOOL)hasText {
    // 保持系统软键盘的退格 (Delete) 键处于始终可用状态
    return YES;
}

- (void)deleteBackward {
    UITextRange *markedRange = [self markedTextRange];
    // 仅在无拼音合成过程 (非 Marked Text 状态) 时，向下发 ImGui 退格事件
    if (markedRange == nil || [self positionFromPosition:markedRange.start offset:0] == nil) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_Backspace, true);
        io.AddKeyEvent(ImGuiKey_Backspace, false);
    }
    [super deleteBackward];
}

- (void)onTextChanged:(UITextField *)textField {
    UITextRange *markedRange = [textField markedTextRange];
    // 当非拼音组合中途阶段，提取用户敲定选取的文字 (支持中文、英文、数字、标点) 派发给 ImGui
    if (markedRange == nil || [textField positionFromPosition:markedRange.start offset:0] == nil) {
        NSString *text = textField.text;
        if (text.length > 0) {
            ImGuiIO& io = ImGui::GetIO();
            io.AddInputCharactersUTF8([text UTF8String]);
            textField.text = @"";
        }
    }
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    io.AddKeyEvent(ImGuiKey_Enter, false);
    [textField resignFirstResponder];
    return YES;
}

@end
