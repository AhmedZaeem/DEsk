#!/usr/bin/env python3
"""Android Studio's look in the drawer (Red Ronin): a netsuke -- the little carved
toggle a samurai's things hung from his belt by -- of jade, carved as the Android
head, lying on its back on the felt and seen from straight above, its silk cord
running off through a brass ojime bead to a tassel. Drawn in plain cairo (no
Blender here), lit as the drawer's lamp lights everything: from the upper right.

    python3 art/looks/android_netsuke.py OUTDIR

writes OUTDIR/look.png and OUTDIR/look.ini (docs/LOOKS.md), at 40 px a centimetre.
"""

import math
import os
import random
import sys

import cairo

PX = 40.0                      # pixels a centimetre
W, H = int(13.5 * PX), int(6.8 * PX)
LIGHT = (0.62, -0.78)          # toward the lamp: right, and up the picture


def jade_fill(cr, x0, y0, x1, y1, seed):
    """Polished jade: green, mottled, deeper in the shadow side."""
    g = cairo.LinearGradient(x1, y0, x0, y1)
    g.add_color_stop_rgb(0.0, 0.58, 0.82, 0.55)
    g.add_color_stop_rgb(0.45, 0.30, 0.60, 0.36)
    g.add_color_stop_rgb(1.0, 0.10, 0.30, 0.16)
    cr.set_source(g)
    cr.fill_preserve()
    # the stone's clouds: soft pale and dark patches
    rnd = random.Random(seed)
    cr.save()
    cr.clip()
    for _ in range(26):
        cx = rnd.uniform(x0, x1)
        cy = rnd.uniform(y0, y1)
        r = rnd.uniform(8, 34)
        pale = rnd.random() < 0.55
        rg = cairo.RadialGradient(cx, cy, 0, cx, cy, r)
        if pale:
            rg.add_color_stop_rgba(0, 0.80, 0.95, 0.78, 0.22)
        else:
            rg.add_color_stop_rgba(0, 0.04, 0.18, 0.08, 0.22)
        rg.add_color_stop_rgba(1, 0, 0, 0, 0)
        cr.set_source(rg)
        cr.arc(cx, cy, r, 0, 2 * math.pi)
        cr.fill()
    cr.restore()


def gloss(cr, cx, cy, rx, ry, a):
    rg = cairo.RadialGradient(cx, cy, 0, cx, cy, max(rx, ry))
    rg.add_color_stop_rgba(0, 1, 1, 0.96, a)
    rg.add_color_stop_rgba(1, 1, 1, 1, 0)
    cr.save()
    cr.translate(cx, cy)
    cr.scale(rx / max(rx, ry), ry / max(rx, ry))
    cr.translate(-cx, -cy)
    cr.set_source(rg)
    cr.arc(cx, cy, max(rx, ry), 0, 2 * math.pi)
    cr.fill()
    cr.restore()


def head(cr, cx, cy, r):
    """The Android head: a dome on a flat chin, two antennae, two eyes."""
    # antennae first, under the dome's edge
    for s in (-1, 1):
        a = math.radians(-90 + s * 38)
        bx, by = cx + math.cos(a) * r * 0.80, cy + math.sin(a) * r * 0.80
        tx, ty = cx + math.cos(a) * r * 1.30, cy + math.sin(a) * r * 1.30
        cr.set_line_cap(cairo.LINE_CAP_ROUND)
        cr.set_line_width(r * 0.13)
        cr.move_to(bx, by)
        cr.line_to(tx, ty)
        cr.set_source_rgb(0.18, 0.44, 0.24)
        cr.stroke_preserve()
        cr.set_line_width(r * 0.07)
        cr.set_source_rgba(0.75, 0.95, 0.72, 0.45)
        cr.stroke()
    # the dome, and a thick chin under it, carved as one stone
    cr.new_path()
    rr, chin = r * 0.18, cy + r * 0.52
    cr.arc(cx, cy, r, math.pi, 2 * math.pi)
    cr.line_to(cx + r, chin - rr)
    cr.arc(cx + r - rr, chin - rr, rr, 0, math.pi / 2)
    cr.line_to(cx - r + rr, chin)
    cr.arc(cx - r + rr, chin - rr, rr, math.pi / 2, math.pi)
    cr.close_path()
    path = cr.copy_path()
    jade_fill(cr, cx - r, cy - r, cx + r, cy + r * 0.52, 7)
    cr.new_path()
    cr.append_path(path)
    cr.set_line_width(2.0)
    cr.set_source_rgba(0.04, 0.16, 0.07, 0.75)
    cr.stroke()
    # the carver's line where dome meets chin
    cr.move_to(cx - r * 0.98, cy + r * 0.02)
    cr.line_to(cx + r * 0.98, cy + r * 0.02)
    cr.set_line_width(2.2)
    cr.set_source_rgba(0.05, 0.20, 0.09, 0.55)
    cr.stroke()
    cr.move_to(cx - r * 0.98, cy + r * 0.02 - 1.4)
    cr.line_to(cx + r * 0.98, cy + r * 0.02 - 1.4)
    cr.set_line_width(0.9)
    cr.set_source_rgba(0.85, 1.0, 0.85, 0.35)
    cr.stroke()
    # eyes: inlaid black lacquer, a glint each
    for s in (-1, 1):
        ex, ey = cx + s * r * 0.40, cy - r * 0.40
        cr.arc(ex, ey, r * 0.105, 0, 2 * math.pi)
        cr.set_source_rgb(0.02, 0.02, 0.02)
        cr.fill()
        cr.arc(ex + r * 0.035, ey - r * 0.035, r * 0.03, 0, 2 * math.pi)
        cr.set_source_rgba(1, 1, 1, 0.75)
        cr.fill()
    # the polish: the lamp in the dome, a line of light along its rim
    gloss(cr, cx + r * 0.38, cy - r * 0.58, r * 0.34, r * 0.20, 0.55)
    cr.new_path()
    cr.arc(cx, cy, r * 0.93, math.radians(-75), math.radians(-15))
    cr.set_line_width(2.4)
    cr.set_source_rgba(1, 1, 0.95, 0.35)
    cr.stroke()


def cord(cr, pts, width):
    """Braided red silk: a dark core, the twist of the braid along it."""
    cr.set_line_cap(cairo.LINE_CAP_ROUND)
    cr.set_line_join(cairo.LINE_JOIN_ROUND)

    def path():
        cr.move_to(*pts[0])
        for i in range(1, len(pts) - 2, 3):
            cr.curve_to(*pts[i], *pts[i + 1], *pts[i + 2])

    path()
    cr.set_line_width(width)
    cr.set_source_rgb(0.36, 0.03, 0.03)
    cr.stroke()
    path()
    cr.set_line_width(width * 0.66)
    cr.set_source_rgb(0.62, 0.07, 0.06)
    cr.stroke()
    path()
    cr.set_line_width(width * 0.66)
    cr.set_dash([width * 0.35, width * 0.45])
    cr.set_source_rgba(0.92, 0.30, 0.22, 0.55)
    cr.stroke()
    cr.set_dash([])


def ojime(cr, cx, cy, r):
    """The bead the cord runs through: old brass."""
    rg = cairo.RadialGradient(cx + r * 0.35, cy - r * 0.4, r * 0.1, cx, cy, r)
    rg.add_color_stop_rgb(0, 0.98, 0.86, 0.55)
    rg.add_color_stop_rgb(0.5, 0.66, 0.48, 0.20)
    rg.add_color_stop_rgb(1, 0.22, 0.14, 0.05)
    cr.arc(cx, cy, r, 0, 2 * math.pi)
    cr.set_source(rg)
    cr.fill()
    for k in range(5):
        a = math.radians(-60 + k * 30)
        cr.arc(cx, cy, r * 0.75, a, a + 0.18)
        cr.set_line_width(1.0)
        cr.set_source_rgba(0.15, 0.08, 0.02, 0.5)
        cr.stroke()


def tassel(cr, x, y, length, spread):
    rnd = random.Random(3)
    cr.set_line_cap(cairo.LINE_CAP_ROUND)
    for i in range(34):
        t = i / 33.0
        ang = (t - 0.5) * spread
        ex = x + math.cos(ang) * length * rnd.uniform(0.86, 1.0)
        ey = y + math.sin(ang) * length * rnd.uniform(0.86, 1.0)
        cr.move_to(x, y)
        cr.curve_to(x + length * 0.3, y, ex - length * 0.3, ey, ex, ey)
        cr.set_line_width(1.3)
        shade = 0.45 + 0.35 * rnd.random()
        cr.set_source_rgba(0.75 * shade + 0.2, 0.06, 0.05, 0.85)
        cr.stroke()
    # the binding round its top
    cr.rectangle(x - 3, y - 7, 9, 14)
    cr.set_source_rgb(0.80, 0.62, 0.25)
    cr.fill()


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    s = cairo.ImageSurface(cairo.FORMAT_ARGB32, W, H)
    cr = cairo.Context(s)
    hx, hy, hr = 3.3 * PX, 3.85 * PX, 2.35 * PX
    # the cord first: out of the hole under the chin, through the bead, to the tassel
    cord(cr, [(hx + hr * 0.45, hy + hr * 0.40), (hx + hr * 1.1, hy + hr * 0.95),
              (6.2 * PX, 5.2 * PX), (7.4 * PX, 4.6 * PX),
              (8.4 * PX, 4.1 * PX), (9.0 * PX, 3.3 * PX), (10.0 * PX, 3.1 * PX)], 0.32 * PX)
    ojime(cr, 7.4 * PX, 4.6 * PX, 0.48 * PX)
    tassel(cr, 10.0 * PX, 3.1 * PX, 3.0 * PX, 0.9)
    head(cr, hx, hy, hr)
    s.write_to_png(os.path.join(out, "look.png"))
    with open(os.path.join(out, "look.ini"), "w", encoding="utf-8") as f:
        f.write("[Look]\nName=Android netsuke\nName[ja]=アンドロイドの根付\n"
                "Size=20\nThick=2.4\nOrder=20\nAuthor=Red Ronin\n"
                "Comment=Android Studio as a jade netsuke (art/looks/android_netsuke.py)\n")
    print("wrote", out)


if __name__ == "__main__":
    main()
