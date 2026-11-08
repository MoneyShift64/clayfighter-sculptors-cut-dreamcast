/*
 * The full-screen fades (0x8002A340-0x8002C688), shared by every screen. gScreenFade
 * (0x800F8DB8) is the main one; the fight has a second one (0x800E0E00), the intro its own.
 *
 * A fade goes from level 0 to 1 (fading in: speed +1) or 1 to 0 (fading out: speed -1). Types:
 *   0 none, 1 plain (a colour, or the screen grab, see-through by level), 2 a wipe from the
 *   left / to the right, 3 an iris (a ring of 16 spokes closing in), 4 horizontal blinds,
 *   5 vertical blinds, 6 the screen grab shattering into 32x32 shards that fall away.
 * The screen grab (+0x18, = 0x800F8DD0 for gScreenFade) is an image the screen code makes of
 * the last picture (func_80081804) before starting the fade; the fade owns it and frees it.
 */
#include "common.h"
#include "mpfplat.h"
#include "combat.h"

typedef struct FadeShard {                 /* one piece of the shattering grab (0x120 bytes) */
    /* 0x000 */ f32 m1[16];                /* top-left triangle: its matrix, */
    /* 0x040 */ f32 spin1[16];             /* ... the turn it makes each tick, */
    /* 0x080 */ f32 m2[16];                /* bottom-right triangle, */
    /* 0x0C0 */ f32 spin2[16];
    /* 0x100 */ s32 x, y;                  /* where it is in the grab (pixels) */
    /* 0x108 */ f32 vel1[3];               /* how each triangle moves each tick */
    /* 0x114 */ f32 vel2[3];
} FadeShard;

typedef struct ScreenFadeFull {            /* ScreenFade (combat.h) with the two pointers */
    /* 0x00 */ s32 active;
    /* 0x04 */ s32 music;                  /* fade the music with it */
    /* 0x08 */ s32 type;
    /* 0x0C */ f32 level;                  /* 0 = covered .. 1 = clear */
    /* 0x10 */ f32 speed;                  /* +1 fading in, -1 fading out */
    /* 0x14 */ u8  rgb[3];
    /* 0x18 */ u8 *grab;                   /* the screen grab (an image), or NULL */
    /* 0x1C */ FadeShard *shards;          /* type 6 */
} ScreenFadeFull;

void func_8007AB70(f32 volume);                         /* music volume */
void func_800796C0(void);                               /* stop the music */
void func_800881D4(void *m, s32 angle);                 /* rotate about x */
void func_800882C4(void *m, void *by);                  /* m = m * by */
void func_80087774(f32 *m3, s32 angle);                 /* a 2D (3x3) rotation */
void func_8007F2A8(u32 **dl, s32 *rect, s32 r, s32 g, s32 b);   /* a filled rectangle */
void func_80080590(u32 **dl, void *image, s32 *rect, s32 a3, s32 a4, s32 a5);   /* part of an image */

#define RAND() ((s32)guRandom())          /* (30 bits) */

static void FreeBuffers(ScreenFadeFull *f) {
    if (f->grab != NULL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, f->grab);
        f->grab = NULL;
    }
    if (f->shards != NULL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, f->shards);
        f->shards = NULL;
    }
}

/* one triangle's random turn and drift */
static void ShardSpin(f32 *spin, f32 *vel) {
    func_80087EE8(spin, -16.0f, 16.0f, 0.0f);
    func_800881D4(spin, RAND() % 6000 - 3000);
    func_80088224(spin, RAND() % 6000 - 3000);
    func_80088184(spin, 16.0f, -16.0f, 0.0f);
    vel[0] = (f32)((f64)(f32)(RAND() % 2048) / 512.0 - 1.0);
    vel[1] = (f32)((f64)(f32)(RAND() % 2048) / 512.0 - 0.0);
    vel[2] = (f32)((f64)(f32)(RAND() % 4096) / 1024.0);
}

/* 0x8002A340: Fade_Start - start fading in (level 0 -> 1) */
void func_8002A340(ScreenFade *fade, s32 type) {
    ScreenFadeFull *f = (ScreenFadeFull *)fade;
    s32 cols, rows, r, c, i;
    FadeShard *sh, *s;

    f->active = 1;
    f->type = type;
    f->level = 0.0f;
    f->speed = 1.0f;
    FreeBuffers(f);
    if (f->music != 0) func_8007AB70(1.0f);
    if (type != 6) return;
    cols = ((B_800F5D00 & 0x1F) != 0) + B_800F5D00 / 32;
    rows = ((B_800F5BEC & 0x1F) != 0) + B_800F5BEC / 32;
    sh = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, cols * rows * sizeof(FadeShard), 0);
    f->shards = sh;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            i = r * cols + c;
            s = &sh[i];
            func_80087EE8(s->m1, (f32)(c * 32 - B_800F5D00 / 2), (f32)(-(r * 32) + B_800F5BEC / 2), 0.0f);
            func_80087EE8(s->m2, (f32)(c * 32 - B_800F5D00 / 2), (f32)(-(r * 32) + B_800F5BEC / 2), 0.0f);
            s->x = c * 32;
            s->y = r * 32;
            ShardSpin(s->spin1, s->vel1);
            ShardSpin(s->spin2, s->vel2);
        }
    }
}

/* 0x8002A990: Fade_StartOut - start fading out (level 1 -> 0) */
void func_8002A990(ScreenFade *fade, s32 type) {
    ScreenFadeFull *f = (ScreenFadeFull *)fade;

    f->active = 1;
    f->type = type;
    f->level = 1.0f;
    f->speed = -1.0f;
    FreeBuffers(f);
}

/* 0x8002AA14: Fade_Step - move the level on by speed (once a tick); the music follows a fade
 * out, and stops when it reaches 0; the shards fall */
void func_8002AA14(ScreenFade *fade, f32 speed) {
    ScreenFadeFull *f = (ScreenFadeFull *)fade;
    f32 old = f->level;
    FadeShard *sh, *s;
    u32 n;
    s32 i;

    f->level = speed * f->speed + old;
    if ((f64)f->level < 0.0) f->level = 0.0f;
    if (1.0 < (f64)f->level) f->level = 1.0f;
    if (f->music != 0 && (f64)f->speed < 0.0) {
        func_8007AB70(f->level);
        if ((f64)f->level == 0.0 && 0.0 < (f64)old) {
            func_8007A530();
            func_800796C0();
        }
    }
    if (f->type != 6) return;
    sh = f->shards;
    if (sh == NULL) return;
    n = mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, sh) / sizeof(FadeShard);
    for (i = 0; i < (s32)n; i++) {
        s = &sh[i];
        func_800882C4(s->m1, s->spin1);
        func_80088184(s->m1, s->vel1[0], s->vel1[1], s->vel1[2]);
        s->vel1[1] = (f32)((f64)s->vel1[1] - 0.07);     /* gravity */
        func_800882C4(s->m2, s->spin2);
        func_80088184(s->m2, s->vel2[0], s->vel2[1], s->vel2[2]);
        s->vel2[1] = (f32)((f64)s->vel2[1] - 0.07);
    }
}

/* 0x8002AC24: Fade_Stop */
void func_8002AC24(ScreenFade *fade) {
    ScreenFadeFull *f = (ScreenFadeFull *)fade;

    f->active = 0;
    FreeBuffers(f);
}

/* ---------------------------------------------------------------- drawing */

static void Cmd(u32 **dl, u32 w0, u32 w1) {
    u32 *g = *dl;
    *dl = g + 2;
    g[0] = w0;
    g[1] = w1;
}

static void SetVtx(Vtx16 *v, s32 x, s32 y, s32 s, s32 t, u8 r, u8 g, u8 b, u8 a) {
    v->x = x;
    v->y = y;
    v->z = 0;
    v->flag = 0;
    v->s = s;
    v->t = t;
    v->r = r;
    v->g = g;
    v->b = b;
    v->a = a;
}

/* the iris: 12 vertices, the spoke from p0 (clear) to p1 (covered) and out to the edge, turned
 * four ways */
static void IrisVerts(Vtx16 *v, ScreenFadeFull *f, f32 *p0, f32 *p1) {
    const f32 zero = 0.0f;
    u8 r = f->rgb[0], g = f->rgb[1], b = f->rgb[2];

    SetVtx(&v[0], (s32)p0[0], (s32)p0[1], 0, 0, r, g, b, 0);
    SetVtx(&v[1], (s32)p1[0], (s32)p1[1], 0, 0, r, g, b, 255);
    SetVtx(&v[2], 160, (s32)p1[1], 0, 0, r, g, b, 255);
    SetVtx(&v[3], (s32)p0[1], (s32)(zero - p0[0]), 0, 0, r, g, b, 0);
    SetVtx(&v[4], (s32)p1[1], (s32)(zero - p1[0]), 0, 0, r, g, b, 255);
    SetVtx(&v[5], 160, (s32)(zero - p1[0]), 0, 0, r, g, b, 255);
    SetVtx(&v[6], (s32)(zero - p0[0]), (s32)(zero - p0[1]), 0, 0, r, g, b, 0);
    SetVtx(&v[7], (s32)(zero - p1[0]), (s32)(zero - p1[1]), 0, 0, r, g, b, 255);
    SetVtx(&v[8], -160, (s32)(zero - p1[1]), 0, 0, r, g, b, 255);
    SetVtx(&v[9], (s32)(zero - p0[1]), (s32)p0[0], 0, 0, r, g, b, 0);
    SetVtx(&v[10], (s32)(zero - p1[1]), (s32)p1[0], 0, 0, r, g, b, 255);
    SetVtx(&v[11], -160, (s32)p1[0], 0, 0, r, g, b, 255);
}

static void Turn(f32 *p, f32 *m) {         /* p = p * m (2D, 3x3) */
    f32 x = (f32)((f64)(p[0] * m[0] + p[1] * m[3]) + (f64)m[6]);
    f32 y = (f32)((f64)(p[0] * m[1] + p[1] * m[4]) + (f64)m[7]);
    p[0] = x;
    p[1] = y;
}

static void FillRect(u32 **dl, ScreenFadeFull *f, s32 *rect) {
    func_8007F2A8(dl, rect, f->rgb[0], f->rgb[1], f->rgb[2]);
}

/* type 1 */
static void DrawPlain(ScreenFadeFull *f, u32 **dl) {
    s32 rect[4];
    f32 up[3], right[3];
    f32 a;

    if (f->grab == NULL) {
        func_8007EB48(dl);
        rect[0] = 0;
        rect[1] = 0;
        rect[2] = B_800F5D00 - 1;
        rect[3] = B_800F5BEC - 1;
        func_8007F3D4(dl, rect, f->rgb[0], f->rgb[1], f->rgb[2], (u8)(u32)((1.0 - (f64)f->level) * 255.0));
        return;
    }
    a = (f32)((1.0 - (f64)f->level) * 255.0);       /* the grab, see-through */
    up[0] = 0.0f;
    up[1] = 1.0f;
    up[2] = 0.0f;
    right[0] = -1.0f;
    right[1] = 0.0f;
    right[2] = 0.0f;
    B_800F7778 = 255; B_800F7770 = 255; B_800F4E34 = 255; B_800F5B78 = (u32)a;
    B_800F7D94 = 255; B_800F7789 = 255; B_800F7E78 = 255; B_800F8760 = (u32)a;
    B_800F7E70 = 255; B_800F88C8 = 255; B_800F2D1C = 255; B_800F2D1D = (u32)a;
    B_800F7788 = 255; B_800F88BC = 255; B_800F7ECC = 255; B_800F7E58 = (u32)a;
    func_8007EEF0(dl);
    func_8007F94C(dl);
    Cmd(dl, 0xBA000C02, 0);                 /* point sampling */
    func_80085FE4(dl, (CharFrame *)f->grab, -161.0f, 120.0f, 0.0f, up, right, 8);
    Cmd(dl, 0xBA000C02, 0x2000);            /* bilinear */
}

/* type 2 */
static void DrawWipe(ScreenFadeFull *f, u32 **dl) {
    s32 rect[4];

    func_8007EB48(dl);
    if (0.0 <= (f64)f->speed) {
        rect[0] = (s32)((f32)B_800F5D00 * f->level);
        rect[1] = 0;
        rect[2] = B_800F5D00 - 1;
    } else {
        rect[0] = 0;
        rect[1] = 0;
        rect[2] = (s32)((f64)(f32)B_800F5D00 * (1.0 - (f64)f->level));
    }
    rect[3] = B_800F5BEC - 1;
    if (f->grab != NULL) func_80080590(dl, f->grab, rect, 0, 0, 0);
    else FillRect(dl, f, rect);
}

/* type 3 */
static void DrawIris(ScreenFadeFull *f, u32 **dl) {
    s32 rect[4];
    f32 p0[2], p1[2];
    f32 m[9];
    f32 outer, inner;
    s32 i;

    outer = (f32)((f64)f->level * 155.0);
    inner = (f32)((f64)outer - 20.0);
    if ((f64)inner < 0.0) inner = 0.0f;
    func_8007EB48(dl);
    rect[0] = 0;                            /* above and below the ring */
    rect[1] = 0;
    rect[2] = 319;
    rect[3] = (s32)(120.0f - outer);
    if (rect[3] >= 0) FillRect(dl, f, rect);
    rect[0] = 0;
    rect[1] = (s32)(outer + 120.0f - 1.0f);
    rect[2] = 319;
    rect[3] = 239;
    if (!(239 < rect[1])) FillRect(dl, f, rect);
    Cmd(dl, 0xE7000000, 0);
    func_8007EEF0(dl);
    func_8007F94C(dl);
    func_8007FB08(dl, 0x204);
    func_8007FB90(dl, 0x0C084000, 0x03024000);
    func_8007FBE8(dl, 1);
    p0[0] = 0.0f;
    p0[1] = inner;
    p1[0] = 0.0f;
    p1[1] = outer;
    func_80087774(m, 0x800);
    for (i = 0; i < 8; i++) {
        IrisVerts(gVtx, f, p0, p1);
        Turn(p0, m);
        Turn(p1, m);
        IrisVerts(gVtx + 12, f, p0, p1);
        Cmd(dl, 0x0400617F, (u32)gVtx);    /* 24 vertices */
        func_8007FB90(dl, 0x0C084000, 0x03024000);
        Cmd(dl, 0xB102041C, 0x00021C1A);    /* the soft inner edge */
        Cmd(dl, 0xB1080A22, 0x00082220);
        Cmd(dl, 0xB10E1028, 0x000E2826);
        Cmd(dl, 0xB114162E, 0x00142E2C);
        Cmd(dl, 0xE7000000, 0);
        func_8007FB90(dl, 0x00404240, 0x00104240);
        Cmd(dl, 0xB100021A, 0x00001A18);    /* the solid outside */
        Cmd(dl, 0xB1060820, 0x0006201E);
        Cmd(dl, 0xB10C0E26, 0x000C2624);
        Cmd(dl, 0xB112142C, 0x00122C2A);
        Cmd(dl, 0xE7000000, 0);
        gVtx += 24;
    }
}

/* types 4 and 5 */
static void DrawBlinds(ScreenFadeFull *f, u32 **dl, s32 across) {
    s32 rect[4];
    s32 h, k;

    h = (s32)((1.0 - (f64)f->level) * 10.0);
    func_8007EB48(dl);
    if (!across) {
        for (k = 9; k < B_800F5BEC + 9; k += 19) {
            rect[0] = 0;
            rect[1] = k + 1 - h;
            rect[2] = 319;
            rect[3] = k - 1 + h;
            if (f->grab != NULL) func_80080590(dl, f->grab, rect, 0, 0, 0);
            else FillRect(dl, f, rect);
        }
    } else {
        for (k = 9; k < B_800F5D00 + 9; k += 19) {
            rect[0] = k + 1 - h;
            rect[1] = 0;
            rect[2] = k - 1 + h;
            rect[3] = 239;
            FillRect(dl, f, rect);
        }
    }
}

/* type 6 */
static void DrawShards(ScreenFadeFull *f, u32 **dl) {
    FadeShard *sh = f->shards, *s;
    f32 m[16];
    u32 n;
    s32 i, left, right;
    u8 *img;
    Vtx16 *v;

    if (f->grab == NULL || sh == NULL) return;
    n = mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, sh) / sizeof(FadeShard);
    func_80087EE8(m, 0.0f, -1.0f, 407.0f);
    func_8008861C(m);
    func_8008004C(dl, m);
    func_8007EEF0(dl);
    func_8007FB08(dl, 0x205);
    func_8007FB90(dl, 0x00442230, 0x00112230);
    func_8007FBE8(dl, 2);
    Cmd(dl, 0xB9000002, 0);
    Cmd(dl, 0xBA000C02, 0x2000);
    for (i = 0; i < (s32)n; i++) {
        s = &sh[i];
        img = f->grab;                      /* its 32x32 piece of the grab, as a texture */
        Cmd(dl, 0xFD100000 | ((*(s16 *)(img + 6) - 1) & 0xFFF),
            (u32)(img + 0x10 + *(s16 *)(img + 6) * s->y * 2 + s->x * 2) & 0x1FFFFFFF);
        Cmd(dl, 0xF5101000, 0x07000000);
        Cmd(dl, 0xE6000000, 0);
        Cmd(dl, 0xF4000000, 0x0707C07C);
        Cmd(dl, 0xE7000000, 0);
        Cmd(dl, 0xF5101000, 0);
        Cmd(dl, 0xF2000000, 0x0007C07C);
        left = -(s->x < B_800F5D00 / 2);
        right = (s->x < B_800F5D00 / 2 - 32) ? 31 : 32;
        func_80080170(dl, s->m1);
        v = gVtx;
        SetVtx(&v[0], left, 0, 0, 0, 255, 255, 255, 255);
        SetVtx(&v[1], right, 0, 0x400, 0, 255, 255, 255, 255);
        SetVtx(&v[2], left, -32, 0, 0x400, 255, 255, 255, 255);
        Cmd(dl, 0x04000C2F, (u32)gVtx);
        gVtx += 3;
        func_80080170(dl, s->m2);
        v = gVtx;
        SetVtx(&v[0], left, -32, 0, 0x400, 255, 255, 255, 255);
        SetVtx(&v[1], right, 0, 0x400, 0, 255, 255, 255, 255);
        SetVtx(&v[2], right, -32, 0x400, 0x400, 255, 255, 255, 255);
        Cmd(dl, 0x04060C2F, (u32)gVtx);
        gVtx += 3;
        Cmd(dl, 0xB1000204, 0x000A0608);
        Cmd(dl, 0xE7000000, 0);
    }
}

/* 0x8002AC80: Fade_Draw */
void func_8002AC80(ScreenFade *fade, u32 **dl) {
    ScreenFadeFull *f = (ScreenFadeFull *)fade;

    if (f->active == 0) return;
    switch ((u32)f->type) {
    case 1: DrawPlain(f, dl); break;
    case 2: DrawWipe(f, dl); break;
    case 3: DrawIris(f, dl); break;
    case 4: DrawBlinds(f, dl, 0); break;
    case 5: DrawBlinds(f, dl, 1); break;
    case 6: DrawShards(f, dl); break;
    }
}
/* (0x8002C670-0x8002C688: 6 orphaned instructions after Fade_Draw, unreachable: they zero
 * five words at a0 - probably a dead little function with no symbol) */
