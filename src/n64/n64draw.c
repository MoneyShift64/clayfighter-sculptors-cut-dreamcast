/*
 * The arena model helpers (0x80082780-0x800836B0) and the 3D sprite drawers
 * (0x800836B0-0x80087420).
 *
 * Models (GfxModel, n64gfx.h; the same layout as BkgModel in src/arena/arena.h) are a grid of
 * cells, each with a few layers of display list + vertices.
 *
 * A 3D sprite is an N64Bitmap drawn as a flat rectangle in the world: its hot spot at x, y, z,
 * its rows along 'up' and its columns along 'right' (one unit a texel). It is drawn in bands
 * of rows that fit the texture memory, two triangles each. flags:
 *   1 mirror, 2 flip, 4 the corner (not the hot spot) at x, y, z, 8 no Z buffer (2D look),
 *   0x10 fog, 0x4000 / 0x8000 (func_80083A64 only) unlit / unlit and see-through,
 *   0xF00 palette number, 0x10000 cull the back instead of no culling.
 * func_80083A64 shades the whole sprite by its facing (the normal as the vertex colour, for
 * the RSP's lighting), func_80084CB0 blends four corner colours down the sprite, and
 * func_80085FE4 four corner colours and alphas.
 */
#include "n64gfx.h"

extern s32 D_800B0D80[4];        /* bytes per texel for formats 1-3 (1, 1, 2, 4) */

/* the cell grid: index of cell (x, y, z) */
#define CELL(m, cells, size, i, j, k) \
    ((GfxCell *)((u8 *)(cells) + ((k) * ((m)->nx * (m)->ny) + (j) * (m)->nx + (i)) * (size)))
#define CELL_SIZE(m) ((m)->numLayers * 20 + 0x18)

/* float -> unsigned int, the way the compiler did it (through 2^31) */
static u32 F32ToU32(f32 f) {
    if (2147483648.0f <= f) return (u32)(s32)(f - 2147483648.0f) | 0x80000000u;
    return (u32)(s32)f;
}
static u32 F64ToU32(f64 f) {
    if (2147483648.0 <= f) return (u32)(s32)(f - 2147483648.0) | 0x80000000u;
    return (u32)(s32)f;
}

/* 0x80082780: Model_Scale - scales the model's points and every vertex */
void func_80082780(GfxModel *m, f32 scale) {
    s32 size = CELL_SIZE(m);
    u8 *cells = (u8 *)m + m->cellsOffset;
    f32 *pts = (f32 *)((u8 *)m + m->pointsOffset);
    s32 i, j, k, l, n;

    for (i = 0; i < m->numPoints * 3; i++) pts[i] = scale * pts[i];
    for (k = 0; k < m->nz; k++) {
        for (j = 0; j < m->ny; j++) {
            for (i = 0; i < m->nx; i++) {
                GfxCell *c = CELL(m, cells, size, i, j, k);
                if (c->used == 0) continue;
                for (l = 0; l < m->numLayers; l++) {
                    GfxSub *e = &c->layers[l];
                    Vtx16 *v;
                    if (e->vtxOffset == 0) continue;
                    v = (Vtx16 *)((u8 *)m + e->vtxOffset);
                    for (n = 0; n < e->numVerts; n++) {
                        v[n].x = (s16)(s32)((f32)v[n].x * scale);
                        v[n].y = (s16)(s32)((f32)v[n].y * scale);
                        v[n].z = (s16)(s32)((f32)v[n].z * scale);
                    }
                }
            }
        }
    }
}

/* 0x80082994: Model_SetAlpha - every vertex's alpha = its original alpha * alpha */
void func_80082994(GfxModel *m, f32 alpha) {
    s32 size = CELL_SIZE(m);
    u8 *cells = (u8 *)m + m->cellsOffset;
    s32 i, j, k, l, n;

    for (k = 0; k < m->nz; k++) {
        for (j = 0; j < m->ny; j++) {
            for (i = 0; i < m->nx; i++) {
                GfxCell *c = CELL(m, cells, size, i, j, k);
                if (c->used == 0) continue;
                for (l = 0; l < m->numLayers; l++) {
                    GfxSub *e = &c->layers[l];
                    Vtx16 *v;
                    u8 *a;
                    if (e->vtxOffset == 0 || e->alphaOffset == 0) continue;
                    v = (Vtx16 *)((u8 *)m + e->vtxOffset);
                    a = (u8 *)m + e->alphaOffset;
                    for (n = 0; n < e->numVerts; n++) {
                        v[n].a = (u8)F32ToU32(alpha * (f32)a[n]);
                    }
                }
            }
        }
    }
}

/* 0x80082B5C: Model_SaveColours - keeps a copy of every vertex's colour (for func_80082ED8) */
void func_80082B5C(GfxModel *m) {
    s32 size = CELL_SIZE(m);
    u8 *cells = (u8 *)m + m->cellsOffset;
    s32 i, j, k, l, n;

    for (k = 0; k < m->nz; k++) {
        for (j = 0; j < m->ny; j++) {
            for (i = 0; i < m->nx; i++) {
                GfxCell *c = CELL(m, cells, size, i, j, k);
                if (c->used == 0) continue;
                for (l = 0; l < m->numLayers; l++) {
                    GfxSub *e = &c->layers[l];
                    Vtx16 *v;
                    if (e->vtxOffset == 0) continue;
                    e->savedColours = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, e->numVerts * 3, 0);
                    v = (Vtx16 *)((u8 *)m + e->vtxOffset);
                    for (n = 0; n < e->numVerts; n++) {
                        e->savedColours[n * 3 + 0] = v[n].r;
                        e->savedColours[n * 3 + 1] = v[n].g;
                        e->savedColours[n * 3 + 2] = v[n].b;
                    }
                }
            }
        }
    }
}

/* 0x80082D60: Model_FreeColours */
void func_80082D60(GfxModel *m) {
    s32 size = CELL_SIZE(m);
    u8 *cells = (u8 *)m + m->cellsOffset;
    s32 i, j, k, l;

    for (k = 0; k < m->nz; k++) {
        for (j = 0; j < m->ny; j++) {
            for (i = 0; i < m->nx; i++) {
                GfxCell *c = CELL(m, cells, size, i, j, k);
                if (c->used == 0) continue;
                for (l = 0; l < m->numLayers; l++) {
                    GfxSub *e = &c->layers[l];
                    if (e->savedColours != NULL) {
                        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, e->savedColours);
                        e->savedColours = NULL;
                    }
                }
            }
        }
    }
}

/* 0x80082ED8: Model_Tint - every vertex's colour = its saved colour * (r, g, b) */
void func_80082ED8(GfxModel *m, f32 r, f32 g, f32 b) {
    u8 lr[256], lg[256], lb[256];
    s32 size = CELL_SIZE(m);
    u8 *cells = (u8 *)m + m->cellsOffset;
    s32 i, j, k, l, n;

    for (i = 0; i < 256; i++) lr[i] = (u8)F32ToU32(r * (f32)i);
    for (i = 0; i < 256; i++) lg[i] = (u8)F32ToU32(g * (f32)i);
    for (i = 0; i < 256; i++) lb[i] = (u8)F32ToU32(b * (f32)i);
    for (k = 0; k < m->nz; k++) {
        for (j = 0; j < m->ny; j++) {
            for (i = 0; i < m->nx; i++) {
                GfxCell *c = CELL(m, cells, size, i, j, k);
                if (c->used == 0) continue;
                for (l = 0; l < m->numLayers; l++) {
                    GfxSub *e = &c->layers[l];
                    Vtx16 *v;
                    if (e->vtxOffset == 0 || e->savedColours == NULL) continue;
                    v = (Vtx16 *)((u8 *)m + e->vtxOffset);
                    for (n = 0; n < e->numVerts; n++) {
                        v[n].r = lr[e->savedColours[n * 3 + 0]];
                        v[n].g = lg[e->savedColours[n * 3 + 1]];
                        v[n].b = lb[e->savedColours[n * 3 + 2]];
                    }
                }
            }
        }
    }
}

/* 0x800831F4: Model_DrawWithModes - the model's lights, geometry mode, render mode and
 * combiner, then the model */
void func_800831F4(u32 **dl, GfxModel *m) {
    func_8007FD70(&gDL, m->lightMode);
    func_8007FB08(&gDL, m->geometryMode);
    func_8007FB90(&gDL, m->renderMode1, m->renderMode2);
    func_8007FBE8(&gDL, m->combine);
    func_80083270(dl, m);
}

/* 0x80083270: Model_Draw - segments 1 and 2 point into the model; a one-cell model is one
 * display list, otherwise each layer's material, then that layer of every visible cell.
 * (The points are projected to the screen into a temporary buffer that is never used.)
 * Apart from the segments, everything goes to gDL. */
void func_80083270(u32 **dl, GfxModel *m) {
    s32 size, i, j, k, l;
    u8 *cells;
    u8 *mats;
    f32 *pts;
    f32 *tmp;
    u32 *p;

    DL_PUSH(dl, 0xBC000406, K0_TO_PHYS(m));                         /* segment 1 */
    DL_PUSH(dl, 0xBC000806, K0_TO_PHYS((u8 *)m + m->segment2Offset)); /* segment 2 */
    if (m->nx == 1 && m->ny == 1 && m->nz == 1) {
        if (((GfxCell *)((u8 *)m + m->cellsOffset))->hidden == 0) {
            p = gDL; gDL = p + 6;
            p[0] = G_RDPPIPESYNC_W0; p[1] = 0;
            p[2] = 0x06000000;  p[3] = m->wholeDL | 0x01000000;
            p[4] = G_RDPPIPESYNC_W0; p[5] = 0;
        }
        return;
    }

    size = CELL_SIZE(m);
    cells = (u8 *)m + m->cellsOffset;
    mats = (u8 *)m + m->layerDLsOffset;
    pts = (f32 *)((u8 *)m + m->pointsOffset);
    tmp = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, m->numPoints * 12, 0);
    for (i = 0; i < m->numPoints; i++) func_800801F4(&pts[i * 3], &tmp[i * 3]);
    for (k = 0; k < m->nz; k++) {
        for (j = 0; j < m->ny; j++) {
            for (i = 0; i < m->nx; i++) {
                GfxCell *c = CELL(m, cells, size, i, j, k);
                if (c->used == 0) continue;
                c->visible = (c->hidden == 0) ? 1 : 0;
            }
        }
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, tmp);

    for (l = 0; l < m->numLayers; l++) {
        if (l < m->numLayerDLs) {
            p = gDL; gDL = p + 6;
            p[0] = G_RDPPIPESYNC_W0; p[1] = 0;
            p[2] = 0x06000000;  p[3] = ((u32 *)mats)[l * 2 + 1];
            p[4] = G_RDPPIPESYNC_W0; p[5] = 0;
        }
        for (k = 0; k < m->nz; k++) {
            for (j = 0; j < m->ny; j++) {
                for (i = 0; i < m->nx; i++) {
                    GfxCell *c = CELL(m, cells, size, i, j, k);
                    GfxSub *e;
                    if (c->used == 0 || c->visible == 0) continue;
                    e = &c->layers[l];
                    if (e->dlist == -1) continue;
                    p = gDL; gDL = p + 6;
                    p[0] = G_RDPPIPESYNC_W0; p[1] = 0;
                    p[2] = 0x06000000;  p[3] = (u32)e->dlist | 0x01000000;
                    p[4] = G_RDPPIPESYNC_W0; p[5] = 0;
                }
            }
        }
    }
}

/* ---- 3D sprites ------------------------------------------------------------------------ */

/* the up / right vectors a sprite is drawn with, after mirroring and flipping: the origin
 * uses u and r, the vertices uu (rows) and rr (columns); 'o' is the top left vertex */
typedef struct SpriteAxes {
    f32 u[3], r[3], rr[3], uu[3];
    f32 o[3];
} SpriteAxes;

static void Sprite_Axes(SpriteAxes *a, N64Bitmap *img, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    a->u[0] = up[0]; a->u[1] = up[1]; a->u[2] = up[2];
    a->uu[0] = up[0]; a->uu[1] = up[1]; a->uu[2] = up[2];
    a->r[0] = right[0]; a->r[1] = right[1]; a->r[2] = right[2];
    a->rr[0] = right[0]; a->rr[1] = right[1]; a->rr[2] = right[2];
    if (flags & 1) {
        a->r[0] = -a->r[0]; a->r[1] = -a->r[1]; a->r[2] = -a->r[2];
    } else {
        a->rr[0] = -a->rr[0]; a->rr[1] = -a->rr[1]; a->rr[2] = -a->rr[2];
    }
    if (flags & 2) {
        a->u[0] = -a->u[0]; a->u[1] = -a->u[1]; a->u[2] = -a->u[2];
    } else {
        a->uu[0] = -a->uu[0]; a->uu[1] = -a->uu[1]; a->uu[2] = -a->uu[2];
    }
    if (flags & 4) {
        a->o[0] = x; a->o[1] = y; a->o[2] = z;
    } else {
        a->o[0] = (x + (f32)img->xHot * a->r[0]) + (f32)img->yHot * a->u[0];
        a->o[1] = (y + (f32)img->xHot * a->r[1]) + (f32)img->yHot * a->u[1];
        a->o[2] = (z + (f32)img->xHot * a->r[2]) + (f32)img->yHot * a->u[2];
    }
}

/* 0x800836B0: Sprite_Corners - where a sprite's four corners would be (as func_80083A64
 * draws it): c0 the origin, c1 + width, c2 + height, c3 both */
void func_800836B0(CharFrame *f, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags,
                   f32 *c0, f32 *c1, f32 *c2, f32 *c3) {
    N64Bitmap *img = (N64Bitmap *)f;
    f32 u[3], r[3], rr[3], uu[3], o[3];

    u[0] = up[0]; u[1] = up[1]; u[2] = up[2];
    uu[0] = up[0]; uu[1] = up[1]; uu[2] = up[2];
    r[0] = right[0]; r[1] = right[1]; r[2] = right[2];
    rr[0] = right[0]; rr[1] = right[1]; rr[2] = right[2];
    if (flags & 1) {
        r[0] = -r[0]; r[1] = -r[1]; r[2] = -r[2];
    } else {
        rr[0] = -rr[0]; rr[1] = -rr[1]; rr[2] = -rr[2];
    }
    if (flags & 2) {
        u[0] = -u[0]; u[1] = -u[1]; u[2] = -u[2];
    } else {
        uu[0] = -uu[0]; uu[1] = -uu[1]; uu[2] = -uu[2];
    }
    if (flags & 4) {
        o[0] = x; o[1] = y; o[2] = z;
    } else {
        o[0] = (x + (f32)img->xHot * r[0]) + (f32)img->yHot * u[0];
        o[1] = (y + (f32)img->xHot * r[1]) + (f32)img->yHot * u[1];
        o[2] = (z + (f32)img->xHot * r[2]) + (f32)img->yHot * u[2];
    }
    c0[0] = o[0]; c0[1] = o[1]; c0[2] = o[2];
    c1[0] = (f32)img->width * rr[0] + o[0];
    c1[1] = (f32)img->width * rr[1] + o[1];
    c1[2] = (f32)img->width * rr[2] + o[2];
    c2[0] = (f32)img->height * uu[0] + o[0];
    c2[1] = (f32)img->height * uu[1] + o[1];
    c2[2] = (f32)img->height * uu[2] + o[2];
    c3[0] = ((f32)img->width * rr[0] + o[0]) + (f32)img->height * uu[0];
    c3[1] = ((f32)img->width * rr[1] + o[1]) + (f32)img->height * uu[1];
    c3[2] = ((f32)img->width * rr[2] + o[2]) + (f32)img->height * uu[2];
}

enum { SPRITE_LIT, SPRITE_CORNERS, SPRITE_CORNERS_ALPHA };

/* the corner colours func_80084CB0 / func_80085FE4 blend: 0 top left (where the sprite
 * starts), 1 top right, 2 bottom left, 3 bottom right (set by the fighter lighting) */
#define C0R B_800F7778
#define C0G B_800F7770
#define C0B B_800F4E34
#define C0A B_800F5B78
#define C1R B_800F7D94
#define C1G B_800F7789
#define C1B B_800F7E78
#define C1A B_800F8760
#define C2R B_800F7E70
#define C2G B_800F88C8
#define C2B B_800F2D1C
#define C2A B_800F2D1D
#define C3R B_800F7788
#define C3G B_800F88BC
#define C3B B_800F7ECC
#define C3A B_800F7E58

/* the three drawers, which differ only in their render modes and vertex colours */
static void Sprite_Draw(s32 kind, u32 **dl, N64Bitmap *img, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                        s32 flags) {
    SpriteAxes ax;
    s32 savedGeom;
    u32 nr = 0, ng = 0, nb = 0, alpha = 0xFF;    /* SPRITE_LIT's vertex colour */
    u32 addr;
    s32 texW, w, h, lineBytes, bandRows, band, row;
    u32 tmem, pal = 0, tw, lrt, tileW;
    s32 combine;
    /* the corner kinds: left (0 -> 2) and right (1 -> 3) colours, now and per band */
    s32 lc[4], rc[4], ls[4], rs[4], ln[4], rn[4];
    s32 i, nBands;

    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    if (kind == SPRITE_LIT) {
        if (B_800F5CD8 & 0x20000) {                      /* lighting: the normal as the colour */
            f32 n[3];
            n[0] = up[1] * right[2] - up[2] * right[1];
            n[1] = up[2] * right[0] - up[0] * right[2];
            n[2] = up[0] * right[1] - up[1] * right[0];
            func_80087DD0(n);
            nr = F64ToU32((f64)n[0] * 127.0);
            ng = F64ToU32((f64)n[1] * 127.0);
            nb = F64ToU32((f64)n[2] * 127.0);
        } else {
            nr = 0xFF; ng = 0xFF; nb = 0xFF;
        }
    }
    Sprite_Axes(&ax, img, x, y, z, up, right, flags);

    /* culling: none, or the back with flags 0x10000 */
    savedGeom = B_800F5CD8;
    if (flags & 0x10000) {
        func_8007FB08(dl, (savedGeom & ~0x1000) | 0x2000);
    } else if (B_800F5CD8 & 0x3000) {
        func_8007FB08(dl, B_800F5CD8 & ~0x3000);
    }

    if (img->format & 0x60) {
        if (img->format & 0x40) {                        /* smooth alpha */
            if (flags & 8) func_8007FB90(dl, 0x00404240, 0x00404240);
            else           func_8007FB90(dl, 0x00404A50, 0x00404A50);
            DL_PUSH(dl, 0xB9000002, 1);
            DL_PUSH(dl, 0xF9000000, 0x80808001);
        } else {                                         /* 1-bit alpha */
            DL_PUSH(dl, 0xB9000002, 1);
            if ((img->format & 0xF) != 2) DL_PUSH(dl, 0xF9000000, 0x80808001);
            if (flags & 8)                func_8007FB90(dl, 0x00404240, 0x00404240);
            else if (kind == SPRITE_LIT)  func_8007FB90(dl, 0x00443030, 0x00443030);
            else                          func_8007FB90(dl, 0x00404A50, 0x00404A50);
        }
    } else {
        if (kind == SPRITE_CORNERS_ALPHA) {
            if (flags & 8) func_8007FB90(dl, 0x00404240, 0x00404240);
            else           func_8007FB90(dl, 0x00404A50, 0x00404A50);
        } else {
            if (flags & 8) func_8007FB90(dl, 0x0C084000, 0x0C084000);
            else           func_8007FB90(dl, 0x00442230, 0x00442230);
        }
        DL_PUSH(dl, 0xB9000002, 0);
    }

    if (kind == SPRITE_LIT) {
        if (flags & 0x8000) {                            /* unlit, see-through */
            DL_PUSH(dl, 0xB9000002, 0);
            func_8007FB08(dl, B_800F5CD8 & 0xFFFDFFFF);
            nr = 0; ng = 0; nb = 0;
            alpha = 0x8C;
        } else {
            if (flags & 0x4000) {                        /* unlit */
                func_8007FB08(dl, B_800F5CD8 & 0xFFFDFFFF);
                nr = 0; ng = 0; nb = 0;
            }
            alpha = 0xFF;
        }
    }

    if (flags & 8) func_8007FB08(dl, B_800F5CD8 & ~1);   /* Z buffer off */
    else           func_8007FB08(dl, B_800F5CD8 | 1);
    if (flags & 0x10) {                                  /* fog */
        func_8007FB08(dl, B_800F5CD8 | 0x10000);
        if (img->format & 0x60) {
            DL_PUSH(dl, 0xBA001402, 0x00100000);         /* 2 cycles */
            func_8007FB90(dl, 0xC8000000, 0x00104A50);
            combine = 5;
        } else {
            func_8007FB90(dl, 0xC8004030, 0xC8004030);
            combine = 4;
        }
    } else {
        func_8007FB08(dl, B_800F5CD8 & 0xFFFEFFFF);
        combine = 2;
    }
    func_8007FBE8(dl, combine);

    addr = ((u32)img + img->dataOffset) & 0x1FFFFFFF;
    if ((img->format & 0xF) == 0) texW = img->stride << 1;
    else                          texW = (s32)img->stride / D_800B0D80[img->format & 0xF];
    w = img->width;
    h = img->height;
    if ((img->format & 0xF) == 0) {
        lineBytes = (w & 1) ? ((w & ~1) + 2) >> 1 : w / 2;
        if (lineBytes & 7) lineBytes = (lineBytes & ~7) + 8;
    } else {
        lineBytes = w * D_800B0D80[img->format & 0xF];
        if (lineBytes & 7) lineBytes = (lineBytes & ~7) + 8;
    }

    if ((img->format & 0xF) < 2) {
        if (img->format & 0x10) {                        /* load the palette */
            DL_PUSH(dl, 0xFD100000, (u32)img + 0x10);
            DL_PUSH(dl, 0xE8000000, 0);
            DL_PUSH(dl, 0xF5000100, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF0000000, ((u32)img->palCount << 14) | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        }
        pal = ((u32)flags & 0xF00) >> 8;
        DL_PUSH(dl, 0xBA000E02, 0x8000);
        tmem = 0x800;
    } else {
        DL_PUSH(dl, 0xBA000E02, 0);
        tmem = 0xF00;
    }
    bandRows = (s32)(tmem / (u32)lineBytes);

    if (kind != SPRITE_LIT) {
        nBands = h / bandRows;
        if (nBands == 0) nBands = 1;
        lc[0] = C0R; lc[1] = C0G; lc[2] = C0B; lc[3] = C0A;
        rc[0] = C1R; rc[1] = C1G; rc[2] = C1B; rc[3] = C1A;
        ls[0] = (C2R - C0R) / nBands; ls[1] = (C2G - C0G) / nBands;
        ls[2] = (C2B - C0B) / nBands; ls[3] = (C2A - C0A) / nBands;
        rs[0] = (C3R - C1R) / nBands; rs[1] = (C3G - C1G) / nBands;
        rs[2] = (C3B - C1B) / nBands; rs[3] = (C3A - C1A) / nBands;
    }

    pal = (pal & 0xF) << 20;
    tileW = ((u32)texW - 1) & 0xFFF;
    for (row = 0; row < h; row += bandRows) {
        Vtx16 *v;
        u32 *p;
        f32 fb;

        band = h - row;
        if (bandRows < band) band = bandRows;
        if (kind != SPRITE_LIT) {
            if (row + band < h) {
                for (i = 0; i < 4; i++) { ln[i] = lc[i] + ls[i]; rn[i] = rc[i] + rs[i]; }
            } else {
                ln[0] = C2R; ln[1] = C2G; ln[2] = C2B; ln[3] = C2A;
                rn[0] = C3R; rn[1] = C3G; rn[2] = C3B; rn[3] = C3A;
            }
        }
        lrt = (((u32)band - 1) << 2) & 0xFFF;
        switch (img->format & 0xF) {
        case 0:                                          /* CI4, loaded as CI8 */
            tw = ((((u32)((s16)w >> 1) + 7) >> 3) & 0x1FF) << 9;
            DL_PUSH(dl, 0xFD480000 | (((u32)(texW >> 1) - 1) & 0xFFF), addr);
            DL_PUSH(dl, 0xF5480000 | tw, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF4000000, ((((u32)(w - 1) << 1) & 0xFFF) << 12) | lrt | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            DL_PUSH(dl, 0xF5400000 | tw, pal);
            DL_PUSH(dl, 0xF2000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt);
            break;
        case 1:                                          /* CI8 */
            tw = ((((u32)(w + 7)) >> 3) & 0x1FF) << 9;
            DL_PUSH(dl, 0xFD480000 | tileW, addr);
            DL_PUSH(dl, 0xF5480000 | tw, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF4000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            DL_PUSH(dl, 0xF5480000 | tw, pal);
            DL_PUSH(dl, 0xF2000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt);
            break;
        case 2:                                          /* RGBA16 */
            tw = ((((u32)(w * 2 + 7)) >> 3) & 0x1FF) << 9;
            DL_PUSH(dl, 0xFD100000 | tileW, addr);
            DL_PUSH(dl, 0xF5100000 | tw, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF4000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            DL_PUSH(dl, 0xF5100000 | tw, 0);
            DL_PUSH(dl, 0xF2000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt);
            break;
        case 3:                                          /* RGBA32 */
            tw = ((((u32)(w * 2 + 7)) >> 3) & 0x1FF) << 9;
            DL_PUSH(dl, 0xFD180000 | tileW, addr);
            DL_PUSH(dl, 0xF5180000 | tw, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF4000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            DL_PUSH(dl, 0xF5180000 | tw, 0);
            DL_PUSH(dl, 0xF2000000, ((((u32)(w - 1) << 2) & 0xFFF) << 12) | lrt);
            break;
        default:
            break;
        }

        /* four vertices: the band's top left, top right, bottom right, bottom left */
        fb = (f32)band;
        v = gVtx;
        v[0].x = (s16)(s32)ax.o[0];
        v[0].y = (s16)(s32)ax.o[1];
        v[0].z = (s16)(s32)ax.o[2];
        v[0].flag = 0; v[0].s = 0; v[0].t = 0;
        v[1].x = (s16)(s32)((f32)w * ax.rr[0] + ax.o[0]);
        v[1].y = (s16)(s32)((f32)w * ax.rr[1] + ax.o[1]);
        v[1].z = (s16)(s32)((f32)w * ax.rr[2] + ax.o[2]);
        v[1].flag = 0; v[1].s = (s16)((u16)w << 5); v[1].t = 0;
        v[2].x = (s16)(s32)((fb * ax.uu[0] + ax.o[0]) + (f32)w * ax.rr[0]);
        v[2].y = (s16)(s32)((fb * ax.uu[1] + ax.o[1]) + (f32)w * ax.rr[1]);
        v[2].z = (s16)(s32)((fb * ax.uu[2] + ax.o[2]) + (f32)w * ax.rr[2]);
        v[2].flag = 0; v[2].s = (s16)((u16)w << 5); v[2].t = (s16)((band + 1) << 5);
        v[3].x = (s16)(s32)(fb * ax.uu[0] + ax.o[0]);
        v[3].y = (s16)(s32)(fb * ax.uu[1] + ax.o[1]);
        v[3].z = (s16)(s32)(fb * ax.uu[2] + ax.o[2]);
        v[3].flag = 0; v[3].s = 0; v[3].t = (s16)((band + 1) << 5);
        if (kind == SPRITE_LIT) {
            for (i = 0; i < 4; i++) {
                v[i].r = (u8)nr; v[i].g = (u8)ng; v[i].b = (u8)nb; v[i].a = (u8)alpha;
            }
        } else {
            v[0].r = (u8)lc[0]; v[0].g = (u8)lc[1]; v[0].b = (u8)lc[2];
            v[1].r = (u8)rc[0]; v[1].g = (u8)rc[1]; v[1].b = (u8)rc[2];
            v[2].r = (u8)rn[0]; v[2].g = (u8)rn[1]; v[2].b = (u8)rn[2];
            v[3].r = (u8)ln[0]; v[3].g = (u8)ln[1]; v[3].b = (u8)ln[2];
            if (kind == SPRITE_CORNERS) {
                v[0].a = 0xFF; v[1].a = 0xFF; v[2].a = 0xFF; v[3].a = 0xFF;
            } else {
                v[0].a = (u8)lc[3]; v[1].a = (u8)rc[3]; v[2].a = (u8)rn[3]; v[3].a = (u8)ln[3];
            }
        }
        p = gDL; gDL = p + 6;                            /* (gDL, not dl) */
        p[0] = 0x0400103F;                               /* the 4 vertices */
        p[1] = (u32)gVtx;
        p[2] = 0xBF000000; p[3] = 0x402;                 /* triangles 0 2 1 and 0 3 2 */
        p[4] = 0xBF000000; p[5] = 0x604;
        gVtx = gVtx + 4;

        addr += img->stride * band;
        ax.o[0] = fb * ax.uu[0] + ax.o[0];
        ax.o[1] = fb * ax.uu[1] + ax.o[1];
        ax.o[2] = fb * ax.uu[2] + ax.o[2];
        if (kind != SPRITE_LIT) {
            for (i = 0; i < 4; i++) { lc[i] += ls[i]; rc[i] += rs[i]; }
        }
    }
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    func_8007FB08(dl, savedGeom);
}

/* 0x80083A64: Sprite_DrawLit - shaded by its facing (the RSP's lights) */
void func_80083A64(u32 **dl, void *image, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    Sprite_Draw(SPRITE_LIT, dl, image, x, y, z, up, right, flags);
}

/* 0x80084CB0: Sprite_DrawCorners - four corner colours */
void func_80084CB0(u32 **dl, void *sprite, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    Sprite_Draw(SPRITE_CORNERS, dl, sprite, x, y, z, up, right, flags);
}

/* 0x80085FE4: Sprite_DrawCornersAlpha - four corner colours and alphas (see-through) */
void func_80085FE4(u32 **dl, CharFrame *f, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    Sprite_Draw(SPRITE_CORNERS_ALPHA, dl, (N64Bitmap *)f, x, y, z, up, right, flags);
}
