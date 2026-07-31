# Spike 8 — OAM Y widening

**Date:** 2026-07-30 · **Status:** complete · Milestone 2, research plan
§10.2. Companion to `spike2b-height-probe.md` §3, which measured the
severity this spike removes.

## 1. The problem, restated precisely

A GBA OBJ stores y in 8 bits. Hardware draws a sprite's rows at
`(y + row) mod 256`, so a sprite above the top edge is expressed by wrapping;
an emulator recovers the sign by reading the band `[screen_height, 255]` as
negative. That recovery is unambiguous only while the band is wider than the
tallest sprite:

| Viewport height | Off-screen band | Most negative y expressible |
|---|---|---|
| 160 | 160..255 | −96 |
| 240 | 240..255 | **−16** |

A double-size affine sprite is 128 px tall, so at 240 lines a sprite hanging
more than 16 px above the top edge has no encoding. It does not degrade — it
resolves to a large *positive* y and reappears near the bottom of the screen,
a 256 px error.

## 2. sa2's `EXTENDED_OAM`: reimplemented natively, not ported

The plan asked for an assessment of whether sa2's split-field approach is
usable as-is, needs repair, or should be reimplemented. **Reimplemented** —
and smaller than the port would have been. Spike 2A had already established
why: the port owns *both* ends of this path, so the shim machinery sa2 needs
to intercept scattered OAM writers has nothing to do here.

## 3. OAM write-site inventory

Every site that touches an OAM attribute, and whether it can produce an
*enabled* entry:

| Site | Writes | Enabled? |
|---|---|---|
| `port/port_draw.c:385` `RenderSpritePieces` | attr0 (y, flags, shape), attr1 (x, flip, size), attr2 | **Yes — the only one** |
| `src/affine.c:76` `CopyOAM` | `0x2A0` into slots `[updated, 0x80)` | No — bit 0x200 is OBJ-disable |
| `src/common.c:623` `ClearOAM` | `0x2A0` into all 128, shadow and hardware | No |
| `src/object/minishPortalCloseup.c:90` `sub_0808D030` | `0x2A0` into all 128 | No |
| `src/vram.c:451` | attr2 `tileNum` only | Does not touch y |
| `port/port_softslots.c:673` | attr0/attr1/attr2 | **Inside `#if 0`** — not compiled |
| `port/port_bios.c:428` | `memset(gOamMem, 0, …)` on `RESET_OAM` | y=0, same as hardware |

**One producer of enabled entries.** That is what makes a side channel
sufficient rather than a best-effort guess, and it is confirmed at runtime in
§5 (`unresolved=0` over ~200k entries).

## 4. What was built

attr0 keeps its hardware-faithful 8-bit encoding; the untruncated y travels
beside OAM.

- `port/port_gba_mem.{c,h}` — `gOamYExtShadow[128]` (engine side) and
  `gOamYExt[128]` (published), plus `Port_OamYExt_Latch()`.
- `port/port_draw.c` — `RenderSpritePieces` records `(s16)y` at the slot it
  is writing.
- `src/interrupts.c` — the latch runs **inside** the same conditional
  `DmaCopy32` that publishes `gOAMControls.oam`. A frame that skips the copy
  keeps last frame's OAM, and the y channel has to go stale with it.
- `libs/ViruaPPU` — `VirtuaPPUMode1GbaMemory.oam_y_ext` (NULL = pure
  hardware interpretation) and `mode1_obj_y()`, used by both the raster and
  the affine overlay path.

Validity is self-checking: the channel is used only where `ext & 0xFF` equals
attr0's y byte. Any writer that sets a sprite's y without updating the channel
disagrees there and gets the hardware reading — which is the correct fallback
for a slot recycled behind the port's back.

## 5. Measurements

`--mapcheck` gained a Spike 8 counter. `rescued` counts enabled entries where
the channel and the wrap heuristic disagree; `unresolved` counts enabled
entries the channel has no valid value for.

| Build | enabled entries | rescued | unresolved |
|---|---|---|---|
| 240×160 (shipping) | 174 263 | **0** | **0** |
| 320×160 (Milestone 1) | 179 741 | **0** | **0** |
| 320×240 (Milestone 2) | 202 769 | **107** | **0** |

Read together these say: the channel is populated and consulted on every
enabled entry (`enabled` is large, `unresolved` is zero); it changes nothing
at either 160-line build, so neither the shipping build nor Milestone 1's
output moves; and at 240 lines it corrects 107 sprite placements.

`TMC_OAMY_TRACE=1` shows the failure directly —
`frame=5120 slot=2 packed=239 legacy=239 ext=-17`: a sprite 17 px above the
top edge, which the wrap heuristic puts at y=+239.

Note the Spike 2B census (`y ∈ [161,239]`, 103 frames / 118 entries) is
**not** the number that drops to zero, because attr0's encoding is
deliberately unchanged. At 240 lines that census rises to 957 frames / 3075
entries, most of which are sprites legitimately at those y values on a taller
screen. `unresolved` is the meaningful form of the DoD's "drops to zero".

## 6. Direct verification at 240 lines

`TMC_OAMY_PROBE=<y>` injects a 64×64 sprite at a chosen y through the real
OAM and the real raster; `TMC_OAMY_LEGACY=1` unbinds the channel. A 64-tall
sprite at y occupies screen rows `y .. y+63`, so the visible rows are
predictable:

| probe y | rows expected | rows observed (channel) | legacy reads | rows observed (legacy) |
|---|---|---|---|---|
| −64 | none | **none** | +192 | 192..239 |
| −63 | 0..0 | **0..0** | +193 | — |
| −48 | 0..15 | **0..15** | +208 | 208..239 |
| −16 | 0..47 | **0..47** | −16 | 0..47 |
| 0 | 0..63 | **0..63** | 0 | 0..63 |

All at columns 128..191, the pinned x. Three things worth reading off it: the
channel's geometry is exact at every y; a sprite 64 px above the edge renders
**nothing** rather than wrapping back on screen; and at y=−16 — the deepest
8 bits can still express at 240 lines — the two paths agree, so the channel
does not perturb what the encoding already handled.

The same A/B on real content (route frame ~5124, a sprite at y=−19) differs in
rows 0..12, columns 224..255: with the channel the sprite hangs into the top
edge, without it the top edge is empty.

## 7. DoD

- [x] Assessment of sa2's `EXTENDED_OAM` — reimplement natively (§2).
- [x] Complete inventory of OAM attr write sites (§3).
- [x] s16 y implemented behind the binding. **x needed no work**: it is
      already 9-bit and at width 320 resolves x ∈ [320,511] → −192…−1
      (Spike 2A §1).
- [x] 320×160 output unchanged from Milestone 1 — `rescued=0` over 179 741
      entries means identical by construction (§5).
- [x] A sprite 64 px above the top edge renders correctly at 240 tall (§6).
- [x] The unrepresentable-sprite count drops to zero — as `unresolved`, with
      the reinterpretation argued in §5.
- [x] Both 240x160 regression gates pass: 11/11 waypoints, 0 mismatches in
      265 497 600 fetches.
