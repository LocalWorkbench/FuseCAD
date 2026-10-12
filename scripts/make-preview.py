#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the README hero image (docs/images/screenshot.png).

Usage:
    scripts/make-preview.py              capture the flange sample from build/dev, then compose
    scripts/make-preview.py shot.png     compose from an existing capture

The capture is the app's own --screenshot output, or a macOS window capture
(Cmd-Shift-4, Space, click; its transparent margin and shadow are trimmed).
The window gets rounded corners and a soft shadow, and floats on a blurred
gradient card in the brand colours, running off the bottom edge. Needs Pillow.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images" / "screenshot.png"
APP = ROOT / "build/dev/apps/fusecad/fusecad.app/Contents/MacOS/fusecad"
CLI = ROOT / "build/dev/apps/fusecad-cli/fusecad-cli"

CANVAS_W = 3200
WINDOW_FRACTION = 0.92  # window width as a share of the canvas width
WINDOW_TOP = 136
WINDOW_RADIUS = 26
CARD_RADIUS = 56
SHADOW_BLUR = 70
SHADOW_OFFSET = 44
SHADOW_ALPHA = 110
OUTPUT_WIDTH = 2000  # final PNG width; keeps the README image light


def capture(tmp: Path) -> Path:
    doc, shot = tmp / "flange.fcad", tmp / "shot.png"
    subprocess.run([CLI, "new", doc, "--sample", "flange"], check=True, capture_output=True)
    subprocess.run([APP, doc, "--screenshot", shot], check=True)
    return shot


def window_bbox(im: Image.Image):
    """Bounds of the fully opaque window, ignoring a capture's soft shadow."""
    return im.getchannel("A").point(lambda v: 255 if v > 250 else 0).getbbox()


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    return mask


def backdrop(size):
    """Soft gradient in the logo's indigo tones: painted small, blurred, scaled up."""
    w, h = size
    small = (200, round(200 * h / w))
    bg = Image.new("RGB", small, (150, 156, 214))
    d = ImageDraw.Draw(bg)
    sw, sh = small
    blobs = [
        # (cx, cy, rx, ry, colour) as fractions of the small canvas
        (0.10, 0.08, 0.45, 0.40, (214, 218, 248)),
        (0.55, 0.02, 0.42, 0.30, (60, 72, 190)),
        (0.96, 0.22, 0.30, 0.45, (196, 202, 246)),
        (0.30, 0.95, 0.55, 0.32, (222, 225, 250)),
        (0.86, 0.96, 0.40, 0.32, (230, 232, 252)),
        (0.02, 0.60, 0.22, 0.35, (110, 122, 222)),
    ]
    for cx, cy, rx, ry, colour in blobs:
        d.ellipse((sw * (cx - rx), sh * (cy - ry), sw * (cx + rx), sh * (cy + ry)), fill=colour)
    bg = bg.filter(ImageFilter.GaussianBlur(28))
    return bg.resize(size, Image.BICUBIC).convert("RGBA")


def compose(src: Path):
    shot = Image.open(src).convert("RGBA")
    box = window_bbox(shot)
    if box is None:
        sys.exit(f"{src}: no opaque region found")
    window = shot.crop(box)

    target_w = round(CANVAS_W * WINDOW_FRACTION)
    window = window.resize((target_w, round(window.height * target_w / window.width)), Image.LANCZOS)
    window.putalpha(rounded_mask(window.size, WINDOW_RADIUS))

    # The card ends at the window's bottom edge, so the whole UI stays visible
    # while the window still reads as running off the frame.
    canvas = backdrop((CANVAS_W, WINDOW_TOP + window.height))
    x, y = (CANVAS_W - window.width) // 2, WINDOW_TOP

    pad = SHADOW_BLUR * 3
    shadow = Image.new("RGBA", (window.width + 2 * pad, window.height + 2 * pad), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (pad, pad, pad + window.width - 1, pad + window.height - 1), WINDOW_RADIUS, fill=(20, 24, 70, SHADOW_ALPHA)
    )
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(SHADOW_BLUR)), (x - pad, y - pad + SHADOW_OFFSET))
    canvas.alpha_composite(window, (x, y))
    canvas.putalpha(rounded_mask(canvas.size, CARD_RADIUS))

    canvas = canvas.resize((OUTPUT_WIDTH, round(canvas.height * OUTPUT_WIDTH / canvas.width)), Image.LANCZOS)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(OUT, optimize=True)
    print(f"wrote {OUT.relative_to(ROOT)} ({canvas.width}x{canvas.height})")


def main():
    if len(sys.argv) > 1:
        compose(Path(sys.argv[1]))
        return
    with tempfile.TemporaryDirectory() as tmp:
        compose(capture(Path(tmp)))


if __name__ == "__main__":
    main()
