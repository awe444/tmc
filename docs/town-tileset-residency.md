# Plan: make both town tilesets resident (B27)

**Status: planned, not started. Written 2026-08-09 at the end of a session, for
execution by someone starting cold.** Everything here was measured; nothing is
estimated unless it says so. The numbers are the point — re-deriving them cost
most of a session and four playtest round-trips.

Read `docs/viewport-bug-tracker.md` B26 first. This document is what B26 could
not fix.

---

## 1. The defect

Hyrule Town, festival Hyrule Town and Minish Village are too large for one
tileset, so a manager swaps tile *graphics* by camera position, choosing a gfx
group from a table of regions. B26 fixed the *selection*: it now reproduces the
GBA's own choice exactly (0 disagreements over 43,000 camera positions, §4).

What remains is not a selection problem and cannot be fixed by choosing better.
A 320x240 viewport shows **80 more rows and 80 more columns** than the data was
authored against. Near a region boundary that periphery displays scenery
belonging to a region whose tileset is *not* resident, so it is drawn from the
wrong tiles. One tileset can be resident; the window is now bigger than any
single tileset covers.

The maintainer has ruled out sacrificing the 320x240 view in these areas, so the
answer is to make **both** alternatives resident and choose per tile.

**Symptom, for recognising it in a playtest:** walking up and down across a
camera threshold makes a band of scenery flip between correct and garbled.
Reported four times in Hyrule Town (stump tables, market stalls, a wall, and a
fourth spot). The fourth reproduces identically at 240x160 — it is the game's
own tileset swap, and only the periphery is wrong.

---

## 2. Why the cheaper options are closed

**Do not retry any of these.** Each was measured, not argued.

| option | why it is closed |
|---|---|
| A smarter selection rule | Three were simulated over all five town region lists and 43,000 camera positions. Disagreements with hardware: plain first-match on the full viewport **4398**, max-overlap **8316**, max-overlap among disjoint regions **3897**, centred first-match **0**. The shipped rule is already optimal. |
| Widen the authored region gaps | Arithmetically impossible. A gap must be >= the screen extent; `regions0` needs 1072 px in a 960 px room (over by 112), `festRegions0` over by 32, `regions1` over by 64. |
| Load the union of both groups into spare tile indices | The alternatives differ in **252/256, 256/256 and 242/256** tiles for gfx indices 0, 1, 2. Spare indices are **12, 0 and 33**. There is no slack. |
| Re-index the room maps so two copies get distinct indices | Unnecessary under the approach in §5, which is the whole reason that approach was chosen. It would also mean hand-authored map overrides, breaking the rule that everything in `assets/` is derived from `baserom.gba`. |
| Confine these areas to a centred 240x160 (the B22 machinery) | Would work and is cheap, but the maintainer has explicitly refused to give up the wide view here. |
| Find 24 KB inside the GBA's 96 KB of VRAM | All 32 BG VRAM blocks carry data in town. B21 already established there is no free adjacent screenblock pair and that a reallocation prototype overwrote BG0 and garbled the HUD. |

---

## 3. The approach

**Enlarge the emulated VRAM beyond the GBA's 96 KB, keep both groups resident,
and choose the tileset per tile from the tile's own room position.**

This is only possible because we are not on hardware, and it works because of a
fact the GBA never had available: in these areas both world layers are bound to
a **full-room map source**, so the renderer knows each tile's room coordinates —
which is exactly the space the region tables are expressed in.

The map never distinguished the two groups because it never had to; the *camera*
did. Now the *tile position* does, which is strictly better information and
already in hand at the fetch site. That is why no map re-indexing is needed.

Precedent for a port-side channel carrying what the hardware encoding cannot
express: `gOamYExt` (untruncated OAM y, Spike 8) and the map-source path itself.

---

## 4. Measured facts

Everything below was measured on 2026-08-09 unless noted. Frame numbers refer to
`build/play-320x240/fourth_town_glitch.script` unless stated.

### 4.1 Affected areas — all three are structurally identical

| area | id | map source | display mode |
|---|---|---|---|
| Minish Village | `0x01` | `mapsrc_mask=0x6`, both layers **bound** | `dispcnt=0x1740` -> GBA mode 0 |
| Hyrule Town | `0x02` | `mapsrc_mask=0x6`, both layers **bound** | `dispcnt=0x1740` -> GBA mode 0 |
| Festival Town | `0x15` | `mapsrc_mask=0x6`, both layers **bound** | `dispcnt=0x1740` -> GBA mode 0 |

Same layer control words in all three: `bg1ctl=0x1D45`, `bg2ctl=0x1C42`.

**The affine path is not involved.** Mode 0 means `virtuappu_mode1_render_frame`
and the text-BG renderer only. Do not touch `mode2.c`.

### 4.2 Charbases and addressable windows

| layer | BGxCNT | charbase | screenbase | addressable tiles |
|---|---|---|---|---|
| BG1 (top) | `0x1D45` | `0x4000` | `0xE800` | 0..1023 -> `0x4000`..`0xBFFF` |
| BG2 (bottom) | `0x1C42` | `0x0000` | `0xE000` | 0..1023 -> `0x0000`..`0x7FFF` |

A tilemap entry's tile index is 10 bits; charbase is 2 bits of BGxCNT. **That is
the reason extra VRAM is not reachable the GBA's way, and the reason the offset
must be applied by the renderer rather than encoded in the map.**

### 4.3 The tileset slots

From `gHyruleTownTileSetManagerGfxInfos` (`src/manager/hyruleTownTileSetManager.c`).
Each entry loads two 4 KB blocks with `LoadResourceAsync(..., BG_SCREEN_SIZE * 2)`,
`BG_SCREEN_SIZE = 0x800`.

| gfx index | region list | groups | VRAM |
|---|---|---|---|
| 0 | `regions0` | 0 / 1 | `0x0000-0x0FFF` (BG2 idx 0..127) + `0x8000-0x8FFF` (BG1 idx 512..639) |
| 1 | `regions1` | 2 / 3 | `0x1000-0x1FFF` + `0x9000-0x9FFF` |
| 2 | `regions2` | 4 / 5 | `0x2000-0x2FFF` + `0xA000-0xAFFF` |

**Both alternatives of a pair load to the same addresses.** That is the design:
swap the bytes under fixed indices. Festival town has a parallel table,
`gHyruleTownTileSetManagerGfxInfosFestival`, with the same destinations.

### 4.4 Region tables

Format is `{group, x0, y0, w, h}` repeated, terminated by `0xff`, in **room
coordinates** (compared against `scroll - origin`).

- `src/manager/hyruleTownTileSetManager.c`: `regions0`, `regions1`, `regions2`,
  `festivalRegions0`, `festivalRegions1` (empty), `festivalRegions2`.
- `src/manager/minishVillageTileSetManager.c`: `gUnk_08108050` — **8 regions,
  with group ids repeating across disjoint rectangles**, and an `#ifdef EU`
  variant. This is the hardest case; use it as the design driver, not town.

Hyrule Town is 1008 x 960.

### 4.5 Frame budget

Baseline in Hyrule Town at 320x240, full `fourth_town_glitch.script`:

```
logic  : mean 0.429ms  p50 0.359ms  p99 1.479ms  max 5.542ms
present: mean 11.839ms p50 10.369ms p99 16.199ms max 21.383ms
```

**p99 is already past the 16.67 ms deadline.** Frame time is one of the
milestone's two open go/no-go items.

The map fetch at `libs/ViruaPPU/src/mode1.c:498` runs **per pixel**, not per
tile: 153,600 fetches/frame for the two layers at 320x240. `tile_col` changes
only every 8 px, so a per-tile cache reduces the region lookup to **19,200 tests
/frame**. Implement the cache from the start; do not land a per-pixel test
intending to optimise later.

### 4.6 Map-source availability

`--mapsource-report` over a town session: `bound 1621 frames, task!=GAME 1269,
substate!=UPDATE 9`. During gameplay the layers are map-sourced on **1621 of
1630 frames**; the 9 exceptions are mid-transition with the screen fading.

**When there is no map source there are no room coordinates**, so the per-tile
path cannot apply and the layer falls back to the screenblock with the existing
camera-based group. That is 9 frames behind a fade — acceptable, but make it a
deliberate branch with a comment, not an accident.

---

## 5. Subtasks

Each step lists its acceptance criterion. Do not proceed past a failing one.

### Step 1 — Enlarge the emulated VRAM (0.25 d)

- `port/port_gba_mem.c:20` — `u8 gVram[0x18000]`.
- `libs/ViruaPPU/include/cpu/mode1.h:175` — `MODE1_VRAM_SIZE = 0x18000`.

Add a port-only extension **above** the GBA's 96 KB so nothing existing moves,
and allocate a second 8 KB slot per gfx index (3 x 8 KB = 24 KB). Keep the GBA's
address space bit-identical: `gba_write16`/`read` guards in `port_gba_mem.c`
must continue to reject writes outside `0x06000000..0x06017FFF`, so the engine
can never see the extension.

`libs/ViruaPPU` is a **git submodule** with its own remote
(`awe444/VirtuaPPU`). Commit and push it before the main repo, or the pointer
references a commit nobody can fetch.

**Acceptance:** both builds compile; the 240x160 regression gate is unchanged
(11/11, `fetches=265497600 mismatched=0`).

### Step 2 — Publish a per-layer region table to the PPU (0.5 d)

Extend `VirtuaPPUMode1MapSource` (or add a parallel setter) with an optional
list of `{x0, y0, w, h, charbaseOffset}` in **tile** units, plus a count. The
port converts the manager's room-pixel rectangles once per room.

Set it from `port_mapsource.c` where the map source is already bound per layer.
Null/empty means "behave exactly as now" — every other room in the game.

**Watch the units.** The tables are in room *pixels*; the map source works in
*tiles*, and its `origin_x/origin_y` are already applied to `tile_row/tile_col`.
This mismatch is the single likeliest source of an off-by-8 and is exactly the
shape of B22. Assert it with a trace before trusting it — print, for a known
tile, which region the lookup picks, and check it against the region table by
hand for at least one tile in each region of Minish Village's 8-region list.

**Acceptance:** a temporary trace shows the correct region id for hand-checked
tiles in all three areas, including two of Minish Village's repeated-group
rectangles.

### Step 3 — Per-tile charbase offset in the renderer (0.5–0.75 d)

At `libs/ViruaPPU/src/mode1.c:498`, in the map-source branch only:

- cache the last `tile_col`/`tile_row` and the offset chosen for it;
- on a change, walk the region list (first match wins, matching the engine's own
  ordering semantics) and pick the offset;
- add it to `tile_addr` alongside `char_base`.

Leave the screenblock branch alone (§4.6).

**Acceptance:** frame-time delta in town measured against §4.5 and reported. If
`present` mean rises by more than ~0.3 ms, stop and reconsider the cache
granularity before continuing — the budget has no room.

### Step 4 — Managers load both groups, stop swapping (0.5 d)

`HyruleTownTileSetManager_LoadGfxGroup` loads a pair into slot and slot-2 at
room entry; the per-frame `..._UpdateLoadGfxGroups` swap becomes a no-op above
native size. `minishVillageTileSetManager` likewise.

**Gate this on viewport size.** At 240x160 keep the current camera-based swap
untouched, so the regression gate stays meaningful and the shipping build cannot
move. `CheckRegionsOnScreen` keeps its current (correct) behaviour for the
native path and for `houseDoorExterior.c`, which uses the single-region form as
a plain visibility test and must not be touched.

**Acceptance:** with the trace from step 2, the resident pair no longer changes
as the camera moves in any of the three areas.

### Step 5 — Verification (0.5–1 d)

1. All four town recordings in `build/play-320x240/`: `town_sprite_glitch`,
   `town_grpahics_glitch_2` *(sic)*, `town_wall_glitch`, `fourth_town_glitch`.
   No scenery corruption at any threshold.
2. A fresh traversal of Minish Village and festival town — **neither has ever
   been playtested for this**, and Minish Village's 8-region table is the least
   exercised code in the change. Ask the maintainer for a recording.
3. Regression gate at 240x160: 11/11 and `fetches=265497600 mismatched=0`.
4. 240x160 unchanged: capture a town route at that size before and after and
   diff — it must be byte-identical, because step 4 is gated.
5. Frame time in town against §4.5, reported explicitly in the commit message.
6. Refresh both play builds and the APK per `CLAUDE.md`, and update
   `build/play-320x240/README.md`.

---

## 6. Risks

- **Units mismatch in step 2** — highest likelihood, lowest severity if asserted
  early. See step 2.
- **Frame time** — the change adds work to the hottest loop in the port, in an
  area whose p99 is already over the deadline. Step 3's acceptance criterion is
  the tripwire.
- **Minish Village's 8-region table** — repeated group ids across disjoint
  rectangles, plus an EU variant. Design against this table, not town's
  two-entry ones.
- **Submodule ordering** — `libs/ViruaPPU` must be pushed before the main repo.
- **Unexercised areas** — festival town and Minish Village have never been
  playtested for this defect at all. A clean run in Hyrule Town is not evidence
  for them.

---

## 7. If it has to be abandoned

The fallback is confining these three areas to a centred 240x160 using the
machinery already in `port_mapsource.c` (`affine_screen`, around line 646),
which is what B22 does for the rolling barrel. Roughly half a day. The
maintainer has refused it, so it is a last resort and needs asking again rather
than assuming.
