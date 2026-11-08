/*
 * CF3MTX2D.C (0x80074ED0-0x800755D0): 3x3 matrices for the fight code's 2D maths (the
 * projectiles' transforms). Row vectors: [x y 1] * M, so the translation sits in the bottom
 * row (m[2][0], m[2][1]). "Multiply" functions apply the new transform after the existing one
 * (M = M * T); "PreMultiply" ones apply it before (M = T * M).
 *
 * The transpose, determinant and inverse were never written: Transpose just sets the result
 * to the identity, Determinant returns 0, and Invert therefore always stops the game with
 * "Attempt to invert non-singular matrix" (sic). Nothing calls them in a normal game.
 */
#include "common.h"
#include "cf3char.h"


/* 0x80074ED0 */
void cfMatrix2D_Identity(Matrix2D *m) {
    m->m[0][0] = 1.0f; m->m[0][1] = 0.0f; m->m[0][2] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = 1.0f; m->m[1][2] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f;
}

/* 0x80074F04 */
void cfMatrix2D_Copy(Matrix2D *dst, Matrix2D *src) {
    bcopy(src, dst, sizeof(Matrix2D));
}

/* 0x80074F2C */
void cfMatrix2D_SetScale(Matrix2D *m, f32 sx, f32 sy) {
    m->m[0][0] = sx;   m->m[0][1] = 0.0f; m->m[0][2] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = sy;   m->m[1][2] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f;
}

/* 0x80074F60 */
void cfMatrix2D_SetTranslate(Matrix2D *m, f32 x, f32 y) {
    m->m[0][0] = 1.0f; m->m[0][1] = 0.0f; m->m[0][2] = 0.0f;
    m->m[1][0] = 0.0f; m->m[1][1] = 1.0f; m->m[1][2] = 0.0f;
    m->m[2][0] = x;    m->m[2][1] = y;    m->m[2][2] = 1.0f;
}

/* 0x80074F94: angle in degrees */
void cfMatrix2D_SetRotate(Matrix2D *m, f32 angle) {
    s32 a = (s32)((f64)(angle * 65536.0f) / 360.0);
    f32 s = vsSin(a);
    f32 c = vsCos(a);
    m->m[0][0] = c;    m->m[0][1] = -s;   m->m[0][2] = 0.0f;
    m->m[1][0] = s;    m->m[1][1] = c;    m->m[1][2] = 0.0f;
    m->m[2][0] = 0.0f; m->m[2][1] = 0.0f; m->m[2][2] = 1.0f;
}

/* 0x80075324: out = a * b */
void cfMatrix2D_Concat(Matrix2D *a, Matrix2D *b, Matrix2D *out) {
    s32 r, c;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            out->m[r][c] = a->m[r][0] * b->m[0][c] + a->m[r][1] * b->m[1][c] + a->m[r][2] * b->m[2][c];
        }
    }
}

/* 0x8007503C: a = a * b */
void cfMatrix2D_Multiply(Matrix2D *a, Matrix2D *b) {
    Matrix2D t;
    cfMatrix2D_Concat(a, b, &t);
    cfMatrix2D_Copy(a, &t);
}

/* 0x80075070 */
void cfMatrix2D_Scale(Matrix2D *m, f32 sx, f32 sy) {
    Matrix2D s, t;
    cfMatrix2D_SetScale(&s, sx, sy);
    cfMatrix2D_Concat(m, &s, &t);
    cfMatrix2D_Copy(m, &t);
}

/* 0x800750C0 */
void cfMatrix2D_Translate(Matrix2D *m, f32 x, f32 y) {
    Matrix2D s, t;
    cfMatrix2D_SetTranslate(&s, x, y);
    cfMatrix2D_Concat(m, &s, &t);
    cfMatrix2D_Copy(m, &t);
}

/* 0x80075110 */
void cfMatrix2D_Rotate(Matrix2D *m, f32 angle) {
    Matrix2D s, t;
    cfMatrix2D_SetRotate(&s, angle);
    cfMatrix2D_Concat(m, &s, &t);
    cfMatrix2D_Copy(m, &t);
}

/* 0x80075160: a = b * a */
void cfMatrix2D_PreMultiply(Matrix2D *a, Matrix2D *b) {
    Matrix2D t;
    cfMatrix2D_Concat(b, a, &t);
    cfMatrix2D_Copy(a, &t);
}

/* 0x8007519C */
void cfMatrix2D_PreScale(Matrix2D *m, f32 sx, f32 sy) {
    Matrix2D s, t;
    cfMatrix2D_SetScale(&s, sx, sy);
    cfMatrix2D_Concat(&s, m, &t);
    cfMatrix2D_Copy(m, &t);
}

/* 0x800751EC */
void cfMatrix2D_PreTranslate(Matrix2D *m, f32 x, f32 y) {
    Matrix2D s, t;
    cfMatrix2D_SetTranslate(&s, x, y);
    cfMatrix2D_Concat(&s, m, &t);
    cfMatrix2D_Copy(m, &t);
}

/* 0x8007523C */
void cfMatrix2D_PreRotate(Matrix2D *m, f32 angle) {
    Matrix2D s, t;
    cfMatrix2D_SetRotate(&s, angle);
    cfMatrix2D_Concat(&s, m, &t);
    cfMatrix2D_Copy(m, &t);
}

/* 0x8007528C: never written */
f32 cfMatrix2D_Determinant(Matrix2D *m) {
    (void)m;
    return 0.0f;
}

/* 0x800754D8: never written - gives the identity */
void cfMatrix2D_Transpose(Matrix2D *src, Matrix2D *dst) {
    (void)src;
    cfMatrix2D_Identity(dst);
}

/* 0x800754F4 */
void cfMatrix2DInvert(Matrix2D *src, Matrix2D *dst) {
    f32 det;
    s32 i;
    cfMatrix2D_Transpose(src, dst);
    det = cfMatrix2D_Determinant(dst);
    if ((f64)det == 0.0) mpFatalError("CF3MTX2D.C : cfMatrix2DInvert() - Attempt to invert non-singular matrix");
    for (i = 0; i < 9; i++) (&dst->m[0][0])[i] = (&dst->m[0][0])[i] / det;
}

/* 0x80075298 */
void cfMatrix2D_TransposeSelf(Matrix2D *m) {
    Matrix2D t;
    cfMatrix2D_Copy(&t, m);
    cfMatrix2D_Transpose(&t, m);
}

/* 0x800752D0 */
void cfMatrix2D_InvertSelf(Matrix2D *m) {
    Matrix2D t;
    cfMatrix2D_Copy(&t, m);
    cfMatrix2DInvert(&t, m);
}

/* 0x80075308 */
void cfMatrix2D_Init(Matrix2D *m) {
    cfMatrix2D_Identity(m);
}

/* ---- small vector helpers that follow (0x800755D0-0x80075A60) ---- */

/* 0x800755D0 */
f32 cfVec2_Length(f32 *v) {
    return __builtin_sqrtf(v[0] * v[0] + v[1] * v[1]);
}

/* 0x80075614 */
void cfVec2_Normalise(f32 *v) {
    f32 len = cfVec2_Length(v);
    v[0] = v[0] / len;
    v[1] = v[1] / len;
}

/* 0x80075650 */
f32 cfVec3_Length(f32 *v) {
    return __builtin_sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* 0x800756A0 */
void cfVec3_Normalise(f32 *v) {
    f32 len = cfVec3_Length(v);
    v[0] = v[0] / len;
    v[1] = v[1] / len;
    v[2] = v[2] / len;
}

/* 0x800756E8: does the line segment seg (x1,y1)-(x2,y2) cross wall (x3,y3)-(x4,y4)?
 * 0 = no, 1 = yes (the crossing point goes to out, if given), 2 = parallel. The classic
 * "faster line segment intersection" (Graphics Gems III): a bounding-box test first. */
s32 cfSegmentsIntersect(f32 *seg, f32 *wall, f32 *out) {
    f32 ax, bx, ay, by, cx, cy, d, e, f, lo, hi;

    ax = seg[2] - seg[0];
    bx = wall[0] - wall[2];
    if ((f64)ax < 0.0) { lo = seg[2]; hi = seg[0]; }
    else               { lo = seg[0]; hi = seg[2]; }
    if (0.0 < (f64)bx) {
        if (hi < wall[2] || wall[0] < lo) return 0;
    } else {
        if (hi < wall[0] || wall[2] < lo) return 0;
    }
    ay = seg[3] - seg[1];
    by = wall[1] - wall[3];
    if ((f64)ay < 0.0) { lo = seg[3]; hi = seg[1]; }
    else               { lo = seg[1]; hi = seg[3]; }
    if (0.0 < (f64)by) {
        if (hi < wall[3] || wall[1] < lo) return 0;
    } else {
        if (hi < wall[1] || wall[3] < lo) return 0;
    }
    cx = seg[0] - wall[0];
    cy = seg[1] - wall[1];
    d = by * cx - bx * cy;
    f = ay * bx - ax * by;
    if (0.0 < (f64)f) {
        if ((f64)d < 0.0 || f < d) return 0;
    } else {
        if (0.0 < (f64)d || d < f) return 0;
    }
    e = ax * cy - ay * cx;
    if (0.0 < (f64)f) {
        if ((f64)e < 0.0 || f < e) return 0;
    } else {
        if (0.0 < (f64)e || e < f) return 0;
    }
    if ((f64)f == 0.0) return 2;
    if (out == NULL) return 1;
    out[0] = d * ax / f + seg[0];
    out[1] = d * ay / f + seg[1];
    return 1;
}
