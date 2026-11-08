/*
 * Curves (0x8005D340-0x8005DDC0): quadratic B-splines through a list of 3D control points,
 * used for the camera's fly-through paths at the start of a fight (and by VS.C). A parameter t
 * from 0 to 1 runs along the whole curve; "wrap" makes t loop round instead of stopping at the
 * ends. The knot vector is uniform and clamped, so the curve starts and ends on the end points.
 */
#include "common.h"
#include "mpfmem.h"
#include "cf3char.h"

/* 0x8005DC40: clamped uniform knot vector for control points 0..n, order k (n + k + 1 knots) */
void Spline_Knots(s32 *knots, s32 n, s32 k) {
    s32 j;
    for (j = 0; j <= n + k; j++) {
        if (j < k)                 knots[j] = 0;
        else if (k <= j && j <= n) knots[j] = j - k + 1;
        else if (n < j)            knots[j] = n - k + 2;
    }
}

/* 0x8005D9E4: the B-spline basis function N(i, k) at u (Cox-de Boor) */
f32 Spline_Basis(s32 i, s32 k, s32 *knots, f32 u) {
    if (k == 1) {
        if ((f32)knots[i] <= u && u < (f32)knots[i + 1]) return 1.0f;
        return 0.0f;
    }
    if (knots[i + k - 1] == knots[i]) {
        if (knots[i + k] == knots[i + 1]) return 0.0f;
        return ((f32)knots[i + k] - u) / (f32)(knots[i + k] - knots[i + 1]) * Spline_Basis(i + 1, k - 1, knots, u);
    }
    if (knots[i + k] == knots[i + 1]) {
        return (u - (f32)knots[i]) / (f32)(knots[i + k - 1] - knots[i]) * Spline_Basis(i, k - 1, knots, u);
    }
    {
        f32 a = Spline_Basis(i, k - 1, knots, u);
        f32 b = Spline_Basis(i + 1, k - 1, knots, u);
        return (u - (f32)knots[i]) / (f32)(knots[i + k - 1] - knots[i]) * a +
               ((f32)knots[i + k] - u) / (f32)(knots[i + k] - knots[i + 1]) * b;
    }
}

/* 0x8005DCCC: the point at u: sum of N(i, k)(u) * pts[i] for i = 0..n */
void Spline_Eval(s32 *knots, s32 n, s32 k, f32 u, f32 *pts, f32 *out) {
    s32 i;
    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    for (i = 0; i <= n; i++) {
        f32 b = Spline_Basis(i, k, knots, u);
        out[0] = b * pts[i * 3 + 0] + out[0];
        out[1] = b * pts[i * 3 + 1] + out[1];
        out[2] = b * pts[i * 3 + 2] + out[2];
    }
}

/* t looped into 0..1 (wrap) or clamped to it */
static f32 FixT(f32 t, s32 wrap) {
    if (wrap) {
        if (1.0 < (f64)t) {
            do t = (f32)((f64)t - 1.0); while (1.0 < (f64)t);
        }
        if ((f64)t < 0.0) {
            do t = (f32)((f64)t + 1.0); while ((f64)t < 0.0);
        }
    } else {
        if (1.0 < (f64)t) t = 1.0f;
        if ((f64)t < 0.0) t = 0.0f;
    }
    return t;
}

static void PointAt(s32 *knots, f32 *pts, s32 m, f32 t, f32 *out) {
    f32 u = (f32)(m - 1) * t;
    if ((f64)t == 1.0) {
        out[0] = pts[m * 3 + 0];
        out[1] = pts[m * 3 + 1];
        out[2] = pts[m * 3 + 2];
    } else {
        Spline_Eval(knots, m, 3, u, pts, out);
    }
}

/* 0x8005D340: the point at t on the curve through n control points */
void Spline_Point(f32 *pts, s32 n, f32 t, f32 *out, s32 wrap) {
    s32 m, *knots;
    t = FixT(t, wrap);
    m = n - 1;
    knots = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, (m + 4) * 4, 0);
    Spline_Knots(knots, m, 3);
    PointAt(knots, pts, m, t, out);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, knots);
}

/* 0x8005D540: t moved on so the point moves about 'step' units along the curve (to within
 * 0.1, by repeatedly rescaling a trial step that starts at 0.1) */
f32 Spline_Advance(f32 *pts, s32 n, f32 t, f32 step, s32 wrap) {
    s32 m, *knots;
    f32 dt, t2, len, diff, p0[3], p1[3], d[3];

    if (!wrap && 1.0 < (f64)t) return 1.0f;
    t = FixT(t, wrap);
    m = n - 1;
    knots = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, (m + 4) * 4, 0);
    Spline_Knots(knots, m, 3);
    PointAt(knots, pts, m, t, p0);
    dt = 0.1f;
    for (;;) {
        t2 = FixT(t + dt, wrap);
        PointAt(knots, pts, m, t2, p1);
        d[0] = p1[0] - p0[0];
        d[1] = p1[1] - p0[1];
        d[2] = p1[2] - p0[2];
        len = cfVec3_Length(d);
        diff = step - len;
        if ((f64)diff < 0.1 && -0.1 < (f64)diff) {
            mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, knots);
            return t2;
        }
        if (0.0 < (f64)diff && !wrap && (f64)t2 == 1.0) {
            mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, knots);
            return 1.0f;
        }
        dt = dt * (step / len);
    }
}

/* 0x80076A50 (its own little file between CF3SEQ.C and MPFPLAT.C): hands out thread ids -
 * the game loop, the video set-up, the combat screen and the graphics scheduler all use it
 * before osCreateThread */
extern s32 D_800B0970;
s32 n64NextThreadId(void) {
    return D_800B0970++;
}
