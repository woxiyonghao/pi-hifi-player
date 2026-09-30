#!/usr/bin/env bash
# ==============================================================================
# PiHifiPlayer macOS 一键构建、打桩与 DMG 独立安装包制作脚本
# 制作完全自包含、无任何外部依赖、开箱即用的 macOS 安装镜像 (.dmg)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# 检查运行平台
if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "❌ 错误: 此脚本仅适用于 macOS 操作系统。"
    exit 1
fi

BUILD_DIR="${PROJECT_ROOT}/build"
OUTPUT_DIR="${PROJECT_ROOT}/dist"
APP_NAME="PiHifiPlayer.app"
APP_BUNDLE="${BUILD_DIR}/${APP_NAME}"
DMG_NAME="PiHifiPlayer-1.0.0-macOS.dmg"
DMG_OUT="${OUTPUT_DIR}/${DMG_NAME}"
ICON_SRC="${PROJECT_ROOT}/assets/icons/music_item_icon.jpg"

echo "=========================================================="
echo " 🚀 开始打包 PiHifiPlayer macOS DMG 安装镜像"
echo " 项目目录: ${PROJECT_ROOT}"
echo " 输出目录: ${OUTPUT_DIR}"
echo "=========================================================="

# 1. 确保可执行程序已编译且最新
echo "=== [1/6] 检查与编译项目 (Release 模式) ==="
if [[ ! -f "${BUILD_DIR}/PiHifiPlayer" ]]; then
    echo ">> 未检测到编译产物，开始使用 CMake 编译..."
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}" -j"$(sysctl -n hw.ncpu)"
else
    echo ">> 增量构建确保代码最新..."
    cmake --build "${BUILD_DIR}" -j"$(sysctl -n hw.ncpu)"
fi

# 2. 定位 SDL2 动态库
echo "=== [2/6] 探测 SDL2 依赖库 ==="
SDL2_DYLIB=""
# 检查二进制实际链接路径
LINKED_SDL2="$(otool -L "${BUILD_DIR}/PiHifiPlayer" | grep -E "libSDL2.*\.dylib" | awk '{print $1}' || true)"
if [[ -n "${LINKED_SDL2}" && -f "${LINKED_SDL2}" ]]; then
    SDL2_DYLIB="${LINKED_SDL2}"
elif command -v brew &>/dev/null; then
    BREW_PREFIX="$(brew --prefix)"
    for candidate in \
        "${BREW_PREFIX}/opt/sdl2-compat/lib/libSDL2-2.0.0.dylib" \
        "${BREW_PREFIX}/opt/sdl2/lib/libSDL2-2.0.0.dylib" \
        "${BREW_PREFIX}/lib/libSDL2-2.0.0.dylib"; do
        if [[ -f "${candidate}" ]]; then
            SDL2_DYLIB="${candidate}"
            break
        fi
    done
fi

if [[ -z "${SDL2_DYLIB}" || ! -f "${SDL2_DYLIB}" ]]; then
    echo "❌ 错误: 未能在系统中定位到 libSDL2-2.0.0.dylib"
    exit 1
fi
echo ">> 定位到 SDL2: ${SDL2_DYLIB}"

# 3. 制作 macOS 应用图标 (.icns)
echo "=== [3/6] 生成应用高清图标 ==="
ICNS_FILE="${BUILD_DIR}/AppIcon.icns"
if [[ -f "${ICON_SRC}" ]]; then
    ICONSET_TMP="$(mktemp -d)/PiHifiPlayer.iconset"
    mkdir -p "${ICONSET_TMP}"

    python3 - <<PYEOF
import os
from PIL import Image

src = "${ICON_SRC}"
dst = "${ICONSET_TMP}"
img = Image.open(src)
w, h = img.size
dim = min(w, h)
left = (w - dim) // 2
top = (h - dim) // 2
square = img.crop((left, top, left + dim, top + dim))

sizes = [
    (16, "icon_16x16.png"),
    (32, "icon_16x16@2x.png"),
    (32, "icon_32x32.png"),
    (64, "icon_32x32@2x.png"),
    (128, "icon_128x128.png"),
    (256, "icon_128x128@2x.png"),
    (256, "icon_256x256.png"),
    (512, "icon_256x256@2x.png"),
    (512, "icon_512x512.png"),
    (1024, "icon_512x512@2x.png"),
]
for sz, name in sizes:
    resized = square.resize((sz, sz), Image.Resampling.LANCZOS)
    resized.save(os.path.join(dst, name))
PYEOF

    iconutil -c icns "${ICONSET_TMP}" -o "${ICNS_FILE}"
    rm -rf "${ICONSET_TMP}"
    echo ">> 成功生成 AppIcon.icns"
else
    echo "⚠️ 未找到图标源文件，跳过图标生成"
fi

# 4. 组装 PiHifiPlayer.app Bundle 目录结构
echo "=== [4/6] 组装 .app Bundle 并嵌入依赖库 ==="
rm -rf "${APP_BUNDLE}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Frameworks"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

# 拷贝主程序
cp "${BUILD_DIR}/PiHifiPlayer" "${APP_BUNDLE}/Contents/MacOS/PiHifiPlayer"

# 拷贝图标
if [[ -f "${ICNS_FILE}" ]]; then
    cp "${ICNS_FILE}" "${APP_BUNDLE}/Contents/Resources/AppIcon.icns"
fi

# 写入 Info.plist
cat << 'EOF' > "${APP_BUNDLE}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>PiHifiPlayer</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>com.hifi.player</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>PiHifiPlayer</string>
    <key>CFBundleDisplayName</key>
    <string>纯音数播</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2026 Pi Hifi Player. All rights reserved.</string>
</dict>
</plist>
EOF

# 拷贝 SDL2 并重写 rpath 打桩 (使其完全脱离 Homebrew，自包含运行)
cp "${SDL2_DYLIB}" "${APP_BUNDLE}/Contents/Frameworks/libSDL2-2.0.0.dylib"
chmod 755 "${APP_BUNDLE}/Contents/Frameworks/libSDL2-2.0.0.dylib"

install_name_tool -id @rpath/libSDL2-2.0.0.dylib "${APP_BUNDLE}/Contents/Frameworks/libSDL2-2.0.0.dylib"

if [[ -n "${LINKED_SDL2}" ]]; then
    install_name_tool -change "${LINKED_SDL2}" @rpath/libSDL2-2.0.0.dylib "${APP_BUNDLE}/Contents/MacOS/PiHifiPlayer"
fi
install_name_tool -add_rpath @executable_path/../Frameworks "${APP_BUNDLE}/Contents/MacOS/PiHifiPlayer" 2>/dev/null || true

# 对所有二进制进行 ad-hoc 签名
echo "=== [5/6] 执行 Code Signing 签名 ==="
codesign -f -s - "${APP_BUNDLE}/Contents/Frameworks/libSDL2-2.0.0.dylib"
codesign -f -s - "${APP_BUNDLE}"
codesign -vvv --deep --strict "${APP_BUNDLE}"
echo ">> App Bundle 签名校验通过！"

# 5. 制作最终 DMG 磁盘映像
echo "=== [6/6] 制作 DMG 磁盘映像 ==="
mkdir -p "${OUTPUT_DIR}"
STAGING_DIR="$(mktemp -d)/dmg_staging"
mkdir -p "${STAGING_DIR}"

# 复制 App 与创建 Applications 软链接 (支持拖拽安装)
cp -R "${APP_BUNDLE}" "${STAGING_DIR}/"
ln -s /Applications "${STAGING_DIR}/Applications"

rm -f "${DMG_OUT}"
hdiutil create \
    -volname "PiHifiPlayer" \
    -srcfolder "${STAGING_DIR}" \
    -ov \
    -format UDZO \
    -imagekey zlib-level=9 \
    "${DMG_OUT}"

rm -rf "${STAGING_DIR}"

echo ""
echo "=========================================================="
echo " 🎉 PiHifiPlayer macOS DMG 安装包打包成功！"
echo " 镜像路径: ${DMG_OUT}"
echo " 镜像大小: $(du -h "${DMG_OUT}" | awk '{print $1}')"
echo " SHA-256:  $(shasum -a 256 "${DMG_OUT}" | awk '{print $1}')"
echo "=========================================================="
