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
| `--record=FILE` | record this session's input as a replayable script, copying the starting save to `FILE.sav`. See "Recording a human session" below |
| `--dump-dir=DIR` | where `dump` writes `.ppm` frames (default `.`) |
| `--frame-stats` | print logic/present mean/p50/p99/max on exit |
| `--exit-frame=N` | hard-quit after N frames (safety net) |
| `--uncapped` | disable frame pacing + vsync (fast headless runs) |
| `--capture-canvas` | dump the composed 320×240 presentation canvas (borders included) instead of raw PPU output |
| `--mapcheck` | per-frame diff of rendered BG tiles vs `gMapData*Special`, plus the camera-delta, OAM-high-Y and expanded-column censuses (`port/port_mapcheck.c`). **Its `spike10:` camera assertion splits converging from stuck**: an out-of-range camera easing into a room is `scroll.c`'s own behaviour — it clamps only in the direction of travel and never snaps — so a room entry is legitimately out of range for ~16 frames at 4 px each. `x-out=32 (30 converging)` is three clean ease-ins and no defect; a **non-converging** run is the thing worth chasing. `[cam10x]`/`[cam10y]` list the first dozen occurrences with room, frame and magnitude |
| `--mapsource-audit` | cross-check every map-sampled tile fetch against the screenblock entry the hardware path would read. **240-only** — it is an equivalence check and there is no valid reference above native width, so it reports nothing there |
| `--mapsource-report` | per-layer bound/rejected frame counts on exit |
| `--no-map-sampling` | disable map-source binding entirely (everything on the screenblock path). The A/B switch behind the Spike 3 equivalence check |

## Diagnostic environment variables

Build-time:

| Var | Effect |
|---|---|
| `TMC_VIEW_W=320` | build a wide viewport. Must be set for **both** `xmake f` and `xmake build`, and **needs `xmake f -c`** — a plain `xmake f` will not drop a previously configured width, so the next build silently stays wide |
| `TMC_VIEW_H=240` | build a tall viewport. Same rules as `TMC_VIEW_W`, and independent of it — the full Milestone 2 build is `TMC_VIEW_W=320 TMC_VIEW_H=240` on both commands |

Run-time (all off unless set):

| Var | Effect |
|---|---|
| `TMC_CAMTRACE=1` | per room, **both axes**: room size, camera position, the legal camera range, and an explicit **out-of-range assertion** per axis. This is what caught the scripted-camera clamp bug. Fires once per room — for a mid-scroll clamp failure use the per-frame assertion in `--mapcheck` (`spike10:` line) instead |
| `TMC_REJECT_TRACE=1` | why each world layer was refused a map source, printed on change, with task/substate/subtask/room/flags |
| `TMC_LAYER_TRACE=1` | which BG indices have a map source (`mapsrc_mask`) and which the centring clip caught (`clip_mask`), with DISPCNT and all four BGxCNT. Printed on change. This is how B2's layer was identified |
| `TMC_BG3_TRACE=1` | every BG3 on/off transition, with frame, room, BGxCNT, offsets and whether the centring clip caught it. BG3 is off in ordinary rooms and carries the gameplay overlays (hole, cloud, light, weather, steam, POW) — this is how B10 was found |
| `TMC_FILL_PROBE=1` | run the script engine's white screen-fill (`SetFillColor(0x7fff, 1)`, the pair `sub_0807FB28` wraps a boss's element award in) on a 60-on/180-off timer during ordinary gameplay, so "does a full-screen fill actually fill the screen" can be asked without the story state that produces one. Same fixture rationale as `TMC_OAMY_PROBE`. It answered B41 in one run — 100.0% of 320x240 — which is what ruled that mechanism out |
| `TMC_BLEND_TRACE=1` | the colour-special-effects registers (BLDCNT decoded into target/effect/target, BLDALPHA, BLDY) plus how much of the BG palette is not black, printed on change, and the engine's working copy alongside the live one. "Black except the sprites" has three causes a frame cannot separate — the layer draws nothing, it draws black, or BLDCNT darkens it — and this rules out the third and localises the second. A colourful working copy over a black live one is a fade; both black is missing data. This is what found B36 |
| `TMC_BLEND_TRACE=1` (cont.) | also counts enabled sprites in **OBJ mode 1** (`semi_objs`). An empty `tgt1` beside a non-zero `semi_objs` is a scene that blends its sprites through the OBJ mode rather than through BLDCNT — B38's signature — and a route whose `semi_objs` is 0 throughout does not exercise that path at all, which is the honest way to ask whether a gate covers it |
| `TMC_BLEND_TRACE=2` | also every palette group as it loads (`[pltt]`), the source bytes the loader actually read for it (`[pltt-src]`), and a report whenever the working palette loses 32+ colours, with a per-row breakdown (`[pltt-drop]`). The row breakdown is what pointed at B36's culprit: rows 2..14 black with 0, 1 and 15 intact names the writer's extent, and the drop being attributed to `frame` rather than to `LoadPalettes` is what said the palette path was innocent |
| `TMC_BG3_TRACE=2` | the same line **every frame**, plus `anchor=` (what the overlay declared about itself, see `Port_MapSource_DeclareBg3ScreenAnchor`) and `camy=`. A transition-only trace answers "is BG3 on here"; it cannot answer "why is this frame's overlay in that position", which is the B31 shape. This is what identified which light-ray state each room runs, and what caught the barrel minish house running the same handler as Minish Woods (B21) |
| `TMC_MAPSRC_DIAG=1` | periodic per-layer agreement sample between the special map and the screenblock |
| `TMC_MAPSRC_LAYERS=0\|1\|2` | bind only the bottom layer, only the top, or both. Bisection aid — this is how the layer→BG mapping was pinned down |
| `TMC_WINTRACE=1` | widest window edge committed during the run; proves the >255 window path is live |
| `TMC_HDMA_TRACE=1` | one line per distinct HBlank-DMA registration (destination register, halfwords per line, bytes the table needs at this height), then a per-frame report for the WIN0H channel: lines driven, right-edge range, DISPCNT window bits, WININ/WINOUT, and `PER-LINE` when the edge actually varies between lines. This is how B11 was found. **Its frame counter is VBlanks, not the capture's presented frames — the two do not line up** |
| `TMC_HDMA_NOWIN=1` | stop forwarding per-scanline WIN0H to the raster, i.e. restore the pre-B11 behaviour. The A/B for circular windows |
| `TMC_OAMY_TRACE=1` | every enabled sprite whose y the 8-bit encoding cannot express at this height, with the packed byte, what the wrap heuristic reads, and the true y. Needs `--mapcheck` |
| `TMC_OAMY_LEGACY=1` | unbind the untruncated OAM y channel, leaving the PPU on the 8-bit wrap heuristic. The A/B for Spike 8 |
| `TMC_OAMY_PROBE=<y>` | park a 64×64 sprite at signed screen y in OAM slot 127, x=128, published through the y channel. Renders rows `y..y+63`, so the visible portion is predictable — the fixture for "a sprite above the top edge" without needing a scene that has one |
| `TMC_STUCK_TRACE=1` | report when the player has been in `PLAYER_ROOMTRANSITION` for 180 frames, with the direction, collisions and position that decide whether he can walk off the doorway tile. The instrument B16 needed and did not have — a hang in that state produces no error and no log line, only silence. Set it to a frame count instead of 1 to lower the threshold; `=5` fires on every ordinary doorway, which is how you prove it runs before trusting its silence |
| `TMC_UILATCH_TRACE=1` | report every frame the UI/world classification wants to change but is being held, with the black-screen verdict and the hold count. The instrument behind B20 — it is what showed the first two "is the screen black" tests returning false on *every* frame, i.e. a fix that appeared to work while running entirely on its timeout |
| `TMC_DISABLE_BG1=1`, `TMC_DISABLE_BG2=1`, `TMC_DISABLE_BG3=1` | drop those layers, beside the existing `TMC_DISABLE_OBJ` / `TMC_DISABLE_BG0`. For a scene with **parallax** no single alignment exists — layers move at different rates, so a two-frame comparison reports a difference for every layer except the one it is shifted for. Leaving exactly one on turns "did this layer scroll cleanly" into a question with an exact answer: zero residual under a pure shift, on every consecutive pair. This is what measured B32 |
| `TMC_MASK_BG0..3=1` | paint every non-transparent pixel of that layer flat magenta and take it out of the alpha blend, so a layer you cannot see can be located *in the frame* rather than by differencing two builds. The case it was built for is B21's light shaft: a 32x64 map blank across two thirds of its columns, alpha-blended at eva=9 over foliage of nearly its own hue — invisible to a human and to a screenshot reader alike. Magenta is not a colour the GBA palette can produce (5-bit channels stop at 0xF8), so counting it has no false positives; set the value to `RRGGBB` hex for another. Priority, windows and the OBJ stack are untouched, so a pixel hardware would have hidden stays hidden — **pure magenta means exactly "this layer is what you see here"** |
| `TMC_BORDER_TRACE=1` | per frame: task/state/substate, the current subtask, whether the renderer is centring this scene (UI or affine), and the scene's own backdrop `gPaletteBuffer[0]`. The instrument behind the per-scene border colours (`port/port_border_color.c`) — it is how each replacement colour was read off the scene that already wears it, and how the Nintendo/Capcom logo screen was caught sharing `TASK_TITLE` with the title |
| `TMC_OOB_TRACE=1` | report any read of `gUnk_0811C0F8` / `gUnk_0811C108` past their four declared entries. These sit contiguously in ROM with `gUnk_0811C110` and are indexed by `direction >> 2` (reaches 63) and `animationState >> 1` (reaches 127); on hardware the index wraps into an identical adjacent copy, ported it reads whatever the toolchain placed next. B16 extended only `gUnk_0811C110` |

**All of these work on Android too**, via `--env=NAME=VALUE` — an app has no
environment, so without it the one platform where a bug reproduces is the one
platform where no instrument can be switched on. See `android/README.md`.

## Script format

One command per line, `#` comments. Commands execute when the run reaches
the given frame number (input applies from the *next* input poll).

```
<frame> keys A+START      # hold this key set from now on (NONE = release)
<frame> warp AREA ROOM X Y LAYER   # debug warp; retries until TASK_GAME
<frame> dump NAME         # write NAME.ppm of the just-presented frame
<frame> subtask N PARAM   # MenuFadeIn(N, PARAM): 1 = pause, 7 = figurine
<frame> action NAME       # giveallitems | maxhearts | healfull
                          # | equipsword | equipbombs
<frame> quit
```

Key names: `A B SELECT START RIGHT LEFT UP DOWN R L NONE`.
Numbers accept `0x` hex. While a script is active, all human input and the
title auto-START hack are suppressed.

### Reaching the pause menu's map screens

They are gated on inventory, not on story state: without `ITEM_MAP`,
`PauseMenu_Variant2` (`src/menu/pauseMenu.c`) silently redirects every request
for screens 4 (overworld map), 5 (dungeon map) and 6 (detail map) back to Items
or Quest Status. Nothing fails and nothing is logged — the screens simply never
appear in a capture. That is how **B18** survived both milestones, and why
`giveallitems` now grants it.

With `giveallitems` done first, the detail map is two `R` presses and an `A`
away from an ordinary pause menu:

```
11420 action giveallitems
11500 subtask 1 0
11820 keys R      # ITEMS -> QUEST STATUS
11824 keys NONE
11920 keys R      # QUEST STATUS -> MAP
11924 keys NONE
12020 keys A      # MAP -> detail map (needs the cursor on an unlocked
12024 keys NONE   #   windcrest; the map screen unlocks 7-10 and 16 itself)
12100 dump detail
12360 keys DOWN   # scroll it, to exercise the bottom of the map window
```

Hold `DOWN` on the detail map to scroll it — the up/down arrow sprites mark the
top and bottom of the map window, which is what B18's band has to line up with.

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
| `bugs.script` | dumps at the frames of specific reported bugs (`docs/viewport-bug-tracker.md`). **Its frame numbers have drifted and its waypoint names now lie** — `B2_glass_text_a` (frame 6600) lands on the smith-room dialogue, not the stained glass, which is at 2000–4500 in `sweep.script`. Re-run `sweep` and regenerate before trusting a label |
| `walk.script` | warp-tour of rooms ≥320 wide, each captured on entry and after a right-sweep |

**Frame numbers are only valid for the exact input sequence in the file.**
Inserting one press shifts everything after it — which has already happened
once, when adding a save-popup press moved every later waypoint. If a script
stops landing on the right moment, re-run `sweep` and re-read the frames.

### What the scripted tester cannot do

It presses buttons from a blank save. It **cannot walk Link to a specific
place**, which is why B5 (an interior doorway transition) has never been
reproduced. Use `--record` (below) to have a human produce the script instead.

## Recording a human session

```bash
tmc_pc --record=/tmp/b5.script          # play; quit normally when done
tmc_pc --script=/tmp/b5.script          # replay, from /tmp/b5.script.sav
```

`--record` logs the committed KEYINPUT every frame it changes, in the same
`<frame> keys ...` format the replay path already consumes, and copies the
starting `tmc.sav` to `FILE.sav`. **The file is line-buffered**, so it survives
however the run ends — a hang, a crash, a kill. Only the trailing `<frame>
quit` line needs a clean exit, and replay does not need it. That was not always
true: the file used to be fully buffered and flushed only by an `atexit`
handler, so a recording of a hang lost everything. It surfaced on Android,
where `SDL_main` returns to JNI and the handler never runs at all, leaving a
0-byte file. Replay then reproduces the session
frame-for-frame — verified by recording a scripted run and replaying it: all
17 dumped frames byte-identical.

Three things make this work, and breaking any of them breaks replay:

- **It records from frame 0, not from a hotkey.** Replay begins at a fresh
  boot, so input from before recording started would be missing and
  everything after would desync. Menu and file-select navigation must be in
  the log.
- **It records after `Port_Capture_OverrideInput`**, i.e. exactly the
  KEYINPUT the engine reads. That includes the title auto-START hack, which
  fires during normal play but is suppressed during scripted replay — logging
  the committed value means the recording supplies it either way.
- **The starting save must accompany the script.** The live `tmc.sav` is
  overwritten as you play, which is why a copy is taken at launch.

`build/play-320x160/record-bug.sh` and its 320x240 twin wrap this for
playtesters. **This is the highest-yield diagnostic in the toolbox**: B13 (town
NPCs popping in and out) was located in one pass from a recording, after
instrumenting five plausible mechanisms against a scripted repro found nothing
— because the scripted repro could not reach the state the bug needed.

### Reaching the BG3 gameplay overlays

None of the committed scripts ever activate BG3 — it is off in ordinary rooms,
which is why it went unswept for so long. The rooms that use it are listed in
`data/map/entity_headers.s` as `manager subtype=`: `0x10` weather, `0x14`
steam, `0x18` cloud, `0x19` pow, `0x1A` hole, `0x1C` rain, `0x22` light,
`0x23` light-level. Map a `Room_*` label to a warp target by walking the
`Area_*` blocks in that file (index within the block is the room number) and
the `AREA_*` enum in `include/area.h` for the area ordinal.

Generating a warp tour that way, plus `TMC_BG3_TRACE=1`, is how B10 was found.
Note such a tour crashes after ~16 rooms at **both** widths — it warps to
fixed coordinates that are out of bounds for interior rooms.

### Why not a save state

`F5`/`F6` quicksave exist (`port/port_quicksave.c`) but are process-local.
Persisting them to disk was implemented and **abandoned**: a snapshot restores
`gEntities` without every global that participates in the entity linked
lists, so loading one in a fresh process segfaults in `CollideFollowers`
walking `->next`. Not an ASLR artefact — it reproduces with `setarch -R`.
Making it portable needs an exhaustive global inventory plus pointer
relocation. Input recording sidesteps the whole problem because a key mask
has no pointers in it.

## Sweeping for screenblock fallbacks

Above GBA-native size a world layer that loses its map source falls back to the
VRAM screenblock, which covers 256 px and cannot fill 320 — the shape of B5,
B15 and B17. `TMC_REJECT_TRACE` and `--mapsource-report` answer "which paths
can that happen on" without any new code:

```bash
TMC_REJECT_TRACE=1 tmc_pc --uncapped --mapsource-report --script=<script> --exit-frame=13000
```

- `--mapsource-report` prints a per-layer, per-reason **frame count** at exit —
  a sustained non-zero count against a world view is the signal.
- `TMC_REJECT_TRACE` prints the area, room, width and flags each time the
  reason changes, which identifies *where*.

Score the frames by **distinct colours per column**, not by testing for black
(lesson 6): a room narrower than the viewport legitimately has flat border
columns, and their count is `viewport width − room width`. A count far above
that means the room is not being drawn.

Expect `task!=GAME` and `substate!=UPDATE` in quantity — those are title, file
select and menus, which the centring clip handles correctly. `mid-transition`
is the B5 fade window with the screen black by design. Anything else against a
world view is worth a frame dump.

The 2026-08-06 sweep and its two remaining gaps are written up at the end of
`docs/viewport-bug-tracker.md`.

## Running any of this on Android

The device build takes the same flags; it just has no argv. Push an `args.txt`
(one argument per line) to the app's external files directory, which `adb push`
writes to without `run-as`, and anything pushed alongside is staged into the
working directory:

```bash
adb shell mkdir -p /sdcard/Android/data/org.tmc.port/files
printf -- '--script=bug.script\n' > args.txt
adb push args.txt bug.script /sdcard/Android/data/org.tmc.port/files/
adb logcat -c && adb logcat -s tmc:V > run.log
```

stdout and stderr reach logcat, so every trace in the table above works there.
`--record=` works too, to an absolute path under that same directory.

**Running one script on both a device and the desktop, then diffing the
traces, is what distinguishes a viewport bug from a platform one.** It is what
identified B16, after six rounds of reasoning about the platform got it wrong.
See `android/README.md`.

## Useful measurements

Border bleed — content that has escaped into the letterbox columns. Should be
0 for any centred surface.

**This check assumes the border is black, and on a UI screen it is not.** The
border bands show the PPU *backdrop*, which is black during gameplay and in
the legend but green on the pause menu and grey in the figurine gallery — so
the check reports a correctly-clipped pause menu as 12 800 px of bleed. Use
the distinct-colours-per-column form below unless you know the backdrop is
black; a clipped border is *uniform*, whatever colour it is.

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

Border uniformity + wrap detection — the two checks that actually found and
cleared B2. A clipped border is one distinct column value per band; a column
identical to one 256 px earlier is the BG wrap period repeating, i.e. a layer
stretched past what a screenblock can cover. Skip all-black columns in the
wrap check or every letterboxed frame is a false positive:

```bash
python3 - <<'PY'
import sys, glob
sys.path.insert(0, "tools/capture")
from ppm2png import read_ppm
from pathlib import Path
for p in sorted(glob.glob("/tmp/out/*.ppm")):
    w, h, rgb = read_ppm(Path(p))
    cols = [bytes(rgb[(y*w+x)*3+c] for y in range(h) for c in range(3)) for x in range(w)]
    black = b'\x00' * (h*3)
    wrap = [x for x in range(256, w) if cols[x] == cols[x-256] and cols[x] != black]
    print(f"{Path(p).stem}: left_uniform={len(set(cols[0:40]))} "
          f"right_uniform={len(set(cols[280:320]))} wrap={len(wrap)}")
PY
```

`left_uniform`/`right_uniform` of 1 means that band is a solid border (the
surface is clipped and centred); 40 means it is full of content (the surface
fills the viewport, which is correct for a wide room). `wrap` should be 0
everywhere.

World coverage — non-black pixels in columns 280–319 of a room wider than the
viewport. Should be ~6400 (the full band); 0 means the world has been
wrongly clipped to native width.

Layer extent — which columns a masked layer actually reaches, from one run of
one binary. `TMC_MASK_BG<n>` makes this a census rather than a difference, so
it needs no second build and no reference frame:

```bash
python3 - <<'PY'
import sys, glob
sys.path.insert(0, "tools/capture")
from ppm2png import read_ppm
from pathlib import Path
MASK = b'\xff\x00\xff'
for p in sorted(glob.glob("/tmp/out/*.ppm")):
    w, h, rgb = read_ppm(Path(p))
    cols = [sum(1 for y in range(h) if rgb[(y*w+x)*3:(y*w+x)*3+3] == MASK)
            for x in range(w)]
    hit = [x for x, c in enumerate(cols) if c]
    print(f"{Path(p).stem}: px={sum(cols)} cols="
          f"{min(hit) if hit else '-'}..{max(hit) if hit else '-'} of {w}")
PY
```

On the `lightray` waypoint with `TMC_MASK_BG3=1` this printed `cols=115..239`
at **both** 240x160 and 320x240 before B21 was fixed — the same extent B21 had
established by building twice with BG3 forced off, in one run instead of two
builds, and with the 80 empty columns visible in the dump rather than inferred
from it. It now reads `195..319` in Minish Woods and `155..279` in the barrel
minish house, each flush with its own room's right edge.

**The mask ignores `BLDALPHA`, so it shows a layer the game has faded to
nothing.** That is deliberate — excluding the highlight from the blend is what
keeps it a flat, countable colour — but it means a magenta band is *not* by
itself evidence that anything is on screen. Three frames of B21's camera sweep
reported a full band while the shaft was at `eva=0` and contributing zero
visible pixels. To ask "is this layer actually visible", take the other layers
away instead (`TMC_DISABLE_BG0/1/2` + `TMC_DISABLE_OBJ`) and count non-backdrop
pixels with the mask *off*; the two questions need different runs.

## Regression gate

Before committing any viewport change, both of these must hold at the
**default 240x160 build**:

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
for an expanded build altered what the shipping 240x160 build renders.
