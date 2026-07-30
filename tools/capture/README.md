# Capture / replay tooling

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
| `--capture-canvas` | dump the composed 320×240 presentation canvas (borders included) instead of raw PPU output |
| `--mapcheck` | per-frame diff of rendered BG tiles vs `gMapData*Special`, plus the camera-delta, OAM-high-Y and expanded-column censuses (`port/port_mapcheck.c`) |
| `--mapsource-audit` | cross-check every map-sampled tile fetch against the screenblock entry the hardware path would read. **240-only** — it is an equivalence check and there is no valid reference above native width, so it reports nothing there |
| `--mapsource-report` | per-layer bound/rejected frame counts on exit |
| `--no-map-sampling` | disable map-source binding entirely (everything on the screenblock path). The A/B switch behind the Spike 3 equivalence check |

## Diagnostic environment variables

Build-time:

| Var | Effect |
|---|---|
| `TMC_VIEW_W=320` | build a wide viewport. Must be set for **both** `xmake f` and `xmake build`, and **needs `xmake f -c`** — a plain `xmake f` will not drop a previously configured width, so the next build silently stays wide |

Run-time (all off unless set):

| Var | Effect |
|---|---|
| `TMC_CAMTRACE=1` | per room: width, camera position, the legal camera range, and an explicit **out-of-range assertion**. This is what caught the scripted-camera clamp bug |
| `TMC_REJECT_TRACE=1` | why each world layer was refused a map source, printed on change, with task/substate/subtask/room/flags |
| `TMC_LAYER_TRACE=1` | which BG indices currently have a map source, with DISPCNT and all four BGxCNT. Intended for B2; produced no output on first use, unverified |
| `TMC_MAPSRC_DIAG=1` | periodic per-layer agreement sample between the special map and the screenblock |
| `TMC_MAPSRC_LAYERS=0\|1\|2` | bind only the bottom layer, only the top, or both. Bisection aid — this is how the layer→BG mapping was pinned down |
| `TMC_WINTRACE=1` | widest window edge committed during the run; proves the >255 window path is live |

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

## Scripts

`scripts/*.script` are committed build products of `make_script.py`. Reaching
a point in the game means replaying every press from a blank save, and the
opening needs several hundred, so they are generated rather than
hand-maintained:

```bash
tools/capture/make_script.py sweep > tools/capture/scripts/sweep.script
```

| Script | Purpose |
|---|---|
| `route.script` | the canonical 11-waypoint regression route (see above). Hand-written; the reference captures depend on it, so **do not regenerate it** |
| `intro.script` | press through the whole opening from a blank save |
| `sweep.script` | the opening with a dump every 250 frames, 2000–12000. **Use this first** to find which frame a moment happens on |
| `bugs.script` | dumps at the frames of specific reported bugs (`docs/viewport-bug-tracker.md`) |
| `walk.script` | warp-tour of rooms ≥320 wide, each captured on entry and after a right-sweep |

**Frame numbers are only valid for the exact input sequence in the file.**
Inserting one press shifts everything after it — which has already happened
once, when adding a save-popup press moved every later waypoint. If a script
stops landing on the right moment, re-run `sweep` and re-read the frames.

### What the scripted tester cannot do

It presses buttons from a blank save. It **cannot walk Link to a specific
place**, which is why B5 (an interior doorway transition) has never been
reproduced. Options for that class of bug: a `tmc.sav` parked at the moment,
or `warp` to the room and hold a direction and hope.

## Useful measurements

Border bleed — content that has escaped into the letterbox columns. Should be
0 for any centred surface:

```bash
python3 - <<'PY'
import sys,glob; sys.path.insert(0,"tools/capture")
from ppm2png import read_ppm
from pathlib import Path
for p in sorted(glob.glob("/tmp/out/*.ppm")):
    w,h,rgb = read_ppm(Path(p))
    n = sum(1 for y in range(40,200) for x in list(range(0,40))+list(range(280,320))
            if rgb[(y*w+x)*3:(y*w+x)*3+3] != b'\x00\x00\x00')
    print(f"{p}: {n}")
PY
```

Wrap detection — a column identical to one 256 px earlier is the BG wrap
period repeating, i.e. a layer stretched past what a screenblock can cover.
Should be 0 everywhere.

World coverage — non-black pixels in columns 280–319 of a room wider than the
viewport. Should be ~6400 (the full band); 0 means the world has been
wrongly clipped to native width.

## Regression gate

Before committing any viewport change, both of these must hold at the
**default 240 build**:

```bash
xmake f -c -y -m release && xmake build -y --rebuild tmc_pc
tools/capture/run_route.sh /tmp/out
python3 tools/capture/diff_captures.py tools/capture/references/spike0-240x160 /tmp/out
```

Expected: `11 waypoints compared, 0 with differences`.

```bash
tmc_pc --no-audio --uncapped --mapsource-audit --script=tools/capture/route.script \
       --dump-dir=/tmp/out2 --exit-frame=13000 2>&1 | grep fetches
```

Expected: `fetches=265497600 mismatched=0`.

Both have caught real regressions — including one where a change intended
for the wide build altered what the shipping 240 build renders.
