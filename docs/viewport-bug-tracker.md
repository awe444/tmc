# Viewport expansion — bug tracker

Bugs found playtesting the 320×160 build (`docs/viewport-expansion-research-plan.md`
Milestone 1). Reported by the maintainer over two rounds of testing; IDs are
theirs. **Last updated 2026-07-30**, at commit `b47ec0cc`.

Anything at 240 is a release blocker. Anything at 320 blocks the Milestone 1
exit criteria but not the shipping build, which is still GBA-native.

## Status

| ID | Summary | Status |
|---|---|---|
| B1 | Save/erase popups' text garbled | **Fixed** (verified 320) |
| B2 | Legend artwork repeats past x=240 | **Regressed** — was fixed, broke again with the B3 ordering fix |
| B3 | Zelda-walking cutscene not full width | **Probably fixed, unconfirmed** — cause found and fixed, not retested in situ |
| B4 | Smith-room sprites/layers wrong at first dialogue | **Open** — cannot reproduce |
| B5 | Interior room-to-room scroll glitches | **Open** — cannot reproduce (needs walking) |
| B6 | Zelda sprite in the left border | **Fixed** (confirmed by maintainer) |
| B7 | Camera-pan softlock in Hyrule Town | **Fixed** (confirmed by maintainer) |

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

## B2 — legend artwork repeats past x=240 *(regressed, open)*

Opening stained-glass narration: the artwork draws a second partial copy at
the right edge.

**Cause.** A layer still reading a 32-tile VRAM screenblock covers 256 px and
*wraps*; it cannot fill 320, so stretching it repeats its content.

**Fix applied, then lost.** A mechanical rule — *a layer with no map source
is clipped to `DISPLAY_WIDTH` and centred* — fixed it (border bleed in the
legend region went 1300–6400 px/frame → 0). Fixing B3's ordering bug
(below) re-exposed it: the artwork's layer is not being clipped and I have
not determined which BG index it is on. A `TMC_LAYER_TRACE` hook exists for
exactly this question but produced no output on the run I tried; that is the
next thing to chase.

Repro: `scripts/sweep.script`, frames ~2500–5750; or `scripts/bugs.script`
waypoint `B2_legend_text`. Measure with the border-bleed check in
`tools/capture/README.md`.

## B3 — Zelda-walking cutscene not full width *(probably fixed, unconfirmed)*

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

Needs confirming in the actual cutscene.

## B4 — smith-room sprites/layers wrong at first dialogue *(open)*

**Cannot reproduce.** Captures of that room *with* dialogue render correctly
at 320 (`scripts/bugs.script` waypoint `B4_smith_dialogue`). The report
specifies "the very first character dialogue", and the scripted run appears
to land on a later one. Needs either a save file parked at that moment or a
screenshot to identify which of several possible faults it is.

## B5 — interior room-to-room scroll glitches *(open)*

Walking from the left interior room into the right one: visible glitching,
scrolling not smooth.

**Cause (understood, mitigation unverified).** Mid-transition the map-source
predicate correctly declines to bind (the window blends two rooms, so
`scrollAction >= 2` is rejected), and the layers fall back to a 32-tile
screenblock that cannot fill 320 — so the extra columns show wrapped
garbage. Mitigation applied: during a transition the world layers and their
sprites are clipped to the authored width, giving a clean 240-wide slice with
borders instead.

**Cannot reproduce.** The scripted tester only presses buttons; this needs
Link walked to a specific doorway. The mitigation has never been observed
working.

Maintainer preference on record: *a fade transition would be acceptable, and
preferable, if the borders cannot contain the adjacent room.* That is a
design change rather than a fix and has not been made.

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

---

## Next actions, in order

1. **B2** — find which BG index the legend artwork is on, then make sure the
   clip rule reaches it. `TMC_LAYER_TRACE=1` was added for exactly this and
   produced no output on first use; check the trace itself before trusting
   its silence. Repro is cheap: `scripts/sweep.script`, frames 2500–5750,
   measure border bleed.
2. **Confirm B3** in the actual cutscene now that the ordering bug is fixed.
   Wide rooms already went 0 → 6400/6400 px of world in the far-right
   columns, so the mechanism is proven; this is just confirmation.
3. **B4 and B5 need a maintainer-supplied `tmc.sav`** parked at each moment,
   or screenshots. Three rounds of inferring these from prose has a poor hit
   rate — B4's captures render *correctly* in the scripted run, and B5 cannot
   be reached by button presses at all.
4. **Then** close out Milestone 1: measure frame time at 320 (the one exit
   criterion never measured) and record the go/no-go.

## Carry-forward items not from playtesting

Recorded here so they are not lost with the plan's spike sections:

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
- **BG3 gameplay overlays** (hole, light, weather) are screen-fixed and were
  never swept for wrap past 256 px.
- **Milestone 1 frame time at 320** is unmeasured. Baseline for comparison is
  the Spike 1 canvas build (present 6.48 ms mean), *not* the Spike 0 240
  baseline — the canvas cost is paid once and should not be charged twice.
