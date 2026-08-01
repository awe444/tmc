# Spike 11 — Vertical culling and integration

**Date:** 2026-07-31 · **Status:** complete · Milestone 2, research plan
§10.2. The vertical mirror of Spike 7, plus Milestone 2's integration pass.

## 1. The culling audit found a live defect on *both* axes

Spike 7 reported that it converted the culling literals and that "vertical
siblings were converted at the same time; they are no-ops today". Re-auditing
rather than trusting that turned up the one it missed, and it is the most
important thing in this spike.

`CheckOnScreen` (`port/port_draw.c`) is the engine's per-entity visibility
test and the gate on whether an entity is drawn at all. It biased the screen
coordinate by a `0x3F` slack margin and compared against **raw literals**:

```c
x += 0x3F;  if ((u32)x >= 0x16E) return 0;   /* 0x16E = 240 + 2*0x3F */
y += 0x3F;  if ((u32)y >= 0x11E) return 0;   /* 0x11E = 160 + 2*0x3F */
```

The literals are *screen size plus twice the margin*, so at an expanded
viewport they cull a band at the far edge that is genuinely on screen —
**17 px at the right at width 320, and 17 px at the bottom at height 240**.
Entities blink out shortly before reaching the edge.

The horizontal half was live throughout Milestone 1. It is a narrow band and
Spike 7's artifact scans looked for wrap-shaped repetition and black columns,
neither of which a missing sprite produces — which is why 12 rooms of walking
did not catch it.

Both are now `VIEWPORT_WIDTH/HEIGHT + ONSCREEN_MARGIN * 2`. The margin stays
`0x3F`: it is slack for a sprite whose origin has left the screen while its
body has not, which is a property of sprite size, not of the viewport.

**Evidence.** A/B over the canonical route at 320×240 differs on exactly two
waypoints, both inside the predicted bands: `woods` at rows 227–239 (bottom
band, predicted ≥223) and `textbox` at columns 294–313 (right band, predicted
≥303). The `woods` difference is a heart object whose sprite was being culled,
leaving only the background pedestal.

Also converted, found in the same audit: five "whole screen" **vertical
window** ranges still hardcoded to 160 — `common.c` ×2 and `lightRay.c` ×3 —
against `lightRay.c:130`, which Milestone 1 *had* converted to
`WIN_VIEWPORT_HEIGHT`. One of four in the same file: the same partial-sweep
shape as B7. `scroll.c:299` was `WIN_RANGE(0, 0xf0)`, which happens to equal
240 and so covered the screen at both sizes; converted for intent.

## 2. Twelve rooms ≥240 tall, walked end to end

Weighted to the high-playtime overworld per §6, one room per major area, each
captured on entry and after a down-sweep and an up-sweep — 36 captures:

Minish Woods, Minish Village, Hyrule Town, Hyrule Field, Castor Wilds, Ruins,
Mt Crenel, Castle Garden, Cloud Tops, Royal Valley, Veil Falls, Lake Hylia.

Scanned for the two artifact classes that indicate a real fault:

- **Vertical wrap-shaped repetition** (a row identical to one 256 px earlier,
  the BG wrap period): **0 across all 36 captures**.
- **Fully black rows** inside a room taller than the viewport: 3 captures, all
  Royal Valley — and **disproven**. It is the dark-graveyard lantern, a
  circular window around the player. Measured at both sizes the lit region is
  **identical: 71 lit rows, 45 px at its widest**. The extra black rows are
  more darkness around the same circle. Spike 7 reached the same disposition
  for this room horizontally; this is the tighter form of that measurement.

No crash across the 12 rooms, and the per-frame camera assertion held: **0 out
of range on either axis over 14 432 gameplay frames**.

## 3. Canonical route at 320×240

Completes without crash, three times over, 12 700 frames each. Wrap scan on
the 11 waypoints: **0 vertical wrap rows**.

The horizontal scan flagged 16 columns on four waypoints, and **three of the
four are an artifact of the scan, not of the render**: on `pause`, `figurine`
and `fileselect` all 16 are solid border columns at x=280–295 matching the
border at x=24–39. A uniform border band matches itself at any period. This is
the caveat CLAUDE.md already records about black-keyed metrics, in a new
costume.

`title` is the real one: 15 of its 16 carry content. It is the **unclipped
affine path** — the carry-forward title-screen sword — and it measures
**identical at 320×160 and 320×240** (16 columns, 1 solid), so it is a
width-driven Milestone 1 inheritance, not anything the height work introduced.

## 4. Frame time

Canonical route, 12 700 frames, headless dummy video, uncapped, release, n=3 —
the same method as the Spike 0/1 baselines.

| Build | present mean | present p99 | present max |
|---|---|---|---|
| Spike 1 canvas baseline (240×160 into a 320×240 canvas) | 6.48 ms | — | — |
| Milestone 1, 320×160 | 7.19 ms | 9.25–11.12 ms | 12.5–14.7 ms |
| **Milestone 2, 320×240** | **9.15 / 9.31 / 9.17 ms** | **12.39–12.76 ms** | **17.66–21.66 ms** |

Logic is unchanged at 0.15–0.30 ms mean. Total ≈ **9.4 ms against the 16.67 ms
budget (56%)**.

Two things the maintainer should weigh rather than have decided for them:

- **+41% over the canvas baseline**, against the +25% that was Milestone 1's
  stated criterion (Milestone 1 itself came in at +10.9%). Milestone 2 has no
  stated budget of its own — the Spike 11 DoD asks only that the number be
  recorded and compared. Against Milestone 1's 320×160 it is +27% for 1.5× the
  pixels, i.e. sub-linear, which is the expected shape for a present path
  whose texture upload is a fixed cost.
- **Peak frames now exceed the 16.67 ms deadline** — max 17.7–21.7 ms, where
  320×160 peaked at 12.5–14.7 ms. Mean and p99 are comfortably inside it, so
  this is occasional rather than sustained, but it is a real change and it is
  the number most likely to be felt as a stutter.

## 5. Decision: BG3 stays unclipped vertically

Spike 10 deferred this here. In a room shorter than the viewport the world
layers correctly stop at the room edge, but BG3 covers the whole frame, so the
bands above and below a short *overworld* room show overlay content rather
than backdrop. Interiors have BG3 off and show clean bands.

**Keep B10's rule; do not add a vertical exception.** B10 established that
BG3 during a world view is a gameplay overlay in two families — world-locked
ones that align via `scroll`, and screen-fixed ones that are meant to cover
the viewport — and that clipping it moved the world-locked family 40 px and
broke alignment *in the middle of the screen*. The vertical argument is the
same argument; a vertical exception would reintroduce the horizontal bug's
twin. The bands showing overlay rather than backdrop is what a full-screen
overlay means.

## 6. DoD

- [x] At least 10 rooms ≥240 tall walked end to end — **12**, 36 captures,
      issues logged (§2). One flag raised and disproven.
- [x] Canonical route completes at 320×240 without crash or visual
      regression (§3), with the one flagged item traced to a Milestone 1
      carry-forward and shown identical at 320×160.
- [x] Mean and p99 frame time recorded and compared (§4).
- [ ] **Go/no-go on shipping — maintainer's call**, see §7.
- [x] Both 240×160 gates pass: 11/11 waypoints, 0 mismatches in 265 497 600
      fetches.

## 7. Go/no-go input

Not a decision I should take. What the evidence supports:

**Working at 320×240:** world rendering at full size with no wrapping in 36
tall-room captures; camera centring and clamping on both axes with a
zero-violation per-frame assertion over ~14k frames; sprite y above the top
edge; per-scanline tables and the circular windows; entity culling to the true
viewport edge.

**Known not done**, each recorded with its reasoning:

1. ~~**UI screens are top-anchored**~~ — **done**, `docs/ui-vertical-centring.md`.
2. ~~**Affine scenes unconverted**~~ — **done for the title and the barrel**
   (`docs/affine-viewport.md`); the barrel turned out to be one warp away.
   Vaati's tornado and the screen-shrink cinematic remain unreached.
3. ~~**The iris is a 240-wide circle** on a 320-wide screen.~~ — **fixed**; it
   was Spike 9's validity key, not the authored radius (see B11's neighbour in
   `spike9-hdma-240.md` §6).
4. **Peak frame time** now exceeds the 60 fps deadline occasionally (§4).

My reading at the time: not shippable, blocker (1). **Superseded — (1), (2)
for the reachable scenes, and (3) all landed after this was written, along
with B13 from playtesting. Only (4), frame time, is still open.** See the
research plan's Milestone 2 status for the current picture.
