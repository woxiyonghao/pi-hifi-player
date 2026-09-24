#pragma once
#include "imgui.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "SidebarTypes.hpp"
// 供父级（SidebarView）绘制背景滑动发光胶囊使用的坐标目标
struct FeatureIndicatorTarget {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// 功能菜单项元数据
struct FeatureItem {
    SidebarTab tab;
    std::string label;
};

class SidebarFeatureWidget {
public:
    using TabSelectCallback = std::function<void(SidebarTab)>;
    SidebarFeatureWidget();
    ~SidebarFeatureWidget() = default;
    // 渲染 Feature 功能区
    // @param current_tab 当前选中的侧边栏标签
    // @param on_select 点击选项时的回调
    // @return 若当前激活项属于本组件，返回指示器坐标；否则返回 std::nullopt
    std::optional<FeatureIndicatorTarget> render(SidebarTab current_tab, TabSelectCallback on_select = nullptr);
    [[nodiscard]] const std::vector<FeatureItem>& getFeatures() const { return features_; }
private:
    // 渲染分组小标题
    void drawHeader(const char* title);
    // 渲染单项菜单胶囊
    bool drawFeatureItem(SidebarTab tab, const char* label, bool is_selected, FeatureIndicatorTarget& out_target);
    // 纯 GPU 几何矢量图标绘制 (5大功能专属图标：搜索、EQ、DAC、主题、系统)
    void drawFeatureIcon(ImDrawList* dl, ImVec2 center, SidebarTab tab, ImU32 color);
private:
    std::vector<FeatureItem> features_;
};