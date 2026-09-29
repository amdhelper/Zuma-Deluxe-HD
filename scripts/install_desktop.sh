#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# Zuma Deluxe HD — 装到当前用户 + 生成桌面快捷方式
#
#   ./scripts/install_desktop.sh              # 用已出的包（没有就现打）
#   PREFIX=~/.local/share/zuma-deluxe-hd ./scripts/install_desktop.sh
#
# 做完的事：
#   1) 把包解到 $PREFIX（默认 ~/.local/share/zuma-deluxe-hd）
#   2) 从游戏素材切一颗球做图标 → ~/.local/share/icons/hicolor/256x256/apps/
#   3) 写 .desktop 到 ~/.local/share/applications/（应用菜单里能看到）
#   4) 复制一份到桌面目录（~/桌面 或 ~/Desktop），并标记为"可信任"
#   5) desktop-file-validate 校验 + gtk-launch 冒烟（有就做）
#
# 卸载：rm -rf $PREFIX ~/.local/share/applications/zuma-deluxe-hd.desktop
#       以及桌面上的 zuma-deluxe-hd.desktop
# ═══════════════════════════════════════════════════════════════════════════════
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist"
PREFIX="${PREFIX:-$HOME/.local/share/zuma-deluxe-hd}"

echo "== 安装 Zuma Deluxe HD 到 $PREFIX =="

# ── 1. 需要包：没有就先出包 ───────────────────────────────────────────────────
TARBALL="$(ls -1t "$DIST"/zuma-deluxe-hd-*-linux-x86_64.tar.gz 2>/dev/null | head -1 || true)"
if [ -z "$TARBALL" ]; then
    echo "   dist/ 里没有包，先打包..."
    "$ROOT/scripts/package_linux.sh" >/dev/null
    TARBALL="$(ls -1t "$DIST"/zuma-deluxe-hd-*-linux-x86_64.tar.gz | head -1)"
fi
echo "   使用包: $TARBALL"

TMP="$(mktemp -d)"
tar xzf "$TARBALL" -C "$TMP"
SRC="$(find "$TMP" -maxdepth 1 -mindepth 1 -type d | head -1)"

rm -rf "$PREFIX"
mkdir -p "$(dirname "$PREFIX")"
cp -a "$SRC" "$PREFIX"
rm -rf "$TMP"
echo "   程序: $PREFIX/zuma.sh"

# ── 2. 图标 ───────────────────────────────────────────────────────────────────
ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
mkdir -p "$ICON_DIR"
ICON="$ICON_DIR/zuma-deluxe-hd.png"

if [ -f "$ROOT/content/images/gameobjects.png" ]; then
    # 打包脚本已经把图标放进包里了（icons/），优先用它；否则现场生成
    if [ -f "$PREFIX/icons/zuma-deluxe-hd.png" ]; then
        cp "$PREFIX/icons/zuma-deluxe-hd.png" "$ICON"
    else
        python3 "$ROOT/scripts/make_icon.py" "$ROOT/content/images/gameobjects.png" "$ICON" >/dev/null 2>&1 || true
    fi
fi
[ -f "$ICON" ] && echo "   图标: $ICON" || echo "  ⚠️  图标缺失（不影响运行）"

# ── 3. .desktop ──────────────────────────────────────────────────────────────
APPS="$HOME/.local/share/applications"
mkdir -p "$APPS"
DESKTOP_FILE="$APPS/zuma-deluxe-hd.desktop"

cat > "$DESKTOP_FILE" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Zuma Deluxe HD
Name[zh_CN]=祖玛高清重制版
Comment=Open-source Zuma Deluxe HD remake
Comment[zh_CN]=开源的祖玛高清重制版
Exec=$PREFIX/zuma.sh %U
Path=$PREFIX/share/content
Icon=zuma-deluxe-hd
Terminal=false
Categories=Game;ArcadeGame;
Keywords=zuma;marble;shooter;puzzle;
StartupNotify=true
DESKTOP

chmod +x "$DESKTOP_FILE"
echo "   菜单项: $DESKTOP_FILE"

if command -v desktop-file-validate >/dev/null 2>&1; then
    if desktop-file-validate "$DESKTOP_FILE"; then
        echo "   desktop-file-validate: 通过"
    else
        echo "  ⚠️  desktop-file-validate 有警告（见上）"
    fi
fi

# ── 4. 桌面快捷方式 ───────────────────────────────────────────────────────────
for D in "$HOME/桌面" "$HOME/Desktop"; do
    [ -d "$D" ] || continue

    cp "$DESKTOP_FILE" "$D/zuma-deluxe-hd.desktop"
    chmod +x "$D/zuma-deluxe-hd.desktop"

    # GNOME/Nautilus：标记为可信任，否则双击只会打开文本编辑器
    gio set "$D/zuma-deluxe-hd.desktop" metadata::trusted true 2>/dev/null || true

    echo "   桌面快捷方式: $D/zuma-deluxe-hd.desktop"
done

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true

echo "== 完成：应用菜单或桌面点「祖玛高清重制版」即可开玩 =="