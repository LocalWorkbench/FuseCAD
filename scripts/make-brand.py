# SPDX-License-Identifier: GPL-3.0-or-later
# Generate the logo SVGs in docs/brand: mark, wordmarks (text as outlines) and app icon.
# Usage: python3 scripts/make-brand.py <dir with InterDisplay-Bold.ttf and -Regular.ttf> docs/brand
# Needs: pip install fonttools uharfbuzz. Inter: https://github.com/rsms/inter (SIL OFL).
# Then run scripts/make-icons.sh to rebuild the app icons.
import math, sys, uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
FONTS = sys.argv[1]; OUT = sys.argv[2]

# ---- mark: three folded faces forming a hexagon ring ----
def hexpts(R, cx, cy):
    return [(cx+R*math.cos(math.radians(-90+60*k)), cy+R*math.sin(math.radians(-90+60*k))) for k in range(6)]
def isect(p, d, a, b):
    ex, ey = b[0]-a[0], b[1]-a[1]
    t = ((a[0]-p[0])*ey - (a[1]-p[1])*ex)/(d[0]*ey - d[1]*ex)
    return (p[0]+t*d[0], p[1]+t*d[1])
def mark(cx, cy, R, tones, ratio=0.41, rounding=None):
    O, I = hexpts(R, cx, cy), hexpts(R*ratio, cx, cy)
    Q = {j: isect(I[j], (O[j][0]-O[j-1][0], O[j][1]-O[j-1][1]), O[j], O[(j+1)%6]) for j in (0,2,4)}
    sw = rounding if rounding is not None else R*0.05
    out = []
    for n, j in enumerate((0,2,4)):
        k = (j+2)%6
        pts = [Q[j], O[(j+1)%6], O[(j+2)%6], Q[k], I[k], I[(k-1)%6], I[j]]
        d = "M" + " L".join(f"{x:.2f} {y:.2f}" for x, y in pts) + " Z"
        out.append(f'<path d="{d}" fill="{tones[n]}" stroke="{tones[n]}" stroke-width="{sw:.2f}" stroke-linejoin="round"/>')
    return "".join(out)

TONES = ["#7b88ff", "#4b59ee", "#2a35a6"]  # right, bottom, top-left

# ---- text to outlines ----
def text_path(fontfile, text, x, baseline, size, fill):
    blob = hb.Blob.from_file_path(fontfile); face = hb.Face(blob); font = hb.Font(face)
    buf = hb.Buffer(); buf.add_str(text); buf.guess_segment_properties(); hb.shape(font, buf, {"kern": True, "liga": True})
    tt = TTFont(fontfile); gs = tt.getGlyphSet(); upem = tt["head"].unitsPerEm; order = tt.getGlyphOrder()
    s = size/upem; pen = SVGPathPen(gs); cx = 0
    for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
        g = order[info.codepoint]
        gs[g].draw(TransformPen(pen, (s, 0, 0, -s, x + (cx+pos.x_offset)*s, baseline - pos.y_offset*s)))
        cx += pos.x_advance
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>', x + cx*s

def svg(w, h, body, title="FuseCAD"):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}"><title>{title}</title>{body}</svg>\n'

bold, reg = f"{FONTS}/InterDisplay-Bold.ttf", f"{FONTS}/InterDisplay-Regular.ttf"

# mark alone
open(f"{OUT}/mark.svg","w").write(svg(256, 256, mark(128, 128, 116, TONES)))

# wordmarks
for theme, ink in (("light", "#141833"), ("dark", "#f3f4fb")):
    H = 160; size = 96; base = 114
    m = mark(76, 80, 62, TONES)
    p1, x = text_path(bold, "Fuse", 164, base, size, ink)
    p2, x = text_path(reg, "CAD", x + 4, base, size, ink)
    open(f"{OUT}/wordmark-{theme}.svg","w").write(svg(round(x + 12), H, m + p1 + p2))

# app icon: mark on a soft light tile
tile = '<rect x="100" y="100" width="824" height="824" rx="185" fill="#f4f5fb"/>'
open(f"{OUT}/icon.svg","w").write(svg(1024, 1024, tile + mark(512, 512, 300, TONES)))
