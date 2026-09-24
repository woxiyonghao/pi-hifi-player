#include "Application.hpp"
#include <iostream>

// ==============================================================================
// 纯音数播桌面端主入口 (Modern C++20)
// 极简启动外壳，所有视窗驱动与视图管线已收敛至 Application 与 View 三驾马车
// ==============================================================================
int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    std::cout << "[PiHifiPlayer] 启动发烧级纯音数播系统 (Modern C++20)..." << std::endl;

    Application app;
    if (!app.init()) {
        std::cerr << "[PiHifiPlayer] 应用程序图形与事件管线初始化失败，紧急终止！" << std::endl;
        return -1;
    }

    return app.run();
}
