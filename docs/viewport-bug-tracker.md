# Viewport expansion — bug tracker

Bugs found across both viewport milestones. B1–B9 came from the maintainer
playtesting the 320×160 build; B10–B12 from sweeps during Milestone 2; B13–B22
from the maintainer playtesting 320×240, most with recordings; B22–B25 from the 2026-08-08 barrel and lily-pad sessions, and B26 from a 2026-08-09 Hyrule Town report. B34, B46 and B49 were found by instrument rather than by report. B16 and B17 were
reported from the Android build — which is the same viewport on other hardware,
and neither turned out to be a platform bug.

**Status: Milestone 1 signed off 2026-07-30. Milestone 2 is functionally
complete — see `docs/milestone2-status.md`.** Fifty-one of the fifty-six
bugs are closed: forty-eight fixed with a root cause and evidence, B4 closed as
**no longer observed** rather than diagnosed, and B52 closed as a deliberate
divergence from hardware rather than as a defect. **There is a hardware oracle
now** — mGBA runs headless on this machine and its savestates carry a frame's
state and picture together; `tools/mgba/README.md` is the technique, and B45
and B47 are what it produced. **B41, B42, B46, B47 and B49 are
open**; B51's port-side struct fix is deferred behind a save migration, and
B52 is closed as the first **intentional divergence** from hardware — see
`docs/hardware-divergences.md`, which is where that class is recorded from now
on. B41 and B42
were reported 2026-08-20 without recordings, both in story-gated cutscenes the
scripted tester cannot reach; **B45** arrived 2026-08-22 with a recording, is
**closed 2026-08-22** against mGBA, which turned out to run headless here and
to write savestates carrying a frame's state and picture together. **B46**
was found by inspection while ruling a mechanism out of B45 — the second bug in
the tracker found by instrument rather than by report, after B34; **B49** is
the third, found 2026-08-23 while A/B-ing B48's fix against the shipping size.
**B48** arrived 2026-08-22 as "the game hangs climbing the beanstalk" and was a
SIGSEGV in every beanstalk in the game, at both sizes. **B43 closed 2026-08-22**, once the maintainer's 240x160
recording made it reproducible on the shipping build: it was one port-only line
in `CreateVaatiApparateManager`, and it owned #93 as well. **B36 is not a viewport bug at all** — it
reproduces at 240x160 and was live in the shipping build; it arrived as a
playtest report during this work and is tracked here because that is where
the reports come. **B21 closed 2026-08-20**, after nearly two
weeks recorded as unfixable: the blocked routes were all attempts to make the
layer *reach* further, and the fix was to stop it wrapping and pin it to the
edge it belongs to. B34 was found the same day, in the same layer, by the
instrument built to look at it.

**Nine of these were live in the shipping 240×160 build or through all of
Milestone 1** — B11, B12's horizontal half, B13's horizontal half, the iris
veto, B23's angle-gate bypass, B25's post-menu buffer copy, B28's truncated
room properties, B48's unterminated ones and B56's stale HBlank DMA channel. The expansion exposed them; it did not cause them, and B23 and
B25 were only found because the 320×240 build made the rolling barrel worth
playing. This document stays the authoritative record of what the expansion
actually did to the engine — which includes distinguishing that from what it
merely revealed.

**B28 is the first of these that the viewport had nothing to do with at all.**
It is a port data bug: the same picture and the same walk-through door at both
sizes, reported only because someone was playing the 320×240 build. Kept here
because this is where the port's bugs are tracked, not because it is a
viewport defect.

**B48 is the same again and the worst of them** — a crash rather than a
picture, in every beanstalk in the game, at both sizes. Like B28 it is
extracted data disagreeing with ROM, and like B28 it was reported only because
someone was playing the 320×240 build.

**B29 looks identical from the outside and is the opposite.** It too reproduces
at 240×160, but it is a Milestone 1 regression: Spike 6 relocated `gBG0Buffer`
and the relocation is unconditional, so the damage is the same at every size.
"Present at the shipping size" answers *"did the expansion's geometry cause
this"*, not *"did the expansion cause this"* — the first thing the 240×160
build is for is a narrower instrument than it looks.

**B22 is the fourth appearance of one assumption — that the screen is the
room.** B5, B15 and B17 were the first three, all horizontal. B22 is the
vertical case and the first to break *gameplay* rather than rendering: a room
that is exactly viewport-sized on hardware lets camera-relative and
room-relative coordinates be written interchangeably, and only a viewport
change tells them apart.

Anything at 240x160 is a release blocker. Anything at 320x160 blocked the
Milestone 1 exit criteria but not the shipping build, which is still
GBA-native. Builds are named WxH throughout: 240x160 (shipping), 320x160
(Milestone 1), 320x240 (Milestone 2).

## Status

| ID | Summary | Status |
|---|---|---|
| B1 | Save/erase popups' text garbled | **Fixed** (verified 320x160) |
| B2 | Legend artwork repeats past x=240 | **Fixed** (verified 320x160, in situ) |
| B3 | Zelda-walking cutscene not full width | **Fixed** (verified 320x160, in situ) |
| B4 | Smith-room sprites/layers wrong at first dialogue | **Closed** 2026-08-02 — no longer observed in any build (maintainer) |
| B5 | Interior room-to-room scroll glitches | **Fixed** 2026-08-02 — reproduced from a recording; slide replaced by a fade above native size |
| B6 | Zelda sprite in the left border | **Fixed** (confirmed by maintainer) |
| B7 | Camera-pan softlock in Hyrule Town | **Fixed** (confirmed by maintainer) |
| B8 | Large heart offset left of the centred HUD | **Fixed** (verified 320x160, pixel-exact vs 240x160) |
| B9 | Legend card artwork dimmed right of a vertical seam | **Fixed** (verified 320x160, pixel-exact vs 240x160) |
| B10 | BG3 gameplay overlays clipped and misaligned | **Fixed** (found by sweep, not by playtesting) |
| B11 | Circular-window transitions render as a near-black screen | **Fixed** (Milestone 2 Spike 9; was live at 240x160) |
| B12 | Entities culled in a band at the far viewport edge | **Fixed** (Milestone 2 Spike 11; horizontal half was live through Milestone 1) |
| B13 | Town NPCs pop in and out inside the visible frame | **Fixed**, confirmed by maintainer 2026-08-01 (reported with a recording; horizontal half was live through Milestone 1) |
| B14 | UI screens' side borders forced black while their top/bottom borders show the backdrop | **Fixed** 2026-08-02 |
| B15 | Room furniture lit against black through a door/stair fade | **Fixed** 2026-08-02 |
| B16 | Softlock entering the smith room after a scrolling transition | **Fixed** 2026-08-05 — reported from Android, reproduced on desktop once an out-of-bounds read stopped masking it |
| B17 | Minish house interiors render as sprites over black | **Fixed** 2026-08-06 — third instance of the screenblock being unable to cover 320 px; needed the tile mutators to maintain the degraded map, not just a relaxed predicate |
| B18 | Pause map detail view shows only the top of the map | **Fixed** 2026-08-06 — the per-scanline BG3 curtain's band was still in 240x160 rows; the only per-scanline table on a UI screen |
| B19 | Segfault entering a room narrower than the viewport | **Fixed** 2026-08-06 — a `u32` local made a pointer offset unsigned, so a negative camera offset wrapped to +4.29e9. Reported from Android with a recording; reproduced on desktop first try |
| B20 | Gameplay flashes at 240x160, offset, across a pause transition | **Fixed** 2026-08-06 — the centring clip changed several frames before the picture did; it now changes only on a black frame |
| B21 | Minish Woods light shaft ends 80 px short of the right edge | **Fixed** 2026-08-20 — the shaft is screen-anchored, not world-anchored, and the port left a world view's BG3 unclipped so the *tiled* overlays could wrap across the wider viewport. This one wrapped its blank left end into the columns past 239. Now clipped to the authored width and pinned to the room's right edge |
| B22 | Rolling barrel interior: doors out of reach, room spills past 160 rows | **Fixed** 2026-08-08 — the player pin measured the barrel's midline from the camera, not the room; 40 px of error at 320x240. Rim sprites in the border left open as a costed decision |
| B23 | Barrel's drawn hole/doors rotationally apart from the exits that fire | **Fixed** 2026-08-08 — the port's `#ifdef PC_PORT` angle-gate bypass (predates the expansion, identical at 240x160) removed on the maintainer's decision. Hardware gate restored and verified landable |
| B27 | Town scenery in the outer 40 px drawn from a non-resident tileset | **Open, fully diagnosed, plan written.** The residual B26 cannot reach: one tileset is resident and the viewport now shows more world than any single tileset covers. Not a selection problem — B26's rule is provably optimal. Fix is to keep both resident in enlarged emulated VRAM and choose per tile; **`docs/town-tileset-residency.md`** is a step-by-step plan with every measurement it needs |
| B26 | Hyrule Town scenery drawn from the wrong tileset past a camera threshold | **Fixed** 2026-08-09 — the tileset managers pick a gfx group by *first* region touching the screen, and 80 extra rows widen the band where two regions match from 32 px to 112 px. Now picks the region covering most of the screen; unchanged at 240x160 |
| B25 | Rolling barrel comes back as noise after a pause | **Fixed** 2026-08-08 — a port-only forced buffer→VRAM copy wrote text tilemaps over *both* of the room's own maps, BG2's affine one and BG1's grain layer. Reproduces at 240x160, so pre-existing. Frame is now pixel-identical across the pause |
| B24 | Riding a lily pad through a room scroll strands the player outside the room | **Fixed** 2026-08-08 — the vehicle's carry state (`LilypadLarge_Action3`) exits on `reload_flags == 0`, which the faded path leaves true for the 32 frames it defers the apply, so the pad exited before the room changed and never carried anyone. Found from a second recording |
| B28 | Lon Lon Ranch's locked door lets the player walk in; the door beside it draws as an open black doorway | **Fixed** 2026-08-17 — **not a viewport bug; identical at 240x160.** Asset extraction truncated every room-property blob at the first ROM pointer embedded in it, so the house-door list was half a record long and the engine read the rest off the end of the buffer. Four properties across three areas were short; all are whole now |
| B33 | Minish Village's blue house changes tileset when the camera crosses a threshold | **Fixed** 2026-08-20 — a tile in an authored *gap* takes the group the engine loaded, which is right inside the GBA's screen and arbitrary outside it. Peripheral gap tiles now take the group of the region they adjoin; a guard rect keeps the inside byte-identical |
| B34 | Light shaft's lower rows show the top of its own block | **Fixed** 2026-08-20 — the vertical twin of B32 in a different manager: a 64-px re-base leaves `yOffset` up to 63, and a 240-row screen needs the block to cover `yOffset + 240` of its 256. Re-based on 16 px. Found while measuring B21, never reported |
| B35 | Light rays jump left when they fade, or when a text box opens | **Fixed** 2026-08-20 — B21's anchor was declared per frame and cleared per frame, so it died whenever the *handler* stopped ticking rather than when the *overlay* ended. A fade-out dispatches to a null handler on its first frame; a text box suspends the managers. Anchor now lives until BG3 goes off or the room changes |
| B36 | Mt Crenel summit renders black apart from sprites | **Fixed** 2026-08-20 — **not a viewport bug: reproduces at 240x160.** `gPalette_549` is 26 contiguous palettes on hardware and the weather manager cross-fades against `gPalette_549 + 0xD0`; the port allocated the block but never filled it, so both sides of the mix read zeros and the summit's 13 terrain palettes went black. `port_rom.c` now loads it |
| B37 | Mt Crenel rain sheet fills only the centred 240 columns | **Fixed** 2026-08-20 — the weather manager takes BG1 from the room's top map layer and fills it with a tiled rain sheet; with the map layer off the map source is refused and the fallback clip confined it. A tiled pattern wants the screenblock wrap, exactly as BG3's overlays do. The layer now declares itself |
| B38 | Vapour wisps and steam render opaque white | **Fixed** 2026-08-20 — **not a viewport bug.** VirtuaPPU never read OAM attr0 bits 10-11, so OBJ mode 1 (semi-transparent) was ignored. On hardware such a sprite is a blend first target regardless of BLDCNT, which is why Mt Crenel's `bldcnt=0x2F40` has an *empty* first-target field and still blends |
| B39 | Rain layer is garbage after returning from the pause menu | **Fixed** 2026-08-20 — **pre-existing, not caused by B37**: 13,567 garbage px in the centred 240 before that fix, 13,540 after, which only widened it into the borders. The port's post-menu `gBGxBuffer`→VRAM copy wrote the room's top tilemap into BG1's screenblock, over the reload the re-run handler had just done. It now skips a BG no map layer is bound to — B25's exclusion, one step further |
| B40 | Cave of Flames minecart never emerges: softlock | **Fixed** 2026-08-20 — B24's defect in the file B24's own fix predicted. `Minecart_Action5` ends its carry state on `reload_flags == 0`, which reads true for all 32 frames of a deferred faded transition, so the cart hands the camera back before the room changes and the ride never completes. Guarded with `ScrollTransitionIsPending()`, as the lily pad already was |
| B41 | White flash after a boss's element award covers only part of the screen | **Open, awaiting a recording.** `SetFillColor`'s flat fill is *ruled out* — measured at 100% of 320x240 with `TMC_FILL_PROBE=1`. Suspect is `WHITE_TRIANGLE_EFFECT` (spawned by `bossDoor.c:215`), whose per-scanline window rasteriser clips with explicit `MAX_X_COORD = 240; MAX_Y_COORD = 160` |
| B42 | Table behind Vaati disappears when he warps out | **Open, no lead.** Elemental Sanctuary flashback. `vaatiAppearingManager.c` drives the same window/BG3 machinery as B41's suspect (`sub_0801E104`, `DISPCNT_BG3_ON`), which is suggestive and not evidence. Needs a recording |
| B43 | Vaati takeover cutscene ends on a permanent black screen | **Fixed. NOT a viewport bug** — reproduced on the 240x160 play build from a recording made there, and on the pre-session baseline. `CreateVaatiApparateManager`'s `DeleteManager` call is a documented no-op on hardware (its argument is a ROM function pointer); a port commit re-pointed it at `gArea.transitionManager`, which during the takeover is a stale entity at the head of list 6, so `UnlinkEntity` writes `gEntityLists[6].first` back onto the overworld chain and the takeover orchestrator — never deleted, never unlinked — stops being reached. Restoring the hardware no-op fixes this, #93, and the missing fade-in; the #93 watchdog is removed |
| B44 | Closing the window skips every SDL teardown step | **Fixed** 2026-08-20 — **not a viewport bug.** Window close sets `gQuitRequested` and `VBlankIntrWait` calls `exit(0)` from inside the frame loop, so `AgbMain` never returns and main()'s five teardown calls never run: the window, its renderer and textures, and the audio device stream stayed live, with the SDL audio callback thread running while atexit handlers and static destructors went off. Routed through one idempotent `Port_Shutdown()` |
| B45 | Link vanishes entirely on entering Castor Wilds mud | **Open, diagnosed. NOT a viewport bug** — the decisive state is byte-identical at 240x160. `OBJECT_70` puts the player at OAM priority 3 (behind BG2's opaque ground, which is correct and matches the ARM), so what stays visible must be `OBJECT_70` itself; it emits twelve OAM entries a frame and every one points at OBJ VRAM **tile 133, which is all zeros**. Its definition carries `gfx_type` 2 — *load nothing, use fixed tile 133* — **Fixed.** That tile is blank on hardware too, because **OBJECT_70 is a priority window, not a mask**: the OBJ layer composites at the priority of the *last covering sprite in OAM order*, opaque or not, so twelve blank priority-2 sprites lend the player's priority-3 sprite a tie against the priority-2 ground and he shows through the mud, clipped from the bottom as he sinks out of the rectangle. VirtuaPPU dropped transparent OBJ pixels outright. Two mGBA savestates pin "last" rather than "best": the swamp and the name-entry glyph give opposite answers to the same shape. 11/11, audit clean, dense 173-frame route diff identical |
| B46 | The wading overlay never draws in shallow water | **Open, found by inspection while ruling it out of B45.** `ProcessEntityForDraw`'s feet overlay (ROM table `0xB2B58`) uses `(spriteSettings & 0x30) >> 2` plus `frame << 1` where the ARM uses `(ss & 0x30)` as a *byte* offset plus `frame * 2`, i.e. `row + frame/2`. Shallow water's frames are 32..38, so the port's index is 64..76 against an `idx < 16` guard and it never draws; tall grass draws the wrong entry. The ROM table has 36 pointers, the port copies 16 |
| B47 | The port's `tmc.sav` will not load in mGBA or on hardware | **Open, diagnosed.** `port_save.c` skips the EEPROM serial protocol and stores each 8-byte block in the game's in-memory order; hardware stores the wire order, which is the reverse. Confirmed against a save the real game initialised under mGBA. The game reads its signature back scrambled and offers a new file — symmetric, so emulator saves do not load in the port either. `tools/mgba/savconv.py` converts either way; fixing `port_save.c` needs a migration for existing saves |
| B32 | MinishPaths parallax grass pops in instead of scrolling | **Fixed** 2026-08-20 — the manager re-bases its layers' 32-tile screenblock every 64 px, and `yOffset + 240` runs past the block's 256 px. Re-based on 16 px instead; both layers now scroll with zero residual on every frame pair |
| B31 | Every Hyrule Town tileset slot is undeclared from room entry until its first camera swap | **Fixed** 2026-08-20 — the manager's init reset ran a frame *after* OnEnterRoom had declared the room's slots and wiped all three. Only a group change re-declares, so the periphery drew from the centred screen's group until the camera crossed a threshold |
| B30 | Scenery in the outer 40 px drawn from the previous room's tileset until the camera moves | **Fixed** 2026-08-18 — the residual B27 case. A slot whose regions the centred 240x160 never touches is never loaded, and `LoadGfxGroup` is the only thing that declares a slot, so it had no per-tile answer at all. Declared now with no resident group |
| B29 | The stylized area-name banner never appears on entering a new area | **Fixed** 2026-08-18 — a Spike 6 regression the gate cannot see. Relocating `gBG0Buffer` out of `gEwram` left ROM `Font` blobs, whose `dest` is a raw GBA address, drawing the banner into dead memory. The canonical route spawns five banners per run and samples none of them |
| B48 | Climbing any beanstalk crashes the port | **Fixed** 2026-08-23 — **not a viewport bug: a SIGSEGV, not the reported hang, and size-independent.** Twelve room-property entity lists carry no `entity_list_end` of their own and borrow the *next symbol's* on hardware, which contiguous ROM makes a defined read. Each extracted into its own heap buffer, the walk ran into allocator slack and appended entities to `gEntityLists[11..14]` of 9. All ten beanstalk rooms, plus Temple of Droplets 51 and Dark Hyrule Castle 2. The extractor now extends such a blob to the terminator hardware would find; `kExtractorFormatVersion` 2 |
| B49 | Beanstalk-top rooms' sky renders differently at 320x240 | **Open, measured, undiagnosed.** Found by instrument while A/B-ing B48's fix. `Area_Beanstalks` room 0 differs from the centred 240x160 sub-rect by 1352 BG-only px (3.52%) in the sky rows 0-46 and 120-159; the climb room next door is 0. Static layer, same figure on all twelve candidate frames, so not animation phase. Suspects are B32/B34's screenblock height and B21/B37's BG3 clip; neither tested. Cosmetic |
| B50 | Every conditional whirlwind is invisible and inert | **Fixed** 2026-08-23 — **not a viewport bug: reproduces at 240x160.** `gUnk_020342F8` *is* `gArea.filler6` on GBA (`0x02033A90 + 0x868`), but the port gave it separate storage, so the delayed-entity manager set bits in one object and `whirlwind.c` / `cutsceneMiscObject.c` read another that was always zero — every gated entity self-deleted before its Init. 44 whirlwinds and 10 Cloud Tops clouds, including all 40 of Cloud Tops'. Aliased with a macro, as `gUnk_02035542` already is |
| B51 | Every port save is one byte out of layout for `flags` onward | **Tool fixed** 2026-08-23; **port struct still open.** `KinstoneSave` sums to 327 bytes where the GBA layout documented in `include/save.h` leaves 328, so the port writes `flags[0x200]` and the `dungeon*` arrays one byte early. Name/stats/inventory/kinstones read correctly and every story flag is shifted — on hardware Link loses Ezlo and world events un-do. Hidden by a `u32` alignment hole that makes `sizeof` coincidentally right. `savconv.py` realigns and re-checksums; fixing the struct needs a save migration |
| B52 | Beanstalk base draws solid magenta | **Closed 2026-08-23 as an intentional divergence** (`docs/hardware-divergences.md` D-1). **Not a faithfulness bug:** Hardware has the same placeholder: OBJ palette 5 and BG palette 3 are both 12/16 `0x7C1F` in an mGBA savestate of the same room, and `LoadRoomTileSet`'s BG3→OBJ5 copy is faithful. The base sits at y=208..237, outside the GBA's centred y=40..199, so it is never on screen on hardware — the periphery again (B26/B27/B30/B31/B33). OBJ palette 5 is now loaded explicitly from each beanstalk's source-room ground palette (via `gUnk_080B4410`), deterministic on any entry path, expanded viewport only |
| B53 | Syrup never reacts to the mushroom; it snaps back and she offers potion | **Fixed** 2026-08-24 — **not a viewport bug: identical at 240x160.** Syrup's mushroom is an `ItemForSale` (`ITEM_QST_MUSHROOM` 0x38) and `ItemForSale_Action2` reads the interaction target with raw GBA offsets: `*(int*)(ptr+8)` is `InteractableObject.entity` on GBA but `customHitbox` in the port, where 64-bit pointers push `entity` to 16. `customHitbox` is NULL for nearly every interactable, so every A press while carrying a shop item took the cancel branch. Typed access under `PC_PORT` |
| B54 | The darknut fight crashes | **Fixed** 2026-08-26 — **not a viewport bug: the 240x160 build segfaults on the same recording.** `EnemyDetachFX` NULLs a dying darknut's child, so a sword slash that outlives its owner reaches its first update with `parent == NULL` — and `DarkNutSwordSlash` dereferences it twice *before* the `parent == NULL` check two statements later. On GBA address 0 is BIOS and returns open bus; here it is unmapped. Deleted before the init block under `PC_PORT`, as `rupeeLike.c` already does. Swept: population of one |
| B55 | Reversing during a room scroll strands the player outside the new room | **Fixed** 2026-08-26 — **320x240 only**, the mechanism is `VIEWPORT_SCROLL_FADE`. The faded transition is queued and applied 32 frames later, and Link keeps walking: reverse in that window and the commit runs from where he got back to (measured, x=2501 at queue → 2531 at commit), so `Scroll2Step`'s nudge lands him *outside* the room he is entering, where no edge transition fires. The third thing the deferral loses after his facing (B16) and the camera target (B24); his position is now carried across too, skipped when a vehicle owns the camera |

---

## B1 — save/erase popup text garbled *(fixed)*

File-select "Saving file…" / "Erasing file…", and the pause-menu save popup.
Text rows interleaved; frame drawn correctly.

**Root cause.** Not the message layer, which is where I looked twice. It was
the *shared text renderer*: `sub_0805F67C` (`src/text.c`) writes a
character's top tile at `param_1[0]` and its bottom tile at `param_1[0x20]`,
because a character is two tiles tall and a row *was* 0x20 entries. With a
widened BG0 stride the bottom halves landed mid-row. Same shape in the
per-line advance `dest += 0x40`.

Three further row-size literals had the same defect and are now
`UI_BG0_ROW_BYTES`: `subtask.c:54,95`, `fileselect.c:747`,
`enterRoomTextboxManager.c:73,86` (the last had never been converted).

**Now moot by construction**: since the switch back to a 32-tile BG0
(`b47ec0cc`) `UiDestStride()` always returns `0x20`, so none of these sites
can disagree with the buffer again.

Repro: `scripts/bugs.script`, waypoint `B1_saving`.

## B2 — legend artwork repeats past x=240 *(fixed)*

Opening stained-glass narration: the artwork draws a second partial copy at
the right edge.

**Cause.** A layer still reading a 32-tile VRAM screenblock covers 256 px and
*wraps*; it cannot fill 320, so stretching it repeats its content.

**Fix.** The mechanical rule was already right — *a layer with no map source
is clipped to `DISPLAY_WIDTH` and centred*. What broke it was **where the
rule was being called from.** The B3 ordering fix moved `mapsource_bind_ui()`
inside `Port_MapSource_CamTrace()`, which is a *diagnostic*: it returns early
unless `TMC_CAMTRACE` is set **and** the room has just changed **and** the
task is `TASK_GAME`. That did put the call after the world bindings, but it
also meant the clips were never applied in an ordinary run at all — and it
took `virtuappu_mode1_set_obj_clip`/`set_obj_offset` and `sUiCentered` down
with them, since those are set in the same function.

The call now lives at the end of `Port_MapSource_Update()`, after the binding
loop: correctly ordered *and* unconditional. `CamTrace` is a pure diagnostic
again.

This is also why `TMC_LAYER_TRACE` printed nothing — the trace was inside the
same early-returning function. It works now, and reports a `clip_mask`
alongside `mapsrc_mask` so "which layer is it on, and did the rule reach it"
is answerable in one line. The legend runs as `SUBTASK_AUXCUTSCENE` with
`mapsrc_mask=0x6 clip_mask=0x9`: the artwork is on BG0/BG3 and is clipped.

**Verified 320x160:** all legend frames in `scripts/sweep.script` (2000–4500)
have single-colour border bands, and the whole opening sweep (2000–11750,
40 frames) has **0 columns that repeat at the 256 px wrap period**.

Repro: `scripts/sweep.script`, frames ~2000–4500. Measure with the
border-bleed check in `tools/capture/README.md` — but see the caveat added
there about backdrop colour.

## B3 — Zelda-walking cutscene not full width *(fixed)*

Reported first as centred-240-with-borders, then after a partial fix as
"left-clamped 240 with a discontinuous x>240 region".

**Two causes, both found.**

1. The "is this a 240-authored UI screen" test was `substate !=
   GAMEMAIN_UPDATE`, which classifies *cutscenes* as UI. Cutscenes are views
   of the world and must fill the viewport. Now discriminated on the subtask
   type (`gUI.lastState`): `PAUSEMENU`/`MAPHINT`/`KINSTONEMENU`/
   `FIGURINEMENU`/`LOCALMAPHINT` are UI; `AUXCUTSCENE`/`PORTALCUTSCENE`/
   `WORLDEVENT`/`FASTTRAVEL` are world. Cutscene subtasks call
   `UpdateScrollVram` (`subtaskAuxCutscene.c:85`, `subtaskWorldEvent.c:57`),
   so the special maps are live during them and may be map-sourced. **That
   relaxation is gated to expanded builds** — applying it at 240x160 changed what the
   shipping build renders (audit 0 → 179 136 mismatches).
2. **An ordering bug that made the diagnostics look like liars.**
   `mapsource_bind_ui()` applied its "no map source ⇒ clip" rule *before* the
   world layers were bound, so it saw them unbound and clipped the whole
   world to 240 — while `TMC_REJECT_TRACE` correctly reported them as
   `bound`. The UI pass now runs after the world bindings. Wide rooms went
   from 0 to **6400/6400 px** of world in columns 280–319.

   The first attempt at (2) fixed the order by moving the call into a
   diagnostic that almost never runs, which is what regressed B2. See B2 for
   the real fix; the 6400/6400 measurement was taken while nothing was
   clipping and did not distinguish the two.

**Verified 320x160, in situ:** the cutscene renders full 320 width with world
content edge to edge and Zelda correctly placed in world space
(`scripts/sweep.script` frames 4750–5750, right band 6286–6400/6400 px).

## B4 — smith-room sprites/layers wrong at first dialogue *(closed, not diagnosed)*

**Closed 2026-08-02 by the maintainer: no longer observed in any build.**

Recorded precisely, because this is the one entry in this document that closed
without a root cause. B4 was **never reproduced** — captures of that room with
dialogue rendered correctly at 320x160 (`scripts/bugs.script` waypoint
`B4_smith_dialogue`, and the smith-room frames in `sweep.script`), and the
report specified "the very first character dialogue", which the scripted run
never lands on. Four rounds of inferring from the prose produced nothing.

So there is no fix to point at and no mechanism to name. Two readings are
consistent with the evidence and nothing here distinguishes them:

- it was fixed incidentally by one of B10–B15, several of which moved layer
  binding, clipping and entity culling in ways that would plausibly cover it;
- or it was reported from a build with a defect that no longer exists for some
  other reason.

**What that means for anyone reading this later.** A closed-as-unobserved entry
is weaker than the rest of this document. If something in the smith room at
first dialogue ever looks wrong again, this entry is not evidence that it is a
new bug — reopen it rather than filing a fresh one, and get a recording, which
is the step that was never taken here and which resolved B5, B13 and B15 in one
pass each.

**Possible retrospective identification: B16.** Recorded as a possibility, not
a finding. B16 is a softlock in this same room, entered by scrolling, in which
**the player is absent from the first dialogue with Zelda and the Smith** —
which is one reading of "sprites wrong at the very first character dialogue".
The B4 report predates the B5 fade by two milestones, and the scroll transition
into that room existed the whole time, so the entry path matches. Against it:
B4 described a rendering fault, not a hang, and nothing in B16's mechanism
produces wrong *layers*.

Nothing here distinguishes them and B4 was never reproduced, so this is not a
claim that they are the same defect — only a note that the next person should
read B16 before concluding B4 was fixed by something else. Both a recording and
this identification are still missing, which is the same gap the paragraph
above describes.

## B5 — interior room-to-room scroll glitches *(fixed)*

Walking from one interior room into an adjoining one: visible glitching,
scrolling not smooth. **Reproduced 2026-08-02** from a maintainer recording,
after being deferred since Milestone 1 for want of one.

**The cause recorded here for two milestones was wrong**, and wrong in a way
that would have sent a fix to the wrong line. This entry said the map-source
predicate declines to bind because "the window blends two rooms, so
`scrollAction >= 2` is rejected". The trace says otherwise:

```
substate=2 ... sa=1 -> bottom=bound            top=bound            mapsrc=0x6 clip=0x1
substate=1 ... sa=2 -> bottom=substate!=UPDATE  top=substate!=UPDATE  mapsrc=0x0 clip=0x7
```

Substate 1 is `GAMEMAIN_CHANGEROOM`. `mapsource_reason` admits only
`GAMEMAIN_UPDATE` and, above native width, `GAMEMAIN_SUBTASK` — so the scroll
is refused one clause *earlier* than this entry claimed, and `scrollAction`
is never consulted. `sa=2` is right there in the trace and does nothing.

**The mitigation this entry claimed was applied did not exist.** It described
world layers and sprites being clipped to the authored width during a
transition, "giving a clean 240-wide slice with borders". The horizontal half
was real; there was no vertical half, and a world view was handed
`content_height = MODE1_GBA_HEIGHT`, so the rows above the room sampled a
screenblock that holds no valid data there — the striped band over the HUD in
the recording.

**Neither half was fixable by clipping, which is the finding that settled the
design.** Two probes, both reverted:

| probe | striped band | the void |
|---|---|---|
| let `CHANGEROOM` bind a map source | **gone** | 12.0% → 14.5% |
| complete the clip on both axes | **gone** | 12.0% → 12.5% |

A map source renders one room; a slide is two rooms at once. The screenblock
is 32x32 tiles = 256x256 px against a 320x240 viewport, so mid-slide there is
genuinely no tile data for much of the frame. That meets the condition on the
maintainer's standing preference, recorded at Milestone 1 sign-off: *a fade
would be acceptable, and preferable, if the borders cannot contain the
adjacent room.* They cannot.

**Fix: above native size the slide is replaced by a fade** — see
`VIEWPORT_SCROLL_FADE` in `include/viewport.h`. The outgoing room dims whole
to black, the room swaps unseen, the incoming room fades up complete and
centred. Modelled on a maintainer reference recording of the transition the
engine already uses for doors. Gated, so 240x160 still slides.

Three things had to be true at once and each cost a round to find:

- **`FADE_INSTANT` is load-bearing and misnamed.** `FadeMain` only keeps a
  fade alive if `type` carries one of `FADE_INSTANT`/`MOSAIC`/`IRIS`; with
  none set, `active` is cleared on the first update and nothing renders. It is
  the palette-fade handler, which is why `cutscene.c` always passes it.
- **The whole commit has to be deferred, not just the reload.** Deferring only
  the reload changed nothing: `sub_0807BD14` updates `gRoomControls.room` at
  the commit point and `Scroll2Sub0` then refreshes VRAM against the *new*
  room, so the tiles were swapped out from under the fade before the reload
  ran. The transition is now queued and applied once black.
- **The fade in cannot be started from `Scroll2`.** `GameMain_ChangeRoom`
  refuses to finish while any fade is active and the room cannot render until
  it finishes — starting it there deadlocks the two and the fade in reveals
  the same half-drawn frame. It fires on completion instead, keyed on
  `gRoomVars.didEnterScrolling`.

**Evidence.** Replaying the maintainer's recording: **0 frames of the
transition show a partially drawn room**, against 20 before. The outgoing room
holds 98.7% fill while dimming, black for ~6 frames, the incoming room is
96.8-99.7% filled from the first visible frame of the fade in. Both 240x160
gates pass.

**Carry-forward.** `Scroll2Sub2` still slides on the literals `0x3c` and
`0x28` — 240 px and 160 px at 4 px per frame, the GBA screen. They no longer
matter above native size, where the slide runs to completion in one frame
behind the fade, but they are wrong for anyone restoring sliding.

**Lesson (11).** *A recorded cause that was never reproduced is a hypothesis
wearing a fact's clothing.* This entry carried a confident mechanism, a
mitigation described in the past tense, and "never reproduced" — for two
milestones. One recording overturned the mechanism and showed the mitigation
had never been built. Mark unreproduced causes as unreproduced.

## Reproducing B4 and B5

**This works — B13 was found with it in one pass**, after a round of inferring
from the prose found nothing. B4 and B5 have now been open since Milestone 1
for want of a recording, which is the cheapest thing on this list to obtain.

Both need a human at the controls, which is why they survived four rounds.
`--record=FILE` exists for exactly this and turns a human-reached moment into
a headless, frame-exact, re-runnable fixture:

```bash
cd build/play-320x160 && ./record-bug.sh B5
```

Play to the bug, quit **normally** (not `kill`), and keep both produced files:
the `.script` and the `.script.sav` beside it. Replay with
`tmc_pc --script=<file>` from a directory holding that save as `tmc.sav`.
Start recording from the title screen — the log begins at frame 0 and replay
starts from a fresh boot, so file-select navigation must be in it. Full
mechanism and the three things that break replay:
`tools/capture/README.md`, "Recording a human session".

A portable save-state file would have been the obvious alternative and **does
not work** — see the carry-forward item on quicksave portability.

## B6 — stray Zelda sprite in the left border *(fixed)*

**Cause.** On hardware the screen *is* the world view, so a sprite is either
on it or off it. Once the viewport is wider than the room being shown, the
leftover columns are border, and an entity standing there is something
hardware would never have drawn.

**Fix.** New PPU `virtuappu_mode1_set_obj_clip(left, right)`, driven from the
room's on-screen span.

## B7 — camera-pan softlock in Hyrule Town *(fixed)*

The bell→town-square pan hung forever; hard softlock.

**Cause.** `WaitForCameraTouchRoomBorder` (`src/script.c`) predicts where the
camera will rest and waits for `scroll_x` to equal it *exactly* — but
computed the prediction from `DISPLAY_WIDTH` while the camera clamps on
`VIEWPORT_CAM_MIN_X/MAX_X`. The equality could never hold.

**This was a process failure, not just a code one.** The Spike 5 `sed` meant
to convert `script.c` silently matched nothing (the real text has
`gRoomControls.` prefixes), and the verification grep only covered
`scroll.c` — so the spike reported the file as converted when it was not.
Every remaining `DISPLAY_WIDTH/HEIGHT` in `src/` has since been audited.

Also fixed alongside: the scripted camera helpers (`sub_08080974`,
`sub_080809D4`) pinned to `origin_x` when the target is near the left edge,
which for a room narrower than the viewport is not the resting place. Caught
by the `TMC_CAMTRACE` in-range assertion flagging a narrow room at `cam=0`
instead of `-40`.

## B8 — large heart offset left of the centred HUD *(fixed)*

The heart row's small hearts are BG0 *tiles*, so they ride the layer's
centring clip. The large heart is not a tile — it is the animated overlay
`UI_ELEMENT_HEART`, an OBJ positioned in screen coordinates. It kept its
authored 240-wide position while everything around it moved.

**Cause.** HUD sprites take `UI_HUD_SPRITE_DX` at source, because a global
OBJ offset would drag world sprites along too. The button elements get it via
`gHUD.buttonX` (`ui.c`), and the item and text elements inherit their x from
the button element — so three of the five element families were covered by
one assignment and nobody noticed the other two. `HeartUIElement` derives its
x from `health` instead (`x = ((health+3)>>2)*8 + 3`), which is why it was
missed.

**Fixed at both sites**, not just the reported one: `HeartUIElement` and
`EzloNagUIElement_Action0` (`element->x = 0x10`, the Ezlo-has-something-to-say
indicator) were the only two handlers setting a screen x without the shift.
The Ezlo nag was never reported — it only appears when Ezlo wants to talk —
but it is the identical one-line defect and would have been the next report.

**Why it presented as two different symptoms.** In the build the maintainer
was playing, `mapsource_bind_ui()` never ran (see B2), so there was no OBJ
clip and the heart was simply *visible in the wrong place* — 40 px left. In a
build with the clip working, the same sprite at x=27 falls inside the left
border band and the OBJ clip **deletes it entirely**. Same defect, and the
"missing large heart" it would have become is worth recognising as this bug
rather than a new one.

**Verified 320x160:** with the fix, the whole f11000 frame (smith's house, a
240-wide room) shifted by 40 px is **pixel-identical to the 240x160 frame
— 0 mismatches over all 38 400 pixels**, HUD included. `UI_HUD_SPRITE_DX` is
0 at native width, and both 240x160 gates still pass.

**Lesson.** The stale comment on `UI_CENTER_DX` in `include/viewport.h` still
said "unlike the in-game HUD which is edge-anchored" — three weeks after D1
was reversed. That is exactly the sentence that makes someone not think to
shift a HUD sprite. It has been corrected, and `UI_HUD_SPRITE_DX` now
documents which element sites need the shift and which inherit it.

## B9 — legend card artwork dimmed right of a vertical seam *(fixed)*

On each Picori legend card, once the text is on screen a vertical strip of the
stained-glass artwork — everything right of Link's sword — renders at reduced
brightness. The seam is sharp and does not correspond to anything in the art.

**Cause.** The story panels use a hardware window (WIN0) plus a blend to dim
the panel outside the artwork region. `sub_08053800` (`src/cutscene.c`) takes
the window's edges from a per-card table, `gUnk_080FCCB4[].width`, which packs
left and right into one u16 — `0..120` for the two tall portrait cards,
`0..240` for the four wide ones. Those are **240-authored screen
coordinates.**

The panels are a 240-authored surface and are centred like every other one,
by the BG clip. **A PPU window is not a BG, and the clip does not reach it**:
it is applied in raw screen coordinates. So the artwork moved +40 and the
window did not, leaving the blend boundary 40 px inside the artwork. Measured
directly: per-column brightness held ~390–407 up to x=118 and dropped to
~206 from x=120 — exactly the table's `120` with no shift applied.

**Fix.** Add `UI_CENTER_DX` to both edges at the source site, matching how
every other 240-authored coordinate is handled. Zero at GBA-native width.

**Also fixed, same class, unreported:** `kinstoneMenu.c` sets
`WIN_RANGE(0x68, 0x87)` — a 240-authored window on a centred UI screen. That
menu still cannot be entered cold (pre-existing crash chain, CHANGELOG #16),
so the fix is unverified at runtime, but the defect is identical and visible
by inspection.

**Verified 320x160:** all 11 captured legend frames are now **pixel-identical to
240x160 shifted by 40 px — 0 mismatches each**, against 1134–4636
mismatched pixels per frame before the fix. Both 240x160 gates still pass.

**This is a third distinct centring channel.** The BG clip moves layers, the
`UI_HUD_SPRITE_DX` sites move HUD sprites (B8), and PPU windows are a third
thing that must be moved and were not. Every remaining `WIN_RANGE` call site
with a literal coordinate is worth auditing against this — see the note under
carry-forward items.

## B10 — BG3 gameplay overlays clipped and misaligned *(fixed)*

Found by sweeping the carry-forward item "BG3 overlays were never swept for
wrap past 256 px". **Wrap was not the defect.** BG3 never had a map source,
so the "no map source ⇒ clip" rule caught it every time it was on — which
both removed the overlay from the border columns and, because the clip also
shifts by `UI_CENTER_DX`, moved it 40 px.

**Two families, and the shift is wrong for one of them.**

- *World-locked* overlays set `bg3.xOffset = scroll_x + k` (`holeManager.c:299`,
  `powBackgroundManager.c:32`). At a wider viewport `scroll_x` is already 40 px
  further left, so the layer aligns with the world on its own. The clip's extra
  +40 broke that — **visible in the middle of the screen, not just the borders**.
- *Screen-fixed* overlays sit at `ofs=(0,0)` (the Minish Woods light rays).
  Unclipped they render at their natural phase across all 320 columns.

**Fix.** BG3 is not clipped during a world view. On a UI screen it *is*
authored content and still takes the clip.

**Evidence.** The route's `field` and `textbox` waypoints were 7559 and 6123
mismatched pixels against Spike 0 through the centre 240 columns; both are now
**0**. A warp-tour probe of `SouthHyruleField` went 6197 → **0**. I had
previously written those differences off as camera clamping — they were this.
UI waypoints (cutscene, fileselect, pause, figurine) stay at 0 with solid
borders. 240x160 unaffected: 11/11 and 0/265,497,600.

`lightray` moved the other way, 29453 → 31673, and that is expected rather
than a regression: it is the screen-fixed family, so unclipping changes the
*phase* of a repeating diagonal pattern by 40 px while still covering the
viewport. There is no ground truth for a decorative full-screen overlay on a
wider screen, and no wrap seam appears — **0 wrap-period columns in every
gameplay waypoint**.

**How to find these:** `TMC_BG3_TRACE=1` logs every BG3 on/off transition with
the room, control word, offsets and whether the clip caught it. BG3 is off in
ordinary rooms, which is why none of the existing scripts ever exercised it —
reaching it needs the warp tour built from `data/map/entity_headers.s`
(`manager subtype=` 0x10 weather, 0x14 steam, 0x18 cloud, 0x19 pow, 0x1A hole,
0x1C rain, 0x22 light, 0x23 light-level).

**Unrelated crash noticed while sweeping:** the generated warp tour segfaults
after ~16 rooms **at both 240x160 and 320x160**, so it is not a widening bug. It warps
to arbitrary rooms at fixed coordinates (0x1E0, 0x1E0) that are out of bounds
for interiors. Not chased.

## B11 — circular-window transitions render as a near-black screen *(fixed)*

Found in Milestone 2's Spike 9, by sweeping the carry-forward item
"per-scanline circular windows have not been widened". They had a worse
problem than width: **they were not being drawn at all.**

**Cause.** Spike 4 widened the window registers by handing full-width bounds
to the PPU through `Port_Screen_CommitWindows`, since the packed 8-bit
registers cannot express an edge past 255. The PPU prefers those bounds
whenever they have been supplied — and `UpdateScreenRegs` supplies them every
frame, so the flag is true from the first frame and never cleared. The
HBlank DMA's per-line writes to `WIN0H` went into a register nothing read, and
every line got whatever whole-frame bounds were last committed. With
`winin=3F3F winout=0000` — all layers inside, none outside — that renders the
screen almost entirely black.

**This was a defect in the shipping 240x160 build**, not a widening bug.

**Fix.** `virtuappu_mode1_set_window_h_bounds()` replaces only a window's
horizontal pair; `port_hdma_step_line` calls it per line for any channel
targeting WIN0H.

**Evidence.** On the canonical route at 240×160, 4879 frames drive the
channel, 111 have WIN0 enabled, and **64 have a right edge that varies
between lines** — genuine circular windows. An A/B over 300 sampled frames
(`TMC_HDMA_NOWIN=1` restores the old behaviour) differs on 19, with the
differing pixel count sweeping 38151 → 518 and back: an iris animating. The
frame shows a clean circle of world content with the fix and a near-black
screen without it.

**Why four rounds of playtesting and the gate both missed it.** The gate is
11 still frames; the defect lives in a ~1 second transition between them, and
still reports 11/11 with the fix in. Playtesting would have shown it — this
is the fade between rooms — which suggests it was introduced after the
playtest rounds, i.e. by Spike 4 itself.

**Lesson (7).** *A gate made of still frames cannot see a defect that only
exists mid-transition.* When a change alters a mechanism rather than a
surface, ask which frames exercise the mechanism and count them, rather than
reading the gate's pass as coverage.

## B12 — entities culled in a band at the far viewport edge *(fixed)*

Found by re-auditing Spike 7's culling conversions rather than trusting its
"vertical siblings were converted at the same time" note.

**Cause.** `CheckOnScreen` (`port/port_draw.c`) — the per-entity visibility
test that gates whether an entity is drawn at all — compared against raw
literals `0x16E` and `0x11E`. Those are *screen size plus twice the 0x3F slack
margin* (240+126, 160+126), so at an expanded viewport they cull a band at the
far edge that is genuinely on screen: **17 px at the right at width 320, 17 px
at the bottom at height 240.** Entities blink out shortly before the edge.

**The horizontal half was live for the whole of Milestone 1.** Spike 7 walked
12 wide rooms and did not catch it because its two artifact scans looked for
wrap-shaped repetition and black columns, and a missing sprite produces
neither.

**Fix.** Both bounds are now `VIEWPORT_WIDTH/HEIGHT + ONSCREEN_MARGIN * 2`.
The margin stays `0x3F` — it is slack for a sprite whose origin has left the
screen while its body has not, a property of sprite size rather than of the
viewport.

**Evidence.** A/B over the canonical route at 320x240 differs on exactly two
waypoints, both inside the predicted bands: `woods` at rows 227-239 and
`textbox` at columns 294-313. The woods difference is a heart object whose
sprite was culled, leaving only its background pedestal.

**Lesson (8).** *A conversion sweep's own report of what it converted is not
evidence.* Spike 7 said the vertical siblings were done; one of the two most
important sites had neither axis converted. The same shape as B7, and as the
five `WIN_RANGE(0, 160)` sites found alongside it — where Milestone 1 had
converted one of four in a single file.

## B13 — town NPCs pop in and out inside the visible frame *(fixed)*

Reported by the maintainer playing the 320x240 build: NPCs in the Hyrule Town
square appear and disappear as Link moves vertically, while Zelda is
unaffected.

**Cause.** `CheckRectOnScreen` (`port/port_linked_stubs.c`) is not a drawing
predicate — it is the gate `DelayedEntityLoadManager` uses to decide which
NPCs *exist*. A cleared bit makes `NPCUpdate` call `DeleteThisEntity`, and a
set bit re-creates the NPC from `gNPCData`. Its bounds were the literals
`0xF0` and `0xA0`:

```c
if (dx >= halfW * 2 + 0xF0) return 0;   /* 240 */
if (dy >= halfH * 2 + 0xA0) return 0;   /* 160 */
```

With the manager's `halfH` of `0x20` that puts the live band at screen
y ∈ [-32, 192) — so an NPC was destroyed **48 px above the bottom edge of a
240-row screen** and rebuilt on the way back. Zelda survives because
`RecycleEntities` and this path both spare `ENT_PERSIST` entities; ordinary
townspeople are streamed.

**The horizontal half was equally wrong and live through all of Milestone 1**:
the band was x ∈ [-24, 264) on a 320-wide screen, so NPCs blinked in the
rightmost 56 columns too.

**Fix.** Both bounds become `VIEWPORT_WIDTH` / `VIEWPORT_HEIGHT`, which reduce
to the original literals at 240x160.

**Evidence.** Replaying the maintainer's recording and tracing NPC list
membership, the window around the reported movement had five
appear/disappear events; three were an NPC at screen y 190-193 — inside the
visible frame — and they are gone after the fix. The two that remain are at
y = -33/-30, outside the frame, which is the margin working as intended.
All 130 sampled frames differ, with the missing townspeople restored in a
consistent band below the old boundary.

**Why the earlier sweeps missed it.** Spike 7 and Spike 11 both audited
culling, and B12 fixed `CheckOnScreen` in `port_draw.c` — the *drawing* gate.
This is a second, differently-named predicate in `port_linked_stubs.c`, a file
of ported engine functions that the `src/`-focused greps never covered. It
is the only viewport literal in that file, which is exactly why nothing
flagged it.

**Repro.** `build/play-320x240/recordings/npcpop.script` plus its `.sav`,
replayed with `--script=`. The window worth watching is frames 11540–11800.
It is not committed: `*.sav` is gitignored project-wide, so turning this into
a permanent regression fixture needs a deliberate force-add.

**Lesson (9).** *Ask what a predicate gates, not just what it is called.*
`CheckOnScreen` and `CheckRectOnScreen` sound like the same kind of test; one
decides whether to draw an entity this frame and the other decides whether the
entity exists at all. The second is far more visible when it is wrong, and it
lived in the file the audits treated as stubs.

## B14 — UI side borders forced black, top/bottom borders not *(fixed)*

Reported by the maintainer playing 320x240: the Nintendo/Capcom logo screen,
the title screen and file select each had black bars left and right, while
their top and bottom bands showed the screen's own colour — white, pale yellow
and green respectively. Two borders around one surface, two different colours.

**Cause.** `Port_PPU_ComposeCanvas` (`port/port_ppu.cpp`) repainted the columns
either side of a centred UI screen with `PORT_VIEW_BORDER_COLOR`, to satisfy
the plan's original D3 "solid black borders". Nothing did the equivalent for
the rows above and below, so once the viewport grew a second axis the two
disagreed by construction.

**This was two decisions out of date, not a slip.** D3 was amended at Milestone
1 sign-off to accept coloured borders — a clipped UI screen shows the PPU
backdrop, which is what hardware shows outside every layer anyway — and the
repaint that D3 had motivated stayed behind. Milestone 2 then added the
vertical bands, which take the backdrop because they are ordinary PPU output
that nothing overpaints. The horizontal repaint was the odd one out from that
moment on.

**Fix.** Delete the repaint and let the PPU's own output stand on both axes.
`PORT_VIEW_BORDER_COLOR` still fills the canvas ring outside
`PORT_VIEW_CONTENT_*`, which is a different thing — canvas the PPU never
renders into, with no colour of its own.

**Evidence.** Measured as distinct colours per band, per the D3 note below and
lesson 6. Before: `L=000000 R=000000` with `T`/`B` two-coloured (backdrop
across the middle 240 columns, black in the corners where the side repaint cut
through). After, on every UI screen captured: **all four bands are a single
colour and it is the same colour** — `f8f8f8` on the logo screen, `f8f8a8` on
the title, `40b088` on file select.

**Scope.** Against the same build without the change, the four UI waypoints
(title, fileselect, pause, figurine) differ by exactly **19 200 px each = two
40x240 bands**, confined to columns 0-39 and 280-319 — so nothing inside the
centred screen moved. Every world waypoint is byte-identical. Both 240x160
gates pass; at native width the block could not run at all (`fw > DISPLAY_WIDTH`
is false), so the shipping build never had this defect.

**Lesson (10).** *When a decision is reversed, grep for what it motivated.*
D3's reversal is recorded twice in this document and the code it had justified
outlived it by two milestones — in a file nobody re-read, because the border
colour was not what anyone was working on. The same shape as B8's stale
comment, one level up: there the wrong sentence survived the decision, here the
wrong code did.

## B15 — room furniture lit against black through a door/stair fade *(fixed)*

Reported from a recording: entering a stairway, the room's furniture stays at
full brightness against a mostly empty screen through both halves of the fade,
then the room snaps in whole. Measured at **9.3%** of the play area filled
while fading out and **2.2%** while fading in, against 99.9% either side.

**Cause.** `GAMEMAIN_CHANGEAREA` is the door and stair transition, and
`GameMain_ChangeArea` draws *only sprites* while the fade runs — `FlushSprites`,
`DrawUIElements`, `DrawEntities`, `CopyOAM`, with no background work at all.
`GAMEMAIN_CHANGEROOM` is the same shape on the other side of the swap. At
240x160 that costs nothing: the VRAM screenblock covers the screen and still
holds the room. Above native size `mapsource_reason` refused both substates
with `substate!=UPDATE`, so the layers fell back to a screenblock that had
never been kept current while a map source was bound. What faded was a stale
slice on black — and the furniture was visible only because it is drawn as OBJ
sprites, which need no background at all. That is the whole symptom: *sprites
are the only thing that does not depend on the layer that went missing.*

**Fix.** Both room-change substates keep their map source above native size.
During either one `gRoomControls` describes a real room and the camera is at
rest on it, so the map source is exactly the right thing to draw.

**This fix was not available until B5 was fixed, and the evidence is a probe
that failed.** The identical relaxation was tried during the B5 work and
rejected on measurement: 12.0% → 14.5%, no better. A *sliding* `CHANGEROOM`
has the camera between two rooms, and one map source renders one room, so
binding filled part of the frame and left the rest backdrop. Replacing the
slide with a fade removed the between-two-rooms state, and the same one-line
change became correct.

**Evidence.** Replaying the recording at 320x240: the outgoing room holds
**99.9%** fill while dimming and the incoming room is **86.9–100%** while
brightening. The B5 recording still shows **0** frames of its transition with a
partially drawn room, so the room-to-room fade is unaffected. Both 240x160
gates pass; the relaxation sits inside the existing
`VIEWPORT_WIDTH > DISPLAY_WIDTH` guard, so the shipping build cannot reach it.

**Not established: whether this pre-dated the B5 fade.** The code path is
untouched by that commit, which is strong, but it is reasoning rather than
measurement — the fade shifts timing enough that the recording desynchronises
on a pre-fade build and never reaches the stairs. Which is itself worth
knowing: **a recording is now tied to the binary that produced it.** Any change
that alters how many frames a transition takes invalidates every existing
recording for frame-exact replay.

**Lesson (12).** *A probe that failed is evidence about the state it ran in,
not about the change.* This relaxation was measured, rejected and reverted one
session before it became the right fix. What made it wrong was a condition —
the camera sitting between two rooms — that a later change removed. When a
fix lands that alters the state a rejected probe depended on, re-run the probe
rather than trusting the earlier verdict.

---

## B16 — softlock entering the smith room after a scrolling transition *(fixed)*

Walking east from Link's house entrance into the room where Zelda and the
Master Smith are: the room appears, Link never emerges from the doorway, and
the game hangs. **Reported from the Android build, reproduced there 2 of 3
times on a fresh save, and initially not reproducible on desktop at all.**

Three defects in a row, each hiding the next. Only the third is the cause; the
first two had to be fixed before it could be seen.

**1. The one-frame slide stopped at the GBA screen width.** `Scroll2Step`
terminates on `0x3c` and `0x28`. Those are not arbitrary: at 4 px of camera
travel per step they are 240 and 160 px — the GBA screen, spelled as step
counts. B5's fade runs that step to completion in a single frame, so at 320x240
the camera stopped 80 px short and the player, who drifts 0.25 px per step,
landed 5 px short of where he belonged. Both now scale with the viewport
(`VIEWPORT_SCROLL_STEPS_X/Y`). Measured 60 steps / 240 px / 15 px drift before,
80 / 320 / 20 after. **The B5 commit predicted this and dismissed it** — its
carry-forward note says these literals "no longer matter above native size,
where the slide completes in one frame behind the fade". They matter precisely
*because* it completes in one frame: the loop still terminates on them.

**2. An out-of-bounds table read was masking the bug on desktop.**
`sub_080797C4` indexes `gUnk_0811C110` with `direction >> 3`, and `direction`
is a `u8`, so the index reaches 31 in a **four-entry** table. On hardware that
reads on into adjacent ROM and is perfectly defined — 0x0811C14E holds
`0x0807`. On PC the array is its own object and everything past it is whatever
the toolchain placed next, which differed between x86-64/GCC and arm64/Clang.
Desktop's garbage happened to satisfy `tmp == (collisions & tmp)` and released
the player from the doorway; Android's did not. **That single accident is why
this looked like an Android bug for six rounds of investigation.** The table
now carries the real ROM bytes for the full index range (`PC_PORT` only — the
ROM build needs the original four-entry object or its data layout moves).
Fixing it made desktop reproduce the softlock, which is what finally made the
bug tractable.

**Cause (3).** The player arrives in the new room with `direction == 0xff`,
which `LinearMoveDirectionOLD` reads as *not moving* and refuses to act on. He
is standing on the doorway tile (`ACT_TILE_41`, `SURFACE_DOOR`), which routes
`sub_080724DC` into the sub-state whose only job is to walk him off it — and it
cannot move him. He never leaves `PLAYER_ROOMTRANSITION`, so his queued
`PLAYER_SLEEP` is never consumed, so the cutscene script he was handed never
runs, so sync flags `0x4` and `0x8` are never set, and Zelda (`id=34`) and
Smith (`id=40`) wait on each other for ever.

The direction is lost *because of the fade*. The sliding path commits on the
same frame the boundary is crossed, so the player still carries the heading he
was walking. Deferring the commit 32 frames to fade out does not: he comes to
rest while the screen darkens. Traced directly — `playerDir=8` at the queue,
`playerDir=255` at the commit.

**Fix.** Capture the player's facing when the transition is queued and restore
it at the commit, which is exactly the state the slide had at its commit point.

**Evidence.** On the desktop repro the player now walks off the door —
`x=255 → 258 → 261 → 265`, the tile ahead changes from `0x29` to `0x23`, he is
released, and the cutscene proceeds (3 sync sets, `action=28`). Confirmed on
the reporter's device. B5's own recording still transitions correctly, carrying
`dir=24` westward. Re-measured B5's fade at 320x240 after the change: the
outgoing room holds a constant 80 border columns all the way down from
brightness 77 to 2.6, ~6 frames of black, then the incoming room fades in — no
frame shows a partially drawn room.

**Coverage gap, recorded because it is worse than the bug.** `sub_080797C4`
has exactly one caller and `gUnk_0811C110` exactly one user, and **the
canonical route never reaches either** — zero events in 13 000 frames. The
regression gate cannot see this code at all. It is only safe to claim the
shipping build is unaffected because indices 0-3 are byte-identical and the
extension can only change a previously-undefined read.

**Lesson (13).** *An out-of-bounds read in decompiled code is a platform
difference waiting to happen.* On hardware it has a defined answer, because ROM
is contiguous and the bytes after a table are real data. Ported to a machine
where that array is its own object, the same read returns whatever the linker
happened to place next — stable per toolchain, different between them, and
indistinguishable from a correct answer until something moves. B16 read 27
entries past a four-entry table and behaved differently on two platforms for
that reason alone. Where an index can exceed a table, the ROM bytes are the
specification.

**Lesson (14).** *A bug that only reproduces on one platform is not
necessarily a platform bug.* Six rounds went into what differed about Android —
frame rate, `char` signedness, audio threading, allocator behaviour — and all
of it was wrong. The engine ran identically on both; one accidental read made
desktop recover from a fault both platforms had. The question that ended it was
not "what is different about the device" but "what does the device do that
desktop does not", asked of a trace rather than of the code.

## B17 — Minish house interiors render as sprites over black *(fixed)*

Entering a Picori/Minish building interior: the room is not drawn at all. Only
sprites appear — Link, the NPC, the furniture drawn as OBJ — over a black
frame. **Reported 2026-08-05 from a maintainer recording
(`build/play-320x240/picori_village_room_glitch.script`), reproducing on both
Android and x86_64**, which places it in the viewport rather than the platform.

**Diagnosed, and the port's own instrument named it in one run.** With
`TMC_REJECT_TRACE=1`:

```
[reject] area=0x20 room=0x00 w=240 sf=0x01 sa=0 -> bottom=scroll_flags&1 top=scroll_flags&1
```

`scroll_flags & 1` is the *degraded room* exclusion: rooms whose map came from
the `0xffff` sentinel path, built 512x512 by `sub_0807C5F4` and not maintained
by the tile mutators. `AREA_MINISH_HOUSE_INTERIORS` is marked that way
explicitly (`playerUtils.c`, `roomControls->scroll_flags |= 1`). The map source
refuses those rooms by design, so both world layers fall back to the VRAM
screenblock — and a 256x256 screenblock cannot cover a 320-wide viewport.
Measured across the recording: the village holds 99.5% of the frame, and it
drops to **5.4% on the frame the interior loads** and never recovers.

**This is the third bug of this milestone with that same structural cause** —
after B5 (sliding CHANGEROOM) and B15 (door/stair fade). Each was reported
separately, diagnosed separately and fixed separately, and each was the
screenblock being asked to cover 320 px. *Enumerating every remaining path that
can fall back to the screenblock above native size is worth more than fixing
them one report at a time*, and is the first thing to do here.

**A one-line relaxation renders the room correctly, and is not yet a fix.**
Letting the predicate bind these rooms above native size takes the frame from
5.4% to **46.4%**, which is essentially the ceiling for a 240x160 room centred
in a 320x240 viewport, and the room is visually correct. The probe was reverted
rather than kept.

**The question that decides it was answered from the engine, not by
experiment.** The exclusion exists because the degraded map is not updated by
the tile mutators — and that is literally true in the source. All three
mutators (`SetTileType`, `SetTileByIndex`, `RestorePrevTileEntity`) wrap their
special-map write in `if ((gRoomControls.scroll_flags & 1) == 0)`. In a
degraded room that block is skipped entirely. Binding alone would have traded
a black room for a stale one — cut grass rendering as uncut — so **the
one-line relaxation was wrong**, confirmed rather than suspected.

**Fix.** Two halves, both behind `VIEWPORT_MAINTAIN_DEGRADED_MAP`:

- the three mutators maintain the special map in degraded rooms as well, so
  there is a current map to read;
- with that true, the map-source predicate binds them.

The map itself was never the problem: `sub_0807C5F4` builds it into the same
arrays at the same 0x80 stride the sampler reads, which is why binding
rendered a correct room in the first probe. Only its *maintenance* was missing.

**Evidence.** The reject count for `scroll_flags&1` across the reporter's
recording goes from **706 frames to zero**, and the frame from 5.4% filled to
46.4% — about the ceiling for a 240x160 room centred in a 320x240 viewport,
with the balance being the border the camera clamp produces. Regression gate at
240x160 passes. `VIEWPORT_MAINTAIN_DEGRADED_MAP` is 0 at GBA-native, which is
static-asserted while verifying, so every one of the four touched conditions
reduces to the original expression and the shipping build cannot reach any of
this.

**Lesson 12 applies in the direction it was written, and the answer was no.**
The probe was rejected statically in Spike 2, passed the rendering test two
milestones later, and was still wrong — it needed the *other* test, the one it
had been rejected for. A probe that passes the test you thought to run is not
evidence about the test you did not.

## Screenblock-fallback sweep — 2026-08-06

B5, B15 and B17 all have the same root shape: a world layer loses its map
source, falls back to the VRAM screenblock, and a 32-tile screenblock covers
256 px and cannot fill 320. Three separate reports, three separate diagnoses.
This sweep asks the question once: **which other paths can leave a layer on the
screenblock above native size, and do any of them show?**

Run entirely on instruments the port already had — `TMC_REJECT_TRACE=1` for
per-reason transitions with area/room/width, `--mapsource-report` for
per-reason frame counts, and frame dumps scored by *distinct colours per
column*, the border test lesson 6 prescribes. No new code.

**Coverage.** Eleven scripts and recordings at 320x240 (route, sweep, walk,
bugs, intro, and the B5 / npcpop / stairway / zelda-exit / smith /
picori recordings) — roughly 73,000 gameplay frames — plus warp probes into
the three areas whose managers the predicate names, covering 13 rooms.

| rejection class | fired | verdict |
|---|---|---|
| `task!=GAME` | heavily | title / file select; the clip rule handles it. Expected |
| `substate!=UPDATE` | heavily | menus and subtasks; same. Expected |
| `scroll_flags&1` | 706 frames (picori), 296 (area sweep) | **B17 — the only defect found** |
| `mid-transition` | 2–4 frames per recording | the B5 fade window; the screen is black by design |
| `bad geometry` | 1–2 frames per run | `substate=7 area=0 room=0 w=0` — a subtask before a room exists. Transient |
| `layer off` | 148 frames, 4 areas | **benign, measured** — see below |
| `subTileMap rebound` | **never** | **unverified** — see below |

**`layer off` is benign and that is measured, not assumed.** It fires in
MinishPaths (0x11), CrenelMinishPaths (0x12), 0x1A and MinishRafters (0x2E) —
BG1 detached while BG2 stays bound. Every one of the 13 rooms reached renders
**full width, 0 flat columns**. The top layer is genuinely unused in those
rooms, so there is nothing for the screenblock to fail to cover.

**`subTileMap rebound` never fired anywhere, and that is a gap rather than a
result.** The predicate's comment names bigGoron, minish paths and minish
rafters as its causes. Minish paths and rafters were reached — and produced
`layer off`, not `rebound`. So either the comment is stale about which managers
reach that state, or it needs a room or phase this sweep did not hit. **It
remains the most likely place for a fourth instance.**

**What limited the sweep — and a correction to what this section first said.**
Debug warps crash, and the first version of this entry attributed that to
out-of-range room indices. **That was wrong, and a control run disproved it**:
a script with a valid warp crashed 2 of 3 times, and a script with *no warp at
all* also crashed 2 of 3 times. There are at least two distinct faults here.

One is intermittent and warp-independent, near teardown, and it is the noise
that made whole sweep chunks look like failures when their dumps and reports
had in fact been written.

The other is deterministic per area: particular destinations crash on arrival
every time. Two real validation gaps were found and closed —
`Port_DebugAction_Warp` checked neither that the room exists in its area's
RoomHeader table nor that the coordinates fall inside the room, and it now does
both (rejecting the first, clamping the second). That measurably widened
coverage — one chunk went from 10 of 16 areas to 16 of 16 — **but it did not
eliminate the crash**, which still kills other chunks at specific areas.

So the sweep still covers a fraction of the ~128 areas, and the crash is still
open. It is the thing to fix before this sweep can be finished.

**Conclusion.** On everything reachable, **B17 is the only outstanding
screenblock-fallback defect.** That is a narrower claim than "there are no
others": `subTileMap rebound` is unverified, and most of the game's areas were
unreachable because the warp crashes. Both are named above so the next person
starts from them rather than from a playtest report.

## B18 — pause map detail view shows only the top of the map *(fixed)*

Pause menu → MAP → A on a windcrest. The detail map draws down to roughly two
thirds of the frame and then stops; below it is bare parchment down to the
frame's bottom bar. **Reported by the maintainer 2026-08-06 from the 320x240
build, with a screenshot.** Not a screenblock-fallback bug — the map is on the
right layer and drawn correctly, it is being *covered*.

**Root cause: an HBlank-DMA table indexed by physical scanline, whose band
bounds are rows of the authored 240x160 screen.**

`sub_080A67C4` (`src/menu/pauseMenuScreen6.c`) builds a per-line BG3CNT table
and hands it to `SetVBlankDMA`. BG2 carries the scrolling map at priority 3
(`gUnk_08128AD8[4]`); BG3 carries the frame's parchment, and the table flips
BG3's priority per line. `0x1e0b` is priority 3, which loses the tie to the
lower-numbered BG2 and lets the map show; `0x1e0a` is priority 2, which beats
BG2 and covers it. **BG3 is a curtain**, and rows `8 .. unk5+unk4`
(`gUnk_08128E94`; 132 for thirteen of the seventeen windcrests, 120 for the
rest) are the window the map is seen through.

Those two bounds are rows of the authored screen. The table is not:
`port_hdma_step_line` replays one entry per rendered line starting at line 0,
so the index is a **physical scanline**, while `mapsource_bind_ui`
(`port_mapsource.c`) centres the whole UI screen `UI_CENTER_DY` = 40 rows
further down. The curtain therefore opens 40 rows too high and closes 40 rows
too early — the top 8-row margin leaks map, and the bottom 40 rows of map are
replaced by parchment. 92 of the map's 124 rows survive, which is the "top
half" in the report.

**The screen said so itself.** The down-scroll arrow is drawn at y=0x84 = 132
(`sub_080A66D0`) — the same 132 that ends the band. It is an OBJ, so it takes
the UI screen's sprite offset and lands at physical row 172; the curtain closed
at 132. The arrow marking the bottom of the map window and the bottom of the
map window disagreed by exactly `UI_CENTER_DY`, on screen, in the reporter's
own screenshot.

**Spike 9 widened this table and did not move it.** It is one of the nine
per-scanline tables that spike lengthened to `VIEWPORT_HEIGHT`, which is why
the screen is not garbage below line 160. Lengthening a table and relocating
what it addresses are different edits, and only the first was needed anywhere
else.

**Fix.** Add `UI_CENTER_DY` to both band bounds, the way the figurine
gallery's and the kinstone menu's *static* window bounds already do
(`figurineMenu.c:129`, `kinstoneMenu.c:299`, `cutscene.c:247`). One edit fixes
two screens: `Subtask_LocalMapHint` builds its band through the same function.

**Evidence.**

- 320x240, before → after, on the reported frame: rows 132..171 change from
  parchment to map, and rows 40..43 from map to frame margin. Nothing else on
  the frame moves — rows 0..39, 44..131 and 172..239 are pixel-identical.
- The centred 240x160 region of all five map-screen captures (`n0_map`,
  `n1_detail`, `n2_detail`, `n3_detail`, `n4_scrolled`) is now **pixel-identical
  to the 240x160 build**. Before the fix the four detail frames differed by
  7602–7666 px each.
- Regression gate at 240x160: canonical route 11/11 with 0 differences;
  map-source audit 0 mismatched in 265,497,600 fetches.

**The gate cannot see this screen, so the shipping build got its own check.**
The canonical route never opens the map menu. `UI_CENTER_DY` is 0 at
GBA-native, so both bounds reduce to the original `8` and `unk5+unk4` and the
first loop's `i < 8` is exactly the original `i <= 7`; the only codegen
difference at 240x160 is one dead store of `8` into the loop counter, which
already holds 8. Empirically: the same capture script run on 240x160 binaries
built with and without the fix is **byte-identical across all 14 waypoints**,
including the five map screens. That is the argument the route could not make.

**Why no capture had ever rendered this screen.**
`Port_DebugAction_GiveAllItems` did not set `ITEM_MAP`, and
`PauseMenu_Variant2` bounces every request for screens 4, 5 and 6 back to
Items or Quest Status without it (`pauseMenu.c:139`). The entire map-screen
family was unreachable to *any* script, in either milestone. `giveallitems`
now sets it — `ITEM_MAP`'s only other use in the engine is that gate, so it
cannot disturb the inventory grid the way the blanket patterns that function
avoids would — and from `subtask 1 0` the detail map is two `R` presses and an
`A`.

**The general question, asked at the first instance rather than the third.**
Nine sites register a per-scanline HBlank DMA (`viewport.h`). Eight are world
effects: four circular WIN0H windows (fade iris, lantern, white triangle,
minish portal closeup), three BG3HOFS scrollers, and the rolling barrel's
affine matrix. Their line index is a screen position the world already places
correctly, and the port shifts nothing vertically in a world view. The pause
detail map is the only one of the nine on a **UI screen**, and a UI screen is
the only surface the port shifts vertically. So this class has exactly one
member and it is fixed — unlike the screenblock family, which was reported
three times before anyone asked.

## B19 — segfault entering a room narrower than the viewport *(fixed)*

Walking south through the door from Deepwood Shrine's pot-bridge room (`0x03`)
into the double-statue room (`0x04`): hard crash, not a hang. **Reported
2026-08-06 from the Android build with a `--record` script and a logcat**, both
captured with the diagnostics added the same day.

**It reproduced on desktop from the reporter's recording on the first try**,
which is the whole point of the recording: `SIGSEGV`, exit 139, deterministic,
in a debugger. Two Android-only reports in this milestone (B16, B17) each cost
rounds of asking what was different about the device; this one cost one replay.

**Root cause: one `u32` local makes a pointer offset unsigned, and a negative
camera offset wraps instead of subtracting.**

`sub_0807D280` (`src/screenTileMap.c`) repopulates the screenblock during a
scroll. Its `case 2` — the southward branch — computed

```c
mapspecial = mapspecial + (((unk_18 * 0x10000 >> 0x12) << 8) + ((xdiff >> 4) << 1));
```

`unk_18` is the function's only `u32` local. Pulling it into the sum makes the
whole expression unsigned by the usual arithmetic conversions, so a negative
`xdiff` does not subtract — it wraps. Measured at the fault:

```
unk_18 = 0   xdiff = -24   ydiff = -240
scroll_x = 232  origin_x = 256   width = 272  height = 160
gMapDataBottomSpecial = 0x555556dafee0
mapspecial            = 0x555756dafed8      (+0x1FFFFFFF8 bytes)
```

`0u + (-4)` is `0xFFFFFFFC`, and the pointer advanced **4 294 967 292 entries —
8.6 GB** out of a 32 KB array. The `DmaSet` on the next line is the `memcpy`
that died.

**Why the viewport exposed it.** `xdiff` is negative exactly when the room is
narrower than the viewport, because the camera is then pinned left of the room
origin (`VIEWPORT_CAM_MIN_X`) and the columns either side are backdrop. At
GBA-native width that is unreachable: 240 is also the narrowest room in the
game, so `xdiff >= 0` always and the unsigned sum was free. At 320 it is the
common case — **443 of 617 rooms are narrower**, and this one is 272.

The other three branches build their offsets out of the `s32` `tmp` locals and
were already signed. `case 2` is the only one that needed the fix.

**Fix.** Cast the `u32` term to `s32` so the sum is signed. At GBA-native size
both spellings agree exactly, because the sum cannot be negative there.

**Evidence.** The reporter's recording segfaults at frame ~8161 before and runs
to completion (exit 0, 8400 frames) after. Regression gate at 240x160:
canonical route 11/11 with 0 differences, map-source audit 0 mismatched in
265 497 600 fetches.

**This is the same shape as the carried-forward 8-bit window masks** — "at 240 a
screen x could not exceed 255 and the mask was free". Same sentence, different
width: *at 240 a camera offset could not be negative and the unsigned type was
free*. Both are the expansion making a previously-unreachable value reachable,
and neither is a clipping bug. Worth checking the rest of the engine for
arithmetic that assumes a non-negative camera offset, which is now routine.

**Still open, found while fixing this and deliberately not fixed here.**
`ydiff` is `-40` in the *steady state* of any room shorter than the viewport,
and `case 1` and the `default` branch feed it to
`(ydiff >> 4) * 0x100` — a small negative index, reading *before*
`gMapDataBottomSpecial` rather than past it. Those branches are signed, so they
do not wrap and do not crash; they read a kilobyte or two of the wrong globals
into the screenblock. Above native size the world is drawn from the map source
rather than the screenblock, which is likely why nothing has been seen. It
wants its own reproduction before anyone edits it.

**Lesson (17).** *An unsigned type is an assumption about sign, and the
expansion invalidated a lot of them.* B19's `u32` was correct for as long as
the camera could never sit outside its room. Grep cannot find this: the type is
in the declaration and the defect is in the arithmetic three lines away. What
finds it is asking which quantities changed sign range when the viewport grew —
camera offsets against room origins, which is now negative for two thirds of
the game's rooms.

## B20 — gameplay flashes at 240x160, offset, across a pause transition *(fixed)*

Opening or closing the pause menu shows the world at 240x160 and shifted 40 px
for a few frames before the transition completes, so the picture appears to jump
sideways and down. **Reported by the maintainer 2026-08-06 against the 320x240
build**, both directions, not platform-specific.

**Root cause: the clip changes several frames before the picture does.**

`mapsource_is_ui_screen()` answers from `gMain.substate` and `gUI.lastState`,
and `MenuFadeIn` sets both the instant the menu is *requested* — about eight
frames before the screen reaches black. In between, the world is still the thing
on screen but is already being clipped to `DISPLAY_WIDTH` x `DISPLAY_HEIGHT` and
shifted by `UI_CENTER_DX/DY`. Nothing is wrong with the clip; it is applied at
the wrong moment.

Measured on the transition, counting distinct colours per border band — the
metric the tracker's lesson 6 insists on, since the border here is the PPU
backdrop rather than black:

| frame | l / r / t / b | centre |
|---|---|---|
| gameplay | 24 / 51 / 152 / 70 | 202 |
| `open+2` (before) | **1 / 1 / 1 / 1** | 223 |
| `open+10` (before) | 1 / 1 / 1 / 1 | 1 (black) |

All four bands collapse four frames before the centre does. Closing is worse:
the world fades back *in* inside the small box and snaps to full size at full
brightness.

**Fix.** Do not classify differently — change classification only on a frame
where the change cannot be seen, i.e. while the screen is black. The engine
already fades both ways across this transition, so such a frame exists. Bounded
by a 40-frame hold so a transition that never blacks out cannot strand the clip.

**The two directions are not symmetric, and that is the whole difficulty.**
Opening changes state first and reaches black second, so waiting is enough.
Closing reaches black *while the subtask is still current* and only returns
`gMain.substate` to `GAMEMAIN_UPDATE` once the world is already fading back in —
there is no black frame left to wait for. So the close is anticipated from
`gUI.nextToLoad >= 3`, which `Subtask_Exit` sets at the top of the fade out.

**Three wrong answers, each caught by the instrument rather than by reading.**
`TMC_UILATCH_TRACE` was added when the first version appeared to work, and
reported `black=0` on *every* frame of both transitions — the latch was running
entirely on its timeout and the apparent fix was the timeout outlasting the
fade:

1. `gPaletteBuffer` is the engine's working copy, not what the PPU renders.
2. `gBgPltt` is what the PPU renders, and it is still **bright** at the black
   frame. The engine reaches black by *switching the layers off* and showing the
   backdrop (`PauseMenu_Variant3` clears the BG enable bits), not by darkening
   colour. So "is it black" is a DISPCNT question first and a palette question
   second.
3. `gUI.nextToLoad == 3` closes one frame too early: traced, `nextToLoad` is
   already `4` on the single black frame in the middle of the close, so an
   exact-match window has nothing to apply. `>= 3` is the teardown.

**Evidence.** Both directions, before → after: `close+2` goes from 1/1/1/1 to
23/46/80/64 and `close+8` from 1/1/1/1 to 24/51/151/70, i.e. the world fades in
at full size throughout; `open+2` goes from 1/1/1/1 to 24/51/153/70. The settled
frames either side are unchanged. Regression gate at 240x160: canonical route
11/11 with 0 differences, map-source audit 0 mismatched in 265 497 600 fetches —
and the whole change sits inside `#if UI_CENTER_DX > 0 || UI_CENTER_DY > 0`,
which does not exist at GBA-native size.

**Lesson (18).** *A fix that works for the wrong reason measures the same as a
fix that works.* This one passed its before/after comparison on the opening
transition while its central test — "is the screen black" — had never once
returned true. Only the trace separated them. Where a fix has an internal
condition that is supposed to fire, log whether it fires, not just whether the
output improved.

## B21 — Minish Woods light shaft ends 80 px short of the right edge *(fixed)*

Entering Minish Woods from the west, the shaft of light reaches the right edge
of the screen at 240x160 and stops 80 px short of it at 320x240. **Reported
2026-08-06, measured by the maintainer on 2026-08-07 as exactly 80 px.**

**Diagnosed. It is not a clip, a clamp, or a mask — there is nothing out there
to draw.**

The shaft is BG3, a 32x64 tilemap loaded straight into `gBG3Buffer` by gfx group
0x25 (4096 bytes to GBA `0x02001A40`). Read back at runtime:

```
[bg3dump] xOffset=16
  col  0..20  first=0x8340        <- one filler tile, repeated
  col 21..31  0x8360..0x8364, 0x8341..0x8345, 0x8350   <- the shaft
```

Blank on the left, ~16 columns of artwork on the right ending exactly at map
px 255. At the constant `xOffset = 0x10` that is screen 115..239 — mid-screen
to the right edge of a 240-wide screen, which is what the maintainer describes
seeing at 240x160. At 320 the same band still occupies 115..239 and the screen
simply got 80 px wider.

**No offset can fix it.** The layer wraps at 256. Putting ray columns at screen
256..319 requires ray content in map px `X..X+63`, and that same 64-px window is
also what renders at screen 0..63 — so a shaft at the right edge implies a
second shaft at the left. **Repeated shafts were rejected by the maintainer**
(2026-08-07), which closes that branch rather than leaving it as an option.

**The measurement that settled it was an A/B of the layer, not of the picture.**
Three earlier readings were artifacts and had to be thrown away: a "seam at
x=280" in a zoomed crop that per-column brightness showed was a tile boundary;
an extent from a single frame pair, which static scenery would have produced
identically; and a camera-move test whose premise was wrong because this state
sets `bg3.xOffset` to a *constant*, so the layer never moves with the camera
anyway. Building with BG3 forced off and differencing gave the answer in one
run — `BG3 contributes to columns 115..239, and 0 px beyond` — and that is the
technique to reach for first next time.

**That technique answers the question and leaves nothing to look at, which is
its cost.** It is a subtraction between two builds: the number is trustworthy
and the frame it came from still shows nothing anyone can point at. All three
of the discarded readings above were attempts to read the shaft off the picture
directly, and the picture will not support it — the layer is blank across two
thirds of its columns and alpha-blended at eva=9 over foliage of nearly its own
hue everywhere else. `TMC_MASK_BG3=1` (2026-08-20) paints the layer's every
non-transparent pixel flat magenta and drops it out of the blend, which turns
the same question into a census of one frame from one binary: the shaft reads
`cols=115..239` at both sizes, and at 320x240 the 80 empty columns to its right
are *visible* rather than inferred. Reach for the mask first and the difference
second — the difference is still what proves a change moved something, but it
cannot show you what you are changing.

**The fix that would work is blocked by VRAM, not by artwork.** BGCNT's size
field offers 512x256, which covers 320 with no wrap at all — `512 - 320 = 192`,
so the screen shows map 192..511. Everything downstream already supports it:
the PPU honours the size bits (`map_width_tiles = (size_flag & 1) ? 64 : 32`),
`scroll_x` is masked to 9 bits, and `sub_08016CA8` takes its upload length from
`gUnk_080B2CD8[control >> 14]` = `0x1000` for that size, so the 4 KB lands
across two screenblocks with no change to the DMA path. And the extra 256 px
needs only *blank*, which is one repeated tile — no new artwork.

It still cannot be done, because a 512-wide BG needs an adjacent **pair** of
screenblocks and there is no free pair:

| layer | screenbase | VRAM |
|---|---|---|
| BG1 | 28 | `0x0600E000` |
| BG2 | 29 | `0x0600E800` |
| BG3 | 30 | `0x0600F000` |
| BG0 | 31 | `0x0600F800` |

All four are occupied and contiguous, and everything below block 28 is
character data — gfx groups load tiles right up to `0x0600F000`. Prototyped
anyway to be sure: BG3 at size 512 from base 30 writes 4 KB over blocks 30 *and*
31, which is BG0's tilemap, and the HUD and text box render as garbage. Reverted.

**Fixed 2026-08-20, and none of the blocked routes was the fix.** Every one of
them — the 512-wide BG, the repeats, the offsets — was an attempt to make the
layer *reach* the extra 80 px. It never had to. The port already clips
240-authored layers to their authored width and places them; BG3 was the one
layer deliberately exempted from that rule in a world view, and the exemption
was written for a different kind of overlay.

`mapsource_bind_ui()` skips BG3 outside a UI screen because these overlays are
tiled patterns locked to the world — hole parallax, cloud shadows, weather,
steam, POW all set `bg3.xOffset` from `scroll_x` — so letting the screenblock
wrap past 256 px is exactly what covers a wider viewport with more of the same
pattern, and adding `UI_CENTER_DX` on top would misalign them from the world
they belong to. Both halves of that are true, and neither holds for the light
shaft: its `xOffset` is the constant `0x10`, so there is no world alignment to
preserve, and its map is blank for two thirds of its columns, so what the wrap
brings into view past 239 is that blank end rather than more pattern. **The
layer was never short. It was showing the wrong 80 px of itself.**

An overlay now says which it is. `sub_08057450` declares
`PORT_BG3_ANCHOR_RIGHT` through `Port_MapSource_DeclareBg3ScreenAnchor`, and
a layer that has declared it gets the clip the rest of the 240-authored
surfaces get, pinned to the right edge instead of centred.

**Anchored to the right edge of the *room*, not of the viewport, and that
distinction is the whole of the second bug this fix nearly shipped.** Exactly
two rooms in the game run this handler — `Area_MinishWoods` room 0 and
`Area_MinishHouseInteriors` room 9, the barrel minish house — and they differ
on precisely this point. Minish Woods is 1008 px wide, fills the screen, and
the room's right edge is the viewport's. The barrel house is **240x368**:
narrower than the viewport, so the room is centred with 40 px of border either
side and its right edge is screen 279. Pinned to the viewport the band measured
`cols=195..319` there — hanging 40 px out into the border. Pinned to the room
span it measures `155..279`. The span is the same one the sprite clip below it
computes, and for the same reason: outside it is border, not world.

**Measured.** With `TMC_MASK_BG3=1`:

| | before | after | room's right edge |
|---|---|---|---|
| Minish Woods 320x240 | `cols=115..239` | `cols=195..319` | 319 |
| Minish Woods 320x160 | `cols=115..239` | `cols=195..319` | 319 |
| barrel minish house 320x240 | `cols=115..239` | `cols=155..279` | 279 |
| barrel minish house 320x160 | `cols=115..239` | `cols=178..279` | 279 |

and with the HUD and sprites taken out, the whole band is a **pure 80-px
translation** — 7552 mask pixels before and after, zero residual under the
shift, on every frame. Steady across a 49-frame walk and a 14-position camera
sweep. The 342-pixel discrepancy the first comparison reported was entirely
HUD and sprite occlusion changing as the band moved under fixed-screen content,
which is what the isolation switches exist to remove.

At GBA-native width the declaration is compiled out — `#if defined(PC_PORT) &&
UI_CENTER_DX > 0` — and `lightRayManager.c`'s generated code is byte-identical
to the engine's, checked with `objdump`. Gate: 11/11 waypoints, 0 of
265,497,600 fetches.

**Lesson (31).** *A layer that ends too early may not be short — it may be
wrapping, and showing you the blank part of itself.* The 2026-08-07 diagnosis
was right about every fact it established (the map is 256 px, the shaft ends at
map px 255, no offset can place ray content past 255 without repeating it) and
wrong in the conclusion it drew, because it never asked what the columns past
239 were *currently* showing. `BG3 contributes 0 px beyond 239` was read as
"there is nothing out there to draw" when it also fits "the thing out there is
transparent". Those need different fixes and only one of them was possible.

**Lesson (32).** *An exemption is a claim about a class, and a class acquires
members you did not check.* The "leave a world view's BG3 unclipped" rule was
measured and correct for the five tiled overlays it was written for, and it
silently governed a sixth that shares none of their properties. When a rule is
justified by what its members have in common, the guard has to test that
property rather than the layer index.

**Lesson (19).** *"Supported end to end" is a claim about a pipeline, and a
pipeline has more stages than the ones you thought to check.* The size-bit route
was proposed after confirming the renderer honoured it and the DMA length table
sized it — two real checks that were both true and neither of which was the one
that decides. VRAM allocation was, and it was never looked at until the
prototype corrupted the HUD. When a change needs a resource, check the resource
is free *before* checking the code that would use it.

## B22 — rolling barrel interior: doors out of reach, room spills past 160 rows *(fixed)*

Deepwood Shrine's `InsideBarrel` (area `0x48`, room `0x20`). **Reported
2026-08-08 by the maintainer: "the doors do not line up with the walkable room
area", the room unplayable at 320x240.** Fixed the same day.

**The doors never moved. The player did.** The room is exactly 240x160 — the
one size at which a room is the screen — so at 320x240 it is centred and the
camera is pinned 40 px above the room origin (`camx=-40 camy=-40`,
`TMC_CAMTRACE`). `sub_08058CFC` holds the player on the barrel's midline and
measured that midline **from `scroll_y`**, i.e. from the camera. On hardware
`scroll_y == origin_y` here by construction, so the two spellings are the same
number and the engine's choice was free. Above native height it is 40 px of
error, and everything the player interacts with — the four door hitboxes, the
cobweb hole at room y 69..92, the roll-speed reading, the quadrant split at
`0x50` — is measured from `origin_y` and stayed put.

Measured on Link's sprite: **room y 27 at 320x240 against 63 at 240x160.**

Three symptoms from the one defect, which is why it read as several problems:

- the lower pair of doors (room y 136..146) was outside the reachable band;
- the barrel rolled continuously and unprompted, because sitting permanently
  above the midline satisfies `tmp < 0x49` every frame — and a barrel that will
  not hold still cannot hold the narrow angle windows the doors test;
- the quadrant split always chose the upper half, whichever half the player was
  in.

**The visual half is the vertical twin of B5/B15/B17.** The barrel's picture is
BG2 as an affine layer, driven by a per-scanline matrix the manager rebuilds
every frame in screen coordinates it spells as literals — `scrX = 0x78`,
`scrY = 0x80`, and a `/0xA0` that spreads a quarter sine period across the
screen's 160 scanlines. Run over 240 rows, the index reached 191 instead of 127
and the stave curvature inverted over the bottom rows, and the layer's own
screenblock ran out of authored content at row 160 and showed 80 rows of
unrelated tiles below it.

**Fix.** Three parts, all reducing to the original at GBA-native size:

1. `sub_08058CFC` anchors to `gRoomControls.origin_y`. Same number at 240x160.
2. `sub_08058BC8` builds entry `tmp3` from room row `tmp3 - UI_CENTER_DY`,
   clamped to `DISPLAY_HEIGHT`, so the 160 authored rows land in the centred
   band and no sine index leaves the range the 240x160 build uses.
   `UI_CENTER_DY` is 0 at native, so `row == tmp3`.
3. `port_mapsource.c` gives the room's layers `offset_y = UI_CENTER_DY,
   content_height = DISPLAY_HEIGHT` — the vertical twin of the existing width
   clip. **BG0 is excluded**: it carries the HUD, whose bands anchor to the top
   and bottom of the *viewport*, and confining it would push the hearts down
   40 px and delete the counters.

**The room is reached mechanically, not by area and room number.** The
predicate is "a world view in a GBA affine display mode" (`DISPCNT` mode 1 or
2). `grep DISPCNT_MODE_ src/` returns exactly two sites in the whole game: the
title screen, which is already a centred UI screen, and this room. An affine
layer has no room map behind it and does not follow the camera, so it is a
240x160-authored surface by construction — `mode2.c` already says so where it
honours the clip on that layer.

**Verification, because a gate pass is not coverage here.** This changes a
per-frame mechanism, not a surface, so a static waypoint proves little. Driving
a full roll — down, up, and settling — from a script:

- 26 sampled frames **byte-identical at 240x160** before and after, with 23 of
  25 consecutive pairs differing from each other, i.e. the mechanism is
  genuinely exercised rather than 26 copies of a still scene;
- at 320x240 the play area (crop rows 37..128, between the two HUD bands) is
  **pixel-identical to the 240x160 build on all 26 frames**, and the player's
  position tracks it exactly at +40. The only differing rows are 6..36 and
  129..156 — the two HUD bands, which are bottom- and top-anchored and are
  supposed to differ;
- canonical route 11/11, map-source audit `fetches=265497600 mismatched=0`.

**Left open, deliberately: ~24 px of the barrel's rim sprites still show in the
top and bottom border.** The rim is twelve 32x32 sprites (tile `0x8EB0`,
priority 3) at room y -24 and y 152 — six across, mirrored. Hardware clipped
them at the screen edge; a taller screen reveals 24 more rows of each. The
horizontal twin of this is already solved (`set_obj_clip` to the room's
on-screen span) and the HUD survives it **by construction** — the narrowest room
is 240, so the span is at worst `[40,280)` and the HUD is authored inside 240
and shifted by 40 into it. **Vertically there is no such construction**: the
shortest room is 160, so the span is at worst `[40,200)`, and the HUD's own
sprites sit at y 4..21 and near the bottom edge — inside the band a rim clip
would have to remove. Confining the rim therefore needs a per-slot world/UI
distinction plumbed through `RenderSpritePieces`, `gOam*` and the PPU's OBJ
raster, which is a hot loop, at a milestone whose frame time is already an open
go/no-go. Costed and put to the maintainer rather than spent unasked.

**Lesson (20).** *A coordinate the engine had two equal spellings for is a
defect waiting for the viewport to separate them.* `scroll_y` and `origin_y`
were the same number in this room on hardware, so nothing distinguished the
right one from the wrong one until the camera could move relative to the room.
The same shape as B5/B15/B17 — an assumption that the screen is the room — and
the fourth time it has been the answer. When a room is exactly viewport-sized,
every camera-relative expression in it is unverified code.

## B23 — barrel's drawn hole and the fall it triggers are rotationally apart *(fixed)*

**Reported 2026-08-08 with a recording, immediately after B22 was fixed:** the
invisible exits are now in the right place, but the *drawn* door/hole sits at a
different rotation from the exit that fires. Clearest on the middle exit — the
drawn cobweb hole is nowhere near where Link starts falling.

**Not the expansion, and not B22's fix. It is the port's own angle-gate bypass**
(`sub_08058A04`, `#ifdef PC_PORT`, from CHANGELOG #6, commits `107e7451` /
`cd99dd4d` — long before Milestone 2). Hardware gates the fall on
`unk_20 - 0x118 < 0xD`, i.e. a 13-unit window out of a 512-unit revolution,
*because the cobweb hole rotates with the barrel*. The gate is what guarantees
the drawn hole is at room centre when the fall fires — and the fall snaps the
player to room centre (`origin + 0x78, 0x50`) unconditionally. Remove the gate
and the fall still uses room centre while the hole is wherever the rotation put
it.

**Measured on the maintainer's recording:** at the frame he falls, `unk_20` is
`0xA4`; the window `0x118..0x124` opened on **0 of the sampled frames** of the
whole run. `0xA4` is the barrel's own rest angle (the snap targets are `0x48`,
`0xA0`, `0xF0`), so the bypass makes the fall fire at a rest angle essentially
every time — a fixed ~0x74 of rotation, about 82°, from where the hole is drawn.

**Viewport-independent by construction**: the bypass is `#ifdef PC_PORT`, which
is defined at both sizes, so 240x160 is misaligned identically. This was
invisible until B22 was fixed because the room was not playable enough to reach
the hole deliberately.

**It is a decision, not a defect to fix silently** — the bypass was the
maintainer's deliberate call to make the hole reachable, and the difficulty it
worked around is real at 240x160 too (so it was *not* a workaround for B22).
Three routes, put to the maintainer 2026-08-08:

1. **Restore the hardware gate.** Drawn hole and fall coincide exactly; the
   original difficulty returns.
2. **Keep the bypass.** Easy to trigger, permanently misaligned — today's
   report.
3. **Rotate the barrel into alignment, then fall.** When the player stands in
   the hole with the cobweb gone, drive `unk_20` toward the window over a few
   frames and fall when it opens. Keeps the bypass's reachability and restores
   the visual relationship.

**Resolved 2026-08-08: option 1, the maintainer's choice.** The bypass is gone
and `angleOk = (this->unk_20 - 0x118 < 0xDu)` is unconditional again.

**Restoring the gate exposed the real defect, which was in the renderer, not
the engine.** The maintainer re-tested and reported standing dead centre on the
drawn hole without falling. Traced: he was in the hitbox (`px=120 py=84`,
`inhole=1`) at `unk_20 = 0xCA`, while the gate wants `0x118` — **the drawn
barrel was 0x4E out of step with the angle the logic reads.**

`virtuappu_mode2_render_frame` computed `tex_y = ref_y + pd * rel_line + ...`.
That accumulation is right when BG2X/BG2Y are latched once per frame, which is
how the affine BG normally works. It is wrong when HBlank-DMA **rewrites the
reference every scanline**, because hardware's internal accumulator is
overwritten before each line is drawn — the value just written *is* that line's
reference. The barrel supplies `texY = (angle + line) << 8` per line, so the
line term was counted twice: the texture was sampled at **twice the vertical
rate**, and the picture sat

    (unk_20 + 2r - 128) - (unk_20 + r - 128) = r

texture rows out of step, which at the centre of the authored frame (r = 80) is
**0x50** — against 0x4E measured, the 2 being `sy` not quite 1.0.

Fixed in the PPU rather than worked around in the engine:
`port_hdma_drives_bg2_reference()` reports whether an active channel covers
IO 0x28..0x2F, `Port_PPU_PresentFrame` publishes it through
`virtuappu_mode1_set_bg2_ref_per_line()`, and mode2.c drops the `pb`/`pd` line
term while it is set. **Verified**: the same scripted approach that lands the
window now falls through with the hole under the player, and the barrel's plank
spacing and grain are correct instead of vertically doubled — which is also the
long-standing "renders as flat brown bands / warp not honoured" complaint from
CHANGELOG 0.1.2 and #6, and the actual reason the angle was unlandable enough
to be bypassed in the first place.

**Not viewport-specific**: the double-count was there at 240x160 too, so the
shipping build's barrel was equally out of step. The regression gate passes
(11/11, 0 mismatches) but never enters this room, so it is not evidence here —
the evidence is the capture above.

**Reachability was the risk and was measured before shipping it** — a gate that
cannot be landed makes the dungeon unfinishable, which is far worse than a
misaligned hole. Two facts settled it:

- **The window is on the barrel's free-rolling arc.** The snap targets are
  0x48, 0xA0 and 0xF0, and traced over a full revolution the angle runs
  0xF0 -> 0x117 -> 0x146 -> ... -> 0x1D2 -> 0x0 -> 0x2F -> 0x48: nothing between
  0xF0 and the wrap, so a barrel pushed past its last rest rolls straight
  through 0x118..0x124 rather than stopping short of it.
- **It can be landed, and the technique is the authentic one.** Roll until the
  angle is inside the window, then stop pushing; the angle freezes as soon as
  the player is back inside 0x49..0x57, and the hole band (room y 69..91)
  overlaps that. Swept five release timings 60 frames apart: **1720 froze at
  0x118 and 1733 at 0x120, both `win=1` and both fell through**; 1700 stopped
  short at 0xF4, 1745 and 1760 overshot to 0x135 and 0x149. A ~25-frame band
  out of a ~60-frame roll, with the hole visibly rotating into place as
  feedback.

## B24 — riding a lily pad through a room-scroll transition strands the player outside the room *(fixed)*

Deepwood Shrine B2, floating east from room `0x14` into `0x15`. **Reported
2026-08-08 with the same recording.** The player and the pad end up west of the
room's own west edge, held there by the room-border collision, drawn in the
left border and unable to move — a softlock.

**Exact numbers, from a per-frame trace of the transition:**

| | |
|---|---|
| room `0x14` | origin_x 736, width 464 → east edge **1200** |
| room `0x15` | origin_x **1200**, width 272 |
| player at commit (f5743) | absolute x **1169** — 31 px short of the boundary |
| after the slide (f5745) | absolute x **1189**, `unk_18=80`, camera 880→1200 |
| settles | absolute x **1177** → room x **-23**, camera correctly pinned at -24 |

**Root cause: `VIEWPORT_SCROLL_FADE` collapses the slide into a single frame,
and that preserves the camera and the player's per-step drift but not the
motion of the vehicle he is riding.** `Scroll2Sub2` runs all
`VIEWPORT_SCROLL_STEPS_X` (80) steps in one frame; each step moves the camera
4 px and nudges the player 0.25 px, so the player gains exactly 80 x 0.25 = 20
px — which the trace confirms to the pixel (1169 → 1189). On hardware the same
slide takes 60 *frames*, during which the lily pad is a live entity with
`action = 3` and `speed = 0x100` (1 px/frame east, set by `sub_08085E74`, which
also makes the pad `gRoomControls.camera_target`) and therefore carries the
player a further ~60 px — comfortably across the 31 px boundary. Collapsed, the
pad gets one frame instead of sixty.

`Scroll2Sub2`'s own comment states the intent — "land on exactly the state the
sliding path would have reached". For a walking player it does; for a *ridden*
one it does not, because the drift models only the player.

The lily pad is one of four callers of `sub_0807BD14`; **`minecart.c` is
another and is the same shape**, so this is a class, not one room.

**Expansion-caused but not caused by B22's fix** — proved two ways: the two
functions B22 touched are called only from the barrel manager, and room `0x15`
runs in DISPCNT mode 0, where B22's clip predicate is false. The fade path only
exists above GBA-native size, so 240x160 is unaffected.

**Not reproducible by replaying the recording on an older build** — on the
pre-B22 binary the player bounces out of the barrel four times and never
reaches B2 at all, and at 240x160 the run diverges after room `0x14` because
transitions slide rather than fade.

**A two-part fix was attempted first and did not clear it** (commit
`030ed14a`). It was not wrong — it was *inert*, and why is the whole lesson
here. Both parts key off `gRoomControls.camera_target` being the vehicle, and it
never was, because the state that claims the camera had already exited. With
that state alive (below) both parts do exactly the work they were written for:
the pad holds the camera through the deferral, and `Scroll2Sub2` supplies the
travel the collapsed slide skips. The two halves are:

1. `ScrollTransitionApplyWhenBlack` (playerUtils.c) saves and restores
   `gRoomControls.camera_target` around the apply. The deferral inverts an
   order: a vehicle hands off by calling `sub_0807BD14` and *then* claiming the
   camera, so sliding the claim lands after the apply's reset and stands, while
   fading it lands 32 frames before and is wiped. That wipe is why the player
   received the *walker's* 0.25 px-per-step nudge at all — `Scroll2Step` applies
   it only when the target is the player. Same shape as
   `sScrollFadePlayerDirection` (B16), one field over.
2. `Scroll2Sub2` (scroll.c) advances a non-player camera target, and the player
   with it, by `steps x speed` along the scroll direction — the travel the
   vehicle would have had over the frames the collapse skipped.

**The actual root cause, found from a second recording (2026-08-08).**

`LilypadLarge_Action3` *is* the carry-across-a-room-scroll state. It runs
`LinearMoveUpdate` on the pad and on the player every frame, and it exits — hands
the camera back, drops `ENT_PERSIST`, returns to ordinary floating — on

    if (gRoomControls.reload_flags == 0)

which is the engine saying "the scroll is over".

Sliding, `sub_0807BD14` applies the transition *inside itself*, so that flag is
already set the first time action 3 runs, and the pad carries for the whole
60-frame slide — about 60 px, which is what takes it across the boundary.
Fading, the apply is deferred until black, and **for those 32 frames nothing
marks a transition as in progress**. Traced: the hand-off fires at f4395 with
the pad at room x 440, action 3 is entered, and by f4400 the pad is back in
action 1 with the camera handed back — 28 frames *before* the room changes. It
carried for zero frames.

So the pad crossed nothing. It sat at the old room's x 441, which is the new
room's **-23**, and stayed there for the rest of the recording, with the player
on it at -21, both drawn in the left border. The player's earlier-measured
+20 px was the *walker's* per-step nudge, applied precisely because the pad had
already given the camera back.

**Fix.** `ScrollTransitionIsPending()` (playerUtils.c) reports a decided but
un-applied faded transition, and action 3 treats that as a scroll still in
progress. Defined outside the `#if VIEWPORT_SCROLL_FADE` block and returning
`FALSE` at GBA-native size, so the condition there reduces to the original
exactly.

**Verified on the recording that found it**: the pad now carries from 435
through the deferral, is at room `0x15` x **91** with the player at 89 when it
returns to floating — comfortably inside a 272-wide room — against **-23**
before. Regression gate 11/11, `fetches=265497600 mismatched=0`.

**Then trimmed, on the maintainer's report that it entered too far in.** The
first version landed the pad 91 px into a 272-wide room where hardware puts it
at ~36, because it was given the 32 fade frames of its own travel *and* a full
`VIEWPORT_SCROLL_STEPS_X` slide on top. Two things were wrong with that number:

- **How far a vehicle carries the player is a fact about the vehicle and the
  room, not about how wide the screen is.** It is `speed` for as many frames as
  the slide lasts, and on hardware that is `DISPLAY_WIDTH / 4` across (60) or
  `DISPLAY_HEIGHT / 4` down (40). `VIEWPORT_SCROLL_STEPS_*` is right for the
  *camera*, which genuinely has more screen to bring on at 320 wide, and wrong
  for him.
- **The fade frames are travel too.** The carry state runs throughout the
  deferral, so by the time the slide is collapsed the vehicle has already had
  those frames; only the remainder is owed. The fade is `0x100` of progress at
  `VIEWPORT_SCROLL_FADE_SPEED`, so its length is `0x100 / SPEED` = 32 frames.

`Scroll2Sub2` now tops up by `nativeSteps - fadeFrames` — 28 frames across, 8
down — clamped at zero. **Re-measured on the same recording: the pad enters
still carrying at room x 7 and comes to rest at 39 with the player at 37**,
against 91 before and ~36 on hardware. The 3 px over is the few frames of carry
that run after the swap before `reload_flags` clears.

Freezing the vehicle during the fade instead was rejected: it would visibly
stall while the screen is still bright, where being carried is what hardware
shows.

**`minecart.c` has the same `sub_0807BD14`-then-claim shape and has still never
been exercised.** If a cart ever strands you on a room boundary, this entry is
where to start.

**Lesson (21).** *Two clocks that agree on hardware will not agree once one of
them is deferred.* `VIEWPORT_SCROLL_FADE` puts 32 frames between a transition
being decided and being applied, and everything that asks "is a scroll in
progress" answers *no* across that gap. B16 lost the player's facing to it and
B24 lost a lily pad's entire carry state. The deferral is now three bugs old and
its shape is known: anything running between a hand-off and the end of a scroll
must be checked against `ScrollTransitionIsPending()`.

**Lesson (22).** *A fix that is inert measures exactly like a fix that is
wrong.* B24's first attempt keyed off `camera_target` being the vehicle, which
it never was, because the state that claims the camera had already exited. The
change did nothing and the bug reproduced unchanged, which read as "wrong
diagnosis"; it was in fact a correct half waiting on a precondition. Before
concluding a fix failed, check that its precondition ever held — a trace of the
guard is cheaper than a new theory.

## B25 — the rolling barrel comes back as noise after a pause *(fixed)*

Pause inside Deepwood's InsideBarrel and close the menu: the barrel returns as
fine yellow/blue tile noise. **Reported 2026-08-08 with a recording, fixed the
same day.**

**Not the expansion.** It reproduces identically at 240x160 from a warp-in,
pause, unpause fixture — the fifth defect this milestone that was live in the
shipping build all along.

**Measured rather than guessed, and the first two guesses were wrong.** The
room's enter handler *is* re-run on the way out — `RestoreGameTask` ->
`sub_0801AE44` -> `gArea.onEnter` — which was confirmed by tracing, so
"the handler never runs" was out. Re-running the handler a second time after
the palette restore changed nothing, so "the palette backup clobbers it" was
out too. Checksumming the three candidates across the pause settled it in one
run:

| | before | after |
|---|---|---|
| BG2 character data (`0x06000000`, 16 KB) | `AA105DE5` | `AA105DE5` |
| BG2 map (screenbase 28, `0x0600E000`) | `EB1BBC50` | **`0A853C7E`** |
| BG palette | `101EF5AB` | `101EF5AB` |

Only the map moves. **The tiles and the palette were never the problem.**

**Root cause: a port-only line writing a text tilemap into an affine map.**
`RestoreGameTask` ends with a `#ifdef PC_PORT` block that forces
`sub_08016CA8` on BG0, BG1 and BG2 to push the staging buffers into VRAM,
added because the GBA mechanism that does so after a map subtask does not fire
here. In GBA display mode 1 or 2 that layer's screenbase does not hold a text
tilemap at all — it holds a one-byte-per-tile *affine* map, which the room's
own handler loads straight from a gfx group (`LoadGfxGroup(0x16)`, whose second
entry lands on exactly that screenbase). The forced copy overwrites it. That is
also why re-running the handler did not help: the handler reloads the map
correctly and this line then destroys it, one step later.

**Two layers, and they fail differently — which is why this took two passes.**
`LoadGfxGroup(0x16)` writes *four* destinations, and two of them are maps:

| dest | size | what |
|---|---|---|
| `0x06000000` | 16384 | BG2 character data (BG2CNT `0xBC82`, charbase 0) |
| `0x0600E000` | 4096 | **BG2's affine map** (screenbase 28) |
| `0x06004000` | 8192 | BG1 character data (BG1CNT `0x5E86`, charbase 1) |
| `0x0600F000` | 2048 | **BG1's map** (screenbase 30) |

Overwriting BG2's affine map with a text tilemap turns the barrel into noise —
the reported symptom. Overwriting BG1's map is quieter: BG1 carries the wood
grain, alpha-blended over the staves (`layerFXControl = 0x3456`,
`alphaBlend = 0x909`), so losing it leaves the barrel legible but flat.

**Fixing only BG2 left exactly that behind, and it was mis-read as a palette
problem** — the palette buffer checksum was identical across the pause, the
colour *count* had changed, and "different colours" was the obvious reading. It
was the maintainer who named it: the grain lines were simply absent. The lesson
is that a colour-count delta says "the image changed", not "the palette
changed", and the two were only distinguishable by someone looking at it.

Fixed by skipping **both** the BG1 and BG2 copies when the display mode makes
the room affine. BG0 keeps its copy: it carries the HUD and the text box, which
are genuinely buffer-driven in every room including this one. The mode read is
the room's own, because the handler has already applied it by that point.

**Verified**: on a stationary fixture the frame after the pause is
**pixel-identical to the frame before it — 0 differing pixels of 76800** — and
on the maintainer's own recording the colour count matches at 107 either side.
Gate 11/11, `fetches=265497600 mismatched=0`.

**Lesson (23).** *Fix every layer the room owns, not the one whose symptom you
can see.* B25's forced buffer copy destroyed two of the rolling barrel's maps.
The affine one turned the room into noise and was fixed first; the other only
removed an alpha-blended detail layer, which read as "different colours" and was
chased as a palette problem until the maintainer said the wood grain lines were
missing. A colour-count delta says the image changed, not that the palette did.
When a handler loads whole layers from a gfx group, enumerate its destinations —
`LoadGfxGroup(0x16)` writes four and two of them are maps.

## B26 — Hyrule Town scenery drawn from the wrong tileset past a camera threshold *(fixed)*

Walking up and down in festival Hyrule Town, the stump tables below and right
of the player turn to garbage past a particular y, and come back clean when you
walk down again. **Reported 2026-08-09 with a recording.** Reported as a sprite
glitch; it is not one.

**Three hypotheses died before the right one, each killed by a measurement
rather than by argument.** Worth listing, because all three were plausible and
two of them were about budgets the wider viewport really does strain:

| hypothesis | measurement | verdict |
|---|---|---|
| sprite gfx slots exhausted | peak 17/40 at *both* sizes, 0 allocation failures | dead |
| 128-entry OAM cap hit | peak 82/128 at 320x240, 59/128 at 240x160, **0 cap hits** | dead |
| screenblock cannot cover the viewport (the B5/B15/B17 family) | `mapsrc_mask=0x6`, both world layers **bound** to full-room map sources | dead |

**It is not a sprite at all.** Re-rendering the glitch frame with each layer
disabled and measuring how much of the table region changed: OBJ 4.1%, BG0
0.6%, **BG1 38.5%, BG2 55.5%**, BG3 0.0%. Rendering BG2 alone shows the garbled
shape. The tables are background tiles.

**The map is not involved either.** The special-map entries for the table
(`C064 C065 / C068 C069` at map 54-55, 112-113) are written once when the room
loads and never touched again for the rest of the recording. What changes
underneath them is the *character data* those tile indices point at:

| camy | gfx group | tile checksum |
|---|---|---|
| 513 | 1 | `6885ECC2` (correct) |
| 510 | 0 | `9811A795` (corrupt) |

flipping back and forth on every up/down cycle, with the boundary at camy
511/512.

**Root cause: `CheckRegionsOnScreen` takes the first region that touches the
screen, and the region tables are authored for a 160-row screen.**
`hyruleTownTileSetManager` swaps tile graphics by camera position across a
1008x960 room; its `regions0` covers y 0..512 for group 0 and y 640..960 for
group 1, with a deliberate **128 px gap** between them. A region begins matching
at `camy > regionBottom - VIEWPORT_HEIGHT`, so:

| viewport | both regions match for | region-1 scenery visible at camy 511 |
|---|---|---|
| 160 | camy 480..512 | 31 px |
| 240 | camy **400**..512 | **111 px** |

The gap is sized to absorb a 160-row screen's ~31 px overhang. A 240-row screen
overhangs 111 px, so it is looking at real second-region scenery while the first
region's tileset is still loaded. `512 = 0x200` is region 0's own bottom edge,
which is why the threshold sits exactly there.

**Fix: ask the region test about the screen the data was authored for.** The
rule was never wrong — taking the first region that touches is load-bearing,
because one of these lists is a specific box in front of a whole-room default,
i.e. an override where order is the entire point. What is wrong is the *screen*.
A region starts touching at `cam > regionEdge - VIEWPORT_SIZE`, so 80 extra rows
make every region trigger 80 px of camera travel early and the manager loads a
tileset for scenery still only in the periphery.

For the same player position the GBA's camera sits `UI_CENTER_DX/DY` inside the
expanded one — that is what centring a 240x160 view in a larger viewport means —
so **the GBA's screen is exactly the centred `DISPLAY_WIDTH x DISPLAY_HEIGHT`
sub-rect of ours.** Testing regions against that sub-rect reproduces hardware's
selection by construction. At GBA-native size `UI_CENTER_*` are zero and
`DISPLAY_*` are `VIEWPORT_*`, so it is the original test unchanged and needs no
`#if`.

**Two cleverer rules were tried first and both shipped before being disproved,
which is the lesson here.** Simulating all five of Hyrule Town's region lists
over 43,000 camera positions and comparing against the GBA's own choice:

| rule | disagreements with hardware |
|---|---|
| first-match on the full viewport (pre-fix) | 4398 |
| max-overlap (first attempt) | 8316 |
| max-overlap restricted to disjoint regions (second attempt) | 3897 |
| **first-match on the centred 240x160 sub-rect** | **0** |

Both invented rules are *worse than doing nothing* on the override-shaped
lists — max-overlap catastrophically so, because a whole-room default always has
full-screen coverage. Each passed the recording in front of it and failed the
next one: the stump tables at camy 511/512, then `graphicsGroups[2]` flipping
4<->5 at 511/515, then again at 191/195 against a wall. Three reports, one
mechanism, two wrong fixes.

**Verified on all three recordings.** Every group change now happens while
walking *across* town, and none during the up-and-down walks that provoked the
reports: the wall recording's last change is frame 1563 and its up/down section
runs 2231-3903 with zero. Gate 11/11, `fetches=265497600 mismatched=0`.
`minishVillageTileSetManager` is the only other caller and gets the same fix.

**Lesson (25).** *When authored data assumes a screen size, give it that screen
— do not invent a better rule.* Two attempts here replaced the engine's
selection rule with something that reasoned about geometry, and both were worse
than the original on some lists while fixing others, because one rule cannot
serve a partition and an override at once. The answer was to leave the rule
alone and correct its *input*: the GBA's screen is the centred 240x160 sub-rect
of ours, and against that the original rule is exactly right everywhere. When a
fix needs a case analysis of the data it reads, that is the signal it is at the
wrong level.

**Lesson (25a).** *Simulate the whole table set before changing a selection
rule.* Both wrong fixes could have been rejected in minutes: the five region
lists and the camera range are static data, and scoring a candidate rule against
hardware's choice over all of them is a short script with no game runs in it.
Two playtest round-trips were spent discovering what that script printed in one
go.

**Lesson (24).** *A table of authored regions encodes an assumption about how
much of the world fits on screen.* Hyrule Town's 128 px gap is exactly that
assumption written down, and it is invisible until the screen grows. When
region data drives a resource swap, check the gaps against the viewport before
trusting first-match-wins — and note the three budget hypotheses above cost more
than the region table would have, had it been read first.

## B27 — scenery in the outer 40 px is drawn from a non-resident tileset *(fixed: Hyrule Town, festival town and Minish Village; playtested 2026-08-11)*

The residual after B26, reported as a fourth Hyrule Town glitch on 2026-08-09.
**It is not a selection defect and no selection rule can fix it.**

B26 made `CheckRegionsOnScreen` reproduce the GBA's own choice exactly. This
recording proves it: the tileset group flips at camy 710/713 at 320x240, and the
*shipping 240x160 build flips the same group on the identical frames* (1374,
1449, 1510, 1606, 1726 ...) at camy 750/753, which is camy+40. **The swap is the
game's own behaviour.**

And the rendering agrees. Comparing the centred 240x160 region against the
shipping build frame for frame, the only differing rows are **7..36 and
147..156** — the two HUD bands, which are deliberately repositioned. World
content is pixel-identical.

**So what is wrong is the 80 extra rows and columns.** Matching hardware's
choice only guarantees correct tiles for the 240x160 the GBA would have shown;
the periphery displays world the GBA never had on screen, and near a region
boundary that scenery belongs to a region whose tileset is not resident. One
tileset can be resident and the window is now bigger than one covers.

**Options were costed on 2026-08-09** and all the cheap ones are closed — see
§2 of the plan for the numbers, which include the arithmetic showing the region
gaps cannot be widened (over by 32..112 px in a 960 px room) and the tile census
showing the two groups differ in 242..256 of 256 tiles with 0..33 spare indices.

**The maintainer has refused to give up the 320x240 view in these areas**, so
the chosen path is to enlarge the *emulated* VRAM past the GBA's 96 KB, keep
both groups resident, and pick the tileset per tile from the tile's own room
position — possible only because both world layers here are bound to a full-room
map source, so the renderer knows the room coordinates the region tables are
expressed in. That removes the map re-indexing that made this look expensive.

**`docs/town-tileset-residency.md` was the implementation plan** and now records
what was built and what is left.

### The fix, 2026-08-10

**Both of a slot's tilesets are kept resident and the renderer picks between
them per tile, from the tile's own room position.** Four pieces:

1. `gVram` grows a **shadow bank** above the GBA's 96 KB
   (`PORT_VRAM_SHADOW_OFFSET`, port_gba_mem.h). Every `gba_read/write` guard
   still stops at `0x06017FFF`, so no engine access can see or reach it. The
   bank mirrors the whole 96 KB rather than the 24 KB town needs, so the offset
   is **one constant** whichever charbase window a tile falls in.
2. `VirtuaPPUMode1CharSlot` (mode1.h) lets the host say "tiles whose character
   data is in `[addr_lo, addr_hi)` and whose room position is in this region
   read at `+offset`". Regions are in tile units, first-match-wins, with a
   `fallback` for a tile matching none.
3. `mode1.c` resolves the offset **once per tile column** and adds it to the
   character address. The bound stays `MODE1_VRAM_SIZE`, so exactly the same
   addresses read as colour 0 as before; the offset only relocates a read that
   was already legal.
4. `port_tileset_residency.c` copies the alternative group into the bank and
   publishes the slots, driven by `hyruleTownTileSetManager`.

**The slot is found by the tile's character address, not by its position.**
Hyrule Town runs *three* independent region tables over the same room at once —
a tile at (0x100, 0x100) is in `regions0`'s group 0, `regions1`'s group 2 and
`regions2`'s group 4 simultaneously. Which one governs depends on whether its
tile index lands in slot 0's, 1's or 2's VRAM window. Position alone cannot
answer it, and a merged region list would be wrong everywhere.

**The engine's camera-driven swap is deliberately left running.** The plan said
to make it a no-op above native size; that is worse, and the first attempt at it
is the lesson below. Re-pairing on each swap instead makes "whichever group the
manager just loaded is the one in the GBA's own VRAM" true *by construction*,
every time, with no state to go stale.

**Tiles in the authored gaps between regions follow the engine's own camera
choice**, via `gRoomVars.graphicsGroups`, which the manager still maintains. The
gap is exactly where the data declines to say which group a tile belongs to, and
on hardware it is rendered from whichever group is loaded — so this reproduces
hardware there rather than inventing a rule (lesson 25).

**Evidence.**

| check | result |
|---|---|
| canonical route, 240x160 | **11/11, 0 differences** |
| map-source audit, 240x160 | **`fetches=265497600 mismatched=0`** |
| town capture at 240x160, before vs after | **byte-identical**, 21 frames |
| centred 240x160 sub-rect of the 320x240 frame vs the 240x160 build | differs only on rows **6..36 and 147..156** — the two HUD bands |
| frame time, `fourth_town_glitch`, n=3 | present mean **9.53 -> 9.69 ms (+0.16)**, p99 11.08 -> 10.71, logic unchanged |

The periphery itself is the one thing no gate can score, so it was checked
against the game's own rendering of the same tiles. At frame 1447 group 5 is
resident and the bottom 40 px belong to `regions2`'s whole-room default, group
4. The **shipping build paints that band from group 5** — green bush tiles. The
fixed build paints it from group 4, and that is **pixel-for-pixel what the
shipping build itself paints at frame 1451**, once the camera crosses the
threshold and group 4 becomes resident. The band stops changing when only the
residency changes, which is the whole claim.

The +0.16 ms is against the plan's +0.3 ms tripwire, so the per-pixel tilemap
fetch at `mode1.c` was left alone as agreed. The offset is resolved once per
tile column — 40 lookups a line rather than 320 — because within a column the
tile entry, and so the index, and so the slot, cannot change.

**Festival town's tables convert correctly and the mechanism engages there**
(reached by debug warp to area 0x15; `festivalRegions0` -> px 0..399/0..463 and
0..399/672..959, `festivalRegions2` -> 0..399/432..751, all exact). That is a
code-path exercise, **not a playtest** — nobody has walked festival town's
region boundaries at 320x240 and no recording of it exists.

**Lesson (26).** *Prefer an invariant re-established by the thing that can break
it, over state that records what that thing did.* The first version of this
suppressed the manager's swap and remembered which group was resident. It was
wrong within one recording: something moved `gRoomControls.room` without the
manager's entry path running, a room-identity backstop fired, and it silently
dropped slot 0 mid-room — the trace showed gfx 2 re-pairing over and over while
gfx 0 never came back. Letting the swap run and re-deriving the pairing from it
each time has no such window, costs one 8 KB copy per swap, and made the whole
change additive. The bug was visible only because the trace printed *every*
declaration rather than the final state.

**Lesson (27).** *A metric that moves for the ordinary reason cannot measure the
extraordinary one.* Two attempts to score this defect numerically failed the
same way: whole-frame pixel deltas across the group swap were ~21,000 pixels
both before and after, because one pixel of camera scroll already changes that
many. The defect was settled by rendering the same tiles under both residencies
and comparing them to each other — a comparison with the camera held still.

### Minish Village, 2026-08-11

**Reported with six recordings on 2026-08-11 and fixed the same day** — four,
then two more against the first build of the fix, then two interiors against
the build after that. **The maintainer playtested the result and confirmed it
2026-08-11.** That is worth separating from everything measured below: the
numbers say the periphery stopped changing and matches the centre, and the
playtest says the areas look right to someone playing them, which no
measurement here establishes. The
maintainer asked first for proof the glitch could be *seen* — absence and
presence — before anything was changed. That is why the numbers below are
paired: the same measurement with the selection suppressed and applied.

Each recording parks Link at one threshold and walks across it repeatedly, so
the camera oscillates by a pixel or two and the group flips with it. That gives
frame pairs one frame apart — and in the first recording, where the region gap
produces hysteresis, two frames at the *same* camera with different groups.

**Two measurement traps, both hit before the numbers meant anything.** Whole-
frame deltas are useless here for the reason lesson 27 gives. Worse, the
obvious refinements are also wrong: two frames a pixel of camera apart must be
shifted a pixel to compare, and that shift misaligns everything drawn at a
fixed *screen* position, so the HUD reports as a difference along every edge it
has — and sprite animation differs between any two frames at all. Both land in
the same periphery the defect does. `TMC_DISABLE_OBJ` and `TMC_DISABLE_BG0`
exist to take them out of the picture; with them, the numbers are clean.

| recording | groups | palette groups | periphery changes, fix off | fix on |
|---|---|---|---|---|
| 1 | 4 ↔ 1 | 0x18 / 0x17 — **differ** | 10.67% | **0.00%** |
| 2 | 0 ↔ 1 | 0x16 / 0x17 — **differ** | 7.58% | **0.00%** |
| 3 | 1 ↔ 2 | 0x17 / 0x17 — same | 2.24% | **0.00%** |
| 4 | 4 ↔ 3 | 0x18 / 0x18 — same | 2.41% | **0.00%** |
| 5 | 0 ↔ 1 | 0x16 / 0x17 — **differ** | 7.58% | **0.00%** |
| 6 | 1 ↔ 2 | 0x17 / 0x17 — same | 2.24% | **0.00%** |

The centred 240x160 measures 0.00% in every case both before and after, which
is what makes this the expansion's defect rather than the game's.

**Zero is stability, not correctness, so one more check.** On recording 4's
shoreline band — screen rows 0..39, entirely inside `gUnk_08108050`'s group-4
rectangle — against the frame where the unfixed build has group 4 loaded and
therefore draws it right:

| | |
|---|---|
| unfixed, group 4 loaded, vs unfixed, group 3 loaded | 7.35% — the glitch |
| unfixed, group 4 loaded, vs **fixed**, group 3 loaded | **0.00%** |
| unfixed, group 4 loaded, vs fixed, group 4 loaded | **0.00%** |

So the fix draws exactly what the game itself draws once the right group is
resident, and leaves the already-correct case alone. That comparison has to be
scoped to one region's band: **no single unfixed frame is a correct reference
across the whole periphery**, because two or three groups are on screen at once
and only one can be loaded — which is the defect stated as a measurement
problem.

**The better oracle, found on recordings 5 and 6, is to walk the same world
content into the centre.** Whatever the periphery ought to look like, the GBA
draws it correctly once the camera brings it inside the centred 240x160 — so
capture that frame and compare the same world rows or columns against it. It
needs no assumption about which group is loaded when, and it is what caught
the palette-mask bug that "0.00% across the threshold" had passed:

| | unfixed | fixed |
|---|---|---|
| recording 5, top 40 rows vs the centred reference | 34.85% | **0.00%** |
| recording 6, left 40 columns vs the centred reference | 8.39% | **0.00%** |

**And a third report, of two Minish Village *interiors* rendering with the
wrong palette** — different areas entirely, which the residency has no business
touching. It was retiring a slot wrongly. When the room no longer matches, the
published entry was blanked in place: `count = 0`, `fallback = 0` — and
`fallback_palette_set` left as it was, because blanking has to remember every
field and that one was added later. The address range stayed live, an
interior's tiles share the village's character addresses, so every one of them
matched a slot that had nothing to say about them and took a village palette.
57% and 38% of those two rooms.

The published array is now **rebuilt from the slots on every publish** rather
than edited in place, so a slot that does not apply is simply absent. That also
fixes the trap in the obvious repair: emptying the address range instead would
have stranded the slot, because `DeclareSlot` early-outs when the room matches
what it recorded and would never have put the range back.

**Lesson (29).** *Stability is not correctness, and a metric that only compares
two peripheral frames can only measure stability.* Recordings 5 and 6 scored
0.00% across the threshold — the number this work had been using throughout —
while the ledge was plainly the wrong colour, because the palette was wrong on
*both* sides of the flip. The centred view is the reference that was available
all along.

**Lesson (30).** *Prefer rebuilding derived state to blanking it.* Both of this
entry's own regressions are the same shape: a value carried from a moment when
it was right into one where it was not, because the code that was supposed to
neutralise it enumerated fields — the palette-bank mask derived by diffing, and
the retired slot blanked in place. Neither list was wrong when written; both
were outgrown. Deriving the whole thing from its inputs each time has no list
to keep current.

Three things had to change beyond what town needed.

**1. N-way residency.** Minish Village has five alternatives for the same
addresses, not a pair, and 72 of ~8,400 camera positions need three of them at
once. Banks are now indexed by group id, six of them, and a slot's regions each
carry their own offset — so "how many at once" stopped being a design question.

**2. No group reads the GBA's own VRAM here.** Town names a resident group so
the second oracle house — an overlay written into a slot's range behind the
manager's back — survives. Minish must not: the manager owns the whole
character range *and stages its load over eight frames*, so for eight frames
VRAM holds half the outgoing group and half the incoming one and is honestly
neither. `PORT_TILESET_NO_RESIDENT` makes every group read its own bank. **This
was found the hard way**: the first version named the staged group as resident,
and the frames the maintainer's recordings pause on sit inside exactly that
eight-frame window, so the fix appeared to do nothing at all. The tile probe
(`TMC_TILE_PROBE`) showed `offset=0` on a tile whose region plainly matched,
which is what pointed at the lag rather than at the region test.

**3. The palette.** Each group also loads a 13-bank BG palette group. A
per-tile character offset alone draws the right shapes in the wrong colours, so
`VirtuaPPUMode1CharRegion` carries a palette set too, and the port keeps one
shadow BG palette per group. Those are rebuilt **every frame from inside
`FadeVBlank`**, right after it writes the live palette: the palette is faded per
bank every VBlank, and a snapshot taken at room entry would leave the periphery
bright while the rest of the screen faded. `Port_FadeApply16` is the fade
transform split out of `Port_MakeFadeBuff256` so both can use it rather than
keeping two copies in step.

**Which banks belong to a group has to come from `gUsedPalettes`, not from a
diff.** Only the banks a group's palette group writes are its own; the rest
must follow the live palette. The first version worked out which those were by
comparing the loaded result against what had been loaded before — and that is
wrong in a way that hides. **The group whose palette is already live diffs to
nothing**, so its mask comes out empty, every one of its banks then tracks the
live palette, and its tiles turn whatever colour the current group is. Which is
the defect this exists to fix, one level down. `LoadPalettes` already records
exactly the banks it touched in `gUsedPalettes` — including on the port's asset
path, which goes through the same function — so zero it, load, and read it
back. The correct mask is `0x7FFC` for all five groups: banks 2..14, which is
what `palette_groups.json` declares.

This is what recordings 5 and 6 were: reported 2026-08-11 against the build
that had the first version in it. Recording 5's ledge and ladder had the right
shapes in the wrong colours — the maintainer's diagnosis, "purely a palette
issue", was exactly right and named the bug.

Gate 11/11 and `fetches=265497600 mismatched=0`. Town's flip frames are
byte-identical to the ones the town fix was verified on, across both the
n-way refactor and the palette change. Frame time in town is 9.42-9.53 ms
against 9.50-10.16 before B27 — no cost; Minish Village measures 9.51 ms.

**Lesson (28).** *When a fix looks like it did nothing, check that it ran on the
frame you are looking at.* The character half worked from the first build, but
every frame the recordings were sampled at fell inside an eight-frame staged
load, where the port's idea of what VRAM held was two groups out of date. Three
rounds went into the region test, the address ranges and the charbases — all
correct — before a probe printed the offset actually chosen for a tile.

### How the plan mis-costed Minish Village

Kept because the plan is otherwise a good record and this is where it was
wrong. All three were measured from `gUnk_08108050`, `gUnk_081080A4` and
`gUnk_081081E4` before any code was written:

| | plan assumed | actually |
|---|---|---|
| groups on screen at once, 320x240 | 2 | **3** — 72 of ~8,400 camera positions, worst case `{0,1,4}` at cam (152,160); 2 groups at 2,595. Same for the EU table. |
| bytes per group | 8 KB | **32 KB** (8 x 4 KB blocks), five alternatives -> 128 KB of shadow for full residency against town's 24 KB |
| what a group swaps | tile graphics | tile graphics **and a 13-bank BG palette group** — `gUnk_081081E4` maps groups 0..4 to palette groups 0x16/0x17/0x17/0x18/0x18, each loading 13 palettes into banks 2..14 |

The palette was the one that changed the shape of the work, and the estimate
was right about that: per-tile *tileset* selection alone would have drawn the
right graphics in whatever colours were loaded. What the estimate got wrong was
the price — the palette path is one field on a region plus a per-frame rebuild
inside the fade, because the fade transform was already factorable.

**Still worth knowing:** at 240x160 two palette groups are needed at once in 308
camera positions, so a little of this is the shipping build's own behaviour. It
has never been looked at, and the 240x160 play build is where to look.

**Note, argued rather than observed:** `BuildSecondOracleHouse` writes tiles to
`BG_SCREEN_ADDR(3)` = `0x1800`, which is inside slot 1's character range, so
that overlay lives in whichever copy was resident when it ran. A later swap
re-copies group 2's *original* bytes into the bank and would lose it. It is
unreachable: `CheckRegionsOnScreen` picks group 3 only for `camX >= 360`, and
the oracle house sits at room x 40..88, off screen by then. If slot 1 ever
misbehaves near the oracle house, start here.

## B28 — Lon Lon Ranch's locked door is walk-through, its neighbour is an open doorway *(fixed)*

Reported 2026-08-17 with a recording (`lon_lon_ranch_door.script`) ending with
the player facing the ranch house's left-hand door. Two symptoms, one cause:

- the **left** door should be locked until Talon has his key back, and the
  player walks straight through it into the bedroom;
- the **right** door should show as a blue door and instead shows the map's
  bare black doorway.

**Identical at 240x160.** The first question this build exists to answer said
"not the expansion" in one run, and that is where the search should have
started rather than in the renderer — the second symptom looks like a drawing
bug and is not one.

### Root cause: extracted room properties are cut at their embedded pointers

House doors are not map tiles. A room property holds a list of 12-byte door
records, `HouseDoorExterior_Type0` (`src/object/houseDoorExterior.c`) walks it
and spawns one child entity per door that is on screen. The list for
Hyrule Field / Lon Lon Ranch is property `0x11`, which the entity table names
`gUnk_additional_11_HyruleField_LonLonRanch`:

```
  x=0x148 y=0x263 layer=1 sub=0 type2=3 type=5  script=script_LonLonRanchDoor
  x=0x178 y=0x263 layer=1 sub=0 type2=2 type=5  script=NULL
  0xFFFF ...                                     terminator
```

36 bytes. The extracted asset the port actually read was **8 bytes** — the
first record with its script pointer chopped off.

The decomp emits a pointer inside a data blob as `.4byte <symbol>` between two
`.incbin`s, because the address is a link-time relocation and not a byte the
`.bin` can carry. `port/port_asset_index.c` is generated from those `.incbin`
directives, so one symbol appears in it as several fragments with an
**unindexed four-byte hole** wherever a pointer sits:

```
{ 0x000F7A20, 0x0008, "…/gUnk_additional_11_HyruleField_LonLonRanch.bin"   },
                    ← 0x000F7A28..2B: script_LonLonRanchDoor, in no entry
{ 0x000F7A2C, 0x0018, "…/gUnk_additional_11_HyruleField_LonLonRanch_1.bin" },
```

`extract_area_tables` sized each room property from the single index entry at
its address, so the property stopped at the first pointer. Everything after it
— including the whole second door — came from whatever the heap held past the
end of an 8-byte buffer.

From there both symptoms follow mechanically:

- Record 0's script address is read four bytes past the buffer, so
  `Port_ResolveRomData` gets a host pointer and returns NULL, the spawner never
  calls `StartCutscene`, and `HouseDoorExterior_Type3` returns at its
  `context == NULL` guard. A Type3 door that returns there never reaches
  `sub_080868EC`, which is what calls `sub_0800445C` — the push-the-player-out
  collision — on every frame the door's `frameIndex` is 0. **The door drew
  itself perfectly and was not there.**
- Record 1 is never read as a record at all, so no entity is spawned at
  x=0x178 and the map's own black doorway is what you see. The right door was
  never "drawn wrong"; nothing was drawn.

The three port-only guards in `houseDoorExterior.c` are all this bug. Each was
added against a symptom, each named a cause that did not exist ("EntityData
read the wrong byte for type2's high half"), and between them they converted a
crash into a silent wrong-looking door — which is why it survived to be
reported as a rendering glitch. `01948f13` had the right instinct and the wrong
model: it read the short buffer as a *native 16-byte struct* rather than a
truncated packed one, which is why it was reverted twice without ever fixing
anything.

### It is already on the issue tracker, three times

`CHANGELOG.md`'s still-open list carries **#37 "Lon Lon Ranch shrink/door"**,
**#40 "Hyrule Town door texture"** and **#28 "random door above staircase"**.
The first two are this bug reported from the two rooms above. The third is its
other half: a garbage record that happens to pass `CheckRegionOnScreen` spawns a
door at a nonsense position, which is exactly what "#28, #29, #30" meant in the
comments the port left in `houseDoorExterior.c`. All three should be gone — the
lists now terminate where the data says they do, so there are no garbage records
left to spawn from. **#21 "Link's house glitched doors"** is worth re-testing
against this too; it was not investigated here.

### Reach — four properties, three areas

| Area / room | Prop | Consumer | Was | Now |
|---|---|---|---|---|
| Hyrule Field / Lon Lon Ranch | `0x11` | house doors | 8 B — 1½ of 2 doors | 36 B — both |
| Hyrule Town (`Room_HyruleTown_0`) | `0x0c` | house doors | 44 B — 3½ of 15 doors | 192 B — all 15 |
| Cave of Flames / boss door | `0x08` | lava platforms | 32 B — 2 of 10, then off the end | 176 B — all 10 |
| Cave of Flames / Rollobite | `0x08` | lava platforms | 4 B — *the pointer word only* | 32 B — its 1 platform |

Only Lon Lon Ranch was seen fixed. **A debug `warp 0x02 0x00` does not reach
`Room_HyruleTown_0`** — it lands in `Room_HyruleTown_1`, which the runtime
reports as area 21 (`AREA_FESTIVAL_TOWN`) room 0 and which reads its doors from
property `0x08` (`gUnk_additional_8_HyruleTown_1`, a macro-emitted list that was
never truncated). Both signals agree: the trace names property 8, and only
`Entities_HyruleTown_1_1`'s door parent asks for it. That is also why the gate's
`town` waypoint could not have moved, and it means the town half of this fix is
**verified in the data and not on screen**. `Room_HyruleTown_0` is the everyday
town — every `destArea=0x2` exit lands there, including the ones out of Dr.
Left's, Romio's and the Cucco house — and record 3 of its list carries
`script_DrLeftDoor`, the same Type3-with-a-script shape that failed at the
ranch.

The Cave of Flames pair are the same defect in `lavaPlatform.c`, which walks
16-byte records to a `raw[9] == 0xff` terminator: neither truncated blob
contained one, so both rooms were reading past their buffers on entry. The
Rollobite room's blob begins *with* a pointer, so its indexed fragment starts
four bytes in and the extractor fell back to the boundary rule and wrote the
pointer word alone.

### The fix

`infer_room_property_size` in `tools/src/assets_extractor/assets_extractor.hpp`
rejoins the fragments: take the entry at the property's address (or, when the
symbol opens with a pointer, the one four bytes in), then keep absorbing
`four-byte hole + indexed entry` pairs. **Only a hole continues the walk** — an
indexed entry that starts exactly where the previous one ended is the next
symbol, not the next fragment, and `gUnk_additional_8_HyruleTown_1` sits flush
against the end of `gUnk_additional_c_HyruleTown_0_2` waiting to be swallowed
by a rule that does not check. The continuation's *name* is checked too
(`X.bin` → `X_1.bin` → `X_2.bin`), so the walk models "fragments of one decomp
symbol" rather than "whatever is four bytes later".

Rejoined blobs are written to the generated `room_properties/offset_XXXXX.bin`
path instead of the indexed one. An indexed path must keep the size the index
gives it — the decomp's own `.incbin` of that file depends on it — so the
merged view gets its own file and the fragments keep their identity.

**A fix in the extractor reaches nobody who is past first run**, because
`RuntimeAssetsUpToDateImpl` only fingerprints the ROM, and the ROM has not
changed. `kExtractorFormatVersion` (`assets_extractor_api.cpp`) is now recorded
in `.asset_build_state.json` and compared on launch, so an existing `assets/`
re-extracts once. Both play builds and the APK's baked tree were regenerated
this way; the four blobs are byte-identical to ROM at their addresses.

### Evidence

- Same input script, same 320x240 binary, only the door list differing: before,
  the player is inside the bedroom by frame 3800; after, he is still outside
  after 500 frames of holding *up* against the door, which rattles and stays
  shut. The right door is blue in the same frames.
- Regression gate at 240x160: canonical route 11/11 pixel-identical, map-source
  audit `fetches=265497600 mismatched=0`. The route's `town` waypoint is not in
  the affected room at all — see above — so the gate says nothing either way
  about the town doors, and everything about nothing else having moved.

### Worth keeping

**A defensive guard whose comment names a cause nobody confirmed is a bug that
cannot be found.** Three of them here each turned a crash into plausible-looking
output, and the report that eventually came in described the *rendering*. When
adding one, say what was actually observed and leave the failure loud enough to
be traced.

**Extracted assets are not the ROM.** Anything the decomp writes as a
relocation — every `.4byte <symbol>` inside a data blob — is a byte the
extraction pipeline cannot represent and must reconstruct. Room properties are
the case found; the same shape exists anywhere an `.incbin` run is interrupted
by a symbol reference. Scanning `data/map/entity_headers.s` for symbols whose
body mixes `.incbin` with `.4byte` enumerates every one of them — there are
five, and it takes seconds.

**When the data layer is fixable, fix it there.** Every reader of these
properties — the door spawner, `lavaPlatform.c`, anything found later — was
wrong for one reason, and none of them needed to change.

---

## B29 — the area-name banner never appears *(fixed)*

Reported 2026-08-18: the stylized area name that appears on first entering an
area — "Minish Woods" in white with a blue swirl either side, no enclosing box
— is simply absent. Present at 240x160 too, so the first question said "not the
expansion" again; that answer was **wrong this time**, and the reason it was
wrong is the interesting part.

### Root cause: Spike 6 relocated gBG0Buffer, and a Font can name it in *data*

The banner is ordinary UI text. `EnterRoomTextboxManager` (`sub_0805E1F8`)
picks one of two 24-byte GBA `Font` blobs in ROM — `gUnk_08108E30` for an
overworld name, `gUnk_08108E48` for a dungeon's red one — and hands it to
`ShowTextBox`. A GBA `Font` carries its destination as a raw 32-bit pointer;
both of these say `dest = 0x02034E0E`, which on hardware is row 5, column 15 of
`gBG0Buffer` at EWRAM `0x02034CB0`.

`Port_DecodeFontGBA` resolves that through `gba_TryMemPtr`, which mapped every
EWRAM address into `gEwram[]`. Until Spike 6 that was right, because
`gBG0Buffer` *was* `gEwram[0x34CB0]` — a `#define`, not an array. Spike 6 made
it a standalone array so a wide viewport could have more than 32 columns, and
from that commit on the banner has been rendered, correctly and completely,
into 2 KB of dead EWRAM that nothing draws.

Spike 6 knew about this hazard and went looking for it. Its commit message
records finding `phonograph.c`, which held a `Font` whose destination was the
literal `(u16*)0x2034fce`, and concludes "every other Font table names the
buffer symbolically". That was true of every Font *in C source* — which is
where a grep can look. It could not be true of a Font that exists only as
twenty-four bytes of ROM, whose `dest` is not a symbol, not a literal in any
file, and not present until `Port_DecodeFontGBA` assembles it at runtime.

### Why nothing caught it for three weeks

The canonical route enters five new areas and spawns a banner in each — the
mechanism runs. Every waypoint dump lands 300 frames after its warp, and the
banner lives 120. **The route exercises this every run and samples it never**,
so the gate reference (captured at Spike 0, two days *before* the regression)
still matches, both before this fix and after. 11/11 was never evidence about
this.

Adding a waypoint 60 frames after a warp would close that, at the cost of
regenerating the reference set. Not done here — the gate's definition is quoted
in several places and changing it is the maintainer's call.

### The fix

`gba_TryMemPtr` now maps `[0x02034CB0, +0x800)` onto the real `gBG0Buffer`
before its generic EWRAM case, so a GBA address naming the UI tilemap reaches
the UI tilemap however it arrives — decoded from a ROM Font blob, resolved by
`port_resolve_addr`, or written by a DMA.

The mapping is linear, which is only correct while the buffer keeps the GBA's
32x32 shape. It does, and `viewport.h` explains why: widening BG0 was tried and
abandoned, and the whole 240-wide layer is centred instead. That file also
warns that the failures from changing the stride are silent, so the mapping is
guarded by a `PORT_STATIC_ASSERT` on `UI_BG0_WIDTH_TILES` — a second attempt at
widening now stops at a compile error on that line instead of quietly writing
into the wrong rows.

### Evidence

- Both fonts, both viewports: "Minish Woods" in the overworld colour and
  "Deepwood Shrine" in the dungeon one, matching the reference screenshot the
  maintainer supplied.
- Position is exact rather than approximate. Diffing the frame against its
  pre-fix twin isolates the banner: bbox centre **x=120 of 240** and **x=160 of
  320**, i.e. centred at both sizes, and the 320 copy is the 240 one shifted by
  exactly `UI_CENTER_DX` (40 px).
- Blast radius is one font family. Tracing every `ShowTextBox` call across the
  canonical route: the four banner calls move into `gBG0Buffer`; the three that
  still resolve into `gEwram` target `gBG1Buffer` (0x02021F30 + 0x42, 0x12C,
  0x39E), which is still aliased there and correct.
- Gate at 240x160: 11/11 pixel-identical, `fetches=265497600 mismatched=0` —
  which, as above, says nothing about the banner and everything about nothing
  else having moved.

### Worth keeping

**A grep over source cannot see an address that only exists as data.** Spike 6
did the right search and got a complete answer to the wrong question. When
relocating something the GBA addressed by a fixed number, the search has to
cover ROM blobs the engine decodes at runtime — Fonts, room properties (B28),
anything with a pointer field — and the cheapest way to cover all of them at
once is to make the address resolver itself know where the thing moved to,
which is what the fix does.

**A regression gate that runs a mechanism is not a gate that covers it.** This
is the concrete case CLAUDE.md's "count the frames that exercise the mechanism"
line was written about, and even so it took a bug report to find.

---

## B30 — a tileset slot the camera never selects is never declared *(fixed)*

Reported 2026-08-18 with a recording (`hyrule_town_residual_glitch.script`).
Link stands at the Hyrule Town entrance; the sliver of building at the
lower-left edge is drawn from the wrong tileset, and pops to its proper tiles
the moment he walks far enough left. The residual B27 case, and the last one
the outer 40 px had left.

### Root cause

`HyruleTownTileSetManager_UpdateRoomGfxGroup` loads a slot only when
`CheckRegionsOnScreen` matches, and **B26 made that test the centred 240x160
sub-rect** — the screen the GBA would have had, which is what makes the
original first-match-wins rule right everywhere. A slot whose scenery only ever
appears in the outer 40 px therefore never matches: `gRoomVars.graphicsGroups`
keeps the `0xff` `OnEnterRoom` wrote, and `LoadGfxGroup` never runs.

**`LoadGfxGroup` is the only thing that declares a slot to the renderer.** So
the slot has no regions, no banks and no per-tile answer, and its tiles read
whatever the previous room left in that VRAM range. B27 kept every alternative
resident and chose per tile — but only for slots something had chosen a group
for at least once.

Traced with `TMC_TILESET_TRACE=2`, the whole bug is two lines:

```
[frame] 1250 area 0x02 room 0x00 cam 344,720 groups 1,255,4
[frame] 1258 area 0x02 room 0x00 cam 341,720 groups 1,2,4
```

Slot 1 sits at 255 from room entry until the camera has moved 3 px past the
threshold, 618 frames later.

On hardware this is unreachable and the engine is right not to load it: a
240x160 screen cannot show that scenery. Above native size it is on screen from
the frame the room loads.

### The fix

Declare the pair anyway. `HyruleTownTileSetManager_DeclareUnselectedSlot` runs
after the load pass and, for any slot still at `0xff`, makes its group pair
resident with **`PORT_TILESET_NO_RESIDENT`** — because this room put neither
group in the GBA's own VRAM, unlike the loaded case where the resident group is
named so the second oracle house survives.

Nothing else moves. Tiles in the authored gaps between regions still resolve to
`PublishForBg`'s fallback, which reads `gRoomVars.graphicsGroups[i]`, sees
`0xff >= PORT_VRAM_BANKS` and yields offset 0 — real VRAM, exactly what they
got before. The change only ever *adds* an answer where the region tables
already had one.

Festival town takes the same treatment for slots 0 and 2 and not for slot 1,
which is the split the load pass above it already makes: its slot-1 region list
is empty, so there would be nothing to answer with.

### Evidence

The oracle is the one B27 established — walk the same world content to where
the engine draws it correctly and compare — and here the recording supplies it
for free, because the pop *is* the engine catching up.

- The world column at screen x 0..8 before the camera moves, against the same
  world column 28 px right after it has: **29/448 px matching before the fix,
  448/448 after.** The tiles are not merely different, they are the ones the
  loaded tileset draws.
- Only the frames before the pop change at all: 419 px each in
  `x 0..8, y 120..176`, and 0 px in every frame after it.
- A 2 500-frame walk around town, 42 sampled frames: **0 differ.**
- All twelve 320x240 waypoints including festival town: **0 differ.**
- Gate at 240x160: 11/11, `fetches=265497600 mismatched=0` — the code is behind
  `VIEWPORT_TILESET_RESIDENCY`, which is 0 there.

### Worth keeping

**A fix that runs off an engine event only covers what the engine does.** B27
hung the port's per-tile machinery off `LoadGfxGroup`, which is the natural
hook and was right for every slot the camera selects — and silently absent for
the one class of slot the camera never selects, which is exactly the class the
wider viewport added. When a port mechanism exists because the viewport shows
more than hardware did, check what it does for the parts of the screen the
engine has no reason to think about.

**Its instrument already existed and named the bug in one run.**
`TMC_TILESET_TRACE=2` prints `groups` per frame; `255` in that line is the
whole diagnosis. Reaching for it first would have skipped the frame-diffing.

---

## B31 — every Hyrule Town slot is undeclared from room entry until its first swap *(fixed)*

Reported 2026-08-20 with a recording. Link stands still at the town entrance,
walks down, and a strip of scenery at the bottom of the viewport is drawn from
the wrong tileset until the camera crosses a region threshold, at which point it
pops to its proper tiles. The maintainer added the two facts that broke the case
open: **it persists while walking rather than flashing, and coming back to the
same spot later is stable.**

### Root cause: the init reset throws away what OnEnterRoom just declared

`HyruleTownTileSetManager_Main`'s first-frame branch called
`Port_TilesetResidency_Reset()` to drop the previous room's pairs. But the
manager entity is created a frame *after* `OnEnterRoom` has already declared the
new room's three slots, so the reset destroyed those instead:

```
[dbg] f=1059 RESET had 0 slots      <- OnEnterRoom, then declares gfx 0/1/2
[dbg] f=1060 RESET had 3 slots      <- Main's init, wipes all three
```

Nothing re-declares a *loaded* slot: `UpdateRoomGfxGroup` only calls
`LoadGfxGroup` when the camera changes the group, and `LoadGfxGroup` is the only
thing that declares. So all three slots stayed gone, and every tile in their
character ranges fell through `Port_TilesetResidency_OffsetFor` to offset 0 —
real VRAM, holding whatever group the *centred* 240x160 had selected. That is
the whole bug, and it explains each observation exactly: it persists because
nothing restores the slot; it ends at a swap because the swap re-declares; and
it stays fixed on a return visit because the declaration then survives.

Measured, the slots came back one at a time, hundreds of frames apart:

```
[dbg] f=1568 LOAD slot 1 group 3 declared=0
[dbg] f=1814 LOAD slot 2 group 5 declared=0
[dbg] f=5173 LOAD slot 0 group 1 declared=0      <- the reported pop
```

### The fix

Reset in that branch only when the slots are not already this room's, which is
what the reset was for. `Port_TilesetResidency_SlotDeclared` answers that.

### Why B30 hid half of it

B30's `DeclareUnselectedSlot` runs from `UpdateLoadGfxGroups` every frame, so
the slots it covers — the ones the camera never selects — were re-declared a
frame after the wipe and looked fine. The slots the camera *does* select had no
such path, so exactly those stayed undeclared. Fixing the visible half of a
defect can leave the other half looking like a separate bug, which is how this
came back as a fresh report.

### Evidence

- The reported corner: **29/448 px** matching the correct tiles before, **448/448**
  after — the same oracle B30 used, the pop being the engine catching up.
- Frame-by-frame across the walk, aligned for scroll: the single corner event at
  5173→5174 is gone and nothing replaces it.
- Both town recordings, 135 sampled frames: every difference is **outside the
  centred 240x160** — the bottom-right band in one, the top band in the other —
  i.e. the periphery now follows the region tables instead of the centred
  screen's group. That is the fix reaching further than the one corner reported.
- Gate at 240x160: 11/11 pixel-identical, `fetches=265497600 mismatched=0`; the
  code is behind `VIEWPORT_TILESET_RESIDENCY`, which is 0 there.

### Worth keeping

**Two wrong diagnoses cost most of this, and the instrument that settled it was
the one measuring the thing itself.** The first theory was a bank/VRAM flip at
the swap; the second was an in-flight `LoadResourceAsync`. Both were plausible,
both fit the one-frame pop, and both were wrong. What settled it was printing,
per tile, *why* the renderer chose what it chose — "no published slot holds this
character address" — and then asking who emptied the table.

**A no-op-looking A/B is worth running before believing a fix.** The in-flight
theory produced a fix that removed the pop *and* changed 384 px of unrelated
periphery on every frame. A null test — clean rebuild versus the installed
binary — showed 0 px, which proved the change was mine and not noise, and that
is what stopped it shipping.

**`TMC_TILESET_TRACE=2` is still the first thing to reach for**, but its `groups`
line only reports the engine's choice. A slot can be perfectly chosen and still
not be *declared*; that needs the residency side, not the manager side.

---

## B32 — MinishPaths parallax grass pops in as the camera scrolls *(fixed)*

Reported 2026-08-20 with a recording. On the Minish Village entry path — the
vertical corridor strewn with oversized acorns — the foreground grass blades on
either side arrive in discontinuous jumps instead of scrolling in. The room is
`AREA_MINISH_PATHS` room 0, **240x800**, so it is also one of the narrow rooms
the viewport centres: `camx` is pinned at -40 and only the vertical axis moves.

### Root cause: a 64 px re-base step against a 256 px screenblock

`VerticalMinishPathBackgroundManager` scrolls its two parallax layers by hand.
Each is a 32-tile screenblock — 256 px — and `sub_0805754C` keeps a fine offset
in `yOffset` while re-pointing `subTileMap` a whole 64 px at a time:

```c
bgOffset = scroll_y - origin_y;
bgOffset += bgOffset >> 3;                 /* BG3 runs 9/8, BG1 5/4 */
gScreen.bg3.yOffset   = bgOffset & 0x3f;
gScreen.bg3.subTileMap = gMapDataTopSpecial + (bgOffset / 0x40) * 0x200;
```

The window the block has to cover is `yOffset + screen height`, so the fine
offset may only range over `256 - height`. At 160 rows that is 96 and the
engine's 64 fits with room to spare. **At 240 rows it is 16**, and 64 does not:
for most of every cycle the bottom of the screen ran past the end of the block
and wrapped to its top, and each re-base swapped what that wrapped strip
contained. That is the pop.

It is the vertical, parallax cousin of B5/B15/B17 — "32 tiles cover 256 px, not
320" — with the arithmetic one level up: the layer never lost its map, the
*window into it* was sized for the GBA's screen.

### The fix

Re-base on a step small enough that the whole screen stays inside the block:
16 px, with the map pointer moving 2 tile rows (0x80) instead of 8 (0x200).
Position is unchanged either way — `64*(B/64) + (B&63)` and `16*(B/16) +
(B&15)` are both `B` — only how often the pointer moves differs, and a
`static_assert` on `MINISH_PATH_REBASE + VIEWPORT_HEIGHT <= 256` keeps the two
in step if either changes. At 160 rows the constant is still 64 and the code is
the engine's.

### Evidence, and the measurement that made it possible

Whole-frame diffs are useless here for a reason the earlier bugs did not have:
this scene has **three layers moving at three rates**, so no single alignment
exists and every frame pair reports thousands of differing pixels whichever
shift you pick. The pre-fix mean residual was 913 px per frame pair — all of it
parallax, none of it the bug.

`TMC_DISABLE_BG1/BG2/BG3` (new, beside the existing OBJ and BG0 switches) leave
one layer on. Against a single layer the question has an answer, and the answer
is exact: **a clean scroll is a residual of zero at some shift, on every
consecutive pair.**

| Layer, alone | Before | After |
|---|---|---|
| BG3, 109 consecutive pairs | one 5115 px discontinuity, mean 104.4 | **0 px on every pair, mean 0.0** |
| BG1, 109 consecutive pairs | one 5408 px discontinuity, mean 49.6 | **0 px on every pair, mean 0.0** |

Both re-base events the manager reports (`cam_y=52` for BG1, `cam_y=57` for
BG3) line up with the two discontinuities.

Whole-frame, 45 samples across the recording: 37 differ and **every difference
is a band at the bottom of the screen** (`y 213..240` and narrower) — precisely
the strip that was wrapping.

Gate at 240x160: 11/11 pixel-identical, `fetches=265497600 mismatched=0`; the
constant is 64 there and the code is byte-for-byte the engine's.

### Worth keeping

**A scrolling layer has a better oracle than "did the periphery change".**
Zero residual under a pure shift, on every consecutive frame pair, says the
layer is one coherent image being scrolled — which is the actual thing being
claimed. It is stronger than a before/after comparison and it needs no
reference build.

**Parallax defeats whole-frame comparison, and the fix is a switch not a
cleverer metric.** Two of the earlier bugs were measured by removing sprites
and the HUD; this one needed the same treatment applied to the backgrounds.

**The horizontal twin is untouched and cannot be fixed this way.**
`horizontalMinishPathBackgroundManager.c` has the same shape on x, where the
requirement is `xOffset + 320 <= 256` — impossible at any re-base step. If a
horizontal Minish path shows the same pop it needs a map source, not a smaller
step. Not reproduced, not attempted.

---

## B33 — Minish Village's blue house changes tileset at a camera threshold *(fixed)*

Reported 2026-08-20 with a recording: walking left and right across one spot
makes the blue house at the left edge flip appearance, and the maintainer
recognised the location as an older report with the same symptom.

`TMC_TILESET_TRACE=2` shows the engine swapping slot 0 between groups 1 and 2
every time the camera crosses x=88, which is the threshold; the picture changes
on exactly those frames and on no others.

### Root cause: the authored gap, seen from outside the GBA's screen

The per-tile classifier says the affected tiles are **class 3 — in no region at
all**. They sit at room y 560..599, immediately below region [1] (group 1,
y 464..560) and in the same columns; the only other region in those columns is
far above. They are in one of the authored gaps.

B27 decided what a gap tile gets, and the reasoning was sound: *"there is no
per-tile answer for it — the gap is exactly where the data declines to say.
Give it the group the engine itself loaded, so those tiles keep rendering the
way hardware renders them."* That is right for a gap tile the GBA can see,
because hardware shows it with the loaded group and the gaps are sized so that
works out on a 160-row screen.

It is not right in the **periphery**. Those tiles are ones hardware never shows,
so "the way hardware renders them" is undefined, and following the centred
screen's group means a tile 40 px outside the screen changes tileset whenever
the camera crosses a threshold that has nothing to do with it. The blue house
is that tile.

### The rule, and why it needed a guard

The obvious fix — grow each authored rectangle by the margin the wider viewport
adds, so a gap tile takes the group of the region it adjoins — is **wrong on
its own**, and simulating it first is what showed that. Over all 8,439 camera
positions in the room:

```
gap tiles inside the centred 240x160 : 3,176,480, grown rule disagrees with hardware on 162,922
gap tiles in the periphery           : 3,150,580, grown rule changes            367,460
```

162,922 tiles where it would have overruled hardware inside the screen. That is
B26's lesson again — a rule that fixes the report in front of you and breaks
what the authored data already had right.

So the published list gains **two** things rather than one, and first-match-wins
does the rest:

1. the authored rectangles, unchanged;
2. a **guard rect** covering the centred `DISPLAY_WIDTH x DISPLAY_HEIGHT`,
   carrying the old fallback — so a gap tile inside the GBA's screen still gets
   the engine's group, exactly as before;
3. the grown copies, which only a gap tile in the periphery can reach.

The guard makes the inside-the-screen disagreement zero *by construction*
rather than by argument, which is the property worth having: no simulation of
mine has to be right for hardware-visible output to be untouched.

### Evidence

- The discontinuity at the swap frame — 781 px — is gone; no frame pair in the
  window has one.
- 101 frames sampled across the recording: **0 changed pixels inside the centred
  240x160**, periphery changed on 54 frames.
- Both Hyrule Town recordings, 169 frames: **0 changed pixels anywhere.** Town's
  regions leave few peripheral gaps, so the guard and grown rects never fire
  there — the change is confined to where the defect was.
- Gate at 240x160: 11/11 pixel-identical, `fetches=265497600 mismatched=0`.

### Worth keeping

**"There is no answer for this tile" was true, and stopped being true when the
viewport grew.** B27's gap decision was correct for the screen it was written
against. What changed is that the periphery reaches into gaps the authored data
never expected to be visible — so the question is no longer "what does hardware
do here" but "what does this tile belong to", and the region it adjoins is the
only evidence there is.

**A guard rect is cheaper than being right.** The alternative was to prove the
grown rule never disagrees with hardware inside the screen, which the simulation
says is false. Publishing the old behaviour as a higher-priority rectangle makes
the proof unnecessary and costs one entry in a list that is already scanned.

---

## B34 — light shaft's lower rows show the top of its own block *(fixed)*

Found 2026-08-20 while measuring B21, in the same layer, with the instrument
built for B21. **Not reported by anyone** — which is worth noting, because it
had been live in both light-ray rooms since the height expansion.

**The vertical twin of B32, in a different manager.** `sub_08057450` scrolls
BG3 by keeping a fine offset in `yOffset` and re-pointing `subTileMap` a whole
64 px at a time. The block it points into is 32 tiles, so the window it must
cover is `yOffset + screen height`, and the fine offset may only range over
`256 - height` before the bottom of the screen wraps to the top of the block.

At 160 rows that is 96 and the engine's 64 fits with room to spare. At 240 it
is 16, and `yOffset` runs to 63: for **47 of every 64 camera positions** the
bottom of the screen showed the top of the block instead — up to 47 rows of it.
The symptom is the ray band breaking partway down with a disconnected fragment
beneath the break, and the fragment changing whenever the map pointer moves.

**Measured.** BG3 alone, masked, camera stepped 4 px at a time so the layer
scrolls exactly 1 px per step. A correct layer is a pure 1-row shift between
consecutive frames:

```
before  yOff 61->62: 0    62->63: 0    63->0: 47 rows (192..238)    0->1: 0
after   yOff 61->62: 0    62->63: 0    63->0:  0                    0->1: 0
```

The 47 is exactly the predicted `240 - (256 - 63)`, and it lands at the
re-point, which is where a wrap becomes *visible* rather than merely wrong.

**A pure-shift test cannot see the wrap itself, only the re-point.** Wrapping
is consistent under a shift — screen row `r` at `yOffset+1` and row `r+1` at
`yOffset` read the same block row either way — so eight consecutive pairs
inside one block scored 0 while the layer was wrong on all of them. What the
test catches is the moment the block's contents change under the wrapped strip.
Where a room's camera range never crosses a re-point the test is blind
altogether: the barrel minish house scored 0 before *and* after, and the defect
there had to be established from the offsets instead — `yOffset` reached 29–31,
needing 269–271 rows of a 256-row block. After: 0–15.

**Fix.** B32's, transplanted: re-base on a 16-px step. The scroll position is
unchanged either way — `64*(y/64) + (y&63)` and `16*(y/16) + (y&15)` are both
`y` — so only how often the map pointer moves differs, and a `static_assert`
now states the invariant the constant exists to hold. At 160 rows the macro
selects 64 and the generated code is byte-identical to the engine's.

Both rooms were affected: Minish Woods up to 47 rows, the barrel minish house
up to 15.

**Lesson (33).** *When a measurement technique is chosen for one bug, check it
can see the next one.* The consecutive-pair shift test settled B32 and was
reached for again here, where it returned 0 for a layer that was wrong in 47
of every 64 positions. It answers "is this layer scrolling cleanly", and a
uniformly wrapped layer scrolls perfectly cleanly. The question that separates
them is "can the block cover the screen", which is arithmetic on `yOffset`, not
a comparison of pictures — and it is the same arithmetic in both bugs, which is
the clue that the sweep should have been by mechanism rather than by report.

## B35 — light rays jump left as they fade, and when a text box opens *(fixed)*

Reported 2026-08-20 with two recordings, against the build carrying B21's fix,
by the maintainer: *"when Link walks sufficiently east into the Minish woods
and the light rays fade, upon the commencing of their fade out the light rays
jump to the left"*, and *"inside the Minish village barrel house, when Link
talks to the Picori NPC, when the textbox appears the light rays also jump
left"*. Both are the same defect in B21's fix, from two different triggers.

**B21's anchor was declared per frame and cleared per frame.** That was a
deliberate choice, made to avoid B30 and B31 — a declaration that outlives what
it describes — and it over-corrected into the opposite failure. The
declaration's lifetime became *the handler's tick* rather than *the overlay's
existence*, and those come apart in ordinary play:

- **Fade-out.** `LightRayManager_Action1` sets `unk_21` to the *trigger* type,
  not to the state being left, so entering a type-3 rect sets `unk_21 = 3` and
  `gUnk_08107C48[3]` is `nullsub_494`. The state-4 handler stops being
  dispatched on the **first** frame of a fade that runs eighty more. The band
  loses its clip and jumps 80 px left while still fully visible — which is
  exactly "upon the commencing of their fade out".
- **Text box.** The managers are suspended outright, for as long as the
  conversation lasts. In the barrel minish house that was 254 frames with the
  band 40 px out of place.

**Both reproduced from the recordings, by replay, against the binary the
maintainer actually played.** With `TMC_MASK_BG3=1`, sampling the band's
columns every 8 frames across the fade:

```
frame  played build   fixed
f08     195..319      195..319
f09     115..239      195..319     <- the jump, mid-fade
f13     115..239      195..319
```

and every 22 frames across the conversation in the barrel house:

```
g03     155..279      155..279
g04     115..239      155..279     <- the jump, on the text box
g14     115..239      155..279
```

`TMC_BG3_TRACE=2` names it without the pictures: 720 frames `clipped=1
anchor=1` followed by 44 frames `xOfs=16 clipped=0 anchor=0` — `bg3.xOffset`
still the state-4 constant, so it is still that overlay, with the anchor gone.

**Fix.** The anchor now expires on the two conditions that actually end the
overlay, both observable in the port: **BG3 goes off** (which
`LightRayManager_OnExitRoom` does) or **the room changes**. Nothing depends on
a handler remembering to tick. Because silence now means "unchanged" rather
than "off", the other light state — `sub_080573AC`, the world-locked parallax
rays, which want the unclipped rule — declares `PORT_BG3_ANCHOR_NONE`
explicitly instead of going quiet.

After: both recordings replay through the installed play binary with **zero**
frames where BG3 is on at `xOffset=16` without an anchor, and the single
expiring frame in recording 2 is the BG3-off transition itself, where nothing
is drawn.

**Lesson (34).** *A per-frame declaration and a latched one fail in opposite
directions, and "declare it every frame" is not automatically the safe one.*
B30 and B31 were both a declaration that outlived what it described, and the
lesson drawn from them — re-declare rather than latch — was applied here
without asking what "every frame" was actually keyed to. It was keyed to a
dispatch table entry that the engine changes one frame into an eighty-frame
fade. The right question is not "how often is this refreshed" but "what event
ends the thing being described", and then to watch for *that* — here, two
conditions the port can see for itself, neither of which is a function call
someone has to remember to make.

## B36 — Mt Crenel summit renders black apart from sprites *(fixed)*

Reported 2026-08-20 with a recording. **Not a viewport bug — it reproduces at
240x160**, which is the first thing that was established and the thing that
sets everything else about it apart from the rest of this tracker.

`Area_MtCrenel` room 0, `ROOM_MT_CRENEL_TOP`. The world layers render as pure
black; Link, the enemies and the HUD are fine.

**The two documented first moves were both wrong here, and cheaply.**
`TMC_REJECT_TRACE=1` reported `bottom=bound top=bound` — this is not the
B5/B15/B17 screenblock fallback that "sprites over black" usually means. And
replaying the recording at 240x160 does not answer the size question, because
frame-exact input diverges: Link takes different damage and dies, and the
comparison ends on a GAME OVER screen. A debug warp into the room at both
sizes answered it instead, with two known-good rooms warped to as a control
(Minish Woods 1.4% black, Hyrule Field 20.1%, Mt Crenel 97% and 95%).

**`TMC_MASK_BG2` separated "draws nothing" from "draws black" in one run.**
Masked, BG2 covers 63,698 pixels — the geometry, the map source and the
character data were all correct the whole time. Because the mask bypasses the
palette *and* the blend, that left two candidates, and a new
`TMC_BLEND_TRACE` settled them: `bldcnt=0x2F40` has `tgt1=0x00`, so no layer
is a first target and the blend does nothing, while `bgpltt_nonblack` collapses
from 255/255 to 44/255. The palette, not the blend.

Splitting the live palette from the engine's working copy showed the working
copy going black *first*, so it was not a fade either. Per-row: **rows 2..14
black, rows 0, 1 and 15 intact.** The palette group that owns exactly those
rows is 0x1E, and tracing the bytes the loader actually reads showed it
loading 15-16 non-black colours per row, correctly. So the rows were filled and
then emptied, by something that was not `LoadPalettes`.

**A watchpoint named it in one run.** `weatherChangeManager.c`:

```
MixPalettes(srcPalette1=<gPalette_549+4>, srcPalette2=<gPalette_549+420>,
            destPalette=<gPaletteBuffer+68>, factor=31)
sub_08059894(gPalette_549, gPalette_549 + 0xD0, ...)
```

Mt Crenel's summit cross-fades its terrain between a clear palette set and a
stormy one, and spells the second `gPalette_549 + 0xD0` — 13 palettes past the
first. **That is only an address because the GBA linker laid
gPalette_549..gPalette_574 out sequentially in the `gfxAndPalettes` blob**, and
it is B16's lesson in a new place: in decompiled code an out-of-range access is
defined on hardware and undefined here.

`port_linked_stubs.c` already knew this. It allocates `u16 gPalette_549[0x1A0]`
— the whole 26-palette block — with a comment explaining exactly this read, and
ending "port_rom.c populates it from gGlobalGfxAndPalettes after ROM load."
**It never did.** No code in the port ever wrote that symbol, so *both* sides of
the mix were zeros, and `MixColors` at factor 0 is 100% of the second one.

**Fix.** `port_rom.c` fills the block beside the other ROM-resolved symbols.
Palette N lives at `N*32` in that blob — the same arithmetic
`LoadPaletteGroup`'s hardware path uses — so it needs no new offset and is
right for both regions: `0x5A2E80 + 549*32` is `0x005A7320`, exactly the ROM
offset `port_asset_index.c` records for `gPalette_549.gbapal`.

Measured on the maintainer's recording: **97.3% black → 1.1%** at 320x240, and
**94.9% → 2.2%** at 240x160.

**Lesson (35).** *A comment that says another file does something is a claim,
not a fact.* The stub's comment described this defect, its cause, its symptom
and its fix, and had done since issue #34 — and the fix it described was never
written. The read walked off the end for as long as the comment claimed it did
not. When a note says "X populates this", grep for the write before believing
it; here `grep gPalette_549 port/` returns the allocation, the comment, an
asset-index row, and nothing that assigns it.

**Lesson (36).** *When two of the three explanations for a symptom are
indistinguishable in a frame, reach for the instrument that removes one of
them rather than for another picture.* "Black except sprites" is a layer
drawing nothing, a layer drawing black, or a layer darkened afterwards.
`TMC_MASK_BG<n>` kills the first in one run because it bypasses palette and
blend both; `TMC_BLEND_TRACE` kills the third by reading the registers instead
of inferring them. Neither needed a second build or a reference frame.

## B37 — Mt Crenel rain sheet fills only the centred 240 columns *(fixed)*

Reported 2026-08-20 with a recording, alongside B38, once B36 made the summit
visible at all.

**The inverse of B21, on a different layer.** `mapsource_bind_ui()` applies one
rule to a layer with no map source — it is reading a 32-tile screenblock, which
covers 256 px and wraps, so clip it to the authored width and centre it. That
is right for a room map caught mid-transition, where repeating the content
would be wrong, and it is exactly wrong for a repeating pattern, where the wrap
is what covers a wider viewport. BG3 is exempted wholesale for that reason.

Mt Crenel's weather manager takes BG1 away from the room's top map layer and
fills it with a rain sheet from gfx groups `0x2B`-`0x2E` (`unk_22` 0-3; 4
clears it, and only 5 hands the layer back). With the map layer off the
port refuses BG1 a map source — `TMC_REJECT_TRACE` names the class
`top=layer off`, and `TMC_LAYER_TRACE` shows `mapsrc_mask` dropping from `0x6`
to `0x4` while `clip_mask` rises from `0x1` to `0x3`. The clip then confined the
rain to `cols 40..279`, measured with `TMC_MASK_BG1`.

**Fix.** The layer says what it is. `Port_MapSource_DeclareTiledOverlay(1)`
from the weather manager while it owns BG1, and the clip skips a declared
layer so the screenblock wraps. `cols 0..319` after, with the density
unchanged (2656 px over 240 columns becomes 3547 over 320; 2656 x 320/240 is
3541).

Its lifetime is the room, not the declaring tick — B35's lesson, applied
without having to relearn it. Handing the layer back needs no undeclaration:
BG1 regains a map source then, and the clip only ever applies without one.

**Lesson (37).** *When a rule has one exemption, the exemption is the rule's
real shape and the layer index is an accident of where it was first needed.*
BG3 is exempted from the fallback clip because the overlays that live there are
tiled and world-locked. Nothing about that reasoning is about BG3 — it is about
being a tiled overlay — and the moment a different layer carried one, the rule
caught the wrong thing. B21 was the same sentence read the other way round: a
layer inside the exemption that did not belong there.

## B38 — vapour wisps and steam render opaque white *(fixed)*

Reported 2026-08-20 with the same recording. **Not a viewport bug** — the OBJ
mode was never implemented at any size.

The wisps hanging in the summit's gap are sprites (423 near-white pixels with
OBJ on, 0 with `TMC_DISABLE_OBJ=1`) and are meant to be translucent.

**VirtuaPPU never read OAM attr0 bits 10-11.** The only `>> 10` in the whole
renderer are a tilemap flip bit, attr2's priority field and the blue channel of
the colour conversion. OBJ mode 1 — semi-transparent — did not exist, so the
sprites composited opaque.

On hardware a mode-1 sprite is a blend **first target whether or not BLDCNT
says so**, and it forces alpha blending whichever effect BLDCNT selects. That
is not a refinement; it is the only way this scene blends at all, because
`steam.c` sets `spriteRendering.alphaBlend = 1` and leaves BLDCNT at
`0xbd << 6` = `0x2F40`, whose first-target field is **empty** — `tgt1=0x00`,
`effect=1`, `tgt2=0x2F`. Reading only BLDCNT, nothing was ever a first target
and nothing ever blended. `TMC_BLEND_TRACE` prints that pair, and an empty
`tgt1` beside a non-zero `semi_objs` is the signature.

**Fix.** `render_obj_line` records which pixels came from a mode-1 sprite,
and `composite_line` alpha-blends those against the layer below regardless of
BLDCNT's first-target bits and effect field. Where BLDCNT *did* already name
OBJ as an alpha first target the two paths compute the identical blend, so
that case cannot move.

**Coverage, because an 11-frame gate does not have any for a global renderer
change.** The canonical route does contain semi-transparent sprites — the
`semi_objs` census reports six of them in places — and a **177-frame** dense
diff of the whole route at 240x160, with and without the change, is identical
on every frame. That is the claim worth making; 11/11 on the waypoints alone
would not have been.

**Lesson (38).** *A register is not the only thing that selects a behaviour.*
Every blend in this renderer was decided by reading BLDCNT, which is where the
GBA documents blending — and one of the two ways to become a first target is a
bit in the sprite, not in the register. A scene that sets `tgt1` to zero and
still expects blending looks like a scene that has disabled blending, and the
port agreed with that reading for two milestones.

## B39 — rain layer is garbage after returning from the pause menu *(fixed)*

Reported 2026-08-20, immediately after B37 made the rain full-width. **B37 did
not cause it.** Measured on the build the maintainer had been playing before
that fix: 13,567 garbage pixels inside the centred 240 columns, against 13,540
after — identical defect, and B37 only extended the same wrap into the border
bands (0 px there before, 2667 + 1953 after). It had been there for as long as
the pause menu had.

**This is B25 a second time, in the same function.** `RestoreGameTask`'s
`#ifdef PC_PORT` tail pushes `gBGxBuffer` into VRAM after a menu, because the
GBA mechanism that does it does not fire in the port. B25 already carved out
one exclusion — a room in an affine display mode draws itself, so none of its
layers come from those buffers — and its comment states the general shape
outright: *"The room handler is re-run on the way out and reloads them
correctly — these copies then overwrite them."*

That is exactly what happens here, for a reason the affine test cannot see.
Mt Crenel's weather manager takes BG1 away from the room's top map layer
(`gMapTop.bgSettings = 0`) and fills it with a rain sheet loaded straight to
VRAM by gfx groups `0x2B`-`0x2E`. `gScreen.bg1.subTileMap` still points at the
room's top tilemap, so the copy wrote *that* into the rain's screenblock — over
the reload the re-run weather handler had already done correctly — and the rain
came back as a grid of wrong tiles drawn with the rain tileset.

**Fix.** Copy only into a BG some map layer is actually bound to. Which BG a
map layer displays through is per room, which `mapsource_bg_index` already
knows; the same question asked here is `gMapBottom.bgSettings == &gScreen.bg1
|| gMapTop.bgSettings == &gScreen.bg1`, and the same for BG2. 18,160 garbage
pixels to 0.

**The gate does not cover this path and the dense diff does.** The canonical
route opens the pause menu, dumps it, closes it and opens the figurine menu —
so its eleven waypoints contain two *menus* and no frame of the gameplay that
follows one. The 177-frame dense diff does: `d166` is route frame 12020,
Hyrule Town, 116 frames after the pause menu closed. Identical with and
without the change, on all 177.

**Lesson (39).** *When a fix carves out an exclusion, ask what the general
rule behind it is before writing the specific test.* B25 excluded affine rooms
because their layers do not come from `gBGxBuffer`, and wrote the test as "is
the display mode affine". The rule was always "is this buffer what belongs in
that screenblock", and a second way to fail it — a layer the engine has
temporarily handed to something else — was sitting one room away. The comment
B25 left described the general rule correctly; only the code was specific.

## B40 — Cave of Flames minecart never emerges, softlocking the ride *(fixed)*

Reported 2026-08-20 with a recording: *"Link and the minecart do not emerge
from the door the minecart is supposed to pass through. We briefly see Link and
the minecart bounce back after entering the doorway from the right, the camera
shifts left into the room Link and minecart are expected to enter, but they
never do."*

**This one was predicted.** `ScrollTransitionIsPending`'s own comment, written
for B24, ends: *"(LilypadLarge_Action3, and minecart.c has the same shape)"*,
and CLAUDE.md carried it forward as "`minecart.c` has the same shape and is
still unexercised". It stayed unexercised until someone rode the cart.

`Minecart_Action5` is the carry-across-the-scroll state, and it is
`LilypadLarge_Action3` line for line in the part that matters:

```c
if (gRoomControls.reload_flags == 0) {
    super->action = 3;
    super->speed = 0x700;
    gRoomControls.camera_target = &gPlayerEntity.base;
}
```

`reload_flags == 0` is the engine saying the scroll is over. Sliding, the
transition is applied on the spot and the flag is already set the first time
this runs. **Fading, the apply is deferred 32 frames and nothing marks a
transition as in progress**, so the test passes on the carry state's very first
frame: the cart drops out of its ride and hands the camera back to the player
before the room has changed. The camera pans on into the destination room while
the cart and the player stay behind in the old one — exactly what the report
describes, in that order.

**Fix.** B24's, verbatim: `&& !ScrollTransitionIsPending()`. The ride completes
and the scene continues into the NPC dialogue that follows it.

**It is an expansion bug by construction, and this is the rare case where that
needs no experiment.** The deferral only exists when `VIEWPORT_SCROLL_FADE` is
set, which is `VIEWPORT_WIDTH > DISPLAY_WIDTH || VIEWPORT_HEIGHT >
DISPLAY_HEIGHT` — so at 240x160 `ScrollTransitionIsPending()` returns FALSE and
the condition is the engine's own. Three instructions are added there (the call
and its test), the same as the lily pad's fix, which is why that shape was kept
rather than compiled out. Gate: 11/11, 0 of 265,497,600 fetches.

Replaying the recording at 240x160 does **not** answer the size question here —
it diverges into different rooms entirely (0x03/0x16/0x15 against 0x05/0x04/0x06),
which is the third report running where frame-exact input replay cannot be used
as an A/B across sizes. The code answers it instead.

### The camera-range assertions in that room are not a defect

Flagged while fixing B40 and chased afterwards, because `TMC_CAMTRACE` reports
`** X OUT OF RANGE **` on entry to several rooms in this dungeon — `camx=688`
against a `[0,368]` limit in room 0x06, `camx=232` against `[-24,-24]` in 0x05.

**They are the once-per-room trace catching a transient the engine has always
had.** `TMC_CAMTRACE` fires on the first frame the room number changes, which
is before the camera has eased into the new room; the README already points at
`--mapcheck`'s per-frame `spike10:` assertion for exactly this, and that is
what resolves it.

Per frame, over the whole recording: **32 x-out and 8 y-out frames of 6642**.
Listed individually they are *two* runs of 16 frames and *one* of 8, each
strictly converging — overshoot 64, 60, 56 ... 4 at exactly 4 px per frame,
which is `scrollSpeed`. `scroll.c`'s camera follow clamps only in the direction
of travel and never snaps a camera that starts outside the range, so a room
entry eases in and is legitimately out of range the whole way. The steady-state
invariant does not hold during that ease, and never did.

All three runs are entries into rooms **smaller than the viewport on the
flagged axis** (240x208 and 272x208 against 320x240), where min == max and the
camera is pinned. At 240x160 those same rooms are not smaller on that axis, so
the pinned value differs by `UI_CENTER_DX` and there is less distance to ease
away — the extra travel is a consequence of centring a narrow room, which is
the expansion's design, not a fault in it.

**And the worst of it is behind the fade.** The frames with the largest
overshoot are 0.0% non-black; by the time anything is visible the camera has
converged most of the way, and it settles symmetric.

`spike10:` now reports converging frames separately — `x-out=32 (30
converging)`, where the 2 and 1 that do not count are simply each run's first
frame, having no predecessor to improve on. A bare count could not tell an
ease-in from a camera parked outside its room, which is the thing actually
worth catching; and `[cam10x]` now lists occurrences the way the vertical case
already did, an asymmetry that had left `x-out=32` with no room, frame or
magnitude to chase.

**Lesson (41).** *An assertion that encodes a steady-state invariant will fire
on every legitimate transient, and a bare count cannot tell you which it was.*
This one was correct on every frame it flagged and still described no defect.
What made it answerable in minutes was listing the occurrences and asking
whether the error was shrinking — the same question that separates "converging"
from "stuck" for any settling quantity, and worth building into the instrument
rather than re-deriving by hand.

**Lesson (40).** *A defect predicted in a comment is still a defect, and the
comment does not fix it.* B24's fix identified the second instance, named the
file, and left it — reasonably, since it had no way to reach the scene. Three
weeks later it arrived as a softlock in a dungeon. When a fix's investigation
turns up a sibling, the cheap move is to apply the same guard immediately: the
reasoning is already loaded, and the alternative is waiting for someone to find
it the expensive way.

## B41 — white flash after a boss's element award covers only part of the screen *(open)*

Reported 2026-08-20, no recording. **Not yet confirmed against the scene** —
what follows is one ruled-out mechanism and one suspect.

**Ruled out: the script engine's flat fill.** `sub_0807FB28` wraps a boss's
element award in `SetFillColor(0x7fff, 1)` / `SetFillColor(0, 0)`, which sets
the backdrop white and masks every layer out of DISPCNT. That was the obvious
candidate and it is innocent: driven synthetically with `TMC_FILL_PROBE=1` it
fills **100.0%** of the 320x240 frame, on every frame it is active. The probe
exists because the scene needs story state the scripted tester cannot produce —
same rationale as `TMC_OAMY_PROBE`.

**Suspect: the white triangle effect.** `bossDoor.c:215` spawns
`WHITE_TRIANGLE_EFFECT` — the radial white flash — and it does not draw with a
layer at all. It rasterises a per-scanline *window* through `sub_0801E49C`,
and that rasteriser is written against the hardware screen in three separate
places:

```c
void sub_0801E64C(...) {
    // GBA Resolutions
    const s32 MAX_X_COORD = 240;
    const s32 MAX_Y_COORD = 160;
```

plus `sub_0801E49C`'s own `for (y1 = 0xa0; y1 > 0; y1--)` — 160 lines — and the
scratch buffer it fills, `MemFill16(0xffff, gUnk_02018EE0, 0x780)`, where
`0x780` is 160 lines x 3 x 4 bytes. Confining the effect to the top-left
240x160 of the viewport is exactly what those produce, and exactly what was
reported.

**Not a one-liner, which is why it is not fixed here.** The three sites are
coupled to a fixed-size EWRAM scratch buffer that would have to grow — the B29
hazard class, where an address that only exists as data has no grep to find it.
And the per-scanline window edges are written as **bytes**, so an x past 255
wraps; the >255 window path exists (B11's `set_window_h_bounds`) but this
writer does not use it. `include/screen.h` already warns that some sites rely
on that 8-bit wrap deliberately.

**What it needs:** a recording, to confirm the suspect is the mechanism and to
verify a fix. Attempting the spawn synthetically did not reach a visible
effect — it needs setup the real sequence provides.

## B42 — the table behind Vaati disappears when he warps out *(open)*

Reported 2026-08-20, no recording. The Elemental Sanctuary flashback, at the
end where Vaati warps away.

**No lead.** The one observation worth recording so it is not re-derived:
`vaatiAppearingManager.c` tears down with `sub_0801E104()` — the same window
teardown B41's suspect uses — and clears `DISPCNT_BG3_ON` in the same breath.
That the two reports touch the same machinery is suggestive and is not
evidence; a table is room furniture and would not normally be on BG3.

**What it needs:** a recording and the save, per the usual workflow.

## B43 — western-wood cutscene ends on a permanent black screen *(fixed)*

Reported 2026-08-20 with a recording. Walking south into the western wood
starts a Hyrule Castle cutscene; the screen goes black and stays black.

**Not a viewport bug, and not from the recent work.** The pre-session baseline
(`32c9562a`) replays identically, and the maintainer reproduced it on the
240x160 play build and sent a recording made there
(`vaati_takeover_240x160.script`), which is the repro this was fixed against.
It was live in the shipping build.

**Root cause: one port-only line unlinks a stale entity and writes the live
list head out from under the running cutscene.**

`CreateVaatiApparateManager` (`vaatiAppearingManager.c`) ends with a block the
decomp itself flags as a bug:

```c
if (gArea.onEnter != NULL) {
    gScreen.lcd.displayControl &= ~DISPCNT_BG3_ON;
    RoomExitCallback();
    //! @bug: this always variable points to ROM, not a Manager*
    DeleteManager((Manager*)gArea.onEnter);
}
```

`gArea.onEnter` is a ROM *function* pointer, so on hardware `DeleteManager`
reads two words of code as `prev`/`next` and writes back through them into
ROM, where the writes are discarded. The call is a no-op; the block's only
observable effects are the two lines above it.

A port commit (`fix(port): Vaati apparate SIGSEGV`) changed both the guard and
the argument to `gArea.transitionManager`, because dereferencing the raw
`onEnter` segfaults on x86-64. Right instinct, wrong substitute: that deletes a
*real* entity, and during this cutscene it is a stale one.

**Why stale matters.** `Subtask_FadeIn` brackets a subtask with
`sub_0805E958` / `sub_0805E974`, which swap the nine **list heads** into
`gEntityListsBackup` and back. Nothing else moves: every pre-subtask entity
keeps its own `prev`/`next`, and a list *sentinel* — `(Entity*)&gEntityLists[n]`
— is the same object on both sides of the swap. `gArea` is not part of the
bracket, so `gArea.transitionManager` still points at Hyrule Field's transition
handler, which was authored into list 6 and sits at its head. When Vaati
apparates:

```
UnlinkEntity (ent=gUnk_02033290)                    entity.c:642
DeleteManager (ent=gUnk_02033290)                   entity.c:539
CreateVaatiApparateManager                          vaatiAppearingManager.c:196
Vaati_Apparate                                      npc/vaati.c:137
ScriptCommand_Call                                  script_VaatiTakeover: Call Vaati_Apparate
```

`UnlinkEntity`'s `ent->prev->next = ent->next` resolves to
`gEntityLists[6].first`, because `Entity::next` and `LinkedList::first` are the
same offset and `ent->prev` *is* the sentinel. One store puts list 6's head
back on the overworld chain.

**What that produces.** The takeover orchestrator is neither deleted nor
unlinked — its own `prev`/`next` are untouched to the last frame of the run —
it is simply no longer reachable from the head, so `ram_UpdateEntities` never
visits it again. Measured on the 240x160 repro with `TMC_ENT_WATCH=1`:

```
f=3516  gEntityLists save+clear          (Subtask_FadeIn)
f=3518  A inList=-1  B inList=6 reached=1 (B created, running)
f=3549  gEntityLists[6].first := ...     (DeleteManager, above)
f=3550  A inList=6  reached=1            B inList=-1 reached=0, prev/next unchanged
f=3882  gEntityLists restore             (Subtask_FadeOut)
```

Everything downstream follows from that line:

- The orchestrator stops at `SetSyncFlag 0x10`, three helper NPCs spin forever
  on flags it never broadcasts, and the cutscene cannot end — **that is #93**.
- The overworld orchestrator, back in the list, runs the tail of
  `script_CutsceneOrchestratorTakeover` *inside the castle scene* and
  self-deletes there — including the `_0807E800` fade-in that is supposed to
  bring the picture back after the subtask.
- `Subtask_FadeOut`'s `SetFade(gUI.fadeType = 0x5, 0x100)` then hands back a
  black screen, exactly as the engine intends, and the fade-in that would have
  answered it has already been consumed 300 frames early — **that is B43**.
- The restore at f=3882 reinstates a head that now points at an entity deleted
  during the cutscene, so list 6's walk terminates on a zeroed slot from there
  to the end of the run.

**One root cause, four symptoms.** B43 and #93 were correctly suspected of
being one bug; they are, and the missing fade-in and the dead object list are
the same line as well.

**The fix** is to reproduce the hardware no-op: keep the guard on
`gArea.onEnter`, keep the two side effects, and drop the `DeleteManager` call
under `PC_PORT` while leaving the GBA text intact for matching. That removes
the segfault the substitution was made for as well.

**The #93 watchdog is removed with it.** `sub_08053BBC` carried a thirteen-step
replacement sequence that force-set the orchestrator's sync flags and then
forced `SetRoomFlag 0`; its own comment said it rushed the cutscene "by in a
few seconds rather than the GBA-native ~20s". With the orchestrator running,
that sequence is actively wrong — it consumes and sets flags out from under a
working handshake. `sub_08053BBC` is back to its four original lines, and
`TMC_NO_CUTSCENE_WATCHDOG` is gone. The `gActiveScriptInfo.syncFlags |= 0x400`
"belt-and-suspenders" went too: **nothing in the takeover ever sets 0x400** —
four helper scripts wait on it and no script signals it, so `WaitForSyncFlag
0x400` is a park-here-forever idiom and the `DoPostScriptAction 0x0006` after
it is dead code. The subtask's `DeleteAllEntities` is what removes those
helpers, on hardware too.

**Verified** on the maintainer's 240x160 recording. The recording's input stops
at frame 4033 because in the broken build the screen was already black; the
cutscene now reaches King Daltus's `MessageNoOverlap` + `WaitUntilTextboxCloses`
and correctly waits for a button. Replaying it with A pressed every 40 frames
past that point: 13 dumps from f4400 to f13900, **all distinct, none black**,
ending with the player back in the western wood with the HUD and control.
Removing the watchdog is byte-identical to running the old build with
`TMC_NO_CUTSCENE_WATCHDOG=1` (13/13 dumps).

At 320x240, `western_wood_softlock.script` gives the A/B directly, against this
cycle's previous binary rather than a rebuild: **0.00% non-black on all six
dumps before, 74–84% after**, and with the same A presses added it completes to
the same place. Gate green at 240x160 on the installed binary — 11/11 waypoints
and `fetches=265497600 mismatched=0` — and canonical-route frame time at 320x240
is unchanged (present mean 9.113 -> 8.839 ms, max 16.8 -> 13.6 ms), so the
watchdog's per-frame work coming out is worth marginally more than
`TMC_ENT_WATCH`'s disabled branch costs.

**How it was found, and what the first three passes got wrong.** The tracker's
own next step — "a watchpoint on that entity's `next` pointer" — would not have
found it: that pointer is never written. The instrument that did is
`TMC_ENT_WATCH`, which reports two things per frame per tracked entity, *which
list holds it* and *whether the iteration reached it*. `inList=-1` with the
entity's own links intact is a different signature from being unlinked, and it
points at the head rather than at the entity. Earlier passes reached, in order:
"the orchestrator self-deletes" (it was a different orchestrator — the trace
was not tagged by entity), "the restore drops it" (the restore is 332 frames
later; `sub_0805E958`/`sub_0805E974` now print their frame), and "something
unlinks it" (nothing does).

**Ruled out along the way**, each by measurement rather than argument, and all
still true:

- *Stale recycled context.* `DestroyScriptExecutionContext` and
  `InitScriptExecutionContext` both `MemClear` the whole struct.
- *Two entities sharing one context.* Scanning `gEntityScriptCtxTable` every
  frame for duplicate non-NULL entries reports zero over the whole run.
- *The entity-to-slot mapping.* 1 player + 7 aux + 72 entities = 80 =
  `PC_MAX_ENTITY_SLOTS`, both arrays `GenericEntity`.
- *Priority gating.* The port's `ram_UpdateEntities` does none, which looks
  wrong beside `updatePriority` but is not — `EntityDisabled` is consulted
  inside each kind's dispatcher (`object.c:206`, `npc.c:18`, `manager.c:67`).
- *The cutscene index.* `AuxCutscene_Init` sets `gMenu.field_0x3` from
  `sCutsceneData[gUI.field_0x3]._3`; entry 2 yields 1, the Vaati takeover, in
  Hyrule Castle room 2. Correct throughout.
- *A textbox waiting for a button.* In the broken build `MESSAGE_ACTIVE` was 0
  on every sampled frame and ~190 A presses changed nothing. (It is worth
  noting that this is exactly what the *fixed* build does — and there the flag
  reads 1 and the button works.)

**Lesson (43).** *An entity can stop being updated without anything touching
the entity.* Three mechanisms produce "it stopped being updated" and the entity
itself distinguishes only one of them: it was unlinked (its `prev`/`next`
change), the list's head or a predecessor's link was rewritten (nothing about
it changes), or its dispatcher declined to run it (nothing about the list
changes). Two of the three are invisible from the entity, which is why three
passes of watching the entity found nothing. Ask the *iteration* whether it
reached the entity, and ask the *list* who owns it, before watching the entity
at all.

**Lesson (44).** *A save/restore that swaps container heads leaves every
pointer into the old container live.* `sub_0805E958` moves nine head pairs and
nothing else; the entities, and crucially the sentinels, are shared. Any
pointer held across that bracket — `gArea.transitionManager` here — still
writes through to whatever the container now holds. When a port replaces a
pointer the original never dereferenced, check what the original *did with* it,
not just what it pointed at: this substitution was correct about the type and
wrong about the effect.

## B44 — closing the window skips every SDL teardown step *(fixed)*

Reported 2026-08-20: the maintainer's desktop session died when they closed the
game window. **Not a viewport bug and not reproducible here** — see the limits
below — but the exit path had a real defect on exactly the route they used.

`main()` ends with a full teardown:

```c
AgbMain();
Port_Audio_Shutdown();
Port_PPU_Shutdown();
Port_Config_CloseGamepads();
SDL_DestroyWindow(window);
SDL_Quit();
```

**`AgbMain()` never returns on that route.** Closing the window raises
`SDL_EVENT_QUIT`, `Port_PumpEvents` sets `gQuitRequested`, and the next
`VBlankIntrWait` calls `exit(0)` from inside the frame loop
(`port_bios.c:295`). All five calls are skipped. At process exit the window,
its renderer, its textures and the audio device stream are all still live, and
`exit()` then runs the capture atexit handlers and every static destructor
while **the SDL audio callback thread is still running**. `SoftReset` exits the
same way.

**Fix.** One idempotent `Port_Shutdown()` in `port_main.c`, called from
`main()` and from both `exit(0)` sites. Audio is torn down first on purpose:
stopping the callback thread before anything it might touch is destroyed is the
whole point of the ordering. Verified with a breakpoint that it now runs from
`VBlankIntrWait` — the window-close path — and the process exits normally.

**What this does *not* establish.** No crash was reproduced. Headless runs exit
0 with or without the fix, and they cannot show the problem: `SDL_VIDEODRIVER=dummy`
allocates no GPU resources, which is exactly the class being leaked. A
user-space segfault also would not normally take a desktop session with it —
that points at the compositor or driver — so this is a real defect on the
reported path and a plausible contributor, not a demonstrated cause.

**Lesson (42).** *A headless test suite cannot see a bug in the resources it
declines to allocate.* Every run in this project is `SDL_VIDEODRIVER=dummy`,
which is what makes the capture tooling deterministic and what made this
invisible: the dummy driver creates no window, no renderer and no textures, so
the leak has nothing to leak. The gate is green across this fix in both
directions and always would have been. Exit paths are worth reading rather than
testing here.

## B45 — Link vanishes entirely on entering Castor Wilds mud *(fixed)*

Reported 2026-08-22 with a recording (`mud_sink_visual_glitch.script`, 320x240).
Walking west into the swamp should clip the bottom of Link's sprite gradually
as he sinks, until he is warped back out. He disappears completely instead.

**Reproduced.** Link is whole at frame 914 and entirely gone by 916 — one
step, not a fade. The sink itself still works: `SurfaceAction_Swamp` keeps
running, `surfaceTimer` climbs to 0xF0 and `RespawnPlayer()` fires on schedule.
Only the picture is wrong.

**Not a viewport bug.** Nothing in the chain below reads `VIEWPORT_WIDTH` or
`VIEWPORT_HEIGHT`, and the decisive state is byte-identical at 240x160: the
same two gfx loads land on the same OBJ VRAM tile with the same contents
(`[sink] f=1807 gfx group 23 ... covers tile133 (src nonzero 0/32)`). It was
live in the shipping build.

**The chain, measured with `TMC_SINK_TRACE=1`:**

1. `SurfaceAction_Swamp` spawns `OBJECT_70` on the first swamp frame.
2. `Object70_Action1` sets `gPlayerEntity.base.spriteOrientation.flipY = 3`
   every frame it lives. That field is **OAM priority**, not a flip: the ARM
   builds attr2 bits 10-11 from `[entity + 0x1B] & 0xC0`
   (`asm/src/intr.s`, `sub_080B299C`), and the port's
   `ResolveEntitySpriteParams` matches it exactly. Confirmed in the trace as
   `orient=0x80 attr2prio=2` becoming `orient=0xC0 attr2prio=3` at frame 913.
3. Castor Wilds runs `bg2ctl=0x1C42`, i.e. **BG2 at priority 2**, and BG2 is
   the full-coverage ground layer — `TMC_MASK_BG2=1` paints the whole frame
   magenta. An OBJ at priority 3 is behind a BG at priority 2, so Link is
   hidden. `TMC_DISABLE_BG2=1` brings him back, standing in the mud.
   **This step is correct**: it is what the registers say and what hardware
   would do.
4. So what stays visible has to be `OBJECT_70`, and it is drawn in front —
   `attr2prio=2` ties with BG2 and OBJ wins ties. It emits **twelve** OAM
   entries every frame, a 4x3 block of 8x8 sprites covering Link.
5. **Every one of those twelve points at OBJ VRAM tile 133, and tile 133 is
   all zeros.** `OBJECT_70`'s definition is
   `{ { 1, 0, 0, 0, 133, 2, 0 }, { 0, 0, 0, 0, 167, 0, 0 } }` — `gfx_type` 2,
   which `LoadObjectSprite` reads as *load nothing, point `spriteVramOffset`
   at fixed tile 133*. Something else is supposed to have put a graphic
   there. Nothing has.

**Where tile 133's content goes.** Two loads cover it in the whole run, and
only two:

```
f=479  gfx group 15 -> gfx_350de0_112x104  dest=0x06010800  5632 B  (src nonzero 32/32)
f=614  gfx group 23 -> gfx_1d7e0_128x128   dest=0x06010000  8192 B  (src nonzero  0/32)
```

Group 15 fills it during the intro; group 23, loaded on entering the room,
covers tiles 0..255 and its own source is blank at tile 133, so it wipes it.
Groups 25/26/27 patch small ranges *after* group 23 (tiles 192, 213, 228) —
which is the shape a "restore the fixed tiles" load would have — but **no gfx
group in the extracted table targets tile 133**. Searched every group for an
entry whose `dest <= 0x060110A0 < dest + size`: exactly three, groups 15, 23
and 89, all of them large sheets landing on 0x06010000 or 0x06010800, and 89
is the same file as 23. Nothing small and nothing aimed at it. Link's own tiles
are elsewhere (`vramOff=352`), so this is not the player's animation streaming
either.

**Second pass, 2026-08-22: the "missing overlay load" theory is dead.** Both
the first write-up here and the sibling port's comment assumed a graphic was
supposed to be loaded at tile 133 and the port was failing to load it. It is
not:

- **The ROM has no gfx-group entry targeting that tile.** Searching
  `baserom.gba` for the little-endian dest word `0x060110A0` gives **zero**
  hits, and for tile 137 (`0x06011120`) likewise — while the neighbouring
  small patches the same table does carry are right there:
  `0x06011800` (tile 192) twice and `0x06011C80` (tile 228) once, at ROM
  0xFFD50–0xFFD68. So the extracted `gfx_groups.json` is faithful, and nothing
  fills tile 133 on hardware either.
- **The object definition is byte-identical to the ROM.** OBJECT_70's row
  packs to `01 00 85 08 00 00 A7 00`, which occurs exactly once, at ROM
  `0x126B18`, between the two `MULTI_FORM` pointers for 0x6F and 0x71. So
  `gfx = 133, gfx_type = 2, spriteIndex = 167` is what the game says.
- Watched every frame of the run: tile 133 holds data only between the
  intro's group-15 load (f479) and the room's group-23 load (f614). From
  entering Castor Wilds it is blank to the end.

**What OBJECT_70 actually draws is a mask.** Sprite 167 frame 11 —
`frameIndex = type + 0xb`, so type 0 — is **twelve 8x8 pieces in a 4x3 grid,
every one at tile offset 0**, spanning `x -16..+15, y -24..0`. One repeated
tile over a 32x24 rectangle. Frame 12, the type-1 (stairs) case, is the same
shape at 16x24. Painting tile 133 solid (`TMC_SINK_FILL133=1`, with
`TMC_DISABLE_BG2=1` so the player is visible) shows where it lands: **over
Link's upper body, from the top of his head to about his chest**, with his
legs and the green splash below it.

**And the sink is not a translation.** `gPlayerState.spriteOffsetY` is reset
every frame by `interrupts.c:379`, so `SurfaceAction_Swamp`'s
`+= 4 + (surfaceTimer >> 5)` never accumulates: traced across the whole sink it
runs **4 to 11 px** while `surfaceTimer` climbs 0 to 0xF0. Link is pushed a few
pixels down, not slid off the screen. Whatever clips him is the mask, not his
position.

**Third pass: the mask is a red herring, and filling tile 133 would not fix
anything.** Painting it solid (`TMC_SINK_FILL133=1`) and measuring what it
covers, frame by frame, kills the whole line of enquiry the first two passes
were on:

| frame | Link px in box | covered by mask | Link rows | mask rows |
|---|---|---|---|---|
| 912 (before) | 289 | 0 (0%) | 59..83 | — |
| 916 | 313 | 175 (56%) | 64..90 | 56..79 |
| 924 | 336 | 169 (50%) | 65..92 | 56..79 |
| 940 | 289 | 197 (68%) | 63..87 | 56..79 |
| 1000 | 289 | 172 (60%) | 65..89 | 56..79 |
| 1100 | 247 | 149 (60%) | 70..92 | 56..79 |

Three things follow:

- **The coverage does not grow.** It is flat at 50–68% for the whole sink, the
  spread being which walk frame he is on. Whatever produces "clipped gradually
  more and more", it is not this.
- **It covers the wrong end.** The mask holds rows 56..79 while Link occupies
  63..95, so it hides his *head and torso* and leaves his legs showing. For a
  sink you want the opposite. Note the stairs frame (12) is placed the other
  way — its pieces run y `-8, 0, +8`, i.e. mostly *below* the anchor — so the
  two placements are deliberate and different.
- **It is at OAM priority 2, the same as BG2.** Wherever BG2 is opaque the
  mask changes nothing at all. **A correctly filled tile 133 would not make
  Link visible by one pixel.**

So B46's neighbour — the blank tile — is at most a missing cosmetic mud patch.
**The disappearance is entirely `flipY = 3` against an opaque priority-2
ground**, and nothing else in the scene bears on it.

**The only progressive thing in the scene is `spriteOffsetY`**, which moves
Link's sprite down 0 → 11 px (top row 59 → 70, measured across the sink) while
the mask stays put. That is the sole candidate left for a growing clip, and by
itself it reveals *more* of him below the mask rather than less.

**The layer arrangement is ordinary**, so there is no binding mistake to find:

```
mapBottom->bg2  mapTop->bg1
bg0=1F0C(p0) bg1=1D45(p1) bg2=1C42(p2) bg3=1E03(p3) dispcnt=1740
```

BG3 is configured at priority 3 — the one priority that would *tie* with Link
and let the OBJ win — but `dispcnt` bit 11 is clear, so it is off and has no
map bound. Every enabled layer sits above him.

**Stairs is not the differential it looked like.** The type-1 path holds
`flipY = 3` for a single frame: `Object70_Init` sets it, and `Object70_Action1`
replaces it with 1 or 2 on the very next frame from the collision layer. So the
sibling port's "fully invisible on the stairs" is not what the code does, and
the swamp is the only place that *holds* priority 3.

**What this needs now is an oracle, and there isn't one in this repo.** Every
input has been checked against the ROM or the ARM and they all agree: Link is
behind the ground while sinking. Either hardware shows him hidden too — in
which case the report is about something else in the scene — or one of the two
remaining inputs differs there in a way the ROM data alone does not reveal. No
GBA emulator is installed in this environment, so the picture hardware draws
cannot be obtained here.

**The falsifiable prediction, for whoever gets a reference frame.** If hardware
shows Link sunk with only his head above the mud, then *both* of these must be
true and both are then testable in one comparison: the priority-2 layer has a
transparent pit at the mud, and the mask is drawn about 24 px lower than we
draw it — its y offsets would have to run `0, +8, +16` like the stairs frame's
rather than `-24, -16, -8`. If hardware shows him fully hidden, the port is
right about the sink and the report is about the *mud patch* that tile 133
never draws.

**So the question is sharper than it was.** The mask is at OAM priority 2 and
Link at 3, which is the only arrangement in which the mask can cover him — but
priority 2 is also BG2's, and BG2 is the opaque full-coverage ground, so it
covers *all* of him first. For any of this to be visible, **the priority-2
layer has to be transparent where he stands** and it is not: `TMC_MASK_BG2=1`
leaves 24 non-magenta pixels in a 64x64 box around him, and those are sprites
in front, not holes. Two candidates remain, both narrow:

1. the layer *content* is wrong — the bottom map or its tileset should have a
   transparent pit at the mud, which is authored data and cheap to read; or
2. the priority *assignment* is wrong — the room puts the ground at BG2
   priority 2 (`bg2ctl=0x1C42`) and the swamp needs it elsewhere.

Nothing else is left: the OAM priority derivation is verified against the ARM,
the compositing order matches the hardware rule (OBJ wins ties, checked in
`mode1_composite_line`), and the object and gfx data are verified against the
ROM.

**Ruled out this pass**, each by measurement:

- *A missing gfx load for tile 133* — the ROM has no such entry (above).
- *A corrupt extracted sheet* — group 23's file is blank at tile 133 and the
  ROM's group table agrees; 4143 of its 8192 bytes are non-zero, which is an
  ordinary sprite sheet with ordinary holes. The only blank tiles in OBJ VRAM
  0..255 during the sink are 133, 137, 251 and 252.
- *The wading overlay* — `ProcessEntityForDraw`'s `ram_0x80b2b58` overlay fires
  only for act tiles 0x0F and 0x2F, and the swamp's is **0x13**
  (`ACT_TILE_19` in `tiles.h`). It is not part of this. It is, however,
  broken on its own account — see B46.
- *A wrong frame index* — sprite 167's frames 0..15 all decode sanely
  (1, 1, 4, 1, 1, 2, 2, 1, 4, 1, 4, **12**, 6, 1, 1, 2 pieces), so frame 11 is
  not data read past the end of a table.

**Fourth pass: answered against real hardware.** mGBA 0.10.2 was installed on
request, and it turns out to run headless (`SDL_VIDEODRIVER=dummy`) with a
stdin-driven CLI debugger — so the reference implementation can be replayed and
interrogated by script. `tools/mgba/README.md` has the harness. The maintainer's
own recording was replayed into it and the two OAM dumps compared at a matched
frame.

**Aligning the two.** Frame numbers do not correspond (the port skips the BIOS),
so the alignment is on an event: the frame OBJECT_70's twelve mask entries first
appear. mGBA 1034, port 913 — a constant 121, which is the `--offset 120` the
sweep found independently. At that frame the HUD entries, the mask entries and
the mud-ripple entries are at **identical positions** in both, so the scene is
aligned, not merely the clock.

**Hardware does what the port does.** At every sampled frame of the sink:

| | hardware (mGBA) | port |
|---|---|---|
| `DISPCNT` | `1740` | `1740` |
| `BG2CNT` | `1C42` (priority 2) | `1C42` |
| player OAM priority | **3** | **3** |
| mask entries | 12 × tile 133, prio 2, at x 109/117/125/133, y 56/64/72 | identical |
| OBJ VRAM tile 133 | **blank, 0/32 bytes** | **blank** |

So the ROM really does put the player at OAM priority 3 behind an opaque
priority-2 ground, and the mask really does draw a blank tile. **The
disappearance is what the game does, not what the port does to it** — and the
first two passes' theory, that a graphic was missing from tile 133, is now dead
twice over: the ROM has no loader for that tile, and hardware leaves it blank.

**What the port gets wrong is the ripple, not the player.** At the same matched
frame, four sprites sit at identical positions with identical tiles in both:

```
(80,116) tile 78   (80,126) tile 78   (85,119) tile 80   (85,123) tile 80
hardware: palette 9        port: palette 12
```

Same tiles, same places, same frame, **different palette**. That is
phase-independent and it is a real defect: the mud disturbance that marks where
the player went under is drawn in the wrong colours. It is also the only thing
the player *should* still see during the sink, which is very likely what the
report is actually about — "he disappears and nothing marks the spot" rather
than "he should be clipped".

**One difference that is not a defect.** The player's sprite decomposes
differently in the two dumps — hardware 16x32 + 8x16 from tiles 352/360, the
port 16x16 + 8x16 + 16x8 + 8x8 from 352/356/358/360, offset 5 px in x. That is
animation *phase*: the mask follows the entity and the mask positions are
identical, so the entity is in the same place and only the walk frame differs.
Worth re-checking phase-matched before reading anything into it.

**Also observed, harmless:** the port applies `flipY = 3` one frame later than
hardware, so the player is visible for a single extra frame at the start of the
sink.

**Where this leaves B45.** The reported symptom — the player vanishing on
entering the mud — is faithful. What is missing is the ripple's colour, which
is a different and much smaller bug. Before closing, the maintainer's
expectation is worth one look on their own hardware or on mGBA: if the real
game shows him clipped rather than gone, then something outside OAM, BGCNT and
the mask differs and this comparison would have to be extended to the map data.
Everything reachable through those three now says it does not.

**Fifth pass — the fourth pass's conclusion was wrong, and the maintainer's
screenshots are what showed it.** Eight mGBA captures of the same walk show the
player clearly visible in the mud and clipped progressively from the bottom,
head last. "Faithful to hardware" was wrong; the port really is broken here.

**What the hardware state proves, now that the picture is known.** Across the
whole sink on mGBA, sampled every 15 frames from the first mask frame to the
respawn:

| sink frame | Link pieces | Link y-extent | height | mask y | tile 133 |
|---|---|---|---|---|---|
| 0 | 2 | 54..86 | 32 | 56..80 | blank |
| 60 | 2 | 55..87 | 32 | 56..80 | blank |
| 120 | 2 | 57..89 | 32 | 56..80 | blank |
| 180 | 2 | 59..91 | 32 | 56..80 | blank |
| 225 | 2 | 60..92 | 32 | 56..80 | blank |
| 240 | 0 | — | — | — | — |

Three things follow, and together they localise the bug:

- **The sprite is never clipped.** It stays 32 px tall and two pieces for the
  entire sink and simply moves down 6 px, which is `spriteOffsetY` growing
  4 -> 11 exactly as in the port. So the clip in the screenshots is not the
  sprite, and not the mask either — the mask is fixed at y 56..80 and its tile
  is blank on every sampled frame.
- **The player is at OAM priority 3 throughout, and he is visible.** The only
  other sprites on screen are the HUD (priority 0), the twelve blank mask
  entries and three 8x8 ripple dots. Nothing there can draw a head, a face and
  a cap. **So the priority-2 ground is transparent where he shows** — there is
  a pit in it, and he is seen through it.
- **In the port that pit is not there.** `TMC_MASK_BG2=1` paints BG2's
  non-transparent pixels flat magenta and leaves only 24 non-magenta pixels in
  a 64x64 box around him, and those are sprites in front, not holes. BG1 is
  sparse and covers nothing there.

**So the defect is in the port's BG2 content, not in its sprites or its
priorities.** Everything the earlier passes checked — the OAM priority
derivation, the compositing tie rule, the object definition, the gfx table, the
mask geometry, tile 133 — is confirmed identical to hardware and none of it was
ever the problem. What differs is which pixels the ground layer draws over the
mud, and the amount of him covered grows as he sinks the 6 px the offset moves
him.

**Corrections this pass makes to the entries above:**

- The fourth pass's table is right about every register and every OAM entry and
  wrong in its conclusion. Identical sprite state does not imply an identical
  picture when a *background* differs, and the one input never compared against
  hardware was the one that mattered. The mGBA-side check that would have caught
  it needs the BG2 scroll, and the scroll registers are write-only — reading
  `0x04000018` gives open bus — which is why it was skipped. A write watchpoint
  on `0x04000018` does capture it and is the way in.
- The third pass's "the mask covers the wrong end and does not grow" stands and
  is now explained: the mask is not the clip and never was.
- The ripple palette difference (hardware 9, port 12) still stands as a real
  and separate defect.

**Next, and it is now a narrow question:** which tiles the bottom map holds
under the player on hardware, and whether their pixels are transparent where
the port's are opaque. That is the B5/B15/B17 family — a world layer drawing
something other than what the map says — and it is answerable entirely from
mGBA: capture BG2HOFS/BG2VOFS with a write watchpoint, convert his screen
position to a map index, read the entry from the screenblock at `0x0600E000`,
and read that tile's pixels from the char base.

**Lesson.** *A state comparison is only as good as the inputs it covers, and
"every register matches" is not "the same picture".* Four passes compared
sprites, priorities and object data against the ROM and agreed with hardware
every time, because the layer that was actually wrong was never in the
comparison. One look at the real screen refuted the whole chain in a second.
Ask for the picture before concluding from the state.

**Sixth pass: the port put at 240x160, which makes the layers comparable — and
every layer matches.** The earlier passes compared a 320x240 port against
240x160 hardware, which is unsound for anything scroll-dependent. Rebuilt at
240x160 and replayed the same recording: it reaches the same sink, and the
alignment is exact — the twelve mask entries and the four ripple entries are at
**identical screen positions** in both (mask x 109/117/125/133, y 56/64/72;
ripple 80/116, 80/126, 85/119, 85/123). So the two runs are on the same frame
of the same scene and the backgrounds can be diffed directly.

They agree, everywhere it matters:

| compared | result |
|---|---|
| BG2 screenblock (`0x0600E000`, 1024 entries) | **0 differ** |
| BG1 screenblock (`0x0600E800`, 1024 entries) | **0 differ** |
| BG2 char base (`0x06000000`, 8192 halfwords) | 254 differ, in tiles 356–381 |
| …transparency of those tiles | **identical**: 21 tiles hold transparent pixels, 705 pixels, and **no tile differs** in its transparent-pixel count |
| every tile under the player (screen x 104–144, y 48–96) | **fully opaque, 0 transparent pixels, in both** |

The 26 differing tiles are all fully opaque on both sides — a background
tile-animation phase difference, not a pit.

**And the hardware OAM is unambiguous, read raw rather than decoded:**

```
 7  attr0=8036 attr1=8075 attr2=6D60   OBJ mode 0 (normal)  priority 3  tile 352  pal 6
 8  attr0=803E attr1=0085 attr2=6D68   OBJ mode 0 (normal)  priority 3  tile 360  pal 6
13  attr0=0038 attr1=006D attr2=0885   OBJ mode 0 (normal)  priority 2  tile 133  pal 0
```

`DISPCNT = 0x1740` — no windows, BG3 off. No OBJ is in window or
semi-transparent mode.

**So this is now a contradiction, stated plainly.** By the hardware rule an OBJ
at priority 3 is behind a BG at priority 2, and every BG2 tile over the player
is opaque in both emulators, from identical maps and identical transparency. He
therefore cannot be drawn — and mGBA draws him. The port renders him hidden at
240x160 through **both** paths, the map-source one and `--no-map-sampling`'s
screenblock one, so it is not the port's map sampling either.

One of the premises is false and the measurements above do not say which. The
port shares whatever the false premise is, which is why it produces the wrong
picture from state that matches.

**What resolves it in one step:** a savestate taken at the moment of one of
those screenshots, so the exact state behind a *known picture* can be read.
Every dump above is from a replay whose picture cannot be seen from here;
pairing one frame's state with one frame's image is the only way to find which
layer is actually producing the visible pixels. mGBA writes savestates with
Shift+F1 as `<rom>.ss1`.

**Still standing from earlier passes, and unaffected:** the ripple palette
defect (hardware 9, port 12, at identical positions with identical tiles), and
B47's save byte order.

**Seventh pass — root cause found, from a savestate.** The maintainer's mGBA
savestate is a PNG with the state in `gbAs`/`gbAx` chunks, so it carries the
frame's **picture and its state together** — which is exactly what every
previous pass lacked. Decompressed, the block layout is
`[header 0x800][PRAM 0x800][OAM 0xC00][VRAM 0x1000][IWRAM 0x19000][WRAM 0x21000]`.

Reading a column through the player at x=118 in that frame, against the
screenshot decoded from the same file:

| rows | BG0 | BG1 | BG2 | OBJs covering (entry, priority) | what the screen shows |
|---|---|---|---|---|---|
| 60–62 | 0 | 0 | **6** | — | mud |
| 63–78 | 0 | 0 | **6** | (7, prio 3) | **the player** |
| 79–82 | 0 | 0 | **6** | (9, prio 3) | mud |
| 83–89 | 0 | 0 | **6** | (23, prio 3) | mud |

BG2 is opaque at **every** row, and every one of the player's pieces is at
priority 3 — yet entry 7 is drawn over the ground and entries 9 and 23 are not.
The only thing that distinguishes them is the twelve blank priority-2 sprites,
which span y 56..79.

**So OBJECT_70 is not a mask. It is a priority window.** Its twelve pieces are
blank on hardware, and their whole function is that **a transparent OBJ pixel
still claims the pixel's OBJ priority**. Inside that rectangle the OBJ layer
composites at priority 2 rather than 3, ties with the priority-2 ground, and an
OBJ beats a BG on a tie — so the player is drawn over the mud. As he sinks he
slides out of the rectangle, loses the borrowed priority, and is clipped from
the bottom up. That is the entire sinking effect, and it is why every previous
pass found identical state and different pictures: the difference was never in
the state, it was in a compositing rule.

The port drops it in one line, in `virtuappu_mode1_render_obj_line`:

```c
if (color_index == 0u) {
    continue;              /* transparent OBJ pixel contributes nothing at all */
}
```

**The rule, pinned by two savestates.** The OBJ layer is composited against
the BGs at the priority of the **last sprite in OAM order that covers the
pixel**, opaque or not — not at the priority of the sprite that supplied the
colour. The two are different quantities and routinely come from different
sprites. Each savestate carries its own picture, so the state and the pixels it
produced are readable from one file:

| | transparent sprite | opaque sprite | last covering | hardware draws |
|---|---|---|---|---|
| swamp, (118,70) | OAM[14] prio 2 | OAM[7] prio 3 | **14 → prio 2** | ties the priority-2 ground, OBJ wins the tie → **the player** |
| name entry, (27,52) | OAM[27] prio 1 | OAM[33] prio 2 | **33 → prio 2** | loses to BG1 at priority 1 → **white**, the letter's apex |

Taking the *best* priority any covering sprite claims gets the swamp right and
eats two pixels of that apex — which is exactly the regression the first
attempt produced, and what identified the rule. "Last", not "best".

VirtuaPPU walks OAM backwards, so the last sprite in OAM order is the first one
reached: the claim is taken once and later, lower-index sprites leave it alone.
It is kept in its own buffer, separate from the one that resolves sprites
against each other — folding them together also loses colours, because that
buffer decides which sprite's colour survives.

**Verified.** Canonical route 11/11 pixel-identical; map-source audit 0
mismatched in 265,497,600 fetches; and the dense 173-frame diff of the whole
route at 240x160 is **identical on every frame** with and without the change,
which is the bar a global renderer change has to clear (B38). On the
maintainer's own 320x240 recording the frame before the mud is byte-identical
to the pre-fix build and every frame in the mud differs, showing the player
progressively clipped from the bottom exactly as mGBA draws him.

**Lesson (45).** *Two scenes with the same shape and opposite answers are worth
more than either alone.* The swamp said a transparent sprite lends its
priority; the name-entry glyph said it does not. Only holding both at once
gives the rule — "the last covering sprite", which neither scene implies by
itself. The regression that first looked like a setback was the second data
point, and the cheapest way to get it was to ship the wrong rule at a
173-frame diff and read what it broke.

**Where the change lives.** `libs/ViruaPPU`, the same submodule as B38's OBJ
fix; merged as its PR #7 and carried here by the pointer at `53c7cc4`.

**It took two goes to land, and the near-miss is the lesson.** PR #44 merged
the superproject with the pointer at `500b20f`, and the very next commit,
`6230be1a "bump submodule"`, moved it back to `d523d7f` — the pre-B45 commit —
while the submodule's own `main` had not yet been fast-forwarded onto the
branch. For a while the tracker said fixed and the shipping tree did not have
it, with nothing in this repo's diff to show the code that had left. **A
submodule pointer can move backwards in an ordinary-looking commit**: after any
merge that touches one, check `git ls-tree HEAD libs/ViruaPPU` against the
commit you expect, and grep the checkout for something the fix introduced.

**The sibling port reached the same wrong theory and shipped two workarounds
for it.** `999sian/tmc`'s `object70.c` forces `flipY = 2` under `PC_PORT`,
commented "Object70's head-overlay sprite isn't wired up on PC yet ... fully
invisible on the stairs / during the swamp sink", and their `port_draw.c` adds
a per-scanline OBJ clip to fake the waterline. There is no head overlay to wire
up — the ROM never loads that tile — so the first is a guess and the second
reproduces the symptom rather than the mechanism. What their comment *is* good
for is naming a **second scene with the same cause: stairs**, which is the
type-1 case (`frameIndex` 12, a 16x24 mask) and has not been checked here.

**Not fixed on purpose.** Both available fixes are guesses until the tile's
real source is known: forcing priority 2 makes Link visible but unclipped and
in front of scenery he should be behind, and inventing a scanline clip
reproduces the symptom rather than the mechanism.

## B46 — the wading overlay never draws in shallow water *(open, found by inspection)*

Found while ruling the overlay out of B45, not from a report. Recorded because
CLAUDE.md's rule is to guard the sibling while the reasoning is loaded.

`ProcessEntityForDraw` renders a "shoes" overlay over an entity's feet when it
stands on act tile **0x0F** (shallow water) or **0x2F** (tall grass), from the
pointer table at ROM `0xB2B58`. That is what makes Link look like he is wading
rather than standing on top of the water. **The port's index arithmetic can
never reach the water entries, so it never draws.**

The ARM (`asm/src/intr.s`) uses `(spriteSettings & 0x30)` as a **byte** offset
and adds `frame * 2`, then indexes the pointer table with that byte offset
divided by four:

```
ldrb r1, [r4, #0x18] ; and r1, r1, #0x30      @ byte offset, {0,16,32,48}
...
add  r2, r1, r2, lsl #1                        @ + frame*2
ldr  sl, [r3, r2]                              @ table[byteOffset], i.e. index byteOffset>>2
```

The port instead computes `row = (ss & 0x30) >> 2` — `{0,4,8,12}` — and
`idx = row + (frame << 1)`, which is `row + frame*2` where the ARM's is
`row + frame/2`. Two consequences:

- **Shallow water never draws at all.** Its frame is
  `((gOAMControls.field_0x1 & 0x18) + 0x80) >> 2` = **32..38**, so the port's
  `idx` is 64..76 and the guard is `idx < 16u`. It fails every time.
- **Tall grass draws the wrong entry.** Its frame is `(x ^ y) & 6` =
  `{0,2,4,6}`; the ARM wants table indices `row + {0,1,2,3}`, the port asks for
  `row + {0,4,8,12}`.

**And the table is bigger than the port's copy.** `sShoesOverlayPtrs` is
declared `[16]` and `LoadShoesOverlayTableFromRom` reads sixteen words. Reading
the ROM at `0xB2B58` shows **36** consecutive IWRAM pointers (0x03006848 …
0x030068C5) before the data stops being pointers — the water entries the ARM
indexes at 16..31 are inside that range and outside the port's copy. So even
with the arithmetic fixed, the table has to grow.

Not fixed here: it is a rendering change with no recording behind it yet, and
the scene it affects — Link standing in shallow water or tall grass — is worth
a before/after look by someone at the controls. The sibling port
(`999sian/tmc`) fixed both halves, including reading the *integer* position
bytes `x.HALF.HI ^ y.HALF.HI` for the grass frame where the port reads the
fractional ones, so the frame jitters with sub-pixel motion instead of being
stable per tile. That third point has not been verified here.

## B47 — the port's `tmc.sav` will not load in mGBA or on hardware *(open, diagnosed)*

Found 2026-08-22 when the maintainer tried to open one of their recording saves
in mGBA and got an empty file select. Not a viewport bug; it has been true of
every save this port has ever written.

**The port stores each 8-byte EEPROM block in the opposite byte order to real
hardware.** Side by side, the same save:

```
port :  41 47 42 5A 45 4C 44 41   "AGBZELDA"   ":THE MIN"   "ISH CAP:"
mGBA :  41 44 4C 45 5A 42 47 41   "ADLEZBGA"   "NIM EHT:"   ":PAC HSI"
```

Every 8-byte group is reversed, and it holds at every offset checked. The mGBA
side is not a guess: deleting the save and letting the real game initialise a
fresh one produces that layout, so it is what hardware and every emulator write.

**Why it happens.** EEPROM is a serial device — the game shifts 64 bits out
MSB-first, and what lands in a `.sav` is that wire order. `port_save.c` skips
the serial protocol entirely and implements the four BIOS entry points over a
flat `sEeprom[8192]`, copying the game's 8 bytes straight in. The game's
in-memory order is the reverse of the wire order, so the file comes out
mirrored per block.

**How it presents.** The game reads block 0, gets its signature back scrambled,
decides the cartridge holds no valid file, and offers a new one — exactly the
"booted to an empty save file" the maintainer saw. It is symmetric: a save from
mGBA or a real cartridge will not load in the port either.

**Confirmed by the fix working.** `tools/mgba/savconv.py` reverses each block;
with the converted save, mGBA's EEPROM reads return `AGBZELDA`, the file select
shows the file, and replaying the maintainer's recording reaches Castor Wilds
gameplay (`BG2CNT = 0x1C42`) — which is how B45's hardware comparison became
possible at all.

**Not fixed in the port yet, on purpose.** Changing `port_save.c` to store the
wire order makes every existing `tmc.sav` unreadable, including the maintainer's
own and the seven `.script.sav` files the recordings depend on. It needs a
migration: detect the old layout on load (block 0 reading `AGBZELDA` rather than
`ADLEZBGA` is an unambiguous tell), convert in place, and keep writing the new
one. Worth doing — save interchange with mGBA is what made this class of bug
answerable — but it is a separate change with its own risk, and the converter
covers the immediate need.

## B48 — climbing any beanstalk crashes the port *(fixed)*

Reported 2026-08-22 with a recording and a save, as a hang at the top of Mt
Crenel. It is not a hang: the process takes **SIGSEGV**, and it is not a
viewport bug — the crash is size-independent and the crashing data is the same
at 240x160.

**It is every beanstalk in the game, not this one.** All ten `Area_Beanstalks`
rooms carry the defect, plus Temple of Droplets' lantern/scissors room and Dark
Hyrule Castle's northwest outside.

**Symptom is a moving target, which is the first thing worth recording.** Three
replays of the same script crashed in three different places — twice in
`AppendEntityToList`, once in `UpdateEntities` a good while later. The fault
address moves with the heap, because the crash is downstream of a memory
corruption rather than being the defect itself. Breaking on the *first*
out-of-range list index instead of on the segfault is what made it a fixed
point:

```
#0  AppendEntityToList (entity=…, listIndex=11) at src/entity.c:616
#1  RegisterRoomEntity (ent=…, dat=…)           at src/room.c:124
#2  LoadRoomEntity (dat=…)                      at src/room.c:82
#3  LoadRoomEntityList (listPtr=…)              at src/room.c:60
#4  LoadRoom ()                                 at src/room.c:315
```

`gEntityLists` has nine elements. `listIndex` arrives as 11, 13 or 14 depending
on the run, out of `dat->flags & 0xF` — so the entity record being read is not
an entity record at all.

**Root cause.** `Entities_Beanstalks_MountCrenelClimb_1` is sixteen bytes long
and holds exactly one `object_raw`. It has **no `entity_list_end`**:

```
Entities_Beanstalks_MountCrenelClimb_1:: @ 080F6D4C
	object_raw subtype=0x2c, x=0x78, y=0xb8, paramA=0x7
                                          @ ← no terminator
Enemies_Beanstalks_MountCrenelClimb::   @ 080F6D5C
	entity_list_end
```

`LoadRoomEntityList` walks records until `kind == 0xFF`. On hardware it reads
the one real record, steps to `0x080F6D5C`, and stops on the `0xFF` that
belongs to **the next symbol**. ROM is contiguous, so that read is defined and
the list ends where the author meant it to — the terminator is simply
borrowed. Verified in the ROM: the sixteen bytes after each of the twelve
blobs are `ff 00 00 …`.

The port gives each symbol its own heap allocation, where there is no next
symbol. The walk runs into allocator slack — the buffer at the crash was a
16-byte chunk followed by glibc's chunk header, then a host pointer, then the
string `room_properties/offset_f6d4c.bin` — reads a pointer's low nibble as a
list index, and appends an entity to `gEntityLists[11]`. Everything after that
is corruption.

**This is B16's class, not B28's.** B28 was extraction *truncating* a blob
below its symbol; here the blob is exactly its symbol's size and the symbol is
genuinely unterminated. Both land on the same sentence: *an out-of-range read
is defined on hardware because ROM is contiguous, and undefined once every
symbol is its own allocation.*

**Fix.** `extend_room_property_to_terminator` in the extractor. An entity list
(properties 0-2, 16-byte records, `kind == 0xFF`) or a tile-entity list
(property 3, 8-byte records, `type == 0`) that contains no terminator of its
own is extended, from the ROM, to the terminator the hardware walk would find —
so the extracted bytes are the bytes hardware reads and no engine code needs a
port-only bound. `kExtractorFormatVersion` is bumped to **2**; without that the
fix reaches nobody past first run, which is B28's own lesson.

**Swept by mechanism, not by report.** Every room-property blob the port ships
was checked against its consumer's terminator convention rather than only the
reported room. Twelve unterminated blobs sit in real rooms; all twelve need
exactly one extra record, and after the fix **0** remain:

| Area | Rooms | Property |
|---|---|---|
| 13 `Beanstalks` | 0-4 (tops), 16-20 (climbs) | 1 |
| 96 `TempleOfDroplets` | 51 `LanternScissors` | 0 |
| 137 `DarkHyruleCastleOutside` | 2 `OutsideNorthwest` | 0 |

A first pass counted 127 rather than 12, because the extractor's
`scan_pointer_table_count(…, 64)` over-reads every area table into its
neighbour and invents rooms — `Area_GreatFairies` declares 8 entries and the
sweep reported defects in its "rooms" 38 to 63. Filtering against the decomp's
declared room lists is what reduced it to a population worth acting on. The
phantom blobs are extended too (harmlessly, they are never loaded); 24 remain
unterminated past the 64-record guard and every one is a phantom.

**Evidence.** Against the same area tables, unterminated list entries drop from
**696 to 169**, and none of the 169 is reachable. All 131 changed blobs were
checked to be strictly additive: prefix byte-identical, longer, and equal to
the ROM at their own offset — so nothing that already terminated could lose its
terminator, and none did. 2,045 real-room lists now terminate.

**Verification.** The maintainer's recording replays to completion through the
installed play build (exit 0 where it previously took SIGSEGV around frame
5600), and the climb finishes: `area=13 room=0`, Link on the cloud tops. Gate
11/11 and `fetches=265497600 mismatched=0`.

**Reproduced at 240x160, which is how "not a viewport bug" is known.** The
pre-fix 240x160 play binary, extracting with the pre-fix extractor (blob 16
bytes), warped into `Area_Beanstalks` room 16 and took SIGSEGV through the same
call chain — `AppendEntityToList(…, listIndex=10)` from `RegisterRoomEntity`.
The fixed 240x160 build walks the same script clean.

**The rooms are 240x160, so the climb wears a 40 px border at 320x240**, and
its single-colour column runs 0-39 and 280-319 are the room ending rather than
a clip. With OBJ and BG0 off, the climb room's layers are **0 differing pixels**
against the centred sub-rect of the 240x160 build. The *top* room is not — see
**B49**, which this fix made reachable and did not cause.

**A first attempt at that comparison scored 0.00% on both rooms and was
worthless**: the harness had no `tmc.sav`, so the warp — which retries until
`TASK_GAME` — never fired, and both builds were photographed sitting on the
name-entry screen. Two identical wrong pictures. The repo's own rule caught
it late: *when a measurement jumps to its theoretical best, confirm the code
you think produced it actually ran.* A `dump` of the frame before the warp is
the cheap guard and is now in the script.

**Lesson (40).** *When the crash site moves between identical runs, stop
reading it.* It is a report about the heap, not about the defect. Find the
first operation that is already wrong — here, the first out-of-range list
index — and break there instead; it sat four frames below the segfault and
never moved.

**Lesson (41).** *A decomp symbol's length is not its extent.* Any list the
engine walks to a sentinel may be relying on the next symbol to supply it,
which is free on contiguous ROM and unavailable to a per-symbol allocation.
The question to ask of extracted data is not "is this symbol complete" but
"does it contain everything the consumer will read".

## B49 — beanstalk-top rooms' sky renders differently at 320x240 *(open, measured, undiagnosed)*

Found 2026-08-23 by instrument, while checking B48's fix against the 240x160
build. The third bug in the tracker found that way, after B34 and B46. **B48
made these rooms reachable; it did not cause this** — the fix only extends an
entity list, and the difference is in a background layer.

`Area_Beanstalks` room 0, `Room_Beanstalks_MountCrenel`, the cloud tops Link
arrives at. Warped to from the maintainer's save at both sizes, with
`TMC_DISABLE_OBJ=1 TMC_DISABLE_BG0=1` so sprites and HUD cannot contribute:

| Room | BG-only difference vs centred 240x160 sub-rect |
|---|---|
| 16 `MountCrenelClimb` | **0 px** |
| 0 `MountCrenel` | **1352 px of 38400 (3.52%)** |

**It is not animation phase and not a one-frame offset.** The layer is static —
consecutive frames at 240x160 differ by 0 px — and all twelve candidate frames
of the 320x240 run score the *same* 1352, so no alignment exists that removes
it. The difference sits in row runs **0-46 and 120-159**, i.e. the sky above
and below the cloud mass, with the cloud bank itself matching; there is a
vertical band at the right edge too.

**Undiagnosed, and the shape is familiar rather than known.** Sky above and
below a matching middle, in a 240x160 room shown on a 240-row screen, is what
a tiled overlay does when its screenblock cannot cover the taller viewport —
the B32/B34 question, *is `yOffset + VIEWPORT_HEIGHT <= 256`*. It is equally
what a BG3 world-view overlay does when the centring clip is skipped (B21,
B37). Both are guesses; neither has been tested. `TMC_BG3_TRACE=2` and
`TMC_MASK_BG<n>` are the two runs that would separate them, and neither was
made.

**Cosmetic, and the room is now reachable, so it is playtestable.** Nothing
about it blocks the climb.

## B50 — every conditional whirlwind is invisible and inert *(fixed)*

Reported 2026-08-23 with a recording and a save: the ledge near Lon Lon Ranch
that should loft Link has no tornado, and no effect either — fully absent. The
report came with its own control, which is what made this quick: **Mt Crenel's
tornados are visible in the same recording.** Not a viewport bug; it reproduces
at 240x160.

**Two spellings of one address, given two allocations.** On GBA the delayed
entity bitfield lives at `0x020342F8`. That is `gArea.filler6`: `gArea` is at
`0x02033A90` and `filler6` sits at offset `0x868` (`0x894` struct, less twelve
bytes of trailing pointers, less `0x20`). The decomp spells the same bytes both
ways, by file:

| File | Spelling | Role |
|---|---|---|
| `manager/delayedEntityLoadManager.c` | `gUnk_020342F8` | **writes** the bits |
| `npc.c`, `object/pinwheel.c` | `gUnk_020342F8` | read |
| `object/whirlwind.c`, `object/cutsceneMiscObject.c` | `gArea.filler6` | read |
| `physics.c` | `gArea.filler6` | clears on the room-change toggle |

Free on hardware, where both resolve to the same EWRAM. In the port
`port_linked_stubs.c` gave `gUnk_020342F8` its own `u8[0x100]`, so the manager
set bits in one object and the whirlwind read another that nothing ever wrote.
`ReadBit` therefore always returned 0, and

```c
if (((tmp & 0x7f) != 0) && (ReadBit(gArea.filler6, tmp - 1) == 0))
    DeleteThisEntity();
```

deleted the entity on its first frame — before `Whirlwind_Init`, which is why
nothing drew and nothing collided.

**Confirmed rather than argued, and no emulator was needed.** The ROM's own
literal pools carry the two addresses: `0x02033A90` appears 196 times (`gArea`,
heavily referenced) and `0x020342F8` five times, and
`0x02033A90 + 0x868 == 0x020342F8`. The bit arithmetic then coincides exactly —
the manager's `WriteBit(base + 16, 10)` is byte 17 bit 2, and the whirlwind's
`ReadBit(base, 138)` is byte 17 bit 2. **The same bit, reached from both
spellings.** Two objects cannot produce that agreement by accident.

**Why it looked like a tornado bug specifically.** Of the five readers, the two
that used the *written* spelling — delayed NPCs and Cloud Tops' pinwheels —
worked perfectly, so the failure was invisible outside the two objects that
used the other one. And Mt Crenel's tornados are plain `object_raw` in property
0, so their `health` is 0, `(health & 0x7f)` is 0 and the gate never runs. Same
object, different spawn path, opposite outcome: the maintainer's control was
the thing that pointed at the *conditional* path rather than at whirlwinds.

**A second effect, quieter.** `physics.c`'s `sub_0806F364` was clearing the
dead buffer, so the live bitfield was never cleared when the room-change
toggle flipped halves. That is now restored too.

**Population — 54 entities, and Cloud Tops is the serious one:**

| List | Entity | Count |
|---|---|---|
| `gUnk_additional_8_CloudTops_Bottom` | whirlwind | 26 |
| `gUnk_additional_8_CloudTops_Middle` | whirlwind | 14 |
| `gUnk_additional_e_HyruleField_LonLonRanch` | whirlwind | 4 |
| `gUnk_additional_8_CloudTops_Bottom` | `CutsceneMiscObject` type 18 (`MysteriousCloud`) | 10 |

Cloud Tops is the area built around riding tornados between floating islands.
All 40 of its whirlwinds and all 10 clouds were absent.

**Fix.** Alias the symbol instead of allocating it, exactly as `common.c`
already does for `gUnk_02035542` → `gzHeap + 2`: a `#ifdef PC_PORT` macro in
each of the three users mapping `gUnk_020342F8` onto `gArea.filler6`, and the
storage deleted from `port_linked_stubs.c`. The GBA path is untouched. Note
`filler6` is `0x20` and the old stub was `0x100`; `0x20` is exactly right —
the manager's highest reachable byte is 31 for both halves of the toggle
(`unk_20 = 0` → `index2 <= 255`; `unk_20 = 0x80` → `index2 <= 127`).

**Verification.** Same script, same 240x160 build, whirlwind entities counted
by breakpoint:

| Scene | Before | After |
|---|---|---|
| Hyrule Field / Lon Lon Ranch | 0 inits, 1 self-delete | **1 init, 0 self-deletes** |
| Cloud Tops Bottom | 0 inits, 1 self-delete | **2 inits, 0 self-deletes** |

The maintainer's recording shows the tornado on the ledge at frame 12200 where
it was missing before, and Mt Crenel's frames are byte-identical across the
change (0 px at 3300 and 3450) — the control stayed put. Gate 11/11 and
`fetches=265497600 mismatched=0`.

**The gate covers this one, and that was checked rather than assumed.** Both
halves of the change run on the canonical route: `DelayedEntityLoadManager_Main`
executes in area 21 and `sub_0806F364` fires, so 11/11 pixel-identical is
evidence about this mechanism and not merely silence about it.

**Lesson (42).** *A decompiled symbol named for an address and a struct field
can be the same bytes, and the port gives each its own storage without a
word.* Nothing warns: both compile, both link, each half of the code works on
its own object. The tell is a bitfield with writers in one file and readers in
another that never agree. B36 was the same family — a symbol that was a window
onto a larger block — and B29 the same again with `gBG0Buffer`; the general
question is *does this name alias something else on hardware*, and the ROM's
literal pool answers it in seconds.

**Lesson (43).** *A report that comes with its own working control is worth
answering in the order it hands you.* "Mt Crenel's tornados are visible, Lon
Lon Ranch's are not" excluded the object, its sprite, its palette and its
animation before any code was read, and left only what differs between the two
spawn paths.

## B51 — every port save is one byte out of layout for `flags` onward *(tool fixed; port struct open)*

Found 2026-08-23 while trying to get an mGBA savestate for B52. The maintainer
converted a play save with `savconv.py`, and it loaded in mGBA at the right
place with the right hearts and elements — but Link had no Ezlo and the
beanstalk was gone, "like before the kinstone event".

**Not the conversion.** Two independent checks say the bytes arrive intact:
the save mGBA wrote back on exit, reversed, is **byte-identical** to the
port's original (0 of 8192 differ), and the 1204-byte `SaveFile` block in the
file is **byte-identical** to what the real game had in memory at
`0x02002A40` in the savestate. Hardware got exactly the right bytes.

**The port's `SaveFile` disagrees with the GBA layout `include/save.h`
documents in its own comments**, from `flags` onward:

| Field | Header comment (GBA) | Port actual |
|---|---|---|
| `inventory` | `0x0F2` = 242 | 242 |
| `kinstones` | `0x114` = 276 | 276 |
| **`flags[0x200]`** | **`0x25C` = 604** | **603** |
| `dungeonKeys` | `0x45C` = 1116 | 1115 |
| `dungeonItems` | `0x46C` = 1132 | 1131 |
| `dungeonWarps` | `0x47C` = 1148 | 1147 |
| `darknut_timer` | `0x48C` = 1164 | 1164 |

`KinstoneSave`'s members sum to **327** bytes; the documented gap is **328**.
It is all `u8` arrays, so no padding makes it up. Everything before `flags` is
at the correct offset — which is why name, stats, inventory and the kinstone
bag all read perfectly — and everything from `flags` on is a byte early, so
every story flag lands on the wrong bit.

**Two things hid this for the life of the port.** `darknut_timer` realigns
because the compiler inserts a 1-byte hole before it for `u32` alignment, so
`sizeof(SaveFile)` is *coincidentally* 1204 either way and no size assertion
would have caught it — and there is no size assertion on `SaveFile` anyway.
And the port is self-consistent: it writes and reads at 603, so its own saves
work, and nothing is visibly wrong until the file meets the real game.

**The symptom is the diagnosis.** "Hearts and elements right, hat wrong" puts
the boundary exactly at `flags`, which is the one field whose offset the two
layouts disagree about. The maintainer's own framing — *the file select is the
earliest evidence* — is what localised it to a single byte.

**Fix (tool).** `savconv.py` now does more than reverse blocks: it takes its
direction from the signature in block 0, shifts `flags..dungeonWarps` between
the two layouts, and recomputes each affected slot's checksum with the game's
own `CalculateChecksum` (verified by reproducing both stored checksums of an
existing save exactly, `0x3DA7` and `0xE591`). Only slots whose stored
checksum already verified are touched, so an empty or deleted slot is not
blessed into looking real. Confirmed by the maintainer: hat back, beanstalk
back, all flags correct.

**Not fixed: the struct itself.** `KinstoneSave` is still 327 bytes, so the
port still writes the wrong layout and the tool is compensating for it. Fixing
it needs a migration for every existing port save, which is a decision that
has not been taken. **`LAYOUT_FIXED_IN_PORT` in `savconv.py` is the switch**;
leaving the compensation in after the struct is fixed would corrupt every save
the tool touched.

**One byte is unrecoverable in the mGBA→port direction** — the GBA's
`SaveFile+603` (`0x0C` in a real save) is `KinstoneSave` data the port's short
struct cannot hold. The tool reports it on stderr rather than dropping it
silently. Going port→mGBA is lossless and round-trips exactly.

**Lesson (44).** *A struct whose field offsets are documented in comments is
asserting something testable, and nothing was testing it.* Every offset in
`SaveFile` is written down and one of them is wrong; a single
`PORT_STATIC_ASSERT_OFFSET` per documented field would have failed at compile
time on the day the struct was written. The size assert that would normally
catch this could not: the alignment hole made the total right.

**Lesson (45).** *"Most of it is correct" localises a layout bug better than
"none of it is".* A wholly wrong byte order fails at the signature and tells
you nothing about where; a one-byte shift lets every field before it read
perfectly and names the boundary precisely.

## B52 — beanstalk base draws solid magenta *(closed as an intentional divergence)*

Reported 2026-08-23 with a recording: the base of the Mt Crenel beanstalk is a
solid magenta blob, wanted as the dirt/rock of the room below. **Settled
against hardware, and the port is faithful** — this is the periphery again.

**Measured in the port.** The pixels are exactly `#F800F8` = GBA `0x7C1F`, real
palette data and *not* the `0xFF00FF` the `TMC_MASK_BG` diagnostic paints. It
is an OBJ: `TMC_DISABLE_OBJ` removes all 859 px. The base sprites are OAM
26-30 on **OBJ palette slot 5**, which holds Mt Crenel's rock colours until the
beanstalk room loads and then goes 12/16 magenta. A watchpoint named the writer
in one run: `LoadRoomTileSet` (`playerUtils.c:4199`),
`MemCopy(&pal[0x30], &pal[0x150], 0x20)` — BG palette 3 → OBJ palette 5, with
`gUsedPalettes |= 0x200000` confirming slot 21. Faithful to the decomp; the
byte offsets are identical on GBA. The room's BG palette 3 is `gPalette_667`,
an all-`7C1F` placeholder, and the palettes are contiguous in ROM so hardware
reads the same bytes. `LoadObjPaletteAtIndex` deliberately loads nothing for
palette ids ≤ 5 — those slots are meant to already hold what the room put
there — so a sprite with id 5 wearing the room's terrain palette is the
intended mechanism.

**Hardware agrees exactly.** An mGBA savestate of the same room:
`OBJ palette 5 = 12/16 magenta`, `BG palette 3 = 12/16 magenta` — identical to
the port. The real game has the same placeholder in that slot.

**So why is it invisible on hardware?** Because the base is not on screen. The
magenta occupies **y = 208..237**, and the GBA's screen is the centred
`y = 40..199`. The whole blob sits in the 80 rows the expansion added. The
authors left OBJ palette 5 as a placeholder in this room because nothing
visible uses it — the same shape as B26, B27, B30, B31 and B33: *the periphery
shows world the authored data never expected anyone to see.*

**What the fix would be, and why it is not committed.** Suppressing that one
copy so the slot keeps the previous room's terrain palette renders the base as
dirt and rock — measured, 0 magenta px — which is exactly what was asked for.
But the copy is how every other room colours these sprites, and "skip it when
the source looks like a placeholder" is precisely the invented selection rule
B26 was burned by twice. This is therefore a **deliberate divergence from
hardware for the periphery's sake**, in the same class as B22's rim sprites,
and wants to be taken as a costed decision rather than slipped in as a bug fix.

**Taken 2026-08-23, as a divergence rather than a fix** — and deliberately
*not* by suppressing the mirror. The port now loads, into OBJ palette 5, the BG
palette 3 of the overworld room each beanstalk grows out of, mapped through
`gUnk_080B4410`'s five source rooms — the same table the beanstalk subtask
reads, so the mapping is the game's rather than invented. `gPalette_550` /
`381` / `537` / `446` / `446`, applied to all ten area-13 rooms, gated to the
expanded viewport, in `port/port_divergences.c`. 859 magenta px → 0, and the
base draws the ground it is planted in.

**Setting the palette rather than inheriting one is the whole point.**
Suppressing the mirror gives an identical picture when you climb up from the
source area and was how this was first prototyped, but it is not
deterministic: measured, the slot holds `0000 0000 0000 0000` on a first room
load after boot, so the base would render **black**, and descending from a
beanstalk top would re-inherit the placeholder. Verified deterministic — a cold
warp straight into the room now yields `7E16 290D 3593 3E5A`, Mt Crenel's
ground palette.

**The 240x160 build contains none of it**: the function compiles to an empty
stub there, checked in the disassembly. Gate 11/11 and
`fetches=265497600 mismatched=0`.

**This is the first entry in `docs/hardware-divergences.md`**, which exists
because the project settles arguments by asking the hardware and that method
assumes every difference is a bug. It is not one here, and a future session
diffing this room against mGBA needs to be able to find that out.

## B53 — Syrup never reacts to the mushroom; it snaps back and she offers potion *(fixed)*

Reported 2026-08-24 with a recording. Carrying the mushroom to the witch and
pressing A put it straight back on its stand and produced her generic greeting
(`TEXT_SYRUP/message_01`) instead of the quest line. **Not a viewport bug** —
measured identical at 240x160.

**The mushroom is a shop item.** Syrup's mushroom is an `ItemForSale` object of
type `0x38` = `ITEM_QST_MUSHROOM` (56), carried through the shop mechanic:
`ItemForSale_Action1` sets `gPlayerState.heldObject = 4` and
`gPlayerEntity.carriedEntity = super`, and `ItemForSale_Action2` then runs
every frame deciding, on each A press, between *cancel and put it back* and
*let the shopkeeper have it*. That decision reads the player's current
interaction target:

```c
ptr = sub_080784E4();
if (((*(int*)(ptr + 8) == 0) || ((*(u8*)(ptr + 1) != 1 || ...
```

**Raw GBA offsets into a struct the port lays out differently.** `ptr` is an
`InteractableObject*`; `+1` is `type` and `+8` is `entity`. On x86-64 the
64-bit pointers move things:

| Field | GBA | Port |
|---|---|---|
| `type` | 0x01 | 1 |
| `customHitbox` | 0x04 | 8 |
| `entity` | **0x08** | **16** |

So `type` still read correctly — it sits before the first pointer — and
`*(int*)(ptr + 8)` read the low half of `customHitbox`, which is the *optional*
custom rectangle and NULL for nearly every interactable. The port therefore
concluded "there is nothing to interact with" whenever there was, and every A
press while carrying a shop item took the cancel branch.

**"Most of it is correct" localised it again**, exactly as in B51: the field
before the first pointer was fine and the field at it was not, which points at
a layout difference rather than at logic. The two symptoms are one cause — the
item returning to its stand and the generic line are both `sub_080819B4`.

**Fix.** Typed field access under `#ifdef PC_PORT`, keeping the original
expression for the GBA build — the pattern `rupeeLike.c` and `talon.c` already
use for this class.

**Swept: the population was exactly one.** `sub_080784E4`'s other two callers
(`playerUtils.c:1162` and `:1264`) already go through the named fields, so no
other consumer of `InteractableObject` was reading it by offset. The wider
class — raw byte offsets into structs containing pointers — has two prior
fixes (`rupeeLike.c`, `talon.c`, both already guarded and commented) and a few
remaining candidates that are *not* this bug but are worth a look someday:
`staffroll.c:231`, `beanstalkSubtask.c:1289`, `titleScreenObject.c:47`.

**Verification.** Same recording, same 240x160 build, before and after:
`TEXT_SYRUP/message_01` → `TEXT_SYRUP/message_07`. The conversation now runs
the quest dialogue — *"Ah, yes! A ..."*, then *"One whiff, and you're
wide-awake! That is why it's called a wake..."* — and the mushroom stays in
Link's hands. Gate 11/11 and `fetches=265497600 mismatched=0`.

**A false crash cost a round, and it was the harness.** The first replay
SIGSEGV'd and a second run of the same input did not. The runs were sharing a
directory, and **the game rewrites `tmc.sav` as it plays** — so the second run
started from the first's mutated save and went somewhere else entirely. Neither
run meant anything. A fresh copy of the save per run is not optional, and two
clean runs then agreed and neither crashed.

## B54 — the darknut fight crashes *(fixed)*

Reported 2026-08-26 with a recording: SIGSEGV in `Area_CastorDarknut`.
**Not a viewport bug** — the pre-fix 240x160 play build segfaults on the same
recording and the fixed one does not.

**Reproduced twice and the crash site did not move**, which is what says it is
the defect rather than fallout from an earlier corruption (B48 was the
opposite):

```
#0  DarkNutSwordSlash_Init (this=…) at src/projectile/darkNutSwordSlash.c:46
#1  DarkNutSwordSlash (this=…)      at src/projectile/darkNutSwordSlash.c:25
#2  ProjectileUpdate …
```

with `this->parent = (nil)` and `this->type = 0`.

**The array was innocent, which was worth checking first.**
`DarkNutSwordSlash_hitTypes` is an `extern const u8[]` — exactly the shape of
the missing-data stubs behind B36 and B50 — but it holds real ROM data
(`4c 4c 4e 4d 53 …`). The NULL is the parent.

**Root cause: a NULL dereference the GBA tolerates.** The darknut sets
`slash->parent = super` at all five creation sites, and clears it again when it
dies: `EnemyDetachFX` does `this->child->parent = NULL`. So a slash that
outlives its owner by a frame reaches its *first* update with a NULL parent —
a state the code anticipates:

```c
if (this->action == 0) {
    this->action = 1;
    DarkNutSwordSlash_Init(this);                 /* reads parent->type      */
    if (this->type == 3) {
        InitAnimationForceUpdate(this, this->parent->animationState + 0x18);
    }
}
if ((this->parent == NULL) || (this->parent->health == 0)) {
    DeleteThisEntity();                           /* …the check is here      */
}
```

The check is two statements *after* the dereferences. On GBA address 0 is BIOS:
the reads return open bus rather than faulting, the garbage `hitType` is
written to an entity that the very next statement deletes, and nothing ever
observes it. On x86-64 address 0 is unmapped, so the fight crashes.

**Fix.** Under `#ifdef PC_PORT`, delete the entity before the init block —
which is where hardware ends up one line later anyway. Same class and same
remedy as `rupeeLike.c`'s "lick lick" fountain crash.

**Swept: the population is exactly one.** Every function in `src/projectile/`,
`src/enemy/` and `src/object/` was checked for a `parent->` dereference
preceding its own `parent == NULL` check. Three matched; two are false
positives — `acroBandits.c` already carries a guarded PC_PORT self-heal for a
stale parent, and `ambientClouds.c` tests `this->parent != NULL` before
dereferencing — and the third is this file.

**Verification.** Recording replays clean twice at 320x240 and once at
240x160, exit 0, running past the recording's end to frame 5199. Gate 11/11
and `fetches=265497600 mismatched=0`.

**Lesson (46).** *A NULL pointer is readable on the GBA and fatal here, so
"the check is a couple of statements late" is a crash rather than a smell.*
Address 0 is BIOS: the read returns open bus, and code that dereferences then
immediately deletes the entity is correct on hardware by accident. The tell is
a `parent == NULL` (or `child`, `contactedEntity`) test that sits *below* a use
of the same pointer in the same function — grep for that shape rather than for
the crash.

## B55 — reversing during a room scroll strands the player outside the new room *(fixed)*

Reported 2026-08-26 with a recording: walk into a room transition, press the
opposite direction, and Link is stuck. **A 320x240-only bug** — the whole
mechanism is `VIEWPORT_SCROLL_FADE`, which is 0 at the shipping size.

**The stuck-player instrument was silent, and that was informative.**
`TMC_STUCK_TRACE` watches `PLAYER_ROOMTRANSITION`; here the player is in
`PLAYER_NORMAL` and perfectly free to walk. He is simply *outside the room*:

```
[f3100] room=6  bounds x=[1488..2496]  player x=2587  scrollAction=1  scroll_x=2176 (clamped)
```

91 px past the room's right edge, camera pinned at its clamp, and the edge
transition that would take him anywhere only fires from inside. Nothing is
hung; there is just nowhere to go.

**The deferral commits from a stale position.** At 320x240 a room scroll is
queued and applied 32 frames later when the screen is black
(`sub_0807BD14` → `ScrollTransitionApplyWhenBlack`). Link keeps walking for
those 32 frames — `sub_0807BD14`'s own comment says so. Measured:

| | frame | player x |
|---|---|---|
| queued, crossing left into room 6 | 1720 | **2501** |
| committed | 1752 | **2531** |

He had reversed and walked 30 px back into room 5. `Scroll2Step` then nudges
him 0.25 px per step from wherever he *is*: 20 px, landing him at 2511 —
outside room 6, which ends at 2496. From the crossing position it would have
been 2481, comfortably inside.

**This is the third thing the deferral loses**, and the first that strands the
player: his facing was the first (B16), the camera target the second (B24).
Both are already carried across the deferral; his position was not.

**Fix.** Capture `x.WORD`/`y.WORD` when the transition is queued and restore
them at the commit, so the room change lands on exactly the state the sliding
path had. Invisible — the apply only runs once the fade has reached black.
Guarded on the camera target: if a vehicle claimed it, the vehicle is moving
the player under its own speed for every frame of the fade, and that travel is
what B24 exists to preserve, so it is left alone.

**Verification.**

| | room | bounds | player x | |
|---|---|---|---|---|
| before | 6 | 1488..2496 | 2587 | **outside** |
| after | 5 | 2496..3216 | 2510 | inside |

**Vehicles proved untouched rather than argued.** The B24 and B40 recordings —
`lily_pad_softlock`, `lily_pad_softlock_2`, `minecart_softlock` — were replayed
before and after at 320x240 and dumped at three frames past the end of each:
**nine of nine pixel-identical.**

240x160 is untouched by construction (`sScrollFadePlayerX` is absent from the
binary) and gate is 11/11 with `fetches=265497600 mismatched=0` — but note
that gate is *silence* here, not coverage, because the mechanism compiles out
at that size. The vehicle diffs are the coverage.

**A 60-frame `TMC_STUCK_TRACE` threshold cried wolf.** Run at `=60`, the
240x160 build reported repeated `PLAYER_ROOMTRANSITION` stalls and looked like
a second bug; at the real 180-frame threshold it reports none. Mashing left
and right in a doorway dithers there for a while quite legitimately — the
instrument's own docs say `=5` fires on every ordinary doorway.

**Lesson (47).** *"Stuck" is not one state, and the instrument that names one
of them will stay silent for the others.* `TMC_STUCK_TRACE` was built for a
player held in `PLAYER_ROOMTRANSITION` (B16); this player was in
`PLAYER_NORMAL` with full control, outside the room's bounds, with no
transition to re-enter through. Ask *where is he* as well as *what state is
he in* — comparing his position against `origin_x..origin_x+width` is one
line and answers it outright.

## B56 — light rays come back bent after fading out and returning *(fixed)*

Reported 2026-08-27 with a recording (`light_ray_deformation.script`): walk east
through Minish Woods past the light shaft until it fades out, come back, and the
shaft returns *warped* — bent into an S and shoved left of where it belongs.
**Not a viewport bug.** It reproduces at 240x160 and was live in the shipping
build.

**The picture said it before any code did.** `sub_08057450` — the light shaft's
handler — writes exactly one horizontal quantity, the constant
`gScreen.bg3.xOffset = 0x10`. It has no per-scanline mechanism of any kind, so
nothing it does can bend a straight ray. Something else was writing BG3HOFS per
line, and in this room only one thing ever does: `sub_0805732C`, which the
*other* light state registers as an HBlank DMA to drive the parallax rays'
sine wobble.

**Measured with `TMC_MASK_BG3=1`, sprites and HUD off:**

| | 240x160 | 320x240 |
|---|---|---|
| first visit | `cols=115..239` spread 116 | `cols=195..319` spread 116 |
| after the round trip | `cols=14..159` spread 118 | `cols=138..283` spread 122 |
| fixed | `cols=115..239` spread 116 | `cols=195..319` spread 116 |

The band's pixel count is identical before and after at every sampled frame
(9724, 9770, 9771 at 320x240) — it was never gaining or losing content, only
being displaced per scanline.

**The warp is frozen, and that is the whole diagnosis.** Eighty consecutive
frames of the masked band are **byte-identical** while the camera is still, and
the sine phase of a *live* `sub_0805732C` advances every frame with
`gRoomTransition.frameCount`. So the table driving BG3HOFS was one nobody was
writing any more: a stale HDMA channel replaying a dead effect's last table.

**`DmaStop` was `((void)0)` in the port.** On hardware it clears `DMA_ENABLE`
and `DMA_START_MASK`, and for an HBlank-triggered channel that is the *only*
teardown there is: `VBlankIntr` calls `DmaStop(0)` every frame and
`PerformVBlankDMA` re-arms it only while `gVBlankDMA.ready`. So
`LightRayManager_Action3` ending the rays' fade with a bare
`gScreen.vBlankDMA.ready = FALSE` — never going near `DisableVBlankDMA`, which
is the one site that *did* unregister in the port — is correct on hardware and
was a leak here. `port_hdma_vblank_reset`'s rewind then made the leak
permanent by design: it exists so a channel that outlives its frame replays its
table rather than walking off the end of it, which is exactly what kept the
dead sine wobble running for the rest of the room.

**Fix.** `DmaStop(dmaNum)` unregisters the HDMA channel, which is what the
hardware macro it replaces does. Every HBlank DMA in the tree is registered
through `SetVBlankDMA`, so a live effect is re-armed by `PerformVBlankDMA`
inside the same `VBlankIntr` and never sees the teardown — verified rather than
assumed, below.

**`TMC_HDMA_TRACE=2` reports it in one run.** Per frame, any channel that drove
the raster without having been re-armed since the last VBlank:

```
[hdma] frame=2466 ch0 STALE io_off=0x1C count=1 (active, not re-armed this frame)
```

At 240x160 that begins on the frame after `Action3` clears `ready` and never
stops — 1018 frames of it before the shaft is even redrawn, then 304 more with
it on screen. After the fix, zero. `TMC_HDMA_KEEPSTALE=1` restores the old
behaviour from the same binary, which is how the before/after above was taken.

**The fix's blast radius was measured, not argued.** It changes when *any*
HBlank channel stops, so the question is which frames that reaches:

- **The canonical route runs the mechanism 4312 times.** Pre-fix, a single
  unbroken stretch of frames 5990–10301 had a stale WIN0H channel — a third of
  the route. All 4312 have DISPCNT's window bits **off**, so nothing rendered
  differently, which is why the gate can be 11/11 across a change this broad.
- **The live channels are untouched.** The 111 frames where a window is
  actually enabled, and the 64 of them the trace calls `PER-LINE` — the real
  iris transitions — produce byte-identical reports before and after.
- **On the report's own recording**, replaying the same binary with and without
  `TMC_HDMA_KEEPSTALE`, 7 of 11 dumped frames are pixel-identical and the 4
  that differ are the four the maintainer complained about. The frames where
  the dead rays' layer was still being warped are among the identical ones:
  `Action3` has faded it to nothing by then, so the stale wobble was invisible
  there. `TMC_MASK_BG3` still shows it, because the mask bypasses the blend —
  worth remembering when reading that instrument.

**Swept by mechanism, not by report.** Six existing recordings plus the
canonical route were replayed with `TMC_HDMA_KEEPSTALE=1`, which is the old
behaviour, and censused for stale channels:

| recording | stale frames | registers |
|---|---|---|
| canonical route | 4312 | WIN0H |
| `western_wood_softlock` | 1672 | WIN0H, BG3HOFS |
| `light_ray_deformation` | 1324 | BG3HOFS |
| `barrel_middle_exit`, `post-pause-glitch` | 0 | BG2PA registered, never stale |
| `mt_crenel_layers`, `minecart_softlock`, `minish_village_glitch_aug20` | 0 | — |

So three scenes leaked a channel and only one of them showed it. The western
wood cutscene leaked for 1672 frames across both registers and renders
**pixel-identical** at seven sampled frames with and without the fix; the
rolling barrel's affine channel — the one that would be most obvious if it
stuck — is torn down correctly and was never stale at all. The light shaft is
the visible case because it is the only one where a *live* layer was still being
drawn through a register a dead effect owned.

Gate: 11/11 waypoints, `fetches=265497600 mismatched=0`.

**Lesson (48).** *A no-op stub is a claim that the hardware operation did
nothing, and teardown is where that claim is usually wrong.* `DmaStop` was
stubbed out because the port performs DMA immediately and has no channel to
stop — true for every copy and fill in the game, and false for the one mode
where a DMA is a *standing registration*. The tell is a stub whose real
counterpart is called unconditionally every frame: nobody writes that unless it
does something.

**Lesson (49).** *Frozen geometry beside a live camera names a stale
producer.* The shaft was bent identically for eighty frames while the world
scrolled behind it. Anything genuinely driven by the running scene changes
between frames; a picture that does not is being fed by something that stopped.
That test cost one `cmp` of two mask dumps and it pointed straight at a table
nobody was writing, which is a much smaller search than "what bends a ray".

## D3 addendum: three scenes override their border colour

**Requested 2026-08-18 by the maintainer, not a bug fix.** D3 accepts whatever
the PPU backdrop happens to be around a centred 240x160 surface (B14), which is
what hardware shows outside every layer. Three scenes now override it:

| Scene | Was | Is | Because |
|---|---|---|---|
| Title screen | `0x57FF` pale yellow | `0x46C8` green | file select's colour |
| Pause menu | `0x46C8` green | `0x57FF` pale yellow | the title's colour |
| Rolling barrel (Deepwood) | `0x57FF` pale yellow | `0x0000` black | — |

`port/port_border_color.c`. Both replacement colours are the values those other
scenes already carry, read out of `gPaletteBuffer[0]` on the scenes themselves
rather than mixed by eye.

**It is one palette entry, not a repaint.** The border *is* BG palette entry 0,
so the override writes that — which means it needs no knowledge of where the
border is, cannot clip the HUD sprites that sit in the barrel's bands, and is
one store per frame instead of 38 400. It runs at the end of `FadeVBlank`, and
runs the chosen colour through `Port_FadeApply16` with bank 0's current fade
parameters, so the border fades with its scene instead of staying lit through
every transition. That is the same placement and the same reason as the B27
shadow palettes two lines above it.

**This is not B14 coming back.** B14 was a *global* repaint of the horizontal
band only, left behind by a reversed decision, which made a screen's two axes
disagree. This is per-scene, applies to all four bands by construction (it
changes the colour rather than the pixels), and is deliberate. Measured on
every waypoint: the three scenes change **0 px inside the centred 240x160** and
their whole ring outside it; file select, the figurine gallery and all six
world waypoints are byte-identical.

**Eligibility comes from the renderer, not from a second opinion.**
`Port_MapSource_UiCentered()` and the new `Port_MapSource_AffineCentered()` are
the decisions that centre the surface in the first place, so a scene can never
be recoloured in a border it does not have.

### The one that needed a second condition

The Nintendo/Capcom logo screen shares `TASK_TITLE` with the title, and the
intro step (`gUI.lastState`, indexing `sIntroSequenceHandlers`) advances one
fade *before* the picture does — so gating on the step alone recoloured the
logos for the 32 frames they spend fading out. Worse, that screen is the one
place here where the backdrop is drawn **inside** the centred 240x160 as well —
it is the screen's own white background — so the override repainted the whole
screen, logos and all, not its border.

The rule is therefore "the title screen, *and* its backdrop is the pale yellow
this is about" (`0x57FF`). The logo screen is `0x7FFF` throughout, fade
included, so it is never eligible. **The measurement that justifies this whole
approach — backdrop invisible inside the frame — held for the three scenes I
checked and not for the fourth I had not**, which is worth remembering before
adding a scene to the table: check it, do not assume it.

### Adding a scene

Add a case to `Port_BorderColor_Target`. Before doing so, capture the scene and
count how many pixels *inside* the centred 240x160 already carry the backdrop
colour. If that is not 0, this mechanism will repaint them too and the scene
needs the pixel-level treatment instead.

---

## Decision reversal: D1 is now *centered*, not edge-anchored

Recorded because the plan's §0 still shows the original choice.

Edge-anchored was chosen, implemented, and **abandoned**. It required
widening `gBG0Buffer`'s row stride from 32 to 64, and the stride turned out
to be baked into far more than the buffer's own accessors — the shared text
renderer's glyph writer, its per-line advance, and several byte-count clears.
Each was a silent corruption rather than a compile error, so each surfaced
only as a playtest bug. Three rounds found three more.

The variant now in place keeps BG0 at the hardware 32×32 shape. That has a
hard consequence: **a 32-tile map cannot place a tile past x=255**, so the
rupee/shell counter cannot reach the right edge; and a ~28-column text box
cannot be shifted 5 columns inside a 32-column row, so BG0 can only be
shifted *uniformly*. Hearts, counters, text box and UI screens therefore all
move together — i.e. centered. HUD sprites take the same shift at source
(`UI_HUD_SPRITE_DX`, `ui.c`) so they travel with the layer while world
sprites stay put.

**If edge-anchoring is wanted later** it needs the buffer widened *and* the
text renderer properly stride-parameterised — the 2–3 day job, not something
to add between bug fixes.

## Lessons worth keeping

1. **A harness that models the thing it verifies can agree with itself and
   prove nothing.** Spike 2's tile-diff indexed the special map with live
   `gRoomControls` while comparing against one-frame-old VRAM, then gated
   away the frames where that mismatched. Its "zero persistent mismatches"
   was measured over the subset that already agreed. The replacement
   (`--mapsource-audit`) measures *through the real render path*.
2. **Verify the verification's scope.** B7 existed because a grep proved a
   conversion complete in one file and the claim was made for two.
3. **Suspect ordering before suspecting the instruments.** B3 looked like a
   contradiction between trace and capture for an entire round; the trace was
   right and the code ran in the wrong order.
4. **Stride changes in decompiled code are not local.** The compiler cannot
   help: every `0x20` that meant "one row" is indistinguishable from every
   `0x20` that meant something else.
5. **Never put production behaviour inside a diagnostic.** The B3 ordering
   fix needed `mapsource_bind_ui()` to run later, and the convenient place
   that ran later happened to be a trace function gated on an env var and a
   room change. Ordering was fixed and the feature was switched off, in one
   move. It measured as a *success* — "6400/6400 px of world in the far-right
   columns" is exactly what you get when nothing clips — which is the same
   trap as lesson 1: the measurement could not tell "correct" from "disabled".
   When a fix makes a number jump to its theoretical maximum, check that the
   code you think produced it actually ran.
6. **A metric keyed on black is not a metric for borders.** The border-bleed
   check counts non-black pixels in the letterbox columns, which reads a
   correctly-clipped pause menu as 12 800 px of bleed because its backdrop is
   green. Count *distinct colours per column* instead: a clipped border is
   uniform whatever its colour. B14 is the same mistake made in *code* rather
   than in a measurement — the border was painted black to match the metric.

Lessons 7–14 are stated where they were learned: 7 in B11, 8 in B12, 9 in B13,
10 in B14, 11 in B5, 12 in B15, 13 and 14 in B16. 18 is in B20 and 19 in B21,
for the same reason. The three below are here because they are about the
tooling rather than about any one defect.

15. **A per-scanline table is indexed by the raster, not by the surface it
    decorates.** Wherever the port shifts a surface, any table addressing that
    surface by line has to take the same shift — and *lengthening* such a table
    is not *relocating* it. Spike 9 lengthened all nine and needed to relocate
    exactly one, which is why the miss survived a spike whose whole subject was
    these tables. B18.

16. **An item the debug actions do not grant is a screen the tooling cannot
    see.** `giveallitems` omitted `ITEM_MAP`, and without it the pause menu
    silently redirects every request for the three map screens. Nothing failed
    and nothing was logged; the screens were simply never in a capture, in
    either milestone, until a human opened one. When a menu gates a screen on
    inventory, the gate is part of the test surface. B18.

17. **A diagnostic that cannot be switched on where the bug happens is not a
    diagnostic.** Every `TMC_*` trace was gated on `getenv`, and an Android app
    has no environment — so the platform that reproduced B16, B17 and B19 was
    the one platform with no instruments. `--env=NAME=VALUE` in `args.txt` fixed
    that, and B19 then reproduced on desktop from a device recording on the
    first replay, against six rounds for B16. A log that cannot name its own
    build is the same problem one step earlier: the first dungeon-softlock
    report could not be attributed to a binary at all.

---

## Milestone 1 exit criteria — met, signed off 2026-07-30

| Criterion | Result |
|---|---|
| 240 route pixel-identical | **11/11, 0 differences** |
| 240 map-source audit | **0 mismatched in 265 497 600 fetches** |
| No layer wraps/repeats at 320x160 | **0 wrap-period columns**, 40-frame opening sweep + 11-waypoint route |
| Rooms narrower than 320 centred with borders | **verified** on every room tested; borders are a uniform colour (see D3 below) |
| Frame time at 320x160 within +25% | **present 7.19 ms mean** vs the 6.48 ms Spike 1 canvas baseline = **+10.9%** |
| Go/no-go for Milestone 2 | **GO** — maintainer approval, 2026-07-30 |

Frame time measured the same way as the baseline: canonical route (12 700
frames), headless dummy video, uncapped, release build, n=3 runs —
7.263 / 7.148 / 7.151 ms (run 1 carries warm-up). p99 9.25–11.12 ms, max
12.5–14.7 ms. Logic is unchanged at 0.15 ms mean. Total ~7.34 ms against the
16.67 ms budget (**44%**).

The +10.9% over a canvas build whose presented surface is already 320×240 is
the extra PPU rasterisation for 33% more viewport pixels; present cost itself
is dominated by a texture upload whose size did not change.

**Decisions taken at sign-off**, so they are not relitigated:

- **B4 and B5 deferred**, not fixed. Neither was ever reproduced.
- **D3 amended: coloured borders are accepted.** The plan's D3 said solid
  black. That holds wherever the backdrop is black (gameplay, the legend), but
  a clipped UI screen shows the *PPU backdrop* in its border bands — green on
  the pause menu, grey in the figurine gallery. The bands are uniform, so the
  clip is working; they are simply not black, which is what hardware shows
  outside every layer anyway. Accepted as-is rather than forced.
- **World-space window sites deferred** (carry-forward below).

The 240x160 gates above were re-run after every change in this document and are
the standing regression gate; keep running both before any viewport commit
(`tools/capture/README.md`, "Regression gate").

## Carry-forward items — what Milestone 2 inherits

Recorded here so they are not lost with the plan's spike sections. Routing:

| Item | Lands in |
|---|---|
| ~~Title screen affine sword~~ | **Done** — `docs/affine-viewport.md`; verified pixel-exact |
| ~~Per-scanline circular windows~~ | **Done — Spike 9.** See B11 and `docs/spike9-hdma-240.md` |
| World-space window x masked to 8 bits | Spike 9, or sooner if a scene is reported |
| Kinstone menu unverified | any real playthrough |
| Quicksave state files not portable | nothing — recorded as a dead end |

The per-scanline windows turned out to be a live defect rather than an
unwidened one — recorded as B11 below.

**The affine half is done for the scenes that can be reached.** The title
sword and the rolling barrel are both fixed and verified
(`docs/affine-viewport.md`); the barrel was one warp away from the scripted
tester the whole time, which is why "unreachable" was worth re-testing rather
than believing. Vaati's tornado and the screen-shrink cinematic are still
unreached — the tornado's per-line effect turns out to be a BG3 scroller
rather than affine, so it is probably already covered, but that is reasoning
and not observation.

Of the list above, only the 8-bit world-space window masks remain genuinely
untouched, and they still want their scene reproduced before anyone edits
them — `include/screen.h` warns that several rely on the wrap to produce an
*inverted* window.

- ~~**Title screen affine sword** sits ~40 px left.~~ **Fixed** —
  `docs/affine-viewport.md`. `mode2.c`'s affine path now honours the same
  centring clip the text path does, which was the missing channel rather than
  a reference-point calculation. At 320x240 the title's centred 240x160 box is
  pixel-identical to the 240x160 reference. The rolling barrel turned out to
  be reachable by warp (Deepwood Shrine room 32) and is verified too; the
  tornado's per-line effect is a BG3 scroller rather than affine.
- **Kinstone menu** never runtime-verified: it crashes on cold scripted entry
  at *both* 240x160 and 320x160, so it is the pre-existing kinstone crash chain
  (CHANGELOG #16) rather than a widening bug. Verify during a real
  playthrough with fusions available.
- **Per-scanline circular windows** (lantern, fade iris, white-triangle) use
  a DMA'd per-line table that has not been widened — Spike 9.
- **Quicksave state files are not portable across processes.** `F5`/`F6`
  (`port/port_quicksave.c`) are process-local by design. Persisting them was
  implemented and reverted: restoring one in a fresh process segfaults in
  `CollideFollowers` (`src/npcUtils.c:318`) walking `currentEntity->next`,
  because the snapshot restores `gEntities` without every global that
  participates in the entity lists. **Not ASLR** — it reproduces with
  `setarch -R`, so pinning the address space does not help. Making it
  portable means an exhaustive inventory of participating globals plus
  relocation of every host pointer inside the snapshot. Input recording
  (`--record`) solves the actual need instead, and has no pointers to fix up.
- **World-space window sites still mask their x to 8 bits.** Found while
  fixing B9, not yet reproduced, and *not* fixed — these are gameplay effects
  whose windows are computed from world-to-screen coordinates:

  | Site | Expression |
  |---|---|
  | `src/scroll.c:347`, `:414` | `WIN_RANGE(left & 0xff, right & 0xff)` |
  | `src/object/lightDoor.c:77` | `WIN_RANGE((tmp2 - 0x18) & 0xff, (tmp2 + 0x18) & 0xff)` where `tmp2 = entity x - scroll_x` |

  At 240 a screen x could not exceed 255 and the mask was free. At 320 it can:
  a light door at screen x=300 masks to 44 and the window jumps to the far
  side of the screen. `templeOfDropletsManager.c` and `bigGoron.c` compute
  `tmp1`/`tmp2` similarly and want the same look.

  **Deliberately left alone.** `include/screen.h` warns that several sites
  rely on 8-bit wrap-around to produce an *inverted* window (left > right),
  which the PPU renders as a wrap — so removing a mask can change intended
  behaviour, and the header states that widening a site's coordinate range is
  a per-site decision for the spike that needs it. Each needs its scene
  reproduced before it is touched. The light door is the cheapest to reach.
- ~~**BG3 gameplay overlays** were never swept for wrap past 256 px.~~
  **Swept — see B10.** Wrap was not the defect; the centring clip was.
- ~~**Milestone 1 frame time at 320x160** is unmeasured.~~ **Measured** — see the
  exit-criteria table above. Note the baseline for any future comparison is
  the Spike 1 canvas build (present 6.48 ms mean), *not* the Spike 0 240
  baseline: the canvas cost is paid once and must not be charged twice.
