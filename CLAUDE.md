# CLAUDE.md

The Minish Cap decompilation plus a PC port (`PC_PORT`, `port/`). Engine code
under `src/` and `include/` is decompiled — match its style, and treat
unexplained literals as load-bearing until proven otherwise.

## Current work: viewport expansion (240×160 → 320×240)

**Milestone 1 (width) is signed off. Milestone 2 (height) is functionally
complete — every spike landed and fifty-seven of the sixty-two tracked bugs
are closed; B41, B42, B46, B47 and B49 are open.**
What is left is one decision rather than work: frame time is +41% over the
canvas baseline with peak frames past the 16.67 ms deadline, and no go/no-go is
recorded. B21, open for nearly two weeks as "unfixable", closed 2026-08-20 once
the question changed from *how does the layer reach further* to *what are those
columns showing now* — see its Lessons 31 and 32.

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
2. `docs/viewport-bug-tracker.md` — authoritative for behaviour. Forty-four
   bugs, the decisions taken, the screenblock-fallback sweep, and the lessons
   that cost the most to learn. **Read B26, B27 and B30-B33 together**: they are
   one theme — the periphery showing world the authored data never expected to
   be visible — and each later one was mistaken for a fresh bug at first.
   **Read B32 and B34 together too**: same arithmetic, two managers, and the
   second was found by instrument rather than report because nobody swept the
   first by mechanism.
3. `tools/capture/README.md` — the capture/replay tooling and diagnostics.
   The switches matter more than the prose: `TMC_TILESET_TRACE`,
   `TMC_TILE_PROBE` and the per-layer `TMC_DISABLE_*` are what turned the last
   six reports from guesswork into measurement. `TMC_MASK_BG*` is the newest
   and answers a different question — *where is this layer* rather than *what
   did it contribute* — by painting it flat magenta in the frame itself.
4. `tools/mgba/README.md` — the hardware oracle. mGBA runs headless here, and
   its savestates carry a frame's **state and its picture together**, which is
   the only way some questions get answered at all. Also how to replay our own
   capture scripts into it, and the save conversion that has to happen first
   (B47).
5. `android/README.md` — the Android build, and how to drive the same
   capture/replay tooling on a device.
6. `docs/town-tileset-residency.md` — the plan B27 was built from, kept as the
   record of what it measured and its do-not-retry list. **B27 itself is
   closed**; the residency mechanism it created is still where B30, B31 and B33
   live.
7. `docs/hardware-divergences.md` — the places the port renders something the
   real game does not, **on purpose**, and the rules for adding one. Short, and
   worth reading before concluding that a difference from mGBA is a bug: the
   whole hardware-oracle method assumes it is, and this is the list of
   exceptions. Prove faithfulness first, gate to the expanded viewport, set the
   value rather than inherit it, and keep the code in
   `port/port_divergences.c`.
8. `docs/viewport-expansion-research-plan.md` — the original plan and the
   per-spike write-ups, a historical record.

The tracker wins wherever the plan disagrees with it; several spike write-ups
carry inline "superseded" notes pointing at later work.

**Not every difference from hardware is a defect any more (B52).** The
beanstalk base draws magenta on real hardware too — the palette slot it uses is
an authored placeholder, because the sprite sits below the GBA's 160 rows and
is never on screen. The expanded viewport shows it, so the port now diverges
deliberately. That is the first such case; `docs/hardware-divergences.md` is
the list, and it must be consulted before "the port disagrees with mGBA" is
treated as a bug.

**Ten of this milestone's defects were live in the shipping 240×160 build or
through all of Milestone 1** — the expansion exposed them rather than causing
them, B23 and B25 only because it made the rolling barrel worth playing, and
B43 only because the maintainer recorded the cutscene at 240x160 on request. The regression gate proves the shipping build did not *move*; it cannot
prove it was right. When a change alters a mechanism rather than a surface,
count the frames that exercise the mechanism instead of reading a gate pass as
coverage. B16 and B17 both lived in code the canonical route never executes.
B56 is the counting done right: a change to when *every* HBlank DMA stops, and
the route runs that mechanism 4312 times — all with the window disabled, which
is why 11/11 survived a change that broad, and the 111 frames where it was
enabled had to be diffed separately to show live channels were untouched.

**When a bug needs a human at the controls, ask for a recording early.**
`record-bug.sh` found B13, B5, B15 and B16 in one pass each, after rounds of
inference found nothing. **A story-gated cutscene is that case by
construction** — the scripted tester can warp to a room but cannot set story
flags or run a cutscene, so B41 and B42 stalled at "named suspect, unverified".
What inference *is* good for there is narrowing: a synthetic fixture
(`TMC_FILL_PROBE`, `TMC_OAMY_PROBE`) can rule a mechanism out without the
scene, and ruling one out is worth asking for.

**And when a recording cannot be transplanted across a configuration, ask for
one made *on* that configuration.** Frame-exact input replay desynchronises
under any change — viewport, or a compile-time switch like
`VIEWPORT_SCROLL_FADE` — so it cannot serve as a cross-size A/B; three
consecutive reports hit this. B43 was reclassified from "unknown" to "not a
viewport bug" the moment the maintainer recorded the same cutscene on the
240x160 build. That is a minute of their time and unreachable by any amount of
inference.

**There is a hardware oracle on this machine now — use it before arguing.**
mGBA 0.10.2 runs headless under `SDL_VIDEODRIVER=dummy`, and its `-d` CLI
debugger takes commands on stdin, so the real game can be replayed and read by
script: `tools/mgba/README.md`. The game reads `REG_KEYINPUT` once per frame at
`0x0801D6C4`, so `watch/r 0x04000130` is a per-frame breakpoint and `w/r r0`
injects input — our capture scripts already hold GBA key masks, so one replays
directly. OAM, DISPCNT/BGxCNT and OBJ VRAM are all at fixed addresses, so no
game symbols are needed. Align the two runs on an *event* (B45 used "the frame
the mask's twelve OAM entries appear"), not on a frame number, and put the port
at the **same viewport** before comparing anything scroll-dependent — four B45
passes were spent on 320x240-against-240x160 comparisons that could not have
been valid. Watch for animation phase too: the same scene frame with the walk
cycle one frame apart changes the player's whole sprite decomposition. **A save
must be converted first — see B47.**

**A struct whose offsets are written in comments is asserting something, and
nothing was checking it (B51).** `SaveFile`'s every field carries its GBA
offset in `include/save.h`, and one is wrong: `KinstoneSave` sums to 327 bytes
where `kinstones` 0x114 → `flags` 0x25C leaves 328, so the port writes
`flags[0x200]` and the three `dungeon*` arrays a byte early. Nothing caught it
because a `u32` alignment hole before `darknut_timer` makes `sizeof(SaveFile)`
coincidentally right, and because **the port is self-consistent** — it writes
and reads 603, so its own saves work and the fault is invisible until a file
meets the real game. On hardware every story flag is then shifted a bit: Link
loses Ezlo and world events un-do, while name, stats, inventory and kinstones
read perfectly because they sit before the boundary. **"Most of it is correct"
localises a layout bug far better than "none of it is"** — the split names the
field. `savconv.py` compensates and re-checksums; the struct is still wrong and
fixing it needs a save migration, so `LAYOUT_FIXED_IN_PORT` must be flipped in
the same change or the tool will corrupt what it touches.

**A no-op stub is a claim that the hardware operation did nothing, and
teardown is where that claim is usually wrong (B56).** `DmaStop` was
`((void)0)` in the port, which is right for every copy and fill in the game and
wrong for the one mode where a DMA is a *standing registration*: on hardware
`VBlankIntr` calls `DmaStop(0)` every frame and `PerformVBlankDMA` re-arms the
HBlank channel only while `gVBlankDMA.ready`, so that stub is the only teardown
an HBlank DMA ever gets. Code ending an effect with a bare
`gScreen.vBlankDMA.ready = FALSE` — `LightRayManager_Action3` does, never
touching `DisableVBlankDMA` — is therefore correct there and leaked here, and
`port_hdma_vblank_reset`'s rewind made the leak permanent by design. Minish
Woods' light shaft came back **bent** by the sine table the parallax rays left
in BG3HOFS, at both sizes. **Frozen geometry beside a live camera names a stale
producer**: eighty consecutive masked frames were byte-identical while the
world scrolled, and anything the running scene drives changes between frames.
That is one `cmp` of two dumps, and a much smaller search than "what bends a
ray". `TMC_HDMA_TRACE=2` names it outright (`active, not re-armed this
frame`); `TMC_HDMA_KEEPSTALE=1` is the A/B from one binary. **The tell to grep
for is a stub whose real counterpart is called unconditionally every frame** —
nobody writes that unless it does something.

**A NULL pointer is readable on the GBA and fatal here, so a check that sits a
couple of statements too late is a crash rather than a smell (B54).** Address 0
is BIOS: the read returns open bus, so code that dereferences a NULL `parent`
and then deletes the entity on the very next line is correct on hardware by
accident. `DarkNutSwordSlash` does exactly that — `EnemyDetachFX` NULLs a dying
darknut's child, and the slash's first update reads `parent->type` two
statements above its own `parent == NULL` test — and the darknut fight
crashed at both viewport sizes. The tell is a `parent`/`child`/
`contactedEntity` NULL test *below* a use of the same pointer in the same
function; grep for that shape rather than for the crash. `rupeeLike.c` and
`acroBandits.c` are the same family, already guarded.

**A multi-return modelled as a `u64` is a 32-bit-pointer idiom, and survives
translation only where something narrows it (B58).** `GetFuserId` returns the
fuser id in the low word and the fuser text id in the high one — the ARM
function really does return two values, so `asm.h` declaring it `u64` is right.
On the GBA every use of it as an index is truncated for free by 32-bit address
arithmetic; here the high word survives and
`gSave.kinstones.fuserProgress[GetFuserId(this)]` reads 2.7 TB past the array.
**One instruction names it** — `movzbl (%rax,%rdx,1)` with `rax` holding a
value like `0x2850000003b` is a 64-bit quantity being used as an index. It is
the only `u64`-returning function in any header, which bounds the class exactly:
twelve callers, eight narrow to a `u32` local and are fine, four index directly
and crash — the Tingle siblings and all three Great Fairies. **Grep for the
return type, not for the symptom.**

**An enemy struct that spells its extra area as raw bytes is 4 bytes out on
64-bit, and `GE_FIELD` will not save it (B61).** The canonical `Enemy` opens
that area with `Entity* child` — 4 bytes on GBA, 8 here — and `GE_FIELD` shifts
by that difference for `kind == ENEMY`. A subtype that writes `u8 unk_68[0x5]`
instead reserves nothing, so every field below sits 4 bytes early:
`LoadRoomEntity` wrote `dat->type2` to PC 0xA8 while `EyegoreEntity.flag` read
0xA4, got `field_0x78`'s kind/flags byte, and **every Eyegore in the game** took
`Eyegore_Init`'s already-triggered branch — `ENT_COLLIDE` cleared and the
closed-eye animation. Castor Wilds' statues were unshootable. **Assert the
offsets rather than commenting them**: a
`PORT_STATIC_ASSERT_OFFSET(..., flag, 0x7c, 0xA8, ...)` does not compile against
the wrong layout, which is proof independent of viewport. **51 enemy structs
share the shape and are not swept — see the tracker's "Follow-up" section** — `src/object/` and `src/npc/` are fine,
because `GE_FIELD` shifts only for ENEMY and PLAYER and those match
`GenericEntity`. Being broken also needs the struct to *read* a field
`LoadRoomEntity` writes (0x78, 0x7a, 0x7c, 0x80, 0x82, 0x84, 0x86) rather than
use it as scratch.

**A raw GBA byte offset into a struct that holds a pointer is wrong here, and
only from the pointer onward (B53).** `itemForSale.c` read the player's
interaction target as `*(u8*)(ptr + 1)` and `*(int*)(ptr + 8)` —
`InteractableObject.type` and `.entity`. 64-bit pointers leave `type` at 1 and
move `entity` from 8 to 16, so the port read `customHitbox` instead, which is
NULL for nearly every interactable: every A press while carrying a shop item
took the cancel branch, and Syrup's mushroom (an `ItemForSale`,
`ITEM_QST_MUSHROOM` 0x38) snapped back to its stand while she gave her generic
line. **The fields before the first pointer read correctly and the ones at or
after it do not** — the same split that localised B51, and the signature to
look for. Fix with typed field access under `#ifdef PC_PORT`, keeping the
original expression for the GBA build, as `rupeeLike.c` and `talon.c` already
do. `grep` for `*(u32*)&` and `*(int*)(ptr +` to see the rest of the class.

**And ask for a savestate, not just a replay.** A replay gives state and never
pixels; an mGBA savestate is a PNG carrying the frame's state *and* the picture
it produced, which is the only artefact that can answer "why is this pixel this
colour". `tools/mgba/ssextract.py` and `readstate.py` do the rest. Six passes on
B45 compared registers, OAM, maps and tilesets against hardware, matched on
every one, and concluded the port was faithful while the screen plainly
differed — the mismatch was a compositing *rule*, which no state comparison can
see.

**A rule pinned on two data points is pinned on two data points, and a
submodule bisect that never moved the submodule proves nothing (B57).** B45's
OBJ-priority rule — *the layer composites at the priority of the last covering
sprite in OAM order* — was right on both savestates it was taken from and
wrong in general: in both of them the last covering sprite was also the sprite
supplying the colour, so neither could see the difference. A third savestate
(the Deepwood barrel, `baserom.ss1`) separates them, and there the rule hides
the player over most of the room, at both sizes, for five days. **An opaque
sprite that loses the colour lends nothing**; the claim is the colour sprite's
own priority lowered only by *transparent* covering sprites later in OAM order.
When a rule comes from examples, enumerate what the examples have in *common*
that the rule does not require. And **`git checkout` does not move
`libs/ViruaPPU`** — the first bisect built four revisions three weeks apart and
got byte-identical scores from all of them, which reads as "this predates the
tracker" and is exactly what an unchanged renderer produces. `git submodule
update` after each checkout; `git submodule status` before believing the
result. The tell was the score being *identical*, not merely similar.

**Two scenes with the same shape and opposite answers beat either alone
(B45).** A blank sprite over the player lends its priority in the Castor Wilds
swamp and must not in the name-entry glyph; only holding both at once gives the
rule — the OBJ layer composites at the priority of the *last covering sprite in
OAM order*, opaque or not, which neither scene implies by itself. The
regression that looked like a setback was the second data point, and the
cheapest way to get it was to ship the wrong rule at a 173-frame route diff and
read what it broke.

**And "every register matches" is not "the same picture" (B45).** Six passes
compared OAM, BGCNT, maps, tilesets and object data against hardware, found
them identical every time, and concluded the port was faithful — while the
screen plainly differed. The mismatch was a compositing *rule*, which no state
comparison can see. Get the picture before concluding from the state; an mGBA
savestate carries both in one file.

**A recording also ends where the bug did, and the fixed build runs past that
point.** The B43 recording's input stops at frame 4033 because the screen had
gone black; with the fix in, the cutscene reaches a text box forty seconds
later and correctly waits for a button nobody presses. A replay that sits still
after a fix is not necessarily still broken — extend the input before
concluding anything.

**An entity can stop being updated without anything touching the entity
(B43).** Three mechanisms produce that symptom and the entity distinguishes
only one of them: it was unlinked (`prev`/`next` change), the list's *head* was
rewritten out from under it (nothing about it changes), or its dispatcher
declined to run it (nothing about the list changes). Three passes watched the
entity and found nothing, because it was the second. `TMC_ENT_WATCH=1` asks the
other two questions — *which list holds it* and *did this frame's iteration
reach it* — and `inList=-1` beside intact links is the whole diagnosis.

**A subtask's entity-list bracket swaps nine head pairs and nothing else
(B43).** `sub_0805E958`/`sub_0805E974` move `gEntityLists` in and out of
`gEntityListsBackup`; every entity keeps its own links, and a list *sentinel*
is the same object on both sides. So any pointer held across that bracket still
writes through to whatever the lists now hold — `gArea` is not part of the
bracket, and `gArea.transitionManager` unlinked mid-cutscene put list 6's head
back on the overworld chain. **And when the port substitutes a pointer the
original never really dereferenced, ask what the original *did with* it**: the
GBA's `DeleteManager((Manager*)gArea.onEnter)` writes into ROM and is discarded,
so the correct translation is to delete nothing, not to find a better argument.

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
checked against `ScrollTransitionIsPending()`. **`minecart.c` had the same
shape and was named in that comment for three weeks before anyone rode the
cart; it arrived as a Cave of Flames softlock (B40).** When a fix's
investigation turns up a sibling, guard it then — the reasoning is already
loaded, and the alternative is waiting for the expensive report.

**The deferral loses whatever is read at the commit rather than the crossing,
and that is now three things (B55).** `VIEWPORT_SCROLL_FADE` queues a room
scroll and applies it 32 frames later; Link keeps walking for all of them, as
`sub_0807BD14`'s own comment says. His facing was the first casualty (B16) and
the camera target the second (B24) — both are carried across the deferral —
and his **position** was the third: reverse during the fade and the commit runs
from where he got back to, so `Scroll2Step`'s nudge lands him *outside* the
room he is entering, where no edge transition fires. Free to walk, camera
pinned, nowhere to go. Anything the apply reads must be captured at the
crossing, not sampled at the commit — and the guard is the camera target,
because a vehicle legitimately moves him throughout the fade.

**"Stuck" is not one state, and `TMC_STUCK_TRACE` only names one of them
(B55).** It watches `PLAYER_ROOMTRANSITION`, which is B16's shape; a player
stranded outside the room bounds is in `PLAYER_NORMAL` with full control and
the instrument stays silent. Ask *where is he* as well as *what state is he
in*: comparing his x against `origin_x .. origin_x + width` is one line and
settles it. And run that trace at its real 180-frame threshold before
believing it — at `=60` it reports ordinary doorway dithering as a stall.

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

**The map source was read live while OAM was not, so anything drawn both ways
tore by one frame (B60).** `Port_MapSource_Update` bound a *pointer* into
`gMapData*Special`; the renderer dereferences it at draw time, so it showed the
map as of the logic step that runs *after* the binding — one frame ahead of the
OAM that same step produced, which arrives via the engine's buffer at the next
`VBlankIntr`. Invisible until one object is drawn both ways: a large pushed
block is, and it **vanished for exactly one frame at the start of every push
step** (the reverse hand-off overlaps pixel-aligned, so only one end shows).
Hardware settles the design — `baserom.ss1` has the block as OAM[20] over BG2
tile 671, the same tile as the grass beside it — so the engine does clear the
BG and draw a sprite, and lands both at one VBlank. Fixed by snapshotting the
map at bind time (64 KB/frame; present unchanged at 12.03 ms vs 12.12).
**`--mapsource-audit` is the instrument for this whole class and it read
`mismatched=0` for two milestones because the canonical route never pushes a
block** — pointed at the recording it returned 10,240 on the first run. A
passing check whose route was never asked whether it exercises the mechanism is
the number to distrust.

**The port draws entities for several frames after a menu before their first
update, and anything read at draw time is then uninitialised (B62).**
`Subtask_Init` wipes all 32 affine *sources*; the recompute flag is global
(`ui.c` raises it for slot 0), so `CopyOAM` rebuilds every slot from that zero
— and the lily pad's own `SetAffineInfo` does not run until **8 frames after it
starts being drawn**. `pa=pd=1` is 1/256 scale in 8.8, i.e. one texel across
the whole 64x64 box: a solid green square. Two savestates settle it — hardware
holds a live matrix both during the menu and at the black frame, so it never
presents that state. Fixed narrowly (a slot whose source is still zero keeps its
previous matrix); **the ordering itself is not fixed** and is the same family as
B59/B60. **Key every probe to `Port_Capture_Frame()`** — two instruments here
counted `PresentFrame` calls instead, went one frame out, and produced
"identical inputs, different output". And **ask for state, not pictures**: the
decisive savestate arrived with an apology for being mistimed and had a fully
black screen, which does not matter when the answer is in OAM.

**A one-frame fault is invisible to a gate that samples on a stride, and the
tileset publication was one frame late at every swap (B59).** The per-tile
selection was published from `Port_MapSource_Update`, which runs *before* the
frame's game logic — and `LoadGfxGroup` runs *in* that logic, so for one frame
the renderer held "group 4 is resident" while VRAM already held group 5. Only
the periphery shows it, because the centred 240x160 does not reach those rows
until later. **Both dense 176-frame route diffs came back 0 and neither was
coverage**: the fault lasts one frame and the diff samples every 72nd. Dump the
frames the mechanism *fires* on instead — `[groups] frame N slot S: A -> B`
enumerates them from a first run — and the route turns out to contain another
instance. The detector for this shape is cheap: with `TMC_DISABLE_OBJ` and
`TMC_DISABLE_BG0`, a scrolling scene's consecutive-pair residual under a pure
vertical shift has **median 0**, so one bad frame stands out at 2266 against
zeros. **Swept: seventeen recordings, 384 swap events — Hyrule Town changed 27
frames, Minish Village 1.** The asymmetry is `residentGroup`: Hyrule Town passes
the group it just loaded, so its region offsets flip on every swap, while Minish
Village passes `PORT_TILESET_NO_RESIDENT`, which no region can equal, so its
offsets are fixed at declare time. Its one hit is 5 px on a gap tile — the
*fallback*, which does follow the live group. **Festival town (`0x15`) is the
hole**: no recording reaches it, a warp provokes only two swaps, and four frames
is not coverage. Note `town_wall_glitch` and `town_grpahics_glitch_2` have no
`quit` line, so replaying them needs `--exit-frame` or they never end.

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

**The fallback clip's exemption is about *tiled overlays*, not about BG3
(B37).** A layer with no map source is clipped to 240 and centred, which is
right for a room map caught mid-transition and wrong for a repeating pattern,
where the wrap is what covers the viewport. BG3 is exempted wholesale because
that is where such overlays usually live — but Mt Crenel's weather manager
takes **BG1** from the room's top map layer and fills it with a rain sheet, and
the clip caught it (`cols 40..279` of 320). A layer says what it is with
`Port_MapSource_DeclareTiledOverlay`; lifetime is the room, and handing the
layer back needs no undeclaration because it regains a map source and the clip
only applies without one.

**An OBJ in mode 1 is a blend first target whether or not BLDCNT says so
(B38).** VirtuaPPU never read OAM attr0 bits 10-11 at all, so semi-transparent
sprites composited opaque — the vapour wisps and the steam on Mt Crenel.
`steam.c` sets `spriteRendering.alphaBlend = 1` and leaves BLDCNT at
`0xbd << 6` = `0x2F40`, whose **first-target field is empty**: read the register
alone and nothing blends. `TMC_BLEND_TRACE`'s `tgt1=0x00` beside a non-zero
`semi_objs` is that signature. **A global renderer change needs more than the
11-waypoint gate** — the dense 177-frame route diff is what covered it.

**A world-view BG3 overlay is exempted from the centring clip, and the
exemption is a claim about a class (B21).** The rule leaves BG3 unclipped
because the overlays it was written for — hole, cloud, weather, steam, POW —
are *tiled* and *world-locked*, so wrapping the screenblock past 256 px is what
covers a wider viewport and adding `UI_CENTER_DX` would misalign them. The
light shaft is neither: `bg3.xOffset` is the constant `0x10` and its map is
blank across two thirds of its columns, so the wrap brought that blank end into
the columns past 239. It was never short — it was showing the wrong 80 px of
itself. An overlay now declares itself with
`Port_MapSource_DeclareBg3ScreenAnchor` and gets the clip pinned to an edge.
**That declaration lasts until BG3 goes off or the room changes, not until the
declaring handler stops running (B35)** — the two come apart: a light-ray
fade-out sets `unk_21` to the *trigger* type, so `gUnk_08107C48` dispatches to
`nullsub_494` from the first frame of an eighty-frame fade, and a text box
suspends the managers outright. Declaring per frame and clearing per frame made
the band jump on both. Silence therefore means "unchanged", so a state wanting
the unclipped rule back says `PORT_BG3_ANCHOR_NONE` rather than going quiet. **Pin it to the room's right edge, not the viewport's**: the two rooms
that run this handler are `Area_MinishWoods` room 0 (1008 px wide, fills the
screen) and `Area_MinishHouseInteriors` room 9, the barrel minish house, which
is **240x368** and therefore centred with 40 px of border — pinning to the
viewport hangs the band out into it.

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

**A hand-scrolled layer's window is sized for the GBA's screen too (B32,
B34).** MinishPaths' parallax layers keep a fine `yOffset` and re-point
`subTileMap` every 64 px; the block they index is 32 tiles, so the screen must
fit in `256 - yOffset` and at 240 rows it does not. Re-base on a smaller step.
**`lightRayManager.c` had the identical shape and went another eleven days**
because B32 was fixed where it was reported instead of swept by mechanism —
`grep` for `& 0x3f` beside a `/ 0x40` on a `subTileMap` and the pair is the
whole population. Note the consecutive-pair shift test that settled B32 scores
**zero on a uniformly wrapped layer**: it sees the re-point, not the wrap. Ask
whether `yOffset + VIEWPORT_HEIGHT <= 256` instead. The
horizontal twin needs `xOffset + 320 <= 256` and cannot be fixed this way at
all. **And a scene with parallax cannot be judged by whole-frame diffs** —
three layers at three rates means no alignment exists; `TMC_DISABLE_BG1/2/3`
leave one layer on, and then "did it scroll cleanly" has an exact answer: zero
residual under a pure shift on every consecutive pair.

**A per-frame declaration and a latched one fail in opposite directions
(B30, B31 vs B35).** "Re-declare every frame" is not automatically the safe
choice — it keys the lifetime to whatever makes the call, which may stop long
before the thing being described ends. Ask what *event* ends it and watch for
that instead.

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

**`RestoreGameTask`'s post-menu buffer push must skip any BG no map layer is
bound to (B39), not just an affine room's (B25).** The port copies
`gBGxBuffer` into VRAM after a menu because the GBA mechanism does not fire —
and B25's own comment already states the general rule: *the room handler is
re-run on the way out and reloads them correctly, these copies then overwrite
them*. The affine test is one instance of it. Mt Crenel's weather manager
hands BG1 to a rain sheet (`gMapTop.bgSettings = 0`, tiles straight to VRAM),
`gScreen.bg1.subTileMap` still points at the room's top tilemap, and the copy
wrote that over the rain. Ask `gMapBottom/gMapTop.bgSettings == &gScreen.bgN`.
**And note the canonical route's eleven waypoints contain two menus and no
frame of the gameplay after one** — this path is covered by the dense route
diff, not by the gate.

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

**A symbol's length is not its extent, and a list may borrow the next symbol's
terminator (B48).** `LoadRoomEntityList` walks 16-byte records until
`kind == 0xFF`; twelve room-property lists contain no such record at all —
every beanstalk room, Temple of Droplets 51, Dark Hyrule Castle 2. On hardware
the walk steps past the symbol and stops on the *next* one's
`entity_list_end`, which contiguous ROM makes a defined read. Per-symbol heap
buffers have no next symbol, so the walk hit allocator slack and reached
`AppendEntityToList(ent, dat->flags & 0xF)` with a garbage nibble — out of
bounds on a 9-element `gEntityLists`, and a **crash in every beanstalk in the
game, at both sizes**. `extend_room_property_to_terminator` grows such a blob
to the terminator hardware would find. Ask of extracted data not *is this
symbol complete* but *does it contain everything the consumer will read* — and
sweep the whole population by mechanism: the report named one room and the
mechanism named twelve. Note the extractor's `scan_pointer_table_count(…, 64)`
invents rooms past each area's real table, so an exhaustive sweep must be
filtered against the decomp's declared room lists or it reports ten times the
real population.

**When the crash site moves between identical runs, stop reading it (B48).**
Three replays of one script faulted in three places, because the segfault is
downstream of a memory corruption rather than being the defect. Break on the
first operation that is already wrong — a conditional breakpoint on the
out-of-range index, not on the fault — and the backtrace stops moving.

**A decompiled symbol may be a window onto a bigger contiguous block, and
the port only allocates what the symbol declares (B36).** Mt Crenel's weather
manager cross-fades the summit against `gPalette_549 + 0xD0` — 13 palettes
past it — which is an address only because the GBA linker laid
gPalette_549..gPalette_574 out sequentially. `port_linked_stubs.c` allocates
the full 26-palette block and its comment says `port_rom.c` fills it from
`gGlobalGfxAndPalettes`; **it never did**, so both sides of the mix read zeros
and the summit rendered as sprites over black — at 240x160 too. **A comment
claiming another file does something is a claim, not a fact**: grep for the
write. Palette N is at `N*32` in that blob, the same arithmetic
`LoadPaletteGroup`'s hardware path uses, so no new offset is needed.

**Three explanations for "black except the sprites", and the frame separates
none of them**: the layer draws nothing, it draws black, or it is darkened
afterwards. `TMC_MASK_BG<n>` kills the first in one run (it bypasses palette
*and* blend), `TMC_BLEND_TRACE` kills the third by reading BLDCNT rather than
inferring it, and per-row palette counts localise what is left. **And when the
writer is still unaccounted for, a watchpoint costs one run** — B36's took one.

**A symbol named for an address and a struct field can be the same bytes, and
the port gives each its own storage (B50).** `gUnk_020342F8` *is*
`gArea.filler6` on GBA — `gArea` at `0x02033A90`, `filler6` at offset `0x868`
— and the decomp spells it both ways by file. `port_linked_stubs.c` allocated
a separate `u8[0x100]`, so `delayedEntityLoadManager.c` set the delayed-entity
bits in one object while `whirlwind.c` and `cutsceneMiscObject.c` read another
that nothing ever wrote, and every gated entity deleted itself before its Init:
all 44 conditional whirlwinds in the game plus Cloud Tops' 10 clouds. Nothing
warns — both halves compile, link and work on their own object. **The ROM's
literal pool settles it in seconds**: `0x02033A90` appears 196 times and
`0x020342F8` five, and the two spellings' bit arithmetic lands on the same
byte and bit. Alias such a symbol (`#define` onto the field, as `common.c`
already does for `gUnk_02035542` → `gzHeap + 2`) rather than giving it storage.
Same family as B36 and B29. **A `gUnk_0203xxxx` array in
`port_linked_stubs.c` whose address falls inside another object's range is the
thing to grep for.**

**A report that arrives with its own working control is worth answering in the
order it hands you (B50).** "Mt Crenel's tornados are visible, Lon Lon Ranch's
are not" ruled out the object, its sprite, palette and animation before any
code was read: same object, two spawn paths, and only the *conditional* one
was broken — Mt Crenel's are plain `object_raw` with `health == 0`, so the gate
never runs. Ask what differs between the working and broken instance before
asking what the broken one does.

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

**Every capture runs `SDL_VIDEODRIVER=dummy`, and that is a blind spot as well
as the thing that makes replay deterministic (B44).** The dummy driver creates
no window, no renderer and no textures, so anything about real GPU resources —
notably teardown — is invisible to the whole suite and to the gate. Closing the
window exits through `exit(0)` inside the frame loop, which for a long time
skipped all five of `main()`'s shutdown calls; both exit sites now go through
`Port_Shutdown()`. **Never run a windowed build to investigate**: it opens on
the maintainer's desktop.

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

**Do the copy after the commit, and check the binary's own stamp against it.**
The 320x240 play build handed over on 2026-08-26 was compiled at 22:17 and
copied at 22:42, while the B55 commit it was supposed to carry landed at 22:46
— so the maintainer played a binary without that cycle's fix, and an A/B
against it during B56 disagreed with a fresh build for reasons that had nothing
to do with the change under test. The startup line prints `__DATE__`/`__TIME__`;
compare it with `git log -1 --date=iso` before believing any measurement taken
against an installed play binary.

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
- **A capture harness is single-use.** The game rewrites `tmc.sav` as it
  plays, so a second run in the same directory starts from the first run's
  mutated save and goes somewhere else. B53 lost a round to a SIGSEGV that
  reproduced once and never again for exactly this reason — neither run was
  meaningful. Copy the save in fresh for every run, not once per directory.
- **A `warp` needs a save in the harness.** It retries until `TASK_GAME`, and a
  temp dir with no `tmc.sav` never gets there — the run sits on the name-entry
  screen and every `dump` after the warp photographs *that*. Two such runs at
  two viewport sizes agree perfectly and mean nothing; B48's first cross-size
  A/B scored 0.00% that way. Copy a save in, and `dump` the frame before the
  warp so the comparison has a witness that it reached the room.
