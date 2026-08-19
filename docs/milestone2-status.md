# Milestone 2 — status at session close, 2026-08-10

The height expansion (320×160 → 320×240). Every planned spike is landed, plus
the items the plan did not anticipate, and twenty-nine of the thirty
tracked bugs are closed, **B27 included — Hyrule Town, festival town and
Minish Village, playtested and confirmed 2026-08-11.** **Two things are still open and both are judgements rather
than work: frame time, and B21's light shaft.**

**B30 (2026-08-18) closed the last of B27's outer-40 px cases.** A tileset slot
whose regions the centred 240x160 never touches is never loaded, and loading was
the only thing that declared a slot to the per-tile renderer — so it had no
answer at all and drew the previous room's tiles until the camera moved.

**B29 (2026-08-18) is a Milestone 1 regression that the gate could not catch.**
Spike 6 moved `gBG0Buffer` out of `gEwram`, and the ROM `Font` blobs that draw
the area-name banner carry the old EWRAM address in their data, where Spike 6's
source grep could not see it — so the banner has rendered into dead memory since
2026-07-29. The canonical route spawns five of them per run and samples none,
which is why 11/11 stayed green throughout.

**B28 (2026-08-17) is in the tracker but is not a viewport bug at all** — the
asset extractor was truncating room-property blobs at the ROM pointers embedded
in them, so Lon Lon Ranch's locked door had no script and could be walked
through, at 240×160 exactly as at 320×240. It is here because it was reported
off the 320×240 play build, and because it closes three of `CHANGELOG.md`'s open
issues (#28, #37, #40).

The 2026-08-08 session added four (B22–B25), all in or around Deepwood's
rolling barrel, and **two of them — B23 and B25 — turned out to be live at
240×160 as well**, bringing this milestone's tally of pre-existing defects it
merely exposed to six. Neither would have been found without the expansion:
B22 had made the room unplayable, and fixing it is what got anyone to use the
barrel's middle exit on purpose.

Since 2026-08-02 this milestone also grew an **arm64 Android build**
(`android/`). It is not a separate port — it is this viewport on other
hardware, built from the same source list — and it has already earned its keep
as a test surface: B16 and B17 were both reported from it, and neither turned
out to be a platform bug.

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
| **B15** — furniture lit against black through a door/stair fade | bug tracker | Reported with a recording; both room-change substates keep their map source above native size |
| **B5** — interior room-to-room scroll glitches | bug tracker | Deferred since Milestone 1; reproduced from a recording and the slide replaced by a fade above native size |
| **B4** — smith-room sprites at first dialogue | bug tracker | Closed 2026-08-02 as no longer observed — the one entry with no root cause. See B16 for a possible retrospective identification |
| **Android build** | `android/README.md` | arm64-v8a, assets baked in at build time, source list parsed from `xmake.lua`. Gamepad only, no touch |
| **B16** — softlock entering the smith room | bug tracker | Reported from Android, 2 runs in 3; three layered defects, the last being the player losing his facing across the fade's deferral |
| **B17** — Minish interiors render as sprites over black | bug tracker | Third instance of the screenblock-cannot-cover-320 family; needed the tile mutators to maintain the degraded map, not just a relaxed predicate |
| Screenblock-fallback sweep | bug tracker | Asked once which other paths can fall back above native size. On everything reachable, B17 was the only one |
| **B18** — pause map detail view shows only the top of the map | bug tracker | The per-scanline BG3 curtain's band was still in 240x160 rows. The only one of the nine HDMA tables on a UI screen, so the class has one member and it is closed |
| Android diagnostics | `android/README.md` | `--env=NAME=VALUE` carries the `TMC_*` traces onto a device, which had no environment to set them in; every run now logs its build identity; `TMC_STUCK_TRACE` watches the state B16 hung in |
| Playable 240x160 build | `CLAUDE.md` | `build/play-240x160/` alongside the 320x240 one, so "is this the expansion's fault or was it always like that?" is answerable by hand. Installed from the gate's own binary, so it costs no extra configure |
| **B20** — gameplay flashes at 240x160 across a pause transition | bug tracker | The centring clip changed several frames before the picture did. Now changes only on a black frame; the close direction has to be anticipated from `gUI.nextToLoad` because its black frame precedes the state change |
| **B19** — segfault entering a room narrower than the viewport | bug tracker | A `u32` local made a pointer offset unsigned, so a negative camera offset wrapped to +4.29e9 and the pointer walked 8.6 GB. **Reported from Android with a recording and reproduced on desktop on the first replay** — the diagnostics above are why |
| **B22** — rolling barrel unplayable | bug tracker | The player pin measured the barrel's midline from the camera, not the room; 40 px of error in the one room that is exactly viewport-sized. Doors out of reach, barrel self-rolling. Rim sprites in the border left open as a costed decision |
| **B23** — barrel drawn out of step with its own logic | bug tracker | The port had bypassed the barrel's rotation gate since long before this milestone; restoring it exposed the real defect, an affine renderer accumulating `pb`/`pd` on top of a reference HBlank-DMA had already set per scanline. Was wrong at 240x160 too |
| **B24** — lily pad strands the player outside the room | bug tracker | The vehicle's carry state exits on `reload_flags == 0`, which the faded path leaves true for the 32 frames it defers the apply, so the pad quit 28 frames before the room changed. Carry distance then trimmed to a GBA slide's worth |
| **B25** — barrel returns as noise after a pause | bug tracker | A port-only forced buffer→VRAM copy wrote text tilemaps over both of the room's own maps. Frame is now pixel-identical across the pause. Was wrong at 240x160 too |
| **B26** — town scenery from the wrong tileset | bug tracker | Region tables that drive a tileset swap are authored with a gap sized for a 160-row screen; 80 extra rows make two regions match at once and first-match-wins loads the earlier one. Now picks the region covering most of the screen |
| **B27** — scenery in the outer 40 px drawn from a non-resident tileset | bug tracker | The residual B26 could not reach: matching hardware's choice only guarantees the 240×160 the GBA would have shown. Emulated VRAM now carries a bank per tileset group above the GBA's 96 KB, every alternative stays resident, and the renderer picks character data *and palette* per tile from the tile's own room position. All three areas; four Minish Village recordings go from 2.2–10.7% periphery change across a threshold to **0.00%** |

## Gates

Both hold at 240×160 and were re-run before every commit in this milestone,
including all four of the 2026-08-08 barrel and lily-pad commits:

- canonical route: **11/11 waypoints, 0 differences**
- map-source audit: **0 mismatched in 265 497 600 fetches**

**What the gate did not catch, and could not.** B16 lived in
`sub_080797C4`/`gUnk_0811C110`, which the canonical route never executes —
zero events in 13 000 frames. B17 lived in rooms the route never enters. B18
lived on a screen no script could open at all, because `giveallitems` did not
grant the item the menu gates it on. All three passed the gate throughout while
broken. Where a change touches code the route does not reach, say so and find
another argument; for B16's ROM table that argument was that indices 0–3 are
byte-identical, so the change could only alter a read that was previously
undefined. For B18 it was stronger and worth copying — build the shipping
240x160 binary both with and without the change and diff the captures of the
screen itself. Byte-identical across all 14 waypoints is a direct statement
about the code that changed, not an inference from coverage.

## Open item 1: frame time

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

## Open item 2: B21, the Minish Woods light shaft

Fully diagnosed and every route to a fix is blocked — see the tracker for the
account and lesson 19. The short version: the shaft is a 256 px BG3 layer
whose artwork already ends at its own right edge, so a 320 px screen shows
80 px of nothing beyond it. No offset helps because the layer wraps, and the
maintainer has rejected the repeated second shaft that wrapping would give.
A 512-wide BG3 fits exactly and needs no new artwork, but there is no free
adjacent screenblock pair — 28/29/30/31 are BG1/BG2/BG3/BG0 — and prototyping
it overwrote BG0 and garbled the HUD.

So the choice is: accept the shaft ending 80 px short, or reallocate a BG
layer's screenbase and re-check every gfx group's tile destinations against
the new layout. Nothing else was found. No decision is recorded.

## Known remaining, none of them blocking

| Item | State |
|---|---|
| **Vaati's tornado** | Unreached — needs a story state the scripted tester cannot produce. Its per-line effect is a BG3 scroller rather than affine, so it is probably already covered; that is reasoning, not observation. |
| **The screen-shrink cinematic** | Named in the plan; no concrete site found in the source. Unidentified rather than done. |
| ~~**Picori legend panels**~~ | **Centred 2026-08-02** at the maintainer's request — `ui-vertical-centring.md` §5. Pixel-identical to 240x160 shifted 40 px on both axes; the world half of the same cutscene is byte-identical, so B3 stands. |
| **Kinstone menu** | Still never runtime-verified; it crashes on cold scripted entry at 240×160 too, so it is the pre-existing crash chain (CHANGELOG #16). |
| ~~**B4, B5**~~ | **Both closed 2026-08-02.** B5 reproduced from a recording and fixed; B4 closed as no longer observed, the one entry in the tracker without a root cause. |
| **The debug-warp crash** | **Open.** Warping to some destinations segfaults, and a second intermittent crash near teardown is tangled with it. Two real validation gaps were closed (room existence, coordinates inside the room) and coverage widened measurably, but it is not fixed. It is what stops the screenblock sweep from covering more than a fraction of the ~128 areas. |
| **`subTileMap rebound`** | **Never observed.** The one map-source rejection class the sweep could not reach, and therefore the most likely place for a fourth screenblock-fallback instance. Blocked behind the warp crash. |
| **Tile mutation in degraded rooms** | B17's fix makes the mutators maintain that map; the maintenance itself was verified by reading the code, not by driving a mutation. Cutting grass or lifting a pot inside a Minish house is the check nobody has run. |
| **Festival town** | **Never playtested for B27.** Its tables convert correctly and the mechanism engages — verified by debug warp — but nobody has walked its region boundaries at 320×240 and no recording of it exists. The 2026-08-11 playtest covered Hyrule Town and Minish Village, which is where all ten reports came from. |
| **Minish Village at 240×160** | **Never looked at.** Two of its palette groups are needed at once in 308 camera positions at the *shipping* size, so a little of what B27 fixed above may also be visible there. Not reported, not reproduced; the 240×160 play build is where to check. |
| **B21 — Minish Woods light shaft ends 80 px short** | **Open, fully diagnosed, every route blocked.** Not a clip or a clamp: the shaft is a 256 px BG3 layer whose artwork already ends at its own right edge, so at 320 the screen simply got wider. No offset helps — the layer wraps at 256, so a shaft at the right edge implies one at the left, and repeated shafts were rejected by the maintainer 2026-08-07. A 512-wide BG3 would fit exactly (512-320=192, no wrap) and needs no new artwork, but there is no free adjacent screenblock pair: 28/29/30/31 are BG1/BG2/BG3/BG0 and everything below is character data. Prototyped and reverted — it overwrote BG0 and garbled the HUD. Fixing it means reallocating a layer's screenbase. Full account and lesson 19 in the tracker. |
| **`sub_0807D280` reads before its map for short rooms** | **Latent, not reproduced.** B19 fixed the *unsigned wrap* in `case 2`. `case 1` and the `default` branch feed a negative `ydiff` — `-40` in the steady state of any room shorter than the viewport — to `(ydiff >> 4) * 0x100`. Signed, so no wrap and no crash; it reads a kilobyte or two before `gMapDataBottomSpecial` into the screenblock. Above native size the world is drawn from the map source instead, which is likely why nothing has been seen. Wants its own reproduction first. |
| **`gUnk_0811C0F8` / `gUnk_0811C108` read past their end** | **Latent, not reproduced.** Both are four-entry `u16` tables sitting contiguously in ROM with B16's `gUnk_0811C110`, and both are indexed by `direction >> 2`, which reaches 63. On hardware the index wraps into an identical adjacent copy — `0x0811C108[4..7]` is byte-identical to `0x0811C110[0..3]` — so every direction lands on a real value. Ported, each array is its own object. B16 extended only `gUnk_0811C110`. Reachable only on the *swim* branch, so it needs a scene where the player is swimming through a room transition; `TMC_OOB_TRACE=1` reports it and stayed silent across the dungeon-softlock recording. Lesson 13 says the ROM bytes are the specification, so the fix is B16's: extend both with the real bytes, PC_PORT only. |
| **Dungeon-entrance softlock (Deepwood Shrine, room 10 → 6)** | **Open, Android-only, intermittent, and not seen since.** The maintainer's recording traverses the transition and does not hang on desktop; the per-frame camera assertion is clean over it. A device run with `TMC_STUCK_TRACE=1` produced **no `[stuck]` line** and did not reproduce it — but did hit B19 further into the dungeon. Whether the two were ever the same event is unknown. |

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
- **12** — a probe that failed is evidence about the state it ran in, not
  about the change. B15's fix was measured, rejected and reverted one session
  before it became correct.
- **13** — an out-of-bounds read in decompiled code is a platform difference
  waiting to happen. On hardware ROM is contiguous and the read is defined;
  ported, it returns whatever the linker placed next, which is stable per
  toolchain and different between them.
- **14** — a bug that only reproduces on one platform is not necessarily a
  platform bug. B16 cost six rounds of asking what was different about Android
  when the answer was that one accidental read let desktop recover from a fault
  both platforms had.
- **15** — a per-scanline table is indexed by the raster, not by the surface it
  decorates, and lengthening one is not relocating it. Spike 9 lengthened all
  nine of these tables and needed to relocate exactly one, which is how B18
  survived the spike whose whole subject was them.
- **16** — an item the debug actions do not grant is a screen the tooling
  cannot see. `giveallitems` omitted `ITEM_MAP`, so the pause menu silently
  redirected every request for the three map screens and no capture in either
  milestone ever rendered them.

And one that did not need a number because the tracker already had it: after a
round of inference on B13 produced nothing, a recording found it in one pass.
The same held for B5, B15 and B16 — every bug in this milestone that needed a
human at the controls was resolved by a recording, usually after inference had
already failed on it.

**Four of the twenty-five were one assumption reported four times.** B5, B15 and
B17 are all a world layer losing its map source and falling back to a 256 px
screenblock that cannot cover 320. Each was found by a separate playtest report
before anyone asked the general question; when it finally was asked, the sweep
that answered it needed no new code — `TMC_REJECT_TRACE` and
`--mapsource-report` had existed since Milestone 1. Ask the general question
after the second instance, not the third.

**B22 is the fourth, and the sweep did not catch it** — because the sweep asked
about *width*, and B22 is the vertical case. It is also the first of the family
to break gameplay rather than rendering: the rolling barrel held the player on a
midline it measured from the camera instead of from the room, which are the same
number only because that room is exactly 240x160. A room that is exactly
viewport-sized on hardware makes camera-relative and room-relative coordinates
indistinguishable, so every such expression in one is unverified until the
viewport changes. That is the general question this time, and it has not been
swept.
