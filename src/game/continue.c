/*
 * CONTINUE.C (0x80050380-0x80051D00): the "Continue?" screen (see continue.h).
 *
 * Continue_Draw is the logic (Game_Render calls it every tick); Continue_Frame builds the
 * picture once a frame (Continue_Run's loop).
 */
#include "common.h"
#include "mpfplat.h"
#include "continue.h"

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

/* 0x80050380 */
void Continue_Init(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 side) {
    ContinueScreen *c;
    u8 *img;

    mpDebugPrintf("wusPal = %d, bulPal = %d\n", costume1, costume2);
    if (gContinue != NULL) return;
    gContinue = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(ContinueScreen), 0);
    bzero(gContinue, sizeof(ContinueScreen));
    c = gContinue;
    c->char1 = char1;
    c->costume1 = costume1;
    c->char2 = char2;
    c->costume2 = costume2;
    c->side = side;
    c->portraitFile = LoadCharacterPortrait(char2, costume2);
    c->portrait = img = c->portraitFile + *(s32 *)(c->portraitFile + 0x14);
    *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
    *(s16 *)(c->portrait + 0xC) = *(u16 *)(c->portrait + 8) - 1;
    c->obj = LoadCharacter(c->char1, c->costume1);
    cfCharacter_Init(&gContinue->fighter, (CharDef *)gContinue->obj->moves);
    c = gContinue;
    if (c->side == 2) {
        cfCharacter_TurnAround(&c->fighter);
        c = gContinue;
    }
    cfCharacter_PlaySequence(&c->fighter, 0x28, -1, 1, 0xFFFFFFFF);
    c = gContinue;
    c->fighter.pos[0] = (c->side == 2) ? 70.0f : -70.0f;
    c = gContinue;
    c->fighter.pos[1] = -185.0f;
    c->fighter.pos[2] = 0.0f;
    c->textFontFile = func_80080360(0xF56270);
    gContinue->textFont = gContinue->textFontFile + *(s32 *)(gContinue->textFontFile + 0xC);
    gContinue->countFontFile = func_80080360(0xF54510);
    gContinue->countFont = gContinue->countFontFile + *(s32 *)(gContinue->countFontFile + 0xC);
    gContinue->bigFontFile = func_80080360(0xF4D8B0);
    gContinue->bigFont = gContinue->bigFontFile + *(s32 *)(gContinue->bigFontFile + 0xC);
    func_8002A340(&gScreenFade, 1);
    if (!(++D_800AD864 < 4)) D_800AD864 = 0;
    c = gContinue;
    c->line1 = D_800AD640[char2][D_800AD864].line1;
    c->line2 = D_800AD640[char2][D_800AD864].line2;
    c->count = 10;
    c->timer = 60;
    c->fade = 1.0f;
    c->gameOverTimer = -1;
    c->result = -1;
}

/* 0x80050658 */
void Continue_Shutdown(void) {
    if (gContinue == NULL) return;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gContinue->portraitFile);
    FreeCharacter(gContinue->obj);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gContinue->textFontFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gContinue->countFontFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gContinue->bigFontFile);
    func_8002AC24(&gScreenFade);
    CharFrameCache_FreeAll();
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gContinue);
    gContinue = NULL;
}

/* 0x80050714: the logic, once a tick. The count goes down a second at a time (any button
 * hurries it); at 0 "Continue?" fades, "Game Over" is announced and shown for 2 seconds.
 * Start while counting continues. In the attract demo Start is "pressed" below 5. */
s32 Continue_Draw(void) {
    ContinueScreen *c = gContinue;
    s32 buttons;

    if (c == NULL) return 0;
    buttons = (c->side == 2) ? B_800F4E6C : B_800F4E54;
    if ((D_800AA164 != 0 || D_800AA168 != 0) && gContinue->count < 5) buttons = 0x1000;
    (void)D_800B09B0;
    if (D_800B09B4 & 1) cfCharacter_Update(&gContinue->fighter);
    c = gContinue;
    if (c->chosen == 0) {
        if (c->count > 0) {
            if (--c->timer <= 0) {
                c->count--;
                c->timer = 60;
            }
        } else {
            c->fade = (f32)((f64)c->fade - 0.02);
            if ((f64)c->fade < 0.0) c->fade = 0.0f;
        }
        c = gContinue;
        if (c->count > 0 && (buttons & 0xC00F)) {
            c->count--;
            c->timer = 60;
        }
    }
    if (buttons & 0x1000) {
        c = gContinue;
        if (c->count <= 0) goto over;
        if (c->chosen == 0) {
            c->chosen = 1;
            c->continued = 1;
            func_8002A990(&gScreenFade, 1);
        }
    }
    if (gContinue->count > 0) goto timer;
over:
    if (gContinue->gameOver == 0) {
        func_800797EC(B_800F1330, 0, 0, 5, 0x6FF9, 0, 0, 1.0f, 1);
        gContinue->gameOver = 1;
        gContinue->gameOverTimer = 120;
    }
timer:
    c = gContinue;
    if (c->gameOverTimer >= 0 && --c->gameOverTimer <= 0) {
        c->gameOverTimer = -1;
        func_8002A990(&gScreenFade, 1);
        gContinue->gaveUp = 1;
    }
    CharFrameCache_Update();
    func_8002AA14(&gScreenFade, 0.03f);
    if (FadeAt(1.0)) func_8002AC24(&gScreenFade);
    if (FadeAt(0.0)) {
        c = gContinue;
        if (c->result == -1) {
            if (c->continued != 0) c->result = 0;
            else if (gContinue->gaveUp != 0) gContinue->result = 1;
        }
    }
    return gContinue->result;
}

static void Gfx2(u32 w0, u32 w1) {
    u32 *d = gDL;
    gDL = d + 2;
    d[0] = w0;
    d[1] = w1;
}

static void SetVtx16(Vtx16 *v, s32 x, s32 y, u8 r, u8 g, u8 b, u8 a) {
    v->x = x; v->y = y; v->z = 0;
    v->flag = 0; v->s = 0; v->t = 0;
    v->r = r; v->g = g; v->b = b; v->a = a;
}

static void Corner(Vtx16 *v, s32 x, s32 y, u8 r, u8 a) {
    v->x = x; v->y = y; v->z = 0;
    v->flag = 0; v->s = 0; v->t = 0;
    v->r = r; v->g = 0; v->b = 0; v->a = a;
}

static void SetColours(const u8 *c) {
    B_800F7D94 = c[0]; B_800F7778 = c[0];
    B_800F7789 = c[1]; B_800F7770 = c[1];
    B_800F7E78 = c[2]; B_800F4E34 = c[2];
    B_800F7788 = c[3]; B_800F7E70 = c[3];
    B_800F88BC = c[4]; B_800F88C8 = c[4];
    B_800F7ECC = c[5]; B_800F2D1C = c[5];
}

/* 0x80050B38: the picture, once a frame */
s32 Continue_Frame(void) {
    static const u8 portraitColours[6] = { 200, 150, 150, 100, 70, 70 };
    static const u8 countColours[6] = { 255, 255, 255, 150, 170, 150 };
    ContinueScreen *c = gContinue;
    f32 up[3], right[3], x;
    char buf[0x100];
    u8 a;
    s32 flip, i;

    if (c == NULL) return 0;
    func_8007E860(&gDL);
    func_8007E9DC(&gDL);
    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0xC084000, 0x3024000);
    func_8007FBE8(&gDL, 1);
    /* the background: black, going red towards the loser's side */
    Corner(&gVtx[0], -160, 120, 0, 255);
    Corner(&gVtx[1], 160, 120, 0, 255);
    Corner(&gVtx[2], 160, -120, (gContinue->side == 2) ? 0xFF : 0, 255);
    Corner(&gVtx[3], -160, -120, (gContinue->side != 2) ? 0xFF : 0, 255);
    Gfx2(0x0400103F, (u32)gVtx);                   /* gSPVertex: 4 */
    gVtx += 4;
    if (gContinue->side == 2) Gfx2(0xB1000206, 0x00060204);   /* gSP2Triangles */
    else Gfx2(0xB1000204, 0x406);
    Gfx2(0xE7000000, 0);
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    SetTexFilter(0);
    SetColours(portraitColours);
    flip = gContinue->side == 2;
    func_80084CB0(&gDL, gContinue->portrait, flip ? -100.0f : 100.0f, -130.0f, 0, up, right, flip | 8);
    Continue_DrawFighter(&gContinue->fighter, gContinue->obj);

    /* the speech bubble: a box with a point towards the winner */
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0x404240, 0x104240);
    func_8007FBE8(&gDL, 1);
    {
        static const s16 xy[7][2] = { { -140, 104 }, { 140, 104 }, { 140, 64 }, { -140, 64 },
                                      { 30, 64 }, { 10, 64 }, { 50, 40 } };
        for (i = 0; i < 7; i++) {
            s32 px = xy[i][0];
            if (i >= 4 && gContinue->side == 2) px = -px;
            SetVtx16(&gVtx[i], px, xy[i][1], 255, 255, 255, 30);
        }
    }
    Gfx2(0x04001C6F, (u32)gVtx);                   /* gSPVertex: 7 */
    gVtx += 7;
    Gfx2(0xB1000206, 0x00060204);                  /* gSP2Triangles: the box */
    Gfx2(0xBF000000, 0x000C080A);                  /* gSP1Triangle: the point */
    Gfx2(0xE7000000, 0);
    func_80082210(gContinue->textFont, &gDL, 0);
    SetColours(&D_800ADADC[gContinue->char2 * 6]);
    func_80082518(gContinue->textFont, &gDL, gContinue->line1, -130.0f, 88.0f, 0, up, right, 8);
    func_80082518(gContinue->textFont, &gDL, gContinue->line2, -130.0f, 72.0f, 0, up, right, 8);

    /* "Continue?" and the count, on the loser's side */
    x = (gContinue->side == 2) ? 60.0f : -60.0f;
    mpSPrintf(buf, "%d", gContinue->count);
    func_80082210(gContinue->countFont, &gDL, 0);
    SetColours(countColours);
    a = (u32)((f64)gContinue->fade * 255.0);
    B_800F7E58 = a; B_800F2D1D = a; B_800F8760 = a; B_800F5B78 = a;
    func_80082564(gContinue->countFont, &gDL, "Continue?",
                  x - (f32)(func_80082298(gContinue->countFont, "Continue?") / 2), 30.0f, 0.0f, up, right, 8);
    func_80082564(gContinue->countFont, &gDL, buf, x - (f32)(func_80082298(gContinue->countFont, buf) / 2), 10.0f,
                  0.0f, up, right, 8);
    if (gContinue->gameOver != 0) {
        func_8007EB48(&gDL);
        func_80082210(gContinue->bigFont, &gDL, 1);
        func_80082358(gContinue->bigFont, &gDL, "Game Over", 0,
                      (B_800F5D00 - func_80082298(gContinue->bigFont, "Game Over")) / 2, 120, 0);
    }
    SetTexFilter(0x2000);
    func_8002AC80(&gScreenFade, &gDL);
    return gContinue->result;
}

/* 0x800517A8: the fighter's sprites, as in the fight (turned by its angle about the
 * vertical, scaled by its CharDef), but without lighting */
void Continue_DrawFighter(cfCharacter *f, CharObj *obj) {
    f32 up[3], right[3], u2[3], r2[3], m[16], lx, ly, p[3];
    CharFrame *frame;
    s32 i;

    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    if ((f64)f->angle == 0.0) {
        u2[0] = 0.0f; u2[1] = 1.0f; u2[2] = 0.0f;
        r2[0] = -1.0f; r2[1] = 0.0f; r2[2] = 0.0f;
    } else {
        for (i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = 1.0f; m[5] = 1.0f; m[15] = 1.0f;
        m[8] = up[0] * right[0];                        /* -0 */
        m[10] = right[0];
        func_80088440(m, (s32)((f64)((up[0] - f->angle) * 65536.0f) / 360.0));
        u2[0] = (f32)((f64)(up[0] * m[0] + up[1] * m[4] + up[2] * m[8]) + (f64)m[12]);
        u2[1] = (f32)((f64)(up[0] * m[1] + up[1] * m[5] + up[2] * m[9]) + (f64)m[13]);
        u2[2] = (f32)((f64)(up[0] * m[2] + up[1] * m[6] + up[2] * m[10]) + (f64)m[14]);
        r2[0] = (f32)((f64)(right[0] * m[0] + right[1] * m[4] + right[2] * m[8]) + (f64)m[12]);
        r2[1] = (f32)((f64)(right[0] * m[1] + right[1] * m[5] + right[2] * m[9]) + (f64)m[13]);
        r2[2] = (f32)((f64)(right[0] * m[2] + right[1] * m[6] + right[2] * m[10]) + (f64)m[14]);
    }
    for (i = 0; i < 3; i++) u2[i] = u2[i] / f->def->stats[8];
    for (i = 0; i < 3; i++) r2[i] = r2[i] / f->def->stats[8];
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
        frame = LoadCharacterFrame(obj, (s16)l->frame);
        p[0] = (f32)((f64)f->pos[0] + (f64)lx - (f64)(ly - f->pos[1]) * ((f64)up[0] / 2.0));
        p[1] = (f32)(0.0 - ((0.0 - (f64)f->pos[1]) + (f64)ly) * ((f64)up[1] / 2.0) + 0.0);
        p[2] = (f32)((f64)f->pos[2] + (f64)lx * 0.0 - (f64)(ly - f->pos[1]) * ((f64)up[2] / 2.0));
        func_80083A64(&gDL, frame, p[0], p[1], p[2], u2, r2, l->flags | 8);
    }
}
