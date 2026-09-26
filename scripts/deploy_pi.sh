#!/usr/bin/env bash
# ==============================================================================
# Mac 本地代码增量部署与树莓派 5 编译脚本
# 用法：./scripts/deploy_pi.sh [树莓派IP或主机名] (默认: pi-hifi.local)
# ==============================================================================
set -e

PI_HOST="${1:-pi-hifi.local}"
PI_USER="pi"
REMOTE_DIR="/home/${PI_USER}/pi-hifi-music"

echo "=== [1/2] 增量同步代码到树莓派 5 (${PI_USER}@${PI_HOST}) ==="
rsync -avz --delete \
    --exclude "build/" \
    --exclude ".git/" \
    --exclude ".cache/" \
    --exclude "*.DS_Store" \
    ./ "${PI_USER}@${PI_HOST}:${REMOTE_DIR}/"

echo "=== [2/2] 在树莓派 5 上执行原生编译与服务重启 ==="
ssh "${PI_USER}@${PI_HOST}" bash -c "'
    set -e
    cd ${REMOTE_DIR}
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j4
    if systemctl is-enabled pi-hifi.service &>/dev/null; then
        echo \">> 重启 pi-hifi.service 服务...\"
        sudo systemctl restart pi-hifi.service
    fi
'"

echo "=========================================================="
echo " 部署与编译完成！"
echo "=========================================================="
