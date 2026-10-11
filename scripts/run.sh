#!/usr/bin/env bash
# Build (incrementally) and launch the desktop app.
#   scripts/run.sh [file.ocd]
set -euo pipefail
cd "$(dirname "$0")/.."

PRESET="${OC_PRESET:-dev}"
[[ -d "build/$PRESET" ]] || cmake --preset "$PRESET"
cmake --build --preset "$PRESET"

case "$(uname -s)" in
    Darwin) APP="build/$PRESET/apps/opencircuit/opencircuit.app/Contents/MacOS/opencircuit" ;;
    *)      APP="build/$PRESET/apps/opencircuit/opencircuit" ;;
esac
exec "$APP" "$@"
