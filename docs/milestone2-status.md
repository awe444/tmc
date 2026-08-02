# Milestone 2 — status at session close, 2026-08-02

The height expansion (320×160 → 320×240). Every planned spike is landed, plus
six items the plan did not anticipate. **One thing is still open and it is a
judgement, not work: frame time.**

Read this first; the individual spike write-ups are the detail behind each row
and several carry inline "superseded" notes pointing back here.

## What landed

| Piece | Write-up | Result |
|---|---|---|
| `TMC_VIEW_H` build switch | — | `TMC_VIEW_W=320 TMC_VIEW_H=240` on both xmake commands |
| Spike 8 — OAM y | `spike8-oam-y.md` | Untruncated y beside OAM; 0 unresolved over ~200k entries, 0 rescued at both 160-line builds |
| Spike 9 — per-scanline tables | `spike9-hdma-240.md` | All nine tables extended; **B11** found — circular windows had been dead since Spike 4 |
| Spike 10 — vertical camera | `spike10-vertical-camera.md` | Camera centres and clamps on both axes; 0 out-of-range over ~14k frames |
| Spike 11 — vertical culling | `spike11-vertical-culling.md` | 12 tall rooms walked, 0 vertical wrap; **B12** found |
| Vertical UI centring | `ui-vertical-centring.md` | UI screens centred exactly (rows 40–199, cols 40–279) |
| Affine layers | `affine-viewport.md` | Title pixel-identical to 240×160; rolling barrel reached and aligned |
| Iris width | `spike9-hdma-240.md` §6 | Circle no longer sliced at x=240 |
| Text box placement | `ui-vertical-centring.md` §4 | Keeps its authored position inside the centred frame |
| **B13** — NPC pop-in | bug tracker | Found from a maintainer recording; confirmed fixed 2026-08-01 |
| **B14** — UI side borders forced black | bug tracker | Reported by the maintainer 2026-08-02; all four bands now one colour |
| Legend panels centred vertically | `ui-vertical-centring.md` §4-5 | The last deferred vertical case; decided and fixed 2026-08-02 |

## Gates

Both hold at 240×160 and were re-run before every commit in this milestone:

- canonical route: **11/11 waypoints, 0 differences**
- map-source audit: **0 mismatched in 265 497 600 fetches**

## The one open item: frame time

Canonical route, 12 700 frames, headless dummy, uncapped, release, n=3 — the
same method as the Spike 0/1 baselines.

| Build | present mean | p99 | max |
|---|---|---|---|
| Spike 1 canvas baseline | 6.48 ms | — | — |
| Milestone 1, 320×160 | 7.19 ms | 9.25–11.12 | 12.5–14.7 |
| **Milestone 2, 320×240** | **9.15–9.31 ms** | **12.39–12.76** | **17.66–21.66** |

Two facts, both for the maintainer to weigh:

- **+41% over the canvas baseline**, against the +25% that was Milestone 1's
  stated criterion (Milestone 1 itself came in at +10.9%). Milestone 2 was
  never given a budget of its own — the Spike 11 DoD asks only that the number
  be recorded. Against 320×160 it is +27% for 1.5× the pixels, i.e. sub-linear,
  which is the expected shape for a present path whose texture upload is fixed.
- **Peak frames now exceed the 16.67 ms deadline** — 17.7–21.7 ms, where
  320×160 peaked at 14.7. Mean and p99 sit comfortably inside, so this is
  occasional rather than sustained, but it is the number most likely to be felt
  as a stutter.

No go/no-go is recorded. That is the maintainer's call and it rests on this.

## Known remaining, none of them blocking

| Item | State |
|---|---|
| **Vaati's tornado** | Unreached — needs a story state the scripted tester cannot produce. Its per-line effect is a BG3 scroller rather than affine, so it is probably already covered; that is reasoning, not observation. |
| **The screen-shrink cinematic** | Named in the plan; no concrete site found in the source. Unidentified rather than done. |
| ~~**Picori legend panels**~~ | **Centred 2026-08-02** at the maintainer's request — `ui-vertical-centring.md` §5. Pixel-identical to 240x160 shifted 40 px on both axes; the world half of the same cutscene is byte-identical, so B3 stands. |
| **Kinstone menu** | Still never runtime-verified; it crashes on cold scripted entry at 240×160 too, so it is the pre-existing crash chain (CHANGELOG #16). |
| **B4, B5** | Still deferred from Milestone 1. Both now have a viable route: `record-bug.sh`, which found B13 in one pass. |

## What this milestone changed about how the work is checked

Four defects in this milestone were **live in the shipping 240×160 build or
through all of Milestone 1**, not introduced by the height work: B11 (circular
windows dead since Spike 4), B12's horizontal half, B13's horizontal half, and
the iris veto that was Spike 9's own validity key. The expansion did not break
them; it made them visible.

That is worth stating plainly because it changes what the regression gate is
for. The gate proves the shipping build did not move. It cannot prove the
shipping build was right — B11 rendered a near-black screen on 64 frames of
the canonical route and the gate passed 11/11 throughout, because none of the
11 waypoints lands on those frames.

The lessons added here, in the tracker's numbering:

- **7** — a gate made of still frames cannot see a defect that only exists
  mid-transition.
- **8** — a conversion sweep's own report of what it converted is not
  evidence.
- **9** — ask what a predicate *gates*, not just what it is called.
  `CheckOnScreen` decides whether to draw; `CheckRectOnScreen` decides whether
  the entity exists.

And one that did not need a number because the tracker already had it: after a
round of inference on B13 produced nothing, a recording found it in one pass.
The tooling for that has existed since Milestone 1 and B4/B5 are still open
for want of using it.
