# Capture / replay tooling (Spike 0)

Deterministic input scripting, framebuffer dumps, and frame-time stats for
the viewport-expansion effort (`docs/viewport-expansion-research-plan.md`).

Determinism: the engine RNG is constant-seeded (`gRand = 0x1234567`,
`src/main.c`; `Random()` is a pure LCG in `port/port_linked_stubs.c`), so
frame-exact scripted input makes whole runs reproducible — two runs of the
same script from the same save state produce **byte-identical** framebuffer
dumps. Verified on the full canonical route.

## Quick start

```bash
xmake build tmc_pc
tools/capture/run_route.sh /tmp/route_out          # run canonical route
python3 tools/capture/diff_captures.py \
    tools/capture/references/spike0-240x160 /tmp/route_out
```

`run_route.sh` executes in an isolated temp dir (blank EEPROM, the user's
`tmc.sav` is never touched) and collects the 11 waypoint dumps.

## CLI flags (tmc_pc)

| Flag | Effect |
|---|---|
| `--script=FILE` | drive input from a capture script; implies `--frame-stats` |
| `--dump-dir=DIR` | where `dump` writes `.ppm` frames (default `.`) |
| `--frame-stats` | print logic/present mean/p50/p99/max on exit |
| `--exit-frame=N` | hard-quit after N frames (safety net) |
| `--uncapped` | disable frame pacing + vsync (fast headless runs) |
| `--capture-canvas` | dump the composed 320×240 presentation canvas (borders included) instead of raw 240×160 PPU output |

Combine with `SDL_VIDEODRIVER=dummy` for headless capture. Frames are dumped
from `virtuappu_frame_buffer` (pre-upscale, pre-filter), so captures are
independent of window scale, xBRZ, and CRT/LCD settings.

## Script format

One command per line, `#` comments. Commands execute when the run reaches
the given frame number (input applies from the *next* input poll).

```
<frame> keys A+START      # hold this key set from now on (NONE = release)
<frame> warp AREA ROOM X Y LAYER   # debug warp; retries until TASK_GAME
<frame> dump NAME         # write NAME.ppm of the just-presented frame
<frame> subtask N PARAM   # MenuFadeIn(N, PARAM): 1 = pause, 7 = figurine
<frame> action NAME       # giveallitems | maxhearts | healfull
<frame> quit
```

Key names: `A B SELECT START RIGHT LEFT UP DOWN R L NONE`.
Numbers accept `0x` hex. While a script is active, all human input and the
title auto-START hack are suppressed.

## Canonical route (`route.script`)

Starts from **no save file**; creates file "A" and captures 11 waypoints.
Frame numbers assume the exact input sequence in the file — an edit shifts
everything after it.

| Waypoint | Frame | State |
|---|---|---|
| `title` | 940 | title screen, PRESS START visible |
| `fileselect` | 1080 | CHOOSE A FILE |
| `cutscene` | 3300 | Picori legend stained-glass page |
| `textbox` | 9500 | Zelda dialogue at Link's house (field warp) |
| `field` | 9800 | Hyrule Field, house area (post-dialogue) |
| `woods` | 10200 | Minish Woods entry, full HUD visible |
| `lightray` | 10600 | Minish Woods west, WIN0 light-ray effect |
| `deepwood` | 11000 | Deepwood Shrine room 0 |
| `town` | 11400 | Hyrule Town (festival state) |
| `pause` | 11800 | pause menu (ITEMS), via `subtask 1 0` |
| `figurine` | 12600 | figurine gallery, via `subtask 7 0` |

Warp targets used (area ordinals from `include/area.h`):
Minish Woods `0x00`, Hyrule Town `0x02`, Hyrule Field `0x03`,
Deepwood Shrine `0x48`, Temple of Droplets `0x60`.

Reference captures: `references/spike0-240x160/` (`.ppm` = diff source,
`.png` = human-viewable). HUD placement mockups for decision D1:
`references/hud-mockups/`.

## Tools

- `ppm2png.py` — pure-stdlib PPM→PNG (`--scale N` for integer upscale)
- `diff_captures.py` — per-waypoint pixel mismatch counts between two
  capture sets; `--rect X,Y,W,H` restricts comparison to a region
  (Spike 1's centered-region check)

## Reference sets

| Directory | Surface | Use |
|---|---|---|
| `references/spike0-240x160/` | raw PPU output | engine-behaviour baseline; diff any build against it directly |
| `references/spike1-320x240/` | composed canvas (Option B) | presentation baseline; compare a canvas capture against it, or against spike0 via `--rect 40,40,240,160` |

```bash
# canvas capture, then check the centred region still matches the engine baseline
tmc_pc --capture-canvas --script=tools/capture/route.script --dump-dir=/tmp/out
python3 tools/capture/diff_captures.py \
    tools/capture/references/spike0-240x160 /tmp/out --rect 40,40,240,160
```
