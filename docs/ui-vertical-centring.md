# Vertical UI centring — Spike 6's twin

**Date:** 2026-07-31, extended 2026-08-02 · **Status:** complete. Every
240×160 authored surface is centred on both axes; the three cases §4 once left
open are all decided and fixed. Milestone 2, follow-on to Spike 11.

UI screens rendered in the top 160 rows of a 320×240 frame with the backdrop
showing beneath. Milestone 1 built the horizontal centring in three channels
and the bug tracker predicted the same three would be needed vertically. They
were, and there is one asymmetry the horizontal case does not have.

## 1. The asymmetry: DY applies to less than DX does

`UI_CENTER_DX` is applied to **every** 240-authored layer, because a layer
reading a 32-tile screenblock covers 256 px and cannot fill 320 either way.

`UI_CENTER_DY` is applied only where the surface is a **whole authored
screen**. The in-game HUD is anchored to the top of the screen, and the top of
a taller screen is still the top — shifting it down 40 px would be wrong.
BG0 carries both, so the discriminator is the screen, not the layer:

```c
clip.offset_x = UI_CENTER_DX;              /* always */
clip.content_width = DISPLAY_WIDTH;
clip.offset_y = ui_screen ? UI_CENTER_DY : 0;
clip.content_height = ui_screen ? DISPLAY_HEIGHT : MODE1_GBA_HEIGHT;
```

`mapsource_is_ui_screen()` draws the line: title, file select, gameover and
staffroll (not `TASK_GAME`), plus the five menu subtasks. World events and the
other cutscenes are world views and are excluded, which is correct for the
camera. `SUBTASK_AUXCUTSCENE` is the one entry that cannot be answered by
subtask alone — see §5.

## 2. The three channels

| Channel | Horizontal (Milestone 1) | Vertical (this change) |
|---|---|---|
| BG layers | `VirtuaPPUMode1BgClip.offset_x` / `content_width` | `offset_y` / `content_height`; a row outside the span rejects the whole line before the column loop |
| Sprites | `virtuappu_mode1_set_obj_offset(dx, …)` and `set_obj_clip(left, right)` | the `dy` argument, which already existed and was passed 0, and a new `set_obj_clip_v(top, bottom)` |
| PPU windows | shifted by `UI_CENTER_DX` at each set site (B9) | shifted by `UI_CENTER_DY` at `figurineMenu.c:124` and `kinstoneMenu.c:298` |

~~The two `cutscene.c` vertical windows are **deliberately not shifted**~~ —
**both now take `UI_CENTER_DY`** (`cutscene.c:247`, `:290`), because the
surface they bound moved. See §5: the legend panels are now centred
vertically, so the rule is satisfied in the other direction. The rule itself
is unchanged and is the thing to keep hold of: *shift the window exactly where
the surface moves*, which is the B9 defect stated as a policy.

The in-game HUD needed no `UI_HUD_SPRITE_DY`. Its sprites are positioned in
screen coordinates and the layer they belong to does not move vertically, so
they are already right.

## 3. Verification

**Scope is exactly right.** Diffing the 320×240 route against the same build
without this change: the four UI waypoints changed and **every world waypoint
is byte-identical**.

| waypoint | class | changed px |
|---|---|---|
| title, fileselect, pause, figurine | UI | 44 786 / 55 850 / 37 694 / 36 312 |
| cutscene, textbox, field, woods, lightray, deepwood, town | world | **0** each |

**Centring is exact.** A 240×160 surface centred in 320×240 occupies rows
40–199 and columns 40–279. Measured content extent: `fileselect`, `pause` and
`figurine` are **exactly that**, on both axes.

`title` is rows 24–199, cols 25–319 — see §4.

Both 240×160 gates pass: 11/11 waypoints, 0 mismatches in 265 497 600 fetches.
At GBA-native height `UI_CENTER_DY` is 0 and `DISPLAY_HEIGHT ==
MODE1_GBA_HEIGHT`, so the clip is the identity either way.

## 4. Two cases left, both on purpose

~~**The title screen's affine sword takes neither shift.**~~ **Fixed** —
`docs/affine-viewport.md`; the affine path now honours the same clip, and the
title's centred box is pixel-identical to 240x160. Original note: it is a BG2
affine layer drawn by `mode2.c`, which the BG clip and the OBJ offset both
missed —
the long-standing carry-forward that had it sitting ~40 px left. It now sits
40 px high as well, and that is why `title` measures 24–199 rather than
40–199. This change did not create the gap; it made the existing gap visible
on the second axis. Measured: the horizontal overspill is **54 columns before
and after**, unchanged. Fixing it means offsetting the affine reference point,
which is not a plain pixel shift and risks the gameplay affine scenes — the
deferred Spike 9 affine work.

~~**The in-game text box still floats.**~~ **Decided and fixed 2026-07-31.**
The maintainer chose "keep its authored position within the centred 240x160
frame" over bottom-anchoring, so it takes `UI_CENTER_DY` at its own source
(`UI_TEXTBOX_DY`), routed through `Port_MapSource_MessageTileShiftY()` so a
popup on an already-centred UI screen does not move twice. Measured at rows
101..138 relative to the centred frame — the original position exactly.

~~**The legend cutscene panels stay top-anchored**~~ **Decided and fixed
2026-08-02** — the maintainer asked for them centred. See §5.

## 5. The legend panels — centred 2026-08-02

The Picori legend's stained-glass cards were the last surface left
top-anchored, on the reasoning in §1: they run inside an AUXCUTSCENE, cutscenes
are world views, and a world view keeps `offset_y = 0`. That reasoning was
sound about cutscenes and wrong about these cards, and the reason is worth
recording because it is not a thing the subtask can tell you.

**The opening cutscene is one subtask wearing two hats.** Its dispatcher table
(`gUnk_080FCCFC`, `cutscene.c:170`) runs the five story panels as overlay
states 0–10 and *then* fades into Zelda walking through Hyrule Field as states
11–14. Both halves are `SUBTASK_AUXCUTSCENE`, so classifying by subtask
necessarily gets one of them wrong: centre the subtask and B3 comes back;
leave it and the cards stay pinned to the top of a 240-row screen.

What separates the halves is mechanical rather than a guess about which
cutscene is playing. A story panel has no world behind it and says so, by
detaching both map layers (`cutscene.c:230-231`, `gMapBottom.bgSettings` and
`gMapTop.bgSettings` set to `NULL`). With no layer bound there is no world view
to fill, and what is on screen is a 240×160 authored surface like any menu.
`SetBGDefaults()` rebinds them when the cutscene switches to its world half,
which is what puts the hat back — so the predicate flips at exactly the frame
the content changes character, with no state of its own to get stale.

`mapsource_is_ui_screen()` therefore answers `SUBTASK_AUXCUTSCENE` with "UI iff
both map layers are detached". Everything else follows from machinery that
already existed: the panels take `offset_y`/`content_height` from the same clip
that was already centring them horizontally, the OBJ offset and vertical clip
come with it, and `Port_MapSource_MessageTileShiftY()` returns 0 because the
card's text rides the shifted BG0 rather than shifting itself — the same
double-shift B1 was, avoided the same way.

The WIN0 vertical pair in `cutscene.c` takes `UI_CENTER_DY` as of this change
(§2), which is B9's rule applied on the axis that finally needed it.

**Verification.** Every captured legend frame is now **pixel-identical to the
240×160 build's rendering of the same frame, shifted 40 px on both axes — 0
mismatches**, against 8 374–20 634 per frame before the change. That is one
measurement covering artwork, text, window and blending together; a window
left behind would have shown up as a brightness seam, which is exactly how B9
presented.

**Scope is exactly right**, checked the same way §3 checked this document's
first pass. Diffing the 320×240 route against the same build without the
change:

| waypoint | class | changed px |
|---|---|---|
| 13 legend frames + `cutscene` | story panel | 11 418 – 25 603 each |
| `zz_f04600` … `zz_f05900` (13 frames) | the **world half of the same cutscene** | **0** each |
| field, textbox, woods, lightray, deepwood, town | world | **0** each |

The middle row is the one that matters: the Zelda-walking segment is byte-
identical with ~76 000 non-black pixels in it, so B3 is not regressed by
centring the half of the cutscene that precedes it.
