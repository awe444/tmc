# Viewport expansion — bug tracker

Bugs found playtesting the 320×160 build (`docs/viewport-expansion-research-plan.md`
Milestone 1). Reported by the maintainer over four rounds of testing; IDs are
theirs, except B10 which came from a sweep.

**Status: Milestone 1 is done — signed off by the maintainer 2026-07-30.**
Eight of ten bugs are fixed and verified; B4 and B5 are **deferred by
decision**, not outstanding blockers. This document stays the authoritative
record of what the widening actually did to the engine, and §"Carry-forward
items" is the list Milestone 2 inherits.

Anything at 240 is a release blocker. Anything at 320 blocked the Milestone 1
exit criteria but not the shipping build, which is still GBA-native.

## Status

| ID | Summary | Status |
|---|---|---|
| B1 | Save/erase popups' text garbled | **Fixed** (verified 320) |
| B2 | Legend artwork repeats past x=240 | **Fixed** (verified 320, in situ) |
| B3 | Zelda-walking cutscene not full width | **Fixed** (verified 320, in situ) |
| B4 | Smith-room sprites/layers wrong at first dialogue | **Deferred** — never reproduced; needs a recording |
| B5 | Interior room-to-room scroll glitches | **Deferred** — never reproduced; needs a recording |
| B6 | Zelda sprite in the left border | **Fixed** (confirmed by maintainer) |
| B7 | Camera-pan softlock in Hyrule Town | **Fixed** (confirmed by maintainer) |
| B8 | Large heart offset left of the centred HUD | **Fixed** (verified 320, pixel-exact vs 240) |
| B9 | Legend card artwork dimmed right of a vertical seam | **Fixed** (verified 320, pixel-exact vs 240) |
| B10 | BG3 gameplay overlays clipped and misaligned | **Fixed** (found by sweep, not by playtesting) |

---

## B1 — save/erase popup text garbled *(fixed)*

File-select "Saving file…" / "Erasing file…", and the pause-menu save popup.
Text rows interleaved; frame drawn correctly.

**Root cause.** Not the message layer, which is where I looked twice. It was
the *shared text renderer*: `sub_0805F67C` (`src/text.c`) writes a
character's top tile at `param_1[0]` and its bottom tile at `param_1[0x20]`,
because a character is two tiles tall and a row *was* 0x20 entries. With a
widened BG0 stride the bottom halves landed mid-row. Same shape in the
per-line advance `dest += 0x40`.

Three further row-size literals had the same defect and are now
`UI_BG0_ROW_BYTES`: `subtask.c:54,95`, `fileselect.c:747`,
`enterRoomTextboxManager.c:73,86` (the last had never been converted).

**Now moot by construction**: since the switch back to a 32-tile BG0
(`b47ec0cc`) `UiDestStride()` always returns `0x20`, so none of these sites
can disagree with the buffer again.

Repro: `scripts/bugs.script`, waypoint `B1_saving`.

## B2 — legend artwork repeats past x=240 *(fixed)*

Opening stained-glass narration: the artwork draws a second partial copy at
the right edge.

**Cause.** A layer still reading a 32-tile VRAM screenblock covers 256 px and
*wraps*; it cannot fill 320, so stretching it repeats its content.

**Fix.** The mechanical rule was already right — *a layer with no map source
is clipped to `DISPLAY_WIDTH` and centred*. What broke it was **where the
rule was being called from.** The B3 ordering fix moved `mapsource_bind_ui()`
inside `Port_MapSource_CamTrace()`, which is a *diagnostic*: it returns early
unless `TMC_CAMTRACE` is set **and** the room has just changed **and** the
task is `TASK_GAME`. That did put the call after the world bindings, but it
also meant the clips were never applied in an ordinary run at all — and it
took `virtuappu_mode1_set_obj_clip`/`set_obj_offset` and `sUiCentered` down
with them, since those are set in the same function.

The call now lives at the end of `Port_MapSource_Update()`, after the binding
loop: correctly ordered *and* unconditional. `CamTrace` is a pure diagnostic
again.

This is also why `TMC_LAYER_TRACE` printed nothing — the trace was inside the
same early-returning function. It works now, and reports a `clip_mask`
alongside `mapsrc_mask` so "which layer is it on, and did the rule reach it"
is answerable in one line. The legend runs as `SUBTASK_AUXCUTSCENE` with
`mapsrc_mask=0x6 clip_mask=0x9`: the artwork is on BG0/BG3 and is clipped.

**Verified 320:** all legend frames in `scripts/sweep.script` (2000–4500)
have single-colour border bands, and the whole opening sweep (2000–11750,
40 frames) has **0 columns that repeat at the 256 px wrap period**.

Repro: `scripts/sweep.script`, frames ~2000–4500. Measure with the
border-bleed check in `tools/capture/README.md` — but see the caveat added
there about backdrop colour.

## B3 — Zelda-walking cutscene not full width *(fixed)*

Reported first as centred-240-with-borders, then after a partial fix as
"left-clamped 240 with a discontinuous x>240 region".

**Two causes, both found.**

1. The "is this a 240-authored UI screen" test was `substate !=
   GAMEMAIN_UPDATE`, which classifies *cutscenes* as UI. Cutscenes are views
   of the world and must fill the viewport. Now discriminated on the subtask
   type (`gUI.lastState`): `PAUSEMENU`/`MAPHINT`/`KINSTONEMENU`/
   `FIGURINEMENU`/`LOCALMAPHINT` are UI; `AUXCUTSCENE`/`PORTALCUTSCENE`/
   `WORLDEVENT`/`FASTTRAVEL` are world. Cutscene subtasks call
   `UpdateScrollVram` (`subtaskAuxCutscene.c:85`, `subtaskWorldEvent.c:57`),
   so the special maps are live during them and may be map-sourced. **That
   relaxation is gated to wide builds** — applying it at 240 changed what the
   shipping build renders (audit 0 → 179 136 mismatches).
2. **An ordering bug that made the diagnostics look like liars.**
   `mapsource_bind_ui()` applied its "no map source ⇒ clip" rule *before* the
   world layers were bound, so it saw them unbound and clipped the whole
   world to 240 — while `TMC_REJECT_TRACE` correctly reported them as
   `bound`. The UI pass now runs after the world bindings. Wide rooms went
   from 0 to **6400/6400 px** of world in columns 280–319.

   The first attempt at (2) fixed the order by moving the call into a
   diagnostic that almost never runs, which is what regressed B2. See B2 for
   the real fix; the 6400/6400 measurement was taken while nothing was
   clipping and did not distinguish the two.

**Verified 320, in situ:** the cutscene renders full 320 width with world
content edge to edge and Zelda correctly placed in world space
(`scripts/sweep.script` frames 4750–5750, right band 6286–6400/6400 px).

## B4 — smith-room sprites/layers wrong at first dialogue *(deferred)*

**Never reproduced.** Captures of that room *with* dialogue render correctly
at 320 (`scripts/bugs.script` waypoint `B4_smith_dialogue`, and the smith-room
frames in `sweep.script`). The report specifies "the very first character
dialogue", and the scripted run lands on a later one.

**Deferred at Milestone 1 sign-off.** To pick it up, capture a recording — see
"Reproducing B4 and B5" below. Do not spend more time inferring it from prose:
three rounds of that produced no hit.

## B5 — interior room-to-room scroll glitches *(deferred)*

Walking from the left interior room into the right one: visible glitching,
scrolling not smooth.

**Cause (understood, mitigation unverified).** Mid-transition the map-source
predicate correctly declines to bind (the window blends two rooms, so
`scrollAction >= 2` is rejected), and the layers fall back to a 32-tile
screenblock that cannot fill 320 — so the extra columns show wrapped
garbage. Mitigation applied: during a transition the world layers and their
sprites are clipped to the authored width, giving a clean 240-wide slice with
borders instead.

**Never reproduced.** The scripted tester only presses buttons; this needs
Link walked to a specific doorway. The mitigation has never been observed
working, so it is unverified rather than known-good.

Maintainer preference on record: *a fade transition would be acceptable, and
preferable, if the borders cannot contain the adjacent room.* That is a
design change rather than a fix and has not been made.

**Deferred at Milestone 1 sign-off.** See "Reproducing B4 and B5" below.

## Reproducing B4 and B5

Both need a human at the controls, which is why they survived four rounds.
`--record=FILE` exists for exactly this and turns a human-reached moment into
a headless, frame-exact, re-runnable fixture:

```bash
cd build/play-320x160 && ./record-bug.sh B5
```

Play to the bug, quit **normally** (not `kill`), and keep both produced files:
the `.script` and the `.script.sav` beside it. Replay with
`tmc_pc --script=<file>` from a directory holding that save as `tmc.sav`.
Start recording from the title screen — the log begins at frame 0 and replay
starts from a fresh boot, so file-select navigation must be in it. Full
mechanism and the three things that break replay:
`tools/capture/README.md`, "Recording a human session".

A portable save-state file would have been the obvious alternative and **does
not work** — see the carry-forward item on quicksave portability.

## B6 — stray Zelda sprite in the left border *(fixed)*

**Cause.** On hardware the screen *is* the world view, so a sprite is either
on it or off it. Once the viewport is wider than the room being shown, the
leftover columns are border, and an entity standing there is something
hardware would never have drawn.

**Fix.** New PPU `virtuappu_mode1_set_obj_clip(left, right)`, driven from the
room's on-screen span.

## B7 — camera-pan softlock in Hyrule Town *(fixed)*

The bell→town-square pan hung forever; hard softlock.

**Cause.** `WaitForCameraTouchRoomBorder` (`src/script.c`) predicts where the
camera will rest and waits for `scroll_x` to equal it *exactly* — but
computed the prediction from `DISPLAY_WIDTH` while the camera clamps on
`VIEWPORT_CAM_MIN_X/MAX_X`. The equality could never hold.

**This was a process failure, not just a code one.** The Spike 5 `sed` meant
to convert `script.c` silently matched nothing (the real text has
`gRoomControls.` prefixes), and the verification grep only covered
`scroll.c` — so the spike reported the file as converted when it was not.
Every remaining `DISPLAY_WIDTH/HEIGHT` in `src/` has since been audited.

Also fixed alongside: the scripted camera helpers (`sub_08080974`,
`sub_080809D4`) pinned to `origin_x` when the target is near the left edge,
which for a room narrower than the viewport is not the resting place. Caught
by the `TMC_CAMTRACE` in-range assertion flagging a narrow room at `cam=0`
instead of `-40`.

## B8 — large heart offset left of the centred HUD *(fixed)*

The heart row's small hearts are BG0 *tiles*, so they ride the layer's
centring clip. The large heart is not a tile — it is the animated overlay
`UI_ELEMENT_HEART`, an OBJ positioned in screen coordinates. It kept its
authored 240-wide position while everything around it moved.

**Cause.** HUD sprites take `UI_HUD_SPRITE_DX` at source, because a global
OBJ offset would drag world sprites along too. The button elements get it via
`gHUD.buttonX` (`ui.c`), and the item and text elements inherit their x from
the button element — so three of the five element families were covered by
one assignment and nobody noticed the other two. `HeartUIElement` derives its
x from `health` instead (`x = ((health+3)>>2)*8 + 3`), which is why it was
missed.

**Fixed at both sites**, not just the reported one: `HeartUIElement` and
`EzloNagUIElement_Action0` (`element->x = 0x10`, the Ezlo-has-something-to-say
indicator) were the only two handlers setting a screen x without the shift.
The Ezlo nag was never reported — it only appears when Ezlo wants to talk —
but it is the identical one-line defect and would have been the next report.

**Why it presented as two different symptoms.** In the build the maintainer
was playing, `mapsource_bind_ui()` never ran (see B2), so there was no OBJ
clip and the heart was simply *visible in the wrong place* — 40 px left. In a
build with the clip working, the same sprite at x=27 falls inside the left
border band and the OBJ clip **deletes it entirely**. Same defect, and the
"missing large heart" it would have become is worth recognising as this bug
rather than a new one.

**Verified 320:** with the fix, the whole f11000 frame (smith's house, a
240-wide room) shifted by 40 px is **pixel-identical to the 240 build's frame
— 0 mismatches over all 38 400 pixels**, HUD included. `UI_HUD_SPRITE_DX` is
0 at native width, and both 240 gates still pass.

**Lesson.** The stale comment on `UI_CENTER_DX` in `include/viewport.h` still
said "unlike the in-game HUD which is edge-anchored" — three weeks after D1
was reversed. That is exactly the sentence that makes someone not think to
shift a HUD sprite. It has been corrected, and `UI_HUD_SPRITE_DX` now
documents which element sites need the shift and which inherit it.

## B9 — legend card artwork dimmed right of a vertical seam *(fixed)*

On each Picori legend card, once the text is on screen a vertical strip of the
stained-glass artwork — everything right of Link's sword — renders at reduced
brightness. The seam is sharp and does not correspond to anything in the art.

**Cause.** The story panels use a hardware window (WIN0) plus a blend to dim
the panel outside the artwork region. `sub_08053800` (`src/cutscene.c`) takes
the window's edges from a per-card table, `gUnk_080FCCB4[].width`, which packs
left and right into one u16 — `0..120` for the two tall portrait cards,
`0..240` for the four wide ones. Those are **240-authored screen
coordinates.**

The panels are a 240-authored surface and are centred like every other one,
by the BG clip. **A PPU window is not a BG, and the clip does not reach it**:
it is applied in raw screen coordinates. So the artwork moved +40 and the
window did not, leaving the blend boundary 40 px inside the artwork. Measured
directly: per-column brightness held ~390–407 up to x=118 and dropped to
~206 from x=120 — exactly the table's `120` with no shift applied.

**Fix.** Add `UI_CENTER_DX` to both edges at the source site, matching how
every other 240-authored coordinate is handled. Zero at GBA-native width.

**Also fixed, same class, unreported:** `kinstoneMenu.c` sets
`WIN_RANGE(0x68, 0x87)` — a 240-authored window on a centred UI screen. That
menu still cannot be entered cold (pre-existing crash chain, CHANGELOG #16),
so the fix is unverified at runtime, but the defect is identical and visible
by inspection.

**Verified 320:** all 11 captured legend frames are now **pixel-identical to
the 240 build shifted by 40 px — 0 mismatches each**, against 1134–4636
mismatched pixels per frame before the fix. Both 240 gates still pass.

**This is a third distinct centring channel.** The BG clip moves layers, the
`UI_HUD_SPRITE_DX` sites move HUD sprites (B8), and PPU windows are a third
thing that must be moved and were not. Every remaining `WIN_RANGE` call site
with a literal coordinate is worth auditing against this — see the note under
carry-forward items.

## B10 — BG3 gameplay overlays clipped and misaligned *(fixed)*

Found by sweeping the carry-forward item "BG3 overlays were never swept for
wrap past 256 px". **Wrap was not the defect.** BG3 never had a map source,
so the "no map source ⇒ clip" rule caught it every time it was on — which
both removed the overlay from the border columns and, because the clip also
shifts by `UI_CENTER_DX`, moved it 40 px.

**Two families, and the shift is wrong for one of them.**

- *World-locked* overlays set `bg3.xOffset = scroll_x + k` (`holeManager.c:299`,
  `powBackgroundManager.c:32`). At a wider viewport `scroll_x` is already 40 px
  further left, so the layer aligns with the world on its own. The clip's extra
  +40 broke that — **visible in the middle of the screen, not just the borders**.
- *Screen-fixed* overlays sit at `ofs=(0,0)` (the Minish Woods light rays).
  Unclipped they render at their natural phase across all 320 columns.

**Fix.** BG3 is not clipped during a world view. On a UI screen it *is*
authored content and still takes the clip.

**Evidence.** The route's `field` and `textbox` waypoints were 7559 and 6123
mismatched pixels against Spike 0 through the centre 240 columns; both are now
**0**. A warp-tour probe of `SouthHyruleField` went 6197 → **0**. I had
previously written those differences off as camera clamping — they were this.
UI waypoints (cutscene, fileselect, pause, figurine) stay at 0 with solid
borders. 240 unaffected: 11/11 and 0/265,497,600.

`lightray` moved the other way, 29453 → 31673, and that is expected rather
than a regression: it is the screen-fixed family, so unclipping changes the
*phase* of a repeating diagonal pattern by 40 px while still covering the
viewport. There is no ground truth for a decorative full-screen overlay on a
wider screen, and no wrap seam appears — **0 wrap-period columns in every
gameplay waypoint**.

**How to find these:** `TMC_BG3_TRACE=1` logs every BG3 on/off transition with
the room, control word, offsets and whether the clip caught it. BG3 is off in
ordinary rooms, which is why none of the existing scripts ever exercised it —
reaching it needs the warp tour built from `data/map/entity_headers.s`
(`manager subtype=` 0x10 weather, 0x14 steam, 0x18 cloud, 0x19 pow, 0x1A hole,
0x1C rain, 0x22 light, 0x23 light-level).

**Unrelated crash noticed while sweeping:** the generated warp tour segfaults
after ~16 rooms **at both 240 and 320**, so it is not a widening bug. It warps
to arbitrary rooms at fixed coordinates (0x1E0, 0x1E0) that are out of bounds
for interiors. Not chased.

---

## Decision reversal: D1 is now *centered*, not edge-anchored

Recorded because the plan's §0 still shows the original choice.

Edge-anchored was chosen, implemented, and **abandoned**. It required
widening `gBG0Buffer`'s row stride from 32 to 64, and the stride turned out
to be baked into far more than the buffer's own accessors — the shared text
renderer's glyph writer, its per-line advance, and several byte-count clears.
Each was a silent corruption rather than a compile error, so each surfaced
only as a playtest bug. Three rounds found three more.

The variant now in place keeps BG0 at the hardware 32×32 shape. That has a
hard consequence: **a 32-tile map cannot place a tile past x=255**, so the
rupee/shell counter cannot reach the right edge; and a ~28-column text box
cannot be shifted 5 columns inside a 32-column row, so BG0 can only be
shifted *uniformly*. Hearts, counters, text box and UI screens therefore all
move together — i.e. centered. HUD sprites take the same shift at source
(`UI_HUD_SPRITE_DX`, `ui.c`) so they travel with the layer while world
sprites stay put.

**If edge-anchoring is wanted later** it needs the buffer widened *and* the
text renderer properly stride-parameterised — the 2–3 day job, not something
to add between bug fixes.

## Lessons worth keeping

1. **A harness that models the thing it verifies can agree with itself and
   prove nothing.** Spike 2's tile-diff indexed the special map with live
   `gRoomControls` while comparing against one-frame-old VRAM, then gated
   away the frames where that mismatched. Its "zero persistent mismatches"
   was measured over the subset that already agreed. The replacement
   (`--mapsource-audit`) measures *through the real render path*.
2. **Verify the verification's scope.** B7 existed because a grep proved a
   conversion complete in one file and the claim was made for two.
3. **Suspect ordering before suspecting the instruments.** B3 looked like a
   contradiction between trace and capture for an entire round; the trace was
   right and the code ran in the wrong order.
4. **Stride changes in decompiled code are not local.** The compiler cannot
   help: every `0x20` that meant "one row" is indistinguishable from every
   `0x20` that meant something else.
5. **Never put production behaviour inside a diagnostic.** The B3 ordering
   fix needed `mapsource_bind_ui()` to run later, and the convenient place
   that ran later happened to be a trace function gated on an env var and a
   room change. Ordering was fixed and the feature was switched off, in one
   move. It measured as a *success* — "6400/6400 px of world in the far-right
   columns" is exactly what you get when nothing clips — which is the same
   trap as lesson 1: the measurement could not tell "correct" from "disabled".
   When a fix makes a number jump to its theoretical maximum, check that the
   code you think produced it actually ran.
6. **A metric keyed on black is not a metric for borders.** The border-bleed
   check counts non-black pixels in the letterbox columns, which reads a
   correctly-clipped pause menu as 12 800 px of bleed because its backdrop is
   green. Count *distinct colours per column* instead: a clipped border is
   uniform whatever its colour.

---

## Milestone 1 exit criteria — met, signed off 2026-07-30

| Criterion | Result |
|---|---|
| 240 route pixel-identical | **11/11, 0 differences** |
| 240 map-source audit | **0 mismatched in 265 497 600 fetches** |
| No layer wraps/repeats at 320 | **0 wrap-period columns**, 40-frame opening sweep + 11-waypoint route |
| Rooms narrower than 320 centred with borders | **verified** on every room tested; borders are a uniform colour (see D3 below) |
| Frame time at 320 within +25% | **present 7.19 ms mean** vs the 6.48 ms Spike 1 canvas baseline = **+10.9%** |
| Go/no-go for Milestone 2 | **GO** — maintainer approval, 2026-07-30 |

Frame time measured the same way as the baseline: canonical route (12 700
frames), headless dummy video, uncapped, release build, n=3 runs —
7.263 / 7.148 / 7.151 ms (run 1 carries warm-up). p99 9.25–11.12 ms, max
12.5–14.7 ms. Logic is unchanged at 0.15 ms mean. Total ~7.34 ms against the
16.67 ms budget (**44%**).

The +10.9% over a canvas build whose presented surface is already 320×240 is
the extra PPU rasterisation for 33% more viewport pixels; present cost itself
is dominated by a texture upload whose size did not change.

**Decisions taken at sign-off**, so they are not relitigated:

- **B4 and B5 deferred**, not fixed. Neither was ever reproduced.
- **D3 amended: coloured borders are accepted.** The plan's D3 said solid
  black. That holds wherever the backdrop is black (gameplay, the legend), but
  a clipped UI screen shows the *PPU backdrop* in its border bands — green on
  the pause menu, grey in the figurine gallery. The bands are uniform, so the
  clip is working; they are simply not black, which is what hardware shows
  outside every layer anyway. Accepted as-is rather than forced.
- **World-space window sites deferred** (carry-forward below).

The 240 gates above were re-run after every change in this document and are
the standing regression gate; keep running both before any viewport commit
(`tools/capture/README.md`, "Regression gate").

## Carry-forward items — what Milestone 2 inherits

Recorded here so they are not lost with the plan's spike sections. Routing:

| Item | Lands in |
|---|---|
| Title screen affine sword | Spike 9 (affine) |
| Per-scanline circular windows | Spike 9 (HDMA) |
| World-space window x masked to 8 bits | Spike 9, or sooner if a scene is reported |
| Kinstone menu unverified | any real playthrough |
| Quicksave state files not portable | nothing — recorded as a dead end |

None of these blocks starting Milestone 2. The two Spike 9 items are the ones
that will actually be *worked*; the rest are notes.

- **Title screen affine sword** sits ~40 px left. It renders through
  `mode2.c`'s affine path, which neither the BG clip nor the OBJ offset
  reaches. Belongs with Spike 9's affine work; fixing it means offsetting the
  affine *reference point*, which is not a plain pixel shift and risks the
  gameplay affine scenes (barrel, tornado).
- **Kinstone menu** never runtime-verified: it crashes on cold scripted entry
  at *both* 240 and 320, so it is the pre-existing kinstone crash chain
  (CHANGELOG #16) rather than a widening bug. Verify during a real
  playthrough with fusions available.
- **Per-scanline circular windows** (lantern, fade iris, white-triangle) use
  a DMA'd per-line table that has not been widened — Spike 9.
- **Quicksave state files are not portable across processes.** `F5`/`F6`
  (`port/port_quicksave.c`) are process-local by design. Persisting them was
  implemented and reverted: restoring one in a fresh process segfaults in
  `CollideFollowers` (`src/npcUtils.c:318`) walking `currentEntity->next`,
  because the snapshot restores `gEntities` without every global that
  participates in the entity lists. **Not ASLR** — it reproduces with
  `setarch -R`, so pinning the address space does not help. Making it
  portable means an exhaustive inventory of participating globals plus
  relocation of every host pointer inside the snapshot. Input recording
  (`--record`) solves the actual need instead, and has no pointers to fix up.
- **World-space window sites still mask their x to 8 bits.** Found while
  fixing B9, not yet reproduced, and *not* fixed — these are gameplay effects
  whose windows are computed from world-to-screen coordinates:

  | Site | Expression |
  |---|---|
  | `src/scroll.c:347`, `:414` | `WIN_RANGE(left & 0xff, right & 0xff)` |
  | `src/object/lightDoor.c:77` | `WIN_RANGE((tmp2 - 0x18) & 0xff, (tmp2 + 0x18) & 0xff)` where `tmp2 = entity x - scroll_x` |

  At 240 a screen x could not exceed 255 and the mask was free. At 320 it can:
  a light door at screen x=300 masks to 44 and the window jumps to the far
  side of the screen. `templeOfDropletsManager.c` and `bigGoron.c` compute
  `tmp1`/`tmp2` similarly and want the same look.

  **Deliberately left alone.** `include/screen.h` warns that several sites
  rely on 8-bit wrap-around to produce an *inverted* window (left > right),
  which the PPU renders as a wrap — so removing a mask can change intended
  behaviour, and the header states that widening a site's coordinate range is
  a per-site decision for the spike that needs it. Each needs its scene
  reproduced before it is touched. The light door is the cheapest to reach.
- ~~**BG3 gameplay overlays** were never swept for wrap past 256 px.~~
  **Swept — see B10.** Wrap was not the defect; the centring clip was.
- ~~**Milestone 1 frame time at 320** is unmeasured.~~ **Measured** — see the
  exit-criteria table above. Note the baseline for any future comparison is
  the Spike 1 canvas build (present 6.48 ms mean), *not* the Spike 0 240
  baseline: the canvas cost is paid once and must not be charged twice.
