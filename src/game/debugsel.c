/*
 * The debug fight select (0x80031460-0x80031FE0): Game_Init runs it after the attract loop
 * (only in a debug set-up). It shows the two fighters (as flat sprites) over the arena with the
 * build's version, date and time; pad 1 and pad 2 (C up/right, C down/left) step through the
 * characters, pad 1's d-pad left/right through the arenas, Start fades to white and leaves.
 */
#include "common.h"
#include "mpfplat.h"
#include "combat.h"

typedef struct DebugSel {                  /* 0x6A7C bytes */
    /* 0x0000 */ s32 tick;
    /* 0x0004 */ s32 loaded[2];            /* the characters loaded (-1 none) */
    /* 0x000C */ s32 chr[2];               /* the characters chosen */
    /* 0x0014 */ CharObj *obj[2];
    /* 0x001C */ cfCharacter f1;
    /* 0x34FC */ cfCharacter f2;
    /* 0x69DC */ u8 pad[0xA0];
} DebugSel;

extern DebugSel *B_800DC9D0;
#define gDebugSel B_800DC9D0
extern s32 B_800DC9DC;                     /* the arena loaded */
extern u8 B_800DC9E0[];                    /* the camera */
extern f32 B_800DCA20[16];
extern f32 B_800DCA60[3], B_800DCA6C[3];   /* the camera's eye and target */
extern s8 B_800F2D28[26];                  /* the arenas that exist */
extern char D_800AA0A0[];                  /* the version ("1.0") */
extern s32 B_800F2D18, B_800F8C88, B_800F4E54, B_800F5BE4, B_800F8C84, B_800F15E8;
extern s32 B_800F4E6C, B_800F4E60, B_800F77D8, B_800F7F50, B_800F8C78, B_800F15DC;

void func_8007FF20(s32 fovy, f32 nearZ, f32 farZ);
void func_80078040(const char *s, s32 x, s32 y, s32 colour);    /* debug text */
void func_80080590(u32 **dl, void *image, s32 *rect, s32 x, s32 y, s32 flags);
void func_80031EEC(cfCharacter *c, CharObj *obj, s32 x, s32 y);
void func_80031FC8(void);
s32  func_80031460(s32 *char1, s32 *char2, s32 *arena);

static void NextChar(s32 *c) {
    do {
        if (++*c >= 17) *c = 0;
    } while (!gCharacterAvailable[*c]);
}

static void PrevChar(s32 *c) {
    do {
        if (--*c < 0) *c = 16;
    } while (!gCharacterAvailable[*c]);
}

static s32 FadeOver(void) {
    ScreenFade *f = &gScreenFade;
    s32 done = (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
    return done || f->active == 0;
}

static void LoadFighter(s32 k) {
    DebugSel *d = gDebugSel;
    cfCharacter *c = (k == 0) ? &d->f1 : &d->f2;

    if (d->obj[k] != NULL) FreeCharacter(d->obj[k]);
    gDebugSel->obj[k] = LoadCharacter(gDebugSel->chr[k], k);
    cfCharacter_Init(c, (CharDef *)gDebugSel->obj[k]->moves);
    cfCharacter_PlaySequence(c, 0, -1, 1, 0xFFFFFFFF);    /* standing */
    gDebugSel->loaded[k] = gDebugSel->chr[k];
}

/* 0x80031460: DebugSel_Run - returns 1 (always) with the characters and the arena chosen */
s32 func_80031460(s32 *char1, s32 *char2, s32 *arena) {
    char buf[256];
    DebugSel *d;
    s32 leaving = 0;
    s32 k;

    gDebugSel = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(DebugSel), 0);
    mpMemSet(gDebugSel, sizeof(DebugSel), 0);
    cfCharacter_Construct(&gDebugSel->f1);
    cfCharacter_Construct(&gDebugSel->f2);
    gDebugSel->chr[0] = *char1;
    if (*char1 < 0 || *char1 >= 17) gDebugSel->chr[0] = 0;
    gDebugSel->chr[1] = *char2;
    if (*char2 < 0 || *char2 >= 17) gDebugSel->chr[1] = 0;
    d = gDebugSel;
    while (!gCharacterAvailable[d->chr[0]]) {
        if (++d->chr[0] >= 17) d->chr[0] = 0;
        d = gDebugSel;
    }
    d = gDebugSel;
    while (!gCharacterAvailable[d->chr[1]]) {
        if (++d->chr[1] >= 17) d->chr[1] = 0;
        d = gDebugSel;
    }
    B_800DC9DC = -1;
    func_8007FF20(0x21, 60.0f, 15000.0f);
    while (!B_800F2D28[*arena]) {
        if (++*arena >= 26) *arena = 0;
    }
    func_8002A340(&gScreenFade, 3);
    if (B_800F77D8 & 0x20) func_8002A340(&gScreenFade, 4);
    if (B_800F77D8 & 0x10) func_8002A340(&gScreenFade, 5);
    gScreenFade.rgb[0] = 0;
    gScreenFade.rgb[1] = 0;
    gScreenFade.rgb[2] = 0;
    gDebugSel->loaded[0] = -1;
    gDebugSel->loaded[1] = -1;
    for (;;) {
        if (*arena != B_800DC9DC) {
            func_8007CCA4();
            func_8000B704();
            func_8000AF40(*arena);
            func_8007FF20(0x21, 60.0f, 15000.0f);
            B_800DC9DC = *arena;
        }
        func_80076DB8();                    /* the controllers (new presses added in) */
        B_800F2D18 |= B_800F8C88;
        B_800F4E54 |= B_800F5BE4;
        B_800F8C84 |= B_800F15E8;
        B_800F4E6C |= B_800F4E60;
        B_800F77D8 |= B_800F7F50;
        B_800F8C78 |= B_800F15DC;
        if (!leaving) {
            if (B_800F8C78 & 0x1000) {      /* Start: fade to white */
                leaving = 1;
                func_8002A990(&gScreenFade, 1);
                gScreenFade.rgb[0] = 255;
                gScreenFade.rgb[1] = 255;
                gScreenFade.rgb[2] = 255;
            }
            if (B_800F4E54 & 0x500) NextChar(&gDebugSel->chr[0]);
            if (B_800F4E54 & 0xA00) PrevChar(&gDebugSel->chr[0]);
            if (B_800F4E6C & 0x500) NextChar(&gDebugSel->chr[1]);
            if (B_800F4E6C & 0xA00) PrevChar(&gDebugSel->chr[1]);
            if (B_800F8C78 & 8) {
                do {
                    if (--*arena < 0) *arena = 25;
                } while (!B_800F2D28[*arena]);
            }
            if (B_800F8C78 & 4) {
                do {
                    if (++*arena >= 26) *arena = 0;
                } while (!B_800F2D28[*arena]);
            }
        }
        for (k = 0; k < 2; k++)
            if (gDebugSel->chr[k] != gDebugSel->loaded[k]) LoadFighter(k);
        if (gDebugSel->tick & 1) {          /* the fighters at half speed */
            cfCharacter_Update(&gDebugSel->f1);
            cfCharacter_Update(&gDebugSel->f2);
        }
        mpSPrintf(buf, "Version %s", D_800AA0A0);
        func_80078040(buf, 32, 32, 0xFFFF);
        mpSPrintf(buf, "Date: %s", "Jan  0 1900");
        func_80078040(buf, 32, 38, 0xFFFF);
        mpSPrintf(buf, "Time: %s", "15:09:14");
        func_80078040(buf, 32, 44, 0xFFFF);
        func_8007C850();
        func_8007E860(&gDL);
        func_8007E720(&gDL, 0, 0, 0);
        func_8007EEF0(&gDL);
        B_800DCA60[0] = 0.0f;
        B_800DCA60[1] = 4024.0f;
        B_800DCA60[2] = 4024.0f;
        B_800DCA6C[0] = 0.0f;
        B_800DCA6C[1] = 0.0f;
        B_800DCA6C[2] = 0.0f;
        func_80088654(B_800DC9E0, B_800DCA60, B_800DCA6C);
        func_80087E70(B_800DCA20, B_800DC9E0);
        func_8008861C(B_800DCA20);
        func_8008004C(&gDL, B_800DCA20);
        func_8000B894(NULL);                /* the arena */
        func_8000B998(B_800DC9E0, NULL);
        func_8000BCA8(B_800DC9E0, NULL);
        func_8000BE80(B_800DC9E0, NULL);
        func_8000C07C(B_800DC9E0, NULL);
        func_8007EB48(&gDL);
        func_80031EEC(&gDebugSel->f1, gDebugSel->obj[0], 140, 260);
        func_80031EEC(&gDebugSel->f2, gDebugSel->obj[1], 500, 260);
        func_80031FC8();
        func_8002AA14(&gScreenFade, 0.06f);
        func_8002AC80(&gScreenFade, &gDL);
        Game_EndFrame();
        if (leaving && FadeOver()) break;
        if (FadeOver()) func_8002AC24(&gScreenFade);
        CharFrameCache_Update();
        gDebugSel->tick++;
    }
    func_8007CCA4();
    cfCharacter_Destruct(&gDebugSel->f1);
    cfCharacter_Destruct(&gDebugSel->f2);
    if (gDebugSel->obj[0] != NULL) FreeCharacter(gDebugSel->obj[0]);
    if (gDebugSel->obj[1] != NULL) FreeCharacter(gDebugSel->obj[1]);
    CharFrameCache_FreeAll();
    func_8000B704();
    *char1 = gDebugSel->chr[0];
    *char2 = gDebugSel->chr[1];
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gDebugSel);
    return 1;
}

/* 0x80031EEC: DebugSel_DrawFighter - a fighter's sprites, flat and at half size */
void func_80031EEC(cfCharacter *c, CharObj *obj, s32 x, s32 y) {
    SeqLayer *l;
    s32 i, lx, ly;

    for (i = 0; i < c->seq.numLayers; i++) {
        l = &c->seq.layers[i];
        lx = (l->x + x) / 2;
        ly = (l->y + y) / 2;
        func_80080590(&gDL, LoadCharacterFrame(obj, (s16)l->frame), NULL, lx, ly, l->flags);
    }
}

/* 0x80031FC8: an empty function */
void func_80031FC8(void) {
}
