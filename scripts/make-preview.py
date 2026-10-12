#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the README hero image (docs/images/screenshot.png).

Usage:
    scripts/make-preview.py              capture the pillow-block sample from build/dev, then compose
    scripts/make-preview.py shot.png     compose from an existing capture

The capture is the app's own --screenshot output, or a macOS window capture
(Cmd-Shift-4, Space, click; its transparent margin and shadow are trimmed).
The window gets rounded corners and a hairline border on a transparent
background, so it sits cleanly on both GitHub themes. Needs Pillow.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images" / "screenshot.png"
APP = ROOT / "build/dev/apps/fusecad/fusecad.app/Contents/MacOS/fusecad"
CLI = ROOT / "build/dev/apps/fusecad-cli/fusecad-cli"

WINDOW_RADIUS = 26  # at capture resolution
BORDER = (127, 127, 127, 90)  # neutral hairline, visible on light and dark pages
OUTPUT_WIDTH = 2000  # final PNG width; keeps the README image light


def capture(tmp: Path) -> Path:
    doc, shot = tmp / "Pillow Block.fcad", tmp / "shot.png"
    subprocess.run([CLI, "new", doc, "--sample", "pillow-block"], check=True, capture_output=True)
    subprocess.run([APP, doc, "--screenshot", shot], check=True)
    return shot


def window_bbox(im: Image.Image):
    """Bounds of the fully opaque window, ignoring a capture's soft shadow."""
    return im.getchannel("A").point(lambda v: 255 if v > 250 else 0).getbbox()


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    return mask


def compose(src: Path):
    shot = Image.open(src).convert("RGBA")
    box = window_bbox(shot)
    if box is None:
        sys.exit(f"{src}: no opaque region found")
    window = shot.crop(box)

    window.putalpha(rounded_mask(window.size, WINDOW_RADIUS))
    ImageDraw.Draw(window).rounded_rectangle(
        (0, 0, window.width - 1, window.height - 1), WINDOW_RADIUS, outline=BORDER, width=2
    )
    window = window.resize((OUTPUT_WIDTH, round(window.height * OUTPUT_WIDTH / window.width)), Image.LANCZOS)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    window.save(OUT, optimize=True)
    print(f"wrote {OUT.relative_to(ROOT)} ({window.width}x{window.height})")


def main():
    if len(sys.argv) > 1:
        compose(Path(sys.argv[1]))
        return
    with tempfile.TemporaryDirectory() as tmp:
        compose(capture(Path(tmp)))


if __name__ == "__main__":
    main()
