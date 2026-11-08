/*
 * The 3D "stage" (0x8002EA90-0x8003102C, helpers 0x8003102C-0x80031460): an arena drawn
 * outside a fight (VS.C, CREDITS.C, ARENASEL.C, INTRO.C, the "launch" shot in MENUS.C).
 *
 * Besides the arena (drawn by the arena code) and the sky (stage+0x40, StageSky 0x80029410...),
 * the stage has a lump of
 * clay standing in it that now and then (at random, when `events` is set) wobbles, swells
 * up and bursts - throwing sparks (`sparks`, drawn as red streaks) and clay bits (`bits`) -
 * then settles back, and a few decorations (a shadow disc, two models, a row of pictures).
 *
 * X_Draw (the logic) calls func_8002ED34 once a tick, X_Frame draws the arena with
 * func_8002FD50 and the rest with func_8002FE34.
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"

typedef struct StageSpark {                /* 0x14 bytes */
    /* 0x00 */ f32 t;                      /* 0 -> 1, < 0 = free */
    /* 0x04 */ f32 pos[3];
    /* 0x10 */ f32 size;
} StageSpark;

typedef struct StageBit {                  /* a flying bit of clay (0x20 bytes) */
    /* 0x00 */ f32 life;                   /* counts down; < 0 = free (fades below 1) */
    /* 0x04 */ f32 pos[3];
    /* 0x10 */ f32 vel[3];
    /* 0x1C */ f32 size;
} StageBit;

typedef struct Stage {                     /* 0x35D0 bytes */
    /* 0x0000 */ f32 m[16];                /* where the clay stands */
    /* 0x0040 */ u8  sky[0x4C];            /* the sky (StageSky, 0x80029410...) */
    /* 0x008C */ s32 state;                /* the clay: -1 still, 0 swelling, 1 shaking, 2 bursting, 3 settling */
    /* 0x0090 */ f32 t;                    /* 0..1 within the state */
    /* 0x0094 */ s32 angle;                /* the wobble */
    /* 0x0098 */ s32 speed;
    /* 0x009C */ f32 scale[3];             /* the clay's size */
    /* 0x00A8 */ f32 height;               /* its height (scale[1] wobbles about it) */
    /* 0x00AC */ StageSpark sparks[200];
    /* 0x104C */ StageBit bits[300];
    /* 0x35CC */ s32 events;               /* the clay may start by itself */
} Stage;

typedef struct Light {                     /* an N64 Light_t (0x10 bytes) */
    u8 col[3], pad1, colc[3], pad2;
    s8 dir[3], pad3;
    u8 pad4[4];
} Light;
typedef struct Lights4 {
    u8 a[8];
    Light l[4];
} Lights4;

void func_80029410(void *r);                            /* StageSky: set up, */
void func_800294EC(void *r);                            /* ... free, */
void func_80029514(void *r, u32 **dl, f32 *m);          /* ... draw */
void func_800831F4(u32 **dl, void *model);
void func_800882C4(void *m, void *by);
void func_8007FF20(s32 fovy, f32 nearZ, f32 farZ);
void func_8007FFB8(u32 **dl);
void func_8003102C(Stage *s);
void func_80031060(void);
void func_800310A0(void);
void func_80031200(Stage *s, f32 x, f32 y, f32 z, f32 size);
void func_8003128C(Stage *s, f32 x, f32 y, f32 z, f32 size);

extern u8 *D_800ACB40;                     /* the stage's file */
extern s32 D_800ACB44;                     /* how many stages use it */
extern u8 *B_800DC9A0, *B_800DC9A4;        /* two models */
extern u8 *B_800DC9A8;                     /* the clay */
extern u8 *B_800DC9AC, *B_800DC9B0;        /* two pictures (in the file) */
extern u8 *B_800DC9B4;                     /* the bit of clay (its own copy) */
extern Lights4 D_800ACB48, D_800ACB90;     /* the two models' lights */
extern f32 D_800ACBD8[7][3];               /* where the pictures go */
extern f32 D_800ACC2C[4][3];
extern f32 D_800ACC5C[16][2];              /* a circle (sin, cos) */
extern f32 D_800ACCDC[3];                  /* where the clay stands (x, y, z) */
extern f32 D_800ACCE8;                     /* a debug value (pad 1/2 C buttons, printed with B) */
extern f32 B_800DC9B8[2];
extern f32 B_800DC9C0;                     /* a drifting value: its target, */
extern s16 B_800DC9C4;                     /* ... ticks to the next target, */
extern f32 B_800DC9C8, B_800DC9CC;         /* ... the value and its speed */

#define RAND() ((s32)guRandom())          /* (30 bits) */

static void Cmd(u32 w0, u32 w1) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = w0;
    g[1] = w1;
}

static void SetLights4(Lights4 *lt) {      /* gSPSetLights4 */
    Cmd(0xBC000002, 0x800000A0);
    Cmd(0x03860010, (u32)&lt->l[0]);
    Cmd(0x03880010, (u32)&lt->l[1]);
    Cmd(0x038A0010, (u32)&lt->l[2]);
    Cmd(0x038C0010, (u32)&lt->l[3]);
    Cmd(0x038E0010, (u32)lt->a);
}

static u8 *ImageAt(u8 *file, s32 at) { return file + *(s32 *)(file + at); }

/* 0x8002EA90: Stage_Init */
void func_8002EA90(void *stage) {
    Stage *s = stage;
    u8 *img;
    u32 size;
    s32 i;

    func_80087E20(s->m);
    if (D_800ACB40 == NULL) {
        D_800ACB40 = func_80080360(0xFD3010);
        B_800DC9A0 = ImageAt(D_800ACB40, 0x24);
        B_800DC9A4 = ImageAt(D_800ACB40, 0x1C);
        B_800DC9A8 = ImageAt(D_800ACB40, 0x2C);
        img = B_800DC9AC = ImageAt(D_800ACB40, 0xC);
        *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;   /* (its bottom centre) */
        *(u16 *)(img + 0xC) = *(u16 *)(img + 8) - 1;
        img = B_800DC9B0 = ImageAt(D_800ACB40, 0x14);
        *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
        *(u16 *)(img + 0xC) = *(u16 *)(img + 8) - 1;
        size = 0xFAA000 - 0xFA9DD0;
        B_800DC9B4 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
        RomCopy(B_800DC9B4, 0xFA9DD0, size);
        img = B_800DC9B4;
        *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;   /* (its centre) */
        *(s16 *)(img + 0xC) = *(s16 *)(img + 8) / 2;
    }
    D_800ACB44++;
    func_80029410(s->sky);
    func_80031060();
    s->state = -1;
    s->scale[0] = 1.0f;
    s->scale[1] = 1.0f;
    s->scale[2] = 1.0f;
    s->height = 1.0f;
    for (i = 0; i < 200; i++) {
        s->sparks[i].t = -1.0f;
        s->sparks[i].pos[0] = 0.0f;
        s->sparks[i].pos[1] = 0.0f;
        s->sparks[i].pos[2] = 0.0f;
    }
    for (i = 0; i < 300; i++) s->bits[i].life = -1.0f;
    s->events = 1;
}

/* 0x8002ECBC: Stage_Free */
void func_8002ECBC(void *stage) {
    Stage *s = stage;

    if (--D_800ACB44 <= 0) {
        D_800ACB44 = 0;
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACB40);
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800DC9B4);
        D_800ACB40 = NULL;
    }
    func_800294EC(s->sky);
}

/* the burst: two sparks and four bits of clay about the clay's top */
static f32 Around(f32 at, s32 spread, s32 off) {
    return (f32)((0.0 - (f64)at) + (f64)(RAND() % spread - off));
}

static void Burst(Stage *s, s32 bit) {
    f32 x, z, size, y;

    x = Around(D_800ACCDC[0], 40, 20);
    z = Around(D_800ACCDC[2], 40, 30);
    size = (f32)((f64)(f32)(RAND() % 1024) / 1024.0 + 0.5);
    y = (f32)((0.0 - (f64)D_800ACCDC[1]) + (f64)s->scale[1] * 290.0);
    if (bit) func_8003128C(s, x, y, z, size);
    else func_80031200(s, x, y, z, size);
}

/* 0x8002ED34: Stage_Update - once a tick */
void func_8002ED34(void *stage) {
    Stage *s = stage;
    StageSpark *k;
    StageBit *b;
    s32 pad, i;

    pad = (B_800F2D18 & 0x2000) ? B_800F4E54 : B_800F2D18;     /* (debug) */
    if (pad & 0x800) D_800ACCE8 = (f32)((f64)D_800ACCE8 + 1.0);
    if (pad & 0x400) D_800ACCE8 = (f32)((f64)D_800ACCE8 - 1.0);
    if (B_800F4E54 & 0x20) mpDebugPrintf("(%f)\n", (f64)D_800ACCE8);
    if (s->state < 0 && RAND() % 1000 < 4 && s->events != 0) func_8003102C(s);
    func_800310A0();
    switch ((u32)s->state) {
    case 0:                                 /* swelling */
        s->scale[0] = (f32)((f64)s->scale[0] + (f64)s->t * 0.009);
        if (1.4 < (f64)s->scale[0]) s->scale[0] = 1.4f;
        s->scale[2] = (f32)((f64)s->scale[2] + (f64)s->t * 0.009);
        if (1.4 < (f64)s->scale[2]) s->scale[2] = 1.4f;
        s->scale[1] = (f32)((f64)vsSin(s->angle) * ((f64)s->t * 0.035) + (f64)s->height);
        s->angle += s->speed;
        s->speed += 20;
        if (s->speed >= 0x2AAB) s->speed = 0x2AAA;
        s->t = (f32)((f64)s->t + 0.01);
        if (1.0 <= (f64)s->t) {
            s->state++;
            s->t = 0.0f;
        }
        s->height = (f32)((f64)s->height - 0.004);
        break;
    case 1:                                 /* shaking */
        s->scale[1] = (f32)((f64)vsSin(s->angle) * 0.035 + (f64)s->height);
        s->angle += s->speed;
        s->speed += 45;
        if (s->speed >= 0x31C8) s->speed = 0x31C7;
        s->t = (f32)((f64)s->t + 0.008);
        if (1.0 <= (f64)s->t) {
            s->state++;
            s->t = 0.0f;
        }
        break;
    case 2:                                 /* bursting up */
        s->scale[0] = (f32)((f64)s->scale[0] - 0.0025);
        if ((f64)s->scale[0] < 1.0) s->scale[0] = 1.0f;
        s->scale[2] = (f32)((f64)s->scale[2] - 0.0025);
        if ((f64)s->scale[2] < 1.0) s->scale[2] = 1.0f;
        s->height = (f32)((f64)s->height + 0.2);
        if (1.4 <= (f64)s->height) s->height = 1.4f;
        s->scale[1] = (f32)((f64)vsSin(s->angle) * ((1.0 - (f64)s->t / 2.0) * 0.035) + (f64)s->height);
        s->angle += s->speed;
        if (1.39 <= (f64)s->height) {
            Burst(s, 0);
            Burst(s, 0);
            Burst(s, 1);
            Burst(s, 1);
            Burst(s, 1);
            Burst(s, 1);
        }
        s->t = (f32)((f64)s->t + 0.008);
        if (1.0 <= (f64)s->t) {
            s->state++;
            s->t = 0.0f;
        }
        break;
    case 3:                                 /* settling */
        s->scale[0] = (f32)((f64)s->scale[0] - 0.0025);
        if ((f64)s->scale[0] < 1.0) s->scale[0] = 1.0f;
        s->scale[2] = (f32)((f64)s->scale[2] - 0.0025);
        if ((f64)s->scale[2] < 1.0) s->scale[2] = 1.0f;
        s->height = (f32)((f64)s->height - 0.005);
        if ((f64)s->height < 1.0) s->height = 1.0f;
        s->scale[1] = (f32)((f64)vsSin(s->angle) * ((0.5 - (f64)s->t / 2.0) * 0.035) + (f64)s->height);
        s->angle += s->speed;
        s->t = (f32)((f64)s->t + 0.008);
        if (1.0 <= (f64)s->t) {
            s->state = -1;
            s->t = 0.0f;
        }
        break;
    case 4:
        s->state = -1;
        s->scale[0] = 1.0f;
        s->scale[1] = 1.0f;
        s->scale[2] = 1.0f;
        s->height = 1.0f;
        break;
    }
    for (i = 0; i < 200; i++) {
        k = &s->sparks[i];
        if (!(0.0 <= (f64)k->t)) continue;
        k->t = (f32)((f64)k->t + 0.2);
        if (1.0 <= (f64)k->t) k->t = -1.0f;
    }
    for (i = 0; i < 300; i++) {
        b = &s->bits[i];
        if (!(0.0 <= (f64)b->life)) continue;
        b->life = (f32)((f64)b->life - 0.02);
        b->pos[0] += b->vel[0];
        b->pos[1] += b->vel[1];
        b->pos[2] += b->vel[2];
        b->vel[1] = (f32)((f64)b->vel[1] - 0.3);         /* gravity */
        if ((f64)b->vel[1] < -5.0) b->vel[1] = -5.0f;
        if ((f64)b->life < 0.0) b->life = -1.0f;
    }
}

/* 0x8002FD50: Stage_DrawSky - the sky (far clipping 4000 while it draws) */
void func_8002FD50(void *stage, void *cam) {
    Stage *s = stage;
    f32 m[16];

    func_80087E70(m, cam);                  /* (the original copies the camera's axes out here, unused) */
    func_8008861C(m);
    func_80087E70(m, cam);
    func_8008861C(m);
    func_8007FF20(0x21, 60.0f, 4000.0f);
    func_8007FFB8(&gDL);
    func_80029514(s->sky, &gDL, m);
    func_8007FF20(0x21, 60.0f, 3600.0f);
    func_8007FFB8(&gDL);
}

static void Scaled(f32 *out, f32 *v, f64 k) {
    out[0] = (f32)((f64)v[0] * k);
    out[1] = (f32)((f64)v[1] * k);
    out[2] = (f32)((f64)v[2] * k);
}

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

/* 0x8002FE34: Stage_Draw - everything but the arena */
void func_8002FE34(void *stage, void *cam) {
    Stage *s = stage;
    f32 m[16];
    f32 up[3], right[3], fwd[3];           /* the camera's axes */
    f32 side[3], n[3], up2[3], right2[3];
    StageSpark *k;
    StageBit *b;
    s32 i;
    f32 c;
    Vtx16 *v;

    func_80087E70(m, cam);
    up[0] = m[4]; up[1] = m[5]; up[2] = m[6];
    right[0] = m[0]; right[1] = m[1]; right[2] = m[2];
    fwd[0] = m[8]; fwd[1] = m[9]; fwd[2] = m[10];
    func_8008861C(m);
    func_80087E70(m, cam);
    func_8008861C(m);
    func_8007FF20(0x21, 60.0f, 4000.0f);
    func_8007FFB8(&gDL);
    func_8008004C(&gDL, m);
    /* the two models */
    func_80087EE8(m, 0.0f, -95.0f, 0.0f);
    func_800882C4(m, s->m);
    func_80080170(&gDL, m);
    SetLights4(&D_800ACB48);
    func_800831F4(&gDL, B_800DC9A4);
    SetLights4(&D_800ACB90);
    func_800831F4(&gDL, B_800DC9A0);
    /* the pictures, upright and facing the camera */
    Cmd(0xBA000C02, 0);
    func_8007FB08(&gDL, 0x204);
    side[0] = right[0];
    side[1] = 0.0f;
    side[2] = right[2];
    func_80087DD0(side);
    n[0] = side[1] * fwd[2] - side[2] * fwd[1];
    n[1] = side[2] * fwd[0] - side[0] * fwd[2];
    n[2] = side[0] * fwd[1] - side[1] * fwd[0];
    n[0] = -n[0];
    n[1] = -n[1];
    n[2] = -n[2];
    Scaled(up2, n, 1.5);
    Scaled(right2, side, 1.5);
    for (i = 0; i < 7; i++)
        func_80083A64(&gDL, B_800DC9B0, D_800ACBD8[i][0] + -15.0f, D_800ACBD8[i][1] + -5.0f,
                      D_800ACBD8[i][2] + 0.0f, up2, right2, 0);
    Scaled(up2, n, 1.5);
    Scaled(right2, side, 1.5);
    for (i = 0; i < 4; i++)
        func_80083A64(&gDL, B_800DC9AC, D_800ACC2C[i][0] + -15.0f, D_800ACC2C[i][1] + -5.0f,
                      D_800ACC2C[i][2] + 0.0f, up2, right2, 0);
    Cmd(0xBA000C02, 0x2000);
    /* the clay, squashed and stretched about its foot */
    func_80087EE8(m, D_800ACCDC[0], D_800ACCDC[1], D_800ACCDC[2]);
    func_800882C4(m, s->m);
    func_80088134(m, s->scale[0], s->scale[1], s->scale[2]);
    func_80088184(m, 0.0f, -100.0f, 0.0f);
    func_80088184(m, -D_800ACCDC[0], -D_800ACCDC[1], -D_800ACCDC[2]);
    func_80080170(&gDL, m);
    func_800831F4(&gDL, B_800DC9A8);
    /* the shadow disc */
    func_80087EE8(m, 0.0f, -100.0f, 0.0f);
    func_80080170(&gDL, m);
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x00404A50, 0x00104A50);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 16; i++)
        SetVtx(&gVtx[i], (s32)((f64)D_800ACC5C[i][0] * 1000.0 + 200.0), -6,
               (s32)((f64)D_800ACC5C[i][1] * 1000.0 + 280.0), 0, 0, 255, 0);
    SetVtx(&gVtx[16], 200, -6, 280, 150, 150, 100, 255);
    Cmd(0x0400450F, (u32)gVtx);
    gVtx += 17;
    for (i = 0; i < 16; i++)
        Cmd(0xBF000000, ((((i + 1) % 16) * 2 & 0xFF) << 16) | 0x2000 | ((i * 2) & 0xFF));
    Cmd(0xE7000000, 0);
    /* the sparks */
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x0C080233, 0x03020233);
    func_8007FBE8(&gDL, 1);
    Cmd(0xB9000002, 3);
    for (i = 0; i < 200; i++) {
        k = &s->sparks[i];
        if (!(0.0 <= (f64)k->t)) continue;
        v = gVtx;
        SetVtx(&v[0], (s32)k->pos[0], (s32)((f64)k->pos[1] - (f64)k->t * 5.0), (s32)k->pos[2], 255, 255, 0, 255);
        SetVtx(&v[1], (s32)k->pos[0], (s32)((f64)k->pos[1] + (f64)k->t * 300.0 * (f64)k->size), (s32)k->pos[2],
               100, 0, 0, (u32)((f64)k->size * 100.0));
        SetVtx(&v[2], (s32)((f64)k->pos[0] - (f64)k->t * 17.0 * (f64)right[0]), (s32)k->pos[1],
               (s32)((f64)k->pos[2] - (f64)k->t * 17.0 * (f64)right[2]), 100, 0, 0, 100);
        SetVtx(&v[3], (s32)((f64)k->pos[0] + (f64)k->t * 17.0 * (f64)right[0]), (s32)k->pos[1],
               (s32)((f64)k->pos[2] + (f64)k->t * 17.0 * (f64)right[2]), 100, 0, 0, 100);
        Cmd(0x0400103F, (u32)gVtx);
        gVtx += 4;
        Cmd(0xB1000204, 0x00000206);
    }
    Cmd(0xE7000000, 0);
    /* the bits of clay */
    func_8007FB08(&gDL, 0x204);
    for (i = 0; i < 300; i++) {
        b = &s->bits[i];
        if (!(0.0 <= (f64)b->life)) continue;
        c = (1.0 < (f64)b->life) ? 1.0f : b->life;
        B_800F7788 = B_800F7E70 = B_800F7D94 = B_800F7778 = (u32)((f64)c * 255.0);
        B_800F88BC = B_800F88C8 = B_800F7789 = B_800F7770 = (u32)((f64)b->life * 80.0);
        B_800F7ECC = B_800F2D1C = B_800F7E78 = B_800F4E34 = (u32)((f32)(RAND() % 50 + 50) * c);
        Scaled(up2, up, (f64)b->size * 0.1);
        Scaled(right2, right, (f64)b->size * 0.1);
        if (1.0 <= (f64)b->life) {
            func_80084CB0(&gDL, B_800DC9B4, b->pos[0], b->pos[1], b->pos[2], up2, right2, 0);
        } else {
            B_800F7E58 = B_800F2D1D = B_800F8760 = B_800F5B78 = (u32)((f64)__builtin_sqrtf(b->life) * 255.0);
            func_80085FE4(&gDL, (CharFrame *)B_800DC9B4, b->pos[0], b->pos[1], b->pos[2], up2, right2, 0);
        }
    }
    func_8007FF20(0x21, 60.0f, 3600.0f);
    func_8007FFB8(&gDL);
}

/* 0x8003102C: Stage_StartClay - the clay starts swelling */
void func_8003102C(Stage *s) {
    s->state = 0;
    s->t = 0.0f;
    s->angle = 0;
    s->speed = 0xE38;
    s->scale[0] = 1.0f;
    s->scale[1] = 1.0f;
    s->scale[2] = 1.0f;
    s->height = 1.0f;
}

/* 0x80031060: Drift_Init */
void func_80031060(void) {
    B_800DC9B8[0] = -1.0f;
    B_800DC9B8[1] = -1.0f;
    B_800DC9C0 = 0.0f;
    B_800DC9C4 = 0;
    B_800DC9C8 = 0.0f;
    B_800DC9CC = 0.0f;
}

/* 0x800310A0: Drift_Update - a value that springs towards a new random target (0..1) every
 * 20..79 ticks (nothing reads it) */
void func_800310A0(void) {
    if ((s16)--B_800DC9C4 <= 0) {
        B_800DC9C0 = (f32)((f64)(f32)(RAND() % 1024) / 1024.0);
        B_800DC9C4 = RAND() % 60 + 20;
    }
    if (B_800DC9C8 < B_800DC9C0) B_800DC9CC = (f32)((f64)B_800DC9CC + (f64)B_800DC9C0 * 0.001);
    else B_800DC9CC = (f32)((f64)B_800DC9CC - (f64)B_800DC9C0 * 0.0007);
    B_800DC9CC = (f32)((f64)B_800DC9CC * 0.985);
    B_800DC9C8 = B_800DC9C8 + B_800DC9CC;
}

/* 0x80031200: Stage_AddSpark */
void func_80031200(Stage *s, f32 x, f32 y, f32 z, f32 size) {
    StageSpark *k;
    s32 i;

    for (i = 0; i < 200; i++)
        if (!(0.0 <= (f64)s->sparks[i].t)) break;
    if (i >= 200) return;
    k = &s->sparks[i];
    k->t = 0.0f;
    k->pos[0] = x;
    k->pos[1] = y;
    k->pos[2] = z;
    k->size = size;
}

/* 0x8003128C: Stage_AddBit - a bit of clay flying up */
void func_8003128C(Stage *s, f32 x, f32 y, f32 z, f32 size) {
    StageBit *b;
    s32 i;

    for (i = 0; i < 300; i++)
        if (!(0.0 <= (f64)s->bits[i].life)) break;
    if (i >= 300) return;
    b = &s->bits[i];
    b->life = (f32)((f64)size * 3.0);
    b->pos[0] = x;
    b->pos[1] = y;
    b->pos[2] = z;
    b->vel[0] = (f32)(((f64)(f32)(RAND() % 1024) / 512.0 - 1.0) * 1.5);
    b->vel[1] = (f32)((f64)size * 11.0);
    b->vel[2] = (f32)(((f64)(f32)(RAND() % 1024) / 512.0 - 1.0) * 1.5);
    b->size = (f32)(RAND() % 6 + 1);
}
