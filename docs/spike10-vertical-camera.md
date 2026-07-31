# Spike 10 — Camera and clamping, vertical

**Date:** 2026-07-31 · **Status:** complete · Milestone 2, research plan
§10.2. The vertical twin of Spike 5, and it inherits Spike 5's hard-won
lesson: B7 was a softlock caused by one file that a conversion sweep missed.

## 1. What was converted

`VIEWPORT_CAM_MIN_Y` / `VIEWPORT_CAM_MAX_Y` mirror the horizontal pair
exactly. Both reduce to the engine's original expressions at 240x160: the
shortest room in the game is 160 tall, so `height <= VIEWPORT_HEIGHT` can only
be an equality there and the centring term is zero.

| Site | Was | Now |
|---|---|---|
| `scroll.c:142` | `camera_target->y - 0x50` | `- VIEWPORT_HALF_HEIGHT` |
| `scroll.c:147,157` | `controls->origin_y` as the min clamp | `VIEWPORT_CAM_MIN_Y(origin_y, height)` |
| `scroll.c:162` | `origin_y + height - VIEWPORT_HEIGHT` | `VIEWPORT_CAM_MAX_Y(origin_y, height)` |
| `scroll.c:516,549` | `scroll_y + 0xa8` region tests | `VIEWPORT_REGION_HEIGHT` |
| `scroll.c:810-820` `sub_08080974` | y half: bare `origin_y`, literal `80`, **no clamp** | half-height + both clamps, matching the x half |
| `scroll.c:852-862` `sub_080809D4` | same shape | same |
| `script.c:1985-1988` `WaitForCameraTouchRoomBorder` | `origin_y` / `origin_y + height - VIEWPORT_HEIGHT` | `VIEWPORT_CAM_MIN_Y` / `MAX_Y` |
| `script.c:2312` `sub_0807FBA0` | `scroll_x + 120`, `scroll_y + 80` | `VIEWPORT_HALF_WIDTH/HEIGHT` |
| `playerUtils.c:4408-4415` | `0x50`, `height - 0x50 - 0x50` | half-height, `VIEWPORT_CAM_MIN_Y/MAX_Y(0, height)` |

Two of these are worth calling out.

**`script.c:1985` is the B7 site on the other axis.** It predicts where the
camera will come to rest and waits for `scroll_y` to equal it *exactly*, so it
has to apply the same clamp `Scroll1` does or the equality never holds and the
script waits forever. Milestone 1 fixed the x half and left the y half reading
`origin_y`, which is the same latent softlock at a taller viewport.

**The two scripted camera helpers had no vertical clamp at all.** Their x
halves clamp to `[MIN_X, MAX_X]`; the y halves just computed `var1 - 80` and
trusted it, which was safe only because a room could never be shorter than the
screen. Both now clamp.

`script.c:2312` was missed by Milestone 1 on *both* axes — it places an entity
at the camera centre with literal `120`/`80`.

## 2. Verification: camera geometry per height cluster

`TMC_CAMTRACE` now reports both axes, with an out-of-range assertion on each.
The §6 height clusters, measured through the real camera:

| Room | camy | legal range | classification |
|---|---|---|---|
| 480x160 (Hyrule Field #9) | −40 | [−40, −40] | SHORT(centred) |
| 1008x192 (Lake Hylia #1) | −24 | [−24, −24] | SHORT(centred) |
| 480x208 (Hyrule Field #0) | −16 | [−16, −16] | SHORT(centred) |
| 240x240 (House Interiors 1 #11) | 0 | [0, 0] | SHORT(centred) |
| 320x320 (Town Minish Holes #18) | 40 | [0, 80] | scrollable |

A room shorter than the viewport collapses to a single legal position — it
cannot scroll, and that position is `−(240 − height)/2`, i.e. centred. A taller
room keeps a real span. **No axis reported out of range in any room.**

Visually confirmed on a set of 240-wide interiors, where the bands are
unobstructed: 240x160, 240x192 and 240x240 give vertical bands of 40, 24 and 0
px against the predicted `(240 − height)/2`, with the horizontal bands holding
at 40 throughout.

## 3. Verification: the clamp holds every frame, not just on entry

`TMC_CAMTRACE` fires once per room, which catches a wrong resting place but
not a clamp that fails mid-scroll. `--mapcheck` gained a per-frame assertion of
the same invariant:

| Build | gameplay frames checked | x out of range | y out of range |
|---|---|---|---|
| 240x160 | 3915 | **0** | **0** |
| 320x240 | 7643 | **0** | **0** |

**Gating this correctly took two attempts, and the first was noise.** Asserting
on every `TASK_GAME` frame reported 2746 failures with a 2480 px overshoot —
and reported *exactly the same numbers at 240x160*, which is what gave it away.
Room setup leaves `origin` and `scroll` briefly inconsistent, so the
single-room invariant is meaningless there. The assertion now reuses the
map-source predicate, which already encodes "this room's map is authoritative
right now"; the identical-at-both-sizes result is the reason to distrust a
measurement rather than the code.

## 4. Verification: streaming at 240 rows

Spike 2B measured a maximum continuous vertical camera delta of 12 px against
the 32-row buffer's 16 px of slack. Re-measured after this change, at 320x240:
**max continuous dy is still 12**, bands `1-4/5-8/9-16/17-64/>64 =
735/0/1/0/8`. Centring the camera changes where it rests, not how fast it
moves, so the slack finding carries over unchanged.

A scroll-stress run through Hyrule Field (1008x688) shows no tearing: the
row-to-row colour-change count holds a flat max/mean ratio of 2.5 across the
pan, where a torn band would spike.

## 5. DoD

- [x] Every vertical camera constant replaced by a viewport-derived
      expression (§1), including the two helpers that had no vertical clamp
      and the `WaitForCameraTouchRoomBorder` twin of B7.
- [x] One room per height cluster verified — 160, 192, 208, 240, 320 — short
      rooms centred with equal borders, taller rooms scrolling and clamping
      (§2), plus a per-frame assertion over the whole route (§3).
- [x] Vertical streaming verified against Spike 2B's 16 px slack: max
      continuous dy unchanged at 12, no tearing (§4).
- [x] Both 240x160 gates pass: 11/11 waypoints, 0 mismatches in 265 497 600
      fetches.

## 6. Not done here

- **UI screens are still top-anchored.** Title, file select, pause and the
  figurine gallery are 240x160-authored surfaces centred horizontally by
  `UI_CENTER_DX`; there is no `UI_CENTER_DY`, so they sit in the top 160 rows
  with backdrop beneath. That is Spike 6's vertical twin, not the camera's
  job, and it is the most visible remaining gap at 320x240.
- **BG3 fills the out-of-room bands.** In a room shorter than the viewport the
  world layers correctly stop at the room edge, but BG3 is deliberately
  unclipped during a world view (B10) and covers the full frame, so the bands
  above and below a short *overworld* room show overlay content rather than
  backdrop. Interiors, which have BG3 off, show clean bands. Whether B10's
  rule should gain a vertical exception is a decision, not a bug — recorded
  for Spike 11.
