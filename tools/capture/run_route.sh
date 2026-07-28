#!/usr/bin/env bash
# Run the canonical route (tools/capture/route.script) headless and collect
# waypoint dumps. Usage:
#   tools/capture/run_route.sh <output-dir> [path-to-tmc_pc]
#
# Runs in an isolated temp dir so the user's tmc.sav is never touched and
# the run always starts from blank EEPROM (the route creates its own file).
set -euo pipefail

OUT="$(realpath -m "${1:?usage: run_route.sh <output-dir> [tmc_pc]}")"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$(realpath "${2:-$REPO/build/pc/tmc_pc}")"

RUNDIR="$(mktemp -d)"
trap 'rm -rf "$RUNDIR"' EXIT
ln -s "$REPO/baserom.gba" "$RUNDIR/baserom.gba"
ln -s "$REPO/assets" "$RUNDIR/assets"
# no tmc.sav: blank EEPROM = deterministic new-game fixture

mkdir -p "$OUT"
cd "$RUNDIR"
SDL_VIDEODRIVER=dummy "$BIN" --no-audio --uncapped \
    --script="$REPO/tools/capture/route.script" \
    --dump-dir="$OUT" --exit-frame=13000
python3 "$REPO/tools/capture/ppm2png.py" "$OUT"/*.ppm
echo "route captures in $OUT"
