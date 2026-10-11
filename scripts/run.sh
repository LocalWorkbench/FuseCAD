#!/usr/bin/env bash
# Build (incrementally) and launch the desktop app.
#   scripts/run.sh [file.fcad]
set -euo pipefail
cd "$(dirname "$0")/.."

PRESET="${FUSE_PRESET:-dev}"
[[ -d "build/$PRESET" ]] || cmake --preset "$PRESET"
cmake --build --preset "$PRESET"

case "$(uname -s)" in
    Darwin) APP="build/$PRESET/apps/fusecad/fusecad.app/Contents/MacOS/fusecad" ;;
    *)      APP="build/$PRESET/apps/fusecad/fusecad" ;;
esac
exec "$APP" "$@"
