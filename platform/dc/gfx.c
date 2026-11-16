/*
 * gfx.c - runs the game's display lists: the F3DEX microcode's commands (the RSP: matrices,
 * vertices, lighting, triangles) and the RDP's (textures, rectangles, fills, the colour
 * combiner and blender), turned into screen-space triangles for the platform (plat_gfx_*).
 *
 * Addresses in the lists are physical (pointer - 0x80000000, as the game writes them),
 * masked physical (pointer & 0x1FFFFFFF) or segmented (segments 1-11, set by G_MOVEWORD).
 * All of the port's memory is at 0x8C000000-0x8CFFFFFF (the Dreamcast's RAM; the PC test
 * build is linked there too), so both kinds of physical address are 0x0C000000-0x0CFFFFFF.
 *
 * Texture memory: data is kept the way the port stores it (16- and 32-bit texels as native
 * 16- and 32-bit numbers, 4- and 8-bit ones as bytes), so loads copy bytes and the texel
 * reads use the tile's own size.
 *
 * The colour combiner is worked out per vertex twice, with the texel = 0 and = 1, giving
 *     colour = texel * (c1 - c0) + c0
 * which the platform draws as texture * vertex colour + offset colour.
 */
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include "port.h"
#if defined(__sh__)
#include <sh4zam/shz_sh4zam.h>         /* the SH-4's matrix unit (FTRV) and fast square roots */
#endif

#define MAX_OUT 3072                  /* triangle vertices per batch */

typedef struct Tile {
    u32 fmt, siz, line, tmem, palette;
    u32 cms, cmt, masks, maskt, shifts, shiftt;
    u32 uls, ult, lrs, lrt;           /* 10.2 */
} Tile;

typedef struct Vert {
    f32 x, y, z, w;                   /* clip space */
    f32 s, t;                         /* texels (S10.5 scaled by G_TEXTURE, / 32) */
    f32 r, g, b, a;                   /* shade 0..1 */
    f32 fog;                          /* fog amount 0..1 (G_FOG: the RSP's shade alpha) */
    f32 sx, sy, sz, sq;               /* on the screen (made when loaded; w > 0 only) */
    u32 clip;                         /* outside: 1 left 2 right 4 top 8 bottom 16 behind the near plane */
    s32 ready;                        /* the screen position, colours and fog are made (lazily: most
                                         vertices loaded are never in a triangle on the screen) */
    const u8 *src;                    /* the Vtx (its colour bytes, if not lit) */
    s32 lit, fogOn;
    u32 shade8;                       /* the shade colour as ARGB bytes */
    s32 colF;                         /* r, g, b, a are made (lit vertices: always) */
    u32 gen;                          /* the colours below are for this combiner state (0: none) */
    s32 cTex;
    u32 cArgb, cOargb;
    u8 own[16];                       /* the Vtx, once G_MODIFYVTX has changed it */
    u32 pad[3];                       /* (128 bytes: indexing is a shift) */
} Vert;

/* goes up when the combiner, its colours or the other modes change: the colours a vertex
   was given (and the per-state results below) are good until then */
static u32 sCmbGen = 1, sStateGen;
static s32 sStateTexel, sStateBlend;
#define CMB_CHANGED() (sCmbGen++)

static struct {
    u32 seg[16];
    f32 mv[10][4][4];
    s32 mvTop;
    f32 proj[4][4];
    f32 mvp[4][4];
    s32 mvpDirty;
    Vert v[64];
    u8 vclip[64];                     /* each vertex's clip codes again, together (Tri's quick test) */
    u32 geom;
    u32 omL, omH;
    u32 cmb0, cmb1;
    f32 prim[4], env[4], fog[4], blend[4];
    f32 primLod;
    s32 scenery;                      /* between the port's scenery marks (the arena's passes) */
    f32 fogMul, fogOff;               /* gSPFogPosition: fog alpha = z/w * fogMul + fogOff */
    u32 fill;
    f32 texScaleS, texScaleT;
    s32 texOn, texTile;
    u32 timgAddr, timgFmt, timgSiz, timgWidth;
    u32 cimg, zimg;
    Tile tile[8];
    u8 tmem[8192];
    f32 vpScale[3], vpTrans[3];
    s32 numLights;
    f32 lightCol[8][3], lightDir[8][3];
    f32 lookX[3], lookY[3];
    u32 half1, half2;
    s32 scissor[4];
} G;

static GfxVert sOut[MAX_OUT];
static s32 sOutN;
static GfxTex sBatchTex;
static s32 sBatchHasTex, sBatchBlend, sBatchZcmp, sBatchZupd, sBatchOffset;
static u16 sTexBuf[1024 * 512];
#define STAT(i) ((void)0)

extern u32 gFrameCounter;

/* ------------------------------------------------------------------ addresses */

#define K0(a) ((void *)(a))
static void *Addr(u32 a) {
    u32 s = (a >> 24) & 0xF;
    if ((a >> 28) == 0 && s >= 1 && s <= 0xB && G.seg[s] != 0) {
        return K0(((G.seg[s] + (a & 0xFFFFFF)) & 0x1FFFFFFF) | 0x80000000u);
    }
    if ((a >> 29) == 0) return K0(a | 0x80000000u);         /* physical (K0 or masked) */
    return K0(a);                                           /* already a pointer */
}

/* ------------------------------------------------------------------ matrices */

static void MtxLoad(f32 d[4][4], const u32 *m) {
    s32 i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            u32 hi = m[i * 2 + j / 2], lo = m[8 + i * 2 + j / 2];
            s32 v;
            if (j & 1) v = (s32)((hi << 16) | (lo & 0xFFFF));
            else v = (s32)((hi & 0xFFFF0000u) | (lo >> 16));
            d[i][j] = (f32)v * (1.0f / 65536.0f);
        }
}

static void MtxMul(f32 d[4][4], f32 a[4][4], f32 b[4][4]) {
    f32 r[4][4];
    s32 i, j;
#if defined(__sh__)
    /* row vectors: each row of the result is a row of a through b (b loaded as columns) */
    shz_xmtrx_load_unaligned_4x4(&b[0][0]);
    for (i = 0; i < 4; i++) {
        shz_vec4_t v = shz_xmtrx_transform_vec4((shz_vec4_t){ .x = a[i][0], .y = a[i][1], .z = a[i][2], .w = a[i][3] });
        r[i][0] = v.x; r[i][1] = v.y; r[i][2] = v.z; r[i][3] = v.w;
    }
    (void)j;
    memcpy(d, r, sizeof(r));
    return;
#endif
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
    memcpy(d, r, sizeof(r));
}

static void CmdMtx(u32 w0, u32 w1) {
    u32 p = (w0 >> 16) & 0xFF;
    f32 m[4][4];
    MtxLoad(m, Addr(w1));
    if (p & 1) {                                        /* projection */
        if (p & 2) memcpy(G.proj, m, sizeof(m));
        else MtxMul(G.proj, m, G.proj);
    } else {
        if ((p & 4) && G.mvTop < 9) {                   /* push */
            memcpy(G.mv[G.mvTop + 1], G.mv[G.mvTop], sizeof(m));
            G.mvTop++;
        }
        if (p & 2) memcpy(G.mv[G.mvTop], m, sizeof(m));
        else MtxMul(G.mv[G.mvTop], m, G.mv[G.mvTop]);
    }
    G.mvpDirty = 1;
}

/* ------------------------------------------------------------------ vertices */

static void Normalize(f32 *v) {
    f32 l2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (l2 > 0.0f) {
#if defined(__sh__)
        f32 k = shz_inv_sqrtf(l2);
#else
        f32 k = 1.0f / sqrtf(l2);
#endif
        v[0] *= k; v[1] *= k; v[2] *= k;
    }
}

/* which sides of the view a vertex is outside */
/* the game's microcode is F3DEX.NoN: no clipping at the near plane, things nearer than it are
   drawn all the same (a low camera's floor reaches the bottom of the screen). Only what is
   also too close to the eye to put on the screen is cut, at w = NEAR_W (bit 16 below, and
   NearDist). (Both: an orthographic projection's w is 1 everywhere: the title menu's Frosty) */
#define NEAR_W 4.0f
static inline f32 NearDist(const Vert *d) {       /* >= 0: kept */
    f32 a = d->z + d->w, b = d->w - NEAR_W, e = d->w - NEAR_W * 0.01f;
    if (e < a) a = e;                           /* (and in front of the eye: w > 0) */
    return a > b ? a : b;
}
static inline void ClipCodes(Vert *d) {
    u32 c = 0;
    if (d->x < -d->w) c |= 1;
    if (d->x > d->w) c |= 2;
    if (d->y > d->w) c |= 4;
    if (d->y < -d->w) c |= 8;
    if (NearDist(d) < 0.0f) c |= 16;
    d->clip = c;
}

/* where a vertex is on the screen */
static inline void Project(Vert *d) {
    if (d->w > 0.0f) {
#if defined(__sh__)
        f32 q = shz_invf_fsrra(d->w);                   /* (FSRRA: w > 0 here) */
#else
        f32 q = 1.0f / d->w;
#endif
        d->sq = q;
        d->sx = d->x * q * G.vpScale[0] + G.vpTrans[0];
        d->sy = -d->y * q * G.vpScale[1] + G.vpTrans[1];
        d->sz = (d->z * q + 1.0f) * 0.5f;
        d->sz = d->sz < 0.0f ? 0.0f : d->sz > 1.0f ? 1.0f : d->sz;
    }
}

static void CmdVtx(u32 w0, u32 w1) {
    s32 n = (w0 >> 10) & 0x3F, v0 = ((w0 >> 16) & 0xFF) / 2, i;
    const u8 *src = Addr(w1);
    f32 (*m)[4];
    f32 (*mv)[4] = G.mv[G.mvTop];

    if (G.mvpDirty) { MtxMul(G.mvp, G.mv[G.mvTop], G.proj); G.mvpDirty = 0; }
    m = G.mvp;
#if defined(__sh__)
    shz_xmtrx_load_unaligned_4x4(&m[0][0]);
#endif
    for (i = 0; i < n && v0 + i < 64; i++, src += 16) {
        const s16 *sv = (const s16 *)src;               /* x, y, z, flag, s, t: native 16-bit */
        Vert *d = &G.v[v0 + i];
        f32 x = sv[0], y = sv[1], z = sv[2];
#if defined(__sh__)
        {
            shz_vec4_t r = shz_xmtrx_transform_vec4((shz_vec4_t){ .x = x, .y = y, .z = z, .w = 1.0f });
            d->x = r.x; d->y = r.y; d->z = r.z; d->w = r.w;
        }
#else
        d->x = x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0];
        d->y = x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1];
        d->z = x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2];
        d->w = x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3];
#endif
        d->s = sv[4] * G.texScaleS / 32.0f;
        d->t = sv[5] * G.texScaleT / 32.0f;
        if (G.geom & 0x20000) {                          /* G_LIGHTING: the colour is a normal */
            f32 n[3], r, g, b;
            s32 k;
            n[0] = (s8)src[12]; n[1] = (s8)src[13]; n[2] = (s8)src[14];
            {
#if defined(__sh__)
                f32 t0 = shz_dot8f(n[0], n[1], n[2], 0.0f, mv[0][0], mv[1][0], mv[2][0], 0.0f);   /* (FIPR) */
                f32 t1 = shz_dot8f(n[0], n[1], n[2], 0.0f, mv[0][1], mv[1][1], mv[2][1], 0.0f);
                f32 t2 = shz_dot8f(n[0], n[1], n[2], 0.0f, mv[0][2], mv[1][2], mv[2][2], 0.0f);
#else
                f32 t0 = n[0] * mv[0][0] + n[1] * mv[1][0] + n[2] * mv[2][0];
                f32 t1 = n[0] * mv[0][1] + n[1] * mv[1][1] + n[2] * mv[2][1];
                f32 t2 = n[0] * mv[0][2] + n[1] * mv[1][2] + n[2] * mv[2][2];
#endif
                n[0] = t0; n[1] = t1; n[2] = t2;
            }
            Normalize(n);
            r = G.lightCol[G.numLights][0];
            g = G.lightCol[G.numLights][1];
            b = G.lightCol[G.numLights][2];
            for (k = 0; k < G.numLights; k++) {
#if defined(__sh__)
                f32 d2 = shz_dot8f(n[0], n[1], n[2], 0.0f, G.lightDir[k][0], G.lightDir[k][1], G.lightDir[k][2], 0.0f);
#else
                f32 d2 = n[0] * G.lightDir[k][0] + n[1] * G.lightDir[k][1] + n[2] * G.lightDir[k][2];
#endif
                if (d2 > 0.0f) { r += d2 * G.lightCol[k][0]; g += d2 * G.lightCol[k][1]; b += d2 * G.lightCol[k][2]; }
            }
            d->r = r > 1.0f ? 1.0f : r;
            d->g = g > 1.0f ? 1.0f : g;
            d->b = b > 1.0f ? 1.0f : b;
            if (G.geom & 0x40000) {                      /* G_TEXTURE_GEN: an environment map */
                f32 ds = n[0] * G.lookX[0] + n[1] * G.lookX[1] + n[2] * G.lookX[2];
                f32 dt = n[0] * G.lookY[0] + n[1] * G.lookY[1] + n[2] * G.lookY[2];
                d->s = (ds + 1.0f) * 0.5f * G.texScaleS * 32.0f;
                d->t = (dt + 1.0f) * 0.5f * G.texScaleT * 32.0f;
            }
            d->lit = 1;
        } else {
            d->lit = 0;
        }
        d->src = src;
        d->fogOn = (G.geom & 0x10000) != 0;
        ClipCodes(d);
        G.vclip[v0 + i] = d->clip;
        d->ready = 0;
        d->gen = 0;
    }
}

/* G_MODIFYVTX: a loaded vertex's colour or texture coordinates (not scaled by G_TEXTURE) */
static void CmdModifyVtx(u32 w0, u32 w1) {
    Vert *d = &G.v[((w0 & 0xFFFF) / 2) & 63];
    switch ((w0 >> 16) & 0xFF) {
    case 0x10:                                          /* G_MWO_POINT_RGBA */
        if (d->src != d->own) {
            const u32 *a = (const u32 *)d->src;
            u32 *b = (u32 *)d->own;
            b[0] = a[0]; b[1] = a[1]; b[2] = a[2];
            d->src = d->own;
        }
        d->own[12] = (u8)(w1 >> 24); d->own[13] = (u8)(w1 >> 16); d->own[14] = (u8)(w1 >> 8); d->own[15] = (u8)w1;
        d->lit = 0;
        d->shade8 = (w1 << 24) | (w1 >> 8);             /* (as Finish makes it: the position stays) */
        d->colF = 0; d->gen = 0;
        break;
    case 0x14:                                          /* G_MWO_POINT_ST (only the texture coordinates) */
        d->s = (s16)(w1 >> 16) / 32.0f;
        d->t = (s16)w1 / 32.0f;
        break;
    default: break;
    }
}

/* the shade colour as floats (for the general combiner, and clipping) */
static void ColF(Vert *d) {
    const u8 *src = d->src;
    if (!d->lit) {
        d->r = src[12] * (1.0f / 255.0f);
        d->g = src[13] * (1.0f / 255.0f);
        d->b = src[14] * (1.0f / 255.0f);
    }
    d->a = src[15] * (1.0f / 255.0f);
    d->colF = 1;
}

/* the rest of a vertex, when a triangle on the screen first uses it */
static void Finish(Vert *d) {
    const u8 *src = d->src;
    d->colF = 0;
    if (d->lit) d->a = src[15] * (1.0f / 255.0f);
    if (!d->lit) d->shade8 = ((u32)src[15] << 24) | ((u32)src[12] << 16) | ((u32)src[13] << 8) | src[14];
    else d->shade8 = ((u32)src[15] << 24) | ((u32)(d->r * 255.0f + 0.5f) << 16) | ((u32)(d->g * 255.0f + 0.5f) << 8) |
                     (u32)(d->b * 255.0f + 0.5f);
    Project(d);
    d->fog = 0.0f;
    if (d->fogOn && d->w > 0.0f) {                       /* G_FOG */
        f32 f = d->z * d->sq * G.fogMul + G.fogOff;
        d->fog = f <= 0.0f ? 0.0f : f >= 255.0f ? 1.0f : f * (1.0f / 255.0f);
    }
    d->ready = 1;
}

static void Light(s32 i, const u8 *p) {
    /* Light_t: col[3], pad, colc[3], pad, dir[3] (s8), pad */
    G.lightCol[i][0] = p[0] / 255.0f;
    G.lightCol[i][1] = p[1] / 255.0f;
    G.lightCol[i][2] = p[2] / 255.0f;
    G.lightDir[i][0] = (s8)p[8];
    G.lightDir[i][1] = (s8)p[9];
    G.lightDir[i][2] = (s8)p[10];
    Normalize(G.lightDir[i]);
}

static void CmdMoveMem(u32 w0, u32 w1) {
    u32 idx = (w0 >> 16) & 0xFF;
    const u8 *p = Addr(w1);
    if (idx == 0x80) {                                   /* viewport: s16 scale[4], trans[4] (x4) */
        const s16 *v = (const s16 *)p;
        G.vpScale[0] = v[0] / 4.0f; G.vpScale[1] = v[1] / 4.0f; G.vpScale[2] = v[2] / 4.0f;
        G.vpTrans[0] = v[4] / 4.0f; G.vpTrans[1] = v[5] / 4.0f; G.vpTrans[2] = v[6] / 4.0f;
    } else if (idx >= 0x86 && idx <= 0x94) {
        Light((idx - 0x86) / 2, p);
    } else if (idx == 0x82) {
        G.lookY[0] = (s8)p[8]; G.lookY[1] = (s8)p[9]; G.lookY[2] = (s8)p[10]; Normalize(G.lookY);
    } else if (idx == 0x84) {
        G.lookX[0] = (s8)p[8]; G.lookX[1] = (s8)p[9]; G.lookX[2] = (s8)p[10]; Normalize(G.lookX);
    }
}

static void CmdMoveWord(u32 w0, u32 w1) {
    u32 idx = w0 & 0xFF, off = (w0 >> 8) & 0xFFFF;
    switch (idx) {
    case 0x02: G.numLights = (s32)((w1 & 0x7FFFFFFF) / 32) - 1; if (G.numLights < 0) G.numLights = 0; if (G.numLights > 7) G.numLights = 7; break;
    case 0x06: G.seg[(off / 4) & 0xF] = w1 & 0x1FFFFFFF; break;
    case 0x08:                                          /* fog position */
        G.fogMul = (f32)(s16)(w1 >> 16); G.fogOff = (f32)(s16)w1; CMB_CHANGED(); break;
    default: break;
    }
}

/* ------------------------------------------------------------------ textures */

/* (Fletcher-like: two sums, the second of the first, so the order counts; cheap on the SH-4,
   which has no fast multiply to spend on every word) */
/* a 32-bit load from any address: the SH-4 faults on a misaligned word (emulators don't) */
static u32 Ld32(const u8 *p) { u32 v; memcpy(&v, p, 4); return v; }

static u32 Hash(const u8 *p, u32 n, u32 h) {
    const u32 *w = (const u32 *)p;
    u32 i, a = h, b = h ^ 0x9E3779B9u;
    if (((uintptr_t)p & 3) != 0) {                      /* (a texture row on a 2-byte boundary) */
        for (i = 0; i + 4 <= n; i += 4) { a += Ld32(p + i); b += a; }
        for (; i < n; i++) { a += p[i]; b += a; }
        return (a ^ (b << 7) ^ (b >> 25)) * 0x01000193u ^ b;
    }
    for (i = 0; i + 8 <= n; i += 8, w += 2) { a += w[0]; b += a; a += w[1]; b += a; }
    if (i + 4 <= n) { a += w[0]; b += a; i += 4; }
    for (; i < n; i++) { a += p[i]; b += a; }
    return (a ^ (b << 7) ^ (b >> 25)) * 0x01000193u ^ b;
}

static u32 Pow2(u32 v) {
    u32 p = 8;
    while (p < v && p < 1024) p <<= 1;
    return p;
}

/* a texel of tile t at (s, t) in the tile's own coordinates, as 8-bit RGBA */
static void Texel(const Tile *tl, u32 s, u32 t, u8 *o) {
    u32 base = tl->tmem * 8, stride = tl->line * 8, a, c;
    const u8 *m = G.tmem;
    u32 tlut = (G.omH >> 14) & 3;                       /* 2 RGBA16, 3 IA16 */
    switch (tl->siz) {
    case 0: {                                           /* 4-bit */
        u32 b = m[((base + t * stride + s / 2) ^ ((t & 1) << 2)) & 0x1FFF];
        u32 v = (s & 1) ? (b & 0xF) : (b >> 4);
        if (tl->fmt == 2 || (tl->fmt == 0 && tlut)) {   /* CI4 */
            a = 0x800 + ((tl->palette << 4) | v) * 8;
            c = *(const u16 *)(m + (a & 0x1FFF));
            goto pal;
        }
        if (tl->fmt == 3) {                             /* IA4 */
            u8 i = ((v >> 1) * 255) / 7;
            o[0] = o[1] = o[2] = i; o[3] = (v & 1) ? 255 : 0;
        } else {                                        /* I4 */
            o[0] = o[1] = o[2] = o[3] = (u8)(v * 17);
        }
        return;
    }
    case 1: {                                           /* 8-bit */
        u32 v = m[((base + t * stride + s) ^ ((t & 1) << 2)) & 0x1FFF];
        if (tl->fmt == 2 || (tl->fmt == 0 && tlut)) {
            a = 0x800 + v * 8;
            c = *(const u16 *)(m + (a & 0x1FFF));
            goto pal;
        }
        if (tl->fmt == 3) { o[0] = o[1] = o[2] = (u8)((v >> 4) * 17); o[3] = (u8)((v & 15) * 17); }
        else { o[0] = o[1] = o[2] = o[3] = (u8)v; }
        return;
    }
    case 2:                                             /* 16-bit */
        c = *(const u16 *)(m + (((base + t * stride + s * 2) ^ ((t & 1) << 2)) & 0x1FFE));
        if (tl->fmt == 3) { o[0] = o[1] = o[2] = (u8)(c >> 8); o[3] = (u8)c; return; }
        break;
    default:                                            /* 32-bit (kept in a row twice as long) */
        c = *(const u32 *)(m + ((base + t * stride * 2 + s * 4) & 0x1FFC));
        o[0] = (u8)(c >> 24); o[1] = (u8)(c >> 16); o[2] = (u8)(c >> 8); o[3] = (u8)c;
        return;
    }
    /* RGBA5551 */
    o[0] = (u8)(((c >> 11) & 31) * 255 / 31);
    o[1] = (u8)(((c >> 6) & 31) * 255 / 31);
    o[2] = (u8)(((c >> 1) & 31) * 255 / 31);
    o[3] = (c & 1) ? 255 : 0;
    return;
pal:
    if (tlut == 3) { o[0] = o[1] = o[2] = (u8)(c >> 8); o[3] = (u8)c; return; }
    o[0] = (u8)(((c >> 11) & 31) * 255 / 31);
    o[1] = (u8)(((c >> 6) & 31) * 255 / 31);
    o[2] = (u8)(((c >> 1) & 31) * 255 / 31);
    o[3] = (c & 1) ? 255 : 0;
}

/* set when texture memory, a tile or the texture modes change: until then the last texture
   made is the same again (the triangles of a model share it) */
static s32 sTexDirty = 1, sLastTile = -1;
static GfxTex sLastTex;
static f32 sLastTw, sLastTh;

/* ---- decoding a texture from TMEM into sTexBuf (pw x ph, the last texel repeated past w x h), as
   ARGB1555 or, when it has alpha between 0 and 1, ARGB4444. Formats with at most 256 values
   (colour-indexed, 4- and 8-bit grey) go through a table; the others are converted directly. */
static u16 To1555(const u8 *o) { return (u16)(((o[3] >> 7) << 15) | ((o[0] >> 3) << 10) | ((o[1] >> 3) << 5) | (o[2] >> 3)); }
static u16 To4444(const u8 *o) { return (u16)(((o[3] >> 4) << 12) | ((o[0] >> 4) << 8) | ((o[1] >> 4) << 4) | (o[2] >> 4)); }

static s32 Decode(const Tile *tl, u32 w, u32 h, u32 pw, u32 ph) {
    const u8 *m = G.tmem;
    u32 base = tl->tmem * 8, stride = tl->line * 8, x, y, i;
    u32 tlut = (G.omH >> 14) & 3;
    s32 soft = 0;
    u8 o[4];
    if (tl->siz <= 1) {                                 /* 4 or 8 bits a texel: a table of its values */
        static u16 tab[256], tab4[256];
        static u8 used[256];
        u32 n = tl->siz == 0 ? 16 : 256;
        Tile t = *tl;
        memset(used, 0, n);
        for (y = 0; y < h; y++) {
            const u8 *row = m + ((base + y * stride) & 0x1FFF);
            u32 sw = (y & 1) << 2;                      /* (odd lines: 32-bit halves swapped) */
            if (tl->siz == 0) for (x = 0; x < w; x++) { u32 b = row[(x >> 1) ^ sw]; used[(x & 1) ? (b & 15) : (b >> 4)] = 1; }
            else for (x = 0; x < w; x++) used[row[x ^ sw]] = 1;
        }
        /* each value through Texel, as a one-texel image of it */
        for (i = 0; i < n; i++) {
            u32 at = base & 0x1FFF;
            u8 keep = G.tmem[at];
            G.tmem[at] = (u8)(tl->siz == 0 ? i << 4 : i);
            Texel(&t, 0, 0, o);
            G.tmem[at] = keep;
            if (used[i] && o[3] != 0 && o[3] != 255) soft = 1;
            tab[i] = To1555(o);
            tab4[i] = To4444(o);
        }
        if (soft) memcpy(tab, tab4, n * 2);
        for (y = 0; y < ph; y++) {
            u16 *out = sTexBuf + y * pw;
            u32 sy = y < h ? y : h - 1, sw = (sy & 1) << 2;
            const u8 *row = m + ((base + sy * stride) & 0x1FFF);
            if (tl->siz == 0) {
                for (x = 0; x < w; x++) { u32 b = row[(x >> 1) ^ sw]; out[x] = tab[(x & 1) ? (b & 15) : (b >> 4)]; }
            } else {
                for (x = 0; x < w; x++) out[x] = tab[row[x ^ sw]];
            }
            for (; x < pw; x++) out[x] = out[w - 1];
        }
        return soft ? TEXFMT_4444 : TEXFMT_1555;
    }
    if (tl->siz == 2 && tl->fmt != 3) {                 /* RGBA16: one bit of alpha */
        for (y = 0; y < ph; y++) {
            u16 *out = sTexBuf + y * pw;
            u32 sy = y < h ? y : h - 1, sw = (sy & 1) << 1;
            const u16 *row = (const u16 *)(m + ((base + sy * stride) & 0x1FFE));
            for (x = 0; x < w; x++) { u16 c = row[x ^ sw]; out[x] = (u16)(((c & 1) << 15) | (c >> 1)); }
            for (; x < pw; x++) out[x] = out[w - 1];
        }
        return TEXFMT_1555;
    }
    /* IA16 and 32-bit: texel by texel */
    for (y = 0; y < h && !soft; y++)
        for (x = 0; x < w; x++) {
            Texel(tl, x, y, o);
            if (o[3] != 0 && o[3] != 255) { soft = 1; break; }
        }
    for (y = 0; y < ph; y++) {
        u16 *out = sTexBuf + y * pw;
        u32 sy = y < h ? y : h - 1;
        for (x = 0; x < pw; x++) {
            Texel(tl, x < w ? x : w - 1, sy, o);
            out[x] = soft ? To4444(o) : To1555(o);
        }
    }
    return soft ? TEXFMT_4444 : TEXFMT_1555;
}

static void MakeTex2(s32 ti, GfxTex *tex, f32 *tw, f32 *th);
static void Flush(void);
static void Materialize(void);
static s32 LoadedHash(u32 start, u32 end, u32 *h);
static void DumpRecs(void);
static s32 sTexSetup;                     /* TriDraw's texture coordinate set-up is for MakeTex's last texture */
static void MakeTex(s32 ti, GfxTex *tex, f32 *tw, f32 *th) {
    if (!sTexDirty && ti == sLastTile) {
        *tex = sLastTex; *tw = sLastTw; *th = sLastTh;
        return;
    }
    sTexSetup = 0;
    MakeTex2(ti, tex, tw, th);
    sLastTex = *tex; sLastTw = *tw; sLastTh = *th; sLastTile = ti;
    sTexDirty = 0;
}

/* the texture for tile t: its size in texels (*tw, *th, before padding) */
static void MakeTex2(s32 ti, GfxTex *tex, f32 *tw, f32 *th) {
    Tile *tl = &G.tile[ti];
    u32 w, h, pw, ph, x, y, key, bytes, start;
    s32 alphaSoft = 0;
    u8 o[4];

    w = tl->masks ? (1u << tl->masks) : ((tl->lrs - tl->uls) >> 2) + 1;
    h = tl->maskt ? (1u << tl->maskt) : ((tl->lrt - tl->ult) >> 2) + 1;
    if (w > 1024) w = 1024;
    if (h > 512) h = 512;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    pw = Pow2(w);
    ph = Pow2(h);
    *tw = (f32)(s32)w;
    *th = (f32)(s32)h;

    /* the key: the texel bytes used, the palette, the tile's set-up */
    start = tl->tmem * 8;
    bytes = (tl->siz == 3 ? tl->line * 16 : tl->line * 8) * h;
    if (start + bytes > 8192) bytes = 8192 - start;
    {
        u32 hk, hp = 0, ps = 0x800 + (tl->siz == 0 ? tl->palette * 128 : 0), pn = tl->siz == 0 ? 128 : 2048;
        s32 ok = LoadedHash(start, start + bytes, &hk);
        if (ok && tl->siz <= 1) ok = LoadedHash(ps, 0, &hp);   /* (a palette may be shorter) */
        if (ok) {
            key = (hk * 0x01000193u) ^ (hp * 0x2545F491u) ^ 0x811C9DC5u ^ (w << 20) ^ (h << 8) ^ (tl->fmt << 4) ^ tl->siz;
            if (tl->siz <= 1) key ^= (G.omH >> 14) & 3;
        } else {                                        /* (TMEM itself) */
            Materialize();
            key = Hash(G.tmem + start, bytes, 0x811C9DC5u ^ (w << 20) ^ (h << 8) ^ (tl->fmt << 4) ^ tl->siz);
            if (tl->siz <= 1) key = Hash(G.tmem + ps, pn, key ^ ((G.omH >> 14) & 3));
        }
    }
    key ^= (tl->cms << 1) ^ (tl->cmt << 3);

    tex->key = key;
    tex->w = (s32)pw;
    tex->h = (s32)ph;
    tex->wrapS = (tl->cms & 2) || !tl->masks ? WRAP_CLAMP : (tl->cms & 1) ? WRAP_MIRROR : WRAP_REPEAT;
    tex->wrapT = (tl->cmt & 2) || !tl->maskt ? WRAP_CLAMP : (tl->cmt & 1) ? WRAP_MIRROR : WRAP_REPEAT;
    if (tex->wrapS != WRAP_CLAMP && pw != w) tex->wrapS = WRAP_CLAMP;
    if (tex->wrapT != WRAP_CLAMP && ph != h) tex->wrapT = WRAP_CLAMP;
    tex->filter = ((G.omH >> 12) & 3) != 0;
    tex->texels = sTexBuf;
    if (plat_tex_known(key)) { tex->fmt = TEXFMT_4444; return; }
    Materialize();
    /* the triangles waiting may still need the texels in sTexBuf: draw them first */
    if (sOutN && sBatchHasTex) Flush();
    STAT(5);

    tex->fmt = Decode(tl, w, h, pw, ph);
}

/* ---- texture memory, lazily. Loads are recorded and only copied into TMEM when a texture has
   to be decoded (almost all are already in the platform's cache); what a texture is comes from
   the loads that filled its part of TMEM, and each load's source bytes are hashed at most once a
   frame (memory doesn't change while a frame's display list is drawn). */
typedef struct { const u8 *src; u32 dst, rows, n, srcStride, dstStride, tlut, dxt; } PendingLoad;
#define MAX_PEND 2048                 /* (a frame can have hundreds of loads; when full, all are made) */
static PendingLoad sPend[MAX_PEND];
static s32 sPendN;
typedef struct { u32 dst, end; const PendingLoad *p; u32 h; s32 hashed; } LoadRec;   /* TMEM [dst, end) */
static LoadRec sRec[16];
static s32 sRecN;
typedef struct { const u8 *src; u32 rows, n, stride, frame, h, quick; } HashMemo;
static HashMemo sMemo[1024];
static u32 sFrameId = 1;

static void Materialize(void) {
    if (sPendN) STAT(7);
    s32 i;
    u32 r, k;
    for (i = 0; i < sPendN; i++) {
        const PendingLoad *p = &sPend[i];
        if (p->tlut) {
            for (k = 0; k < p->n; k++) *(u16 *)(G.tmem + p->dst + k * 8) = ((const u16 *)p->src)[k];
        } else if (p->rows == 1 && p->dstStride == 0) {
            /* LoadBlock: the RDP counts lines by dxt and stores the odd ones with their 32-bit
               halves swapped (sampling swaps them back) */
            u32 cnt = 0, n8 = (p->n + 7) & ~7u;             /* (whole 64-bit words, as the RDP moves them) */
            u8 *d = G.tmem + p->dst;
            const u8 *sp = p->src;
            if (p->dst + n8 > sizeof(G.tmem)) n8 = (sizeof(G.tmem) - p->dst) & ~7u;
            for (k = 0; k < n8; k += 8, cnt += p->dxt) {
                if ((cnt >> 11) & 1) { memcpy(d + k, sp + k + 4, 4); memcpy(d + k + 4, sp + k, 4); }
                else memcpy(d + k, sp + k, 8);
            }
        } else {
            for (r = 0; r < p->rows; r++) {             /* LoadTile: odd lines are stored swapped too */
                u8 *d = G.tmem + p->dst + r * p->dstStride;
                const u8 *sp = p->src + r * p->srcStride;
                if ((r & 1) && p->dxt != 0xFFFFFFFFu) {
                    u32 n8 = (p->n + 7) & ~7u;                 /* (whole 64-bit words) */
                    if (p->dst + r * p->dstStride + n8 > sizeof(G.tmem)) n8 = p->n & ~7u;
                    for (k = 0; k < n8; k += 8) { memcpy(d + k, sp + k + 4, 4); memcpy(d + k + 4, sp + k, 4); }
                } else memcpy(d, sp, p->n);
            }
        }
    }
    sPendN = 0;
}

static u32 SourceHash(const u8 *src, u32 rows, u32 n, u32 stride) {
    HashMemo *m = &sMemo[(((u32)src >> 3) ^ rows * 31 ^ n * 7) & 1023];
    u32 r, h, quick = 0;
    if (m->frame == sFrameId && m->src == src && m->rows == rows && m->n == n && m->stride == stride) return m->h;
    h = 0x811C9DC5u ^ n;
    if (rows * n <= 1024) {
        for (r = 0; r < rows; r++) h = Hash(src + r * stride, n, h);
    } else if (rows > 8) {                              /* big: every few rows, and the last */
        u32 rstep = rows / 8;
        for (r = 0; r < rows; r += rstep) h = Hash(src + r * stride, n, h);
        h = Hash(src + (rows - 1) * stride, n, h);
    } else {                                            /* (one long block: 64 words of it) */
        u32 k, step = (n / 64 + 3) & ~3u;
        for (r = 0; r < rows; r++)
            for (k = 0; k + 4 <= n; k += step) h = (h ^ Ld32(src + r * stride + k)) * 0x01000193u;
    }
    m->src = src; m->rows = rows; m->n = n; m->stride = stride; m->frame = sFrameId; m->h = h; m->quick = quick;
    return h;
}

static void AddLoad(const u8 *src, u32 dst, u32 rows, u32 n, u32 srcStride, u32 dstStride, s32 tlut, u32 dxt) {
    u32 end = tlut ? dst + n * 8 : dst + (rows - 1) * dstStride + n, h;
    s32 i, j;
    if (rows == 0 || n == 0) return;
    if (end > sizeof(G.tmem)) return;
    sTexDirty = 1;
    if (sPendN == MAX_PEND) { Materialize(); sRecN = 0; }
    sPend[sPendN].src = src; sPend[sPendN].dst = dst; sPend[sPendN].rows = rows; sPend[sPendN].n = n;
    sPend[sPendN].srcStride = srcStride; sPend[sPendN].dstStride = dstStride; sPend[sPendN].tlut = tlut;
    sPend[sPendN].dxt = dxt;
    sPendN++;
    for (i = j = 0; i < sRecN; i++)                     /* the loads this one covers (partly) are gone */
        if (sRec[i].end <= dst || sRec[i].dst >= end) sRec[j++] = sRec[i];
    sRecN = j;
    if (sRecN == 16) { memmove(sRec, sRec + 1, 15 * sizeof(LoadRec)); sRecN = 15; }
    sRec[sRecN].dst = dst; sRec[sRecN].end = end; sRec[sRecN].p = &sPend[sPendN - 1]; sRec[sRecN].hashed = 0;
    sRecN++;
    (void)h;
}

/* the hash of TMEM [start, end) if one load filled all of it */
static void DumpRecs(void) {
}
static s32 LoadedHash(u32 start, u32 end, u32 *h) {
    s32 i;
    for (i = sRecN - 1; i >= 0; i--)
        if (sRec[i].dst <= start && start < sRec[i].end) {
            if (end && end > sRec[i].end) {                 /* (the tile reaches past the load: fine if no
                                                               other load put anything there) */
                s32 k;
                for (k = 0; k < sRecN; k++)
                    if (k != i && sRec[k].dst < end && sRec[k].end > sRec[i].end) return 0;
            }
            if (!sRec[i].hashed) {
                const PendingLoad *p = sRec[i].p;
                sRec[i].h = p->tlut ? SourceHash(p->src, 1, p->n * 2, 0) ^ 0x5bd1e995u
                                    : SourceHash(p->src, p->rows, p->n, p->srcStride) ^ p->dstStride * 0x01000193u ^
                                      p->dxt * 0x9E3779B1u;
                sRec[i].hashed = 1;
            }
            *h = sRec[i].h ^ ((start - sRec[i].dst) * 0x9E3779B1u);
            return 1;
        }
    return 0;
}

static void CmdLoad(u32 w0, u32 w1, s32 block) {
    Tile *tl = &G.tile[(w1 >> 24) & 7];
    u32 bpp = G.timgSiz == 3 ? 4 : G.timgSiz == 2 ? 2 : 1;   /* (4-bit images load as 8 or 16) */
    const u8 *img = Addr(G.timgAddr);
    if (block) {
        u32 uls = (w0 >> 12) & 0xFFF, ult = w0 & 0xFFF, lrs = (w1 >> 12) & 0xFFF;
        u32 n = (lrs - uls + 1) * bpp, dst = tl->tmem * 8;
        const u8 *src = img + (ult * G.timgWidth + uls) * bpp;
        if (G.timgSiz == 0) n = (lrs - uls + 1) / 2;
        if (dst + n > sizeof(G.tmem)) n = sizeof(G.tmem) - dst;
        AddLoad(src, dst, 1, n, 0, 0, 0, G.timgSiz == 3 ? 0 : w1 & 0xFFF);   /* (32-bit: kept unswapped) */
    } else {
        u32 uls = ((w0 >> 12) & 0xFFF) >> 2, ult = (w0 & 0xFFF) >> 2;
        u32 lrs = ((w1 >> 12) & 0xFFF) >> 2, lrt = (w1 & 0xFFF) >> 2, rows;
        u32 stride = tl->line * 8 * (G.timgSiz == 3 ? 2 : 1), n = (lrs - uls + 1) * bpp, dst = tl->tmem * 8;
        const u8 *src = img + (ult * G.timgWidth + uls) * bpp;
        u32 sstride = G.timgWidth * bpp;
        if (G.timgSiz == 0) { n = (lrs - uls + 2) / 2; src = img + (ult * G.timgWidth + uls) / 2; sstride = G.timgWidth / 2; }
        rows = lrt >= ult ? lrt - ult + 1 : 0;
        while (rows && dst + (rows - 1) * stride + n > sizeof(G.tmem)) rows--;
        AddLoad(src, dst, rows, n, sstride, stride, 0, G.timgSiz == 3 ? 0xFFFFFFFFu : 0);
        tl->uls = (w0 >> 12) & 0xFFF; tl->ult = w0 & 0xFFF;
        tl->lrs = (w1 >> 12) & 0xFFF; tl->lrt = w1 & 0xFFF;
    }
}

static void CmdLoadTlut(u32 w0, u32 w1) {
    Tile *tl = &G.tile[(w1 >> 24) & 7];
    u32 n = (((w1 >> 12) & 0xFFF) >> 2) + 1, dst = tl->tmem * 8;
    const u16 *src = (const u16 *)Addr(G.timgAddr);
    src += ((w0 >> 12) & 0xFFF) >> 2;
    while (n && dst + (n - 1) * 8 + 2 > sizeof(G.tmem)) n--;
    AddLoad((const u8 *)src, dst, 1, n, 0, 0, 1, 0);
}

/* ------------------------------------------------------------------ the combiner */

static f32 Clamp(f32 v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* the combiner, decoded when its state changes: for each cycle the four inputs of the colour
   (a - b) * c + d and of the alpha, as slots in a table of the values they can take */
enum { S_COMB, S_TEX, S_PRIM, S_SHADE, S_ENV, S_ONE, S_ZERO, S_COMBA, S_PRIMA, S_SHADEA, S_ENVA,
       S_LOD, S_N };
static struct { u8 rgb[2][4], al[2][4]; s32 cycles; u32 gen; f32 k[3][S_N]; s32 fast; u32 fog8[3]; } C;
/* C.fast: the combiner is texel x shade (one cycle), the alpha factor being 1 shade's alpha 2 1 */
#define FAST_NONE 0
#define FAST_MOD_SA 1
#define FAST_MOD_1 2

static u8 SelRGB(u32 i, s32 which) {           /* which: 0 a, 1 b, 2 c, 3 d */
    static const u8 base[6] = {S_COMB, S_TEX, S_TEX, S_PRIM, S_SHADE, S_ENV};
    if (which == 2) {
        if (i < 6) return base[i];
        switch (i) {
        case 7: return S_COMBA;
        case 8: case 9: return S_TEX;
        case 10: return S_PRIMA;
        case 11: return S_SHADEA;
        case 12: return S_ENVA;
        case 14: return S_LOD;
        default: return S_ZERO;
        }
    }
    if (i < 6) return base[i];
    if (i == 6) return which == 1 ? S_ZERO : S_ONE;
    return S_ZERO;
}
static u8 SelA(u32 i, s32 c) {
    static const u8 base[8] = {S_COMBA, S_TEX, S_TEX, S_PRIMA, S_SHADEA, S_ENVA, S_ONE, S_ZERO};
    if (c && i == 0) return S_ZERO;
    if (c && i == 6) return S_LOD;
    return base[i & 7];
}

static void DecodeCombiner(void) {
    u32 c0 = G.cmb0, c1 = G.cmb1, k;
    C.cycles = ((G.omH >> 20) & 3) == 1 ? 2 : 1;
    C.rgb[0][0] = SelRGB((c0 >> 20) & 15, 0); C.rgb[0][1] = SelRGB((c1 >> 28) & 15, 1);
    C.rgb[0][2] = SelRGB((c0 >> 15) & 31, 2); C.rgb[0][3] = SelRGB((c1 >> 15) & 7, 3);
    C.al[0][0] = SelA((c0 >> 12) & 7, 0); C.al[0][1] = SelA((c1 >> 12) & 7, 0);
    C.al[0][2] = SelA((c0 >> 9) & 7, 1); C.al[0][3] = SelA((c1 >> 9) & 7, 0);
    C.rgb[1][0] = SelRGB((c0 >> 5) & 15, 0); C.rgb[1][1] = SelRGB((c1 >> 24) & 15, 1);
    C.rgb[1][2] = SelRGB(c0 & 31, 2); C.rgb[1][3] = SelRGB((c1 >> 6) & 7, 3);
    C.al[1][0] = SelA((c1 >> 21) & 7, 0); C.al[1][1] = SelA((c1 >> 3) & 7, 0);
    C.al[1][2] = SelA((c1 >> 18) & 7, 1); C.al[1][3] = SelA(c1 & 7, 0);
    for (k = 0; k < 3; k++) {
        C.k[k][S_PRIM] = G.prim[k]; C.k[k][S_ENV] = G.env[k];
        C.k[k][S_ONE] = 1.0f; C.k[k][S_ZERO] = 0.0f;
        C.k[k][S_PRIMA] = G.prim[3]; C.k[k][S_ENVA] = G.env[3]; C.k[k][S_LOD] = G.primLod;
    }
    C.fog8[0] = (u32)(G.fog[0] * 255.0f + 0.5f);
    C.fog8[1] = (u32)(G.fog[1] * 255.0f + 0.5f);
    C.fog8[2] = (u32)(G.fog[2] * 255.0f + 0.5f);
    C.fast = FAST_NONE;
    if (C.cycles == 1) {
        const u8 *q = C.rgb[0], *a = C.al[0];
        s32 mod = (q[0] == S_TEX && q[1] == S_ZERO && q[2] == S_SHADE && q[3] == S_ZERO) ||
                  (q[0] == S_SHADE && q[1] == S_ZERO && q[2] == S_TEX && q[3] == S_ZERO);
        if (mod && a[1] == S_ZERO && a[3] == S_ZERO) {
            if ((a[0] == S_TEX && a[2] == S_SHADEA) || (a[0] == S_SHADEA && a[2] == S_TEX)) C.fast = FAST_MOD_SA;
            else if (a[0] == S_TEX && a[2] == S_TEX) C.fast = FAST_MOD_1;      /* (texel squared, as texel) */
        } else if (mod && a[0] == S_ZERO && a[1] == S_ZERO && a[2] == S_ZERO) {
            if (a[3] == S_TEX || a[3] == S_ONE) C.fast = FAST_MOD_1;
            else if (a[3] == S_SHADEA) C.fast = FAST_MOD_SA;
        }
    }
    C.gen = sCmbGen;
}

/* colour (rgba) at a vertex with texel value t (0 or 1) */
static void Combine(const f32 *shade, f32 t, f32 *out) {
    f32 comb[4] = {0, 0, 0, 0}, r[4];
    s32 cyc, k;
    STAT(6);
    if (C.gen != sCmbGen) DecodeCombiner();
    for (cyc = 0; cyc < C.cycles; cyc++) {
        const u8 *q = C.rgb[cyc], *qa = C.al[cyc];
        for (k = 0; k < 3; k++) {
            f32 *T = C.k[k];
            T[S_COMB] = comb[k]; T[S_TEX] = t; T[S_SHADE] = shade[k];
            T[S_COMBA] = comb[3]; T[S_SHADEA] = shade[3];
            r[k] = (T[q[0]] - T[q[1]]) * T[q[2]] + T[q[3]];
        }
        {
            f32 *T = C.k[0];                    /* (its alpha slots are all set above) */
            r[3] = (T[qa[0]] - T[qa[1]]) * T[qa[2]] + T[qa[3]];
        }
        for (k = 0; k < 4; k++) comb[k] = Clamp(r[k]);
    }
    memcpy(out, comb, sizeof(comb));
}

/* does the combiner use the texture? */
static s32 UsesTexel(void) {
    u32 c0 = G.cmb0, c1 = G.cmb1;
    s32 two = ((G.omH >> 20) & 3) == 1;
#define T(x) ((x) == 1 || (x) == 2)
    if (T((c0 >> 20) & 15) || T((c1 >> 28) & 15) || T((c0 >> 15) & 31) || ((c0 >> 15) & 31) == 8 || ((c0 >> 15) & 31) == 9 ||
        T((c1 >> 15) & 7) || T((c0 >> 12) & 7) || T((c1 >> 12) & 7) || T((c0 >> 9) & 7) || T((c1 >> 9) & 7)) return 1;
    if (two && (T((c0 >> 5) & 15) || T((c1 >> 24) & 15) || T(c0 & 31) || (c0 & 31) == 8 || (c0 & 31) == 9 ||
                T((c1 >> 6) & 7) || T((c1 >> 21) & 7) || T((c1 >> 3) & 7) || T((c1 >> 18) & 7) || T(c1 & 7))) return 1;
#undef T
    return 0;
}

static u32 Pack(const f32 *c) {
    return ((u32)(Clamp(c[3]) * 255.0f + 0.5f) << 24) | ((u32)(Clamp(c[0]) * 255.0f + 0.5f) << 16) |
           ((u32)(Clamp(c[1]) * 255.0f + 0.5f) << 8) | (u32)(Clamp(c[2]) * 255.0f + 0.5f);
}

/* ------------------------------------------------------------------ drawing */

static void Flush(void) {
    if (sOutN) {
        STAT(4);
        plat_gfx_draw(sOut, sOutN, sBatchHasTex ? &sBatchTex : NULL, sBatchBlend, sBatchZcmp, sBatchZupd, sBatchOffset);
        sOutN = 0;
    }
}

static s32 BlendMode(void) {
    u32 l = G.omL;
    if ((l & 3) == 1 && !(l & 0x4000)) return BLEND_CUTOUT;          /* alpha compare, no blending */
    if (l & 0x4000) {                                                /* FORCE_BL */
        u32 m1a = (l >> 30) & 3, m1b = (l >> 26) & 3, m2a = (l >> 22) & 3, m2b = (l >> 18) & 3;
        u32 two = ((G.omH >> 20) & 3) == 1;
        u32 pa = two ? m2a : m1a, pb = two ? m2b : m1b;
        if (two && m2a == 0 && m2b == 0) { pa = m1a; pb = m1b; }
        if (pb == 0 && ((two ? (l >> 16) & 3 : (l >> 20) & 3) == 1)) return BLEND_ALPHA;   /* p*a + mem*(1-a) */
        if (pb == 0 && pa == 0) return BLEND_ALPHA;
        return (l & 3) == 1 ? BLEND_CUTOUT : BLEND_ALPHA;
    }
    if ((l & 3) == 1) return BLEND_CUTOUT;
    if ((l & 0x3000) == 0x3000) return BLEND_CUTOUT;                /* CVG_X_ALPHA | ALPHA_CVG_SEL (TEX_EDGE):
                                                                       alpha is the coverage */
    return BLEND_OPAQUE;
}

/* set the batch's state; flush when it changes */
static void State(s32 textured, const GfxTex *tex, s32 blend, s32 zc, s32 zu, s32 off) {
    if (sOutN && (textured != sBatchHasTex || blend != sBatchBlend || zc != sBatchZcmp || zu != sBatchZupd ||
                  (textured && (tex->key != sBatchTex.key || tex->wrapS != sBatchTex.wrapS ||
                                                        tex->wrapT != sBatchTex.wrapT || tex->filter != sBatchTex.filter))))
        Flush();
    sBatchHasTex = textured;
    if (textured && (sBatchTex.key != tex->key || sBatchTex.texels != tex->texels || sBatchTex.w != tex->w ||
                     sBatchTex.h != tex->h || sBatchTex.fmt != tex->fmt || sBatchTex.wrapS != tex->wrapS ||
                     sBatchTex.wrapT != tex->wrapT || sBatchTex.filter != tex->filter)) sBatchTex = *tex;
    sBatchBlend = blend;
    sBatchZcmp = zc;
    sBatchZupd = zu;
    if (sOutN == 0) sBatchOffset = off;   /* (a batch that has the offset colour on keeps it) */
}

/* does the blender's first cycle mix the fog colour in (G_BL_CLR_FOG, by shade alpha)? */
static s32 FogBlend(void) {
    return (G.geom & 0x10000) && ((G.omL >> 30) & 3) == 3;
}

/* a tile's shift: 1..10 divide the texture coordinates by 2^n, 11..15 multiply them by 2^(16 - n) */
static f32 TexShift(u32 sh) {
    if (sh == 0) return 1.0f;
    if (sh <= 10) return 1.0f / (f32)(1 << sh);
    return (f32)(1 << (16 - sh));
}

static void Emit(Vert *v, f32 tw, f32 th, f32 ts0, f32 tt0, s32 textured, const GfxTex *tex, s32 *offUsed) {
    GfxVert *o = &sOut[sOutN++];
    f32 shade[4], c0[4], c1[4];
    s32 cached = v->gen == sCmbGen && v->cTex == textured;
    o->x = v->sx;
    o->y = v->sy;
    o->z = v->sz;
    o->q = v->sq;
    if (textured) {
        o->u = v->s * tw - ts0;                         /* (see TriDraw) */
        o->v = v->t * th - tt0;
        if (cached) {
            o->argb = v->cArgb;
            o->oargb = v->cOargb;
            if (o->oargb) *offUsed = 1;
            return;
        }
        if (C.gen != sCmbGen) DecodeCombiner();
        if (C.fast != FAST_NONE) {
            u32 c = C.fast == FAST_MOD_1 ? (v->shade8 | 0xFF000000u) : v->shade8, off = 0;
            if (v->fog > 0.0f && FogBlend()) {           /* colour * (1 - fog), offset fog colour * fog */
                u32 fi = (u32)(v->fog * 256.0f), ki = 256 - fi;
                c = (c & 0xFF000000u) | ((((c >> 16) & 255) * ki >> 8) << 16) | ((((c >> 8) & 255) * ki >> 8) << 8) |
                    ((c & 255) * ki >> 8);
                off = ((C.fog8[0] * fi >> 8) << 16) | ((C.fog8[1] * fi >> 8) << 8) | (C.fog8[2] * fi >> 8);
                if (off) *offUsed = 1;
            }
            o->argb = c;
            o->oargb = off;
            v->gen = sCmbGen; v->cTex = textured; v->cArgb = c; v->cOargb = off;
            return;
        }
        if (!v->colF) ColF(v);
        shade[0] = v->r; shade[1] = v->g; shade[2] = v->b; shade[3] = v->a;
        Combine(shade, 0.0f, c0);
        Combine(shade, 1.0f, c1);
        {
            f32 m[4] = {c1[0] - c0[0], c1[1] - c0[1], c1[2] - c0[2], c1[3] - c0[3]};
            if (c0[3] > 0.0f) m[3] = c1[3];
            if (v->fog > 0.0f && FogBlend()) {           /* the blender mixes in the fog colour */
                f32 k = 1.0f - v->fog;
                s32 j;
                for (j = 0; j < 3; j++) { m[j] *= k; c0[j] = c0[j] * k + G.fog[j] * v->fog; }
            }
            o->argb = Pack(m);
            o->oargb = Pack(c0) & 0x00FFFFFF;
            if (o->oargb) *offUsed = 1;
        }
    } else {
        o->u = o->v = 0.0f;
        o->oargb = 0;
        if (cached) { o->argb = v->cArgb; return; }
        if (!v->colF) ColF(v);
        shade[0] = v->r; shade[1] = v->g; shade[2] = v->b; shade[3] = v->a;
        Combine(shade, 0.0f, c0);
        if (v->fog > 0.0f && FogBlend()) {
            f32 k = 1.0f - v->fog;
            s32 j;
            for (j = 0; j < 3; j++) c0[j] = c0[j] * k + G.fog[j] * v->fog;
        }
        o->argb = Pack(c0);
    }
    v->gen = sCmbGen; v->cTex = textured; v->cArgb = o->argb; v->cOargb = o->oargb;
    (void)tw; (void)th;
}

/* clip a triangle at w = NEAR_W (see ClipCodes), then draw it */
static void TriDraw(Vert *v0, Vert *v1, Vert *v2) {
    Vert *in[3] = {v0, v1, v2};
    Vert clip[2], *poly[4];
    s32 nc = 0;
    s32 n = 0, k, textured, blend, zc, zu, off = 0;
    GfxTex tex;
    f32 tw = 1, th = 1, ts0 = 0, tt0 = 0;

    /* culling: which way round the triangle is on the screen, from the clip-space positions
       (the sign of det |x y w| is the other way to the screen's, whose y points down), before
       any of it is projected */
    if ((G.geom & 0x3000) && !((in[0]->clip | in[1]->clip | in[2]->clip) & 16) &&
        in[0]->w > 0 && in[1]->w > 0 && in[2]->w > 0) {
        f32 det = in[0]->x * (in[1]->y * in[2]->w - in[2]->y * in[1]->w) -
                  in[1]->x * (in[0]->y * in[2]->w - in[2]->y * in[0]->w) +
                  in[2]->x * (in[0]->y * in[1]->w - in[1]->y * in[0]->w);
        f32 cross = G.vpScale[0] * G.vpScale[1] > 0.0f ? -det : det;
        if ((G.geom & 0x2000) && cross > 0) { STAT(1); return; }   /* G_CULL_BACK */
        if ((G.geom & 0x1000) && cross < 0) { STAT(1); return; }   /* G_CULL_FRONT */
    }
    if (!in[0]->ready) Finish(in[0]);
    if (!in[1]->ready) Finish(in[1]);
    if (!in[2]->ready) Finish(in[2]);
    if (!((in[0]->clip | in[1]->clip | in[2]->clip) & 16) && in[0]->w > 0 && in[1]->w > 0 && in[2]->w > 0) {   /* (all in front of the near plane) */
        poly[0] = in[0]; poly[1] = in[1]; poly[2] = in[2];
        n = 3;
    } else for (k = 0; k < 3; k++) {
        Vert *a = in[k], *b = in[(k + 1) % 3];
        f32 da = NearDist(a), db = NearDist(b);
        if (da >= 0) poly[n++] = a;
        if ((da >= 0) != (db >= 0) && n < 4 && nc < 2) {
            f32 t = da / (da - db);
            if (!a->colF) ColF(a);
            if (!b->colF) ColF(b);
            Vert *o = &clip[nc++];
            poly[n++] = o;
            o->gen = 0;
            o->x = a->x + (b->x - a->x) * t; o->y = a->y + (b->y - a->y) * t;
            o->z = a->z + (b->z - a->z) * t; o->w = a->w + (b->w - a->w) * t;
            o->s = a->s + (b->s - a->s) * t; o->t = a->t + (b->t - a->t) * t;
            o->r = a->r + (b->r - a->r) * t; o->g = a->g + (b->g - a->g) * t;
            o->b = a->b + (b->b - a->b) * t; o->a = a->a + (b->a - a->a) * t;
            o->fog = a->fog + (b->fog - a->fog) * t;
            o->colF = 1; o->lit = 1;
            o->shade8 = ((u32)(o->a * 255.0f + 0.5f) << 24) | ((u32)(o->r * 255.0f + 0.5f) << 16) |
                        ((u32)(o->g * 255.0f + 0.5f) << 8) | (u32)(o->b * 255.0f + 0.5f);
            ClipCodes(o);
            Project(o);
            o->ready = 1;
        }
    }
    if (n < 3) { STAT(2); return; }   /* (behind the near plane) */
    if ((G.geom & 0x3000) && nc != 0) {
        /* a triangle through the near plane: culled as its clipped part, as the N64 (which clips
           first; a back-facing wall beside the camera in the intro's lab covered the floor).
           Which way round from the whole clipped polygon (its corners in front of the near
           plane, w > 0): from its first three alone, two of them the cut corners close
           together, a floor right in front of the camera could come out the wrong way round
           and go (Happy Harry's Hut, the camera low over the boards before a fight) */
        f32 det = 0.0f, cross;
        for (k = 1; k + 1 < n; k++) {
            Vert *p0 = poly[0], *p1 = poly[k], *p2 = poly[k + 1];
            det += p0->x * (p1->y * p2->w - p2->y * p1->w) -
                   p1->x * (p0->y * p2->w - p2->y * p0->w) +
                   p2->x * (p0->y * p1->w - p1->y * p0->w);
        }
        cross = G.vpScale[0] * G.vpScale[1] > 0.0f ? -det : det;
        if ((G.geom & 0x2000) && cross > 0) { STAT(1); return; }
        if ((G.geom & 0x1000) && cross < 0) { STAT(1); return; }
    }
    STAT(3);

    if (sStateGen != sCmbGen) {
        sStateGen = sCmbGen;
        sStateTexel = UsesTexel();
        sStateBlend = BlendMode();
    }
    textured = G.texOn && sStateTexel;
    if (textured) {
        static f32 cTw, cTh, cTs0, cTt0;
        Tile *tl = &G.tile[G.texTile];
        MakeTex(G.texTile, &tex, &tw, &th);
        if (!sTexSetup) {
            /* u = (s >> shift - uls) / width: Emit makes it as s * tw - ts0 */
            f32 iw = 1.0f / (f32)tex.w, ih = 1.0f / (f32)tex.h;
            cTw = TexShift(tl->shifts) * iw;
            cTh = TexShift(tl->shiftt) * ih;
            cTs0 = (s32)tl->uls * 0.25f * iw;
            cTt0 = (s32)tl->ult * 0.25f * ih;
            sTexSetup = 1;
        }
        tw = cTw; th = cTh; ts0 = cTs0; tt0 = cTt0;
    }
    blend = sStateBlend;
    zc = (G.geom & 1) && (G.omL & 0x10);
    zu = (G.geom & 1) && (G.omL & 0x20);
#ifdef __sh__
    if (!zc && G.scenery) zc = GFX_ZCMP_SCENERY;    /* (see plat_gfx_draw) */
#endif
    if (sOutN + 6 > MAX_OUT) Flush();
    State(textured, &tex, blend, zc, zu, 0);
    {
        s32 start = sOutN;
        Emit(poly[0], tw, th, ts0, tt0, textured, &tex, &off);
        Emit(poly[1], tw, th, ts0, tt0, textured, &tex, &off);
        Emit(poly[2], tw, th, ts0, tt0, textured, &tex, &off);
        if (n == 4) {
            sOut[sOutN] = sOut[start];
            sOutN++;
            sOut[sOutN] = sOut[start + 2];
            sOutN++;
            Emit(poly[3], tw, th, ts0, tt0, textured, &tex, &off);
        }
        if (((G.omL >> 10) & 3) == 3 && zc) {         /* ZMODE_DEC: a decal on a surface drawn before;
                                                         a little nearer, so it passes where they meet */
            s32 k;
            for (k = start; k < sOutN; k++) sOut[k].q *= 1.002f;
        }
        if (off && !sBatchOffset) {                     /* this one needs the offset colour */
            GfxVert keep[6];
            s32 m = sOutN - start;
            memcpy(keep, sOut + start, m * sizeof(GfxVert));
            sOutN = start;
            Flush();
            sBatchOffset = 1;
            memcpy(sOut, keep, m * sizeof(GfxVert));
            sOutN = m;
        }
    }
}

/* a triangle from the vertex buffer: most are entirely outside one side of the view */
static inline void Tri(u32 i0, u32 i1, u32 i2) {
    Vert *a, *b, *c;
    STAT(0);
    if (G.vclip[i0 & 63] & G.vclip[i1 & 63] & G.vclip[i2 & 63]) return;   /* (all outside one side) */
    a = &G.v[i0 & 63]; b = &G.v[i1 & 63]; c = &G.v[i2 & 63];
    if (a->clip & b->clip & c->clip) { STAT(2); return; }
    TriDraw(a, b, c);
}

/* a texture rectangle (screen 10.2 coordinates; s, t S10.5; dsdx, dtdy S5.10) */
static void TexRect(u32 w0, u32 w1, u32 h1, u32 h2, s32 flip) {
    f32 x0 = (s32)((w1 >> 12) & 0xFFF) / 4.0f, y0 = (s32)(w1 & 0xFFF) / 4.0f;
    f32 x1 = (s32)((w0 >> 12) & 0xFFF) / 4.0f, y1 = (s32)(w0 & 0xFFF) / 4.0f;
    f32 s = (s16)(h1 >> 16) / 32.0f, t = (s16)h1 / 32.0f;
    f32 dsdx = (s16)(h2 >> 16) / 1024.0f, dtdy = (s16)h2 / 1024.0f;
    s32 ti = (w1 >> 24) & 7, cyc = (G.omH >> 20) & 3, off = 0;
    Tile *tl = &G.tile[ti];
    GfxTex tex;
    f32 tw, th, s1, t1, ts0, tt0, shade[4] = {1, 1, 1, 1}, c0[4], c1[4];
    u32 argb, oargb = 0;
    s32 blend, textured;
    GfxVert *o;

    if (cyc == 2) { dsdx /= 4.0f; x1 += 1.0f; y1 += 1.0f; }    /* copy mode: the end is included */
    if (x1 <= x0 || y1 <= y0) return;
    MakeTex(ti, &tex, &tw, &th);
    ts0 = (s32)tl->uls / 4.0f;
    tt0 = (s32)tl->ult / 4.0f;
    if (!flip) { s1 = s + dsdx * (x1 - x0); t1 = t + dtdy * (y1 - y0); }
    else { s1 = s + dsdx * (y1 - y0); t1 = t + dtdy * (x1 - x0); }

    textured = 1;
    if (cyc == 2) {
        argb = 0xFFFFFFFF;
        blend = (G.omL & 3) == 1 ? BLEND_CUTOUT : BLEND_OPAQUE;
    } else {
        textured = UsesTexel();
        Combine(shade, 0.0f, c0);
        Combine(shade, 1.0f, c1);
        if (textured) {
            f32 m[4] = {c1[0] - c0[0], c1[1] - c0[1], c1[2] - c0[2], c1[3] - c0[3]};
            if (c0[3] > 0.0f) m[3] = c1[3];
            argb = Pack(m);
            oargb = Pack(c0) & 0x00FFFFFF;
            off = oargb != 0;
        } else {
            argb = Pack(c0);
        }
        blend = BlendMode();
    }
    Flush();
    State(textured, &tex, blend, 0, 0, off);
    o = &sOut[0];
    {
        f32 xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1};
        f32 ss[4], tt[4];
        static const s32 idx[6] = {0, 1, 2, 2, 1, 3};
        s32 k;
        if (!flip) { ss[0] = s; ss[1] = s1; ss[2] = s; ss[3] = s1; tt[0] = t; tt[1] = t; tt[2] = t1; tt[3] = t1; }
        else { ss[0] = s; ss[1] = s; ss[2] = s1; ss[3] = s1; tt[0] = t; tt[1] = t1; tt[2] = t; tt[3] = t1; }
        for (k = 0; k < 6; k++) {
            s32 i = idx[k];
            o[k].x = xs[i]; o[k].y = ys[i]; o[k].z = 0.0f; o[k].q = 1.0f;
            o[k].u = (ss[i] - ts0) / (f32)tex.w;
            o[k].v = (tt[i] - tt0) / (f32)tex.h;
            o[k].argb = argb;
            o[k].oargb = oargb;
        }
    }
    sOutN = 6;
    Flush();
}

static void FillRect(u32 w0, u32 w1) {
    s32 x0 = ((w1 >> 12) & 0xFFF) >> 2, y0 = (w1 & 0xFFF) >> 2;
    s32 x1 = ((w0 >> 12) & 0xFFF) >> 2, y1 = (w0 & 0xFFF) >> 2;
    s32 cyc = (G.omH >> 20) & 3;
    u32 argb;
    Flush();
    if (cyc == 3) {                                      /* fill mode: the end is included */
        x1++; y1++;
        if (G.cimg == G.zimg) { plat_gfx_clear_depth(); return; }
        {
            u32 c = G.fill & 0xFFFF;                    /* RGBA5551 (twice) */
            argb = ((c & 1) ? 0xFF000000u : 0xFF000000u) | ((((c >> 11) & 31) * 255 / 31) << 16) |
                   ((((c >> 6) & 31) * 255 / 31) << 8) | (((c >> 1) & 31) * 255 / 31);
        }
        plat_gfx_fill(x0, y0, x1, y1, argb);
        return;
    }
    {
        f32 shade[4] = {1, 1, 1, 1}, c[4];
        GfxVert *o = sOut;
        static const s32 idx[6] = {0, 1, 2, 2, 1, 3};
        f32 xs[4] = {(f32)x0, (f32)x1, (f32)x0, (f32)x1}, ys[4] = {(f32)y0, (f32)y0, (f32)y1, (f32)y1};
        s32 k;
        Combine(shade, 0.0f, c);
        State(0, NULL, BlendMode(), 0, 0, 0);
        for (k = 0; k < 6; k++) {
            o[k].x = xs[idx[k]]; o[k].y = ys[idx[k]]; o[k].z = 0; o[k].q = 1;
            o[k].u = o[k].v = 0; o[k].argb = Pack(c); o[k].oargb = 0;
        }
        sOutN = 6;
        Flush();
    }
}

static void Color(f32 *d, u32 w1) {
    d[0] = (s32)(w1 >> 24) / 255.0f;
    d[1] = (s32)((w1 >> 16) & 0xFF) / 255.0f;
    d[2] = (s32)((w1 >> 8) & 0xFF) / 255.0f;
    d[3] = (s32)(w1 & 0xFF) / 255.0f;
}

/* ------------------------------------------------------------------ the display list */

void gfx_init(void) {
    memset(&G, 0, sizeof(G));
    G.vpScale[0] = 160; G.vpScale[1] = 120; G.vpScale[2] = 0.5f;
    G.vpTrans[0] = 160; G.vpTrans[1] = 120; G.vpTrans[2] = 0.5f;
    G.texScaleS = G.texScaleT = 1.0f;
}

static void Run(u32 *dl) {
    u32 *stack[18];
    s32 sp = 0;

    for (;;) {
        u32 w0 = dl[0], w1 = dl[1];
        __builtin_prefetch(dl + 16);                    /* (the commands after next: the cache line ahead) */
        dl += 2;
        switch (w0 >> 24) {
        case 0x00:                                              /* G_SPNOOP */
            if (w1 == GFX_MARK_SCENERY_ON) G.scenery = 1;       /* (the port's own marks: the arena drawn) */
            else if (w1 == GFX_MARK_SCENERY_OFF) G.scenery = 0;
            break;
        case 0x01: Flush(); CmdMtx(w0, w1); break;              /* G_MTX */
        case 0x03: Flush(); CmdMoveMem(w0, w1); break;          /* G_MOVEMEM */
        case 0x04: CmdVtx(w0, w1); break;                       /* G_VTX */
        case 0x06:                                              /* G_DL */
            if (((w0 >> 16) & 0xFF) == 0 && sp < 18) stack[sp++] = dl;
            dl = Addr(w1);
            break;
        case 0xB1:                                              /* G_TRI2 */
            Tri(((w0 >> 16) & 0xFF) / 2, ((w0 >> 8) & 0xFF) / 2, (w0 & 0xFF) / 2);
            Tri(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            break;
        case 0xB2: CmdModifyVtx(w0, w1); break;                 /* G_MODIFYVTX (F3DEX) */
        case 0xB3: G.half2 = w1; break;                         /* G_RDPHALF_2 */
        case 0xB4: G.half1 = w1; break;                         /* G_RDPHALF_1 */
        case 0xB5:                                              /* G_QUAD (F3DEX) */
            Tri(((w1 >> 24) & 0xFF) / 2, ((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2);
            Tri(((w1 >> 24) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            break;
        case 0xB6: if (G.geom & w1) { G.geom &= ~w1; CMB_CHANGED(); } break;   /* G_CLEARGEOMETRYMODE */
        case 0xB7: if (~G.geom & w1) { G.geom |= w1; CMB_CHANGED(); } break;   /* G_SETGEOMETRYMODE */
        case 0xB8:                                              /* G_ENDDL */
            if (sp == 0) { Flush(); return; }
            dl = stack[--sp];
            break;
        case 0xB9: {                                            /* G_SETOTHERMODE_L */
            u32 sft = (w0 >> 8) & 0xFF, len = w0 & 0xFF, mask = ((1u << len) - 1) << sft;
            u32 n = (G.omL & ~mask) | (w1 & mask);
            if (n != G.omL) { G.omL = n; CMB_CHANGED(); }
            break;
        }
        case 0xBA: {                                            /* G_SETOTHERMODE_H */
            sTexDirty = 1;
            u32 sft = (w0 >> 8) & 0xFF, len = w0 & 0xFF, mask = ((1u << len) - 1) << sft;
            u32 n = (G.omH & ~mask) | (w1 & mask);
            if (n != G.omH) { G.omH = n; CMB_CHANGED(); }
            break;
        }
        case 0xBB:                                              /* G_TEXTURE */
            G.texOn = w0 & 0xFF;
            G.texTile = (w0 >> 8) & 7;
            G.texScaleS = (s32)(w1 >> 16) / 65536.0f;
            G.texScaleT = (s32)(w1 & 0xFFFF) / 65536.0f;
            if ((w1 >> 16) == 0xFFFF) G.texScaleS = 1.0f;
            if ((w1 & 0xFFFF) == 0xFFFF) G.texScaleT = 1.0f;
            break;
        case 0xBC: CmdMoveWord(w0, w1); break;                  /* G_MOVEWORD */
        case 0xBD:                                              /* G_POPMTX */
            if (G.mvTop > 0) G.mvTop--;
            G.mvpDirty = 1;
            break;
        case 0xBE: break;                                       /* G_CULLDL */
        case 0xBF:                                              /* G_TRI1 */
            Tri(((w1 >> 16) & 0xFF) / 2, ((w1 >> 8) & 0xFF) / 2, (w1 & 0xFF) / 2);
            break;
        case 0xC0: break;                                       /* G_NOOP */
        case 0xAF: break;                                       /* G_LOAD_UCODE (all the same family) */
        case 0xE4: case 0xE5:                                   /* G_TEXRECT(FLIP) */
            if ((dl[0] >> 24) == 0xB4 && (dl[2] >> 24) == 0xB3) {
                G.half1 = dl[1]; G.half2 = dl[3]; dl += 4;
            } else if ((dl[0] >> 24) == 0xB3 || (dl[0] >> 24) == 0xB4) {
                /* other orders: take them as they come */
                u32 a = dl[0] >> 24;
                if (a == 0xB4) G.half1 = dl[1]; else G.half2 = dl[1];
                dl += 2;
                if ((dl[0] >> 24) == 0xB3 || (dl[0] >> 24) == 0xB4) {
                    if ((dl[0] >> 24) == 0xB4) G.half1 = dl[1]; else G.half2 = dl[1];
                    dl += 2;
                }
            }
            TexRect(w0, w1, G.half1, G.half2, (w0 >> 24) == 0xE5);
            break;
        case 0xE6: case 0xE7: case 0xE8: case 0xE9: break;      /* syncs */
        case 0xEA: case 0xEB: case 0xEC: case 0xEE: break;
        case 0xED:                                              /* G_SETSCISSOR */
            G.scissor[0] = ((w0 >> 12) & 0xFFF) >> 2; G.scissor[1] = (w0 & 0xFFF) >> 2;
            G.scissor[2] = ((w1 >> 12) & 0xFFF) >> 2; G.scissor[3] = (w1 & 0xFFF) >> 2;
            break;
        case 0xEF: sTexDirty = 1; G.omH = w0 & 0xFFFFFF; G.omL = w1; CMB_CHANGED(); break;    /* G_RDPSETOTHERMODE */
        case 0xF0: CmdLoadTlut(w0, w1); break;                  /* G_LOADTLUT */
        case 0xF2: {                                            /* G_SETTILESIZE */
            sTexDirty = 1;
            Tile *t = &G.tile[(w1 >> 24) & 7];
            t->uls = (w0 >> 12) & 0xFFF; t->ult = w0 & 0xFFF;
            t->lrs = (w1 >> 12) & 0xFFF; t->lrt = w1 & 0xFFF;
            break;
        }
        case 0xF3: CmdLoad(w0, w1, 1); break;                   /* G_LOADBLOCK */
        case 0xF4: CmdLoad(w0, w1, 0); break;                   /* G_LOADTILE */
        case 0xF5: {                                            /* G_SETTILE */
            sTexDirty = 1;
            Tile *t = &G.tile[(w1 >> 24) & 7];
            t->fmt = (w0 >> 21) & 7; t->siz = (w0 >> 19) & 3;
            t->line = (w0 >> 9) & 0x1FF; t->tmem = w0 & 0x1FF;
            t->palette = (w1 >> 20) & 15;
            t->cmt = (w1 >> 18) & 3; t->maskt = (w1 >> 14) & 15; t->shiftt = (w1 >> 10) & 15;
            t->cms = (w1 >> 8) & 3; t->masks = (w1 >> 4) & 15; t->shifts = w1 & 15;
            break;
        }
        case 0xF6: FillRect(w0, w1); break;                     /* G_FILLRECT */
        case 0xF7: G.fill = w1; break;                          /* G_SETFILLCOLOR */
        case 0xF8: Color(G.fog, w1); CMB_CHANGED(); break;
        case 0xF9: Color(G.blend, w1); break;
        case 0xFA: { static u32 last0 = 1, last1; if (w1 != last1 || (w0 & 0xFF) != last0) { last0 = w0 & 0xFF; last1 = w1; Color(G.prim, w1); G.primLod = (s32)(w0 & 0xFF) / 255.0f; CMB_CHANGED(); } break; }
        case 0xFB: { static u32 last = 0x12345678; if (w1 != last) { last = w1; Color(G.env, w1); CMB_CHANGED(); } break; }
        case 0xFC: if (G.cmb0 != (w0 & 0xFFFFFF) || G.cmb1 != w1) { G.cmb0 = w0 & 0xFFFFFF; G.cmb1 = w1; CMB_CHANGED(); } break;  /* G_SETCOMBINE */
        case 0xFD:                                              /* G_SETTIMG */
            G.timgFmt = (w0 >> 21) & 7; G.timgSiz = (w0 >> 19) & 3;
            G.timgWidth = (w0 & 0xFFF) + 1; G.timgAddr = w1;
            break;
        case 0xFE: G.zimg = w1 & 0x1FFFFFFF; break;             /* G_SETZIMG */
        case 0xFF: G.cimg = w1 & 0x1FFFFFFF; break;             /* G_SETCIMG */
        default:
            break;
        }
    }
}

void gfx_run(u32 *dl) {
    sFrameId++;
    sRecN = 0;
    sPendN = 0;
    sTexDirty = 1;
    plat_gfx_begin();
    sOutN = 0;
    G.mvTop = 0;
    G.scenery = 0;
    Run(dl);
    Flush();
    plat_gfx_end();
}
