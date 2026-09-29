#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Zuma Deluxe HD — Linux 出包（ROADMAP 4.6 / 分发）
#   产物（都在 dist/）：
#     zuma-deluxe-hd-<版本>-linux-x86_64.tar.gz   免安装（解包即跑）
#     zuma-deluxe-hd_<版本>_amd64.deb             apt install ./xxx.deb 即装
#     sha256sums.txt                              校验和（发布页一并提供）
#
#   为什么自带 lib/：BASS/BASS_FX 是免费但不开源的音频库（un4seen），
#   SDL_ttf 用的是仓库内 vendored 2.25（系统包常常没有/版本太老，
#   代码用到 TTF_SetFontWrappedAlign ≥2.20）。RPATH 设成 $ORIGIN/../lib，
#   所以用户不需要装这三个库。
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
DIST="$ROOT/dist"

# 版本：优先命令行/环境，其次 git tag（v0.2.0 → 0.2.0），最后 CMakeLists
VERSION="${VERSION:-}"
if [ -z "$VERSION" ]; then
    VERSION="$(git -C "$ROOT" describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')"
fi
if [ -z "$VERSION" ]; then
    VERSION="$(sed -n 's/.*VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
fi
[ -z "$VERSION" ] && VERSION="0.1.0"

NAME="zuma-deluxe-hd-${VERSION}-linux-x86_64"
STAGE="$DIST/$NAME"
DEB_ROOT="$DIST/deb-root"

echo "== Zuma Deluxe HD 出包 =="
echo "   版本: $VERSION"
echo "   构建目录: $BUILD_DIR"

# ── 1. 构建 ───────────────────────────────────────────────────────────────────
if [ ! -x "$BUILD_DIR/bin/ZumaHD" ] || [ "${REBUILD:-0}" = "1" ]; then
    echo "== 构建 =="
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

# ── 2. 组装免安装目录树 ───────────────────────────────────────────────────────
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/lib" "$STAGE/share" "$STAGE/icons"

cp "$BUILD_DIR/bin/ZumaHD" "$STAGE/bin/"
cp -r "$ROOT/content" "$STAGE/share/content"

for lib in "$ROOT/lib/bass/libs/x86_64/libbass.so" "$ROOT/lib/bass-fx/libs/x86_64/libbass_fx.so"; do
    if [ -f "$lib" ]; then cp "$lib" "$STAGE/lib/"; else echo "  ⚠️  缺 $lib（音频可能不可用）"; fi
done

FOUND_TTF=0
for lib in "$BUILD_DIR"/external/SDL_ttf/libSDL2_ttf*.so*; do
    if [ -f "$lib" ]; then cp -a "$lib" "$STAGE/lib/"; FOUND_TTF=1; fi
done
[ "$FOUND_TTF" = 0 ] && echo "  ⚠️  没找到 vendored SDL_ttf → 需要系统 libSDL2_ttf"

# 启动脚本：cd 到 share/content/（游戏用相对路径读 levels/levels.xml、images/、sounds/）
cat > "$STAGE/zuma.sh" <<'LAUNCH'
#!/bin/bash
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE/share/content"
exec "$HERE/bin/ZumaHD" "$@"
LAUNCH
chmod +x "$STAGE/zuma.sh" "$STAGE/bin/ZumaHD"

if command -v patchelf >/dev/null 2>&1; then
    patchelf --set-rpath '$ORIGIN/../lib' "$STAGE/bin/ZumaHD"
    echo "   RPATH 已设为 \$ORIGIN/../lib"
else
    echo "  ⚠️  未装 patchelf：运行时需自行保证 libbass.so 可被找到"
fi

# ── 3. 图标（从游戏素材里切一颗球做应用图标；没必要另造美术）──────────────────
ICON_PNG=""
if [ -f "$ROOT/content/images/gameobjects.png" ]; then
    ICON_PNG="$STAGE/icons/zuma-deluxe-hd.png"
    "$ROOT/scripts/make_icon.py" "$ROOT/content/images/gameobjects.png" "$ICON_PNG" 2>/dev/null \
        && echo "   图标: $ICON_PNG" \
        || { ICON_PNG=""; echo "  ⚠️  图标生成失败（需要带 PIL 的 python）"; }
fi

cat > "$STAGE/README.txt" <<README
Zuma Deluxe HD (重制版) $VERSION
================================

运行（免安装）：
  ./zuma.sh                 # 或双击桌面快捷方式（scripts/install_desktop.sh 生成）
  ./zuma.sh --help          # 全部命令行选项（含无头自动测试）

操作：鼠标瞄准 / 左键发射 / 右键换球 / ESC 暂停 / F11 或 Alt+Enter 全屏

依赖：SDL2、SDL2_image、expat（发行版包即可）
      BASS、BASS_FX、SDL_ttf 已随包提供（lib/，RPATH 已指向它）

存档：~/.local/share/zumahd/progress.dat（最高分/最佳用时/解锁进度）
README

# 给 deb 复用免安装树（deb 只是换个安装位置 + 桌面项）
cp -a "$STAGE" "$DIST/.deb-stage-$NAME"

# ── 4. 打 tar.gz ──────────────────────────────────────────────────────────────
( cd "$DIST" && tar czf "$NAME.tar.gz" "$NAME" )
echo "   tar.gz: $DIST/$NAME.tar.gz"

# ── 5. 打 .deb（apt install ./xxx.deb 即装，自带桌面菜单项）────────────────────
if command -v dpkg-deb >/dev/null 2>&1; then
    rm -rf "$DEB_ROOT"
    PKG="$DEB_ROOT/opt/zuma-deluxe-hd"

    mkdir -p "$PKG" \
             "$DEB_ROOT/usr/share/applications" \
             "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps" \
             "$DEB_ROOT/usr/bin" \
             "$DEB_ROOT/DEBIAN"

    cp -a "$STAGE/bin" "$STAGE/lib" "$STAGE/share" "$STAGE/zuma.sh" "$PKG/"
    chmod +x "$PKG/zuma.sh"

    if [ -n "$ICON_PNG" ]; then
        cp "$ICON_PNG" "$DEB_ROOT/usr/share/icons/hicolor/256x256/apps/zuma-deluxe-hd.png"
    fi

    ln -sf /opt/zuma-deluxe-hd/zuma.sh "$DEB_ROOT/usr/bin/zuma-deluxe-hd"

    cat > "$DEB_ROOT/usr/share/applications/zuma-deluxe-hd.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Zuma Deluxe HD
Name[zh_CN]=祖玛高清重制版
Comment=Open-source Zuma Deluxe HD remake
Comment[zh_CN]=开源的祖玛高清重制版
Exec=/opt/zuma-deluxe-hd/zuma.sh %U
Path=/opt/zuma-deluxe-hd/share/content
Icon=zuma-deluxe-hd
Terminal=false
Categories=Game;ArcadeGame;
Keywords=zuma;marble;shooter;puzzle;
StartupNotify=true
DESKTOP

    SIZE_KB=$(du -sk "$DEB_ROOT" | awk '{print $1}')

    cat > "$DEB_ROOT/DEBIAN/control" <<CONTROL
Package: zuma-deluxe-hd
Version: $VERSION
Section: games
Priority: optional
Architecture: amd64
Installed-Size: $SIZE_KB
Depends: libsdl2-2.0-0, libsdl2-image-2.0-0, libexpat1
Maintainer: dd88w <dd88five@gmail.com>
Description: Zuma Deluxe HD remake (open source)
 Marble-shooter remake of Zuma Deluxe with balls, power-ups, gauntlet mode,
 13 stages, progress saving and a full headless autotest suite.
CONTROL

    echo "/opt/zuma-deluxe-hd/share/content" > "$DEB_ROOT/DEBIAN/conffiles" 2>/dev/null || true
    rm -f "$DEB_ROOT/DEBIAN/conffiles"

    dpkg-deb --build --root-owner-group "$DEB_ROOT" "$DIST/zuma-deluxe-hd_${VERSION}_amd64.deb" >/dev/null
    echo "   deb:    $DIST/zuma-deluxe-hd_${VERSION}_amd64.deb"

    rm -rf "$DEB_ROOT" "$DIST/.deb-stage-$NAME"
else
    echo "  ⚠️  没装 dpkg-deb，跳过 .deb"
fi

# ── 6. 校验和（发布页一起提供，用户可验完整性）───────────────────────────────
( cd "$DIST" && sha256sum "zuma-deluxe-hd-${VERSION}-linux-x86_64.tar.gz" > sha256sums.txt
  [ -f "zuma-deluxe-hd_${VERSION}_amd64.deb" ] && sha256sum "zuma-deluxe-hd_${VERSION}_amd64.deb" >> sha256sums.txt
  true )
echo "   校验和: $DIST/sha256sums.txt"

echo "== 完成 =="
ls -la "$DIST"/zuma-deluxe-hd* "$DIST"/sha256sums.txt | awk '{print "   ", $5, $9}'