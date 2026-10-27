# SPDX-License-Identifier: GPL-3.0-only
# tools/gen_font.py for a Pillow without libraqm: OpenType features (tnum) are dropped, so digits
# lose their tabular widths. For local simulator builds only; release builds use gen_font.py as is.
#   python3 tools/gen_font_noraqm.py tools/gen_font.py build/gen/felucca_font.h
import sys, runpy
from PIL import ImageFont, ImageDraw
def strip(f):
    def g(*a, **k):
        k.pop("features", None); return f(*a, **k)
    return g
ImageFont.FreeTypeFont.getbbox = strip(ImageFont.FreeTypeFont.getbbox)
ImageFont.FreeTypeFont.getlength = strip(ImageFont.FreeTypeFont.getlength)
ImageDraw.ImageDraw.text = strip(ImageDraw.ImageDraw.text)
script = sys.argv[1]; sys.argv = sys.argv[1:]
sys.path.insert(0, script.rsplit("/", 1)[0])
runpy.run_path(script, run_name="__main__")
