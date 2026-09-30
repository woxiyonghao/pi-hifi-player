#!/usr/bin/env bash
# ==============================================================================
# 本地音乐一键同步到树莓派 5 SD 卡脚本
# 用法:
#   ./scripts/sync_music.sh                      # 默认同步 /Users/lusesi/Desktop/music2
#   ./scripts/sync_music.sh /path/to/my/music    # 指定任意本地音乐目录
# ==============================================================================
set -e

LOCAL_MUSIC_DIR="${1:-/Users/lusesi/Desktop/music2}"
PI_HOST="${2:-192.168.1.169}"
PI_USER="${3:-winheo}"
REMOTE_MUSIC_DIR="/home/${PI_USER}/Music"

GREEN="\033[1;32m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RED="\033[1;31m"
RESET="\033[0m"

echo -e "${CYAN}==========================================================${RESET}"
echo -e "${CYAN} 🎵 树莓派 5 SD 卡 HiFi 音乐库一键同步${RESET}"
echo -e "${CYAN} 本地来源: ${LOCAL_MUSIC_DIR}${RESET}"
echo -e "${CYAN} 目标位置: ${PI_USER}@${PI_HOST}:${REMOTE_MUSIC_DIR}${RESET}"
echo -e "${CYAN}==========================================================${RESET}"

if [ ! -d "${LOCAL_MUSIC_DIR}" ]; then
    echo -e "${RED}错误：本地音乐目录不存在: ${LOCAL_MUSIC_DIR}${RESET}"
    exit 1
fi

# 确保远程目录存在
ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new "${PI_USER}@${PI_HOST}" "mkdir -p ${REMOTE_MUSIC_DIR}"

echo -e "${YELLOW}>> 正在高速增量传输无损音频文件到树莓派 SD 卡...${RESET}"
rsync -avP "${LOCAL_MUSIC_DIR}/" "${PI_USER}@${PI_HOST}:${REMOTE_MUSIC_DIR}/"

echo ""
echo -e "${GREEN}==========================================================${RESET}"
echo -e "${GREEN} 🎉 音乐文件传输完成！${RESET}"
echo -e "${CYAN} 提示：打开树莓派屏幕上的播放器，点击左侧【扫描音乐】${RESET}"
echo -e "${CYAN} 系统将自动解析 FLAC / MP3 / WAV / DSF 母带元数据并加入曲库。${RESET}"
echo -e "${GREEN}==========================================================${RESET}"
