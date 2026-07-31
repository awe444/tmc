# Affine layers at an expanded viewport

**Date:** 2026-07-31 · **Status:** the mechanism is fixed and two of the named
scenes are verified; two remain unreached, §5. Closes the "title screen affine
sword" carry-forward that has been open since Milestone 1.

## 1. Why the affine path missed every shift

`mode2.c` renders BG0 and BG1 through `virtuappu_mode1_render_text_bg_line`,
which honours the centring clip. BG2's affine path is inline in the same
function and honoured nothing — not the clip, not the OBJ offset, not the
window shift. Every centring channel Milestone 1 and Milestone 2 built simply
did not reach it.

That is the whole of the "sits ~40 px left" bug, and after the vertical UI
centring it was 40 px high as well.

## 2. Shifting an affine layer is not a pixel shift

A pixel shift is wrong here because what the transform maps is a *coordinate*,
not an image. For screen (x, y) the GBA samples

```
tex_x = ref_x + pa*x + pb*y
tex_y = ref_y + pc*x + pd*y
```

so moving the rendered image by (dx, dy) means sampling at `(x - dx, y - dy)`
instead. Doing it that way — rather than by adjusting `ref` and hoping —
stays correct under rotation and scale, because the transform is applied to
the shifted coordinate rather than to its result. That matters for the barrel,
whose `pa`/`pd` change every scanline.

## 3. What was built

No new API. The affine path now honours the **same clip the text path
honours**, so an affine layer is treated as the 240×160-authored surface it is
whenever the host has said so:

- a row outside `[offset_y, offset_y + content_height)` leaves the whole line
  as backdrop;
- columns outside `[offset_x, offset_x + content_width)` are skipped;
- inside, sampling uses `(x - offset_x, line - offset_y)`.

`virtuappu_mode1_get_bg_clip()` is the one addition — an accessor so `mode2.c`
can read the clip `mode1.c` owns.

At 240×160 nothing changes *by construction*: `mapsource_bind_ui` is compiled
out entirely (`#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0`), so no clip is ever
set, the accessor returns NULL, and the affine path is the code it was.

## 4. Verified

**Title screen — exact.** At 320×240 the centred 240×160 box is
**pixel-identical to the 240×160 reference: 0 of 38 400 differ**, and **0 px**
of content falls outside that box. Sword included; the fragments that used to
wrap into the left and right borders are gone.

**Rolling barrel — reached, and it is the one that mattered.** The plan and
three write-ups recorded this scene as unreachable. It is reachable: warp to
Deepwood Shrine room 32 (`0x48`/`0x20`), the inside-barrel room. The HDMA
trace confirms it registers the 8-halfword affine matrix — `io_off=0x20
count=8 bytes/line=16 lines-for-240=3840` — which is the per-scanline table
Spike 9 sized the buffer for and could never exercise. **Spike 9's buffer
growth is therefore now runtime-verified**, not just reasoned.

A/B on the scene itself: without the clip the barrel spans all 320 columns but
sits 40 px left of the centred HUD and of Link, so the barrel's interior does
not line up with the character standing in it. With the clip it occupies
columns 40–279, aligned with the rest of the room and pillarboxed exactly as
any 240-wide room is. Vertically it fills the frame, because in a world view
the clip's vertical span is the whole frame — the same disposition as BG3
(B10) and consistent with how every other world layer behaves.

**Decision — clipped-and-pillarboxed, confirmed by the maintainer
2026-07-31.** Both readings were defensible: filling the width keeps a
full-screen effect full-screen, and clipping keeps it aligned with the room.
Alignment won. It also makes the affine layer behave like every other
240-authored surface rather than being a special case, which is the property
that stops this from needing to be re-decided the next time a viewport
constant moves.

Both 240×160 gates pass: 11/11 waypoints, 0 mismatches in 265 497 600 fetches.

## 5. Not verified, and why

**Vaati's tornado.** Its per-line effect turns out not to be affine at all:
Spike 9's inventory found `vaatiAppearingManager` registers **BG3HOFS with
count=1**, one halfword per line, and types its buffer `BgAffineDstData` only
as a decompilation artifact — it advances by 2 bytes, not 16. So the tornado's
scanline effect is a text-BG scroller already covered by the Spike 9 table
extension. If the scene *also* drives BG2 affine it is covered by this change,
but the scene needs a story state the scripted tester cannot produce
(`CreateVaatiApparateManager` is called from an NPC in `vaati.c`), so this is
reasoned, not observed.

**The screen-shrink cinematic.** Named in the plan; I could not find a
concrete site for it in the source, so I cannot say what it uses or reach it.
Recorded as unidentified rather than as done.

Both are candidates for `record-bug.sh` if a playthrough reaches them.

## 6. The claim these three documents got wrong

`spike9-hdma-240.md`, `spike11-vertical-culling.md` and the play-build README
all state that none of the affine scenes had been reached at 240 rows. That
was true when written and is no longer: the barrel is a warp away, and the
only thing that made it look unreachable was that nothing had tried. The
lesson is small but repeats one already in the tracker — an item recorded as
"needs a human at the controls" is worth re-testing against the tooling
occasionally, because the tooling grows.
