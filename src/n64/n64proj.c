/*
 * N64PROJ.C (0x8007FF20-0x80080360): the 3D projection. The perspective matrix
 * (B_800F7790, fixed-point copy B_800F87B8), the camera (B_800F5AA8) and their product
 * (B_800F4E98) used to project points to the screen, and the model matrix (B_800F7DB8).
 * Matrices for the RSP go into this frame's matrix buffer B_800F5B6C.
 *
 * Oddity kept from the original: func_8008004C and func_80080170 write their G_MTX
 * command to the global gDL rather than to the list they were given.
 */
#include "n64gfx.h"

/* 0x8007FF20: Proj_SetPerspective - field of view fovy (degrees), 4:3 */
void func_8007FF20(s32 fovy, f32 nearZ, f32 farZ) {
    func_800973C0(B_800F7790, &B_800F5BC0, (f32)fovy, 1.3333334f, nearZ, farZ, 1.0f);
    func_80096D30(B_800F7790, B_800F87B8);
    B_800F77D4 = (f32)((f64)(f32)(s32)B_800F5BC0 / 65536.0);
}

/* 0x8007FFB8: Proj_Load - the perspective as the projection matrix, and its perspNorm */
void func_8007FFB8(u32 **dl) {
    bcopy(B_800F87B8, B_800F5B6C, 0x40);
    DL_PUSH(dl, 0x01030040, K0_TO_PHYS(B_800F5B6C));     /* G_MTX projection, load */
    B_800F5B6C += 0x40;
    DL_PUSH(dl, 0xBC00000E, (u32)B_800F5BC0);            /* G_MW_PERSPNORM */
}

/* 0x8008004C: Proj_SetCamera - camera matrix m: projection = camera * perspective */
void func_8008004C(u32 **dl, void *m) {
    u32 *p;

    bcopy(m, B_800F5AA8, 0x40);
    func_80096FB0(B_800F5AA8, B_800F7790, B_800F4E98);
    func_8007FFB8(dl);
    func_80096D30(m, B_800F5B6C);
    p = gDL; gDL = p + 2;                                /* (gDL, not dl) */
    p[0] = 0x01010040;                                   /* G_MTX projection, multiply */
    p[1] = K0_TO_PHYS(B_800F5B6C);
    B_800F5B6C += 0x40;
}

/* 0x80080100: Proj_Reload - the perspective and the current camera again */
void func_80080100(u32 **dl) {
    func_8007FFB8(dl);
    func_80096D30(B_800F5AA8, B_800F5B6C);
    DL_PUSH(dl, 0x01010040, K0_TO_PHYS(B_800F5B6C));
    B_800F5B6C += 0x40;
}

/* 0x80080170: Proj_SetModel - the model view matrix */
void func_80080170(u32 **dl, void *m) {
    u32 *p;

    (void)dl;
    bcopy(m, B_800F7DB8, 0x40);
    func_80096D30(m, B_800F5B6C);
    p = gDL; gDL = p + 2;                                /* (gDL, not dl) */
    p[0] = 0x01020040;                                   /* G_MTX model view, load */
    p[1] = K0_TO_PHYS(B_800F5B6C);
    B_800F5B6C += 0x40;
}

/* 0x800801F4: Proj_Point - world point in -> screen x, y (pixels, y down) and depth.
 * w == 0 gives (0, 0, -1). Behind the camera (w < 0) the original multiplies by w instead
 * of dividing. */
void func_800801F4(f32 *in, f32 *out) {
    f32 (*m)[4] = B_800F4E98;
    f32 x = in[0], y = in[1], z = in[2];
    f32 sx = m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0];
    f32 sy = m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1];
    f32 sz = m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2];
    f32 w  = m[0][3] * x + m[1][3] * y + m[2][3] * z + m[3][3];
    f32 px, py;

    if (w == 0.0f) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = -1.0f;
        return;
    }
    if (0.0f < w) {
        px = sx / w;
        py = sy / w;
    } else {
        px = sx * w;
        py = sy * w;
    }
    out[0] = px * B_800F5BC8[0] + B_800F7E00[0];
    out[1] = (f32)B_800F5BEC - (py * B_800F5BC8[1] + B_800F7E00[1]);
    out[2] = sz;
}
