/*
 * CREDITS.C (0x8005C550-0x8005D340): the credits - lines of text scrolling up over a slowly
 * circling arena (the 3D stage). First the staff list (if asked for), then two title lines,
 * then the ending for the fighter the player finished with (or a two-line one).
 *
 * Credits_Draw is the logic (Game_Render calls it every tick); Credits_Frame builds the
 * picture once a frame (Credits_Run's loop).
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"

typedef struct CreditItem {                /* a line of the credits script (0x18 bytes) */
    /* 0x00 */ s32 font;                   /* 0..4: which font */
    /* 0x04 */ s32 mode;                   /* text mode (func_80082210) */
    /* 0x08 */ s32 align;                  /* 1 = left, 2 = right, else centred */
    /* 0x0C */ const char *text;
    /* 0x10 */ u8 colour[6];
} CreditItem;

typedef struct CreditLine {                /* a line on the screen (0x1C bytes) */
    /* 0x00 */ const char *text;           /* NULL = free */
    /* 0x04 */ u8 *font;
    /* 0x08 */ s32 mode;
    /* 0x0C */ s32 y;                      /* -145 (just below the screen) up to 120 */
    /* 0x10 */ s32 x;
    /* 0x14 */ u8 colour[6];
} CreditLine;

typedef struct CreditsScreen {             /* 0x4130 bytes */
    /* 0x0000 */ u8  stage[0x35CC];        /* the 3D stage (func_8002EA90 and friends) */
    /* 0x35CC */ s32 stageFlag;
    /* 0x35D0 */ u8 *fontFile[5][2];       /* file, then the font in it, for fonts 0..4 */
    /* 0x35F8 */ f32 eye[3], at[3];
    /* 0x3610 */ s32 angle, bobAngle, radiusAngle;
    /* 0x361C */ s32 result;               /* -1 while running, 0 when done */
    /* 0x3620 */ s32 tick;
    /* 0x3624 */ CreditLine lines[100];
    /* 0x4114 */ s32 staff;                /* show the staff list first */
    /* 0x4118 */ s32 phase;                /* 0 staff, 1 titles, 2 ending, 3 finished */
    /* 0x411C */ s32 next;                 /* next line of the current part */
    /* 0x4120 */ s32 finished;             /* everything has scrolled in: A or Start leaves */
    /* 0x4124 */ s32 leaving;
    /* 0x4128 */ CreditItem *ending;
    /* 0x412C */ s32 endingLen;
} CreditsScreen;

extern CreditsScreen *D_800B07E0;
#define gCredits D_800B07E0
extern CreditItem D_800AEA10[0x11C];       /* the staff list */
extern CreditItem D_800B04B0[2];           /* the title lines */
extern CreditItem D_800B04E0[2], D_800B0510[7], D_800B05B8[7], D_800B0660[7], D_800B0708[7], D_800B07B0[2];
extern s32 D_800AA0E0, D_800AA128, D_800AA104, D_800AA0BC;   /* (which fighters are unlocked) */

void Credits_Init(s32 staff, s32 ending);
void Credits_Shutdown(void);
s32  Credits_Draw(void);
s32  Credits_Frame(void);
CreditLine *Credits_NewLine(void);
s32  Credits_Space(void);

static void SetTexFilter(u32 mode) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xBA000C02;
    g[1] = mode;
}

static s32 FadeAt(f64 level) {
    ScreenFade *f = &gScreenFade;
    s32 done = (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
    if (!done && f->active != 0) return 0;
    return (f64)f->level == level;
}

/* 0x8005C550 */
void Credits_Init(s32 staff, s32 ending) {
    static const u32 fonts[5] = { 0xF4D8B0, 0xF56270, 0xF54510, 0xF53700, 0xF58F00 };
    CreditsScreen *c;
    s32 i;

    if (gCredits != NULL) return;
    gCredits = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(CreditsScreen), 0);
    bzero(gCredits, sizeof(CreditsScreen));
    for (i = 0; i < 5; i++) {
        gCredits->fontFile[i][0] = func_80080360(fonts[i]);
        gCredits->fontFile[i][1] = gCredits->fontFile[i][0] + *(s32 *)(gCredits->fontFile[i][0] + 0xC);
    }
    c = gCredits;
    c->eye[0] = -1800.0f;
    c->eye[1] = 800.0f;
    c->eye[2] = 200.0f;
    c->at[0] = 0.0f;
    c->at[1] = 200.0f;
    c->at[2] = 200.0f;
    c->angle = 0x871C;
    c->bobAngle = 0;
    c->radiusAngle = 0;
    c->result = -1;
    func_8002EA90(c);
    c = gCredits;
    c->stageFlag = 0;
    c->staff = staff;
    if (ending == 0) {
        c->ending = D_800B04E0;
        c->endingLen = 2;
    } else if (D_800AA0E0 == 0) {
        c->ending = D_800B0510;
        c->endingLen = 7;
    } else if (D_800AA128 == 0) {
        c->ending = D_800B05B8;
        c->endingLen = 7;
    } else if (D_800AA104 == 0) {
        c->ending = D_800B0660;
        c->endingLen = 7;
    } else if (D_800AA0BC == 0) {
        c->ending = D_800B0708;
        c->endingLen = 7;
    } else {
        c->ending = D_800B07B0;
        c->endingLen = 2;
    }
    gCredits->phase = 0;
    gCredits->next = 0;
    func_8002A340(&gScreenFade, 1);
    CharMenuSound_Load(0x2A);
}

/* 0x8005C7DC */
void Credits_Shutdown(void) {
    s32 i;

    if (gCredits == NULL) return;
    for (i = 0; i < 5; i++) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCredits->fontFile[i][0]);
    func_8002ECBC(gCredits);
    func_8002AC24(&gScreenFade);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCredits);
    gCredits = NULL;
}

/* 0x8005C8A4: the logic, once a tick. A part moves on once its lines have all gone in and
 * the last one has scrolled far enough (more space after a short ending). */
s32 Credits_Draw(void) {
    CreditsScreen *c = gCredits;
    CreditItem *item = NULL;
    CreditLine *e;
    u8 *font;
    s32 space, h;
    f32 r;

    if (c == NULL) return 0;
    if (c->phase == 0) {
        if (c->staff == 0) c->phase = 1;
        else if ((u32)gCredits->next < 0x11C) item = &D_800AEA10[gCredits->next];
        else if (!(Credits_Space() < 101)) {
            gCredits->phase++;
            gCredits->next = 0;
        }
    }
    c = gCredits;
    if (c->phase == 1) {
        if ((u32)c->next < 2) item = &D_800B04B0[c->next];
        else {
            space = Credits_Space();
            if (!(space < ((gCredits->endingLen < 3) ? 41 : 31))) {
                gCredits->phase++;
                gCredits->next = 0;
            }
        }
    }
    c = gCredits;
    if (c->phase == 2) {
        if (c->next < c->endingLen) item = &c->ending[c->next];
        else {
            space = Credits_Space();
            if (!(space < ((gCredits->endingLen < 3) ? 81 : 46))) {
                gCredits->phase++;
                gCredits->next = 0;
            }
        }
    }
    c = gCredits;
    if (!(c->phase < 3) && c->finished == 0) c->finished = 1;
    if (item != NULL) {
        switch (item->font) {
        case 0: font = gCredits->fontFile[0][1]; break;
        case 1: font = gCredits->fontFile[1][1]; break;
        case 3: font = gCredits->fontFile[3][1]; break;
        case 4: font = gCredits->fontFile[4][1]; break;
        default: font = gCredits->fontFile[2][1]; break;
        }
        space = Credits_Space();
        h = *(s16 *)(font + 2);
        if (h + h / 4 < space) {
            e = Credits_NewLine();
            e->text = item->text;
            e->font = font;
            e->mode = item->mode;
            if (item->align & 1) e->x = -130;
            else if (item->align & 2) e->x = 130 - func_80082298(font, item->text);
            else e->x = -(func_80082298(font, item->text) / 2);
            e->colour[0] = item->colour[0];
            e->colour[1] = item->colour[1];
            e->colour[2] = item->colour[2];
            e->colour[3] = item->colour[3];
            e->colour[4] = item->colour[4];
            e->colour[5] = item->colour[5];
            gCredits->next++;
        }
    }
    func_8002ED34(gCredits);
    c = gCredits;
    if (c->phase < 3 && !(c->tick & 1)) {               /* scroll every other tick */
        s32 i;
        for (i = 0; i < 100; i++) {
            CreditLine *l = &gCredits->lines[i];
            if (l->text == NULL) continue;
            if (!(++l->y < 120)) l->text = NULL;
        }
        c = gCredits;
    }
    c->angle += 0x12;
    c->bobAngle += 0x3F;
    r = (f32)((f64)vsSin(c->radiusAngle) * 400.0 + 2000.0);
    gCredits->eye[0] = (f32)((f64)(vsSin(gCredits->angle) * r) + 0.0);
    gCredits->eye[1] = (f32)((f64)vsSin(gCredits->bobAngle) * 50.0 + 400.0);
    c = gCredits;
    c->eye[2] = (f32)((f64)(vsCos(c->angle) * r) + 300.0);
    c = gCredits;
    c->tick++;
    if (c->finished != 0 && FadeAt(1.0) && (B_800F8C78 & 0x9000)) {
        func_8002A990(&gScreenFade, 1);
        gCredits->leaving = 1;
    }
    func_8002AA14(&gScreenFade, (0.0 < (f64)gScreenFade.speed) ? 0.03f : 0.01f);
    {
        ScreenFade *f = &gScreenFade;
        s32 done = (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
        if ((done || f->active == 0) && gCredits->leaving != 0 && (f64)f->level == 0.0) gCredits->result = 0;
    }
    return gCredits->result;
}

/* 0x8005CFA0: the picture, once a frame */
s32 Credits_Frame(void) {
    f32 cam[16], up[3], right[3];
    s32 i;

    if (gCredits == NULL) return 0;
    func_8007E860(&gDL);
    func_8007E9DC(&gDL);
    func_8007E720(&gDL, 255, 0, 0);
    func_8007EEF0(&gDL);
    func_80088654(cam, gCredits->eye, gCredits->at);
    func_8002FD50(gCredits, cam);
    func_8002FE34(gCredits, cam);
    func_8007F94C(&gDL);
    func_8007EEF0(&gDL);
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    SetTexFilter(0);
    for (i = 0; i < 100; i++) {
        CreditLine *l = &gCredits->lines[i];
        if (l->text == NULL) continue;
        func_80082210(l->font, &gDL, l->mode);
        l = &gCredits->lines[i];
        B_800F7D94 = l->colour[0]; B_800F7778 = l->colour[0];
        B_800F7789 = l->colour[1]; B_800F7770 = l->colour[1];
        B_800F7E78 = l->colour[2]; B_800F4E34 = l->colour[2];
        B_800F7788 = l->colour[3]; B_800F7E70 = l->colour[3];
        B_800F88BC = l->colour[4]; B_800F88C8 = l->colour[4];
        B_800F7ECC = l->colour[5]; B_800F2D1C = l->colour[5];
        func_80082518(l->font, &gDL, l->text, (f32)l->x, (f32)l->y, 0, up, right, 8);
    }
    SetTexFilter(0x2000);
    func_8002AC80(&gScreenFade, &gDL);
    return gCredits->result;
}

/* 0x8005D238: a free line, just below the screen */
CreditLine *Credits_NewLine(void) {
    s32 i;
    for (i = 0; i < 100; i++) {
        CreditLine *l = &gCredits->lines[i];
        if (l->text != NULL) continue;
        l->font = gCredits->fontFile[0][1];
        l->mode = 0;
        l->y = -145;
        l->x = 16;
        l->colour[2] = 255; l->colour[1] = 255; l->colour[0] = 255;
        l = &gCredits->lines[i];
        l->colour[5] = 255; l->colour[4] = 255; l->colour[3] = 255;
        return &gCredits->lines[i];
    }
    mpFatalError("DUDE");
    return NULL;
}

/* 0x8005D2E4: how far the lowest line has risen from where lines start (265 when there are
 * none) */
s32 Credits_Space(void) {
    s32 lowest = 120, i;
    for (i = 0; i < 100; i++) {
        CreditLine *l = &gCredits->lines[i];
        if (l->text != NULL && l->y < lowest) lowest = l->y;
    }
    return lowest + 145;
}
