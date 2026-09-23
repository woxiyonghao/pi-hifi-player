#include "SidebarView.hpp"
#include "imgui.h"
#include <string>
SidebarView::SidebarView() {}

// ==============================================================================
// 绘制分组小标题 (如: "FEATURES / 功能", "PLAYLISTS / 播放列表")
// ==============================================================================
void SidebarView::drawSectionHeader(const char* title) {
    // ImGui::Dummy 并非创建内存对象，而是推进内部排版光标：此处在标题上方空出 6px 的外边距 (Margin-Top)
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    // 将文本颜色压入 ImGui 样式栈：使用 Apple 经典的次级灰小字 (RGBA: 130, 140, 155)
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(130, 140, 155, 255));
    // 将排版光标右移 12px，使其与下方的胶囊按钮文字保持完美的垂直对齐线
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
    // TextUnformatted 比 Text 效率更高：跳过 printf 字符串格式化解析，直接快速渲染静态文本
    ImGui::TextUnformatted(title);
    // 弹出样式栈，恢复默认文字颜色
    ImGui::PopStyleColor();
    // 在标题下方再空出 2px 的微小间距 (Margin-Bottom)
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

// ==============================================================================
// 绘制单个 Apple 胶囊风格的导航菜单按钮
// @param icon: 图标标识 (如 ">" 或 UTF-8 发烧图标)
// @param label: 按钮文本 (如 "扫描音乐", "蔡琴经典试音")
// @param is_selected: 当前项是否处于被激活选中状态
// @return: bool 本帧是否被用户点击触发
// ==============================================================================
bool SidebarView::drawNavItem(const char* icon, const char* label, bool is_selected) {
    // 获取当前窗口底层的原生绘图命令列表 (用于直接调用 GPU 绘制圆角矩形和线条)
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // 获取当前排版游标在屏幕上的绝对物理像素坐标 (X, Y)
    ImVec2 pos = ImGui::GetCursorScreenPos();
    // 动态计算按钮宽度：填满侧边栏当前可用宽度，右侧预留 4px 安全边距
    float width = ImGui::GetContentRegionAvail().x - 4.0f;
    float height = 34.0f; // 舒适的触控高度 (适合 7 寸屏手指点击与鼠标悬停)
    // 【即时模式交互核心】：放置一个隐形按钮 (InvisibleButton) 覆盖在此区域
    // 负责捕获鼠标/触控屏的 Hover（悬停）与 Click（点击）事件，而视觉画面由我们完全自定义手绘
    std::string btn_id = std::string("##nav_") + label;
    bool clicked = ImGui::InvisibleButton(btn_id.c_str(), ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    // 计算胶囊矩形的左上角 (p_min) 和右下角 (p_max)
    ImVec2 p_min = pos;
    ImVec2 p_max = ImVec2(pos.x + width, pos.y + height);
    // --------------------------------------------------------------------------
    // [1] 背景层绘制：Apple 拟态磨砂高光与圆角胶囊 (Rounding = 8px)
    // --------------------------------------------------------------------------
    if (is_selected) {
        // 激活状态：绘制半透明乳白高光胶囊底色 (Alpha: 36，营造通透玻璃悬浮感)
        dl->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 36), 8.0f);
        // 激活指示线：在胶囊最左侧绘制一条 3px 宽的主色调坚条 (跟随主题色发光)
        dl->AddRectFilled(ImVec2(p_min.x + 2.0f, p_min.y + 7.0f), 
                          ImVec2(p_min.x + 5.0f, p_max.y - 7.0f), 
                          accent_color_, 2.0f);
    } else if (hovered) {
        // 悬停状态：微弱浅亮提示 (Alpha: 16)
        dl->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 16), 8.0f);
    }
    // --------------------------------------------------------------------------
    // [2] 前景层绘制：图标与文本
    // --------------------------------------------------------------------------
    // 图标色彩逻辑：激活时使用主色调 (如 Apple 玫红/湖蓝)，未激活时使用静音灰白
    ImU32 icon_col = is_selected ? accent_color_ : IM_COL32(180, 188, 200, 255);
    // 文本色彩逻辑：激活时纯白高亮，未激活时使用柔和灰白
    ImU32 text_col = is_selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 225, 220);
    // 绘制左侧图标 (文字基线向下微调 8px 实现居中对齐)
    dl->AddText(ImVec2(pos.x + 14.0f, pos.y + 8.0f), icon_col, icon);
    // 绘制菜单标题文字
    dl->AddText(ImVec2(pos.x + 36.0f, pos.y + 8.0f), text_col, label);
    // 每个菜单项底部空出 1px 的垂直微间距，防止连续排列过于紧凑
    ImGui::Dummy(ImVec2(0.0f, 1.0f));
    return clicked;
}

// ==============================================================================
// 主渲染函数：绘制完整侧边栏
// @param playlists: 外部传入的歌单实体数据只读引用 (Model 数据源解耦)
// @param width: 侧边栏宽度 (默认 230px)
// @param height: 侧边栏高度 (满屏 600px)
// ==============================================================================
void SidebarView::render(const std::vector<Playlist>& playlists, float width, float height) {
    // 强制钉死窗口在屏幕左上角 (0, 0)
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    // 锁定窗口尺寸为 (width, height)
    ImGui::SetNextWindowSize(ImVec2(width, height));
    // 配置窗口无边框、无滚动条、不可拖拽缩放，表现如同原生嵌入的侧边面板
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar 
                           | ImGuiWindowFlags_NoResize 
                           | ImGuiWindowFlags_NoMove 
                           | ImGuiWindowFlags_NoCollapse;
    // 配置深邃的半透明深灰底色 (Apple macOS Vibrancy 暗黑风格：RGBA 18, 22, 28, 240)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(18, 22, 28, 240));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0)); // 消除默认外边框
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 12.0f)); // 窗口内部内边距
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f); // 侧栏贴边不需要大圆角
    ImGui::Begin("##AppleSidebar", nullptr, flags);
    // =========================================================================
    // 1. 顶部五大核心功能项 (排版与状态路由)
    // =========================================================================
    drawSectionHeader("FEATURES / 功能");
    if (drawNavItem(">", "扫描音乐", current_tab_ == SidebarTab::ScanMusic)) {
        current_tab_ = SidebarTab::ScanMusic;
    }
    if (drawNavItem(">", "Equalizer (EQ)", current_tab_ == SidebarTab::Equalizer)) {
        current_tab_ = SidebarTab::Equalizer;
    }
    if (drawNavItem(">", "DACSettings", current_tab_ == SidebarTab::DACSettings)) {
        current_tab_ = SidebarTab::DACSettings;
    }
    if (drawNavItem(">", "ThemeSettings", current_tab_ == SidebarTab::ThemeSettings)) {
        current_tab_ = SidebarTab::ThemeSettings;
    }
    if (drawNavItem(">", "SystemSettings", current_tab_ == SidebarTab::SystemSettings)) {
        current_tab_ = SidebarTab::SystemSettings;
    }
    // =========================================================================
    // 2. 播放列表区 (纯数据驱动：根据 Model 实体列表动态循环渲染)
    // =========================================================================
    drawSectionHeader("PLAYLISTS / 播放列表");
    // 固定首项：所有音乐曲库
    if (drawNavItem(">", "所有音乐", current_tab_ == SidebarTab::AllMusic)) {
        current_tab_ = SidebarTab::AllMusic;
    }
    // 遍历真正的 Playlist 业务实体模型
    for (const auto& playlist : playlists) {
        // 判断当前歌单是否被激活：比对歌单的唯一 UUID/ID，而非易出错的数组下标
        bool is_sel = (current_tab_ == SidebarTab::CustomPlaylist && selected_playlist_id_ == playlist.getId());
        if (drawNavItem(">", playlist.getName().c_str(), is_sel)) {
            current_tab_ = SidebarTab::CustomPlaylist;
            selected_playlist_id_ = playlist.getId();
        }
    }
    // --------------------------------------------------------------------------
    // ➕ 添加播放列表交互入口
    // --------------------------------------------------------------------------
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, accent_color_); // 文字使用主色调高亮
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14.0f);
    if (ImGui::Selectable("+ 添加播放列表", false)) {
        // 解耦设计：View 不擅自操作数据，而是通过回调通知外部 Controller/Model 执行新增
        if (on_create_playlist_) {
            on_create_playlist_();
        }
    }
    ImGui::PopStyleColor();
    // =========================================================================
    // 3. 底部硬件 DAC 直通状态 (发烧友常驻微徽标)
    // =========================================================================
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bottom_pos(10.0f, height - 38.0f);
        // 微型悬浮底框
        dl->AddRectFilled(bottom_pos, ImVec2(width - 10.0f, height - 10.0f), 
                          IM_COL32(255, 255, 255, 12), 6.0f);
        // Bit-Perfect 发烧绿灯 (指示无损源码直出状态)
        dl->AddCircleFilled(ImVec2(bottom_pos.x + 14.0f, bottom_pos.y + 14.0f), 4.0f, 
                            IM_COL32(52, 199, 89, 255));
        // 解码芯片硬件标识文本
        dl->AddText(ImVec2(bottom_pos.x + 26.0f, bottom_pos.y + 6.0f), 
                    IM_COL32(190, 200, 215, 210), "ES9038PRO Balanced");
    }
    // =========================================================================
    // 4. 右侧边缘 1px 细线微光分割线 (模拟玻璃折射边缘，分隔左侧栏与主舞台)
    // =========================================================================
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(width - 1.0f, 0.0f), ImVec2(width - 1.0f, height), 
                    IM_COL32(255, 255, 255, 20), 1.0f);
    }
    ImGui::End();
    // 对称弹出之前压入的样式参数，保持样式栈平衡干净
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}