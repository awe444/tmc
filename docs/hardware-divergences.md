# Intentional divergences from hardware

Everything in this file makes the port render something the real game does not,
**on purpose**. Each entry has been checked against hardware and found to be
faithful before it was deliberately made unfaithful.

This document exists because the project settles arguments by asking the
hardware — mGBA runs headless here and its savestates carry a frame's state and
its picture together (`tools/mgba/README.md`). That method quietly assumes any
difference from hardware is a bug. Once that stops being true anywhere, the
list of exceptions has to be written down, or a future session will "fix" one
of these back and a comparison against hardware will look like a regression
when it is the intended behaviour.

**A divergence is not a bug fix.** A bug is the port failing to do what the
game does. A divergence is the port choosing not to. They are recorded
differently and justified differently: a fix needs a root cause, a divergence
needs a reason the hardware behaviour is not good enough *here* and an account
of what it costs.

## Rules

1. **Prove faithfulness first.** Show the port already matches hardware, with a
   measurement, before diverging. If the port does not match, that is a bug —
   fix it, and only then ask whether the fixed behaviour is wanted.
2. **Gate it to the expanded viewport.** 240x160 is the shipping build and must
   stay byte-faithful: the regression gate is defined against it and "anything
   at 240x160 is a release blocker". Use a compile-time gate so the shipping
   binary contains no divergence code at all, and check it compiled out.
3. **Make it deterministic.** A divergence whose result depends on where the
   player came from is a new bug waiting to happen. Set the value; do not
   inherit one.
4. **Keep it in `port/port_divergences.c`.** One home, so the population is
   greppable and this document can be checked against it.
5. **Record it here before writing the code**, with what it costs.

## Why these exist at all

Almost every candidate has the same origin: **the expanded viewport shows world
the authored data never expected anyone to see.** The tracker's B26, B27, B30,
B31 and B33 are all that theme as *bugs* — the periphery drawing from the wrong
tileset, or from a slot nothing declared. The divergences below are the cases
where the authored data is not wrong at all, it simply stops at the edge of the
GBA's 240x160 screen, and there is nothing correct to fall back on because the
authors never had to decide.

---

## D-1 — beanstalk rooms' ground palette

**Status:** in, 2026-08-23. `Port_Divergence_BeanstalkGroundPalette()`.
**Tracker:** B52.

`LoadRoomTileSet` mirrors BG palette 3 into OBJ palette 5 on every room load
(`playerUtils.c`, `MemCopy(&pal[0x30], &pal[0x150], 0x20)`), so sprites can
wear the room's terrain colours — `LoadObjPaletteAtIndex` deliberately loads
nothing for palette ids <= 5 because those slots are expected to already hold
what the room put there.

Area 13's tileset leaves BG palette 3 as an all-`0x7C1F` placeholder. The only
sprite that uses it is the beanstalk base, five OAM entries at **y = 208..237**
— below the GBA's centred `y = 40..199`, so it is never on screen and the
authors never had to colour it. At 240 rows it is on screen, and it draws solid
magenta.

**Hardware was checked and matches.** An mGBA savestate of the same room has
`OBJ palette 5` and `BG palette 3` both 12/16 `0x7C1F`, identical to the port.
The mirror is faithful; the placeholder is the real game's.

**What the port does instead.** Loads, into OBJ palette 5, the BG palette 3 of
the overworld room each beanstalk grows out of — read out of those five rooms
and matched against the ROM's palette table rather than picked by eye:

| Beanstalk | Source room | Palette |
|---|---|---|
| Mt Crenel | area 6 room 0 | `gPalette_550` |
| Lake Hylia | area 11 room 1 | `gPalette_381` |
| Ruins | area 5 room 1 | `gPalette_537` |
| Eastern Hills | area 3 room 3 | `gPalette_446` |
| Western Woods | area 3 room 0 | `gPalette_446` |

The source rooms are `gUnk_080B4410`'s five entries — the same table the
beanstalk subtask itself reads — so the mapping is the game's, not invented.
Applies to all ten area-13 rooms (0-4 tops, 16-20 climbs).

**Cost.**

- The room can no longer be validated against hardware by comparison. Anyone
  diffing it against mGBA will find a difference that is *supposed* to be
  there. This entry is the reason it is not a regression.
- Five OAM entries change; the stalk itself is on OBJ palette 7 (52 entries)
  and is untouched.
- Only the Mt Crenel beanstalk has been looked at in motion. The other four
  take the same code path with their own source palette and are unverified by
  eye.

**Rejected alternative.** Suppressing the mirror in area 13 and letting the
slot keep whatever the previous room left. It produces the same picture when
you climb up from the source area, and was how this was first prototyped — but
it is not deterministic: measured, a first room load after boot leaves the slot
`0000 0000 0000 0000`, so the base would render black, and descending from a
beanstalk top would re-inherit the placeholder. A wrong-but-plausible colour is
also worse to diagnose than magenta, which at least announces itself.

---

## Considered and not taken

Recorded so they are not re-litigated from scratch.

- **B22, rolling barrel rim sprites in the border.** The barrel interior is
  exactly 240x160, so at 320x240 its rim sprites sit in the border. Left as-is
  and closed as a costed decision — the room is playable and the fix was not
  worth its risk. This predates the rules above and is not in
  `port_divergences.c`.
- **B49, beanstalk-top sky.** `Area_Beanstalks` room 0 differs from the centred
  240x160 sub-rect by 1352 BG-only px in the sky rows. **Not** a divergence
  candidate yet: unlike D-1 it has not been shown faithful, so it is still an
  open bug and must be diagnosed before anyone decides to paper over it.
