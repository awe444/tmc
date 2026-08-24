# Comparing the port against real hardware with mGBA

mGBA runs headless here — `SDL_VIDEODRIVER=dummy` gives it no window — and its
CLI debugger (`-d`) reads commands on stdin, so the reference implementation
can be driven and interrogated by script. That makes it an **oracle**: any
question of the form *"is this the port, or is this the game?"* can be settled
by measurement instead of argument.

It closed B45 after six passes of inference had reached a confident wrong
answer, and B47 fell out of getting a save into it.

```bash
mgba --version        # 0.10.2 here; SDL frontend only, no Qt, no Python bindings
```

Three techniques, in the order you will usually want them:

1. **A savestate** — the strongest, because it carries a frame's state *and*
   its picture in one file. Needs a human at the controls once.
2. **Replaying one of our capture scripts** — no human needed, but you only get
   state, never pixels.
3. **The debugger on its own** — memory reads and watchpoints at any address.

---

## 1. Savestates: state and picture together

**This is the technique that matters.** An mGBA savestate is a PNG whose
*image* is the frame it was taken on and whose `gbAs` chunk is the deflated
`GBASerializedState`. So it answers "why is this pixel this colour" outright —
you can read the layers *and* see what they produced, on the same frame.

Every earlier B45 pass failed for want of exactly this. Six of them compared
OAM, BGCNT, maps, tilesets and object data against hardware, found them
identical every time, and concluded the port was faithful — while the screen
plainly differed. The mismatch was a compositing *rule*, and no state
comparison can see one.

Ask the maintainer for one (Shift+F1..F9 in mGBA saves to `<rom>.ssN`), then:

```bash
tools/mgba/ssextract.py ~/mgba-tmc/baserom.ss1 /tmp/state.bin /tmp/shot.raw
tools/mgba/readstate.py /tmp/state.bin            # display registers
tools/mgba/readstate.py /tmp/state.bin 118 70     # ...and one pixel's layers
```

`shot.raw` is 240x160 RGB888 with no header; the `.ss1` itself renders as a
PNG in any viewer, which is usually quicker to eyeball.

`readstate.py <x> <y>` prints, for that pixel, every BG layer's tile, palette
index and priority, and every OBJ covering it in OAM order. That output is what
pinned B45's rule:

```
pixel (118,70):
  BG2 prio=2 tile=526 idx=6 opaque
  OBJs covering it, in OAM order (the LAST one sets the layer's priority):
    OAM[  7] prio=3 tile= 352 pal= 6 16x16 idx=9 OPAQUE
    OAM[ 14] prio=2 tile= 133 pal= 0 8x8  idx=0 transparent
```

**Block layout of the inflated state** (397,312 bytes for GBA):

| block | offset | size |
|---|---|---|
| IO registers | `0x400` | `0x400` |
| palette RAM | `0x800` | `0x400` |
| OAM | `0xC00` | `0x400` |
| VRAM | `0x1000` | `0x18000` |
| IWRAM | `0x19000` | `0x8000` |
| WRAM | `0x21000` | `0x40000` |

Do not guess this from the struct declaration — it is not the order the fields
are declared in, and getting it wrong reads plausible-looking rubbish. Anchor
on something you can recognise (a known OAM entry, or palette RAM's 15-bit
colours with bit 15 clear) and check before trusting it.

### Ask for two scenes, not one

**B45's rule came from a pair of frames with the same shape and opposite
answers**, and neither would have given it alone:

| | transparent sprite | opaque sprite | last covering | hardware draws |
|---|---|---|---|---|
| swamp `(118,70)` | OAM[14] prio 2 | OAM[7] prio 3 | 14 → prio 2 | ties the ground, OBJ wins → the player |
| name entry `(27,52)` | OAM[27] prio 1 | OAM[33] prio 2 | 33 → prio 2 | loses to BG1 → white, the letter's apex |

The first says a blank sprite lends its priority; the second says it does not.
Together they say the layer takes the priority of the *last covering sprite in
OAM order*. When a savestate confirms a rule, look for the scene that would
refute it before believing it — and note that a scene which does not exercise
the difference is itself a result worth recording, not a wasted round trip.

---

## 2. Replaying our capture scripts

The game reads `REG_KEYINPUT` exactly once per frame, at `0x0801D6C4`:

```
0801D6C2:  ldr  r0, =0x04000130
0801D6C4:  ldrh r0, [r0]          <- the read
0801D6C6:  ldr  r1, =0x000003FF
0801D6C8:  bic  r1, r0            <- r1 becomes the pressed mask
```

So `watch/r 0x04000130` is a **per-frame breakpoint** that fires with the raw
value already in `r0`, and `w/r r0` overwrites it before the game uses it. Our
capture scripts already hold GBA KEYINPUT bit masks, so replaying one is
`0x3FF ^ mask` per frame:

```bash
tools/mgba/replay.py <script> <lastframe> --offset N --dump f1,f2 > cmds.txt
SDL_VIDEODRIVER=dummy mgba -d -l 0 baserom.gba < cmds.txt
```

`--offset` exists because the two frame origins differ: the port skips the BIOS
and mGBA does not. Sweep it and watch for the scene you want — B45's mud needed
`--offset 120`, found by watching `BG2CNT` for Castor Wilds' `0x1C42`. Roughly
55 frames/second, so a 1500-frame replay is half a minute.

**Aligning the two runs.** Frame numbers do not correspond; align on an
**event** and count from it. B45 used "the frame OBJECT_70's twelve mask
entries first appear in OAM": mGBA 1034, port 913, a constant 121 apart.

**Put the port at the same viewport first.** Anything scroll-dependent — maps,
screenblocks, which tile is under the player — is meaningless compared across
sizes. At 240x160 the port's and hardware's OAM positions match exactly and the
screenblocks diff byte-for-byte; at 320x240 nothing lines up. Four B45 passes
were spent on comparisons that could not have been valid.

**Beware animation phase.** Two runs can sit on the same *scene* frame with the
player one frame apart in his walk cycle, which changes his sprite's whole
piece decomposition and offsets while everything anchored to his entity
position stays identical. Compare the first frame of an event, where the phase
is forced, before believing a difference.

---

## 3. The debugger directly

Everything worth comparing lives at a fixed hardware address, so no game
symbols are needed:

| what | command | why |
|---|---|---|
| `DISPCNT`, `BG0-3CNT` | `x/2 0x04000000 8` | which layers are on, and their priorities |
| OAM | `x/2 0x07000000 512` | every sprite: position, size, **priority**, tile, palette |
| OBJ VRAM tile *n* | `x/2 0x06010000+n*32 16` | whether a sprite's tiles are blank |
| BG screenblock *n* | `x/2 0x06000000+n*0x800 1024` | the map the layer is drawing |
| palette RAM | `x/2 0x05000000 512` | |

`break`, `watch`, `watch/r`, `watch/w`, `w/1 w/2 w/4`, `w/r` and `continue` all
work. There is no frame-advance command — use the KEYINPUT read watchpoint
above as one.

**The scroll registers are write-only.** Reading `0x04000018` gives open bus
(`0x4381` here, the prefetched instruction). Catch the value with
`watch/w 0x04000018`, or take it from the game's own state.

---

## Saves must be converted first

**A `tmc.sav` from this port will not load in mGBA, or on hardware, until each
8-byte EEPROM block is byte-reversed** — that is B47, and it is why the first
attempt at all of this booted to an empty file select. The port stores each
block in the game's in-memory order; hardware stores the order the bits go over
the serial line, which is the reverse:

```
port :  "AGBZELDA"  ":THE MIN"  "ISH CAP:"
mGBA :  "ADLEZBGA"  "NIM EHT:"  ":PAC HSI"
```

**Byte order was only half of it (B51).** The port's `SaveFile` struct also
puts `flags[0x200]` and the three `dungeon*` arrays **one byte earlier** than
the real game reads them: `KinstoneSave`'s members sum to 327 bytes where the
GBA layout `include/save.h` documents (`kinstones` 0x114 → `flags` 0x25C)
leaves 328. Everything before `flags` is at the right offset, so a
byte-order-only conversion loads with correct hearts, elements and name and
*every story flag shifted a bit* — Link without Ezlo, world events un-done.
That cost most of a session to find precisely because it looks like a working
save. `savconv.py` now realigns the region and recomputes each slot's
checksum with the game's own algorithm.

`savconv.py` converts either way, taking the direction from block 0:

```bash
tools/mgba/savconv.py build/play-320x240/foo.script.sav ~/mgba-tmc/baserom.sav
```

Two things to watch:

- **Converting *into* the port's layout loses one byte** — the GBA's
  `SaveFile+603`, which the port's short `KinstoneSave` has no field for. It
  is reported on stderr and returns as 0. That is the port bug showing
  through; when the struct is fixed, flip `LAYOUT_FIXED_IN_PORT` in the tool
  and delete `relayout()`, or it will corrupt every save it touches.
- **mGBA writes the save back on exit**, in hardware order — so re-convert
  before each run if you want a clean start from a recording's save.
- mGBA keeps the save as `<rom-basename>.sav` next to the ROM. Work in a
  scratch directory with a copy of `baserom.gba`, never in `build/play-*`.

Confirmation that the reversal is the right direction, not a guess: delete the
save, let the real game initialise a fresh one under mGBA, and read it — that
file is in the reversed layout.

---

## Files here

| | |
|---|---|
| `ssextract.py` | savestate PNG → state blob + 240x160 RGB screenshot |
| `readstate.py` | display registers from a state blob; with `x y`, that pixel's BG layers and covering OBJs |
| `replay.py` | one of our capture scripts → mGBA debugger command stream |
| `savconv.py` | `tmc.sav` ↔ hardware/mGBA byte order (B47) |
