/**
 * @file pinwheel.c
 * @ingroup Objects
 *
 * @brief Pinwheel object
 */
#include "entity.h"
#include "flags.h"
#include "object.h"
#include "area.h"
#include "effects.h"

typedef struct {
    /*0x00*/ Entity base;
    /*0x68*/ u16 flag;
} PinwheelEntity;

static const u16 gPinwheelFlags[] = {
    KUMOUE_02_AWASE_01, KUMOUE_02_AWASE_02, KUMOUE_02_AWASE_03, KUMOUE_02_AWASE_04, KUMOUE_02_AWASE_05, BEGIN_1,
};
#ifdef PC_PORT
/* On GBA this is gArea.filler6: gArea is at 0x02033A90 and filler6 sits at
 * offset 0x868, i.e. exactly 0x020342F8. The decomp spells the same bytes two
 * ways — delayedEntityLoadManager/npc/pinwheel by address, whirlwind/
 * cutsceneMiscObject/physics as gArea.filler6 — which costs nothing on
 * hardware and split the bitfield into two objects here. Alias it, the way
 * gUnk_02035542 is aliased to gzHeap+2 in common.c. */
#define gUnk_020342F8 (*(u32*)gArea.filler6)
#else
extern u32 gUnk_020342F8;
#endif

void Pinwheel_Init(PinwheelEntity* this);
void Pinwheel_Action1(PinwheelEntity* this);
void Pinwheel_Action2(PinwheelEntity* this);

void Pinwheel(PinwheelEntity* this) {
    static void (*const Pinwheel_Actions[])(PinwheelEntity*) = {
        Pinwheel_Init,
        Pinwheel_Action1,
        Pinwheel_Action2,
    };
    u16 x = super->health;
    if ((x & 0x7f) != 0) {
        if (ReadBit(&gUnk_020342F8, x - 1) == 0) {
            DeleteThisEntity();
        }
    }
    Pinwheel_Actions[super->action](this);
}

void Pinwheel_Init(PinwheelEntity* this) {
    this->flag = gPinwheelFlags[super->type2];
    super->spritePriority.b0 = 7;
    if (CheckLocalFlag(this->flag) != 0) {
        super->action = 2;
    } else {
        super->action = 1;
    }
    InitializeAnimation(super, 0);
}

void Pinwheel_Action1(PinwheelEntity* this) {
    if (CheckLocalFlag(this->flag) != 0) {
        super->action = 2;
        CreateDeathFx(super);
    }
}
void Pinwheel_Action2(PinwheelEntity* this) {
    GetNextFrame(super);
}
