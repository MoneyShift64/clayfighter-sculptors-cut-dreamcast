/*
 * CHARSEL.C (0x80051D00-0x800569D0): character select - 14 portraits in frames over a
 * scrolling background, one or two players' cursors (pulsing red / yellow), the chosen
 * fighter's name, a burst of light when a fighter is picked, the two "hidden" slots (10 and
 * 13) that cycle through the unlocked secret fighters with R, and a random pick (L+R, or the
 * "?" slots) that rolls the cursor round for half a second.
 *
 * CharSel_Draw is the logic (Game_Render calls it every tick); CharSel_Frame builds the
 * picture once a frame (CharSel_Run's loop).
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"

typedef struct CharSelScreen {             /* 0x1D8 bytes; [2] = player 1, player 2 */
    /* 0x000 */ u8 *frameFile, *frameTop, *frameBack;
    /* 0x00C */ u8 *randomFile, *randomImage;     /* the "?" picture */
    /* 0x014 */ u8 *portraitFile[17];
    /* 0x058 */ u8 *portrait[17];
    /* 0x09C */ u8 *background;
    /* 0x0A0 */ u8 *fontFile, *font;
    /* 0x0A8 */ s32 *charOut[4];           /* player 1's fighter and costume, player 2's */
    /* 0x0B8 */ f32 shine[14];             /* 0 -> 1: a portrait lighting up (1 = lit) */
    /* 0x0F0 */ s32 wobble[14];            /* its wobble angle while lighting up */
    /* 0x128 */ s32 bounce[14];            /* a pick's bounce angle (0 = none) */
    /* 0x160 */ s32 cursor[2];
    /* 0x168 */ s32 chosen[2];
    /* 0x170 */ s32 active[2];             /* the player has pressed Start */
    /* 0x178 */ s32 repeat[2];             /* ticks before the same name is announced again */
    /* 0x180 */ s32 last[2];               /* the last fighter announced */
    /* 0x188 */ s32 random[2];             /* ticks of the random roll left */
    /* 0x190 */ s32 pulse[2];              /* the cursor's pulse angle ... */
    /* 0x198 */ f32 pulseSin[2];           /* ... and its sine */
    /* 0x1A0 */ f32 picked[2];             /* 0 -> 1 after picking */
    /* 0x1A8 */ u8 colour[2][3];           /* each cursor's colour this tick */
    /* 0x1AE */ u8 pad1AE[2];
    /* 0x1B0 */ s32 scrollX, scrollY;      /* the background */
    /* 0x1B8 */ s32 locked[2];             /* the fighter was given (can't change it) */
    /* 0x1C0 */ s32 canQuit;               /* B leaves */
    /* 0x1C4 */ s32 leaveTimer;            /* -1, or ticks before the fade out */
    /* 0x1C8 */ s32 done;
    /* 0x1CC */ s32 quit;
    /* 0x1D0 */ s32 players;               /* 1, 2 or 3 (both) */
    /* 0x1D4 */ s32 result;                /* -1 while running */
} CharSelScreen;

_Static_assert(sizeof(CharSelScreen) == 0x1D8, "CharSelScreen");
_Static_assert(__builtin_offsetof(CharSelScreen, shine) == 0xB8, "shine");
_Static_assert(__builtin_offsetof(CharSelScreen, cursor) == 0x160, "cursor");
_Static_assert(__builtin_offsetof(CharSelScreen, scrollX) == 0x1B0, "scrollX");

extern CharSelScreen *D_800AD9FC;
#define gCharSel D_800AD9FC
extern s32 D_800AD9C0[14];                 /* each slot's fighter ([10] and [13]: the hidden ones; <0 = random) */
extern s32 D_800AD9F8;                     /* how many secret fighters are unlocked ... */
extern s32 B_800E1268[];                   /* ... and which */
extern s32 B_800E1260, B_800E1264;         /* where slots 10 and 13 are in that list */
extern s32 D_800AD8E0[14], D_800AD918[14], D_800AD950[14], D_800AD988[14];   /* up, down, left, right */
extern s32 D_800AD870[14], D_800AD8A8[14]; /* each slot's position */
extern char *D_800AA72C[];                 /* fighters' names */
extern s32 D_800AA0BC, D_800AA0E0, D_800AA104, D_800AA128;   /* secret fighters unlocked */
extern s32 B_800F2D18, B_800F8C84;         /* buttons held, players 1 and 2 */
extern void *B_800F7E6C;
void RomCopy(void *dst, u32 src, u32 len);
void func_80081924(u32 **dl, u8 *image, s32 a2);

void CharSel_Init(s32 *char1, s32 *costume1, s32 *char2, s32 *costume2, s32 flags);
void CharSel_Shutdown(void);
s32  CharSel_Draw(void);
s32  CharSel_Frame(void);
void CharSel_DrawSlot(s32 slot);

static s32 *CharOut(s32 p) { return gCharSel->charOut[p * 2]; }
static s32 *CostumeOut(s32 p) { return gCharSel->charOut[p * 2 + 1]; }

static void SetTexFilter(u32 mode) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xBA000C02;
    g[1] = mode;
}

static void Gfx2(u32 w0, u32 w1) {
    u32 *d = gDL;
    gDL = d + 2;
    d[0] = w0;
    d[1] = w1;
}

static void Sound(s32 sound, s32 volume, s32 a8) {
    func_800797EC(B_800F7E6C, 0, 0, sound, volume, 0, 0, 1.0f, a8);
}

static s32 FadeDone(void) {
    ScreenFade *f = &gScreenFade;
    return (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
}

/* the index of a fighter in a list, or n if it isn't there */
static s32 Find(s32 *list, s32 n, s32 v) {
    s32 i;
    for (i = 0; i < n; i++)
        if (list[i] == v) break;
    return i;
}

/* 0x80051D00: flags: 1, 2 = players 1, 2 playing; 4, 8 = their fighters are fixed; 0x10 = B
 * leaves; 0x20 = the other background */
void CharSel_Init(s32 *char1, s32 *costume1, s32 *char2, s32 *costume2, s32 flags) {
    CharSelScreen *cs;
    s32 i, src, len;

    if (gCharSel != NULL) return;
    gCharSel = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(CharSelScreen), 0);
    bzero(gCharSel, sizeof(CharSelScreen));
    D_800AD9C0[10] = -1;
    D_800AD9C0[13] = -2;
    B_800E1260 = 0;
    B_800E1264 = 0;
    cs = gCharSel;
    cs->frameFile = func_80080360(0xF9BB20);
    cs->frameTop = cs->frameFile + *(s32 *)(cs->frameFile + 0xC);
    *(s16 *)(cs->frameTop + 0xA) = 0x18;
    *(s16 *)(cs->frameTop + 0xC) = 0x23;
    cs->frameBack = cs->frameFile + *(s32 *)(cs->frameFile + 0x14);
    *(s16 *)(cs->frameBack + 0xA) = 0x19;
    *(s16 *)(cs->frameBack + 0xC) = 0x32;
    cs->randomFile = func_80080360(0xF9B7B0);
    cs = gCharSel;
    cs->randomImage = cs->randomFile + *(s32 *)(cs->randomFile + 0xC);
    *(s16 *)(cs->randomImage + 0xA) = 8;
    *(s16 *)(cs->randomImage + 0xC) = 0x22;
    for (i = 0; i < 17; i++) {
        u8 *p = LoadCharacterPortrait(i, 0);
        gCharSel->portraitFile[i] = p;
        gCharSel->portrait[i] = p + *(s32 *)(p + 0xC);
    }
    if (flags & 0x20) {
        src = 0xF9D750;
        len = 0xF9DF80 - 0xF9D750;
    } else {
        src = 0xF9CF20;
        len = 0xF9D750 - 0xF9CF20;
    }
    gCharSel->background = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, len, 0);
    RomCopy(gCharSel->background, src, len);
    cs = gCharSel;
    cs->fontFile = func_80080360(0xF56270);
    cs = gCharSel;
    cs->font = cs->fontFile + *(s32 *)(cs->fontFile + 0xC);
    cs->charOut[0] = char1;
    cs->charOut[1] = costume1;
    cs->charOut[2] = char2;
    cs->charOut[3] = costume2;
    for (i = 0; i < 14; i++) {
        gCharSel->shine[i] = (D_800AD9C0[i] < 0) ? 1.0f : 0.0f;
        gCharSel->wobble[i] = 0;
        gCharSel->bounce[i] = 0;
    }
    if ((u32)*CharOut(0) >= 17) *CharOut(0) = 0;
    if (*CostumeOut(0) < 0 || *CostumeOut(0) > 0) *CostumeOut(0) = 0;
    if ((u32)*CharOut(1) >= 17) *CharOut(1) = 9;
    if (*CostumeOut(1) < 0 || *CostumeOut(1) > 0) *CostumeOut(1) = 0;

    i = Find(D_800AD9C0, 14, *CharOut(0));
    if (i < 14) gCharSel->cursor[0] = i;
    else if ((flags & 1) && D_800AD9F8 > 0 && Find(B_800E1268, D_800AD9F8, *CharOut(0)) < D_800AD9F8) {
        gCharSel->cursor[0] = 10;
        D_800AD9C0[10] = *CharOut(0);
    } else gCharSel->cursor[0] = 0;
    i = Find(D_800AD9C0, 14, *CharOut(1));
    if (i < 14) gCharSel->cursor[1] = i;
    else if ((flags & 2) && D_800AD9F8 > 0 && Find(B_800E1268, D_800AD9F8, *CharOut(1)) < D_800AD9F8) {
        gCharSel->cursor[1] = 13;
        D_800AD9C0[13] = *CharOut(1);
    } else gCharSel->cursor[1] = 4;

    cs = gCharSel;
    cs->chosen[0] = 0;
    cs->chosen[1] = 0;
    cs->pulse[0] = 0x4000;
    cs->pulse[1] = 0x4000;
    cs->pulseSin[0] = 1.0f;
    cs->pulseSin[1] = 1.0f;
    cs->picked[0] = 0.0f;
    cs->picked[1] = 0.0f;
    cs->scrollX = 0;
    cs->scrollY = 0;
    cs->result = -1;
    cs->active[0] = flags & 1;
    cs->active[1] = (flags >> 1) & 1;
    cs->chosen[0] = cs->locked[0] = (flags >> 2) & 1;
    cs->chosen[1] = cs->locked[1] = (flags >> 3) & 1;
    cs->canQuit = (flags >> 4) & 1;
    cs->last[0] = -1;
    cs->last[1] = -1;
    cs->leaveTimer = -1;
    CharMenuSound_Load(7);
    func_8002A340(&gScreenFade, 1);
}

/* 0x800522B0 */
void CharSel_Shutdown(void) {
    s32 i;

    if (gCharSel == NULL) return;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCharSel->frameFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCharSel->randomFile);
    for (i = 0; i < 17; i++)
        if (gCharSel->portraitFile[i] != NULL) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCharSel->portraitFile[i]);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCharSel->background);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCharSel->fontFile);
    func_8002AC24(&gScreenFade);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCharSel);
    gCharSel = NULL;
}

/* a newly unlocked secret fighter joins the list for the hidden slots */
static void AddSecret(s32 c) {
    s32 n = D_800AD9F8;
    if (Find(B_800E1268, n, c) < D_800AD9F8) return;
    n = D_800AD9F8++;
    B_800E1268[n] = c;
    B_800E1260 = n;
    B_800E1264 = n;
}

/* R on a hidden slot: the next secret fighter (unless the other player has picked it) */
static void NextSecret(s32 p) {
    CharSelScreen *cs = gCharSel;
    s32 o = 1 - p;

    if (cs->cursor[p] == 10 && !(cs->chosen[o] != 0 && cs->cursor[o] == cs->cursor[p])) {
        D_800AD9C0[10] = B_800E1268[B_800E1260++];
        if (!(B_800E1260 < D_800AD9F8)) B_800E1260 = 0;
        Sound(7, 0x7FFF, 0);
        cs = gCharSel;
    }
    if (cs->cursor[p] == 13 && !(cs->chosen[o] != 0 && cs->cursor[o] == cs->cursor[p])) {
        D_800AD9C0[13] = B_800E1268[B_800E1264++];
        if (!(B_800E1264 < D_800AD9F8)) B_800E1264 = 0;
        Sound(7, 0x7FFF, 0);
    }
}

/* a fighter picked: announce the name (unless just announced), the costume is the alternate
 * one if picked with a C button - or, picking the other player's fighter, the other costume */
static void Pick(s32 p, s32 alt) {
    CharSelScreen *cs = gCharSel;
    s32 o = 1 - p;

    cs->chosen[p] = 1;
    cs->pulse[p] = 0x4000;
    cs->pulseSin[p] = 1.0f;
    cs->picked[p] = 0.0f;
    if (cs->last[p] != D_800AD9C0[cs->cursor[p]] || cs->repeat[p] <= 0)
        Sound(D_800AD9C0[gCharSel->cursor[p]] + 8, 0x7FFF, 1);
    cs = gCharSel;
    cs->last[p] = D_800AD9C0[cs->cursor[p]];
    *CharOut(p) = D_800AD9C0[cs->cursor[p]];
    cs = gCharSel;
    if (cs->chosen[o] != 0 && cs->cursor[p] == cs->cursor[o]) alt = *CostumeOut(o) == 0;
    gCharSel->bounce[gCharSel->cursor[p]] = 1;
    Sound(0x19, 0x4FFB, 1);
    *CostumeOut(p) = alt;
    gCharSel->repeat[p] = 30;
}

/* one player's buttons */
static void PlayerInput(s32 p) {
    s32 *buttons = (p == 0) ? &B_800F4E54 : &B_800F4E6C;
    s32 held = (p == 0) ? B_800F2D18 : B_800F8C84;
    CharSelScreen *cs = gCharSel;
    s32 o = 1 - p, moved, alt;

    if (cs->active[p] == 0) {
        if (*buttons & 0x9000) cs->active[p] = 1;
        return;
    }
    if (cs->locked[p] != 0) return;
    if (cs->chosen[p] == 0 && cs->random[p] <= 0) {
        moved = 0;
        if (*buttons & 0x800) { cs->cursor[p] = D_800AD8E0[cs->cursor[p]]; moved = 1; }
        if (*buttons & 0x400) { gCharSel->cursor[p] = D_800AD918[gCharSel->cursor[p]]; moved = 1; }
        if (*buttons & 0x200) { gCharSel->cursor[p] = D_800AD950[gCharSel->cursor[p]]; moved = 1; }
        if (*buttons & 0x100) { gCharSel->cursor[p] = D_800AD988[gCharSel->cursor[p]]; moved = 1; }
        if (moved) {
            Sound(7, 0x7FFF, 0);
            gCharSel->pulse[p] = 0x4000;
            gCharSel->pulseSin[p] = 1.0f;
        }
        if ((*buttons & 0x10) && D_800AD9F8 > 0) NextSecret(p);
        if (*buttons & 0x900F) {
            alt = (*buttons & 0x9000) == 0;
            cs = gCharSel;
            if (D_800AD9C0[cs->cursor[p]] >= 0) Pick(p, alt);
            else {
                cs->random[p] = 30;                     /* the "?" slot: roll */
                *buttons &= ~0x9000;
                *CostumeOut(p) = alt;
            }
        }
        if ((*buttons & 0x4000) && gCharSel->canQuit != 0) {
            gCharSel->quit = 1;
            func_8002A990(&gScreenFade, 1);
        }
        if (((held & 0x2020) && (*buttons & 0x10)) || ((held & 0x10) && (*buttons & 0x2020)))
            gCharSel->random[p] = 30;                   /* L+R: roll */
        (void)o;
    } else if (*buttons & 0x4000) {                     /* B: change your mind */
        if (cs->done != 0) return;
        if (cs->random[p] > 0) {
            cs->random[p] = 0;
            return;
        }
        if (!(cs->chosen[o] != 0 && cs->cursor[o] == cs->cursor[p])) gCharSel->shine[gCharSel->cursor[p]] = 0.02f;
        Sound(0x19, 0x4FFB, 1);
        gCharSel->chosen[p] = 0;
        gCharSel->pulse[p] = -0x4000;
        gCharSel->pulseSin[p] = 0.0f;
    }
}

/* the random roll: a new fighter every third tick (not the same one twice), stopping at the
 * end or on A/Start */
static void Roll(s32 p) {
    s32 *buttons = (p == 0) ? &B_800F4E54 : &B_800F4E6C;
    s64 t = (s64)(((u64)D_800B09B0 << 32) | D_800B09B4);
    CharSelScreen *cs;
    s32 old, o = 1 - p;

    if (gCharSel->random[p] <= 0) return;
    if (func_800A4090(t + (p ? 2 : 0), 3) == 0) {
        old = gCharSel->cursor[p];
        do {
            gCharSel->cursor[p] = (s32)guRandom() % 14;
        } while (D_800AD9C0[gCharSel->cursor[p]] < 0 || gCharSel->cursor[p] == old);
        Sound(0x19, 0x4FFB, 1);
        gCharSel->random[p]--;
    }
    if (gCharSel->random[p] > 0 && !(*buttons & 0x9000)) return;
    cs = gCharSel;
    cs->random[p] = 0;
    cs->chosen[p] = 1;
    cs->picked[p] = 0.0f;
    cs->last[p] = D_800AD9C0[cs->cursor[p]];
    *CharOut(p) = D_800AD9C0[cs->cursor[p]];
    cs = gCharSel;
    if (cs->chosen[o] != 0 && cs->cursor[p] == cs->cursor[o]) {
        *CostumeOut(p) = *CostumeOut(o) == 0;
        cs = gCharSel;
    }
    if (cs->last[p] != D_800AD9C0[cs->cursor[p]] || cs->repeat[p] <= 0)
        Sound(D_800AD9C0[gCharSel->cursor[p]] + 8, 0x7FFF, 1);
    gCharSel->bounce[gCharSel->cursor[p]] = 1;
    Sound(0x19, 0x4FFB, 1);
}

/* a cursor's pulse, and its colour (player 1 red, player 2 yellow; brighter once picked) */
static void Pulse(s32 p) {
    CharSelScreen *cs = gCharSel;
    if (cs->active[p] != 0) cs->pulse[p] += (cs->chosen[p] != 0) ? 0x38E : 0xAAA;
    gCharSel->pulseSin[p] = vsSin(gCharSel->pulse[p]);
    cs = gCharSel;
    if (cs->chosen[p] != 0) {
        cs->picked[p] = (f32)((f64)cs->picked[p] + 0.025);
        if (1.0 < (f64)cs->picked[p]) cs->picked[p] = 1.0f;
    }
}

/* 0x80052394: the logic, once a tick */
s32 CharSel_Draw(void) {
    CharSelScreen *cs;
    ScreenFade *f = &gScreenFade;
    s32 i;
    f32 s;

    if (gCharSel == NULL) return 0;
    if (D_800AA164 != 0) B_800F4E54 = 0x1000;           /* (the attract demo presses Start) */
    if (D_800AA168 != 0) B_800F4E6C = 0x1000;
    if (D_800AA0BC != 0) AddSecret(6);
    if (D_800AA0E0 != 0) AddSecret(5);
    if (D_800AA104 != 0) AddSecret(13);
    if (D_800AA128 != 0) AddSecret(3);
    if ((FadeDone() || f->active == 0) && gCharSel->result == -1) {
        PlayerInput(0);
        PlayerInput(1);
        cs = gCharSel;
        if (!(cs->active[0] != 0 && cs->chosen[0] == 0) && !(cs->active[1] != 0 && cs->chosen[1] == 0) &&
            cs->leaveTimer < 0) {
            if (cs->active[0] != 0) cs->players = (cs->active[1] != 0) ? 3 : 1;
            else cs->players = 2;
            gCharSel->done = 1;
            gCharSel->leaveTimer = 100;
        }
    }
    cs = gCharSel;
    if (cs->cursor[0] != 10 && cs->cursor[1] != 10) D_800AD9C0[10] = -1;
    cs = gCharSel;
    if (cs->cursor[0] != 13 && cs->cursor[1] != 13) D_800AD9C0[13] = -1;
    cs = gCharSel;
    if (--cs->repeat[0] < 0) cs->repeat[0] = 0;
    cs = gCharSel;
    if (--cs->repeat[1] < 0) cs->repeat[1] = 0;
    Roll(0);
    Roll(1);
    for (i = 0; i < 14; i++) {
        cs = gCharSel;
        if ((f64)cs->shine[i] == 0.0 &&
            ((s32)guRandom() % 100 < 5 || gCharSel->cursor[0] == i || gCharSel->cursor[1] == i)) {
            gCharSel->shine[i] = 0.02f;
            Sound(0x19, 0x4FFB, 1);
        }
        cs = gCharSel;
        if (0.0 < (f64)cs->shine[i] && (f64)cs->shine[i] < 1.0) {
            cs->shine[i] = (f32)((f64)cs->shine[i] + 0.02);
            if (1.0 < (f64)cs->shine[i]) cs->shine[i] = 1.0f;
        }
        cs = gCharSel;
        if (0.0 < (f64)cs->shine[i]) cs->wobble[i] += 0x1555;
        cs = gCharSel;
        if (cs->bounce[i] > 0) {
            cs->bounce[i] += 0xE38;
            if (0x7FFF < cs->bounce[i]) cs->bounce[i] = 0;
        }
    }
    Pulse(0);
    Pulse(1);
    cs = gCharSel;
    s = (f32)(((f64)cs->pulseSin[0] + 1.0) / 2.0);
    s = (f32)((f64)s * 0.65 + 0.3);
    cs->colour[0][0] = (u32)((f64)s * 255.0);
    gCharSel->colour[0][1] = 0;
    gCharSel->colour[0][2] = 0;
    cs = gCharSel;
    if (cs->chosen[0] != 0) cs->colour[0][1] = (u32)((f64)(s * cs->picked[0]) * 80.0);
    cs = gCharSel;
    s = (f32)(((f64)cs->pulseSin[1] + 1.0) / 2.0);
    s = (f32)((f64)s * 0.65 + 0.3);
    cs->colour[1][0] = (u32)((f64)s * 255.0);
    gCharSel->colour[1][1] = (u32)((f64)s * 255.0);
    gCharSel->colour[1][2] = 0;
    cs = gCharSel;
    if (cs->chosen[1] != 0) cs->colour[1][2] = (u32)((f64)(s * cs->picked[1]) * 125.0);
    cs = gCharSel;
    if (!(++cs->scrollX < *(s16 *)(cs->background + 6))) cs->scrollX = 0;
    cs = gCharSel;
    if (!(++cs->scrollY < *(s16 *)(cs->background + 8))) cs->scrollY = 0;
    cs = gCharSel;
    if (cs->leaveTimer > 0 && --cs->leaveTimer <= 0) {
        cs->leaveTimer = -1;
        func_8002A990(&gScreenFade, 1);
    }
    func_8002AA14(&gScreenFade, 0.05f);
    if (f->active != 0 && FadeDone() && (f64)f->level == 1.0) func_8002AC24(&gScreenFade);
    if ((FadeDone() || f->active == 0) && (f64)f->level == 0.0) {
        cs = gCharSel;
        if (cs->done != 0) {
            cs->result = cs->players;
            cs = gCharSel;
        }
        if (cs->quit != 0) cs->result = 0;
    }
    return gCharSel->result;
}

static void SetColours(const u8 *c) {
    B_800F7D94 = c[0]; B_800F7778 = c[0];
    B_800F7789 = c[1]; B_800F7770 = c[1];
    B_800F7E78 = c[2]; B_800F4E34 = c[2];
    B_800F7788 = c[3]; B_800F7E70 = c[3];
    B_800F88BC = c[4]; B_800F88C8 = c[4];
    B_800F7ECC = c[5]; B_800F2D1C = c[5];
}

/* 0x8005427C: the picture, once a frame */
s32 CharSel_Frame(void) {
    f32 up[3], right[3];
    s32 i, c;

    if (gCharSel == NULL) return 0;
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    func_8007E9DC(&gDL);
    func_8007EB48(&gDL);
    func_80081924(&gDL, gCharSel->background, 3);       /* the background, tiled ... */
    Gfx2(0xBA001402, 0x200000);
    Gfx2(0xE4000000 | ((((B_800F5D00 - 1) * 4) & 0xFFF) << 12) | (((B_800F5BEC - 1) * 4) & 0xFFF), 0);
    Gfx2(0xB4000000, (gCharSel->scrollX << 21) | ((gCharSel->scrollY << 5) & 0xFFFF));   /* ... and scrolling */
    Gfx2(0xB3000000, 0x10000400);
    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);
    func_80082210(gCharSel->font, &gDL, 0);
    SetTexFilter(0);
    if (gCharSel->active[0] != 0 && (c = D_800AD9C0[gCharSel->cursor[0]]) >= 0) {
        SetColours(&D_800ADADC[c * 6]);
        func_80082518(gCharSel->font, &gDL, D_800AA72C[D_800AD9C0[gCharSel->cursor[0]]], -140.0f, 82.0f, 0, up, right, 8);
    }
    if (gCharSel->active[1] != 0 && (c = D_800AD9C0[gCharSel->cursor[1]]) >= 0) {
        SetColours(&D_800ADADC[c * 6]);
        func_80082518(gCharSel->font, &gDL, D_800AA72C[D_800AD9C0[gCharSel->cursor[1]]],
                      (f32)(140 - func_80082298(gCharSel->font, D_800AA72C[D_800AD9C0[gCharSel->cursor[1]]])), 82.0f,
                      0, up, right, 8);
    }
    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);
    for (i = 0; i < 14; i++) CharSel_DrawSlot(i);
    func_8002AC80(&gScreenFade, &gDL);
    return gCharSel->result;
}

/* a point of a pick's burst: angle a, radius k (times 18) */
static void BurstVtx(Vtx16 *v, f32 cx, f32 cy, s32 a, f32 t, f64 kx, f64 ky, u8 r, u8 g, u8 b, u8 alpha) {
    f32 x, y;
    x = (f32)((f64)cx + (f64)vsSin(a) * (((f64)t * kx + 0.0) * 18.0));
    y = (f32)((f64)cy + (f64)vsCos(a) * (((f64)t * ky + 0.0) * 18.0));
    v->x = (s32)x; v->y = (s32)y; v->z = 0;
    v->flag = 0; v->s = 0; v->t = 0;
    v->r = r; v->g = g; v->b = b; v->a = alpha;
}

/* the burst of light round a slot just picked by player p (red for 1, yellow for 2) */
static void PickBurst(s32 p, s32 slot, f32 *up, f32 *right) {
    f32 t = (f32)((f64)gCharSel->picked[p] * 2.5 + 0.2), cx, cy, c;
    s32 k, a0, a1;
    u8 edge, lo = p ? 0 : 10, g;

    (void)up; (void)right;
    if (!((f64)t < 1.0)) return;
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0x404240, 0x104240);
    func_8007FBE8(&gDL, 1);
    cx = (f32)((f64)D_800AD870[slot] + 2.0 + -4.0);
    cy = (f32)((f64)D_800AD8A8[slot] + 25.0);
    c = (f32)((1.0 - (f64)t) * 200.0);
    for (k = 0; k < 16; k++) {
        a0 = k << 12;
        a1 = ((k + 1) % 16) << 12;
        edge = (u32)((1.0 - (f64)t) * (p ? 250.0 : 255.0));
        g = p ? 255 : (u8)(u32)c;
        BurstVtx(&gVtx[0], cx, cy, a0, t, 0.3, 0.3, 255, g, (u32)c, lo);
        BurstVtx(&gVtx[1], cx, cy, a0, t, 2.4, 2.4, 255, g, (u32)c, edge);
        BurstVtx(&gVtx[2], cx, cy, a0, t, 6.0, 6.0, 255, g, (u32)c, lo);
        BurstVtx(&gVtx[3], cx, cy, a1, t, 0.3, 0.3, 255, g, (u32)c, lo);
        BurstVtx(&gVtx[4], cx, cy, a1, t, 2.4, 2.4, 255, g, (u32)c, edge);
        BurstVtx(&gVtx[5], cx, cy, a1, t, 6.0, 6.0, 255, g, (u32)c, lo);
        Gfx2(0x0400185F, (u32)gVtx);                       /* gSPVertex: 6 */
        gVtx += 6;
        Gfx2(0xB1000206, 0x00080602);                      /* gSP2Triangles */
        Gfx2(0xB1040802, 0x000A0804);
    }
    Gfx2(0xE7000000, 0);
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0xC084000, 0x3024000);
    func_8007FBE8(&gDL, 2);
}

/* a frame's colour: the cursor's on it (player 1's first, or player 2's first) or the usual */
static void FrameColour(s32 p2First, s32 slot) {
    static const u8 plain[6] = { 11, 170, 206, 0, 30, 100 };
    CharSelScreen *cs = gCharSel;
    s32 on1 = cs->active[0] != 0 && cs->cursor[0] == slot, on2 = cs->active[1] != 0 && cs->cursor[1] == slot;
    u8 c[6], *src;

    if (on1 && !(p2First && on2)) src = cs->colour[0];
    else if (on2) src = cs->colour[1];
    else {
        SetColours(plain);
        return;
    }
    c[0] = src[0]; c[3] = src[0] / 5;
    c[1] = src[1]; c[4] = src[1] / 5;
    c[2] = src[2]; c[5] = src[2] / 5;
    B_800F7D94 = c[0]; B_800F7778 = c[0];
    B_800F7788 = c[3]; B_800F7E70 = c[3];
    B_800F7789 = c[1]; B_800F7770 = c[1];
    B_800F88BC = c[4]; B_800F88C8 = c[4];
    B_800F7E78 = c[2]; B_800F4E34 = c[2];
    B_800F7ECC = c[5]; B_800F2D1C = c[5];
}

/* 0x80054874: one slot - a pick's burst, the frame's shadow and back, the portrait (or "?")
 * growing in as it lights up and bouncing when picked, and the frame's top */
void CharSel_DrawSlot(s32 slot) {
    f32 up[3], right[3], u[3], r[3], s;
    s32 x, y;

    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    if (gCharSel->cursor[0] == slot && gCharSel->chosen[0] != 0) PickBurst(0, slot, up, right);
    if (gCharSel->cursor[1] == slot && gCharSel->chosen[1] != 0) PickBurst(1, slot, up, right);
    B_800F7788 = 0; B_800F7E70 = 0; B_800F7D94 = 0; B_800F7778 = 0;
    B_800F88BC = 0; B_800F88C8 = 0; B_800F7789 = 0; B_800F7770 = 0;
    B_800F7ECC = 0; B_800F2D1C = 0; B_800F7E78 = 0; B_800F4E34 = 0;
    B_800F7E58 = 0x80; B_800F2D1D = 0x80; B_800F8760 = 0x80; B_800F5B78 = 0x80;
    x = D_800AD870[slot];
    y = D_800AD8A8[slot];
    func_80085FE4(&gDL, (CharFrame *)gCharSel->frameBack, (f32)(x + 6), (f32)(y - 10), 0, up, right, 8);   /* shadow */
    FrameColour(0, slot);
    func_80084CB0(&gDL, gCharSel->frameBack, (f32)(D_800AD870[slot] - 4), (f32)D_800AD8A8[slot], 0, up, right, 8);
    if (0.0 < (f64)gCharSel->shine[slot]) {
        s = (f32)((f64)gCharSel->shine[slot] * 13.0);
        if (1.0 < (f64)s) s = 1.0f;
        if (gCharSel->bounce[slot] != 0) s = (f32)((f64)s + (f64)vsSin(gCharSel->bounce[slot]) * 0.5);
        else
            s = (f32)((f64)s + (f64)vsSin(gCharSel->wobble[slot]) * (1.0 - (f64)gCharSel->shine[slot]) * 0.2);
        u[0] = up[0]; u[1] = up[1]; u[2] = up[2];
        r[0] = right[0]; r[1] = right[1]; r[2] = right[2];
        SetTexFilter(0);
        u[0] = s * u[0];
        u[1] = s * u[1];
        u[0] = s * u[0];                                   /* (sic: u[2] is never scaled) */
        r[0] = s * r[0];
        r[1] = s * r[1];
        r[2] = s * r[2];
        if (D_800AD9C0[slot] >= 0) {
            f64 d = ((f64)s - 1.0) * 6.0;
            func_80083A64(&gDL, gCharSel->portrait[D_800AD9C0[slot]], (f32)((f64)(D_800AD870[slot] - 4) - d),
                          (f32)((f64)D_800AD8A8[slot] + d), 0, u, r, 8);
        } else {
            static const u8 q[6] = { 255, 255, 0, 200, 0, 0 };
            SetColours(q);
            func_80084CB0(&gDL, gCharSel->randomImage, (f32)(D_800AD870[slot] - 4), (f32)D_800AD8A8[slot], 0, u, r, 8);
        }
        SetTexFilter(0x2000);
    }
    FrameColour(1, slot);
    func_80084CB0(&gDL, gCharSel->frameTop, (f32)(D_800AD870[slot] - 4), (f32)D_800AD8A8[slot], 0, up, right, 8);
}
