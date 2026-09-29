#!/usr/bin/env python3
"""程序化生成关卡遮挡层素材（Cutout masks）——ROADMAP 3.7 缺素材的兜底方案

原理：
  曲线 .dat 里每个点带 t1（隧道）/t2（顶层优先）标记。把 t1 标记的曲线段画成
  "沿曲线的粗带"，颜色从该关卡自己的底图上采样 → 看起来就是隧道壁/岩壁，
  球链走到这段时会被它盖住（Level_DrawCutouts 把 cutout 画在球链之上）。

  Cutout 的坐标约定：贴图**中心**对齐 (x,y)（与 TreasurePoint 一致）。
  所以每个 cutout 单独生成一张 1280x720 的 PNG，内容按 (x,y) 做偏移：
  绝对坐标 (ax,ay) 的内容画在 PNG 的 (ax - x + 640, ay - y + 360)。

跑法： python3 scripts/make_cutout_masks.py [--levels-dir content/levels] [--force]
"""
import argparse
import os
import re
import struct
import sys

try:
    from PIL import Image, ImageDraw, ImageFilter
except ImportError:
    sys.exit("需要 Pillow：pip install pillow  （或用 .ai-memory/venv/bin/python 跑）")

SCREEN_W, SCREEN_H = 1280, 720


# ── 曲线 .dat 解析 ────────────────────────────────────────────────────────
def load_curve(path):
    """返回 [(x, y, t1, t2), ...]（游戏内绝对坐标，含 (x+104)*1.5 / y*1.5 换算）"""
    data = open(path, "rb").read()

    count = struct.unpack_from("<I", data, 0x10)[0]
    off = 0x14 + count * 10
    off += 4                      # curveLength
    off += 8                      # startPos (2 floats)

    dots = []
    x = y = 0

    for _ in range(count):
        t1, t2 = data[off], data[off + 1]
        dx = struct.unpack_from("<b", data, off + 2)[0]
        dy = struct.unpack_from("<b", data, off + 3)[0]
        off += 4

        x += dx / 100.0
        y += dy / 100.0

        dots.append(((x + 104) * 1.5, y * 1.5, t1, t2))

        if off + 4 > len(data):
            break

    return dots


# ── levels.xml 解析（只取 <nut><Graphics> 的 Cutout 与曲线名）────────────
def parse_levels_xml(path):
    """按 <Graphics ...> 分块解析（本仓库的 levels.xml 没有 <nut> 包裹层）"""
    xml = open(path, encoding="utf-8", errors="replace").read()
    levels = {}

    # 每个 Graphics 开头 → 下一个 Graphics 开头之间，算这一关的块
    starts = [(m.start(), m) for m in re.finditer(r"<Graphics\b[^>]*>", xml)]
    starts.append((len(xml), None))

    for i in range(len(starts) - 1):
        _, m = starts[i]
        block = xml[starts[i][0]:starts[i + 1][0]]
        attrs = m.group(0)

        gid = re.search(r'id="([^"]+)"', attrs)
        if not gid:
            continue

        gid = gid.group(1)
        curve  = re.search(r'\bcurve="([^"]+)"', attrs)
        curve2 = re.search(r'\bcurve2="([^"]+)"', attrs)

        cutouts = [
            {"image": c.group(1), "pri": c.group(2),
             "x": float(c.group(3)), "y": float(c.group(4))}
            for c in re.finditer(
                r'<Cutout\s+image="([^"]+)"\s+pri="(\d+)"\s+x="(-?\d+)"\s+y="(-?\d+)"\s*/>',
                block)
        ]

        if not cutouts:
            continue

        levels[gid] = {
            "curve":  curve.group(1) if curve else None,
            "curve2": curve2.group(1) if curve2 else None,
            "cutouts": cutouts,
        }

    return levels


def sample_color(img, px, py, radius=24):
    """在底图上采样一个区域的平均颜色（做隧道壁的底色）"""
    x0, y0 = max(0, int(px - radius)), max(0, int(py - radius))
    x1, y1 = min(img.width, int(px + radius)), min(img.height, int(py + radius))

    if x1 <= x0 or y1 <= y0:
        return (90, 80, 70)

    crop = img.crop((x0, y0, x1, y1)).resize((8, 8))

    r = sum(p[0] for p in crop.getdata()) // 64
    g = sum(p[1] for p in crop.getdata()) // 64
    b = sum(p[2] for p in crop.getdata()) // 64

    return (r, g, b)


def make_mask(level_dir, gid, entry, curve_files, bg_img):
    """给单个 cutout 生成一张 1280x720 的 PNG（内容按中心对齐做偏移）"""
    png = os.path.join(level_dir, gid, entry["image"] + ".png")
    os.makedirs(os.path.dirname(png), exist_ok=True)

    offset_x = 640 - entry["x"]
    offset_y = 360 - entry["y"]

    layer = Image.new("RGBA", (SCREEN_W, SCREEN_H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)

    painted = 0

    anchor = (entry["x"], entry["y"])

    for cf in curve_files:
        if not os.path.exists(cf):
            continue

        dots = load_curve(cf)

        # 优先用曲线自己的隧道标记点（t1）；没有标记点的关卡退回
        # "取离该 cutout 锚点最近的这一段曲线"（XML 作者把 cutout 放在那就是为了盖住这一段）
        tunneled = [d for d in dots if d[2]]

        if tunneled:
            use = tunneled
        elif dots:
            idx = min(range(len(dots)),
                      key=lambda i: (dots[i][0] - anchor[0]) ** 2 + (dots[i][1] - anchor[1]) ** 2)
            lo, hi = max(0, idx - 60), min(len(dots), idx + 61)
            use = dots[lo:hi]
        else:
            use = []

        for (ax, ay, _, _) in use:
            px, py = ax + offset_x, ay + offset_y

            if not (-60 < px < SCREEN_W + 60 and -60 < py < SCREEN_H + 60):
                continue

            color = sample_color(bg_img, ax, ay)
            r = 30                       # 带宽（球半径 24 再留点余量）

            draw.ellipse((px - r, py - r, px + r, py + r), fill=color + (235,))
            painted += 1

    if painted == 0:
        # 该关卡没有隧道标记点：不生成空图，避免"生成了但看不见"的假素材
        return None, 0

    layer = layer.filter(ImageFilter.GaussianBlur(3))
    layer.save(png)

    return png, painted


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--levels-dir", default="content/levels")
    ap.add_argument("--force", action="store_true", help="已存在也重做")
    args = ap.parse_args()

    levels_dir = args.levels_dir
    levels = parse_levels_xml(os.path.join(levels_dir, "levels.xml"))

    total = 0

    for gid, info in sorted(levels.items()):
        if not info["cutouts"]:
            continue

        curves = []
        for name in (info["curve"], info["curve2"]):
            if name:
                curves.append(os.path.join(levels_dir, gid, os.path.basename(name) + ".dat"))

        bg_path = os.path.join(levels_dir, gid, gid + ".jpg")

        if not os.path.exists(bg_path):
            # 底图名不一定等于 gid，退而求其次找目录里的第一张 jpg
            cands = [f for f in os.listdir(os.path.join(levels_dir, gid)) if f.endswith(".jpg")]
            bg_path = os.path.join(levels_dir, gid, cands[0]) if cands else None

        bg_img = Image.open(bg_path).convert("RGB") if bg_path and os.path.exists(bg_path) \
            else Image.new("RGB", (SCREEN_W, SCREEN_H), (90, 80, 70))

        for entry in info["cutouts"]:
            png = os.path.join(levels_dir, gid, entry["image"] + ".png")

            if os.path.exists(png) and not args.force:
                print(f"  跳过（已存在）: {png}")
                continue

            out, n = make_mask(levels_dir, gid, entry, curves, bg_img)

            if out:
                print(f"  ✅ {out}  （沿 {n} 个隧道点绘制，pri={entry['pri']}）")
                total += 1
            else:
                print(f"  ⚠️  {gid}/{entry['image']}: 该关卡曲线没有隧道标记点(t1=0)，跳过")

    print(f"\n生成 {total} 张遮挡贴图")
    return 0


if __name__ == "__main__":
    sys.exit(main())