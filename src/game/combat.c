/*
 * COMBAT.C (0x80031FE0-0x80048150): the fight screen - loading the two fighters and the
 * arena, running the match frame by frame, the on-screen display (health bars, timer, round
 * and win texts, "Combo Breaker!"), the sound queue and the lighting of the fighters.
 *
 * Globals are the disassembly's labels (B_ = BSS, D_ = data); combat.h gives them names.
 */
#include "common.h"
#include "mpfplat.h"
#include "combat.h"

/* 0x80031FE0: a fight with a time limit (1200 frames) and a callback each frame (the attract
 * demo) */
s32 Combat_Start(GameSettings *g, s32 a1, s32 a2) {
    gCombatArcade = 1;
    gCombatTimeLimit = 1200;
    B_800E0E38 = a1;
    B_800E0E3C = (void *)a2;
    gCombatSolo = 0;
    return Combat_Main(g);
}

/* 0x80032028: a fight with only one fighter loaded and a callback each frame (nothing calls
 * it) */
s32 func_80032028(GameSettings *g, s32 a1, s32 a2) {
    gCombatArcade = 0;
    gCombatSolo = 1;
    B_800E0E44 = a1;
    B_800E0E48 = (void *)a2;
    return Combat_Main(g);
}

/* 0x80032064: a normal fight */
s32 Combat_StartAlt(GameSettings *g) {
    gCombatArcade = 0;
    gCombatSolo = 0;
    return Combat_Main(g);
}

/* 0x80036D54: fade the screen out (once), as soon as any earlier fade has finished */
void Combat_FadeOut(void) {
    if (B_800E0CE8 != 0) return;
    if (!((0.0 <= (f64)gScreenFade.speed) ? (1.0 <= (f64)gScreenFade.level) : ((f64)gScreenFade.level <= 0.0))) {
        if (gScreenFade.active != 0) return;
    }
    func_8002A990(&gScreenFade, 3);
    gScreenFade.rgb[0] = 0;
    gScreenFade.rgb[1] = 0;
    gScreenFade.rgb[2] = 0;
    B_800E0CE8 = 1;
}

/* 0x80036E1C: set a line of text; align 0 = centred, 1 = from the left, else from the right */
void HudText_Set(HudText *t, void *font, const char *s, s32 align) {
    char one[2];
    f32 x, w;
    s32 i;

    one[0] = 0;
    one[1] = 0;
    t->len = mpStrLen(s);
    w = (f32)func_80082298(font, s);
    if (align == 0)      x = (f32)(0.0 - (f64)w / 2.0);
    else if (align == 1) x = 0.0f;
    else                 x = (f32)(0.0 - (f64)w);
    mpStrCpy(t->text, s);
    for (i = 0; i < t->len; i++) {
        t->x[i] = x;
        t->y[i] = 0.0f;
        one[0] = s[i];
        w = (f32)func_80082298(font, one);
        t->dir[i][0] = 0.0f;
        t->dir[i][1] = 0.0f;
        t->spin[i] = 0;
        x += w;
    }
}

/* 0x80046D00 */
void ComboBanner_Start(ComboBanner *b) {
    if (D_800ACCFC == NULL) return;
    HudText_Set(&b->line1, D_800ACCFC, "Combo Breaker!", 0);
    HudText_Set(&b->line2, D_800ACCFC, "", 0);
    b->t = 0.0f;
    b->state = 0;
    b->alpha = 1.0f;
    b->scale = 1.0f;
    b->active = 1;
    b->v4 = 1;
    b->v538 = 0;
    b->v53C = 2000;
    b->rgb1[0] = 50; b->rgb1[1] = 0; b->rgb1[2] = 0;
    b->rgb2[0] = 255; b->rgb2[1] = 0; b->rgb2[2] = 0;
    b->v548 = 1.25f;
    b->v54C = 0;
    b->v550 = 4000;
    b->v554 = 0.22f;
    b->v558 = 1;
    func_800797EC(B_800F1330, 0, 0, 10, 0x7FFF, 0, 1, 1.0f, 1);
}

/* 0x80046E14 */
void ComboBanner_Update(ComboBanner *b) {
    if (b->active == 0) return;
    b->v538 += b->v53C;
    b->v54C += b->v550;
    switch (b->state) {
    case 0:     /* zoom in */
        b->scale = (f32)(1.0 - (f64)b->t);
        b->t = (f32)((f64)b->t + 0.07);
        if (1.0 < (f64)b->t) {
            b->state++;
            b->t = 0.0f;
            b->scale = 0.0f;
        }
        break;
    case 1:     /* hold */
        b->t = (f32)((f64)b->t + 0.01);
        if (1.0 < (f64)b->t) {
            b->state++;
            b->t = 0.0f;
        }
        break;
    case 2:     /* fade out */
        b->alpha = (f32)(1.0 - (f64)b->t);
        b->t = (f32)((f64)b->t + 0.07);
        if (1.0 < (f64)b->t) {
            b->state++;
            b->t = 0.0f;
        }
        break;
    case 3:
        b->active = 0;
        break;
    }
}

/* 0x8004639C: play the sounds the combat handler queued this frame */
void Combat_PlaySounds(void) {
    s32 i;
    for (i = 0; i < gCombat->numSounds; i++) {
        void *bank = B_800F8768;
        f32 pitch = 1.0f;
        SoundReq *r = &gCombat->sounds[i];
        s32 vol, flag;
        if (r->side == 1) {             /* fighter 1's own sounds */
            if (gCombatFighter1->v468 != 0) continue;
            bank = gCombatChar1->bank;
            pitch = pitch * gCombatFighter1->def->stats[7];
        } else if (r->side == 2) {
            if (gCombatSolo != 0) continue;
            if (gCombatFighter2->v468 != 0) continue;
            bank = gCombatChar2->bank;
            pitch = pitch * gCombatFighter2->def->stats[7];
        }
        r = &gCombat->sounds[i];
        if (r->sound < 0) {
            func_8007A0BC(bank);
            continue;
        }
        if (n64PlaySFX(bank, 0, 0, gCombat->sounds[i].sound) != 0) {
            r = &gCombat->sounds[i];
            flag = r->v20 != 0;
            vol = r->volume;
            pitch = pitch * B_800F5BC4->v48;
        } else {
            r = &gCombat->sounds[i];
            vol = r->volume;
            flag = 0;
            pitch = 1.0f;
        }
        func_800797EC(bank, 0, 0, r->sound, vol, 0, flag, pitch, 1);
    }
    gCombat->numSounds = 0;
}

/* 0x800382CC: the colour of the arena's lights at a point; it tints the fighters */
void Combat_SetLightColour(f32 x, f32 y, f32 z) {
    f32 pos[3], c[3];
    f32 r = 0.0f, g = 0.0f, b = 0.0f;
    u8 v;
    s32 i;

    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    for (i = 0; i < D_800AB4A4; i++) {
        func_800893F8(D_800AB4A8 + i * 0x3C, pos, NULL, &c[0], &c[1], &c[2]);
        r += c[0];
        g += c[1];
        b += c[2];
    }
    if (1.0 < (f64)r) r = 1.0f;
    if (1.0 < (f64)g) g = 1.0f;
    if (1.0 < (f64)b) b = 1.0f;
    v = (u8)(u32)((f64)r * 255.0);
    B_800F7788 = v; B_800F7E70 = v; B_800F7D94 = v; B_800F7778 = v;
    v = (u8)(u32)((f64)g * 255.0);
    B_800F88BC = v; B_800F88C8 = v; B_800F7789 = v; B_800F7770 = v;
    v = (u8)(u32)((f64)b * 255.0);
    B_800F7ECC = v; B_800F2D1C = v; B_800F7E78 = v; B_800F4E34 = v;
}

/* 0x80038574 */
void func_80038574(void) {
    if (gCombat->v68 == 0) return;
    if (gCombatSolo != 0) {
        Fighter_DrawShadow(gCombatFighter1, gCombatChar1);
        func_8003A134(gCombatFighter1, gCombatChar1);
    } else {
        Fighter_DrawShadow(gCombatFighter2, gCombatChar2);
        Fighter_DrawShadow(gCombatFighter1, gCombatChar1);
        func_8003A134(gCombatFighter1, gCombatChar1);
        func_8003A134(gCombatFighter2, gCombatChar2);
    }
}

/* 0x8003A134: empty */
void func_8003A134(void *f, void *c) {
    (void)f; (void)c;
}

/* 0x80041CD4, 0x80041CE0, 0x80041CEC: empty (they only reserve stack space) */
void func_80041CD4(void) {}
void func_80041CE0(void) {}
void func_80041CEC(void) {}

/* 0x80036F74: point the game camera along the combat handler's camera, plus screen shake */
void Combat_SetCamera(void) {
    CombatHandler *h = gCombat;
    f32 x, y, z;

    B_800F5CCC[0] = h->camEye[0];
    B_800F5CCC[1] = h->camEye[1];
    B_800F5CCC[2] = h->camEye[2];
    if (0.9 <= (f64)h->v100) {
        /* the end of the intro fly-in: rise */
        B_800F5CCC[1] = (f32)((f64)h->camEye[1] + (f64)h->vD8 * ((f64)h->v100 - 0.9) * 10.0);
    } else if (gCombat->v54 >= 4) {
        B_800F5CCC[1] = B_800F5CCC[1] + gCombat->vD8;
    }
    h = gCombat;
    B_800F7A70[0] = h->camAt[0];
    B_800F7A70[1] = h->camAt[1];
    B_800F7A70[2] = h->camAt[2];
    func_80088654(gCamera, B_800F5CCC, B_800F7A70);
    B_800F87A4 = B_800F87A4 + B_800F5CDC;
    if (gCombatSolo != 0 && 0.9 <= (f64)gCombat->v100) {
        func_800883F0(gCamera, (s32)(((f64)gCombat->v100 - 0.9) * -23500.0));
    }
    if (B_800E0CFC > 0) {
        x = (f32)(((f64)(f32)((s32)guRandom() % 1024) - 512.0) / 300.0 * (f64)(f32)B_800E0CFC);
        y = (f32)(((f64)(f32)((s32)guRandom() % 1024) - 512.0) / 300.0 * (f64)(f32)B_800E0CFC);
        z = (f32)(((f64)(f32)((s32)guRandom() % 1024) - 512.0) / 5000.0 * (f64)(f32)B_800E0CFC * 182.04444444444445);
        func_80088350(gCamera, x, y, 0.0f);
        func_80088440(gCamera, (s32)z);
    }
}

/* 0x80037264: a free camera on controller 1 (debugging; nothing calls it in the final game) */
void Combat_DebugCamera(void) {
    if (B_800F88E4 & 0x800)  func_80088350(gCamera, 0.0f, 0.0f, -5.0f);
    if (B_800F88E4 & 0x400)  func_80088350(gCamera, 0.0f, 0.0f, 5.0f);
    if (B_800F88E4 & 0x200)  func_80088350(gCamera, -5.0f, 0.0f, 0.0f);
    if (B_800F88E4 & 0x100)  func_80088350(gCamera, 5.0f, 0.0f, 0.0f);
    if (B_800F88E4 & 0x4000) func_80088350(gCamera, 0.0f, 5.0f, 0.0f);
    if (B_800F88E4 & 0x8000) func_80088350(gCamera, 0.0f, -5.0f, 0.0f);
    if (!((u32)(B_800F7E50 + 10) < 21)) func_800883F0(gCamera, -(B_800F7E50 * 5));
    if (!((u32)(B_800F7784 + 10) < 21)) func_800883A0(gCamera, -(B_800F7784 * 5));
    if (B_800F88E4 & 0x20)   func_80088440(gCamera, 300);
    if (B_800F88E4 & 0x2000) func_80088440(gCamera, -300);
}

/* the shown value moves towards the real one, 10 a frame (jumping there when within 10) */
static s32 Approach10(s32 shown, s32 real) {
    if (shown != real) {
        if (shown < real) shown = (real - shown < 11) ? real : shown + 10;
        else              shown = (shown - real < 11) ? real : shown - 10;
    }
    return shown;
}

/* 0x80041CF8: the bars at the top of the screen: slide in, then follow health, dizziness and
 * the super meter (three segments of 100, each flashing and chiming when it fills) */
void Combat_UpdateBars(void) {
    s32 d1, d2, v, old, i;

    if (gCombat->v54 < 3) return;
    if (B_800E0CB8 >= 121) {
        B_800E0CB8 -= 8;
        if (B_800E0CB8 < 120) B_800E0CB8 = 120;
    }
    if (B_800E0CB8 != 120) return;

    if ((f64)B_800E0CA4 < 1.0) {
        B_800E0CA4 = (f32)((f64)B_800E0CA4 + 0.1);
        if (1.0 < (f64)B_800E0CA4) B_800E0CA4 = 1.0f;
    }
    B_800E0CA8 = Approach10(B_800E0CA8, gCombatFighter1->health);
    if (gCombatFighter1->health == 170 && gCombatFighter1->v2C8 > 0) B_800E0CB0 = 1;
    B_800E0CAC = Approach10(B_800E0CAC, gCombatFighter2->health);
    if (gCombatFighter2->health == 170 && gCombatFighter2->v2C8 > 0) B_800E0CB4 = 1;

    d1 = 200 - gCombatFighter1->v2BC;
    d2 = 200 - gCombatFighter2->v2BC;
    if (B_800E0CC0 < d1) B_800E0CC0 = (d1 - B_800E0CC0 < 10) ? d1 : B_800E0CC0 + 10;
    if (d1 < B_800E0CC0) B_800E0CC0 = (B_800E0CC0 - d1 < 10) ? d1 : B_800E0CC0 - 10;
    if (B_800E0CC4 < d2) B_800E0CC4 = (d2 - B_800E0CC4 < 10) ? d2 : B_800E0CC4 + 10;
    if (d2 < B_800E0CC4) B_800E0CC4 = (B_800E0CC4 - d2 < 10) ? d2 : B_800E0CC4 - 10;

    /* super meters: going up shows at once, going down 10 a frame */
    if (B_800E0CC8 != gCombatFighter1->v2CC) {
        if (B_800E0CC8 < gCombatFighter1->v2CC) B_800E0CC8 = gCombatFighter1->v2CC;
        else B_800E0CC8 = (B_800E0CC8 - gCombatFighter1->v2CC < 11) ? gCombatFighter1->v2CC : B_800E0CC8 - 10;
    }
    v = B_800E0CC8;
    for (i = 0; i < 3; i++) {
        HudMeterSeg *s = &B_800F5AF0[i];
        if ((f64)s->flash < 1.0) {
            s->flash = (f32)((f64)s->flash + 0.045);
            if (1.0 < (f64)s->flash) s->flash = 1.0f;
        }
        old = s->fill;
        s->fill = (v < 100) ? v : 100;
        if (old < 100 && !(s->fill < 100)) {
            s->flash = 0.0f;
            func_800797EC(B_800F8768, 0, 0, 0x16, 0x7FFF, 0, 1, 1.0f, 1);
        }
        v -= 100;
        if (v < 0) v = 0;
    }
    if (B_800E0CCC != gCombatFighter2->v2CC) {
        if (B_800E0CCC < gCombatFighter2->v2CC) B_800E0CCC = gCombatFighter2->v2CC;
        else B_800E0CCC = (B_800E0CCC - gCombatFighter2->v2CC < 11) ? gCombatFighter2->v2CC : B_800E0CCC - 10;
    }
    v = B_800E0CCC;
    for (i = 0; i < 3; i++) {
        HudMeterSeg *s = &B_800F88F8[i];
        if ((f64)s->flash < 1.0) {
            s->flash = (f32)((f64)s->flash + 0.045);
            if (1.0 < (f64)s->flash) s->flash = 1.0f;
        }
        old = s->fill;
        s->fill = (v < 100) ? v : 100;
        if (old < 100 && !(s->fill < 100)) s->flash = 0.0f;
        v -= 100;
        if (v < 0) v = 0;
    }
    B_800F4E30 += 4000;
    B_800F7A7C = vsSin(B_800F4E30);
}

static void BannerStyle(ComboBanner *b, s32 v53C, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2, s32 v550, f32 v554) {
    b->v53C = v53C;
    b->rgb1[0] = r1; b->rgb1[1] = g1; b->rgb1[2] = b1;
    b->rgb2[0] = r2; b->rgb2[1] = g2; b->rgb2[2] = b2;
    b->v548 = 1.0f;
    b->v54C = 0;
    b->v550 = v550;
    b->v554 = v554;
}

/* 0x800465A0: "<n> hits" and a name for the combo, with an announcer line (never the same as
 * either of the last two) */
void ComboBanner_ShowHits(ComboBanner *b, s32 hits) {
    char buf[32];
    s32 sound, s;

    if (D_800ACCFC == NULL) return;
    HudText_Set(&b->line1, D_800ACCFC, "???", 0);
    mpSPrintf(buf, "%d hits", hits);
    HudText_Set(&b->line2, D_800ACCFC, buf, 0);
    b->t = 0.0f;
    b->state = 0;
    b->alpha = 1.0f;
    b->scale = 1.0f;
    b->active = 1;
    b->v4 = 0;
    b->v538 = 0;
    BannerStyle(b, 600, 0, 0, 220, 120, 120, 200, 4000, 0.1f);
    b->v558 = 0;
    if (hits < 4) {
        HudText_Set(&b->line1, D_800ACCFC, "Itty Bitty Combo", 0);
        BannerStyle(b, 600, 100, 100, 0, 120, 120, 120, 4000, 0.0f);
        sound = 14;
    } else if (hits < 5) {
        HudText_Set(&b->line1, D_800ACCFC, "Lousy Combo", 0);
        BannerStyle(b, 600, 0, 0, 220, 120, 120, 200, 4000, 0.1f);
        sound = 15;
    } else if (hits < 6) {
        HudText_Set(&b->line1, D_800ACCFC, "Lame Combo", 0);
        BannerStyle(b, 0, 0, 120, 0, 0, 30, 0, 4000, 0.1f);
        sound = 16;
    } else if (hits < 7) {
        HudText_Set(&b->line1, D_800ACCFC, "Weak Combo", 0);
        BannerStyle(b, 600, 0, 0, 255, 130, 130, 200, 4000, 0.1f);
        sound = 17;
    } else if (hits < 8) {
        HudText_Set(&b->line1, D_800ACCFC, "Dumb Combo", 0);
        BannerStyle(b, 600, 0, 190, 0, 100, 100, 0, 4000, 0.1f);
        sound = 18;
    } else if (hits < 9) {
        HudText_Set(&b->line1, D_800ACCFC, "Little Girlie Combo", 0);
        BannerStyle(b, 600, 255, 150, 220, 120, 120, 0, 4000, 0.2f);
        sound = 19;
    } else if (hits < 13) {
        HudText_Set(&b->line1, D_800ACCFC, "Cheesy Combo", 0);
        BannerStyle(b, 250, 255, 150, 0, 230, 230, 0, 3000, 0.15f);
        sound = 20;
    } else if (hits < 16) {
        HudText_Set(&b->line1, D_800ACCFC, "Triple Brown Betty", 0);
        BannerStyle(b, 600, 170, 120, 0, 120, 120, 0, 4000, 0.1f);
        sound = 21;
    } else if (hits < 19) {
        HudText_Set(&b->line1, D_800ACCFC, "Queen Combo", 0);
        BannerStyle(b, 600, 250, 0, 250, 250, 200, 0, 4000, 0.1f);
        sound = 22;
    } else if (hits < 30) {
        HudText_Set(&b->line1, D_800ACCFC, "Ludicrous Combo", 0);
        BannerStyle(b, 600, 255, 0, 0, 0, 0, 255, 4000, 0.1f);
        sound = 23;
    } else {
        HudText_Set(&b->line1, D_800ACCFC, "Insane Combo!", 0);
        BannerStyle(b, 1200, 255, 0, 0, 0, 0, 255, 8000, 0.2f);
        sound = 24;
    }
    do {
        s = sound;
        if ((s32)guRandom() % 1024 < 256) s = (s32)guRandom() % 5 + 9;
    } while (s == D_800ACD18 || s == D_800ACD1C);
    D_800ACD1C = D_800ACD18;
    D_800ACD18 = s;
    func_800797EC(B_800F1330, 0, 0, s, 0x7FFF, 0, 0, 1.0f, 1);
}

/* ---------------------------------------------------------------- set-up and tear-down */

/* a character's portrait, moved into a locked handle; returns the handle */
static u32 LoadPortrait(s32 slot, s32 costume, u8 **locked) {
    u8 *p = LoadCharacterPortrait(slot, costume);
    u32 h = mpLinkedMemoryManager_AllocHndl(&gN64MemoryManager, *(u32 *)(p + 0x10), 0);
    *locked = mpLinkedMemoryManager_LockHndl(&gN64MemoryManager, h);
    bcopy(p + *(s32 *)(p + 0xC), *locked, mpLinkedMemoryManager_SizeOfHndl(&gN64MemoryManager, h));
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, p);
    return h;
}

/* load a font file; returns the font inside it */
static u8 *LoadFont(u32 romAddr, u8 **file) {
    *file = func_80080360(romAddr);
    return *file + *(s32 *)(*file + 0xC);
}

static void WaitRetrace(void) {
    u32 a = D_800B09B0, b = D_800B09B4;
    while (D_800B09B0 == a && D_800B09B4 == b) {
        PORT_SPIN();
    }
}

static void FreeFont(u8 **file, u8 **font) {
    if (*file != NULL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, *file);
        *file = NULL;
        *font = NULL;
    }
}

/* 0x8003208C: load everything, run the fight (func_80032FE8 draws and loops until it ends),
 * free everything. Returns 2 when a fight was skipped with Start, else 1 for a normal end and
 * 0 when it was ended early (time limit, or the handler asking for it). */
s32 Combat_Main(GameSettings *g) {
    u8 *p;
    s32 i, n;
    u16 *b;
    static const u16 defaults[6] = { 0x4000, 0x0002, 0x0008, 0x8000, 0x0004, 0x0001 };

    func_8007CCA4();
    mpLinkedMemoryManager_Pack(&gN64MemoryManager);
    gGame = g;
    func_8007C5BC(0x2800, 0xC0, 0xFA0);
    if (gGame->v58 != 0) CharMenuSound_Load(D_800AB220[gGame->v1C]);
    func_80087F38(B_800E0D00, 0x38E);
    func_80088224(B_800E0D00, 0x1C71);
    func_80087F38(B_800E0D40, 0x38E);
    func_80088224(B_800E0D40, -0x1C71);
    func_80087F38(B_800E0D80, 0x2000);
    func_80088224(B_800E0D80, 0x2000);
    func_80087F38(B_800E0DC0, 0x2000);
    func_80088224(B_800E0DC0, -0x2000);
    B_800E0CEC = -1;
    B_800E0CF0 = -1;
    B_800E0CF4 = -1;
    B_800E0CF8 = 0;
    B_800F84C8 = 0;
    B_800F5BE8 = 0;
    B_800F8C7C = 0;
    B_800F7D98 = 0;
    B_800F4E8C = 1;
    B_800F7E60 = 0;
    B_800F7E74 = B_800F7D54;
    B_800F7EC4 = 0.0f;
    B_800F7E64 = 0;

    /* small bitmaps, shown at half size */
    B_800F5D18 = func_80080360(0xE20D00);
    for (i = 0; i < *(s32 *)(B_800F5D18 + 4); i++) {
        u8 *e = B_800F5D18 + *(s32 *)(B_800F5D18 + 0xC + i * 8);
        *(s16 *)(e + 0xA) = *(s16 *)(e + 6) / 2;
        *(s16 *)(e + 0xC) = *(s16 *)(e + 8) / 2;
    }
    D_800ACCF4 = LoadFont(0xF54510, &D_800ACCF0);
    D_800ACCFC = LoadFont(0xF4D8B0, &D_800ACCF8);
    D_800ACD04 = LoadFont(0xF56270, &D_800ACD00);
    B_800DCAA4 = 10;
    D_800ACD0C = 0;
    B_800F7F60.active = 0;
    B_800F1600.active = 0;
    D_800ACD10 = 0;
    D_800ACD14 = 0;
    B_800F8C8C = 0;
    n = 0xD12E0 - 0xD10A0;
    B_800F5CE0 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, n, 0);
    RomCopy(B_800F5CE0, 0xD10A0, n);

    /* the fighters and the combat handler */
    gCombatFighter1 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(cfCharacter), 0);
    cfCharacter_Construct(gCombatFighter1);
    gCombatFighter2 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(cfCharacter), 0);
    cfCharacter_Construct(gCombatFighter2);
    gCombat = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(CombatHandler), 0);
    cfCombatHandler_Init(gCombat);
    B_800DCA88 = gGame->v0;
    B_800DCA90 = gGame->vC;
    B_800DCA8C = gGame->v4;
    B_800DCA94 = gGame->v10;
    gCombatChar1 = LoadCharacter(gGame->v0, gGame->v4);
    gCombatChar2 = LoadCharacter(B_800DCA90, B_800DCA94);
    cfCharacter_Init(gCombatFighter1, (CharDef *)gCombatChar1->moves);
    cfCharacter_Init(gCombatFighter2, (CharDef *)gCombatChar2->moves);
    gCombatFighter1->v260 = gGame->v8 >= 0;
    gCombatFighter2->v260 = gGame->v14 >= 0;
    if (gGame->v8 >= 0) gCombatFighter1->v264 = gGame->v8;
    if (gGame->v14 >= 0) gCombatFighter2->v264 = gGame->v14;
    b = gCombatFighter1->buttonMap;
    for (i = 0; i < 6; i++) b[i] = gGame->buttons1[i] ? gGame->buttons1[i] : defaults[i];
    b = gCombatFighter2->buttonMap;
    for (i = 0; i < 6; i++) b[i] = gGame->buttons2[i] ? gGame->buttons2[i] : defaults[i];
    if (gGame->v30 != 0) {
        gCombatFighter1->health = 1;
        gCombatFighter1->v2C8 = 1;
    }
    mpLinkedMemoryManager_Pack(&gN64MemoryManager);
    B_800F88B8 = LoadPortrait(g->v0, g->v4, (u8 **)&B_800F7D54);
    B_800F2E20 = LoadPortrait(g->vC, g->v10, (u8 **)&B_800F8CA8);
    B_800F2D24 = p = func_80080360(0xF9BB20);
    B_800F2D48 = p + *(s32 *)(p + 0xC);
    *(s16 *)(B_800F2D48 + 0xA) = 24;
    *(s16 *)(B_800F2D48 + 0xC) = 35;
    B_800F5B70 = p + *(s32 *)(p + 0x14);
    *(s16 *)(B_800F5B70 + 0xA) = 25;
    *(s16 *)(B_800F5B70 + 0xC) = 50;

    /* the arena */
    B_800F88E0 = gGame->v1C;
    func_8000AF40(B_800F88E0);
    func_80087E20(gCamera);
    func_80088184(gCamera, 0.0f, 25.0f, 0.0f);
    B_800F4E80 = gGame->v20;
    if (!(B_800F4E80 < *(s32 *)(D_800AB49C + 0x38))) B_800F4E80 = *(s32 *)(D_800AB49C + 0x38) - 1;
    D_800AB4AC = B_800F4E80;
    cfCombatHandler_Start(gCombat, gCombatFighter1, gCombatFighter2, D_800AB49C, B_800F4E80, gGame->v4C, 1);
    gCombat->v34 = (f32)gGame->v24;
    gCombat->v74 = gGame->v2C;
    if (gCombatSolo != 0 || gGame->v5C == 0) gCombat->v44 = 0;
    else gCombat->v44 = gGame->v18;
    gCombat->v48 = gGame->v28;
    if (gCombatArcade != 0) {
        gCombatFighter1->v260 = 1;
        gCombatFighter1->v264 = 4;
        gCombatFighter2->v260 = 1;
        gCombatFighter2->v264 = 1;
        gCombat->v34 = -1.0f;
        if (B_800E0E38 != 0) ((void (*)(s32))B_800E0E38)(0);
    }
    if (gCombatSolo != 0) {
        gCombatFighter1->v260 = 1;
        gCombatFighter1->v264 = 4;
        gCombatFighter1->v258 = 1;
        gCombatFighter2->v260 = 1;
        gCombatFighter2->v264 = 0;
        gCombatFighter2->v258 = 1;
        gCombatFighter2->v25C = 1;
        gCombat->v34 = -1.0f;
        if (B_800E0E44 != 0) ((void (*)(s32))B_800E0E44)(0);
    }
    B_800F5CDC = *(f32 *)gCombat->arena;
    B_800F5CCC[0] = gCombat->camEye[0];
    B_800F5CCC[1] = gCombat->camEye[1];
    B_800F5CCC[2] = gCombat->camEye[2];
    B_800F7A70[0] = gCombat->camAt[0];
    B_800F7A70[1] = gCombat->camAt[1];
    B_800F7A70[2] = gCombat->camAt[2];

    /* the screen display */
    B_800F7ED0 = 0;
    B_800E0CB8 = 120;
    B_800E0CBC = 20;
    B_800E0CA4 = 0.0f;
    B_800E0CA8 = 0;
    B_800E0CAC = 0;
    B_800E0CB0 = 0;
    B_800E0CB4 = 0;
    B_800E0CC0 = 0;
    B_800E0CC4 = 0;
    B_800E0CC8 = 0;
    B_800E0CCC = 0;
    bzero(B_800F5AF0, 0x54);
    bzero(B_800F88F8, 0x54);
    for (i = 0; i < 3; i++) {
        B_800F5AF0[i].flash = 1.0f;
        B_800F88F8[i].flash = 1.0f;
    }
    B_800F4E30 = 0;
    B_800F7A7C = 0.0f;
    B_800E0CFC = 0;
    B_800E0E00.active = 0;
    B_800E0E00.v4 = 0;
    B_800E0E00.type = 0;
    B_800E0E00.level = 1.0f;
    B_800E0E00.speed = 1.0f;
    B_800E0E00.rgb[0] = 0;
    B_800E0E00.rgb[1] = 0;
    B_800E0E00.rgb[2] = 0;
    ((s32 *)&B_800E0E00)[6] = 0;
    ((s32 *)&B_800E0E00)[7] = 0;
    bzero(B_800E0E50, 0x1E0);
    bzero(B_800E1030, 0x1E0);
    B_800E0E20 = 0;
    B_800E0E24 = 0;
    B_800E0E28 = 0;
    B_800E0E2C = 0;
    B_800F7A80 = 0;
    B_800F4E38 = 0;
    B_800F4E64 = -1;
    B_800F1344 = 0;
    B_800F7EC0 = 0;
    B_800F88D8 = 0;
    B_800F1338 = NULL;
    B_800F15D8 = 0;
    B_800E1210 = 0;
    B_800E1214 = 0;
    mpMutex_Init(&B_800E0C80);
    func_8002A340(&gScreenFade, gScreenFade.type);
    if (D_800ACD08 == 0) {
        osCreateMesgQueue(&B_800DCAB0, &B_800DCAA8, 1);
        func_800999D0(B_800DCAC8, n64NextThreadId(), Combat_LogicThread, NULL, &B_800E0C80, 20);
        func_8009A0D0(B_800DCAC8);
        D_800ACD08 = 1;
    }

    /* the fight */
    func_800779F4(&B_800DCAB0);
    Combat_DrawLoop();
    func_8007CCA4();
    func_80077AF4(&B_800DCAB0);

    if (B_800F7EC0 != 0) {
        func_8005C060();
        B_800F7EC0 = 0;
    }
    if (gCombatArcade != 0 && B_800E0E38 != 0) ((void (*)(s32))B_800E0E38)(-1);
    if (gCombatSolo != 0 && B_800E0E44 != 0) ((void (*)(s32))B_800E0E44)(-1);
    func_8002AC24(&gScreenFade);
    func_8002AC24(&B_800E0E00);
    CharFrameCache_FreeAll();
    WaitRetrace();
    func_8007A530();
    WaitRetrace();
    WaitRetrace();
    FreeCharacter(gCombatChar1);
    FreeCharacter(gCombatChar2);
    cfCharacter_Destruct(gCombatFighter1);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCombatFighter1);
    cfCharacter_Destruct(gCombatFighter2);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCombatFighter2);
    func_8006B6E4(gCombat);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gCombat);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800F5D18);
    mpLinkedMemoryManager_FreeHndl(&gN64MemoryManager, B_800F88B8);
    mpLinkedMemoryManager_FreeHndl(&gN64MemoryManager, B_800F2E20);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800F2D24);
    func_8002AC24(&B_800E0E00);
    func_8000B704();
    CharMenuSound_Free();
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800F5CE0);
    FreeFont(&D_800ACCF0, &D_800ACCF4);
    FreeFont(&D_800ACCF8, (u8 **)&D_800ACCFC);
    FreeFont(&D_800ACD00, &D_800ACD04);
    gGame->v78 = gCombat->v5C;
    gGame->v7C = B_800F88E0;
    gGame->v80 = B_800F4E80;
    if (B_800E0E24 != 0 && gCombatSolo == 0 && gCombatArcade == 0) return 2;
    return B_800E0E20 == 0;
}

/* ---------------------------------------------------------------- the logic thread */

static void Lock(MPF_Mutex *m) {
    if (m->owner == osGetThreadId(NULL)) {
        m->count++;
    } else {
        osRecvMesg(&m->mq, NULL, 1);
        m->owner = osGetThreadId(NULL);
        m->count++;
    }
}

static void Unlock(MPF_Mutex *m) {
    if (m->owner == osGetThreadId(NULL)) {
        if (--m->count == 0) {
            m->owner = 0;
            osSendMesg(&m->mq, NULL, 0);
        }
    }
}

/* a fade has finished (or none is running) */
static s32 FadeIdle(ScreenFade *f) {
    if ((0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0)) return 1;
    return f->active == 0;
}

static void PlayCombatSound(s32 sound) {
    func_800797EC(B_800F1330, 0, 0, sound, 0x7FFF, 0, 1, 1.0f, 1);
}

/* the pause menu opens for player 'who' */
static void OpenPause(s32 who) {
    B_800F4E7C = 0;
    B_800F7A80 = 1;
    B_800F5B68 = 0;
    B_800F7774 = who;
}

/* pre-fight taunt pages: which player's text and voice to show */
static void TauntPage(s32 a, s32 b, s32 c, s32 first) {
    s32 p1 = (gGame->v18 == 1) == first;
    B_800F5BE8 = a;
    B_800F8C7C = b;
    B_800F7D98 = c;
    B_800F4E8C = p1 ? 2 : 1;
    B_800F7E60 = p1 ? gGame->vC : gGame->v0;
    B_800F7E74 = p1 ? B_800F8CA8 : B_800F7D54;
}

/* round title zooming in (state 0 of the "Fight!" and "Claytality" texts) */
static void TitleZoomIn(void *m, f32 *t, s32 *state, s32 sound) {
    f32 s = *t * *t;
    s = s * s;
    func_80087E20(m);
    func_80088134(m, s, s, s);
    func_80088274(m, (s32)((1.0 - (f64)*t) * 4000.0));
    func_80088184(m, 0.0f, 20.0f, 0.0f);
    *t = (f32)((f64)*t + 0.05);
    if (1.0 < (f64)*t) {
        (*state)++;
        *t = 0.0f;
        func_800797EC(B_800F1330, 0, 0, sound, 0x7FFF, 0, 1, 1.0f, 1);
    }
}

/* ... wobbling, then picking a direction for each letter to fly off in */
static s32 TitleWobble(void *m, f32 *t, s32 *state, HudText *text) {
    f32 s = (f32)((f64)vsSin((s32)(*t * 32768.0f)) * 0.1 + 1.0);
    s32 a, i;
    func_80087E20(m);
    func_80088134(m, s, s, s);
    func_80088274(m, (s32)((1.0 - (f64)*t) * (f64)((s32)guRandom() % 2000 - 1000)));
    func_80088184(m, 0.0f, 20.0f, 0.0f);
    *t = (f32)((f64)*t + 0.03);
    if (!(1.0 < (f64)*t)) return 0;
    (*state)++;
    *t = 0.0f;
    a = ((s32)guRandom() % 16384) * 4;
    for (i = 0; i < text->len; i++) {
        text->dir[i][0] = vsSin(a);
        text->dir[i][1] = vsCos(a);
        a += 0xAB35;
    }
    return 1;
}

/* "You Win!" / "You Lose": the letters of each word slide in from above and below */
static void WinTextSlide(void) {
    s32 i;
    for (i = 0; i < B_800F5D30.len; i++) {
        B_800F5D30.y[i] = (f32)((1.0 - (f64)B_800F15F0) * (i < 4 ? -300.0 : 300.0));
    }
    func_80087E98(B_800F2D50, 0.7f, 0.7f, 0.7f);
    func_80088184(B_800F2D50, 0.0f, 30.0f, 0.0f);
    B_800F15F0 = (f32)((f64)B_800F15F0 + 0.04);
    if (1.0 < (f64)B_800F15F0) {
        for (i = 0; i < B_800F5D30.len; i++) B_800F5D30.y[i] = 0.0f;
        B_800F5B5C++;
        B_800F15F0 = 0.0f;
    }
}

/* 0x800349F4: the combat logic thread - one pass a frame: controllers, the pause menu, the
 * combat handler (fighters, hits, camera, rounds), the bars, the round / "Fight!" /
 * "Claytality" / "You Win!" texts, combos, fades and sounds */
void Combat_LogicThread(void *arg) {
    (void)arg;
    for (;;) {
        osRecvMesg(&B_800DCAB0, NULL, 1);
        Lock(&B_800E0C80);

        if (gGame->v50 == 0) {
            if (0.0 < (f64)gCombatFighter1->v2B8) gCombatFighter1->v2B8 = 9999.0f;
            if (0.0 < (f64)gCombatFighter2->v2B8) gCombatFighter2->v2B8 = 9999.0f;
        }
        if (gCombat->v58 != 0 && B_800E0CE8 == 0) {
            Combat_FadeOut();
            B_800E0E20 = 1;
        }
        if (gCombatArcade != 0 && gCombat->v54 >= 4) {
            if (--gCombatTimeLimit <= 0 && B_800E0CE8 == 0) {
                Combat_FadeOut();
                B_800E0E20 = 1;
            }
        }
        if (B_800F7EC0 != 0 && B_800F7A80 == 0) {
            func_8005C0A8();
            B_800F88D8++;
            B_800F2E1C += 0x200;
        }
        func_80076DB8();
        B_800F2D18 |= B_800F8C88;
        B_800F4E54 |= B_800F5BE4;
        B_800F8C84 |= B_800F15E8;
        B_800F4E6C |= B_800F4E60;
        B_800F77D8 |= B_800F7F50;
        B_800F8C78 |= B_800F15DC;

        if (gCombat->v54 == 2) {
            /* the pre-fight taunts, up to four pages */
            if (gGame->v84 == 0 && gGame->v88 == 0 && gGame->v8C == 0) {
                gCombat->v7C = 1;
                goto taunts_done;
            }
            B_800F7EC4 = (f32)((f64)B_800F7EC4 + 0.02);
            if (1.0 < (f64)B_800F7EC4) B_800F7EC4 = 1.0f;
            if (1.0 <= (f64)B_800F7EC4) {
                if ((gGame->v8 < 0 && (B_800F4E54 & 0x9000)) || (gGame->v14 < 0 && (B_800F4E6C & 0x9000)) ||
                    ++B_800F7E64 >= 601) {
                    if (B_800F84C8 >= 0) {
                        B_800F7E64 = 0;
                        if (++B_800F84C8 >= 4) {
                            B_800F84C8 = -1;
                            B_800F5BE8 = 0;
                            B_800F8C7C = 0;
                            B_800F7D98 = 0;
                            gCombat->v7C = 1;
                        }
                    }
                }
                switch (B_800F84C8) {
                case 0: TauntPage(gGame->v84, gGame->v88, gGame->v8C, 1); break;
                case 1: TauntPage(gGame->v90, gGame->v94, gGame->v98, 0); break;
                case 2: TauntPage(gGame->v9C, gGame->vA0, gGame->vA4, 1); break;
                case 3: TauntPage(gGame->vA8, gGame->vAC, gGame->vB0, 0); break;
                }
            }
            B_800F2D18 = 0;
            B_800F4E54 = 0;
            B_800F8C84 = 0;
            B_800F4E6C = 0;
            B_800F77D8 = 0;
            B_800F8C78 = 0;
        }
taunts_done:
        if (gCombat->v54 == 0 && (B_800F8C78 & 0xC000) && gCombatArcade == 0 && gCombatSolo == 0) {
            gCombat->v108 = 1;          /* skip the intro: fly in twice as fast */
        }
        if (gCombatArcade != 0 || gCombatSolo != 0) {
            if (B_800F77D8 & 0xD000) Combat_FadeOut();
        } else if (B_800F7A80 != 0) {
            /* the pause menu: quit? yes / no */
            s32 btn = (B_800F7774 == 1) ? B_800F4E54 : B_800F4E6C;
            if (btn & 0x9000) {
                if (B_800F5B68 == 0) {
                    if (B_800F4E7C == 1) {
                        B_800F5B68 = 1;
                        B_800F4E7C = 1;
                    } else {
                        B_800F7A80 = 0;
                    }
                } else if (B_800F4E7C == 0) {
                    B_800F7A80 = 0;
                    Combat_FadeOut();
                } else {
                    B_800F7A80 = 0;
                }
            }
            if (btn & 0x300) B_800F4E7C = (B_800F4E7C == 0);
            if (btn & 0x4000) B_800F7A80 = 0;
        } else {
            /* Start: skip to the end (a human player) or pause */
            if (gGame->v8 >= 0 && D_800AA164 == 0 && D_800AA168 == 0) {
                if ((B_800F4E54 & 0x1000) && B_800E0CE8 == 0 && gCombat->v54 < 5 && FadeIdle(&gScreenFade) &&
                    B_800E0E24 == 0) {
                    B_800E0E24 = 1;
                    B_800E0E28 = 0;
                }
            } else if ((B_800F4E54 & 0x1000) && B_800E0CE8 == 0) {
                if (gCombatArcade != 0 || gCombatSolo != 0) Combat_FadeOut();
                else if (FadeIdle(&gScreenFade) && B_800E0CE8 == 0) OpenPause(1);
            }
            if (gGame->v14 >= 0 && D_800AA164 == 0 && D_800AA168 == 0) {
                if ((B_800F4E6C & 0x1000) && B_800E0CE8 == 0 && gCombat->v54 < 5 && FadeIdle(&gScreenFade) &&
                    B_800E0E24 == 0) {
                    B_800E0E24 = 1;
                    B_800E0E28 = 0;
                }
            } else if ((B_800F4E6C & 0x1000) && B_800E0CE8 == 0) {
                if (gCombatArcade != 0 || gCombatSolo != 0) Combat_FadeOut();
                else if (FadeIdle(&gScreenFade) && B_800E0CE8 == 0) OpenPause(2);
            }
        }
        if (B_800E0E24 != 0 && ++B_800E0E28 >= 121) Combat_FadeOut();

        if (B_800F7A80 == 0) {
            if (B_800E0CE8 == 0) cfCombatHandler_ReadInput(gCombat, B_800F2D18, B_800F8C84);
            if (B_800E0CF8 == 0) {
                if (gCombatSolo != 0) gCombat->vB8 = 1;
                if (B_800F7EC0 == 0 && B_800E0E24 == 0) cfCombatHandler_Run(gCombat);
                func_8000B894(gCombat);
                if (gCombat->v148 != 0) {
                    B_800E0CFC += 18;
                    func_800797EC(B_800F8768, 0, 0, 11, 0x7FFF, 0, 1, 1.0f, 1);
                    gCombat->v148 = 0;
                }
                if (gCombat->v14C != 0) {
                    B_800E0CFC += 20;
                    func_800797EC(B_800F8768, 0, 0, 11, 0x7FFF, 0, 1, 1.0f, 1);
                    gCombat->v14C = 0;
                }
                if (gCombat->v144 != 0) {
                    func_8000A71C(B_800F88E0, B_800F4E80, gCombat->v140, &B_800E0CEC, &B_800E0CF0, &B_800E0CF4);
                    B_800E0CF8 = 1;
                    gCombat->v144 = 0;
                }
                if (0.0 < (f64)gCombat->shake) {
                    if (B_800F5BD8 != 0) B_800E0CFC = (s32)((f64)B_800E0CFC + (f64)gCombat->shake * 10.0);
                    gCombat->shake = 0.0f;
                }
                /* impact flashes */
                if ((f64)gCombat->v2C70 < 1.0) {
                    B_800F5B84 = gCombat->v2C78 ? gCombat->v2C74 : 1.0f;
                    B_800F7780 = gCombat->v2C7C ? gCombat->v2C74 : 1.0f;
                    B_800F4E90 = gCombat->v2C80 ? gCombat->v2C74 : 1.0f;
                } else {
                    B_800F5B84 = 1.0f;
                    B_800F7780 = 1.0f;
                    B_800F4E90 = 1.0f;
                }
                if ((f64)gCombat->v2C8C < 1.0) {
                    B_800F5B84 = (f32)((f64)B_800F5B84 * ((f64)gCombat->v2C8C + 0.5));
                    if (1.0 < (f64)B_800F5B84) B_800F5B84 = 1.0f;
                    B_800F7780 = B_800F7780 * gCombat->v2C8C;
                    B_800F4E90 = B_800F4E90 * gCombat->v2C8C;
                }
                Combat_UpdateBars();
                if (B_800F7ED0 != 0) Combat_DebugCamera();
                else Combat_SetCamera();
                if (gGame->v54 != 0) {
                    /* debug keys */
                    if (B_800F7D5C & 1) B_800F7ED0 = (B_800F7ED0 == 0);
                    if (B_800F88EC >= 3) {
                        if (B_800F7D5C & 2) gCombat->vB8 = 2;
                        if (B_800F7D5C & 4) gCombat->vB8 = 7;
                        if (B_800F7D5C & 8) gCombat->vB8 = 0;
                    }
                }
                if (B_800E0CFC > 0) B_800E0CFC--;
                if (B_800E1210 == 0 && gCombat->v54 == 0 && 0.5 < (f64)gCombat->v100 && gCombat->v108 == 0 &&
                    gCombatSolo == 0) {
                    PlayCombatSound(1);
                    B_800E1210 = 1;
                }

                /* "Fight!" */
                if (D_800ACD0C == 0 && gCombat->v54 == 3) {
                    if (gCombatSolo != 0) {
                        gCombat->v54 = 4;
                    } else {
                        D_800ACD0C = 1;
                        HudText_Set(&B_800F89E8, D_800ACCFC, "Fight!", 0);
                        func_80087EE8(B_800F7E80, 0.0f, 0.0f, 0.0f);
                        B_800F5BA8 = 0;
                        B_800F5BE0 = 0.0f;
                    }
                }
                if (D_800ACD0C != 0) {
                    switch (B_800F5BA8) {
                    case 0:
                        TitleZoomIn(B_800F7E80, &B_800F5BE0, &B_800F5BA8, 2);
                        break;
                    case 1:
                        if (TitleWobble(B_800F7E80, &B_800F5BE0, &B_800F5BA8, &B_800F89E8)) {
                            if (gCombat->v54 == 3) gCombat->v54 = 4;
                        }
                        break;
                    case 2: {
                        /* the letters fly apart */
                        f32 s = B_800F5BE0 * B_800F5BE0;
                        f64 d;
                        s32 i;
                        s = (f32)((f64)s + 1.0);
                        func_80087E20(B_800F7E80);
                        func_80088134(B_800F7E80, s, s, s);
                        func_80088184(B_800F7E80, 0.0f, 20.0f, 0.0f);
                        d = (f64)B_800F5BE0 + 3.0;
                        for (i = 0; i < B_800F89E8.len; i++) {
                            B_800F89E8.x[i] = (f32)((f64)B_800F89E8.x[i] + (f64)B_800F89E8.dir[i][0] * d);
                            B_800F89E8.y[i] = (f32)((f64)B_800F89E8.y[i] + (f64)B_800F89E8.dir[i][1] * d);
                            B_800F89E8.spin[i] += 4000;
                        }
                        B_800F5BE0 = (f32)((f64)B_800F5BE0 + 0.02);
                        if (1.0 < (f64)B_800F5BE0) {
                            B_800F5BA8++;
                            B_800F5BE0 = 0.0f;
                        }
                        break;
                    }
                    case 3:
                        D_800ACD0C = 0;
                        break;
                    }
                }

                /* a ticking sound when the timer is reset */
                if (gCombat->v78 != 0) {
                    B_800F4E64 = 0;
                    B_800F1344 = 0;
                    gCombat->v78 = 0;
                }
                if (B_800F4E64 >= 0 && --B_800F1344 <= 0) {
                    f32 vol;
                    B_800F1344 = 3;
                    if (B_800F4E64 >= 21) {
                        vol = 0.6f;
                    } else {
                        s32 d = 10 - B_800F4E64;
                        if (d < 0) d = B_800F4E64 - 10;
                        vol = (f32)((f64)(f32)d / 10.0);
                        vol = (f32)((f64)vol * 0.4);
                        vol = (f32)(1.0 - (f64)vol);
                    }
                    func_800797EC(B_800F4E74, 0, 10, 0, (s32)(vol * 32767.0f), 0, 1, 1.0f, 0);
                    B_800F4E64++;
                }

                /* "Claytality" */
                if (D_800ACD10 == 0 && B_800F7EC0 == 0 && gCombat->v54 == 8) {
                    D_800ACD10 = 1;
                    HudText_Set(&B_800F77E0, D_800ACCFC, "Claytality", 0);
                    func_80087EE8(B_800F7E10, 0.0f, 0.0f, 0.0f);
                    B_800F7D20 = 0;
                    B_800F5D14 = 0.0f;
                    B_800F4E38 = 1;
                    func_800797EC(B_800F8768, 0, 0, 0x1A, 0x7FFF, 0, 1, 1.0f, 1);
                    B_800F4E64 = -1;
                }
                if (D_800ACD10 != 0) {
                    switch (B_800F7D20) {
                    case 0:
                        TitleZoomIn(B_800F7E10, &B_800F5D14, &B_800F7D20, 3);
                        break;
                    case 1:
                        TitleWobble(B_800F7E10, &B_800F5D14, &B_800F7D20, &B_800F77E0);
                        break;
                    case 2:
                        B_800F5D14 = (f32)((f64)B_800F5D14 + 0.004);
                        if (1.0 < (f64)B_800F5D14) gCombat->v6C = 1;
                        break;
                    }
                }

                /* "You Win!" / "You Lose" */
                if (D_800ACD14 == 0) {
                    if (gCombat->v2CB4 != 0) {
                        if (gCombatSolo != 0) {
                            gCombat->v54++;
                        } else {
                            D_800ACD14 = 1;
                            HudText_Set(&B_800F5D30, D_800ACCFC, "You Win!", 0);
                            PlayCombatSound(6);
                            func_80087EE8(B_800F2D50, 0.0f, 0.0f, 0.0f);
                            B_800F5B5C = 0;
                            B_800F15F0 = 0.0f;
                        }
                    }
                    if (gCombat->v2CB8 != 0) {
                        if (gCombatSolo != 0) {
                            gCombat->v54++;
                        } else {
                            D_800ACD14 = 1;
                            HudText_Set(&B_800F5D30, D_800ACCFC, "You Lose", 0);
                            PlayCombatSound(7);
                            func_80087EE8(B_800F2D50, 0.0f, 0.0f, 0.0f);
                            B_800F5B5C = 2;
                            B_800F15F0 = 0.0f;
                        }
                    }
                }
                if (D_800ACD14 != 0 && (B_800F5B5C == 0 || B_800F5B5C == 2)) WinTextSlide();

                /* combos */
                if (gCombat->v2CB0 != 0) {
                    B_800F8C8C++;
                } else {
                    if (gCombatFighter1->v454 > 0) {
                        if (B_800F7F60.active == 0) ComboBanner_ShowHits(&B_800F7F60, gCombatFighter1->v454);
                        gCombatFighter1->v454 = -1;
                    }
                    if (gCombatFighter1->v458 != 0) {
                        if (B_800F7F60.active == 0) ComboBanner_Start(&B_800F7F60);
                        gCombatFighter1->v458 = 0;
                    }
                    if (gCombatFighter2->v454 > 0) {
                        if (B_800F1600.active == 0) ComboBanner_ShowHits(&B_800F1600, gCombatFighter2->v454);
                        gCombatFighter2->v454 = -1;
                    }
                    if (gCombatFighter2->v458 != 0) {
                        if (B_800F1600.active == 0) ComboBanner_Start(&B_800F1600);
                        gCombatFighter2->v458 = 0;
                    }
                }
                ComboBanner_Update(&B_800F7F60);
                ComboBanner_Update(&B_800F1600);
                func_8002AA14(&gScreenFade, 0.04f);
                func_8002AA14(&B_800E0E00, 0.1f);
                if (FadeIdle(&B_800E0E00)) func_8002AC24(&B_800E0E00);
                if (gCombatArcade != 0 && B_800E0E38 != 0) ((void (*)(s32))B_800E0E38)(B_800E0E2C + 1);
                if (gCombatSolo != 0 && B_800E0E44 != 0) {
                    if (((s32 (*)(s32))B_800E0E44)(B_800E0E2C + 1)) Combat_FadeOut();
                }
                B_800E0E2C++;
            }
            if (B_800E1214 == 0 && gCombat->v2CAC != 0 && func_8007A3B8(B_800F1330) == 0) {
                func_800797EC(B_800F1330, 0, 0, 8, 0x7FFF, 0, 0, 1.0f, 1);
                B_800E1214 = 1;
            }
            Combat_PlaySounds();
            if (B_800F4E38 != 0) {
                CharMenuSound_Load(D_800AB23C[gGame->v1C]);
                B_800F4E38 = 0;
            }
        }
        Unlock(&B_800E0C80);
    }
}

/* ---------------------------------------------------------------- the drawing loop */

static void SetTexFilter(u32 mode) {       /* gDPSetTextureFilter: 0 = point, 0x2000 = bilinear */
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xBA000C02;
    g[1] = mode;
}

static void ReloadCameraAfterArenaChange(void) {
    B_800F5CDC = *(f32 *)gCombat->arena;
    B_800F5CCC[0] = gCombat->camEye[0];
    B_800F5CCC[1] = gCombat->camEye[1];
    B_800F5CCC[2] = gCombat->camEye[2];
    B_800F7A70[0] = gCombat->camAt[0];
    B_800F7A70[1] = gCombat->camAt[1];
    B_800F7A70[2] = gCombat->camAt[2];
}

/* one side's taunt panel: frame, portrait, frame top */
static void TauntPanel(f32 x, f32 y, s32 flip, void *portrait, f32 *up, f32 *right) {
    func_80084CB0(&gDL, B_800F5B70, x, y, 0, up, right, flip | 8);
    SetTexFilter(0);
    func_80083A64(&gDL, portrait, x, y, 0, up, right, flip | 8);
    SetTexFilter(0x2000);
    func_80084CB0(&gDL, B_800F2D48, x, y, 0, up, right, flip | 8);
}

/* the letters of a title, each with its own position (and spin) under the title's matrix */
static void DrawTitle(HudText *t, void *mtx, s32 mode, s32 spin, f32 *m2, char *one, f32 *up, f32 *right) {
    s32 i;
    func_80082210(D_800ACCFC, &gDL, mode);
    for (i = 0; i < t->len; i++) {
        if (spin) func_80088068(m2, t->spin[i]);
        else func_80087E20(m2);
        func_80088184(m2, t->x[i], t->y[i], 0.0f);
        func_80088100(m2, mtx);
        func_80080170(&gDL, m2);
        one[0] = t->text[i];
        func_800824CC(D_800ACCFC, &gDL, one, 0.0f, 0.0f, 0.0f, up, right, 8);
    }
}

static void DrawWinText(s32 mode, f32 *m2, char *one, f32 *up, f32 *right) {
    s32 i;
    func_80082210(D_800ACCFC, &gDL, mode);
    for (i = 0; i < B_800F5D30.len; i++) {
        func_80087EE8(m2, B_800F5D30.x[i] + B_800F5D30.y[i], 0.0f, 0.0f);
        func_80088100(m2, B_800F2D50);
        func_80080170(&gDL, m2);
        one[0] = B_800F5D30.text[i];
        func_800824CC(D_800ACCFC, &gDL, one, 0.0f, 0.0f, 0.0f, up, right, 8);
    }
}

/* 0x80032FE8: the drawing loop - one pass a frame until the closing fade has finished:
 * arena changes after a ring-out, the full-screen "launch" picture, the arena and fighters,
 * the bars, the pre-fight taunts, the titles, the banners, the pause menu and the fades */
void Combat_DrawLoop(void) {
    f32 m[16], m2[16];
    f32 up[3], right[3], up2[3], right2[3];
    char one[2];

    B_800E0CE8 = 0;
    for (;;) {
        B_800F4E70 = D_800AB4E8;
        Lock(&B_800E0C80);
        if (gCombat->v54 >= 3 && D_800ACD00 != NULL && --B_800DCAA4 <= 0) {
            /* the taunt font is no longer needed */
            mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACD00);
            D_800ACD00 = NULL;
            D_800ACD04 = NULL;
        }
        if (B_800E0E00.active != 0 && FadeIdle(&B_800E0E00)) func_8002AC24(&B_800E0E00);

        if (B_800E0CEC != -1) {
            /* a ring-out into another arena */
            mpDebugPrintf("breakthrough from %d,%d,%d to %d,%d,%d\n", B_800F88E0, B_800F4E80, gCombat->v140,
                          B_800E0CEC, B_800E0CF0, B_800E0CF4);
            B_800F88E0 = B_800E0CEC;
            B_800F4E80 = B_800E0CF0;
            func_8007CCA4();
            CharFrameCache_FreeAll();
            func_8000B704();
            mpLinkedMemoryManager_Pack(&gN64MemoryManager);
            func_8000AF40(B_800F88E0);
            B_800E0CFC = 0;
            D_800AB4AC = B_800F4E80;
            while (func_80077F10() != 0) {
                PORT_SPIN();
            }
            func_8002A340(&B_800E0E00, 1);
            B_800E0E18 = func_80081804();
            cfCombatHandler_EnterArena(gCombat, D_800AB49C, B_800E0CF0, B_800E0CF4);
            ReloadCameraAfterArenaChange();
            B_800E0CEC = -1;
            B_800E0CF8 = 0;
        }
        if ((gCombatFighter1->v468 != 0 || gCombatFighter2->v468 != 0) && B_800F15D8 == 0) {
            /* a fighter was launched: load the full-screen picture */
            func_8007CCA4();
            CharFrameCache_FreeAll();
            mpLinkedMemoryManager_Pack(&gN64MemoryManager);
            func_8007C5BC(0x1000, 0x40, 0x5DC);
            mpLinkedMemoryManager_Dump(&gN64MemoryManager);
            if (gCombatFighter1->v468 != 0) {
                if (gCombatFighter1->seq.numLayers == 0) mpFatalError("No launch frame!");
                B_800F1338 = LoadCharacterFrame(gCombatChar1, (s16)gCombatFighter1->seq.layers[0].frame);
                B_800F1338->xOffset = B_800F1338->width / 2;
                B_800F1338->yOffset = B_800F1338->height / 2;
            } else {
                if (gCombatFighter2->seq.numLayers == 0) mpFatalError("No launch frame!");
                B_800F1338 = LoadCharacterFrame(gCombatChar2, (s16)gCombatFighter2->seq.layers[0].frame);
            }
            func_8005BF80();
            B_800F88D8 = 1;
            B_800F7EC0 = 1;
            B_800F2E1C = 0;
            func_8002A340(&B_800E0E00, 1);
            B_800E0E18 = func_80081804();
            B_800F15D8 = 1;
        }
        if (B_800F7EC0 != 0 && B_800F88D8 >= 241) {
            func_8005C060();
            func_8007CCA4();
            CharFrameCache_FreeAll();
            mpLinkedMemoryManager_Pack(&gN64MemoryManager);
            func_8007C5BC(0x2800, 0xC0, 0xFA0);
            mpLinkedMemoryManager_Dump(&gN64MemoryManager);
            B_800F7EC0 = 0;
            func_8002A340(&B_800E0E00, 1);
            B_800E0E18 = func_80081804();
        }

        func_8007C850();
        if (B_800F7EC0 != 0) {
            f32 a[3], b[3];
            a[0] = 0.0f; a[1] = 1.0f; a[2] = 0.0f;
            b[0] = -1.0f; b[1] = 0.0f; b[2] = 0.0f;
            func_8007E860(&gDL);
            func_8007E720(&gDL, 0, 0, 0);
            func_8007EEF0(&gDL);
            func_8005C21C(B_800F1338, a, b, B_800F2E1C);
        } else {
            func_8007E860(&gDL);
            func_8000B908();
            func_80087E70(m, gCamera);
            func_8008861C(m);
            func_8008004C(&gDL, m);
            B_800E0CD0[0] = (f32)((f64)(gCombatFighter1->pos[0] + gCombatFighter2->pos[0]) / 2.0);
            B_800E0CD0[1] = (f32)((f64)(gCombatFighter1->pos[1] + gCombatFighter2->pos[1]) / 2.0);
            B_800E0CD0[2] = (f32)((f64)(gCombatFighter1->pos[2] + gCombatFighter2->pos[2]) / 2.0);
            func_800801F4(B_800E0CD0, B_800E0CDC);
            func_8007EEF0(&gDL);
            func_8000B998(gCamera, gCombat);
            Fx_DrawSmokeInAir();
            Fx_DrawBitsInAir();
            func_8007EEF0(&gDL);
            func_8000BCA8(gCamera, gCombat);
            func_80087E20(m);
            func_80080170(&gDL, m);
            func_8007EEF0(&gDL);
            func_80038574();
            Fx_DrawSmokeFlat();
            Fx_DrawBitsFlat();
            if (B_800F5D0C != 0) Fx_DrawRings();
            func_8000BE80(gCamera, gCombat);
            if (gCombatArcade == 0 && gCombatSolo == 0) {
                func_8007EB48(&gDL);
                Hud_Draw();
                func_8007EEF0(&gDL);
                func_80087E70(m, gCamera);
                func_8008861C(m);
                func_8008004C(&gDL, m);
            }
            func_80087E20(m);
            func_80080170(&gDL, m);
            func_8007EEF0(&gDL);
            AfterImages_Update();
            Fx_DrawSmokeLanded();
            Fx_DrawBitsLanded();
            Combat_DrawFighters();
            Fx_DrawDebris();
            Fx_DrawFlashes();
            Fx_DrawSparks();
            Fx_DrawGlows();
            Fx_DrawBursts();
            func_80041CD4();
            func_80041CEC();
            func_8000C07C(gCamera, gCombat);
        }
        func_80087E20(m);
        func_80080170(&gDL, m);
        func_80041CE0();

        if (gCombat->v54 == 2) {
            /* the pre-fight taunts: two panels and the current page's text */
            s32 rect[4];
            s32 y, flip;
            f32 x;
            u8 *c;
            rect[0] = 0;
            rect[1] = 0;
            rect[2] = 319;
            rect[3] = 239;
            func_8007EB48(&gDL);
            func_8007F3D4(&gDL, rect, 0, 0, 0, (u32)((f64)B_800F7EC4 * 150.0) & 0xFF);
            up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
            right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
            func_8007F94C(&gDL);
            func_8007EEF0(&gDL);
            if (!((f64)B_800F7EC4 < 1.0)) {
                B_800F7D94 = 11; B_800F7778 = 11;
                B_800F7789 = 170; B_800F7770 = 170;
                B_800F7E78 = 206; B_800F4E34 = 206;
                B_800F7788 = 0; B_800F7E70 = 0;
                B_800F88BC = 30; B_800F88C8 = 30;
                B_800F7ECC = 100; B_800F2D1C = 100;
                x = (gGame->v18 == 1) ? 126.0f : -126.0f;
                flip = gGame->v18 != 1;
                TauntPanel(x, (f32)(70 - 37), flip, (gGame->v18 == 1) ? (void *)B_800F8CA8 : (void *)B_800F7D54, up, right);
                x = (gGame->v18 == 1) ? -126.0f : 126.0f;
                flip = gGame->v18 == 1;
                TauntPanel(x, (f32)(0 - 37), flip, (gGame->v18 == 1) ? (void *)B_800F7D54 : (void *)B_800F8CA8, up, right);
                c = &D_800ADADC[B_800F7E60 * 6];
                B_800F7D94 = c[0]; B_800F7778 = c[0];
                B_800F7789 = c[1]; B_800F7770 = c[1];
                B_800F7E78 = c[2]; B_800F4E34 = c[2];
                B_800F7788 = c[3]; B_800F7E70 = c[3];
                B_800F88BC = c[4]; B_800F88C8 = c[4];
                B_800F7ECC = c[5]; B_800F2D1C = c[5];
                func_80082210(D_800ACD04, &gDL, 0);
                y = (B_800F84C8 & 1) ? 0 : 70;
                SetTexFilter(0);
                if (B_800F4E8C == 1) {
                    if (B_800F5BE8 != 0) {
                        func_80082518(D_800ACD04, &gDL, (char *)B_800F5BE8, -90.0f, (f32)y, 0, up, right, 8);
                        y -= *(s16 *)(D_800ACD04 + 2);
                    }
                    if (B_800F8C7C != 0) {
                        func_80082518(D_800ACD04, &gDL, (char *)B_800F8C7C, -90.0f, (f32)y, 0, up, right, 8);
                        y -= *(s16 *)(D_800ACD04 + 2);
                    }
                    if (B_800F7D98 != 0) {
                        func_80082518(D_800ACD04, &gDL, (char *)B_800F7D98, -90.0f, (f32)y, 0, up, right, 8);
                    }
                } else {
                    s32 w;
                    if (B_800F5BE8 != 0) {
                        w = func_80082298(D_800ACD04, (char *)B_800F5BE8);
                        func_80082518(D_800ACD04, &gDL, (char *)B_800F5BE8, (f32)(90 - w), (f32)y, 0, up, right, 8);
                        y -= *(s16 *)(D_800ACD04 + 2);
                    }
                    if (B_800F8C7C != 0) {
                        w = func_80082298(D_800ACD04, (char *)B_800F8C7C);
                        func_80082518(D_800ACD04, &gDL, (char *)B_800F8C7C, (f32)(90 - w), (f32)y, 0, up, right, 8);
                        y -= *(s16 *)(D_800ACD04 + 2);
                    }
                    if (B_800F7D98 != 0) {
                        w = func_80082298(D_800ACD04, (char *)B_800F7D98);
                        func_80082518(D_800ACD04, &gDL, (char *)B_800F7D98, (f32)(90 - w), (f32)y, 0, up, right, 8);
                    }
                }
                SetTexFilter(0x2000);
            }
        }

        if (gCombatSolo == 0) {
            one[0] = 0;
            one[1] = 0;
            func_8007EEF0(&gDL);
            func_8007F94C(&gDL);
            up2[0] = 0.0f; up2[1] = 1.0f; up2[2] = 0.0f;
            right2[0] = -1.0f; right2[1] = 0.0f; right2[2] = 0.0f;
            if (B_800E0E24 != 0) {
                /* skipping: "New Challenger!" flashes */
                if (!(B_800E0E28 & 0x10)) {
                    s32 w;
                    func_80087EE8(m2, 0.0f, 0.0f, 0.0f);
                    func_80088134(m2, 1.0f, 1.0f, 1.0f);
                    func_80080170(&gDL, m2);
                    func_80082210(D_800ACCF4, &gDL, 0);
                    SetTexFilter(0);
                    w = func_80082298(D_800ACCF4, "New Challenger!");
                    func_800824CC(D_800ACCF4, &gDL, "New Challenger!", (f32)(-(w / 2)), 50.0f, 0.0f, up2, right2, 8);
                    SetTexFilter(0x2000);
                }
            } else {
                if (D_800ACD0C != 0) DrawTitle(&B_800F89E8, B_800F7E80, 0, 1, m2, one, up2, right2);
                if (D_800ACD10 != 0) DrawTitle(&B_800F77E0, B_800F7E10, 1, 0, m2, one, up2, right2);
                if (D_800ACD14 != 0) DrawWinText(gCombat->v2CB4 != 0 ? 0 : 2, m2, one, up2, right2);
                if (gCombat->v2CB0 != 0) {
                    s32 w;
                    func_80082210(D_800ACCFC, &gDL, 2);
                    func_80087EE8(m2, 0.0f, 145.0f, 0.0f);
                    func_80088134(m2, 0.5f, 0.4f, 0.5f);
                    func_80080170(&gDL, m2);
                    w = func_80082298(D_800ACCFC, "Time Out");
                    func_800824CC(D_800ACCFC, &gDL, "Time Out", (f32)(-(w / 2)), 0.0f, 0.0f, up2, right2, 8);
                }
                ComboBanner_Draw(&B_800F7F60, -70.0f, -1.0f);
                ComboBanner_Draw(&B_800F1600, 70.0f, 1.0f);
            }
        }
        if (B_800F7A80 != 0) Combat_DrawPause();
        if (gCombatArcade != 0 && B_800E0E3C != NULL) ((void (*)(void))B_800E0E3C)();
        if (gCombatSolo != 0 && B_800E0E48 != NULL) ((void (*)(void))B_800E0E48)();
        if (!FadeIdle(&B_800E0E00)) func_8002AC80(&B_800E0E00, &gDL);
        if (!FadeIdle(&gScreenFade) || B_800E0CE8 != 0) func_8002AC80(&gScreenFade, &gDL);
        Game_EndFrame();
        if (B_800E0CE8 != 0 && FadeIdle(&gScreenFade)) {
            Unlock(&B_800E0C80);
            return;
        }
        Unlock(&B_800E0C80);
        CharFrameCache_Update();
    }
}

/* ---------------------------------------------------------------- fighters */

/* a light level 0..1 (clamped) as a colour byte */
static u8 ColourByte(f32 v) {
    return (u8)(u32)((f64)v * 255.0);
}

/* v * T (a row vector through a 4x4 matrix), the translation added in double precision */
static void Transform(f32 *out, f32 *v, f32 *t) {
    out[0] = (f32)((f64)(v[0] * t[0] + v[1] * t[4] + v[2] * t[8]) + (f64)t[12]);
    out[1] = (f32)((f64)(v[0] * t[1] + v[1] * t[5] + v[2] * t[9]) + (f64)t[13]);
    out[2] = (f32)((f64)(v[0] * t[2] + v[1] * t[6] + v[2] * t[10]) + (f64)t[14]);
}

static void Copy3(f32 *d, f32 *s) {
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

/* 0x80037410: draw a sprite of a fighter and light its four corners from the arena's lights
 * (the colours are written into the shared corner vertices after the draw is queued; the
 * normals lean outwards at each corner) */
void Fighter_DrawLit(CharFrame *f, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    f32 corner[4][4];           /* sp30..sp60: corner positions (from the sprite drawer) */
    f32 normal[4][4];           /* sp70..spA0: the normal used at each corner */
    f32 lean[4][4];             /* spB0..spE0: the up vector leant four ways */
    f32 u[4], r[4], n[3];
    f32 basis[16], t[16];
    f32 lr, lg, lb;
    f32 c[4][3];
    s32 i, k;
    u8 *mats[4];

    for (i = 0; i < 4; i++) c[i][0] = c[i][1] = c[i][2] = 0.0f;
    u[0] = up[0]; u[1] = up[1]; u[2] = up[2];
    func_80087DD0(u);
    r[0] = right[0]; r[1] = right[1]; r[2] = right[2];
    func_80087DD0(r);
    n[0] = u[1] * r[2] - u[2] * r[1];
    n[1] = u[2] * r[0] - u[0] * r[2];
    n[2] = u[0] * r[1] - u[1] * r[0];
    basis[0] = r[0]; basis[1] = r[1]; basis[2] = r[2]; basis[3] = 0.0f;
    basis[4] = u[0]; basis[5] = u[1]; basis[6] = u[2]; basis[7] = 0.0f;
    basis[8] = n[0]; basis[9] = n[1]; basis[10] = n[2]; basis[11] = 0.0f;
    basis[12] = 0.0f; basis[13] = 0.0f; basis[14] = 0.0f; basis[15] = 1.0f;
    func_800836B0(f, x, y, z, up, right, flags, corner[0], corner[1], corner[2], corner[3]);
    mats[0] = B_800E0D00;
    mats[1] = B_800E0D40;
    mats[2] = B_800E0D80;
    mats[3] = B_800E0DC0;
    for (k = 0; k < 4; k++) {
        func_80087E70(t, mats[k]);
        func_80088100(t, basis);
        Transform(lean[k], u, t);
    }
    if (flags & 1) {
        if (flags & 2) { Copy3(normal[0], lean[3]); Copy3(normal[1], lean[2]); Copy3(normal[2], lean[1]); Copy3(normal[3], lean[0]); }
        else           { Copy3(normal[0], lean[1]); Copy3(normal[1], lean[0]); Copy3(normal[2], lean[3]); Copy3(normal[3], lean[2]); }
    } else {
        if (flags & 2) { Copy3(normal[0], lean[2]); Copy3(normal[1], lean[3]); Copy3(normal[2], lean[0]); Copy3(normal[3], lean[1]); }
        else           { Copy3(normal[0], lean[0]); Copy3(normal[1], lean[1]); Copy3(normal[2], lean[2]); Copy3(normal[3], lean[3]); }
    }
    for (i = 0; i < D_800AB4A4; i++) {
        for (k = 0; k < 4; k++) {
            func_800893F8(D_800AB4A8 + i * 0x3C, corner[k], normal[k], &lr, &lg, &lb);
            c[k][0] += lr;
            c[k][1] += lg;
            c[k][2] += lb;
        }
    }
    for (k = 0; k < 4; k++) {
        if (1.0 < (f64)c[k][0]) c[k][0] = 1.0f;
        if (1.0 < (f64)c[k][1]) c[k][1] = 1.0f;
        if (1.0 < (f64)c[k][2]) c[k][2] = 1.0f;
    }
    B_800F7778 = ColourByte(c[0][0]);
    B_800F7770 = ColourByte(c[0][1]);
    B_800F4E34 = ColourByte(c[0][2]);
    B_800F7D94 = ColourByte(c[1][0]);
    B_800F7789 = ColourByte(c[1][1]);
    B_800F7E78 = ColourByte(c[1][2]);
    B_800F7E70 = ColourByte(c[2][0]);
    B_800F88C8 = ColourByte(c[2][1]);
    B_800F2D1C = ColourByte(c[2][2]);
    B_800F7788 = ColourByte(c[3][0]);
    B_800F88BC = ColourByte(c[3][1]);
    B_800F7ECC = ColourByte(c[3][2]);
}

/* 0x8003A314: draw a fighter's sprite and keep a copy in an after-image slot */
void AfterImage_Capture(cfCharacter *c, CharObj *obj, AfterImage *a) {
    f32 up[4], right[4], gup[4], gright[4], v[4], off[2], pos[3];
    f32 m[16];
    SeqLayer *l;
    CharFrame *f;
    f32 s;
    s32 i;

    if ((f64)c->angle == 0.0) {
        up[0] = 0.0f; up[1] = 2.0f; up[2] = 0.0f;
        right[0] = (f32)((f64)gCombat->camDirX * -2.0);
        right[1] = (f32)((f64)gCombat->camDirY * -2.0);
        right[2] = (f32)((f64)gCombat->camDirZ * -2.0);
        gup[0] = up[0]; gup[1] = 2.0f; gup[2] = up[0];
        gright[0] = right[0]; gright[1] = right[1]; gright[2] = right[2];
    } else {
        /* turned: lean the copy's corners through the turn */
        up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
        right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
        m[0] = gCombat->camDirX; m[1] = gCombat->camDirY; m[2] = gCombat->camDirZ; m[3] = 0.0f;
        m[4] = up[0]; m[5] = 1.0f; m[6] = up[0]; m[7] = up[0];
        v[0] = up[0];
        v[1] = up[0] * -1.0f;
        v[2] = 1.0f;
        m[8] = v[1]; m[9] = up[0]; m[10] = -1.0f; m[11] = up[0];
        m[12] = up[0]; m[13] = up[0]; m[14] = up[0]; m[15] = 1.0f;
        func_80088440(m, (s32)((f64)((up[0] - c->angle) * 65536.0f) / 360.0));
        for (i = 0; i < 3; i++) up[i] = (f32)((f64)up[i] + (f64)up[i]);
        for (i = 0; i < 3; i++) right[i] = (f32)((f64)right[i] + (f64)right[i]);
        Transform(gup, up, m);
        Transform(gright, right, m);
    }
    for (i = 0; i < 3; i++) gup[i] = gup[i] / c->def->stats[8];
    for (i = 0; i < 3; i++) gright[i] = gright[i] / c->def->stats[8];
    if ((f64)gGame->v44 != 1.0) {
        for (i = 0; i < 3; i++) gup[i] = gup[i] * gGame->v44;
        for (i = 0; i < 3; i++) gright[i] = gright[i] * gGame->v44;
    }
    l = &c->seq.layers[0];
    if ((f64)c->angle == 0.0) {
        off[0] = (f32)l->x;
        off[1] = (f32)l->y;
    } else {
        f32 lx = (f32)l->x, ly = (f32)l->y;
        off[0] = (f32)((f64)(lx * c->matrix.m[0][0] + ly * c->matrix.m[1][0]) + (f64)c->matrix.m[2][0]);
        off[1] = (f32)((f64)(lx * c->matrix.m[0][1] + ly * c->matrix.m[1][1]) + (f64)c->matrix.m[2][1]);
    }
    s = off[0];
    pos[0] = s * gCombat->camDirX + c->pos[0];
    pos[1] = (f32)((0.0 - (f64)(c->screenY + off[1])) + (f64)B_800F5CDC);
    pos[2] = s * gCombat->camDirZ + c->pos[2];
    f = LoadCharacterFrame(obj, (s16)l->frame);
    Fighter_DrawLit(f, pos[0], pos[1], pos[2], up, right, l->flags);
    a->flags = l->flags;
    a->colour[0][0] = B_800F7778; a->colour[0][1] = B_800F7770; a->colour[0][2] = B_800F4E34;
    a->colour[1][0] = B_800F7D94; a->colour[1][1] = B_800F7789; a->colour[1][2] = B_800F7E78;
    a->colour[2][0] = B_800F7E70; a->colour[2][1] = B_800F88C8; a->colour[2][2] = B_800F2D1C;
    a->colour[3][0] = B_800F7788; a->colour[3][1] = B_800F88BC; a->colour[3][2] = B_800F7ECC;
    a->pos[0] = pos[0]; a->pos[1] = pos[1]; a->pos[2] = pos[2];
    a->up[0] = gup[0]; a->up[1] = gup[1]; a->up[2] = gup[2];
    a->right[0] = gright[0]; a->right[1] = gright[1]; a->right[2] = gright[2];
    a->frame = (s16)l->frame;
    a->obj = obj;
    a->timer = 16;
}

/* darken a colour byte by d (to 0) */
static void Darken(u8 *c, s32 d) {
    if ((u8)d < *c) *c = *c - d;
    else *c = 0;
}

/* 0x8003AA30: draw the after-images, fading and darkening as they age */
void AfterImages_Draw(AfterImage *a) {
    s32 i, alpha, d;
    CharFrame *f;
    for (i = 0; i < 8; i++) {
        if (a[i].timer <= 0) continue;
        f = LoadCharacterFrame(a[i].obj, a[i].frame);
        alpha = (u32)(a[i].timer * 200) >> 4;
        d = 110 - (a[i].timer * 70) / 16;
        B_800F7778 = a[i].colour[0][0]; B_800F7770 = a[i].colour[0][1]; B_800F4E34 = a[i].colour[0][2]; B_800F5B78 = alpha;
        B_800F7D94 = a[i].colour[1][0]; B_800F7789 = a[i].colour[1][1]; B_800F7E78 = a[i].colour[1][2]; B_800F8760 = alpha;
        B_800F7E70 = a[i].colour[2][0]; B_800F88C8 = a[i].colour[2][1]; B_800F2D1C = a[i].colour[2][2]; B_800F2D1D = alpha;
        B_800F7788 = a[i].colour[3][0]; B_800F88BC = a[i].colour[3][1]; B_800F7ECC = a[i].colour[3][2]; B_800F7E58 = alpha;
        Darken(&B_800F7778, d);
        Darken(&B_800F7770, d);
        Darken(&B_800F7D94, d);
        Darken(&B_800F7789, d);
        Darken(&B_800F7E70, d);
        Darken(&B_800F88C8, d);
        Darken(&B_800F7788, d);
        Darken(&B_800F88BC, d);
        func_80085FE4(&gDL, f, a[i].pos[0], a[i].pos[1], a[i].pos[2], a[i].up, a[i].right, a[i].flags);
    }
}

/* the first free after-image slot gets a copy of this fighter, if it is doing a special move */
static void AfterImageFor(cfCharacter *c, CharObj *obj, AfterImage *a) {
    s32 i;
    if ((c->v420 != 0 || c->v424 != 0) && c->seqIndex >= 0x40) {
        for (i = 0; i < 8; i++) {
            if (a[i].timer <= 0) {
                AfterImage_Capture(c, obj, &a[i]);
                return;
            }
        }
    }
}

/* 0x8003A13C: age the after-images; every third frame, special moves leave a new one */
void AfterImages_Update(void) {
    s32 i;
    for (i = 0; i < 8; i++) {
        if (B_800E0E50[i].timer > 0) B_800E0E50[i].timer--;
        if (B_800E1030[i].timer > 0) B_800E1030[i].timer--;
    }
    if (func_800A4090(((s64)D_800B09B0 << 32) | D_800B09B4, 3) != 0) return;
    AfterImageFor(gCombatFighter1, gCombatChar1, B_800E0E50);
    AfterImageFor(gCombatFighter2, gCombatChar2, B_800E1030);
}

/* draw a fighter's back layer first when it has a second sprite layer turned on */
static s32 HasBackLayer(cfCharacter *c) {
    return c->seq.numLayers >= 2 && c->seq.pairs[0].set != 0;
}

/* 0x80038628: the fighters, back to front, with shadows and after-images */
void Combat_DrawFighters(void) {
    cfCharacter *f1, *f2;
    if (gCombat->v68 == 0) return;
    if (gCombatSolo != 0) {
        Fighter_DrawProjectiles(gCombatFighter1, gCombatChar1, 0);
        AfterImages_Draw(B_800E0E50);
        Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
        Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
        Fighter_DrawProjectiles(gCombatFighter1, gCombatChar1, 1);
        return;
    }
    Fighter_DrawProjectiles(gCombatFighter2, gCombatChar2, 0);
    Fighter_DrawProjectiles(gCombatFighter1, gCombatChar1, 0);
    f1 = gCombatFighter1;
    f2 = gCombatFighter2;
    if (f2->inFront < f1->inFront) {
        AfterImages_Draw(B_800E1030);
        AfterImages_Draw(B_800E0E50);
        if (HasBackLayer(gCombatFighter1)) {
            Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 1);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 0);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
        } else if (HasBackLayer(gCombatFighter2)) {
            Fighter_Draw(gCombatFighter2, gCombatChar2, 1);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 0);
        } else {
            Fighter_Draw(gCombatFighter2, gCombatChar2, 1);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 0);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
        }
    } else {
        AfterImages_Draw(B_800E0E50);
        AfterImages_Draw(B_800E1030);
        if (HasBackLayer(gCombatFighter2)) {
            Fighter_Draw(gCombatFighter2, gCombatChar2, 1);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 0);
        } else if (HasBackLayer(gCombatFighter1)) {
            Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 1);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 0);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
        } else {
            Fighter_Draw(gCombatFighter1, gCombatChar1, 1);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 1);
            Fighter_Draw(gCombatFighter1, gCombatChar1, 0);
            Fighter_Draw(gCombatFighter2, gCombatChar2, 0);
        }
    }
    Fighter_DrawProjectiles(gCombatFighter2, gCombatChar2, 1);
    Fighter_DrawProjectiles(gCombatFighter1, gCombatChar1, 1);
}

/* scale a sprite's corner vectors to the character's size and the game's global scale */
static void ScaleCorners(cfCharacter *c, f32 *gup, f32 *gright) {
    s32 i;
    for (i = 0; i < 3; i++) gup[i] = gup[i] / c->def->stats[8];
    for (i = 0; i < 3; i++) gright[i] = gright[i] / c->def->stats[8];
    if ((f64)gGame->v44 != 1.0) {
        for (i = 0; i < 3; i++) gup[i] = gup[i] * gGame->v44;
        for (i = 0; i < 3; i++) gright[i] = gright[i] * gGame->v44;
    }
}

/* a turned sprite: the up and right vectors (right starts as -x) doubled, and copies of them
 * turned through 'angle' about the facing direction */
static void TurnedCorners(f32 *up, f32 *right, f32 angle, f32 *gup, f32 *gright) {
    f32 m[16], n[3];
    s32 i;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    n[0] = up[1] * right[2] - up[2] * right[1];
    n[1] = up[2] * right[0] - up[0] * right[2];
    n[2] = up[0] * right[1] - up[1] * right[0];
    m[0] = gCombat->camDirX; m[1] = gCombat->camDirY; m[2] = gCombat->camDirZ; m[3] = 0.0f;
    m[4] = up[0]; m[5] = up[1]; m[6] = up[2]; m[7] = 0.0f;
    m[8] = -n[0]; m[9] = -n[1]; m[10] = -n[2]; m[11] = right[1];
    m[12] = right[1]; m[13] = right[1]; m[14] = right[1]; m[15] = 1.0f;
    func_80088440(m, (s32)((f64)((right[1] - angle) * 65536.0f) / 360.0));
    for (i = 0; i < 3; i++) up[i] = (f32)((f64)up[i] + (f64)up[i]);
    for (i = 0; i < 3; i++) right[i] = (f32)((f64)right[i] + (f64)right[i]);
    Transform(gup, up, m);
    Transform(gright, right, m);
}

/* 0x800394CC: a fighter's shadow on the floor, stretched and moved with its height */
void Fighter_DrawShadow(cfCharacter *c, CharObj *obj) {
    f32 h = (f32)(0.0 - (f64)c->screenY);
    f32 dir[3], a[3], b[3], ga[3], gb[3], k, s, x, z;
    f64 kk;
    SeqLayer *l;
    CharFrame *f;
    s32 i;

    if (B_800F7EC8 == 15 || c->v468 != 0) return;
    dir[0] = 1.0f; dir[1] = 0.0f; dir[2] = 0.0f;
    a[0] = 2.0f; a[1] = dir[1]; a[2] = dir[1];
    b[0] = dir[1]; b[1] = dir[1]; b[2] = 2.0f;
    k = (50.0 < (f64)h) ? 1.25f : (f32)((f64)h * 0.025);
    kk = (f64)k * -0.001;
    for (i = 0; i < 3; i++) a[i] = (f32)((f64)a[i] * ((f64)c->screenY * kk + 1.0));
    for (i = 0; i < 3; i++) b[i] = (f32)((f64)b[i] * ((f64)c->screenY * -0.0005 + 1.0));
    if ((f64)gGame->v44 != 1.0) {
        for (i = 0; i < 3; i++) a[i] = a[i] * gGame->v44;
        for (i = 0; i < 3; i++) b[i] = b[i] * gGame->v44;
    }
    for (i = 0; i < c->seq.numLayers; i++) {
        l = &c->seq.layers[i];
        f = LoadCharacterFrame(obj, (s16)l->frame);
        x = (f32)l->x * gCombat->camDirX + c->pos[0];
        s = 0.0f;
        z = (f32)l->x * gCombat->camDirZ + c->pos[2];
        x = (f32)((f64)x + (f64)(h * dir[0]) * 0.4);
        z = (f32)((f64)z + (f64)(h * dir[2]) * 0.4);
        ga[0] = a[0]; ga[1] = a[1]; ga[2] = a[2];
        gb[0] = b[0]; gb[1] = b[1]; gb[2] = b[2];
        kk = (f64)h * 0.012 * -0.001;
        ga[0] = (f32)((f64)ga[0] * ((f64)c->screenY * kk + 1.0));
        ga[1] = (f32)((f64)ga[1] * ((f64)c->screenY * kk + 1.0));
        ga[2] = (f32)((f64)ga[2] * ((f64)c->screenY * kk + 1.0));
        gb[0] = (f32)((f64)gb[0] * ((f64)c->screenY * -0.0005 + 1.0));
        gb[1] = (f32)((f64)gb[1] * ((f64)c->screenY * -0.0005 + 1.0));
        gb[2] = (f32)((f64)gb[2] * ((f64)c->screenY * -0.0005 + 1.0));
        func_80083A64(&gDL, f, x, B_800F5CDC + s, z, ga, gb, l->flags | 0x8000);
    }
}

/* 0x800399A4: a fighter's projectiles - those nearer the camera than fighter 1 in layer 1, the
 * others in layer 0 (only the first sprite of each is drawn, once per sprite it has) */
void Fighter_DrawProjectiles(cfCharacter *c, CharObj *obj, s32 layer) {
    f32 up[3], right[3], gup[3], gright[3], off[2], pos[3];
    CharProjectile *p;
    CharSeq *q;
    SeqLayer *l;
    CharFrame *f;
    s32 i, j, flags;
    f32 d;

    for (i = 0; i < 16; i++) {
        p = &c->projectiles[i];
        if (p->active == 0) continue;
        d = (gCombatFighter1->pos[2] - p->posZ) * gCombat->camDirX - (gCombatFighter1->pos[0] - p->posX) * gCombat->camDirZ;
        if ((f64)d < 0.0) {
            if (layer != 1) continue;
        } else {
            if (layer != 0) continue;
        }
        if ((f64)p->angle == 0.0) {
            up[0] = 0.0f; up[1] = 2.0f; up[2] = 0.0f;
            right[0] = (f32)((f64)gCombat->camDirX * -2.0);
            right[1] = (f32)((f64)gCombat->camDirY * -2.0);
            right[2] = (f32)((f64)gCombat->camDirZ * -2.0);
            gup[0] = 0.0f; gup[1] = 2.0f; gup[2] = 0.0f;
            gright[0] = right[0]; gright[1] = right[1]; gright[2] = right[2];
        } else {
            up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
            TurnedCorners(up, right, p->angle, gup, gright);
        }
        ScaleCorners(c, gup, gright);
        if (p->seqSlot == -1) mpFatalError("COMBAT.C - *** Cannot display nonsequenced projectiles yet!!!");
        q = &c->projSeqs[p->seqSlot];
        for (j = 0; j < c->projSeqs[p->seqSlot].numLayers; j++) {
            l = &q->layers[0];
            flags = l->flags;
            f = LoadCharacterFrame(obj, (s16)l->frame);
            if ((f64)p->angle == 0.0) {
                off[0] = (f32)l->x;
                off[1] = (f32)l->y;
            } else {
                f32 lx = (f32)l->x, ly = (f32)l->y;
                off[0] = (f32)((f64)(lx * p->matrix.m[0][0] + ly * p->matrix.m[1][0]) + (f64)p->matrix.m[2][0]);
                off[1] = (f32)((f64)(lx * p->matrix.m[0][1] + ly * p->matrix.m[1][1]) + (f64)p->matrix.m[2][1]);
            }
            pos[0] = off[0] * gCombat->camDirX + p->posX;
            pos[1] = p->posY - off[1] + B_800F5CDC;
            pos[2] = off[0] * gCombat->camDirZ + p->posZ;
            Combat_SetLightColour(pos[0], pos[1], pos[2]);
            func_80084CB0(&gDL, f, pos[0], pos[1], pos[2], gup, gright, flags);
        }
    }
}

/* blend a corner colour byte towards a target by t (the dark arena) */
static u8 FogByte(f64 target, f32 t, u8 c) {
    return (u8)(u32)(target + (1.0 - (f64)t) * (f64)(f32)c);
}

/* 0x8003899C: a fighter's sprites (layer 0: all but the back one; layer 1: the back one) */
void Fighter_Draw(cfCharacter *c, CharObj *obj, s32 layer) {
    f32 up[3], right[3], gup[3], gright[3], off[2], x, y, z, t;
    SeqLayer *l;
    CharFrame *f;
    s32 i;

    if (c->v468 != 0) return;
    if (layer != 0 && !(c->seq.numLayers >= 2 && c->seq.pairs[0].set != 0)) return;
    if ((f64)c->angle == 0.0) {
        for (i = 0; i < 3; i++) up[i] = (f32)((f64)B_800F8780[i] + (f64)B_800F8780[i]);
        for (i = 0; i < 3; i++) right[i] = (f32)((f64)((f32 *)B_800F8770)[i] * -2.0);
        gup[0] = up[0]; gup[1] = up[1]; gup[2] = up[2];
        gright[0] = right[0]; gright[1] = right[1]; gright[2] = right[2];
    } else {
        up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
        TurnedCorners(up, right, c->angle, gup, gright);
    }
    ScaleCorners(c, gup, gright);
    i = (c->seq.numLayers >= 2) ? (c->seq.pairs[0].set != 0 && layer == 0) : 0;
    for (; i < c->seq.numLayers; i++) {
        l = &c->seq.layers[i];
        if ((f64)c->angle == 0.0) {
            off[0] = (f32)l->x;
            off[1] = (f32)l->y;
        } else {
            f32 lx = (f32)l->x, ly = (f32)l->y;
            off[0] = (f32)((f64)(lx * c->matrix.m[0][0] + ly * c->matrix.m[1][0]) + (f64)c->matrix.m[2][0]);
            off[1] = (f32)((f64)(lx * c->matrix.m[0][1] + ly * c->matrix.m[1][1]) + (f64)c->matrix.m[2][1]);
        }
        f = LoadCharacterFrame(obj, (s16)l->frame);
        x = (f32)((f64)(off[0] * gCombat->camDirX + c->pos[0]) - (f64)(off[1] + c->screenY) * ((f64)up[0] / 2.0));
        y = (f32)((0.0 - (f64)(c->screenY + off[1]) * ((f64)up[1] / 2.0)) + (f64)B_800F5CDC);
        z = (f32)((f64)(off[0] * gCombat->camDirZ + c->pos[2]) - (f64)(off[1] + c->screenY) * ((f64)up[2] / 2.0));
        Fighter_DrawLit(f, x, y, z, up, right, l->flags);
        if (B_800F7EC8 == 15) {
            /* the dark arena: fade the lower corners with height and the flickering light */
            t = (f32)((90.0 - (f64)(y - B_800F5CDC)) / 90.0);
            t = (f32)((f64)t * ((f64)B_800F4E70 * 0.7 + 0.3));
            if ((f64)t < 0.0) t = 0.0f;
            if (1.0 < (f64)t) t = 1.0f;
            B_800F7E70 = FogByte((f64)t * 170.0, t, B_800F7E70);
            B_800F88C8 = FogByte((f64)(t * 25.0f), t, B_800F88C8);
            B_800F2D1C = FogByte((f64)(t * 0.0f), t, B_800F2D1C);
            B_800F7788 = FogByte((f64)t * 170.0, t, B_800F7788);
            B_800F88BC = FogByte((f64)(t * 25.0f), t, B_800F88BC);
            B_800F7ECC = FogByte((f64)(t * 0.0f), t, B_800F7ECC);
        }
        func_80084CB0(&gDL, f, x, y, z, gup, gright, l->flags);
        if (layer != 0) break;
    }
}

/* ---------------------------------------------------------------- smoke and clay bits */

static void FlatCorners(f32 angle, f32 scale, s32 doScale, f32 *u, f32 *v) {
    f32 m[16], a[3], b[3];
    a[0] = 0.0f; a[1] = 0.0f; a[2] = 2.0f;
    b[0] = -2.0f; b[1] = 0.0f; b[2] = 0.0f;
    func_80087FD0(m, (s32)((f64)(angle * 65536.0f) / 360.0));
    func_8009708C(m, a[0], a[1], a[2], &u[0], &u[1], &u[2]);
    func_8009708C(m, b[0], b[1], b[2], &v[0], &v[1], &v[2]);
    if (doScale) {
        u[0] = scale * u[0]; u[1] = scale * u[1]; u[2] = scale * u[2];
        v[0] = scale * v[0]; v[1] = scale * v[1]; v[2] = scale * v[2];
    }
}

/* 0x8003ADB8: smoke that has landed, lying flat on the floor */
void Fx_DrawSmokeFlat(void) {
    f32 u[3], v[3];
    s32 i;
    func_8008144C(&gDL, gCombatChar1->lightPalettes[0], 0);
    for (i = 0; i < 64; i++) {
        FxSmoke *e = &gCombat->smoke[i];
        if (e->active == 0) continue;
        if (!(0.0 <= (f64)e->pos[1])) continue;
        FlatCorners(e->angle, (f32)((f64)e->v10 * 0.3), 1, u, v);
        func_80083A64(&gDL, B_800F5D18 + *(s32 *)(B_800F5D18 + e->vC * 8 + 0xC), e->pos[0], B_800F5CDC, e->pos[2],
                      u, v, 0x8008);
    }
}

/* 0x8003B670: clay bits that have landed, lying flat on the floor */
void Fx_DrawBitsFlat(void) {
    f32 u[3], v[3];
    CharObj *obj;
    s32 i;
    for (i = 0; i < 32; i++) {
        FxBit *e = &gCombat->bits[i];
        if (e->active == 0) continue;
        if (!(0.0 <= (f64)e->pos[1])) continue;
        obj = (e->v4 == 1) ? gCombatChar1 : gCombatChar2;
        FlatCorners(e->angle, 0.0f, 0, u, v);
        func_80083A64(&gDL, LoadCharacterFrame(obj, e->vC), e->pos[0], B_800F5CDC, e->pos[2], u, v, 0x8008);
    }
}

/* smoke puffs facing the camera, turned about its forward axis: those still in the air
 * (landed = 0) or those that have landed (1), each drawn at its own point in the frame */
static void DrawSmoke(s32 landed) {
    f32 m[16], up[3], right[3], u[3], v[3], s;
    s32 lastPal = -1, i, j, r;
    CharObj *lastObj = NULL, *obj;
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    for (i = 0; i < 64; i++) {
        FxSmoke *e = &gCombat->smoke[i];
        if (e->active == 0) continue;
        if (landed ? !(0.0 <= (f64)e->pos[1]) : !((f64)e->pos[1] < 0.0)) continue;
        obj = (e->v4 == 1) ? gCombatChar1 : gCombatChar2;
        if (e->v8 < 0) {
            /* pick one of the character's light palettes, weighted */
            r = (s32)guRandom() % 100;
            for (j = 0; j < 4; j++) {
                if (r < obj->lightValues[j]) {
                    e->v8 = j;
                    break;
                }
            }
            if (j >= 4) e->v8 = 0;
        }
        s = (f32)((f64)e->v10 * 0.3);
        func_80097740(m, e->angle, B_800F8790[0], B_800F8790[1], B_800F8790[2]);
        func_8009708C(m, up[0], up[1], up[2], &u[0], &u[1], &u[2]);
        func_8009708C(m, right[0], right[1], right[2], &v[0], &v[1], &v[2]);
        u[0] = s * u[0]; u[1] = s * u[1]; u[2] = s * u[2];
        v[0] = s * v[0]; v[1] = s * v[1]; v[2] = s * v[2];
        if (lastPal != e->v8 || lastObj != obj) {
            func_8008144C(&gDL, obj->lightPalettes[e->v8], 0);
            lastPal = e->v8;
            lastObj = obj;
        }
        Combat_SetLightColour(e->pos[0], e->pos[1] + B_800F5CDC, e->pos[2]);
        func_80084CB0(&gDL, B_800F5D18 + *(s32 *)(B_800F5D18 + e->vC * 8 + 0xC), e->pos[0], e->pos[1] + B_800F5CDC,
                      e->pos[2], u, v, 0);
    }
}

/* clay bits facing the camera (in the air or landed, as for smoke) */
static void DrawBits(s32 landed) {
    f32 m[16], up[3], right[3], u[3], v[3];
    CharObj *obj;
    CharFrame *f;
    s32 i;
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    for (i = 0; i < 32; i++) {
        FxBit *e = &gCombat->bits[i];
        if (e->active == 0) continue;
        if (landed ? !(0.0 <= (f64)e->pos[1]) : !((f64)e->pos[1] < 0.0)) continue;
        obj = (e->v4 == 1) ? gCombatChar1 : gCombatChar2;
        func_80097740(m, e->angle, B_800F8790[0], B_800F8790[1], B_800F8790[2]);
        func_8009708C(m, up[0], up[1], up[2], &u[0], &u[1], &u[2]);
        func_8009708C(m, right[0], right[1], right[2], &v[0], &v[1], &v[2]);
        f = LoadCharacterFrame(obj, e->vC);
        Combat_SetLightColour(e->pos[0], e->pos[1] + B_800F5CDC, e->pos[2]);
        func_80084CB0(&gDL, f, e->pos[0], e->pos[1] + B_800F5CDC, e->pos[2], u, v, 0);
    }
}

/* 0x8003AFF0, 0x8003B330, 0x8003B874, 0x8003BA9C */
void Fx_DrawSmokeInAir(void) { DrawSmoke(0); }
void Fx_DrawSmokeLanded(void) { DrawSmoke(1); }
void Fx_DrawBitsInAir(void)  { DrawBits(0); }
void Fx_DrawBitsLanded(void)  { DrawBits(1); }

/* ---------------------------------------------------------------- debris and 3D effects */

static void SetVtx(Vtx16 *v, s32 x, s32 y, s32 z, u8 r, u8 g, u8 b, u8 a) {
    v->x = x;
    v->y = y;
    v->z = z;
    v->flag = 0;
    v->s = 0;
    v->t = 0;
    v->r = r;
    v->g = g;
    v->b = b;
    v->a = a;
}

static void Gfx2(u32 w0, u32 w1) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = w0;
    g[1] = w1;
}

/* 0x8003E0C4: debris - each piece a small cyan triangle, fading as it ages */
void Fx_DrawDebris(void) {
    f32 up[3], right[3], u[3], r[3], c[3], s, a;
    s32 i;
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0x404240, 0x104240);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 32; i++) {
        FxDebris *d = &gCombat->debris[i];
        if (d->active == 0) continue;
        s = (f32)((f64)(d->v4 * 10.0f) * 0.5);
        u[0] = s * up[0]; u[1] = s * up[1]; u[2] = s * up[2];
        r[0] = s * right[0]; r[1] = s * right[1]; r[2] = s * right[2];
        a = (f32)((36.0 - (f64)d->v4) * 200.0 / 36.0 + 55.0);
        c[0] = d->velX * r[0] + d->pos[0] + d->velY * u[0];
        c[1] = d->velX * r[1] + d->pos[1] + d->velY * u[1];
        c[2] = d->velX * r[2] + d->pos[2] + d->velY * u[2];
        SetVtx(&gVtx[0], (s32)c[0], (s32)(c[1] + B_800F5CDC), (s32)c[2], 0, 255, 255, (u32)a);
        SetVtx(&gVtx[1], (s32)((f64)c[0] - 6.0), (s32)(((f64)c[1] - 6.0) + (f64)B_800F5CDC), (s32)c[2], 0, 255, 255, (u32)a);
        SetVtx(&gVtx[2], (s32)((f64)c[0] + 6.0), (s32)(((f64)c[1] - 6.0) + (f64)B_800F5CDC), (s32)c[2], 0, 255, 255, (u32)a);
        Gfx2(0x04000C2F, (u32)gVtx);    /* gSPVertex: 3 */
        gVtx += 3;
        Gfx2(0xBF000000, 0x204);        /* gSP1Triangle(0, 1, 2) */
        Gfx2(0xE7000000, 0);            /* gDPPipeSync */
    }
    func_8007EEF0(&gDL);
}

/* a fan of 16 triangles round vertex 16 (gSP2Triangles, two at a time) */
static void DrawFan16(void) {
    s32 k;
    for (k = 0; k < 16; k += 2) {
        u32 *g = gDL;
        gDL = g + 2;
        g[0] = 0xB1000000 | ((((k + 1) % 16 * 2) & 0xFF) << 16) | 0x2000 | ((k * 2) & 0xFF);
        g[1] = ((((k + 2) % 16 * 2) & 0xFF) << 16) | 0x2000 | (((k + 1) % 16 * 2) & 0xFF);
    }
}

/* 0x8003CA64: glows - a disc of 16 points round a centre, its own colour at the middle and
 * the edge, fading as it ages; facing the camera */
void Fx_DrawGlows(void) {
    f32 up[3], right[3], p[3], fade, s;
    s32 i, k, a;
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x404A50, 0x104A50);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 16; i++) {
        FxGlow *g = &gCombat->glows[i];
        if (g->active == 0) continue;
        fade = (f32)(1.0 - (f64)g->v20);
        for (k = 0; k < 16; k++) {
            a = k << 12;
            s = vsSin(a) * g->v18 * up[0] + g->pos[0];
            p[0] = s + vsCos(a) * g->v18 * right[0];
            s = vsSin(a) * g->v18 * up[1] + g->pos[1];
            p[1] = s + vsCos(a) * g->v18 * right[1];
            s = vsSin(a) * g->v18 * up[2] + g->pos[2];
            p[2] = s + vsCos(a) * g->v18 * right[2];
            SetVtx(&gVtx[k], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colB[0], g->colB[1], g->colB[2],
                   (u32)(fade * (f32)g->colB[3]));
        }
        SetVtx(&gVtx[16], (s32)g->pos[0], (s32)(g->pos[1] + B_800F5CDC), (s32)g->pos[2], g->colA[0], g->colA[1],
               g->colA[2], (u32)(fade * (f32)g->colA[3]));
        Gfx2(0x0400450F, (u32)gVtx);    /* gSPVertex: 17 */
        gVtx += 17;
        DrawFan16();
    }
    Gfx2(0xE7000000, 0);
    func_8007EEF0(&gDL);
}

/* the gSP2Triangles index packing, including its "flag" that rotates a triangle's vertices
 * (the game passes the vertex-buffer slot as the flag, which only rotates them) */
#define TRI_W1(v0, v1, v2) (((((v0) * 2) & 0xFF) << 16) | ((((v1) * 2) & 0xFF) << 8) | (((v2) * 2) & 0xFF))
#define TRI_W1F(v0, v1, v2, f) ((f) == 0 ? TRI_W1(v0, v1, v2) : (f) == 1 ? TRI_W1(v1, v2, v0) : TRI_W1(v2, v0, v1))

/* a point at angle a, distance r from the centre, in the camera's plane; double precision */
static void RingPointD(f32 *out, FxRing *g, s32 a, f32 *up, f32 *right, s32 outer) {
    s32 k;
    for (k = 0; k < 3; k++) {
        f64 s = (f64)vsSin(a);
        f32 c = vsCos(a);
        f64 r = outer ? (f64)g->v24 + (f64)g->v18 / 2.0 : (f64)g->v24 - (f64)g->v18 / 2.0;
        out[k] = (f32)((f64)g->pos[k] + s * r * (f64)up[k] + (f64)c * r * (f64)right[k]);
    }
}

/* 0x8003CF60: bursts - a ring of 18 segments (each three points across: the colours fade
 * from the outer colour to the middle colour and back) */
void Fx_DrawBursts(void) {
    f32 up[3], right[3], p[3], fade, s;
    s32 i, k, slot, prev, a, j;
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x404A50, 0x104A50);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 16; i++) {
        FxRing *g = &gCombat->bursts[i];
        if (g->active == 0) continue;
        fade = (f32)(1.0 - (f64)g->age);
        prev = 0;
        slot = 0;
        for (k = 0; k < 18; k++) {
            u32 *d;
            a = k << 12;
            RingPointD(p, g, a, up, right, 0);
            SetVtx(&gVtx[0], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colB[0], g->colB[1], g->colB[2],
                   (u32)(fade * (f32)g->colB[3]));
            for (j = 0; j < 3; j++) {
                s = vsSin(a) * g->v24 * up[j] + g->pos[j];
                p[j] = s + vsCos(a) * g->v24 * right[j];
            }
            SetVtx(&gVtx[1], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colA[0], g->colA[1], g->colA[2],
                   (u32)(fade * (f32)g->colA[3]));
            RingPointD(p, g, a, up, right, 1);
            SetVtx(&gVtx[2], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colB[0], g->colB[1], g->colB[2],
                   (u32)(fade * (f32)g->colB[3]));
            Gfx2(0x04000C2F | (slot << 17), (u32)gVtx);    /* gSPVertex: 3 into this slot */
            gVtx += 3;
            if (k > 0) {
                d = gDL;
                gDL = d + 2;
                d[0] = 0xB1000000 | TRI_W1F(slot, slot + 1, prev + 1, slot);
                d[1] = TRI_W1F(slot, prev + 1, prev, slot);
                d = gDL;
                gDL = d + 2;
                d[0] = 0xB1000000 | TRI_W1F(slot + 1, slot + 2, prev + 2, slot + 1);
                d[1] = TRI_W1F(slot + 1, prev + 2, prev + 1, slot + 1);
            }
            prev = slot;
            slot = (slot == 0) ? 3 : 0;
        }
    }
    Gfx2(0xE7000000, 0);
    func_8007EEF0(&gDL);
}

/* 0x8003D93C: rings - like the bursts, but lying flat on the floor (at height 0, whatever
 * the ring's own height) */
void Fx_DrawRings(void) {
    f32 p[3], fade;
    s32 i, k, slot, prev, a;
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x404240, 0x104240);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 8; i++) {
        FxRing *g = &gCombat->rings[i];
        if (g->active == 0) continue;
        fade = (f32)(1.0 - (f64)g->age);
        prev = 0;
        slot = 0;
        for (k = 0; k < 18; k++) {
            u32 *d;
            a = k << 12;
            p[0] = (f32)((f64)g->pos[0] + (f64)vsCos(a) * ((f64)g->v24 - (f64)g->v18 / 2.0));
            p[1] = 0.0f;
            p[2] = (f32)((f64)g->pos[2] + (f64)vsSin(a) * ((f64)g->v24 - (f64)g->v18 / 2.0));
            SetVtx(&gVtx[0], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colB[0], g->colB[1], g->colB[2],
                   (u32)(fade * (f32)g->colB[3]));
            p[0] = vsCos(a) * g->v24 + g->pos[0];
            p[1] = 0.0f;
            p[2] = vsSin(a) * g->v24 + g->pos[2];
            SetVtx(&gVtx[1], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colA[0], g->colA[1], g->colA[2],
                   (u32)(fade * (f32)g->colA[3]));
            p[0] = (f32)((f64)g->pos[0] + (f64)vsCos(a) * ((f64)g->v24 + (f64)g->v18 / 2.0));
            p[1] = 0.0f;
            p[2] = (f32)((f64)g->pos[2] + (f64)vsSin(a) * ((f64)g->v24 + (f64)g->v18 / 2.0));
            SetVtx(&gVtx[2], (s32)p[0], (s32)(p[1] + B_800F5CDC), (s32)p[2], g->colB[0], g->colB[1], g->colB[2],
                   (u32)(fade * (f32)g->colB[3]));
            Gfx2(0x04000C2F | (slot << 17), (u32)gVtx);
            gVtx += 3;
            if (k > 0) {
                d = gDL;
                gDL = d + 2;
                d[0] = 0xB1000000 | TRI_W1F(slot, slot + 1, prev + 1, slot);
                d[1] = TRI_W1F(slot, prev + 1, prev, slot);
                d = gDL;
                gDL = d + 2;
                d[0] = 0xB1000000 | TRI_W1F(slot + 1, slot + 2, prev + 2, slot + 1);
                d[1] = TRI_W1F(slot + 1, prev + 2, prev + 1, slot + 1);
            }
            prev = slot;
            slot = (slot == 0) ? 3 : 0;
        }
    }
    Gfx2(0xE7000000, 0);
    func_8007EEF0(&gDL);
}

/* a spark point: the centre plus its direction scaled by s, in the camera's plane */
static void SparkPoint(f32 *out, FxSpark *g, f64 s, f32 *right, f32 *up) {
    s32 k;
    for (k = 0; k < 3; k++)
        out[k] = (f32)((f64)g->pos[k] + (f64)right[k] * ((f64)g->dirX * s) + (f64)up[k] * ((f64)g->dirY * s));
}

/* a point beside p, across the spark's direction by d */
static s32 SparkSide(f32 *p, s32 k, f32 *perp, f64 d, f32 *right, f32 *up) {
    return (s32)((f64)p[k] + (f64)right[k] * ((f64)perp[0] * d) + (f64)up[k] * ((f64)perp[1] * d));
}

/* 0x8003BCC4: hit sparks - a streak from the hit point out along the spark's direction:
 * a flickering red-to-white core with green-yellow edges, growing and fading with age */
void Fx_DrawSparks(void) {
    f32 up[3], right[3], p0[3], p1[3], p2[3], perp[2], fade;
    s32 i, k, r, q[3];
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x4049D8, 0x1049D8);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 48; i++) {
        FxSpark *g = &gCombat->sparks[i];
        f64 age;
        u32 *d;
        if (g->active == 0) continue;
        age = (f64)g->age;
        fade = (f32)(1.0 - age);
        perp[0] = -g->dirY;
        perp[1] = g->dirX;
        SparkPoint(p0, g, 0.0, right, up);
        SparkPoint(p1, g, (f64)g->size / 3.0 * age, right, up);
        SparkPoint(p2, g, ((f64)g->size + 30.0) * age, right, up);
        p0[1] += B_800F5CDC;
        p1[1] += B_800F5CDC;
        p2[1] += B_800F5CDC;
        r = (s32)guRandom() % 100;
        SetVtx(&gVtx[0], (s32)p0[0], (s32)p0[1], (s32)p0[2], ~r, 0, 0, (u32)((f64)fade * 160.0 + 70.0));
        SetVtx(&gVtx[1], (s32)p1[0], (s32)p1[1], (s32)p1[2], ~r, 150, 0, (u32)((f64)fade * 80.0 + 50.0));
        SetVtx(&gVtx[2], (s32)p2[0], (s32)p2[1], (s32)p2[2], 255, 255, 255, (u32)((f64)fade * 100.0 + 60.0));
        for (k = 0; k < 3; k++) q[k] = SparkSide(p0, k, perp, 15.0, right, up);
        SetVtx(&gVtx[3], q[0], q[1], q[2], ~r, 255, 0, 0);
        for (k = 0; k < 3; k++) q[k] = SparkSide(p0, k, perp, -15.0, right, up);
        SetVtx(&gVtx[4], q[0], q[1], q[2], ~r, 255, 0, 0);
        for (k = 0; k < 3; k++) q[k] = SparkSide(p1, k, perp, 2.5, right, up);
        SetVtx(&gVtx[5], q[0], q[1], q[2], ~(r / 2), 255, 0, 0);
        for (k = 0; k < 3; k++) q[k] = SparkSide(p1, k, perp, -2.5, right, up);
        SetVtx(&gVtx[6], q[0], q[1], q[2], ~(r / 2), 255, 0, 0);
        Gfx2(0x04001C6F, (u32)gVtx);                       /* gSPVertex: 7 */
        gVtx += 7;
        d = gDL;
        gDL = d + 6;
        d[0] = 0xB1000602; d[1] = 0x00000208;             /* gSP2Triangles */
        d[2] = 0xB102060A; d[3] = 0x000C0802;             
        d[4] = 0xBF000000; d[5] = 0x000C0A04;             /* gSP1Triangle */
    }
    Gfx2(0xE7000000, 0);
    func_8007EEF0(&gDL);
}

/* one ray of a flash: a kite from the centre out along (dx, dy) (in camera space), at
 * distance 60 to 280 scaled by the fade, w across at its widest point */
static void Flash_Ray(FxFlash *f, f32 fade, f32 s, f32 *right, f32 *up, f64 dx, f64 dy, f64 wx, f64 wy) {
    f32 a[3], b[3], c[3], e[3];
    f64 near = (f64)fade * 60.0, far = (f64)fade * 280.0;
    s32 k;
    u32 *d;
    for (k = 0; k < 3; k++) a[k] = (f32)((f64)f->pos[k] + (f64)right[k] * (near * dx) + (f64)up[k] * (near * dy));
    for (k = 0; k < 3; k++) b[k] = (f32)((f64)f->pos[k] + (f64)right[k] * (far * dx) + (f64)up[k] * (far * dy));
    for (k = 0; k < 3; k++) {
        c[k] = (f32)((f64)a[k] + (f64)right[k] * wx + (f64)up[k] * wy);
        e[k] = (f32)((f64)a[k] - (f64)right[k] * wx - (f64)up[k] * wy);
    }
    a[1] += B_800F5CDC;
    b[1] += B_800F5CDC;
    c[1] += B_800F5CDC;
    e[1] += B_800F5CDC;
    SetVtx(&gVtx[0], (s32)a[0], (s32)a[1], (s32)a[2], 230, 230, 255, (u32)((f64)s * 50.0));
    SetVtx(&gVtx[1], (s32)b[0], (s32)b[1], (s32)b[2], 0, 0, 255, (u32)((f64)s * 25.0));
    SetVtx(&gVtx[2], (s32)c[0], (s32)c[1], (s32)c[2], 0, 0, 255, (u32)((f64)s * 3.125));
    SetVtx(&gVtx[3], (s32)e[0], (s32)e[1], (s32)e[2], 0, 0, 255, (u32)((f64)s * 3.125));
    SetVtx(&gVtx[4], (s32)f->pos[0], (s32)(f->pos[1] + B_800F5CDC), (s32)f->pos[2], 255, 255, 255, 0);
    Gfx2(0x0400144F, (u32)gVtx);                           /* gSPVertex: 5 */
    gVtx += 5;
    d = gDL;
    gDL = d + 4;
    d[0] = 0xB1000204; d[1] = 0x00000206;                 /* gSP2Triangles */
    d[2] = 0xB1000804; d[3] = 0x00000806;
}

/* 0x8003E590: flashes - eight blue rays (four straight, four diagonal) that grow out from a
 * point and brighten then fade */
void Fx_DrawFlashes(void) {
    f32 up[3], right[3], fade, s, t;
    s32 i;
    const f64 diag = (f64)0.707f, w = (f64)7.07f;
    up[0] = B_800F8780[0]; up[1] = B_800F8780[1]; up[2] = B_800F8780[2];
    right[0] = ((f32 *)B_800F8770)[0]; right[1] = ((f32 *)B_800F8770)[1]; right[2] = ((f32 *)B_800F8770)[2];
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x404A50, 0x104A50);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 4; i++) {
        FxFlash *f = &gCombat->flashes[i];
        if (f->active == 0) continue;
        if ((f64)f->age <= 0.65) {
            t = (f32)((f64)f->age / 0.65);
            s = t * t;
        } else {
            s = (f32)(1.0 - ((f64)f->age - 0.65) / 0.35);
        }
        fade = (f32)(1.0 - (f64)f->age);
        Flash_Ray(f, fade, s, right, up, 0.0, 1.5, 15.0, -0.0);
        Flash_Ray(f, fade, s, right, up, 1.5, 0.0, 0.0, -15.0);
        Flash_Ray(f, fade, s, right, up, 0.0, -1.5, -15.0, -0.0);
        Flash_Ray(f, fade, s, right, up, -1.5, 0.0, 0.0, 15.0);
        Flash_Ray(f, fade, s, right, up, diag, diag, w, -w);
        Flash_Ray(f, fade, s, right, up, diag, -diag, -w, -w);
        Flash_Ray(f, fade, s, right, up, -diag, -diag, -w, w);
        Flash_Ray(f, fade, s, right, up, -diag, diag, w, w);
    }
    Gfx2(0xE7000000, 0);
    func_8007EEF0(&gDL);
}

/* 0x800423C8: a screen rectangle from (x0, y0) to (x1, y1) (320x240 pixels), shaded from
 * colour 0 to colour 1 - top to bottom, or left to right when "across" is set; drawn
 * opaque when both alphas are 255 */
void Hud_Gradient(s32 x0, s32 y0, s32 r0, s32 g0, s32 b0, s32 a0, s32 x1, s32 y1, s32 r1, s32 g1, s32 b1, s32 a1,
                  s32 across) {
    s32 left = x0 - 159, top = -(y0 - 119), right = x1 - 158, bottom = ~(y1 - 119);
    u32 *d;
    func_8007FB08(&gDL, 0x204);
    if ((u8)a0 < 255 || (u8)a1 < 255)
        func_8007FB90(&gDL, 0x404240, 0x104240);
    else
        func_8007FB90(&gDL, 0xC084000, 0x3024000);
    func_8007FBE8(&gDL, 1);
    SetVtx(&gVtx[0], left, top, 0, r0, g0, b0, a0);
    if (across) SetVtx(&gVtx[1], right, top, 0, r0, g0, b0, a0);
    else        SetVtx(&gVtx[1], right, top, 0, r1, g1, b1, a1);
    if (across) SetVtx(&gVtx[2], left, bottom, 0, r1, g1, b1, a1);
    else        SetVtx(&gVtx[2], left, bottom, 0, r0, g0, b0, a0);
    SetVtx(&gVtx[3], right, bottom, 0, r1, g1, b1, a1);
    Gfx2(0x0400103F, (u32)gVtx);                           /* gSPVertex: 4 */
    gVtx += 4;
    d = gDL;
    gDL = d + 4;
    d[0] = 0xB1000206; d[1] = 0x00000604;                 /* gSP2Triangles */
    d[2] = 0xE7000000; d[3] = 0;                          /* gDPPipeSync */
}

typedef struct HudRect { s32 x0, y0, x1, y1; } HudRect;

/* the colour of a fighter's health: red on low health once they have been on full health
 * after a round (B_800E0CB0/B4), otherwise their player colour */
static void HealthColour(cfCharacter *f, s32 full, u8 *c) {
    if (f->health < 41 && full != 0) {
        c[0] = 255; c[1] = 0; c[2] = 0;
    } else {
        c[0] = D_800ACD20[f->v2C8 * 3 + 0];
        c[1] = D_800ACD20[f->v2C8 * 3 + 1];
        c[2] = D_800ACD20[f->v2C8 * 3 + 2];
    }
}

/* the two-pixel bevelled frame round fighter 1's (left) bars; "open" leaves its top off
 * (when it sits right under the bar above) */
static void FrameLeft(HudRect *r, u8 a, s32 open, s32 skip) {
    r->x0--; r->y0 -= skip; r->x1++; r->y1++;
    if (!open) Hud_Gradient(r->x0, r->y0, 127, 127, 127, a, r->x1, r->y0, 50, 50, 100, a, 0);
    Hud_Gradient(r->x0, r->y0, 127, 127, 127, a, r->x0, r->y1, 50, 50, 100, a, 1);
    Hud_Gradient(r->x0, r->y1, 255, 255, 255, a, r->x1, r->y1, 100, 100, 200, a, 0);
    Hud_Gradient(r->x1, r->y0, 0x99, 0x99, 0x99, a, r->x1, r->y1, 100, 100, 200, a, 1);
    r->x0--; r->y0 -= skip; r->x1++; r->y1++;
    if (!open) Hud_Gradient(r->x0, r->y0, 255, 255, 255, a, r->x1, r->y0, 100, 100, 200, a, 0);
    Hud_Gradient(r->x0, r->y0 - open, 255, 255, 255, a, r->x0, r->y1, 100, 100, 200, a, 1);
    Hud_Gradient(r->x0, r->y1, 127, 127, 127, a, r->x1, r->y1, 50, 50, 100, a, 0);
    Hud_Gradient(r->x1, r->y0, 127, 127, 127, a, r->x1, r->y1, 50, 50, 100, a, 1);
}

/* the same for fighter 2's (right) bars, mirrored */
static void FrameRight(HudRect *r, u8 a, s32 open, s32 skip) {
    r->x0--; r->y0 -= skip; r->x1++; r->y1++;
    if (!open) Hud_Gradient(r->x1, r->y0, 127, 127, 127, a, r->x0, r->y0, 50, 50, 100, a, 0);
    Hud_Gradient(r->x1, r->y0, 127, 127, 127, a, r->x1, r->y1, 50, 50, 100, a, 1);
    Hud_Gradient(r->x1 + 1, r->y1, 255, 255, 255, a, r->x0, r->y1, 100, 100, 200, a, 0);
    Hud_Gradient(r->x0, r->y0, 0x99, 0x99, 0x99, a, r->x0, r->y1, 100, 100, 200, a, 1);
    r->x0--; r->y0 -= skip; r->x1++; r->y1++;
    if (!open) Hud_Gradient(r->x1, r->y0, 255, 255, 255, a, r->x0, r->y0, 100, 100, 200, a, 0);
    Hud_Gradient(r->x1, r->y0 - open, 255, 255, 255, a, r->x1, r->y1, 100, 100, 200, a, 1);
    Hud_Gradient(r->x1, r->y1, 127, 127, 127, a, r->x0, r->y1, 50, 50, 100, a, 0);
    Hud_Gradient(r->x0, r->y0, 127, 127, 127, a, r->x0, r->y1, 50, 50, 100, a, 1);
}

/* a bar filling w pixels from the left (or from the right), the rest black */
static void BarLeft(HudRect *r, s32 w, u8 *c, u8 a) {
    Hud_Gradient(r->x0, r->y0, c[0], c[1], c[2], a, r->x0 + w - 1, r->y1, c[0] >> 1, c[1] >> 1, c[2] >> 1, a, 1);
    Hud_Gradient(r->x0 + w, r->y0, 0, 0, 0, a, r->x1, r->y1, 0, 0, 0, a, 0);
}
static void BarRight(HudRect *r, s32 w, u8 *c, u8 a) {
    Hud_Gradient(r->x1 - w + 1, r->y0, c[0], c[1], c[2], a, r->x1, r->y1, c[0] >> 1, c[1] >> 1, c[2] >> 1, a, 1);
    Hud_Gradient(r->x0, r->y0, 0, 0, 0, a, r->x1 - w, r->y1, 0, 0, 0, a, 0);
}

static s32 BarWidth(s32 v, s32 scale, s32 max) {
    s32 w = v * scale / max;
    if (w == 0 && v > 0) w = 1;
    return w;
}

/* 0x800426F8: the HUD - health bars, dizzy bars and super meters (each with a bevelled
 * frame, sliding in from the sides and fading in), the portraits and the round timer */
void Hud_Draw(void) {
    HudRect r;
    u8 a, c[3];
    s32 y, x, i;
    f32 up[3], right[3], tup[3], tright[3], px;
    char text[2];

    if (gCombat->v54 < 3) return;
    a = (u8)(u32)(B_800E0CA4 * 255.0f);
    B_800F4E34 = 255; B_800F7770 = 255; B_800F7778 = 255; B_800F7E78 = 255;
    B_800F7789 = 255; B_800F7D94 = 255; B_800F2D1C = 255; B_800F88C8 = 255;
    B_800F7E70 = 255; B_800F7ECC = 255; B_800F88BC = 255; B_800F7788 = 255;
    B_800F7E58 = a; B_800F2D1D = a; B_800F8760 = a; B_800F5B78 = a;
    y = B_800E0CBC;
    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);

    if (gGame->showHealth != 0) {
        r.x0 = 0xBA - B_800E0CB8; r.x1 = r.x0 + 0x49; r.y0 = y; r.y1 = y + 3;
        HealthColour(gCombatFighter1, B_800E0CB0, c);
        BarLeft(&r, BarWidth(B_800E0CA8, 74, 170), c, a);
        FrameLeft(&r, a, 0, 1);
        r.x0 = B_800E0CB8 + 0x38; r.x1 = B_800E0CB8 + 0x81; r.y0 = y; r.y1 = y + 3;
        HealthColour(gCombatFighter2, B_800E0CB4, c);
        BarRight(&r, BarWidth(B_800E0CAC, 74, 170), c, a);
        FrameRight(&r, a, 0, 1);
        y += 6;
    }
    if (gGame->showDizzy != 0) {
        c[0] = 255; c[1] = 90; c[2] = 0;
        r.x0 = 0xBA - B_800E0CB8; r.x1 = r.x0 + 0x22; r.y0 = y; r.y1 = y + 2;
        BarLeft(&r, BarWidth(B_800E0CC0, 35, 200), c, a);
        FrameLeft(&r, a, gGame->showHealth != 0, gGame->showHealth == 0);
        r.x0 = B_800E0CB8 + 0x5F; r.x1 = B_800E0CB8 + 0x81; r.y0 = y; r.y1 = y + 2;
        BarRight(&r, BarWidth(B_800E0CC4, 35, 200), c, a);
        FrameRight(&r, a, gGame->showHealth != 0, gGame->showHealth == 0);
        y += 3;
    }
    if (gGame->showSuper != 0) {
        y += 5;
        /* the full segments first, then the flashing ones over them */
        for (i = 0, x = 0xC2 - B_800E0CB8; i < 3; i++, x += 14) {
            HudMeterSeg *m = &B_800F5AF0[i];
            m->w = 13; m->h = 3; m->x = x; m->y = y;
            if ((f64)m->flash == 1.0) Hud_DrawMeterSeg(m, 0);
        }
        for (i = 0; i < 3; i++)
            if (!((f64)B_800F5AF0[i].flash == 1.0)) Hud_DrawMeterSeg(&B_800F5AF0[i], 0);
        for (i = 0, x = B_800E0CB8 + 0x71; i < 3; i++, x -= 14) {
            HudMeterSeg *m = &B_800F88F8[i];
            m->w = 13; m->h = 3; m->x = x; m->y = y;
            if ((f64)m->flash == 1.0) Hud_DrawMeterSeg(m, 1);
        }
        for (i = 0; i < 3; i++)
            if (!((f64)B_800F88F8[i].flash == 1.0)) Hud_DrawMeterSeg(&B_800F88F8[i], 1);
    }
    if (gGame->showPortraits != 0) {
        func_8007EEF0(&gDL);
        func_8007F94C(&gDL);
        up[0] = 0.0f; up[1] = 0.8f; up[2] = 0.0f;
        right[0] = -0.8f; right[1] = 0.0f; right[2] = 0.0f;
        B_800F7D94 = 11; B_800F7778 = 11;
        B_800F7789 = 170; B_800F7770 = 170;
        B_800F7E78 = 206; B_800F4E34 = 206;
        B_800F7788 = 0; B_800F7E70 = 0;
        B_800F88BC = 30; B_800F88C8 = 30;
        B_800F7ECC = 100; B_800F2D1C = 100;
        px = (f32)(0.0 - ((f64)B_800E0CB8 + 6.0));
        func_80084CB0(&gDL, B_800F5B70, px, 65.0f, 0, up, right, 9);
        func_80083A64(&gDL, (void *)B_800F7D54, px, 65.0f, 0, up, right, 9);
        func_80084CB0(&gDL, B_800F2D48, px, 65.0f, 0, up, right, 9);
        px = (f32)((f64)B_800E0CB8 + 4.0 + 0.0);
        func_80084CB0(&gDL, B_800F5B70, px, 65.0f, 0, up, right, 8);
        func_80083A64(&gDL, (void *)B_800F8CA8, px, 65.0f, 0, up, right, 8);
        func_80084CB0(&gDL, B_800F2D48, px, 65.0f, 0, up, right, 8);
    }
    if (0.0f <= gCombat->v34) {                       /* the round timer */
        text[0] = 0; text[1] = 0;
        tup[0] = 0.0f; tup[1] = 0.5f; tup[2] = 0.0f;
        tright[0] = -0.5f; tright[1] = 0.0f; tright[2] = 0.0f;
        SetTexFilter(0);
        func_80082210(D_800ACCFC, &gDL, 0);
        text[0] = (s32)gCombat->v34 / 10 + '0';
        func_800824CC(D_800ACCFC, &gDL, text, (f32)(-9.0 - (f64)(func_80082298(D_800ACCFC, text) / 2) + 7.0), 87.0f,
                      0.0f, tup, tright, 8);
        text[0] = (s32)gCombat->v34 % 10 + '0';
        func_800824CC(D_800ACCFC, &gDL, text, (f32)(6.0 - (f64)(func_80082298(D_800ACCFC, text) / 2) + 7.0), 87.0f,
                      0.0f, tup, tright, 8);
        SetTexFilter(0x2000);
    }
}

/* a point of a filling segment's burst: angle a, radius scale k */
static void BurstPoint(Vtx16 *v, HudMeterSeg *m, f32 cx, f32 cy, s32 a, f32 t, f64 k, u8 c, u8 alpha) {
    f32 x, y;
    f64 r;
    r = (f64)t * k + 0.0;
    x = (f32)((f64)cx + (f64)vsSin(a) * ((f64)(f32)m->w * r));
    y = (f32)((f64)cy + (f64)vsCos(a) * ((f64)(f32)m->h * r));
    SetVtx(v, (s32)x, (s32)y, 0, c, c, 255, alpha);
}

/* one pixel of a full segment's frame, flashing white-blue when it has just filled */
static void MeterEdge(HudMeterSeg *m, HudRect *t, u8 a) {
    if ((f64)m->flash < 1.0)
        func_8007F3D4(&gDL, (s32 *)t, (u32)((f64)m->flash * 150.0) & 0xFF, (u32)((f64)m->flash * 150.0) & 0xFF,
                      (u32)((f64)m->flash * 255.0) & 0xFF, a);
    else
        func_8007F3D4(&gDL, (s32 *)t, (u32)((f64)m->flash * 150.0) & 0xFF, (u32)((f64)m->flash * 150.0) & 0xFF,
                      (u32)((f64)m->flash * 255.0) & 0xFF, (u32)((f64)m->flash * 255.0) & 0xFF);
}

/* 0x800440CC: a third of a super meter - the fill (pulsing blue) and the empty part, and
 * once full a frame and, as it fills, a burst of light */
void Hud_DrawMeterSeg(HudMeterSeg *m, s32 right) {
    u32 a;
    s32 w, k;
    f32 t, cx, cy, c, pulse;
    HudRect r, b;
    u8 rr, bb;

    a = (u32)(B_800E0CA4 * 255.0f);
    B_800F4E34 = 255; B_800F7770 = 255; B_800F7778 = 255; B_800F7E78 = 255;
    B_800F7789 = 255; B_800F7D94 = 255; B_800F2D1C = 255; B_800F88C8 = 255;
    B_800F7E70 = 255; B_800F7ECC = 255; B_800F88BC = 255; B_800F7788 = 255;
    B_800F7E58 = a; B_800F2D1D = a; B_800F8760 = a; B_800F5B78 = a;
    w = BarWidth(m->fill, m->w, 100);
    if (!(w < m->w) && m->fill < 100) w = m->w - 1;
    t = (f32)m->fill / 100.0f;
    if (w < m->w) t = (f32)((f64)t * 0.2);
    t = t * m->flash;
    if (!(m->fill < 100) && (f64)m->flash < 1.0) {
        func_8007EEF0(&gDL);
        func_8007F94C(&gDL);
        func_8007FB08(&gDL, 0x204);
        func_8007FB90(&gDL, 0x404240, 0x104240);
        func_8007FBE8(&gDL, 1);
        cx = (f32)(m->w / 2 + m->x) - (f32)(B_800F5D00 / 2);
        cy = (f32)(0.0 - (f64)((f32)(m->h / 2 + m->y) - (f32)(B_800F5BEC / 2)));
        c = (f32)((1.0 - (f64)m->flash) * 200.0);
        for (k = 0; k < 16; k++) {
            s32 a0 = k << 12, a1 = ((k + 1) % 16) << 12;
            u8 edge = (u32)((1.0 - (f64)t) * 250.0);
            u32 *d;
            BurstPoint(&gVtx[0], m, cx, cy, a0, t, 0.3, (u32)c, 0);
            BurstPoint(&gVtx[1], m, cx, cy, a0, t, 1.9, (u32)c, edge);
            BurstPoint(&gVtx[2], m, cx, cy, a0, t, 4.5, (u32)c, 0);
            BurstPoint(&gVtx[3], m, cx, cy, a1, t, 0.3, (u32)c, 0);
            BurstPoint(&gVtx[4], m, cx, cy, a1, t, 1.9, (u32)c, edge);
            BurstPoint(&gVtx[5], m, cx, cy, a1, t, 4.5, (u32)c, 0);
            Gfx2(0x0400185F, (u32)gVtx);                   /* gSPVertex: 6 */
            gVtx += 6;
            d = gDL;
            gDL = d + 4;
            d[0] = 0xB1000206; d[1] = 0x00080602;         /* gSP2Triangles */
            d[2] = 0xB1040802; d[3] = 0x000A0804;
        }
        Gfx2(0xE7000000, 0);
        func_8007EB48(&gDL);
    }
    r.x0 = m->x; r.y0 = m->y; r.x1 = m->x - 1 + m->w; r.y1 = m->y - 1 + m->h;
    if (right) {
        b.x0 = r.x1 - w + 1; b.y0 = r.y0; b.x1 = r.x1; b.y1 = r.y1;
    } else {
        b.x0 = r.x0; b.y0 = r.y0; b.x1 = r.x0 + w - 1; b.y1 = r.y1;
    }
    pulse = t * B_800F7A7C;
    rr = (u32)((f64)pulse * 40.0 + 40.0);
    bb = (u32)((f64)pulse * 40.0 + 200.0);
    func_8007F3D4(&gDL, (s32 *)&b, rr, rr, bb, a & 0xFF);       /* the fill */
    if (right) {
        b.x0 = r.x0; b.y0 = r.y0; b.x1 = r.x1 - w; b.y1 = r.y1;
        Hud_Gradient(b.x0 - 2, b.y0 - 1, 60, 60, 60, a & 0xFF, b.x1 - 2, b.y1 - 1, 120, 120, 120, a & 0xFF, 0);
    } else {
        b.x0 = r.x0 + w; b.y0 = r.y0; b.x1 = r.x1; b.y1 = r.y1;
        Hud_Gradient(b.x0 - 2, b.y0 - 1, 120, 120, 120, a & 0xFF, b.x1 - 2, b.y1 - 1, 60, 60, 60, a & 0xFF, 0);
    }
    if (m->fill < 100) return;
    r.x0--; r.y0--; r.x1++; r.y1++;
    b.x0 = r.x0; b.y0 = r.y0; b.x1 = r.x1; b.y1 = r.y0;
    MeterEdge(m, &b, a & 0xFF);
    b.x0 = r.x0; b.y0 = r.y1; b.x1 = r.x1; b.y1 = r.y1;
    MeterEdge(m, &b, a & 0xFF);
    b.x0 = r.x0; b.y0 = r.y0 + 1; b.x1 = r.x0; b.y1 = r.y1 - 1;
    MeterEdge(m, &b, a & 0xFF);
    b.x0 = r.x1; b.y0 = r.y0 + 1; b.x1 = r.x1; b.y1 = r.y1 - 1;
    MeterEdge(m, &b, a & 0xFF);
}

/* a banner letter: white, faded by the banner's alpha (drawn see-through while it fades) */
static void BannerLetter(ComboBanner *b, char *text, f32 *up, f32 *right) {
    B_800F4E34 = 255; B_800F7770 = 255; B_800F7778 = 255;
    B_800F5B78 = (u32)((f64)b->alpha * 255.0);
    B_800F7E78 = 255; B_800F7789 = 255; B_800F7D94 = 255;
    B_800F8760 = (u32)((f64)b->alpha * 255.0);
    B_800F2D1C = 255; B_800F88C8 = 255; B_800F7E70 = 255;
    B_800F2D1D = (u32)((f64)b->alpha * 255.0);
    B_800F7ECC = 255; B_800F88BC = 255; B_800F7788 = 255;
    B_800F7E58 = (u32)((f64)b->alpha * 255.0);
    if ((f64)b->alpha < 1.0)
        func_80082564(D_800ACCFC, &gDL, text, 0.0f, 0.0f, 0.0f, up, right, 8);
    else
        func_800824CC(D_800ACCFC, &gDL, text, 0.0f, 0.0f, 0.0f, up, right, 8);
}

/* 0x80046F7C: a combo banner - a wobbling, spinning star (16 points, alternating colours)
 * behind the two lines of text, at x, sliding out sideways in direction dir as it goes */
void ComboBanner_Draw(ComboBanner *b, f32 x, f32 dir) {
    f32 m[16], m2[16], up[3], right[3], px, sx, sy, a, sc;
    char text[2];
    s32 k, i;
    Vtx16 *v;

    text[0] = 0; text[1] = 0;
    if (D_800ACCFC == 0 || b->active == 0) return;
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    px = (f32)((f64)x + (f64)(dir * b->scale) * 90.0);
    func_80087EE8(m, px, 66.0f, 0.0f);
    func_80080170(&gDL, m);
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0x404240, 0x104240);
    func_8007FBE8(&gDL, 1);
    for (k = 0; k < 16; k++) {
        a = (f32)(k << 12);
        sx = vsSin((s32)(a - (f32)b->v538 * dir)) * 65.0f * b->v548;
        sy = vsCos((s32)(a - (f32)b->v538 * dir)) * 15.0f * b->v548;
        sx = (f32)((f64)sx * ((f64)(vsSin(b->v54C) * b->v554) + 1.0));
        sy = (f32)((f64)sy * ((f64)(vsSin(b->v54C) * b->v554) + 1.0));
        sx = (f32)((f64)sx * (1.0 - (f64)b->scale));
        sy = (f32)((f64)sy * (1.0 - (f64)b->scale));
        if (k & 1) {
            sx = (f32)((f64)sx * 1.2);
            sy = (f32)((f64)sy * 1.2);
        }
        v = &gVtx[k];
        v->x = (s32)sx; v->y = (s32)sy; v->z = 0;
        v->flag = 0; v->s = 0; v->t = 0;
        v->r = (k & 1) ? b->rgb2[0] : b->rgb1[0];
        v->g = (k & 1) ? b->rgb2[1] : b->rgb1[1];
        v->b = (k & 1) ? b->rgb2[2] : b->rgb1[2];
        v->a = (k & 1) ? (u8)(u32)((f64)b->alpha * 50.0) : 0;
    }
    SetVtx(&gVtx[16], 0, 0, 0, b->rgb1[0] >> 1, b->rgb1[1] >> 1, b->rgb1[2] >> 1, (u32)((f64)b->alpha * 255.0));
    Gfx2(0x0400450F, (u32)gVtx);                           /* gSPVertex: 17 */
    gVtx += 17;
    for (k = 0; k < 16; k++)                               /* gSP1Triangle: a fan round the centre */
        Gfx2(0xBF000000, (((((k + 1) % 16) * 2) & 0xFF) << 16) | 0x2000 | ((k * 2) & 0xFF));
    Gfx2(0xE7000000, 0);

    func_80087EE8(m2, px, 60.0f + 5.0f, 0.0f);
    func_80082210(D_800ACCFC, &gDL, 0);
    for (i = 0; i < b->line1.len; i++) {
        if (b->v4 != 0) {
            func_80087EE8(m, b->line1.x[i], (f32)((f64)b->line1.y[i] - 8.0), 0.0f);
            sc = 0.35f;
        } else {
            func_80087EE8(m, b->line1.x[i], b->line1.y[i], 0.0f);
            sc = 0.3f;
        }
        func_80088134(m, sc, sc, sc);
        func_80088100(m, m2);
        func_80080170(&gDL, m);
        text[0] = b->line1.text[i];
        BannerLetter(b, text, up, right);
    }
    func_80087EE8(m2, px, 60.0f - 5.0f, 0.0f);
    func_80082210(D_800ACCFC, &gDL, 0);
    for (i = 0; i < b->line2.len; i++) {
        func_80087EE8(m, b->line2.x[i], b->line2.y[i], 0.0f);
        func_80088134(m, 0.27f, 0.27f, 0.27f);
        func_80088100(m, m2);
        func_80080170(&gDL, m);
        text[0] = b->line2.text[i];
        BannerLetter(b, text, up, right);
    }
}

/* 0x80047B4C: the pause menu - the screen darkened, "pause" over "continue" and "quit" (or
 * "Confirm Quit" over "Yes" and "No"), the chosen option bigger and highlighted */
void Combat_DrawPause(void) {
    f32 m[16], up[3], right[3], gap;
    s32 rect[4], i;
    char text[2];

    text[0] = 0; text[1] = 0;
    if (D_800ACCFC == 0) return;
    if (B_800F5B68 == 0) {
        HudText_Set(&B_800F84D0, D_800ACCFC, "pause", 0);
        HudText_Set(&B_800F1348, D_800ACCFC, "continue", 1);
        HudText_Set(&B_800F7A90, D_800ACCFC, "quit", 2);
    } else {
        HudText_Set(&B_800F84D0, D_800ACCFC, "Confirm Quit", 0);
        HudText_Set(&B_800F1348, D_800ACCFC, "Yes", 1);
        HudText_Set(&B_800F7A90, D_800ACCFC, "No", 2);
    }
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    func_8007EB48(&gDL);
    rect[0] = 0; rect[1] = 0; rect[2] = B_800F5D00 - 1; rect[3] = B_800F5BEC - 1;
    func_8007F3D4(&gDL, rect, 0, 0, 0, 0x50);
    gap = (f32)func_80082298(D_800ACCFC, B_800F1348.text);
    gap = (f32)((f64)gap + 50.0 + (f64)(f32)func_80082298(D_800ACCFC, B_800F7A90.text));
    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);
    func_80082210(D_800ACCFC, &gDL, 0);
    for (i = 0; i < B_800F84D0.len; i++) {
        func_80087E98(m, 0.6f, 0.6f, 0.6f);
        func_80088184(m, 0.0f, 35.0f, 0.0f);
        func_80088350(m, B_800F84D0.x[i], B_800F84D0.y[i], 0.0f);
        func_80080170(&gDL, m);
        text[0] = B_800F84D0.text[i];
        func_800824CC(D_800ACCFC, &gDL, text, 0.0f, 0.0f, 0.0f, up, right, 8);
    }
    func_80082210(D_800ACCFC, &gDL, B_800F4E7C == 0);
    for (i = 0; i < B_800F1348.len; i++) {
        func_80087E98(m, (B_800F4E7C == 0) ? 0.45f : 0.4f, (B_800F4E7C == 0) ? 0.45f : 0.4f,
                      (B_800F4E7C == 0) ? 0.45f : 0.4f);
        func_80088184(m, 0.0f, 15.0f, 0.0f);
        func_80088350(m, B_800F1348.x[i] - gap / 2.0f, B_800F1348.y[i], 0.0f);
        func_80080170(&gDL, m);
        text[0] = B_800F1348.text[i];
        func_800824CC(D_800ACCFC, &gDL, text, 0.0f, 0.0f, 0.0f, up, right, 8);
    }
    func_80082210(D_800ACCFC, &gDL, B_800F4E7C == 1);
    for (i = 0; i < B_800F7A90.len; i++) {
        func_80087E98(m, (B_800F4E7C == 1) ? 0.45f : 0.4f, (B_800F4E7C == 1) ? 0.45f : 0.4f,
                      (B_800F4E7C == 1) ? 0.45f : 0.4f);
        func_80088184(m, 0.0f, 15.0f, 0.0f);
        func_80088350(m, gap / 2.0f + B_800F7A90.x[i], B_800F7A90.y[i], 0.0f);
        func_80080170(&gDL, m);
        text[0] = B_800F7A90.text[i];
        func_800824CC(D_800ACCFC, &gDL, text, 0.0f, 0.0f, 0.0f, up, right, 8);
    }
}

static void Px(s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    s32 e[4];
    e[0] = x0; e[1] = y0; e[2] = x1; e[3] = y1;
    func_8007F3D4(&gDL, e, r, g, b, a);
}

/* 0x80045E50: a two-pixel bevelled frame round a rectangle, raised or sunken, in a colour
 * and a third of it (unused) */
void Hud_Bevel(s32 *rect, s32 r, s32 g, s32 b, s32 a, s32 raised) {
    u8 cr = r, cg = g, cb = b, dr = cr / 3, dg = cg / 3, db = cb / 3;
    s32 x0 = rect[0] - 1, y0 = rect[1] - 1, x1 = rect[2] + 1, y1 = rect[3] + 1;
    if (raised) {
        Px(rect[0], y0, x1, y0, dr, dg, db, a);
        Px(x0, y1, x1, y1, cr, cg, cb, a);
        Px(x0, y0, x0, y1 - 1, cr, cg, cb, a);
        Px(x1, y0 + 1, x1, y1 - 1, dr, dg, db, a);
        x0--; y0--; x1++; y1++;
        Px(x0 + 1, y0, x1, y0, cr, cg, cb, a);
        Px(x0, y1, x1, y1, dr, dg, db, a);
        Px(x0, y0, x0, y1, dr, dg, db, a);
        Px(x1, y0, x1, y1 - 1, cr, cg, cb, a);
    } else {
        Px(x0, y0, x1 - 1, y0, dr, dg, db, a);
        Px(x0, y1, x1, y1, cr, cg, cb, a);
        Px(x0, y0 + 1, x0, y1 - 1, dr, dg, db, a);
        Px(x1, y0, x1, y1 - 1, cr, cg, cb, a);
        x0--; y0--; x1++; y1++;
        Px(x0, y0, x1 - 1, y0, cr, cg, cb, a);
        Px(x0, y1, x1, y1, dr, dg, db, a);
        Px(x0, y0 + 1, x0, y1 - 1, cr, cg, cb, a);
        Px(x1, y0, x1, y1 - 1, dr, dg, db, a);
    }
}
