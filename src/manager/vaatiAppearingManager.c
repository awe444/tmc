/**
 * @file vaatiAppearingManager.c
 * @ingroup Managers
 *
 * @brief Handles the appearing and disappearing effect for Vaati.
 */
#include "manager/vaatiAppearingManager.h"
#include "area.h"
#include "common.h"
#include "game.h"
#include "main.h"
#include "room.h"
#include "screen.h"
#include "physics.h"

void sub_0805D9D8(VaatiAppearingManager*);
void VaatiAppearingManager_Action3(VaatiAppearingManager*);
void VaatiAppearingManager_Action2(VaatiAppearingManager*);
void VaatiAppearingManager_Action1(VaatiAppearingManager*);
void VaatiAppearingManager_Init(VaatiAppearingManager*);
void sub_0805DA08(u32, u32, u32);

static const u8 gUnk_08108D74[] = { 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x0 };

extern struct BgAffineDstData gUnk_02017AA0[];

void VaatiAppearingManager_Main(VaatiAppearingManager* this) {
    static void (*const VaatiAppearingManager_Actions[])(VaatiAppearingManager*) = {
        VaatiAppearingManager_Init,
        VaatiAppearingManager_Action1,
        VaatiAppearingManager_Action2,
        VaatiAppearingManager_Action3,
    };
    VaatiAppearingManager_Actions[super->action](this);
    this->field_0x24 = gRoomTransition.frameCount << 4;
    sub_0805D9D8(this);
    sub_0805DA08(this->field_0x20, this->field_0x28, this->field_0x24);
}

void VaatiAppearingManager_Init(VaatiAppearingManager* this) {
    u32 index;

    super->action = (super->type == 0) ? 1 : 2;
    this->field_0x20 = 0;
    this->field_0x24 = 0;
    switch (gRoomControls.area) {
        case 7:
        default:
            index = 0;
            break;
        case 3:
            index = 1;
            break;
        case 0x81:
            index = 2;
            break;
        case 0x80:
            index = 3;
            break;
        case 0x88:
            index = 4;
            break;
        case 0x78:
            index = 5;
            break;
        case 0x38:
            index = 6;
            break;
    }

    LoadGfxGroup(gUnk_08108D74[index]);
    gScreen.bg3.control = 0x1e04;
}

void VaatiAppearingManager_Action1(VaatiAppearingManager* this) {
    switch (super->subAction) {
        case 0:
            super->subAction = 1;
            this->field_0x20 = 0x80;
            gScreen.lcd.displayControl |= DISPCNT_BG3_ON;
            break;
        case 1:
            if (--this->field_0x20 == 0x10) {
                super->subAction = 2;
                super->timer = 45;
            }
            break;
        case 2:
            if (--super->timer == 0) {
                super->subAction = 3;
            }
            break;
        case 3:
            if (--this->field_0x20 == 0) {
                super->subAction = 4;
                super->timer = 60;
            }
            break;
        default:
            if (--super->timer == 0) {
                sub_0801E104();
                gScreen.lcd.displayControl &= ~DISPCNT_BG3_ON;
                DeleteThisEntity();
            }
    }
}

void VaatiAppearingManager_Action2(VaatiAppearingManager* this) {
    switch (super->subAction) {
        case 0:
            super->subAction = 1;
            super->timer = 45;
            this->field_0x20 = 1;
            gScreen.lcd.displayControl |= DISPCNT_BG3_ON;
            break;
        case 1:
            if (--super->timer == 0) {
                super->subAction = 2;
                super->timer = 20;
            }
            break;
        case 2:
            this->field_0x20++;
            if (--super->timer != 0) {
                return;
            }
            super->subAction = 3;
            break;
        default:
            this->field_0x20 += 4;
            if (this->field_0x20 > 0x80) {
                sub_0801E104();
                gScreen.lcd.displayControl &= ~DISPCNT_BG3_ON;
                DeleteThisEntity();
            }
            break;
    }
}

void VaatiAppearingManager_Action3(VaatiAppearingManager* this) {
    if ((gInput.heldKeys & DPAD_UP) != 0) {
        this->field_0x20--;
    }
    if ((gInput.heldKeys & DPAD_DOWN) != 0) {
        this->field_0x20++;
    }
    if ((gInput.heldKeys & DPAD_LEFT) != 0) {
        this->field_0x28--;
    }
    if ((gInput.heldKeys & DPAD_RIGHT) != 0) {
        this->field_0x28++;
    }
}

void sub_0805D9D8(VaatiAppearingManager* this) {
    Entity* vaati = super->parent;
    if (vaati != NULL) {
        gScreen.bg3.xOffset = 0x80 - (vaati->x.HALF.HI - gRoomControls.scroll_x);
        gScreen.bg3.yOffset = 0x8c - (vaati->y.HALF.HI - gRoomControls.scroll_y);
    }
}

void sub_0805DA08(u32 x, u32 y, u32 param_3) {
    u32 i;
    struct BgAffineDstData* affineDstData = &gUnk_02017AA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_AFFINE];
    for (i = 0; i < VIEWPORT_HEIGHT; ++i, y += 0x17) {
        affineDstData->pa = ((gSineTable[(param_3 + i + y) & 0xff] * x) >> 8) + gScreen.bg3.xOffset;
        affineDstData = (struct BgAffineDstData*)&affineDstData->pb;
    }
    SetVBlankDMA((u16*)&gUnk_02017AA0[gUnk_03003DE4[0] * VIEWPORT_HDMA_HALF_AFFINE], (u16*)REG_ADDR_BG3HOFS,
                 ((DMA_ENABLE | DMA_START_HBLANK | DMA_16BIT | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_RELOAD) << 16) +
                     0x1);
}

void CreateVaatiApparateManager(VaatiAppearingManager* this, u32 type) {
    Manager* manager = GetEmptyManager();
    if (manager != NULL) {
        manager->kind = MANAGER;
        manager->id = VAATI_APPARATE_MANAGER;
        manager->type = type;
        manager->parent = (Entity*)super;
        AppendEntityToList((Entity*)manager, 8);
    }
    if (gArea.onEnter != NULL) {
        gScreen.lcd.displayControl &= ~DISPCNT_BG3_ON;
        RoomExitCallback();
#ifndef PC_PORT
        //! @bug: this always variable points to ROM, not a Manager*
        DeleteManager((Manager*)gArea.onEnter);
#else
        /* B43/#93 — the DeleteManager call is dropped, not translated.
         *
         * gArea.onEnter is a ROM *function* pointer, so on hardware
         * DeleteManager unlinks ROM: it reads two words of code as
         * prev/next, writes through them into ROM, and the writes are
         * discarded. The decomp flags it above. Its only observable
         * effects are the two lines kept here.
         *
         * A previous port made both the guard and the argument
         * gArea.transitionManager, to stop the raw onEnter deref
         * SIGSEGVing on x86-64. That deletes a *real* entity, and during
         * the castle takeover it is a stale one: Subtask_FadeIn has
         * swapped the nine list heads into gEntityListsBackup, but the
         * pre-subtask entities keep their prev/next, and a list sentinel
         * is the same object on both sides of the swap. Hyrule Field's
         * transition handler sits at the head of list 6, so
         * UnlinkEntity's `ent->prev->next = ent->next` writes
         * gEntityLists[6].first back to the overworld chain — orphaning
         * the takeover orchestrator, which is never deleted and never
         * updated again. Its sync-flag broadcasts stop, the helper NPCs
         * spin forever, and the cutscene cannot end.
         *
         * Reproducing the hardware no-op fixes both that and the deref. */
#endif
    }
}
