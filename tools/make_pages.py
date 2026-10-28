#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the NACHTFAHRT site for GitHub Pages (https://fartypants.github.io/fm1-nachtfahrt/):

  index.html                  what NACHTFAHRT is, and the ways in: try, install, manual, download, source
  img/                        screenshots (docs/img)
  emu/                        NACHTFAHRT in the browser (build/emu, from web/emu/build.sh)
  words/                      the screen reader's words files (web/words, from firmware/words.tsv)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  firmware/nachtfahrt-VERSION.fwsc   the package the installer writes; also the download

  tools/make_pages.py build/nachtfahrt-0.9.0.fwsc 0.9.0 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_8XXYYZZ, is what
the installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = "https://github.com/FartyPants/fm1-nachtfahrt"

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>NACHTFAHRT · Berlin School for the FM-1</title>
<meta name="description" content="A generative 1970s Berlin School instrument for the M-VAVE FM-1: two sequences drifting over a drone, a dreaming lead. Let it play for hours, or play the melody with it on the keys.">
<meta name="theme-color" content="#000000">
<style>
/* NACHTFAHRT's colours, as its screen: black, a pale lavender ink, cyan; the layers' four colours for the list */
:root { --ground: #000; --card: #0d0b14; --ink: #efe7ff; --muted: #a49cb8; --rule: #241f33; --acc: #7fd8ff;
        --drone: #b48cff; --seq: #ff6ad5; --lead: #ffd27f; --seq2: #7fd8ff; --atmos: #baae9c; color-scheme: dark; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 0 64px;
       font: 400 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
.banner { display: block; width: 100%; max-width: 1600px; height: auto; margin: 0 auto 1.6rem; }
main { max-width: 42rem; margin: 0 auto; }
h1 { font-size: 2.6rem; font-weight: 600; letter-spacing: .12em; margin: 0 0 .2rem; }
.sub { text-transform: uppercase; letter-spacing: .1em; font-size: .9rem; color: var(--acc); margin: 0 0 1.2rem;
       padding-bottom: .8rem; border-bottom: 1px solid var(--rule); }
h2 { font-size: 1.05rem; font-weight: 400; text-transform: uppercase; letter-spacing: .08em; margin: 2.4rem 0 .6rem;
     padding-top: .6rem; border-top: 1px solid var(--rule); color: var(--acc); }
.lede { font-size: 1.15rem; margin: 0 0 1.6rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 0 0 1.8rem; }
.shots img { width: 100%; height: auto; image-rendering: pixelated; border: 1px solid var(--rule); border-radius: 6px; }
.ways { display: grid; gap: 10px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--card); border: 1px solid var(--rule);
          border-radius: 6px; padding: 12px 16px; }
.ways a:first-child { border-color: var(--acc); }
.ways a:hover, .ways a:focus-visible { border-color: var(--acc); }
a:focus-visible { outline: 2px solid var(--acc); outline-offset: 2px; }
.ways strong { font-weight: 400; font-size: 1.1rem; text-transform: uppercase; letter-spacing: .06em; display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
.ways em.v { font-style: normal; text-transform: none; letter-spacing: 0; }
ul.layers { padding-left: 1.2rem; margin: 0 0 1rem; }
ul.layers li { margin: 0 0 .45rem; }
ul.layers li.d::marker { color: var(--drone); } ul.layers li.s::marker { color: var(--seq); }
ul.layers li.l::marker { color: var(--lead); } ul.layers li.t::marker { color: var(--seq2); }
ul.layers li.a::marker { color: var(--atmos); }
.mood { width: 100%; height: auto; border-radius: 6px; margin: .4rem 0 1rem; }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--acc); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .9em; }
</style>
</head>
<body>
<img class="banner" src="img/site/banner.jpg" width="1600" height="500" alt="">
<main>
<h1>NACHTFAHRT</h1>
<p class="sub">Berlin School for the M-VAVE FM-1 · by Cranky Man</p>
<p class="lede">A generative 1970s Berlin School instrument. Two sequencers drift against each other over a slow drone,
their filters breathing; a lead voice dreams above them. Let it play for hours, or play with it: the white keys give you
the melody, always in the piece's key, over the sequences and the drone, while the lead plays its own phrases around yours.</p>
<div class="shots">
  <img src="img/screen-launch.png" width="240" height="240" alt="The launch screen: NACHTFAHRT, by CRANKY MAN">
  <img src="img/screen-main.png" width="240" height="240" alt="The main screen: the chord's name among orbiting dots, the five layers' meters, the two sequences' lanes">
  <img src="img/screen-perform.png" width="240" height="240" alt="PERFORM: Filter, Wet, Energy and Sweep on the knobs">
</div>
<nav class="ways" aria-label="Get NACHTFAHRT">
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>From Chrome or Edge (also Chrome on Android), with the FM-1 connected by USB. Nothing to install.</span></a>
  <a href="emu/"><strong>Try it in the browser</strong><span>The same code the FM-1 runs, with sound. Mouse, touch or keyboard; no FM-1 needed.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Getting started in a few minutes, then everything else.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 tools/fm1_install.py __PKG__</code></span></a>
  <a href="__REPO__"><strong>Source</strong><span>GitHub, GPL-3.0. Based on DUB-1, itself based on AMB-1.</span></a>
</nav>

<h2>Listen, and play</h2>
<p>NACHTFAHRT plays itself from the moment it is switched on, and it never ends: each piece builds, opens into a
solo, drifts through a bridge and starts its long rise again. You are invited in. The <b>white keys</b> play the
melody in the piece's scale, so every note fits, and the lead makes room for you. The <b>black keys</b>
are effects while held: Rise and Close sweep every filter, Transpose lifts the sequences, Supernova, Twitter and more.
SELECT turns the level of the layer you are looking at; PERFORM (SEL) mutes and solos the layers and pushes Filter,
Wet and Energy.</p>

<h2>What it plays</h2>
<ul class="layers">
  <li class="d"><b>Drone.</b> Organ, strings, choir, saws or glass, through a slow phaser, with the root an octave below; its chords walk slowly.</li>
  <li class="s"><b>Sequence.</b> The arpeggio at the heart of it: a Moog-style line repeated by the echo, its filter wandering, rising over the piece.</li>
  <li class="t"><b>Sequence 2.</b> A second line, usually a step shorter, so the two drift apart and meet again; now and then it moves an octave.</li>
  <li class="l"><b>Lead.</b> A gliding solo voice far in the reverb, phrasing in calls and answers; or your own melody on the keys.</li>
  <li class="a"><b>Atmosphere.</b> Wind, surf, space or tape hiss, where a piece wants it.</li>
</ul>
<img class="mood" src="img/site/night-drive.jpg" width="1200" height="900" alt="">
<p>Eight pieces after the records of Tangerine Dream, Klaus Schulze and Ashra (Timewind, Wahnfried, Phaedra,
Rubycon, Stratosfear, Sunrain, Mirage, Cyborg), each remembering how you left it. Every layer has its sound pages and
a Move page, where its filter wanders relative to the knob you set.</p>
<p class="small">To go back to the official firmware, use the installer's "Back to the stock firmware", or
M-VAVE's updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
NACHTFAHRT is based on <a href="https://github.com/sbegueria/fm1-dub">DUB-1</a> by sbegueria, itself based on
<a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal; its granular cloud follows JIANT's;
the platform is Felucca by Leo Kuroshita (Hügelton Instruments). GPL-3.0. Pictures made with ChatGPT.
M-VAVE and FM-1 are trademarks of their owners. NACHTFAHRT is not affiliated with M-VAVE or Hügelton Instruments.</p>
</main>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_8\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"nachtfahrt-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"amb_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "amb.wasm").exists():
        raise SystemExit("no build/emu/amb.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "web" / "words", out / "words")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, words/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
