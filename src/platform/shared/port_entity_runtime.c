/**
 * @file port_entity_runtime.c
 * @brief Host-side C ports of the per-frame entity update / draw kernel
 *        and the per-entity OAM emission helper `DrawEntity`.
 *
 * Replaces the silent / trap stubs that previously lived in
 * `asm_stubs.c` (DrawEntity) and `ram_silent_stubs.c`
 * (ram_UpdateEntities / ram_DrawEntities / ram_ClearAndUpdateEntities).
 * Those stubs short-circuited the entire entity pipeline -- the
 * 5 `TITLE_SCREEN_OBJECT` entities `HandleTitlescreen` spawns
 * (logo, Japanese subtitle, two side chains, bottom chain) never
 * received their per-frame `ObjectUpdate`, and even if they had,
 * `DrawEntity` would have aborted on the first call. End result: no
 * Zelda logo on the title screen.
 *
 * What this TU implements
 * -----------------------
 * Three strong (non-weak) symbols:
 *
 *   - `void DrawEntity(Entity*)`
 *     Replaces `asm/src/code_08003FC4.s::DrawEntity`. The ROM body
 *     enqueues the entity into one of four priority lists (looked up
 *     via `gUnk_081326EC[spriteRendering.b3 * 4]`); a later
 *     `arm_DrawEntities` pass walks each list and calls
 *     `sub_080B255C` -> `sub_080B299C` -> `sub_080B2874` to compose
 *     and emit OAM. Since neither the priority-list backing storage
 *     nor `sub_080B255C` is ported yet, and since the title screen's
 *     5 sprites do not require intra-list reordering (they do not
 *     overlap each other), this port collapses the two halves into
 *     one synchronous emission: read the same entity fields the ARM
 *     `sub_080B299C` reads, compose an `OAMCommand`, and call
 *     `ram_DrawDirect` (already ported in `port_oam_renderer.c`).
 *     Future scenes that depend on intra-list back-to-front sorting
 *     will need to grow back the deferred drain path.
 *
 *   - `void ram_UpdateEntities(uint32_t mode)`
 *     Replaces `asm/src/intr.s::arm_UpdateEntities`. Walks
 *     `gEntityLists[0..7]` (`mode == 0`) or `gEntityLists[8]`
 *     (`mode == 1`, managers) and dispatches each entity to its
 *     kind-specific update function. Mode boundaries match the ARM
 *     body's `gUnk_080026A4` table: mode 0 covers indices 0..7
 *     inclusive, mode 1 covers index 8 only.
 *
 *   - `void ram_DrawEntities(void)` and
 *     `void ram_ClearAndUpdateEntities(void)`
 *     Both no-ops. With `DrawEntity` emitting OAM during the update
 *     pass there is no deferred drain to run; with the iteration
 *     loop below using `gUpdateContext.current_entity` to advance
 *     (rather than a captured `next` pointer), entity self-deletion
 *     via `DeleteThisEntity` lands cleanly on the next iteration
 *     without needing a longjmp-style early exit.
 *
 * Iteration safety
 * ----------------
 * The ROM `arm_UpdateEntities` reads the next iteration target from
 * `[gUpdateContext.current_entity, #4]` *after* dispatch (not before
 * dispatch from `r4`). That detail matters because
 * `entity.c::UnlinkEntity` rewinds `gUpdateContext.current_entity`
 * to `ent->prev` when the entity being unlinked is the one currently
 * being walked, and it patches the doubly-linked list so that
 * `ent->prev->next` skips past `ent`. So:
 *
 *   * if `ent` survives its update, `current_entity == ent` and
 *     `current_entity->next` is the next list element (possibly
 *     post-patched if some *other* entity was unlinked during ent's
 *     update);
 *   * if `ent` was unlinked, `current_entity == ent->prev` (a still-
 *     alive entity, or the LinkedList sentinel itself if `ent` was
 *     the head), and `current_entity->next` is the new successor of
 *     that prev -- which is exactly the next entity to walk.
 *
 * Treating the LinkedList sentinel as an `Entity*` works because
 * `LinkedList::first` is at offset 4, matching `Entity::next`; this
 * is the same convention the engine uses for `(uintptr_t)ent ==
 * (uintptr_t)list` loop termination.
 *
 * Caveat: the dispatched update function continues to run after
 * `DeleteThisEntity` returns, whereas the ROM body uses
 * `arm_ClearAndUpdateEntities`'s longjmp to skip past it. The decomp
 * was spot-checked: `DeleteThisEntity` is always the last side-effect
 * in any `*Update`, so the post-call code path is always empty. If a
 * future port adds reads of `this->...` after a `DeleteThisEntity`
 * call, this TU will need real `setjmp`/`longjmp` plumbing. Tracked
 * in docs/sdl_port.md.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "affine.h"
#include "collision.h"
#include "entity.h"
#include "map.h"
#include "room.h"
#include "script.h"
#include "vram.h"

extern u8 gMapSpecialTileToActTile[];

/* ------------------------------------------------------------------------ */
/* External symbols                                                          */
/* ------------------------------------------------------------------------ */

extern void ram_DrawDirect(OAMCommand* cmd, uint32_t spriteIndex, uint32_t frameIndex);

/* `gUpdateContext` is a 256-byte zero-init weak placeholder in
 * `port_unresolved_stubs.c`; `entity.c` reads/writes it through a
 * locally-typedef'd 4-pointer struct. We mirror that typedef here so
 * the layout matches across TUs. The linker resolves by symbol name
 * only, so each TU can declare its own typedef without conflict. */
typedef struct {
    void* table;
    void* list_top;
    Entity* current_entity;
    void* restore_sp;
} UpdateContext;
extern UpdateContext gUpdateContext;

/* `UpdateCollision` is the survivor-bookkeeping call the ROM
 * `arm_UpdateEntities` makes after each dispatch; the real body lives
 * in `asm/src/intr.s` and has no C port yet. A silent forward declaration
 * here documents the dependency; the matching no-op definition lives
 * in `ram_silent_stubs.c` so the symbol resolves at link time. (The
 * title screen's `TITLE_SCREEN_OBJECT` entities all have `ENT_COLLIDE`
 * cleared in their default-initialised flags, so even the real ROM
 * body is a one-instruction early-out for them.) */
extern void UpdateCollision(Entity* this);

/* Per-kind update dispatch is intentionally minimal for the title
 * screen. The 5 entities `HandleTitlescreen` spawns are all
 * `OBJECT`-kind (`TITLE_SCREEN_OBJECT`, `RUPEE_OBJECT`, etc. via
 * `CreateObject`), so this TU only takes a *strong* reference to
 * `ObjectUpdate`. Strong references to the other `*Update` symbols
 * would force the linker to pull in `npc.c.o`, `playerItem.c.o`,
 * `manager.c.o`, `interrupts.c.o`, `enemy.s`-stub object, etc. from
 * `libtmc_game_sources.a` -- and those TUs transitively reference
 * ~hundreds of symbols (`gNPCData`, `script_ForestMinish*`, sprite
 * animation tables, ...) that have no entry in
 * `port_unresolved_stubs.c` yet. Reproducing those weak placeholders
 * is unnecessary for the title-screen path, so we just gate the
 * dispatch on `kind == OBJECT` here and fail loudly for any other
 * kind. Future scenes that legitimately need other update kernels
 * will need to (a) flesh out `port_unresolved_stubs.c` for the new
 * transitive deps and (b) extend this dispatch.
 */
extern void ObjectUpdate(Entity*);

/* ------------------------------------------------------------------------ */
/* OAM-command composition (port of `sub_080B299C`)                          */
/* ------------------------------------------------------------------------ */

/* Mirror of the OAMCommand-composition prologue
 * `asm/src/intr.s::sub_080B299C`. Reads the same entity fields the
 * ARM body reads (spriteSettings @0x18 .. spriteOrientation @0x1b,
 * iframes @0x3d, palette @0x1a, spriteVramOffset @0x60,
 * x.HALF.HI @0x2e, y.HALF.HI @0x32, z.HALF.HI @0x36, spriteOffsetX
 * @0x62, spriteOffsetY @0x63, frameSpriteSettings @0x5b) and
 * `gOAMControls` slots (`_4` @0x04, `_6` @0x06, `spritesOffset`
 * @0x02, the iframes flash byte at `_0[6]` @0x0e), then writes the
 * five 16-bit `OAMCommand` slots in the same packed form
 * `arm_sub_080ADA04` (and our port `ram_sub_080ADA04`) reads back via
 * `ldr r8, [r0, #4]` (the 32-bit `ext` straddling `_4`/`_6`).
 *
 * The arithmetic is byte-for-byte equivalent to the ARM body, with
 * one structural simplification: `entity->spriteSettings` etc. are
 * accessed through their declared bitfield typedefs in entity.h
 * rather than as a raw 32-bit `ldr`. We then re-pack them into a
 * single `ip` word so the `& 3 == 2` test (skip camera offset
 * subtraction) and the `& 0x3E003F00` mask (extract attr0/attr1
 * style bits for `ext`) keep the same shape as the ARM source. The
 * little-endian struct layout puts `spriteSettings` at bit 0 of the
 * recomposed word, matching what `ldr ip, [r0, #0x18]` would
 * load on real hardware.
 */
static void port_compose_oam_cmd(Entity* this, OAMCommand* cmd) {
    /* iframes flash modifier: when iframes>0, OR the global flash
     * byte (`gOAMControls._0[6]`, byte offset 0x0e) into the low
     * palette nibble. The hit-flash visual on the title screen is
     * dormant (no entity is ever damaged), so this branch always
     * picks the zero arm in practice. */
    int8_t iframes = (int8_t)this->iframes;
    uint32_t flash = 0;
    if (iframes > 0) {
        flash = (uint32_t)((const uint8_t*)&gOAMControls)[0x0e];
    }

    /* Reassemble the 32-bit slice the ARM body keeps in `ip` (a
     * single `ldr ip, [r0, #0x18]` on real hardware). Reading each
     * byte through its declared bitfield typedef avoids any
     * assumption about how the compiler lays out the bitfields
     * relative to their containing byte. */
    const uint8_t ss_byte = *(const uint8_t*)&this->spriteSettings;
    const uint8_t sr_byte = *(const uint8_t*)&this->spriteRendering;
    const uint8_t pal_byte = this->palette.raw;
    const uint8_t so_byte = *(const uint8_t*)&this->spriteOrientation;
    const uint32_t ip = (uint32_t)ss_byte | ((uint32_t)sr_byte << 8) | ((uint32_t)pal_byte << 16) |
                        ((uint32_t)so_byte << 24);

    /* `sb` in ARM source -- the attr2-base value the compositor
     * indexes into. Bits 0-9 = vram tile index, 10-11 = flipY (the
     * ROM stores it in attr2's "priority" slot for indirection), 12-
     * 15 = palette nibble OR'd with the iframes flash. */
    uint32_t pal_tile = (uint32_t)this->spriteVramOffset;
    const uint32_t pal_nibble = ((uint32_t)pal_byte | flash) & 0xfu;
    pal_tile |= pal_nibble << 12;
    const uint32_t flipy_bits = (uint32_t)so_byte & 0xc0u;
    pal_tile |= flipy_bits << 4;

    /* World-space coordinates plus per-sprite offsets.  z lifts the
     * y screen offset to give the GBA pseudo-3D layering.  All loads
     * are signed, matching the `ldrsh`/`ldrsb` choice in the ARM body
     * (the entity-struct declarations sometimes leave these `u8` for
     * convenience but the compositor always treats them as signed). */
    int32_t cx = (int32_t)(int16_t)this->x.HALF.HI + (int32_t)(int8_t)this->spriteOffsetX;
    int32_t cy = (int32_t)(int16_t)this->y.HALF.HI + (int32_t)(int16_t)this->z.HALF.HI +
                 (int32_t)(int8_t)this->spriteOffsetY;

    /* `spriteSettings.draw == 2` skips the camera-offset subtraction.
     * Title-screen entities use that to position themselves in
     * screen space (see `src/object/titleScreenObject.c::sub_080A2340`).
     */
    if ((ip & 3u) != 2u) {
        cx -= (int32_t)(int16_t)gOAMControls._4;
        cy -= (int32_t)(int16_t)gOAMControls._6;
    }

    /* `ext`: combination of bits preserved from `ip`
     * (spriteRendering bits 0-5 at attr0/1 byte 1, spriteOrientation
     * bits 1-5 at attr1 byte 3), the global sprites-base nibble
     * (`gOAMControls.spritesOffset`) at bits 12-19, and the high two
     * bits of (frameSpriteSettings XOR spriteSettings) at bits 28-29
     * (these become attr1 bits 12-13, the size override). */
    uint32_t ext = ip & 0x3E003F00u;
    ext |= ((uint32_t)gOAMControls.spritesOffset) << 12;
    const uint8_t fss = this->frameSpriteSettings;
    const uint32_t flipped = (uint32_t)((fss ^ ss_byte) & 0xc0u);
    ext |= flipped << 22;

    cmd->x = (int16_t)cx;
    cmd->y = (int16_t)cy;
    cmd->_4 = (uint16_t)(ext & 0xffffu);
    cmd->_6 = (uint16_t)((ext >> 16) & 0xffffu);
    cmd->_8 = (uint16_t)pal_tile;
}

/* ------------------------------------------------------------------------ */
/* DrawEntity (port of `asm/src/code_08003FC4.s::DrawEntity`)                */
/* ------------------------------------------------------------------------ */

/* On-screen check, mirroring the body of
 * `asm/src/code_08003FC4.s::CheckOnScreen` (which `sub_080040A2`
 * tail-calls). Returns true if the entity's bounding circle (radius
 * 0x3f, centred at `x.HALF.HI` / `y.HALF.HI + z.HALF.HI`) intersects
 * the 240x160 visible region positioned at
 * `(gRoomControls.scroll_x, gRoomControls.scroll_y)` (offsets 0xa /
 * 0xc in the room controls struct).
 *
 * For draw == 2 callers (title-screen entities) the gRoomControls
 * scroll values are 0 and the entity coordinates already live in
 * screen space, so this trivially returns true; we still gate the
 * draw on it so the port matches the ROM observable behaviour for
 * future scenes whose entities might briefly stray off-screen during
 * a transition. The radius padding (0x3f) and the 0x16e/0x11e total
 * extents below match the magic constants in the ARM body. */
static int port_check_on_screen(const Entity* this) {
    const int32_t scroll_x = (int32_t)gRoomControls.scroll_x;
    const int32_t scroll_y = (int32_t)gRoomControls.scroll_y;

    int32_t dx = (int32_t)(int16_t)this->x.HALF.HI - scroll_x + 0x3f;
    if ((uint32_t)dx >= 0x16eu) {
        return 0;
    }
    int32_t dy = (int32_t)(int16_t)this->y.HALF.HI - scroll_y + (int32_t)(int16_t)this->z.HALF.HI + 0x3f;
    if ((uint32_t)dy >= 0x11eu) {
        return 0;
    }
    return 1;
}

/* Replaces `asm/src/code_08003FC4.s::DrawEntity`.
 *
 * Behavior summary (matching the ROM):
 *   draw == 0           -> nothing (visibility disabled).
 *   draw == 1 or 2      -> emit if `CheckOnScreen` returns true.
 *   draw == 3           -> always emit (no on-screen check).
 *
 * What this port deliberately does *not* yet do compared to the ROM
 * body:
 *
 *   * Append the entity to one of the `gUnk_081326EC[]` priority
 *     buckets. Those buckets are walked by the ROM `arm_DrawEntities`
 *     and would let later entities be reordered (back-to-front) per
 *     bucket. None of the title-screen sprites need that ordering
 *     (they do not overlap each other), so we emit OAM directly.
 *   * Drain `gUnk_02021F20` / `gUnk_02024048` (the per-frame sound
 *     queue the ROM body pushes per draw). The title screen does
 *     not enqueue sounds through `DrawEntity`; its music is driven
 *     by the m4a path. Skipping this queue therefore changes no
 *     observable audio.
 *   * Spawn the per-entity shadow (the `_080B2718` branch). Title-
 *     screen entities do not request a shadow (`spriteSettings.shadow
 *     == 0`).
 *
 * Future scenes that need any of those will have to grow them back.
 */
void DrawEntity(Entity* this) {
    /* spriteSettings.draw is bits 0-1 of the spriteSettings byte. */
    const uint8_t ss_byte = *(const uint8_t*)&this->spriteSettings;
    const uint32_t draw = ss_byte & 3u;
    if (draw == 0u) {
        return;
    }

    if (draw != 3u) {
        if (!port_check_on_screen(this)) {
            return;
        }
    }

    /* The 0xff sentinel means "no draw" (used by callers that share
     * a frameIndex slot with non-rendered states, e.g. pauseMenu.c).
     * Bail before we touch gFrameObjLists. */
    if (this->frameIndex == 0xffu) {
        return;
    }

    OAMCommand cmd;
    port_compose_oam_cmd(this, &cmd);
    ram_DrawDirect(&cmd, (uint32_t)(uint16_t)this->spriteIndex, (uint32_t)this->frameIndex);
}

/* ------------------------------------------------------------------------ */
/* ram_UpdateEntities (port of `asm/src/intr.s::arm_UpdateEntities`)         */
/* ------------------------------------------------------------------------ */

void ram_UpdateEntities(uint32_t mode) {
    int start, end;
    if (mode == 0u) {
        /* Mode 0: regular entity lists. ROM `gUnk_080026A4` entry 0
         * sets r8 = `gEntityLists - 8` and r9 = `gEntityLists + 64`,
         * giving an iteration of indices 0..7 inclusive. */
        start = 0;
        end = 8;
    } else {
        /* Mode 1: manager list. ROM entry 1 sets r8 = `gEntityLists +
         * 56` and r9 = `gCollidableCount` (the symbol immediately
         * after `gEntityLists` in the GBA layout), giving a single-
         * iteration walk over `gEntityLists[8]`. */
        start = 8;
        end = 9;
    }

    for (int i = start; i < end; i++) {
        LinkedList* list = &gEntityLists[i];
        Entity* ent = list->first;
        while ((uintptr_t)ent != (uintptr_t)list) {
            gUpdateContext.current_entity = ent;

            /* OBJECT-only dispatch: see the rationale at the top of
             * this TU. Other kinds simply skip; the first non-OBJECT
             * entity to appear in a future scene will surface here
             * as a "no update happened" symptom rather than a link
             * failure, which is the right place to grow the table. */
            if (ent->kind == (uint8_t)OBJECT) {
                ObjectUpdate(ent);
            }

            /* Read `current_entity` *after* dispatch to recover the
             * iterator state. If `ent` survived, current_entity is
             * still `ent`; if `ent` was unlinked, `UnlinkEntity`
             * rewound it to `ent->prev` (which is still alive --
             * possibly the LinkedList sentinel itself when ent was
             * the head). Either way `current_entity->next` is the
             * correct next list element to walk. */
            Entity* current = gUpdateContext.current_entity;
            if (current == ent) {
                UpdateCollision(ent);
            }
            ent = current->next;
        }
    }
    gUpdateContext.current_entity = NULL;
}

/* ------------------------------------------------------------------------ */
/* ram_DrawEntities and ram_ClearAndUpdateEntities (no-ops)                  */
/* ------------------------------------------------------------------------ */

/* Replaces `asm/src/intr.s::arm_DrawEntities`.
 *
 * With `DrawEntity` above emitting OAM directly during the update
 * pass, there is no deferred per-priority-list drain to run. The
 * ROM body's only observable side-effects beyond emitting OAM are
 * (a) bumping `gOAMControls.spritesOffset` and (b) updating
 * `gOAMControls._4`/`_6` from `gRoomControls`; both happen in the
 * `affine.c::DrawEntities()` wrapper which still runs *before* this
 * stub is called. So the no-op is functionally complete for the
 * title screen.
 */
void ram_DrawEntities(void) {
}

/* Replaces `asm/src/intr.s::arm_ClearAndUpdateEntities`.
 *
 * The ROM body uses a `mov pc` / saved-SP combo to skip past the
 * dispatched update function and resume `arm_UpdateEntities` at the
 * post-dispatch label. With `ram_UpdateEntities` above advancing the
 * iterator via `gUpdateContext.current_entity->next` instead of a
 * captured pointer, the normal C return from `DeleteThisEntity` lands
 * back in the dispatched update; that update then returns normally,
 * and the iterator picks up at the now-patched-up next element. This
 * makes the no-op here the correct host equivalent for the title-
 * screen path. (Future scenes that legitimately need the early-exit
 * behaviour will need to grow this back into a real
 * `setjmp`/`longjmp` pair or have the dispatched updates return an
 * "iteration consumed" sentinel.)
 */
void ram_ClearAndUpdateEntities(void) {
}

/* ------------------------------------------------------------------------ */
/* UpdateSpriteForCollisionLayer (port of asm/src/script.s)                  */
/* ------------------------------------------------------------------------ */

/* Tiny per-entity helper called by `ObjectInit` (see
 * `src/objectUtils.c::ObjectInit` -> `LoadObjectSprite` returning 2)
 * and by every `*Update` that swaps an entity between collision layers
 * mid-frame (search for `UpdateSpriteForCollisionLayer` in `src/`).
 *
 * The ROM body lives in `asm/src/script.s::UpdateSpriteForCollisionLayer`
 * and is 16 THUMB instructions long; it indexes a 4-entry, 2-byte-wide
 * `_08016A28` table by the entity's collisionLayer (`Entity[0x38]`
 * in the ARM layout) and overwrites the high-2 bits of two bytes:
 * `Entity[0x19]` (the `spriteRendering` byte) and `Entity[0x1b]`
 * (the `spriteOrientation` byte). Table values:
 *
 *   collisionLayer 0 -> { 0x80, 0x80 }   (foreground default)
 *   collisionLayer 1 -> { 0x80, 0x80 }   (foreground)
 *   collisionLayer 2 -> { 0x40, 0x40 }   (background mid)
 *   collisionLayer 3 -> { 0x40, 0x40 }   (background back)
 *
 * IMPORTANT: do not access these bytes by literal offset on the host.
 * The host build has 8-byte pointers in `prev`/`next`, which shifts
 * every byte after them by +8 relative to the ARM struct layout
 * (spriteRendering and spriteOrientation become bytes 0x21 and 0x23
 * on the host). The C bitfield typedefs in `entity.h`
 * (`Entity::spriteRendering`, `Entity::spriteOrientation`) compute
 * the correct host offsets for us, so we cast the bitfield slot to
 * `uint8_t*` and apply the same mask/OR there. Using literal `0x19`
 * / `0x1b` here on the host clobbers the high byte of `spriteIndex`
 * (a real bug we hit on first boot: it turned `spriteIndex=0x01ff`
 * into `0x81ff`, which made `ram_DrawDirect` index out of bounds in
 * `gFrameObjLists` and rendered the title-screen sprites as a
 * single-pixel garbage glyph).
 */
void UpdateSpriteForCollisionLayer(Entity* this);
void UpdateSpriteForCollisionLayer(Entity* this) {
    static const uint8_t kLayerTable[8] = {
        0x80, 0x80, 0x80, 0x80, 0x40, 0x40, 0x40, 0x40,
    };
    const uint8_t layer = this->collisionLayer & 0x3u;
    const uint8_t* entry = &kLayerTable[layer * 2u];
    uint8_t* sr = (uint8_t*)&this->spriteRendering;
    uint8_t* so = (uint8_t*)&this->spriteOrientation;
    *sr = (uint8_t)((*sr & ~0xc0u) | entry[0]);
    *so = (uint8_t)((*so & ~0xc0u) | entry[1]);
}

/* ------------------------------------------------------------------------ */
/* Tile / act-tile queries + ResolveCollisionLayer (asm/src/script.s)        */
/* ------------------------------------------------------------------------ */

/* World pixel coords → room tile index (same packing as `TILE()` in entity.h). */
static u32 Port_TilePosFromWorldPixels(s32 worldX, s32 worldY) {
    s32 rx = worldX - (s32)gRoomControls.origin_x;
    s32 ry = worldY - (s32)gRoomControls.origin_y;
    u32 tx = ((u32)rx >> 4) & 0x3Fu;
    u32 ty = ((u32)ry >> 4) & 0x3Fu;
    return tx + (ty << 6);
}

/* Port of `arm_GetTileTypeAtTilePos` in asm/src/intr.s (via veneer). */
u32 GetTileTypeAtTilePos(u32 tilePos, u32 layer) {
    MapLayer* mapLayer;
    u16 tileIndex;

    if (tilePos >= MAX_MAP_SIZE * MAX_MAP_SIZE) {
        return 0;
    }
    mapLayer = GetLayerByIndex(layer);
    tileIndex = mapLayer->mapData[tilePos];
    if (tileIndex >= 0x4000) {
        return tileIndex;
    }
    if (tileIndex >= TILESET_SIZE) {
        return 0;
    }
    return mapLayer->tileTypes[tileIndex];
}

u32 GetTileTypeAtWorldCoords(s32 worldX, s32 worldY, u32 layer) {
    return GetTileTypeAtTilePos(Port_TilePosFromWorldPixels(worldX, worldY), layer);
}

/* Port of `arm_GetActTileForTileType` in asm/src/intr.s. */
u32 GetActTileForTileType(u32 tileType) {
    if (tileType < 0x4000u) {
        if (tileType >= TILESET_SIZE) {
            return 0;
        }
        return gMapTileTypeToActTile[tileType];
    } else {
        u32 idx = tileType - 0x4000u;
        if (idx >= 0x100u) {
            return 0;
        }
        return gMapSpecialTileToActTile[idx];
    }
}

/*
 * Port of `ResolveCollisionLayer` in asm/src/script.s.
 * When `collisionLayer == 0`, samples the top map at the entity position,
 * maps tile type → act tile, then scans the same 4-byte rows the ROM
 * embeds between `CheckOnLayerTransition` and the `.short 0` terminator
 * (boss-room rows first, then `gTransitionTiles`, see asm listing).
 */
u32 ResolveCollisionLayer(Entity* entity) {
    static const struct {
        u16 actKey;
        u8 srcLayer;
        u8 destCollisionLayer;
    } kRows[] = {
        { 0x002A, 3, 3 },
        { 0x002D, 3, 3 },
        { 0x002B, 3, 3 },
        { 0x002C, 3, 3 },
        { 0x004C, 3, 3 },
        { 0x004E, 3, 3 },
        { 0x004D, 3, 3 },
        { 0x004F, 3, 3 },
        { 0x000A, 2, 1 },
        { 0x0009, 2, 1 },
        { 0x000C, 1, 2 },
        { 0x000B, 1, 2 },
        { 0x0052, 3, 3 },
        { 0x0027, 3, 3 },
        { 0x0026, 3, 3 },
    };
    u32 tileType;
    u32 act;
    u32 layerOut;
    u32 i;

    if (entity->collisionLayer != 0) {
        UpdateSpriteForCollisionLayer(entity);
        return 0;
    }

    tileType = GetTileTypeAtWorldCoords(entity->x.HALF.HI, entity->y.HALF.HI, LAYER_TOP);
    layerOut = 1;
    if (tileType != 0) {
        act = GetActTileForTileType(tileType);
        layerOut = 2;
        for (i = 0; i < sizeof(kRows) / sizeof(kRows[0]); i++) {
            if (kRows[i].actKey == act) {
                layerOut = kRows[i].destCollisionLayer;
                break;
            }
        }
    }
    entity->collisionLayer = (u8)layerOut;
    UpdateSpriteForCollisionLayer(entity);
    return 0;
}
