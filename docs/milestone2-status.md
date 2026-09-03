# Milestone 2 — status at session close, 2026-08-22

The height expansion (320×160 → 320×240). Every planned spike is landed, plus
the items the plan did not anticipate, and **fifty-four of the fifty-nine
tracked bugs are closed** as of 2026-08-27 (B41, B42, B46, B47 and B49 remain
open — the tracker is authoritative for the count), B27 included —
Hyrule Town, festival town and Minish Village, playtested and confirmed
2026-08-11. **One thing is still open and it is a judgement rather than work:
frame time.**

**B45 closed 2026-08-22 with a hardware oracle, and it is the session's other
result.** mGBA turned out to run headless on this machine, with a stdin-driven
debugger and savestates that carry a frame's state *and* its picture — so "is
this the port or is this the game?" became a measurement. The Castor Wilds mud
was the port dropping transparent OBJ pixels: the game draws twelve *blank*
priority-2 sprites over the player and the OBJ layer composites at the priority
of the **last covering sprite in OAM order**, opaque or not, so those blanks
lend the player a tie against the ground and he shows through the mud, clipped
from the bottom as he sinks out of the rectangle. Six passes had compared every
register, OAM entry, map and tileset against hardware, matched on all of them,
and concluded the port was faithful — the difference was a compositing rule,
which no state comparison can see. `tools/mgba/README.md` is the technique.
The fix is in the `libs/ViruaPPU` submodule (its PR #7) and is carried here by
the pointer at `53c7cc4`; it took a second pass to land, and why is worth
reading in B45.

**B46 and B47 came out of the same work and are open.** B46: the wading overlay
never draws in shallow water, provable by inspection. B47: the port's `tmc.sav`
is byte-reversed per 8-byte EEPROM block against hardware, so saves do not
interchange with mGBA or a cartridge in either direction — `tools/mgba/savconv.py`
converts, and fixing `port_save.c` needs a migration for the existing
recordings' saves.

**B43 closed 2026-08-22, and it was never a viewport bug.** The western-wood
takeover cutscene ended on a permanent black screen at 240x160 as well as at
320x240 — the maintainer's 240x160 recording is what made it reproducible on
the shipping build. One port-only line in `CreateVaatiApparateManager` deleted
a real entity where the GBA's version dereferences a ROM function pointer and
does nothing; the entity was stale across the cutscene subtask's list-head
swap, so unlinking it wrote `gEntityLists[6].first` back onto the overworld
chain and the takeover orchestrator stopped being reached — never deleted,
never unlinked. That single line owned the black screen, issue #93's softlock,
and the missing fade-in. The #93 watchdog in `sub_08053BBC` is removed with it,
which restores the cutscene's GBA-native ~20 s pacing in place of the three
seconds the watchdog rushed it through. Gate green in both halves.

**B21 closed 2026-08-20**, having been recorded as unfixable since 2026-08-07.
Every blocked route had been an attempt to make the layer reach the extra
80 px; it never had to. See below. **B34** was found the same day in the same
layer — the vertical twin of B32 — and is the only bug in the tracker found by
instrument rather than by a playtest report. **B35** is a defect in B21's own
fix, reported the same day with two recordings: the anchor was tied to the
declaring handler's tick rather than to the overlay's lifetime, so the band
jumped left when the rays began to fade and whenever a text box opened.

**B33 (2026-08-20) is B27's authored-gap decision meeting the periphery.** A
tile in a gap between region rectangles takes the group the engine loaded, which
is what hardware shows it — right inside the GBA's screen, arbitrary outside it,
where the tile changes tileset whenever the camera crosses a threshold that has
nothing to do with it. Peripheral gap tiles now take the group of the region
they adjoin, with a guard rect keeping the centred screen byte-identical.

**B32 (2026-08-20) is the same "the window was sized for the GBA's screen"
shape one level up.** MinishPaths' parallax layers re-base their 32-tile
screenblock every 64 px, which leaves `yOffset + 240` running past the block's
256 px and wrapping; re-basing every 16 px keeps the whole screen inside it.
Its horizontal twin cannot be fixed the same way and is untouched — see the
tracker.

**B31 (2026-08-20) was the actual last of B27's cases.** The town manager's init reset
ran a frame after `OnEnterRoom` had declared the room's slots and wiped all
three, and only a camera-driven group change re-declares one — so from every
room entry the periphery drew from the centred screen's group until the camera
crossed a region threshold. B30's fix hid half of it, because the slots *it*
covers are re-declared every frame.

**B30 (2026-08-18) closed what looked like the last of B27's outer-40 px cases.** A tileset slot
whose regions the centred 240x160 never touches is never loaded, and loading was
the only thing that declared a slot to the per-tile renderer — so it had no
answer at all and drew the previous room's tiles until the camera moved.

**B29 (2026-08-18) is a Milestone 1 regression that the gate could not catch.**
Spike 6 moved `gBG0Buffer` out of `gEwram`, and the ROM `Font` blobs that draw
the area-name banner carry the old EWRAM address in their data, where Spike 6's
source grep could not see it — so the banner has rendered into dead memory since
2026-07-29. The canonical route spawns five of them per run and samples none,
which is why 11/11 stayed green throughout.

**B28 (2026-08-17) is in the tracker but is not a viewport bug at all** — the
asset extractor was truncating room-property blobs at the ROM pointers embedded
in them, so Lon Lon Ranch's locked door had no script and could be walked
through, at 240×160 exactly as at 320×240. It is here because it was reported
off the 320×240 play build, and because it closes three of `CHANGELOG.md`'s open
issues (#28, #37, #40).

The 2026-08-08 session added four (B22–B25), all in or around Deepwood's
rolling barrel, and **two of them — B23 and B25 — turned out to be live at
240×160 as well**, bringing this milestone's tally of pre-existing defects it
merely exposed to six. Neither would have been found without the expansion:
B22 had made the room unplayable, and fixing it is what got anyone to use the
barrel's middle exit on purpose.

Since 2026-08-02 this milestone also grew an **arm64 Android build**
(`android/`). It is not a separate port — it is this viewport on other
hardware, built from the same source list — and it has already earned its keep
as a test surface: B16 and B17 were both reported from it, and neither turned
out to be a platform bug.

Read this first; the individual spike write-ups are the detail behind each row
and several carry inline "superseded" notes pointing back here.

## What landed

| Piece | Write-up | Result |
|---|---|---|
| `TMC_VIEW_H` build switch | — | `TMC_VIEW_W=320 TMC_VIEW_H=240` on both xmake commands |
| Spike 8 — OAM y | `spike8-oam-y.md` | Untruncated y beside OAM; 0 unresolved over ~200k entries, 0 rescued at both 160-line builds |
| Spike 9 — per-scanline tables | `spike9-hdma-240.md` | All nine tables extended; **B11** found — circular windows had been dead since Spike 4 |
| Spike 10 — vertical camera | `spike10-vertical-camera.md` | Camera centres and clamps on both axes; 0 out-of-range over ~14k frames |
| Spike 11 — vertical culling | `spike11-vertical-culling.md` | 12 tall rooms walked, 0 vertical wrap; **B12** found |
| Vertical UI centring | `ui-vertical-centring.md` | UI screens centred exactly (rows 40–199, cols 40–279) |
| Affine layers | `affine-viewport.md` | Title pixel-identical to 240×160; rolling barrel reached and aligned |
| Iris width | `spike9-hdma-240.md` §6 | Circle no longer sliced at x=240 |
| Text box placement | `ui-vertical-centring.md` §4 | Keeps its authored position inside the centred frame |
| **B13** — NPC pop-in | bug tracker | Found from a maintainer recording; confirmed fixed 2026-08-01 |
| **B14** — UI side borders forced black | bug tracker | Reported by the maintainer 2026-08-02; all four bands now one colour |
| Legend panels centred vertically | `ui-vertical-centring.md` §4-5 | The last deferred vertical case; decided and fixed 2026-08-02 |
| **B15** — furniture lit against black through a door/stair fade | bug tracker | Reported with a recording; both room-change substates keep their map source above native size |
| **B5** — interior room-to-room scroll glitches | bug tracker | Deferred since Milestone 1; reproduced from a recording and the slide replaced by a fade above native size |
| **B4** — smith-room sprites at first dialogue | bug tracker | Closed 2026-08-02 as no longer observed — the one entry with no root cause. See B16 for a possible retrospective identification |
| **Android build** | `android/README.md` | arm64-v8a, assets baked in at build time, source list parsed from `xmake.lua`. Gamepad only, no touch |
| **B16** — softlock entering the smith room | bug tracker | Reported from Android, 2 runs in 3; three layered defects, the last being the player losing his facing across the fade's deferral |
| **B17** — Minish interiors render as sprites over black | bug tracker | Third instance of the screenblock-cannot-cover-320 family; needed the tile mutators to maintain the degraded map, not just a relaxed predicate |
| Screenblock-fallback sweep | bug tracker | Asked once which other paths can fall back above native size. On everything reachable, B17 was the only one |
| **B18** — pause map detail view shows only the top of the map | bug tracker | The per-scanline BG3 curtain's band was still in 240x160 rows. The only one of the nine HDMA tables on a UI screen, so the class has one member and it is closed |
| Android diagnostics | `android/README.md` | `--env=NAME=VALUE` carries the `TMC_*` traces onto a device, which had no environment to set them in; every run now logs its build identity; `TMC_STUCK_TRACE` watches the state B16 hung in |
| Playable 240x160 build | `CLAUDE.md` | `build/play-240x160/` alongside the 320x240 one, so "is this the expansion's fault or was it always like that?" is answerable by hand. Installed from the gate's own binary, so it costs no extra configure |
| **B20** — gameplay flashes at 240x160 across a pause transition | bug tracker | The centring clip changed several frames before the picture did. Now changes only on a black frame; the close direction has to be anticipated from `gUI.nextToLoad` because its black frame precedes the state change |
| **B19** — segfault entering a room narrower than the viewport | bug tracker | A `u32` local made a pointer offset unsigned, so a negative camera offset wrapped to +4.29e9 and the pointer walked 8.6 GB. **Reported from Android with a recording and reproduced on desktop on the first replay** — the diagnostics above are why |
| **B22** — rolling barrel unplayable | bug tracker | The player pin measured the barrel's midline from the camera, not the room; 40 px of error in the one room that is exactly viewport-sized. Doors out of reach, barrel self-rolling. Rim sprites in the border left open as a costed decision |
| **B23** — barrel drawn out of step with its own logic | bug tracker | The port had bypassed the barrel's rotation gate since long before this milestone; restoring it exposed the real defect, an affine renderer accumulating `pb`/`pd` on top of a reference HBlank-DMA had already set per scanline. Was wrong at 240x160 too |
| **B24** — lily pad strands the player outside the room | bug tracker | The vehicle's carry state exits on `reload_flags == 0`, which the faded path leaves true for the 32 frames it defers the apply, so the pad quit 28 frames before the room changed. Carry distance then trimmed to a GBA slide's worth |
| **B25** — barrel returns as noise after a pause | bug tracker | A port-only forced buffer→VRAM copy wrote text tilemaps over both of the room's own maps. Frame is now pixel-identical across the pause. Was wrong at 240x160 too |
| **B26** — town scenery from the wrong tileset | bug tracker | Region tables that drive a tileset swap are authored with a gap sized for a 160-row screen; 80 extra rows make two regions match at once and first-match-wins loads the earlier one. Now picks the region covering most of the screen |
| **B27** — scenery in the outer 40 px drawn from a non-resident tileset | bug tracker | The residual B26 could not reach: matching hardware's choice only guarantees the 240×160 the GBA would have shown. Emulated VRAM now carries a bank per tileset group above the GBA's 96 KB, every alternative stays resident, and the renderer picks character data *and palette* per tile from the tile's own room position. All three areas; four Minish Village recordings go from 2.2–10.7% periphery change across a threshold to **0.00%** |

## Gates

Both hold at 240×160 and were re-run before every commit in this milestone,
including all four of the 2026-08-08 barrel and lily-pad commits:

- canonical route: **11/11 waypoints, 0 differences**
- map-source audit: **0 mismatched in 265 497 600 fetches**

**What the gate did not catch, and could not.** B16 lived in
`sub_080797C4`/`gUnk_0811C110`, which the canonical route never executes —
zero events in 13 000 frames. B17 lived in rooms the route never enters. B18
lived on a screen no script could open at all, because `giveallitems` did not
grant the item the menu gates it on. All three passed the gate throughout while
broken. Where a change touches code the route does not reach, say so and find
another argument; for B16's ROM table that argument was that indices 0–3 are
byte-identical, so the change could only alter a read that was previously
undefined. For B18 it was stronger and worth copying — build the shipping
240x160 binary both with and without the change and diff the captures of the
screen itself. Byte-identical across all 14 waypoints is a direct statement
about the code that changed, not an inference from coverage.

## The one open item: frame time

Canonical route, 12 700 frames, headless dummy, uncapped, release, n=3 — the
same method as the Spike 0/1 baselines.

| Build | present mean | p99 | max |
|---|---|---|---|
| Spike 1 canvas baseline | 6.48 ms | — | — |
| Milestone 1, 320×160 | 7.19 ms | 9.25–11.12 | 12.5–14.7 |
| **Milestone 2, 320×240** | **9.15–9.31 ms** | **12.39–12.76** | **17.66–21.66** |

Two facts, both for the maintainer to weigh:

- **+41% over the canvas baseline**, against the +25% that was Milestone 1's
  stated criterion (Milestone 1 itself came in at +10.9%). Milestone 2 was
  never given a budget of its own — the Spike 11 DoD asks only that the number
  be recorded. Against 320×160 it is +27% for 1.5× the pixels, i.e. sub-linear,
  which is the expected shape for a present path whose texture upload is fixed.
- **Peak frames now exceed the 16.67 ms deadline** — 17.7–21.7 ms, where
  320×160 peaked at 14.7. Mean and p99 sit comfortably inside, so this is
  occasional rather than sustained, but it is the number most likely to be felt
  as a stutter.

No go/no-go is recorded. That is the maintainer's call and it rests on this.

## Closed item: B21, the Minish Woods light shaft

**Fixed 2026-08-20.** The 2026-08-07 diagnosis established a set of facts that
were all true — the map is 256 px, the shaft ends at map px 255, no offset can
place ray content past 255 without repeating it — and drew the wrong conclusion
from them, because it never asked what the columns past 239 were *currently*
showing. `BG3 contributes 0 px beyond 239` was read as "there is nothing out
there to draw"; it also fits "the thing out there is transparent", and that is
what it was. The layer wraps at 256 and the wrap was bringing its own blank
left end into view.

The port already clips 240-authored layers and places them. BG3 in a world view
was deliberately exempt, for overlays that are tiled and world-locked — hole,
cloud, weather, steam, POW — where the wrap is what covers the wider screen.
The light shaft shares neither property. It now declares itself
(`Port_MapSource_DeclareBg3ScreenAnchor`) and takes the clip, pinned to the
right edge of the **room**: Minish Woods fills the screen, but the barrel
minish house is 240×368 and centred, so the viewport's edge is 40 px past the
room's. No VRAM, no new artwork, no repeated shaft.

Full account and lessons 31–32 in the tracker.

## Known remaining, none of them blocking

| Item | State |
|---|---|
| **Vaati's tornado** | Unreached — needs a story state the scripted tester cannot produce. Its per-line effect is a BG3 scroller rather than affine, so it is probably already covered; that is reasoning, not observation. |
| **The screen-shrink cinematic** | Named in the plan; no concrete site found in the source. Unidentified rather than done. |
| ~~**Picori legend panels**~~ | **Centred 2026-08-02** at the maintainer's request — `ui-vertical-centring.md` §5. Pixel-identical to 240x160 shifted 40 px on both axes; the world half of the same cutscene is byte-identical, so B3 stands. |
| **Kinstone menu** | Still never runtime-verified; it crashes on cold scripted entry at 240×160 too, so it is the pre-existing crash chain (CHANGELOG #16). |
| ~~**B4, B5**~~ | **Both closed 2026-08-02.** B5 reproduced from a recording and fixed; B4 closed as no longer observed, the one entry in the tracker without a root cause. |
| **The debug-warp crash** | **Open.** Warping to some destinations segfaults, and a second intermittent crash near teardown is tangled with it. Two real validation gaps were closed (room existence, coordinates inside the room) and coverage widened measurably, but it is not fixed. It is what stops the screenblock sweep from covering more than a fraction of the ~128 areas. |
| **`subTileMap rebound`** | **Never observed.** The one map-source rejection class the sweep could not reach, and therefore the most likely place for a fourth screenblock-fallback instance. Blocked behind the warp crash. |
| **Tile mutation in degraded rooms** | B17's fix makes the mutators maintain that map; the maintenance itself was verified by reading the code, not by driving a mutation. Cutting grass or lifting a pot inside a Minish house is the check nobody has run. |
| **Festival town** | **Never playtested for B27.** Its tables convert correctly and the mechanism engages — verified by debug warp — but nobody has walked its region boundaries at 320×240 and no recording of it exists. The 2026-08-11 playtest covered Hyrule Town and Minish Village, which is where all ten reports came from. |
| **Minish Village at 240×160** | **Never looked at.** Two of its palette groups are needed at once in 308 camera positions at the *shipping* size, so a little of what B27 fixed above may also be visible there. Not reported, not reproduced; the 240×160 play build is where to check. |
| ~~**B21 — Minish Woods light shaft ends 80 px short**~~ | **Fixed 2026-08-20.** Not a reach problem — a wrap problem. BG3 in a world view is exempt from the centring clip so the *tiled* overlays can wrap across the wider screen; this layer's map is blank across two thirds of its columns, so the wrap showed that blank end past x=239. Now declares itself screen-anchored and takes the clip, pinned to the room's right edge (the barrel minish house is 240×368 and centred, so room edge ≠ viewport edge). Lessons 31–32. |
| **B34 — light shaft's lower rows show the top of its own block** | **Fixed 2026-08-20.** The vertical twin of B32 in a different manager, found while measuring B21 and never reported. A 64-px re-base leaves `yOffset` up to 63 and a 240-row screen needs `yOffset + 240` of a 256-row block; re-based on 16 px. Lesson 33 — the consecutive-pair shift test that settled B32 scores zero on a uniformly wrapped layer. |
| **B35 — light rays jump left on fade-out and on a text box** | **Fixed 2026-08-20.** B21's anchor was declared per frame and cleared per frame, so its lifetime was the handler's tick rather than the overlay's. A fade-out dispatches to a null handler on its first frame (`unk_21` is set to the *trigger* type) and a text box suspends the managers. Now expires on BG3 going off or the room changing. Reported with two recordings; both reproduced by replay and fixed. Lesson 34. |
| **B36 — Mt Crenel summit renders black apart from sprites** | **Fixed 2026-08-20, and not a viewport bug** — it reproduces at 240x160 and was live in the shipping build. `gPalette_549` is the head of a 26-palette contiguous block; the weather manager cross-fades the summit against `gPalette_549 + 0xD0`, which is an address only because the GBA linker laid them out sequentially. `port_linked_stubs.c` allocates the whole block and its comment says `port_rom.c` fills it — it never did, so both sides of the mix read zeros. 97.3% black to 1.1%. Lessons 35-36. |
| **B37 — Mt Crenel rain fills only the centred 240 columns** | **Fixed 2026-08-20.** The weather manager takes BG1 from the room's top map layer for a tiled rain sheet; with the map layer off the map source is refused and the fallback clip confined it. The exemption that lets BG3's overlays wrap is about being a *tiled overlay*, not about BG3. `cols 40..279` to `0..319`. Lesson 37. |
| **B38 — vapour wisps and steam render opaque white** | **Fixed 2026-08-20, and not a viewport bug** — OAM attr0's OBJ-mode field was never implemented at any size. A mode-1 sprite is a blend first target regardless of BLDCNT, which is why `bldcnt=0x2F40` has an empty first-target field and still blends. Covered by a 177-frame dense route diff rather than the 11-waypoint gate. Lesson 38. |
| **B39 — rain layer is garbage after the pause menu** | **Fixed 2026-08-20, and pre-existing** — 13,567 garbage px in the centred 240 before B37, 13,540 after, so B37 only widened it. `RestoreGameTask`'s post-menu buffer push wrote the room's top tilemap into BG1's screenblock, over the reload the re-run weather handler had just done. Now skips a BG no map layer is bound to — B25's exclusion generalised. Covered by the dense route diff; the gate's waypoints contain no post-menu gameplay frame. Lesson 39. |
| **B40 — Cave of Flames minecart softlock** | **Fixed 2026-08-20.** B24's defect in the file B24's own fix named as having the same shape. `Minecart_Action5` ends its carry state on `reload_flags == 0`, true for all 32 frames of a deferred faded transition, so the cart hands the camera back before the room changes. Guarded with `ScrollTransitionIsPending()`. An expansion bug by construction — the deferral only exists above native size. Lesson 40. |
| **`sub_0807D280` reads before its map for short rooms** | **Latent, not reproduced.** B19 fixed the *unsigned wrap* in `case 2`. `case 1` and the `default` branch feed a negative `ydiff` — `-40` in the steady state of any room shorter than the viewport — to `(ydiff >> 4) * 0x100`. Signed, so no wrap and no crash; it reads a kilobyte or two before `gMapDataBottomSpecial` into the screenblock. Above native size the world is drawn from the map source instead, which is likely why nothing has been seen. Wants its own reproduction first. |
| **`gUnk_0811C0F8` / `gUnk_0811C108` read past their end** | **Latent, not reproduced.** Both are four-entry `u16` tables sitting contiguously in ROM with B16's `gUnk_0811C110`, and both are indexed by `direction >> 2`, which reaches 63. On hardware the index wraps into an identical adjacent copy — `0x0811C108[4..7]` is byte-identical to `0x0811C110[0..3]` — so every direction lands on a real value. Ported, each array is its own object. B16 extended only `gUnk_0811C110`. Reachable only on the *swim* branch, so it needs a scene where the player is swimming through a room transition; `TMC_OOB_TRACE=1` reports it and stayed silent across the dungeon-softlock recording. Lesson 13 says the ROM bytes are the specification, so the fix is B16's: extend both with the real bytes, PC_PORT only. |
| **Dungeon-entrance softlock (Deepwood Shrine, room 10 → 6)** | **Open, Android-only, intermittent, and not seen since.** The maintainer's recording traverses the transition and does not hang on desktop; the per-frame camera assertion is clean over it. A device run with `TMC_STUCK_TRACE=1` produced **no `[stuck]` line** and did not reproduce it — but did hit B19 further into the dungeon. Whether the two were ever the same event is unknown. |

## What this milestone changed about how the work is checked

Four defects in this milestone were **live in the shipping 240×160 build or
through all of Milestone 1**, not introduced by the height work: B11 (circular
windows dead since Spike 4), B12's horizontal half, B13's horizontal half, and
the iris veto that was Spike 9's own validity key. The expansion did not break
them; it made them visible.

That is worth stating plainly because it changes what the regression gate is
for. The gate proves the shipping build did not move. It cannot prove the
shipping build was right — B11 rendered a near-black screen on 64 frames of
the canonical route and the gate passed 11/11 throughout, because none of the
11 waypoints lands on those frames.

The lessons added here, in the tracker's numbering:

- **7** — a gate made of still frames cannot see a defect that only exists
  mid-transition.
- **8** — a conversion sweep's own report of what it converted is not
  evidence.
- **9** — ask what a predicate *gates*, not just what it is called.
  `CheckOnScreen` decides whether to draw; `CheckRectOnScreen` decides whether
  the entity exists.
- **12** — a probe that failed is evidence about the state it ran in, not
  about the change. B15's fix was measured, rejected and reverted one session
  before it became correct.
- **13** — an out-of-bounds read in decompiled code is a platform difference
  waiting to happen. On hardware ROM is contiguous and the read is defined;
  ported, it returns whatever the linker placed next, which is stable per
  toolchain and different between them.
- **14** — a bug that only reproduces on one platform is not necessarily a
  platform bug. B16 cost six rounds of asking what was different about Android
  when the answer was that one accidental read let desktop recover from a fault
  both platforms had.
- **15** — a per-scanline table is indexed by the raster, not by the surface it
  decorates, and lengthening one is not relocating it. Spike 9 lengthened all
  nine of these tables and needed to relocate exactly one, which is how B18
  survived the spike whose whole subject was them.
- **16** — an item the debug actions do not grant is a screen the tooling
  cannot see. `giveallitems` omitted `ITEM_MAP`, so the pause menu silently
  redirected every request for the three map screens and no capture in either
  milestone ever rendered them.

And one that did not need a number because the tracker already had it: after a
round of inference on B13 produced nothing, a recording found it in one pass.
The same held for B5, B15 and B16 — every bug in this milestone that needed a
human at the controls was resolved by a recording, usually after inference had
already failed on it.

**Four of the twenty-five were one assumption reported four times.** B5, B15 and
B17 are all a world layer losing its map source and falling back to a 256 px
screenblock that cannot cover 320. Each was found by a separate playtest report
before anyone asked the general question; when it finally was asked, the sweep
that answered it needed no new code — `TMC_REJECT_TRACE` and
`--mapsource-report` had existed since Milestone 1. Ask the general question
after the second instance, not the third.

**B22 is the fourth, and the sweep did not catch it** — because the sweep asked
about *width*, and B22 is the vertical case. It is also the first of the family
to break gameplay rather than rendering: the rolling barrel held the player on a
midline it measured from the camera instead of from the room, which are the same
number only because that room is exactly 240x160. A room that is exactly
viewport-sized on hardware makes camera-relative and room-relative coordinates
indistinguishable, so every such expression in one is unverified until the
viewport changes. That is the general question this time, and it has not been
swept.
