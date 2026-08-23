# Comparing the port against real hardware with mGBA

mGBA runs headless here — `SDL_VIDEODRIVER=dummy` gives it no window — and its
CLI debugger (`-d`) reads commands from stdin, so the reference implementation
can be driven and interrogated from a script. That makes it an **oracle**: any
question of the form *"is this the port or is this the game?"* can be answered
by measurement instead of argument.

This is what settled B45 after three passes of inference had not.

```bash
mgba --version        # 0.10.2 here; SDL frontend only, no Qt, no Python bindings
```

## Driving input

The game reads `REG_KEYINPUT` exactly once per frame, at `0x0801D6C4`:

```
0801D6C2:  ldr  r0, =0x04000130
0801D6C4:  ldrh r0, [r0]          <- the read
0801D6C6:  ldr  r1, =0x000003FF
0801D6C8:  bic  r1, r0            <- r1 becomes the pressed mask
```

So `watch/r 0x04000130` is a **per-frame breakpoint** that fires with the raw
value already in `r0`, and `w/r r0 <value>` overwrites it before the game uses
it. Our capture scripts already store GBA KEYINPUT bit masks, so replaying one
is `0x3FF ^ mask` per frame:

```bash
tools/mgba/replay.py <script> <lastframe> --offset N --dump f1,f2 > cmds.txt
SDL_VIDEODRIVER=dummy mgba -d -l 0 baserom.gba < cmds.txt
```

`--offset` exists because the two frame origins differ: the port skips the BIOS
and mGBA does not. Sweep it and look for the scene you want — B45's mud needed
`--offset 120`, found by watching `BG2CNT` for Castor Wilds' `0x1C42`. Roughly
55 frames/second, so a 1500-frame replay is half a minute.

## The save has to be converted first

**A `tmc.sav` from this port will not load in mGBA, or on hardware, until each
8-byte EEPROM block is byte-reversed** — see B47. `tools/mgba/savconv.py` does
it, and is its own inverse:

```bash
tools/mgba/savconv.py build/play-320x240/foo.script.sav /path/to/baserom.sav
```

mGBA writes the save back on exit, so re-convert before each run.

## What to read once you are there

Everything worth comparing is at a fixed hardware address, so no game symbols
are needed:

| what | address | why |
|---|---|---|
| `DISPCNT`, `BG0-3CNT` | `x/2 0x04000000 8` | which layers are on, and their priorities |
| OAM | `x/2 0x07000000 512` | every sprite: position, size, **priority**, tile, palette |
| OBJ VRAM tile *n* | `x/2 0x0601000+n*32 16` | whether a sprite's tiles are blank |
| palette RAM | `x/2 0x05000000 512` | |

The scroll registers are **write-only**; reading `0x04000018` gives open bus
(`0x4381` here, the prefetched instruction), so get the scroll from the game's
own state or from a write watchpoint instead.

## Matching frames

Frame numbers do not correspond between the two, so align on an **event** and
count from it. For B45 that was "the frame OBJECT_70's twelve mask entries
first appear in OAM": mGBA 1034, port 913, a constant 121 apart. The port's
`TMC_SINK_TRACE` prints its OAM in the same decode the debugger produces, so
the two dumps diff directly.

Beware animation phase: two runs can be on the same *scene* frame with the
player one frame apart in his walk cycle, which changes his sprite's piece
decomposition and offsets completely while leaving everything anchored to his
entity position identical. Compare the first frame of an event, where the phase
is forced, before trusting a difference.
