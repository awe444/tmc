#!/usr/bin/env bash
# Record a play session as a replayable input script, for reproducing a bug
# that only a human can reach (docs/viewport-bug-tracker.md B4/B5).
#
# Usage:  ./record-bug.sh <name> [extra tmc_pc flags...]
#   e.g.  ./record-bug.sh B5
#
# Play normally until the bug happens, then quit the game normally (window
# close or in-game exit). The log is line-buffered, so killing the process only
# loses the trailing "<frame> quit" line -- which replay does not need. That
# matters because the interesting recordings end in a hang.
#
# Produces, next to this script:
#   recordings/<name>.script      the input log
#   recordings/<name>.script.sav  the save it started from
# Both are needed to replay: the live tmc.sav is overwritten as you play.
#
# Replay:  tmc_pc --script=recordings/<name>.script
#          (from a directory holding that .sav as tmc.sav, plus baserom.gba
#           and assets/ -- see tools/capture/README.md)
#
# Start recording from the TITLE SCREEN, before loading your file: the log
# begins at frame 0 and replay starts from a fresh boot, so file-select
# navigation has to be part of it.
set -euo pipefail

NAME="${1:?usage: ./record-bug.sh <name>   e.g. ./record-bug.sh B5}"
shift
HERE="$(cd "$(dirname "$0")" && pwd)"

# The binary: an explicit TMC_BIN wins; otherwise prefer one sitting next to
# this script (a distributed playtest directory), else the repo's normal output.
#
# Globbed rather than named, because there is now a play directory per viewport
# -- build/play-240x160 and build/play-320x240 -- and one copy of this script
# serves both. It was previously hardcoded, and the copy in the play directory
# had been hand-edited to match while this one still said `tmc_pc_320`, a name
# no build has produced since Milestone 1. A glob cannot drift that way.
#
# `.prev` is the previous cycle's binary kept for comparison and must never win.
if [ -n "${TMC_BIN:-}" ]; then
    BIN="$TMC_BIN"
else
    BIN=""
    for candidate in "$HERE"/tmc_pc_[0-9]*x[0-9]*; do
        case "$candidate" in
            *.prev) continue ;;
        esac
        [ -x "$candidate" ] || continue
        BIN="$candidate"
        break
    done
    if [ -z "$BIN" ] && [ -x "$HERE/../../build/pc/tmc_pc" ]; then
        BIN="$(cd "$HERE/../.." && pwd)/build/pc/tmc_pc"
    fi
    if [ -z "$BIN" ]; then
        echo "error: no tmc_pc found. Set TMC_BIN=/path/to/binary." >&2
        exit 1
    fi
fi

mkdir -p "$HERE/recordings"
cd "$HERE"

echo "Binary:    $BIN"
echo "Recording: recordings/$NAME.script"
echo "Play until the bug happens, then quit normally (a kill only loses the final quit line)."
echo

"$BIN" --record="$HERE/recordings/$NAME.script" "$@"

echo
if [ -f "$HERE/recordings/$NAME.script" ]; then
    echo "Done. Send both of these:"
    ls -la "$HERE/recordings/$NAME.script" "$HERE/recordings/$NAME.script.sav" 2>/dev/null \
        || ls -la "$HERE/recordings/$NAME.script"
else
    echo "No recording was produced." >&2
    exit 1
fi
