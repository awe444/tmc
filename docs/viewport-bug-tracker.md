# Viewport expansion — bug tracker

Bugs found across both viewport milestones. B1–B9 came from the maintainer
playtesting the 320×160 build; B10–B12 from sweeps during Milestone 2; B13–B15
from the maintainer playtesting 320×240, the last three with recordings.

**Status: Milestone 1 signed off 2026-07-30. Milestone 2 is functionally
complete — see `docs/milestone2-status.md`.** Fourteen of fifteen bugs are
fixed and verified. Only B4 remains **deferred**, and it is reachable the same
way B5 finally was: with `record-bug.sh`.

**Four of these were live in the shipping 240×160 build or through all of
Milestone 1** — B11, B12's horizontal half, B13's horizontal half, and the
iris veto. The expansion exposed them; it did not cause them. This document
stays the authoritative record of what the expansion actually did to the
engine.

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
| B4 | Smith-room sprites/layers wrong at first dialogue | **Deferred** — never reproduced; needs a recording |
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

## B4 — smith-room sprites/layers wrong at first dialogue *(deferred)*

**Never reproduced.** Captures of that room *with* dialogue render correctly
at 320x160 (`scripts/bugs.script` waypoint `B4_smith_dialogue`, and the smith-room
frames in `sweep.script`). The report specifies "the very first character
dialogue", and the scripted run lands on a later one.

**Deferred at Milestone 1 sign-off.** To pick it up, capture a recording — see
"Reproducing B4 and B5" below. Do not spend more time inferring it from prose:
three rounds of that produced no hit.

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

Lessons 7–12 are stated where they were learned: 7 in B11, 8 in B12, 9 in B13,
10 in B14, 11 in B5, 12 in B15.

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
