/*
 * The arena's 3D "ground" renderer (0x800245B0-0x8002A340): the round floor of the arenas
 * built from geometry rather than from the background bank - a disc with a scrolling texture
 * lit by a glow, a low dome over it, a backdrop, an optional sun - and the texture loader they
 * share. Also three helpers of the 3D "stage" (0x8002EA90, the arena shown outside a fight):
 * set-up/free of its floor-shadow picture and the drawing of its floor and fighter shadows.
 *
 * The ground state lives at B_800F5BF0 (one per game, set up by the arena loader
 * func_8000AF40 and drawn by func_8000B998 / func_8000C07C).
 *
 * Globals are the disassembly's labels (B_ = BSS, D_ = data).
 */
#include "common.h"
#include "mpfplat.h"
#include "combat.h"

/* ---- the arena ground (0xDC bytes, B_800F5BF0) ---- */
typedef struct ArenaGround {
    /* 0x00 */ s32 mode;              /* 0 nothing, 1 floor + dome + backdrop, 2 floor + dome, 3 nothing drawn */
    /* 0x04 */ u8 *tex[4];            /* 0 dome, 1 floor, 2 floor overlay, 3 backdrop (images loaded from ROM) */
    /* 0x14 */ f32 mtx[16];           /* the ground's turn about y */
    /* 0x54 */ u8  glowRgb[4];        /* dome colour far from the glow centre */
    /* 0x58 */ u8  nearRgb[4];        /* ... and at the glow centre */
    /* 0x5C */ f32 glowRadius;        /* x 500 */
    /* 0x60 */ f32 domeTexScale;
    /* 0x64 */ s32 domeScroll[2];     /* texture offsets s, t (wrap at 2048) */
    /* 0x6C */ s32 domeScrollSpeed[2];
    /* 0x74 */ s32 sun;               /* 1: draw the sun */
    /* 0x78 */ f32 glowDist;          /* the glow centre: (0, 500 * glowDist) */
    /* 0x7C */ u8  rimRgb[4];         /* floor: rim colour (only r is used, for r, g and b) */
    /* 0x80 */ u8  centreRgb[4];      /* floor: colour of vertex 0 */
    /* 0x84 */ f32 floorTexScale;
    /* 0x88 */ s32 floorScroll[2];
    /* 0x90 */ s32 floorScrollSpeed[2];
    /* 0x98 */ u8  overlayRgb[4];
    /* 0x9C */ f32 overlayTexScale;
    /* 0xA0 */ f32 backZ0, backZ1, backX0, backX1; /* the backdrop's last four outline points */
    /* 0xB0 */ s32 shadows;           /* 1: draw two black triangles over the overlay */
    /* 0xB4 */ u8  backRgb[4][4];     /* backdrop corner colours: top left, top right, bottom left, bottom right */
    /* 0xC4 */ f32 sunPos[3];         /* where the sun is (500 away) */
    /* 0xD0 */ f32 sunScreen[3];      /* ... and where that is on the screen (centred, y up) */
} ArenaGround;

extern u32 D_800AC3F0[], D_800AC400[];   /* ROM [start, end) of the dome textures */
extern u32 D_800AC410[], D_800AC414[];   /* ... floor textures */
extern u32 D_800AC418[], D_800AC428[];   /* ... floor overlay textures */
extern u32 D_800AC438[], D_800AC43C[];   /* ... the backdrop texture */
extern f32 D_800AC440[32];               /* 16 points round a unit circle (x, z) */
extern f32 D_800AC4C0[20];               /* the backdrop's outline (10 points x, z) */
extern s8  D_800AC510[16][3];            /* floor triangles, mode 1 */
extern s8  D_800AC540[20][3];            /* floor triangles, mode 2 */
extern s8  D_800AC57C[12][3];            /* overlay triangles */
extern s8  D_800AC5A0[2][3];             /* the two black triangles */
extern f32 D_800AC5A8[40];               /* backdrop vertices (u, v) */
extern s8  D_800AC684[18][3];            /* backdrop triangles */
extern f32 D_800AC6BC[3];                /* dome rings: radius */
extern f32 D_800AC6C8[3];                /* ... height */
extern f32 D_800AC6D4;                   /* the dome's top */
extern s8  D_800AC6D8[16][6];            /* dome triangles (pairs) */
extern s8  D_800AC738[3];                /* ... how many pairs per ring (mode 1) */

extern s32 B_800F5D00, B_800F5BEC;
f32  func_80087630(f32 *v);                     /* length of (v[0], v[1]) */
void func_800882C4(void *m, void *n);           /* m = n x m */

void func_800245B0(ArenaGround *g, s32 mode, s32 angle, s32 domeTex, s32 floorTex, s32 overlayTex);
void func_80024904(ArenaGround *g);
void func_80024994(ArenaGround *g, u32 **dl, void *cam);
void func_80028E20(u32 **dl, u8 *img);

static void Dl2(u32 **dl, u32 w0, u32 w1) {
    u32 *p = *dl;
    *dl = p + 2;
    p[0] = w0;
    p[1] = w1;
}

/* the gSP1Triangle index packing with its "flag" (the game passes the first vertex as the
 * flag, which only rotates the triangle) */
#define TRI_W1(v0, v1, v2) ((u32)((((v0) * 2) & 0xFF) << 16) | (u32)((((v1) * 2) & 0xFF) << 8) | (u32)(((v2) * 2) & 0xFF))
#define TRI_W1F(v0, v1, v2, f) ((f) == 0 ? TRI_W1(v0, v1, v2) : (f) == 1 ? TRI_W1(v1, v2, v0) : TRI_W1(v2, v0, v1))

static void Tris(u32 **dl, s8 (*t)[3], s32 n) {
    s32 i;
    for (i = 0; i < n; i++) Dl2(dl, 0xBF000000, TRI_W1F(t[i][0], t[i][1], t[i][2], t[i][0]));
}

/* the ROM image [start, end) loaded into a new block */
static u8 *LoadRom(u32 start, u32 end) {
    u8 *p = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, end - start, 0);
    RomCopy(p, start, end - start);
    return p;
}

/* 0x800245B0: ArenaGround_Init - the defaults, the turn, and the textures (index < 0: none).
 * The backdrop texture is always number 0 (the original's index is a constant 0). */
void func_800245B0(ArenaGround *g, s32 mode, s32 angle, s32 domeTex, s32 floorTex, s32 overlayTex) {
    g->mode = mode;
    g->tex[0] = NULL;
    g->tex[1] = NULL;
    g->tex[2] = NULL;
    g->tex[3] = NULL;
    func_80087FD0(g->mtx, angle);
    g->glowRgb[0] = 50; g->glowRgb[1] = 50; g->glowRgb[2] = 50;
    g->nearRgb[0] = 255; g->nearRgb[1] = 20; g->nearRgb[2] = 0;
    g->glowRadius = 0.7f;
    g->domeTexScale = 0.1f;
    g->domeScroll[0] = 0;
    g->domeScroll[1] = 0;
    g->domeScrollSpeed[0] = 1;
    g->domeScrollSpeed[1] = 1;
    g->sun = 1;
    g->glowDist = 1.0f;
    g->rimRgb[0] = 50; g->rimRgb[1] = 50; g->rimRgb[2] = 50;
    g->centreRgb[0] = 180; g->centreRgb[1] = 80; g->centreRgb[2] = 0;
    g->floorTexScale = 0.3f;
    g->floorScroll[0] = 0;
    g->floorScroll[1] = 0;
    g->floorScrollSpeed[0] = 0;
    g->floorScrollSpeed[1] = 1;
    g->overlayRgb[0] = 80; g->overlayRgb[1] = 80; g->overlayRgb[2] = 80;
    g->backZ0 = 0.015f;
    g->backZ1 = -0.01f;
    g->backX0 = -0.015f;
    g->backX1 = 0.015f;
    g->overlayTexScale = 2.0f;
    g->backRgb[0][0] = 230; g->backRgb[0][1] = 190; g->backRgb[0][2] = 190;
    g->backRgb[1][0] = 180; g->backRgb[1][1] = 145; g->backRgb[1][2] = 145;
    g->backRgb[2][0] = 100; g->backRgb[2][1] = 100; g->backRgb[2][2] = 100;
    g->backRgb[3][0] = 20; g->backRgb[3][1] = 20; g->backRgb[3][2] = 20;
    g->shadows = 0;
    g->sunPos[0] = 0.0f;
    g->sunPos[1] = 0.0f;
    g->sunPos[2] = 500.0f;
    g->sunScreen[0] = 0.0f;
    g->sunScreen[1] = 0.0f;
    g->sunScreen[2] = 0.0f;
    if (domeTex >= 0) g->tex[0] = LoadRom(D_800AC3F0[domeTex], D_800AC400[domeTex]);
    if (floorTex >= 0) g->tex[1] = LoadRom(D_800AC410[floorTex], D_800AC414[floorTex]);
    if (overlayTex >= 0) g->tex[2] = LoadRom(D_800AC418[overlayTex], D_800AC428[overlayTex]);
    g->tex[3] = LoadRom(D_800AC438[0], D_800AC43C[0]);
}

/* 0x80024904: ArenaGround_Free */
void func_80024904(ArenaGround *g) {
    if (g->tex[0] != NULL) { mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, g->tex[0]); g->tex[0] = NULL; }
    if (g->tex[1] != NULL) { mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, g->tex[1]); g->tex[1] = NULL; }
    if (g->tex[2] != NULL) { mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, g->tex[2]); g->tex[2] = NULL; }
    if (g->tex[3] != NULL) { mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, g->tex[3]); g->tex[3] = NULL; }
}

/* a scroll offset kept within 0..2048 (the original "wraps" a negative one to 2048 - v) */
static void WrapScroll(s32 *v, s32 speed) {
    *v += speed;
    if ((f64)*v < 0.0) *v = (s32)(2048.0 - (f64)*v);
    if (2048.0 < (f64)*v) *v = (s32)((f64)*v - 2048.0);
}

/* one colour channel: near at t = 1, far at t = 0, scaled from 0..1 to 0..255 */
static u32 GlowChannel(f32 t, f32 nearC, f32 farC) {
    return (u32)(((f64)(t * nearC) + (1.0 - (f64)t) * (f64)farC) * 255.0);
}

typedef struct GlowColours {
    f32 glow[3];     /* glowRgb / 255 */
    f32 base[3];     /* nearRgb / 255 */
    f32 cx, cz;      /* the glow centre */
    f32 radius;
} GlowColours;

/* how far (x, z) is from the glow centre, 0..1 */
static f32 GlowT(GlowColours *c, f32 x, f32 z) {
    f32 d[2];
    f32 t;
    d[0] = x - c->cx;
    d[1] = z - c->cz;
    t = func_80087630(d) / c->radius;
    if ((f64)t > 1.0) t = 1.0f;
    return t;
}

/* the floor's vertices 0..17: a ring of 16 (radius 500) and two points either side of vertex 0 */
static void FloorRing(ArenaGround *g, f32 *a) {
    s32 i;
    Vtx16 *v;
    for (i = 0; i < 16; i++) {
        f32 x = D_800AC440[i * 2] * 500.0f;
        f32 z = D_800AC440[i * 2 + 1] * 500.0f;
        if (i == 0) {
            a[0] = (f32)((f64)x - 50.0);
            a[1] = z;
            a[2] = (f32)((f64)x + 50.0);
            a[3] = z;
        }
        v = &gVtx[i];
        v->x = (s32)x;
        v->y = 0;
        v->z = (s32)z;
        v->flag = 0;
        v->s = (s32)((f64)(x * g->floorTexScale) * 32.0) + (u16)g->floorScroll[0];
        v->t = (s32)((f64)(z * g->floorTexScale) * 32.0) + (u16)g->floorScroll[1];
        if (i == 0) {
            gVtx[0].r = g->centreRgb[0];
            gVtx[0].g = g->centreRgb[1];
            gVtx[0].b = g->centreRgb[2];
        } else {
            gVtx[i].r = g->rimRgb[0];
            gVtx[i].g = g->rimRgb[1];
            gVtx[i].b = g->rimRgb[2];
        }
        gVtx[i].a = 255;
    }
}

/* a floor vertex in rim colour (r used for r, g and b) */
static void RimVertex(ArenaGround *g, Vtx16 *v, s32 x, s32 z, s32 s, s32 t) {
    v->x = x;
    v->y = 0;
    v->z = z;
    v->flag = 0;
    v->s = s + (u16)g->floorScroll[0];
    v->t = t + (u16)g->floorScroll[1];
    v->r = g->rimRgb[0];
    v->g = g->rimRgb[0];
    v->b = g->rimRgb[0];
    v->a = 255;
}

/* the dome (both modes): the floor ring's colours and dome texture coordinates, three rings
 * and the top. `mode2` raises the rings by half and always draws 16 pairs per ring. */
static void Dome(ArenaGround *g, u32 **dl, GlowColours *c, s32 mode2) {
    s32 i, k, slot, other;
    f32 t;
    Vtx16 *v;
    for (i = 0; i < 16; i++) {
        f32 x = D_800AC440[i * 2] * 500.0f;
        f32 z = D_800AC440[i * 2 + 1] * 500.0f;
        u32 rgba;
        t = GlowT(c, x, z);
        rgba = GlowChannel(t, c->glow[0], c->base[0]) << 24;
        rgba |= GlowChannel(t, c->glow[1], c->base[1]) << 16;
        rgba |= GlowChannel(t, c->glow[2], c->base[2]) << 8;
        Dl2(dl, 0xB2100000 | ((i * 2) & 0xFFFF), rgba | 0xFF);          /* gSPModifyVertex RGBA */
        Dl2(dl, 0xB2140000 | ((i * 2) & 0xFFFF),                         /* ... ST */
            ((u32)((s32)((f64)(x * g->domeTexScale) * 32.0) + g->domeScroll[0]) << 16)
            | (((s32)((f64)(z * g->domeTexScale) * 32.0) + g->domeScroll[1]) & 0xFFFF));
    }
    func_80028E20(dl, g->tex[0]);
    slot = 16;
    for (k = 0; k < 3; k++) {
        s32 n;
        for (i = 0; i < 16; i++) {
            f32 x = D_800AC440[i * 2] * D_800AC6BC[k] * 500.0f;
            f32 z = D_800AC440[i * 2 + 1] * D_800AC6BC[k] * 500.0f;
            t = GlowT(c, x, z);
            v = &gVtx[i];
            v->x = (s32)x;
            if (mode2) v->y = (s32)((f64)D_800AC6C8[k] * 1.5 * 500.0);
            else v->y = (s32)(D_800AC6C8[k] * 500.0f);
            v->z = (s32)z;
            v->flag = 0;
            v->s = (s32)((f64)(x * g->domeTexScale) * 32.0) + (u16)g->domeScroll[0];
            v->t = (s32)((f64)(z * g->domeTexScale) * 32.0) + (u16)g->domeScroll[1];
            gVtx[i].r = GlowChannel(t, c->glow[0], c->base[0]);
            gVtx[i].g = GlowChannel(t, c->glow[1], c->base[1]);
            gVtx[i].b = GlowChannel(t, c->glow[2], c->base[2]);
            gVtx[i].a = 255;
        }
        Dl2(dl, 0x040040FF | (((slot * 2) & 0xFF) << 16), (u32)gVtx);  /* gSPVertex 16 at slot */
        gVtx += 16;
        n = mode2 ? 16 : D_800AC738[k];
        for (i = 0; i < n; i++) {
            s8 *p = D_800AC6D8[i];
            Dl2(dl, 0xB1000000 | TRI_W1F(p[0], p[1], p[2], p[0]), TRI_W1F(p[3], p[4], p[5], p[3]));
        }
        slot = (slot == 0) ? 16 : 0;
    }
    {
        f32 x = 0.0f, z = 0.0f;
        t = GlowT(c, x, z);
        v = gVtx;
        v->x = (s32)x;
        if (mode2) v->y = (s32)((f64)D_800AC6D4 * 1.5 * 500.0);
        else v->y = (s32)(D_800AC6D4 * 500.0f);
        v->z = (s32)z;
        v->flag = 0;
        v->s = (s32)((f64)(x * g->domeTexScale) * 32.0) + (u16)g->domeScroll[0];
        v->t = (s32)((f64)(z * g->domeTexScale) * 32.0) + (u16)g->domeScroll[1];
        gVtx->r = GlowChannel(t, c->glow[0], c->base[0]);
        gVtx->g = GlowChannel(t, c->glow[1], c->base[1]);
        gVtx->b = GlowChannel(t, c->glow[2], c->base[2]);
        gVtx->a = 255;
    }
    Dl2(dl, 0x0400040F | (((slot * 2) & 0xFF) << 16), (u32)gVtx);      /* gSPVertex 1 at slot */
    gVtx += 1;
    other = (slot == 0) ? 16 : 0;
    for (i = 0; i < 16; i += 2) {
        Dl2(dl, 0xB1000000 | TRI_W1F(slot, other + i, other + i + 1, slot),
            TRI_W1F(slot, other + i + 1, other + ((i + 2) & 15), slot));
    }
    Dl2(dl, 0xE7000000, 0);
}

/* the sun: a yellow fan and a red halo round it, 500 away along z */
static void Sun(u32 **dl) {
    s32 i;
    Vtx16 *v;
    func_8007FB90(dl, 0x00404240, 0x00104240);
    func_8007FBE8(dl, 1);
    for (i = 0; i < 9; i++) {
        f32 x = (f32)-((f64)D_800AC440[i * 2 + 1] * 0.1 * 160.0);
        f32 y = (f32)((f64)D_800AC440[i * 2] * 0.1 * 80.0);
        v = &gVtx[i];
        v->x = (s32)x;
        v->y = (s32)y;
        v->z = 500;
        v->flag = 0;
        v->s = 0;
        v->t = 0;
        v->r = 255;
        gVtx[i].g = 255;
        gVtx[i].b = 0;
        gVtx[i].a = 0;
        x = (f32)-((f64)D_800AC440[i * 2 + 1] * 11.2);
        y = (f32)((f64)D_800AC440[i * 2] * 5.6);
        v = &gVtx[i + 9];
        v->x = (s32)x;
        v->y = (s32)y;
        v->z = 500;
        v->flag = 0;
        v->s = 0;
        v->t = 0;
        v->r = 255;
        gVtx[i + 9].g = 0;
        gVtx[i + 9].b = 0;
        gVtx[i + 9].a = 100;
    }
    v = &gVtx[18];
    v->x = 0;
    v->y = 0;
    v->z = 500;
    v->flag = 0;
    v->s = 0;
    v->t = 0;
    v->r = 255;
    gVtx[18].g = 0;
    gVtx[18].b = 0;
    gVtx[18].a = 128;
    Dl2(dl, 0x04004D2F, (u32)gVtx);                                     /* gSPVertex 19 */
    gVtx += 19;
    for (i = 0; i < 8; i++) {
        Dl2(dl, 0xB1000000 | TRI_W1F(i, i + 1, i + 10, i), TRI_W1F(i, i + 10, i + 9, i));
        Dl2(dl, 0xBF000000, TRI_W1(i + 10, 18, i + 9));
    }
}

/* 0x80024994: ArenaGround_Draw - the floor, the dome, the backdrop and the sun, seen from cam */
void func_80024994(ArenaGround *g, u32 **dl, void *cam) {
    f32 m[16];
    f32 a[4];
    GlowColours c;
    f32 back[4][3];
    s32 i;
    Vtx16 *v;

    if (g->mode == 0) return;
    /* the camera turned with the ground, without its position: things at infinity */
    func_80087E70(m, cam);
    m[12] = 0.0f;
    m[13] = (g->mode == 1) ? -2.0f : -5.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
    func_800882C4(m, g->mtx);
    func_8008004C(dl, m);
    func_80087E20(m);
    func_80080170(dl, m);
    func_800801F4(g->sunPos, g->sunScreen);
    g->sunScreen[0] = g->sunScreen[0] - (f32)(B_800F5D00 / 2);
    g->sunScreen[1] = -(g->sunScreen[1] - (f32)(B_800F5BEC / 2));
    func_8007EEF0(dl);
    func_8007FB08(dl, 0x204);
    func_8007FB90(dl, 0x0C084000, 0x03024000);
    func_8007FBE8(dl, 2);

    c.cx = D_800AC440[0] * g->glowDist * 500.0f;
    c.cz = D_800AC440[1] * g->glowDist * 500.0f;
    c.radius = g->glowRadius * 500.0f;
    for (i = 0; i < 3; i++) {
        c.glow[i] = (f32)((f64)(f32)g->glowRgb[i] / 255.0);
    }
    for (i = 0; i < 3; i++) {
        c.base[i] = (f32)((f64)(f32)g->nearRgb[i] / 255.0);
    }
    for (i = 0; i < 4; i++) {
        back[i][0] = (f32)g->backRgb[i][0];
        back[i][1] = (f32)g->backRgb[i][1];
        back[i][2] = (f32)g->backRgb[i][2];
    }
    WrapScroll(&g->domeScroll[0], g->domeScrollSpeed[0]);
    WrapScroll(&g->domeScroll[1], g->domeScrollSpeed[1]);
    WrapScroll(&g->floorScroll[0], g->floorScrollSpeed[0]);
    WrapScroll(&g->floorScroll[1], g->floorScrollSpeed[1]);

    if (g->mode == 1) {
        u32 rgba;
        /* the backdrop's last four outline points */
        D_800AC4C0[12] = g->backX0;
        D_800AC4C0[13] = g->backZ0;
        D_800AC4C0[14] = g->backX1;
        D_800AC4C0[15] = g->backZ0;
        D_800AC4C0[16] = g->backX1;
        D_800AC4C0[17] = g->backZ1;
        D_800AC4C0[18] = g->backX0;
        D_800AC4C0[19] = g->backZ1;
        /* the floor: ring 0..15, two points 16, 17 and the backdrop's foot 18..27 */
        FloorRing(g, a);
        RimVertex(g, &gVtx[16], (s32)a[0], (s32)a[1],
                  (s32)((f64)(a[0] * g->floorTexScale) * 32.0), (s32)((f64)(a[1] * g->floorTexScale) * 32.0));
        RimVertex(g, &gVtx[17], (s32)a[2], (s32)a[3],
                  (s32)((f64)(a[2] * g->floorTexScale) * 32.0), (s32)((f64)(a[3] * g->floorTexScale) * 32.0));
        for (i = 0; i < 10; i++) {
            f32 x = D_800AC4C0[i * 2] * 500.0f;
            f32 z = D_800AC4C0[i * 2 + 1] * 500.0f;
            RimVertex(g, &gVtx[18 + i], (s32)x, (s32)z,
                      (s32)((f64)(x * g->floorTexScale) * 32.0), (s32)((f64)(z * g->floorTexScale) * 32.0));
        }
        Dl2(dl, 0x040071BF, (u32)gVtx);                                 /* gSPVertex 28 */
        gVtx += 28;
        func_80028E20(dl, g->tex[1]);
        Tris(dl, D_800AC510, 16);
        Dl2(dl, 0xE7000000, 0);
        /* the overlay: re-coloured and re-mapped vertices 18..27, 6 and 10 */
        rgba = (g->overlayRgb[0] << 24) | (g->overlayRgb[1] << 16) | (g->overlayRgb[2] << 8) | 0xFF;
        for (i = 0; i < 10; i++) {
            f32 x = D_800AC4C0[i * 2] * 500.0f;
            f32 z = D_800AC4C0[i * 2 + 1] * 500.0f;
            Dl2(dl, 0xB2140000 | (((i + 18) * 2) & 0xFFFF),
                ((u32)(s32)((f64)(x * g->overlayTexScale) * 32.0) << 16)
                | ((s32)((f64)(z * g->overlayTexScale) * 32.0) & 0xFFFF));
            Dl2(dl, 0xB2100000 | (((i + 18) * 2) & 0xFFFF), rgba);
        }
        for (i = 0; i < 2; i++) {
            s32 k = i ? 10 : 6;
            f32 x = D_800AC440[k * 2] * 500.0f;
            f32 z = D_800AC440[k * 2 + 1] * 500.0f;
            Dl2(dl, 0xB2140000 | (k * 2),
                ((u32)(s32)((f64)(x * g->overlayTexScale) * 32.0) << 16)
                | ((s32)((f64)(z * g->overlayTexScale) * 32.0) & 0xFFFF));
            Dl2(dl, 0xB2100000 | (k * 2), rgba);
        }
        func_80028E20(dl, g->tex[2]);
        Tris(dl, D_800AC57C, 12);
        Dl2(dl, 0xE7000000, 0);
        if (g->shadows) {
            func_8007FBE8(dl, 1);
            for (i = 0; i < 2; i++) {
                s8 *t = D_800AC5A0[i];
                Dl2(dl, 0xB2100000 | ((t[0] * 2) & 0xFFFF), 0xFF);
                Dl2(dl, 0xB2100000 | ((t[1] * 2) & 0xFFFF), 0xFF);
                Dl2(dl, 0xB2100000 | ((t[2] * 2) & 0xFFFF), 0xFF);
                Dl2(dl, 0xBF000000, TRI_W1F(t[0], t[1], t[2], t[0]));
            }
            Dl2(dl, 0xE7000000, 0);
            func_8007FBE8(dl, 2);
        }
        Dome(g, dl, &c, 0);
        /* the backdrop: a 20-vertex sheet over the outline's first edge, its colours mixed
         * from the four corners */
        for (i = 0; i < 20; i++) {
            f32 w = D_800AC4C0[2] - D_800AC4C0[0];
            f32 x = (w * D_800AC5A8[i * 2] + D_800AC4C0[0]) * 500.0f;
            f32 y = (f32)((f64)D_800AC5A8[i * 2 + 1] * 0.5 * (f64)w * 500.0);
            f32 u = D_800AC5A8[i * 2];
            f32 vv = (f32)((f64)D_800AC5A8[i * 2 + 1] / 0.632);
            f64 iu;
            v = &gVtx[i];
            v->x = (s32)x;
            v->y = (s32)y;
            v->z = (s32)(((f64)D_800AC4C0[1] + (f64)D_800AC5A8[i * 2 + 1] * -0.09) * 500.0);
            v->flag = 0;
            v->s = (s32)(((f64)x - 232.0) * 1.22 * 32.0);
            v->t = (s32)(((f64)y + 1.0) * -32.0 * 1.75);
            iu = 1.0 - (f64)u;
            gVtx[i].r = (u32)((((f64)(back[1][0] * u) + (f64)back[0][0] * iu) * (f64)vv)
                              + (((f64)(back[3][0] * u) + (f64)back[2][0] * iu) * (1.0 - (f64)vv)));
            gVtx[i].g = (u32)((((f64)(back[1][1] * u) + (f64)back[0][1] * iu) * (f64)vv)
                              + (((f64)(back[3][1] * u) + (f64)back[2][1] * iu) * (1.0 - (f64)vv)));
            gVtx[i].b = (u32)((((f64)(back[1][2] * u) + (f64)back[0][2] * iu) * (f64)vv)
                              + (((f64)(back[3][2] * u) + (f64)back[2][2] * iu) * (1.0 - (f64)vv)));
            gVtx[i].a = 255;
        }
        Dl2(dl, 0x0400513F, (u32)gVtx);                                 /* gSPVertex 20 */
        gVtx += 20;
        func_80028E20(dl, g->tex[3]);
        Tris(dl, D_800AC684, 18);
        Dl2(dl, 0xE7000000, 0);
        if (!g->sun) return;
        Sun(dl);
    } else if (g->mode == 2) {
        /* the floor: ring 0..15, two points 16, 17 beside vertex 0 and two at twice their x */
        FloorRing(g, a);
        RimVertex(g, &gVtx[16], (s32)a[0], (s32)a[1],
                  (s32)((f64)(a[0] * g->floorTexScale) * 32.0), (s32)((f64)(a[1] * g->floorTexScale) * 32.0));
        RimVertex(g, &gVtx[17], (s32)a[2], (s32)a[3],
                  (s32)((f64)(a[2] * g->floorTexScale) * 32.0), (s32)((f64)g->floorTexScale * 0.0 * 32.0));
        RimVertex(g, &gVtx[18], (s32)((f64)a[0] + (f64)a[0]), 0,
                  (s32)(((f64)a[0] + (f64)a[0]) * (f64)g->floorTexScale * 32.0),
                  (s32)((f64)g->floorTexScale * 0.0 * 32.0));
        RimVertex(g, &gVtx[19], (s32)((f64)a[2] + (f64)a[2]), 0,
                  (s32)(((f64)a[2] + (f64)a[2]) * (f64)g->floorTexScale * 32.0),
                  (s32)((f64)g->floorTexScale * 0.0 * 32.0));
        Dl2(dl, 0x0400513F, (u32)gVtx);                                 /* gSPVertex 20 */
        gVtx += 20;
        func_80028E20(dl, g->tex[1]);
        Tris(dl, D_800AC540, 20);
        Dl2(dl, 0xE7000000, 0);
        Dome(g, dl, &c, 1);
        if (!g->sun) return;
        Sun(dl);
    } else {
        return;
    }
    Dl2(dl, 0xE7000000, 0);
}

/* 0x80028E20: Image_LoadTexture - the display-list commands that load an image (and its
 * palette) as tile 0. Image header: 0x00 format (low nibble: 0 4-bit CI, 1 8-bit CI, 2 16-bit
 * RGBA, 3 16-bit IA; 0x10: a palette follows the header at 0x10), 0x01 palette size - 1,
 * 0x02 offset of the pixels, 0x06 width, 0x08 height. Other formats load nothing. */
void func_80028E20(u32 **dl, u8 *img) {
    s32 fmt = img[0] & 0xF;
    s16 w = *(s16 *)(img + 6);
    s16 h = *(s16 *)(img + 8);
    u32 pix = (u32)(img + *(u16 *)(img + 2));
    u32 tile = ((((h - 1) << 2) & 0xFFF)) | 0x07000000;

    if ((u32)fmt < 2) {
        if (img[0] & 0x10) {
            Dl2(dl, 0xFD100000, (u32)(img + 0x10));                    /* gDPLoadTLUT */
            Dl2(dl, 0xE8000000, 0);
            Dl2(dl, 0xF5000100, 0x07000000);
            Dl2(dl, 0xE6000000, 0);
            Dl2(dl, 0xF0000000, ((u32)img[1] << 14) | 0x07000000);
            Dl2(dl, 0xE7000000, 0);
        }
        Dl2(dl, 0xBA000E02, 0x8000);                                    /* TLUT RGBA16 */
    }
    switch (img[0] & 0xF) {
    case 0:                                                             /* 4-bit, loaded as 8-bit */
        Dl2(dl, 0xFD480000 | (((w >> 1) - 1) & 0xFFF), pix);
        Dl2(dl, 0xF5480000 | (((((w >> 1) + 7) >> 3) & 0x1FF) << 9), 0x07018060);
        Dl2(dl, 0xE6000000, 0);
        Dl2(dl, 0xF4000000, ((((w - 1) << 1) & 0xFFF) << 12) | tile);
        Dl2(dl, 0xE7000000, 0);
        Dl2(dl, 0xF5400000 | (((((w >> 1) + 7) >> 3) & 0x1FF) << 9), 0x00018060);
        break;
    case 1:
        Dl2(dl, 0xFD480000 | ((w - 1) & 0xFFF), pix);
        Dl2(dl, 0xF5480000 | ((((w + 7) >> 3) & 0x1FF) << 9), 0x07014060);
        Dl2(dl, 0xE6000000, 0);
        Dl2(dl, 0xF4000000, ((((w - 1) << 2) & 0xFFF) << 12) | tile);
        Dl2(dl, 0xE7000000, 0);
        Dl2(dl, 0xF5480000 | ((((w + 7) >> 3) & 0x1FF) << 9), 0x00014060);
        break;
    case 2:
        Dl2(dl, 0xFD100000 | ((w - 1) & 0xFFF), pix);
        Dl2(dl, 0xF5100000 | (((((w << 1) + 7) >> 3) & 0x1FF) << 9), 0x07014060);
        Dl2(dl, 0xE6000000, 0);
        Dl2(dl, 0xF4000000, ((((w - 1) << 2) & 0xFFF) << 12) | tile);
        Dl2(dl, 0xE7000000, 0);
        Dl2(dl, 0xF5100000 | (((((w << 1) + 7) >> 3) & 0x1FF) << 9), 0x00014060);
        break;
    case 3:
        Dl2(dl, 0xFD180000 | ((w - 1) & 0xFFF), pix);
        Dl2(dl, 0xF5180000 | (((((w << 1) + 7) >> 3) & 0x1FF) << 9), 0x07014050);
        Dl2(dl, 0xE6000000, 0);
        Dl2(dl, 0xF4000000, ((((w - 1) << 2) & 0xFFF) << 12) | tile);
        Dl2(dl, 0xE7000000, 0);
        Dl2(dl, 0xF5180000 | (((((w << 1) + 7) >> 3) & 0x1FF) << 9), 0x00014050);
        break;
    default:
        return;
    }
    Dl2(dl, 0xF2000000, ((((w - 1) << 2) & 0xFFF) << 12) | (((h - 1) << 2) & 0xFFF)); /* tile size */
}

extern f32 D_800AC78C[12][3];           /* lens flares: radius of the inner disc, ring 2, ring 3 (0: none) */
extern f32 D_800AC81C[12];              /* ... where along the line from the sun through the screen centre */
extern u8  D_800AC84C[12][4][4];        /* ... colours: centre, ring 1, ring 2, ring 3 (rgb + alpha 0..255) */
void func_80080100(u32 **dl);
void func_80028490(ArenaGround *g, u32 **dl, void *cam);

/* one ring of a flare: 16 vertices round (cx, cy) */
static void FlareRing(f32 cx, f32 cy, f32 r, u8 *rgb, u32 alpha) {
    s32 i;
    Vtx16 *v;
    for (i = 0; i < 16; i++) {
        f32 x = D_800AC440[i * 2] * r + cx;
        f32 y = D_800AC440[i * 2 + 1] * r + cy;
        v = &gVtx[i];
        v->x = (s32)x;
        v->y = (s32)y;
        v->z = 0;
        v->flag = 0;
        v->s = 0;
        v->t = 0;
        v->r = rgb[0];
        gVtx[i].g = rgb[1];
        gVtx[i].b = rgb[2];
        gVtx[i].a = alpha;
    }
}

/* the band between ring slots 0..15 and 16..31 */
static void FlareBand(u32 **dl) {
    s32 i;
    for (i = 0; i < 16; i++) {
        s32 n = (i + 1) & 15;
        Dl2(dl, 0xB1000000 | TRI_W1F(i, i + 16, n + 16, i), TRI_W1F(i, n + 16, n, i));
    }
}

/* 0x80028490: ArenaGround_DrawLensFlare - in screen space, 12 flares along the line from the
 * sun through the centre of the screen, fainter the further the sun is from the centre.
 * (The last command goes to the main display list gDL whatever `dl` is.) */
void func_80028490(ArenaGround *g, u32 **dl, void *cam) {
    f32 d[2];
    f32 f, cx, cy;
    s32 k, i;
    u32 a0, a1, a2, a3;
    Vtx16 *v;
    (void)cam;

    if ((f64)g->sunScreen[2] <= 0.0) return;                /* behind the camera */
    d[0] = -g->sunScreen[0];
    d[1] = -g->sunScreen[1];
    f = func_80087630(d) / (f32)B_800F5D00;
    if (1.0 <= (f64)f) return;
    f = (f32)(1.0 - (f64)f);
    f = f * f;
    func_8007EEF0(dl);
    func_8007F94C(dl);
    func_8007FB08(dl, 0x204);
    func_8007FB90(dl, 0x00404240, 0x00104240);
    func_8007FBE8(dl, 1);
    for (k = 0; k < 12; k++) {
        cx = d[0] * D_800AC81C[k] + g->sunScreen[0];
        cy = d[1] * D_800AC81C[k] + g->sunScreen[1];
        a0 = (u32)((f32)D_800AC84C[k][0][3] * f);
        a1 = (u32)((f32)D_800AC84C[k][1][3] * f);
        a2 = (u32)((f32)D_800AC84C[k][2][3] * f);
        a3 = (u32)((f32)D_800AC84C[k][3][3] * f);
        /* the inner disc: a fan round vertex 16 */
        v = &gVtx[16];
        v->x = (s32)cx;
        v->y = (s32)cy;
        v->z = 0;
        v->flag = 0;
        v->s = 0;
        v->t = 0;
        v->r = D_800AC84C[k][0][0];
        gVtx[16].g = D_800AC84C[k][0][1];
        gVtx[16].b = D_800AC84C[k][0][2];
        gVtx[16].a = a0;
        FlareRing(cx, cy, D_800AC78C[k][0], D_800AC84C[k][1], a1);
        Dl2(dl, 0x0400450F, (u32)gVtx);                                 /* gSPVertex 17 */
        gVtx += 17;
        for (i = 0; i < 16; i++) Dl2(dl, 0xBF000000, TRI_W1((i + 1) & 15, 16, i));
        if ((f64)D_800AC78C[k][1] == 0.0) continue;
        FlareRing(cx, cy, D_800AC78C[k][1], D_800AC84C[k][2], a2);
        Dl2(dl, 0x042040FF, (u32)gVtx);                                 /* gSPVertex 16 at 16 */
        gVtx += 16;
        FlareBand(dl);
        if ((f64)D_800AC78C[k][2] == 0.0) continue;
        FlareRing(cx, cy, D_800AC78C[k][2], D_800AC84C[k][3], a3);
        Dl2(dl, 0x040040FF, (u32)gVtx);                                 /* gSPVertex 16 at 0 */
        gVtx += 16;
        FlareBand(dl);
    }
    Dl2(dl, 0xE7000000, 0);
    func_80080100(&gDL);
}

/* ---- the 3D stage's sky (stage + 0x40, see func_8002EA90) ---- */
typedef struct StageSky {
    /* 0x00 */ f32 mtx[16];           /* its turn about y */
    /* 0x40 */ u8 *cloud;             /* the cloud picture */
    /* 0x44 */ u8  rgb44[4];
    /* 0x48 */ u8  rgb48[4];
} StageSky;

typedef struct StageCloud {           /* a cloud drawn bigger than the picture */
    f32 x, y, z;
    f32 rightScale, upScale;
} StageCloud;

extern f32 D_800AC910[32];            /* 16 points round a unit circle (x, z) */
extern f32 D_800AC998[];              /* the big clouds' z: the table starts 8 bytes before */
#define gStageClouds ((StageCloud *)((u8 *)D_800AC998 - 8))   /* 0x800AC990: 8 clouds */
extern s32 D_800ACA30[7];             /* where the small clouds are (angles round the stage) */
void func_80029410(StageSky *s);
void func_800294EC(StageSky *s);
void func_80029514(StageSky *s, u32 **dl, f32 *cam);

/* 0x80029410: StageSky_Init - no turn; the cloud picture from ROM [0xD4D640, 0xD4E150) */
void func_80029410(StageSky *s) {
    func_80087FD0(s->mtx, 0);
    s->cloud = LoadRom(0xD4D640, 0xD4E150);
    *(s16 *)(s->cloud + 0xA) = *(s16 *)(s->cloud + 6) / 2;   /* the picture's centre */
    *(s16 *)(s->cloud + 0xC) = *(s16 *)(s->cloud + 8) / 2;
    s->rgb44[0] = 200; s->rgb44[1] = 200; s->rgb44[2] = 255;
    s->rgb48[0] = 100; s->rgb48[1] = 100; s->rgb48[2] = 190;
}

/* 0x800294EC: StageSky_Free */
void func_800294EC(StageSky *s) {
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, s->cloud);
}

/* one ring of the sky: 16 vertices round the y axis, radius 1000 */
static void SkyRing(s32 y, u8 r, u8 gr, u8 b) {
    s32 i;
    Vtx16 *v;
    for (i = 0; i < 16; i++) {
        f32 x = D_800AC910[i * 2] * 1000.0f;
        f32 z = D_800AC910[i * 2 + 1] * 1000.0f;
        v = &gVtx[i];
        v->x = (s32)x;
        v->y = y;
        v->z = (s32)z;
        v->flag = 0;
        v->s = 0;
        v->t = 0;
        v->r = r;
        gVtx[i].g = gr;
        gVtx[i].b = b;
        gVtx[i].a = 255;
    }
}

/* the band between the ring in slots 16..31 and the one in 0..15 (the flag, 16 + i, only
 * rotates the triangles) */
static void SkyBand(u32 **dl) {
    s32 i;
    for (i = 0; i < 16; i++) {
        s32 n = (i + 1) % 16;
        Dl2(dl, 0xB1000000 | TRI_W1F(i + 16, i, n, i + 16), TRI_W1F(i + 16, n, n + 16, i + 16));
    }
}

/* 0x80029514: StageSky_Draw - a sky ball round the camera (blue above, darker below, the
 * horizon lower the higher the camera is) and clouds: seven round the horizon and eight
 * bigger ones in the world */
void func_80029514(StageSky *s, u32 **dl, f32 *cam) {
    f32 m[16], inv[16];
    f32 up[3], right[3], u[3], r[3], p[3], pos[3];
    f32 h, base;
    s32 i;
    Vtx16 *v;

    func_80087E70(m, cam);
    m[12] = 0.0f;
    m[13] = 30.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
    func_800882C4(m, s->mtx);
    func_80087E70(inv, cam);
    func_8008861C(inv);
    up[0] = inv[4]; up[1] = inv[5]; up[2] = inv[6];
    right[0] = inv[0]; right[1] = inv[1]; right[2] = inv[2];
    /* where the camera is (the original uses p[0] for all three: p is 0 anyway) */
    p[0] = 0.0f; p[1] = 0.0f; p[2] = 0.0f;
    pos[0] = (f32)((f64)(inv[0] * p[0] + inv[4] * p[0] + inv[8] * p[0]) + (f64)inv[12]);
    pos[1] = (f32)((f64)(inv[1] * p[0] + inv[5] * p[0] + inv[9] * p[0]) + (f64)inv[13]);
    pos[2] = (f32)((f64)(inv[2] * p[0] + inv[6] * p[0] + inv[10] * p[0]) + (f64)inv[14]);
    (void)pos[2];
    h = (f32)(((f64)pos[1] + 230.0) / 900.0);
    if ((f64)h < 0.2) h = 0.2f;
    func_80087E20(inv);
    func_80080170(dl, inv);
    func_8007EEF0(dl);
    base = 0.0f;
    func_8008004C(dl, m);

    /* the top: ring 0 (blue) and a point above */
    SkyRing((s32)base, 0, 0, 255);
    v = &gVtx[16];
    v->x = 250;
    v->y = 900;
    v->z = 250;
    v->flag = 0;
    v->s = 0;
    v->t = 0;
    v->r = 150;
    gVtx[16].g = 150;
    gVtx[16].b = 255;
    gVtx[16].a = 255;
    Dl2(dl, 0x0400450F, (u32)gVtx);                                     /* gSPVertex 17 */
    gVtx += 17;
    func_8007FB08(dl, 0x204);
    func_8007FB90(dl, 0x0C084000, 0x03024000);
    func_8007FBE8(dl, 1);
    for (i = 0; i < 16; i++) Dl2(dl, 0xBF000000, TRI_W1((i + 1) % 16, 16, i));
    /* the horizon */
    SkyRing((s32)(base - h * 90.0f), 70, 70, 180);
    Dl2(dl, 0x042040FF, (u32)gVtx);                                     /* gSPVertex 16 at 16 */
    gVtx += 16;
    SkyBand(dl);
    /* below */
    SkyRing((s32)(base + h * -140.0f), 0, 0, 100);
    Dl2(dl, 0x040040FF, (u32)gVtx);                                     /* gSPVertex 16 at 0 */
    gVtx += 16;
    SkyBand(dl);
    /* the bottom point */
    v = gVtx;
    v->x = 0;
    v->y = -1000;
    v->z = 0;
    v->flag = 0;
    v->s = 0;
    v->t = 0;
    v->r = 0;
    gVtx->g = 0;
    gVtx->b = 0;
    gVtx->a = 255;
    Dl2(dl, 0x0420040F, (u32)gVtx);                                     /* gSPVertex 1 at 16 */
    gVtx += 1;
    for (i = 0; i < 16; i++) Dl2(dl, 0xBF000000, TRI_W1((i + 1) % 16, 16, i));
    Dl2(dl, 0xE7000000, 0);

    /* the clouds round the horizon, half size, facing the camera */
    func_8007FB08(dl, 0x205);
    func_8007FBE8(dl, 2);
    func_8007FB90(dl, 0x0C084000, 0x03024000);
    func_8008004C(dl, m);
    for (i = 0; i < 7; i++) {
        f32 x, z;
        f64 y;
        u[0] = (f32)((f64)up[0] * 0.5);
        u[1] = (f32)((f64)up[1] * 0.5);
        u[2] = (f32)((f64)up[2] * 0.5);
        r[0] = (f32)((f64)right[0] * 0.5);
        r[1] = (f32)((f64)right[1] * 0.5);
        r[2] = (f32)((f64)right[2] * 0.5);
        x = (f32)((f64)vsSin(D_800ACA30[i]) * 1000.0);
        z = (f32)((f64)vsCos(D_800ACA30[i]) * 1000.0);
        y = (i & 1) ? 90.0 - (f64)h * 50.0 : 40.0 - (f64)h * 50.0;
        func_80083A64(dl, s->cloud, x, (f32)y, z, u, r, 8);
    }
    /* the big clouds, in the world */
    func_8008004C(dl, cam);
    func_8007FB90(dl, 0x00442230, 0x00112230);
    for (i = 0; i < 8; i++) {
        StageCloud *c = &gStageClouds[i];
        u[0] = up[0] * c->upScale;
        u[1] = up[1] * c->upScale;
        u[2] = up[2] * c->upScale;
        r[0] = right[0] * c->rightScale;
        r[1] = right[1] * c->rightScale;
        r[2] = right[2] * c->rightScale;
        func_80083A64(dl, s->cloud, c->x, c->y, c->z, u, r, 0);
    }
}
