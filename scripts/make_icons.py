#!/usr/bin/env python3
# ============================================================================
#  make_icons.py — v1.63.0 professional icon set generator
#
#  The six raster icons embedded in the EXE (RCDATA 201..206) used to be crude,
#  hand-drawn bitmaps: the gear was an 8-spoke blob with square teeth, the
#  «last receipt» glyph was a printer with an unreadable clock jammed into its
#  corner, the calculator's keys were 1-px dots. They read as amateur at the
#  22 px size the toolbar actually draws them at.
#
#  This script re-draws all six from exact geometry as SOLID WHITE shapes on a
#  transparent canvas (the C++ side recolours them through a GDI+ colour matrix
#  in gpDrawTintedImageRes, so only the ALPHA channel carries the artwork).
#
#  Quality rules enforced here:
#    • drawn at SS× the final size and box-downsampled → true anti-aliasing
#      with no jaggies at any DPI;
#    • one shared 256 px art-board and one 24-unit design grid, so all six
#      icons share optical weight, stroke width and padding (this is what makes
#      an icon set look designed instead of assembled);
#    • strokes are real geometry (annulus / capsule polygons), never 1-px
#      lines, so they stay visible after downsampling;
#    • every glyph is optically centred on the art-board.
# ============================================================================
import math
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "icons")
SIZE = 256          # final PNG edge
SS = 8              # supersample factor
W = SIZE * SS       # working edge
G = W / 24.0        # one design-grid unit
STROKE = 1.55 * G   # the set's canonical stroke weight


def u(v):
    """design-grid units → working pixels"""
    return v * G


def new_canvas():
    img = Image.new("L", (W, W), 0)
    return img, ImageDraw.Draw(img)


# --------------------------------------------------------------- primitives --
def rrect(d, x0, y0, x1, y1, r, fill=255):
    d.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=fill)


def rring(d, x0, y0, x1, y1, r, t=None):
    """rounded-rect outline of real thickness t (drawn as two filled rrects)"""
    if t is None:
        t = STROKE
    rrect(d, x0, y0, x1, y1, r, 255)
    ri = max(r - t, 0)
    rrect(d, x0 + t, y0 + t, x1 - t, y1 - t, ri, 0)


def disc(d, cx, cy, r, fill=255):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=fill)


def ring(d, cx, cy, r, t=None):
    if t is None:
        t = STROKE
    disc(d, cx, cy, r, 255)
    disc(d, cx, cy, max(r - t, 0), 0)


def capsule(d, x0, y0, x1, y1, t=None, fill=255):
    """a round-capped line of thickness t from (x0,y0) to (x1,y1)"""
    if t is None:
        t = STROKE
    hr = t / 2.0
    ang = math.atan2(y1 - y0, x1 - x0)
    nx, ny = -math.sin(ang) * hr, math.cos(ang) * hr
    d.polygon(
        [(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)],
        fill=fill,
    )
    disc(d, x0, y0, hr, fill)
    disc(d, x1, y1, hr, fill)


def polyline(d, pts, t=None, fill=255):
    for i in range(len(pts) - 1):
        capsule(d, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], t, fill)


def arc_stroke(d, cx, cy, r, a0, a1, t=None, steps=96):
    """a circular arc of real thickness t, degrees, CCW screen coords"""
    if t is None:
        t = STROKE
    pts = []
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    polyline(d, pts, t)


def save(img, name):
    small = img.resize((SIZE, SIZE), Image.LANCZOS)
    out = Image.new("RGBA", (SIZE, SIZE), (255, 255, 255, 0))
    out.putalpha(small)
    white = Image.new("RGBA", (SIZE, SIZE), (255, 255, 255, 255))
    white.putalpha(small)
    path = os.path.join(OUT, name)
    white.save(path, "PNG", optimize=True)
    return path


# ------------------------------------------------------------------- glyphs --
def ic_settings():
    """A precise 8-tooth cog: trapezoidal teeth on a true circle, concentric
    hub ring. Replaces the old blobby spoke-wheel."""
    img, d = new_canvas()
    cx = cy = W / 2.0
    r_out = u(11.0)     # tooth tip
    r_body = u(8.6)     # gear body
    n = 8
    half = math.pi / n
    tooth_root = half * 0.62
    tooth_tip = half * 0.34
    pts = []
    for i in range(n):
        a = i * 2 * math.pi / n
        for aa, rr in (
            (a - tooth_root, r_body),
            (a - tooth_tip, r_out),
            (a + tooth_tip, r_out),
            (a + tooth_root, r_body),
        ):
            pts.append((cx + rr * math.cos(aa), cy + rr * math.sin(aa)))
    # fill the toothed body, then punch the hub bore
    d.polygon(pts, fill=255)
    disc(d, cx, cy, r_body * 0.995, 255)
    disc(d, cx, cy, u(3.7), 0)
    return save(img, "ic_settings.png")


def ic_calc():
    """Calculator: rounded chassis, inset display, 3x3 keys + a tall = key."""
    img, d = new_canvas()
    bw, bh = u(15.0), u(19.0)
    x0, y0 = W / 2 - bw / 2, W / 2 - bh / 2
    x1, y1 = x0 + bw, y0 + bh
    rring(d, x0, y0, x1, y1, u(2.6), STROKE)
    pad = u(2.1)
    # display
    dh = u(3.3)
    rrect(d, x0 + pad, y0 + pad, x1 - pad, y0 + pad + dh, u(0.8), 255)
    # keypad: 3 columns x 3 rows, plus a double-height key at bottom-right
    gy0 = y0 + pad + dh + u(1.7)
    gx0, gx1 = x0 + pad, x1 - pad
    cols, rows = 3, 3
    gap = u(1.15)
    kw = (gx1 - gx0 - gap * (cols - 1)) / cols
    kh = (y1 - pad - gy0 - gap * (rows - 1)) / rows
    kr = u(0.62)
    for r in range(rows):
        for c in range(cols):
            kx = gx0 + c * (kw + gap)
            ky = gy0 + r * (kh + gap)
            if r == rows - 1 and c == cols - 1:
                continue
            rrect(d, kx, ky, kx + kw, ky + kh, kr, 255)
    # tall "=" key spanning the last two rows of the last column
    kx = gx0 + (cols - 1) * (kw + gap)
    ky = gy0 + (rows - 2) * (kh + gap)
    rrect(d, kx, ky, kx + kw, gy0 + rows * kh + (rows - 1) * gap, kr, 255)
    return save(img, "ic_calc.png")


def ic_printer():
    """Printer: paper in at the top, chassis with a status lamp, printed sheet
    emerging at the bottom with two ruled lines."""
    img, d = new_canvas()
    cx = W / 2.0
    # paper feeding in (behind the chassis)
    pw = u(9.6)
    rrect(d, cx - pw / 2, u(2.6), cx + pw / 2, u(8.4), u(0.7), 255)
    d.rectangle([cx - pw / 2 + STROKE, u(2.6) + STROKE, cx + pw / 2 - STROKE, u(8.4)], fill=0)
    # chassis
    bw = u(17.4)
    rrect(d, cx - bw / 2, u(8.0), cx + bw / 2, u(16.0), u(2.2), 255)
    # status lamp (punched out of the chassis)
    disc(d, cx + bw / 2 - u(2.6), u(10.4), u(0.95), 0)
    # ventilation slot
    rrect(d, cx - bw / 2 + u(2.0), u(10.0), cx - u(1.2), u(10.9), u(0.45), 0)
    # printed sheet emerging
    sw = u(11.2)
    rrect(d, cx - sw / 2, u(14.2), cx + sw / 2, u(21.4), u(0.9), 255)
    d.rectangle([cx - sw / 2 + STROKE, u(14.2), cx + sw / 2 - STROKE, u(21.4) - STROKE], fill=0)
    # two ruled lines on the sheet
    capsule(d, cx - sw / 2 + u(2.0), u(17.2), cx + sw / 2 - u(2.0), u(17.2), STROKE * 0.72)
    capsule(d, cx - sw / 2 + u(2.0), u(19.2), cx + sw / 2 - u(3.6), u(19.2), STROKE * 0.72)
    return save(img, "ic_printer.png")


def ic_receipt():
    """Invoice: sheet with a torn (zig-zag) bottom edge, three ruled lines and
    a total rule — the classic receipt silhouette."""
    img, d = new_canvas()
    cx = W / 2.0
    bw = u(14.0)
    x0, x1 = cx - bw / 2, cx + bw / 2
    y0, ybody = u(2.4), u(19.2)
    # outline: top rounded, bottom zig-zag
    teeth = 5
    step = (x1 - x0) / (teeth * 2)
    pts = [(x0, y0 + u(1.2)), (x0 + u(1.2), y0), (x1 - u(1.2), y0), (x1, y0 + u(1.2))]
    pts.append((x1, ybody))
    x = x1
    dn = u(1.5)
    for i in range(teeth * 2):
        x -= step
        pts.append((x, ybody + (dn if i % 2 == 0 else 0)))
    pts.append((x0, ybody))
    d.polygon(pts, fill=255)
    # hollow it out
    inner = [
        (x0 + STROKE, y0 + u(1.2) + STROKE * 0.4),
        (x0 + u(1.2) + STROKE * 0.4, y0 + STROKE),
        (x1 - u(1.2) - STROKE * 0.4, y0 + STROKE),
        (x1 - STROKE, y0 + u(1.2) + STROKE * 0.4),
        (x1 - STROKE, ybody - STROKE * 0.2),
    ]
    x = x1 - STROKE
    for i in range(teeth * 2):
        x -= step
        inner.append((x, ybody - STROKE * 0.2 + (dn - STROKE if i % 2 == 0 else 0)))
    inner.append((x0 + STROKE, ybody - STROKE * 0.2))
    d.polygon(inner, fill=0)
    # ruled content lines
    lx0, lx1 = x0 + u(2.4), x1 - u(2.4)
    for yy, frac in ((u(6.4), 1.0), (u(9.2), 1.0), (u(12.0), 0.66)):
        capsule(d, lx0, yy, lx0 + (lx1 - lx0) * frac, yy, STROKE * 0.78)
    # total rule (heavier, shorter, right-aligned like a sum line)
    capsule(d, lx0 + (lx1 - lx0) * 0.40, u(15.6), lx1, u(15.6), STROKE * 0.9)
    return save(img, "ic_receipt.png")


def ic_shield():
    """Insurance shield: a proper heraldic silhouette (straight shoulders,
    tapered point) with a centred check mark punched through it."""
    img, d = new_canvas()
    cx = W / 2.0
    top, bot = u(2.6), u(21.4)
    hw = u(8.4)
    shoulder = u(7.2)
    steps = 34
    pts = [(cx - hw, top + u(1.4)), (cx - hw + u(1.0), top)]
    pts += [(cx + hw - u(1.0), top), (cx + hw, top + u(1.4)), (cx + hw, shoulder)]
    # right flank curving to the point
    for i in range(steps + 1):
        t = i / steps
        x = cx + hw * (1 - t * t * 0.98)
        y = shoulder + (bot - shoulder) * (t ** 0.78)
        pts.append((x, y))
    for i in range(steps, -1, -1):
        t = i / steps
        x = cx - hw * (1 - t * t * 0.98)
        y = shoulder + (bot - shoulder) * (t ** 0.78)
        pts.append((x, y))
    pts.append((cx - hw, shoulder))
    d.polygon(pts, fill=255)
    # check mark punched out
    polyline(
        d,
        [(cx - u(3.5), u(10.9)), (cx - u(0.9), u(13.6)), (cx + u(4.0), u(8.0))],
        STROKE * 1.30,
        0,
    )
    return save(img, "ic_shield.png")


def ic_last():
    """«چاپ آخرین قبض» — a document with a history dial (clock + counter-clock
    arrow) on it. Replaces the old printer-with-a-clock-in-the-corner mess."""
    img, d = new_canvas()
    # document behind, biased up-left; the whole composition is optically
    # centred on the art-board (doc bbox + dial bbox share one centre).
    dx0, dy0 = u(2.2), u(2.4)
    dx1, dy1 = u(14.8), u(18.0)
    rring(d, dx0, dy0, dx1, dy1, u(1.9), STROKE)
    for yy, frac in ((u(6.1), 1.0), (u(8.5), 1.0), (u(10.9), 0.58)):
        lx0, lx1 = dx0 + u(2.2), dx1 - u(2.2)
        capsule(d, lx0, yy, lx0 + (lx1 - lx0) * frac, yy, STROKE * 0.72)
    # history dial in front, bottom-right — punched halo so it reads clearly
    hcx, hcy, hr = u(16.2), u(16.2), u(5.4)
    disc(d, hcx, hcy, hr + STROKE * 1.05, 0)
    ring(d, hcx, hcy, hr, STROKE)
    # clock hands
    capsule(d, hcx, hcy, hcx, hcy - u(2.9), STROKE * 0.9)
    capsule(d, hcx, hcy, hcx + u(2.3), hcy + u(0.4), STROKE * 0.9)
    # counter-clockwise arrow head sitting on the ring's 9-o'clock point
    ax, ay = hcx - hr, hcy
    polyline(
        d,
        [(ax - u(1.45), ay - u(1.45)), (ax + u(0.1), ay + u(0.15)), (ax + u(1.6), ay - u(1.35))],
        STROKE * 0.9,
    )
    return save(img, "ic_last.png")


def main():
    made = [
        ic_settings(),
        ic_calc(),
        ic_printer(),
        ic_receipt(),
        ic_shield(),
        ic_last(),
    ]
    for p in made:
        print("wrote", os.path.relpath(p), os.path.getsize(p), "bytes")


if __name__ == "__main__":
    main()
