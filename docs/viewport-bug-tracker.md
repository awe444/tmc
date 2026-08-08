# Viewport expansion — bug tracker

Bugs found across both viewport milestones. B1–B9 came from the maintainer
playtesting the 320×160 build; B10–B12 from sweeps during Milestone 2; B13–B22
from the maintainer playtesting 320×240, most with recordings; B22–B25 from the 2026-08-08 barrel and lily-pad sessions. B16 and B17 were
reported from the Android build — which is the same viewport on other hardware,
and neither turned out to be a platform bug.

**Status: Milestone 1 signed off 2026-07-30. Milestone 2 is functionally
complete — see `docs/milestone2-status.md`.** Twenty-four of the twenty-five
bugs are closed: twenty fixed with a root cause and evidence, and B4 closed as
**no longer observed** rather than diagnosed. **B21 is open** — diagnosed in
full, but every route to a fix is blocked, so it is a decision rather than
work.

**Four of these were live in the shipping 240×160 build or through all of
Milestone 1** — B11, B12's horizontal half, B13's horizontal half, and the
iris veto. The expansion exposed them; it did not cause them. This document
stays the authoritative record of what the expansion actually did to the
engine.

**B22 is the fourth appearance of one assumption — that the screen is the
room.** B5, B15 and B17 were the first three, all horizontal. B22 is the
vertical case and the first to break *gameplay* rather than rendering: a room
that is exactly viewport-sized on hardware lets camera-relative and
room-relative coordinates be written interchangeably, and only a viewport
change tells them apart.

Anything at 240x160 is a release blocker. Anything at 320x160 blocked the
Milestone 1 exit criteria but not the shipping build, which is still
GBA-native. Builds are named WxH throughout: 240x160 (shipping), 320x160
(Milestone 1), 320x240 (Milestone 2).

## Status

| ID | Summary | Status |
|---|---|---|
| B1 | Save/erase popups' text garbled | **Fixed** (verified 320x160) |
| B2 | Legend artwork repeats past x=240 | **Fixed** (verified 320x160, in situ) |
| B3 | Zelda-walking cutscene not full width | **Fixed** (verified 320x160, in situ) |
| B4 | Smith-room sprites/layers wrong at first dialogue | **Closed** 2026-08-02 — no longer observed in any build (maintainer) |
| B5 | Interior room-to-room scroll glitches | **Fixed** 2026-08-02 — reproduced from a recording; slide replaced by a fade above native size |
| B6 | Zelda sprite in the left border | **Fixed** (confirmed by maintainer) |
| B7 | Camera-pan softlock in Hyrule Town | **Fixed** (confirmed by maintainer) |
| B8 | Large heart offset left of the centred HUD | **Fixed** (verified 320x160, pixel-exact vs 240x160) |
| B9 | Legend card artwork dimmed right of a vertical seam | **Fixed** (verified 320x160, pixel-exact vs 240x160) |
| B10 | BG3 gameplay overlays clipped and misaligned | **Fixed** (found by sweep, not by playtesting) |
| B11 | Circular-window transitions render as a near-black screen | **Fixed** (Milestone 2 Spike 9; was live at 240x160) |
| B12 | Entities culled in a band at the far viewport edge | **Fixed** (Milestone 2 Spike 11; horizontal half was live through Milestone 1) |
| B13 | Town NPCs pop in and out inside the visible frame | **Fixed**, confirmed by maintainer 2026-08-01 (reported with a recording; horizontal half was live through Milestone 1) |
| B14 | UI screens' side borders forced black while their top/bottom borders show the backdrop | **Fixed** 2026-08-02 |
| B15 | Room furniture lit against black through a door/stair fade | **Fixed** 2026-08-02 |
| B16 | Softlock entering the smith room after a scrolling transition | **Fixed** 2026-08-05 — reported from Android, reproduced on desktop once an out-of-bounds read stopped masking it |
| B17 | Minish house interiors render as sprites over black | **Fixed** 2026-08-06 — third instance of the screenblock being unable to cover 320 px; needed the tile mutators to maintain the degraded map, not just a relaxed predicate |
| B18 | Pause map detail view shows only the top of the map | **Fixed** 2026-08-06 — the per-scanline BG3 curtain's band was still in 240x160 rows; the only per-scanline table on a UI screen |
| B19 | Segfault entering a room narrower than the viewport | **Fixed** 2026-08-06 — a `u32` local made a pointer offset unsigned, so a negative camera offset wrapped to +4.29e9. Reported from Android with a recording; reproduced on desktop first try |
| B20 | Gameplay flashes at 240x160, offset, across a pause transition | **Fixed** 2026-08-06 — the centring clip changed several frames before the picture did; it now changes only on a black frame |
| B21 | Minish Woods light shaft ends 80 px short of the right edge | **Open** — diagnosed 2026-08-07. Not a clip: the artwork is a 256 px layer whose shaft already ends at its own right edge. Every fix is blocked — repeats rejected, and a 512-wide BG has no free screenblock pair |
| B22 | Rolling barrel interior: doors out of reach, room spills past 160 rows | **Fixed** 2026-08-08 — the player pin measured the barrel's midline from the camera, not the room; 40 px of error at 320x240. Rim sprites in the border left open as a costed decision |
| B23 | Barrel's drawn hole/doors rotationally apart from the exits that fire | **Fixed** 2026-08-08 — the port's `#ifdef PC_PORT` angle-gate bypass (predates the expansion, identical at 240x160) removed on the maintainer's decision. Hardware gate restored and verified landable |
| B25 | Rolling barrel comes back as noise after a pause | **Fixed** 2026-08-08 — a port-only forced buffer→VRAM copy wrote text tilemaps over *both* of the room's own maps, BG2's affine one and BG1's grain layer. Reproduces at 240x160, so pre-existing. Frame is now pixel-identical across the pause |
| B24 | Riding a lily pad through a room scroll strands the player outside the room | **Fixed** 2026-08-08 — the vehicle's carry state (`LilypadLarge_Action3`) exits on `reload_flags == 0`, which the faded path leaves true for the 32 frames it defers the apply, so the pad exited before the room changed and never carried anyone. Found from a second recording |

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

**Verified 320x160:** all legend frames in `scripts/sweep.script` (2000–4500)
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
   relaxation is gated to expanded builds** — applying it at 240x160 changed what the
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

**Verified 320x160, in situ:** the cutscene renders full 320 width with world
content edge to edge and Zelda correctly placed in world space
(`scripts/sweep.script` frames 4750–5750, right band 6286–6400/6400 px).

## B4 — smith-room sprites/layers wrong at first dialogue *(closed, not diagnosed)*

**Closed 2026-08-02 by the maintainer: no longer observed in any build.**

Recorded precisely, because this is the one entry in this document that closed
without a root cause. B4 was **never reproduced** — captures of that room with
dialogue rendered correctly at 320x160 (`scripts/bugs.script` waypoint
`B4_smith_dialogue`, and the smith-room frames in `sweep.script`), and the
report specified "the very first character dialogue", which the scripted run
never lands on. Four rounds of inferring from the prose produced nothing.

So there is no fix to point at and no mechanism to name. Two readings are
consistent with the evidence and nothing here distinguishes them:

- it was fixed incidentally by one of B10–B15, several of which moved layer
  binding, clipping and entity culling in ways that would plausibly cover it;
- or it was reported from a build with a defect that no longer exists for some
  other reason.

**What that means for anyone reading this later.** A closed-as-unobserved entry
is weaker than the rest of this document. If something in the smith room at
first dialogue ever looks wrong again, this entry is not evidence that it is a
new bug — reopen it rather than filing a fresh one, and get a recording, which
is the step that was never taken here and which resolved B5, B13 and B15 in one
pass each.

**Possible retrospective identification: B16.** Recorded as a possibility, not
a finding. B16 is a softlock in this same room, entered by scrolling, in which
**the player is absent from the first dialogue with Zelda and the Smith** —
which is one reading of "sprites wrong at the very first character dialogue".
The B4 report predates the B5 fade by two milestones, and the scroll transition
into that room existed the whole time, so the entry path matches. Against it:
B4 described a rendering fault, not a hang, and nothing in B16's mechanism
produces wrong *layers*.

Nothing here distinguishes them and B4 was never reproduced, so this is not a
claim that they are the same defect — only a note that the next person should
read B16 before concluding B4 was fixed by something else. Both a recording and
this identification are still missing, which is the same gap the paragraph
above describes.

## B5 — interior room-to-room scroll glitches *(fixed)*

Walking from one interior room into an adjoining one: visible glitching,
scrolling not smooth. **Reproduced 2026-08-02** from a maintainer recording,
after being deferred since Milestone 1 for want of one.

**The cause recorded here for two milestones was wrong**, and wrong in a way
that would have sent a fix to the wrong line. This entry said the map-source
predicate declines to bind because "the window blends two rooms, so
`scrollAction >= 2` is rejected". The trace says otherwise:

```
substate=2 ... sa=1 -> bottom=bound            top=bound            mapsrc=0x6 clip=0x1
substate=1 ... sa=2 -> bottom=substate!=UPDATE  top=substate!=UPDATE  mapsrc=0x0 clip=0x7
```

Substate 1 is `GAMEMAIN_CHANGEROOM`. `mapsource_reason` admits only
`GAMEMAIN_UPDATE` and, above native width, `GAMEMAIN_SUBTASK` — so the scroll
is refused one clause *earlier* than this entry claimed, and `scrollAction`
is never consulted. `sa=2` is right there in the trace and does nothing.

**The mitigation this entry claimed was applied did not exist.** It described
world layers and sprites being clipped to the authored width during a
transition, "giving a clean 240-wide slice with borders". The horizontal half
was real; there was no vertical half, and a world view was handed
`content_height = MODE1_GBA_HEIGHT`, so the rows above the room sampled a
screenblock that holds no valid data there — the striped band over the HUD in
the recording.

**Neither half was fixable by clipping, which is the finding that settled the
design.** Two probes, both reverted:

| probe | striped band | the void |
|---|---|---|
| let `CHANGEROOM` bind a map source | **gone** | 12.0% → 14.5% |
| complete the clip on both axes | **gone** | 12.0% → 12.5% |

A map source renders one room; a slide is two rooms at once. The screenblock
is 32x32 tiles = 256x256 px against a 320x240 viewport, so mid-slide there is
genuinely no tile data for much of the frame. That meets the condition on the
maintainer's standing preference, recorded at Milestone 1 sign-off: *a fade
would be acceptable, and preferable, if the borders cannot contain the
adjacent room.* They cannot.

**Fix: above native size the slide is replaced by a fade** — see
`VIEWPORT_SCROLL_FADE` in `include/viewport.h`. The outgoing room dims whole
to black, the room swaps unseen, the incoming room fades up complete and
centred. Modelled on a maintainer reference recording of the transition the
engine already uses for doors. Gated, so 240x160 still slides.

Three things had to be true at once and each cost a round to find:

- **`FADE_INSTANT` is load-bearing and misnamed.** `FadeMain` only keeps a
  fade alive if `type` carries one of `FADE_INSTANT`/`MOSAIC`/`IRIS`; with
  none set, `active` is cleared on the first update and nothing renders. It is
  the palette-fade handler, which is why `cutscene.c` always passes it.
- **The whole commit has to be deferred, not just the reload.** Deferring only
  the reload changed nothing: `sub_0807BD14` updates `gRoomControls.room` at
  the commit point and `Scroll2Sub0` then refreshes VRAM against the *new*
  room, so the tiles were swapped out from under the fade before the reload
  ran. The transition is now queued and applied once black.
- **The fade in cannot be started from `Scroll2`.** `GameMain_ChangeRoom`
  refuses to finish while any fade is active and the room cannot render until
  it finishes — starting it there deadlocks the two and the fade in reveals
  the same half-drawn frame. It fires on completion instead, keyed on
  `gRoomVars.didEnterScrolling`.

**Evidence.** Replaying the maintainer's recording: **0 frames of the
transition show a partially drawn room**, against 20 before. The outgoing room
holds 98.7% fill while dimming, black for ~6 frames, the incoming room is
96.8-99.7% filled from the first visible frame of the fade in. Both 240x160
gates pass.

**Carry-forward.** `Scroll2Sub2` still slides on the literals `0x3c` and
`0x28` — 240 px and 160 px at 4 px per frame, the GBA screen. They no longer
matter above native size, where the slide runs to completion in one frame
behind the fade, but they are wrong for anyone restoring sliding.

**Lesson (11).** *A recorded cause that was never reproduced is a hypothesis
wearing a fact's clothing.* This entry carried a confident mechanism, a
mitigation described in the past tense, and "never reproduced" — for two
milestones. One recording overturned the mechanism and showed the mitigation
had never been built. Mark unreproduced causes as unreproduced.

## Reproducing B4 and B5

**This works — B13 was found with it in one pass**, after a round of inferring
from the prose found nothing. B4 and B5 have now been open since Milestone 1
for want of a recording, which is the cheapest thing on this list to obtain.

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

**Verified 320x160:** with the fix, the whole f11000 frame (smith's house, a
240-wide room) shifted by 40 px is **pixel-identical to the 240x160 frame
— 0 mismatches over all 38 400 pixels**, HUD included. `UI_HUD_SPRITE_DX` is
0 at native width, and both 240x160 gates still pass.

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

**Verified 320x160:** all 11 captured legend frames are now **pixel-identical to
240x160 shifted by 40 px — 0 mismatches each**, against 1134–4636
mismatched pixels per frame before the fix. Both 240x160 gates still pass.

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
borders. 240x160 unaffected: 11/11 and 0/265,497,600.

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
after ~16 rooms **at both 240x160 and 320x160**, so it is not a widening bug. It warps
to arbitrary rooms at fixed coordinates (0x1E0, 0x1E0) that are out of bounds
for interiors. Not chased.

## B11 — circular-window transitions render as a near-black screen *(fixed)*

Found in Milestone 2's Spike 9, by sweeping the carry-forward item
"per-scanline circular windows have not been widened". They had a worse
problem than width: **they were not being drawn at all.**

**Cause.** Spike 4 widened the window registers by handing full-width bounds
to the PPU through `Port_Screen_CommitWindows`, since the packed 8-bit
registers cannot express an edge past 255. The PPU prefers those bounds
whenever they have been supplied — and `UpdateScreenRegs` supplies them every
frame, so the flag is true from the first frame and never cleared. The
HBlank DMA's per-line writes to `WIN0H` went into a register nothing read, and
every line got whatever whole-frame bounds were last committed. With
`winin=3F3F winout=0000` — all layers inside, none outside — that renders the
screen almost entirely black.

**This was a defect in the shipping 240x160 build**, not a widening bug.

**Fix.** `virtuappu_mode1_set_window_h_bounds()` replaces only a window's
horizontal pair; `port_hdma_step_line` calls it per line for any channel
targeting WIN0H.

**Evidence.** On the canonical route at 240×160, 4879 frames drive the
channel, 111 have WIN0 enabled, and **64 have a right edge that varies
between lines** — genuine circular windows. An A/B over 300 sampled frames
(`TMC_HDMA_NOWIN=1` restores the old behaviour) differs on 19, with the
differing pixel count sweeping 38151 → 518 and back: an iris animating. The
frame shows a clean circle of world content with the fix and a near-black
screen without it.

**Why four rounds of playtesting and the gate both missed it.** The gate is
11 still frames; the defect lives in a ~1 second transition between them, and
still reports 11/11 with the fix in. Playtesting would have shown it — this
is the fade between rooms — which suggests it was introduced after the
playtest rounds, i.e. by Spike 4 itself.

**Lesson (7).** *A gate made of still frames cannot see a defect that only
exists mid-transition.* When a change alters a mechanism rather than a
surface, ask which frames exercise the mechanism and count them, rather than
reading the gate's pass as coverage.

## B12 — entities culled in a band at the far viewport edge *(fixed)*

Found by re-auditing Spike 7's culling conversions rather than trusting its
"vertical siblings were converted at the same time" note.

**Cause.** `CheckOnScreen` (`port/port_draw.c`) — the per-entity visibility
test that gates whether an entity is drawn at all — compared against raw
literals `0x16E` and `0x11E`. Those are *screen size plus twice the 0x3F slack
margin* (240+126, 160+126), so at an expanded viewport they cull a band at the
far edge that is genuinely on screen: **17 px at the right at width 320, 17 px
at the bottom at height 240.** Entities blink out shortly before the edge.

**The horizontal half was live for the whole of Milestone 1.** Spike 7 walked
12 wide rooms and did not catch it because its two artifact scans looked for
wrap-shaped repetition and black columns, and a missing sprite produces
neither.

**Fix.** Both bounds are now `VIEWPORT_WIDTH/HEIGHT + ONSCREEN_MARGIN * 2`.
The margin stays `0x3F` — it is slack for a sprite whose origin has left the
screen while its body has not, a property of sprite size rather than of the
viewport.

**Evidence.** A/B over the canonical route at 320x240 differs on exactly two
waypoints, both inside the predicted bands: `woods` at rows 227-239 and
`textbox` at columns 294-313. The woods difference is a heart object whose
sprite was culled, leaving only its background pedestal.

**Lesson (8).** *A conversion sweep's own report of what it converted is not
evidence.* Spike 7 said the vertical siblings were done; one of the two most
important sites had neither axis converted. The same shape as B7, and as the
five `WIN_RANGE(0, 160)` sites found alongside it — where Milestone 1 had
converted one of four in a single file.

## B13 — town NPCs pop in and out inside the visible frame *(fixed)*

Reported by the maintainer playing the 320x240 build: NPCs in the Hyrule Town
square appear and disappear as Link moves vertically, while Zelda is
unaffected.

**Cause.** `CheckRectOnScreen` (`port/port_linked_stubs.c`) is not a drawing
predicate — it is the gate `DelayedEntityLoadManager` uses to decide which
NPCs *exist*. A cleared bit makes `NPCUpdate` call `DeleteThisEntity`, and a
set bit re-creates the NPC from `gNPCData`. Its bounds were the literals
`0xF0` and `0xA0`:

```c
if (dx >= halfW * 2 + 0xF0) return 0;   /* 240 */
if (dy >= halfH * 2 + 0xA0) return 0;   /* 160 */
```

With the manager's `halfH` of `0x20` that puts the live band at screen
y ∈ [-32, 192) — so an NPC was destroyed **48 px above the bottom edge of a
240-row screen** and rebuilt on the way back. Zelda survives because
`RecycleEntities` and this path both spare `ENT_PERSIST` entities; ordinary
townspeople are streamed.

**The horizontal half was equally wrong and live through all of Milestone 1**:
the band was x ∈ [-24, 264) on a 320-wide screen, so NPCs blinked in the
rightmost 56 columns too.

**Fix.** Both bounds become `VIEWPORT_WIDTH` / `VIEWPORT_HEIGHT`, which reduce
to the original literals at 240x160.

**Evidence.** Replaying the maintainer's recording and tracing NPC list
membership, the window around the reported movement had five
appear/disappear events; three were an NPC at screen y 190-193 — inside the
visible frame — and they are gone after the fix. The two that remain are at
y = -33/-30, outside the frame, which is the margin working as intended.
All 130 sampled frames differ, with the missing townspeople restored in a
consistent band below the old boundary.

**Why the earlier sweeps missed it.** Spike 7 and Spike 11 both audited
culling, and B12 fixed `CheckOnScreen` in `port_draw.c` — the *drawing* gate.
This is a second, differently-named predicate in `port_linked_stubs.c`, a file
of ported engine functions that the `src/`-focused greps never covered. It
is the only viewport literal in that file, which is exactly why nothing
flagged it.

**Repro.** `build/play-320x240/recordings/npcpop.script` plus its `.sav`,
replayed with `--script=`. The window worth watching is frames 11540–11800.
It is not committed: `*.sav` is gitignored project-wide, so turning this into
a permanent regression fixture needs a deliberate force-add.

**Lesson (9).** *Ask what a predicate gates, not just what it is called.*
`CheckOnScreen` and `CheckRectOnScreen` sound like the same kind of test; one
decides whether to draw an entity this frame and the other decides whether the
entity exists at all. The second is far more visible when it is wrong, and it
lived in the file the audits treated as stubs.

## B14 — UI side borders forced black, top/bottom borders not *(fixed)*

Reported by the maintainer playing 320x240: the Nintendo/Capcom logo screen,
the title screen and file select each had black bars left and right, while
their top and bottom bands showed the screen's own colour — white, pale yellow
and green respectively. Two borders around one surface, two different colours.

**Cause.** `Port_PPU_ComposeCanvas` (`port/port_ppu.cpp`) repainted the columns
either side of a centred UI screen with `PORT_VIEW_BORDER_COLOR`, to satisfy
the plan's original D3 "solid black borders". Nothing did the equivalent for
the rows above and below, so once the viewport grew a second axis the two
disagreed by construction.

**This was two decisions out of date, not a slip.** D3 was amended at Milestone
1 sign-off to accept coloured borders — a clipped UI screen shows the PPU
backdrop, which is what hardware shows outside every layer anyway — and the
repaint that D3 had motivated stayed behind. Milestone 2 then added the
vertical bands, which take the backdrop because they are ordinary PPU output
that nothing overpaints. The horizontal repaint was the odd one out from that
moment on.

**Fix.** Delete the repaint and let the PPU's own output stand on both axes.
`PORT_VIEW_BORDER_COLOR` still fills the canvas ring outside
`PORT_VIEW_CONTENT_*`, which is a different thing — canvas the PPU never
renders into, with no colour of its own.

**Evidence.** Measured as distinct colours per band, per the D3 note below and
lesson 6. Before: `L=000000 R=000000` with `T`/`B` two-coloured (backdrop
across the middle 240 columns, black in the corners where the side repaint cut
through). After, on every UI screen captured: **all four bands are a single
colour and it is the same colour** — `f8f8f8` on the logo screen, `f8f8a8` on
the title, `40b088` on file select.

**Scope.** Against the same build without the change, the four UI waypoints
(title, fileselect, pause, figurine) differ by exactly **19 200 px each = two
40x240 bands**, confined to columns 0-39 and 280-319 — so nothing inside the
centred screen moved. Every world waypoint is byte-identical. Both 240x160
gates pass; at native width the block could not run at all (`fw > DISPLAY_WIDTH`
is false), so the shipping build never had this defect.

**Lesson (10).** *When a decision is reversed, grep for what it motivated.*
D3's reversal is recorded twice in this document and the code it had justified
outlived it by two milestones — in a file nobody re-read, because the border
colour was not what anyone was working on. The same shape as B8's stale
comment, one level up: there the wrong sentence survived the decision, here the
wrong code did.

## B15 — room furniture lit against black through a door/stair fade *(fixed)*

Reported from a recording: entering a stairway, the room's furniture stays at
full brightness against a mostly empty screen through both halves of the fade,
then the room snaps in whole. Measured at **9.3%** of the play area filled
while fading out and **2.2%** while fading in, against 99.9% either side.

**Cause.** `GAMEMAIN_CHANGEAREA` is the door and stair transition, and
`GameMain_ChangeArea` draws *only sprites* while the fade runs — `FlushSprites`,
`DrawUIElements`, `DrawEntities`, `CopyOAM`, with no background work at all.
`GAMEMAIN_CHANGEROOM` is the same shape on the other side of the swap. At
240x160 that costs nothing: the VRAM screenblock covers the screen and still
holds the room. Above native size `mapsource_reason` refused both substates
with `substate!=UPDATE`, so the layers fell back to a screenblock that had
never been kept current while a map source was bound. What faded was a stale
slice on black — and the furniture was visible only because it is drawn as OBJ
sprites, which need no background at all. That is the whole symptom: *sprites
are the only thing that does not depend on the layer that went missing.*

**Fix.** Both room-change substates keep their map source above native size.
During either one `gRoomControls` describes a real room and the camera is at
rest on it, so the map source is exactly the right thing to draw.

**This fix was not available until B5 was fixed, and the evidence is a probe
that failed.** The identical relaxation was tried during the B5 work and
rejected on measurement: 12.0% → 14.5%, no better. A *sliding* `CHANGEROOM`
has the camera between two rooms, and one map source renders one room, so
binding filled part of the frame and left the rest backdrop. Replacing the
slide with a fade removed the between-two-rooms state, and the same one-line
change became correct.

**Evidence.** Replaying the recording at 320x240: the outgoing room holds
**99.9%** fill while dimming and the incoming room is **86.9–100%** while
brightening. The B5 recording still shows **0** frames of its transition with a
partially drawn room, so the room-to-room fade is unaffected. Both 240x160
gates pass; the relaxation sits inside the existing
`VIEWPORT_WIDTH > DISPLAY_WIDTH` guard, so the shipping build cannot reach it.

**Not established: whether this pre-dated the B5 fade.** The code path is
untouched by that commit, which is strong, but it is reasoning rather than
measurement — the fade shifts timing enough that the recording desynchronises
on a pre-fade build and never reaches the stairs. Which is itself worth
knowing: **a recording is now tied to the binary that produced it.** Any change
that alters how many frames a transition takes invalidates every existing
recording for frame-exact replay.

**Lesson (12).** *A probe that failed is evidence about the state it ran in,
not about the change.* This relaxation was measured, rejected and reverted one
session before it became the right fix. What made it wrong was a condition —
the camera sitting between two rooms — that a later change removed. When a
fix lands that alters the state a rejected probe depended on, re-run the probe
rather than trusting the earlier verdict.

---

## B16 — softlock entering the smith room after a scrolling transition *(fixed)*

Walking east from Link's house entrance into the room where Zelda and the
Master Smith are: the room appears, Link never emerges from the doorway, and
the game hangs. **Reported from the Android build, reproduced there 2 of 3
times on a fresh save, and initially not reproducible on desktop at all.**

Three defects in a row, each hiding the next. Only the third is the cause; the
first two had to be fixed before it could be seen.

**1. The one-frame slide stopped at the GBA screen width.** `Scroll2Step`
terminates on `0x3c` and `0x28`. Those are not arbitrary: at 4 px of camera
travel per step they are 240 and 160 px — the GBA screen, spelled as step
counts. B5's fade runs that step to completion in a single frame, so at 320x240
the camera stopped 80 px short and the player, who drifts 0.25 px per step,
landed 5 px short of where he belonged. Both now scale with the viewport
(`VIEWPORT_SCROLL_STEPS_X/Y`). Measured 60 steps / 240 px / 15 px drift before,
80 / 320 / 20 after. **The B5 commit predicted this and dismissed it** — its
carry-forward note says these literals "no longer matter above native size,
where the slide completes in one frame behind the fade". They matter precisely
*because* it completes in one frame: the loop still terminates on them.

**2. An out-of-bounds table read was masking the bug on desktop.**
`sub_080797C4` indexes `gUnk_0811C110` with `direction >> 3`, and `direction`
is a `u8`, so the index reaches 31 in a **four-entry** table. On hardware that
reads on into adjacent ROM and is perfectly defined — 0x0811C14E holds
`0x0807`. On PC the array is its own object and everything past it is whatever
the toolchain placed next, which differed between x86-64/GCC and arm64/Clang.
Desktop's garbage happened to satisfy `tmp == (collisions & tmp)` and released
the player from the doorway; Android's did not. **That single accident is why
this looked like an Android bug for six rounds of investigation.** The table
now carries the real ROM bytes for the full index range (`PC_PORT` only — the
ROM build needs the original four-entry object or its data layout moves).
Fixing it made desktop reproduce the softlock, which is what finally made the
bug tractable.

**Cause (3).** The player arrives in the new room with `direction == 0xff`,
which `LinearMoveDirectionOLD` reads as *not moving* and refuses to act on. He
is standing on the doorway tile (`ACT_TILE_41`, `SURFACE_DOOR`), which routes
`sub_080724DC` into the sub-state whose only job is to walk him off it — and it
cannot move him. He never leaves `PLAYER_ROOMTRANSITION`, so his queued
`PLAYER_SLEEP` is never consumed, so the cutscene script he was handed never
runs, so sync flags `0x4` and `0x8` are never set, and Zelda (`id=34`) and
Smith (`id=40`) wait on each other for ever.

The direction is lost *because of the fade*. The sliding path commits on the
same frame the boundary is crossed, so the player still carries the heading he
was walking. Deferring the commit 32 frames to fade out does not: he comes to
rest while the screen darkens. Traced directly — `playerDir=8` at the queue,
`playerDir=255` at the commit.

**Fix.** Capture the player's facing when the transition is queued and restore
it at the commit, which is exactly the state the slide had at its commit point.

**Evidence.** On the desktop repro the player now walks off the door —
`x=255 → 258 → 261 → 265`, the tile ahead changes from `0x29` to `0x23`, he is
released, and the cutscene proceeds (3 sync sets, `action=28`). Confirmed on
the reporter's device. B5's own recording still transitions correctly, carrying
`dir=24` westward. Re-measured B5's fade at 320x240 after the change: the
outgoing room holds a constant 80 border columns all the way down from
brightness 77 to 2.6, ~6 frames of black, then the incoming room fades in — no
frame shows a partially drawn room.

**Coverage gap, recorded because it is worse than the bug.** `sub_080797C4`
has exactly one caller and `gUnk_0811C110` exactly one user, and **the
canonical route never reaches either** — zero events in 13 000 frames. The
regression gate cannot see this code at all. It is only safe to claim the
shipping build is unaffected because indices 0-3 are byte-identical and the
extension can only change a previously-undefined read.

**Lesson (13).** *An out-of-bounds read in decompiled code is a platform
difference waiting to happen.* On hardware it has a defined answer, because ROM
is contiguous and the bytes after a table are real data. Ported to a machine
where that array is its own object, the same read returns whatever the linker
happened to place next — stable per toolchain, different between them, and
indistinguishable from a correct answer until something moves. B16 read 27
entries past a four-entry table and behaved differently on two platforms for
that reason alone. Where an index can exceed a table, the ROM bytes are the
specification.

**Lesson (14).** *A bug that only reproduces on one platform is not
necessarily a platform bug.* Six rounds went into what differed about Android —
frame rate, `char` signedness, audio threading, allocator behaviour — and all
of it was wrong. The engine ran identically on both; one accidental read made
desktop recover from a fault both platforms had. The question that ended it was
not "what is different about the device" but "what does the device do that
desktop does not", asked of a trace rather than of the code.

## B17 — Minish house interiors render as sprites over black *(fixed)*

Entering a Picori/Minish building interior: the room is not drawn at all. Only
sprites appear — Link, the NPC, the furniture drawn as OBJ — over a black
frame. **Reported 2026-08-05 from a maintainer recording
(`build/play-320x240/picori_village_room_glitch.script`), reproducing on both
Android and x86_64**, which places it in the viewport rather than the platform.

**Diagnosed, and the port's own instrument named it in one run.** With
`TMC_REJECT_TRACE=1`:

```
[reject] area=0x20 room=0x00 w=240 sf=0x01 sa=0 -> bottom=scroll_flags&1 top=scroll_flags&1
```

`scroll_flags & 1` is the *degraded room* exclusion: rooms whose map came from
the `0xffff` sentinel path, built 512x512 by `sub_0807C5F4` and not maintained
by the tile mutators. `AREA_MINISH_HOUSE_INTERIORS` is marked that way
explicitly (`playerUtils.c`, `roomControls->scroll_flags |= 1`). The map source
refuses those rooms by design, so both world layers fall back to the VRAM
screenblock — and a 256x256 screenblock cannot cover a 320-wide viewport.
Measured across the recording: the village holds 99.5% of the frame, and it
drops to **5.4% on the frame the interior loads** and never recovers.

**This is the third bug of this milestone with that same structural cause** —
after B5 (sliding CHANGEROOM) and B15 (door/stair fade). Each was reported
separately, diagnosed separately and fixed separately, and each was the
screenblock being asked to cover 320 px. *Enumerating every remaining path that
can fall back to the screenblock above native size is worth more than fixing
them one report at a time*, and is the first thing to do here.

**A one-line relaxation renders the room correctly, and is not yet a fix.**
Letting the predicate bind these rooms above native size takes the frame from
5.4% to **46.4%**, which is essentially the ceiling for a 240x160 room centred
in a 320x240 viewport, and the room is visually correct. The probe was reverted
rather than kept.

**The question that decides it was answered from the engine, not by
experiment.** The exclusion exists because the degraded map is not updated by
the tile mutators — and that is literally true in the source. All three
mutators (`SetTileType`, `SetTileByIndex`, `RestorePrevTileEntity`) wrap their
special-map write in `if ((gRoomControls.scroll_flags & 1) == 0)`. In a
degraded room that block is skipped entirely. Binding alone would have traded
a black room for a stale one — cut grass rendering as uncut — so **the
one-line relaxation was wrong**, confirmed rather than suspected.

**Fix.** Two halves, both behind `VIEWPORT_MAINTAIN_DEGRADED_MAP`:

- the three mutators maintain the special map in degraded rooms as well, so
  there is a current map to read;
- with that true, the map-source predicate binds them.

The map itself was never the problem: `sub_0807C5F4` builds it into the same
arrays at the same 0x80 stride the sampler reads, which is why binding
rendered a correct room in the first probe. Only its *maintenance* was missing.

**Evidence.** The reject count for `scroll_flags&1` across the reporter's
recording goes from **706 frames to zero**, and the frame from 5.4% filled to
46.4% — about the ceiling for a 240x160 room centred in a 320x240 viewport,
with the balance being the border the camera clamp produces. Regression gate at
240x160 passes. `VIEWPORT_MAINTAIN_DEGRADED_MAP` is 0 at GBA-native, which is
static-asserted while verifying, so every one of the four touched conditions
reduces to the original expression and the shipping build cannot reach any of
this.

**Lesson 12 applies in the direction it was written, and the answer was no.**
The probe was rejected statically in Spike 2, passed the rendering test two
milestones later, and was still wrong — it needed the *other* test, the one it
had been rejected for. A probe that passes the test you thought to run is not
evidence about the test you did not.

## Screenblock-fallback sweep — 2026-08-06

B5, B15 and B17 all have the same root shape: a world layer loses its map
source, falls back to the VRAM screenblock, and a 32-tile screenblock covers
256 px and cannot fill 320. Three separate reports, three separate diagnoses.
This sweep asks the question once: **which other paths can leave a layer on the
screenblock above native size, and do any of them show?**

Run entirely on instruments the port already had — `TMC_REJECT_TRACE=1` for
per-reason transitions with area/room/width, `--mapsource-report` for
per-reason frame counts, and frame dumps scored by *distinct colours per
column*, the border test lesson 6 prescribes. No new code.

**Coverage.** Eleven scripts and recordings at 320x240 (route, sweep, walk,
bugs, intro, and the B5 / npcpop / stairway / zelda-exit / smith /
picori recordings) — roughly 73,000 gameplay frames — plus warp probes into
the three areas whose managers the predicate names, covering 13 rooms.

| rejection class | fired | verdict |
|---|---|---|
| `task!=GAME` | heavily | title / file select; the clip rule handles it. Expected |
| `substate!=UPDATE` | heavily | menus and subtasks; same. Expected |
| `scroll_flags&1` | 706 frames (picori), 296 (area sweep) | **B17 — the only defect found** |
| `mid-transition` | 2–4 frames per recording | the B5 fade window; the screen is black by design |
| `bad geometry` | 1–2 frames per run | `substate=7 area=0 room=0 w=0` — a subtask before a room exists. Transient |
| `layer off` | 148 frames, 4 areas | **benign, measured** — see below |
| `subTileMap rebound` | **never** | **unverified** — see below |

**`layer off` is benign and that is measured, not assumed.** It fires in
MinishPaths (0x11), CrenelMinishPaths (0x12), 0x1A and MinishRafters (0x2E) —
BG1 detached while BG2 stays bound. Every one of the 13 rooms reached renders
**full width, 0 flat columns**. The top layer is genuinely unused in those
rooms, so there is nothing for the screenblock to fail to cover.

**`subTileMap rebound` never fired anywhere, and that is a gap rather than a
result.** The predicate's comment names bigGoron, minish paths and minish
rafters as its causes. Minish paths and rafters were reached — and produced
`layer off`, not `rebound`. So either the comment is stale about which managers
reach that state, or it needs a room or phase this sweep did not hit. **It
remains the most likely place for a fourth instance.**

**What limited the sweep — and a correction to what this section first said.**
Debug warps crash, and the first version of this entry attributed that to
out-of-range room indices. **That was wrong, and a control run disproved it**:
a script with a valid warp crashed 2 of 3 times, and a script with *no warp at
all* also crashed 2 of 3 times. There are at least two distinct faults here.

One is intermittent and warp-independent, near teardown, and it is the noise
that made whole sweep chunks look like failures when their dumps and reports
had in fact been written.

The other is deterministic per area: particular destinations crash on arrival
every time. Two real validation gaps were found and closed —
`Port_DebugAction_Warp` checked neither that the room exists in its area's
RoomHeader table nor that the coordinates fall inside the room, and it now does
both (rejecting the first, clamping the second). That measurably widened
coverage — one chunk went from 10 of 16 areas to 16 of 16 — **but it did not
eliminate the crash**, which still kills other chunks at specific areas.

So the sweep still covers a fraction of the ~128 areas, and the crash is still
open. It is the thing to fix before this sweep can be finished.

**Conclusion.** On everything reachable, **B17 is the only outstanding
screenblock-fallback defect.** That is a narrower claim than "there are no
others": `subTileMap rebound` is unverified, and most of the game's areas were
unreachable because the warp crashes. Both are named above so the next person
starts from them rather than from a playtest report.

## B18 — pause map detail view shows only the top of the map *(fixed)*

Pause menu → MAP → A on a windcrest. The detail map draws down to roughly two
thirds of the frame and then stops; below it is bare parchment down to the
frame's bottom bar. **Reported by the maintainer 2026-08-06 from the 320x240
build, with a screenshot.** Not a screenblock-fallback bug — the map is on the
right layer and drawn correctly, it is being *covered*.

**Root cause: an HBlank-DMA table indexed by physical scanline, whose band
bounds are rows of the authored 240x160 screen.**

`sub_080A67C4` (`src/menu/pauseMenuScreen6.c`) builds a per-line BG3CNT table
and hands it to `SetVBlankDMA`. BG2 carries the scrolling map at priority 3
(`gUnk_08128AD8[4]`); BG3 carries the frame's parchment, and the table flips
BG3's priority per line. `0x1e0b` is priority 3, which loses the tie to the
lower-numbered BG2 and lets the map show; `0x1e0a` is priority 2, which beats
BG2 and covers it. **BG3 is a curtain**, and rows `8 .. unk5+unk4`
(`gUnk_08128E94`; 132 for thirteen of the seventeen windcrests, 120 for the
rest) are the window the map is seen through.

Those two bounds are rows of the authored screen. The table is not:
`port_hdma_step_line` replays one entry per rendered line starting at line 0,
so the index is a **physical scanline**, while `mapsource_bind_ui`
(`port_mapsource.c`) centres the whole UI screen `UI_CENTER_DY` = 40 rows
further down. The curtain therefore opens 40 rows too high and closes 40 rows
too early — the top 8-row margin leaks map, and the bottom 40 rows of map are
replaced by parchment. 92 of the map's 124 rows survive, which is the "top
half" in the report.

**The screen said so itself.** The down-scroll arrow is drawn at y=0x84 = 132
(`sub_080A66D0`) — the same 132 that ends the band. It is an OBJ, so it takes
the UI screen's sprite offset and lands at physical row 172; the curtain closed
at 132. The arrow marking the bottom of the map window and the bottom of the
map window disagreed by exactly `UI_CENTER_DY`, on screen, in the reporter's
own screenshot.

**Spike 9 widened this table and did not move it.** It is one of the nine
per-scanline tables that spike lengthened to `VIEWPORT_HEIGHT`, which is why
the screen is not garbage below line 160. Lengthening a table and relocating
what it addresses are different edits, and only the first was needed anywhere
else.

**Fix.** Add `UI_CENTER_DY` to both band bounds, the way the figurine
gallery's and the kinstone menu's *static* window bounds already do
(`figurineMenu.c:129`, `kinstoneMenu.c:299`, `cutscene.c:247`). One edit fixes
two screens: `Subtask_LocalMapHint` builds its band through the same function.

**Evidence.**

- 320x240, before → after, on the reported frame: rows 132..171 change from
  parchment to map, and rows 40..43 from map to frame margin. Nothing else on
  the frame moves — rows 0..39, 44..131 and 172..239 are pixel-identical.
- The centred 240x160 region of all five map-screen captures (`n0_map`,
  `n1_detail`, `n2_detail`, `n3_detail`, `n4_scrolled`) is now **pixel-identical
  to the 240x160 build**. Before the fix the four detail frames differed by
  7602–7666 px each.
- Regression gate at 240x160: canonical route 11/11 with 0 differences;
  map-source audit 0 mismatched in 265,497,600 fetches.

**The gate cannot see this screen, so the shipping build got its own check.**
The canonical route never opens the map menu. `UI_CENTER_DY` is 0 at
GBA-native, so both bounds reduce to the original `8` and `unk5+unk4` and the
first loop's `i < 8` is exactly the original `i <= 7`; the only codegen
difference at 240x160 is one dead store of `8` into the loop counter, which
already holds 8. Empirically: the same capture script run on 240x160 binaries
built with and without the fix is **byte-identical across all 14 waypoints**,
including the five map screens. That is the argument the route could not make.

**Why no capture had ever rendered this screen.**
`Port_DebugAction_GiveAllItems` did not set `ITEM_MAP`, and
`PauseMenu_Variant2` bounces every request for screens 4, 5 and 6 back to
Items or Quest Status without it (`pauseMenu.c:139`). The entire map-screen
family was unreachable to *any* script, in either milestone. `giveallitems`
now sets it — `ITEM_MAP`'s only other use in the engine is that gate, so it
cannot disturb the inventory grid the way the blanket patterns that function
avoids would — and from `subtask 1 0` the detail map is two `R` presses and an
`A`.

**The general question, asked at the first instance rather than the third.**
Nine sites register a per-scanline HBlank DMA (`viewport.h`). Eight are world
effects: four circular WIN0H windows (fade iris, lantern, white triangle,
minish portal closeup), three BG3HOFS scrollers, and the rolling barrel's
affine matrix. Their line index is a screen position the world already places
correctly, and the port shifts nothing vertically in a world view. The pause
detail map is the only one of the nine on a **UI screen**, and a UI screen is
the only surface the port shifts vertically. So this class has exactly one
member and it is fixed — unlike the screenblock family, which was reported
three times before anyone asked.

## B19 — segfault entering a room narrower than the viewport *(fixed)*

Walking south through the door from Deepwood Shrine's pot-bridge room (`0x03`)
into the double-statue room (`0x04`): hard crash, not a hang. **Reported
2026-08-06 from the Android build with a `--record` script and a logcat**, both
captured with the diagnostics added the same day.

**It reproduced on desktop from the reporter's recording on the first try**,
which is the whole point of the recording: `SIGSEGV`, exit 139, deterministic,
in a debugger. Two Android-only reports in this milestone (B16, B17) each cost
rounds of asking what was different about the device; this one cost one replay.

**Root cause: one `u32` local makes a pointer offset unsigned, and a negative
camera offset wraps instead of subtracting.**

`sub_0807D280` (`src/screenTileMap.c`) repopulates the screenblock during a
scroll. Its `case 2` — the southward branch — computed

```c
mapspecial = mapspecial + (((unk_18 * 0x10000 >> 0x12) << 8) + ((xdiff >> 4) << 1));
```

`unk_18` is the function's only `u32` local. Pulling it into the sum makes the
whole expression unsigned by the usual arithmetic conversions, so a negative
`xdiff` does not subtract — it wraps. Measured at the fault:

```
unk_18 = 0   xdiff = -24   ydiff = -240
scroll_x = 232  origin_x = 256   width = 272  height = 160
gMapDataBottomSpecial = 0x555556dafee0
mapspecial            = 0x555756dafed8      (+0x1FFFFFFF8 bytes)
```

`0u + (-4)` is `0xFFFFFFFC`, and the pointer advanced **4 294 967 292 entries —
8.6 GB** out of a 32 KB array. The `DmaSet` on the next line is the `memcpy`
that died.

**Why the viewport exposed it.** `xdiff` is negative exactly when the room is
narrower than the viewport, because the camera is then pinned left of the room
origin (`VIEWPORT_CAM_MIN_X`) and the columns either side are backdrop. At
GBA-native width that is unreachable: 240 is also the narrowest room in the
game, so `xdiff >= 0` always and the unsigned sum was free. At 320 it is the
common case — **443 of 617 rooms are narrower**, and this one is 272.

The other three branches build their offsets out of the `s32` `tmp` locals and
were already signed. `case 2` is the only one that needed the fix.

**Fix.** Cast the `u32` term to `s32` so the sum is signed. At GBA-native size
both spellings agree exactly, because the sum cannot be negative there.

**Evidence.** The reporter's recording segfaults at frame ~8161 before and runs
to completion (exit 0, 8400 frames) after. Regression gate at 240x160:
canonical route 11/11 with 0 differences, map-source audit 0 mismatched in
265 497 600 fetches.

**This is the same shape as the carried-forward 8-bit window masks** — "at 240 a
screen x could not exceed 255 and the mask was free". Same sentence, different
width: *at 240 a camera offset could not be negative and the unsigned type was
free*. Both are the expansion making a previously-unreachable value reachable,
and neither is a clipping bug. Worth checking the rest of the engine for
arithmetic that assumes a non-negative camera offset, which is now routine.

**Still open, found while fixing this and deliberately not fixed here.**
`ydiff` is `-40` in the *steady state* of any room shorter than the viewport,
and `case 1` and the `default` branch feed it to
`(ydiff >> 4) * 0x100` — a small negative index, reading *before*
`gMapDataBottomSpecial` rather than past it. Those branches are signed, so they
do not wrap and do not crash; they read a kilobyte or two of the wrong globals
into the screenblock. Above native size the world is drawn from the map source
rather than the screenblock, which is likely why nothing has been seen. It
wants its own reproduction before anyone edits it.

**Lesson (17).** *An unsigned type is an assumption about sign, and the
expansion invalidated a lot of them.* B19's `u32` was correct for as long as
the camera could never sit outside its room. Grep cannot find this: the type is
in the declaration and the defect is in the arithmetic three lines away. What
finds it is asking which quantities changed sign range when the viewport grew —
camera offsets against room origins, which is now negative for two thirds of
the game's rooms.

## B20 — gameplay flashes at 240x160, offset, across a pause transition *(fixed)*

Opening or closing the pause menu shows the world at 240x160 and shifted 40 px
for a few frames before the transition completes, so the picture appears to jump
sideways and down. **Reported by the maintainer 2026-08-06 against the 320x240
build**, both directions, not platform-specific.

**Root cause: the clip changes several frames before the picture does.**

`mapsource_is_ui_screen()` answers from `gMain.substate` and `gUI.lastState`,
and `MenuFadeIn` sets both the instant the menu is *requested* — about eight
frames before the screen reaches black. In between, the world is still the thing
on screen but is already being clipped to `DISPLAY_WIDTH` x `DISPLAY_HEIGHT` and
shifted by `UI_CENTER_DX/DY`. Nothing is wrong with the clip; it is applied at
the wrong moment.

Measured on the transition, counting distinct colours per border band — the
metric the tracker's lesson 6 insists on, since the border here is the PPU
backdrop rather than black:

| frame | l / r / t / b | centre |
|---|---|---|
| gameplay | 24 / 51 / 152 / 70 | 202 |
| `open+2` (before) | **1 / 1 / 1 / 1** | 223 |
| `open+10` (before) | 1 / 1 / 1 / 1 | 1 (black) |

All four bands collapse four frames before the centre does. Closing is worse:
the world fades back *in* inside the small box and snaps to full size at full
brightness.

**Fix.** Do not classify differently — change classification only on a frame
where the change cannot be seen, i.e. while the screen is black. The engine
already fades both ways across this transition, so such a frame exists. Bounded
by a 40-frame hold so a transition that never blacks out cannot strand the clip.

**The two directions are not symmetric, and that is the whole difficulty.**
Opening changes state first and reaches black second, so waiting is enough.
Closing reaches black *while the subtask is still current* and only returns
`gMain.substate` to `GAMEMAIN_UPDATE` once the world is already fading back in —
there is no black frame left to wait for. So the close is anticipated from
`gUI.nextToLoad >= 3`, which `Subtask_Exit` sets at the top of the fade out.

**Three wrong answers, each caught by the instrument rather than by reading.**
`TMC_UILATCH_TRACE` was added when the first version appeared to work, and
reported `black=0` on *every* frame of both transitions — the latch was running
entirely on its timeout and the apparent fix was the timeout outlasting the
fade:

1. `gPaletteBuffer` is the engine's working copy, not what the PPU renders.
2. `gBgPltt` is what the PPU renders, and it is still **bright** at the black
   frame. The engine reaches black by *switching the layers off* and showing the
   backdrop (`PauseMenu_Variant3` clears the BG enable bits), not by darkening
   colour. So "is it black" is a DISPCNT question first and a palette question
   second.
3. `gUI.nextToLoad == 3` closes one frame too early: traced, `nextToLoad` is
   already `4` on the single black frame in the middle of the close, so an
   exact-match window has nothing to apply. `>= 3` is the teardown.

**Evidence.** Both directions, before → after: `close+2` goes from 1/1/1/1 to
23/46/80/64 and `close+8` from 1/1/1/1 to 24/51/151/70, i.e. the world fades in
at full size throughout; `open+2` goes from 1/1/1/1 to 24/51/153/70. The settled
frames either side are unchanged. Regression gate at 240x160: canonical route
11/11 with 0 differences, map-source audit 0 mismatched in 265 497 600 fetches —
and the whole change sits inside `#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0`,
which does not exist at GBA-native size.

**Lesson (18).** *A fix that works for the wrong reason measures the same as a
fix that works.* This one passed its before/after comparison on the opening
transition while its central test — "is the screen black" — had never once
returned true. Only the trace separated them. Where a fix has an internal
condition that is supposed to fire, log whether it fires, not just whether the
output improved.

## B21 — Minish Woods light shaft ends 80 px short of the right edge *(open)*

Entering Minish Woods from the west, the shaft of light reaches the right edge
of the screen at 240x160 and stops 80 px short of it at 320x240. **Reported
2026-08-06, measured by the maintainer on 2026-08-07 as exactly 80 px.**

**Diagnosed. It is not a clip, a clamp, or a mask — there is nothing out there
to draw.**

The shaft is BG3, a 32x64 tilemap loaded straight into `gBG3Buffer` by gfx group
0x25 (4096 bytes to GBA `0x02001A40`). Read back at runtime:

```
[bg3dump] xOffset=16
  col  0..20  first=0x8340        <- one filler tile, repeated
  col 21..31  0x8360..0x8364, 0x8341..0x8345, 0x8350   <- the shaft
```

Blank on the left, ~16 columns of artwork on the right ending exactly at map
px 255. At the constant `xOffset = 0x10` that is screen 115..239 — mid-screen
to the right edge of a 240-wide screen, which is what the maintainer describes
seeing at 240x160. At 320 the same band still occupies 115..239 and the screen
simply got 80 px wider.

**No offset can fix it.** The layer wraps at 256. Putting ray columns at screen
256..319 requires ray content in map px `X..X+63`, and that same 64-px window is
also what renders at screen 0..63 — so a shaft at the right edge implies a
second shaft at the left. **Repeated shafts were rejected by the maintainer**
(2026-08-07), which closes that branch rather than leaving it as an option.

**The measurement that settled it was an A/B of the layer, not of the picture.**
Three earlier readings were artifacts and had to be thrown away: a "seam at
x=280" in a zoomed crop that per-column brightness showed was a tile boundary;
an extent from a single frame pair, which static scenery would have produced
identically; and a camera-move test whose premise was wrong because this state
sets `bg3.xOffset` to a *constant*, so the layer never moves with the camera
anyway. Building with BG3 forced off and differencing gave the answer in one
run — `BG3 contributes to columns 115..239, and 0 px beyond` — and that is the
technique to reach for first next time.

**The fix that would work is blocked by VRAM, not by artwork.** BGCNT's size
field offers 512x256, which covers 320 with no wrap at all — `512 - 320 = 192`,
so the screen shows map 192..511. Everything downstream already supports it:
the PPU honours the size bits (`map_width_tiles = (size_flag & 1) ? 64 : 32`),
`scroll_x` is masked to 9 bits, and `sub_08016CA8` takes its upload length from
`gUnk_080B2CD8[control >> 14]` = `0x1000` for that size, so the 4 KB lands
across two screenblocks with no change to the DMA path. And the extra 256 px
needs only *blank*, which is one repeated tile — no new artwork.

It still cannot be done, because a 512-wide BG needs an adjacent **pair** of
screenblocks and there is no free pair:

| layer | screenbase | VRAM |
|---|---|---|
| BG1 | 28 | `0x0600E000` |
| BG2 | 29 | `0x0600E800` |
| BG3 | 30 | `0x0600F000` |
| BG0 | 31 | `0x0600F800` |

All four are occupied and contiguous, and everything below block 28 is
character data — gfx groups load tiles right up to `0x0600F000`. Prototyped
anyway to be sure: BG3 at size 512 from base 30 writes 4 KB over blocks 30 *and*
31, which is BG0's tilemap, and the HUD and text box render as garbage. Reverted.

**Status: open, approaches exhausted short of reallocating a BG layer's
screenbase** — which means re-checking every gfx group's tile destinations
against the new layout, a change far larger than this defect justifies. The
standing options are to accept the shaft ending 80 px short, or to spend that
work. No decision is recorded.

**Lesson (19).** *"Supported end to end" is a claim about a pipeline, and a
pipeline has more stages than the ones you thought to check.* The size-bit route
was proposed after confirming the renderer honoured it and the DMA length table
sized it — two real checks that were both true and neither of which was the one
that decides. VRAM allocation was, and it was never looked at until the
prototype corrupted the HUD. When a change needs a resource, check the resource
is free *before* checking the code that would use it.

## B22 — rolling barrel interior: doors out of reach, room spills past 160 rows *(fixed)*

Deepwood Shrine's `InsideBarrel` (area `0x48`, room `0x20`). **Reported
2026-08-08 by the maintainer: "the doors do not line up with the walkable room
area", the room unplayable at 320x240.** Fixed the same day.

**The doors never moved. The player did.** The room is exactly 240x160 — the
one size at which a room is the screen — so at 320x240 it is centred and the
camera is pinned 40 px above the room origin (`camx=-40 camy=-40`,
`TMC_CAMTRACE`). `sub_08058CFC` holds the player on the barrel's midline and
measured that midline **from `scroll_y`**, i.e. from the camera. On hardware
`scroll_y == origin_y` here by construction, so the two spellings are the same
number and the engine's choice was free. Above native height it is 40 px of
error, and everything the player interacts with — the four door hitboxes, the
cobweb hole at room y 69..92, the roll-speed reading, the quadrant split at
`0x50` — is measured from `origin_y` and stayed put.

Measured on Link's sprite: **room y 27 at 320x240 against 63 at 240x160.**

Three symptoms from the one defect, which is why it read as several problems:

- the lower pair of doors (room y 136..146) was outside the reachable band;
- the barrel rolled continuously and unprompted, because sitting permanently
  above the midline satisfies `tmp < 0x49` every frame — and a barrel that will
  not hold still cannot hold the narrow angle windows the doors test;
- the quadrant split always chose the upper half, whichever half the player was
  in.

**The visual half is the vertical twin of B5/B15/B17.** The barrel's picture is
BG2 as an affine layer, driven by a per-scanline matrix the manager rebuilds
every frame in screen coordinates it spells as literals — `scrX = 0x78`,
`scrY = 0x80`, and a `/0xA0` that spreads a quarter sine period across the
screen's 160 scanlines. Run over 240 rows, the index reached 191 instead of 127
and the stave curvature inverted over the bottom rows, and the layer's own
screenblock ran out of authored content at row 160 and showed 80 rows of
unrelated tiles below it.

**Fix.** Three parts, all reducing to the original at GBA-native size:

1. `sub_08058CFC` anchors to `gRoomControls.origin_y`. Same number at 240x160.
2. `sub_08058BC8` builds entry `tmp3` from room row `tmp3 - UI_CENTER_DY`,
   clamped to `DISPLAY_HEIGHT`, so the 160 authored rows land in the centred
   band and no sine index leaves the range the 240x160 build uses.
   `UI_CENTER_DY` is 0 at native, so `row == tmp3`.
3. `port_mapsource.c` gives the room's layers `offset_y = UI_CENTER_DY,
   content_height = DISPLAY_HEIGHT` — the vertical twin of the existing width
   clip. **BG0 is excluded**: it carries the HUD, whose bands anchor to the top
   and bottom of the *viewport*, and confining it would push the hearts down
   40 px and delete the counters.

**The room is reached mechanically, not by area and room number.** The
predicate is "a world view in a GBA affine display mode" (`DISPCNT` mode 1 or
2). `grep DISPCNT_MODE_ src/` returns exactly two sites in the whole game: the
title screen, which is already a centred UI screen, and this room. An affine
layer has no room map behind it and does not follow the camera, so it is a
240x160-authored surface by construction — `mode2.c` already says so where it
honours the clip on that layer.

**Verification, because a gate pass is not coverage here.** This changes a
per-frame mechanism, not a surface, so a static waypoint proves little. Driving
a full roll — down, up, and settling — from a script:

- 26 sampled frames **byte-identical at 240x160** before and after, with 23 of
  25 consecutive pairs differing from each other, i.e. the mechanism is
  genuinely exercised rather than 26 copies of a still scene;
- at 320x240 the play area (crop rows 37..128, between the two HUD bands) is
  **pixel-identical to the 240x160 build on all 26 frames**, and the player's
  position tracks it exactly at +40. The only differing rows are 6..36 and
  129..156 — the two HUD bands, which are bottom- and top-anchored and are
  supposed to differ;
- canonical route 11/11, map-source audit `fetches=265497600 mismatched=0`.

**Left open, deliberately: ~24 px of the barrel's rim sprites still show in the
top and bottom border.** The rim is twelve 32x32 sprites (tile `0x8EB0`,
priority 3) at room y -24 and y 152 — six across, mirrored. Hardware clipped
them at the screen edge; a taller screen reveals 24 more rows of each. The
horizontal twin of this is already solved (`set_obj_clip` to the room's
on-screen span) and the HUD survives it **by construction** — the narrowest room
is 240, so the span is at worst `[40,280)` and the HUD is authored inside 240
and shifted by 40 into it. **Vertically there is no such construction**: the
shortest room is 160, so the span is at worst `[40,200)`, and the HUD's own
sprites sit at y 4..21 and near the bottom edge — inside the band a rim clip
would have to remove. Confining the rim therefore needs a per-slot world/UI
distinction plumbed through `RenderSpritePieces`, `gOam*` and the PPU's OBJ
raster, which is a hot loop, at a milestone whose frame time is already an open
go/no-go. Costed and put to the maintainer rather than spent unasked.

**Lesson (20).** *A coordinate the engine had two equal spellings for is a
defect waiting for the viewport to separate them.* `scroll_y` and `origin_y`
were the same number in this room on hardware, so nothing distinguished the
right one from the wrong one until the camera could move relative to the room.
The same shape as B5/B15/B17 — an assumption that the screen is the room — and
the fourth time it has been the answer. When a room is exactly viewport-sized,
every camera-relative expression in it is unverified code.

## B23 — barrel's drawn hole and the fall it triggers are rotationally apart *(fixed)*

**Reported 2026-08-08 with a recording, immediately after B22 was fixed:** the
invisible exits are now in the right place, but the *drawn* door/hole sits at a
different rotation from the exit that fires. Clearest on the middle exit — the
drawn cobweb hole is nowhere near where Link starts falling.

**Not the expansion, and not B22's fix. It is the port's own angle-gate bypass**
(`sub_08058A04`, `#ifdef PC_PORT`, from CHANGELOG #6, commits `107e7451` /
`cd99dd4d` — long before Milestone 2). Hardware gates the fall on
`unk_20 - 0x118 < 0xD`, i.e. a 13-unit window out of a 512-unit revolution,
*because the cobweb hole rotates with the barrel*. The gate is what guarantees
the drawn hole is at room centre when the fall fires — and the fall snaps the
player to room centre (`origin + 0x78, 0x50`) unconditionally. Remove the gate
and the fall still uses room centre while the hole is wherever the rotation put
it.

**Measured on the maintainer's recording:** at the frame he falls, `unk_20` is
`0xA4`; the window `0x118..0x124` opened on **0 of the sampled frames** of the
whole run. `0xA4` is the barrel's own rest angle (the snap targets are `0x48`,
`0xA0`, `0xF0`), so the bypass makes the fall fire at a rest angle essentially
every time — a fixed ~0x74 of rotation, about 82°, from where the hole is drawn.

**Viewport-independent by construction**: the bypass is `#ifdef PC_PORT`, which
is defined at both sizes, so 240x160 is misaligned identically. This was
invisible until B22 was fixed because the room was not playable enough to reach
the hole deliberately.

**It is a decision, not a defect to fix silently** — the bypass was the
maintainer's deliberate call to make the hole reachable, and the difficulty it
worked around is real at 240x160 too (so it was *not* a workaround for B22).
Three routes, put to the maintainer 2026-08-08:

1. **Restore the hardware gate.** Drawn hole and fall coincide exactly; the
   original difficulty returns.
2. **Keep the bypass.** Easy to trigger, permanently misaligned — today's
   report.
3. **Rotate the barrel into alignment, then fall.** When the player stands in
   the hole with the cobweb gone, drive `unk_20` toward the window over a few
   frames and fall when it opens. Keeps the bypass's reachability and restores
   the visual relationship.

**Resolved 2026-08-08: option 1, the maintainer's choice.** The bypass is gone
and `angleOk = (this->unk_20 - 0x118 < 0xDu)` is unconditional again.

**Restoring the gate exposed the real defect, which was in the renderer, not
the engine.** The maintainer re-tested and reported standing dead centre on the
drawn hole without falling. Traced: he was in the hitbox (`px=120 py=84`,
`inhole=1`) at `unk_20 = 0xCA`, while the gate wants `0x118` — **the drawn
barrel was 0x4E out of step with the angle the logic reads.**

`virtuappu_mode2_render_frame` computed `tex_y = ref_y + pd * rel_line + ...`.
That accumulation is right when BG2X/BG2Y are latched once per frame, which is
how the affine BG normally works. It is wrong when HBlank-DMA **rewrites the
reference every scanline**, because hardware's internal accumulator is
overwritten before each line is drawn — the value just written *is* that line's
reference. The barrel supplies `texY = (angle + line) << 8` per line, so the
line term was counted twice: the texture was sampled at **twice the vertical
rate**, and the picture sat

    (unk_20 + 2r - 128) - (unk_20 + r - 128) = r

texture rows out of step, which at the centre of the authored frame (r = 80) is
**0x50** — against 0x4E measured, the 2 being `sy` not quite 1.0.

Fixed in the PPU rather than worked around in the engine:
`port_hdma_drives_bg2_reference()` reports whether an active channel covers
IO 0x28..0x2F, `Port_PPU_PresentFrame` publishes it through
`virtuappu_mode1_set_bg2_ref_per_line()`, and mode2.c drops the `pb`/`pd` line
term while it is set. **Verified**: the same scripted approach that lands the
window now falls through with the hole under the player, and the barrel's plank
spacing and grain are correct instead of vertically doubled — which is also the
long-standing "renders as flat brown bands / warp not honoured" complaint from
CHANGELOG 0.1.2 and #6, and the actual reason the angle was unlandable enough
to be bypassed in the first place.

**Not viewport-specific**: the double-count was there at 240x160 too, so the
shipping build's barrel was equally out of step. The regression gate passes
(11/11, 0 mismatches) but never enters this room, so it is not evidence here —
the evidence is the capture above.

**Reachability was the risk and was measured before shipping it** — a gate that
cannot be landed makes the dungeon unfinishable, which is far worse than a
misaligned hole. Two facts settled it:

- **The window is on the barrel's free-rolling arc.** The snap targets are
  0x48, 0xA0 and 0xF0, and traced over a full revolution the angle runs
  0xF0 -> 0x117 -> 0x146 -> ... -> 0x1D2 -> 0x0 -> 0x2F -> 0x48: nothing between
  0xF0 and the wrap, so a barrel pushed past its last rest rolls straight
  through 0x118..0x124 rather than stopping short of it.
- **It can be landed, and the technique is the authentic one.** Roll until the
  angle is inside the window, then stop pushing; the angle freezes as soon as
  the player is back inside 0x49..0x57, and the hole band (room y 69..91)
  overlaps that. Swept five release timings 60 frames apart: **1720 froze at
  0x118 and 1733 at 0x120, both `win=1` and both fell through**; 1700 stopped
  short at 0xF4, 1745 and 1760 overshot to 0x135 and 0x149. A ~25-frame band
  out of a ~60-frame roll, with the hole visibly rotating into place as
  feedback.

## B24 — riding a lily pad through a room-scroll transition strands the player outside the room *(fixed)*

Deepwood Shrine B2, floating east from room `0x14` into `0x15`. **Reported
2026-08-08 with the same recording.** The player and the pad end up west of the
room's own west edge, held there by the room-border collision, drawn in the
left border and unable to move — a softlock.

**Exact numbers, from a per-frame trace of the transition:**

| | |
|---|---|
| room `0x14` | origin_x 736, width 464 → east edge **1200** |
| room `0x15` | origin_x **1200**, width 272 |
| player at commit (f5743) | absolute x **1169** — 31 px short of the boundary |
| after the slide (f5745) | absolute x **1189**, `unk_18=80`, camera 880→1200 |
| settles | absolute x **1177** → room x **-23**, camera correctly pinned at -24 |

**Root cause: `VIEWPORT_SCROLL_FADE` collapses the slide into a single frame,
and that preserves the camera and the player's per-step drift but not the
motion of the vehicle he is riding.** `Scroll2Sub2` runs all
`VIEWPORT_SCROLL_STEPS_X` (80) steps in one frame; each step moves the camera
4 px and nudges the player 0.25 px, so the player gains exactly 80 x 0.25 = 20
px — which the trace confirms to the pixel (1169 → 1189). On hardware the same
slide takes 60 *frames*, during which the lily pad is a live entity with
`action = 3` and `speed = 0x100` (1 px/frame east, set by `sub_08085E74`, which
also makes the pad `gRoomControls.camera_target`) and therefore carries the
player a further ~60 px — comfortably across the 31 px boundary. Collapsed, the
pad gets one frame instead of sixty.

`Scroll2Sub2`'s own comment states the intent — "land on exactly the state the
sliding path would have reached". For a walking player it does; for a *ridden*
one it does not, because the drift models only the player.

The lily pad is one of four callers of `sub_0807BD14`; **`minecart.c` is
another and is the same shape**, so this is a class, not one room.

**Expansion-caused but not caused by B22's fix** — proved two ways: the two
functions B22 touched are called only from the barrel manager, and room `0x15`
runs in DISPCNT mode 0, where B22's clip predicate is false. The fade path only
exists above GBA-native size, so 240x160 is unaffected.

**Not reproducible by replaying the recording on an older build** — on the
pre-B22 binary the player bounces out of the barrel four times and never
reaches B2 at all, and at 240x160 the run diverges after room `0x14` because
transitions slide rather than fade.

**A two-part fix was attempted first and did not clear it** (commit
`030ed14a`). It was not wrong — it was *inert*, and why is the whole lesson
here. Both parts key off `gRoomControls.camera_target` being the vehicle, and it
never was, because the state that claims the camera had already exited. With
that state alive (below) both parts do exactly the work they were written for:
the pad holds the camera through the deferral, and `Scroll2Sub2` supplies the
travel the collapsed slide skips. The two halves are:

1. `ScrollTransitionApplyWhenBlack` (playerUtils.c) saves and restores
   `gRoomControls.camera_target` around the apply. The deferral inverts an
   order: a vehicle hands off by calling `sub_0807BD14` and *then* claiming the
   camera, so sliding the claim lands after the apply's reset and stands, while
   fading it lands 32 frames before and is wiped. That wipe is why the player
   received the *walker's* 0.25 px-per-step nudge at all — `Scroll2Step` applies
   it only when the target is the player. Same shape as
   `sScrollFadePlayerDirection` (B16), one field over.
2. `Scroll2Sub2` (scroll.c) advances a non-player camera target, and the player
   with it, by `steps x speed` along the scroll direction — the travel the
   vehicle would have had over the frames the collapse skipped.

**The actual root cause, found from a second recording (2026-08-08).**

`LilypadLarge_Action3` *is* the carry-across-a-room-scroll state. It runs
`LinearMoveUpdate` on the pad and on the player every frame, and it exits — hands
the camera back, drops `ENT_PERSIST`, returns to ordinary floating — on

    if (gRoomControls.reload_flags == 0)

which is the engine saying "the scroll is over".

Sliding, `sub_0807BD14` applies the transition *inside itself*, so that flag is
already set the first time action 3 runs, and the pad carries for the whole
60-frame slide — about 60 px, which is what takes it across the boundary.
Fading, the apply is deferred until black, and **for those 32 frames nothing
marks a transition as in progress**. Traced: the hand-off fires at f4395 with
the pad at room x 440, action 3 is entered, and by f4400 the pad is back in
action 1 with the camera handed back — 28 frames *before* the room changes. It
carried for zero frames.

So the pad crossed nothing. It sat at the old room's x 441, which is the new
room's **-23**, and stayed there for the rest of the recording, with the player
on it at -21, both drawn in the left border. The player's earlier-measured
+20 px was the *walker's* per-step nudge, applied precisely because the pad had
already given the camera back.

**Fix.** `ScrollTransitionIsPending()` (playerUtils.c) reports a decided but
un-applied faded transition, and action 3 treats that as a scroll still in
progress. Defined outside the `#if VIEWPORT_SCROLL_FADE` block and returning
`FALSE` at GBA-native size, so the condition there reduces to the original
exactly.

**Verified on the recording that found it**: the pad now carries from 435
through the deferral, is at room `0x15` x **91** with the player at 89 when it
returns to floating — comfortably inside a 272-wide room — against **-23**
before. Regression gate 11/11, `fetches=265497600 mismatched=0`.

**Then trimmed, on the maintainer's report that it entered too far in.** The
first version landed the pad 91 px into a 272-wide room where hardware puts it
at ~36, because it was given the 32 fade frames of its own travel *and* a full
`VIEWPORT_SCROLL_STEPS_X` slide on top. Two things were wrong with that number:

- **How far a vehicle carries the player is a fact about the vehicle and the
  room, not about how wide the screen is.** It is `speed` for as many frames as
  the slide lasts, and on hardware that is `DISPLAY_WIDTH / 4` across (60) or
  `DISPLAY_HEIGHT / 4` down (40). `VIEWPORT_SCROLL_STEPS_*` is right for the
  *camera*, which genuinely has more screen to bring on at 320 wide, and wrong
  for him.
- **The fade frames are travel too.** The carry state runs throughout the
  deferral, so by the time the slide is collapsed the vehicle has already had
  those frames; only the remainder is owed. The fade is `0x100` of progress at
  `VIEWPORT_SCROLL_FADE_SPEED`, so its length is `0x100 / SPEED` = 32 frames.

`Scroll2Sub2` now tops up by `nativeSteps - fadeFrames` — 28 frames across, 8
down — clamped at zero. **Re-measured on the same recording: the pad enters
still carrying at room x 7 and comes to rest at 39 with the player at 37**,
against 91 before and ~36 on hardware. The 3 px over is the few frames of carry
that run after the swap before `reload_flags` clears.

Freezing the vehicle during the fade instead was rejected: it would visibly
stall while the screen is still bright, where being carried is what hardware
shows.

**`minecart.c` has the same `sub_0807BD14`-then-claim shape and has still never
been exercised.** If a cart ever strands you on a room boundary, this entry is
where to start.

## B25 — the rolling barrel comes back as noise after a pause *(fixed)*

Pause inside Deepwood's InsideBarrel and close the menu: the barrel returns as
fine yellow/blue tile noise. **Reported 2026-08-08 with a recording, fixed the
same day.**

**Not the expansion.** It reproduces identically at 240x160 from a warp-in,
pause, unpause fixture — the fifth defect this milestone that was live in the
shipping build all along.

**Measured rather than guessed, and the first two guesses were wrong.** The
room's enter handler *is* re-run on the way out — `RestoreGameTask` ->
`sub_0801AE44` -> `gArea.onEnter` — which was confirmed by tracing, so
"the handler never runs" was out. Re-running the handler a second time after
the palette restore changed nothing, so "the palette backup clobbers it" was
out too. Checksumming the three candidates across the pause settled it in one
run:

| | before | after |
|---|---|---|
| BG2 character data (`0x06000000`, 16 KB) | `AA105DE5` | `AA105DE5` |
| BG2 map (screenbase 28, `0x0600E000`) | `EB1BBC50` | **`0A853C7E`** |
| BG palette | `101EF5AB` | `101EF5AB` |

Only the map moves. **The tiles and the palette were never the problem.**

**Root cause: a port-only line writing a text tilemap into an affine map.**
`RestoreGameTask` ends with a `#ifdef PC_PORT` block that forces
`sub_08016CA8` on BG0, BG1 and BG2 to push the staging buffers into VRAM,
added because the GBA mechanism that does so after a map subtask does not fire
here. In GBA display mode 1 or 2 that layer's screenbase does not hold a text
tilemap at all — it holds a one-byte-per-tile *affine* map, which the room's
own handler loads straight from a gfx group (`LoadGfxGroup(0x16)`, whose second
entry lands on exactly that screenbase). The forced copy overwrites it. That is
also why re-running the handler did not help: the handler reloads the map
correctly and this line then destroys it, one step later.

**Two layers, and they fail differently — which is why this took two passes.**
`LoadGfxGroup(0x16)` writes *four* destinations, and two of them are maps:

| dest | size | what |
|---|---|---|
| `0x06000000` | 16384 | BG2 character data (BG2CNT `0xBC82`, charbase 0) |
| `0x0600E000` | 4096 | **BG2's affine map** (screenbase 28) |
| `0x06004000` | 8192 | BG1 character data (BG1CNT `0x5E86`, charbase 1) |
| `0x0600F000` | 2048 | **BG1's map** (screenbase 30) |

Overwriting BG2's affine map with a text tilemap turns the barrel into noise —
the reported symptom. Overwriting BG1's map is quieter: BG1 carries the wood
grain, alpha-blended over the staves (`layerFXControl = 0x3456`,
`alphaBlend = 0x909`), so losing it leaves the barrel legible but flat.

**Fixing only BG2 left exactly that behind, and it was mis-read as a palette
problem** — the palette buffer checksum was identical across the pause, the
colour *count* had changed, and "different colours" was the obvious reading. It
was the maintainer who named it: the grain lines were simply absent. The lesson
is that a colour-count delta says "the image changed", not "the palette
changed", and the two were only distinguishable by someone looking at it.

Fixed by skipping **both** the BG1 and BG2 copies when the display mode makes
the room affine. BG0 keeps its copy: it carries the HUD and the text box, which
are genuinely buffer-driven in every room including this one. The mode read is
the room's own, because the handler has already applied it by that point.

**Verified**: on a stationary fixture the frame after the pause is
**pixel-identical to the frame before it — 0 differing pixels of 76800** — and
on the maintainer's own recording the colour count matches at 107 either side.
Gate 11/11, `fetches=265497600 mismatched=0`.

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
   uniform whatever its colour. B14 is the same mistake made in *code* rather
   than in a measurement — the border was painted black to match the metric.

Lessons 7–14 are stated where they were learned: 7 in B11, 8 in B12, 9 in B13,
10 in B14, 11 in B5, 12 in B15, 13 and 14 in B16. 18 is in B20 and 19 in B21,
for the same reason. The three below are here because they are about the
tooling rather than about any one defect.

15. **A per-scanline table is indexed by the raster, not by the surface it
    decorates.** Wherever the port shifts a surface, any table addressing that
    surface by line has to take the same shift — and *lengthening* such a table
    is not *relocating* it. Spike 9 lengthened all nine and needed to relocate
    exactly one, which is why the miss survived a spike whose whole subject was
    these tables. B18.

16. **An item the debug actions do not grant is a screen the tooling cannot
    see.** `giveallitems` omitted `ITEM_MAP`, and without it the pause menu
    silently redirects every request for the three map screens. Nothing failed
    and nothing was logged; the screens were simply never in a capture, in
    either milestone, until a human opened one. When a menu gates a screen on
    inventory, the gate is part of the test surface. B18.

17. **A diagnostic that cannot be switched on where the bug happens is not a
    diagnostic.** Every `TMC_*` trace was gated on `getenv`, and an Android app
    has no environment — so the platform that reproduced B16, B17 and B19 was
    the one platform with no instruments. `--env=NAME=VALUE` in `args.txt` fixed
    that, and B19 then reproduced on desktop from a device recording on the
    first replay, against six rounds for B16. A log that cannot name its own
    build is the same problem one step earlier: the first dungeon-softlock
    report could not be attributed to a binary at all.

---

## Milestone 1 exit criteria — met, signed off 2026-07-30

| Criterion | Result |
|---|---|
| 240 route pixel-identical | **11/11, 0 differences** |
| 240 map-source audit | **0 mismatched in 265 497 600 fetches** |
| No layer wraps/repeats at 320x160 | **0 wrap-period columns**, 40-frame opening sweep + 11-waypoint route |
| Rooms narrower than 320 centred with borders | **verified** on every room tested; borders are a uniform colour (see D3 below) |
| Frame time at 320x160 within +25% | **present 7.19 ms mean** vs the 6.48 ms Spike 1 canvas baseline = **+10.9%** |
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

The 240x160 gates above were re-run after every change in this document and are
the standing regression gate; keep running both before any viewport commit
(`tools/capture/README.md`, "Regression gate").

## Carry-forward items — what Milestone 2 inherits

Recorded here so they are not lost with the plan's spike sections. Routing:

| Item | Lands in |
|---|---|
| ~~Title screen affine sword~~ | **Done** — `docs/affine-viewport.md`; verified pixel-exact |
| ~~Per-scanline circular windows~~ | **Done — Spike 9.** See B11 and `docs/spike9-hdma-240.md` |
| World-space window x masked to 8 bits | Spike 9, or sooner if a scene is reported |
| Kinstone menu unverified | any real playthrough |
| Quicksave state files not portable | nothing — recorded as a dead end |

The per-scanline windows turned out to be a live defect rather than an
unwidened one — recorded as B11 below.

**The affine half is done for the scenes that can be reached.** The title
sword and the rolling barrel are both fixed and verified
(`docs/affine-viewport.md`); the barrel was one warp away from the scripted
tester the whole time, which is why "unreachable" was worth re-testing rather
than believing. Vaati's tornado and the screen-shrink cinematic are still
unreached — the tornado's per-line effect turns out to be a BG3 scroller
rather than affine, so it is probably already covered, but that is reasoning
and not observation.

Of the list above, only the 8-bit world-space window masks remain genuinely
untouched, and they still want their scene reproduced before anyone edits
them — `include/screen.h` warns that several rely on the wrap to produce an
*inverted* window.

- ~~**Title screen affine sword** sits ~40 px left.~~ **Fixed** —
  `docs/affine-viewport.md`. `mode2.c`'s affine path now honours the same
  centring clip the text path does, which was the missing channel rather than
  a reference-point calculation. At 320x240 the title's centred 240x160 box is
  pixel-identical to the 240x160 reference. The rolling barrel turned out to
  be reachable by warp (Deepwood Shrine room 32) and is verified too; the
  tornado's per-line effect is a BG3 scroller rather than affine.
- **Kinstone menu** never runtime-verified: it crashes on cold scripted entry
  at *both* 240x160 and 320x160, so it is the pre-existing kinstone crash chain
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
- ~~**Milestone 1 frame time at 320x160** is unmeasured.~~ **Measured** — see the
  exit-criteria table above. Note the baseline for any future comparison is
  the Spike 1 canvas build (present 6.48 ms mean), *not* the Spike 0 240
  baseline: the canvas cost is paid once and must not be charged twice.
