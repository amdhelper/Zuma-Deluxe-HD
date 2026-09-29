#!/usr/bin/env bash
# Windows(x64) 交叉打包：mingw-w64 + SDL2 系列
#
# 用法： scripts/package_windows.sh            # 生成 dist/zuma-deluxe-hd-<VERSION>-win64.zip
#
# 依赖（Ubuntu）： sudo apt install mingw-w64 cmake zip
#   SDL2/SDL2_image/SDL2_ttf 的 mingw 版需要预先放到 /usr/x86_64-w64-mingw32
#   （CI 里用 MSYS2 装包更省事，见 .github/workflows/release.yml 的 windows job；
#     本脚本是"本机交叉编译"的等价实现）
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VERSION="${VERSION:-$(sed -n 's/.*VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)}"
VERSION="${VERSION:-0.1.0}"

BUILD_DIR="$ROOT/build-win64"
DIST="$ROOT/dist"
OUT="$DIST/zuma-deluxe-hd-${VERSION}-win64"

echo "== 打包 Zuma Deluxe HD ${VERSION} (Windows x64) =="

command -v x86_64-w64-mingw32-gcc >/dev/null || {
    echo "❌ 缺 mingw-w64：sudo apt install mingw-w64"; exit 1; }

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" -j"$(nproc)"

rm -rf "$OUT"
mkdir -p "$OUT"

cp "$BUILD_DIR/bin/ZumaHD.exe" "$OUT/"a

# 资源（游戏用相对路径读 levels/ images/ sounds/ → content 目录要平铺在同一层）
cp -r content/* "$OUT/"

# BASS 音频（Windows 版用仓库自带的 dll）
[ -f lib/bass/x64/bass.dll ]      && cp lib/bass/x64/bass.dll      "$OUT/"
[ -f lib/bass-fx/libs/x64/bass_fx.dll ] && cp lib/bass-fx/libs/x64/bass_fx.dll "$OUT/"

# SDL2 运行时 dll（从 mingw 环境拷）
for dll in SDL2.dll SDL2_image.dll SDL2_ttf.dll libpng16-16.dll zlib1.dll \
           libjpeg-8.dll libwebp-7.dll libtiff-5.dll libexpat-1.dll; do
    for cand in "/usr/x86_64-w64-mingw32/bin/$dll" \
                "/usr/x86_64-w64-mingw32/sys-root/mingw/bin/$dll" \
                "/mingw64/bin/$dll"; do
        [ -f "$cand" ] && cp "$cand" "$OUT/" && break
    done
done

cat > "$OUT/玩祖玛.bat" <<'BAT'
@echo off
cd /d "%~dp0"
start "" ZumaHD.exe
BAT

cd "$DIST"
zip -qr "zuma-deluxe-hd-${VERSION}-win64.zip" "$(basename "$OUT")"
(cd "$DIST" && sha256sum "$(basename "$OUT").zip" >> sha256sums.txt)

ls -la "$DIST/zuma-deluxe-hd-${VERSION}-win64.zip"
echo "== 完成：$DIST/zuma-deluxe-hd-${VERSION}-win64.zip =="