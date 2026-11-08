/*
 * The game's own N64 graphics layer (0x8007E720-0x80089790, not libultra): display-list
 * helpers, the projection (N64PROJ.C), bitmaps (N64BMP.C), text and sprite drawing, and the
 * VS library's matrices (VSLMTX2D.C, VSLMTX.C).
 *
 * Shared by src/n64/n64gfx.c, n64proj.c, n64bmp.c, n64text.c, n64draw.c, vslmtx.c, vslmtx2d.c.
 * Functions combat.h already declares are not repeated here.
 */
#ifndef N64GFX_H
#define N64GFX_H

#include "common.h"
#include "combat.h"
#include "mpfmem.h"

/* ---- display lists ---------------------------------------------------------------------- */

/* Appends one 64-bit RSP/RDP command (two words) to the display list *dl. */
#define DL_PUSH(dl, w0, w1) do { u32 *_p = *(dl); *(dl) = _p + 2; _p[0] = (w0); _p[1] = (w1); } while (0)
/* K0 (cached) address -> physical, as the RSP wants it */
#define K0_TO_PHYS(p) ((u32)(p) - 0x80000000u)

#define G_RDPPIPESYNC_W0 0xE7000000u

/* ---- bitmaps (N64BMP.C) ------------------------------------------------------------------ */
/* An N64 bitmap: a 16-byte header, then (for colour-indexed ones) the palette, then texels.
 * The same layout is used for character frames (CharFrame), font glyphs and menu pictures. */
typedef struct N64Bitmap {
    /* 0x00 */ u8  format;      /* low nibble: 0 CI4, 1 CI8, 2 RGBA16, 3 RGBA32;
                                   0x10 has a palette at +0x10, 0x20 1-bit alpha, 0x40 smooth alpha */
    /* 0x01 */ u8  palCount;    /* palette size for the TLUT load (count << 14) */
    /* 0x02 */ u16 dataOffset;  /* texels, from the start of the header */
    /* 0x04 */ u16 alphaOffset; /* 0, or a run-length coded alpha channel */
    /* 0x06 */ s16 width;
    /* 0x08 */ s16 height;
    /* 0x0A */ s16 xHot;        /* hot spot (the point drawn at x, y) */
    /* 0x0C */ s16 yHot;
    /* 0x0E */ u16 stride;      /* bytes per row */
    /* 0x10 */ u16 palette[1];
} N64Bitmap;

extern s32 D_800B0D60[4];          /* bytes per texel for formats 1-3 (1, 1, 2, 4) */
extern u32 D_800B0D70[2];          /* alpha for the 1-bit alpha formats: 0, 255 */

void func_80080590(u32 **dl, N64Bitmap *bmp, s32 *clip, s32 x, s32 y, s32 flags);
N64Bitmap *mpDecodeBitmapTo32Bit(N64Bitmap *bmp);
void func_80081924(u32 **dl, N64Bitmap *bmp, s32 mode);
void func_80092988(s32 *rect, s32 x0, s32 y0, s32 x1, s32 y1);   /* clip a rectangle to a box */
void *func_80098070(void);                                       /* the frame buffer being shown */
void RomCopy(void *dst, u32 romAddr, u32 size);

/* ---- fonts ----------------------------------------------------------------------------- */
typedef struct FontGlyph {
    /* 0x00 */ u32 offset;      /* the glyph's N64Bitmap, from the start of the font; 0 = none */
    /* 0x04 */ s16 width;
    /* 0x06 */ s16 pad;
} FontGlyph;

typedef struct Font {
    /* 0x00 */ u32 v0;
    /* 0x04 */ s16 spaceWidth;  /* a space (a tab is three) */
    /* 0x06 */ s16 spacing;     /* gap after each letter */
    /* 0x08 */ s16 numPalettes;
    /* 0x0A */ u16 palettesOffset;  /* table of palette offsets (u32, from the font) */
    /* 0x0C */ s16 firstChar;
    /* 0x0E */ s16 lastChar;
    /* 0x10 */ u32 glyphsOffset;    /* FontGlyph[lastChar - firstChar + 1] */
} Font;

/* the drawing function a 3D text line uses for each letter */
typedef void (*GlyphDrawFn)(u32 **dl, void *img, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void func_800825B0(Font *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                   s32 flags, GlyphDrawFn draw);
void func_80082358(Font *font, u32 **dl, const char *s, s32 *clip, s32 x, s32 y, s32 flags);

/* ---- the arena models (same layout as BkgModel in src/arena/arena.h) --------------- */
typedef struct GfxSub {        /* one layer of one cell (0x14 bytes) */
    /* 0x00 */ s32 dlist;        /* segment-1 display list, -1 = none */
    /* 0x04 */ s32 numVerts;
    /* 0x08 */ u32 vtxOffset;    /* Vtx16[numVerts], from the model */
    /* 0x0C */ u32 alphaOffset;  /* u8[numVerts] original alpha, from the model */
    /* 0x10 */ u8 *savedColours; /* rgb copy (func_80082B5C) */
} GfxSub;

typedef struct GfxCell {          /* 0x18 + 0x14 * numLayers bytes */
    /* 0x00 */ u8  pad0[0x12];
    /* 0x12 */ u16 hidden;
    /* 0x14 */ s8  used;
    /* 0x15 */ s8  visible;
    /* 0x16 */ u8  pad16[2];
    /* 0x18 */ GfxSub layers[1];
} GfxCell;

typedef struct GfxModel {
    /* 0x00 */ u8  pad0[2];
    /* 0x02 */ s8  lightMode;    /* func_8007FD70 */
    /* 0x03 */ u8  pad3;
    /* 0x04 */ s32 geometryMode;
    /* 0x08 */ u32 renderMode1, renderMode2;
    /* 0x10 */ s32 combine;
    /* 0x14 */ u8  nx, ny, nz;   /* grid of cells */
    /* 0x17 */ u8  v17;
    /* 0x18 */ u16 cellsOffset;
    /* 0x1A */ u16 numLayers;
    /* 0x1C */ u16 numLayerDLs;
    /* 0x1E */ u16 v1E;
    /* 0x20 */ u16 pointsOffset; /* f32[numPoints][3] */
    /* 0x22 */ u16 numPoints;
    /* 0x24 */ u16 wholeDL;      /* segment-1 display list for a one-cell model */
    /* 0x26 */ u16 v26, v28;
    /* 0x2A */ u16 layerDLsOffset;   /* {u32, u32 dlist}[numLayerDLs] */
    /* 0x2C */ u32 segment2Offset;
} GfxModel;

void func_8007FD70(u32 **dl, s32 mode);
void func_80083270(u32 **dl, GfxModel *m);
void func_800831F4(u32 **dl, GfxModel *m);
void func_80082780(GfxModel *m, f32 scale);
void func_80082994(GfxModel *m, f32 alpha);
void func_80082B5C(GfxModel *m);
void func_80082D60(GfxModel *m);
void func_80082ED8(GfxModel *m, f32 r, f32 g, f32 b);

/* ---- display-list state the helpers cache ------------------------------------------------ */
extern u32 *B_800F882C;          /* colour (frame) buffer being drawn */
extern u32 *B_800F1334;          /* the Z buffer */
extern u8  B_800F5B48[];         /* the viewport */
extern s32 B_800F5CD8;           /* geometry mode set */
extern u32 B_800F4E68, B_800F88E8;   /* render mode set (two cycles) */
extern s32 B_800F7E54;           /* combiner set (func_8007FBE8's index) */
extern u32 B_800F5D24;           /* fill colour set */
extern s32 B_800F5B60;           /* lighting set (func_8007FD70's mode) */
extern u8 *B_800F5B6C;           /* where this frame's matrices go */
extern s32 D_800B0D50, D_800B0D54;   /* debug line drawing: points so far, vertex slot */
extern u8  D_800C2C80[], D_800C3480[], D_800C3C80[];   /* light sets (G_MOVEMEM) */
extern u8  D_800A64D0[], D_800A7900[], D_800A8CD0[];   /* their light display lists */

void func_8007FCF8(u32 **dl, s32 r, s32 g, s32 b);
void func_8007F2A8(u32 **dl, s32 *rect, s32 r, s32 g, s32 b);
void func_80097140(f32 mf[4][4], f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale);   /* guOrthoF */
void func_80096D30(f32 mf[4][4], void *mtx);                                             /* guMtxF2L */
void func_800973C0(f32 mf[4][4], u16 *perspNorm, f32 fovy, f32 aspect, f32 n, f32 f, f32 scale); /* guPerspectiveF */
void func_80096FB0(f32 a[4][4], f32 b[4][4], f32 out[4][4]);                            /* guMtxCatF */

/* ---- projection (N64PROJ.C) -------------------------------------------------------------- */
extern f32 B_800F7790[4][4];     /* the perspective matrix */
extern u16 B_800F5BC0;           /* its perspNorm */
extern u8  B_800F87B8[];         /* ... as a fixed-point Mtx */
extern f32 B_800F77D4;           /* perspNorm / 65536 */
extern f32 B_800F5AA8[4][4];     /* the camera matrix */
extern f32 B_800F4E98[4][4];     /* camera * perspective */
extern f32 B_800F7DB8[4][4];     /* the model matrix */
extern f32 B_800F5BC8[2];        /* screen scale (x, y) */
extern f32 B_800F7E00[2];        /* screen centre (x, y) */
void func_8007FF20(s32 fovy, f32 nearZ, f32 farZ);
void func_8007FFB8(u32 **dl);
void func_80080100(u32 **dl);

/* ---- VSLMTX2D.C / VSLMTX.C ---------------------------------------------------------------- */
typedef struct vsMatrix2D { f32 m[3][3]; } vsMatrix2D;
typedef struct vsMatrix   { f32 m[4][4]; } vsMatrix;     /* row vectors: [x y z 1] * M */

extern f32 D_800B0D90[0x4000];   /* sine table, a quarter turn */
f32  vsSin(s32 angle);           /* 0x10000 = a full turn */
f32  vsCos(s32 angle);
void vsMatrix2DInvert(vsMatrix2D *src, vsMatrix2D *dst);
void vsMatrixInvert(vsMatrix *src, vsMatrix *dst);
f32  func_80087D80(f32 *v);      /* length of a 3D vector */
void func_800887EC(vsMatrix *a, vsMatrix *b, vsMatrix *out);   /* out = a * b */
f32  func_80088490(vsMatrix *m); /* determinant */
void func_80088BF0(vsMatrix *src, vsMatrix *dst);              /* adjugate */

/* a light (0x3C bytes) */
typedef struct vsLight {
    /* 0x00 */ s32 type;         /* 0 ambient, 1 and 3 directional spot, 2 point */
    /* 0x04 */ f32 colour[3];
    /* 0x10 */ f32 pos[3];
    /* 0x1C */ f32 dir[3];
    /* 0x28 */ f32 intensity;
    /* 0x2C */ f32 range;        /* point lights */
    /* 0x30 */ f32 v30;
    /* 0x34 */ f32 coneOuter;    /* angles (0x10000 = a full turn); not used by func_800893F8 */
    /* 0x38 */ f32 coneInner;
} vsLight;

#endif
