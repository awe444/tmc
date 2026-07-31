# Vertical UI centring — Spike 6's twin

**Date:** 2026-07-31 · **Status:** complete for whole authored screens; two
cases deliberately left, §4. Milestone 2, follow-on to Spike 11.

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

`mapsource_is_ui_screen()` already draws exactly the needed line: title, file
select, gameover and staffroll (not `TASK_GAME`), plus the five menu subtasks.
Cutscenes and world events are world views and are excluded — which is correct
for the camera but has a consequence, §4.

## 2. The three channels

| Channel | Horizontal (Milestone 1) | Vertical (this change) |
|---|---|---|
| BG layers | `VirtuaPPUMode1BgClip.offset_x` / `content_width` | `offset_y` / `content_height`; a row outside the span rejects the whole line before the column loop |
| Sprites | `virtuappu_mode1_set_obj_offset(dx, …)` and `set_obj_clip(left, right)` | the `dy` argument, which already existed and was passed 0, and a new `set_obj_clip_v(top, bottom)` |
| PPU windows | shifted by `UI_CENTER_DX` at each set site (B9) | shifted by `UI_CENTER_DY` at `figurineMenu.c:124` and `kinstoneMenu.c:298` |

The two `cutscene.c` vertical windows are **deliberately not shifted**: the
legend panels are classified world, so their surface takes no vertical shift,
and shifting the window without the surface is precisely the B9 defect.
Shift the window exactly where the surface moves.

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

**The title screen's affine sword takes neither shift.** It is a BG2 affine
layer drawn by `mode2.c`, which the BG clip and the OBJ offset both miss —
the long-standing carry-forward that had it sitting ~40 px left. It now sits
40 px high as well, and that is why `title` measures 24–199 rather than
40–199. This change did not create the gap; it made the existing gap visible
on the second axis. Measured: the horizontal overspill is **54 columns before
and after**, unchanged. Fixing it means offsetting the affine reference point,
which is not a plain pixel shift and risks the gameplay affine scenes — the
deferred Spike 9 affine work.

**The in-game text box still floats.** It is authored near the bottom of 160
rows and rides BG0 with the top-anchored HUD, so at 240 rows it sits
mid-screen. Neither a uniform shift nor none is right for both surfaces on
that layer; moving it needs a shift at its own source, the way
`UI_HUD_SPRITE_DX` handles HUD sprites. Not attempted here — it is a
different decision (where *should* a text box sit on a taller screen?) rather
than a missing conversion, and it wants an answer before an implementation.

**The legend cutscene panels stay top-anchored**, following from §1: they are
classified world, so they take the horizontal shift but not the vertical.
Whether a 240×160 authored *panel* inside a world-classified cutscene should
be vertically centred is the same question as the text box, and is best
answered with it.
