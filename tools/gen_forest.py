#!/usr/bin/env python3
"""Generate the Photo face's background: a layered pine forest at dawn.

Writes main/assets/photo.rgb565 (1024x600, RGB565 little-endian, what
face_photo.c embeds) and, with --png, a preview PNG beside it.

Needs only Pillow. Seeded, so the output is the same every run.

    python3 tools/gen_forest.py [--png]
"""
import math
import random
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

W, H = 1024, 600
OUT = Path(__file__).resolve().parent.parent / "main" / "assets" / "photo.rgb565"

rng = random.Random(1729)
SUN = (690, 330)
MIST = (232, 196, 164)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def sky():
    img = Image.new("RGB", (W, H))
    px = img.load()
    top, mid, low = (24, 44, 84), (104, 136, 176), (250, 190, 128)
    sun_x, sun_y = SUN
    for y in range(H):
        t = y / H
        base = lerp(top, mid, t / 0.35) if t < 0.35 else lerp(mid, low, min(1, (t - 0.35) / 0.25))
        for x in range(W):
            d = math.hypot(x - sun_x, (y - sun_y) * 1.4)
            glow = max(0.0, 1.0 - d / 420.0) ** 2
            px[x, y] = lerp(base, (255, 232, 180), glow * 0.85)
    return img


def pine(draw, x, base_y, height, colour):
    """A conifer as stacked triangles, slightly irregular so a row of them
    does not read as clip art."""
    width = height * rng.uniform(0.32, 0.42)
    tiers = rng.randint(4, 6)
    for i in range(tiers):
        t0 = i / tiers
        top = base_y - height * (1 - t0 * 0.85) - height * 0.15
        bottom = base_y - height * (0.12 + (1 - t0) * 0.05) * (1 - t0) * 3
        bottom = min(base_y - height * 0.08, top + height * 0.45)
        half = width * (0.35 + t0 * 0.65) / 2
        jitter = rng.uniform(-0.08, 0.08) * half
        draw.polygon([(x, top), (x - half + jitter, bottom), (x + half + jitter, bottom)], fill=colour)
    draw.rectangle([x - height * 0.02, base_y - height * 0.1, x + height * 0.02, base_y], fill=colour)


def ridge(draw, y0, amp, colour, seed):
    r = random.Random(seed)
    pts = [(0, H)]
    phase = [r.uniform(0, 6.28) for _ in range(3)]
    for x in range(0, W + 8, 8):
        y = y0 + amp * (0.6 * math.sin(x / 170 + phase[0]) + 0.3 * math.sin(x / 61 + phase[1])
                        + 0.1 * math.sin(x / 23 + phase[2]))
        pts.append((x, y))
    pts.append((W, H))
    draw.polygon(pts, fill=colour)


def tree_row(img, base_y, h_range, spacing, colour, haze, blur, clearing=None):
    """One depth band of forest. `clearing` = (x0, x1) leaves a gap there,
    with trees thinning toward it rather than stopping at a wall."""
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    c = colour + (255,)
    if clearing is None:
        d.rectangle([0, base_y - 4, W, H], fill=c)
    x = rng.uniform(-spacing, 0)
    while x < W + spacing:
        h = rng.uniform(*h_range)
        if clearing:
            x0, x1 = clearing
            edge = min(abs(x - x0), abs(x - x1))
            inside = x0 < x < x1
            if inside or (edge < 120 and rng.random() < 0.6):
                x += spacing * rng.uniform(0.55, 1.25)
                continue
        pine(d, x, base_y + rng.uniform(-6, 6), h, c)
        x += spacing * rng.uniform(0.55, 1.25)
    if blur:
        layer = layer.filter(ImageFilter.GaussianBlur(blur))
    # Mist: lift each row toward the sky colour the further back it is.
    mist = Image.new("RGBA", (W, H), MIST + (int(255 * haze),))
    mist.putalpha(Image.eval(layer.getchannel("A"), lambda a: int(a * haze)))
    img.alpha_composite(layer)
    img.alpha_composite(mist)


def rays(img):
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(7):
        a = math.radians(100 + i * 9 + rng.uniform(-2, 2))
        sx, sy = SUN
        x2, y2 = sx + math.cos(a) * 900, sy + math.sin(a) * 900
        spread = rng.uniform(18, 40)
        d.polygon([(sx, sy), (x2 - spread, y2), (x2 + spread, y2)], fill=(255, 236, 190, 26))
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(12)))


def meadow(img):
    d = ImageDraw.Draw(img)
    # Clearing floor: lit near the sun, darker toward the edges.
    for y in range(500, H):
        t = (y - 500) / (H - 500)
        d.line([(0, y), (W, y)], fill=lerp((96, 112, 52), (30, 52, 26), t) + (255,))
    for _ in range(2600):
        x = rng.uniform(0, W)
        y = rng.uniform(505, H)
        h = rng.uniform(6, 22) * (0.4 + (y - 505) / 95)
        lean = rng.uniform(-5, 5)
        g = rng.randint(60, 110)
        d.line([(x, y), (x + lean, y - h)], fill=(g // 2, g, g // 3, 255), width=1)
    # Sunlit flecks in the grass.
    for _ in range(140):
        x, y = rng.uniform(0, W), rng.uniform(530, H)
        d.ellipse([x, y, x + 2, y + 2], fill=(236, 214, 120, 255))


def main():
    base = sky().convert("RGBA")
    d = ImageDraw.Draw(base)
    sx, sy = SUN
    d.ellipse([sx - 34, sy - 34, sx + 34, sy + 34], fill=(255, 244, 214, 255))
    ridge(d, 352, 26, (168, 150, 160), 1)
    ridge(d, 380, 18, (132, 124, 138), 2)
    rays(base)
    tree_row(base, 420, (50, 90), 20, (96, 104, 108), 0.55, 1.4)
    tree_row(base, 455, (80, 130), 28, (58, 74, 66), 0.3, 0.9)
    tree_row(base, 505, (120, 200), 40, (26, 46, 36), 0.08, 0.5, clearing=(560, 820))
    meadow(base)
    tree_row(base, 640, (330, 520), 70, (12, 24, 18), 0.0, 0, clearing=(300, 860))

    img = base.convert("RGB")
    with open(OUT, "wb") as f:
        for r, g, b in img.getdata():
            f.write(struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)))
    if "--png" in sys.argv:
        img.save(OUT.with_suffix(".png"))
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
