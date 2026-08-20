# CLAUDE.md

The Minish Cap decompilation plus a PC port (`PC_PORT`, `port/`). Engine code
under `src/` and `include/` is decompiled — match its style, and treat
unexplained literals as load-bearing until proven otherwise.

## Current work: viewport expansion (240×160 → 320×240)

**Milestone 1 (width) is signed off. Milestone 2 (height) is functionally
complete — every spike landed and thirty-two of the thirty-three tracked bugs
are closed. B21 is the one still open, and what is left is two decisions rather
than work: frame time is +41% over the canvas baseline with peak frames past the
16.67 ms deadline, and B21's light shaft cannot reach the right edge without
reallocating a BG layer's screenbase — no go/no-go is recorded for either.

**The tracker keeps growing from playtest reports, not from sweeps.** B28-B33
all arrived as recordings after the milestone was called complete, and four of
them (B30, B31, B33 and B27 before them) are the same theme: the periphery shows
world the authored data never expected anyone to see. Expect more of that shape
rather than fewer.

There is also an **arm64 Android build** (`android/`), which is the same
viewport on other hardware and is played on an Ayaneo Pocket S 2K.

Read in this order:

1. `docs/milestone2-status.md` — where things stand, what is left, and the
   frame-time numbers the shipping decision rests on.
2. `docs/viewport-bug-tracker.md` — authoritative for behaviour. Thirty-three
   bugs, the decisions taken, the screenblock-fallback sweep, and the lessons
   that cost the most to learn. **Read B26, B27 and B30-B33 together**: they are
   one theme — the periphery showing world the authored data never expected to
   be visible — and each later one was mistaken for a fresh bug at first.
3. `tools/capture/README.md` — the capture/replay tooling and diagnostics.
   The switches matter more than the prose: `TMC_TILESET_TRACE`,
   `TMC_TILE_PROBE` and the per-layer `TMC_DISABLE_*` are what turned the last
   six reports from guesswork into measurement. `TMC_MASK_BG*` is the newest
   and answers a different question — *where is this layer* rather than *what
   did it contribute* — by painting it flat magenta in the frame itself.
4. `android/README.md` — the Android build, and how to drive the same
   capture/replay tooling on a device.
5. `docs/town-tileset-residency.md` — the plan B27 was built from, kept as the
   record of what it measured and its do-not-retry list. **B27 itself is
   closed**; the residency mechanism it created is still where B30, B31 and B33
   live.
6. `docs/viewport-expansion-research-plan.md` — the original plan and the
   per-spike write-ups, a historical record.

The tracker wins wherever the plan disagrees with it; several spike write-ups
carry inline "superseded" notes pointing at later work.

**Six of this milestone's defects were live in the shipping 240×160 build or
through all of Milestone 1** — the expansion exposed them rather than causing
them, and the last two (B23, B25) only because it made the rolling barrel worth
playing. The regression gate proves the shipping build did not *move*; it cannot
prove it was right. When a change alters a mechanism rather than a surface,
count the frames that exercise the mechanism instead of reading a gate pass as
coverage. B16 and B17 both lived in code the canonical route never executes.

**When a bug needs a human at the controls, ask for a recording early.**
`record-bug.sh` found B13, B5, B15 and B16 in one pass each, after rounds of
inference found nothing.

**Three bugs — B5, B15, B17 — were one defect reported three times:** a world
layer loses its map source, falls back to the VRAM screenblock, and 32 tiles
cover 256 px, not 320. If a room renders as sprites over black above native
size, start from `TMC_REJECT_TRACE=1`, which names the rejection class in one
run. The sweep in the tracker enumerates the rest.

**The faded room transition (VIEWPORT_SCROLL_FADE) defers the apply by 32
frames, and state that keys off "is a scroll in progress" reads *no* during
them.** B16 lost the player's facing that way; B24 lost a lily pad's whole
carry-across-the-scroll state, which exits on `reload_flags == 0` and so quit
28 frames before the room changed, leaving the pad and the player outside the
room. Anything that runs between a hand-off and the end of a scroll must be
checked against `ScrollTransitionIsPending()`. `minecart.c` has the same shape
and is still unexercised.

**B22 is the same assumption a fourth time, one axis over, and it broke
gameplay rather than rendering.** A room that is *exactly* viewport-sized on
hardware — 240x160 — pins the camera on the room origin, which makes
`scroll_y` and `origin_y` the same number and lets the engine spell a room
position either way. Above native size they differ by the centring offset. The
rolling barrel held the player 40 px off its own midline that way, so the doors
and the cobweb hole were out of reach. **In such a room, every camera-relative
expression is unverified code.** The width sweep did not catch this because it
asked about width; the vertical case has not been swept.

**Authored region tables encode how much of the world fits on screen.** Hyrule
Town and Minish Village swap tilesets by camera position from tables whose
regions have gaps between them — Town's is 128 px — sized so a 160-row screen
only ever overhangs one region a little before the next takes over, which makes
`CheckRegionsOnScreen`'s first-match-wins right. At 240 rows the overhang
triples and the screen shows the next region's scenery with the previous
region's tiles loaded (B26). **The fix is to test regions against the centred
`DISPLAY_WIDTH x DISPLAY_HEIGHT` sub-rect, which is exactly where the GBA's
camera would be for the same player position** — the original rule is then right
everywhere, and it needs no viewport gate. Two attempts to invent a smarter rule
(max-overlap, then max-overlap among disjoint regions) each fixed the report in
front of them and broke another list, because these tables are partitions in
some places and override-plus-default in others; simulated over all five lists
and 43,000 camera positions they score 8316 and 3897 disagreements with hardware
against 0 for the centred sub-rect. **When authored data assumes a screen size,
give it that screen rather than reasoning about the geometry** — and simulate
every table before changing a selection rule; it is static data and costs
minutes. **Before blaming a per-frame budget, read the authored data**: B26 also
cost three measured-and-discarded hypotheses (sprite gfx slots, the 128-entry
OAM cap, screenblock coverage).

**Above native size the port keeps *every* alternative tileset in memory and
picks between them per tile (B27).** `gVram` carries one bank per gfx group
above the GBA's 96 KB — `PORT_VRAM_BANK_OFFSET`, unreachable by the engine
because every `gba_read/write` guard still stops at `0x06017FFF` — and
`VirtuaPPUMode1CharSlot` tells the renderer which offset *and which BG palette*
to use for tiles whose room position falls in a given region. **The slot is
found by the tile's character address, not its position**: Hyrule Town runs
three region tables over the same room at once and only the address says which
one governs. Minish Village needs the palette half too, and its shadow palettes
are rebuilt inside `FadeVBlank` so they carry the same per-bank fade the live
one does.

**A tile in an authored *gap* has no hardware answer once it is in the
periphery (B33).** B27 gave gap tiles the group the engine loaded — correct
inside the GBA's screen, arbitrary outside it, where the tile then changes
tileset whenever the camera crosses an unrelated threshold. Peripheral gap tiles
take the group of the region they adjoin instead. **Growing the rectangles alone
is wrong**: simulated over 8,439 camera positions it overrules hardware on
162,922 gap tiles *inside* the screen. A guard rect covering the centred
`DISPLAY_WIDTH x DISPLAY_HEIGHT`, published ahead of the grown copies and
carrying the old fallback, makes that zero by construction rather than by
argument — cheaper than proving a rule safe.

**A hand-scrolled layer's window is sized for the GBA's screen too (B32).**
MinishPaths' parallax layers keep a fine `yOffset` and re-point `subTileMap`
every 64 px; the block they index is 32 tiles, so the screen must fit in
`256 - yOffset` and at 240 rows it does not. Re-base on a smaller step. The
horizontal twin needs `xOffset + 320 <= 256` and cannot be fixed this way at
all. **And a scene with parallax cannot be judged by whole-frame diffs** —
three layers at three rates means no alignment exists; `TMC_DISABLE_BG1/2/3`
leave one layer on, and then "did it scroll cleanly" has an exact answer: zero
residual under a pure shift on every consecutive pair.

**Declaring a slot and *keeping* it declared are different problems (B31).**
The manager's init reset ran a frame after `OnEnterRoom` had already declared
the room's slots and wiped all three; only a camera-driven group change
re-declares one, so from every town entry the periphery drew from the centred
screen's group until the camera crossed a threshold. `TMC_TILESET_TRACE=2`
cannot see this — its `groups` line reports the engine's choice, which was
right the whole time. The question to ask the renderer is *why it chose what it
chose*; "no published slot holds this character address" is the answer that
names it, and then you look for who emptied the table.

**And a slot the camera never selects was never declared at all (B30)**, because
the declaration hung off `LoadGfxGroup` and B26's centred sub-rect means some
slots never match — so their tiles drew the previous room's until the camera
moved. `TMC_TILESET_TRACE=2` prints the per-frame `groups`; a `255` there is the
whole diagnosis.

Two things about it are load-bearing and were each learned by getting them
wrong. **Whether a group reads real VRAM is a per-area decision**: town names a
resident group so the oracle-house overlay survives, Minish names
`PORT_TILESET_NO_RESIDENT` because its load is staged over eight frames and VRAM
is briefly neither group. And **when a fix seems to do nothing, check it ran on
the frame you are looking at** — the character half worked from the first build,
but every sampled frame sat inside that eight-frame window. `TMC_TILE_PROBE`
prints the offset actually chosen for a tile and settles it in one run.

**Measuring this class of defect needs the noise removed first.** Whole-frame
pixel deltas are useless — one pixel of camera scroll already changes 21,000
pixels — and so are the obvious refinements: aligning two frames a pixel apart
misaligns the HUD, which is drawn at a fixed screen position, and sprites differ
between any two frames. `TMC_DISABLE_OBJ` and `TMC_DISABLE_BG0` take both out;
`TMC_TILESET_OFF` gives before and after from one binary. With those, all six
Minish recordings score 0.00% across the threshold — **but that is stability,
not correctness.** Two of the six were still visibly wrong at 0.00%, because
the palette was wrong on both sides of the flip. **The oracle is to walk the
same world content into the centred 240x160, where the GBA draws it right, and
compare against that**; it needs no assumption about which group is loaded.

**A room in a GBA affine display mode (1 or 2) draws itself: its enter handler
loads *whole layers* from a gfx group, maps included, and none of them come from
`gBGxBuffer`.** Port code that pushes those buffers into VRAM must skip them.
B25 was a port-only line doing exactly that, live at 240x160 too. It also shows
how such a bug hides: overwriting BG2's affine map turned the barrel into
obvious noise, while overwriting BG1's map only removed the alpha-blended wood
grain, which read as "different colours" until the maintainer said the lines
were missing. **Fix every layer the handler owns, not the one whose symptom you
can see** — `LoadGfxGroup(0x16)` writes four destinations and two are maps.
`grep DISPCNT_MODE_ src/` still returns exactly two sites: the title screen and
the rolling barrel.

**Extracted assets are not the ROM, and the gap is exactly where pointers
live.** The decomp writes a pointer inside a data blob as `.4byte <symbol>` —
a relocation, not bytes — so `port_asset_index.c` describes such a symbol as
several `.incbin` fragments with an unindexed four-byte hole per pointer. Any
consumer sized from one index entry stops at the first pointer and then walks
records off the end of the buffer. B28: Lon Lon Ranch's house-door list was 8 of
its 36 bytes, so the locked door drew itself and had no collision, and its
neighbour was never spawned. `infer_room_property_size` in the extractor rejoins
the fragments; **a change there reaches nobody who is past first run** unless
`kExtractorFormatVersion` is bumped, because the up-to-date check only
fingerprints the ROM. Scan `data/map/entity_headers.s` for symbols whose body
mixes `.incbin` with `.4byte` to enumerate the rest — there are five.

**A grep over source cannot see an address that only exists as data.** Spike 6
relocated `gBG0Buffer` out of `gEwram[0x34CB0]`, searched the tree for code
naming that address, found the one site and fixed it. The area-name banner's
`Font` is twenty-four bytes of ROM whose `dest` becomes `0x02034E0E` only inside
`Port_DecodeFontGBA`, so it was invisible to that search and the banner drew
into dead memory for three weeks (B29). When relocating something the GBA
addressed by a fixed number, make the resolver itself know where it went —
`gba_TryMemPtr` now maps the BG0 range — rather than chasing the callers.

**The 240x160 build answers "was it the expansion's geometry", not "was it the
expansion".** B29 reproduces there and is still a Milestone 1 regression,
because the buffer move applies at every size. A port-wide change is invisible
to a size comparison.

**A regression gate that runs a mechanism is not a gate that covers it.** The
canonical route enters five new areas per run — five banners — and dumps each
waypoint 300 frames after its warp, while a banner lives 120. 11/11 stayed
green across the regression and across the fix.

**A defensive guard whose comment names an unconfirmed cause is a bug that
cannot be found.** Three of them in `houseDoorExterior.c` each turned B28's
crash into plausible output, and the report that eventually arrived described
the *rendering*. `01948f13` was reverted twice for reading the short buffer as a
native 16-byte struct — right instinct, wrong model; the buffer is packed and
merely short.

**A platform-only symptom is not a platform bug.** B16 reproduced on Android 2
runs in 3 and never on desktop, and six rounds went into what was different
about the device — all wrong. The engine was identical; one out-of-bounds read
of a four-entry table returned different padding per toolchain and let desktop
recover from a fault both platforms had. In decompiled code an out-of-range
index is defined on hardware (ROM is contiguous) and undefined here.

## Building

```bash
xmake f -c -y -m release && xmake build tmc_pc     # 240x160 build -> build/pc/tmc_pc
```

For 320x160 prefix **both** commands with `TMC_VIEW_W=320`; for 320x240 add
`TMC_VIEW_H=240` as well. The `-c` is required — a plain `xmake f` will not
drop a previously configured size, and the next build silently stays expanded.

Name builds WxH (240x160, 320x160, 320x240), never "the 240 build" — with two
axes in play a bare number no longer says which.

The **Android** build is separate and lives in `android/` (Gradle, arm64-v8a,
defaults to 320x240):

```bash
cd android && ./gradlew assembleDebug
```

It parses the source list out of `xmake.lua`'s `tmc_pc` target rather than
duplicating it, so an engine file added there joins that build too. See
`android/README.md` — including how to run capture scripts and read the port's
own traces on a device, which is what identified B16.

## Always refresh both playable builds at the end of a work cycle

`build/play-320x240/` and `build/play-240x160/` are the self-contained builds
the maintainer actually plays, and they are how every bug in the tracker that
needed a human at the controls was found. **Rebuild and reinstall both before
handing work back**, even when the change looks headless — a fix that is only in
`build/pc/tmc_pc` is a fix nobody can playtest, and the maintainer has no way to
tell a binary is stale short of not seeing their bug fixed.

The 240x160 one exists to answer *"is this the expansion's fault or was it
always like that?"* by hand. Six of Milestone 2's defects were live in the
shipping build all along and only looked new, and each cost rounds before that
was established. It is the first thing to ask of any new report — B23 and B25
were both settled in one run each by warping into the room at 240x160.

**Order matters, and it makes the 240x160 copy free.** The gate below needs a
240x160 build and the other play build needs a 320x240 one, and `xmake f -c`
drops the previous size. Do the gate first, install its *exact binary* as the
240x160 play build, then configure 320x240 once:

```bash
# 1. gate (see below) at the default 240x160, then install that same binary
cd build/play-240x160
rm -f tmc_pc_240x160.prev            # older .prev is dropped, 45 MB apiece
mv tmc_pc_240x160 tmc_pc_240x160.prev 2>/dev/null || true
cp ../../build/pc/tmc_pc tmc_pc_240x160

# 2. then the expanded one
TMC_VIEW_W=320 TMC_VIEW_H=240 xmake f -c -y -m release
TMC_VIEW_W=320 TMC_VIEW_H=240 xmake build -y tmc_pc
cd build/play-320x240
rm -f tmc_pc_320x240.prev
mv tmc_pc_320x240 tmc_pc_320x240.prev
cp ../../build/pc/tmc_pc tmc_pc_320x240
```

Doing it in that order means the 240x160 play binary *is* the binary the gate
passed on, at no extra configure. Rebuilding it separately is not equivalent and
not free — and note a rebuild is no longer byte-reproducible anyway, because the
startup identity line embeds `__DATE__`/`__TIME__`; compare `.text` rather than
whole files if you need to prove two builds are the same code.

Neither play directory ships a `tmc.sav`. Do not create one: let the maintainer
copy their own in. Saves are viewport-independent, so the same file works in
both and moving one across is the fastest way to compare a scene at both sizes.

Confirm what you installed rather than assuming the copy was the right binary:
`cmp` each against `build/pc/tmc_pc` at the time you copy it, and replay a
capture script through it from a temp dir to check it renders what you verified.
The 240x160 one has a stronger check available and worth using — replay the
canonical route through the installed binary and diff against
`tools/capture/references/spike0-240x160`; it must be 11/11.

Then update those directories' `README.md`: they are written to the playtester
and say what changed since the last binary, so a stale one is worse than none.
Move the previous cycle's notes down a section rather than deleting them. The
240x160 README is about *why you would reach for it* rather than a per-cycle
changelog, so it usually needs nothing beyond the build date.

Two ways to damage these directories, both already done once:

- **Never run anything from inside it.** The game overwrites `tmc.sav` as it
  plays, and that save is the maintainer's. Run captures from a `mktemp -d`
  with `baserom.gba` and `assets/` symlinked in, the way
  `tools/capture/run_route.sh` does.
- **Do not symlink `rom_data/` into such a harness.** The port extracts ROM
  pages into it on boot, and the write follows the link — one verification run
  left 2789 files and 11 MB behind. Let the temp dir keep its own.

`build/` is gitignored, so none of this appears in `git status` and no commit
will remind you. It is only ever as fresh as the last time someone did it.

**The APK is the same rule on the other platform, and it drifts further.** The
maintainer plays 320x240 on an Ayaneo as well as on the desktop, so a fix that
is only in `build/play-320x240/` is a fix half the playtesting cannot see.
Rebuild it in the same cycle:

```bash
cd android && ./gradlew assembleDebug     # APK in app/build/outputs/apk/debug/
```

Confirm the fix reached the artifact rather than the intermediate — the APK
carries a *stripped* `libmain.so`, so read that one:

```bash
unzip -p android/app/build/outputs/apk/debug/app-debug.apk lib/arm64-v8a/libmain.so > /tmp/libmain.so
```

and disassemble the function you changed with the NDK's `llvm-objdump`. This is
the same "confirm what you installed" step the desktop copy gets, and it is
cheap: the Gradle build reuses its native cache and takes well under a minute
when only a few files changed.

`./gradlew installDebug` puts it on a connected device; with none attached the
APK is all that can be produced here, and someone has to install it. Say so
when handing back, because nothing else will.

## Regression gate — run before any viewport commit

At the **default 240x160 build**, both must hold:

- canonical route: 11/11 waypoints pixel-identical
- map-source audit: 0 mismatches in 265,497,600 fetches

Exact commands in `tools/capture/README.md` ("Regression gate"). Both have
caught real regressions, including a change intended for an expanded build
that altered what the shipping build renders. 240x160 is the shipping build.

## Conventions

- **Do not commit extracted assets.** Everything in `assets/` is derived from
  `baserom.gba` except five hand-authored config files; `assets/.gitignore`
  has the allowlist and the reasoning.
- `baserom.gba`, `tmc.sav` and `config.json` are gitignored and must stay so.
- Verify a border by counting **distinct colours per column**, not by testing
  for black — a clipped UI screen's border is the PPU backdrop, which is not
  black. This has produced false results before.
- When a fix makes a measurement jump to its theoretical best, confirm the code
  you think produced it actually ran. That has produced false results too.
