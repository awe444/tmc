# Viewport Expansion Research Plan — 240×160 → 320×240

**Status:** research plan, no implementation committed
**Target:** PC port only (`PC_PORT`). GBA-native build target is *not* preserved.
**Scope:** USA assets only.
**Goal:** render a 320×240 viewport. Rooms smaller than the viewport in either
axis are centered with borders.

---

## 1. Why this document exists

The obvious framing — "change 240 to 320 and 160 to 240" — is wrong here, and
the repo already contains evidence of that. A Phase 1 widescreen spike exists
and deliberately stops short of real expansion, with a comment naming the
blocker. This plan enumerates the actual options, grades them against evidence
gathered from the codebase, and proposes an ordered sequence of spikes to
confirm the recommendation before committing to a build.

The single most important finding is in §5: **the engine already maintains a
live, full-room, runtime-accurate 8×8 tilemap** (`gMapDataBottomSpecial` /
`gMapDataTopSpecial`). That changes which option is cheapest, and it is not
something the existing Phase 1 comments account for.

---

## 2. What already exists

### 2.1 Rendering stack

The port replaces GBA hardware with a software PPU:

- `libs/ViruaPPU` — submodule (`MatheoVignaud/VirtuaPPU-experimental`; readable
  fork at `awe444/VirtuaPPU`). ~1730 LOC total; `src/mode1.c` is 741 lines and
  does all text-BG, OBJ, and composite work.
- `port/port_ppu.cpp` — drives VirtuaPPU once per VBlank, then handles
  upscaling (xBRZ), CRT/LCD filters, fit-rect, and SDL present.
- `port/port_hdma.c` — per-scanline HBlank-DMA simulation, driven through
  `virtuappu_mode1_pre_line_callback`.

VirtuaPPU is modified at build time by four patches in `port/patches/`, applied
by `xmake.lua:377-430`. This is a fragile mechanism (context diffs against a
moving submodule) and is itself a candidate for replacement — see Option F
notes in §7.

### 2.2 The Phase 1 widescreen spike

`xmake.lua:39-43` exposes a `widescreen_width` option that compiles VirtuaPPU
with `-DMODE1_GBA_WIDTH=N`. What it actually does:

- `port/patches/viruappu-widescreen.patch` makes `MODE1_GBA_WIDTH` overridable
  and introduces `MODE1_GBA_VIEWPORT_X` / `MODE1_GBA_BG_CLIP_X`, both pinned to
  240. BG composition force-blacks past column 240; OAM is clipped at 240.
- `port/port_ppu.cpp:411-423` then uniformly stretches the 240-px frame across
  the wider framebuffer.

So Phase 1 is **a stretch, not an expansion**. No additional world is revealed.
Height is not parameterized at all — `MODE1_GBA_HEIGHT = 160` is still a plain
enum constant in `include/cpu/mode1.h`, so the 240 half of the target is
entirely unaddressed.

The patch header states the intended Phase 2: *"Real widescreen needs a 64-tile
`BGCNT_TXT512x256` (sa2-style) extension to the engine."* Part of this plan is
to test whether that is in fact the right Phase 2 — §5 suggests it may not be.

---

## 3. Where the viewport is baked in

Evidence gathered, with locations, so spikes can start from known ground.

| Area | Location | What is hardcoded |
|---|---|---|
| Display constants | `include/gba/defines.h:83-84` | `DISPLAY_WIDTH 240`, `DISPLAY_HEIGHT 160`. Only 65 references across `src`/`include`/`port` — most viewport math uses raw magic numbers instead. |
| Camera centering | `src/scroll.c:103,141` | `target->x - 0x78` (120), `target->y - 0x50` (80) — half-viewport constants. |
| Camera clamping | `src/scroll.c:122` | `origin_x + width - 0xf0` (240). |
| Scroll-region tests | `src/scroll.c:515-560` | `scroll_y + 0xa8` (168), `scroll_x + 0xf8` (248). |
| Scripted camera | `src/scroll.c:792,796,800,827,831,835` | literal `120`. |
| Script camera clamp | `src/script.c:1976-1977` | `origin_x + width - DISPLAY_WIDTH`. |
| BG tile streaming | `src/screenTileMap.c` (343 lines) | `0x20` row strides, `& 0x1f` wrap masks, `0x1e` (30 columns), `0x16` (22 rows), DMA row shifts. Register-named decomp output. |
| BG staging buffers | `include/vram.h:58-68` | `gBG0/1/2Buffer` = `0x400` u16 (32×32); `gBG3Buffer` = `0x800` (64×32). |
| BG→VRAM upload | `src/interrupts.c:104-108` | VBlank DMA of `bg->subTileMap` into a screenbase; size from `gUnk_080B2CD8[dest >> 14]`, indexed by the BGCNT screen-size bits. |
| HUD composition | `src/ui.c:233-559` | Direct writes into `gBG0Buffer` at hardcoded linear offsets (`0x258`, `0x278`, `0x219`, `0x239`, …) that assume a 32-wide stride. |
| Window regs | `src/object/lightRay.c`, `lightDoor.c`, `cutscene.c`, `menu/figurineMenu.c`, `menu/kinstoneMenu.c`, `common.c`, `interrupts.c` | `WIN0H`/`WIN0V` pack two 8-bit edges into one `u16` (`0x80f0`, `(a << 8) \| b`). |
| Memory map | `port/port_gba_mem.h:11-17,47-51` | `gVram[0x18000]` fixed; `gba_TryMemPtr` bounds VRAM at `0x06018000`. |

Aggregate scale of the BG-buffer coupling: **101 `gBG[0-3]Buffer` references
across 22 files, 42 of them stride-sensitive indexed accesses.**

---

## 4. Constraint inventory

Seven distinct blockers. They are not equally hard, and — critically — they do
not all apply to every option.

1. **BG staging buffers are 32×32.** 320×240 needs 40×30 tiles visible, so ≥42×32
   with a scroll margin. Worse, under `PC_PORT` these are `#define`s aliasing
   fixed offsets inside `gEwram[]` (`0x34CB0`, `0x21F30`, `0x344B0`, `0x1A40`),
   so growing them collides with neighbouring decomp data at fixed addresses.
   *Mitigating find:* `include/vram.h:65-68` already carries a non-`PC_PORT`
   branch declaring them as plain `extern u16[]` arrays. The port could switch
   to real, larger arrays and sidestep the EWRAM overlay entirely — needs
   verification that nothing addresses them by GBA address.

2. **Tile streaming assumes a 32-wide stride.** `src/screenTileMap.c` is the
   hardest single file in this effort. It is decompiled register-named code
   with the stride and wrap masks fused into DMA calls and loop bounds.

3. **OAM Y is 8 bits.** VirtuaPPU does `if (obj_y >= MODE1_GBA_HEIGHT) obj_y -= 256`.
   At height 240 only `y ∈ [240,255]` maps to −16…−1, so a sprite more than 16 px
   above the top edge becomes unrepresentable. This is a hard correctness bug,
   not a cosmetic one, and it appears the moment height exceeds ~200.

4. **Window registers are 8 bits per edge.** 320 does not fit in `WIN0H`. Used by
   light rays, light doors, cutscene letterboxing, and two menus.

5. **Camera constants are scattered magic numbers** (§3). Mechanical but wide.

6. **HDMA is a 160-line pipeline.** `port_hdma.c` plus the engine's per-scanline
   tables (water FX, `BLDY` fades, affine matrices). 240 lines requires those
   tables extended or resampled.

7. **VRAM is fixed at 96 KB.** Larger screenblocks need more than `gVram[0x18000]`.

---

## 5. The finding that reframes the problem

`SetTileType` (`src/playerUtils.c:3436-3475`) writes every runtime tile mutation
into `gMapDataBottomSpecial` / `gMapDataTopSpecial` and then raises
`gUpdateVisibleTiles`, which causes `screenTileMap.c` to refresh the 32×32
window *from those arrays*. The streaming functions take them as their
`mapspecial` parameter.

Those arrays are `u16[0x4000]` each (`include/tileMap.h:10-11`) — 16384 entries
at a row stride of `0x80` (128), i.e. a **128×128 grid of 8×8 tiles = 1024×1024
pixels**, which covers the largest room in the game (1024×1008, §6).

The consequence: **for world/gameplay layers, a complete and runtime-accurate
tilemap for the entire room already exists in memory, independent of the 32×32
hardware window.** A PPU that samples *that* does not need blockers 1, 2, or 7
solved at all. No EWRAM layout surgery, no rewrite of `screenTileMap.c`, no
VRAM growth.

The natural boundary this creates:

- **World BG layers** have a full-room source → can be sampled at any viewport size.
- **UI / menu / title layers** do not — `ui.c`, `title.c`, `fileselect.c`,
  `cutscene.c`, and the menus write `gBGxBuffer` directly and are authored for
  240×160.

That boundary lines up exactly with the requirement. HUD and menus authored for
240×160 *should* be centered rather than stretched or extended, so leaving them
on the 32×32 path is the correct behaviour, not a compromise.

**This must be verified before it is relied on.** See Spike 2 (§9).

---

## 6. Content survey — how much of the game actually needs borders

Extracted from `data/map/room_headers.s` (738 entries; 121 are `0x0/0x0`
placeholders, leaving **617 real rooms**):

| Metric | Count | Share of real rooms |
|---|---|---|
| ≥ 320 wide **and** ≥ 240 tall | 135 | 22% |
| Narrower than 320 | 443 | 72% |
| Shorter than 240 | 356 | 58% |
| Bordered in at least one axis | 482 | **78%** |
| Exactly 240×160 | 168 | 27% |

Width distribution clusters at 240 (285), 272 (130), 336 (20), 304 (15).
Height clusters at 160 (189), 208 (110), 240 (39), 320 (36), 192 (33).
Maximum room: 1024×1008.

Two implications that should drive the design:

1. **Bordering is the common case, not the exception.** A build that only ever
   letterboxed would already be correct for 78% of rooms. Border handling
   deserves first-class treatment, not a fallback path.
2. **The "reveal more world" path — where entity culling, off-screen spawning,
   and scripted-camera bugs live — affects a minority of rooms.** But that
   minority includes Hyrule Field, Hyrule Town, Minish Woods, and Castor Wilds:
   the overworld where players spend most of their time. Low room count, high
   exposure. Test selection must be weighted by playtime, not by room count.

---

## 7. Option catalogue

### Option A — Stretch (status quo Phase 1)
Render 240×160, scale to 320×240.
**Unlocks:** nothing. **Cost:** zero, already built.
**Verdict:** baseline for comparison. Distorts 3:2 → 4:3. Does not meet the ask.

### Option B — Native render, centered in a 320×240 window
Render 240×160 untouched, letterbox it.
**Unlocks:** correct aspect, no distortion. **Cost:** trivial.
**Verdict:** strictly better than A and worth landing as a safety net, but it
borders *every* room, including the 22% that could show more. Not the ask.

### Option C — Per-room adaptive viewport
Render each room at `min(room_size, 320×240)` and center the result. Rooms
≥320×240 show a full 320×240 of world; smaller rooms render at natural size
with borders.
**Unlocks:** the literal requirement, with the minimum possible expansion
surface — the engine only ever renders beyond 240×160 where real map data
exists.
**Cost:** all seven blockers still apply for the ≥320×240 rooms, but the
*variable* viewport adds its own complexity (camera clamp, HUD placement, and
window regs must all track a per-room size).
**Verdict:** strong on requirements fit; the variable-size machinery is a real
cost and may be more trouble than a fixed 320×240 viewport plus per-room
clamping that naturally produces borders.

### Option D — Full 320×240 engine viewport (sa2-style)
Make `DISPLAY_WIDTH`/`DISPLAY_HEIGHT` build config; extend OAM to 12-byte
entries with s16 x/y; widen window registers to 32-bit; grow BG buffers to
64×32; rewrite `screenTileMap.c`; grow VRAM.

This is the approach taken by `awe444/sa2`, and its implementation is worth
reading closely:

- `include/gba/defines.h:41-76` — `DISPLAY_WIDTH 320 / DISPLAY_HEIGHT 240` for
  PC, with `WIDESCREEN_HACK` and `EXTENDED_OAM` auto-enabled at ≥256.
- `VRAM_SIZE (0x18000 + (0x800 * 14))` — 14 extra screenblocks (28 KB).
- `OAM_SIZE (OAM_ENTRY_COUNT * 0xC)` — 12-byte entries; `split.x`/`split.y` as
  s16, with `OAM_SET_GBA_ATTR0/1/2` shims unpacking GBA-format writes
  (`include/gba/types.h:270-294`).
- `winreg_t` widened to `uint32_t` with `WIN_RANGE` / `WIN_GET_LOWER` /
  `WIN_GET_HIGHER` accessors (`defines.h:139-150`).
- Renderer honours the split fields and skips the wrap fixups under
  `EXTENDED_OAM` (`sa1/src/platform/pret_sdl/sdl2.c:1625-1635`).

**Two cautions from that codebase.** First, its own header carries
`// TODO: EXTENDED_OAM is not yet functional` — the mechanism may be less
settled than it looks. Second, and more telling: `WIDESCREEN_HACK` requires
**per-scene hand-tuning**. `zone_2`, `zone_3`, `zone_4`, `zone_5`, `zone_7`,
`background.c`, `camera.c`, and several menus all carry `#if WIDESCREEN_HACK`
special cases. That is the honest cost signal for this option — it is not a
mechanical transform, and TMC's analogue is per-area tilemap streaming plus
every BG-authored UI screen.

**Unlocks:** maximum fidelity, everything native at 320×240.
**Cost:** highest. All seven blockers, plus 42 stride-sensitive BG-buffer sites,
plus a per-screen tuning tail.
**Verdict:** the reference implementation to learn from, but adopting it
wholesale imports the parts TMC does not need.

### Option E — PPU samples the full-room map directly (recommended)
Add a non-GBA BG mode to VirtuaPPU that, for world layers, reads
`gMapDataBottomSpecial` / `gMapDataTopSpecial` at a caller-supplied scroll
origin and viewport size, instead of the 32×32 screenblock. UI layers keep the
existing 32×32 path and are composited centered.

Since GBA legality is explicitly not required and VirtuaPPU is ours to modify,
nothing forces the BG source to be a hardware-shaped screenblock.

**Unlocks:** a true 320×240 world view without touching blockers 1, 2, or 7.
**Still requires:** OAM Y widening (3), window registers (4), camera constants
(5), HDMA line count (6). Those are unavoidable in any option that genuinely
expands the viewport, and sa2 has proven patterns for 3 and 4.
**Cost:** moderate, and concentrated in code we own (VirtuaPPU + `port/`) rather
than in 343 lines of register-named decomp.
**Risk:** rests entirely on the §5 premise. If the special map turns out to be
incomplete for some rooms, paged for large rooms, or bypassed by some layer,
the option degrades sharply. `sub_0807C5F4` (`src/playerUtils.c:4264-4311`) does
chunked expansion with `width > 0xff` / `height > 0xff` branches that need to be
understood before this is trusted.
**Verdict:** highest value-to-cost ratio *if* Spike 2 confirms the premise. This
is the option the plan is built to validate or kill first.

### Option F — Fork VirtuaPPU instead of patching it
Not a viewport option, but a prerequisite decision. Options C, D, and E all
require substantially more VirtuaPPU modification than four context diffs can
carry. `awe444/VirtuaPPU` is available as a fork point. Recommend switching the
submodule to a fork and retiring `port/patches/` as part of whichever option is
chosen.

---

## 8. Recommendation

**Pursue Option E, with Option B landed first as an unconditional safety net,
and Option D's OAM and window-register mechanisms adopted wholesale from sa2.**

Reasoning:

- Option B is nearly free, is strictly better than the current stretch, and
  gives a correct-looking 320×240 build to fall back to if E is killed.
- Option E is the only option that avoids the two genuinely expensive blockers
  (the EWRAM-aliased BG buffers and `screenTileMap.c`), and it avoids them by
  exploiting a data structure the engine already maintains correctly.
- The blockers E does *not* avoid — OAM Y, window registers — are exactly the
  ones sa2 has already solved, with readable reference code. No need to invent.
- The §6 data supports E's asymmetry: 78% of rooms are bordered anyway and never
  exercise the expanded path, so the risky surface is small and concentrated in
  a well-defined set of overworld areas that can be tested exhaustively.
- Option C's per-room variable viewport is worth reconsidering only if Spike 5
  shows that fixed-320×240 camera clamping produces bad results on mid-size
  rooms. Fixed viewport plus clamping should produce centering naturally.

**This recommendation is conditional on Spike 2.** If the special-map premise
fails, the decision reverts to Option D and the effort roughly triples.

---

## 9. Research plan

Ordered so that the cheapest thing most likely to invalidate the recommendation
runs first.

### Spike 0 — Baseline and instrumentation (0.5 day)
**Questions:** What does the current build actually do at `widescreen_width=320`?
What is the frame budget?
**Method:** Build at 240 and 320. Capture reference frames for a fixed route
(title → file select → Hyrule Town → Minish Woods → a dungeon → pause menu →
text box). Add a frame-time counter. Confirm the patch pipeline in
`xmake.lua:377-430` reapplies cleanly.
**Exit:** reproducible before/after harness exists; frame budget known.

### Spike 1 — Land Option B (0.5 day)
**Method:** 320×240 window, 240×160 render centered with borders. No engine
changes.
**Exit:** shippable fallback exists and is committed. De-risks everything after.

### Spike 2 — Validate the special-map premise *(decision gate)* (2–3 days)
**Questions:**
- Is `gMapDataTopSpecial` / `gMapDataBottomSpecial` complete for the whole room,
  in every room, at all times?
- What exactly do the `width > 0xff` / `height > 0xff` branches in
  `sub_0807C5F4` do, and is the map ever paged rather than fully resident?
- Which BG layers in gameplay derive from it, and which do not (BG3 parallax,
  `backgroundAnimations.c`, `hyruleTownTileSetManager.c` tileset swaps,
  `weatherChangeManager.c`, `holeManager.c`)?
- Do runtime mutations (destructible tiles, bombable walls, doors, chests,
  `lightManager.c`) all route through `SetTileType`?
**Method:** Instrument the port to dump the special map alongside the composed
32×32 window each frame, and diff the overlapping region. Any mismatch is a
layer or mutation path that bypasses the map. Walk the full test route plus
targeted destructible/door/chest interactions.
**Exit:** either (a) diff is clean across the route → **proceed with Option E**,
or (b) documented list of bypassing layers → assess whether they can be
special-cased, else **fall back to Option D**.

### Spike 3 — OAM Y widening (1–2 days)
**Questions:** Can sa2's `EXTENDED_OAM` split-field approach be ported, given
its "not yet functional" TODO? How many TMC sites write OAM attrs raw?
**Method:** Read `sa2/include/gba/types.h:95-300` and
`sa2/sa1/src/platform/pret_sdl/sdl2.c:1600-1660`. Inventory TMC's OAM writers
(`gOAMControls.oam`, `structures.h:298`, `vram.h:91`). Prototype s16 x/y in
VirtuaPPU with `OAM_SET_GBA_ATTR*`-style shims.
**Exit:** sprites render correctly at y < −16 in a 240-tall viewport.

### Spike 4 — Window register widening (1 day)
**Method:** Follow sa2's `winreg_t` / `WIN_RANGE` pattern. Convert the seven
consuming files. Verify against light rays, light doors, cutscene letterbox,
figurine menu, kinstone menu.
**Exit:** all window effects correct at 320 width.

### Spike 5 — Camera and clamping (2 days)
**Questions:** Do the magic constants in `scroll.c` generalise cleanly to
half-viewport expressions? Does fixed-320×240 clamping produce correct centering
on rooms smaller than the viewport, or is Option C's variable viewport needed?
**Method:** Replace `0x78`/`0x50`/`0xf8`/`0xa8`/`0xf0`/`120` with
viewport-derived expressions. Test across the width/height clusters from §6
(240, 272, 304, 336 wide; 160, 208, 240, 320 tall).
**Exit:** camera behaves correctly in all size classes; verdict recorded on
C-vs-fixed.

### Spike 6 — HDMA at 240 lines (1–2 days)
**Questions:** Which effects use per-scanline tables, how long are those tables,
and do they resample or need extension?
**Method:** Inventory HDMA registrations. Test water FX, `BLDY` fades, affine
scenes (Vaati's tornado, rolling barrel, screen-shrink cinematic).
**Exit:** no scanline effect regressions at 240 lines.

### Spike 7 — UI centering and composition (2 days)
**Questions:** Should UI layers be centered, or anchored to viewport edges? What
happens to the HUD, text boxes, title, file select, and pause menu?
**Method:** Composite the 32×32 UI path centered over the expanded world.
Evaluate whether the HUD reads correctly centered or wants edge anchoring — note
that edge anchoring reopens the 42 stride-sensitive `gBG0Buffer` sites in
`ui.c`, so centering is strongly preferred.
**Exit:** design decision recorded; all UI screens visually correct.

### Spike 8 — Entity culling and off-screen behaviour (2–3 days)
**Questions:** With more world visible, do entities that the engine assumed
off-screen become visible before they spawn, after they despawn, or in parked
positions? The existing patch notes parked sprites at OAM x ≥ 240.
**Method:** Focus on the 135 rooms ≥320×240, weighted to Hyrule Field, Hyrule
Town, Minish Woods, Castor Wilds. Look for pop-in, parked-sprite leakage, and
scripted-cutscene framing breaks.
**Exit:** catalogue of culling issues with severity; fixes scoped.

### Spike 9 — Integration and performance (2 days)
**Method:** Full playthrough of the test route at 320×240. Frame budget against
Spike 0 baseline. The parallel-render path in the widescreen patch (OpenMP over
scanlines, `io_snapshots[MODE1_GBA_HEIGHT][MODE1_IO_MEM_SIZE]`) grows 50% in
both line count and per-line work — confirm it still fits.
**Exit:** go/no-go on shipping.

**Total estimate:** 14–19 days to a confident answer, of which Spikes 0–2
(3–4 days) determine whether the recommended path survives.

---

## 10. Open questions

1. Does `gMapData*Special` cover the full room in *all* cases, or does
   `sub_0807C5F4`'s chunked expansion leave gaps for certain room geometries?
   (Decision gate — Spike 2.)
2. Are `gBG0/1/2/3Buffer` ever addressed by GBA address (`0x02...`) rather than
   through the `gEwram` alias? `src/subtask.c:109` does
   `MemCopy(gBG1Buffer, (void*)0x600e000, 0x800)` — a raw VRAM address — which
   suggests at least some raw-address coupling exists.
3. Is `EXTENDED_OAM` in sa2 actually working, or is the TODO accurate? Affects
   whether Spike 3 is a port or an original implementation.
4. What is `gUnk_080B2CD8` (`data/const/interrupts.s:42`) — confirmed as a
   BGCNT-screen-size → DMA-length table? It is the hook point if any option
   needs larger screenblocks.
5. Does the mode-2 path (`libs/ViruaPPU/src/mode2.c`, used for GBA modes 1 and 2
   per `port_ppu.cpp:376-386`) need the same treatment as mode 1? Affine BG2
   scenes route through it.
6. Should the viewport be a build-time constant or runtime-configurable? The
   port already has a runtime config system (`port_runtime_config.h`) and an
   internal-scale setting; a runtime toggle would ease A/B testing but forbids
   compile-time sizing of buffers.

---

## 11. Risk register

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| Special-map premise fails | Option E dead; effort ~3× | Medium | Spike 2 is the gate, runs 3rd |
| `screenTileMap.c` rewrite required after all | High — hardest file in the effort | Medium | Only reached under Option D |
| Per-scene tuning tail (as in sa2) | Schedule creep, long bug tail | High | Weight testing by playtime (§6), not room count |
| OAM Y widening destabilises sprite rendering | Broad visual regressions | Medium | Spike 3 isolated and early; sa2 reference available |
| Perf regression from 50% more scanline work | Frame drops | Low–Medium | Spike 0 baseline; OpenMP path already exists |
| Patch pipeline can't carry the change | Build breakage, merge pain | High | Option F: fork VirtuaPPU, retire `port/patches/` |
| Cutscene/scripted framing breaks at 320×240 | Visible authored-content bugs | Medium | Spike 8; `cutscene.c` already uses window regs for letterboxing |

---

## 12. Reference material

- `awe444/sa2` — `include/gba/defines.h:41-150` (config, `WIDESCREEN_HACK`,
  `EXTENDED_OAM`, `winreg_t`), `include/gba/types.h:95-300` (extended OAM),
  `sa1/src/platform/pret_sdl/sdl2.c:1600-1660` (renderer), and the
  `#if WIDESCREEN_HACK` sites across `src/game/*/stage/backgrounds/`.
- `awe444/VirtuaPPU` — readable mirror of the PPU submodule.
- This repo: `port/patches/viruappu-widescreen.patch` (Phase 1 rationale),
  `xmake.lua:29-43` (the option and its caveats).
