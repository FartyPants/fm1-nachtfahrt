#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A video of the simulated FM-1: the host simulator's film (amb_host's `film`) and its WAV, drawn as the
FM-1's panel (the web emulator's drawing, web/emu/index.html) with the screen, the lights and the buttons and
keys as they are pressed, encoded with ffmpeg.

  tools/film.py FILM WAV OUT.mp4 [--layout split|panel|screen|vertical|center] [--fps 30]
  tools/film.py FILM - OUT.mp4 ...        (WAV "-" or --no-audio: the video alone, to mux later)

Layouts: split (1920x1080: the panel on the left, the screen large on the right), panel (1920x1080: the
panel alone), screen (1080x1080: the screen alone, 4x), vertical (1080x1920, a short: the screen large on top,
the panel under it), center (1920x1080: the screen alone, 4x, in the middle, a faint glow around it; for the
long listening videos). --captions FILE.tsv (seconds<TAB>text): a caption in the lower left, each for 8 s,
fading in and out. FILM may be a named pipe (amb_host writing to it as it runs: nothing on disk). Each press of a button or key, and each turn of a knob, sends a short teal
ripple out from it (in the panel framings). Needs Pillow, numpy and ffmpeg."""
import argparse
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

NE = 7
HDR = 6 + NE
FRAME = HDR * 4 + 240 * 240 * 2

BTN = {"FX": 0, "SEL": 1, "ENV": 2, "LFO": 3, "EDIT": 4, "GLO": 5, "HOME": 6, "SAVE": 7, "ARP": 8, "SEQ": 9,
       "PLAY": 10, "REC": 11, "OCT-": 12, "OCT+": 13}
ENC = {"SELECT": 0, "ALGORITHM": 1, "PRESETS": 2, "KNOB1": 3, "KNOB2": 4, "KNOB3": 5, "KNOB4": 6}
WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]
BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
BODY, TRAY, KEY_W, KEY_B, KNOB, BTNC, LABEL = "#3a3d42", "#26282c", "#d9dadc", "#c9cacd", "#1f2124", "#2e3135", "#e8e8ea"
LIT_BTN, LIT_TXT, LIT_KW, LIT_KB, HELD = "#f28a5a", "#2a140a", "#f6b48e", "#4cc3b6", "#f28a5a"
GROUND = (13, 16, 20)
RIP_C, RIP_FR = (92, 196, 204), 20            # a press's ripple: the screen's teal (K_ACC), 2/3 s at 30 fps
FONT = os.path.join(os.path.dirname(__file__), "..", "assets", "fonts", "BarlowSemiCondensed-SemiBold.ttf")
CAP_FONT = os.path.join(os.path.dirname(__file__), "..", "assets", "fonts", "BarlowSemiCondensed-Medium.ttf")
CAP_C, CAP_A, CAP_S, CAP_F = (222, 228, 232), 0.85, 10.0, 1.5   # a caption: light grey at 85%, 10 s, 1.5 s fades
SS = 2                                         # supersampling for the static drawing and the sprites


def font(px):
    return ImageFont.truetype(FONT, max(6, int(round(px))))


class Panel:
    """The panel in panel units (1000 x 640), drawn at s pixels a unit."""

    def __init__(self, s):
        self.s = s
        self.knobs = []                        # (x, y, r, role)
        self.buttons = []                      # (x, y, w, h, label, two)
        self.keys = {}                         # key index -> (x, y, w, h, white)

    def layout(self):
        def knob(x, y, r, label, role):
            self.knobs.append((x, y, r, label, role))
        knob(105, 100, 24, "MASTER", "master")
        knob(215, 100, 24, "SELECT", ENC["SELECT"])
        knob(105, 205, 24, "PRESETS", ENC["PRESETS"])
        knob(215, 205, 24, "ALGORITHM", ENC["ALGORITHM"])
        for i in range(4):
            knob(610 + i * 104, 100, 22, f"KNOB{i + 1}", ENC[f"KNOB{i + 1}"])
        self.buttons += [(72, 268, 82, 38, "OCT-", None), (170, 268, 82, 38, "OCT+", None)]
        row1, row2 = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]
        for i in range(6):
            self.buttons.append((582 + i * 63, 174, 54, 48, row1[i], None))
            self.buttons.append((582 + i * 63, 236, 54, 48, row2[i], "STOP" if row2[i] == "PLAY" else None))
        x0, y0, w, h = 36, 340, 928, 270
        pitch = (w - 24) / 16
        kw = pitch * 0.78
        for i in range(16):
            self.keys[WHITE_K[i]] = (x0 + 12 + i * pitch + (pitch - kw) / 2, y0 + h * 0.43, kw, h * 0.52, True)
        for k, a in enumerate(BLACK_AFTER):
            self.keys[BLACK_K[k]] = (x0 + 12 + (a + 1) * pitch - kw / 2, y0 + h * 0.06, kw, h * 0.33, False)

    def base(self):
        s = self.s * SS
        im = Image.new("RGB", (int(1000 * s), int(640 * s)), GROUND)
        d = ImageDraw.Draw(im)
        R = lambda x, y, w, h, r, **k: d.rounded_rectangle((x * s, y * s, (x + w) * s, (y + h) * s), r * s, **k)
        R(4, 4, 992, 632, 44, fill=BODY)
        R(58, 258, 210, 58, 14, fill=TRAY)
        R(300, 44, 236, 236, 26, fill="#111214")
        R(568, 160, 400, 140, 18, fill=TRAY)
        R(36, 340, 928, 270, 18, fill=TRAY)
        for x, y, r, label, _ in self.knobs:
            d.ellipse(((x - r - 5) * s, (y - r - 5) * s, (x + r + 5) * s, (y + r + 5) * s), fill=TRAY)
            d.ellipse(((x - r) * s, (y - r) * s, (x + r) * s, (y + r) * s), fill=KNOB, outline="#44474c", width=int(2 * s))
            d.text((x * s, (y - r - 12) * s), label, font=font(17 * s), fill=LABEL, anchor="ms")
        im = im.resize((int(1000 * self.s), int(640 * self.s)), Image.LANCZOS)
        for b in self.buttons:
            self.paste(im, *self.button_sprite(b, False, False))
        for k in self.keys:
            self.paste(im, *self.key_sprite(k, False, False))
        return im

    @staticmethod
    def paste(im, sp, xy):
        im.paste(sp, xy, sp)

    def sprite(self, x, y, w, h, draw):
        """A control drawn on its own, supersampled: (RGBA image, top-left on the panel image)."""
        s, pad = self.s, 4
        X, Y = int(x * s) - pad, int(y * s) - pad
        W, H = int((x + w) * s) + pad - X + 1, int((y + h) * s) + pad - Y + 1
        im = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
        draw(ImageDraw.Draw(im), lambda u, v: ((u * s - X) * SS, (v * s - Y) * SS), s * SS)
        return im.resize((W, H), Image.LANCZOS), (X, Y)

    def button_sprite(self, b, lit, held):
        x, y, w, h, label, two = b

        def draw(d, P, s):
            stroke, sw = (HELD, 4) if held else ("#4a4d52", 1.5)
            d.rounded_rectangle((*P(x, y), *P(x + w, y + h)), 9 * s, fill=LIT_BTN if lit else BTNC,
                                outline=stroke, width=max(1, int(sw * s)))
            c = LIT_TXT if lit else LABEL
            if two:
                d.text(P(x + w / 2, y + h / 2 - 1), label, font=font(12 * s), fill=c, anchor="ms")
                d.text(P(x + w / 2, y + h / 2 + 12), two, font=font(12 * s), fill=c, anchor="ms")
            else:
                d.text(P(x + w / 2, y + h / 2 + 5), label, font=font(15 * s), fill=c, anchor="ms")
        return self.sprite(x, y, w, h, draw)

    def key_sprite(self, k, lit, held):
        x, y, w, h, white = self.keys[k]

        def draw(d, P, s):
            fill = (LIT_KW if white else LIT_KB) if lit else (KEY_W if white else KEY_B)
            if held:                           # pressed: the outline, and a little darker
                fill = mix(fill, "#000000", 0.18)
            d.rounded_rectangle((*P(x, y), *P(x + w, y + h)), w / 2 * s, fill=fill,
                                outline=HELD if held else None, width=int(4 * s) if held else 0)
        return self.sprite(x, y, w, h, draw)


def mix(a, b, t):
    a, b = [tuple(int(c[i:i + 2], 16) for i in (1, 3, 5)) for c in (a, b)]
    return "#%02x%02x%02x" % tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def lcd(scrn, sz):
    """the screen as a close-up of the real one (--look lcd): each pixel a cell with a thin dark gap, a faint
    glow over it; a whole number of cells a pixel, so nothing is smoothed away"""
    k = max(1, sz // 240)
    a = np.asarray(scrn.resize((240 * k, 240 * k), Image.NEAREST)).astype(np.float32)
    m = LCD_MASK.get(k)
    if m is None:
        m = np.ones((240 * k, 240 * k, 1), np.float32)
        g = max(1, k // 4)                       # (the gap: a quarter of a cell)
        for j in range(g):
            m[k - 1 - j::k, :, :] *= 0.55
            m[:, k - 1 - j::k, :] *= 0.55
        LCD_MASK[k] = m
    im = Image.fromarray(np.clip(a * m, 0, 255).astype(np.uint8))
    glow = scrn.resize((240 * k, 240 * k), Image.NEAREST).filter(ImageFilter.GaussianBlur(0.75 * k))
    im = Image.blend(im, glow, 0.18)
    return im if im.size[0] == sz else im.resize((sz, sz), Image.LANCZOS)


LCD_MASK = {}


def glow_ground(W, H, scr, k):
    """the ground for the center layout: a soft, faint glow around the screen, as of its backlight"""
    x, y, sz = scr
    lay = Image.new("L", (W, H), 0)
    ImageDraw.Draw(lay).rectangle((x - 6 * k, y - 6 * k, x + sz + 6 * k, y + sz + 6 * k), fill=255)
    lay = lay.filter(ImageFilter.GaussianBlur(36 * k))
    g = np.asarray(lay).astype(np.float32)[..., None] / 255.0 * 0.10
    base = np.array(GROUND, np.float32) * (1 - g) + np.array(RIP_C, np.float32) * g
    return base.astype(np.uint8)


def load_captions(path, W, H, k):
    """--captions: (start s, end s, x, y, premultiplied rgb float array, alpha float array) a caption, each drawn
    once with a soft shadow, in the lower left. "4:15 · Pressure" (a " · " in it) is drawn as a title: the first
    part small and grey, the second large below it"""
    out = []
    small = ImageFont.truetype(CAP_FONT, 40 * k)
    big = ImageFont.truetype(os.path.join(os.path.dirname(CAP_FONT), "BarlowSemiCondensed-Bold.ttf"), 84 * k)
    for line in open(path, encoding="utf-8"):
        if not line.strip() or line.startswith("#"):
            continue
        t, text = line.rstrip("\n").split("\t", 1)
        t = float(t)
        parts = text.split(" \u00b7 ", 1)
        lines = [(parts[0], small, 0.7), (parts[1], big, 1.0)] if len(parts) == 2 else [(text, small, 1.0)]
        pad = 28 * k
        gap = 10 * k
        boxes = [f.getbbox(s_) for s_, f, _ in lines]
        w = max(b_[2] for b_ in boxes) + 2 * pad
        h = sum(b_[3] for b_ in boxes) + gap * (len(lines) - 1) + 2 * pad
        sh = Image.new("L", (w, h), 0)
        tx = Image.new("L", (w, h), 0)
        lum = Image.new("L", (w, h), 0)              # (each line's brightness: the time dimmer)
        y = pad
        for (s_, f, v), b_ in zip(lines, boxes):
            ImageDraw.Draw(sh).text((pad + 3 * k, y + 4 * k), s_, font=f, fill=255)
            ImageDraw.Draw(tx).text((pad, y), s_, font=f, fill=255)
            ImageDraw.Draw(lum).text((pad, y), s_, font=f, fill=int(255 * v))
            y += b_[3] + gap
        sh = np.asarray(sh.filter(ImageFilter.GaussianBlur(7 * k))).astype(np.float32) / 255.0 * 0.75
        tx = np.asarray(tx).astype(np.float32) / 255.0 * CAP_A
        lum = np.asarray(lum).astype(np.float32) / 255.0
        a = tx + sh * (1 - tx)                   # the text over its shadow (black)
        rgb = (lum * CAP_A)[..., None] * np.array(CAP_C, np.float32)   # (premultiplied: the time line dimmer)
        X, Y = 64 * k - pad, H - 60 * k - h + pad
        out.append((t, t + CAP_S, X, Y, rgb, a))
    return out


def caption_on(frame, caps, sec):
    for t0, t1, X, Y, rgb, a in caps:
        if t0 <= sec < t1:
            v = min(1.0, (sec - t0) / CAP_F, (t1 - sec) / CAP_F)
            h, w = a.shape
            reg = frame[Y:Y + h, X:X + w].astype(np.float32)
            frame[Y:Y + h, X:X + w] = (reg * (1 - a[..., None] * v) + rgb * v).astype(np.uint8)


def center_screen(job):
    """(a worker) one frame's screen, drawn large: its bytes"""
    raw, sz, look = job
    scrn = screen_rgb(raw)
    return (lcd(scrn, sz) if look == "lcd" else scrn.resize((sz, sz), Image.NEAREST)).tobytes()


def run_center(a, ff, ground, scr, caps):
    """the center layout: the frames read in order (FILM may be a pipe being written), their screens drawn by a
    pool of processes, at most a window of them ahead (so a fast writer is not read into memory), each put on
    the precomputed ground with its caption and sent to ffmpeg in order"""
    from collections import deque
    from concurrent.futures import ProcessPoolExecutor
    x, y, sz = scr
    nw = max(2, min(8, (os.cpu_count() or 4) // 2))
    q, n = deque(), 0
    with open(a.film, "rb") as f, ProcessPoolExecutor(nw) as pool:
        def put(buf):
            nonlocal n
            fr = ground.copy()
            fr[y:y + sz, x:x + sz] = np.frombuffer(buf, np.uint8).reshape(sz, sz, 3)
            if caps:
                caption_on(fr, caps, n / a.fps)
            ff.stdin.write(fr.tobytes())
            n += 1
        while True:
            rec = f.read(FRAME)
            if len(rec) < FRAME:
                break
            q.append(pool.submit(center_screen, (rec[HDR * 4:], sz, a.look)))
            if len(q) >= 4 * nw:
                put(q.popleft().result())
        while q:
            put(q.popleft().result())
    return n


def screen_rgb(raw):
    v = np.frombuffer(raw, dtype=">u2").reshape(240, 240).astype(np.uint32)
    rgb = np.stack([(v >> 11 & 31) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31], -1)
    return Image.fromarray(rgb.astype(np.uint8))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("film")
    ap.add_argument("wav")
    ap.add_argument("out")
    ap.add_argument("--layout", default="split", choices=["split", "panel", "screen", "vertical", "center"])
    ap.add_argument("--no-audio", action="store_true", help="the video alone (as WAV '-'), to mux later")
    ap.add_argument("--captions", help="FILE.tsv: seconds<TAB>text, a caption in the lower left")
    ap.add_argument("--crf", type=int, default=14)
    ap.add_argument("--preset", default="medium")
    ap.add_argument("--fps", type=int, default=30)
    ap.add_argument("--scale", type=int, default=1, choices=[1, 2], help="2: 4K (3840x2160; the screen layout 2160x2160)")
    ap.add_argument("--look", default="lcd", choices=["lcd", "pixels"], help="the large screen: lcd (a close-up, the default) or plain pixels")
    a = ap.parse_args()

    k = a.scale                                # (2: 4K, the same layout doubled, the screen 8x)
    audio = not a.no_audio and a.wav != "-"
    if a.layout == "center":                   # the screen alone, 4x, in the middle of 1920x1080
        W, H, ps, off = 1920 * k, 1080 * k, None, None
        scr = (480 * k, 60 * k, 960 * k)
    elif a.layout == "screen":
        W, H, ps, scr, off = 1080 * k, 1080 * k, None, (60 * k, 60 * k, 960 * k), None
    elif a.layout == "vertical":               # a short: the screen (3x) on top, the panel under it, centred; clear of the
        W, H, ps = 1080 * k, 1920 * k, 0.9 * k  # Shorts' own overlays (the bottom fifth, the right edge's buttons)
        scr = (180 * k, 170 * k, 720 * k)
        off = (90 * k, 930 * k)                 # (centred, as the screen)
    elif a.layout == "panel":
        W, H, ps = 1920 * k, 1080 * k, 1.8 * k
        off = ((W - int(1000 * ps)) // 2, (H - int(640 * ps)) // 2)
        scr = (off[0] + int(318 * ps), off[1] + int(62 * ps), int(200 * ps))
    else:
        W, H, ps = 1920 * k, 1080 * k, 0.85 * k
        off = (30 * k, (H - int(640 * ps)) // 2)
        scr = (W - 1000 * k, 60 * k, 960 * k)  # 4x (8x at 4K), on the right
    canvas = Image.new("RGB", (W, H), GROUND)
    ground = glow_ground(W, H, scr, k) if a.layout == "center" else None
    caps = load_captions(a.captions, W, H, k) if a.captions else []
    if ps:
        P = Panel(ps)
        P.layout()
        base = P.base()
        canvas.paste(base, off)
        bsprites, ksprites = {}, {}
    ff = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
                           "-s", f"{W}x{H}", "-r", str(a.fps), "-i", "-"] + (["-i", a.wav] if audio else []) +
                          ["-c:v", "libx264", "-preset", a.preset, "-crf", str(a.crf), "-pix_fmt", "yuv420p"] +
                          (["-c:a", "aac", "-b:a", "320k", "-shortest"] if audio else ["-an"]) +
                          ["-movflags", "+faststart", a.out], stdin=subprocess.PIPE)
    angle = {e: 0.0 for e in range(NE)}
    n = 0
    prev_b = prev_k = 0
    rip = []                                   # press ripples: (kind, x, y, w, h, first frame), panel units
    knob_last = {e: -99 for e in range(NE)}
    if ground is not None:                     # center: the screens drawn in parallel, a few frames ahead
        n = run_center(a, ff, ground, scr, caps)
        ff.stdin.close()
        ff.wait()
        print(f"film: {a.out}  {n} frames, {n / a.fps:.1f} s, {W}x{H}")
        return ff.returncode
    with open(a.film, "rb") as f:
        while True:
            rec = f.read(FRAME)
            if len(rec) < FRAME:
                break
            h = struct.unpack(f"<{HDR}I", rec[:HDR * 4])
            _, held_b, held_k, lit_b, lit_k, master = h[:6]
            turns = struct.unpack(f"<{NE}i", rec[24:HDR * 4])
            for e in range(NE):
                angle[e] += 18.0 * turns[e]
            scrn = screen_rgb(rec[HDR * 4:])
            im = canvas.copy()
            if ps:                             # a press: a ripple from the control (a knob: one per turn, ~1/3 s apart)
                for b in P.buttons:
                    bit = 1 << BTN[b[4]]
                    if held_b & bit and not prev_b & bit:
                        rip.append(("box", b[0], b[1], b[2], b[3], n))
                for k, (x, y, w, hh, _) in P.keys.items():
                    if held_k >> k & 1 and not prev_k >> k & 1:
                        rip.append(("box", x, y, w, hh, n))
                for x, y, r, label, role in P.knobs:
                    if role != "master" and turns[role] and n - knob_last[role] > 10:
                        rip.append(("ring", x - r, y - r, 2 * r, 2 * r, n))
                    if role != "master" and turns[role]:
                        knob_last[role] = n
                prev_b, prev_k = held_b, held_k
                rip = [q for q in rip if n - q[5] < RIP_FR]
            if ps:
                for b in P.buttons:
                    bit = BTN[b[4]]
                    st = (bool(lit_b >> bit & 1), bool(held_b >> bit & 1))
                    if st != (False, False):
                        key = (b[4], st)
                        if key not in bsprites:
                            bsprites[key] = P.button_sprite(b, *st)
                        sp, (x, y) = bsprites[key]
                        im.paste(sp, (x + off[0], y + off[1]), sp)
                for k in P.keys:
                    st = (bool(lit_k >> k & 1), bool(held_k >> k & 1))
                    if st != (False, False):
                        key = (k, st)
                        if key not in ksprites:
                            ksprites[key] = P.key_sprite(k, *st)
                        sp, (x, y) = ksprites[key]
                        im.paste(sp, (x + off[0], y + off[1]), sp)
                d = ImageDraw.Draw(im)
                for x, y, r, label, role in P.knobs:
                    ang = -135 + 270 * master / 4096 if role == "master" else angle[role]
                    t = np.radians(ang)
                    cx, cy = off[0] + x * ps, off[1] + y * ps
                    p0 = (cx + np.sin(t) * (r - 5) * ps, cy - np.cos(t) * (r - 5) * ps)
                    p1 = (cx + np.sin(t) * (r - 15) * ps, cy - np.cos(t) * (r - 15) * ps)
                    wd = max(2, int(3 * ps))
                    d.line((p0, p1), fill=LABEL, width=wd)
                    for q in (p0, p1):
                        d.ellipse((q[0] - wd / 2, q[1] - wd / 2, q[0] + wd / 2, q[1] + wd / 2), fill=LABEL)
                if rip:
                    lay = Image.new("RGBA", im.size, (0, 0, 0, 0))
                    dl = ImageDraw.Draw(lay)
                    for kind, x, y, w, hh, f0 in rip:
                        t = (n - f0) / RIP_FR
                        g = (4 + 26 * (t * (2 - t))) * ps          # (out fast, then slower)
                        al = int(255 * (1 - t) ** 1.5)
                        box = (off[0] + x * ps - g, off[1] + y * ps - g, off[0] + (x + w) * ps + g, off[1] + (y + hh) * ps + g)
                        wd = max(2, int(round(3.5 * ps * (1 - 0.5 * t))))
                        if kind == "ring":
                            dl.ellipse(box, outline=RIP_C + (al,), width=wd)
                        else:
                            rad = min(w, hh) * ps / 2 + g if w < 30 else 9 * ps + g
                            dl.rounded_rectangle(box, rad, outline=RIP_C + (al,), width=wd)
                    im = Image.alpha_composite(im.convert("RGBA"), lay).convert("RGB")
                    d = ImageDraw.Draw(im)
                if a.layout in ("split", "vertical"):   # the panel's own screen, small
                    sz = int(200 * ps)
                    im.paste(scrn.resize((sz, sz), Image.LANCZOS), (off[0] + int(318 * ps), off[1] + int(62 * ps)))
            x, y, sz = scr
            if a.look == "lcd":
                im.paste(lcd(scrn, sz), (x, y))
            else:
                im.paste(scrn.resize((sz, sz), Image.NEAREST if sz % 240 == 0 else Image.LANCZOS), (x, y))
            ff.stdin.write(im.tobytes())
            n += 1
    ff.stdin.close()
    ff.wait()
    print(f"film: {a.out}  {n} frames, {n / a.fps:.1f} s, {W}x{H}")
    return ff.returncode


if __name__ == "__main__":
    sys.exit(main())
