/*
 * ARENASEL.C (0x8004E480-0x8004E7B0): an arena viewer - the 3D stage with a free camera on
 * the first controller (d-pad, C buttons and the stick move, turn and zoom it), A or Start to
 * leave. Nothing in the game calls ArenaSel_Run any more (VS mode picks its arena in VS.C).
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"

typedef struct ArenaSelScreen {            /* 0x3614 bytes */
    /* 0x0000 */ u8 stage[0x35D0];
    /* 0x35D0 */ f32 cam[16];
    /* 0x3610 */ s32 done;
} ArenaSelScreen;

extern ArenaSelScreen *D_800ACD50;
#define gArenaSel D_800ACD50

void ArenaSel_Init(void);
void ArenaSel_Shutdown(void);
s32  ArenaSel_Draw(void);
void ArenaSel_Frame(void);

/* 0x8004E480 */
void ArenaSel_Init(void) {
    if (gArenaSel != NULL) return;
    gArenaSel = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(ArenaSelScreen), 0);
    bzero(gArenaSel, sizeof(ArenaSelScreen));
    func_8002EA90(gArenaSel);
    func_80087EE8(gArenaSel->cam, 0.0f, 0.0f, 800.0f);
}

/* 0x8004E4F0 */
void ArenaSel_Shutdown(void) {
    if (gArenaSel == NULL) return;
    func_8002ECBC(gArenaSel);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gArenaSel);
    gArenaSel = NULL;
}

/* 0x8004E538: the logic. Returns 1 always (the return value is left over from the button
 * test's delay slot); A or Start sets `done`. */
s32 ArenaSel_Draw(void) {
    if (gArenaSel == NULL) return 0;
    if (B_800F88E4 & 0x800) func_80088350(gArenaSel->cam, 0.0f, 0.0f, -5.0f);
    if (B_800F88E4 & 0x400) func_80088350(gArenaSel->cam, 0.0f, 0.0f, 5.0f);
    if (B_800F88E4 & 0x200) func_80088350(gArenaSel->cam, -5.0f, 0.0f, 0.0f);
    if (B_800F88E4 & 0x100) func_80088350(gArenaSel->cam, 5.0f, 0.0f, 0.0f);
    if (B_800F88E4 & 0x4000) func_80088350(gArenaSel->cam, 0.0f, 5.0f, 0.0f);
    if (B_800F88E4 & 0x8000) func_80088350(gArenaSel->cam, 0.0f, -5.0f, 0.0f);
    if ((u32)(B_800F7E50 + 10) >= 21) func_800883F0(gArenaSel->cam, -(B_800F7E50 * 5));
    if ((u32)(B_800F7784 + 10) >= 21) func_800883A0(gArenaSel->cam, -(B_800F7784 * 5));
    if (B_800F88E4 & 0x20) func_80088440(gArenaSel->cam, 300);
    if (B_800F88E4 & 0x2000) func_80088440(gArenaSel->cam, -300);
    func_8002ED34(gArenaSel);
    if (B_800F8C78 & 0x9000) gArenaSel->done = 1;
    return 1;
}

/* 0x8004E740: the picture */
void ArenaSel_Frame(void) {
    if (gArenaSel == NULL) return;
    func_8007E860(&gDL);
    func_8007E9DC(&gDL);
    func_8007EEF0(&gDL);
    func_8002FD50(gArenaSel, gArenaSel->cam);
    func_8002FE34(gArenaSel, gArenaSel->cam);
}
