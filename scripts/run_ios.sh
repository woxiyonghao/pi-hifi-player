#!/usr/bin/env bash
set -e

# ==============================================================================
# PiHiEndMusic / PiHifiPlayer iOS 一键构建、模拟器启动与调试脚本
# 支持在 CLion 中直接作为 CMake Custom Target 运行，完全免用 Xcode GUI
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
IOS_DIR="${ROOT_DIR}/ios"
BUILD_DIR="${IOS_DIR}/build"
PROJECT_PATH="${IOS_DIR}/PiHiEndMusic.xcodeproj"
BUNDLE_ID="cpnice.PiHiEndMusic"

ACTION="${1:-iphone}" # 可选: iphone, ipad, build

echo "=========================================================="
echo ">> [CLion iOS Launcher] 目标动作: ${ACTION}"
echo "=========================================================="

# 1. 确保核心静态库已打包至 ios/lib
if [ ! -f "${IOS_DIR}/lib/iphonesimulator/libPiHifiCore.a" ] || [ "${ACTION}" = "rebuild" ]; then
    echo "=== [1/3] 核心静态库缺失，正在触发 build_ios_lib.sh ==="
    "${ROOT_DIR}/scripts/build_ios_lib.sh"
else
    echo "=== [1/3] 核心静态库就绪 (${IOS_DIR}/lib/iphonesimulator/libPiHifiCore.a) ==="
fi

# 2. 编译 iOS App
echo "=== [2/3] 编译 iOS App (Universal Simulator) ==="
xcodebuild -project "${PROJECT_PATH}" \
    -scheme PiHiEndMusic \
    -destination 'generic/platform=iOS Simulator' \
    -derivedDataPath "${BUILD_DIR}" \
    build

APP_PATH="${BUILD_DIR}/Build/Products/Debug-iphonesimulator/PiHiEndMusic.app"

if [ "${ACTION}" = "build" ]; then
    echo "=========================================================="
    echo ">> [OK] iOS App 构建成功！产物位于:"
    echo "   ${APP_PATH}"
    echo "=========================================================="
    exit 0
fi

# 3. 决定目标模拟器设备
# 优先检查当前是否有已开机的模拟器
BOOTED_UDID=$(xcrun simctl list devices | grep "(Booted)" | head -n 1 | awk -F '[()]' '{print $2}' || true)

if [ -z "${BOOTED_UDID}" ]; then
    if [ "${ACTION}" = "ipad" ]; then
        TARGET_DEVICE_NAME="iPad Pro 11-inch (M5)"
    else
        TARGET_DEVICE_NAME="iPhone 17"
    fi

    # 查询目标 UDID
    TARGET_UDID=$(xcrun simctl list devices "iOS 27.0" | grep "${TARGET_DEVICE_NAME}" | head -n 1 | awk -F '[()]' '{print $2}' || true)
    if [ -z "${TARGET_UDID}" ]; then
        # 降级匹配任意可用的 iPhone/iPad
        if [ "${ACTION}" = "ipad" ]; then
            TARGET_UDID=$(xcrun simctl list devices available | grep -i "iPad" | head -n 1 | awk -F '[()]' '{print $2}' || true)
        else
            TARGET_UDID=$(xcrun simctl list devices available | grep -i "iPhone" | head -n 1 | awk -F '[()]' '{print $2}' || true)
        fi
    fi

    echo "=== [3/3] 正在启动模拟器: ${TARGET_DEVICE_NAME} (${TARGET_UDID}) ==="
    xcrun simctl boot "${TARGET_UDID}" 2>/dev/null || true
    BOOTED_UDID="${TARGET_UDID}"
else
    echo "=== [3/3] 检测到已开机模拟器 (UDID: ${BOOTED_UDID}) ==="
fi

# 尝试打开模拟器 GUI 前台窗口
open -a Simulator 2>/dev/null || open -b com.apple.CoreSimulator.SimulatorTrampoline 2>/dev/null || true

# 4. 安装并拉起 App
echo ">> 正在安装并拉起 App: ${BUNDLE_ID}..."
xcrun simctl install "${BOOTED_UDID}" "${APP_PATH}"
xcrun simctl launch "${BOOTED_UDID}" "${BUNDLE_ID}"

echo "=========================================================="
echo ">> [SUCCESS] PiHiEndMusic 已经在模拟器中成功启动！"
echo "=========================================================="
