/**
 * @file asm_stubs.c
 * @brief Trap stubs for the unported ARM-assembly functions in `asm/src/`.
 *
 * Sub-step 2b.1 of the SDL-port roadmap (see docs/sdl_port.md). The
 * decomp ships ~106 functions that are still raw THUMB / ARM assembly
 * across `code_08000E44.s`, `code_08000F10.s`, `code_08001A7C.s`,
 * `code_08003FC4.s`, `code_080043E8.s`, `code_08007CAC.s`, `enemy.s`,
 * `player.s`, `projectileUpdate.s`, and `script.s`. None of those run on
 * the host build because the SDL CMake target does not assemble the GBA
 * `.s` files. Once PR #2b starts wiring the C sources from `src/` into
 * the SDL target, those C sources reference these functions by name --
 * without a definition the link would fail with 106 undefined symbols at
 * once, which makes incremental progress impossible to bisect.
 *
 * Each stub below has the same external symbol name as its `.s`
 * counterpart and a `void(void)` signature. C has no name mangling, so
 * the linker happily resolves callers regardless of the prototype the
 * caller saw at compile time. If any stub is actually executed, it
 * `assert`s with the original function name so the unported code path
 * is loud and obvious instead of silently corrupting state.
 *
 * The boot-only assembly (`crt0.s`, `intr.s`, `stack_check.s`,
 * `veneer.s`) is intentionally excluded -- per the roadmap those files
 * have no host equivalent and the platform layer subsumes them.
 *
 * To regenerate this file, see the script in the PR that introduced it
 * (sub-step 2b.1).  Manual edits are fine; they will not be clobbered
 * unless someone re-runs the generator wholesale.
 */
#include <stdio.h>
#include <stdlib.h>

/* Suppress -Wmissing-prototypes / -Wmissing-declarations: these stubs are
 * deliberately exposed only by symbol name for the linker.  Their real
 * prototypes live in the game headers under include/. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

/* `noreturn` attribute. The stubs must never return -- callers may have
 * non-void prototypes, so silently falling through with an undefined
 * return value would corrupt state. Only GCC / Clang are supported
 * (Microsoft Windows / MSVC builds are out of scope -- see
 * docs/sdl_port.md). */
#define PORT_ASM_STUB_NORETURN __attribute__((noreturn))

/* Centralised trap: log which unported asm function was called and
 * abort.  Kept out of line so the per-symbol stubs stay tiny.  Uses
 * abort() (not assert()) so the trap survives -DNDEBUG, which is the
 * default in CMake Release / RelWithDebInfo builds. */
static PORT_ASM_STUB_NORETURN void Port_AsmStubTrap(const char* name) {
    fprintf(stderr,
            "[tmc_sdl] FATAL: unported asm function called: asm/src/%s\n"
            "         See docs/sdl_port.md, roadmap PR #2b.\n",
            name);
    fflush(stderr);
    abort();
}

/* The trap macro factors out the noreturn-tagged stub body so each stub
 * stays a single line and the file is mechanical to read. */
#define PORT_ASM_STUB(name)                  \
    PORT_ASM_STUB_NORETURN void name(void);  \
    PORT_ASM_STUB_NORETURN void name(void) { \
        Port_AsmStubTrap(#name);             \
    }

/* ---- asm/src/code_08000E44.s (5 symbols) ---- */
PORT_ASM_STUB(sub_08000E44)
/* `Random` is now a real C port in src/platform/shared/port_random.c. */
PORT_ASM_STUB(sub_08000E62)
/* `sub_08000E92` and `LoadResourceAsync` are now real C ports in
 * src/platform/shared/port_load_resource.c. */

/* ---- asm/src/code_08000F10.s (3 symbols) ---- */
PORT_ASM_STUB(CheckBits)
PORT_ASM_STUB(SumDropProbabilities)
PORT_ASM_STUB(SumDropProbabilities2)

/* ---- asm/src/code_08001A7C.s (11 symbols) ---- */
PORT_ASM_STUB(GetFuserId)
/* sub_080026C4 / sub_080026F2 are now real C ports in
 * src/platform/shared/port_text_unpacker.c. */
PORT_ASM_STUB(GetNextFunction)
PORT_ASM_STUB(LinearMoveDirectionOLD)
PORT_ASM_STUB(CalcCollisionDirectionOLD)
PORT_ASM_STUB(sub_080028E0)
PORT_ASM_STUB(GetRandomByWeight)
PORT_ASM_STUB(CheckRectOnScreen)
PORT_ASM_STUB(CheckPlayerInRegion)

/* ---- asm/src/code_08003FC4.s (29 symbols) ---- */
PORT_ASM_STUB(GravityUpdate)
PORT_ASM_STUB(sub_08003FDE)
PORT_ASM_STUB(CheckEntityPickup)
PORT_ASM_STUB(DrawEntity)
PORT_ASM_STUB(sub_080040A2)
PORT_ASM_STUB(CheckOnScreen)
PORT_ASM_STUB(sub_080040D8)
PORT_ASM_STUB(sub_080040E2)
PORT_ASM_STUB(sub_080040EC)
PORT_ASM_STUB(SnapToTile)
PORT_ASM_STUB(sub_0800417E)
PORT_ASM_STUB(sub_0800419C)
PORT_ASM_STUB(EntityInRectRadius)
PORT_ASM_STUB(sub_080041DC)
PORT_ASM_STUB(sub_080041E8)
PORT_ASM_STUB(CalcDistance)
PORT_ASM_STUB(sub_08004202)
PORT_ASM_STUB(sub_08004212)
PORT_ASM_STUB(InitializeAnimation)
PORT_ASM_STUB(GetNextFrame)
PORT_ASM_STUB(UpdateAnimationVariableFrames)
PORT_ASM_STUB(InitAnimationForceUpdate)
PORT_ASM_STUB(UpdateAnimationSingleFrame)
PORT_ASM_STUB(sub_080042BA)
PORT_ASM_STUB(sub_080042D0)
PORT_ASM_STUB(CreateDrownFx)
PORT_ASM_STUB(CreateLavaDrownFx)
PORT_ASM_STUB(CreateSwampDrownFx)
PORT_ASM_STUB(CreatePitFallFx)

/* ---- asm/src/code_080043E8.s (16 symbols) ---- */
PORT_ASM_STUB(GetTileHazardType)
PORT_ASM_STUB(sub_0800442E)
PORT_ASM_STUB(sub_0800445C)
PORT_ASM_STUB(CalcCollisionStaticEntity)
PORT_ASM_STUB(EnqueueSFX)
PORT_ASM_STUB(SoundReqClipped)
PORT_ASM_STUB(sub_080044AE)
PORT_ASM_STUB(BounceUpdate)
PORT_ASM_STUB(sub_0800451C)
PORT_ASM_STUB(sub_08004542)
PORT_ASM_STUB(ResetCollisionLayer)
PORT_ASM_STUB(sub_08004596)
PORT_ASM_STUB(sub_080045B4)
PORT_ASM_STUB(GetFacingDirection)
PORT_ASM_STUB(CalculateDirectionTo)
PORT_ASM_STUB(CalculateDirectionFromOffsets)

/* ---- asm/src/code_08007CAC.s (3 symbols) ---- */
PORT_ASM_STUB(sub_08007DCE)
PORT_ASM_STUB(FindValueForKey)
PORT_ASM_STUB(FindEntryForKey)

/* ---- asm/src/enemy.s (10 symbols) ---- */
PORT_ASM_STUB(EnemyUpdate)
PORT_ASM_STUB(sub_08001214)
PORT_ASM_STUB(GenericConfused)
PORT_ASM_STUB(sub_08001290)
PORT_ASM_STUB(EnemyFunctionHandler)
PORT_ASM_STUB(sub_080012DC)
PORT_ASM_STUB(sub_08001318)
PORT_ASM_STUB(GenericKnockback)
PORT_ASM_STUB(GenericKnockback2)
PORT_ASM_STUB(sub_0800132C)

/* ---- asm/src/player.s (20 symbols) ---- */
PORT_ASM_STUB(sub_0800857C)
PORT_ASM_STUB(sub_080085B0)
PORT_ASM_STUB(sub_080085CC)
PORT_ASM_STUB(sub_080086B4)
PORT_ASM_STUB(sub_080086D8)
PORT_ASM_STUB(DoItemTileInteraction)
PORT_ASM_STUB(DoTileInteractionOffset)
PORT_ASM_STUB(DoTileInteractionHere)
PORT_ASM_STUB(DoTileInteraction)
PORT_ASM_STUB(sub_08008926)
PORT_ASM_STUB(UpdateIcePlayerVelocity)
PORT_ASM_STUB(sub_08008942)
PORT_ASM_STUB(sub_08008A1A)
PORT_ASM_STUB(AddPlayerVelocity)
PORT_ASM_STUB(ClampPlayerVelocity)
PORT_ASM_STUB(sub_08008AA0)
PORT_ASM_STUB(sub_08008AC6)
PORT_ASM_STUB(GetNonCollidedSide)
PORT_ASM_STUB(CheckNEastTile)
PORT_ASM_STUB(PlayerCheckNEastTile)

/* ---- asm/src/projectileUpdate.s (1 symbol) ---- */
PORT_ASM_STUB(ProjectileUpdate)

/* ---- asm/src/script.s (8 symbols) ---- */
PORT_ASM_STUB(GetNextScriptCommandHalfword)
PORT_ASM_STUB(GetNextScriptCommandHalfwordAfterCommandMetadata)
PORT_ASM_STUB(GetNextScriptCommandWord)
PORT_ASM_STUB(GetNextScriptCommandWordAfterCommandMetadata)
PORT_ASM_STUB(UpdateSpriteForCollisionLayer)
PORT_ASM_STUB(ResolveCollisionLayer)
PORT_ASM_STUB(CheckOnLayerTransition)
PORT_ASM_STUB(UpdateCollisionLayer)

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

/**
 * Force-reference table so the linker keeps every stub in the final
 * binary even when nothing else in the SDL build calls them yet.  Once
 * PR #2b wires `src/` in, the real callers will pull these symbols in
 * directly and this table becomes redundant (but harmless).
 */
typedef void (*PortAsmStubFn)(void);
static const PortAsmStubFn sPortAsmStubTable[] = {
    AddPlayerVelocity,
    BounceUpdate,
    CalcCollisionDirectionOLD,
    CalcCollisionStaticEntity,
    CalcDistance,
    CalculateDirectionFromOffsets,
    CalculateDirectionTo,
    CheckBits,
    CheckEntityPickup,
    CheckNEastTile,
    CheckOnLayerTransition,
    CheckOnScreen,
    CheckPlayerInRegion,
    CheckRectOnScreen,
    ClampPlayerVelocity,
    CreateDrownFx,
    CreateLavaDrownFx,
    CreatePitFallFx,
    CreateSwampDrownFx,
    DoItemTileInteraction,
    DoTileInteraction,
    DoTileInteractionHere,
    DoTileInteractionOffset,
    DrawEntity,
    EnemyFunctionHandler,
    EnemyUpdate,
    EnqueueSFX,
    EntityInRectRadius,
    FindEntryForKey,
    FindValueForKey,
    GenericConfused,
    GenericKnockback,
    GenericKnockback2,
    GetFacingDirection,
    GetFuserId,
    GetNextFrame,
    GetNextFunction,
    GetNextScriptCommandHalfword,
    GetNextScriptCommandHalfwordAfterCommandMetadata,
    GetNextScriptCommandWord,
    GetNextScriptCommandWordAfterCommandMetadata,
    GetNonCollidedSide,
    GetRandomByWeight,
    GetTileHazardType,
    GravityUpdate,
    InitAnimationForceUpdate,
    InitializeAnimation,
    LinearMoveDirectionOLD,
    PlayerCheckNEastTile,
    ProjectileUpdate,
    /* `Random` is a real C port now; no force-reference needed. */
    ResetCollisionLayer,
    ResolveCollisionLayer,
    SnapToTile,
    SoundReqClipped,
    SumDropProbabilities,
    SumDropProbabilities2,
    UpdateAnimationSingleFrame,
    UpdateAnimationVariableFrames,
    UpdateCollisionLayer,
    UpdateIcePlayerVelocity,
    UpdateSpriteForCollisionLayer,
    sub_08000E44,
    sub_08000E62,
    sub_08001214,
    sub_08001290,
    sub_080012DC,
    sub_08001318,
    sub_0800132C,
    sub_080028E0,
    sub_08003FDE,
    sub_080040A2,
    sub_080040D8,
    sub_080040E2,
    sub_080040EC,
    sub_0800417E,
    sub_0800419C,
    sub_080041DC,
    sub_080041E8,
    sub_08004202,
    sub_08004212,
    sub_080042BA,
    sub_080042D0,
    sub_0800442E,
    sub_0800445C,
    sub_080044AE,
    sub_0800451C,
    sub_08004542,
    sub_08004596,
    sub_080045B4,
    sub_08007DCE,
    sub_0800857C,
    sub_080085B0,
    sub_080085CC,
    sub_080086B4,
    sub_080086D8,
    sub_08008926,
    sub_08008942,
    sub_08008A1A,
    sub_08008AA0,
    sub_08008AC6,
};

/* Public entry point so the table cannot be dead-stripped at static-link
 * time.  Returns the number of stubs registered; never called from the
 * frame loop. */
size_t Port_AsmStubCount(void);
size_t Port_AsmStubCount(void) {
    return sizeof(sPortAsmStubTable) / sizeof(sPortAsmStubTable[0]);
}
