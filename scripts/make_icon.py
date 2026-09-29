#!/usr/bin/env python3
"""从游戏素材里切一颗球当应用图标（避免另造美术）。

用法: make_icon.py <gameobjects.png> <输出.png> [尺寸，默认 256]

gameobjects.png 的球体条：6 列 × 48px，从 (0,0) 起竖向排列（蓝球列 47 帧）。
这里取第一列前几帧里"最完整"的一帧，缩放到目标尺寸并加一点圆形底，
做成 hicolor 图标（GNOME/KDE 都会用它）。
"""
import sys
from PIL import Image, ImageDraw

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1

    src, dst = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 256

    sheet = Image.open(src).convert('RGBA')

    # 蓝球列（x=0..47）第 20 帧：转动到正面、没有道具图标
    ball = sheet.crop((0, 20 * 48, 48, 21 * 48))

    canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(canvas)

    # 圆形深色底（深色主题下也看得清）
    d.ellipse((2, 2, size - 2, size - 2), fill=(24, 30, 44, 255))
    d.ellipse((2, 2, size - 2, size - 2), outline=(255, 208, 96, 255), width=max(2, size // 48))

    inner = int(size * 0.72)
    resample = getattr(Image, "Resampling", Image).LANCZOS   # Pillow 9.1+ / 老版本都兼容
    ball = ball.resize((inner, inner), resample)
    off = (size - inner) // 2
    canvas.alpha_composite(ball, (off, off))

    canvas.save(dst)
    print("icon:", dst)
    return 0

if __name__ == '__main__':
    sys.exit(main())