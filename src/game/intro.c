/*
 * INTRO.C (0x80048820-0x8004E480): the attract intro. A scripted timeline, one tick at a time:
 *
 *  1. the "ClayFighter" logo drops in front of the stage (a 3D arena) with a rumble, bounces
 *     on the floor until it settles, flashes white and the letters of the subtitle pop up one
 *     by one; the title spins in, a shock-wave ring spreads out and the camera flies off;
 *  2. a fade to the second part: arena 16 is loaded with fighter 4 walking in (Intro_Control
 *     loads characters when the logic asks for them), a picture of him is frozen, then four
 *     more fighters drop from the sky, land and taunt, and a sprite falls on fighter 2;
 *  3. a final picture fades in over the first stage.
 *
 * Intro_Draw is the logic (Game_Render calls it every tick); Intro_Frame builds the picture
 * (Intro_Run's loop: Intro_Control, then Intro_Frame, until Intro_Frame returns other than -1).
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"
#include "combat.h"
#include "char.h"

typedef struct IntroScreen {               /* 0x6590 bytes */
    /* 0x0000 */ u8  stage[0x35CC];        /* the 3D stage (func_8002EA90 and friends) */
    /* 0x35CC */ s32 stageFlag;
    /* 0x35D0 */ s32 part;                 /* 1, 2, 3 (see above) */
    /* 0x35D4 */ s32 tick;                 /* the timeline */
    /* 0x35D8 */ f32 eye[3], at[3];        /* parts 1 and 3: the camera */
    /* 0x35F0 */ f32 dEye[3], dAt[3];      /* ... and how it moves each tick */
    /* 0x3608 */ f32 cam[16];
    /* 0x3648 */ f32 eye2[3], at2[3];      /* part 2: the camera */
    /* 0x3660 */ f32 dEye2[3], dAt2[3];
    /* 0x3678 */ f32 cam2[16];
    /* 0x36B8 */ s32 rumbled;              /* the logo's rumble has been played */
    /* 0x36BC */ s32 whooshed;
    /* 0x36C0 */ f32 logoBase[3];          /* where the logo comes to rest */
    /* 0x36CC */ f32 logoDir[3];           /* the direction it comes from (normalised) */
    /* 0x36D8 */ f32 logoDrop;             /* 26 -> 0: the logo coming in (shaking below 8) */
    /* 0x36DC */ f32 logoPos[3];
    /* 0x36E8 */ f32 logoScale;
    /* 0x36EC */ f32 logoDist;
    /* 0x36F0 */ s32 bouncing;
    /* 0x36F4 */ f32 bounceSpeed;
    /* 0x36F8 */ f32 trailPos[3];          /* the start of the logo's trail */
    /* 0x3704 */ f32 trail;                /* 26 -> 0 */
    /* 0x3708 */ f32 flash;                /* the logo's shadow, 1 -> 0 (then the white flash) */
    /* 0x370C */ s32 bounces;
    /* 0x3710 */ u8 *letters[6];           /* the subtitle's pieces (decoded images) */
    /* 0x3728 */ f32 grow[5];              /* each piece popping up: 1 -> 0, -1 = hidden */
    /* 0x373C */ f32 titleScale, titleScaleMul;
    /* 0x3744 */ s32 titleAngle, titleSpin;
    /* 0x374C */ f32 titleY, titleDy;
    /* 0x3754 */ u8 *picture;              /* drawn instead of the stage (never set here) */
    /* 0x3758 */ ScreenFade fade;          /* the intro's own fade (white flash, cross-fades) */
    /* 0x3770 */ s32 fadeGrab;             /* (the fade's screen grab, ScreenFade + 0x18) */
    /* 0x3774 */ s32 v3774;
    /* 0x3778 */ f32 fadeSpeed;            /* ... its speed */
    /* 0x377C */ f32 screenFadeSpeed;      /* the screen fade's speed */
    /* 0x3780 */ f32 shake1;               /* > 0: three random numbers drawn a frame */
    /* 0x3784 */ f32 shake;                /* the camera shaking when the logo lands */
    /* 0x3788 */ f32 ring;                 /* the shock-wave ring: 0 -> 1, -1 = none */
    /* 0x378C */ s32 camAccel;
    /* 0x3790 */ s32 toPart2;              /* fade out, then part 2 */
    /* 0x3794 */ s32 loadPart2;            /* (Intro_Control) arena 16 and fighter 4 */
    /* 0x3798 */ s32 part2Start;
    /* 0x379C */ f32 walk;                 /* fighter 4 walking in, 1 -> 0 */
    /* 0x37A0 */ s32 frozen;               /* fighter 4 is a still picture */
    /* 0x37A4 */ f32 frozenPos[3];
    /* 0x37B0 */ s32 freeFighter4;         /* (Intro_Control) */
    /* 0x37B4 */ u8 *frozenFrame;          /* a copy of his last frame */
    /* 0x37B8 */ CharFrame *lastFrame;     /* the last frame drawn of him */
    /* 0x37BC */ s32 load12, load8, load9, load2;   /* (Intro_Control) load these fighters */
    /* 0x37CC */ s32 fall12; f32 fallSpeed12;       /* ... and drop them in */
    /* 0x37D4 */ s32 fall8; f32 fallSpeed8;
    /* 0x37DC */ s32 fall9; f32 fallSpeed9;
    /* 0x37E4 */ s32 fall2; f32 fallSpeed2;
    /* 0x37EC */ s32 drop, dropShown;      /* a sprite falling on fighter 2 */
    /* 0x37F4 */ f32 dropSpeed;
    /* 0x37F8 */ f32 dropPos[3];
    /* 0x3804 */ u8 *dropImage;
    /* 0x3808 */ s32 dropSound;
    /* 0x380C */ u8 pad380C[0x64E4 - 0x380C];
    /* 0x64E4 */ CharObj *objs[17];        /* the characters loaded */
    /* 0x6528 */ cfCharacter *fighters[17];
    /* 0x656C */ f32 floorY;
    /* 0x6570 */ s32 toPart3;              /* (Intro_Control) free everything, load the picture */
    /* 0x6574 */ s32 part3Start;
    /* 0x6578 */ u8 *endPicture;
    /* 0x657C */ f32 endAlpha;
    /* 0x6580 */ s32 endDelay;
    /* 0x6584 */ s32 skipped;
    /* 0x6588 */ s32 result;               /* -1 while running, 1 = done */
    /* 0x658C */ s32 finished;
} IntroScreen;

extern IntroScreen *D_800ACD48;
#define gIntro D_800ACD48
extern s32 D_800ACD44;                     /* screens using the images below */
extern u8 *D_800ACD40;                     /* the logo (loaded once) */
extern u8 *B_800E1220, *B_800E1224;        /* the two pictures of the falling sprite */
extern CharFrame *B_800E1228;              /* the last fighter frame Intro_DrawFighter drew */
extern s32 B_800F8DD0;                     /* the screen fade's screen grab (gScreenFade + 0x18) */
extern void *B_800F7E6C;                   /* the menu sounds */
u8 *mpDecodeBitmapTo32Bit(u8 *bmp);
void func_80079D70(void *bank, s32 a1, s32 a2, s32 a3, s32 volume, s32 a5);   /* start a tune */
void func_8003102C(void *stage);

void Intro_Init(void);
void Intro_Shutdown(void);
s32  Intro_Draw(void);
s32  Intro_Frame(void);
void Intro_Control(void);
void Intro_DrawFighter(cfCharacter *f, CharObj *obj, f32 *cam);
void Intro_WaitRetrace(void);

#define FREE(p) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, (p))

static void HalfOrigin(u8 *img) {
    *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
    *(s16 *)(img + 0xC) = *(s16 *)(img + 8) / 2;
}

static void BottomOrigin(u8 *img) {
    *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
    *(s16 *)(img + 0xC) = *(u16 *)(img + 8) - 1;
}

static u8 *LoadRom(u32 start, u32 end) {
    u8 *p = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, end - start, 0);
    RomCopy(p, start, end - start);
    return p;
}

static u8 *LoadImage(u32 start, u32 end) {
    u8 *tmp = LoadRom(start, end);
    u8 *img = mpDecodeBitmapTo32Bit(tmp);
    FREE(tmp);
    return img;
}

static s32 FadeDone(ScreenFade *f) {
    return (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
}

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

static void Sound(void *bank, s32 sound, s32 volume, s32 a6) {
    func_800797EC(bank, 0, 0, sound, volume, 0, a6, 1.0f, 1);
}

/* the retrace counter changes (Intro_Init waits for the next one) */
__attribute__((noinline)) void Intro_WaitRetrace(void) {
    u32 a = D_800B09B0, b = D_800B09B4;
    while (D_800B09B0 == a && D_800B09B4 == b) { PORT_SPIN(); }
}

/* 0x80048820 */
void Intro_Init(void) {
    IntroScreen *s;
    u8 *file;
    s32 grab;

    if (gIntro != NULL) return;
    gIntro = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(IntroScreen), 0);
    bzero(gIntro, sizeof(IntroScreen));
    func_8002EA90(gIntro);
    gIntro->stageFlag = 0;
    if (D_800ACD44++ == 0) {
        D_800ACD40 = LoadRom(0xFBA490, 0xFBB6A0);
        HalfOrigin(D_800ACD40);
        B_800E1220 = LoadImage(0xFBF8B0, 0xFC0740);
        BottomOrigin(B_800E1220);
        B_800E1224 = LoadImage(0xFC0740, 0xFC15D0);
        BottomOrigin(B_800E1224);
    }
    s = gIntro;
    s->logoBase[0] = 0.0f;
    s->logoBase[1] = 360.0f;
    s->logoBase[2] = 600.0f;
    s->logoDir[0] = -0.1f;
    s->logoDir[1] = -0.4f;
    s->logoDir[2] = 1.0f;
    func_80087DD0(s->logoDir);
    s = gIntro;
    s->logoDrop = 26.0f;
    s->logoPos[0] = s->logoBase[0];
    s->logoPos[1] = s->logoBase[1];
    s->logoPos[2] = s->logoBase[2];
    s->logoScale = 1.0f;
    s->logoDist = 26.0f;
    s->trailPos[0] = s->logoPos[0];
    s->trailPos[1] = s->logoPos[1];
    s->trailPos[2] = s->logoPos[2];
    s->trail = 26.0f;
    s->bounces = 0;
    s->flash = 0.0f;
    s->eye[0] = -1235.0f;
    s->eye[1] = 50.0f;
    s->eye[2] = 1283.0f;
    s->at[0] = 396.0f;
    s->at[1] = 504.0f;
    s->at[2] = -737.0f;
    func_80088654(s->cam, s->eye, s->at);
    s = gIntro;
    s->dEye[0] = 0.0f;
    s->dEye[1] = 0.5f;
    s->dEye[2] = -1.0f;
    s->dAt[0] = s->logoDir[0];
    s->dAt[1] = s->logoDir[1];
    s->dAt[2] = s->logoDir[2];
    s->dAt[1] = (f32)((f64)s->dAt[1] - 0.1);
    s->dAt[0] = (f32)((f64)s->dAt[0] * 13.0);
    s->dAt[1] = (f32)((f64)s->dAt[1] * 13.0);
    s->dAt[2] = (f32)((f64)s->dAt[2] * 13.0);
    s->fade.active = 0;
    s->fade.v4 = 0;
    s->fade.type = 0;
    s->fade.level = 1.0f;
    s->fade.speed = 1.0f;
    s->fade.rgb[0] = 0;
    gIntro->fade.rgb[1] = 0;
    gIntro->fade.rgb[2] = 0;
    s = gIntro;
    s->fadeGrab = 0;
    s->v3774 = 0;
    s->fadeSpeed = 0.01f;
    s->screenFadeSpeed = 0.02f;
    grab = 0;
    if ((f64)gScreenFade.level == 1.0) {
        grab = func_80081804();
        gIntro->screenFadeSpeed = 0.03f;
    }
    func_8002A340(&gScreenFade, 1);
    B_800F8DD0 = grab;
    s = gIntro;
    s->shake1 = 0.0f;
    s->shake = 0.0f;
    s->ring = -1.0f;
    s->grow[0] = -1.0f;
    s->grow[1] = -1.0f;
    s->grow[2] = -1.0f;
    s->grow[3] = -1.0f;
    s->grow[4] = -1.0f;
    s->titleScale = 1.0f;
    s->titleScaleMul = 1.0f;
    s->titleAngle = 0;
    s->titleSpin = 0;
    s->titleY = 0.0f;
    s->titleDy = 0.0f;
    s->part2Start = 1;
    s->walk = 0.0f;
    s->floorY = 0.0f;
    file = func_80080360(0xFC15D0);
    s = gIntro;
    s->letters[0] = mpDecodeBitmapTo32Bit(file + *(s32 *)(file + 0x14));
    *(s16 *)(s->letters[0] + 0xA) = 10;
    *(s16 *)(s->letters[0] + 0xC) = *(u16 *)(s->letters[0] + 8) - 1;
    gIntro->letters[1] = mpDecodeBitmapTo32Bit(file + *(s32 *)(file + 0xC));
    HalfOrigin(gIntro->letters[1]);
    gIntro->letters[2] = mpDecodeBitmapTo32Bit(file + *(s32 *)(file + 0x1C));
    HalfOrigin(gIntro->letters[2]);
    gIntro->letters[3] = mpDecodeBitmapTo32Bit(file + *(s32 *)(file + 0x24));
    HalfOrigin(gIntro->letters[3]);
    gIntro->letters[4] = mpDecodeBitmapTo32Bit(file + *(s32 *)(file + 0x2C));
    HalfOrigin(gIntro->letters[4]);
    gIntro->letters[5] = mpDecodeBitmapTo32Bit(file + *(s32 *)(file + 0x34));
    HalfOrigin(gIntro->letters[5]);
    FREE(file);
    gIntro->result = -1;
    Intro_WaitRetrace();
    s = gIntro;
    s->tick = 0;
    s->part = 1;
}

static void FreeImages(IntroScreen *s) {
    s32 i;
    for (i = 0; i < 6; i++) {
        if (gIntro->letters[i] != NULL) {
            FREE(gIntro->letters[i]);
            gIntro->letters[i] = NULL;
        }
    }
    (void)s;
    if (gIntro->picture != NULL) {
        FREE(gIntro->picture);
        gIntro->picture = NULL;
    }
}

static void FreeFighters(void) {
    s32 i;
    for (i = 0; i < 17; i++) {
        if (gIntro->fighters[i] != NULL) {
            cfCharacter_Destruct(gIntro->fighters[i]);
            FREE(gIntro->fighters[i]);
            gIntro->fighters[i] = NULL;
        }
    }
    for (i = 0; i < 17; i++) {
        if (gIntro->objs[i] != NULL) {
            FreeCharacter(gIntro->objs[i]);
            gIntro->objs[i] = NULL;
        }
    }
    if (gIntro->frozenFrame != NULL) {
        FREE(gIntro->frozenFrame);
        gIntro->frozenFrame = NULL;
    }
}

/* 0x80048FB8 */
void Intro_Shutdown(void) {
    if (gIntro == NULL) return;
    func_8002ECBC(gIntro);
    if (--D_800ACD44 <= 0) {
        D_800ACD44 = 0;
        FREE(D_800ACD40);
        D_800ACD40 = NULL;
        FREE(B_800E1220);
        B_800E1220 = NULL;
        FREE(B_800E1224);
        B_800E1224 = NULL;
    }
    FreeImages(gIntro);
    CharFrameCache_FreeAll();
    FreeFighters();
    func_8000B704();
    if (gIntro->endPicture != NULL) {
        FREE(gIntro->endPicture);
        gIntro->endPicture = NULL;
    }
    if ((f64)gScreenFade.level == 0.0) func_8007A530();
    func_8002AC24(&gIntro->fade);
    FREE(gIntro);
    gIntro = NULL;
}

/* a fighter dropped in by part 2 */
static void LoadFighter(s32 slot, f32 x, f32 z, s32 turn) {
    cfCharacter *f;
    gIntro->objs[slot] = LoadCharacter(slot, 0);
    gIntro->fighters[slot] = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(cfCharacter), 0);
    cfCharacter_Construct(gIntro->fighters[slot]);
    cfCharacter_Init(gIntro->fighters[slot], (CharDef *)gIntro->objs[slot]->moves);
    cfCharacter_PlaySequence(gIntro->fighters[slot], 0x11, 1, 1, 0xFFFFFFFF);
    f = gIntro->fighters[slot];
    f->pos[0] = x;
    gIntro->fighters[slot]->pos[1] = 500.0f;
    gIntro->fighters[slot]->pos[2] = z;
    if (turn) cfCharacter_TurnAround(gIntro->fighters[slot]);
}

/* 0x8004D614: loads and frees what the logic asks for (Intro_Run calls it before each frame) */
void Intro_Control(void) {
    if (gIntro == NULL) return;
    if (gIntro->loadPart2 != 0) {
        func_8007CCA4();
        FreeImages(gIntro);
        func_8000AF40(16);
        func_8007C5BC(2000, 0x20, 1000);
        gIntro->objs[4] = LoadCharacter(4, 0);
        gIntro->fighters[4] = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(cfCharacter), 0);
        cfCharacter_Construct(gIntro->fighters[4]);
        cfCharacter_Init(gIntro->fighters[4], (CharDef *)gIntro->objs[4]->moves);
        cfCharacter_PlaySequence(gIntro->fighters[4], 6, -1, 1, 0xFFFFFFFF);
        gIntro->loadPart2 = 0;
    }
    if (gIntro->freeFighter4 != 0) {
        cfCharacter_Destruct(gIntro->fighters[4]);
        FREE(gIntro->fighters[4]);
        gIntro->fighters[4] = NULL;
        FreeCharacter(gIntro->objs[4]);
        gIntro->objs[4] = NULL;
        CharFrameCache_FreeAll();
        gIntro->freeFighter4 = 0;
    }
    if (gIntro->load8 != 0) {
        LoadFighter(8, 120.0f, -70.0f, 1);
        gIntro->fallSpeed8 = 0.0f;
        gIntro->load8 = 0;
    }
    if (gIntro->load9 != 0) {
        LoadFighter(9, 20.0f, -170.0f, 1);
        gIntro->fallSpeed9 = 0.0f;
        gIntro->load9 = 0;
    }
    if (gIntro->load12 != 0) {
        LoadFighter(12, -310.0f, -70.0f, 0);
        gIntro->fallSpeed12 = 0.0f;
        gIntro->load12 = 0;
    }
    if (gIntro->load2 != 0) {
        LoadFighter(2, -55.0f, 50.0f, 1);
        gIntro->fallSpeed2 = 0.0f;
        gIntro->load2 = 0;
    }
    if (gIntro->toPart3 != 0) {
        func_8007CCA4();
        CharFrameCache_FreeAll();
        func_8000B704();
        FreeFighters();
        mpLinkedMemoryManager_Pack(&gN64MemoryManager);
        func_8007C5BC(10000, 0x100, 0xDAC);
        gIntro->endPicture = LoadRom(0xFC8D20, 0xFD3010);
        HalfOrigin(gIntro->endPicture);
        gIntro->toPart3 = 0;
    }
}

/* part 2: a fighter falling from the sky until he lands on the floor */
static void Fall(cfCharacter *f, s32 *falling, f32 *speed) {
    IntroScreen *s = gIntro;
    if (*falling != 0) {
        f->pos[1] = f->pos[1] - *speed;
        if (f->pos[1] < s->floorY) {
            f->pos[1] = s->floorY;
            *falling = 0;
            cfCharacter_PlaySequence(f, 0x14, 1, 1, 0xFFFFFFFF);
            Sound(B_800F8768, 0xC, 0x2FFD, 0);
        }
        *speed = (f32)((f64)*speed + 0.5);
    }
}

static void Landed(cfCharacter *f) {
    if (f != NULL && f->seq.seq == NULL && f->seqIndex == 0x14) cfCharacter_PlaySequence(f, 0, -1, 1, 0xFFFFFFFF);
}

static void Shrink(f32 *g, f64 k) {
    if (0.0 < (f64)*g) {
        *g = (f32)((f64)*g - k);
        if ((f64)*g < 0.0) *g = 0.0f;
    }
}

/* 0x80049300: the logic, once a tick */
s32 Intro_Draw(void) {
    IntroScreen *s = gIntro;
    ScreenFade *f = &gScreenFade;
    f32 t, shake, level;
    s32 i;

    if (s == NULL) return 0;          /* (the original returns whatever was in v0) */
    func_8002ED34(s);
    if ((B_800F8C78 & 0x9000) &&
        (FadeDone(f) || f->active == 0 || (B_800F8DD0 == 0 && 0.5 <= (f64)f->level))) {
        level = f->level;             /* skipped: fade out from where it is */
        func_8002A990(&gScreenFade, 1);
        f->level = level;
        gIntro->screenFadeSpeed = 0.03f;
        gIntro->skipped = 1;
    }
    func_8002AA14(&gScreenFade, gIntro->screenFadeSpeed);
    if (FadeDone(f) || f->active == 0) func_8002AC24(&gScreenFade);
    if (gIntro->skipped != 0 && (FadeDone(f) || f->active == 0)) gIntro->result = 1;
    func_8002AA14(&gIntro->fade, gIntro->fadeSpeed);
    if (FadeDone(&gIntro->fade) || gIntro->fade.active == 0) func_8002AC24(&gIntro->fade);
    s = gIntro;
    if (s->toPart2 != 0 && (FadeDone(&s->fade) || gIntro->fade.active == 0)) {
        s = gIntro;
        s->part = 2;
        s->toPart2 = 0;
        s->loadPart2 = 1;
        func_8002A340(&s->fade, 4);
        s = gIntro;
        s->fadeSpeed = 0.04f;
        s->eye2[0] = 0.0f;
        s->eye2[1] = 102.0f;
        s->eye2[2] = 262.0f;
        s->at2[0] = -220.0f;
        s->at2[1] = 94.0f;
        s->at2[2] = -151.0f;
    }

    s = gIntro;
    if (s->part == 1) {
        /* the logo coming in, shaking more as it gets close */
        if (0.0 < (f64)s->logoDrop) {
            if (8.0 <= (f64)s->logoDrop) t = 1.0f;
            else t = (f32)((f64)s->logoDrop / 8.0);
            if ((f64)t < 1.0) {
                s32 r = (s32)guRandom() % 1024;
                shake = (f32)(((f64)(f32)r / 1024.0 - 0.5) * (f64)t);
                if (gIntro->rumbled == 0) {
                    Sound(B_800F7E6C, 5, 0x2FFD, 0);
                    gIntro->rumbled = 1;
                }
            } else shake = 0.0f;
            s = gIntro;
            s->logoDrop = (f32)((f64)s->logoDrop - 0.12);
            if ((f64)s->logoDrop <= 0.0) {
                s->dAt[0] = 0.0f;
                s->dAt[1] = 0.0f;
                s->dAt[2] = 0.0f;
                s->logoDrop = 0.0f;
            } else {
                s->dAt[0] = t * s->dAt[0];
                s->dAt[1] = t * s->dAt[1];
                s->dAt[2] = t * s->dAt[2];
            }
            s = gIntro;
            s->logoDist = (f32)((f64)s->logoDist - (f64)t * 0.12);
            s->logoPos[0] = (f32)((f64)s->logoBase[0] - (f64)(s->logoDir[0] * s->logoDist) * 80.0 -
                                  (f64)(shake * s->logoDir[0]) * 80.0);
            s->logoPos[1] = (f32)((f64)s->logoBase[1] - (f64)(s->logoDir[1] * s->logoDist) * 80.0 -
                                  (f64)(shake * s->logoDir[1]) * 80.0);
            s->logoPos[2] = (f32)((f64)s->logoBase[2] - (f64)(s->logoDir[2] * s->logoDist) * 80.0 -
                                  (f64)(shake * s->logoDir[2]) * 80.0);
            s->logoScale = (f32)((f64)s->logoScale + (f64)t * 0.004);
        }
        s = gIntro;
        if (0.0 < (f64)s->trail) {
            f64 v;
            s->trailPos[0] = (f32)((f64)s->logoBase[0] - (f64)(s->logoDir[0] * s->trail) * 80.0);
            s->trailPos[1] = (f32)((f64)s->logoBase[1] - (f64)(s->logoDir[1] * s->trail) * 80.0);
            s->trailPos[2] = (f32)((f64)s->logoBase[2] - (f64)(s->logoDir[2] * s->trail) * 80.0);
            v = (f64)s->trail;
            s->trail = (5.0 < (f64)s->logoDrop) ? (f32)(v - 0.05) : (f32)(v - 1.2);
            s = gIntro;
            if ((f64)s->trail < 1.0) {
                s->trail = 0.0f;
                s->bouncing = 1;
                s->bounceSpeed = 0.0f;
            }
        }
        s = gIntro;
        if (s->bouncing != 0) {
            f32 y = s->logoPos[1] + s->bounceSpeed;
            s->logoPos[1] = y;
            if (y < -30.0f) {                       /* hits the floor */
                s->logoPos[1] = -30.0f;
                s->bounceSpeed = 0.0f;
                s->shake = 5.0f;
                s->bounces++;
                Sound(B_800F8768, 0xE, 0x2FFD, 0);
                s = gIntro;
                if (s->bounces > 0) {
                    s->bouncing = 0;
                    s->flash = 1.0f;
                }
            }
            s = gIntro;
            s->bounceSpeed = (f32)((f64)s->bounceSpeed - 0.7);
            if (s->bounceSpeed < -25.0f && s->whooshed == 0) {
                Sound(B_800F7E6C, 6, 0x4FFB, 0);
                gIntro->whooshed = 1;
            }
            s = gIntro;
            if ((f64)s->bounceSpeed < -50.0) s->bounceSpeed = -50.0f;
        }
        s = gIntro;
        if (0.0 < (f64)s->flash) {
            s->flash = (f32)((f64)s->flash - 0.02);
            if ((f64)s->flash <= 0.0) {
                s->flash = 0.0f;
                func_8002A340(&s->fade, 1);
                s = gIntro;
                s->ring = 0.0f;
                s->fade.rgb[0] = 0xFF;
                gIntro->fade.rgb[1] = 0xFF;
                gIntro->fade.rgb[2] = 0xFF;
                gIntro->fadeSpeed = 0.03f;
                Sound(B_800F7E6C, 4, 0x4FFB, 0);
            }
        }
        s = gIntro;
        Shrink(&s->grow[0], 0.08);
        Shrink(&gIntro->grow[1], 0.1);
        Shrink(&gIntro->grow[2], 0.1);
        Shrink(&gIntro->grow[3], 0.1);
        Shrink(&gIntro->grow[4], 0.1);
        s = gIntro;
        s->titleAngle += s->titleSpin;
        if (s->titleSpin > 0) s->titleSpin += 0x48;
        s = gIntro;
        s->titleScale = s->titleScale * s->titleScaleMul;
        s->titleY = s->titleY + s->titleDy;
        if (120.0 < (f64)s->titleY) {
            s->grow[0] = -1.0f;
            s->grow[1] = -1.0f;
            s->grow[2] = -1.0f;
            s->grow[3] = -1.0f;
            s->grow[4] = -1.0f;
        }
        s = gIntro;
        if (s->camAccel != 0) {
            s->dEye[0] = (f32)((f64)s->dEye[0] * 1.015);
            s->dEye[1] = (f32)((f64)s->dEye[1] * 1.015);
            s->dEye[2] = (f32)((f64)s->dEye[2] * 1.015);
        }
        s = gIntro;
        s->eye[0] = s->eye[0] + s->dEye[0];
        s->eye[1] = s->eye[1] + s->dEye[1];
        s->eye[2] = s->eye[2] + s->dEye[2];
        s->at[0] = s->at[0] + s->dAt[0];
        s->at[1] = s->at[1] + s->dAt[1];
        s->at[2] = s->at[2] + s->dAt[2];
    }

    s = gIntro;
    if (s->part == 2 && s->loadPart2 == 0) {
        cfCharacter *f4;
        if (s->part2Start != 0) {
            cfCharacter_PlaySequence(s->fighters[4], 6, -1, 1, 0xFFFFFFFF);
            s = gIntro;
            f4 = s->fighters[4];
            f4->pos[0] = -500.0f;
            s->fighters[4]->pos[1] = 0.0f;
            s->fighters[4]->pos[2] = -200.0f;
            s->walk = 1.0f;
            s->eye2[0] = -13.0f;
            s->eye2[1] = 462.0f;
            s->eye2[2] = 775.0f;
            s->at2[0] = s->fighters[4]->pos[0];
            s->at2[1] = s->fighters[4]->pos[1];
            s->at2[2] = s->fighters[4]->pos[2];
            s->at2[1] = (f32)((f64)s->at2[1] + 50.0);
            s->dEye2[0] = 0.0f;
            s->dEye2[1] = -0.2f;
            s->dEye2[2] = -0.2f;
            s->dAt2[0] = 0.0f;
            s->dAt2[1] = 0.0f;
            s->dAt2[2] = 0.0f;
            s->dropImage = B_800E1224;
            s->part2Start = 0;
        }
        s = gIntro;
        if (0.0 < (f64)s->walk) {
            s->walk = (f32)((f64)s->walk - 0.005);
            if ((f64)s->walk <= 0.0) {
                s->walk = 0.0f;
                cfCharacter_PlaySequence(s->fighters[4], 0x38, 1, 1, 0xFFFFFFFF);
            }
            s = gIntro;
            f4 = s->fighters[4];
            f4->pos[0] = (f32)((f64)f4->pos[0] + 1.5);
            f4 = s->fighters[4];
            f4->pos[2] = (f32)((f64)f4->pos[2] - 0.2);
            s->at2[0] = (f32)((f64)s->at2[0] + 2.0);
        }
        s = gIntro;
        Fall(s->fighters[12], &s->fall12, &s->fallSpeed12);
        Landed(gIntro->fighters[12]);
        Fall(gIntro->fighters[8], &gIntro->fall8, &gIntro->fallSpeed8);
        Landed(gIntro->fighters[8]);
        Fall(gIntro->fighters[9], &gIntro->fall9, &gIntro->fallSpeed9);
        Landed(gIntro->fighters[9]);
        Fall(gIntro->fighters[2], &gIntro->fall2, &gIntro->fallSpeed2);
        Landed(gIntro->fighters[2]);
        s = gIntro;
        if (s->drop != 0) {
            f32 y = s->dropPos[1] - s->dropSpeed;
            s->dropPos[1] = y;
            if (y < s->floorY) {
                s->dropPos[1] = s->floorY;
                s->drop = 0;
            }
            s = gIntro;
            if ((f64)s->dropPos[1] < 100.0 && s->dropSound == 0) {
                Sound(B_800F8768, 0x1B, 0x4FFB, 1);
                cfCharacter_PlaySequence(gIntro->fighters[2], 0x31, 1, 1, 0xFFFFFFFF);
                gIntro->dropSound = 1;
            }
            s = gIntro;
            s->dropSpeed = (f32)((f64)s->dropSpeed + 0.5);
        }
        func_8000B894(NULL);
        s = gIntro;
        s->eye2[0] = s->eye2[0] + s->dEye2[0];
        s->eye2[1] = s->eye2[1] + s->dEye2[1];
        s->eye2[2] = s->eye2[2] + s->dEye2[2];
        s->at2[0] = s->at2[0] + s->dAt2[0];
        s->at2[1] = s->at2[1] + s->dAt2[1];
        s->at2[2] = s->at2[2] + s->dAt2[2];
    }

    s = gIntro;
    if (s->part == 3 && s->toPart3 == 0) {
        if (s->part3Start != 0) {
            s->eye[0] = -500.0f;
            s->eye[1] = 200.0f;
            s->eye[2] = 200.0f;
            s->at[0] = 0.0f;
            s->at[1] = 20.0f;
            s->at[2] = 200.0f;
            s->dEye[0] = -1.5f;
            s->dEye[1] = -2.5f;
            s->dEye[2] = -0.6f;
            s->dAt[0] = 0.0f;
            s->dAt[1] = 0.5f;
            s->dAt[2] = 0.0f;
            s->endAlpha = 0.0f;
            s->endDelay = 250;
            s->part3Start = 0;
        }
        s = gIntro;
        s->eye[0] = s->eye[0] + s->dEye[0];
        s->eye[1] = s->eye[1] + s->dEye[1];
        s->eye[2] = s->eye[2] + s->dEye[2];
        s->at[0] = s->at[0] + s->dAt[0];
        s->at[1] = s->at[1] + s->dAt[1];
        s->at[2] = s->at[2] + s->dAt[2];
        s->dEye[1] = (f32)((f64)s->dEye[1] * 0.9);
        s->dAt[1] = (f32)((f64)s->dAt[1] * 0.95);
        if (--s->endDelay < 0) {
            s->endDelay = 0;
            s->endAlpha = (f32)((f64)s->endAlpha + 0.005);
            if (1.0 < (f64)s->endAlpha) s->endAlpha = 1.0f;
        }
    }

    /* the script */
    s = gIntro;
    switch (s->tick) {
    case 0:
        func_80079D70(B_800F7E6C, 0, 0, 1, 0x7FFF, 0);
        s = gIntro;
        s->at[0] = s->logoPos[0];
        s->at[1] = s->logoPos[1];
        s->at[2] = s->logoPos[2];
        s->at[1] = (f32)((f64)s->at[1] + 50.0);
        s->at[0] = (f32)((f64)s->at[0] - 10.0);
        s->at[2] = (f32)((f64)s->at[2] - 180.0);
        s->trailPos[0] = s->logoPos[0];
        s->trailPos[1] = s->logoPos[1];
        s->trailPos[2] = s->logoPos[2];
        break;
    case 30:
        Sound(B_800F7E6C, 3, 0x4FFB, 0);
        break;
    case 472:
        s->grow[0] = 1.0f;
        break;
    case 510:
        s->grow[1] = 1.0f;
        break;
    case 531:
        s->grow[2] = 1.0f;
        break;
    case 542:
        s->grow[3] = 1.0f;
        break;
    case 580:
        s->grow[4] = 1.0f;
        break;
    case 678:
        s->titleDy = 10.0f;
        break;
    case 700:
        s->dEye[0] = 2.0f;
        s->dEye[1] = -0.6f;
        s->dEye[2] = 0.0f;
        s->dAt[0] = -0.3f;
        s->dAt[1] = -0.7f;
        s->dAt[2] = -1.0f;
        s->camAccel = 1;
        break;
    case 800:
        func_8002A990(&s->fade, 2);
        s = gIntro;
        s->fade.rgb[2] = 0;
        s->fade.rgb[1] = 0;
        s->fade.rgb[0] = 0;
        s = gIntro;
        s->fadeSpeed = 0.02f;
        s->toPart2 = 1;
        break;
    case 1117:
        s->frozen = 1;
        s->frozenPos[0] = s->fighters[4]->pos[0];
        s->frozenPos[1] = s->fighters[4]->pos[1];
        s->frozenPos[2] = s->fighters[4]->pos[2];
        s->frozenFrame = mpMemoryManager_DuplicatePtr((MPF_MemoryManager *)&gN64MemoryManager, s->lastFrame, 0);
        break;
    case 1120:
        s->freeFighter4 = 1;
        break;
    case 1123:
        s->load9 = 1;
        break;
    case 1126:
        cfCharacter_PlaySequence(s->fighters[9], 0x11, 1, 1, 0xFFFFFFFF);
        gIntro->fall9 = 1;
        break;
    case 1144:
        s->load12 = 1;
        break;
    case 1147:
        cfCharacter_PlaySequence(s->fighters[12], 0x11, 1, 1, 0xFFFFFFFF);
        gIntro->fall12 = 1;
        break;
    case 1174:
        s->load8 = 1;
        break;
    case 1177:
        cfCharacter_PlaySequence(s->fighters[8], 0x11, 1, 1, 0xFFFFFFFF);
        gIntro->fall8 = 1;
        break;
    case 1200:
        s->load2 = 1;
        break;
    case 1203:
        cfCharacter_PlaySequence(s->fighters[2], 0x11, 1, 1, 0xFFFFFFFF);
        gIntro->fall2 = 1;
        break;
    case 1315:
        cfCharacter_PlaySequence(s->fighters[12], 0x40, 1, 1, 0xFFFFFFFF);
        Sound(B_800F8768, 3, 0x4FFB, 1);
        break;
    case 1340:
        cfCharacter_PlaySequence(s->fighters[8], 0x40, 1, 1, 0xFFFFFFFF);
        Sound(B_800F8768, 6, 0x4FFB, 1);
        break;
    case 1365:
        cfCharacter_PlaySequence(s->fighters[2], 0x38, -1, 1, 0xFFFFFFFF);
        break;
    case 1460:
        s->drop = 1;
        s->dropShown = 1;
        s->dropPos[0] = s->fighters[2]->pos[0];
        s->dropPos[1] = s->fighters[2]->pos[1];
        s->dropPos[2] = s->fighters[2]->pos[2];
        s->dropPos[1] = 500.0f;
        s->dropSpeed = 0.0f;
        break;
    case 1465:
        Sound(B_800F7E6C, 6, 0x4FFB, 0);
        break;
    case 1525:
    case 1539:
        s->dropImage = B_800E1220;
        break;
    case 1532:
    case 1546:
        s->dropImage = B_800E1224;
        break;
    case 1620:
        s->part = 3;
        s->toPart3 = 1;
        func_8002A340(&s->fade, 1);
        i = func_80081804();
        s = gIntro;
        s->fadeGrab = i;
        s->fadeSpeed = 0.02f;
        s->eye[0] = -800.0f;
        s->eye[1] = 0.0f;
        s->eye[2] = 0.0f;
        s->dEye[0] = 0.0f;
        s->dEye[1] = 0.0f;
        s->dEye[2] = 0.0f;
        s->at[0] = 0.0f;
        s->at[1] = 0.0f;
        s->at[2] = 0.0f;
        s->dAt[0] = 0.0f;
        s->dAt[1] = 0.0f;
        s->dAt[2] = 0.0f;
        s->part3Start = 1;
        break;
    case 475:
    case 1660:
    case 2140:
        func_8003102C(s);
        break;
    case 2457:
        s->finished = 1;
        s->result = 0;
        break;
    }

    s = gIntro;
    s->shake1 = (f32)((f64)s->shake1 - 0.1);
    s->shake = (f32)((f64)s->shake - 0.1);
    if (0.0 <= (f64)s->ring) {
        s->ring = (f32)((f64)s->ring + 0.01);
        if (1.0 <= (f64)s->ring) s->ring = -1.0f;
    }
    for (i = 0; i < 17; i++) {
        if (gIntro->fighters[i] != NULL && !(gIntro->tick & 1)) cfCharacter_Update(gIntro->fighters[i]);
    }
    if (!(gIntro->tick & 1)) CharFrameCache_Update();
    s = gIntro;
    s->tick++;
    return s->result;
}

static void SetVtx(Vtx16 *v, s32 x, s32 y, s32 z, u8 r, u8 g, u8 b, u8 a) {
    v->x = x; v->y = y; v->z = z;
    v->flag = 0; v->s = 0; v->t = 0;
    v->r = r; v->g = g; v->b = b; v->a = a;
}

static void SetColours(u8 r0, u8 g0, u8 b0, u8 r1, u8 g1, u8 b1) {
    B_800F7D94 = r0; B_800F7778 = r0;
    B_800F7789 = g0; B_800F7770 = g0;
    B_800F7E78 = b0; B_800F4E34 = b0;
    B_800F7788 = r1; B_800F7E70 = r1;
    B_800F88BC = g1; B_800F88C8 = g1;
    B_800F7ECC = b1; B_800F2D1C = b1;
}

/* the camera shaking: the stage drawn moved by a random amount */
static void Shake(f32 *dx, f32 *dz) {
    IntroScreen *s = gIntro;
    if (0.0 < (f64)s->shake1) {
        guRandom();
        guRandom();
        guRandom();
    }
    if (0.0 < (f64)gIntro->shake) {
        s32 r = (s32)guRandom() % 1024;
        f64 v = ((f64)(f32)r / 512.0 - 1.0) * (f64)gIntro->shake;
        *dx = (f32)(v + v);
        r = (s32)guRandom() % 1024;
        v = ((f64)(f32)r / 512.0 - 1.0) * (f64)gIntro->shake;
        *dz = (f32)(v + v);
    } else {
        *dz = 0.0f;
        *dx = 0.0f;
    }
}

/* the camera's matrices for drawing */
static void SetCamera(f32 *m, f32 *cam) {
    func_80087E70(m, cam);
    func_8008861C(m);
    func_8008004C(&gDL, m);
    func_80087E20(m);
    func_80080170(&gDL, m);
}

/* the shock-wave ring: one corner of a segment */
static void RingVtx(Vtx16 *v, s32 angle, f64 radius, f32 cx, f32 cz, u8 g, u8 b, u8 a) {
    f32 x, z;
    x = (f32)((f64)cx + (f64)vsSin(angle) * radius);
    z = (f32)((f64)cz + (f64)vsCos(angle) * radius);
    SetVtx(v, (s32)x, 20, (s32)z, 255, g, b, a);
}

/* a piece of the subtitle popping up (grow 1 -> 0: twice the size down to normal) */
static void Letter(f32 grow, f64 size, u8 *img, f32 x, f32 y, u8 *img2, f32 x2, f32 y2) {
    f32 up[3], right[3], zero;
    if (!(0.0 <= (f64)grow)) return;
    if (0.0 < (f64)grow || 1.0 < (f64)gIntro->titleScale) SetTexFilter(0x2000);
    else SetTexFilter(0);
    up[0] = 0.0f;
    up[1] = (f32)size;
    up[2] = 0.0f;
    zero = up[0];
    right[0] = zero - up[1];
    right[1] = zero;
    right[2] = zero;
    func_80083A64(&gDL, img, x, y, zero, up, right, 8);
    if (img2 != NULL) func_80083A64(&gDL, img2, x2, y2, zero, up, right, 8);
}

static void Double(f32 *d, const f32 *v) {
    d[0] = (f32)((f64)v[0] + (f64)v[0]);
    d[1] = (f32)((f64)v[1] + (f64)v[1]);
    d[2] = (f32)((f64)v[2] + (f64)v[2]);
}

/* 0x8004B3CC: the picture, once a frame */
s32 Intro_Frame(void) {
    IntroScreen *s = gIntro;
    f32 m[16], up[3], right[3], u[3], r[3], u2[3], r2[3], dx, dz;
    s32 i;

    if (s == NULL) return 0;
    if (s->result == 1) {
        func_8007E720(&gDL, 0, 0, 0);
        return 1;
    }
    if (gIntro->part == 1) {
        Shake(&dx, &dz);
        func_8007E860(&gDL);
        func_8007E9DC(&gDL);
        func_8007EEF0(&gDL);
        func_80088654(gIntro->cam, gIntro->eye, gIntro->at);
        s = gIntro;
        up[0] = s->cam[4];
        up[1] = s->cam[5];
        up[2] = s->cam[6];
        right[0] = -s->cam[0];
        right[1] = -s->cam[1];
        right[2] = -s->cam[2];
        if (s->picture != NULL) {
            if (s->titleSpin == 0) {
                u[0] = 0.0f; u[1] = 1.0f; u[2] = 0.0f;
                r[0] = -1.0f; r[1] = 0.0f; r[2] = 0.0f;
                func_8007F94C(&gDL);
                SetTexFilter(0);
                func_80083A64(&gDL, gIntro->picture, -161.0f, 120.0f, 0.0f, u, r, 8);
                SetTexFilter(0x2000);
            }
        } else {
            func_80087EE8(s, dx, 0.0f, dz);
            func_8002FD50(gIntro, gIntro->cam);
            func_8002FE34(gIntro, gIntro->cam);
            Gfx2(0xB9000002, 0);
        }
        SetCamera(m, gIntro->cam);

        /* the logo's trail */
        s = gIntro;
        if (0.0 < (f64)s->trail) {
            Vtx16 *v;
            func_8007FB08(&gDL, 0x204);
            func_8007FB90(&gDL, 0x404240, 0x404240);
            func_8007FBE8(&gDL, 1);
            s = gIntro;
            SetVtx(&gVtx[0], (s32)s->trailPos[0], (s32)s->trailPos[1], (s32)s->trailPos[2], 255, 0, 0, 0);
            SetVtx(&gVtx[1], (s32)s->logoPos[0], (s32)(s->logoPos[1] + 25.0f), (s32)s->logoPos[2], 255, 100, 0, 200);
            SetVtx(&gVtx[2], (s32)s->logoPos[0], (s32)(s->logoPos[1] - 25.0f), (s32)s->logoPos[2], 255, 100, 0, 200);
            SetVtx(&gVtx[3], (s32)s->logoPos[0], (s32)(s->logoPos[1] + 55.0f), (s32)s->logoPos[2], 255, 255, 255, 0);
            SetVtx(&gVtx[4], (s32)s->logoPos[0], (s32)(s->logoPos[1] - 55.0f), (s32)s->logoPos[2], 255, 255, 255, 0);
            v = gVtx;
            Gfx2(0x0400144F, (u32)v);           /* gSPVertex: 5 */
            gVtx = v + 5;
            Gfx2(0xBF000000, 0x204);            /* gSP1Triangle x 3 */
            Gfx2(0xBF000000, 0x206);
            Gfx2(0xBF000000, 0x408);
            Gfx2(0xE7000000, 0);
        }

        /* the logo, and its shadow while it bounces */
        s = gIntro;
        if (0.0 < (f64)s->logoDrop || s->bouncing != 0 || 0.0 < (f64)s->flash) {
            s32 flags;
            if (s->bouncing != 0 || 0.0 < (f64)s->flash) {
                f32 k, z;
                k = (f32)((500.0 - (f64)gIntro->logoPos[1]) * 0.005 + 0.1);
                z = k * 0.0f;
                u[0] = k; u[1] = z; u[2] = z;
                r[0] = z; r[1] = z; r[2] = -k;
                SetColours(0, 0, 0, 0, 0, 0);
                B_800F7E58 = 0x80; B_800F2D1D = 0x80; B_800F8760 = 0x80; B_800F5B78 = 0x80;
                func_80085FE4(&gDL, (CharFrame *)D_800ACD40, gIntro->logoPos[0],
                              (f32)(-30 - *(s16 *)(D_800ACD40 + 0xC)), gIntro->logoPos[2], u, r, 0);
            }
            s = gIntro;
            u2[0] = up[0] * s->logoScale;
            u2[1] = up[1] * s->logoScale;
            u2[2] = up[2] * s->logoScale;
            r2[0] = right[0] * s->logoScale;
            r2[1] = right[1] * s->logoScale;
            r2[2] = right[2] * s->logoScale;
            SetColours(200, 200, 200, 255, 255, 255);
            B_800F7778 = 0; B_800F7770 = 0; B_800F4E34 = 0;
            B_800F7E70 = 240; B_800F88C8 = 240; B_800F2D1C = 240;
            if (s->bouncing != 0) flags = 0;
            else flags = (0.0 < (f64)s->flash) ? 0 : 8;
            func_80084CB0(&gDL, D_800ACD40, s->logoPos[0], s->logoPos[1], s->logoPos[2], u2, r2, flags);
        }

        /* the shock-wave ring: 16 segments, three rings of vertices */
        s = gIntro;
        if (0.0 <= (f64)s->ring) {
            f32 a, inv, cx, cz;
            u8 alpha;
            func_8007FB08(&gDL, 0x205);
            func_8007FB90(&gDL, 0x404A50, 0x104A50);
            func_8007FBE8(&gDL, 1);
            s = gIntro;
            cx = s->logoPos[0];
            cz = s->logoPos[2];
            a = s->ring;
            inv = (f32)(1.0 - (f64)a);
            for (i = 0; i < 16; i++) {
                s32 a0 = i << 12, a1 = ((i + 1) % 16) << 12;
                f64 base = (f64)a * 900.0;
                Vtx16 *v;
                alpha = (u32)((f64)inv * 255.0);
                RingVtx(&gVtx[0], a0, base + 200.0, cx, cz, 255, 255, 0);
                RingVtx(&gVtx[1], a0, base + 300.0, cx, cz, 200, 0, alpha);
                RingVtx(&gVtx[2], a0, base + 400.0, cx, cz, 255, 255, 0);
                RingVtx(&gVtx[3], a1, base + 200.0, cx, cz, 255, 255, 0);
                RingVtx(&gVtx[4], a1, base + 300.0, cx, cz, 200, 0, alpha);
                RingVtx(&gVtx[5], a1, base + 400.0, cx, cz, 255, 255, 0);
                v = gVtx;
                Gfx2(0x0400185F, (u32)v);       /* gSPVertex: 6 */
                gVtx = v + 6;
                Gfx2(0xB1000206, 0x00080602);   /* gSP2Triangles */
                Gfx2(0xB1040802, 0x000A0804);
            }
            Gfx2(0xE7000000, 0);
        }

        /* the subtitle: spun, scaled and moved as one */
        func_8007F94C(&gDL);
        func_80088068(m, gIntro->titleAngle);
        func_80088134(m, gIntro->titleScale, gIntro->titleScale, gIntro->titleScale);
        func_80088184(m, 0.0f, gIntro->titleY, 0.0f);
        func_80088350(m, 0.0f, 5.0f, 0.0f);
        func_80080170(&gDL, m);
        SetTexFilter(0x2000);
        s = gIntro;
        Letter(s->grow[0], (f64)s->grow[0] + 1.0, s->letters[0], -120.0f, 47.0f, s->letters[1], -10.0f, -15.0f);
        s = gIntro;
        Letter(s->grow[1], (f64)s->grow[1] + (f64)s->grow[1] + 1.0, s->letters[2], -78.0f, 10.0f, NULL, 0, 0);
        s = gIntro;
        Letter(s->grow[2], (f64)s->grow[2] + (f64)s->grow[2] + 1.0, s->letters[3], 12.0f, 30.0f, NULL, 0, 0);
        s = gIntro;
        Letter(s->grow[3], (f64)s->grow[3] + (f64)s->grow[3] + 1.0, s->letters[4], 92.0f, 25.0f, NULL, 0, 0);
        s = gIntro;
        Letter(s->grow[4], (f64)s->grow[4] + (f64)s->grow[4] + 1.0, s->letters[5], 5.0f, -35.0f, NULL, 0, 0);
    }

    s = gIntro;
    if (s->part == 2) {
        if (s->part2Start != 0) {
            func_8007E860(&gDL);
            func_8007E720(&gDL, 0, 0, 0);
            return gIntro->result;
        }
        func_8007E860(&gDL);
        func_8000B908();
        func_80088654(gIntro->cam2, gIntro->eye2, gIntro->at2);
        s = gIntro;
        up[0] = s->cam2[4];
        up[1] = s->cam2[5];
        up[2] = s->cam2[6];
        right[0] = -s->cam2[0];
        right[1] = -s->cam2[1];
        right[2] = -s->cam2[2];
        SetCamera(m, s->cam2);
        func_8007EEF0(&gDL);
        func_8000B998(gIntro->cam2, NULL);
        func_8000BCA8(gIntro->cam2, NULL);
        func_8000BE80(gIntro->cam2, NULL);
        s = gIntro;
        if (s->frozen != 0) {
            Double(u, up);
            Double(r, right);
            func_80083A64(&gDL, s->frozenFrame, s->frozenPos[0] + 14.0f, s->frozenPos[1] - 8.0f, s->frozenPos[2],
                          u, r, 8);
        } else {
            if (s->fighters[4] != NULL && s->objs[4] != NULL) Intro_DrawFighter(s->fighters[4], s->objs[4], s->cam2);
            gIntro->lastFrame = B_800E1228;
        }
        s = gIntro;
        if (s->fighters[9] != NULL && s->objs[9] != NULL) Intro_DrawFighter(s->fighters[9], s->objs[9], s->cam2);
        s = gIntro;
        if (s->fighters[8] != NULL && s->objs[8] != NULL) Intro_DrawFighter(s->fighters[8], s->objs[8], s->cam2);
        s = gIntro;
        if (s->fighters[12] != NULL && s->objs[12] != NULL) Intro_DrawFighter(s->fighters[12], s->objs[12], s->cam2);
        s = gIntro;
        if (s->fighters[2] != NULL && s->objs[2] != NULL) Intro_DrawFighter(s->fighters[2], s->objs[2], s->cam2);
        s = gIntro;
        if (s->dropShown != 0) {
            Double(u2, up);
            Double(r2, right);
            func_80083A64(&gDL, s->dropImage, s->dropPos[0], s->dropPos[1], s->dropPos[2], u2, r2, 8);
        }
        func_8000C07C(gIntro->cam2, NULL);
    }

    s = gIntro;
    if (s->part == 3) {
        u8 a;
        if (s->part3Start != 0) return s->result;
        Shake(&dx, &dz);
        func_8007E860(&gDL);
        func_8007E9DC(&gDL);
        func_8007EEF0(&gDL);
        func_80088654(gIntro->cam, gIntro->eye, gIntro->at);
        s = gIntro;
        up[0] = s->cam[4];
        up[1] = s->cam[5];
        up[2] = s->cam[6];
        right[0] = -s->cam[0];
        right[1] = -s->cam[1];
        right[2] = -s->cam[2];
        func_80087EE8(s, dx, 0.0f, dz);
        func_8002FD50(gIntro, gIntro->cam);
        SetColours(255, 200, 200, 150, 100, 100);
        a = (u32)((f64)gIntro->endAlpha * 180.0);
        B_800F8760 = a;
        B_800F5B78 = a;
        B_800F7E58 = 0;
        B_800F2D1D = 0;
        u[0] = (f32)((f64)up[0] * 3.5);
        u[1] = (f32)((f64)up[1] * 3.5);
        u[2] = (f32)((f64)up[2] * 3.5);
        r[0] = (f32)((f64)right[0] * 4.0);
        r[1] = (f32)((f64)right[1] * 4.0);
        r[2] = (f32)((f64)right[2] * 4.0);
        func_80087E20(m);
        func_80080170(&gDL, m);
        func_80087E70(m, gIntro->cam);
        func_8008861C(m);
        func_8008004C(&gDL, m);
        func_80085FE4(&gDL, (CharFrame *)gIntro->endPicture, 0.0f, 150.0f, 200.0f, u, r, 8);
        func_8002FE34(gIntro, gIntro->cam);
        Gfx2(0xB9000002, 0);
        SetCamera(m, gIntro->cam);
    }
    func_8002AC80(&gIntro->fade, &gDL);
    func_8002AC80(&gScreenFade, &gDL);
    return gIntro->result;
}

/* 0x8004DDC8: a fighter's sprites facing the camera (turned by its angle), as in the fight */
void Intro_DrawFighter(cfCharacter *f, CharObj *obj, f32 *cam) {
    f32 up[3], right[3], u[3], r[3], m[16], c[3], lx, ly, p[3];
    CharFrame *frame;
    s32 i;

    if ((f64)f->angle == 0.0) {
        up[0] = cam[4]; up[1] = cam[5]; up[2] = cam[6];
        up[0] = (f32)((f64)up[0] + (f64)up[0]);
        up[1] = (f32)((f64)up[1] + (f64)up[1]);
        up[2] = (f32)((f64)up[2] + (f64)up[2]);
        right[0] = (f32)((f64)cam[0] * -2.0);
        right[1] = (f32)((f64)cam[1] * -2.0);
        right[2] = (f32)((f64)cam[2] * -2.0);
        u[0] = up[0]; u[1] = up[1]; u[2] = up[2];
        r[0] = right[0]; r[1] = right[1]; r[2] = right[2];
    } else {
        f32 zero;
        up[0] = cam[4]; up[1] = cam[5]; up[2] = cam[6];
        right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
        m[0] = cam[0]; m[1] = cam[1]; m[2] = cam[2]; m[3] = 0.0f;
        m[4] = up[0]; m[5] = up[1]; m[6] = up[2]; m[7] = 0.0f;
        zero = right[1];
        c[0] = up[1] * zero - up[2] * zero;
        c[1] = -up[2] - up[0] * zero;
        c[2] = up[0] * zero - -up[1];
        m[8] = -c[0]; m[9] = -c[1]; m[10] = -c[2];
        m[11] = zero; m[12] = zero; m[13] = zero; m[14] = zero;
        m[15] = 1.0f;
        func_80088440(m, (s32)((f64)((zero - f->angle) * 65536.0f) / 360.0));
        up[0] = (f32)((f64)up[0] + (f64)up[0]);
        up[1] = (f32)((f64)up[1] + (f64)up[1]);
        up[2] = (f32)((f64)up[2] + (f64)up[2]);
        right[0] = (f32)((f64)right[0] + (f64)right[0]);
        right[1] = (f32)((f64)right[1] + (f64)right[1]);
        right[2] = (f32)((f64)right[2] + (f64)right[2]);
        u[0] = (f32)((f64)(up[0] * m[0] + up[1] * m[4] + up[2] * m[8]) + (f64)m[12]);
        u[1] = (f32)((f64)(up[0] * m[1] + up[1] * m[5] + up[2] * m[9]) + (f64)m[13]);
        u[2] = (f32)((f64)(up[0] * m[2] + up[1] * m[6] + up[2] * m[10]) + (f64)m[14]);
        r[0] = (f32)((f64)(right[0] * m[0] + right[1] * m[4] + right[2] * m[8]) + (f64)m[12]);
        r[1] = (f32)((f64)(right[0] * m[1] + right[1] * m[5] + right[2] * m[9]) + (f64)m[13]);
        r[2] = (f32)((f64)(right[0] * m[2] + right[1] * m[6] + right[2] * m[10]) + (f64)m[14]);
    }
    for (i = 0; i < 3; i++) u[i] = u[i] / f->def->stats[8];
    for (i = 0; i < 3; i++) r[i] = r[i] / f->def->stats[8];
    for (i = 0; i < f->seq.numLayers; i++) {
        SeqLayer *l = &f->seq.layers[i];
        if ((f64)f->angle == 0.0) {
            lx = (f32)l->x;
            ly = (f32)l->y;
        } else {
            f32 x = (f32)l->x, y = (f32)l->y;
            lx = (f32)((f64)(x * f->matrix.m[0][0] + y * f->matrix.m[1][0]) + (f64)f->matrix.m[2][0]);
            ly = (f32)((f64)(x * f->matrix.m[0][1] + y * f->matrix.m[1][1]) + (f64)f->matrix.m[2][1]);
        }
        frame = B_800E1228 = LoadCharacterFrame(obj, (s16)l->frame);
        p[0] = (f32)((f64)(lx * cam[0] + f->pos[0]) - (f64)(ly - f->pos[1]) * ((f64)up[0] / 2.0));
        p[1] = (f32)((0.0 - ((0.0 - (f64)f->pos[1]) + (f64)ly) * ((f64)up[1] / 2.0)) + (f64)gIntro->floorY);
        p[2] = (f32)((f64)(lx * cam[2] + f->pos[2]) - (f64)(ly - f->pos[1]) * ((f64)up[2] / 2.0));
        func_80083A64(&gDL, frame, p[0], p[1], p[2], u, r, l->flags | 8);
    }
}
