#!/usr/bin/env bash
# ==============================================================================
# 树莓派 5 (Raspberry Pi OS Lite 64-bit) 基础运行环境一键配置脚本
# 用法：在树莓派终端直接执行：bash scripts/setup_pi.sh
# ==============================================================================
set -e

echo "=== [1/5] 更新系统软件源并安装基础编译套件 ==="
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config git

echo "=== [2/5] 安装图形(KMS/DRM + GLESv2)与中文字体库 ==="
# 树莓派官方源的 libsdl2-dev 自带 kmsdrm 后端支持
sudo apt install -y libsdl2-dev libgles2-mesa-dev libgl1-mesa-dev fonts-wqy-microhei

echo "=== [3/5] 音频解码、数据库与硬件接口依赖预装 ==="
sudo apt install -y libasound2-dev libgpiod-dev libsqlite3-dev libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

echo "=== [4/5] 配置普通用户 KMS/DRM 直接访问显卡与输入设备权限 ==="
CURRENT_USER=$(whoami)
sudo usermod -aG video,render,input,audio,dialout "$CURRENT_USER"

echo "=== [5/5] 配置普通用户免密管理 pi-hifi 服务规则 ==="
echo "${CURRENT_USER} ALL=(ALL) NOPASSWD: /usr/bin/systemctl restart pi-hifi.service, /usr/bin/systemctl stop pi-hifi.service, /usr/bin/systemctl start pi-hifi.service, /usr/bin/systemctl status pi-hifi.service, /bin/systemctl restart pi-hifi.service, /bin/systemctl stop pi-hifi.service, /bin/systemctl start pi-hifi.service, /bin/systemctl status pi-hifi.service" | sudo tee /etc/sudoers.d/pi-hifi > /dev/null
sudo chmod 0440 /etc/sudoers.d/pi-hifi

echo ""
echo "=========================================================="
echo " 树莓派 5 HiFi 数播运行环境配置完成！"
echo " 提示：为了让用户组权限彻底生效，建议执行一次: sudo reboot"
echo "=========================================================="
