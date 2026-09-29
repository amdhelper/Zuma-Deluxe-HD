#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Zuma Deluxe HD — Linux 一键出包（ROADMAP 4.6）
#   产物：dist/zuma-deluxe-hd-<版本>-linux-x86_64.tar.gz
#   内容：bin/ZumaHD + content/（素材）+ lib/（BASS 动态库）+ 启动脚本
#   自带 BASS 动态库并把 RPATH 设成 $ORIGIN/../lib，解包即跑（无需装 BASS）
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
VERSION="${VERSION:-$(sed -n 's/.*VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)}"
[ -z "$VERSION" ] && VERSION="0.1.0"

NAME="zuma-deluxe-hd-${VERSION}-linux-x86_64"
DIST="$ROOT/dist"
STAGE="$DIST/$NAME"

echo "== Zuma Deluxe HD 出包 =="
echo "   版本: $VERSION"
echo "   构建目录: $BUILD_DIR"

# ── 1. 构建（如果没有 ZumaHD）──────────────────────────────────────────────────
if [ ! -x "$BUILD_DIR/bin/ZumaHD" ]; then
    echo "== 构建 =="
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

# ── 2. 组装 ───────────────────────────────────────────────────────────────────
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/lib" "$STAGE/share"

cp "$BUILD_DIR/bin/ZumaHD" "$STAGE/bin/"

# 素材（CMake 会把 content/ 拷到 build/bin，此处以源码目录为准）
cp -r "$ROOT/content" "$STAGE/share/content"

# BASS / BASS_FX 动态库（自带，避免用户去 un4seen 下载）
for lib in "$ROOT/lib/bass/libs/x86_64/libbass.so" "$ROOT/lib/bass-fx/libs/x86_64/libbass_fx.so"; do
    [ -f "$lib" ] && cp "$lib" "$STAGE/lib/" || echo "  ⚠️  缺 $lib（音频可能不可用）"
done

# vendored SDL_ttf（构建时用的是仓库内 2.25 版；系统包常常没有/版本太老）——
# 不带它的话运行时直接 "error while loading shared libraries: libSDL2_ttf-2.0.so.0"
FOUND_TTF=0
for lib in "$BUILD_DIR"/external/SDL_ttf/libSDL2_ttf*.so*; do
    [ -f "$lib" ] && cp -a "$lib" "$STAGE/lib/" && FOUND_TTF=1
done
[ "$FOUND_TTF" = 0 ] && echo "  ⚠️  没找到 vendored SDL_ttf（打包出的程序需要系统 libSDL2_ttf）"

# 启动脚本：cd 到 share/content/（游戏用相对路径读 levels/ levels.xml、images/、sounds/，
# 即"资源根目录"必须是工作目录）
cat > "$STAGE/zuma.sh" <<'LAUNCH'
#!/bin/bash
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE/share/content"
exec "$HERE/bin/ZumaHD" "$@"
LAUNCH
chmod +x "$STAGE/zuma.sh" "$STAGE/bin/ZumaHD"

# RPATH: 优先找同包 lib/
if command -v patchelf >/dev/null 2>&1; then
    patchelf --set-rpath '$ORIGIN/../lib' "$STAGE/bin/ZumaHD"
    echo "   RPATH 已设为 \$ORIGIN/../lib"
else
    echo "  ⚠️  未装 patchelf：运行时需自行保证 libbass.so 可被找到"
fi

cat > "$STAGE/README.txt" <<README
Zuma Deluxe HD (重制版) $VERSION
================================
运行：解包后执行 ./zuma.sh
      或 cd share && ../bin/ZumaHD

操作：鼠标瞄准 / 左键发射 / 右键换球 / ESC 暂停
      F11 或 Alt+Enter 切换全屏

依赖：SDL2 / SDL2_image / expat（发行版包即可）
      BASS、BASS_FX、SDL_ttf 已随包提供（lib/，RPATH 已指向它）
README

# ── 3. 打包 ───────────────────────────────────────────────────────────────────
( cd "$DIST" && tar czf "$NAME.tar.gz" "$NAME" )

echo "== 完成 =="
echo "   产物: $DIST/$NAME.tar.gz"
ls -la "$DIST/$NAME.tar.gz"