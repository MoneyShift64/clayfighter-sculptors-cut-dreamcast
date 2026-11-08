/*
 * VSLMTX.C (0x80087E20-0x80089790): the VS library's 4x4 float matrices and lights.
 *
 * Row vectors: [x y z 1] * M, so the translation sits in the bottom row (m[3][0..2]).
 * "Multiply" functions apply the new transform after the existing one (M = M * T),
 * "PreMultiply" ones before it (M = T * M). Angles: 0x10000 = a full turn.
 * The camera is a matrix too: func_80088654 builds one looking from an eye point at a target.
 */
#include "n64gfx.h"
#ifdef __sh__
#include <sh4zam/shz_sh4zam.h>     /* the SH-4's matrix unit (FTRV) */
#endif

#define M(p) (((vsMatrix *)(p))->m)

/* 0x80087E20: vsMatrix_Identity */
void func_80087E20(void *mp) {
    f32 (*m)[4] = M(mp);
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

/* 0x80087E70: vsMatrix_Copy (dst, src) */
void func_80087E70(void *dst, void *src) {
    bcopy(src, dst, sizeof(vsMatrix));
}

/* 0x80087E98: vsMatrix_SetScale */
void func_80087E98(void *mp, f32 x, f32 y, f32 z) {
    f32 (*m)[4] = M(mp);
    m[0][0] = x;    m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = y;    m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = z;    m[2][3] = 0.0f;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

/* 0x80087EE8: vsMatrix_SetTranslate */
void func_80087EE8(void *mp, f32 x, f32 y, f32 z) {
    f32 (*m)[4] = M(mp);
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
    m[3][0] = x;    m[3][1] = y;    m[3][2] = z;    m[3][3] = 1.0f;
}

/* 0x80087F38: vsMatrix_SetRotateX */
void func_80087F38(void *mp, s32 angle) {
    f32 (*m)[4] = M(mp);
    f32 s = vsSin(angle);
    f32 c = vsCos(angle);
    m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = c;    m[1][2] = s;    m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = -s;   m[2][2] = c;    m[2][3] = 0.0f;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

/* 0x80087FD0: vsMatrix_SetRotateY */
void func_80087FD0(void *mp, s32 angle) {
    f32 (*m)[4] = M(mp);
    f32 s = vsSin(angle);
    f32 c = vsCos(angle);
    m[0][0] = c;    m[0][1] = 0.0f; m[0][2] = -s;   m[0][3] = 0.0f;
    m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = s;    m[2][1] = 0.0f; m[2][2] = c;    m[2][3] = 0.0f;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

/* 0x80088068: vsMatrix_SetRotateZ */
void func_80088068(void *mp, s32 angle) {
    f32 (*m)[4] = M(mp);
    f32 s = vsSin(angle);
    f32 c = vsCos(angle);
    m[0][0] = c;    m[0][1] = s;    m[0][2] = 0.0f; m[0][3] = 0.0f;
    m[1][0] = -s;   m[1][1] = c;    m[1][2] = 0.0f; m[1][3] = 0.0f;
    m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
    m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
}

/* 0x800887EC: vsMatrix_Concat - out = a * b */
void func_800887EC(vsMatrix *a, vsMatrix *b, vsMatrix *out) {
    s32 r, c;
#ifdef __sh__
    /* each row of a through b (b's rows loaded as XMTRX's columns) */
    f32 t[4][4];
    shz_xmtrx_load_unaligned_4x4(&b->m[0][0]);
    for (r = 0; r < 4; r++) {
        shz_vec4_t v = shz_xmtrx_transform_vec4((shz_vec4_t){ .x = a->m[r][0], .y = a->m[r][1], .z = a->m[r][2], .w = a->m[r][3] });
        t[r][0] = v.x; t[r][1] = v.y; t[r][2] = v.z; t[r][3] = v.w;
    }
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) out->m[r][c] = t[r][c];
    return;
#endif
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            out->m[r][c] = a->m[r][0] * b->m[0][c] + a->m[r][1] * b->m[1][c] + a->m[r][2] * b->m[2][c]
                         + a->m[r][3] * b->m[3][c];
        }
    }
}

/* 0x80088100: vsMatrix_Multiply - m = m * b */
void func_80088100(void *m, void *b) {
    vsMatrix t;
    func_800887EC(m, b, &t);
    func_80087E70(m, &t);
}

/* 0x80088134: vsMatrix_Scale */
void func_80088134(void *m, f32 x, f32 y, f32 z) {
    vsMatrix s, t;
    func_80087E98(&s, x, y, z);
    func_800887EC(m, &s, &t);
    func_80087E70(m, &t);
}

/* 0x80088184: vsMatrix_Translate */
void func_80088184(void *m, f32 x, f32 y, f32 z) {
    vsMatrix s, t;
    func_80087EE8(&s, x, y, z);
    func_800887EC(m, &s, &t);
    func_80087E70(m, &t);
}

/* 0x800881D4: vsMatrix_RotateX */
void func_800881D4(void *m, s32 angle) {
    vsMatrix s, t;
    func_80087F38(&s, angle);
    func_800887EC(m, &s, &t);
    func_80087E70(m, &t);
}

/* 0x80088224: vsMatrix_RotateY */
void func_80088224(void *m, s32 angle) {
    vsMatrix s, t;
    func_80087FD0(&s, angle);
    func_800887EC(m, &s, &t);
    func_80087E70(m, &t);
}

/* 0x80088274: vsMatrix_RotateZ */
void func_80088274(void *m, s32 angle) {
    vsMatrix s, t;
    func_80088068(&s, angle);
    func_800887EC(m, &s, &t);
    func_80087E70(m, &t);
}

/* 0x800882C4: vsMatrix_PreMultiply - m = b * m */
void func_800882C4(void *m, void *b) {
    vsMatrix t;
    func_800887EC(b, m, &t);
    func_80087E70(m, &t);
}

/* 0x80088300: vsMatrix_PreScale */
void func_80088300(void *m, f32 x, f32 y, f32 z) {
    vsMatrix s, t;
    func_80087E98(&s, x, y, z);
    func_800887EC(&s, m, &t);
    func_80087E70(m, &t);
}

/* 0x80088350: vsMatrix_PreTranslate (moves the camera) */
void func_80088350(void *m, f32 x, f32 y, f32 z) {
    vsMatrix s, t;
    func_80087EE8(&s, x, y, z);
    func_800887EC(&s, m, &t);
    func_80087E70(m, &t);
}

/* 0x800883A0: vsMatrix_PreRotateX */
void func_800883A0(void *m, s32 angle) {
    vsMatrix s, t;
    func_80087F38(&s, angle);
    func_800887EC(&s, m, &t);
    func_80087E70(m, &t);
}

/* 0x800883F0: vsMatrix_PreRotateY */
void func_800883F0(void *m, s32 angle) {
    vsMatrix s, t;
    func_80087FD0(&s, angle);
    func_800887EC(&s, m, &t);
    func_80087E70(m, &t);
}

/* 0x80088440: vsMatrix_PreRotateZ */
void func_80088440(void *m, s32 angle) {
    vsMatrix s, t;
    func_80088068(&s, angle);
    func_800887EC(&s, m, &t);
    func_80087E70(m, &t);
}

/* 0x80088490: vsMatrix_Determinant (cofactors along the first row) */
f32 func_80088490(vsMatrix *mp) {
    f32 *m = &mp->m[0][0];
    f32 a = m[10] * m[15] - m[11] * m[14];
    f32 b = m[9] * m[15] - m[11] * m[13];
    f32 c = m[9] * m[14] - m[10] * m[13];
    f32 d = m[8] * m[15] - m[11] * m[12];
    f32 e = m[8] * m[14] - m[10] * m[12];
    f32 f = m[8] * m[13] - m[9] * m[12];
    f32 t0 = ((a * m[5] - b * m[6]) + c * m[7]) * m[0];
    f32 t1 = ((a * m[4] - d * m[6]) + e * m[7]) * m[1];
    f32 t2 = ((b * m[4] - d * m[5]) + f * m[7]) * m[2];
    f32 t3 = ((c * m[4] - e * m[5]) + f * m[6]) * m[3];
    return ((t0 - t1) + t2) - t3;
}

/* one 3x3 minor of func_80088BF0, in the original's order of operations */
static inline f32 Minor3(f32 a, f32 b, f32 c, f32 d, f32 e, f32 g, f32 h, f32 i, f32 j) {
    return ((a * b - c * d) * e - (g * b - c * h) * i) + (g * d - a * h) * j;
}
#define MINOR(s, A, B, C, D, E, G, H, I, J) \
    Minor3(s[A], s[B], s[C], s[D], s[E], s[G], s[H], s[I], s[J])

/* 0x80088BF0: vsMatrix_Adjugate - dst = transposed cofactors of src (inverse * determinant) */
void func_80088BF0(vsMatrix *src, vsMatrix *dst) {
    f32 *s = &src->m[0][0];
    f32 *d = &dst->m[0][0];
    d[0]  =        MINOR(s, 10, 15, 11, 14, 5, 9, 13, 6, 7);
    d[4]  = 0.0f - MINOR(s, 10, 15, 11, 14, 4, 8, 12, 6, 7);
    d[8]  =        MINOR(s, 9, 15, 11, 13, 4, 8, 12, 5, 7);
    d[12] = 0.0f - MINOR(s, 9, 14, 10, 13, 4, 8, 12, 5, 6);
    d[1]  = 0.0f - MINOR(s, 10, 15, 11, 14, 1, 9, 13, 2, 3);
    d[5]  =        MINOR(s, 10, 15, 11, 14, 0, 8, 12, 2, 3);
    d[9]  = 0.0f - MINOR(s, 9, 15, 11, 13, 0, 8, 12, 1, 3);
    d[13] =        MINOR(s, 9, 14, 10, 13, 0, 8, 12, 1, 2);
    d[2]  =        MINOR(s, 6, 15, 7, 14, 1, 5, 13, 2, 3);
    d[6]  = 0.0f - MINOR(s, 6, 15, 7, 14, 0, 4, 12, 2, 3);
    d[10] =        MINOR(s, 5, 15, 7, 13, 0, 4, 12, 1, 3);
    d[14] = 0.0f - MINOR(s, 5, 14, 6, 13, 0, 4, 12, 1, 2);
    d[3]  = 0.0f - MINOR(s, 6, 11, 7, 10, 1, 5, 9, 2, 3);
    d[7]  =        MINOR(s, 6, 11, 7, 10, 0, 4, 8, 2, 3);
    d[11] = 0.0f - MINOR(s, 5, 11, 7, 9, 0, 4, 8, 1, 3);
    d[15] =        MINOR(s, 5, 10, 6, 9, 0, 4, 8, 1, 2);
}

/* 0x800885E4: vsMatrix_AdjugateSelf */
void func_800885E4(vsMatrix *m) {
    vsMatrix t;
    func_80087E70(&t, m);
    func_80088BF0(&t, m);
}

/* 0x8008861C: vsMatrix_InvertSelf */
void func_8008861C(void *m) {
    vsMatrix t;
    func_80087E70(&t, m);
    vsMatrixInvert(&t, m);
}

/* 0x80088654: vsMatrix_LookAt - a camera at eye looking at at, y up (screen y is down) */
void func_80088654(void *mp, f32 *eye, f32 *at) {
    f32 (*m)[4] = M(mp);
    f32 z[3], x[3], side[3], y[3];
    /* (volatile: keeps the compiler from folding the zeros and negations, which changes
       the sign of NaNs when eye == at) */
    volatile f32 up0, nx0, nx2;

    z[0] = eye[0]; z[1] = eye[1]; z[2] = eye[2];
    z[0] = z[0] - at[0];
    z[1] = z[1] - at[1];
    z[2] = z[2] - at[2];
    func_80087DD0(z);
    x[0] = z[0]; x[1] = 0.0f; x[2] = z[2];          /* the view direction, flat */
    func_80087DD0(x);
    up0 = 0.0f;                                      /* up = (0, -1, 0) */
    /* side = x cross up (the original folded the -1s into negations) */
    nx2 = -x[2];
    side[0] = x[1] * up0 - nx2;
    side[1] = x[2] * up0 - x[0] * up0;
    nx0 = -x[0];
    side[2] = nx0 - x[1] * up0;
    /* y = z cross side */
    y[0] = z[1] * side[2] - z[2] * side[1];
    y[1] = z[2] * side[0] - z[0] * side[2];
    y[2] = z[0] * side[1] - z[1] * side[0];
    m[0][0] = side[0]; m[0][1] = side[1]; m[0][2] = side[2]; m[0][3] = up0;
    m[1][0] = y[0];    m[1][1] = y[1];    m[1][2] = y[2];    m[1][3] = up0;
    m[2][0] = z[0];    m[2][1] = z[1];    m[2][2] = z[2];    m[2][3] = up0;
    m[3][0] = eye[0];  m[3][1] = eye[1];  m[3][2] = eye[2];  m[3][3] = 1.0f;
}

/* 0x8008925C */
void vsMatrixInvert(vsMatrix *src, vsMatrix *dst) {
    f32 det;
    s32 i;
    func_80088BF0(src, dst);
    det = func_80088490(dst);
    if ((f64)det == 0.0) mpFatalError("VSLMTX.C : vsMatrixInvert() - Attempt to invert non-singular matrix");
    for (i = 0; i < 16; i++) (&dst->m[0][0])[i] = (&dst->m[0][0])[i] / det;
}

/* 0x80089380 (no symbol; nothing calls it): vsLight_Init - a default ambient light */
void vsLight_Init(vsLight *l) {
    l->type = 0;
    l->colour[0] = 0.5f; l->colour[1] = 0.5f; l->colour[2] = 0.5f;
    l->pos[0] = 0.0f; l->pos[1] = 0.0f; l->pos[2] = 0.0f;
    l->dir[0] = 0.0f; l->dir[1] = 1.0f; l->dir[2] = 0.0f;
    l->intensity = 0.5f;
    l->range = 1.0f;
    l->v30 = 0.0f;
    l->coneOuter = 5461.0f;      /* 30 degrees */
    l->coneInner = 910.0f;       /* 5 degrees */
}

/* 0x800893F8: vsLight_Shade - the colour light l gives a point pos with surface normal
 * normal (NULL: facing the light), into *r, *g, *b */
void func_800893F8(void *lp, f32 *pos, f32 *normal, f32 *r, f32 *g, f32 *b) {
    vsLight *l = lp;
    f32 k = 0.0f;
    f32 d[3], len, dot, n, fall;

    switch (l->type) {
    case 0:
        k = l->intensity;
        break;
    case 1:
    case 3:
        d[0] = pos[0]; d[1] = pos[1]; d[2] = pos[2];
        d[0] = d[0] - l->pos[0];
        d[1] = d[1] - l->pos[1];
        d[2] = d[2] - l->pos[2];
        func_80087DD0(d);
        dot = d[0] * l->dir[0] + d[1] * l->dir[1] + d[2] * l->dir[2];
        if (!(0.0 < (f64)dot)) break;
        n = (normal != NULL) ? d[0] * normal[0] + d[1] * normal[1] + d[2] * normal[2] : -1.0f;
        if (!((f64)n < 0.0)) break;
        k = (f32)(-(f64)dot * (f64)n);
        k = k * l->intensity;
        break;
    case 2:
        if (!(0.0 < (f64)l->range)) break;
        d[0] = pos[0]; d[1] = pos[1]; d[2] = pos[2];
        d[0] = d[0] - l->pos[0];
        d[1] = d[1] - l->pos[1];
        d[2] = d[2] - l->pos[2];
        len = func_80087D80(d);
        d[0] = d[0] / len;
        d[1] = d[1] / len;
        d[2] = d[2] / len;
        n = (normal != NULL) ? d[0] * normal[0] + d[1] * normal[1] + d[2] * normal[2] : -1.0f;
        if (!((f64)n < 0.0)) break;
        k = (f32)-(f64)n;
        fall = (f32)(1.0 - (f64)(len / l->range));
        if ((f64)fall < 0.0) {
            k = 0.0f;
            break;
        }
        k = k * fall;
        k = k * l->intensity;
        break;
    }
    *r = k * l->colour[0];
    *g = k * l->colour[1];
    *b = k * l->colour[2];
}
