# Spike 2A — Width-only feasibility probe + static inventories

**Date:** 2026-07-28 · **Status:** complete · Companion to
`viewport-expansion-research-plan.md` §10 (Spike 2A). Static trace only —
no implementation. Verdict at the end; the §8.1 width column survives with
one correction and one addition, and both "unknowns moved here from later
spikes" resolved *favourably*.

## 1. §8.1 width column, cell by cell (320×160 target)

| Constraint | §8.1 claim | Verdict | Evidence |
|---|---|---|---|
| BG staging buffer | Needs 42 tiles, buffer is 32 — blocked unless Option E | **Confirmed; moot under E** | Buffers `0x400`×u16 32×32 (`vram.h:59-62`, dup `structures.h:207-210`). Option E samples `gMapData*Special` directly — premise now *measured*, not assumed (Spike 2: 7.6M comparisons, 0 persistent). Correction to §8.1's margin note: the streamed window is **sliding, not wrapping** (`hofs=cx&15`), so horizontal slack is 16 px, same shape as vertical. Irrelevant under E. |
| Tile streamer | Stride rewrite — blocked unless E | **Confirmed; moot under E** | Streamers: `screenTileMap.c` (3 fns) + port `ram_sub_080B197C_c` / `UpdateScrollVram` (`port_linked_stubs.c:365,883`). Under E the world layers stop using them; UI keeps the path untouched. |
| OAM X | 9-bit, ample at 320 | **Confirmed** | `mode1_oam_x` = `attr1 & 0x1FF` (`mode1.c:104-107`); wrap at `obj_x >= MODE1_GBA_WIDTH → -512` (`:393-395`). At width 320: x∈[320,511] → −192…−1. |
| Window registers | 320 > 8-bit edges — blocked | **Confirmed, edit-site list now complete** | Seven engine files (plan §3) **plus** PPU-side clamps of *right and bottom* edges (`mode1.c:560-571`) — both dimensions clamp against `MODE1_GBA_WIDTH/HEIGHT`, so Spike 4 touches the same lines Milestone 2 needs. |
| HDMA | Untouched by width | **Confirmed** | `port_hdma.c`: zero width references (grep). Per-line callback count is height-driven only. |
| BG scroll regs | 9-bit, fine | **Confirmed and downgraded** | Under E the world layers aren't addressed via HOFS/VOFS at all — the PPU mode takes the camera origin directly (Spike 2 measured the current convention: `hofs=cx&15`, `vofs=(cy&15)+8`). Only UI layers keep scroll regs, which never exceed GBA ranges. |
| Camera constants (H) | `0x78`/`0xf8`/`0xf0`/`120` | **Confirmed + one NEW site** | `scroll.c:103` (0x78), `:122` (0xf0), `:515-560` (0xf8 region tests), `:792-835` (literal 120), `script.c:1976-1977` (`DISPLAY_WIDTH`). **New, missed by §3:** `playerUtils.c:4401-4403` — room-entry camera init `scroll_x = width - 0x78 - 0x78` / `targetX - 0x78`. Added to the Spike 5 work list. |
| Rooms ≥320 wide | 174/617 (28%) | **Confirmed** | Recomputed from `room_headers.s` in Spike 0 review; matches. |

## 2. Per-entity OAM write site — FOUND, and it is port-owned C

Open question 2 resolves *better than either hypothesis*. The entire
entity→OAM path is:

```
DrawEntities (src/affine.c:99)          sets gOAMControls._4/_6 = scroll+aff
  → ram_DrawEntities                    PORT C, port_draw.c
    → entity screen pos                 port_draw.c:514,519:
                                          x = entity->x.HALF.HI - gRoomControls.scroll_x
    → RenderSpritePieces                port_draw.c:292-406  ← THE WRITE SITE
```

Inside `RenderSpritePieces`:

- **Culling literals:** `if (y >= 160) continue;` / `if (x >= 240) continue;`
  (`port_draw.c:359,367`, plus left/top via anchor+size). Widening the
  sprite cull for a bigger viewport is a two-literal change in port code.
- **The truncation Spike 8 must widen:** `oamWord = (y & 0xFF) | ((x & 0x1FF) << 16)`
  (`port_draw.c:381-382`). The port owns both the producer (this function)
  and the consumer (VirtuaPPU fork) — an s16 side-channel does not need
  sa2's `OAM_SET_GBA_ATTR*` shim machinery at all; sa2 remains reference,
  not template. Spike 8's "usable as-is / repair / reimplement" question is
  answered in advance: **reimplement natively, smaller than the sa2 port**.
- **Parking convention documented** (Spike 7 DoD item): `CopyOAM`
  (`affine.c:65-80`) fills every unused OAM entry with attr0 `0x2A0` =
  **OBJ-disable bit (0x200) + y=0xA0(160)**. Parked sprites are *disabled*,
  not merely off-screen — they cannot leak into widened columns. Off-screen
  pieces are `continue`d (never written), not parked at x≥240. The §2.2
  patch header's "parked sprites live at x≥240" claim is **wrong**; the
  Phase 1 scaffold clipped OAM at 240 to defend against a convention that
  does not exist.

**Consequence:** Spike 7's estimate is *not* a floor (no `asm/` at all);
its scope shrinks to entity-side pop-in (update culling), since OAM-side
leakage is structurally impossible. Spike 8 shrinks from "port sa2's
EXTENDED_OAM" to "widen one pack site + one unpack site we own".

## 3. Camera-position consumer inventory (Spike 5 blast radius)

`gRoomControls.scroll_x/scroll_y` outside `scroll.c`/`script.c`:
**165 references in 50 files** (grep recorded in plan §3 style). Categories:

- **Screen-relative positioning of effects/spawns** (majority): entities
  computing `worldX - scroll_x` for draw or spawn placement. Unaffected by
  viewport size *unless* they also bake a viewport literal —
- **16 sites bake viewport literals** (`0x78`, `0xf0`, `DISPLAY_WIDTH`, …)
  and are the true Spike 5/7 blast radius. Notable: `rainfallManager.c:39`
  and `cutsceneMiscObject.c:339` (spawn across "screen width"),
  `bird.c:336`, `guardWithSpear.c:294` (off-screen tests), `main.c:295`.
- **BG3/parallax managers** (~12 files: staticBackground, pow, vaati3,
  cloudOverlay, animatedBackground, …): derive BG3 offsets from scroll —
  scale with camera automatically; only their *coverage* (BG3 is a 32×32
  UI-path layer) matters, which is Spike 6's composition question.
- **Harness/port consumers** (`port_mapcheck.c`, `port_draw.c`,
  `port_linked_stubs.c`): ours, already viewport-aware or trivially made so.

## 4. Width-only effort re-estimate (under Option E)

| Work | Was | Now | Basis |
|---|---|---|---|
| Spike 3 map-sampling mode | 3–4 d | **3–4 d (unchanged)** | Plumbing measured by Spike 2 (BG2/BG1 blocks, predicate reference impl exists) — de-risked, not shrunk: the mode itself is still the work. |
| Spike 4 window regs | 1 d | **1 d** | Edit-site list now closed (7 files + `mode1.c:560-571`). |
| Spike 5 camera H | 2 d | **2 d** | +1 new site (`playerUtils.c:4401`), −risk (consumer inventory done, 16-site blast radius known). |
| Spike 7 culling | 2–3 d *(floor?)* | **1.5–2 d, not a floor** | OAM side is two port literals + no parking hazard; remaining scope is entity update-culling pop-in on the 174 wide rooms. |
| **Milestone 1 total** | 11–15 d | **10–13 d** | Spike 6 (2 d) unchanged, pending D1. |

## 5. DoD checklist

- [x] Every §8.1 width cell confirmed/corrected with citations (§1).
- [x] One missed constraint added (`playerUtils.c:4401-4403`); one §8.1
      margin note corrected (sliding window, 16 px H slack).
- [x] OAM write site located and documented — port C, not `asm/`;
      Spike 7/8 estimates *revised down*, not up (§2).
- [x] `scroll_x/scroll_y` consumer inventory: 165 refs / 50 files,
      16-site literal-baking blast radius (§3).
- [x] Width-only effort re-estimated with basis (§4).

Axis-order confirmation (width-first vs height-first) is **Spike 2B's**
final DoD item, not this probe's; nothing found here argues against
width-first — every width-side unknown resolved at or below its estimate.
