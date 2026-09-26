#!/usr/bin/env bash
# ==============================================================================
# Mac 本地代码增量部署与树莓派 5 编译脚本
# 用法：./scripts/deploy_pi.sh [树莓派IP或主机名] [用户名] (默认: winheo-pi.local winheo)
# ==============================================================================
set -e

PI_HOST="${1:-winheo-pi.local}"
PI_USER="${2:-winheo}"
REMOTE_DIR="/home/${PI_USER}/pi-hifi-music"

echo "=== [1/3] 检查网络连接与创建远程目录 (${PI_USER}@${PI_HOST}) ==="
ssh "${PI_USER}@${PI_HOST}" "mkdir -p ${REMOTE_DIR}"

echo "=== [2/3] 增量同步代码与脚本到树莓派 5 ==="
rsync -avz --delete \
    --exclude "build/" \
    --exclude ".git/" \
    --exclude ".cache/" \
    --exclude "*.DS_Store" \
    ./ "${PI_USER}@${PI_HOST}:${REMOTE_DIR}/"

echo "=== [3/3] 远程检查依赖、编译并热重启 ==="
ssh -t "${PI_USER}@${PI_HOST}" bash -c "'
    set -e
    cd ${REMOTE_DIR}

    # 如果尚未安装 CMake 或 SDL2，自动执行初始化环境脚本
    if ! command -v cmake &> /dev/null || ! pkg-config --exists sdl2 2>/dev/null; then
        echo \">> [首次部署] 检测到树莓派未安装完整依赖，正在自动执行 setup_pi.sh...\"
        bash scripts/setup_pi.sh
        echo \">> 提示：首次安装赋予了 DRM 显卡硬件权限，若后续显示异常请执行一次 sudo reboot\"
    fi

    # 原生编译 Release 高性能版本
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j4

    # 若已注册为自启动服务，自动热重启更新界面
    if systemctl is-enabled pi-hifi.service &>/dev/null; then
        echo \">> 检测到自启动服务已激活，正在热重启 pi-hifi.service...\"
        sudo systemctl restart pi-hifi.service
    fi
'"

echo ""
echo "=========================================================="
echo " 🎉 代码同步与编译完成！"
echo "=========================================================="
