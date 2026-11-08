/*
 * Display-list helpers (0x8007E720-0x8007FF20): clearing the screen, the standard 2D and 3D
 * RDP set-ups, filled rectangles, and setters for the geometry mode / render mode / colour
 * combiner / fill colour / lighting that only emit a command when the value changes (the
 * values set are cached in B_800F5CD8, B_800F4E68+B_800F88E8, B_800F7E54, B_800F5D24 and
 * B_800F5B60).
 *
 * Oddity kept from the original: some helpers write part of their output to the global
 * display list gDL instead of the list they were given (func_8007E720, func_8007E9DC,
 * func_8007F94C). The game always passes &gDL, so it makes no difference there.
 */
#include "n64gfx.h"

/* the screen size as the RDP wants it (fill rectangle bottom right) */
#define SCREEN_FILLRECT_W0() (0xF6000000u | ((((u32)B_800F5D00 - 1) & 0x3FF) << 14) | ((((u32)B_800F5BEC - 1) & 0x3FF) << 2))
#define SET_COLOR_IMAGE_W0() (0xFF100000u | (((u32)B_800F5D00 - 1) & 0xFFF))

/* 0x8007E720: Screen_Clear - fills the frame buffer with an r, g, b colour (RGBA5551) */
void func_8007E720(u32 **dl, s32 r, s32 g, s32 b) {
    u32 c;
    u32 *p;

    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0xBA001402, 0x00300000);                 /* fill mode */
    p = gDL; gDL = p + 2;                                /* (gDL, not dl) */
    p[0] = SET_COLOR_IMAGE_W0();
    p[1] = K0_TO_PHYS(B_800F882C);
    DL_PUSH(dl, 0xB900031D, 0);                          /* render mode 0 */
    c = ((r << 8) & 0xF800) | ((g << 3) & 0x7C0) | ((b >> 2) & 0x3E);
    DL_PUSH(dl, 0xF7000000, (((c | 1) << 16) | 1) | c); /* fill colour, twice */
    DL_PUSH(dl, SCREEN_FILLRECT_W0(), 0);
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007E860: ZBuffer_Clear - fills the Z buffer with the far value, then points the colour
 * image back at the frame buffer */
void func_8007E860(u32 **dl) {
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0xBA001402, 0x00300000);                 /* fill mode */
    DL_PUSH(dl, SET_COLOR_IMAGE_W0(), K0_TO_PHYS(B_800F1334));
    DL_PUSH(dl, 0xB900031D, 0);
    DL_PUSH(dl, 0xF7000000, 0xFFFCFFFC);                 /* the far Z */
    DL_PUSH(dl, SCREEN_FILLRECT_W0(), 0);
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0xFE000000, K0_TO_PHYS(B_800F1334));     /* Z image */
    DL_PUSH(dl, SET_COLOR_IMAGE_W0(), K0_TO_PHYS(B_800F882C));
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007E9DC: SetColorImage - draw into the frame buffer again */
void func_8007E9DC(u32 **dl) {
    u32 *p;

    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    p = gDL; gDL = p + 2;                                /* (gDL, not dl) */
    p[0] = SET_COLOR_IMAGE_W0();
    p[1] = K0_TO_PHYS(B_800F882C);
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007EA50 (no symbol; nothing calls it): fills the screen in the prim depth / blend
 * colour set-up of a depth-only pass */
void n64gfx_UnusedDepthFill(u32 **dl) {
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0xBA001402, 0);
    DL_PUSH(dl, 0xF9000000, 0xFFFFFFFF);                 /* blend colour */
    DL_PUSH(dl, 0xEE000000, 0xFFFFFFFF);                 /* prim depth */
    DL_PUSH(dl, 0xB9000201, 4);                          /* Z source: primitive */
    DL_PUSH(dl, 0xB900031D, 0x0FA54040);
    DL_PUSH(dl, SCREEN_FILLRECT_W0(), 0);
}

/* the scissor command both standard set-ups use (the whole screen less 5 lines) */
static u32 Scissor_W1(void) {
    return (((u32)(s32)((f32)B_800F5D00 * 4.0f) & 0xFFF) << 12)
         | ((u32)(s32)((f32)(B_800F5BEC - 5) * 4.0f) & 0xFFF);
}

/* 0x8007EB48: Gfx_Init2D - the standard set-up for 2D drawing (no Z, 1 cycle) */
void func_8007EB48(u32 **dl) {
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0x03800010, (u32)B_800F5B48);            /* viewport */
    DL_PUSH(dl, 0xB6000000, 0x000F3205);                 /* clear geometry mode */
    DL_PUSH(dl, 0xB7000000, 0x204);                      /* set: shade, smooth */
    DL_PUSH(dl, 0xBB000000, 0xFFFFFFFF);                 /* texture off */
    DL_PUSH(dl, 0xBC000404, 2);                          /* clip ratio */
    DL_PUSH(dl, 0xBC000C04, 2);
    DL_PUSH(dl, 0xBC001404, 0xFFFE);
    DL_PUSH(dl, 0xBC001C04, 0xFFFE);
    DL_PUSH(dl, 0xBA001402, 0);                          /* 1 cycle */
    DL_PUSH(dl, 0xBA001701, 0);
    DL_PUSH(dl, 0xED000010, Scissor_W1());               /* scissor */
    DL_PUSH(dl, 0xBA001001, 0);
    DL_PUSH(dl, 0xBA000E02, 0);
    DL_PUSH(dl, 0xBA001102, 0);
    DL_PUSH(dl, 0xBA001301, 0);
    DL_PUSH(dl, 0xBA000C02, 0x2000);
    DL_PUSH(dl, 0xBA000903, 0xC00);
    DL_PUSH(dl, 0xBA000801, 0);
    DL_PUSH(dl, 0xB9000002, 0);
    DL_PUSH(dl, 0xB900031D, 0x0C084000);                 /* render mode */
    DL_PUSH(dl, 0xFCFFFFFF, 0xFFFCF279);                 /* combiner: shade */
    DL_PUSH(dl, 0xC0000000, 0);                          /* no-op */
    DL_PUSH(dl, 0xBA000602, 0x40);
    DL_PUSH(dl, 0xF7000000, 0x00010001);                 /* fill colour */
    DL_PUSH(dl, 0xF9000000, 0x01010101);                 /* blend colour */
    B_800F5CD8 = 0x204;
    B_800F4E68 = 0x0C084000;
    B_800F88E8 = 0x0C084000;
    B_800F7E54 = 2;
    B_800F5D24 = 0x00010001;
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007EEF0: Gfx_Init3D - the standard set-up for 3D drawing (Z buffer, lit, textured) */
void func_8007EEF0(u32 **dl) {
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0x03800010, (u32)B_800F5B48);            /* viewport */
    DL_PUSH(dl, 0xB6000000, 0x000F3205);
    DL_PUSH(dl, 0xB7000000, 0x2205);                     /* Z, shade, smooth, cull back */
    DL_PUSH(dl, 0xBB000001, 0xFFFFFFFF);                 /* texture on */
    DL_PUSH(dl, 0xBC000404, 2);
    DL_PUSH(dl, 0xBC000C04, 2);
    DL_PUSH(dl, 0xBC001404, 0xFFFE);
    DL_PUSH(dl, 0xBC001C04, 0xFFFE);
    DL_PUSH(dl, 0xBA001402, 0);
    DL_PUSH(dl, 0xBA001701, 0);
    DL_PUSH(dl, 0xED000010, Scissor_W1());
    DL_PUSH(dl, 0xBA001001, 0);
    DL_PUSH(dl, 0xBA000E02, 0);
    DL_PUSH(dl, 0xBA001102, 0);
    DL_PUSH(dl, 0xBA001301, 0x80000);                    /* perspective correction */
    DL_PUSH(dl, 0xBA000C02, 0x2000);
    DL_PUSH(dl, 0xBA000903, 0xC00);
    DL_PUSH(dl, 0xBA000801, 0);
    DL_PUSH(dl, 0xB9000002, 0);
    DL_PUSH(dl, 0xB900031D, 0x00442438);                 /* render mode: Z, opaque */
    DL_PUSH(dl, 0xFC121824, 0xFF33FFFF);                 /* combiner: texture * shade */
    DL_PUSH(dl, 0xC0000000, 0);
    DL_PUSH(dl, 0xBA000602, 0x40);
    DL_PUSH(dl, 0xF7000000, 0x00010001);
    DL_PUSH(dl, 0xF9000000, 0x01010101);
    B_800F5CD8 = 0x2205;
    B_800F4E68 = 0x00442038;
    B_800F88E8 = 0x00442038;
    B_800F7E54 = 2;
    B_800F5D24 = 0x00010001;
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007F2A8: Gfx_FillRect - a solid rectangle {x0, y0, x1, y1} in fill mode */
void func_8007F2A8(u32 **dl, s32 *rect, s32 r, s32 g, s32 b) {
    s32 rc[4];

    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    rc[0] = rect[0]; rc[1] = rect[1]; rc[2] = rect[2]; rc[3] = rect[3];
    DL_PUSH(dl, 0xBA001402, 0x00300000);                 /* fill mode */
    func_8007FB90(dl, 0, 0);
    func_8007FCF8(dl, r & 0xFF, g & 0xFF, b & 0xFF);
    DL_PUSH(dl, 0xF6000000u | (((u32)rc[2] & 0x3FF) << 14) | (((u32)rc[3] & 0x3FF) << 2),
                (((u32)rc[0] & 0x3FF) << 14) | (((u32)rc[1] & 0x3FF) << 2));
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007F3D4: Gfx_Rect - a rectangle {x0, y0, x1, y1} in colour r, g, b with alpha a:
 * nothing if a is 0, a fill-mode rectangle if it is 255, else a blended one */
void func_8007F3D4(u32 **dl, s32 *rect, s32 r, s32 g, s32 b, s32 a) {
    s32 rc[4];
    u32 ua = (u32)a & 0xFF;

    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    if (ua == 0) return;
    if (ua == 0xFF) {
        func_8007F2A8(dl, rect, r & 0xFF, g & 0xFF, b & 0xFF);
    }
    /* (the opaque case then draws the blended rectangle over it as well) */
    rc[0] = rect[0]; rc[1] = rect[1]; rc[2] = rect[2]; rc[3] = rect[3];
    DL_PUSH(dl, 0xBA001402, 0);                          /* 1 cycle */
    func_8007FB90(dl, 0x00404240, 0x00104240);           /* transparent surface */
    DL_PUSH(dl, 0xFA00FF00, ((u32)r << 24) | (((u32)g & 0xFF) << 16) | (((u32)b & 0xFF) << 8) | ua);
    func_8007FBE8(dl, 0);                                /* combiner: primitive colour */
    DL_PUSH(dl, 0xF6000000u | ((((u32)rc[2] + 1) & 0x3FF) << 14) | ((((u32)rc[3] + 1) & 0x3FF) << 2),
                (((u32)rc[0] & 0x3FF) << 14) | (((u32)rc[1] & 0x3FF) << 2));
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8007F578 (no symbol; nothing calls it): start of the debug line drawing - flat-shaded
 * primitive-colour set-up */
void n64gfx_DebugLinesBegin(u32 **dl) {
    D_800B0D50 = 0;
    D_800B0D54 = 0;
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0xB6000000, 0x000F3205);
    DL_PUSH(dl, 0xB900031D, 0x0C084000);
    DL_PUSH(dl, 0xFCFFFFFF, 0xFFFDF6FB);
    B_800F5CD8 = 0;
    B_800F4E68 = 0x0C084000;
    B_800F88E8 = 0x0C084000;
    B_800F7E54 = 0;
    DL_PUSH(dl, 0xFA000000, 0xFFFF00FF);                 /* yellow */
}

/* 0x8007F64C (no symbol; nothing calls it): debug line to point (x, y, z) - two vertices
 * 2 units apart, joined to the previous point with two triangles */
void n64gfx_DebugLineTo(s32 unused, f32 x, f32 y, f32 z) {
    Vtx16 *v = gVtx;
    s16 vy;
    s32 prev;
    u32 *p, w;

    (void)unused;
    v->x = (s16)(s32)((f64)x - 2.0);
    vy = (s16)(s32)y;
    v->y = vy;
    v->z = (s16)(s32)((f64)z - 2.0);
    v->flag = 0; v->s = 0; v->t = 0;
    v->r = 0xFF;
    gVtx->g = 0xFF;
    gVtx->b = 0;
    gVtx->a = 0xFF;
    v = gVtx;
    v[1].x = (s16)(s32)((f64)x + 2.0);
    v[1].y = vy;
    v[1].z = (s16)(s32)((f64)z + 2.0);
    v[1].flag = 0; v[1].s = 0; v[1].t = 0;
    v[1].r = 0xFF;
    gVtx[1].g = 0xFF;
    gVtx[1].b = 0;
    gVtx[1].a = 0xFF;
    p = gDL; gDL = p + 2;
    p[0] = ((((u32)D_800B0D54 << 1) & 0xFF) << 16) | 0x0400081F;   /* load the 2 vertices */
    p[1] = (u32)gVtx;
    gVtx = gVtx + 2;
    if (D_800B0D50++ > 0) {
        prev = D_800B0D54 - 2;
        if (prev < 0) prev = 30;
        p = gDL; gDL = p + 2;
        p[0] = 0xBF000000;
        w = ((((u32)prev << 1) & 0xFF) << 16) | (((u32)D_800B0D54 << 9) & 0xFE00);
        p[1] = w | (((u32)(prev + 1) << 1) & 0xFF);
        gDL = p + 4;
        p[2] = 0xBF000000;
        p[3] = w | (((u32)(D_800B0D54 + 1) << 1) & 0xFF);
    }
    D_800B0D54 += 2;
    if (D_800B0D54 >= 32) D_800B0D54 = 0;
}

/* 0x8007F86C (no symbol; nothing calls it): end of the debug line drawing - back to 3D */
void n64gfx_DebugLinesEnd(u32 **dl) {
    D_800B0D50 = -1;
    D_800B0D54 = 0;
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    DL_PUSH(dl, 0xB6000000, 0x000F3205);
    DL_PUSH(dl, 0xB7000000, 0x2205);
    DL_PUSH(dl, 0xB900031D, 0x00442438);
    DL_PUSH(dl, 0xFC121824, 0xFF33FFFF);
    B_800F5CD8 = 0x2205;
    B_800F4E68 = 0x00442038;
    B_800F88E8 = 0x00442038;
    B_800F7E54 = 2;
}

/* 0x8007F94C: Gfx_Ortho - a pixel projection (origin at the screen centre) and an identity
 * model matrix */
void func_8007F94C(u32 **dl) {
    f32 mf[4][4], id[4][4];
    u32 mtx[16];
    s32 l = -(B_800F5D00 / 2);
    s32 r = l - 1 + B_800F5D00;
    s32 b = -(B_800F5BEC / 2);
    s32 t = b + B_800F5BEC;
    u32 *p;

    func_80097140(mf, (f32)l, (f32)r, (f32)b, (f32)t, 0.0f, 1000.0f, 1.0f);
    func_80096D30(mf, mtx);
    bcopy(mtx, B_800F5B6C, 0x40);
    DL_PUSH(dl, 0x01030040, K0_TO_PHYS(B_800F5B6C));     /* projection */
    B_800F5B6C += 0x40;
    id[0][0] = 1.0f; id[0][1] = 0.0f; id[0][2] = 0.0f; id[0][3] = 0.0f;
    id[1][0] = 0.0f; id[1][1] = 1.0f; id[1][2] = 0.0f; id[1][3] = 0.0f;
    id[2][0] = 0.0f; id[2][1] = 0.0f; id[2][2] = 1.0f; id[2][3] = 0.0f;
    id[3][0] = 0.0f; id[3][1] = 0.0f; id[3][2] = 0.0f; id[3][3] = 1.0f;
    func_80096D30(id, B_800F5B6C);
    p = gDL; gDL = p + 2;                                /* (gDL, not dl) */
    p[0] = 0x01020040;                                   /* model view */
    p[1] = K0_TO_PHYS(B_800F5B6C);
    B_800F5B6C += 0x40;
}

/* 0x8007FB08: Gfx_SetGeometryMode - clears and sets only the bits that change */
void func_8007FB08(u32 **dl, s32 mode) {
    s32 clear = ~mode & B_800F5CD8;
    s32 set = (mode ^ B_800F5CD8) & mode;

    if (clear != 0) {
        DL_PUSH(dl, 0xB6000000, (u32)clear);
        B_800F5CD8 = ~clear & B_800F5CD8;
    }
    if (set != 0) {
        DL_PUSH(dl, 0xB7000000, (u32)set);
        B_800F5CD8 = set | B_800F5CD8;
    }
}

/* 0x8007FB90: Gfx_SetRenderMode */
void func_8007FB90(u32 **dl, u32 mode1, u32 mode2) {
    if (mode1 != B_800F4E68 || mode2 != B_800F88E8) {
        DL_PUSH(dl, 0xB900031D, mode1 | mode2);
        B_800F4E68 = mode1;
        B_800F88E8 = mode2;
    }
}

/* 0x8007FBE8: Gfx_SetCombine - one of six colour combiner set-ups (other values: none, but
 * remembered) */
void func_8007FBE8(u32 **dl, s32 combine) {
    if (B_800F7E54 == combine) return;
    switch ((u32)combine) {
    case 0: DL_PUSH(dl, 0xFCFFFFFF, 0xFFFDF6FB); break;  /* primitive colour */
    case 1: DL_PUSH(dl, 0xFCFFFFFF, 0xFFFE793C); break;  /* shade */
    case 2: DL_PUSH(dl, 0xFC121824, 0xFF33FFFF); break;  /* texture * shade */
    case 3: DL_PUSH(dl, 0xFCFFFE04, 0xFF1279FF); break;
    case 4: DL_PUSH(dl, 0xFC121224, 0xFF27FFFF); break;
    case 5: DL_PUSH(dl, 0xFC1213FF, 0xFFFFFE38); break;
    default: break;
    }
    B_800F7E54 = combine;
}

/* 0x8007FCF8: Gfx_SetFillColour - r, g, b as RGBA5551, twice */
void func_8007FCF8(u32 **dl, s32 r, s32 g, s32 b) {
    u32 g5 = ((u32)g << 3) & 0x7C0;
    u32 b5 = ((u32)b >> 2) & 0x3E;
    u32 c = (((((u32)r << 8) & 0xF800) | g5 | b5 | 1) << 16 | 1) | ((((u32)r << 8) & 0xF800) | g5 | b5);

    if (c != B_800F5D24) {
        DL_PUSH(dl, 0xF7000000, c);
        B_800F5D24 = c;
    }
}

/* 0x8007FD70: Gfx_SetLights - picks one of three light sets (0, 1, other) and re-does the
 * 3D set-up with the current camera and model matrices */
void func_8007FD70(u32 **dl, s32 mode) {
    if (B_800F5B60 == mode) return;
    B_800F5B60 = mode;
    if (mode == 0) {
        DL_PUSH(dl, 0xB4000000, (u32)D_800C2C80);
        DL_PUSH(dl, 0xAF0007FF, (u32)D_800A64D0);
    } else if (mode == 1) {
        DL_PUSH(dl, 0xB4000000, (u32)D_800C3480);
        DL_PUSH(dl, 0xAF0007FF, (u32)D_800A7900);
    } else {
        DL_PUSH(dl, 0xB4000000, (u32)D_800C3C80);
        DL_PUSH(dl, 0xAF0007FF, (u32)D_800A8CD0);
    }
    DL_PUSH(dl, 0xFE000000, K0_TO_PHYS(B_800F1334));
    DL_PUSH(dl, SET_COLOR_IMAGE_W0(), K0_TO_PHYS(B_800F882C));
    func_8007EEF0(dl);
    func_80080170(dl, B_800F7DB8);
    func_8008004C(dl, B_800F5AA8);
}
