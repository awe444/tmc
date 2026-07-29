# Spike 2B — Height-only feasibility probe

**Date:** 2026-07-28 · **Status:** complete · Companion to
`viewport-expansion-research-plan.md` §10 (Spike 2B) and
`spike2a-width-probe.md`. Static trace of the §8.1 height column plus two
runtime measurements taken with the `--mapcheck` harness over the canonical
route (12 700 frames) and the mutation walk (11 750 frames).

## 1. §8.1 height column, cell by cell (240-tall target)

| Constraint | §8.1 claim | Verdict | Evidence |
|---|---|---|---|
| BG staging buffer | 32 rows fit; slack collapses 96→16 px | **Confirmed, and the 16 px is measured-sufficient** | See §2: max continuous per-frame vertical camera delta observed is **12 px**; everything larger is a teleport (warp/room entry), which triggers a full-window refill (`gUpdateVisibleTiles=1`), not incremental streaming. Moot for world layers under Option E regardless. |
| Tile streamer | Cadence increase only — softer | **Confirmed; moot under E** | Same streamers as the width case; world layers stop using them. |
| OAM Y 8-bit | Hard blocker; y∈[240,255]→−16…−1 only | **Confirmed, with the severity number the plan asked for (§3)** | `mode1_oam_y` = `attr0 & 0xFF` (`mode1.c:94-97`); wrap `obj_y >= MODE1_GBA_HEIGHT → −256` (`:386-388`). |
| Window regs V | 240 < 255 fits | **Confirmed engine-side; one PPU edit site** | Bottom edge 240 = 0xF0 fits 8 bits. The PPU clamps bottoms against `MODE1_GBA_HEIGHT` (`mode1.c:563-570`) — the same lines Spike 4 already edits for the width clamps. |
| HDMA 160→240 | Blocked; tables need extension | **Confirmed structurally** | `port_hdma_step_line` fires once per rendered line (mode1 render loop bounds by `MODE1_GBA_HEIGHT`); per-frame rewind semantics (`port_hdma.c:110-124`) replay whatever-length table the engine registered. At 240 lines, 160-entry tables run 80 lines past their data. Registration inventory is Spike 9's job as planned. |
| BG VOFS 9-bit | Fine | **Confirmed; moot under E** | World layers sample at camera origin; UI never scrolls past GBA ranges. |
| Camera constants (V) | `0x50`/`0xa8`/height clamp | **Confirmed + the §2A sibling site** | `scroll.c:141` (0x50), `:160` (`origin_y + height - DISPLAY_HEIGHT`), `:515-560` (0xa8 region tests), scripted `:792-835` (literal 80), and the room-entry camera init `playerUtils.c:4408-4412` (`height - 0x50 - 0x50`) — vertical twin of 2A's new width site, added to Spike 10's list. |
| Rooms ≥240 tall | 261/617 (42%) | **Confirmed** | Recomputed in Spike 0 review. |

## 2. Measurement: vertical scroll cadence vs the 16 px slack

Per-frame |Δscroll| on continuous segments (same room, no transition
scroller), across both runs (~24 000 frames, ~10 900 in gameplay):

| |Δy| band | 1–4 | 5–8 | 9–16 | 17–64 | >64 (teleports) |
|---|---|---|---|---|---|
| route | 761 | 0 | 1 (dy=12) | 0 | 8 |
| mutation walk | 490 | 0 | 0 | 0 | 4 |

**Max continuous vertical delta: 12 px** — inside the 16 px budget, with
the >64 events being exactly the scripted warps (which take the
full-refill path, never the incremental streamer). **Verdict: the 32-row
buffer is genuinely sufficient at observed cadence.** Caveats recorded:
one route's worth of play; scripted cutscene pans were not exhaustively
sampled. Under Option E the question is moot for world layers — this
matters only if some UI-path surface ever scrolls vertically at 240 tall.

## 3. Measurement: 8-bit OAM Y severity (blocker 3)

Enabled OAM entries with `attr0.y ∈ [161,239]` — sprites currently
encoding "partially above the top edge" that a 240-line interpretation
would misplace onto the visible bottom half:

| run | frames with ≥1 | entry-frames total | max simultaneous |
|---|---|---|---|
| route | 103 | 118 | 2 |
| mutation walk | 60 | 60 | 1 |

(~1% of gameplay frames; the disabled parking pattern `0x2A0` is excluded
by construction.) **Blocker 3 is real — every one of those frames would
visibly glitch at 240 tall — but the affected population is 1–2 sprites
per affected frame, and per Spike 2A the fix is one pack site
(`port_draw.c:381-382`) plus one unpack site (`mode1.c:386-388`), both
port-owned.** The Spike 8 DoD target "count drops to zero" is now
concretely re-measurable with this same counter.

## 4. Height-only effort re-estimate

| Work | Was | Now | Basis |
|---|---|---|---|
| Spike 8 OAM Y | 2–3 d | **1–1.5 d** | Native widening of two owned sites + re-run this counter to zero; sa2 port abandoned (2A §2). |
| Spike 9 HDMA 240 | 2 d | **2 d** | Unchanged; registration inventory still the real work. |
| Spike 10 camera V | 1–2 d | **1–1.5 d** | +1 init site; consumer blast radius shared with 2A's 16-site list. |
| Spike 11 culling/integration | 2–3 d | **2–2.5 d** | Entity-side only (OAM side structurally safe, 2A §2). |
| **Milestone 2 total** | 7–10 d | **6.5–8.5 d** | |

## 5. Axis order — CONFIRMED: width first (§8.3 stands)

The §8.3 argument was risk-weighted: do the axis whose blockers are
cheap and reversible first, defer the two dangerous ones (OAM Y, HDMA)
until the map-sampling path is proven. Both probes moved the numbers but
not the ordering logic:

- Width-side blockers all resolved *at or below* estimate (2A §4), and
  the map-sampling premise the width milestone rides on is now
  runtime-verified (Spike 2).
- Height retains the only two blockers with no partial-credit failure
  mode: OAM Y (severity now quantified, §3) and HDMA (structurally
  confirmed, §1). Both are smaller than feared but still the riskiest
  items on the board.
- The counter-argument (height covers 42% of rooms vs 28%) is unchanged
  and still loses on risk weighting.

**Width first. Milestone 1 may begin.**

## 6. DoD checklist

- [x] Every §8.1 height cell confirmed/corrected with citations (§1).
- [x] Max per-frame vertical delta measured (12 px continuous) and
      compared against the 16 px slack; buffer-sufficiency verdict
      recorded (§2).
- [x] Unrepresentable-sprite count measured: 103 frames / 118 entries /
      max 2 simultaneous on the route (§3) — the severity number for
      blocker 3, re-measurable via `--mapcheck` for Spike 8's
      drops-to-zero DoD.
- [x] Height-only effort re-estimated: Milestone 2 = 6.5–8.5 d (§4).
- [x] **Axis order confirmed in writing: width first** (§5).
