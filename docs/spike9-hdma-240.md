# Spike 9 — HDMA and per-scanline tables at 240 lines

**Date:** 2026-07-30 · **Status:** height work complete; two follow-ups
recorded in §6. Milestone 2, research plan §10.2.

The spike found more than the plan expected. The height problem was real and
mechanical. The larger finding is that **the per-scanline window path had been
dead in the port since Spike 4**, and every circular-window transition was
rendering as a near-black screen — at 240×160, the shipping build.

## 1. Registration inventory

Nine call sites, all reaching the port through
`SetVBlankDMA` → `PerformVBlankDMA` → `DmaSet` → `port_DmaTransfer`, which
routes `DMA_START_HBLANK` to `port_hdma_register`. `TMC_HDMA_TRACE=1` prints
each distinct registration at runtime with its destination register and its
per-line footprint.

| Site | Dest | Fill | Halfwords/line | Bytes @160 | Bytes @240 | Class |
|---|---|---|---|---|---|---|
| `common.c:961` `sub_0801E160` | WIN0H | `sub_0801E290` | 1 | 320 | 480 | extend |
| `common.c:978` | WIN0H | `sub_0801E290` | 1 | 320 | 480 | extend |
| `common.c:1126` | WIN0H | `sub_0801E290` | 1 | 320 | 480 | extend |
| `common.c:1182` | WIN0H | `sub_0801E290` | 1 | 320 | 480 | extend |
| `lightRayManager.c:214` | BG3HOFS | `for i<0xA0` | 1 | 320 | 480 | extend |
| `steamOverlayManager.c:149` | BG3HOFS | `for i<0xA0` | 1 | 320 | 480 | extend |
| `vaatiAppearingManager.c:165` | BG3HOFS | `for i<0xa0` | 1 | 320 | 480 | extend |
| `pauseMenuScreen6.c:162` | BG3CNT | `for i<0xa0` | 1 | 320 | 480 | extend |
| `rollingBarrelManager.c:213` | BG2PA | `for tmp3<0xA0` | **8** | **2560** | **3840** | extend + grow buffer |

Every fill loop writes exactly one entry per scanline and is bounded by
`0xA0` = 160. **No table needed resampling** — each is a function evaluated
per line, so extending the loop bound produces the right values for the extra
lines. That answers the DoD's extend/resample/leave question: all extend.

Two notes on the table shapes, both worth having written down:

- `vaatiAppearingManager.c` types the buffer `struct BgAffineDstData*` but
  advances with `affineDstData = (struct BgAffineDstData*)&affineDstData->pb`,
  which steps **2 bytes**, not 16. Its footprint is one halfword per line
  despite the type.
- The rolling barrel is the only 8-halfword consumer, and at 160 lines it
  fills `160 × 16 = 0xA00` bytes — *exactly* one half of the double buffer.
  At 240 lines it needs 0xF00 and would have run 1280 bytes past its half;
  on buffer index 1 that is past the end of the array entirely.

## 2. Height conversion

`include/viewport.h` gained the constants that make a half's size follow the
viewport, sized by the widest consumer:

```c
#define VIEWPORT_HDMA_UNITS_PER_LINE 8   /* halfwords; the affine-matrix case */
#define VIEWPORT_HDMA_HALF_BYTES  (VIEWPORT_HEIGHT * VIEWPORT_HDMA_UNITS_PER_LINE * 2)
```

At 240x160 that is 0xA00, and the pair is the 0x1400 `gUnk_02017AA0` has
always been — so the shipping build's layout is unchanged. Converted with it: every `0xA0` fill bound to `VIEWPORT_HEIGHT`,
every `* 0x500` / `* 0xA0` half-stride to `VIEWPORT_HDMA_HALF_U16` /
`_HALF_AFFINE`, the `struct_02017AA0.filler` size, two `MemClear(…, 0xa00)`
in `common.c`, and both buffer definitions in `port_linked_stubs.c`.

Verified at 320×240: the trace reports `lines-for-240=480` and the window
channel drives **240** lines per frame where it drove 160.

## 3. The dead per-scanline window path

Spike 4 widened the window registers by adding a 32-bit `winreg_t` and
handing full-width bounds to the PPU through `Port_Screen_CommitWindows`,
because the packed 8-bit registers cannot express an edge past 255. The PPU
prefers those bounds:

```c
/* mode1.c */
int win0_left  = mode1_window_bounds_active[0] ? mode1_window_bounds[0].left  : (win0h >> 8);
int win0_right = mode1_window_bounds_active[0] ? mode1_window_bounds[0].right : (win0h & 0xFF);
```

`Port_Screen_CommitWindows` is called every frame from `UpdateScreenRegs`, so
`mode1_window_bounds_active[0]` is true from the first frame onward and never
cleared. **The HBlank DMA's per-line writes to WIN0H therefore went into a
register nothing read.** The comment at `interrupts.c:168` — "the per-scanline
window DMA still consumes that" — describes the write, not its fate.

Measured on the canonical route at 240×160:

- 4 879 frames register and drive the WIN0H channel, 160 lines each.
- 111 of them have WIN0 enabled in DISPCNT.
- **64** have a right edge that varies between lines, i.e. a genuinely
  circular window rather than a whole-frame one.
- On those frames `winin=3F3F winout=0000` — every layer on inside the
  window, every layer off outside — so the window is not cosmetic.

### The fix

`virtuappu_mode1_set_window_h_bounds(index, left, right)` replaces only a
window's horizontal pair. `port_hdma_step_line` calls it for any channel whose
destination is WIN0H, once per line, after that line's transfer.

**Evidence it works.** A/B over 300 sampled route frames with
`TMC_HDMA_NOWIN=1` (which restores the old behaviour) differs on 19 of them,
in two runs — capture frames 5880–5912 and 5952–5988 — with the differing
pixel count sweeping 38 151 → 518 and back, the profile of an iris animating.
The frame itself is unambiguous: with the fix, a clean circle of world content
on black; without it, a near-black screen.

### Why the regression gate never caught it

None of the 11 canonical waypoints lands on one of those 64 frames, and the
gate still reports 11/11 with the fix in place. This is worth remembering as a
limit of the gate rather than a fault in it: the route is a fixed set of
still frames, and a defect confined to a ~1 second transition falls between
them. The `TMC_HDMA_TRACE` per-frame report exists so the question "did any
frame exercise this at all" is answerable without guessing.

**Note on frame numbers.** `port_hdma_vblank_reset` and the capture's
`sFrame` are different clocks — the trace's frame 5424 is not the capture's
frame 5424. Comparing them directly produces a confident wrong answer; the
sweep-and-diff above avoids the question.

## 4. Widening the per-scanline window

The table `sub_0801E290` fills is **byte pairs**, because WIN0H is byte
pairs, so an edge past 255 cannot be stored in it at any viewport size. The
32-bit `winreg_t` widening does not reach here.

`gWin0hExtLeft/Right[VIEWPORT_HEIGHT]` (`port_gba_mem.{c,h}`) carry the
untruncated edges alongside the hardware table, validated against the byte the
DMA wrote — the same self-checking pattern as the OAM y channel in Spike 8,
for the same reason. `sub_0801E290` fills both; the hardware table keeps the
authored 240 ceiling, the channel carries the real one. Identical at native
width.

## 5. Verification

- 240x160 route gate: **11/11, 0 differences**.
- 240x160 map-source audit: **0 mismatched in 265 497 600 fetches**.
- 320×240: tables drive 240 lines; the iris renders as a full-height circle.

## 6. Follow-ups, not done here

- **The iris is still a 240-wide circle on a 320-wide screen.** The channel
  that can carry a wider edge is in place, but the centre and radius
  `sub_0801E290` is handed are 240-authored screen coordinates and are not
  shifted or scaled. Measured at 320×240: `right=0..240`, never past 240.
  This is the same class as B9 — a 240-authored coordinate on a wider
  surface — and wants the same treatment at the call sites that compute the
  centre.
- **Affine reference points are untouched.** `rollingBarrelManager.c:216`
  sets `scrX=0x78, scrY=0x80` per line, and `vaatiAppearingManager` the same
  shape. These are the affine carry-forward items (with the title sword);
  the barrel and tornado scenes have not been reached at 240 lines, so the
  DoD item "all three affine scenes verified at 240 lines" is **not met** —
  reaching them needs a save at those points or a recording.
- **A per-scanline IO snapshot** sized for 240 lines was not needed: the port
  applies HDMA writes directly to `gIoMem` from the pre-line callback rather
  than snapshotting IO per line, so there is no `io_snapshots[160][…]` to
  grow. The sketch the plan refers to exists only in the unapplied widescreen
  patch.
