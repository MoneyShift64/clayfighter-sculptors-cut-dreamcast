/*
 * 0x8002C688-0x8002EA90: logos and small graphics.
 *   - a meter drawn from 16-pixel clay pieces (0x8002C688/0x8002C748, nothing calls them) and
 *     its pictures (0x8002C92C, loaded by Game_Init);
 *   - the licence screen's animated logo (0x8002C9A0-0x8002DF30, LICENSE.C): two 3D models
 *     turning in opposite directions, lit by a light that flashes, throwing out sparks;
 *   - the title menu's logo (0x8002DF30-0x8002EA90, MENUS.C, at MenuScreen+0x58): the four
 *     parts of the title that wobble and bounce, over a background picture.
 */
#include "common.h"
#include "mpfplat.h"
#include "combat.h"

void func_80080590(u32 **dl, void *image, s32 *rect, s32 x, s32 y, s32 flags);   /* a 2D image */
void func_800831F4(u32 **dl, void *model);                                       /* a 3D model */
u8  *mpDecodeBitmapTo32Bit(u8 *bmp);

#define RAND() ((s32)guRandom())          /* (30 bits) */

static u8 *ImageAt(u8 *file, s32 at) { return file + *(s32 *)(file + at); }

/* ---------------------------------------------------------------- the clay meter */

typedef struct ClayMeter {
    /* 0x00 */ f32 x, y;
    /* 0x08 */ s32 v8;
    /* 0x0C */ s32 leftward;               /* grows to the left */
    /* 0x10 */ s32 value;                  /* 16 per whole piece */
    /* 0x14 */ s8  shape[24];              /* each whole piece's picture (0..2) */
    /* 0x2C */ u8  flip[24];               /* ... and its mirroring (1 = x, 2 = y) */
    /* 0x44 */ u8  lastFlip;               /* the part piece's mirroring */
} ClayMeter;

extern u8 *D_800ACA50;                     /* the pieces' pictures */

/* 0x8002C688: ClayMeter_Set - a new value; the whole pieces get random shapes */
void func_8002C688(ClayMeter *m, s32 value) {
    s32 i;

    if (value == m->value) return;
    m->value = value;
    for (i = 0; value >= 17; i++) {
        m->shape[i] = RAND() % 3;
        m->flip[i] = RAND() % 2;
        value -= 16;
    }
    m->lastFlip = 0;
}

/* 0x8002C748: ClayMeter_Draw */
void func_8002C748(ClayMeter *m, u32 **dl) {
    s32 step = (m->leftward != 0) ? -1 : 1;
    s32 x, n, i;
    u8 f;

    func_8007EB48(dl);
    x = (s32)m->x;
    n = m->value;
    if (m->leftward != 0) x -= 15;
    step <<= 4;
    for (i = 0; n >= 17; i++) {
        f = m->flip[i];
        func_80080590(dl, ImageAt(D_800ACA50, 0xC + m->shape[i] * 8), NULL, x + ((f & 1) ? 15 : 0),
                      (s32)((f32)((f & 2) ? 9 : 0) + m->y), (s8)f);
        n -= 16;
        x += step;
    }
    if (n <= 0) return;
    f = m->lastFlip;                        /* the last, part piece: one of 16 sizes */
    if (m->leftward != 0)
        func_80080590(dl, ImageAt(D_800ACA50, 0x1C + n * 8), NULL, x, (s32)((f32)((f & 2) ? 9 : 0) + m->y),
                      (s8)m->lastFlip);
    else
        func_80080590(dl, ImageAt(D_800ACA50, 0x1C + n * 8), NULL, x + 15,
                      (s32)((f32)((f & 2) ? 9 : 0) + m->y), (s8)(m->lastFlip | 1));
}

/* 0x8002C92C: ClayMeter_Load - its pictures (once) */
void func_8002C92C(void) {
    u32 size;

    if (D_800ACA50 != NULL) return;
    size = 0xE22190 - 0xE21760;
    D_800ACA50 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
    RomCopy(D_800ACA50, 0xE21760, size);
}

/* ---------------------------------------------------------------- the licence logo */

typedef struct LogoSpark {                 /* 0x20 bytes */
    /* 0x00 */ f32 life;                   /* 2 .. 0 (fades below 1) */
    /* 0x04 */ f32 pos[3];
    /* 0x10 */ f32 vel[3];
    /* 0x1C */ f32 size;
} LogoSpark;

typedef struct Logo {                      /* 0x194C bytes (LicenseScreen+0) */
    /* 0x0000 */ f32 m[16];                /* where it is */
    /* 0x0040 */ s32 angle;                /* the models' turn (0..0xFFFF) */
    /* 0x0044 */ s32 quarter;              /* which quarter turn it is in (90, 180, 270, 0) */
    /* 0x0048 */ LogoSpark sparks[200];
    /* 0x1948 */ f32 flash;                /* the light, 1 -> 0 */
} Logo;

typedef struct Light {                     /* an N64 Light_t (0x10 bytes) */
    u8 col[3], pad1, colc[3], pad2;
    s8 dir[3], pad3;
    u8 pad4[4];
} Light;
typedef struct Lights4 {                   /* Lights4 (0x48 bytes) */
    u8 a[8];                               /* ambient */
    Light l[4];
} Lights4;

extern u8 *D_800ACAF0, *D_800ACAF4;        /* the two models */
extern u8 *D_800ACAF8;                     /* the spark picture */
extern s32 D_800ACAFC;                     /* how many logos use them */
extern Lights4 D_800ACA60, D_800ACAA8;     /* each model's lights; the 4th light flashes */
extern f32 B_800F5AA8[3], B_800F5AB8[3];   /* the camera's right and up directions */

static u8 *LoadFile(u32 start, u32 end) {
    u8 *p = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, end - start, 0);
    RomCopy(p, start, end - start);
    return p;
}

/* 0x8002C9A0: Logo_Init */
void func_8002C9A0(Logo *l) {
    s32 i;

    func_80087E20(l->m);
    l->angle = 0x3FFF;
    l->quarter = 90;
    for (i = 0; i < 200; i++) l->sparks[i].life = 0.0f;
    l->flash = 0.0f;
    if (D_800ACAF0 == NULL) D_800ACAF0 = LoadFile(0xF9DF80, 0xFA4B40);
    if (D_800ACAF4 == NULL) D_800ACAF4 = LoadFile(0xFA4B40, 0xFA6890);
    if (D_800ACAF8 == NULL) {
        D_800ACAF8 = LoadFile(0xFA6890, 0xFA6AC0);
        *(s16 *)(D_800ACAF8 + 0xA) = *(s16 *)(D_800ACAF8 + 6) / 2;     /* its centre */
        *(s16 *)(D_800ACAF8 + 0xC) = *(s16 *)(D_800ACAF8 + 8) / 2;
    }
    D_800ACAFC++;
}

/* 0x8002CB44: Logo_Free */
void func_8002CB44(Logo *l) {
    (void)l;
    if (--D_800ACAFC > 0) return;
    D_800ACAFC = 0;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACAF0);
    D_800ACAF0 = NULL;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACAF4);
    D_800ACAF4 = NULL;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACAF8);
    D_800ACAF8 = NULL;
}

/* where sparks come from: a point on the logo, turned with it */
static void EmitPoint(Logo *l, f32 x, f32 y, f32 *out) {
    f32 m[16];
    f32 v[3];

    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    func_80087EE8(m, x, y, v[0]);
    func_80088224(m, l->angle);
    func_80088100(m, l->m);
    out[0] = (f32)((f64)(v[0] * m[0] + v[1] * m[4] + v[2] * m[8]) + (f64)m[12]);
    out[1] = (f32)((f64)(v[0] * m[1] + v[1] * m[5] + v[2] * m[9]) + (f64)m[13]);
    out[2] = (f32)((f64)(v[0] * m[2] + v[1] * m[6] + v[2] * m[10]) + (f64)m[14]);
}

/* 0x8002CBCC: Logo_Update - once a tick */
void func_8002CBCC(Logo *l) {
    s32 n[5];
    f32 at[5][4];
    LogoSpark *s;
    s32 i, k, a;
    f32 r;

    n[0] = n[1] = n[2] = n[3] = 0;
    n[4] = 0;      /* (the original never sets this one before use: the caller's $fp, 0 in the game) */
    l->angle = (l->angle - 0x16C) & 0xFFFF;
    for (i = 0; i < 200; i++) {
        s = &l->sparks[i];
        if (!(0.0 < (f64)s->life)) continue;
        s->life = (f32)((f64)s->life - 0.02);
        s->pos[0] += s->vel[0];
        s->pos[1] += s->vel[1];
        s->pos[2] += s->vel[2];
        s->vel[1] = (f32)((f64)s->vel[1] - 0.02);       /* gravity */
        if ((f64)s->vel[1] < -6.0) s->vel[1] = -6.0f;
        if ((f64)s->life < 0.0) s->life = 0.0f;
    }
    if (0.0 < (f64)l->flash) {
        l->flash = (f32)((f64)l->flash - 0.05);
        if ((f64)l->flash < 0.0) l->flash = 0.0f;
    }
    /* each quarter turn: a flash and a burst of sparks */
    if (0xBFFF < l->angle && l->quarter != 0) {
        l->quarter = 0;
        n[2] = 4; n[3] = 4;
        l->flash = 0.6f;
    }
    if ((u32)(l->angle - 0x8000) < 0x4000 && l->quarter != 270) {
        l->quarter = 270;
        n[0] = 10; n[1] = 8; n[4] = 2;
        l->flash = 0.2f;
    }
    if ((u32)(l->angle - 0x4000) < 0x4000 && l->quarter != 180) {
        l->quarter = 180;
        n[2] = 4; n[3] = 4;
        l->flash = 0.6f;
    }
    if (l->angle < 0x4000 && l->quarter != 90) {
        l->quarter = 90;
        n[0] = 10; n[1] = 8; n[4] = 2;
        l->flash = 1.0f;
    }
    for (k = 0; k < 5; k++) n[k] *= 3;
    if (!(n[0] > 0 || n[1] > 0 || n[2] != 0 || n[3] != 0 || n[4] != 0)) return;
    EmitPoint(l, -47.0f, -7.0f, at[0]);
    EmitPoint(l, -26.0f, -10.0f, at[1]);
    EmitPoint(l, 12.5f, -10.0f, at[2]);
    EmitPoint(l, 55.0f, -3.0f, at[3]);
    EmitPoint(l, 12.0f, -15.0f, at[4]);
    for (i = 0; i < 200; i++) {
        if (!(n[0] > 0 || n[1] > 0 || n[2] > 0 || n[3] > 0 || n[4] > 0)) return;
        s = &l->sparks[i];
        if ((f64)s->life != 0.0) continue;
        if (n[0] > 0)      k = 0;
        else if (n[1] > 0) k = 1;
        else if (n[2] > 0) k = 2;
        else if (n[3] > 0) k = 3;
        else               k = 4;
        s->pos[0] = at[k][0];
        s->pos[1] = at[k][1];
        s->pos[2] = at[k][2];
        n[k]--;
        a = (s32)((f64)((f32)(RAND() % 360) * 65536.0f) / 360.0);
        r = (f32)((f64)(f32)(RAND() % 1000) / 1000.0);
        s->vel[0] = (f32)((f64)(vsSin(a) * r) * 0.8);
        s->vel[2] = (f32)((f64)(vsCos(a) * r) * 0.8);
        s->vel[1] = (f32)((f64)r * -0.4);
        s->life = (f32)((f64)(f32)(RAND() % 2000) / 1000.0);
        s->size = (f32)((f64)(f32)(RAND() % 1000) / 1000.0 + 1.0);
    }
}

static void Cmd(u32 w0, u32 w1) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = w0;
    g[1] = w1;
}

/* gSPSetLights4 */
static void SetLights4(Lights4 *lt) {
    Cmd(0xBC000002, 0x800000A0);
    Cmd(0x03860010, (u32)&lt->l[0]);
    Cmd(0x03880010, (u32)&lt->l[1]);
    Cmd(0x038A0010, (u32)&lt->l[2]);
    Cmd(0x038C0010, (u32)&lt->l[3]);
    Cmd(0x038E0010, (u32)lt->a);
}

static void Flash(Light *li, f32 k) {
    li->col[0] = (u32)((f64)k * 200.0);
    li->col[1] = (u32)((f64)k * 200.0);
    li->col[2] = (u32)((f64)k * 255.0);
    li->colc[0] = li->col[0];
    li->colc[1] = li->col[1];
    li->colc[2] = li->col[2];
}

/* 0x8002D680: Logo_Draw */
void func_8002D680(Logo *l) {
    f32 up[3], right[3], up2[3], right2[3];
    f32 m[16];
    LogoSpark *s;
    f32 k;
    s32 i;
    u8 c;

    up[0] = B_800F5AB8[0];
    up[1] = B_800F5AB8[1];
    up[2] = B_800F5AB8[2];
    right[0] = B_800F5AA8[0];
    right[1] = B_800F5AA8[1];
    right[2] = B_800F5AA8[2];
    k = (f32)((f64)l->flash * 0.7);
    Flash(&D_800ACA60.l[3], k);
    SetLights4(&D_800ACA60);
    func_80087FD0(m, l->angle);
    func_80088100(m, l->m);
    func_80080170(&gDL, m);
    func_800831F4(&gDL, D_800ACAF0);
    Flash(&D_800ACAA8.l[3], k);
    SetLights4(&D_800ACAA8);
    func_80087FD0(m, -l->angle);
    func_80088100(m, l->m);
    func_80080170(&gDL, m);
    func_800831F4(&gDL, D_800ACAF4);
    func_8007FB08(&gDL, 0x204);
    func_80087E20(m);
    func_80080170(&gDL, m);
    for (i = 0; i < 200; i++) {
        s = &l->sparks[i];
        if (!(0.0 < (f64)s->life)) continue;
        c = RAND() % 50 - 126;                          /* a flickering blue-white */
        B_800F7788 = c; B_800F7E70 = c; B_800F7D94 = c; B_800F7778 = c;
        c = RAND() % 50 - 106;
        B_800F88BC = c; B_800F88C8 = c; B_800F7789 = c; B_800F7770 = c;
        B_800F7ECC = 255; B_800F2D1C = 255; B_800F7E78 = 255; B_800F4E34 = 255;
        up2[0] = (f32)((f64)up[0] * ((f64)s->size * 0.1));
        up2[1] = (f32)((f64)up[1] * ((f64)s->size * 0.1));
        up2[2] = (f32)((f64)up[2] * ((f64)s->size * 0.1));
        right2[0] = (f32)((f64)right[0] * ((f64)s->size * 0.1));
        right2[1] = (f32)((f64)right[1] * ((f64)s->size * 0.1));
        right2[2] = (f32)((f64)right[2] * ((f64)s->size * 0.1));
        if (1.0 <= (f64)s->life) {
            func_80084CB0(&gDL, D_800ACAF8, s->pos[0], s->pos[1], s->pos[2], up2, right2, 0);
        } else {
            c = (u32)((f64)__builtin_sqrtf(s->life) * 255.0);     /* fading */
            B_800F7E58 = c; B_800F2D1D = c; B_800F8760 = c; B_800F5B78 = c;
            func_80085FE4(&gDL, (CharFrame *)D_800ACAF8, s->pos[0], s->pos[1], s->pos[2], up2, right2, 0);
        }
    }
}

/* ---------------------------------------------------------------- the title menu's logo */

typedef struct MenuLogo {                  /* 0x20 bytes (MenuScreen+0x58) */
    /* 0x00 */ f32 x, y;
    /* 0x08 */ s32 wait;                   /* ticks to the next wobble */
    /* 0x0C */ f32 wobble;                 /* counts down; its fraction rocks the letters */
    /* 0x10 */ f32 t;                      /* the bounce, 0..1 */
    /* 0x14 */ f32 dt;
    /* 0x18 */ f32 size;                   /* how big the bounce is */
    /* 0x1C */ f32 slide;                  /* how far the parts slide in from the left */
} MenuLogo;

extern u8 *D_800ACB00;                     /* the logo's file */
extern u8 *D_800ACB04, *D_800ACB08, *D_800ACB0C, *D_800ACB10;   /* its four parts (decoded) */
extern u8 *D_800ACB14;                     /* the background (in the file) */
extern s32 D_800ACB18;                     /* how many use them */
extern s32 D_800ACB1C, D_800ACB20, D_800ACB24, D_800ACB28, D_800ACB2C, D_800ACB30;   /* parts' offsets */

static u8 *DecodeCentred(s32 at) {
    u8 *img = mpDecodeBitmapTo32Bit(D_800ACB00 + *(s32 *)(D_800ACB00 + at));
    *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
    *(s16 *)(img + 0xC) = *(s16 *)(img + 8) / 2;
    return img;
}

/* 0x8002DF30: MenuLogo_Init */
void func_8002DF30(MenuLogo *l) {
    u8 *bg;

    if (D_800ACB00 == NULL) {
        D_800ACB00 = func_80080360(0xFAA000);
        D_800ACB04 = DecodeCentred(0xC);
        D_800ACB08 = DecodeCentred(0x14);
        D_800ACB0C = DecodeCentred(0x1C);
        D_800ACB10 = DecodeCentred(0x24);
        bg = ImageAt(D_800ACB00, 0x2C);
        D_800ACB14 = bg;
        *(u16 *)(bg + 0xA) = *(u16 *)(bg + 6) - 1;      /* (its bottom-right corner) */
        *(u16 *)(bg + 0xC) = *(u16 *)(bg + 8) - 1;
    }
    D_800ACB18++;
    l->x = 0.0f;
    l->y = 10.0f;
    l->wait = RAND() % 300 + 100;
    l->wobble = 0.0f;
    l->t = 0.0f;
    l->dt = 0.0f;
    l->size = 0.0f;
}

/* 0x8002E160: MenuLogo_Free (the decoded parts' pointers are left dangling) */
void func_8002E160(MenuLogo *l) {
    (void)l;
    if (--D_800ACB18 > 0) return;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACB00);
    D_800ACB00 = NULL;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACB04);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACB08);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACB0C);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800ACB10);
}

/* 0x8002E1F8: MenuLogo_Update - once a tick */
void func_8002E1F8(MenuLogo *l) {
    if (--l->wait <= 0 && (f64)l->wobble <= 0.0) {
        l->wait = RAND() % 300 + 100;
        l->wobble = (f32)((RAND() % 2 + 1) * 2);       /* rock 2 or 4 times */
    }
    l->wobble = (f32)((f64)l->wobble - 0.03);
    if ((f64)l->wobble < 0.0) l->wobble = 0.0f;
    l->t = l->t + l->dt;
    if (1.0 < (f64)l->t) {
        l->t = 0.0f;
        l->dt = 0.0f;
    }
}

static void Scaled(f32 *out, f32 *v, f64 k) {
    out[0] = (f32)((f64)v[0] * k);
    out[1] = (f32)((f64)v[1] * k);
    out[2] = (f32)((f64)v[2] * k);
}

/* 0x8002E334: MenuLogo_Draw */
void func_8002E334(MenuLogo *l) {
    f32 up[3], right[3], up2[3], right2[3];
    f32 rock, b, in;
    f64 bd;
    s32 n;

    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);
    up[0] = 0.0f;
    up[1] = 1.0f;
    up[2] = 0.0f;
    right[0] = -1.0f;
    right[1] = 0.0f;
    right[2] = 0.0f;
    n = (s32)l->wobble;
    if (n & 1) rock = l->wobble - (f32)n;
    else rock = (f32)(1.0 - (f64)(l->wobble - (f32)n));
    rock = (f32)((f64)rock * 8.0);
    b = vsSin((s32)(l->t * 32768.0f)) * l->size;
    in = ((f64)l->t == 0.0) ? 0.0f : (f32)(1.0 - (f64)l->t);
    bd = (f64)b;
    Cmd(0xBA000C02, 0);                     /* point sampling */
    Scaled(up2, up, bd * 0.1 + 1.0);
    Scaled(right2, right, bd * 0.1 + 1.0);
    func_80083A64(&gDL, D_800ACB04, l->x, l->y, 0.0f, up2, right2, 8);
    Scaled(up2, up, bd * 0.2 + 1.0);
    Scaled(right2, right, bd * 0.2 + 1.0);
    func_80083A64(&gDL, D_800ACB10,
                  (f32)((f64)l->x - bd * 5.0 - (f64)l->slide * 20.0 * (f64)in + (f64)D_800ACB2C + (f64)(s32)rock),
                  (f32)D_800ACB30 + l->y, 0.0f, up2, right2, 8);
    func_8007EB48(&gDL);
    func_80080590(&gDL, D_800ACB14, NULL, 319, 239, 0);
    func_8007EEF0(&gDL);
    Cmd(0xBA000C02, 0);
    Scaled(up2, up, bd * 0.5 + 1.0);
    Scaled(right2, right, bd * 0.5 + 1.0);
    func_80083A64(&gDL, D_800ACB08,
                  (f32)((f64)l->x - bd * 20.0 - (f64)l->slide * 40.0 * (f64)in + (f64)D_800ACB1C),
                  (f32)D_800ACB20 + l->y, 0.0f, up2, right2, 8);
    Scaled(up2, up, bd + 1.0);
    Scaled(right2, right, bd + 1.0);
    func_80083A64(&gDL, D_800ACB0C,
                  (f32)((f64)l->x + bd * 30.0 - (f64)l->slide * 100.0 * (f64)in + (f64)D_800ACB24),
                  (f32)D_800ACB28 + l->y, 0.0f, up2, right2, 8);
}

/* 0x8002E9B0: MenuLogo_Bounce - a big bounce (a menu changed), with a sound */
void func_8002E9B0(MenuLogo *l) {
    l->t = 0.5f;
    l->dt = 0.05f;
    l->size = 0.4f;
    l->slide = 0.0f;
    func_800797EC(B_800F8768, 0, 0, 8, 0x7FFF, 0, 0, 1.0f, 0);
}

/* 0x8002EA3C: MenuLogo_SlideIn - a slow bounce with the parts sliding in (from the title) */
void func_8002EA3C(MenuLogo *l) {
    l->t = 0.3f;
    l->dt = 0.007f;
    l->size = 0.7f;
    l->slide = 1.0f;
}
