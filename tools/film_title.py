#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The launch screen as a video title: ui.c's splash_band drawn again at any size, in continuous time (the
device steps 26 frames of 55 ms), slowed and then held, with the screen's own fonts and colours.

  tools/film_title.py OUT.mp4 [--size 3840x2160] [--fps 60] [--slow 2.5] [--length 4] [--version 1.1.0]
                      [--still OUT.png]

The design is the 240 x 240 screen, centred and scaled to --fill of the frame's height (0.9) (the background, K_BG, fills
the rest). --slow stretches the animation (2.5: ~3.6 s), --length holds the last frame up to that."""
import argparse
import os
import subprocess

import numpy as np
from PIL import Image, ImageDraw, ImageFont

FONTS = os.path.join(os.path.dirname(__file__), "..", "assets", "fonts")
K_BG, K_TEXT, K_PAD, K_BELL = (16, 20, 26), (222, 228, 232), (84, 200, 186), (128, 170, 224)
K_BASS, K_PULSE, K_ACC, K_DIM = (204, 180, 112), (236, 150, 84), (92, 196, 204), (146, 160, 176)
# ui.c: the five "DUB-1"s: FONT_L, L, M, B, S (gen_font.py's barlow set), their cell heights
FACE = [("Bold", 31), ("Bold", 31), ("SemiBold", 23), ("Bold", 15), ("Medium", 15)]
FH = [32, 32, 24, 16, 16]
YC, STEP = 70, 0.055


def mix(a, b, t16):
    t = min(max(t16, 0.0), 16.0) / 16.0
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def frame(f, W, H, s, fonts, version):
    """the launch screen at (continuous) frame f, s pixels a screen pixel, centred"""
    im = Image.new("RGB", (W, H), K_BG)
    d = ImageDraw.Draw(im)
    ox, oy = (W - 240 * s) / 2, (H - 240 * s) / 2
    P = lambda x, y: (ox + x * s, oy + y * s)
    C = [K_TEXT, K_PAD, K_BELL, K_BASS, K_PULSE]
    for k in range(5):                           # the rings, one a strike
        age = f - (1 + k * 4)
        r = 12 + age * 8
        a = 1.0 - r / 140.0
        if age < 0 or a <= 0:
            continue
        c = mix(K_BG, K_PAD if k == 0 else C[k], 16 * a * (0.8 if k else 0.6))
        cx, cy = P(120, YC)
        lw = max(2, int(round(s * 0.7)))
        d.ellipse((cx - r * s, cy - r * s, cx + r * s, cy + r * s), outline=c, width=lw)
    for k in range(4, 0, -1):                    # the repeats, thrown out of the name, falling to their places
        age = f - (1 + k * 4)
        if age < 0:
            continue
        t = min(age / 3.0, 1.0)
        t = t * (2.0 - t)
        y = YC + (YC + 14 + k * 26 - YC) * t
        col = mix(K_BG, C[k], 16.0 * t - 2 * k)
        d.text(P(120, y), "DUB-1", font=fonts[k], fill=col, anchor="mm")
    if f >= 1:
        d.text(P(120, YC), "DUB-1", font=fonts[0], fill=mix(K_BG, K_TEXT, 16 if f >= 3 else 6 * f), anchor="mm")
    if f >= 20:
        d.text(P(120, 204 + 8), "Dub techno for the FM-1", font=fonts[5],
               fill=mix(K_BG, K_ACC, 16 if f >= 23 else (f - 19) * 4), anchor="mm")
    if f >= 22:
        d.text(P(120, 224 + 7.5), "DUB-1  " + version, font=fonts[6], fill=K_DIM, anchor="mm")
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--size", default="3840x2160")
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--slow", type=float, default=2.5)
    ap.add_argument("--length", type=float, default=4.0)
    ap.add_argument("--version", default="1.1.0")
    ap.add_argument("--fill", type=float, default=0.9, help="the screen's share of the frame's height")
    ap.add_argument("--still")
    a = ap.parse_args()
    W, H = (int(v) for v in a.size.split("x"))
    s = H * a.fill / 240.0
    fonts = [ImageFont.truetype(os.path.join(FONTS, f"BarlowSemiCondensed-{w}.ttf"), int(round(px * s)))
             for w, px in FACE]
    fonts.append(ImageFont.truetype(os.path.join(FONTS, "BarlowSemiCondensed-Medium.ttf"), int(round(15 * s))))
    fonts.append(ImageFont.truetype(os.path.join(FONTS, "BarlowSemiCondensed-SemiBold.ttf"), int(round(12 * s))))
    n = int(a.length * a.fps)
    ff = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", f"{W}x{H}", "-r", str(a.fps), "-i", "-", "-c:v", "libx264", "-preset", "slow",
                           "-crf", "12", "-pix_fmt", "yuv420p", "-movflags", "+faststart", a.out],
                          stdin=subprocess.PIPE)
    last = None
    for i in range(n):
        f = min(i / a.fps / (STEP * a.slow), 25.0)   # (the device's frames 0..25, the last held)
        im = frame(f, W, H, s, fonts, a.version)
        ff.stdin.write(im.tobytes())
        last = im
    ff.stdin.close()
    ff.wait()
    if a.still:
        last.save(a.still)
    print(f"title: {a.out}  {W}x{H}, {n} frames at {a.fps} fps ({a.length:.1f} s)")
    return ff.returncode


if __name__ == "__main__":
    raise SystemExit(main())
