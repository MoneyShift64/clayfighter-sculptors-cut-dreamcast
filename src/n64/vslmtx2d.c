/*
 * The VS library's sine/cosine (0x80087420) and VSLMTX2D.C (0x80087630-0x80087D80): 3x3
 * matrices for 2D maths, plus the vector helpers after it (0x80087D80-0x80087E20).
 *
 * The same library as the fight code's CF3MTX2D.C (src/char/cf3mtx2d.c), but angles are in
 * VS units (0x10000 = a full turn). Row vectors: [x y 1] * M, the translation sits in the
 * bottom row. "Multiply" functions apply the new transform after the existing one (M = M * T),
 * "PreMultiply" ones before it (M = T * M).
 *
 * As in CF3MTX2D.C the transpose, determinant and inverse were never written: Transpose gives
 * the identity, Determinant returns 0, so Invert always stops the game with "Attempt to invert
 * non-singular matrix" (sic). Nothing calls them.
 */
#include "n64gfx.h"

/* 0x80087420: vsSin - from a quarter-wave table; the exact quarter angles are special-cased */
f32 vsSin(s32 angle) {
    u32 a = (u32)angle & 0xFFFF;
    u32 i;

    if (a == 0x4000) return 1.0f;
    if ((s32)a < 0x4001) {
        if (a == 0) return 0.0f;
    } else {
        if (a == 0x8000) return 0.0f;
        if (a == 0xC000) return -1.0f;
    }
    if (a < 0x4000) return D_800B0D90[a];
    if (a - 0x4000 < 0x4000) return D_800B0D90[(0x4000 - a) & 0x3FFF];
    if (a - 0x8000 < 0x4000) i = a & 0x3FFF;
    else                     i = (0x4000 - a) & 0x3FFF;
    return (f32)(0.0 - (f64)D_800B0D90[i]);
}

/* 0x80087520: vsCos - vsSin a quarter turn on */
f32 vsCos(s32 angle) {
    u32 a = ((u32)angle + 0x4000) & 0xFFFF;
    u32 i;

    if (a == 0x4000) return 1.0f;
    if ((s32)a < 0x4001) {
        if (a == 0) return 0.0f;
    } else {
        if (a == 0x8000) return 0.0f;
        if (a == 0xC000) return -1.0f;
    }
    if (a < 0x4000) return D_800B0D90[a];
    if (a - 0x4000 < 0x4000) return D_800B0D90[(0x4000 - a) & 0x3FFF];
    if (a - 0x8000 < 0x4000) i = a & 0x3FFF;
    else                     i = (0x4000 - a) & 0x3FFF;
    return (f32)(0.0 - (f64)D_800B0D90[i]);
}

/* 0x80087630: vsVec2_Length */
f32 func_80087630(f32 *v) {
    return __builtin_sqrtf(v[0] * v[0] + v[1] * v[1]);
}

/* 0x80087674: vsVec2_Normalise */
void func_80087674(f32 *v) {
    f32 len = func_80087630(v);
    v[0] = v[0] / len;
    v[1] = v[1] / len;
}

/* 0x800876B0: vsMatrix2D_Identity */
void func_800876B0(vsMatrix2D *m) {
    m->m[0][0] = 1.0f; m->m[0][1] = 0.0f; m->m[0][2] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = 1.0f; m->m[1][2] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f;
}

/* 0x800876E4: vsMatrix2D_Copy */
void func_800876E4(vsMatrix2D *dst, vsMatrix2D *src) {
    bcopy(src, dst, sizeof(vsMatrix2D));
}

/* 0x8008770C: vsMatrix2D_SetScale */
void func_8008770C(vsMatrix2D *m, f32 sx, f32 sy) {
    m->m[0][0] = sx;   m->m[0][1] = 0.0f; m->m[0][2] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = sy;   m->m[1][2] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f;
}

/* 0x80087740: vsMatrix2D_SetTranslate */
void func_80087740(vsMatrix2D *m, f32 x, f32 y) {
    m->m[0][0] = 1.0f; m->m[0][1] = 0.0f; m->m[0][2] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = 1.0f; m->m[1][2] = 0.0f;
    m->m[2][0] = x;    m->m[2][1] = y;    m->m[2][2] = 1.0f;
}

/* 0x80087774: vsMatrix2D_SetRotate */
void func_80087774(vsMatrix2D *m, s32 angle) {
    f32 s = vsSin(angle);
    f32 c = vsCos(angle);
    m->m[0][0] = c;    m->m[0][1] = -s;   m->m[0][2] = 0.0f;
    m->m[1][0] = s;    m->m[1][1] = c;    m->m[1][2] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f;
}

/* 0x80087AD8: vsMatrix2D_Concat - out = a * b */
void func_80087AD8(vsMatrix2D *a, vsMatrix2D *b, vsMatrix2D *out) {
    s32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            out->m[r][c] = a->m[r][0] * b->m[0][c] + a->m[r][1] * b->m[1][c] + a->m[r][2] * b->m[2][c];
        }
    }
}

/* 0x800877F0: vsMatrix2D_Multiply - a = a * b */
void func_800877F0(vsMatrix2D *a, vsMatrix2D *b) {
    vsMatrix2D t;
    func_80087AD8(a, b, &t);
    func_800876E4(a, &t);
}

/* 0x80087824: vsMatrix2D_Scale */
void func_80087824(vsMatrix2D *m, f32 sx, f32 sy) {
    vsMatrix2D s, t;
    func_8008770C(&s, sx, sy);
    func_80087AD8(m, &s, &t);
    func_800876E4(m, &t);
}

/* 0x80087874: vsMatrix2D_Translate */
void func_80087874(vsMatrix2D *m, f32 x, f32 y) {
    vsMatrix2D s, t;
    func_80087740(&s, x, y);
    func_80087AD8(m, &s, &t);
    func_800876E4(m, &t);
}

/* 0x800878C4: vsMatrix2D_Rotate */
void func_800878C4(vsMatrix2D *m, s32 angle) {
    vsMatrix2D s, t;
    func_80087774(&s, angle);
    func_80087AD8(m, &s, &t);
    func_800876E4(m, &t);
}

/* 0x80087914: vsMatrix2D_PreMultiply - a = b * a */
void func_80087914(vsMatrix2D *a, vsMatrix2D *b) {
    vsMatrix2D t;
    func_80087AD8(b, a, &t);
    func_800876E4(a, &t);
}

/* 0x80087950: vsMatrix2D_PreScale */
void func_80087950(vsMatrix2D *m, f32 sx, f32 sy) {
    vsMatrix2D s, t;
    func_8008770C(&s, sx, sy);
    func_80087AD8(&s, m, &t);
    func_800876E4(m, &t);
}

/* 0x800879A0: vsMatrix2D_PreTranslate */
void func_800879A0(vsMatrix2D *m, f32 x, f32 y) {
    vsMatrix2D s, t;
    func_80087740(&s, x, y);
    func_80087AD8(&s, m, &t);
    func_800876E4(m, &t);
}

/* 0x800879F0: vsMatrix2D_PreRotate */
void func_800879F0(vsMatrix2D *m, s32 angle) {
    vsMatrix2D s, t;
    func_80087774(&s, angle);
    func_80087AD8(&s, m, &t);
    func_800876E4(m, &t);
}

/* 0x80087A40: vsMatrix2D_Determinant - never written */
f32 func_80087A40(vsMatrix2D *m) {
    (void)m;
    return 0.0f;
}

/* 0x80087C8C: vsMatrix2D_Transpose - never written, gives the identity */
void func_80087C8C(vsMatrix2D *src, vsMatrix2D *dst) {
    (void)src;
    func_800876B0(dst);
}

/* 0x80087A4C: vsMatrix2D_TransposeSelf */
void func_80087A4C(vsMatrix2D *m) {
    vsMatrix2D t;
    func_800876E4(&t, m);
    func_80087C8C(&t, m);
}

/* 0x80087A84: vsMatrix2D_InvertSelf */
void func_80087A84(vsMatrix2D *m) {
    vsMatrix2D t;
    func_800876E4(&t, m);
    vsMatrix2DInvert(&t, m);
}

/* 0x80087ABC: vsMatrix2D_Init */
void func_80087ABC(vsMatrix2D *m) {
    func_800876B0(m);
}

/* 0x80087CA8 */
void vsMatrix2DInvert(vsMatrix2D *src, vsMatrix2D *dst) {
    f32 det;
    s32 i;
    func_80087C8C(src, dst);
    det = func_80087A40(dst);
    if ((f64)det == 0.0) mpFatalError("VSLMTX2D.C : vsMatrix2DInvert() - Attempt to invert non-singular matrix");
    for (i = 0; i < 9; i++) (&dst->m[0][0])[i] = (&dst->m[0][0])[i] / det;
}

/* 0x80087D80: vsVec3_Length */
f32 func_80087D80(f32 *v) {
    return __builtin_sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* 0x80087DD0: vsVec3_Normalise */
void func_80087DD0(f32 *v) {
    f32 len = func_80087D80(v);
    v[0] = v[0] / len;
    v[1] = v[1] / len;
    v[2] = v[2] / len;
}
