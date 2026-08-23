#include "global.h"
#include "entity.h"
#include "room.h"
#include "npc.h"
#include "area.h"
#include "flags.h"

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

extern void InitNPC(Entity*);

void NPCUpdate(Entity* this) {
    u32 health = this->health;
    u32 temp;
    if ((health & 0x7f) && !ReadBit(&gUnk_020342F8, health - 1))
        DeleteThisEntity();
    if (this->action == 0 && (this->flags & ENT_DID_INIT) == 0)
        NPCInit(this);
    if (!EntityDisabled(this))
        gNPCFunctions[this->id][0](this);
    if (this->next != NULL) {
        if (gNPCFunctions[this->id][1] != NULL)
            gNPCFunctions[this->id][1](this);

        if (this->health % 0x80) { // If this NPC was created by DelayedEntityLoadManager_Main, we need to update the
                                   // location in gNPCData.
            NPCStruct* npc = gNPCData;
            npc += (this->health - 1);
            npc->x = this->x.HALF.HI - gRoomControls.origin_x;
            npc->y = this->y.HALF.HI - gRoomControls.origin_y;
        }
        DrawEntity(this);
    }
}
