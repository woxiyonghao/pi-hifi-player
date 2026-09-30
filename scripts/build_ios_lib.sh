#!/usr/bin/env bash
set -e

# ==============================================================================
# PiHiEndMusic / PiHifiPlayer iOS & iPadOS 静态库 (.a / .xcframework) 一键打包脚本
# 支持: 
#   1. iOS 真机 (arm64)
#   2. iOS 模拟器 (arm64 + x86_64 通用架构，适配 Apple Silicon 与 Intel Mac)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== [1/5] 检测 Apple SDK 环境 ==="
IPHONEOS_SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
SIMULATOR_SDK="$(xcrun --sdk iphonesimulator --show-sdk-path)"
CLANG="$(xcrun --find clang++)"
LIBTOOL="$(xcrun --find libtool)"
LIPO="$(xcrun --find lipo)"

echo "Device SDK:    ${IPHONEOS_SDK}"
echo "Simulator SDK: ${SIMULATOR_SDK}"

BUILD_DIR="${ROOT_DIR}/build-ios"
OBJ_DEVICE_DIR="${BUILD_DIR}/obj-device"
OBJ_SIM_ARM64_DIR="${BUILD_DIR}/obj-sim-arm64"
OBJ_SIM_X86_DIR="${BUILD_DIR}/obj-sim-x86_64"
OUT_DEVICE_DIR="${BUILD_DIR}/device"
OUT_SIM_DIR="${BUILD_DIR}/simulator"
OUT_XCFRAMEWORK="${BUILD_DIR}/PiHifiCore.xcframework"

rm -rf "${BUILD_DIR}"
mkdir -p "${OBJ_DEVICE_DIR}" "${OBJ_SIM_ARM64_DIR}" "${OBJ_SIM_X86_DIR}" "${OUT_DEVICE_DIR}" "${OUT_SIM_DIR}"

INCLUDE_FLAGS=(
    -I "${ROOT_DIR}/include"
    -I "${ROOT_DIR}/include/public"
    -I "${ROOT_DIR}/include/types"
    -I "${ROOT_DIR}/include/views"
    -I "${ROOT_DIR}/include/tools"
    -I "${ROOT_DIR}/include/themes"
    -I "${ROOT_DIR}/include/widgets"
    -I "${ROOT_DIR}/audio_engine/include"
    -I "${ROOT_DIR}/audio_engine/third_party"
    -I "${ROOT_DIR}/third_party/imgui"
)

SOURCES=(
    "${ROOT_DIR}/audio_engine/src/AudioEngine.cpp"
    "${ROOT_DIR}/audio_engine/src/DsfParser.cpp"
    "${ROOT_DIR}/audio_engine/src/decoders/FlacDecoder.cpp"
    "${ROOT_DIR}/audio_engine/src/decoders/WavDecoder.cpp"
    "${ROOT_DIR}/audio_engine/src/decoders/Mp3Decoder.cpp"
    "${ROOT_DIR}/audio_engine/src/sinks/AudioQueueSink.cpp"
    "${ROOT_DIR}/src/tools/MusicDatabase.cpp"
    "${ROOT_DIR}/src/tools/MusicScanManager.cpp"
    "${ROOT_DIR}/src/tools/PlayerAdmin.cpp"
    "${ROOT_DIR}/src/public/Font.cpp"
    "${ROOT_DIR}/src/themes/ThemeManager.cpp"
    "${ROOT_DIR}/src/themes/AccuphaseMeterRenderer.cpp"
    "${ROOT_DIR}/src/themes/VUMeterRenderer.cpp"
    "${ROOT_DIR}/src/themes/TapeReelRenderer.cpp"
    "${ROOT_DIR}/src/themes/SiriOrbRenderer.cpp"
    "${ROOT_DIR}/src/themes/SiriWaveformRenderer.cpp"
    "${ROOT_DIR}/src/themes/NeonWaveformRenderer.cpp"
    "${ROOT_DIR}/src/themes/CyberGridRenderer.cpp"
    "${ROOT_DIR}/src/themes/AudioBubblesRenderer.cpp"
    "${ROOT_DIR}/src/themes/GlassClockRenderer.cpp"
    "${ROOT_DIR}/src/views/MainStageView.cpp"
    "${ROOT_DIR}/src/views/SidebarView.cpp"
    "${ROOT_DIR}/src/views/BottomBarView.cpp"
    "${ROOT_DIR}/src/views/AllMusicPlaylistView.cpp"
    "${ROOT_DIR}/src/views/CustomPlaylistView.cpp"
    "${ROOT_DIR}/src/views/DACSettingView.cpp"
    "${ROOT_DIR}/src/views/EQConfigView.cpp"
    "${ROOT_DIR}/src/views/MagicTuningView.cpp"
    "${ROOT_DIR}/src/views/SystemSettingsView.cpp"
    "${ROOT_DIR}/src/views/ThemeSettingView.cpp"
    "${ROOT_DIR}/src/views/ScanMusicWidget.cpp"
    "${ROOT_DIR}/src/widgets/MusicItem.cpp"
    "${ROOT_DIR}/src/widgets/VolumeWidget.cpp"
    "${ROOT_DIR}/src/widgets/PlayPauseWidget.cpp"
    "${ROOT_DIR}/src/widgets/NextTrackWidget.cpp"
    "${ROOT_DIR}/src/widgets/PrevTrackWidget.cpp"
    "${ROOT_DIR}/src/widgets/PlayModeWidget.cpp"
    "${ROOT_DIR}/src/widgets/SidebarPlaylistWidget.cpp"
    "${ROOT_DIR}/src/widgets/SidebarFeatureWidget.cpp"
    "${ROOT_DIR}/src/widgets/SidebarDacWidget.cpp"
    "${ROOT_DIR}/src/widgets/LEDSpectrumWidget.cpp"
    "${ROOT_DIR}/src/widgets/GlassCardRenderer.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui_draw.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui_tables.cpp"
    "${ROOT_DIR}/third_party/imgui/imgui_widgets.cpp"
)

echo "=== [2/5] 编译 iOS Device 切片 (arm64-apple-ios17.0) ==="
DEVICE_OBJS=()
for src in "${SOURCES[@]}"; do
    bname="$(basename "${src}" .cpp)"
    obj="${OBJ_DEVICE_DIR}/${bname}.o"
    "${CLANG}" -target arm64-apple-ios17.0 \
        -isysroot "${IPHONEOS_SDK}" \
        -std=c++20 -O3 -fPIC -DNDEBUG \
        "${INCLUDE_FLAGS[@]}" \
        -c "${src}" -o "${obj}" &
    DEVICE_OBJS+=("${obj}")
done
wait
echo "  [Device] 打包静态库: ${OUT_DEVICE_DIR}/libPiHifiCore.a"
"${LIBTOOL}" -static -o "${OUT_DEVICE_DIR}/libPiHifiCore.a" "${DEVICE_OBJS[@]}"

echo "=== [3/5] 并行编译 iOS Simulator arm64 与 x86_64 切片 ==="
SIM_ARM64_OBJS=()
SIM_X86_OBJS=()
for src in "${SOURCES[@]}"; do
    bname="$(basename "${src}" .cpp)"
    obj_arm="${OBJ_SIM_ARM64_DIR}/${bname}.o"
    obj_x86="${OBJ_SIM_X86_DIR}/${bname}.o"
    
    "${CLANG}" -target arm64-apple-ios17.0-simulator \
        -isysroot "${SIMULATOR_SDK}" \
        -std=c++20 -O3 -fPIC -DNDEBUG \
        "${INCLUDE_FLAGS[@]}" \
        -c "${src}" -o "${obj_arm}" &

    "${CLANG}" -target x86_64-apple-ios17.0-simulator \
        -isysroot "${SIMULATOR_SDK}" \
        -std=c++20 -O3 -fPIC -DNDEBUG \
        "${INCLUDE_FLAGS[@]}" \
        -c "${src}" -o "${obj_x86}" &

    SIM_ARM64_OBJS+=("${obj_arm}")
    SIM_X86_OBJS+=("${obj_x86}")
done
wait

echo "  [Simulator] 打包 arm64 与 x86_64 模拟器切片..."
"${LIBTOOL}" -static -o "${BUILD_DIR}/libPiHifiCore_sim_arm64.a" "${SIM_ARM64_OBJS[@]}"
"${LIBTOOL}" -static -o "${BUILD_DIR}/libPiHifiCore_sim_x86_64.a" "${SIM_X86_OBJS[@]}"

echo "  [Simulator] 使用 lipo 合并为双架构通用模拟器静态库: ${OUT_SIM_DIR}/libPiHifiCore.a"
"${LIPO}" -create "${BUILD_DIR}/libPiHifiCore_sim_arm64.a" "${BUILD_DIR}/libPiHifiCore_sim_x86_64.a" \
    -output "${OUT_SIM_DIR}/libPiHifiCore.a"

echo "=== [4/5] 组装发布包 ==="
HEADERS_DIR="${BUILD_DIR}/include"
mkdir -p "${HEADERS_DIR}"
cp -R "${ROOT_DIR}/include/" "${HEADERS_DIR}/"
cp -R "${ROOT_DIR}/audio_engine/include/" "${HEADERS_DIR}/"
cp -R "${ROOT_DIR}/third_party/imgui/"*.h "${HEADERS_DIR}/"

xcodebuild -create-xcframework \
    -library "${OUT_DEVICE_DIR}/libPiHifiCore.a" -headers "${HEADERS_DIR}" \
    -library "${OUT_SIM_DIR}/libPiHifiCore.a" -headers "${HEADERS_DIR}" \
    -output "${OUT_XCFRAMEWORK}"

cp "${OUT_DEVICE_DIR}/libPiHifiCore.a" "${BUILD_DIR}/libPiHifiCore.a"

# 同步到 PiHiEndMusic Xcode 工程
DEST_PROJECT_CXX="/Users/mk10/Desktop/PiHiEndMusic/PiHiEndMusic/cxx"
if [ -d "${DEST_PROJECT_CXX}" ]; then
    echo "=== [5/5] 自动同步至 Xcode 项目 (${DEST_PROJECT_CXX}) ==="
    mkdir -p "${DEST_PROJECT_CXX}/lib/iphoneos" "${DEST_PROJECT_CXX}/lib/iphonesimulator"
    cp "${OUT_DEVICE_DIR}/libPiHifiCore.a" "${DEST_PROJECT_CXX}/lib/iphoneos/"
    cp "${OUT_SIM_DIR}/libPiHifiCore.a" "${DEST_PROJECT_CXX}/lib/iphonesimulator/"
fi

echo "=========================================================="
echo ">> 打包完成！"
echo " 1. [静态库 .a (iOS 真机/iPad)]:      ${BUILD_DIR}/libPiHifiCore.a"
echo " 2. [静态库 .a (iOS 模拟器 Universal)]: ${OUT_SIM_DIR}/libPiHifiCore.a"
echo " 3. [XCFramework (真机+模拟器自适应)]:  ${OUT_XCFRAMEWORK}"
echo " 4. [头文件目录]:                     ${HEADERS_DIR}"
echo "=========================================================="
