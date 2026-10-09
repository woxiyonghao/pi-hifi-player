#!/usr/bin/env bash
set -e

# ==============================================================================
# PiHiEndMusic / PiHifiPlayer iOS 一键构建、模拟器启动与真机调试脚本
# 支持在 CLion 中直接运行，完全免用 Xcode GUI
# 支持目标:
#   - iphone : iPhone 模拟器
#   - ipad   : iPad 模拟器
#   - device : 真实连接的 iPhone / iPad 物理真机
#   - build  : 仅构建 iOS 产物
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
IOS_DIR="${ROOT_DIR}/ios"
BUILD_DIR="${IOS_DIR}/build"
PROJECT_PATH="${IOS_DIR}/PiHiEndMusic.xcodeproj"
BUNDLE_ID="cpnice.PiHiEndMusic"

ACTION="${1:-iphone}"

echo "=========================================================="
echo ">> [CLion iOS Launcher] 目标动作: ${ACTION}"
echo "=========================================================="

# 1. 确保核心静态库已打包至 ios/lib
if [ ! -f "${IOS_DIR}/lib/iphonesimulator/libPiHifiCore.a" ] || [ ! -f "${IOS_DIR}/lib/iphoneos/libPiHifiCore.a" ] || [ "${ACTION}" = "rebuild" ]; then
    echo "=== [1/3] 核心静态库缺失，正在触发 build_ios_lib.sh ==="
    "${ROOT_DIR}/scripts/build_ios_lib.sh"
else
    echo "=== [1/3] 核心静态库就绪 ==="
fi

# ==============================================================================
# 真机运行逻辑 (Device)
# ==============================================================================
if [ "${ACTION}" = "device" ]; then
    echo "=== [2/3] 扫描已连接的物理真机设备 ==="
    PHYS_LINE=$(xcrun devicectl list devices | grep -i "physical" | grep -i "connected" | head -n 1 || true)
    if [ -z "${PHYS_LINE}" ]; then
        echo "❌ [Error] 未检测到已连接的真机设备！"
        echo "请检查："
        echo "  1. iPhone 是否已通过数据线连接到 Mac；"
        echo "  2. iPhone 屏幕上是否已点击「信任此电脑」；"
        echo "  3. 系统设置中是否已启用「开发者模式」。"
        exit 1
    fi

    TARGET_UDID=$(echo "${PHYS_LINE}" | awk '{for(i=1;i<=NF;i++) if($i=="(UDID)") print $(i-1)}')
    DEV_NAME=$(echo "${PHYS_LINE}" | awk '{print $1}')
    echo ">> 命中物理设备: ${DEV_NAME} (UDID: ${TARGET_UDID})"

    echo "=== [2/3] 正在针对真机编译并签名 App ==="
    xcodebuild -project "${PROJECT_PATH}" \
        -scheme PiHiEndMusic \
        -destination "id=${TARGET_UDID}" \
        -derivedDataPath "${BUILD_DIR}" \
        build

    DEVICE_APP_PATH="${BUILD_DIR}/Build/Products/Debug-iphoneos/PiHiEndMusic.app"

    echo "=== [3/3] 安装 App 到真机 (${DEV_NAME}) ==="
    xcrun devicectl device install app --device "${TARGET_UDID}" "${DEVICE_APP_PATH}"
    echo ">> [OK] 应用安装成功！正在拉起应用..."

    xcrun devicectl device process launch --device "${TARGET_UDID}" "${BUNDLE_ID}" || {
        echo "----------------------------------------------------------"
        echo "⚠️ 注意：如果手机当前处于锁屏状态，iOS 安全限制会阻止后台拉起。"
        echo "请点亮并解锁 iPhone 屏幕，点击桌面上的「PiHiEndMusic」即可！"
        echo "----------------------------------------------------------"
    }

    echo "=========================================================="
    echo ">> [SUCCESS] iPhone 真机部署完成！"
    echo "=========================================================="
    exit 0
fi

# ==============================================================================
# 通用构建逻辑 (Build Only)
# ==============================================================================
if [ "${ACTION}" = "build" ]; then
    echo "=== [2/2] 编译 iOS App (Universal Simulator) ==="
    xcodebuild -project "${PROJECT_PATH}" \
        -scheme PiHiEndMusic \
        -destination 'generic/platform=iOS Simulator' \
        -derivedDataPath "${BUILD_DIR}" \
        build

    APP_PATH="${BUILD_DIR}/Build/Products/Debug-iphonesimulator/PiHiEndMusic.app"
    echo "=========================================================="
    echo ">> [OK] iOS App 构建成功！产物位于:"
    echo "   ${APP_PATH}"
    echo "=========================================================="
    exit 0
fi

# ==============================================================================
# 模拟器运行逻辑 (iPhone / iPad)
# ==============================================================================
echo "=== [2/3] 编译 iOS App (Universal Simulator) ==="
xcodebuild -project "${PROJECT_PATH}" \
    -scheme PiHiEndMusic \
    -destination 'generic/platform=iOS Simulator' \
    -derivedDataPath "${BUILD_DIR}" \
    build

APP_PATH="${BUILD_DIR}/Build/Products/Debug-iphonesimulator/PiHiEndMusic.app"

# 决定目标模拟器设备
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

# 安装并拉起 App
echo ">> 正在安装并拉起 App: ${BUNDLE_ID}..."
xcrun simctl install "${BOOTED_UDID}" "${APP_PATH}"
xcrun simctl launch "${BOOTED_UDID}" "${BUNDLE_ID}"

echo "=========================================================="
echo ">> [SUCCESS] PiHiEndMusic 已经在模拟器中成功启动！"
echo "=========================================================="
