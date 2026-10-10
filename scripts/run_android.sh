#!/usr/bin/env bash
set -e

# ==============================================================================
# PiHiEndMusic / PiHifiPlayer Android 一键构建、模拟器启动与真机调试脚本
# 支持在 CLion 中直接运行，完全免用 Android Studio GUI
# 支持目标:
#   - emulator : AVD 模拟器运行 (默认自动拉起 Pixel_9_Pro 或 Medium_Phone)
#   - device   : 真实连接的 USB Android 手机 / HiFi 数播硬件
#   - build    : 仅构建 Android APK 产物
#   - log      : 跟踪原生 C++ 与 App 运行日志
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ANDROID_DIR="${ROOT_DIR}/android"
APK_PATH="${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
PACKAGE_NAME="com.pihifi.player"
ACTIVITY_NAME=".MainActivity"

export ANDROID_HOME="${ANDROID_HOME:-/Users/mk10/Library/Android/sdk}"
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-/Users/mk10/Library/Android/sdk}"
ADB="$(which adb || echo "${ANDROID_HOME}/platform-tools/adb")"
EMULATOR="${ANDROID_HOME}/emulator/emulator"

ACTION="${1:-emulator}"

echo "=========================================================="
echo ">> [CLion Android Launcher] 目标动作: ${ACTION}"
echo "=========================================================="

# 确保 ADB 可执行
if [ ! -x "${ADB}" ]; then
    echo "❌ [Error] 未找到 adb 工具，请确认 Android SDK 安装路径！"
    exit 1
fi

# ==============================================================================
# 1. 编译 APK
# ==============================================================================
build_apk() {
    echo "=== [1/3] 正在编译 Android Debug APK (arm64-v8a + x86_64) ==="
    (cd "${ANDROID_DIR}" && ./gradlew assembleDebug)
    if [ ! -f "${APK_PATH}" ]; then
        echo "❌ [Error] APK 构建失败，未找到产物: ${APK_PATH}"
        exit 1
    fi
    echo ">> [OK] APK 编译成功: ${APK_PATH}"
}

# ==============================================================================
# 2. 纯构建模式 (Build Only)
# ==============================================================================
if [ "${ACTION}" = "build" ]; then
    build_apk
    echo "=========================================================="
    echo ">> [SUCCESS] Android APK 构建完成！"
    echo "   产物路径: ${APK_PATH}"
    echo "=========================================================="
    exit 0
fi

# ==============================================================================
# 3. 日志捕获模式 (Log Only)
# ==============================================================================
if [ "${ACTION}" = "log" ]; then
    echo "=== 正在过滤 Android 运行时日志 (PiHifiBridge / AudioEngine / AAudio) ==="
    "${ADB}" logcat -v time -s PiHifiBridge:V AudioEngine:V AAudioSink:V AndroidRuntime:E
    exit 0
fi

# ==============================================================================
# 4. 真机部署模式 (Device)
# ==============================================================================
if [ "${ACTION}" = "device" ]; then
    build_apk

    echo "=== [2/3] 检测已连接的 Android 物理真机 ==="
    DEVICE_ID=$("${ADB}" devices | grep -v "emulator" | grep "device$" | head -n 1 | awk '{print $1}' || true)
    if [ -z "${DEVICE_ID}" ]; then
        echo "❌ [Error] 未检测到已连接的 Android 物理真机！"
        echo "请检查："
        echo "  1. 手机是否已通过 USB 数据线连接并开启「开发者选项」；"
        echo "  2. 手机是否已启用「USB 调试」并在弹窗中勾选「允许此电脑调试」；"
        echo "  3. 执行 'adb devices' 确认设备状态为 'device'。"
        exit 1
    fi

    echo ">> 命中物理设备: ${DEVICE_ID}"
    echo "=== [3/3] 安装并启动 App ==="
    "${ADB}" -s "${DEVICE_ID}" install -r -t "${APK_PATH}"
    "${ADB}" -s "${DEVICE_ID}" shell am start -n "${PACKAGE_NAME}/${ACTIVITY_NAME}"
    echo "=========================================================="
    echo ">> [SUCCESS] 已成功部署并启动到 Android 真机！"
    echo "=========================================================="
    exit 0
fi

# ==============================================================================
# 5. AVD 模拟器运行模式 (Emulator)
# ==============================================================================
build_apk

echo "=== [2/3] 检测或启动 Android 模拟器 ==="
EMU_RUNNING=$("${ADB}" devices | grep "emulator-" | head -n 1 | awk '{print $1}' || true)

if [ -z "${EMU_RUNNING}" ]; then
    AVD_NAME=""
    if [ -x "${ANDROID_HOME}/cmdline-tools/latest/bin/avdmanager" ]; then
        AVD_NAME=$("${ANDROID_HOME}/cmdline-tools/latest/bin/avdmanager" list avd | grep "Name:" | head -n 1 | awk '{print $2}' || true)
    elif [ -x "${ANDROID_HOME}/tools/bin/avdmanager" ]; then
        AVD_NAME=$("${ANDROID_HOME}/tools/bin/avdmanager" list avd | grep "Name:" | head -n 1 | awk '{print $2}' || true)
    fi

    if [ -z "${AVD_NAME}" ]; then
        # 兜底检测本地 ~/.android/avd
        AVD_NAME=$(ls -1 "${HOME}/.android/avd/" 2>/dev/null | grep "\.avd$" | head -n 1 | sed 's/\.avd$//' || true)
    fi

    if [ -z "${AVD_NAME}" ]; then
        echo "❌ [Error] 未找到可用的 Android Virtual Device (AVD) 模拟器镜像！"
        echo "请在 Android Studio Device Manager 中创建一个 AVD（如 Pixel 9 Pro）。"
        exit 1
    fi

    echo ">> 正在后台启动 AVD 模拟器: ${AVD_NAME}..."
    "${EMULATOR}" -avd "${AVD_NAME}" -no-snapshot-load -no-boot-anim > /dev/null 2>&1 &
    
    echo ">> 等待模拟器启动中..."
    "${ADB}" wait-for-device
    while [ "$("${ADB}" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; do
        sleep 2
    done
    echo ">> [OK] 模拟器启动完成！"
    EMU_RUNNING=$("${ADB}" devices | grep "emulator-" | head -n 1 | awk '{print $1}')
else
    echo ">> 检测到已有模拟器正在运行: ${EMU_RUNNING}"
fi

echo "=== [3/3] 安装并拉起 PiHiEndMusic ==="
"${ADB}" -s "${EMU_RUNNING}" install -r -t "${APK_PATH}"
"${ADB}" -s "${EMU_RUNNING}" shell am start -n "${PACKAGE_NAME}/${ACTIVITY_NAME}"

echo "=========================================================="
echo ">> [SUCCESS] 模拟器运行成功！"
echo ">> 可执行 './scripts/run_android.sh log' 实时查看音频与渲染日志"
echo "=========================================================="
