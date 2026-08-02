#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# v1.60.0 — generate the welcome-screen background images (light + dark).
# Synced with the app's purpose (clinic reception & management): a clean
# medical gradient canvas with a subtle ECG heartbeat trace, soft bokeh
# light and translucent medical-cross motifs. Fully deterministic, no
# external assets, writes assets/bg_light.jpg + assets/bg_dark.jpg (1600x900).

import math
import random
from PIL import Image, ImageDraw, ImageFilter

W, H = 1600, 900


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def vertical_diagonal_gradient(size, stops, diagonal=0.35):
    """Multi-stop gradient blended along (y + x*diagonal).

    Rendered fully on a quarter-width canvas (every pixel written) then
    upscaled — no black stride gaps leaking into the blur.
    """
    w, h = size
    sw = max(1, w // 4)
    img = Image.new("RGB", (sw, h))
    px = img.load()
    span = h + w * diagonal
    n = len(stops) - 1
    for y in range(h):
        for sx in range(sw):
            t = (y + (sx * 4) * diagonal) / span
            t = max(0.0, min(1.0, t)) * n
            i = min(int(t), n - 1)
            px[sx, y] = lerp(stops[i], stops[i + 1], t - i)
    return img.resize((w, h), Image.BILINEAR)


def ecg_path(x0, x1, base_y, amp):
    """One heartbeat trace as a list of (x, y) points."""
    pts = []
    w = x1 - x0
    # segments: flat, P bump, QRS spike, T bump, flat  (fractions of width)
    def qrs(u):
        if u < 0.40:  # flat
            return 0.0
        if u < 0.46:  # P wave
            return -0.16 * math.sin((u - 0.40) / 0.06 * math.pi)
        if u < 0.50:  # Q dip
            return 0.10 * math.sin((u - 0.46) / 0.04 * math.pi)
        if u < 0.54:  # R spike
            return 1.00 * math.sin((u - 0.50) / 0.04 * math.pi)
        if u < 0.58:  # S dip
            return -0.28 * math.sin((u - 0.54) / 0.04 * math.pi)
        if u < 0.62:  # return
            return 0.0
        if u < 0.72:  # T wave
            return -0.24 * math.sin((u - 0.62) / 0.10 * math.pi)
        return 0.0
    for i in range(w + 1):
        u = i / w
        pts.append((x0 + i, base_y - qrs(u) * amp))
    return pts


def glow_cross(size, color, alpha):
    """A soft, rounded medical cross as an RGBA patch."""
    s = size
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    arm = s // 3
    off = (s - arm) // 2
    d.rounded_rectangle([off, 0, off + arm, s], radius=arm // 2, fill=color + (alpha,))
    d.rounded_rectangle([0, off, s, off + arm], radius=arm // 2, fill=color + (alpha,))
    return im.filter(ImageFilter.GaussianBlur(size * 0.06))


def bokeh(img, blobs, color):
    """Soft out-of-focus light circles."""
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for (x, y, r, a) in blobs:
        d.ellipse([x - r, y - r, x + r, y + r], fill=color + (a,))
    layer = layer.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(layer)


def build(dark, out):
    random.seed(104 if dark else 103)
    if dark:
        stops = [(7, 18, 38), (10, 32, 66), (14, 52, 96), (18, 74, 122)]
        ecg_col = (96, 200, 255)
        cross_col = (120, 200, 255)
        bokeh_col = (90, 160, 240)
    else:
        stops = [(255, 255, 255), (240, 248, 255), (219, 236, 251), (190, 219, 245)]
        ecg_col = (24, 106, 205)
        cross_col = (255, 255, 255)
        bokeh_col = (255, 255, 255)

    base = vertical_diagonal_gradient((W, H), stops).convert("RGBA")

    # translucent cross motifs scattered (denser at the edges, clear centre)
    crosses = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for _ in range(26):
        s = random.choice((60, 80, 110, 150, 200))
        x = random.randint(-40, W - s + 40)
        y = random.randint(-40, H - s + 40)
        # keep the central band clean for the hero panel
        if W * 0.22 < x < W * 0.78 and H * 0.18 < y < H * 0.80 and random.random() < 0.75:
            continue
        a = random.randint(14, 34) if dark else random.randint(24, 55)
        patch = glow_cross(s, cross_col, a)
        crosses.alpha_composite(patch, (x, y))
    base.alpha_composite(crosses)

    # ECG heartbeat trace across the lower third — the clinic signature.
    ecg = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(ecg)
    y_base = int(H * 0.70)
    amp = int(H * 0.10)
    pts = ecg_path(-20, W + 20, y_base, amp)
    # glow pass (wide, soft) then the crisp line
    d.line(pts, fill=ecg_col + (70,), width=10, joint="curve")
    ecg = ecg.filter(ImageFilter.GaussianBlur(6))
    base.alpha_composite(ecg)
    ecg2 = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d2 = ImageDraw.Draw(ecg2)
    d2.line(pts, fill=ecg_col + (200 if dark else 150,), width=3, joint="curve")
    base.alpha_composite(ecg2)
    # a bright pulse dot riding the R peak
    pk = pts[int((W + 40) * 0.52)]
    dot = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dd = ImageDraw.Draw(dot)
    dd.ellipse([pk[0] - 7, pk[1] - 7, pk[0] + 7, pk[1] + 7], fill=ecg_col + (230,))
    base.alpha_composite(dot.filter(ImageFilter.GaussianBlur(2)))

    # bokeh light
    blobs = []
    for _ in range(22):
        x = random.randint(0, W)
        y = random.randint(0, int(H * 0.55))
        r = random.randint(18, 90)
        a = random.randint(10, 30) if dark else random.randint(28, 70)
        blobs.append((x, y, r, a))
    bokeh(base, blobs, bokeh_col)

    # gentle centre spotlight so the hero content pops (darker edges)
    vign = Image.new("L", (W, H), 0)
    vd = ImageDraw.Draw(vign)
    vd.ellipse([-W * 0.25, -H * 0.35, W * 1.25, H * 1.15], fill=70)
    vign = vign.filter(ImageFilter.GaussianBlur(120))
    if dark:
        base = Image.composite(Image.new("RGBA", (W, H), (4, 12, 26, 60)), base, vign)
    else:
        base = Image.composite(Image.new("RGBA", (W, H), (255, 255, 255, 70)), base, vign)

    base.convert("RGB").save(out, "JPEG", quality=88, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    build(False, "assets/bg_light.jpg")
    build(True, "assets/bg_dark.jpg")
