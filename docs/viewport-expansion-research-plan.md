# Viewport Expansion Research Plan — 240×160 → 320×240

**Status: Milestone 1 is DONE — signed off by the maintainer 2026-07-30, go
for Milestone 2.** A 320×160 viewport builds, runs the canonical route, centres
narrower rooms with borders, and costs +10.9% present time against the Spike 1
canvas baseline. The 240 build is verified pixel-identical to the
pre-expansion references and stays that way — that is the standing regression
gate.

**Read `docs/viewport-bug-tracker.md` first.** It is authoritative wherever it
disagrees with this document: four playtest rounds plus one sweep produced ten
bugs and several corrections to claims made below. It also carries the
**carry-forward list Milestone 2 inherits**, and the lessons that cost the most
to learn.

Headlines from that record, because they change what this plan says: **D1 was
reversed** from edge-anchored to centered (§0); **D3 was amended** — borders are
a uniform colour, not necessarily black (§0); Spike 2's original measurement was
invalidated and re-run (§10); Spike 5 missed `script.c`, causing a softlock; and
B4/B5 were **deferred, never reproduced**.

Next work is §10.2 — Spikes 8–11.

**Target:** PC port only (`PC_PORT`). GBA-native build target is *not* preserved.
**Scope:** USA assets only. Mod/pak compatibility (`port_asset_pak*`) at
320×240 is **best-effort, not gating** — mods are authored against 240×160
assumptions, and breakage they exhibit does not block any milestone.
**Estimate unit:** all day figures are focused working days for a single
engineer, implementation *and* verification included. The static-trace and
mechanical-edit portions compress heavily under agent execution; the
interactive portions (route walks, visual checks) do not — the verification
tail is the floor regardless of who writes the code.
**Goal:** render a 320×240 viewport. Rooms smaller than the viewport in either
axis are centered with borders.

---

## 0. Decisions required before execution

Four decisions that the spikes would otherwise make implicitly, mid-task, on
implementation-cost grounds. They are pulled forward so they are made
deliberately. None require code first.

| # | Decision | Default if unmade | Made in | Status |
|---|---|---|---|---|
| **D1** | **HUD placement at expanded width.** Centered (hearts/rupees float 40 px in from the window edge) or edge-anchored (reopens the stride-sensitive `gBG0Buffer` sites, §3)? This is a product choice, not an engineering one — decide from the two Spike 0 mockups. Menus/title/file-select are settled (centered, §5); this is specifically the in-game overlay. | Centered | Before Spike 6 | **Decided edge-anchored 2026-07-28, REVERSED to centered 2026-07-30.** Edge-anchoring needs `gBG0Buffer`'s stride widened, and the stride proved to be baked into the shared text renderer and several byte-count clears — silent corruptions, not compile errors, so each surfaced only as a playtest bug (three rounds, three more found). BG0 is back to the hardware 32×32 shape, which *cannot* place a tile past x=255, so the layer can only shift uniformly and D1 is realised as centered. Full reasoning and the cost of retrying: `docs/viewport-bug-tracker.md`. |
| **D2** | **Viewport size: build-time constant or runtime-configurable?** Gates Spike 3's architecture (fork API shape, buffer sizing) and the settings-menu surface. The port already exposes internal-scale at runtime (`port_runtime_config.h`). | Build-time for Milestone 1; revisit before ship | Spike 0 | **Decided 2026-07-27: build-time for M1** |
| **D3** | **Border appearance** for centered rooms: solid black, or something else? 78% of rooms show borders (§6) — this is most of what players see. | Solid black | Spike 0 | **Decided 2026-07-27: solid black. AMENDED 2026-07-30: a uniform colour, not necessarily black.** In gameplay and the legend the backdrop *is* black, so those match. A clipped UI screen shows the PPU backdrop in its border bands — green on the pause menu, grey in the figurine gallery. The bands are uniform (the clip works); they are just not black, which is what hardware shows outside every layer. Accepted as-is rather than forced. Verify borders by **distinct colours per column**, not by "is it black" — see the bug tracker's lesson 6. |
| **D4** | **Phase 1 scaffold: wire or delete?** (was open question 8). Constraint either way: `viruappu-widescreen.patch` survives as reference material — Spike 9's per-line IO snapshot design lives only in that unapplied patch. | Wire | Spike 0 | **Decided 2026-07-27: delete** — the inert `widescreen_width` option and the dead stretch branch are removed; width plumbing arrives properly in Milestone 1. The patch file stays as Spike 9 reference. |

---

## 1. Why this document exists

The obvious framing — "change 240 to 320 and 160 to 240" — is wrong here, and
the repo already contains evidence of that. A Phase 1 widescreen scaffold exists
and deliberately stops short of real expansion, with a comment naming the
blocker — though as §2.2 shows, it is not actually wired into the build and has
never run. This plan enumerates the actual options, grades them against evidence
gathered from the codebase, and proposes an ordered sequence of spikes with
explicit definitions of done, to confirm the recommendation before committing to
a build.

The single most important finding is in §5: **the engine already maintains a
live, full-room, runtime-accurate 8×8 tilemap** (`gMapDataBottomSpecial` /
`gMapDataTopSpecial`). That changes which option is cheapest, and it is not
something the existing Phase 1 comments account for.

---

## 2. What already exists

### 2.1 Rendering stack

The port replaces GBA hardware with a software PPU:

- `libs/ViruaPPU` — submodule, now tracking `awe444/VirtuaPPU` (§2.3).
  ~1730 LOC total; `src/mode1.c` is 741 lines and does all text-BG, OBJ, and
  composite work.
- `port/port_ppu.cpp` — drives VirtuaPPU once per VBlank, then handles
  upscaling (xBRZ), CRT/LCD filters, fit-rect, and SDL present.
- `port/port_hdma.c` — per-scanline HBlank-DMA simulation, driven through
  `virtuappu_mode1_pre_line_callback`.

VirtuaPPU is modified at build time by patches in `port/patches/`, applied by
`xmake.lua:379-432`. The submodule now tracks the `awe444/VirtuaPPU` fork (see
§2.3); of the four patch files on disk, the fork already carries `hdma-hook`
and `mosaic`, so only `internal-scale` still applies.

### 2.2 The Phase 1 widescreen spike is not actually wired up

`xmake.lua:39-43` declares a `widescreen_width` option whose description says it
sets `-DMODE1_GBA_WIDTH=N`. It does not. Verified:

- **The option is never consumed.** There is no `add_defines("MODE1_GBA_WIDTH=…")`
  anywhere in `xmake.lua` or `build.py`. Setting `widescreen_width` has no effect
  on the build.
- **`viruappu-widescreen.patch` is never applied.** The `patches` table in
  `xmake.lua` lists only `hdma-hook`, `mosaic`, and `internal-scale`. The
  widescreen patch sits on disk unreferenced.
- Consequently `MODE1_GBA_WIDTH` always resolves to the fallback
  `#define MODE1_GBA_WIDTH 240` in `port/port_main.c:5-6`, and the stretch branch
  at `port_ppu.cpp:411` is dead code.

So the current state is not "a stretch we need to replace" — it is **GBA-native
240×160 with an inert widescreen scaffold beside it**. The scaffold is still
useful as a design sketch (it is where the BG-clip reasoning lives), but nothing
in it has ever run. Any plan step that assumes you can build at
`widescreen_width=320` today is wrong; wiring the option is itself a task.

Even if it were wired, Phase 1 would be **a stretch, not an expansion**:
`viruappu-widescreen.patch` pins `MODE1_GBA_VIEWPORT_X` and
`MODE1_GBA_BG_CLIP_X` at 240, force-blacks BG past column 240, clips OAM at 240,
and `port_ppu.cpp:411-423` uniformly rescales the 240-px frame. No additional
world is revealed.
Height is not parameterized at all — `MODE1_GBA_HEIGHT = 160` is still a plain
enum constant in `include/cpu/mode1.h`, so the 240 half of the target is
entirely unaddressed.

The patch header states the intended Phase 2: *"Real widescreen needs a 64-tile
`BGCNT_TXT512x256` (sa2-style) extension to the engine."* Part of this plan is
to test whether that is in fact the right Phase 2 — §5 suggests it may not be.

### 2.3 Submodule now tracks the fork *(done)*

`.gitmodules` previously pointed `libs/ViruaPPU` at
`MatheoVignaud/VirtuaPPU-experimental`, which is unreachable — this is why the
submodule directory was empty. It now points at `awe444/VirtuaPPU`.

The fork does **not** contain the previously recorded gitlink
`a0c4781`; its history is a separate 9-commit lineage ending at `e69f60b`. The
gitlink was therefore repointed to `e69f60b` as part of the same change, since
leaving it would have made `git submodule update` fail against the new URL.
Verified after the swap:

- `git submodule update --init libs/ViruaPPU` clones and checks out cleanly.
- `hdma-hook` and `mosaic` marker symbols (`virtuappu_mode1_pre_line_callback`
  in `src/mode2.c`, `MODE1_IO_MOSAIC` in `include/cpu/mode1.h`) are already
  present in the fork, so `xmake`'s idempotent marker check skips both.
- `internal-scale` applies cleanly (direct application; the `-3` three-way path
  warns because the fresh clone lacks the base blob, then falls back and
  succeeds).

This resolves the mechanical half of Option F (§7). The remaining half —
retiring `port/patches/` in favour of committing changes directly to the fork —
is still open and becomes worthwhile as soon as any option below is started,
because none of them can be carried by context diffs.

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
| BG staging buffers | `include/vram.h:58-68` **and duplicated verbatim at** `include/structures.h:207-210` | `gBG0/1/2Buffer` = `0x400` u16 (32×32); `gBG3Buffer` = `0x800` (64×32). Both definition sites must change together. |
| BG→VRAM upload | `src/interrupts.c:104-108` | VBlank DMA of `bg->subTileMap` into a screenbase; size from `gUnk_080B2CD8[dest >> 14]`, indexed by the BGCNT screen-size bits. |
| HUD composition | `src/ui.c:233-559` | Direct writes into `gBG0Buffer` at hardcoded linear offsets (`0x258`, `0x278`, `0x219`, `0x239`, …) that assume a 32-wide stride. |
| Window regs | `src/object/lightRay.c`, `lightDoor.c`, `cutscene.c`, `menu/figurineMenu.c`, `menu/kinstoneMenu.c`, `common.c`, `interrupts.c`; plus PPU-side clamps at `libs/ViruaPPU/src/mode1.c:563-570` | `WIN0H`/`WIN0V` pack two 8-bit edges into one `u16` (`0x80f0`, `(a << 8) \| b`). The PPU clamps window bottoms against `MODE1_GBA_HEIGHT` — an eighth edit site beyond the seven engine files. |
| Memory map | `port/port_gba_mem.h:11-17,47-51` | `gVram[0x18000]` fixed; `gba_TryMemPtr` bounds VRAM at `0x06018000`. |

Aggregate scale of the BG-buffer coupling:
`grep -rn "gBG[0-3]Buffer" --include=*.c --include=*.h src include` returns
**118 references across 24 files** (121 / 25 files including `port/`); roughly
40 are stride-sensitive indexed accesses. The stride-sensitive count is
approximate until the Spike 6 inventory re-derives it — any number quoted from
this table should carry its grep so it stays checkable.

---

## 4. Constraint inventory

Seven distinct blockers. They are not equally hard, and — critically — they do
not all apply to every option.

1. **BG staging buffers are 32×32.** 320×240 needs 40×30 tiles visible, so ≥42×32
   with a scroll margin. Worse, under `PC_PORT` these are `#define`s aliasing
   fixed offsets inside `gEwram[]` (`0x34CB0`, `0x21F30`, `0x344B0`, `0x1A40`) —
   defined twice, in `vram.h:59-62` *and* `structures.h:207-210` — so growing
   them collides with neighbouring decomp data at fixed addresses.
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
   light rays, light doors, cutscene letterboxing, and two menus. The PPU also
   clamps window bottoms against `MODE1_GBA_HEIGHT`
   (`libs/ViruaPPU/src/mode1.c:563-570`) — an edit site beyond the seven engine
   files.

5. **Camera constants are scattered magic numbers** (§3). Mechanical but wide.

6. **HDMA is a 160-line pipeline.** `port_hdma.c` plus the engine's per-scanline
   tables (water FX, `BLDY` fades, affine matrices). 240 lines requires those
   tables extended or resampled.

7. **VRAM is fixed at 96 KB.** Larger screenblocks need more than `gVram[0x18000]`.

---

## 5. The finding that reframes the problem

`SetTileType` (`src/playerUtils.c:3436-3475`) writes runtime tile mutations
into `gMapDataBottomSpecial` / `gMapDataTopSpecial` and then raises
`gUpdateVisibleTiles`, which causes `screenTileMap.c` to refresh the 32×32
window *from those arrays*. The streaming functions take them as their
`mapspecial` parameter. (The write is gated — `playerUtils.c:3455` skips it
when `gRoomControls.scroll_flags & 1` is set; see §5.1.)

Those arrays are `u16[0x4000]` each (`include/tileMap.h:10-11`) — 16384 entries
at a row stride of `0x80` (128), i.e. a **128×128 grid of 8×8 tiles = 1024×1024
pixels**, which covers the largest room in the game (1024×1008, §6). On the
normal room-load path they are populated whole-room by
`RenderMapLayerToSubTileMap` (`src/beanstalkSubtask.c:1126` — 64×64 16-px
tiles at stride `0x80`).

The consequence: **for world/gameplay layers, in the normal case, a complete
and runtime-accurate tilemap for the entire room already exists in memory,
independent of the 32×32 hardware window.** A PPU that samples *that* does not
need blockers 1, 2, or 7 solved at all. No EWRAM layout surgery, no rewrite of
`screenTileMap.c`, no VRAM growth.

The natural boundary this creates:

- **World BG layers** have a full-room source → can be sampled at any viewport size.
- **UI / menu / title layers** do not — `ui.c`, `title.c`, `fileselect.c`,
  `cutscene.c`, and the menus write `gBGxBuffer` directly and are authored for
  240×160.

That boundary lines up exactly with the requirement. HUD and menus authored for
240×160 *should* be centered rather than stretched or extended, so leaving them
on the 32×32 path is the correct behaviour, not a compromise.

### 5.1 Known exclusions — found in review, ahead of Spike 2

The premise does **not** hold unconditionally. Three counter-signals are
visible statically, and they shape both Spike 2 and the design:

1. **The arrays are aggressively repurposed outside gameplay** — the
   `tileMap.h:6-7` comment says so, and it is true:
   - `fileselect.c:378` treats `gMapDataBottomSpecial` as a save-file struct
     (`.saves[idx]`).
   - `kinstoneMenu.c:385` re-declares it as `struct_02019EE0[16]`.
   - `pauseMenu.c:1199,1278` uses it as the dungeon-map draw target.
   - `gyorgFemale.c:95-96,224-247` `MemClear`s both arrays whole (`0x8000`
     bytes each) and uses them as a scratch bitmap during the fight.

   A PPU that samples them unconditionally renders garbage in file select,
   kinstone fusion, the pause dungeon map, and the Gyorg fight.
2. **`SetTileType`'s special-map write is gated** on
   `(gRoomControls.scroll_flags & 1) == 0` (`playerUtils.c:3455`).
3. **A room class exists where the map is neither full-room nor live.** On the
   `clearBottomMap` / `AREA_PALACE_OF_WINDS_BOSS` path
   (`playerUtils.c:4074-4087`) the special maps are built not by
   `RenderMapLayerToSubTileMap` but by `sub_0807C5F4`'s staging shuffle — four
   32×32 chunks at offsets `0/0x20/0x1000/0x1020`, i.e. **64×64 subtiles =
   512×512 px**, not 1024×1024 — and the same branch sets `scroll_flags |= 1`,
   which then disables the `SetTileType` write per (2).

Consequently Option E cannot be a static per-layer flag. It needs a **runtime
"is this BG currently a map-authoritative world layer" predicate**, evaluated
per BG per frame off an explicit engine-side signal — never a heuristic. The
natural signal exists: the engine rebinds `gScreen.bgN.subTileMap` per scene
(`common.c:636-642`), and `scroll_flags` marks the degraded room class. The
predicate is specified in Spike 2's static pre-check and implemented in
Spike 3; scenes it excludes stay on the unchanged 32×32 path — which is
exactly the §5 boundary behaviour anyway.

**The premise must still be verified at runtime before it is relied on.** See
Spike 2 (§10).

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
**Verdict:** ~~strong on requirements fit; the variable-size machinery is a
real cost and may be more trouble than a fixed 320×240 viewport plus
per-room clamping that naturally produces borders.~~ **Dropped
2026-07-28 (Spike 5).** Fixed viewport plus clamping produced correct
centring for all 443 narrow rooms from two range macros, with no
variable-size machinery. The suspicion recorded here was right.

### Option D — Full 320×240 engine viewport (sa2-style)
Make `DISPLAY_WIDTH`/`DISPLAY_HEIGHT` build config; extend OAM to 12-byte
entries with s16 x/y; widen window registers to 32-bit; grow BG buffers to
64×32; rewrite `screenTileMap.c`; grow VRAM.

This is the approach taken by `awe444/sa2`
(<https://github.com/awe444/sa2> — our fork of the SAT-R/sa2 Sonic Advance 2
decompilation; all "sa2" references in this doc mean this fork), and its
implementation is worth reading closely:

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
**Cost:** highest. All seven blockers, plus ~40 stride-sensitive BG-buffer sites (§3),
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

Two design elements this option requires that an implementer would otherwise
have to invent mid-spike:

- **The map-authoritative predicate (§5.1).** Which BG is "world" vs "UI" is
  dynamic, not static — `common.c:636-642` rebinds `gScreen.bgN.subTileMap`
  per scene, and menus repurpose both the buffers and the special maps. The
  PPU switches source per BG per frame off that binding plus `scroll_flags`.
  Spec: Spike 2. Implementation: Spike 3.
- **Affine BG2 is out of scope for the new mode.** The mode-2 path
  (`libs/ViruaPPU/src/mode2.c`) has its own `MODE1_GBA_WIDTH/HEIGHT` loops and
  no full-room source to sample. Affine scenes render 240×160 and letterbox
  through Milestone 1 at least (resolves open question 6 for now).

**Unlocks:** a true 320×240 world view without touching blockers 1, 2, or 7.
**Still requires:** OAM Y widening (3), window registers (4), camera constants
(5), HDMA line count (6). Those are unavoidable in any option that genuinely
expands the viewport, and sa2 has proven patterns for 3 and 4.
**Cost:** moderate, and concentrated in code we own (VirtuaPPU + `port/`) rather
than in 343 lines of register-named decomp.
**Risk:** rests on the §5 premise, whose known exceptions are catalogued in
§5.1. The residual risk is that Spike 2 finds the exclusion set larger than the
predicate can cleanly route — specifically, a *mid-gameplay world* layer that
bypasses the map. `sub_0807C5F4` (`src/playerUtils.c:4264-4311`) is now
understood: four 32×32-chunk copies covering 512×512 px, used only on the
degraded room class of §5.1(3).
**Verdict:** highest value-to-cost ratio *if* Spike 2 confirms the premise. This
is the option the plan is built to validate or kill first.

### Option F — Fork VirtuaPPU instead of patching it *(done)*
Not a viewport option, but a prerequisite decision. Options C, D, and E all
require substantially more VirtuaPPU modification than context diffs can carry.
The submodule swap to `awe444/VirtuaPPU` is done (§2.3), and as of 2026-07-27
the patch pipeline is **retired**: `internal-scale` — the only patch still
applying at build time — was committed to the fork (`276c73a`), the gitlink
bumped, the `before_build` patch loop removed from `xmake.lua`, and the three
applied patch files deleted (`port/patches/README.md` records where each
change now lives). `viruappu-widescreen.patch` is retained as Spike 9 design
reference only; it was never applied (§2.2).

Verified behaviour-neutral: clean rebuild with no patch step, then a full
canonical-route capture diffed against the Spike 0 references —
**11/11 waypoints pixel-identical**. The submodule working tree now stays
clean, so `git status` no longer reports phantom modifications and a real
PPU change is visible as a reviewable gitlink bump.

---

## 8. Axis decomposition: width-first or height-first?

Expanding one axis at a time is attractive as an intermediate milestone, and the
constraint inventory (§4) turns out to decompose along the two axes in a
genuinely useful way. **Each direction is blocked by a different, non-overlapping
pair of constraints.**

### 8.1 Per-axis constraint matrix

| Constraint | Width-only (240→320) | Height-only (160→240) |
|---|---|---|
| **BG staging buffer** | Needs 40+2 = **42 tiles wide**; buffer is 32. **Blocked** (unless Option E) | Needs 30+2 = **32 tiles tall**; buffer is already 32. **Fits** — but scroll slack collapses from 96 px to 16 px |
| **Tile streamer** (`screenTileMap.c`) | Stride change, the hard rewrite. **Blocked** (unless Option E) | Stride unchanged; only refresh *cadence* must increase. **Softer** |
| **OAM coordinate** | X is 9-bit (0–511). At width 320, x ∈ [320,511] → −192…−1. Ample. **Clear** | Y is 8-bit. At height 240, y ∈ [240,255] → only −16…−1. **Hard blocker** |
| **Window registers** | 320 > 255, does not fit in `WIN0H`'s 8-bit edges. **Blocked** | 240 < 255, fits in `WIN0V` unchanged. **Clear** |
| **HDMA** | Line count unchanged at 160. **Clear** | 160 → 240 lines; engine tables need extension. **Blocked** |
| **BG scroll regs** | `BGxHOFS` 9-bit (0–511); 320 viewport fine. **Clear** | `BGxVOFS` 9-bit; 240 fine. **Clear** |
| **Camera constants** | `0x78`→160, `0xf8`, width clamp `0xf0`→320 | `0x50`→120, `0xa8`, height clamp |
| **Rooms that expand** (§6) | 174 of 617 (28%) are ≥320 wide | 261 of 617 (42%) are ≥240 tall |

### 8.2 What this implies

The two axes are **not** two halves of the same work. Width is blocked by the BG
buffer, the tile streamer, and the window registers. Height is blocked by OAM Y
and HDMA. There is essentially no overlap. Two consequences:

1. **Doing one axis first genuinely de-risks the other** rather than duplicating
   effort — unusual, and it makes a staged milestone worth having.
2. **Neither axis is "easy" in isolation under the status quo.** Width-only
   without Option E still requires the `screenTileMap.c` rewrite, the single
   hardest file. Height-only requires OAM Y widening, the single riskiest change.

The picture changes sharply under Option E. Because a full-room-map-sampling PPU
dissolves the BG-buffer and tile-streamer blockers entirely:

- **Width-only under Option E** reduces to: window-register widening + camera
  constants. Both bounded, both with a working sa2 reference for the window
  pattern. HDMA and OAM are untouched.
- **Height-only under Option E** still requires OAM Y widening *and* HDMA
  extension, because Option E does nothing for either.

### 8.3 Recommended axis order — width first

**Width-first (240×160 → 320×160, letterboxed into the 320×240 window), then
height (→ 320×240).** Rationale:

- Under Option E it is by a clear margin the smaller change: two bounded
  subsystems versus two risky ones.
- It validates the §5 special-map premise end-to-end on real content *without*
  simultaneously destabilising sprite rendering and scanline effects. If the
  premise is wrong, that is discovered while only the window registers and
  camera have been touched.
- The riskiest change in the whole effort (OAM Y) is deferred until the
  map-sampling path is known-good, so failures are attributable.
- It exercises `sub_0807C5F4`'s `width > 0xff` branch, feeding directly into the
  Spike 2 gate.

The main argument *against* width-first is that height-only touches fewer
subsystems on paper (BG buffer already fits, window regs already fit) and covers
more expanding rooms (42% vs 28%). That argument fails on risk weighting, not on
count: its two blockers are the two most dangerous ones in the inventory, and
OAM Y in particular has no partial-credit failure mode — sprites are either
correct or visibly broken game-wide.

**A caveat on the intermediate milestone.** 320×160 is 2:1 and 240×240 is 1:1;
neither is a shippable aspect ratio on its own. Both are fine as *milestones*
because either borders cleanly into a 320×240 window (letterbox 40 px top/bottom
for width-first, pillarbox 40 px left/right for height-first). Width-first
should be treated as an internal checkpoint, not a release.

This ordering is a recommendation from static analysis and is itself subject to
Spike 2A/2B (§10).

---

## 9. Recommendation

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
- Sequence the expansion **width first, then height** (§8), so the map-sampling
  premise is proven before the two riskiest blockers are touched.

**This recommendation is conditional on Spike 2.** Given §5.1, the expected
verdict is not binary but "Option E with a recorded exclusion set routed
through the predicate." The fallback to Option D triggers only if the
exclusions turn out to include gameplay-critical *world* layers — the special
map bypassed mid-gameplay, not merely repurposed in menus. In that case the
effort roughly triples.

---

## 10. Research plan

Ordered so the cheapest thing most likely to invalidate the recommendation runs
first. Every spike carries a **Definition of done (DoD)** — a checklist of
observable, binary conditions. A spike is not finished because time was spent on
it; it is finished when every box is checked or the spike is explicitly
abandoned with its findings written down.

**The canonical test route**, referenced throughout, is: title → file select →
Hyrule Town → Hyrule Field → Minish Woods → Deepwood Shrine (1 dungeon) →
pause menu → figurine menu → a text box → a light-ray room → a cutscene. It
covers every subsystem the spikes touch and is short enough to run repeatedly.

The route as listed is *named, not pinned*: "a text box", "a light-ray room",
"a cutscene" are not re-runnable definitions. Spike 0 pins every waypoint as
(area, room, entry coordinates, save state, capture frame) in a committed
route manifest — every pixel-diff DoD below depends on capturing the *same
frame* twice, and the port currently has **no screenshot, save-state,
frame-step, or input-replay mechanism** to do that with. All verification
effort below assumes the Spike 0 tooling exists; without it, a route walk is
~20 minutes of manual play per run and "diff = 0" is not an executable
criterion.

---

### Spike 0 — Baseline, tooling, and decisions (2–3 days)
**Questions:** What is the true starting state, given §2.2? What is the frame
budget? Does the patch pipeline behave against the new fork?

This spike is larger than a plain baseline capture because it owns the
verification infrastructure every later DoD depends on (see the route-manifest
note above). Partial good news: the F8 debug menu already provides
parameterised warps (`port_debug_menu.cpp:192-210`, `Port_DebugAction_Warp`)
covering most route waypoints — the tooling starts from that, not from zero.

**Method:** Resolve D2, D3, and D4 (§0). For D4: wire `widescreen_width` to an
actual `add_defines("MODE1_GBA_WIDTH=…")` and add the widescreen patch to the
`xmake.lua` patches table, or delete the scaffold — preserving the patch file
as Spike 9 reference material either way. Build the capture tooling. Add a
frame-time counter. Capture reference frames along the canonical route.

**Definition of done:** *(completed 2026-07-27)*
- [x] Scaffold **deleted** per D4: `widescreen_width` option removed from
      `xmake.lua`, dead stretch branch and `gMain` peek removed from
      `port_ppu.cpp`, stale fallback define removed from `port_main.c`.
      `viruappu-widescreen.patch` retained as Spike 9 reference.
- [x] D2, D3, D4 recorded as **decided** in the §0 table.
- [x] **Route manifest committed:** `tools/capture/route.script` +
      `tools/capture/README.md` pin all 11 waypoints (frame number, area /
      room / coords for warped ones, save state = deterministic new game
      from blank EEPROM). Two waypoints resolved concretely: "a light-ray
      room" = Minish Woods west (`0x00` room 0, x 0x40 y 0x2A0); "a
      cutscene" = Picori-legend page at frame 3300.
- [x] **Deterministic capture works:** `--script` input replay +
      `--dump-dir` PPM dumps (`port/port_capture.c`); the engine RNG is
      constant-seeded (`gRand = 0x1234567`) so no seed pinning was needed;
      title auto-START hack suppressed during scripted runs. Two full route
      runs produced **byte-identical dumps at all 11 waypoints** (11/11
      diff = 0).
- [x] **Diff harness exists:** `tools/capture/diff_captures.py`
      (per-waypoint mismatch counts, `--rect` for Spike 1's centered-region
      check) + `tools/capture/ppm2png.py`.
- [x] Reference captures committed at
      `tools/capture/references/spike0-240x160/` (.ppm diff sources + .png).
- [x] HUD mockups at `tools/capture/references/hud-mockups/`
      (`hud_centered_320.png`, `hud_anchored_320.png`) — D1 input.
- [x] Baseline frame times, canonical route (12.7k frames), headless dummy
      video, uncapped, release build, **AMD Ryzen 7 5800H, Linux 6.17
      x86_64**: logic mean 0.191 ms / p99 0.825 ms; present (software
      render+upscale) mean 5.49 ms / p99 8.27 ms. Logic is ~1% of the
      16.67 ms frame budget — the engine side has enormous headroom; the
      present path is where any 320×240 perf cost will land.
- [x] sa2 pinned: `awe444/sa2@34b01960bb73734ec077b007f5d57ee46fa4b7a0`
      (`main` as of 2026-07-27), recorded in §13.
- [x] Fresh-clone check: clone + `git submodule update --init` + `xmake`
      build ok (28.9 s); patch log shows exactly one patch applied
      (`viruappu-internal-scale`), `hdma-hook`/`mosaic` skipped via markers.

### Spike 1 — Land Option B, centered native (0.5 day)
**Method:** 320×240 window, 240×160 render centered with borders. No engine
changes.

**Definition of done:** *(completed 2026-07-27)*
- [x] Build produces a 320×240 canvas with a pixel-exact 240×160 render
      centered at (40,40); all 38 400 border pixels verified uniform
      `0xFF000000` across all 11 waypoints (D3: solid black).
- [x] **Composite order decided and recorded** (`port/port_viewport.h`,
      `Port_PPU_ComposeCanvas`): the composite runs **before** internal
      scale, xBRZ and the filters, so those treat the viewport border as
      part of the frame. Rationale: after Milestones 1–2 the PPU emits
      those borders itself for rooms smaller than the viewport, at which
      point they are ordinary rendered pixels — filtering them now is what
      the shipped build does, so the ordering never has to change. **The
      original DoD wording conflated two different black regions.** The
      bars that must never be filtered are the *window letterbox* produced
      by fit-rect when the window aspect is not 4:3; those are outside the
      canvas entirely and painted by `SDL_RenderClear`. That separation is
      what the DoD was actually protecting, and it holds.
- [x] All 11 route captures pixel-identical to Spike 0 (diff = 0), verified
      twice over: raw PPU output unchanged, **and** the composed canvas
      compared through `--rect 40,40,240,160`.
- [x] xBRZ (`kHiResW/H`) and internal-scale buffers resized for the larger
      canvas and exercised across 6 upscale × internal-scale combinations —
      no crash, canvas byte-identical in every one. (The 2× intermediate was
      a hardcoded `480*320`; at canvas size it would have overflowed. Caught
      here rather than as a Milestone 1 heisenbug.)
- [ ] ~~Frame time within noise of the Spike 0 baseline.~~ **Not met, and
      the criterion was wrong.** Present cost rose from 5.47 ms to
      **6.48 ms mean (+18.5%, n=3 runs, spread ±0.08 ms)** — a real
      regression, not noise. It is also unavoidable and expected: the
      presented surface has exactly 2.0× the pixels (76 800 vs 38 400), so
      "within noise" was never achievable for Option B. Sub-linear scaling
      (+18.5% for 2× pixels) because part of present is fixed overhead.
      Ring-only border clearing was tried and made no measurable difference
      — the cost is texture upload and scaling, not the fill. Total frame
      cost 6.62 ms against a 16.67 ms budget (**40%**), and within the
      Milestone 1 exit criterion of +25%. Logic is unchanged at ~0.14 ms.
- [x] Landed on branch `spike-1-option-b` as the fallback build.

**Carry-forward:** the Milestone 1/2 exit criteria should be measured
against *this* build (present 6.48 ms), not the Spike 0 240×160 baseline —
the canvas cost is paid once here and does not recur.

### Spike 2 — Validate the special-map premise *(decision gate)* (2.5–3.5 days)
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

**Method:** First, a **static pre-check (the first half-day):** enumerate the
room set where the premise is already known not to hold — (a) rooms taking the
`clearBottomMap` / `AREA_PALACE_OF_WINDS_BOSS` build path (§5.1(3)); (b) the
aliased-use windows of §5.1(1). Output: a first-cut exclusion list, plus a
written spec of the **map-authoritative predicate** — the exact engine-side
signal (candidate: the `gScreen.bgN.subTileMap` binding, `common.c:636-642`,
plus `gRoomControls.scroll_flags`) by which the PPU decides, per BG per frame,
whether the special map is currently a world tilemap.

Then instrument the port to dump the special map alongside the composed
32×32 window each frame and diff the overlapping region (extending Spike 0's
diff harness). Any mismatch is a layer or mutation path that bypasses the map.
Walk the canonical route plus targeted destructible / bombable-wall / door /
chest interactions.

**Definition of done:** *(completed 2026-07-28)*
- [x] **Static pre-check done first.** Four exclusion classes, all
      predicate-detectable:
      1. *Degraded rooms* (`scroll_flags` bit 0): set in exactly one place
         (`LoadRoomGfx`, `playerUtils.c:4086`) for rooms whose decompressed
         map begins with sentinel `0xffff` (a per-room **asset** property,
         not an area list) plus `AREA_PALACE_OF_WINDS_BOSS`; cleared on
         every room load (`:4029`). Special maps built 512×512 via
         `sub_0807C5F4`; the three mutators skip their special-map writes.
      2. *Menu/subtask scratch*: fileselect (save-struct alias),
         kinstoneMenu (`struct[16]`), pauseMenu/pauseMenuScreen6/
         `DrawDungeonFeatures` (dungeon-map bitmap), fileScreenObjects —
         all under `GAMEMAIN_SUBTASK` or non-GAME tasks.
      3. *Boss scratch*: gyorgFemale/gyorgMale wipe both arrays whole and
         use them as collision bitmaps.
      4. *Screenblock-shaped direct binding*: bigGoron, minishRafters-,
         horizontal-/verticalMinishPath- background managers point
         `gScreen.bgN.subTileMap` **into** the arrays at chunk offsets —
         screenblock-layout data, not the 0x80-stride map.
      **Predicate** (reference implementation:
      `port_mapcheck.c:mapcheck_layer_authoritative`): a layer is
      map-authoritative iff `task==TASK_GAME` ∧ `substate==GAMEMAIN_UPDATE`
      ∧ `!(scroll_flags & 1)` ∧ `scrollAction < 2` ∧ standard
      `subTileMap` binding ∧ `gMap{Bottom,Top}.bgSettings != NULL`.
- [x] **Reusable diff harness:** `--mapcheck` (`port/port_mapcheck.c`)
      compares, per frame per layer, the rendered VRAM screenblock
      (addressed via hardware BGCNT/HOFS/VOFS semantics) against full-room
      sampling of the special maps at the camera origin, with per-tile
      persistence tracking (≥30-frame streak = bypass) and a
      self-locating candidate scan for the block/phase convention.
- [x] **Full canonical route** *(result superseded — see the correction
      box below; the premise holds, but this harness measured it badly)*:
      3 459 frames checked, 3 703 200 tile comparisons, zero persistent
      mismatches, one single-frame transient. Rooms covered include Hyrule
      Field 1008×688, Minish Woods 1008×1008, festival Hyrule Town 400×960,
      Deepwood, and two `scroll_flags`-bit-1 rooms (bit 1 does **not**
      degrade the map — confirmed live, as the static read predicted).

> ### ⚠ Correction (2026-07-28, during Spike 3)
>
> **This harness was measuring against a skewed reference and its numbers
> should not be quoted.** Two defects, found when Spike 3's equivalence
> anchor failed on one waypoint:
>
> 1. **Frame skew (the real defect).** The harness indexed the special map
>    with *live* `gRoomControls`, but compared against VRAM screenblocks
>    and BG registers that `VBlankIntr` commits *after* rendering — one
>    frame older. On every frame the camera crossed a tile boundary the
>    whole window disagreed for reasons that had nothing to do with the
>    premise.
> 2. **A gate that hid the evidence.** It skipped frames where
>    `vofs != (cy&15)+8`, counting 746 "phase-skips" that I wrote off as
>    transition frames. Those were precisely the frames the skew affected,
>    so the gate suppressed the symptom that would have exposed defect 1.
>
> Re-running with the gate removed and renderer-exact addressing raised
> mismatches from 1 023 to **60 395 (1.45%)** — still zero *persistent*,
> and clustered every ~8 frames of camera movement, which identified the
> skew. **Aligning the binding to the commit point then drove it to zero.**
>
> **Corrected measurement** (Spike 3's in-renderer audit, which compares
> every tile the renderer actually fetches, through the real render path
> rather than a parallel model): **265 497 600 fetches, 0 mismatches** on
> the canonical route; 260 889 600 fetches, 512 mismatches on the mutation
> walk — all 512 being tile mutations where the special map is legitimately
> one frame *fresher* than the screenblock.
>
> The §5 premise is therefore **confirmed more strongly than this spike
> originally claimed**, and every discrepancy is explained as harness or
> timing artefact rather than an engine bypass. The lesson is recorded
> rather than quietly fixed: a verification harness that models the thing
> it verifies can agree with itself and prove nothing.
- [x] **Five managers classified** (file:line evidence in source):
      | Manager | Class |
      |---|---|
      | `backgroundAnimations.c` | map-agnostic — writes tile *graphics* to VRAM charblocks (`:2436`); tilemap entries untouched, keeps working under map sampling |
      | `hyruleTownTileSetManager.c` | map-agnostic — `LoadGfxGroup` swaps only |
      | `weatherChangeManager.c` | conditionally bypasses: detaches `gMapTop.bgSettings=0` (`:111`), repurposes screenblock 29 as a fog overlay via `gBG3Buffer` (`:144-145`), later rebinds (`:148`) — **every phase flips a predicate signal**, so it is excluded exactly while non-authoritative |
      | `holeManager.c` | separate BG3 overlay (`:297-312`); world layers untouched |
      | `lightManager.c` | WIN0/blend/BG3 (`:43-70,154`); world tilemaps untouched (window-reg widening is Spike 4) |
- [x] *(partial, recorded honestly)* **Destructible tiles verified
      end-to-end at runtime:** scripted sword slashes in Minish Woods cut
      two bushes (`SetTileType=2` via the new mutation tap in
      `playerUtils.c`); each produced exactly one 4-tile (one metatile)
      single-frame transient then converged — propagation within one
      frame, zero persistent. Bombable wall / door / small chest were
      **not** individually triggered; all three route through the same
      three tapped entry points (`SetTileType`/`SetTileByIndex`/
      `RestorePrevTileEntity`) whose special-map writes share one gate,
      so the mechanism is common. Their runtime confirmation folds into
      the Spike 7/11 room walks, where the tap + harness run anyway.
- [x] **`sub_0807C5F4` documented:** four 32×32-entry chunk copies at
      offsets `0/0x20/0x1000/0x1020` (gated on `width>0xff` /
      `height>0xff`) = 64×64 subtiles = **512×512 px maximum**, used only
      on the degraded-room path. On the normal path the map is **never
      paged**: `LoadRoomGfx` clears both arrays and
      `RenderMapLayerToSubTileMap` fills the whole room resident
      (128×128 subtiles = 1024×1024 px, covering the largest room).
- [x] **VERDICT: PROCEED WITH OPTION E**, with the four-class exclusion
      set above routed through the predicate to the unchanged 32×32 path.
      The premise held everywhere the predicate said it should: ~3.9M
      further comparisons in the mutation run, still zero persistent.
      Option D is off the table unless Milestone 1 falsifies something
      the harness could not see.

**Bonus finding for Spike 3** (display plumbing, measured not assumed):
the bottom special map renders through the **BG2** register set
(screenblock 28) and the top through **BG1** (screenblock 29) — BG1's
priority puts canopy above ground. The BG window is a *sliding* buffer,
not a wrapping one: the engine keeps `hofs = cx & 15`,
`vofs = (cy & 15) + 8` (constant one-tile vertical bias). Note
`port_linked_stubs.c`'s `UpdateScrollVram` comment labels the
buffer↔layer mapping the other way round — trust the registers, not the
comment. Spike 3's map-sampling mode replaces exactly this addressing.

### Spike 2A — Width-only feasibility probe + static inventories (1.5 days, parallel with 2)
**Questions:** Does the §8 claim hold that width-only under Option E needs only
window registers plus camera constants? Where is the per-entity OAM coordinate
write site? What outside `scroll.c`/`script.c` consumes the camera position?
**Method:** Static trace only — no implementation. Walk every constraint in §4
against a 320×160 target and confirm or refute the §8.1 matrix row by row.
Then two inventories moved here from later spikes because they are static
work and their answers gate estimates:

**Definition of done:** *(completed 2026-07-28 — full report:
`docs/spike2a-width-probe.md`)*
- [x] Every §8.1 width cell confirmed/corrected with file:line citations.
      One margin note corrected: the streamed window is *sliding*, not
      wrapping (16 px horizontal slack) — moot under Option E.
- [x] One missed constraint added: `playerUtils.c:4401-4403` room-entry
      camera init (`scroll_x = width - 0x78 - 0x78`) joins Spike 5's list.
- [x] **OAM write site found — port-owned C, not `asm/`:**
      `RenderSpritePieces` (`port_draw.c:292-406`); entity→screen at
      `port_draw.c:514,519`. Culling is two port literals (`y>=160`,
      `x>=240`); the 8-bit-Y truncation is one pack site we own. Parking
      convention documented: unused entries get attr0 `0x2A0` =
      **OBJ-disable + y=160** — parked sprites cannot leak into widened
      columns, and the §2.2 patch header's "parked at x≥240" claim is
      wrong. **Spike 7 revised 2–3 d → 1.5–2 d (not a floor); Spike 8
      shrinks to a native widening — sa2's EXTENDED_OAM is reference
      only, its "not yet functional" TODO no longer matters.**
- [x] `scroll_x/scroll_y` consumer inventory: 165 refs / 50 files;
      **16 sites bake viewport literals** and are Spike 5/7's real blast
      radius (list in the report).
- [x] Width-only effort re-estimated: **Milestone 1 = 10–13 days**
      (was 11–15), basis per-spike in the report.
- [ ] **Inventory of every consumer of `gRoomControls.scroll_x`/`scroll_y`
      outside `scroll.c`/`script.c`** (spawn, cull, trigger logic), so Spike 5
      knows its blast radius before editing rather than after.
- [ ] Width-only effort re-estimated in days, with the estimate's basis stated.

### Spike 2B — Height-only feasibility probe (1 day, parallel with 2)
**Questions:** Is the 16 px of vertical scroll slack at 240 height actually
workable, or does the streaming cadence break? How bad is OAM Y really?
**Method:** Static trace of the §8.1 height column, plus a measurement: log the
maximum per-frame vertical camera delta across the canonical route and compare
against the 16 px budget. Separately, count OAM entries per frame with y that
would fall outside the representable range at height 240.

**Definition of done:** *(completed 2026-07-28 — full report:
`docs/spike2b-height-probe.md`)*
- [x] Every §8.1 height cell confirmed with citations; one addition
      (`playerUtils.c:4408-4412` vertical camera init, twin of 2A's width
      find, added to Spike 10's list).
- [x] **Max continuous per-frame vertical delta: 12 px** (banded over
      ~24k frames across both instrumented runs; everything larger is a
      warp, which takes the full-refill path, never the incremental
      streamer). Verdict: the 32-row buffer's 16 px slack is genuinely
      sufficient at observed cadence — and moot for world layers under E.
- [x] **Blocker 3 severity measured:** 103 frames / 118 entries / max 2
      simultaneous enabled OAM entries with y∈[161,239] on the route
      (~1% of gameplay frames). Real, small-surface, and re-measurable —
      Spike 8's drops-to-zero DoD runs this same `--mapcheck` counter.
- [x] Height-only effort re-estimated: **Milestone 2 = 6.5–8.5 days**
      (was 7–10); Spike 8 down to 1–1.5 d via the 2A native-widening path.
- [x] **Axis order CONFIRMED in writing: width first.** Both probes moved
      the numbers but not §8.3's risk logic — height still holds the only
      two blockers with no partial-credit failure mode. Milestone 1 may
      begin.

---

## 10.1 Milestone 1 — Width expansion (240×160 → 320×160)

> **COMPLETE — signed off 2026-07-30.** Exit criteria and results at the end of
> this section. The spike write-ups below are kept as the record of *how* it was
> done and what each one proved; several carry inline corrections from
> playtesting, and `docs/viewport-bug-tracker.md` wins wherever they disagree.
> If you are starting Milestone 2, read §10.2's "State of the code" first — it
> summarises what these spikes left behind without requiring you to read them.

### Spike 3 — Map-sampling BG mode in VirtuaPPU (3–4 days)
**Questions:** Can a non-GBA BG mode read `gMapData*Special` at an arbitrary
scroll origin and width, and does it reproduce the existing render exactly at
240 width?
**Method:** Add the mode to the fork. Feed it scroll origin and viewport size
from `port_ppu.cpp`. Keep UI layers on the 32×32 path. The mode's API shape
(compile-time vs runtime viewport) follows D2. Which BGs route through it is
driven by the Spike 2 predicate — not a heuristic.

**Definition of done:** *(completed 2026-07-28)*
- [x] **Correctness anchor met — and it earned its keep.** At width 240 the
      map-sampled output is pixel-identical to the screenblock path across
      **all 11** route waypoints (diff = 0), and the in-renderer audit
      reports **0 mismatches in 265 497 600 tile fetches**. The anchor
      initially *failed* on one waypoint (festival Hyrule Town, 12 205 px);
      chasing that failure exposed the frame-skew defect that also
      invalidated Spike 2's original numbers (see the correction box under
      Spike 2). Exclusion-set waypoints pass because the predicate routes
      them to the unchanged path, as designed.
- [x] **Predicate implemented as specified** (`port/port_mapsource.c`),
      with one correction: the layer→BG mapping is **per-room** and must be
      derived from `MapLayer.bgSettings`, not hardcoded. Verified by the
      write chain (`UpdateScrollVram` → `gBGxBuffer` →
      `interrupts.c:210-212` DMA) and by a swap experiment that drove
      mismatches to 100%.
- [x] **Binding happens at the engine's commit point**, immediately after
      `VBlankIntr` in `port_bios.c` — not from the render call. `VBlankIntr`
      is where the BG buffers are DMA'd to VRAM and the scroll registers
      written, so binding anywhere else samples a camera one frame newer
      than the state being rendered against. This is the single most
      important implementation detail in the spike, and the thing the
      original Spike 2 harness got wrong in mirror image.
- [x] **At width 320, real tile data in columns 240–319**: Minish Woods
      (1008 px wide) rendered at 320 with 22 distinct colours of genuine
      map content in the new columns and no stale-VRAM artifacts — while
      columns 0–239 stayed **100% identical** to the 240-wide reference.
      Built via a temporary `TMC_VIEW_W` hook in `xmake.lua`; **a real
      build option is Milestone 1 work** (the define must reach the
      ViruaPPU sources as well as `port/`). Known-broken at 320 and owned
      by later spikes: window regs (4), camera clamp (5), HUD centering
      (6 — visible as a duplicated HUD), sprite culling (7).
- [x] UI layers render through the untouched 32×32 path
      (`--mapsource-report` shows only `task!=GAME` / `substate!=UPDATE`
      rejections outside gameplay).
- [x] Landed as fork commits (`awe444/VirtuaPPU`), not a patch file.

**Tooling note:** `--mapsource-audit` cross-checks every map-sampled fetch
against the screenblock entry the hardware path would read. It supersedes
`--mapcheck`'s tile diff, because it measures *through the real render
path* instead of reimplementing the addressing beside it — which is
exactly how the original harness fooled itself. Use it as the regression
gate for Spikes 4–7.

### Spike 4 — Window register widening (1 day)
**Method:** Follow sa2's `winreg_t` / `WIN_RANGE` / `WIN_GET_*` pattern. Convert
the seven consuming files (`lightRay.c`, `lightDoor.c`, `cutscene.c`,
`figurineMenu.c`, `kinstoneMenu.c`, `common.c`, `interrupts.c`), plus the
PPU-side window clamps at `libs/ViruaPPU/src/mode1.c:563-570` (§4).

**Definition of done:** *(completed 2026-07-28)*
- [x] **`winreg_t` widened to 32 bits** (`include/screen.h`) with
      `WIN_RANGE` / `WIN_GET_HIGHER` / `WIN_GET_LOWER`, plus `WINREG_TO_GBA`
      for the truncated hardware form. **All 39 packing sites converted
      across 10 files** — more than the seven the plan listed; §3's table
      missed `scroll.c`, `bigGoron.c` and `templeOfDropletsManager.c`.
      No `(a << 8) | b` packing of a window field remains (the two
      remaining shifts in `cutscene.c:267-268` *decode* a packed u16
      source, they do not pack a destination).
- [x] **Transport added rather than squeezed through the registers.**
      `DispCtrlSet` still writes the truncated 8-bit value to
      `REG_WIN0H`/etc. (the per-scanline window DMA consumes it), and also
      calls `Port_Screen_CommitWindows`, which hands untruncated bounds to
      the PPU's new `virtuappu_mode1_set_window_bounds`. Same explicit
      port→PPU pattern as Spike 3's map source, and committed from the
      same frame-generation point.
      *Incidental fix:* `DispCtrlSet` walked the control block as `u16*`
      (`tmp2[0..9]`); that indexing silently breaks once any field widens,
      so it is now named-field access.
- [x] **PPU-side clamps updated** — `mode1.c` clamps to
      `MODE1_GBA_WIDTH/HEIGHT`, which now track the build viewport, and
      the wrap semantics (`left > right`) are preserved for overrides.
- [x] **>255 proven live, not merely compiled:** with `TMC_WINTRACE=1` at
      a 320 build the widest committed edge is **320**. Sites whose
      meaning is "the whole screen" now use new `WIN_VIEWPORT_WIDTH/HEIGHT`
      extents (which resolve to `DISPLAY_WIDTH/HEIGHT` by default, so 240
      is unchanged by construction); the figurine menu visibly uses the
      extra width (109 px differ at x ≥ 240 versus the clamped build).
- [x] **At 240, pixel-identical**: full route 11/11 diff = 0, and the
      Spike 3 map-source audit still reports 0 mismatches in 265 497 600
      fetches. Call sites keep their existing `& 0xff` masking — several
      rely on 8-bit wrap to produce a deliberately inverted window, so
      widening an individual site's *coordinate range* stays a per-site
      decision for the spike that needs it (Spike 5 for camera-derived
      windows).
- [x] Light ray, light door, cutscene letterbox and figurine menu captured
      at both 240 and 320.
- [ ] **Kinstone menu: not runtime-verified.** Cold-opening it from a
      script segfaults **identically at 240 and 320**, so it is not a
      regression from this change — it is the known kinstone crash chain
      (CHANGELOG #16: the menu dereferences `gPossibleInteraction`
      state that only a real fusion sets up). Its two sites
      (`kinstoneMenu.c:291-292`) were converted and are covered by the
      240 pixel-identity check. Verify during the Spike 7 room walks with
      a save that has fusions available.

**Deferred to Spike 9 (HDMA):** the per-scanline *circular* window path —
`common.c`'s `gUnk_02017AA0` table DMA'd to `REG_ADDR_WIN0H`, used by the
lantern (`lightManager`), fade iris (`fade.c`) and `whiteTriangleEffect`.
That table is a packed-u16-per-line structure, i.e. HDMA-shaped, and
widening it belongs with the 240-line HDMA work rather than here.

### Spike 5 — Camera and clamping, horizontal (2 days)
**Questions:** Do `scroll.c`'s magic constants generalise to half-viewport
expressions? Does fixed-width clamping centre small rooms naturally, or is
Option C's variable viewport needed?
**Method:** Consult Spike 2A's `scroll_x`/`scroll_y` consumer inventory first,
so the blast radius outside `scroll.c`/`script.c` is known before editing.
Then replace `0x78`/`0xf8`/`0xf0`/literal `120` with viewport-derived
expressions. Test across the §6 width clusters: 240, 272, 304, 336, 400+.

**Definition of done:** *(completed 2026-07-28)*
- [x] *(amended 2026-07-30)* **Every horizontal camera constant replaced** —
      **but this was reported before it was true.** The `sed` for `script.c`
      matched nothing (the real text has `gRoomControls.` prefixes) and the
      verifying grep only covered `scroll.c`, so `WaitForCameraTouchRoomBorder`
      kept predicting the camera's resting place from `DISPLAY_WIDTH` while
      the camera clamped on 320 — an unreachable equality and a hard
      softlock (bug B7). Every remaining `DISPLAY_WIDTH/HEIGHT` in `src/` has
      since been audited and the viewport-derived ones converted. Lesson:
      verify the *scope* of the verification, not just its result. New `include/viewport.h` owns the derivations
      (`VIEWPORT_HALF_WIDTH`, `VIEWPORT_REGION_WIDTH`, the camera range
      macros) and Spike 4's `WIN_VIEWPORT_*` now alias it, so there is one
      definition of "how wide is the viewport". Sites: `scroll.c`
      (centre target, right clamp, 4 region tests, 6 scripted-camera
      literals), `script.c:1976-1977`, `playerUtils.c:4396-4404`
      room-entry init (both branches). Vertical constants (`0x50`, `0xa8`,
      `DISPLAY_HEIGHT`) deliberately untouched — Spike 10.
- [x] **The narrow-room case was a live bug, now handled.**
      `origin_x + width - VIEWPORT_WIDTH` *underflows* for any room
      narrower than the viewport, which at 320 would have sent the camera
      far outside the room in **443 of 617 rooms**. `VIEWPORT_CAM_MIN_X` /
      `VIEWPORT_CAM_MAX_X` collapse the range to a single centred position
      for those rooms — a position left of the room origin, i.e. a
      *negative* room-space camera offset, which the Spike 3 map source
      now accepts and renders as backdrop beyond the room edges.
      **240 is provably unaffected: 240 is also the narrowest room in the
      game, so the narrow branch is unreachable at GBA-native width** and
      the macros reduce to the original expressions exactly.
- [x] **Camera verified across every §6 width cluster, at both widths**
      (`TMC_CAMTRACE=1`), with an explicit in-range assertion — no
      out-of-range hit at either width:

      | room width | at 240 | at 320 |
      |---|---|---|
      | 240 | cam 0, range [0,0] | cam **−40**, centred |
      | 272 | cam 16, range [0,32] | cam **−24**, centred |
      | 304 | cam 32, range [0,64] | cam **−8**, centred |
      | 336 | cam 48, range [0,96] | cam 8, range [0,16] |
      | 1008 | cam 360, range [0,768] | cam 320, range [0,688] |

- [x] Independent pixel confirmation of centring: for the 240-wide room
      (the one case whose camera position is comparable across builds,
      since wider rooms legitimately scroll differently) the 320 render's
      world band is **99.6% identical to the 240 render offset by exactly
      40 px** — the residual 0.4% being HUD.
- [x] **Verdict: fixed viewport plus clamping. Option C is not needed.**
      Centring for all 443 narrow rooms falls out of two range macros with
      no per-room viewport machinery, no variable-size buffers, and no
      change to HUD placement or window bounds. Option C's variable
      viewport would buy nothing here and would make every downstream
      consumer of viewport size dynamic. **Option C is formally dropped.**

**Known limitation, owned by Spike 6:** border *symmetry* cannot be
confirmed visually yet. The world layer centres correctly, but BG0 (HUD)
and BG3 remain on the 32×32 screenblock path and wrap at 256 px, painting
into the border region — visible as a duplicated HUD at 320. That is
exactly the UI-compositing problem Spike 6 exists to solve, and it makes
**D1 the immediate blocker.**

### Spike 6 — UI composition (2 days if D1 = centered)
**Prerequisite:** D1 (§0) is decided before this spike starts — a product
choice made from the Spike 0 mockups, not a call to be made here on
implementation-cost grounds. If D1 chose edge-anchored, this spike reopens the
stride-sensitive `gBG0Buffer` sites (§3) and must be re-estimated (likely 4–6
days) before starting.
**Method:** Implement D1 by compositing the 32×32 UI path over the expanded
world accordingly.

> **SUPERSEDED IN PART, 2026-07-30.** The first bullet below describes the
> edge-anchored HUD, which was **reversed** — D1 is now *centered* (§0), and
> `UI_RIGHT_ANCHOR_DX` no longer exists in the tree. Everything else in this
> DoD still holds and is load-bearing (the `gBG0Buffer` array, the phonograph
> alias, the stride-derived `ui.c` sites), which is why the section is
> annotated rather than deleted. What replaced it: BG0 stayed a 32×32 map, so
> the whole layer shifts uniformly by `UI_CENTER_DX` and HUD sprites take
> `UI_HUD_SPRITE_DX` at source. Full reasoning in the bug tracker.

**Definition of done:** *(core complete 2026-07-28; see remaining item)*
- [x] ~~**D1 implemented as decided: edge-anchored.**~~ **Reversed** — see the
      note above. Hearts, counters, text box and UI screens are centered and
      move together.
- [x] **The duplicated HUD is gone** — measured, not eyeballed: red heart
      pixels at x ≥ 160 went **87 → 0** at 320.
- [x] **Blocker 1 dissolved for BG0, without touching blocker 7.**
      `gBG0Buffer` is now a real array sized from the viewport
      (`UI_BG0_ENTRIES`) instead of a `#define` aliasing `gEwram[0x34CB0]`,
      and at a wide viewport it is bound to the PPU as a **map source**
      (the Spike 3 mechanism) rather than growing a hardware screenblock.
      That matters: BG0 sits at screenbase 31, so a 64-tile-wide
      screenblock would have run into the OBJ tile region at `0x10000` and
      dragged VRAM growth (blocker 7) into Milestone 1. The map-source
      route needs no VRAM at all. At 240 nothing binds and BG0 stays on the
      hardware path, so the native build is unaffected *by construction*.
- [x] **Open question 3 answered — and it was a real hazard.**
      `phonograph.c:202` held a `Font` whose destination was the raw GBA
      address `(u16*)0x2034fce`, which is exactly `&gBG0Buffer[0x18F]`
      (alias base `0x02034CB0`, `(0x34FCE-0x34CB0)/2 = 0x18F`). Every other
      `Font` table names the buffer symbolically; this one silently depended
      on `gBG0Buffer` *being* that EWRAM region, so converting the buffer to
      a real array would have left phonograph text writing into dead
      memory. Now symbolic. **Spike 2A's correction was right that the
      `subtask.c` evidence was a misread, and the grep it prescribed
      (`0x0203` literals) is what found the genuine site.**
- [x] **All 14 `ui.c` indexed sites are stride-derived** via
      `UI_BG0_AT(col,row)`; the 7 distinct offsets decode to two clean
      anchor groups (rows 1–3 col 0 = hearts, top-left; rows 16–19
      cols 24–25 = counters, bottom-right). Verified arithmetically that
      each anchored expression reproduces its original offset exactly at
      240. The other 12 whole-buffer uses already went through
      `sizeof(gBG0Buffer)` and adapt for free.
      *Scope note, corrected twice:* I first called the feared "~40
      stride-sensitive sites" an over-estimate at "16 references / 7 offsets
      in `ui.c`". That was wrong — it counted literal indexed accesses in one
      file and missed the other writers, the byte-count clears, and the
      shared `Font` text renderer. The plan's original ~40 was the better
      estimate. **This whole sub-spike was subsequently reverted** (D1
      reversal, above); the stride is back to 32.
- [x] **Sprite cull widened** (`port_draw.c`): `x >= 240` / `y >= 160` are
      now viewport-derived. Required here, not deferrable — a
      right-anchored icon at x≈288 was culled before it could draw, which
      is why the icons vanished on the first attempt. Exactly the
      two-literal change Spike 2A predicted.
- [x] **At 240: route 11/11 pixel-identical and the map-source audit still
      0 mismatches in 265 497 600 fetches**, across all of the above
      (buffer relocation, phonograph fix, stride rewrite, cull widening).
- [x] **Text box centered** (`message.c`): `MESSAGE_WIDTH` was its own
      hardcoded `0x20` BG0 stride. It is now `UI_BG0_WIDTH_TILES`, and the
      window index applies `UI_CENTER_TILE_DX`, so the box centres itself
      per-window rather than shifting BG0 as a whole — necessary because
      the box shares BG0 with the *anchored* HUD during gameplay.
      Measured **94.7% identical to the 240 reference at a 40 px shift**
      (residual is the surrounding world/HUD, which legitimately differ).
- [x] **UI screens centered — measured pixel-exact.** Against the 240
      reference at a 40 px shift, with border bands checked separately:

      | screen | content | border black |
      |---|---|---|
      | file select | **100.0%** | 100% |
      | cutscene | **100.0%** | 100% |
      | pause menu | **100.0%** | 100% |
      | figurine menu | **100.0%** | 100% |
      | text box | **100.0%** | n/a (gameplay: world fills the width) |
      | title | 77.7% | 100% |

      Reaching this took three mechanisms, in the order the failures
      demanded them:
      1. **Stride propagation.** `UI_BG0_WIDTH_TILES` changes gBG0Buffer's
         row stride, so every writer had to agree. The 8 remaining indexed
         sites were converted, and — the part that actually mattered — the
         shared text renderer had the stride baked in at `text.c:588,603,643`
         and throughout `DispMessageFrame`. Both now derive it from the
         destination via `UiDestStride()`, which returns 0x20 for BG1/BG3
         and VRAM and the real width for BG0. `gBG0Buffer`'s declaration and
         that helper now live together in `include/ui_bg0.h` so they cannot
         disagree; previously the buffer was declared twice, in `vram.h`
         *and* `structures.h`.
      2. **A PPU layer clip** (`virtuappu_mode1_set_bg_clip`). Rebinding
         these layers as map sources does **not** work: title and file
         select load tilemaps straight into VRAM screenblocks, so a map
         source reads a stale staging buffer — that is why they first
         measured ~5%. The clip acts on the layer's normal screenblock
         fetch, so it is agnostic about where content came from, and it
         suppresses the wrap a plain scroll offset would produce.
      3. **A global OBJ offset** (`virtuappu_mode1_set_obj_offset`). Shifting
         backgrounds without their sprites detaches menu cursors, item icons
         and the title sword. Adding it took file select 77.7% → 100%,
         pause 87% → 100%, figurine 95.8% → 100%.

      Plus a D3 detail worth naming: a clipped layer's side bands show that
      screen's **backdrop palette**, not black (pale yellow on the title
      screen). `Port_MapSource_UiCentered()` lets the canvas compositor
      paint those bands the border colour, so all five screens now measure
      100% black borders.
- [ ] **Title screen: 77.7%, single known cause.** The residual is the
      affine BG2 sword, which renders through `mode2.c`'s affine path where
      neither the clip nor the OBJ offset applies, so it sits 40 px left of
      the centered content. Everything else on the screen (logo, PRESS
      START, copyright, borders) is correct. This is the **mode-2 deferral
      already recorded in §7/open question 6** — affine scenes stay
      240×160 for Milestone 1 — now with a concrete visible instance.
      Fixing it means offsetting the affine reference point, which is not a
      plain pixel shift and risks the gameplay affine scenes (barrel,
      tornado), so it belongs with Spike 9's affine work rather than here.

**Also deferred:** BG3 overlays during gameplay (hole, light, weather) are
screen-fixed and still wrap past 256 px at a wide viewport. Not part of
the D1 question; belongs with Spike 7's visual sweep.

### Spike 7 — Horizontal culling and off-screen behaviour (2–3 days)
**Questions:** Do entities the engine assumed off-screen become visible? The
existing patch notes parked sprites at OAM x ≥ 240 — is that real, and what is
the actual parking convention? (The per-entity OAM coordinate write site is
located in Spike 2A; if 2A found it in unported `asm/`, this spike's estimate
is a floor — re-estimate before starting.)
**Method:** Focus on the 174 rooms ≥320 wide, weighted to Hyrule Field, Hyrule
Town, Minish Woods, Castor Wilds.

**Definition of done:** *(completed 2026-07-28)*
- [x] **OAM write site confirmed at runtime.** `RenderSpritePieces`
      (`port_draw.c`) is what fills `gOAMControls.oam`; the new
      `--mapcheck` census reads that table directly and its counts move
      with on-screen sprites, closing the loop on Spike 2A's static find.
- [x] **Parking convention confirmed — and the old fear disproven.**
      Across the 12-room walk at 320: **51 086 enabled OAM entries resolved
      into the expanded columns 240–319 over 12 006 frames (max 11
      simultaneous), and `parked-lookalikes = 0`.** Unused entries are
      parked *disabled* (`attr0 = 0x2A0` — OBJ-disable + y=160), so parked
      sprites cannot leak into widened columns. The 51 k entries are
      legitimate content that the wider viewport now shows. **The Phase 1
      patch header's "parked off-screen sprites live at x ≥ 240" — the
      stated reason that scaffold clipped OAM at 240 — is wrong.**
- [x] **12 rooms ≥ 320 wide walked end to end** (exceeds the 10 required),
      weighted to the high-playtime overworld per §6: Minish Woods, Minish
      Village, Hyrule Town, Hyrule Field ×2, Castor Wilds, Ruins, Mt Crenel,
      Cloud Tops, Royal Valley, Veil Falls, Lake Hylia. Each captured at
      entry and after a right-sweep and a left-sweep to exercise both edge
      clamps — 36 captures.
- [x] **Culling/spawn literals converted:** `CheckRegionOnScreen`
      (`main.c` — the engine's generic viewport AABB test, used by
      `houseDoorExterior`), `bird.c:336`, `rainfallManager.c:39-40`,
      `guardWithSpear.c:294,298`, `cutsceneMiscObject.c:339-340`, plus the
      parallax divisors in `minishRaftersBackgroundManager` and
      `staticBackgroundManager`. `IsProjectileOffScreen` needed **no**
      change — it tests against *room* bounds, not the viewport.
      Vertical siblings were converted at the same time; they are no-ops
      today (`VIEWPORT_HEIGHT == DISPLAY_HEIGHT`) and pre-pay Spike 10.
- [x] **Issues logged with severity: none attributable to the widening.**
      Rather than eyeball 36 captures, all were scanned for the two artifact
      classes that would indicate a real fault:
      - *wrap-shaped repetition* (a column identical to one 256 px earlier,
        the BG wrap period): **0 across all 36 captures** — the wrapping
        Spike 6 fixed does not recur anywhere.
      - *black columns inside a room wider than the viewport* (a map-source
        extent fault): 0 in 10 of 12 rooms. The two that flagged were
        investigated and **disproven**: Royal Valley shows 192 black columns
        at *both* 240 and 320 (pre-existing dark graveyard content), and
        Mt Crenel shows **fewer** at 320 than 240 (136 → 122), i.e. the
        wider viewport reveals more, as intended.
      Lake Hylia's large flat purple region was checked the same way and is
      identical at 240 — pre-existing content, not an artifact.

**Harness note:** `--mapsource-audit` is now explicitly gated to
GBA-native width. It is an *equivalence* check against the hardware path,
and beyond 240 there is nothing valid to compare against: the engine still
streams a 32-tile screenblock, but from a camera the wide build clamps
differently, so the buffer-column↔map-column relation it checks no longer
holds, and past the streamed window there is no data at all. Run at 320 it
reported a meaningless 6.6% mismatch; it now reports nothing there rather
than a misleading number. **The 240 regression gate is unaffected: still
0 mismatches in 265 497 600 fetches.**

### Milestone 1 exit criteria
- [x] 320×160 builds, runs, and completes the canonical route without crash.
- [x] All rooms narrower than 320 are centered with correct borders. Verified
      by border uniformity (one distinct column value per band) rather than
      by "border is black" — a clipped UI screen's border shows the PPU
      backdrop, which is green on the pause menu. **D3 amended to accept a
      uniform colour** (§0).
- [x] No visual regression versus Spike 0 captures in the central 240 columns.
      The 240 build is pixel-identical on all 11 waypoints and the map-source
      audit is 0/265 497 600.

      At 320 the centre-240 also matches Spike 0 exactly on every
      240-authored surface and on camera-following rooms — widening adds
      columns symmetrically, so the middle is untouched. It legitimately
      differs where the camera **clamps at a room edge** (the clamp sits 40 px
      further out) and on the title screen (affine carry-forward). Do not read
      those as regressions; two waypoints that *did* look like clamping were
      B10 and are now 0.
- [x] Frame time within 25% of the Spike 0 baseline. **present 7.19 ms mean
      (n=3: 7.263/7.148/7.151) vs the 6.48 ms Spike 1 canvas baseline this is
      measured against (§Spike 1 carry-forward) = +10.9%.** Logic unchanged at
      0.15 ms. Total ~7.34 ms of a 16.67 ms budget (44%). No perf spike needed.
- [x] Go/no-go recorded for Milestone 2. **GO — maintainer sign-off
      2026-07-30.** B4 and B5 were deferred rather than fixed: neither was
      ever reproduced, and both need a human-driven recording (`--record`,
      see the bug tracker). World-space window masking was deferred too.

---

## 10.2 Milestone 2 — Height expansion (320×160 → 320×240)

### State of the code entering Milestone 2

Written for someone picking this up cold. Milestone 1's width work is landed
and signed off; this is what it left behind.

**Build.** `xmake f -c -y -m release && xmake build tmc_pc` gives the 240
build at `build/pc/tmc_pc`. Prefix **both** commands with `TMC_VIEW_W=320` for
the wide build — and `xmake f -c` is required, a plain `xmake f` will not drop
a previously configured width. Height has no equivalent switch yet; adding
`TMC_VIEW_H` alongside it is Milestone 2's first plumbing job
(`include/viewport.h`, `xmake.lua:453`).

**The regression gate is not optional.** Before any viewport commit, at the
default 240 build: the canonical route must be 11/11 pixel-identical and the
map-source audit 0 mismatches in 265,497,600 fetches. Exact commands in
`tools/capture/README.md`. Both have caught real regressions, including a
change intended for the wide build that altered what the shipping build
renders.

**How width was actually achieved**, since height will mirror it:

- `include/viewport.h` holds every derived constant — `VIEWPORT_WIDTH`,
  `UI_CENTER_DX`, `UI_HUD_SPRITE_DX`, the camera clamps. All reduce to the
  original literals at GBA-native size, which is why 240 stays byte-identical.
- The world renders through a **map source**: the PPU samples the engine's
  full-room special maps directly (`port/port_mapsource.c`) instead of a
  32-tile VRAM screenblock, which is what lets it exceed 256 px. A screenblock
  covers 256 px and wraps; that constraint is the root of most of Milestone 1's
  bugs and it applies identically on the vertical axis.
- Everything *not* map-sourced is 240-authored and gets **clipped and centred**
  — with three separate channels that must each be handled, a lesson that cost
  three bugs: BG layers via the clip (`mapsource_bind_ui`), HUD sprites via
  `UI_HUD_SPRITE_DX` at each source site, and **PPU windows**, which the BG
  clip cannot reach and which must be shifted where they are set (B9).
  Expect the same three channels vertically.
- BG3 during a world view is a gameplay overlay and is deliberately **not**
  clipped (B10).

**Tooling that now exists** (`tools/capture/README.md` is the reference):
deterministic scripted replay and framebuffer capture; `--record=FILE` to
turn a human-played session into a replayable script; per-waypoint diffing;
and diagnostics `TMC_CAMTRACE`, `TMC_REJECT_TRACE`, `TMC_LAYER_TRACE`,
`TMC_BG3_TRACE`, `TMC_MAPSRC_DIAG`, `TMC_WINTRACE`.

**Two verification traps**, both of which produced false results during
Milestone 1 (bug-tracker lessons 1, 5 and 6):

1. A metric keyed on "is it black" is not a border check — count *distinct
   colours per column* instead.
2. When a fix makes a number jump to its theoretical best, confirm the code
   you think produced it actually ran. A "6400/6400 px" success was measured
   while the feature was silently disabled.

**Inherited work items** are listed in the bug tracker's carry-forward table;
the two that land in Spike 9 are the per-scanline window tables and the affine
paths (title sword, barrel, tornado).

### Spike 8 — OAM Y widening (2–3 days)
**Questions:** Can sa2's `EXTENDED_OAM` split-field approach be ported given its
"not yet functional" TODO? How many TMC sites write OAM attrs raw?
**Method:** Read `sa2/include/gba/types.h:95-300` and
`sa2/sa1/src/platform/pret_sdl/sdl2.c:1600-1660`. Inventory TMC's OAM writers
(`gOAMControls.oam`, `structures.h:298`, `vram.h:91`). Prototype s16 x/y in the
fork with `OAM_SET_GBA_ATTR*`-style shims.

**Definition of done:**
- [ ] Written assessment of whether sa2's `EXTENDED_OAM` is usable as-is, needs
      repair, or should be reimplemented.
- [ ] Complete inventory of TMC OAM attr write sites.
- [ ] s16 x/y implemented in the fork behind the shims.
- [ ] At 320×160, output is pixel-identical to Milestone 1 captures (diff = 0).
- [ ] A sprite positioned 64 px above the top edge renders its visible portion
      correctly in a 240-tall viewport — the specific case 8-bit Y cannot express.
- [ ] The Spike 2B unrepresentable-sprite count drops to zero.

### Spike 9 — HDMA at 240 lines (2 days)
**Questions:** Which effects use per-scanline tables, how long are they, and do
they resample or need extension?
**Method:** Inventory HDMA registrations. Test water FX, `BLDY` fades, and
affine scenes (Vaati's tornado, rolling barrel, screen-shrink cinematic).

**Definition of done:**
- [ ] Every HDMA registration site inventoried with its table length.
- [ ] Each table classified: extend, resample, or leave.
- [ ] Water FX, a `BLDY` fade, and all three named affine scenes verified at 240
      lines against 160-line captures.
- [ ] A per-scanline IO snapshot sized for 240 lines is implemented and its
      memory cost recorded. (The `io_snapshots[MODE1_GBA_HEIGHT][…]` sketch
      exists only in the *unapplied* widescreen patch — reference material per
      D4, not a symbol in any build.)

### Spike 10 — Camera and clamping, vertical (1–2 days)
**Method:** As Spike 5, for `0x50`/`0xa8` and the height clamp. Test across §6
height clusters: 160, 192, 208, 240, 320.

**Definition of done:**
- [ ] Every vertical camera constant replaced by a viewport-derived expression.
- [ ] One room per height cluster verified: shorter-than-viewport rooms centered
      with equal borders; taller rooms scroll and clamp cleanly.
- [ ] Vertical streaming verified against the 16 px slack finding from Spike 2B —
      no tile tearing at maximum scroll speed.

### Spike 11 — Vertical culling and integration (2–3 days)
**Method:** As Spike 7, for the 261 rooms ≥240 tall. Then a full canonical-route
playthrough at 320×240.

**Definition of done:**
- [ ] At least 10 rooms ≥240 tall walked end to end; issues logged as in Spike 7.
- [ ] Canonical route completes at 320×240 without crash or visual regression.
- [ ] Mean and p99 frame time recorded and compared against Spike 0.
- [ ] Go/no-go on shipping recorded.

---

**Total estimate:** 24–31 days of effort (unit: see header) to a shippable
320×240. The gate phase — **Spikes 0–2B — is 7–9.5 days of effort, ~5–7 days
of calendar with 2A/2B run parallel to Spike 2 as planned**, and determines
whether the recommended path survives and confirms the axis order. Milestone 1
alone is 12–16 days including its share of the gate phase. The increase over
this plan's earlier 20–27 figure buys the capture/determinism tooling in
Spike 0 and the inventory work moved into Spike 2A — not new implementation
scope.

The estimate is higher than a single-pass 320×240 attempt would appear to be on
paper. That is deliberate: the axis split buys attributable failures and a
working intermediate build, which matters more than raw day count on an effort
where the dominant risk is a late discovery that the premise was wrong.

---

## 11. Open questions

1. Does `gMapData*Special` cover the full room in *all* cases? **Partially
   answered statically (§5.1): no** — the `clearBottomMap` /
   `AREA_PALACE_OF_WINDS_BOSS` path builds only 512×512 px via `sub_0807C5F4`,
   `SetTileType`'s write is gated on `scroll_flags & 1`, and four scenes alias
   the arrays outright. Spike 2 confirms the complete exclusion set at
   runtime. (Decision gate — Spike 2.)
2. Where is the per-entity OAM coordinate write site? `gOAMControls.oam` is
   DMA'd to OAM at `interrupts.c:94`, but the code that converts entity world
   coordinates into OAM attrs was not located during this research — it may live
   in `asm/`. Blocks a firm answer on the sprite parking convention.
   (**Moved to Spike 2A** — it is static-trace work; Spikes 7 and 8 consume
   the answer.)
3. Are `gBG0/1/2/3Buffer` ever addressed by GBA address (`0x0203…`) rather than
   through the `gEwram` alias? Note `src/subtask.c:109`'s
   `MemCopy(gBG1Buffer, (void*)0x600e000, 0x800)` is *not* evidence of this —
   `gBG1Buffer` is the source and `0x600e000` a raw VRAM destination (already
   handled by `gba_TryMemPtr`). The right check is a grep for `0x0203` literals
   and raw arithmetic on `gEwram` offsets.
4. Is `EXTENDED_OAM` in sa2 actually working, or is the TODO accurate? Affects
   whether Spike 8 is a port or an original implementation.
5. What is `gUnk_080B2CD8` (`data/const/interrupts.s:42`) — confirmed as a
   BGCNT-screen-size → DMA-length table? It is the hook point if any option
   needs larger screenblocks.
6. Does the mode-2 path (`libs/ViruaPPU/src/mode2.c`, used for GBA modes 1 and 2
   per `port_ppu.cpp:376-386`) need the same treatment as mode 1? Affine BG2
   scenes route through it. **Resolved for Milestone 1** (§7, Option E): affine
   scenes stay 240×160 and letterbox. Revisit before ship.
7. Should the viewport be a build-time constant or runtime-configurable?
   **Promoted to D2 (§0)** — decided in Spike 0, because it gates Spike 3's
   architecture. A runtime toggle would ease A/B testing but forbids
   compile-time sizing of buffers.
8. Should the inert Phase 1 scaffold (§2.2) be wired up or deleted? Carrying a
   declared-but-dead build option is a trap for the next reader. **Promoted to
   D4 (§0)** — decided in Spike 0, with the constraint that the widescreen
   patch file survives as reference material either way (Spike 9 depends on
   it).

---

## 12. Risk register

| Risk | Impact | Likelihood | Mitigation |
|---|---|---|---|
| ~~Special-map premise fails beyond the known §5.1 exclusions~~ | ~~Option E dead; effort ~3×~~ | **Closed 2026-07-28** | Spike 2 ran: 7.6M tile comparisons, zero persistent mismatches; verdict recorded (§10 Spike 2) |
| ~~Per-entity OAM write site lives in unported `asm/`~~ | ~~Spike 7/8 estimates blow up~~ | **Closed 2026-07-28** | Spike 2A: it is port-owned C (`port_draw.c:292-406`); estimates revised *down* (`docs/spike2a-width-probe.md` §2) |
| Special maps repurposed during non-gameplay scenes (§5.1) | PPU samples garbage in file select, pause map, kinstone fusion, Gyorg fight | Certain (known) | Map-authoritative predicate: spec'd in Spike 2, implemented in Spike 3; excluded scenes stay on the 32×32 path |
| Pixel-diff DoDs run without deterministic capture | Verification silently degrades to eyeballing | High if Spike 0 tooling is skipped | Route manifest + warp/dump/diff tooling are named Spike 0 deliverables, not assumed substrate |
| Per-entity OAM write site lives in unported `asm/` | Spike 7/8 estimates blow up | Medium | Located in Spike 2A, before either spike starts |
| `screenTileMap.c` rewrite required after all | High — hardest file in the effort | Medium | Only reached under Option D |
| Per-scene tuning tail (as in sa2) | Schedule creep, long bug tail | High | Weight testing by playtime (§6), not room count |
| OAM Y widening destabilises sprite rendering | Broad visual regressions | Medium | Deferred to Milestone 2 so failures are attributable; sa2 reference available |
| ~~16 px vertical slack too tight for the 32-row buffer~~ | ~~Tile tearing on fast vertical scroll~~ | **Closed 2026-07-28** | Spike 2B measured: max continuous dy = 12 px over ~24k frames; teleports take the full-refill path (`docs/spike2b-height-probe.md` §2) |
| Perf regression from 50% more scanline work | Frame drops | Low–Medium | Spike 0 baseline; OpenMP path already exists |
| ~~Patch pipeline can't carry the change~~ | ~~Build breakage, merge pain~~ | **Closed 2026-07-27** | Pipeline retired; all PPU changes are fork commits (§7 Option F) |
| Cutscene/scripted framing breaks at 320×240 | Visible authored-content bugs | Medium | Spikes 7 and 11; `cutscene.c` already uses window regs for letterboxing |
| Milestone 1 ships and Milestone 2 never does | Stuck at a 2:1 aspect ratio | Low–Medium | Milestone 1 is explicitly an internal checkpoint (§8.3); Option B remains the shippable fallback |

---

## 13. Reference material

- `awe444/sa2` — <https://github.com/awe444/sa2>, our fork of `SAT-R/sa2`
  (default branch `main`; not affiliated with upstream). Key material:
  `include/gba/defines.h` (config, `WIDESCREEN_HACK`, `EXTENDED_OAM`,
  `winreg_t` — as of 2026-07-27 at lines ~36-78), `include/gba/types.h`
  (extended OAM `split` struct and `OAM_SET_GBA_ATTR*` shims at ~157-250; the
  `// TODO: EXTENDED_OAM is not yet functional` comment at ~116 is confirmed
  present), `sa1/src/platform/pret_sdl/sdl2.c` (renderer), and the
  `#if WIDESCREEN_HACK` sites across `src/game/*/stage/backgrounds/`.
  **The fork is actively developed (3300+ commits), so line citations drift —
  Spikes 4/8 should re-cite against the pinned commit.** Pinned by Spike 0:
  **`34b01960bb73734ec077b007f5d57ee46fa4b7a0`** (`main`, 2026-07-27).
  sa2 is a read-only design reference throughout: nothing in this plan
  links, vendors, or submodules it.
- `awe444/VirtuaPPU` — the PPU submodule as of §2.3, pinned at `e69f60b`.
- This repo: `port/patches/viruappu-widescreen.patch` (Phase 1 rationale — note
  this file is never applied, see §2.2), `xmake.lua:29-43` (the inert option),
  `xmake.lua:379-432` (the marker-guarded patch pipeline).
- This repo, verification tooling: `port/port_debug_menu.cpp:192-210` — the F8
  debug menu's parameterised warp table (`Port_DebugAction_Warp`), the seed of
  the Spike 0 route manifest.
