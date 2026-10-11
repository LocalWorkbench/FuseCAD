#!/usr/bin/env bash
# Live-preview loop: rebuild and relaunch the app whenever a source file changes.
# A failed build keeps the previous app running and prints the compiler errors.
#   scripts/watch.sh [file.ocd]
set -uo pipefail
cd "$(dirname "$0")/.."

PRESET="${OC_PRESET:-dev}"
[[ -d "build/$PRESET" ]] || cmake --preset "$PRESET"
case "$(uname -s)" in
    Darwin) APP="build/$PRESET/apps/opencircuit/opencircuit.app/Contents/MacOS/opencircuit" ;;
    *)      APP="build/$PRESET/apps/opencircuit/opencircuit" ;;
esac

pid=""
stop_app() { [[ -n "$pid" ]] && kill "$pid" 2>/dev/null && wait "$pid" 2>/dev/null; pid=""; }
trap 'stop_app; exit 0' INT TERM

rebuild_and_launch() {
    echo "── building ($(date +%T)) ──"
    if cmake --build --preset "$PRESET" --target opencircuit; then
        stop_app
        "$APP" "$@" &
        pid=$!
        echo "── running (pid $pid). Watching src/ and apps/ for changes; Ctrl-C to quit ──"
    else
        echo "── build failed; keeping the previous app running ──"
    fi
}

snapshot() {
    local files=(src apps cmake CMakeLists.txt)
    if [[ "$(uname -s)" == Darwin ]]; then
        find "${files[@]}" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.txt' -o -name '*.cmake' \) -exec stat -f '%m %N' {} +
    else
        find "${files[@]}" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.txt' -o -name '*.cmake' \) -printf '%T@ %p\n'
    fi
}

rebuild_and_launch "$@"
last="$(snapshot | sort)"
while true; do
    sleep 1
    now="$(snapshot | sort)"
    if [[ "$now" != "$last" ]]; then
        last="$now"
        sleep 0.3 # let editors finish writing
        rebuild_and_launch "$@"
    fi
done
