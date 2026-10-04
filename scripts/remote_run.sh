#!/usr/bin/env bash
# ==============================================================================
# 树莓派 5 远端自动化构建与运行脚本 (在树莓派本地执行)
# 支持动作：foreground (默认前台运行), daemon (后台运行), service (自启服务), kill (停止), log (查日志)
# ==============================================================================
set -e

ACTION="${1:-foreground}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}"

CURRENT_USER=$(whoami)

# 颜色输出定义
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RED="\033[1;31m"
RESET="\033[0m"

echo -e "${CYAN}=== 树莓派 5 核心执行引擎 (用户: ${CURRENT_USER}, 目录: ${PROJECT_DIR}, 动作: ${ACTION}) ===${RESET}"

# 1. 如果只需要停止进程
if [ "${ACTION}" = "kill" ]; then
    echo -e "${YELLOW}>> 正在停止树莓派上的 PiHifiPlayer 进程及服务...${RESET}"
    if systemctl is-active pi-hifi.service &>/dev/null; then
        sudo systemctl stop pi-hifi.service || true
    fi
    pkill -9 -f PiHifiPlayer 2>/dev/null || true
    echo -e "${GREEN}>> ✅ 已完全停止树莓派上的播放器进程。${RESET}"
    exit 0
fi

# 2. 如果只需要查看日志
if [ "${ACTION}" = "log" ]; then
    if systemctl is-active pi-hifi.service &>/dev/null; then
        echo -e "${CYAN}>> 正在实时追踪 pi-hifi.service 运行日志 (按 Ctrl+C 退出)...${RESET}"
        sudo journalctl -u pi-hifi.service -f -n 50
    elif [ -f "${PROJECT_DIR}/pi-hifi.log" ]; then
        echo -e "${CYAN}>> 正在实时追踪后台日志 pi-hifi.log (按 Ctrl+C 退出)...${RESET}"
        tail -f -n 50 "${PROJECT_DIR}/pi-hifi.log"
    else
        echo -e "${RED}>> 未找到运行日志，当前播放器可能未启动。${RESET}"
    fi
    exit 0
fi

# 3. 依赖检查与自动安装
MISSING_DEPS=0
if ! command -v cmake &> /dev/null || ! pkg-config --exists sdl2 2>/dev/null; then
    MISSING_DEPS=1
fi

if [ "${MISSING_DEPS}" -eq 1 ]; then
    echo -e "${YELLOW}>> [依赖检查] 检测到系统缺少必要编译库 (如 SQLite3 / SDL2 / CMake)，正在自动补全...${RESET}"
    echo -e "${YELLOW}>> 提示：若终端提示密码，请输入树莓派用户 ${CURRENT_USER} 的 sudo 密码。${RESET}"
    sudo apt update
    sudo apt install -y build-essential cmake ninja-build pkg-config git \
        libsdl2-dev libgles2-mesa-dev libgl1-mesa-dev fonts-wqy-microhei \
        libasound2-dev libsqlite3-dev
    
    # 确保当前用户有 DRM/KMS 访问权限
    sudo usermod -aG video,render,input,audio,dialout "${CURRENT_USER}"
fi

# 4. 原生 Release 性能优化编译
echo -e "${CYAN}>> [1/2] 正在进行树莓派 5 (Cortex-A76) 原生高性能编译...${RESET}"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || echo 4)
echo -e "${GREEN}>> ✅ 编译完成！可执行程序: ${PROJECT_DIR}/build/PiHifiPlayer${RESET}"

# 5. 解除 ALSA 声卡硬件静音并提升至 100% 线路电平 (确保输出给后级功放不偏小)
echo -e "${CYAN}>> 正在校准 ALSA 硬件混音器 (解除静音并拉满 100% 线路电平直通功放)...${RESET}"
for c in 0 1 2 3; do
    amixer -c $c sset Master 100% unmute 2>/dev/null || true
    amixer -c $c sset PCM 100% unmute 2>/dev/null || true
    amixer -c $c sset Headphone 100% unmute 2>/dev/null || true
    amixer -c $c sset Speaker 100% unmute 2>/dev/null || true
    amixer -c $c sset 'Line Out' 100% unmute 2>/dev/null || true
done

# 6. 根据所选动作启动程序
echo -e "${CYAN}>> [2/2] 执行启动策略 (${ACTION})...${RESET}"

if [ "${ACTION}" = "service" ]; then
    echo -e "${YELLOW}>> 正在注册并激活开机免密直启服务 (pi-hifi.service)...${RESET}"
    
    # 写入免密管理 sudoers 规则
    echo "${CURRENT_USER} ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart pi-hifi.service, /usr/bin/systemctl stop pi-hifi.service, /usr/bin/systemctl start pi-hifi.service, /usr/bin/systemctl status pi-hifi.service, /bin/systemctl restart pi-hifi.service, /bin/systemctl stop pi-hifi.service, /bin/systemctl start pi-hifi.service, /bin/systemctl status pi-hifi.service" | sudo tee /etc/sudoers.d/pi-hifi > /dev/null
    sudo chmod 0440 /etc/sudoers.d/pi-hifi

    # 动态写入 systemd 服务文件
    sudo cp scripts/pi-hifi.service /etc/systemd/system/
    sudo sed -i "s|User=winheo|User=${CURRENT_USER}|g" /etc/systemd/system/pi-hifi.service
    sudo sed -i "s|Group=winheo|Group=${CURRENT_USER}|g" /etc/systemd/system/pi-hifi.service
    sudo sed -i "s|/home/winheo/pi-hifi-music|${PROJECT_DIR}|g" /etc/systemd/system/pi-hifi.service

    sudo systemctl daemon-reload
    sudo systemctl enable pi-hifi.service
    sudo systemctl restart pi-hifi.service
    
    echo -e "${GREEN}>> 🎉 pi-hifi.service 已成功激活并在屏幕点亮！已配置开机自启。${RESET}"
    systemctl status pi-hifi.service --no-pager
    exit 0
fi

if [ "${ACTION}" = "daemon" ]; then
    echo -e "${YELLOW}>> 正在后台常驻启动 PiHifiPlayer (断开 SSH 屏幕依然保持点亮)...${RESET}"
    # 停止旧进程与可能冲突的 service
    if systemctl is-active pi-hifi.service &>/dev/null; then
        sudo systemctl stop pi-hifi.service || true
    fi
    pkill -f PiHifiPlayer 2>/dev/null || true
    sleep 0.5

    nohup env SDL_VIDEODRIVER=kmsdrm SDL_RENDER_VSYNC=1 ./build/PiHifiPlayer > "${PROJECT_DIR}/pi-hifi.log" 2>&1 &
    sleep 1

    if pgrep -f PiHifiPlayer > /dev/null; then
        echo -e "${GREEN}>> 🎉 播放器已在后台成功启动 (PID: $(pgrep -f PiHifiPlayer))！${RESET}"
        echo -e "${CYAN}>> 屏幕已点亮！如需查看日志请运行: ./scripts/deploy_pi.sh -l${RESET}"
    else
        echo -e "${RED}>> ❌ 后台启动失败，最近日志：${RESET}"
        tail -n 25 "${PROJECT_DIR}/pi-hifi.log"
        exit 1
    fi
    exit 0
fi

# 默认模式：如果已经安装了自启服务，则优先重启服务
if systemctl is-enabled pi-hifi.service &>/dev/null; then
    echo -e "${YELLOW}>> 检测到已安装自启服务 pi-hifi.service，正在热重启服务...${RESET}"
    sudo systemctl restart pi-hifi.service
    echo -e "${GREEN}>> 🎉 服务已热重启，新版本界面已呈现在树莓派屏幕上！${RESET}"
    echo -e "${CYAN}>> 正在追踪最近运行日志 (按 Ctrl+C 即可退出日志追踪，播放器保持运行):${RESET}"
    sudo journalctl -u pi-hifi.service -f -n 15
    exit 0
fi

# 默认前台交互模式：直接在终端输出日志，树莓派屏幕实时显示，按 Ctrl+C 安全停止
echo -e "${YELLOW}>> 正在前台直通启动 PiHifiPlayer (KMS/DRM 直出屏幕，按 Ctrl+C 优雅停止)...${RESET}"
pkill -f PiHifiPlayer 2>/dev/null || true
sleep 0.3
exec env SDL_VIDEODRIVER=kmsdrm SDL_RENDER_VSYNC=1 ./build/PiHifiPlayer
