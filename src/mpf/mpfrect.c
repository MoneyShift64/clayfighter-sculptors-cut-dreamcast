/*
 * MPF rectangle helpers (original code 0x80092700-0x80092B80).
 *
 * Rectangles are { x0, y0, x1, y1 } with inclusive corners, either as four
 * s16 (MPF_Rect16) or four s32 (MPF_Rect32). Intersecting moves the top-left
 * corner in and the bottom-right corner in; the union moves them out. Nothing
 * checks for an empty result.
 *
 * Seven of these functions have no symbol in the ROM (nothing in the game
 * calls them); they get the names func_<address> here so they stay available.
 * Mixed-size versions compare the full 32-bit value and store it truncated to
 * 16 bits, as the original does.
 */
#include "common.h"

typedef struct MPF_Rect16 { s16 x0, y0, x1, y1; } MPF_Rect16;
typedef struct MPF_Rect32 { s32 x0, y0, x1, y1; } MPF_Rect32;

void func_80092700(MPF_Rect16 *r, s32 x0, s32 y0, s32 x1, s32 y1);
void mpRectIntersect(MPF_Rect16 *a, const MPF_Rect16 *b);
void func_800927E0(MPF_Rect16 *a, const MPF_Rect32 *b);
void func_80092858(MPF_Rect16 *a, const MPF_Rect16 *b);
void func_800928C4(MPF_Rect16 *a, const MPF_Rect32 *b);
void mpRectNormalize(MPF_Rect16 *r);
void func_80092988(MPF_Rect32 *r, s32 x0, s32 y0, s32 x1, s32 y1);
void func_800929D4(MPF_Rect32 *a, const MPF_Rect16 *b);
void func_80092A2C(MPF_Rect32 *a, const MPF_Rect32 *b);
void func_80092A84(MPF_Rect32 *a, const MPF_Rect16 *b);
void func_80092ADC(MPF_Rect32 *a, const MPF_Rect32 *b);
void func_80092B34(MPF_Rect32 *r);

/*
 * 0x80092700: intersects a 16-bit rectangle with the given corners. The
 * corner arguments are 16-bit in the original (only their low halves count).
 */
void func_80092700(MPF_Rect16 *r, s32 x0, s32 y0, s32 x1, s32 y1) {
    if (r->x0 < (s16)x0) r->x0 = (s16)x0;
    if (r->y0 < (s16)y0) r->y0 = (s16)y0;
    if ((s16)x1 < r->x1) r->x1 = (s16)x1;
    if ((s16)y1 < r->y1) r->y1 = (s16)y1;
}

/* 0x80092770: a = a intersected with b */
void mpRectIntersect(MPF_Rect16 *a, const MPF_Rect16 *b) {
    if (a->x0 < b->x0) a->x0 = b->x0;
    if (a->y0 < b->y0) a->y0 = b->y0;
    if (b->x1 < a->x1) a->x1 = b->x1;
    if (b->y1 < a->y1) a->y1 = b->y1;
}

/* 0x800927E0 (no symbol): 16-bit a intersected with 32-bit b */
void func_800927E0(MPF_Rect16 *a, const MPF_Rect32 *b) {
    if (a->x0 < b->x0) a->x0 = (s16)b->x0;
    if (a->y0 < b->y0) a->y0 = (s16)b->y0;
    if (b->x1 < a->x1) a->x1 = (s16)b->x1;
    if (b->y1 < a->y1) a->y1 = (s16)b->y1;
}

/* 0x80092858 (no symbol): a = union of a and b */
void func_80092858(MPF_Rect16 *a, const MPF_Rect16 *b) {
    if (b->x0 < a->x0) a->x0 = b->x0;
    if (b->y0 < a->y0) a->y0 = b->y0;
    if (a->x1 < b->x1) a->x1 = b->x1;
    if (a->y1 < b->y1) a->y1 = b->y1;
}

/* 0x800928C4 (no symbol): union of 16-bit a and 32-bit b */
void func_800928C4(MPF_Rect16 *a, const MPF_Rect32 *b) {
    if (b->x0 < a->x0) a->x0 = (s16)b->x0;
    if (b->y0 < a->y0) a->y0 = (s16)b->y0;
    if (a->x1 < b->x1) a->x1 = (s16)b->x1;
    if (a->y1 < b->y1) a->y1 = (s16)b->y1;
}

/* 0x8009293C: swaps the corners where needed so that x0 <= x1 and y0 <= y1 */
void mpRectNormalize(MPF_Rect16 *r) {
    s16 t;

    if (r->x1 < r->x0) { t = r->x0; r->x0 = r->x1; r->x1 = t; }
    if (r->y1 < r->y0) { t = r->y0; r->y0 = r->y1; r->y1 = t; }
}

/* 0x80092988: intersects a 32-bit rectangle with the given corners */
void func_80092988(MPF_Rect32 *r, s32 x0, s32 y0, s32 x1, s32 y1) {
    if (r->x0 < x0) r->x0 = x0;
    if (r->y0 < y0) r->y0 = y0;
    if (x1 < r->x1) r->x1 = x1;
    if (y1 < r->y1) r->y1 = y1;
}

/* 0x800929D4 (no symbol): 32-bit a intersected with 16-bit b */
void func_800929D4(MPF_Rect32 *a, const MPF_Rect16 *b) {
    if (a->x0 < b->x0) a->x0 = b->x0;
    if (a->y0 < b->y0) a->y0 = b->y0;
    if (b->x1 < a->x1) a->x1 = b->x1;
    if (b->y1 < a->y1) a->y1 = b->y1;
}

/* 0x80092A2C: a = a intersected with b (32-bit) */
void func_80092A2C(MPF_Rect32 *a, const MPF_Rect32 *b) {
    if (a->x0 < b->x0) a->x0 = b->x0;
    if (a->y0 < b->y0) a->y0 = b->y0;
    if (b->x1 < a->x1) a->x1 = b->x1;
    if (b->y1 < a->y1) a->y1 = b->y1;
}

/* 0x80092A84 (no symbol): union of 32-bit a and 16-bit b */
void func_80092A84(MPF_Rect32 *a, const MPF_Rect16 *b) {
    if (b->x0 < a->x0) a->x0 = b->x0;
    if (b->y0 < a->y0) a->y0 = b->y0;
    if (a->x1 < b->x1) a->x1 = b->x1;
    if (a->y1 < b->y1) a->y1 = b->y1;
}

/* 0x80092ADC (no symbol): a = union of a and b (32-bit) */
void func_80092ADC(MPF_Rect32 *a, const MPF_Rect32 *b) {
    if (b->x0 < a->x0) a->x0 = b->x0;
    if (b->y0 < a->y0) a->y0 = b->y0;
    if (a->x1 < b->x1) a->x1 = b->x1;
    if (a->y1 < b->y1) a->y1 = b->y1;
}

/* 0x80092B34 (no symbol): 32-bit mpRectNormalize */
void func_80092B34(MPF_Rect32 *r) {
    s32 t;

    if (r->x1 < r->x0) { t = r->x0; r->x0 = r->x1; r->x1 = t; }
    if (r->y1 < r->y0) { t = r->y0; r->y0 = r->y1; r->y1 = t; }
}
