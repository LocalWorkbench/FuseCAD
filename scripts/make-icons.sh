#!/usr/bin/env bash
# Regenerate the app icons from docs/brand/icon.svg. Run after editing the logo.
# Needs rsvg-convert (brew install librsvg) and, for the .icns, macOS iconutil.
set -euo pipefail
cd "$(dirname "$0")/.."

SVG=docs/brand/icon.svg
OUT=apps/fusecad/resources
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

rsvg-convert -w 256 "$SVG" -o "$OUT/fusecad.png"
rsvg-convert -w 1024 "$SVG" -o docs/brand/icon-1024.png

if command -v iconutil >/dev/null; then
    SET="$TMP/fusecad.iconset"
    mkdir "$SET"
    for s in 16 32 128 256 512; do
        rsvg-convert -w "$s" "$SVG" -o "$SET/icon_${s}x${s}.png"
        rsvg-convert -w "$((s * 2))" "$SVG" -o "$SET/icon_${s}x${s}@2x.png"
    done
    iconutil -c icns "$SET" -o "$OUT/fusecad.icns"
fi
echo "Icons written to $OUT"
