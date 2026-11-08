/*
 * PrepareBkgBank (0x80006A00): unpacks an arena's BKG bank from the ROM into a BkgBank -
 * textures, ready-made F3DEX display lists and vertices (see arena.h for the formats).
 *
 * The bank shares three tables between all its models - vertex positions, texture coordinates
 * and colours - stored as tightly packed bit fields. Each model is a grid of cells; each cell
 * has a few display lists, stored as a bit stream of commands (triangles with delta-coded
 * vertex numbers, vertex loads, texture loads, render mode changes ...) which are expanded here
 * into the real GBI commands. The original was one huge function made of gbi.h macros; this is
 * the same work split into small helpers.
 */
#include "common.h"
#include "mpfmem.h"
#include "mpfplat.h"
#include "mpfcomp.h"
#include "arena.h"

void RomCopy(void *dst, u32 romAddr, u32 size);

extern const char D_800C5900[];   /* "PrepareBkgBank() - Unrecognized texture type!" */
extern const char D_800C5930[];   /* "PrepareBkgBank() - *** illegal render mode" */
extern const char D_800C595C[];   /* "PrepareBkgBank() - *** illegal combine mode (1)" */
extern const char D_800C598C[];   /* "PrepareBkgBank() - *** illegal combine mode (2)" */
extern const char D_800C59BC[];   /* "*** %d commands over (should have been %d)\n" */
extern const char D_800C59E8[];   /* "*** WARNING: %d commands under (should have been %d)\n" */

/* K0 (cached) address -> physical, as the RSP wants for vertices and display lists */
#define PHYS(p) ((u32)(p) - 0x80000000)

#define DL_PUT(dl, w0, w1) do { (dl)[0] = (u32)(w0); (dl)[1] = (u32)(w1); (dl) += 2; } while (0)

/* ---- reading the packed bit fields (least significant bit first) ---- */
typedef struct BitStream {
    const u8 *p;
    s32 bit;          /* 0..7 */
} BitStream;

/* the next n (1..8) bits */
static u32 Bits_Get(BitStream *bs, s32 n) {
    u32 v = (u32)bs->p[0] >> bs->bit;
    if (bs->bit + n > 8) v |= (u32)bs->p[1] << (8 - bs->bit);
    v &= (1u << n) - 1;
    bs->bit += n;
    if (bs->bit >= 8) {
        bs->bit -= 8;
        bs->p++;
    }
    return v;
}

/* n bits as a signed number */
static s32 Bits_GetSigned(BitStream *bs, s32 n) {
    s32 v = (s32)Bits_Get(bs, n);
    if (v & (1 << (n - 1))) v |= -(1 << (n - 1));
    return v;
}

/* 11 bits (read as 8 + 3) */
static s32 Bits_Get11(BitStream *bs) {
    s32 v = (s32)Bits_Get(bs, 8);
    return v | (s32)(Bits_Get(bs, 3) << 8);
}

/* a 5-bit colour component (0xF8 becomes full 0xFF) */
static u8 Bits_GetColour(BitStream *bs) {
    u8 c = (u8)(Bits_Get(bs, 5) << 3);
    if (c >= 0xF8) c = 0xFF;
    return c;
}

/* ---- display list helpers (what gbi.h's macros produced) ---- */

/* G_TRI1/G_TRI2 vertex word. The original passed the first vertex as the "flag" argument of
   gSP1Triangle, so the vertex order is rotated by v0 (0: v0 v1 v2, 1: v1 v2 v0, else v2 v0 v1). */
static u32 TriWord(s32 v0, s32 v1, s32 v2) {
    s32 a, b, c;
    if (v0 == 0)      { a = v0; b = v1; c = v2; }
    else if (v0 == 1) { a = v1; b = v2; c = v0; }
    else              { a = v2; b = v0; c = v1; }
    return ((u32)((a * 2) & 0xFF) << 16) | ((u32)((b * 2) & 0xFF) << 8) | (u32)((c * 2) & 0xFF);
}

/* a texture width rounded up to the next multiple of 'align' (if it isn't one) */
static s32 PadWidth(s16 w, s32 align) {
    if (w & (align - 1)) return (s16)(w & ~(align - 1)) + align;
    return w;
}

/*
 * Loads a texture's palette and texels and sets tile 0 up for it:
 * gDPLoadTLUT_pal16/256 + gDPLoadTextureBlock_4b / gDPLoadTextureBlock (CI), with the width
 * rounded up to 16 (4-bit) or 8 (8-bit) texels.
 */
static u32 *Bkg_LoadTexture(u32 *dl, const BkgFileTexture *t, u8 *out) {
    s32 cms, masks, cmt, maskt;
    s32 W, words, dxt, lrs, line;
    u8 *img = out + t->dstOffset;

    if (t->wrapS) { cms = 0; masks = t->maskS; } else { cms = 2; masks = 0; }   /* G_TX_WRAP / CLAMP */
    if (t->wrapT) { cmt = 0; maskt = t->maskT; } else { cmt = 2; maskt = 0; }

    /* the palette */
    DL_PUT(dl, 0xFD100000, img);                                  /* SetTextureImage RGBA16 */
    DL_PUT(dl, 0xE8000000, 0);                                    /* TileSync */
    DL_PUT(dl, 0xF5000100, 0x07000000);                           /* SetTile 7 at tmem 0x100 */
    DL_PUT(dl, 0xE6000000, 0);                                    /* LoadSync */
    DL_PUT(dl, 0xF0000000, (t->is8bit ? 0x073FC000 : 0) | 0x0703C000);   /* LoadTLUT 16 / 256 */
    DL_PUT(dl, 0xE7000000, 0);                                    /* PipeSync */
    DL_PUT(dl, 0xBA000E02, 0x8000);                               /* TextLUT RGBA16 */

    if (t->is8bit == 0) {
        /* CI4: the texels follow the 16-colour palette */
        W = PadWidth(t->width, 16);
        DL_PUT(dl, 0xFD100000, img + 0x20);
        DL_PUT(dl, 0xF5500000, 0x07000000);
        DL_PUT(dl, 0xE6000000, 0);
        words = W / 16;
        if (words < 1) words = 1;
        dxt = (0x800 + words - 1) / words;
        lrs = ((W * t->height + 3) >> 2) - 1;
        if (lrs >= 0x7FF) lrs = 0x7FF;
        DL_PUT(dl, 0xF3000000, 0x07000000 | ((lrs & 0xFFF) << 12) | (dxt & 0xFFF));   /* LoadBlock */
        DL_PUT(dl, 0xE7000000, 0);
        line = ((W >> 1) + 7) >> 3;
        dl[0] = 0xF5400000 | ((line & 0x1FF) << 9);                /* SetTile 0, CI4 */
    } else {
        /* CI8: the texels follow the 256-colour palette */
        W = PadWidth(t->width, 8);
        DL_PUT(dl, 0xFD500000, img + 0x200);
        DL_PUT(dl, 0xF5500000, 0x07000000);
        DL_PUT(dl, 0xE6000000, 0);
        words = W / 8;
        if (words < 1) words = 1;
        dxt = (0x800 + words - 1) / words;
        lrs = ((W * t->height + 1) >> 1) - 1;
        if (lrs >= 0x7FF) lrs = 0x7FF;
        DL_PUT(dl, 0xF3000000, 0x07000000 | ((lrs & 0xFFF) << 12) | (dxt & 0xFFF));
        DL_PUT(dl, 0xE7000000, 0);
        line = (W + 7) >> 3;
        dl[0] = 0xF5480000 | ((line & 0x1FF) << 9);                /* SetTile 0, CI8 */
    }
    dl[1] = ((cmt & 3) << 18) | ((maskt & 0xF) << 14) | ((cms & 3) << 8) | ((masks & 0xF) << 4);
    dl += 2;
    DL_PUT(dl, 0xF2000000, ((((t->width - 1) << 2) & 0xFFF) << 12) | (((t->height - 1) << 2) & 0xFFF));
    return dl;                                                     /* SetTileSize */
}

/* the six colour combiner settings a model can use */
static u32 *Bkg_SetCombine(u32 *dl, u32 combine) {
    switch (combine) {
    case 0: DL_PUT(dl, 0xFCFFFFFF, 0xFFFDF6FB); break;
    case 1: DL_PUT(dl, 0xFCFFFFFF, 0xFFFE793C); break;
    case 2: DL_PUT(dl, 0xFC121824, 0xFF33FFFF); break;
    case 3: DL_PUT(dl, 0xFCFFFE04, 0xFF1279FF); break;
    case 4: DL_PUT(dl, 0xFC121224, 0xFF27FFFF); break;
    case 5: DL_PUT(dl, 0xFC1213FF, 0xFFFFFE38); break;
    default: mpFatalError(D_800C598C); break;
    }
    return dl;
}

/* four colour bytes as a word, r in the high byte (as the N64 reads them; right on either byte order) */
#define BYTES_RGBA(p) (((u32)(p)[0] << 24) | ((u32)(p)[1] << 16) | ((u32)(p)[2] << 8) | (p)[3])

/* what PrepareBkgBank keeps while it works */
typedef struct BkgWork {
    u8 *file;                 /* the packed bank */
    u8 *out;                  /* the BkgBank being built */
    BkgFileTexture *textures;
    s16 *positions;           /* [n][3] */
    u32 *texCoords;           /* (s << 16) | t */
    u8  *colours;             /* [n][4] */
} BkgWork;

/*
 * Expands one cell display list's command stream. Each command starts with a bit:
 *   1  a triangle: 2-bit coding, then its three vertex numbers - as 2-, 3- or 4-bit signed
 *      steps from the last triangle's highest vertex, or (3) as 5-bit numbers. Consecutive
 *      triangles are paired into G_TRI2.
 *   01 (3-bit step) / 0001 (5-bit vertex) + 11-bit index: a vertex's texture coordinates;
 *   001 / 00001: ... its colour (G_MODIFYVTX);
 *   00000 + 3-bit op:
 *      0 load the next (5 bits + 1) vertices     1 load a texture (8-bit index)
 *      2 render mode (3 bits)                    3 colour combiner (3 bits: mode)
 *      4 2-cycle mode on/off                     5 back-face culling off/on
 *      6 alpha compare on/off                    7 end: restore the defaults, G_ENDDL
 * A G_RDPPIPESYNC is put before the first state change after a triangle.
 */
static u32 *Bkg_ExpandCommands(BkgWork *w, BkgFileModel *m, BitStream *bs, u32 *dl, u8 *vtx) {
    s32 loaded = 0;           /* vertices loaded so far (0xEC) */
    s32 last = 1;             /* the last triangle's highest vertex (0xF4) */
    s32 pending = 0;          /* the previous command was a triangle (0x124) */
    s32 paired = 0;           /* ... which is already the second of a G_TRI2 (0x12C) */
    s32 synced = 1;           /* no triangle since the last pipe sync (r22) */
    s32 modeSet = 0, combineSet = 0, alphaSet = 0, cull = 1, twoCycle = 0;
    s32 pa = 0, pb = 0, pc = 0;   /* the previous triangle */
    s32 a, b, c, n, idx, op;
    u32 combine = 0;          /* (garbage in the original if the "illegal combine mode (1)"
                                 error returned; it doesn't) */

    for (;;) {
        if (Bits_Get(bs, 1)) {
            /* ---- a triangle ---- */
            switch (Bits_Get(bs, 2)) {
            case 0:
                a = last + Bits_GetSigned(bs, 2);
                b = last + Bits_GetSigned(bs, 2);
                c = last + Bits_GetSigned(bs, 2);
                break;
            case 1:
                a = last + Bits_GetSigned(bs, 3);
                b = last + Bits_GetSigned(bs, 3);
                c = last + Bits_GetSigned(bs, 3);
                break;
            case 2:
                a = last + Bits_GetSigned(bs, 4);
                b = last + Bits_GetSigned(bs, 4);
                c = last + Bits_GetSigned(bs, 4);
                break;
            default:
                a = (s32)Bits_Get(bs, 5);
                b = (s32)Bits_Get(bs, 5);
                c = (s32)Bits_Get(bs, 5);
                break;
            }
            if (b < a && c < a) last = a;
            else if (a < b && c < b) last = b;
            else last = c;

            if (pending && !paired) {
                /* turn the previous G_TRI1 into a G_TRI2 */
                dl[-2] = 0xB1000000 | TriWord(pa, pb, pc);
                dl[-1] = TriWord(a, b, c);
            } else {
                DL_PUT(dl, 0xBF000000, TriWord(a, b, c));
            }
            if (pending) paired = !paired;
            pa = a; pb = b; pc = c;
            pending = 1;
            synced = 0;
        } else {
            pending = 0;
            if (Bits_Get(bs, 1)) {
                /* texture coordinates, vertex as a step */
                a = last + Bits_GetSigned(bs, 3);
                idx = Bits_Get11(bs);
                DL_PUT(dl, 0xB2140000 | ((a << 1) & 0xFFFF), w->texCoords[idx]);
            } else if (Bits_Get(bs, 1)) {
                /* colour, vertex as a step */
                a = last + Bits_GetSigned(bs, 3);
                idx = Bits_Get11(bs);
                DL_PUT(dl, 0xB2100000 | ((a << 1) & 0xFFFF), BYTES_RGBA(w->colours + idx * 4));
            } else if (Bits_Get(bs, 1)) {
                /* texture coordinates, vertex number */
                a = (s32)Bits_Get(bs, 5);
                idx = Bits_Get11(bs);
                DL_PUT(dl, 0xB2140000 | ((a << 1) & 0xFFFF), w->texCoords[idx]);
            } else if (Bits_Get(bs, 1)) {
                /* colour, vertex number */
                a = (s32)Bits_Get(bs, 5);
                idx = Bits_Get11(bs);
                DL_PUT(dl, 0xB2100000 | ((a << 1) & 0xFFFF), BYTES_RGBA(w->colours + idx * 4));
            } else {
                op = (s32)Bits_Get(bs, 3);
                switch (op) {
                case 0:     /* gSPVertex(next n vertices, 0) */
                    n = (s32)Bits_Get(bs, 5) + 1;
                    DL_PUT(dl, 0x04000000 | (((n << 10) | ((n << 4) - 1)) & 0xFFFF),
                           PHYS(vtx + loaded * 16));
                    loaded += n;
                    last = 1;
                    break;
                case 1:     /* a texture */
                    if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                    idx = (s32)Bits_Get(bs, 8);
                    if (w->textures[idx].is8bit >= 2) mpFatalError(D_800C5900);
                    dl = Bkg_LoadTexture(dl, &w->textures[idx], w->out);
                    break;
                case 2:     /* render mode */
                    if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                    n = (s32)Bits_Get(bs, 3);
                    if ((u32)n < 5) DL_PUT(dl, 0xB900031D, m->modes[n].renderMode1 | m->modes[n].renderMode2);
                    else mpFatalError(D_800C5930);
                    modeSet = n;
                    break;
                case 3:     /* colour combiner of a render mode */
                    if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                    n = (s32)Bits_Get(bs, 3);
                    if ((u32)n < 5) combine = m->modes[n].combine;
                    else mpFatalError(D_800C595C);
                    dl = Bkg_SetCombine(dl, combine);
                    combineSet = n;
                    break;
                case 4:     /* cycle type: 1 or 2 cycles */
                    if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                    n = (s32)Bits_Get(bs, 1);
                    DL_PUT(dl, 0xBA001402, n ? 0x00100000 : 0);
                    twoCycle = n;
                    break;
                case 5:     /* back-face culling (no sync needed) */
                    n = (s32)Bits_Get(bs, 1);
                    DL_PUT(dl, n ? 0xB7000000 : 0xB6000000, 0x2000);
                    cull = n;
                    break;
                case 6:     /* alpha compare */
                    n = (s32)Bits_Get(bs, 1);
                    if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                    DL_PUT(dl, 0xB9000002, n);
                    alphaSet = n;
                    break;
                default:    /* 7: the end - put back whatever was changed */
                    if (modeSet) {
                        if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                        DL_PUT(dl, 0xB900031D, m->modes[0].renderMode1 | m->modes[0].renderMode2);
                    }
                    if (combineSet) {
                        if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                        dl = Bkg_SetCombine(dl, m->modes[0].combine);
                    }
                    if (alphaSet) {
                        if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                        DL_PUT(dl, 0xB9000002, 0);
                    }
                    if (cull != 1) {
                        if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                        DL_PUT(dl, 0xB7000000, 0x2000);
                    }
                    if (twoCycle) {
                        if (!synced) { DL_PUT(dl, 0xE7000000, 0); synced = 1; }
                        DL_PUT(dl, 0xBA001402, 0);
                    }
                    if (!synced) DL_PUT(dl, 0xE7000000, 0);
                    DL_PUT(dl, 0xB8000000, 0);
                    return dl;
                }
            }
        }
        if (!pending) paired = 0;
    }
}

/* one model of the bank (fp in the original); 'obj' is its entry in the bank's object table */
static void Bkg_PrepareModel(BkgWork *w, BkgFileObject *obj) {
    u8 *out = w->out;
    BkgFileModel *m = (BkgFileModel *)(w->file + obj->srcOffset);
    u8 *dst = out + obj->dstOffset;
    u16 base = (u16)obj->dstOffset;          /* the model's offsets are 16-bit, from itself */
    s16 *texIndex;
    BkgMaterial *mats;
    u16 *pointIndex;
    f32 *points;
    u32 pos;                                 /* where the next display list / vertices go */
    s32 i, x, y, z, k;

    dst[0] = 0;
    dst[1] = 3;
    dst[2] = m->v0;
    ((u32 *)dst)[1] = m->v4[0];
    ((u32 *)dst)[2] = m->v4[1];
    ((u32 *)dst)[3] = m->v4[2];
    ((u32 *)dst)[4] = m->v4[3];
    dst[0x14] = (u8)m->gridX;
    dst[0x15] = (u8)m->gridY;
    dst[0x16] = (u8)m->gridZ;
    *(u16 *)(dst + 0x18) = (u16)m->outCellsOffset - base;
    *(u16 *)(dst + 0x1A) = (u16)m->subsPerCell;
    *(u16 *)(dst + 0x1C) = (u16)m->numMaterials;
    *(u16 *)(dst + 0x20) = (u16)m->outPointsOffset - base;
    *(u16 *)(dst + 0x22) = (u16)m->numPoints;
    *(u16 *)(dst + 0x24) = (u16)m->outDataOffset - base;
    *(u16 *)(dst + 0x28) = (u16)m->numMaterials;
    *(u16 *)(dst + 0x2A) = (u16)m->outMaterialsOffset - base;
    *(u32 *)(dst + 0x2C) = 0;
    *(u32 *)(dst + 0x30) = 0;

    /* the materials: a display list each that loads its texture */
    texIndex = (s16 *)(w->file + m->materialTexOffset);
    mats = (BkgMaterial *)(out + m->outMaterialsOffset);
    for (i = 0; i < m->numMaterials; i++) {
        u32 *dl = (u32 *)(out + m->outMaterialDLOffset + i * 0x80);
        mats[i].v0 = 0;
        mats[i].dl = PHYS(dl);
        dl = Bkg_LoadTexture(dl, &w->textures[texIndex[i]], out);
        dl[0] = 0xB8000000;
        dl[1] = 0;
    }

    /* the points (cell corners) */
    pointIndex = (u16 *)(w->file + m->pointsOffset);
    points = (f32 *)(out + m->outPointsOffset);
    for (i = 0; i < m->numPoints; i++) {
        s16 *p = &w->positions[pointIndex[i] * 3];
        points[i * 3 + 0] = (f32)p[0];
        points[i * 3 + 1] = (f32)p[1];
        points[i * 3 + 2] = (f32)p[2];
    }

    /* the cells */
    pos = m->outDataOffset;
    for (z = 0; z < m->gridZ; z++) {
        for (y = 0; y < m->gridY; y++) {
            for (x = 0; x < m->gridX; x++) {
                s32 cell = z * (m->gridX * m->gridY) + y * m->gridX + x;
                u8 *cc = w->file + m->cellsOffset + cell * (m->subsPerCell * 8 + 0x14);
                u8 *dc = out + m->outCellsOffset + cell * (m->subsPerCell * 20 + 0x18);
                u16 *src16 = (u16 *)cc, *dst16 = (u16 *)dc;

                dst16[0] = src16[0];
                dst16[3] = src16[1];
                dst16[2] = src16[2];
                dst16[1] = src16[3];
                dst16[8] = src16[4];
                dst16[4] = src16[5];
                dst16[7] = src16[6];
                dst16[6] = src16[7];
                dst16[5] = src16[8];
                dst16[9] = 0;
                dc[0x14] = cc[0x13];
                dc[0x15] = 0;

                for (k = 0; k < m->subsPerCell; k++) {
                    struct BkgFileSub *sub = (struct BkgFileSub *)(cc + 0x14 + k * 8);
                    BkgSub *out_sub = (BkgSub *)(dc + 0x18 + k * 20);
                    u32 *dlStart, *dl;
                    u8 *vtx, *bytes = NULL;
                    BitStream bs;
                    s32 count, v;

                    if (pos & 7) pos = (pos & ~7u) + 8;
                    out_sub->dlOffset = (s32)(pos - obj->dstOffset);
                    dlStart = (u32 *)(out + pos);
                    pos += sub->numCommands * 8;
                    out_sub->numVertices = sub->numVertices;
                    out_sub->vtxOffset = (s32)(pos - obj->dstOffset);
                    vtx = out + pos;
                    pos += sub->numVertices * 16;
                    if (m->hasVertexBytes) {
                        bytes = out + pos;
                        out_sub->bytesOffset = (s32)(pos - obj->dstOffset);
                        pos += sub->numVertices;
                    } else {
                        out_sub->bytesOffset = 0;
                    }
                    if (sub->numCommands == 0) {
                        out_sub->dlOffset = -1;
                        out_sub->numVertices = 0;
                        out_sub->vtxOffset = 0;
                        out_sub->bytesOffset = 0;
                        out_sub->v10 = 0;
                        continue;
                    }

                    /* the display list */
                    bs.p = (u8 *)m + sub->commandsOffset;
                    bs.bit = 0;
                    dl = Bkg_ExpandCommands(w, m, &bs, dlStart, vtx);
                    count = (s32)(dl - dlStart) >> 1;
                    if (sub->numCommands < count) {
                        mpFatalErrorf(D_800C59BC, count - sub->numCommands, sub->numCommands);
                    } else if (count < sub->numCommands) {
                        mpFatalErrorf(D_800C59E8, sub->numCommands - count, sub->numCommands);
                    }

                    /* the vertices */
                    bs.p = (u8 *)m + sub->verticesOffset;
                    bs.bit = 0;
                    for (v = 0; v < sub->numVertices; v++) {
                        s32 pi, ti, ci;
                        u16 *vt = (u16 *)vtx;
                        pi = (s32)Bits_Get(&bs, 8);
                        pi |= (s32)(Bits_Get(&bs, 4) << 8);
                        vt[0] = (u16)w->positions[pi * 3 + 0];
                        vt[1] = (u16)w->positions[pi * 3 + 1];
                        vt[2] = (u16)w->positions[pi * 3 + 2];
                        vt[3] = 0;
                        ti = Bits_Get11(&bs);
                        vt[4] = (u16)(w->texCoords[ti] >> 16);
                        vt[5] = (u16)w->texCoords[ti];
                        ci = Bits_Get11(&bs);
                        vtx[0xC] = w->colours[ci * 4 + 0];
                        vtx[0xD] = w->colours[ci * 4 + 1];
                        vtx[0xE] = w->colours[ci * 4 + 2];
                        vtx[0xF] = w->colours[ci * 4 + 3];
                        vtx += 16;
                    }
                    /* the extra bytes follow, from the next whole byte */
                    if (m->hasVertexBytes) {
                        bcopy(bs.p + (bs.bit > 0), bytes, sub->numVertices);
                    }
                }
            }
        }
    }
}

/* 0x80006A00 */
BkgBank *PrepareBkgBank(u32 romAddr) {
    BkgFileHeader hdr;
    BkgWork w;
    BkgFileObject *objs;
    BitStream bs;
    u8 *tmp;
    u32 hndl;
    s32 i, j;

    /* the header */
    tmp = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(BkgFileHeader), 0);
    RomCopy(tmp, romAddr, mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, tmp));
    bcopy(tmp, &hdr, sizeof(BkgFileHeader));
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, tmp);

    /* the whole bank, and room for what is built from it */
    hndl = mpLinkedMemoryManager_AllocHndl(&gN64MemoryManager, hdr.fileSize, 0);
    w.file = mpLinkedMemoryManager_LockHndl(&gN64MemoryManager, hndl);
    RomCopy(w.file, romAddr, hdr.fileSize);
    w.out = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, hdr.outSize, 0);
    objs = (BkgFileObject *)(w.file + sizeof(BkgFileHeader));
    w.textures = (BkgFileTexture *)(w.file + hdr.texturesOffset);

    /* the shared vertex positions: 3 x 11-bit signed */
    w.positions = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, hdr.numPositions * 6, 0);
    bs.p = w.file + hdr.positionsOffset;
    bs.bit = 0;
    for (i = 0; i < hdr.numPositions; i++) {
        for (j = 0; j < 3; j++) {
            s32 v = Bits_Get11(&bs);
            if (v & 0x400) v |= -0x800;
            w.positions[i * 3 + j] = (s16)(v + (u16)hdr.bias[j]);
        }
    }

    /* the texture coordinates: 2 x 11-bit signed, in 10.5 fixed point */
    w.texCoords = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, hdr.numTexCoords * 4, 0);
    bs.p = w.file + hdr.texCoordsOffset;
    bs.bit = 0;
    for (i = 0; i < hdr.numTexCoords; i++) {
        u16 st[2];
        for (j = 0; j < 2; j++) {
            s32 v = Bits_Get11(&bs);
            if (v & 0x400) v |= -0x800;
            st[j] = (u16)(v << 5);
        }
        w.texCoords[i] = ((u32)st[0] << 16) | st[1];      /* (as a word: right on either byte order) */
    }

    /* the colours: 4 x 5 bits */
    w.colours = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, hdr.numColours * 4, 0);
    bs.p = w.file + hdr.coloursOffset;
    bs.bit = 0;
    for (i = 0; i < hdr.numColours; i++) {
        for (j = 0; j < 4; j++) w.colours[i * 4 + j] = Bits_GetColour(&bs);
    }

    /* the textures */
    for (i = 0; i < hdr.numTextures; i++) {
        mpDataDecompress(w.out + w.textures[i].dstOffset, w.file + w.textures[i].srcOffset);
    }

    /* the objects */
    for (i = 0; i < hdr.numObjects; i++) {
        if (objs[i].packed) {
            mpDataDecompress(w.out + objs[i].dstOffset, w.file + objs[i].srcOffset);
        } else {
            Bkg_PrepareModel(&w, &objs[i]);
        }
    }

    /* the BkgBank's header */
    {
        BkgBank *bank = (BkgBank *)w.out;
        bank->type = 1;
        bank->v1 = 0;
        bank->v2 = 0;
        bank->numObjects = hdr.numObjects;
        bank->size = hdr.outSize;
        for (i = 0; i < hdr.numObjects; i++) {
            bank->objects[i].offset = objs[i].dstOffset;
            bank->objects[i].v4 = 0;
        }
    }

    mpLinkedMemoryManager_FreeHndl(&gN64MemoryManager, hndl);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, w.positions);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, w.texCoords);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, w.colours);
    return (BkgBank *)w.out;
}
