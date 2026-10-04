#!/usr/bin/env bash
# ==============================================================================
# PiHiFi Player 独立固件检查更新与原生并发重编译脚本
#
# 用法：
#   ./scripts/update_and_rebuild.sh              # 检查更新并自动编译、重启服务
#   ./scripts/update_and_rebuild.sh --check-only # 仅检查是否有更新，不执行拉取与编译
#   ./scripts/update_and_rebuild.sh --branch main# 指定拉取分支 (默认: dev/lyh)
# ==============================================================================
set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}"

DEFAULT_BRANCH="dev/lyh"
TARGET_BRANCH="${DEFAULT_BRANCH}"
CHECK_ONLY=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--check-only)
            CHECK_ONLY=1
            shift
            ;;
        -b|--branch)
            TARGET_BRANCH="$2"
            shift 2
            ;;
        --help)
            echo "用法: $0 [选项]"
            echo "  -c, --check-only    仅检测远端是否有更新，不修改代码与编译"
            echo "  -b, --branch <分支> 指定检查的目标分支 (默认: ${DEFAULT_BRANCH})"
            exit 0
            ;;
        *)
            shift
            ;;
    esac
done

GREEN="\033[1;32m"
YELLOW="\033[1;33m"
CYAN="\033[1;36m"
RED="\033[1;31m"
RESET="\033[0m"

echo -e "${CYAN}==========================================================${RESET}"
echo -e "${CYAN} 📦 PiHiFi Player 在线固件检查更新与重编译系统${RESET}"
echo -e "${CYAN} 工程目录: ${PROJECT_DIR}${RESET}"
echo -e "${CYAN} 目标分支: ${TARGET_BRANCH}${RESET}"
echo -e "${CYAN}==========================================================${RESET}"

# 1. 确保 Git 仓库正常
if [ ! -d ".git" ]; then
    echo -e "${RED}错误：未检测到 .git 目录，无法进行 Git 在线更新。${RESET}"
    exit 1
fi

LOCAL_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
LOCAL_DATE=$(git log -1 --format=%cd --date=short 2>/dev/null || echo "unknown")
echo -e "${CYAN}>> 本地版本: ${LOCAL_HASH} (${LOCAL_DATE})${RESET}"

# 2. 检查网络与 Fetch 远端元数据 (若 SSH 未配置则自动使用公开免密 HTTPS)
echo -ne "${YELLOW}>> 正在连接远端仓库 (git fetch origin ${TARGET_BRANCH})... ${RESET}"
if ! git fetch origin "${TARGET_BRANCH}" --quiet 2>/dev/null; then
    echo -e "${YELLOW}[切换至公开免密 HTTPS 通道重试]... ${RESET}"
    git remote set-url origin https://github.com/woxiyonghao/pi-hifi-player.git 2>/dev/null || true
    if ! git fetch origin "${TARGET_BRANCH}" --quiet 2>/dev/null; then
        echo -e "${RED}[连接失败]${RESET}"
        echo -e "${RED}无法连接到远程 Git 仓库，请检查网络连接。${RESET}"
        exit 1
    fi
fi
echo -e "${GREEN}[OK]${RESET}"

REMOTE_HASH=$(git rev-parse --short "origin/${TARGET_BRANCH}" 2>/dev/null || echo "")
if [ -z "${REMOTE_HASH}" ]; then
    echo -e "${RED}无法解析远程分支 origin/${TARGET_BRANCH}${RESET}"
    exit 1
fi

BEHIND_COUNT=$(git rev-list --count "HEAD..origin/${TARGET_BRANCH}" 2>/dev/null || echo "0")

if [ "${BEHIND_COUNT}" -eq 0 ]; then
    echo -e "${GREEN}>> 🎉 当前已是最新版本 (${LOCAL_HASH})，无需更新！${RESET}"
    exit 0
fi

echo -e "${YELLOW}>> 🔥 发现远端有 ${BEHIND_COUNT} 个新提交 (最新: ${REMOTE_HASH})！${RESET}"
echo -e "${CYAN}>> 变更概览:${RESET}"
git log -n 5 --format="   * %h %s (%cr)" "HEAD..origin/${TARGET_BRANCH}"

if [ "${CHECK_ONLY}" -eq 1 ]; then
    echo -e "${CYAN}>> [检查模式] 退出，未做任何更改。${RESET}"
    exit 0
fi

# 3. 暂存本地改动并拉取代码
echo -e "${YELLOW}>> [1/3] 正在同步最新源码 (git pull --rebase)...${RESET}"
git stash --quiet 2>/dev/null || true
git pull --rebase origin "${TARGET_BRANCH}"
echo -e "${GREEN}>> ✅ 源码已成功同步到最新版本！${RESET}"

# 4. 原生 CMake 多核并发编译
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
BUILD_DIR="build"

echo -e "${YELLOW}>> [2/3] 正在启动 CMake 多核并发编译 (并行线程: ${CORES})...${RESET}"
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j"${CORES}"

TARGET_BIN="${PROJECT_DIR}/${BUILD_DIR}/PiHifiPlayer"
if [ ! -f "${TARGET_BIN}" ]; then
    echo -e "${RED}错误：编译后未生成目标程序 ${TARGET_BIN}${RESET}"
    exit 1
fi

echo -e "${GREEN}>> ✅ 编译完成！目标产物: ${TARGET_BIN}${RESET}"

# 5. 重启策略
echo -e "${YELLOW}>> [3/3] 检查服务运行状态并应用更新...${RESET}"
if command -v systemctl &>/dev/null && systemctl is-active pi-hifi.service &>/dev/null; then
    echo -e "${CYAN}>> 检测到正在运行的 pi-hifi.service，正在自动重启服务...${RESET}"
    sudo systemctl restart pi-hifi.service || systemctl --user restart pi-hifi.service || true
    echo -e "${GREEN}>> ✅ pi-hifi.service 已成功热重启并运行全新固件！${RESET}"
else
    echo -e "${GREEN}>> ✅ 固件重编译完毕！您可随时启动播放器测试：${RESET}"
    echo "   ${TARGET_BIN}"
fi

echo -e "${GREEN}==========================================================${RESET}"
echo -e "${GREEN} 🚀 全部更新流程圆满完成！当前版本: ${REMOTE_HASH}${RESET}"
echo -e "${GREEN}==========================================================${RESET}"
