#!/usr/bin/env bash
# ==============================================================================
# 树莓派 5 (Raspberry Pi OS Lite 64-bit) 基础运行环境一键配置脚本
# 用法：在树莓派终端直接执行：bash scripts/setup_pi.sh
# ==============================================================================
set -e

echo "=== [1/4] 更新系统软件源并安装基础编译套件 ==="
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config git

echo "=== [2/4] 安装图形(KMS/DRM + GLESv2)与中文字体库 ==="
# 树莓派官方源的 libsdl2-dev 自带 kmsdrm 后端支持
sudo apt install -y libsdl2-dev libgles2-mesa-dev libgl1-mesa-dev fonts-wqy-microhei

echo "=== [3/4] 后续关卡外设与音频解码依赖预装 (ALSA / libgpiod / FFmpeg) ==="
sudo apt install -y libasound2-dev libgpiod-dev libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

echo "=== [4/4] 配置普通用户 KMS/DRM 直接访问显卡与输入设备权限 ==="
CURRENT_USER=$(whoami)
sudo usermod -aG video,render,input,audio,dialout "$CURRENT_USER"

echo ""
echo "=========================================================="
echo " 树莓派 5 HiFi 数播运行环境配置完成！"
echo " 提示：为了让用户组权限生效，建议执行: sudo reboot"
echo "=========================================================="
