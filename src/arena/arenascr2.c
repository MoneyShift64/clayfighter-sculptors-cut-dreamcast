/*
 * The per-arena animation scripts, part 2 (0x8001ABF0-0x800245B0): the lab, the war room,
 * the sewer, the junkyard, Sumo's arena, ... and a few helpers they share (drawing an arena
 * part with or without culling, a falling-particles system).
 *
 * The arena runtime (func_8000AF40 / B894 / B998 / BCA8 / BE80 / C07C, another file) calls
 * two tables indexed by the arena number:
 *   D_800AB3CC[arena](tick, ctx, bank): the update - tick 0 when the arena is loaded, then
 *       1, 2, ... every frame, and -1 when it is freed;
 *   D_800AB434[arena](cam, pass, ctx, bank): drawing - pass 0 (func_8000B998: the arena),
 *       1 (BCA8), 2 (BE80: see-through parts) and 3 (C07C: last, effects).
 * ctx (ArenaContext) is filled by func_8000CEDC from the combat handler (or defaults outside a
 * fight); bank is the arena's loaded BkgBank: a header and a table of display lists/images.
 */
#include "common.h"
#include "mpfplat.h"
#include "combat.h"
#include "arena.h"

/* the context the scripts get (ArenaContext, arena.h; func_8000CEDC fills it): the ones here
 * use idle (no culling: outside the fight proper), the fighters' and the camera's positions,
 * the camera's right vector, and zone / zoneFlag / zoneFlagTime: a fighter being knocked
 * through a zone (a door, a wall) - the zone's name is at zone + 4 */

/* object i of the arena's BkgBank (a model, a display list, an image) */
#define PART(bank, i) ((u8 *)(bank) + (bank)->objects[i].offset)
#define ANGLE(deg) ((s32)((f64)((deg) * 65536.0f) / 360.0))   /* degrees -> 0x10000 a turn */

/* ---- elsewhere ---- */
void func_800831F4(u32 **dl, void *part);                 /* draw an arena part */
void func_8000C9C8(void *image, ArenaContext *ctx, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void func_80081924(u32 **dl, void *image, s32 mode);       /* load a texture */
void func_8000D94C(void);
void func_8000DA08(void);
void func_8000DD0C(void *cam);
extern f32 D_800AC3D0[3], D_800AC3DC[3];                  /* up and right for the arena's sprites */
extern u8 B_800DC890[], B_800DC8D0[];                     /* matrices the scripts use */
void func_80088300(void *m, f32 x, f32 y, f32 z);         /* scale (used to mirror) */
extern s32 B_800DC960[];                                  /* script state (door angles, frames ...) */
extern f32 B_800DC910[];                                  /* script state (floats) */

/* ---- the falling particles (dust, drips ...): 150 of them ---- */
typedef struct Scr2_Particle {
    /* 0x00 */ f32 pos[3];
    /* 0x0C */ s32 active;
    /* 0x10 */ f32 phase;             /* degrees: their wobble */
} Scr2_Particle;
extern Scr2_Particle B_800F4EF0[150];
extern f32 B_800F5D08;                /* how many start each tick */
extern f32 B_800F5B98;                /* ... the fraction carried over */
extern f32 B_800F5B58;                /* the wobble's angle (degrees) */
extern f32 B_800F7D80[3];             /* their velocity */
extern f32 B_800F7E68, B_800F88D4;    /* x range they start in */
extern f32 B_800F1B5C, B_800F7A88;    /* z range */
extern f32 B_800F8838;                /* the height they start at */
extern f32 B_800F8820;                /* ... and where they vanish */

/* 0x8001ABF0: Scr2_DrawPart - an arena model, culled (func_8000C2B8) unless the fight is
 * idle or a fighter is being knocked through a zone */
void func_8001ABF0(void *part, ArenaContext *ctx) {
    if (ctx->idle != 0 || ctx->zone != NULL) {
        func_800831F4(&gDL, part);
    } else {
        func_8000C2B8((BkgModel *)part, ctx);
    }
}

/* 0x8001AC3C: Scr2_DrawSprite - an image standing in the arena (D_800AC3D0/DC: up, right) */
void func_8001AC3C(void *image, f32 x, f32 y, f32 z, ArenaContext *ctx) {
    if (ctx->idle != 0 || ctx->zone != NULL) {
        func_80083A64(&gDL, image, x, y, z, D_800AC3D0, D_800AC3DC, 0);
    } else {
        func_8000C9C8(image, ctx, x, y, z, D_800AC3D0, D_800AC3DC, 0);
    }
}

/* 0x8001ACEC: Scr2_DrawPartMode - patches a part's geometry/render-mode words first
 * (in a zone: lit and see-through), then draws it (culled if cull) */
void func_8001ACEC(u32 *part, ArenaContext *ctx, s32 cull) {
    if (ctx->zone != NULL) {
        part[1] = 0x2205;
        part[2] = 0x442230;
        part[3] = 0x112230;
    } else {
        part[1] = 0x2204;
        part[2] = 0xC084000;
        part[3] = 0x3024000;
    }
    if (cull != 0) {
        func_8001ABF0(part, ctx);
    } else {
        func_800831F4(&gDL, part);
    }
}

/* 0x8001AD70: Scr2_DrawPartMode2 - the same with other modes */
void func_8001AD70(u32 *part, ArenaContext *ctx, s32 cull) {
    u32 m;
    if (ctx->zone != NULL) {
        part[1] = 0x12205;
        m = 0xC8004030;
    } else {
        part[1] = 0x12204;
        m = 0xC8004000;
    }
    part[2] = m;
    part[3] = m;
    if (cull != 0) {
        func_8001ABF0(part, ctx);
    } else {
        func_800831F4(&gDL, part);
    }
}

/* 0x8001ADE8: Particles_Init - none active, 0.5 a tick, falling at (-0.1, -1.8, -0.1) */
void func_8001ADE8(void) {
    s32 i;
    for (i = 0; i < 150; i++) B_800F4EF0[i].active = 0;
    B_800F5D08 = 0.5f;
    B_800F5B98 = 0.0f;
    B_800F5B58 = 0.0f;
    B_800F7D80[0] = -0.1f;
    B_800F7D80[1] = -1.8f;
    B_800F7D80[2] = -0.1f;
}

/* 0x8001AE70: Particles_SetArea - x range, the height they vanish at, the start height, z range */
void func_8001AE70(f32 x0, f32 x1, f32 bottom, f32 top, f32 z0, f32 z1) {
    B_800F7E68 = x0;
    B_800F88D4 = x1;
    B_800F8820 = bottom;
    B_800F8838 = top;
    B_800F1B5C = z0;
    B_800F7A88 = z1;
}

/* 0x8001AEAC: Particles_SetRate */
void func_8001AEAC(f32 rate) {
    B_800F5D08 = rate;
}

/* 0x8001AEB8: Particles_Update - start new ones (at random places in the area), move them */
void func_8001AEB8(void) {
    s32 w = (s32)(B_800F88D4 - B_800F7E68);
    s32 d = (s32)(B_800F7A88 - B_800F1B5C);
    s32 i = 0, n;
    B_800F5B98 = B_800F5B98 + B_800F5D08;
    if ((f64)B_800F5B98 >= 1.0) {
        n = (s32)B_800F5B98;
        B_800F5B98 = B_800F5B98 - (f32)n;
        for (n--; n >= 0; n--) {
            Scr2_Particle *p;
            for (i = 0; i < 150 && B_800F4EF0[i].active != 0; i++) {}
            if (i >= 150) break;
            p = &B_800F4EF0[i];
            p->pos[0] = (f32)((s32)guRandom() % w) + B_800F7E68;
            p->pos[1] = B_800F8838;
            p->pos[2] = (f32)((s32)guRandom() % d) + B_800F1B5C;
            p->active = 1;
            p->phase = (f32)((s32)guRandom() % 360);
        }
        i = 0;
    }
    for (; i < 150; i++) {
        Scr2_Particle *p = &B_800F4EF0[i];
        if (p->active == 0) continue;
        p->pos[0] = p->pos[0] + B_800F7D80[0];
        p->pos[1] = p->pos[1] + B_800F7D80[1];
        p->pos[2] = p->pos[2] + B_800F7D80[2];
        if (p->pos[1] < B_800F8820) p->active = 0;
    }
    B_800F5B58 = B_800F5B58 + 5.0f;
}

/* 0x8001B1DC: Particles_Draw - each one a little hexagon facing the camera (camDir: the
 * direction it looks along), wobbling; lit: drawn with the lit render modes */
void func_8001B1DC(f32 *camDir, s32 lit) {
    f32 side[3], vel[3], p[3], d[3];
    s32 i;
    side[0] = -camDir[0];
    side[1] = -camDir[1];
    side[2] = -camDir[2];
    side[1] = 0.0f;
    func_80087DD0(side);
    vel[0] = B_800F7D80[0];
    vel[1] = B_800F7D80[1];
    vel[2] = B_800F7D80[2];
    func_80087DD0(vel);                                  /* (not used) */
    gDL[0] = 0xE7000000;                                 /* gDPPipeSync */
    gDL[1] = 0;
    gDL += 2;
    if (lit != 0) {
        func_8007FB08(&gDL, 0x10205);
        func_8007FB90(&gDL, 0xC8004030, 0xC8004030);
        func_8007FBE8(&gDL, 3);
    } else {
        func_8007FB08(&gDL, 0x205);
        func_8007FB90(&gDL, 0x442078, 0x112078);
        func_8007FBE8(&gDL, 1);
    }
    for (i = 0; i < 150; i++) {
        Scr2_Particle *q = &B_800F4EF0[i];
        Vtx16 *v;
        s32 k, x, y, z;
        if (q->active == 0) continue;
        p[0] = side[0] * 8.0f * vsSin(ANGLE(q->phase + B_800F5B58)) + q->pos[0];
        p[1] = vsSin(ANGLE(q->phase + B_800F5B58) << 1) * 4.0f + q->pos[1];
        p[2] = side[2] * 8.0f * vsSin(ANGLE(q->phase + B_800F5B58)) + q->pos[2];
        d[0] = side[0] * 3.0f;
        d[1] = 2.598f;
        d[2] = side[2] * 3.0f;
        v = gVtx;
        for (k = 0; k < 7; k++) {
            v[k].flag = 0;
            v[k].s = 0;
            v[k].t = 0;
            v[k].r = 150; v[k].g = 150; v[k].b = 200; v[k].a = 255;
        }
        v[0].r = 225; v[0].g = 225; v[0].b = 225;
        v[0].x = (s16)(s32)p[0];
        v[0].z = (s16)(s32)p[2];
        v[2].x = (s16)(s32)(p[0] + d[0]);
        v[2].z = (s16)(s32)(p[2] + d[2]);
        v[5].x = (s16)(s32)(p[0] - d[0]);
        v[5].z = (s16)(s32)(p[2] - d[2]);
        y = (s32)p[1];
        v[0].y = v[5].y = v[2].y = (s16)y;
        d[0] = d[0] / 2.0f;
        d[2] = d[2] / 2.0f;
        x = (s32)(p[0] + d[0]); v[3].x = v[1].x = (s16)x;
        x = (s32)(p[0] - d[0]); v[6].x = v[4].x = (s16)x;
        y = (s32)(p[1] + d[1]); v[6].y = v[1].y = (s16)y;
        y = (s32)(p[1] - d[1]); v[4].y = v[3].y = (s16)y;
        z = (s32)(p[2] + d[2]); v[3].z = v[1].z = (s16)z;
        z = (s32)(p[2] - d[2]); v[6].z = v[4].z = (s16)z;
        gDL[0] = 0x04001C6F;                             /* 7 vertices */
        gDL[1] = (u32)v;
        gVtx = v + 7;
        gDL[2] = 0xB1000204;                             /* 6 triangles round the centre */
        gDL[3] = 0x406;
        gDL[4] = 0xB1000608;
        gDL[5] = 0x80A;
        gDL[6] = 0xB1000A0C;
        gDL[7] = 0xC02;
        gDL += 8;
    }
    gDL[0] = 0xE7000000;
    gDL[1] = 0;
    gDL += 2;
}

/* a door opening while a fighter is knocked through its zone: 0 .. -75 degrees, 3 a tick */
static void DoorAngle(ArenaContext *ctx, const char *name, s32 *angle) {
    if (mpStrEqualNoCase((char *)ctx->zone + 4, name) && ctx->zoneFlag != 0) {
        *angle = (25.0 < (f64)ctx->zoneFlagTime) ? -75 : (s32)((f64)-ctx->zoneFlagTime * 3.0);
    }
}

/* 0x8001B7F8: Lab_Update - the four doors (lablow1/2, labdoor2/1) */
void func_8001B7F8(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)bank;
    if (tick == 0) {
        B_800DC960[3] = 0;
        B_800DC960[2] = 0;
        B_800DC960[1] = 0;
        B_800DC960[0] = 0;
    }
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "lablow1", &B_800DC960[0]);
    DoorAngle(ctx, "lablow2", &B_800DC960[1]);
    DoorAngle(ctx, "labdoor2", &B_800DC960[2]);
    DoorAngle(ctx, "labdoor1", &B_800DC960[3]);
}

/* a door part: turned by angle degrees about a hinge (pos), the matrix loaded */
static void DrawDoor(void *part, ArenaContext *ctx, f32 x, f32 y, f32 z, f32 deg, f32 hx, f32 hz) {
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, x, y, z);
    func_800883F0(B_800DC890, ANGLE(deg));
    func_80088350(B_800DC890, hx, 0.0f, hz);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(part, ctx);
}

/* 0x8001BA0C: Lab_DrawUpper - the upper floor and its two doors */
void func_8001BA0C(ArenaContext *ctx, BkgBank *bank) {
    func_800831F4(&gDL, PART(bank, 6));
    func_8001ABF0(PART(bank, 7), ctx);
    func_8001ABF0(PART(bank, 8), ctx);
    func_8001ABF0(PART(bank, 12), ctx);
    func_8001ABF0(PART(bank, 13), ctx);
    DrawDoor(PART(bank, 11), ctx, 796.0f, 612.0f, 339.0f, (f32)B_800DC960[2] + 180.0f, 128.0f, 49.0f);
    DrawDoor(PART(bank, 11), ctx, 819.0f, 612.0f, -168.0f, (f32)B_800DC960[3] + 180.0f, 128.0f, 49.0f);
}

/* 0x8001BBFC: Lab_DrawLower - the lower floor and its two doors */
void func_8001BBFC(ArenaContext *ctx, BkgBank *bank) {
    func_800831F4(&gDL, PART(bank, 9));
    if (-78.0f < ctx->camPos[1]) {
        func_800831F4(&gDL, PART(bank, 10));
    } else {
        func_8001ABF0(PART(bank, 10), ctx);
    }
    func_8001ABF0(PART(bank, 14), ctx);
    func_8001ABF0(PART(bank, 15), ctx);
    DrawDoor(PART(bank, 11), ctx, -128.0f, 0.0f, -49.0f, (f32)B_800DC960[0], 128.0f, 49.0f);
    DrawDoor(PART(bank, 11), ctx, 700.0f, 0.0f, 80.0f, (f32)B_800DC960[1] + 180.0f, 128.0f, 49.0f);
}

/* 0x8001BE04: Lab_Draw - which floor depends on the camera's height */
void func_8001BE04(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    (void)cam;
    if (pass == 1) {
        u32 *p;
        func_800831F4(&gDL, PART(bank, 1));
        func_800831F4(&gDL, PART(bank, 4));
        if (ctx->camRight[0] < 0.0f || 0.0f < ctx->camRight[2]) {
            func_8001ACEC((u32 *)PART(bank, 3), ctx, 1);
        }
        if (!(ctx->camPos[1] <= -78.0f)) return;
        func_800831F4(&gDL, PART(bank, 0));
        p = (u32 *)PART(bank, 2);
        if (0.0f < ctx->camRight[0] || ctx->zone != NULL) {
            p[1] = 0x2205;
            p[2] = 0x442230;
            p[3] = 0x112230;
        } else {
            p[1] = 0x2204;
            p[2] = 0xC084000;
            p[3] = 0x3024000;
        }
        func_8001ABF0(p, ctx);
        func_800831F4(&gDL, PART(bank, 5));
        func_8001BA0C(ctx, bank);
    } else if (pass == 0) {
        if (!(-78.0f < ctx->camPos[1])) return;
        func_800831F4(&gDL, PART(bank, 0));
        func_8001ACEC((u32 *)PART(bank, 2), ctx, 1);
        func_8001BBFC(ctx, bank);
        func_80087E20(B_800DC890);
        func_80080170(&gDL, B_800DC890);
        func_800831F4(&gDL, PART(bank, 5));
    } else if (pass == 2) {
        if (ctx->camPos[1] <= -78.0f) {
            func_8001BBFC(ctx, bank);
        } else {
            func_8001BA0C(ctx, bank);
        }
    }
}

/* 0x8001C060: WarRoom_Update - the door (zone "warroom") */
void func_8001C060(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)bank;
    if (tick == 0) B_800DC960[0] = 0;
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "warroom", &B_800DC960[0]);
}

/* 0x8001C108: WarRoom_Draw */
void func_8001C108(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 0));
    } else if (pass == 2) {
        func_800831F4(&gDL, PART(bank, 5));
        func_8001ACEC((u32 *)PART(bank, 1), ctx, 0);
        func_800831F4(&gDL, PART(bank, 2));
        func_8001ABF0(PART(bank, 3), ctx);
        DrawDoor(PART(bank, 4), ctx, -608.0f, 0.0f, -42.0f, (f32)B_800DC960[0], 608.0f, 42.0f);
        func_80087E20(B_800DC890);
        func_80080170(&gDL, B_800DC890);
        func_800831F4(&gDL, PART(bank, 6));
    }
}

/* 0x8001C27C: Arena23_Update - a lift going up and down on a 1200-tick cycle, a swinging
 * thing, and an animated texture (5 frames, 8 ticks each)
 *   B_800DC910[0]: counts down from 92 by 0.18 (while >= 0.165)
 *   [1]: the lift's height, [2]: its base height (0..240), [3]: a tilt (0..50 degrees)
 *   [4]: a turn (sine, +-364), [5]: a tilt */
void func_8001C27C(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    f32 *a = B_800DC910;
    s32 t = tick % 1200;
    f32 ft, s;
    s32 ang;
    (void)ctx; (void)bank;
    if (tick == 0) {
        a[0] = 92.0f;
        a[5] = 0.0f;
        a[4] = 0.0f;
        a[3] = 0.0f;
        a[2] = 0.0f;
        func_8000D94C();
    }
    if ((f64)a[0] >= 0.165) a[0] = (f32)((f64)a[0] - 0.18);
    if (t >= 701) {
        if (a[2] > 0.0f) a[2] = a[2] - 3.0f;
    } else if (t >= 501) {
        if (a[3] > 0.0f) a[3] = a[3] - 1.0f;
    } else if (t >= 401) {
        if (a[3] < 50.0f) a[3] = a[3] + 1.0f;
        if (a[2] < 240.0f) a[2] = a[2] + 3.0f;
    }
    ft = (f32)tick;
    ang = (s32)((f64)(ft * 5.0f * 65536.0f) / 360.0);
    a[4] = vsSin(ang) * 364.0f;
    s = vsSin((s32)((f64)(ft * 7.0f * 65536.0f) / 360.0));
    a[5] = (f32)(s32)((f64)((-a[3] + (s + s)) * 65536.0f) / 360.0);
    s = vsSin(ang);
    a[1] = (a[2] + 20.0f) + s * 4.0f;
    B_800DC960[0] = (tick >> 3) % 5;
    func_8000DA08();
}

/* 0x8001C5B4: Arena23_Draw (the lift, the swinging parts; pass 3: func_8000DD0C) */
void func_8001C5B4(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    f32 *a = B_800DC910;
    switch (pass) {
    case 0:
        if (ctx->camPos[1] < 380.0f) func_800831F4(&gDL, PART(bank, 0));
        func_800831F4(&gDL, PART(bank, 1));
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, 0.0f, a[1], 0.0f);
        func_80080170(&gDL, B_800DC890);
        func_80081924(&gDL, PART(bank, 9 + B_800DC960[0]), 1);
        func_800831F4(&gDL, PART(bank, 7));
        func_80087E20(B_800DC890);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 8), ctx);
        break;
    case 1:
        if (380.0f < ctx->camPos[1]) func_800831F4(&gDL, PART(bank, 0));
        break;
    case 2:
        func_800831F4(&gDL, PART(bank, 5));
        func_8001ABF0(PART(bank, 2), ctx);
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, -664.0f, 892.0f, -1272.0f);
        func_80088440(B_800DC890, (s32)((f64)(a[3] * 65536.0f) / 360.0));
        func_80088350(B_800DC890, 664.0f, -892.0f, 1272.0f);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 3), ctx);
        func_80088350(B_800DC890, -848.0f, 893.0f, -1241.0f);
        func_800883A0(B_800DC890, (s32)a[4]);
        func_80088440(B_800DC890, (s32)a[5]);
        func_80088350(B_800DC890, 848.0f, -893.0f, 1241.0f);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 4), ctx);
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, 0.0f, 323.0f, -1120.0f);
        func_800883A0(B_800DC890, (s32)((f64)(a[0] * 65536.0f) / 360.0));
        func_80088350(B_800DC890, 0.0f, -323.0f, 1110.0f);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 6), ctx);
        break;
    case 3:
        func_8000DD0C(cam);
        break;
    }
}

/* 0x8001C964: Sewer_Update - six animated things (frames 5..12, a step every 8 ticks,
 * starting at random frames) and the door (zone "sewer2") */
void func_8001C964(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)bank;
    s32 i;
    if (tick == 0) {
        for (i = 5; i >= 0; i--) B_800DC960[1 + i] = (s32)guRandom() % 8 + 5;
        B_800DC960[0] = 0;
    }
    if ((tick & 7) == 0) {
        for (i = 5; i >= 0; i--) {
            if (++B_800DC960[1 + i] == 13) B_800DC960[1 + i] = 5;
        }
    }
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "sewer2", &B_800DC960[0]);
}

/* 0x8001CAB0: Sewer_Draw - the two doors and six animated sprites */
void func_8001CAB0(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    f32 *up = D_800AC3D0, *right = D_800AC3DC;
    s32 *fr = B_800DC960;
    f32 s;
    (void)cam;
    if (pass == 1) {
        func_8001AD70((u32 *)PART(bank, 0), ctx, 0);
        func_800831F4(&gDL, PART(bank, 1));
        return;
    }
    if (pass != 2) return;
    func_800831F4(&gDL, PART(bank, 2));
    func_8001ABF0(PART(bank, 3), ctx);
    DrawDoor(PART(bank, 4), ctx, -874.0f, 0.0f, -171.0f, (f32)fr[0], 874.0f, 171.0f);
    DrawDoor(PART(bank, 4), ctx, -874.0f, 0.0f, 225.0f, (f32)(-fr[0]), 874.0f, -22.0f);
    func_80087E20(B_800DC890);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 21), ctx);

    right[0] = -6.4f;
    right[2] = 0.0f;
    up[2] = 0.0f;
    up[0] = 0.0f;
    up[1] = 1.4f;
    func_8001AC3C(PART(bank, fr[6] + 8), -64.0f, 340.0f, -880.0f, ctx);
    right[0] = (f32)((f64)vsCos(0x2444) * -2.2);
    right[2] = (f32)((f64)vsSin(0x2444) * -2.2);
    up[1] = 2.9f;
    func_8001AC3C(PART(bank, fr[1]), 512.0f, 412.0f, -569.0f, ctx);
    right[0] = (f32)((f64)vsCos(-0xCCC) * -2.2);
    right[2] = (f32)((f64)vsSin(-0xCCC) * -2.2);
    up[1] = 2.4f;
    func_8001AC3C(PART(bank, fr[2]), -307.0f, 255.0f, -826.0f, ctx);
    right[0] = (f32)((f64)vsCos(-0x182D) * -2.2);
    right[2] = (f32)((f64)vsSin(-0x182D) * -2.2);
    up[1] = 1.9f;
    func_8001AC3C(PART(bank, fr[3]), -581.0f, 118.0f, -550.0f, ctx);
    right[0] = (f32)((f64)vsCos(-0x960B) * -2.3);
    right[2] = (f32)((f64)vsSin(-0x960B) * -2.3);
    up[1] = (f32)((f64)vsCos(0xCCC) * 2.3);
    s = vsSin(-0x960B);
    up[0] = (f32)((f64)s * 2.3 * (f64)vsSin(0xCCC));
    s = vsCos(-0x960B);
    up[2] = (f32)((f64)s * -2.3 * (f64)vsSin(0xCCC));
    func_8001AC3C(PART(bank, fr[4]), 366.0f, 132.0f, 617.0f, ctx);
    right[0] = (f32)((f64)vsCos(0x4000) * -2.2);
    right[2] = (f32)((f64)vsSin(0x4000) * -2.2);
    up[1] = (f32)((f64)vsCos(-0x1555) * 3.1);
    s = vsSin(0x4000);
    up[0] = (f32)((f64)s * 3.1 * (f64)vsSin(-0x1555));
    s = vsCos(0x4000);
    up[2] = (f32)((f64)s * -3.1 * (f64)vsSin(-0x1555));
    func_8001AC3C(PART(bank, fr[5]), 637.0f, 285.0f, 633.0f, ctx);
}

/* 0x8001D0A8: Junkyard1_Update - the door (zone "junk_down1") */
void func_8001D0A8(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)bank;
    if (tick == 0) B_800DC960[0] = 0;
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "junk_down1", &B_800DC960[0]);
}

/* 0x8001D150: Junkyard1_Draw - a double door (the second half mirrored) */
void func_8001D150(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    u32 *door;
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 0));
        return;
    }
    if (pass != 2) return;
    func_8001ACEC((u32 *)PART(bank, 2), ctx, 1);
    func_8001ABF0(PART(bank, 1), ctx);
    func_8001ABF0(PART(bank, 4), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, 842.0f, 0.0f, -165.0f);
    func_80088300(B_800DC890, 1.0f, 1.0f, -1.0f);
    func_800883F0(B_800DC890, ANGLE((f32)B_800DC960[0]));
    func_80088350(B_800DC890, -842.0f, 0.0f, -115.0f);
    func_80080170(&gDL, B_800DC890);
    door = (u32 *)PART(bank, 3);
    door[1] |= 0x1000;                                   /* cull the other faces (mirrored) */
    door = (u32 *)PART(bank, 3);
    door[1] &= ~0x2000;
    func_8001ABF0(PART(bank, 3), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, 842.0f, 0.0f, 115.0f);
    func_800883F0(B_800DC890, ANGLE((f32)B_800DC960[0]));
    func_80088350(B_800DC890, -842.0f, 0.0f, -115.0f);
    func_80080170(&gDL, B_800DC890);
    door = (u32 *)PART(bank, 3);
    door[1] |= 0x2000;
    door = (u32 *)PART(bank, 3);
    door[1] &= ~0x1000;
    func_8001ABF0(PART(bank, 3), ctx);
}

/* 0x8001D3B0: Junkyard2_Update - B_800DC960: [0] a swing's phase (5 degrees a tick),
 * [1] its size (5 while a fighter is up on the thing, then down by 1 a turn), [2] ticks
 * (to 300), [3] the door (zone "junk_in1", opening the other way) */
void func_8001D3B0(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    (void)bank;
    if (tick == 0) {
        b[1] = 0;
        b[2] = 0;
        b[3] = 0;
    }
    b[0] = tick * 5;
    if ((0.0f < ctx->fighter1[0] && ctx->fighter1[0] < 360.0f && 220.0f < ctx->fighter1[1]) ||
        (0.0f < ctx->fighter2[0] && ctx->fighter2[0] < 360.0f && 220.0f < ctx->fighter2[1])) {
        b[1] = 5;
    } else if (b[0] % 360 == 0 && b[1] != 0) {
        b[1] = b[1] - 1;
    }
    if (b[2] < 300) b[2] = b[2] + 1;
    if (ctx->zone == NULL) return;
    if (mpStrEqualNoCase((char *)ctx->zone + 4, "junk_in1") && ctx->zoneFlag != 0) {
        b[3] = (25.0 < (f64)ctx->zoneFlagTime) ? 75 : (s32)((f64)ctx->zoneFlagTime * 3.0);
    }
}

/* 0x8001D5A8: Junkyard2_Draw */
void func_8001D5A8(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    u32 *door;
    s32 show;
    (void)cam;
    if (pass == 1) {
        /* the double door */
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, -410.0f, 0.0f, 108.0f);
        func_800883F0(B_800DC890, ANGLE((f32)b[3]));
        func_80088350(B_800DC890, 410.0f, 0.0f, -108.0f);
        func_80080170(&gDL, B_800DC890);
        door = (u32 *)PART(bank, 6);
        door[1] |= 0x2000;
        door = (u32 *)PART(bank, 6);
        door[1] &= ~0x1000;
        func_8001ABF0(PART(bank, 6), ctx);
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, -410.0f, 0.0f, -108.0f);
        func_80088300(B_800DC890, 1.0f, 1.0f, -1.0f);
        func_800883F0(B_800DC890, ANGLE((f32)b[3]));
        func_80088350(B_800DC890, 410.0f, 0.0f, -108.0f);
        func_80080170(&gDL, B_800DC890);
        door = (u32 *)PART(bank, 6);
        door[1] |= 0x1000;
        door = (u32 *)PART(bank, 6);
        door[1] &= ~0x2000;
        func_8001ABF0(PART(bank, 6), ctx);
        func_80087E20(B_800DC890);
        func_80080170(&gDL, B_800DC890);
        /* drawn whole for the first 300 ticks */
        show = (ctx->idle != 0 && b[2] < 300) || ctx->zone != NULL;
        if (show) {
            func_800831F4(&gDL, PART(bank, 5));
        } else {
            func_8000C2B8((BkgModel *)PART(bank, 5), ctx);
        }
        func_8001ACEC((u32 *)PART(bank, 0), ctx, 0);
        func_800831F4(&gDL, PART(bank, 1));
        func_800831F4(&gDL, PART(bank, 7));
    } else if (pass == 2) {
        f32 s;
        func_800831F4(&gDL, PART(bank, 2));
        show = (ctx->idle != 0 && b[2] < 300) || ctx->zone != NULL;
        if (show) {
            func_800831F4(&gDL, PART(bank, 3));
            func_800831F4(&gDL, PART(bank, 4));
        } else {
            func_8000C2B8((BkgModel *)PART(bank, 3), ctx);
            func_8000C2B8((BkgModel *)PART(bank, 4), ctx);
        }
        /* the swinging thing */
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, 186.0f, 427.0f, 0.0f);
        s = vsSin(ANGLE((f32)b[0]));
        func_80088440(B_800DC890, (s32)((f32)ANGLE((f32)b[1]) * s));
        s = vsSin(ANGLE((f32)b[0] + 30.0f));
        func_800883A0(B_800DC890, (s32)((f32)ANGLE((f32)b[1] / 2.0f) * s));
        func_80088350(B_800DC890, -186.0f, -427.0f, 0.0f);
        func_80080170(&gDL, B_800DC890);
        func_800831F4(&gDL, PART(bank, 8));
    }
}

/* 0x8001DA64: Junkyard3_Update - B_800DC960: [0] tick*10, [1] a 5-frame animation and
 * [5] an 8-frame one (a frame every 10 ticks), [2] a bounce's phase, [3]/[4] fighter 1/2
 * is in the air, [6] the door (zone "junk_out1"); B_800DC910: [0] a lever following the
 * fighters (x), [1] the bounce's size (30 when a fighter lands, then down), [2] the tilt */
void func_8001DA64(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    f32 *a = B_800DC910;
    f32 s, size;
    (void)bank;
    if (tick == 0) {
        b[4] = 0;
        b[3] = 0;
        a[1] = 0.0f;
        b[2] = 0;
        b[6] = 0;
    }
    b[5] = (tick / 10) % 8;
    b[1] = (tick / 10) % 5;
    b[0] = tick * 10;
    a[0] = (f32)((f64)(ctx->fighter1[0] + ctx->fighter2[0] + 1500.0f) * 0.1 + 110.0);
    if (110.0f < a[0]) a[0] = 110.0f;
    /* a fighter landing makes it bounce */
    if (b[3] != 0) {
        if (ctx->fighter1[1] == 0.0f) {
            b[3] = 0;
            if (a[1] < 5.0f) {
                a[1] = 30.0f;
                if (guRandom() & 1) b[2] = 0; else b[2] = 22;
            }
        }
    } else if (ctx->fighter1[1] != 0.0f) {
        b[3] = 1;
    }
    if (b[4] != 0) {
        if (ctx->fighter2[1] == 0.0f) {
            b[4] = 0;
            if (a[1] < 5.0f) {
                a[1] = 30.0f;
                if (guRandom() & 1) b[2] = 0; else b[2] = 22;
            }
        }
    } else if (ctx->fighter2[1] != 0.0f) {
        b[4] = 1;
    }
    s = vsSin(ANGLE((f32)b[2] * 8.0f));
    size = a[1];
    a[2] = (f32)(s32)((f64)(size * 65536.0f) / 360.0) * s;
    b[2] = b[2] + 1;
    if (size != 0.0f && (tick & 7) == 0) a[1] = size - 1.0f;
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "junk_out1", &b[6]);
}

/* 0x8001DE84: Junkyard3_Draw - upstairs (camera above -100) or downstairs */
void func_8001DE84(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    f32 *a = B_800DC910;
    f32 *up = D_800AC3D0, *right = D_800AC3DC;
    u32 *door;
    f32 s;
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 1));
        if (-100.0f < ctx->camPos[1]) {
            func_800831F4(&gDL, PART(bank, 3));
            func_800831F4(&gDL, PART(bank, 2));
            func_800831F4(&gDL, PART(bank, 9));
            func_800831F4(&gDL, PART(bank, 11));
            func_800831F4(&gDL, PART(bank, 6));
        } else {
            func_8001ABF0(PART(bank, 3), ctx);
        }
    } else if (pass == 0) {
        if (ctx->camPos[1] < -100.0f) {
            func_8001ABF0(PART(bank, 10), ctx);
            func_800831F4(&gDL, PART(bank, 7));
            func_800831F4(&gDL, PART(bank, 5));
            func_800831F4(&gDL, PART(bank, 12));
            func_800831F4(&gDL, PART(bank, 11));
            func_800831F4(&gDL, PART(bank, 9));
        }
        func_800831F4(&gDL, PART(bank, 0));
    } else if (pass == 2) {
        if (-100.0f < ctx->camPos[1]) {
            func_800831F4(&gDL, PART(bank, 5));
            /* the bouncing platform */
            func_80087E20(B_800DC890);
            func_80087EE8(B_800DC890, 0.0f, 52.0f, 242.0f);
            func_800883A0(B_800DC890, (s32)a[2]);
            func_80088350(B_800DC890, 0.0f, -52.0f, -242.0f);
            func_80080170(&gDL, B_800DC890);
            func_800831F4(&gDL, PART(bank, 12));
            func_80087E20(B_800DC890);
            func_80080170(&gDL, B_800DC890);
            func_800831F4(&gDL, PART(bank, 7));
            func_8001ABF0(PART(bank, 10), ctx);
            /* the lever, about a slanted axis */
            func_80087E20(B_800DC890);
            func_80087EE8(B_800DC890, -1162.0f, 225.0f, -264.0f);
            func_80088440(B_800DC890, -0x888);
            func_800883A0(B_800DC890, -0xEEE);
            func_800883F0(B_800DC890, (s32)((f64)(a[0] * 65536.0f) / 360.0));
            func_800883A0(B_800DC890, 0xEEE);
            func_80088440(B_800DC890, 0x888);
            func_80088350(B_800DC890, 1162.0f, -225.0f, 264.0f);
            func_80080170(&gDL, B_800DC890);
            func_8001ABF0(PART(bank, 8), ctx);
            /* the double door */
            func_80087E20(B_800DC890);
            func_80087EE8(B_800DC890, 1069.0f, 0.0f, 226.0f);
            func_800883F0(B_800DC890, ANGLE((f32)b[6]));
            func_80088350(B_800DC890, -1069.0f, 0.0f, -226.0f);
            func_80080170(&gDL, B_800DC890);
            door = (u32 *)PART(bank, 4);
            door[1] |= 0x2000;
            door = (u32 *)PART(bank, 4);
            door[1] &= ~0x1000;
            func_8001ABF0(PART(bank, 4), ctx);
            func_80087E20(B_800DC890);
            func_80087EE8(B_800DC890, 1069.0f, 0.0f, 57.0f);
            func_80088300(B_800DC890, 1.0f, 1.0f, -1.0f);
            func_800883F0(B_800DC890, ANGLE((f32)b[6]));
            func_80088350(B_800DC890, -1069.0f, 0.0f, -226.0f);
            func_80080170(&gDL, B_800DC890);
            door = (u32 *)PART(bank, 4);
            door[1] |= 0x1000;
            door = (u32 *)PART(bank, 4);
            door[1] &= ~0x2000;
            func_8001ABF0(PART(bank, 4), ctx);
            func_80087E20(B_800DC890);
            func_80080170(&gDL, B_800DC890);
            /* an animated sprite, tilted */
            right[0] = (f32)((f64)vsCos(-0x27D2) * -1.8);
            right[1] = 0.0f;
            right[2] = (f32)((f64)vsSin(-0x27D2) * -1.8);
            up[1] = (f32)((f64)vsCos(0x71C) * 1.9);
            s = vsSin(-0x27D2);
            up[0] = (f32)((f64)s * 1.9 * (f64)vsSin(0x71C));
            s = vsCos(-0x27D2);
            up[2] = (f32)((f64)s * -1.9 * (f64)vsSin(0x71C));
            func_8001AC3C(PART(bank, b[5] + 18), 452.0f, 226.0f, -264.0f, ctx);
        } else {
            func_800831F4(&gDL, PART(bank, 2));
            func_80087E20(B_800DC890);
            func_80080170(&gDL, B_800DC890);
            right[0] = (f32)((f64)vsCos(-0xEEE) * -1.5);
            right[2] = (f32)((f64)vsSin(-0xEEE) * -1.5);
            up[2] = 0.0f;
            up[0] = 0.0f;
            right[1] = 0.0f;
            up[1] = 1.7f;
            func_8001AC3C(PART(bank, b[1] + 13), -155.0f, -160.0f, 782.0f, ctx);
        }
    }
}

/* an arm swinging up to +-45 degrees */
static void SwingUp(f32 *a) {
    if (*a < 45.0f) *a = *a + 5.0f;
}

/* 0x8001E5C8: Arena18_Update - a 2400-tick show: something launched (B_800DC910[0..3]:
 * its height, position and shaking), arms swinging ([13]..[17]), a turning thing ([18]),
 * random jitter for three parts ([4..12]); B_800DC960[0] = tick*10, [1] a frame (14/15,
 * flashing every 512/10 ticks) */
void func_8001E5C8(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    f32 *a = B_800DC910;
    s32 *b = B_800DC960;
    s32 t = tick % 2400, u;
    (void)ctx; (void)bank;
    if (tick == 0) {
        b[1] = 14;
        a[0] = -80.0f;
        a[2] = 0.0f;
        a[18] = 0.0f;
        a[17] = -180.0f;
        a[3] = a[1] = a[2];
        a[6] = a[5] = a[4] = a[2];
        a[9] = a[8] = a[7] = a[2];
        a[12] = a[11] = a[10] = a[2];
        a[16] = a[15] = a[14] = a[13] = -45.0f;
        a[19] = (f32)((s32)guRandom() % 300);
    }
    if (t >= 1981) {
        a[2] = a[2] - a[2] / 50.0f;
    } else if (t == 1979) {
        a[2] = 876.0f;
        a[19] = (f32)((s32)guRandom() % 300);
    } else if (t >= 980) {
        a[0] = -80.0f;
        if ((f32)t < a[19] + 979.0f) {
            if (t < 998) a[16] = a[16] - 5.0f; else SwingUp(&a[16]);
            a[18] = (f32)((f64)a[18] + 0.5);
        }
    } else if (t >= 900) {
        a[0] = a[0] - 1.0f;
        a[2] = 2200.0f;
        a[1] = 0.0f;
        a[3] = 0.0f;
        if (a[14] < 45.0f) a[14] = a[14] + 3.0f;
    } else if (t >= 801) {
        f32 c, s;
        s32 k;
        f64 r;
        c = vsCos(ANGLE(a[18]));
        k = (t - 800) / 10;
        r = (f64)k * 10.5;
        a[3] = (f32)((f64)a[3] - r * (f64)c);
        s = vsSin(ANGLE(a[18]));
        a[1] = (f32)((f64)a[1] - r * (f64)s);
        a[2] = (f32)(k * 12) + a[2];
        if (t < 819) a[15] = a[15] - 5.0f; else SwingUp(&a[15]);
    } else if (t == 800) {
        a[3] = 0.0f;
        a[2] = 0.0f;
        a[1] = 0.0f;
        a[19] = (f32)((s32)guRandom() % 300);
    } else if (t >= 701) {
        a[1] = (f32)(3 - (s32)guRandom() % 7);
        a[2] = (f32)((s32)guRandom() % 4);
        a[3] = (f32)(3 - (s32)guRandom() % 7);
    } else if (t >= 381) {
        a[0] = 0.0f;
        if ((f32)t < a[19] + 380.0f) {
            if (t < 399) a[16] = a[16] - 5.0f; else SwingUp(&a[16]);
            a[18] = (f32)((f64)a[18] + 0.5);
        }
    } else if (t >= 301) {
        a[2] = 0.0f;
        a[0] = a[0] + 1.0f;
        if (-45.0f < a[14]) a[14] = a[14] - 3.0f;
    }
    if (t >= 1981 || (u32)(t - 701) < 279) {
        a[4] = (f32)(2 - (s32)guRandom() % 5);
        a[5] = (f32)((s32)guRandom() % 3);
        a[6] = (f32)(2 - (s32)guRandom() % 5);
        a[7] = (f32)(2 - (s32)guRandom() % 5);
        a[8] = (f32)((s32)guRandom() % 3);
        a[9] = (f32)(2 - (s32)guRandom() % 5);
        a[10] = (f32)(2 - (s32)guRandom() % 5);
        a[11] = (f32)((s32)guRandom() % 3);
        a[12] = (f32)(2 - (s32)guRandom() % 5);
        b[1] = (b[0] & 0x200) ? 15 : 14;
    } else if (b[1] == 15 && (b[0] & 0x200) == 0) {
        b[1] = 14;
    }
    u = tick % 1080;
    if (u >= 811) {
        if (-45.0f < a[13]) a[13] = a[13] - 4.0f;
        if (-180.0f < a[17]) a[17] = a[17] - 1.0f;
    } else if (u >= 271) {
        if (a[13] < 45.0f) a[13] = a[13] + 4.0f;
        if (a[17] < 90.0f) a[17] = a[17] + 1.0f;
    }
    b[0] = tick * 10;
}

/* one of Arena18's arms: turned by deg degrees about its shoulder, under the turntable */
static void Arena18_Arm(BkgBank *bank, f32 x, f32 z, s32 turn, f32 deg) {
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, x, 13.0f, z);
    if (turn != 0) func_800883F0(B_800DC890, turn);
    func_800883A0(B_800DC890, (s32)((f64)(deg * 65536.0f) / 360.0));
    func_80088350(B_800DC890, 185.0f, -3.0f, -252.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_800831F4(&gDL, PART(bank, 11));
}

/* 0x8001F074: Arena18_Draw */
void func_8001F074(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    f32 *a = B_800DC910;
    s32 *b = B_800DC960;
    f32 *up = D_800AC3D0, *right = D_800AC3DC;
    f32 s;
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 0));
        func_800831F4(&gDL, PART(bank, 1));
        return;
    }
    if (pass != 2) return;
    func_8001ABF0(PART(bank, 3), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, a[10] + 818.0f, a[11], a[12] + -390.0f);
    func_800883F0(B_800DC890, -0xC0B6);
    func_80088350(B_800DC890, 212.0f, 0.0f, 22.0f);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 4), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, -5.0f, 0.0f, 848.0f);
    func_800883F0(B_800DC890, ANGLE(a[17] / 2.0f));
    func_80088350(B_800DC890, 5.0f, 0.0f, -848.0f);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 5), ctx);
    func_80088350(B_800DC890, -6.0f, 494.0f, 579.0f);
    s = vsSin(ANGLE((f32)(b[0] / 2) + 200.0f));
    func_800883A0(B_800DC890, (s32)(s * -910.0f));
    s = vsSin(ANGLE((f32)(b[0] / 3) + 230.0f));
    func_80088440(B_800DC890, (s32)(s * 910.0f));
    func_80088350(B_800DC890, 6.0f, -494.0f, -579.0f);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 6), ctx);
    /* the turntable: everything below turns with it */
    func_80087E20(B_800DC8D0);
    func_800883F0(B_800DC8D0, ANGLE(a[18]));
    func_80080170(&gDL, B_800DC8D0);
    func_800831F4(&gDL, PART(bank, 2));
    func_8001ABF0(PART(bank, 13), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, 0.0f, 0.0f, 320.0f);
    func_800883A0(B_800DC890, ANGLE((f32)((f64)a[0] / 2.0)));
    func_80088350(B_800DC890, 0.0f, 0.0f, -320.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 8), ctx);
    func_80088184(B_800DC890, a[1], a[2], a[3]);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 9), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, 0.0f, 0.0f, 320.0f);
    func_800883A0(B_800DC890, ANGLE((f32)((f64)a[0] / 4.0)));
    func_80088350(B_800DC890, 0.0f, 0.0f, -320.0f);
    func_80088134(B_800DC890, 1.0f, a[0] / 115.0f + 1.0f, 1.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 10), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, a[4], a[5], a[6]);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 4), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, a[7] + 144.0f, a[8], a[9] + -281.0f);
    func_800883F0(B_800DC890, 0x293E);
    func_80088350(B_800DC890, 212.0f, 0.0f, 22.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 4), ctx);
    /* four arms */
    Arena18_Arm(bank, -185.0f, 252.0f, 0, a[13]);
    Arena18_Arm(bank, 226.0f, 256.0f, 0, a[14]);
    Arena18_Arm(bank, 297.0f, 256.0f, 0, a[15]);
    Arena18_Arm(bank, 315.0f, -165.0f, 0x238E, a[16]);
    /* three bobbing things */
    func_80087E20(B_800DC890);
    s = vsSin(ANGLE((f32)b[0] / 2.0f));
    func_80087EE8(B_800DC890, 357.0f, (s - 1.0f) * 5.0f, 100.0f);
    s = vsSin(ANGLE((f32)b[0] / 4.0f));
    func_800883F0(B_800DC890, ANGLE(s * 20.0f + 70.0f));
    func_80088350(B_800DC890, -357.0f, 0.0f, -100.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_800831F4(&gDL, PART(bank, 12));
    func_80087E20(B_800DC890);
    s = vsSin(ANGLE((f32)(b[0] / 2) + 90.0f));
    func_80087EE8(B_800DC890, 372.0f, (s - 1.0f) * 5.0f, 23.0f);
    s = vsSin(ANGLE((f32)b[0] / 7.0f));
    func_800883F0(B_800DC890, ANGLE(s * 60.0f));
    func_80088350(B_800DC890, -357.0f, 0.0f, -100.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_800831F4(&gDL, PART(bank, 12));
    func_80087E20(B_800DC890);
    s = vsSin(ANGLE((f32)b[0] / 2.0f));
    func_80087EE8(B_800DC890, 385.0f, (s - 1.0f) * 5.0f, -48.0f);
    s = vsSin(ANGLE((f32)b[0] / 5.0f));
    func_800883F0(B_800DC890, ANGLE(s * 40.0f + 200.0f));
    func_80088350(B_800DC890, -357.0f, 0.0f, -100.0f);
    func_80088100(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    func_800831F4(&gDL, PART(bank, 12));
    func_80087E70(B_800DC890, B_800DC8D0);
    func_80080170(&gDL, B_800DC890);
    /* six sprites round the turntable (the same frame) */
    right[0] = -2.2f;
    up[2] = 0.0f;
    up[0] = 0.0f;
    right[2] = 0.0f;
    right[1] = 0.0f;
    up[1] = 1.9f;
    func_8001AC3C(PART(bank, b[1]), -65.0f, 100.0f, 433.0f, ctx);
    right[0] = 2.2f;
    func_8001AC3C(PART(bank, b[1]), 45.0f, 100.0f, -419.0f, ctx);
    right[0] = (f32)((f64)vsCos(0x4CCC) * -2.2);
    right[2] = (f32)((f64)vsSin(0x4CCC) * -2.2);
    func_8001AC3C(PART(bank, b[1]), -391.0f, 100.0f, -188.0f, ctx);
    right[0] = (f32)((f64)vsCos(0x327D) * -2.2);
    right[2] = (f32)((f64)vsSin(0x327D) * -2.2);
    func_8001AC3C(PART(bank, b[1]), -416.0f, 100.0f, 90.0f, ctx);
    right[0] = (f32)((f64)vsCos(-0x327D) * -2.2);
    right[2] = (f32)((f64)vsSin(-0x327D) * -2.2);
    func_8001AC3C(PART(bank, b[1]), 390.0f, 100.0f, 175.0f, ctx);
    right[0] = (f32)((f64)vsCos(-0x67D2) * -2.2);
    right[2] = (f32)((f64)vsSin(-0x67D2) * -2.2);
    func_8001AC3C(PART(bank, b[1]), 286.0f, 100.0f, -304.0f, ctx);
}

/* ---- Sumo's arena: four security cameras that follow a fighter and shake when one lands ----
 * B_800DC910: five floats a camera (cam*5 + ...): [0] its shake, [1] the shake's phase
 * (degrees, 0 = still), [2] its size, [3] its tilt, [4] its turn.
 * B_800DC960: [0] tick*10, [1] a monitor's frame (13..15), [2]/[3] fighter 1/2 is in the
 * air, [4..7] which fighter each camera follows, [8..11] ticks until it changes its mind,
 * [12..14] the doors (zones "sumo_3", "sumo_2", "sumo_1"). */

/* |x + ox| + |z + oz|, the way the original works it out */
static f32 Sumo_Dist(f32 *pos, f32 ox, f32 oz) {
    f32 d = pos[0] + ox, e;
    if (!(0.0f < d)) d = -d;
    e = pos[2] + oz;
    if (0.0f < e) return d + e;
    return d - (pos[2] + oz);
}

/* the tilt a camera wants to look at a fighter dist away */
static f32 Sumo_Tilt(f32 dist, f32 *pos) {
    f32 t = dist - 250.0f;
    t = (t + t) / 150.0f;
    t = t + -20.0f;
    return t + 25.0f / dist * pos[1];
}

/* moves *v towards *v + diff by at most 0.5 */
static void Sumo_Approach(f32 *v, f32 diff) {
    if ((f64)diff < -0.5) *v = (f32)((f64)*v - 0.5);
    else if (0.5 < (f64)diff) *v = (f32)((f64)*v + 0.5);
    else *v = diff + *v;
}

/* a fighter landing dist away shakes a still camera */
static void Sumo_Land(f32 *c, f32 dist) {
    if (c[1] == 0.0f) {
        c[2] = 100.0f / dist * 45.0f;
        if (5.0f < c[2]) c[1] = 8.0f;
    }
}

/* 0x8001FFC8: Sumo_Update */
void func_8001FFC8(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    f32 *a = B_800DC910;
    s32 *b = B_800DC960;
    f32 tilt = 0.0f, turn = 0.0f;
    f32 d1[4], d2[4];
    s32 k;
    (void)bank;
    if (tick == 0) {
        b[1] = 13;
        b[4] = (s32)guRandom() % 2;
        b[5] = (s32)guRandom() % 2;
        b[6] = (s32)guRandom() % 2;
        b[7] = (s32)guRandom() % 2;
        b[8] = (s32)guRandom() % 1500;
        b[9] = (s32)guRandom() % 1500;
        b[10] = (s32)guRandom() % 1500;
        b[11] = (s32)guRandom() % 1500;
        a[0] = a[1] = a[4] = a[3] = a[5] = a[6] = 0.0f;
        a[9] = a[8] = a[10] = a[11] = a[14] = a[13] = 0.0f;
        a[15] = a[16] = a[19] = a[18] = 0.0f;
        b[3] = 0;
        b[2] = 0;
        b[14] = 0;
        b[13] = 0;
        b[12] = 0;
        func_8001ADE8();                                 /* dust falling from the roof */
        func_8001AE70(-1500.0f, 1500.0f, 0.0f, 1000.0f, -1500.0f, 1500.0f);
        func_8001AEAC(0.3f);
    }
    func_8001AEB8();
    if (ctx->idle == 0 && ctx->zone == NULL) {
        f32 *p;
        f32 d;
        /* camera 1 */
        d1[0] = Sumo_Dist(ctx->fighter1, 340.0f, 919.0f);
        d2[0] = Sumo_Dist(ctx->fighter2, 340.0f, 919.0f);
        if (b[4] != 0) { d = d2[0]; p = ctx->fighter2; } else { d = d1[0]; p = ctx->fighter1; }
        tilt = Sumo_Tilt(d, p);
        if (-919.0f < p[2]) {
            turn = (p[0] + 340.0f) * 17.0f / (p[2] + 919.0f);
            if (45.0f < turn) turn = 45.0f;
            else if (turn < -45.0f) turn = -45.0f;
        }
        turn = turn - a[4];
        tilt = tilt - a[3];
        Sumo_Approach(&a[4], turn);
        Sumo_Approach(&a[3], tilt);
        /* camera 2 */
        d1[1] = Sumo_Dist(ctx->fighter1, 874.0f, -436.0f);
        d2[1] = Sumo_Dist(ctx->fighter2, 874.0f, -436.0f);
        if (b[5] != 0) { d = d2[1]; p = ctx->fighter2; } else { d = d1[1]; p = ctx->fighter1; }
        tilt = Sumo_Tilt(d, p);
        if (p[2] < 436.0f) {
            turn = (p[0] + 874.0f) * 17.0f / (p[2] - 436.0f);
            if (25.0f < turn) turn = 25.0f;
            else if (turn < -45.0f) turn = -45.0f;
        }
        turn = turn - a[9];
        tilt = tilt - a[8];
        Sumo_Approach(&a[9], turn);
        Sumo_Approach(&a[8], tilt);
        /* camera 3 */
        d1[2] = Sumo_Dist(ctx->fighter1, -866.0f, -432.0f);
        d2[2] = Sumo_Dist(ctx->fighter2, -866.0f, -432.0f);
        if (b[6] != 0) { d = d2[2]; p = ctx->fighter2; } else { d = d1[2]; p = ctx->fighter1; }
        tilt = Sumo_Tilt(d, p);
        if (p[2] < 432.0f) {
            turn = (p[0] - 866.0f) * 17.0f / (p[2] - 432.0f);
            if (45.0f < turn) turn = 45.0f;
            else if (turn < -25.0f) turn = -25.0f;
        }
        turn = turn - a[14];
        tilt = tilt - a[13];
        Sumo_Approach(&a[14], turn);
        Sumo_Approach(&a[13], tilt);
        /* camera 4 (the original puts the tilt it wants straight into the camera, and
         * moves it by what was left of camera 3's) */
        d1[3] = Sumo_Dist(ctx->fighter1, -897.0f, 438.0f);
        d2[3] = Sumo_Dist(ctx->fighter2, -897.0f, 438.0f);
        if (b[7] != 0) { d = d2[3]; p = ctx->fighter2; } else { d = d1[3]; p = ctx->fighter1; }
        a[18] = Sumo_Tilt(d, p);
        if (p[0] < 897.0f) {
            turn = (p[2] + 438.0f) * -17.0f / (p[0] - 897.0f);
            if (45.0f < turn) turn = 45.0f;
            else if (turn < -45.0f) turn = -45.0f;
        }
        turn = turn - a[19];
        tilt = tilt - a[18];
        Sumo_Approach(&a[19], turn);
        Sumo_Approach(&a[18], tilt);
        /* a fighter landing shakes the cameras */
        if (b[2] != 0) {
            if (ctx->fighter1[1] == 0.0f) {
                b[2] = 0;
                for (k = 0; k < 4; k++) Sumo_Land(&a[k * 5], d1[k]);
            }
        } else if (ctx->fighter1[1] != 0.0f) {
            b[2] = 1;
        }
        if (b[3] != 0) {
            if (ctx->fighter2[1] == 0.0f) {
                b[3] = 0;
                for (k = 0; k < 4; k++) Sumo_Land(&a[k * 5], d2[k]);
            }
        } else if (ctx->fighter2[1] != 0.0f) {
            b[3] = 1;
        }
        for (k = 0; k < 4; k++) {
            f32 *c = &a[k * 5];
            if (c[1] == 0.0f) continue;
            c[0] = vsSin((s32)((f64)(c[1] * 65536.0f) / 360.0)) * c[2];
            c[1] = c[1] + 8.0f;
            if (180.0f < c[1]) {
                c[2] = c[2] / 2.0f;
                if (c[2] < 5.0f) {
                    c[1] = 0.0f;
                    c[0] = 0.0f;
                } else {
                    c[1] = 8.0f;
                }
            }
        }
        /* now and then a camera turns to the other fighter */
        for (k = 0; k < 4; k++) {
            if (--b[8 + k] == 0) {
                b[4 + k] ^= 1;
                b[8 + k] = (s32)guRandom() % 1500;
            }
        }
    }
    b[0] = tick * 10;
    if (b[0] % 24 == 0) {
        if (++b[1] == 16) b[1] = 13;
    }
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "sumo_3", &b[12]);
    DoorAngle(ctx, "sumo_2", &b[13]);
    DoorAngle(ctx, "sumo_1", &b[14]);
}

/* a Sumo door half: hinge at (x, y, z) (mirrored by (sx, sy, sz) unless 0), turned by deg */
static void Sumo_Door(BkgBank *bank, ArenaContext *ctx, f32 x, f32 y, f32 z, f32 sx, f32 sy, f32 sz, f32 deg,
                      u32 set, u32 clear) {
    u32 *door;
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, x, y, z);
    if (sx != 0.0f) func_80088300(B_800DC890, sx, sy, sz);
    func_800883F0(B_800DC890, ANGLE(deg));
    func_80088350(B_800DC890, -1207.0f, 0.0f, -337.0f);
    func_80080170(&gDL, B_800DC890);
    door = (u32 *)PART(bank, 5);
    door[1] |= set;
    door = (u32 *)PART(bank, 5);
    door[1] &= ~clear;
    func_8001ABF0(PART(bank, 5), ctx);
}

/* a Sumo security camera (c: its five floats): the bracket, the arm and the camera itself.
 * (x, z): where it hangs, k: how far it shakes, off: the bracket's turn, (mx, mz): the pivot
 * the original moves to (cameras 3 and 4 reuse 1 and 2's); shake2: the shake the arm moves
 * by (camera 2's arm uses camera 1's in the original) */
static void Sumo_DrawCamera(BkgBank *bank, ArenaContext *ctx, f32 *c, f32 x, f32 z, f64 off, f32 mx, f32 mz, s32 part,
                            f32 shake2) {
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, x, (f32)((f64)c[0] * 0.4), z);
    func_800883F0(B_800DC890, (s32)(((f64)c[4] * 0.3 + off) * 65536.0 / 360.0));
    func_80088350(B_800DC890, mx, 0.0f, mz);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, part), ctx);
    func_80088350(B_800DC890, -mx, (f32)((f64)shake2 * 0.7 + 180.0), -mz);
    func_800883F0(B_800DC890, (s32)((f64)c[4] * 0.6 * 65536.0 / 360.0));
    func_80088440(B_800DC890, ANGLE(c[3]));
    func_80088350(B_800DC890, mx, -180.0f, mz);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, part + 1), ctx);
    func_80088350(B_800DC890, -mx, c[0] + 330.0f, -mz);
    func_800883F0(B_800DC890, ANGLE(c[4]));
    func_80088440(B_800DC890, ANGLE(c[3]));
    func_80088350(B_800DC890, mx, -330.0f, mz);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, part + 2), ctx);
}

/* 0x80021A58: Sumo_Draw - the doors, the four cameras, the dust and the monitors */
void func_80021A58(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    f32 *a = B_800DC910;
    s32 *b = B_800DC960;
    f32 *up = D_800AC3D0, *right = D_800AC3DC;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 0));
        func_800831F4(&gDL, PART(bank, 1));
        func_800831F4(&gDL, PART(bank, 2));
        return;
    }
    if (pass != 2) return;
    func_800831F4(&gDL, PART(bank, 3));
    func_800831F4(&gDL, PART(bank, 12));
    func_8001ABF0(PART(bank, 4), ctx);
    Sumo_Door(bank, ctx, 1207.0f, 0.0f, 337.0f, 0.0f, 0.0f, 0.0f, (f32)b[12], 0x2000, 0x1000);
    Sumo_Door(bank, ctx, 1207.0f, 0.0f, -109.0f, 1.0f, 1.0f, -1.0f, (f32)b[12], 0x1000, 0x2000);
    Sumo_Door(bank, ctx, 229.0f, 22.0f, -1291.0f, 0.0f, 0.0f, 0.0f, (f32)b[13] + 90.0f, 0x2000, 0x1000);
    Sumo_Door(bank, ctx, -213.0f, 22.0f, -1291.0f, -1.0f, 1.0f, 1.0f, (f32)b[13] + 90.0f, 0x1000, 0x2000);
    Sumo_Door(bank, ctx, -1220.0f, 0.0f, -114.0f, 0.0f, 0.0f, 0.0f, (f32)b[14] + 180.0f, 0x2000, 0x1000);
    Sumo_Door(bank, ctx, -1220.0f, 0.0f, 322.0f, 1.0f, 1.0f, -1.0f, (f32)b[14] + 180.0f, 0x1000, 0x2000);
    Sumo_DrawCamera(bank, ctx, &a[0], -340.0f, -919.0f, -90.0, 340.0f, 919.0f, 6, a[0]);
    Sumo_DrawCamera(bank, ctx, &a[5], -874.0f, 436.0f, 90.0, 874.0f, -136.0f, 9, a[0]);
    Sumo_DrawCamera(bank, ctx, &a[10], 866.0f, 432.0f, 90.0, 340.0f, 919.0f, 6, a[10]);
    Sumo_DrawCamera(bank, ctx, &a[15], 897.0f, -438.0f, 180.0, 874.0f, -136.0f, 9, a[15]);
    func_80087E20(B_800DC890);
    func_80080170(&gDL, B_800DC890);
    func_8001B1DC((f32 *)cam, 0);                        /* the dust (the camera matrix's first row) */
    /* the monitors (all the same frame) */
    right[0] = (f32)((f64)vsCos(0xEEE) * -1.9);
    up[2] = 0.0f;
    up[0] = 0.0f;
    right[1] = 0.0f;
    right[2] = (f32)((f64)vsSin(0xEEE) * -1.9);
    up[1] = 1.5f;
    func_8001AC3C(PART(bank, b[1]), 511.0f, 456.0f, 412.0f, ctx);
    func_8001AC3C(PART(bank, b[1]), -583.0f, 456.0f, 412.0f, ctx);
    func_8001AC3C(PART(bank, b[1]), 224.0f, 496.0f, -1232.0f, ctx);
    func_8001AC3C(PART(bank, b[1]), -258.0f, 496.0f, -1232.0f, ctx);
    right[0] = (f32)((f64)vsCos(-0x305B) * -1.9);
    right[2] = (f32)((f64)vsSin(-0x305B) * -1.9);
    func_8001AC3C(PART(bank, b[1]), 236.0f, 496.0f, -1208.0f, ctx);
    func_8001AC3C(PART(bank, b[1]), -248.0f, 496.0f, -1208.0f, ctx);
    func_8001AC3C(PART(bank, b[1]), 520.0f, 456.0f, 439.0f, ctx);
    func_8001AC3C(PART(bank, b[1]), -574.0f, 456.0f, 439.0f, ctx);
}

/* 0x80022C48: Arena8_Update - falling particles (snow?) over a big area */
void func_80022C48(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)ctx; (void)bank;
    if (tick == 0) {
        func_8001ADE8();
        func_8001AE70(-3000.0f, 3000.0f, -500.0f, 1800.0f, -3000.0f, 3000.0f);
        func_8001AEAC(0.2f);
    }
    func_8001AEB8();
}

/* the lit / unlit render modes of a part (bit 0 of its geometry mode word: lighting) */
static void SetLit(BkgBank *bank, s32 i, s32 lit) {
    u32 *p;
    u32 m;
    p = (u32 *)PART(bank, i);
    if (lit) p[1] |= 1; else p[1] &= ~1u;
    m = lit ? 0x442230 : 0xC084000;
    p = (u32 *)PART(bank, i);
    p[2] = m;
    p = (u32 *)PART(bank, i);
    p[3] = m;
}

/* 0x80022CB4: Arena8_Draw - the particles fall in the part of the arena the camera is in
 * (above or below 1000) */
void func_80022CB4(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    if (pass == 1) {
        if (ctx->camPos[1] < 1000.0f || ctx->idle != 0) {
            func_800831F4(&gDL, PART(bank, 2));
            func_800831F4(&gDL, PART(bank, 0));
            func_800831F4(&gDL, PART(bank, 10));
        }
        return;
    }
    if (pass != 2) return;
    SetLit(bank, 4, ctx->idle != 0);
    SetLit(bank, 5, ctx->idle != 0);
    func_8001ABF0(PART(bank, 1), ctx);
    if (ctx->camPos[1] < 1000.0f || ctx->idle != 0) func_8001ABF0(PART(bank, 3), ctx);
    if (1200.0f < ctx->camPos[1] && ctx->idle == 0) {
        func_8001ABF0(PART(bank, 5), ctx);
        func_8001ABF0(PART(bank, 7), ctx);
        func_8001ABF0(PART(bank, 8), ctx);
        func_8001ABF0(PART(bank, 3), ctx);
        func_8001ABF0(PART(bank, 11), ctx);
        func_8001ABF0(PART(bank, 4), ctx);
        func_8001ABF0(PART(bank, 6), ctx);
    } else {
        func_8001ABF0(PART(bank, 4), ctx);
        func_8001ABF0(PART(bank, 6), ctx);
        func_8001ABF0(PART(bank, 3), ctx);
        func_8001ABF0(PART(bank, 11), ctx);
        func_8001ABF0(PART(bank, 5), ctx);
        func_8001ABF0(PART(bank, 7), ctx);
        func_8001ABF0(PART(bank, 8), ctx);
    }
    if (ctx->camPos[1] < 1000.0f || ctx->idle != 0) {
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, 0.0f, 0.0f, 10.0f);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 9), ctx);
    }
    if (ctx->idle == 0) {
        if (ctx->camPos[1] < 1000.0f) {
            func_8001AE70(-1500.0f, 2000.0f, -500.0f, 500.0f, 2000.0f, 5000.0f);
        } else {
            func_8001AE70(-1000.0f, 0.0f, 1000.0f, 1800.0f, 0.0f, 1000.0f);
        }
    }
    func_80087E20(B_800DC890);
    func_80080170(&gDL, B_800DC890);
    func_8001B1DC((f32 *)cam, 0);
}

/* 0x800230B4: Arena24_Update - nothing (an unnamed empty function at the end of
 * func_80022CB4 in the original; the update table points at it) */
void func_800230B4(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)tick; (void)ctx; (void)bank;
}

/* 0x800230BC: Arena24_Draw */
void func_800230BC(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 0));
    } else if (pass == 0) {
        func_800831F4(&gDL, PART(bank, 1));
        SetLit(bank, 0, ctx->idle != 0);
        func_8001ABF0(PART(bank, 2), ctx);
    } else if (pass == 2) {
        func_800831F4(&gDL, PART(bank, 4));
        func_800831F4(&gDL, PART(bank, 3));
    }
}

/* 0x800231E0: Arena25_Update - the machinery's angle (an unnamed function inside
 * func_800230BC in the original; the update table points at it) */
void func_800231E0(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    (void)ctx; (void)bank;
    B_800DC960[0] = tick * 10 + 200000;
}

extern u8 *B_800F5B6C;                                    /* this frame's matrices */
void func_80096D30(void *m, void *mtx);                   /* guMtxF2L */

/* B_800DC890 as this frame's next matrix, multiplied into the model matrix */
static void PushMatrix(void) {
    u32 *p;
    func_80096D30(B_800DC890, B_800F5B6C);
    p = gDL;
    gDL = p + 2;
    p[0] = 0x01020040;                                   /* G_MTX modelview, multiply, push */
    p[1] = (u32)B_800F5B6C - 0x80000000u;
    B_800F5B6C += 0x40;
}

/* 0x80023204: Arena25_Draw - two levers following the fighters, and turning gear wheels */
void func_80023204(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    f32 s;
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 1));
        func_800831F4(&gDL, PART(bank, 5));
        func_800831F4(&gDL, PART(bank, 0));
        return;
    }
    if (pass != 2) return;
    func_800831F4(&gDL, PART(bank, 2));
    if (ctx->idle != 0) {
        func_800831F4(&gDL, PART(bank, 3));
        func_800831F4(&gDL, PART(bank, 4));
        func_800831F4(&gDL, PART(bank, 12));
        func_800831F4(&gDL, PART(bank, 13));
    } else {
        func_8001ABF0(PART(bank, 3), ctx);
        func_8001ABF0(PART(bank, 4), ctx);
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, 661.0f, 392.0f, 33.0f);
        func_800883F0(B_800DC890, ANGLE(ctx->fighter1[2] / 10.0f + -90.0f));
        func_80088350(B_800DC890, -661.0f, -392.0f, -33.0f);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 12), ctx);
        func_80087E20(B_800DC890);
        func_80087EE8(B_800DC890, 515.0f, 269.0f, -513.0f);
        func_800883F0(B_800DC890, ANGLE(ctx->fighter2[2] / 15.0f + -60.0f));
        func_80088350(B_800DC890, -515.0f, -269.0f, 513.0f);
        func_80080170(&gDL, B_800DC890);
        func_8001ABF0(PART(bank, 13), ctx);
    }
    func_80087E20(B_800DC890);
    func_800883F0(B_800DC890, b[0] << 2);
    PushMatrix();
    func_800831F4(&gDL, PART(bank, 8));
    func_800831F4(&gDL, PART(bank, 11));
    func_80087E20(B_800DC890);
    func_800883F0(B_800DC890, -(b[0] * 3));
    PushMatrix();
    func_800831F4(&gDL, PART(bank, 9));
    s = vsSin(ANGLE((f32)b[0] / 5.0f));
    func_800883A0(B_800DC890, (s32)(s * 546.0f));
    s = vsSin(ANGLE((f32)b[0] / 5.0f));
    func_80088440(B_800DC890, (s32)(s * 1092.0f));
    PushMatrix();
    func_800831F4(&gDL, PART(bank, 7));
    func_80087E20(B_800DC890);
    func_800883F0(B_800DC890, -b[0]);
    s = vsSin(ANGLE((f32)b[0] / 10.0f));
    func_800883A0(B_800DC890, (s32)(s * 910.0f));
    s = vsSin(ANGLE((f32)b[0] / 10.0f));
    func_80088440(B_800DC890, (s32)(s * 1820.0f));
    PushMatrix();
    func_800831F4(&gDL, PART(bank, 14));
    func_80087E20(B_800DC890);
    func_800883F0(B_800DC890, b[0] << 1);
    PushMatrix();
    func_800831F4(&gDL, PART(bank, 10));
    func_80087E20(B_800DC890);
    func_800883F0(B_800DC890, b[0] << 1);
    PushMatrix();
}

/* 0x80023934: LabUp_Update - B_800DC960: [0] tick*2, [1] a 10-frame animation (a frame
 * every 8 ticks), [2] counts its loops (0..3), [3]/[4] the doors (zones "lab_up2" and
 * "lab_up1"); B_800DC910[0..5]: jitter (-1..1, every other tick) */
void func_80023934(s32 tick, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    f32 *a = B_800DC910;
    s32 i;
    (void)bank;
    if (tick == 0) {
        b[1] = 0;
        b[2] = 0;
        a[2] = a[1] = a[0] = 0.0f;
        a[5] = a[4] = a[3] = 0.0f;
        b[4] = 0;
        b[3] = 0;
    }
    b[0] = tick << 1;
    if ((tick & 7) == 0) {
        if (++b[1] == 10) {
            b[1] = 0;
            if (++b[2] == 4) b[2] = 0;
        }
    }
    if (tick & 1) {
        for (i = 0; i < 6; i++) a[i] = (f32)(1 - (s32)guRandom() % 3);
    }
    if (ctx->zone == NULL) return;
    DoorAngle(ctx, "lab_up2", &b[3]);
    DoorAngle(ctx, "lab_up1", &b[4]);
}

/* a LabUp sprite whose position moves with its width (img + 6), along the camera's right */
static void LabUp_Sprite(BkgBank *bank, ArenaContext *ctx, s32 i, f64 k, f64 x, f64 z) {
    u8 *img = PART(bank, i);
    f64 w = (f64)*(s16 *)(img + 6) * k;
    func_8001AC3C(img, (f32)(x - w * (f64)ctx->camRight[0]), 192.0f, (f32)(z - w * (f64)ctx->camRight[2]), ctx);
}

/* 0x80023C58: LabUp_Draw */
void func_80023C58(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank) {
    s32 *b = B_800DC960;
    f32 *a = B_800DC910;
    f32 *up = D_800AC3D0, *right = D_800AC3DC;
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, PART(bank, 1));
        func_8001ACEC((u32 *)PART(bank, 0), ctx, 0);
        return;
    }
    if (pass != 2) return;
    func_800831F4(&gDL, PART(bank, 2));
    func_8001ABF0(PART(bank, 3), ctx);
    func_80087E20(B_800DC890);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 5), ctx);
    func_8001ABF0(PART(bank, 60), ctx);
    func_8001ABF0(PART(bank, 61), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, -217.0f, 0.0f, -606.0f);
    func_800883F0(B_800DC890, -0x78E3);
    func_80088350(B_800DC890, -403.0f, 0.0f, 488.0f);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 5), ctx);
    /* the doors */
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, -661.0f, 0.0f, 122.0f);
    func_800883F0(B_800DC890, ANGLE((f32)b[3]));
    func_80088350(B_800DC890, 661.0f, 0.0f, -122.0f);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 4), ctx);
    func_80087E20(B_800DC890);
    func_80087EE8(B_800DC890, -610.0f, 0.0f, -276.0f);
    func_800883F0(B_800DC890, ANGLE((f32)b[4] - 36.0f));
    func_80088350(B_800DC890, 661.0f, 0.0f, -122.0f);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 4), ctx);
    /* a machine shaking once its animation has looped twice */
    func_80087E20(B_800DC890);
    if (b[2] >= 2) func_80087EE8(B_800DC890, a[3], 0.0f, a[2]);
    func_80080170(&gDL, B_800DC890);
    func_8001ABF0(PART(bank, 59), ctx);
    /* sprites */
    right[0] = (f32)((f64)vsCos(0xC16) * -1.4);
    right[2] = (f32)((f64)vsSin(0xC16) * -1.4);
    right[1] = 0.0f;
    up[2] = 0.0f;
    up[0] = 0.0f;
    up[1] = 0.9f;
    func_8001AC3C(PART(bank, b[2] + 55), 155.0f, 99.0f, -377.0f, ctx);
    right[0] = (f32)((f64)ctx->camRight[0] * -2.2);
    right[2] = (f32)((f64)ctx->camRight[2] * -2.2);
    up[1] = 2.3f;
    func_80087E20(B_800DC890);
    func_80080170(&gDL, B_800DC890);
    /* four screens, drawn back to front */
    if (ctx->camRight[2] < 0.0f) {
        LabUp_Sprite(bank, ctx, b[1] + 10, 1.1, -257.0, -488.0);
        func_8001ABF0(PART(bank, 6), ctx);
        LabUp_Sprite(bank, ctx, b[1] + 20, 1.1, -159.0, -500.0);
        func_8001ABF0(PART(bank, 7), ctx);
        LabUp_Sprite(bank, ctx, b[1] + 30, 1.1, -49.0, -500.0);
        func_8001ABF0(PART(bank, 8), ctx);
        LabUp_Sprite(bank, ctx, b[1] + 40, 1.1, 374.0, -298.0);
        func_8001ABF0(PART(bank, 9), ctx);
    } else {
        LabUp_Sprite(bank, ctx, b[1] + 40, 1.1, 374.0, -298.0);
        func_8001ABF0(PART(bank, 9), ctx);
        LabUp_Sprite(bank, ctx, b[1] + 30, 1.1, -49.0, -500.0);
        func_8001ABF0(PART(bank, 8), ctx);
        LabUp_Sprite(bank, ctx, b[1] + 20, 1.1, -159.0, -500.0);
        func_8001ABF0(PART(bank, 7), ctx);
        LabUp_Sprite(bank, ctx, b[1] + 10, 1.1, -257.0, -488.0);
        func_8001ABF0(PART(bank, 6), ctx);
    }
    right[0] = (f32)((f64)vsCos(0x888) * -1.9);
    right[2] = (f32)((f64)vsSin(0x888) * -1.9);
    up[1] = 1.6f;
    func_8001AC3C(PART(bank, b[1] % 5 + 50), -2.0f, 431.0f, -579.0f, ctx);
}
