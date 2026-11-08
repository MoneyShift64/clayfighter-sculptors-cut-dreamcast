/*
 * Per-arena animation scripts, part 1 (0x8000D2B0-0x8001ABF0).
 *
 * Every arena has two entries in the tables at 0x800AB3CC / 0x800AB434 (indexed by the arena
 * number, B_800F7EC8):
 *   update(tick, info, bank)          tick 0 = set-up (from the loader), -1 = tear-down,
 *                                     1, 2, ... = once a frame (func_8000B894)
 *   draw(cam, pass, info, bank)       pass 0..3 = the four drawing passes (func_8000B998,
 *                                     BCA8, BE80, C07C)
 * "bank" is the arena's object bank (D_800AB4A0): object i starts at bank + bank[0xC + 8 i].
 * "info" (0x6C bytes, filled by func_8000CEDC from the combat handler) tells the scripts
 * whether a fight is on, where the fighters are and whether a fighter is being knocked out
 * through one of the arena's ring-out zones (doors that open, things that move ...).
 *
 * This file: the drawing helpers, a lightning flash, falling snow, a wobble, and the
 * scripts of arenas 0 (candy), 1 (hangar), 2, 4 (Houngan inside), 5 (Houngan outside), 6, 7,
 * 13, 15, 17, 20, 21 and 22.
 */
#include "common.h"
#include "combat.h"

/* ---- what func_8000CEDC tells the scripts (0x6C bytes on the caller's stack) ---- */
typedef struct Scr1_Info {
    /* 0x00 */ s32 inFight;            /* 0: no combat handler (screens outside a fight) */
    /* 0x04 */ s32 notStarted;         /* no fight, or the round hasn't started (handler 0x54 == 0) */
    /* 0x08 */ s32 v8;                 /* frames since the round state reached 4 */
    /* 0x0C */ s32 state;              /* the handler's round state (0x54) */
    /* 0x10 */ s32 v10;                /* handler 0x138 */
    /* 0x14 */ f32 pos1[3];            /* fighter 1 (x, -y, z) */
    /* 0x20 */ f32 pos2[3];            /* fighter 2 */
    /* 0x2C */ f32 camPt[3];           /* the camera's position */
    /* 0x38 */ f32 v38, v3C;           /* handler 0x00 / 0x08 (camera direction) */
    /* 0x40 */ f32 v40[3], v4C[3];     /* camera axes */
    /* 0x58 */ u8 *zone;               /* the ring-out zone being used (name at +4), or NULL */
    /* 0x5C */ s32 v5C;                /* round state was 10 when the zone was entered */
    /* 0x60 */ f32 v60;                /* time since the zone was entered */
    /* 0x64 */ s32 ringOut;            /* the fighter has gone through (handler 0x148) */
    /* 0x68 */ f32 ringOutTime;        /* time since then */
} Scr1_Info;

#define OBJ(bank, i) ((u8 *)(bank) + *(s32 *)((u8 *)(bank) + 0xC + 8 * (i)))

/* ---- elsewhere ---- */
void func_800831F4(u32 **dl, void *obj);                  /* draw an object (plain) */
void func_8000C2B8(void *obj, Scr1_Info *info);           /* draw an object (lit, faded) */
void func_8000C9C8(void *image, Scr1_Info *info, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void func_800882C4(void *m, void *place);
void func_80087DD0(f32 *v);
s32  mpStrEqualNoCase(const char *a, const char *b);
extern void *B_800F7E6C;                                  /* the menu sound bank */

/* ---- 0x8000D2B0: Scr1_DrawObj - draw an object, lit unless a fight is in a ring-out ---- */
void func_8000D2B0(u32 **dl, Scr1_Info *info, void *obj) {
    (void)dl;
    if (info->notStarted != 0 || info->zone != NULL) func_800831F4(&gDL, obj);
    else func_8000C2B8(obj, info);
}

/* ---- 0x8000D2FC: Scr1_DrawObj2 - the same, but lit whenever no zone is in use ---- */
void func_8000D2FC(u32 **dl, Scr1_Info *info, void *obj) {
    (void)dl;
    if (info->zone != NULL) func_800831F4(&gDL, obj);
    else func_8000C2B8(obj, info);
}

/* ---- 0x8000D33C: Scr1_DrawImage - a flat picture, lit like D2B0 ---- */
void func_8000D33C(u32 **dl, Scr1_Info *info, void *image, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                   s32 flags) {
    (void)dl;
    if (info->notStarted != 0 || info->zone != NULL) func_80083A64(&gDL, image, x, y, z, up, right, flags);
    else func_8000C9C8(image, info, x, y, z, up, right, flags);
}

/* ---- 0x8000D3D0: Scr1_DrawImageFlag - always plain, flag 0x10000 when it would be lit ---- */
void func_8000D3D0(u32 **dl, Scr1_Info *info, void *image, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                   s32 flags) {
    (void)dl;
    if (!(info->notStarted != 0 || info->zone != NULL)) flags |= 0x10000;
    func_80083A64(&gDL, image, x, y, z, up, right, flags);
}

/* ---- 0x8000D454: Scr1_DrawSeeThrough - patches the object's render mode (see-through
   while a zone is in use), then draws it (lit when 'lit') ---- */
void func_8000D454(u32 *obj, Scr1_Info *info, s32 lit) {
    if (info->zone != NULL) {
        obj[1] = 0x2205; obj[2] = 0x00442230; obj[3] = 0x00112230;
    } else {
        obj[1] = 0x2204; obj[2] = 0x0C084000; obj[3] = 0x03024000;
    }
    if (lit) func_8000D2B0(&gDL, info, obj);
    else func_800831F4(&gDL, obj);
}

/* ---- 0x8000D4E0: Scr1_DrawSeeThrough2 - the same with another render mode ---- */
void func_8000D4E0(u32 *obj, Scr1_Info *info, s32 lit) {
    u32 m;
    if (info->zone != NULL) { obj[1] = 0x2205; m = 0xC8004030; }
    else { obj[1] = 0x2204; m = 0xC8004000; }
    obj[2] = m; obj[3] = m;
    if (lit) func_8000D2B0(&gDL, info, obj);
    else func_800831F4(&gDL, obj);
}

/* ---- lightning (Houngan's arenas): a flash that rises to a peak and fades ---- */
extern f32 B_800DC180;     /* peak brightness */
extern f32 B_800DC184;     /* rise per frame */
extern f32 B_800DC188;     /* fall per frame */
extern s16 B_800DC18C;     /* frames left rising */
extern f32 B_800DC190;     /* brightness */
extern f32 B_800DC194;     /* brightness with flicker */

/* 0x8000D558: Lightning_Init */
void func_8000D558(void) {
    B_800DC180 = 1.0f;
    B_800DC184 = 0.1f;
    B_800DC188 = 0.1f;
    B_800DC18C = 0;
    B_800DC190 = 0.0f;
    B_800DC194 = 0.0f;
}

/* 0x8000D5A0: Lightning_Strike - starts a flash (unless one is still rising) */
void func_8000D5A0(void) {
    f32 peak;
    if (B_800DC18C > 0) return;
    if ((s32)guRandom() % 16 < 3) peak = 1.0f;
    else peak = (f32)((f64)(f32)((s32)guRandom() % 700 + 300) / 1000.0);
    B_800DC180 = peak;
    peak = B_800DC180;
    B_800DC184 = peak * peak * peak;
    if ((f64)peak > 0.5) B_800DC188 = 0.018f;
    else B_800DC188 = (f32)((1.1 - (f64)B_800DC180) * 0.03);
    B_800DC18C = (s16)((s32)guRandom() % 10 + 20);
}

/* 0x8000D728: Lightning_Update */
void func_8000D728(void) {
    f32 v;
    if (B_800DC18C > 0) {
        B_800DC18C = B_800DC18C - 1;
        B_800DC190 = B_800DC190 + B_800DC184;
        if (B_800DC180 < B_800DC190) {
            B_800DC190 = B_800DC180;
            if ((f64)B_800DC180 < 1.0) B_800DC18C = 0;
        }
    } else {
        if (0.0f < B_800DC190) B_800DC190 = B_800DC190 - B_800DC188;
        if ((f64)B_800DC190 < 0.0) B_800DC190 = 0.0f;
    }
    v = B_800DC190;
    B_800DC194 = v;
    if (0.0 < (f64)v && (f64)v < 1.0) {
        s32 r = (s32)guRandom() % 600 - 300;
        B_800DC194 = (f32)((f64)B_800DC194 + (f64)(f32)r / 1000.0 * (f64)B_800DC190);
        if (1.0 < (f64)B_800DC194) B_800DC194 = 1.0f;
        if ((f64)B_800DC194 < 0.0) B_800DC194 = 0.0f;
    }
}

/* ---- falling snow: 300 flakes (0x14 bytes each) ---- */
typedef struct Scr1_Flake {
    /* 0x00 */ f32 pos[3];
    /* 0x0C */ f32 size;
    /* 0x10 */ s32 active;
} Scr1_Flake;
extern Scr1_Flake B_800F6000[300];
extern f32 B_800F5BB0;         /* flakes made per frame */
extern f32 B_800F4E4C;         /* ... the fraction carried over */
extern f32 B_800F5B9C[3];      /* fall velocity */
extern f32 B_800F881C, B_800F4E5C;   /* x range where flakes start */
extern f32 B_800F15F4, B_800F4EDC;   /* z range */
extern f32 B_800F8824;         /* the height they start at */
extern f32 B_800F7D8C;         /* the height they vanish at */

/* 0x8000D94C: Snow_Init */
void func_8000D94C(void) {
    s32 i;
    for (i = 0; i < 300; i++) B_800F6000[i].active = 0;
    B_800F5BB0 = 10.0f;
    B_800F4E4C = 0.0f;
    B_800F5B9C[0] = -70.0f;
    B_800F5B9C[1] = -250.0f;
    B_800F5B9C[2] = -70.0f;
    B_800F881C = -2000.0f;
    B_800F4E5C = 2000.0f;
    B_800F15F4 = -2000.0f;
    B_800F4EDC = 2000.0f;
    B_800F8824 = 1000.0f;
    B_800F7D8C = 0.0f;
}

/* 0x8000DA08: Snow_Update - new flakes in the first free slots, then every flake falls */
void func_8000DA08(void) {
    s32 xr = (s32)(B_800F4E5C - B_800F881C);
    s32 zr = (s32)(B_800F4EDC - B_800F15F4);
    s32 i, n;
    f32 acc, floorY;
    acc = B_800F4E4C + B_800F5BB0;
    B_800F4E4C = acc;
    if ((f64)acc >= 1.0) {
        n = (s32)acc;
        B_800F4E4C = acc - (f32)n;
        for (n--; n >= 0; n--) {
            Scr1_Flake *f;
            for (i = 0; i < 300 && B_800F6000[i].active != 0; i++) {}
            if (i >= 300) break;
            f = &B_800F6000[i];
            f->pos[0] = (f32)((s32)guRandom() % xr) + B_800F881C;
            f->pos[1] = B_800F8824;
            f->pos[2] = (f32)((s32)guRandom() % zr) + B_800F15F4;
            f->active = 1;
            f->size = (f32)((s32)guRandom() % 300 + 100);
        }
    }
    floorY = B_800F7D8C;
    for (i = 0; i < 300; i++) {
        Scr1_Flake *f = &B_800F6000[i];
        if (f->active == 0) continue;
        f->pos[0] = f->pos[0] + B_800F5B9C[0];
        f->pos[1] = f->pos[1] + B_800F5B9C[1];
        f->pos[2] = f->pos[2] + B_800F5B9C[2];
        if (f->pos[1] < floorY) f->active = 0;
    }
}

/* 0x8000DD0C: Snow_Draw - each flake is a streak (a triangle) along its fall direction;
   'side' is a direction across the view (only x and z are used) */
void func_8000DD0C(f32 *side) {
    f32 s[3], d[3];
    s32 i;
    s[0] = side[0]; s[1] = side[1]; s[2] = side[2];
    s[1] = 0.0f;
    func_80087DD0(s);
    d[0] = B_800F5B9C[0]; d[1] = B_800F5B9C[1]; d[2] = B_800F5B9C[2];
    func_80087DD0(d);
    *gDL++ = 0xE7000000; *gDL++ = 0;
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x004049D8, 0x001049D8);
    func_8007FBE8(&gDL, 1);
    for (i = 0; i < 300; i++) {
        Scr1_Flake *f = &B_800F6000[i];
        Vtx16 *v;
        u32 *g;
        if (f->active == 0) continue;
        v = gVtx;
        v[0].x = (s16)(s32)f->pos[0];
        v[0].y = (s16)(s32)f->pos[1];
        v[0].z = (s16)(s32)f->pos[2];
        v[0].flag = 0; v[0].s = 0; v[0].t = 0;
        v[0].r = 100; v[0].g = 100; v[0].b = 150; v[0].a = 0;
        v[1].x = (s16)(s32)((f64)(f->size * d[0] + f->pos[0]) + (f64)s[0]);
        v[1].y = (s16)(s32)((f64)(f->size * d[1] + f->pos[1]) + (f64)s[1]);
        v[1].z = (s16)(s32)((f64)(f->size * d[2] + f->pos[2]) + (f64)s[2]);
        v[1].flag = 0; v[1].s = 0; v[1].t = 0;
        v[1].r = 200; v[1].g = 200; v[1].b = 255; v[1].a = 128;
        v[2].x = (s16)(s32)((f64)(f->size * d[0] + f->pos[0]) - (f64)s[0]);
        v[2].y = (s16)(s32)((f64)(f->size * d[1] + f->pos[1]) - (f64)s[1]);
        v[2].z = (s16)(s32)((f64)(f->size * d[2] + f->pos[2]) - (f64)s[2]);
        v[2].flag = 0; v[2].s = 0; v[2].t = 0;
        v[2].r = 230; v[2].g = 230; v[2].b = 230; v[2].a = 140;
        g = gDL;
        gDL = g + 2;
        g[0] = 0x04000C2F;                  /* gSPVertex(3 vertices) */
        g[1] = (u32)gVtx;
        gVtx = gVtx + 3;
        gDL = g + 4;
        g[2] = 0xBF000000; g[3] = 0x204;    /* gSP1Triangle(0, 1, 2) */
    }
    *gDL++ = 0xE7000000; *gDL++ = 0;
}

/* ---- a wobble: a spring that chases a random target (B_800F84BC) ---- */
extern f32 B_800F8968[2];
extern f32 B_800F5AE8;     /* target */
extern u16 B_800F2DD0;     /* frames until a new target */
extern f32 B_800F84BC;     /* position */
extern f32 B_800F5BDC;     /* velocity */

/* 0x8000E128: Wobble_Init */
void func_8000E128(void) {
    B_800F8968[0] = -1.0f;
    B_800F8968[1] = -1.0f;
    B_800F5AE8 = 0.0f;
    B_800F2DD0 = 0;
    B_800F84BC = 0.0f;
    B_800F5BDC = 0.0f;
}

/* 0x8000E168: Wobble_Update */
void func_8000E168(void) {
    f64 v;
    B_800F2DD0 = (u16)(B_800F2DD0 - 1);
    if ((s16)B_800F2DD0 <= 0) {
        B_800F5AE8 = (f32)((f64)(f32)((s32)guRandom() % 1024) / 1024.0);
        B_800F2DD0 = (u16)((s32)guRandom() % 180 + 20);
    }
    /* (odd: the push is proportional to the target, not to the distance from it) */
    if (B_800F84BC < B_800F5AE8) v = (f64)B_800F5BDC + (f64)B_800F5AE8 * 0.001;
    else v = (f64)B_800F5BDC - (f64)B_800F5AE8 * 0.0007;
    B_800F5BDC = (f32)v;
    B_800F5BDC = (f32)((f64)B_800F5BDC * 0.98);
    B_800F84BC = B_800F84BC + B_800F5BDC;
}

/* ---- angles the scripts turn things by ---- */
extern s32 B_800DBCC0[5];
extern u8 B_800DBBF0[];            /* the scripts' working matrix */
extern u8 B_800DBD00[], B_800DBD40[], B_800DBD80[], B_800DBDC0[], B_800DBE00[];
extern u8 B_800DBE40[], B_800DBE80[], B_800DBEC0[], B_800DBF00[], B_800DBF40[];

/* the angle a ring-out door has turned to after t (degrees = t * speed) */
static s32 Scr1_DoorAngle(f32 t, f64 speed) {
    return (s32)((f64)((f32)((f64)t * speed) * 65536.0f) / 360.0);
}

/* set the working matrix to 'place' turned by 'angle' (tilt), and use it */
static void Scr1_Tilt(void *place, s32 angle) {
    func_80087E70(B_800DBBF0, place);
    func_8008861C(B_800DBBF0);
    func_800883F0(B_800DBBF0, angle);
    func_800882C4(B_800DBBF0, place);
    func_80080170(&gDL, B_800DBBF0);
}

/* ---- arena 0 (candy) ---- */
extern f32 D_800AB4EC[16], D_800AB52C[16], D_800AB56C[16], D_800AB5AC[16], D_800AB5EC[16];   /* matrices (4 x 4 floats) */

/* 0x8000E2D0: Candy_Update - five wheels; the first is the ring-out door */
void func_8000E2D0(s32 tick, Scr1_Info *info, u8 *bank) {
    (void)bank;
    if (tick == 0) {
        B_800DBCC0[0] = 0; B_800DBCC0[1] = 0; B_800DBCC0[2] = 0; B_800DBCC0[3] = 0; B_800DBCC0[4] = 0;
    }
    B_800DBCC0[4] += 0xB6;
    if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "candy1") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DBCC0[0] = 0x471C;
        else B_800DBCC0[0] = Scr1_DoorAngle(info->ringOutTime, 4.0);
    }
}

/* 0x8000E3D0: Candy_Draw */
void func_8000E3D0(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    if (pass == 1) {
        func_8000D454((u32 *)OBJ(bank, 0), info, 0);
        func_8000D454((u32 *)OBJ(bank, 1), info, 0);
        func_800831F4(&gDL, OBJ(bank, 2));
    } else if (pass == 2) {
        func_800831F4(&gDL, OBJ(bank, 3));
        func_800831F4(&gDL, OBJ(bank, 4));
        Scr1_Tilt(D_800AB4EC, B_800DBCC0[0]);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
        Scr1_Tilt(D_800AB52C, B_800DBCC0[1]);
        func_800831F4(&gDL, OBJ(bank, 6));
        Scr1_Tilt(D_800AB56C, B_800DBCC0[2]);
        func_800831F4(&gDL, OBJ(bank, 7));
        Scr1_Tilt(D_800AB5AC, B_800DBCC0[3]);
        func_800831F4(&gDL, OBJ(bank, 8));
        Scr1_Tilt(D_800AB5EC, B_800DBCC0[4]);
        func_800831F4(&gDL, OBJ(bank, 9));
        func_80087E20(B_800DBBF0);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 10));
    }
}

/* ---- arena 1 (hangar) ---- */
extern f32 D_800AB62C[16], D_800AB66C[16], D_800AB6AC[16], D_800AB6EC[16], D_800AB72C[16];   /* matrices (4 x 4 floats) */

/* 0x8000E6EC: Hangar_Update */
void func_8000E6EC(s32 tick, Scr1_Info *info, u8 *bank) {
    (void)bank;
    if (tick == 0) {
        func_80087E70(B_800DBD00, D_800AB6AC); func_8008861C(B_800DBD00);
        func_80087E70(B_800DBD40, D_800AB6EC); func_8008861C(B_800DBD40);
        func_80087E70(B_800DBD80, D_800AB72C);
        B_800DBCC0[0] = 0; B_800DBCC0[1] = 0;
    }
    B_800DBCC0[0] += 0xB6;
    if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "hanger") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DBCC0[1] = -0x3555;
        else B_800DBCC0[1] = Scr1_DoorAngle(-info->ringOutTime, 3.0);
    }
}

/* 0x8000E838: Hangar_Draw */
void func_8000E838(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, OBJ(bank, 0));
    } else if (pass == 2) {
        func_8000D454((u32 *)OBJ(bank, 1), info, 0);
        func_800831F4(&gDL, OBJ(bank, 2));
        func_800831F4(&gDL, OBJ(bank, 3));
        func_8000D2B0(&gDL, info, OBJ(bank, 4));
        Scr1_Tilt(D_800AB62C, B_800DBCC0[1]);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
        func_80087E70(B_800DBBF0, D_800AB66C);
        func_8008861C(B_800DBBF0);
        func_80088440(B_800DBBF0, B_800DBCC0[0]);
        func_800882C4(B_800DBBF0, D_800AB66C);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 6));
        func_80087E70(B_800DBBF0, B_800DBD00);
        func_800882C4(B_800DBBF0, D_800AB6AC);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 7));
        func_80087E70(B_800DBBF0, B_800DBD40);
        func_800882C4(B_800DBBF0, D_800AB6EC);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 8));
        /* (the original draws object 8 a second time, with the third copy's matrix moved
           to the second one's place) */
        func_80087E70(B_800DBBF0, B_800DBD80);
        func_800882C4(B_800DBBF0, D_800AB6EC);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 8));
    }
}

/* ---- arena 4 (Houngan's place, inside) ---- */
/* seven things that swing when something knocks them (0x30 bytes each) */
typedef struct Scr1_Swing {
    /* 0x00 */ s16 idle;               /* frames until the next knock */
    /* 0x02 */ s16 busy;               /* frames the knock lasts */
    /* 0x04 */ f32 target;             /* how far it is pushed */
    /* 0x08 */ f32 value;              /* how far it is */
    /* 0x0C */ f32 speed;
    /* 0x10 */ f32 kx, kz;             /* the direction it moves in */
    /* 0x18 */ f32 x, y, z;            /* where that puts it */
    /* 0x24 */ s32 phase;              /* bobbing angle */
    /* 0x28 */ s32 turn;               /* its turn */
    /* 0x2C */ s32 tilt;               /* its spin (adds up) */
} Scr1_Swing;
extern Scr1_Swing B_800DC198[7];

/* four sprites that drift across the room (0x30 bytes each) */
typedef struct Scr1_Drifter {
    /* 0x00 */ s16 life;               /* frames left on screen */
    /* 0x02 */ s16 wait;               /* frames until the next one starts */
    /* 0x04 */ s32 side;               /* 1: the near side (drawn in pass 0), 0: far (pass 2) */
    /* 0x08 */ f32 pos[3];
    /* 0x14 */ f32 vel[3];
    /* 0x20 */ s16 image;              /* which of the three pictures (objects 19..21) */
    /* 0x24 */ f32 fadeIn;
    /* 0x28 */ f32 alpha;
    /* 0x2C */ u8  flags;              /* mirrored */
} Scr1_Drifter;
extern Scr1_Drifter B_800DC2E8[4];
extern f32 B_800DC100;                 /* the ring-out door's angle (degrees); arena 6: the lift */
extern f32 B_800DC104;                 /* arena 6: the floor */
extern f32 D_800AB76C[16], D_800AB7AC[16], D_800AB7EC[16], D_800AB82C[16], D_800AB86C[16];   /* matrices (4 x 4 floats) */
extern f32 D_800AB8AC[16], D_800AB8EC[16], D_800AB92C[16], D_800AB96C[16], D_800AB9AC[16];   /* matrices (4 x 4 floats) */
extern f32 D_800AB4D0[3];              /* "up" for the sprites */
f32  func_80087D80(f32 *v);            /* length */
void func_80082994(void *obj, f32 flash);   /* how much an object is lit up by the lightning */

/* 0x8000EAD8: HounganIn_Update */
void func_8000EAD8(s32 tick, Scr1_Info *info, u8 *bank) {
    s32 i;
    (void)bank;
    if (tick == 0) {
        B_800DC100 = 0.0f;
        func_8000D558();
        func_80087E70(B_800DBD00, D_800AB76C); func_8008861C(B_800DBD00);
        func_80087E70(B_800DBD40, D_800AB7AC);
        func_80087E70(B_800DBD80, D_800AB7EC); func_8008861C(B_800DBD80);
        func_80087E70(B_800DBDC0, D_800AB82C);
        func_80087E70(B_800DBE00, D_800AB86C); func_8008861C(B_800DBE00);
        B_800DBCC0[0] = 0;
        func_80087E70(B_800DBE40, D_800AB8AC); func_8008861C(B_800DBE40);
        func_80087E70(B_800DBE80, D_800AB8EC); func_8008861C(B_800DBE80);
        func_80087E70(B_800DBEC0, D_800AB92C); func_8008861C(B_800DBEC0);
        func_80087E70(B_800DBF00, D_800AB96C); func_8008861C(B_800DBF00);
        func_80087E70(B_800DBF40, D_800AB9AC);
        for (i = 0; i < 7; i++) {
            Scr1_Swing *s = &B_800DC198[i];
            s->idle = (s16)((s32)guRandom() % 1024 + 1);
            s->busy = 0;
            s->target = 0.0f; s->value = 0.0f; s->speed = 0.0f;
            s->kx = 0.0f; s->kz = 0.0f;
            s->phase = 0; s->turn = 0; s->tilt = 0;
        }
        B_800DC198[0].kx = -0.2f; B_800DC198[0].kz = 0.2f;
        B_800DC198[1].kx = 0.2f;  B_800DC198[1].kz = -0.2f;
        B_800DC198[2].kx = -0.2f; B_800DC198[2].kz = -0.2f;
        B_800DC198[3].kx = 0.2f;  B_800DC198[3].kz = 0.2f;
        B_800DC198[4].kx = -0.2f; B_800DC198[4].kz = 0.2f;
        B_800DC198[5].kx = -0.4f; B_800DC198[5].kz = 0.0f;
        B_800DC198[6].kx = 0.0f;  B_800DC198[6].kz = -0.4f;
        for (i = 0; i < 4; i++) {
            Scr1_Drifter *d = &B_800DC2E8[i];
            d->wait = (s16)((s32)guRandom() % 1024 + 1);
            d->life = 0;
            d->side = 1;
            d->pos[0] = 0.0f; d->pos[1] = 0.0f; d->pos[2] = 0.0f;
            d->vel[0] = 0.0f; d->vel[1] = 0.0f; d->vel[2] = 0.0f;
            d->image = 0;
            d->fadeIn = 0.0f;
        }
    }
    B_800DBCC0[0] += 500;
    for (i = 0; i < 7; i++) {
        Scr1_Swing *s = &B_800DC198[i];
        if (s->idle > 0) {
            s->idle = (s16)(s->idle - 1);
            if (s->idle <= 0) {
                s->idle = 0;
                s->busy = (s16)((s32)guRandom() % 256 + 50);
                s->target = (f32)((s32)guRandom() % 200 + 50);
            }
        }
        if (s->busy > 0) {
            s->busy = (s16)(s->busy - 1);
            if (s->busy <= 0) {
                s->busy = 0;
                s->idle = (s16)((s32)guRandom() % 1024 + 1);
                s->target = 0.0f;
            }
        }
        s->speed = (f32)((f64)s->speed * 0.5);
        if (s->value != s->target) {
            f32 push = (f32)((f64)(s->target - s->value) * 0.05);
            if (1.0 < (f64)push) push = 1.0f;
            if ((f64)push < -1.0) push = -1.0f;
            s->speed = push + s->speed;
            if (5.0 < (f64)s->speed) s->speed = 5.0f;
            if ((f64)s->speed < -5.0) s->speed = -5.0f;
            s->value = s->value + s->speed;
            if (s->value < 0.0f) { s->value = 0.0f; s->speed = 0.0f; s->phase = 0; }
            if (0.0f < s->value) s->phase += 1000;
        }
        s->x = s->value * s->kx;
        {
            f32 sn = vsSin(s->phase);
            f64 v = (f64)s->value;
            s->y = (f32)(v + (f64)sn * (v * 0.2));
        }
        s->z = s->value * s->kz;
        s->turn = (s32)((f64)s->value * 100.0);
        s->tilt += (s32)((f64)s->value * 3.0);
    }
    for (i = 0; i < 4; i++) {
        Scr1_Drifter *d = &B_800DC2E8[i];
        if (d->wait > 0) {
            d->wait = (s16)(d->wait - 1);
            if (d->wait <= 0) {
                s32 x, z;
                d->wait = 0;
                d->life = (s16)((s32)guRandom() % 256 + 200);
                d->image = (s16)((s32)guRandom() % 3);
                d->fadeIn = 0.0f;
                d->flags = (u8)((s32)guRandom() % 2);
                if ((guRandom() & 1) == 0) {
                    x = (s32)guRandom() % 1000 - 500;
                    z = (s32)guRandom() % 682;
                    d->side = 1;
                    d->pos[0] = (f32)x;
                    d->pos[2] = (f32)(z + 115);
                    d->pos[1] = -146.0f;
                } else {
                    x = (s32)guRandom() % 1000 - 500;
                    z = (s32)guRandom() % 459;
                    d->side = 0;
                    d->pos[0] = (f32)x;
                    d->pos[2] = (f32)(z - 800);
                    d->pos[1] = 104.0f;
                }
                d->vel[0] = (d->pos[0] < 0.0f) ? 1.0f : -1.0f;
                d->vel[1] = 0.0f;
                d->vel[2] = (f32)((s32)guRandom() % 800 - 400) / 1000.0f;
                func_80087DD0(d->vel);
            }
        }
        if (d->life > 0) {
            d->life = (s16)(d->life - 1);
            if (d->life <= 0) {
                d->life = 0;
                d->wait = (s16)((s32)guRandom() % 1024 + 1);
            }
            if ((f64)d->fadeIn < 1.0) d->fadeIn = (f32)((f64)d->fadeIn + 0.01);
            d->pos[0] = d->pos[0] + d->vel[0];
            d->pos[1] = d->pos[1] + d->vel[1];
            d->pos[2] = d->pos[2] + d->vel[2];
            if (d->life < 100) d->alpha = (f32)((f64)(f32)d->life / 100.0);
            else d->alpha = d->fadeIn;
        }
    }
    if ((s32)guRandom() % 1024 < 10) {
        func_8000D5A0();                               /* lightning, with thunder */
        func_800797EC(B_800F7E6C, 0, 0, 0, (s32)((f64)(B_800DC180 * 32767.0f) * 0.6), 0, 1, 1.0f, 1);
    }
    func_8000D728();
    if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "hont_in1") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DC100 = -75.0f;
        else B_800DC100 = (f32)((f64)-info->ringOutTime * 3.0);
    }
}

/* the working matrix = a copy, moved and turned like a swinging thing, put in 'place' */
static void Scr1_SwingMatrix(void *copy, Scr1_Swing *s, void *place) {
    func_80087E70(B_800DBBF0, copy);
    func_80088350(B_800DBBF0, s->x, s->y, s->z);
    func_800883F0(B_800DBBF0, s->tilt);
    func_800883A0(B_800DBBF0, s->turn);
    func_800882C4(B_800DBBF0, place);
    func_80080170(&gDL, B_800DBBF0);
}

/* the drifting sprites on one side, fading out as both fighters get near them */
static void Scr1_DrawDrifters(Scr1_Info *info, u8 *bank, s32 side) {
    f32 up[3], right[3], v[3];
    s32 i;
    right[0] = info->v4C[0]; right[1] = info->v4C[1]; right[2] = info->v4C[2];
    right[1] = 0.0f;
    func_80087DD0(right);
    up[0] = D_800AB4D0[0]; up[1] = D_800AB4D0[1]; up[2] = D_800AB4D0[2];
    right[0] = (f32)((f64)right[0] * 3.0);
    right[1] = (f32)((f64)right[1] * 3.0);
    right[2] = (f32)((f64)right[2] * 3.0);
    up[0] = (f32)((f64)up[0] * 3.0);
    up[1] = (f32)((f64)up[1] * 3.0);
    up[2] = (f32)((f64)up[2] * 3.0);
    B_800F7788 = 0xFF; B_800F7E70 = 0xFF; B_800F7D94 = 0xFF; B_800F7778 = 0xFF;
    B_800F88BC = 0xFF; B_800F88C8 = 0xFF; B_800F7789 = 0xFF; B_800F7770 = 0xFF;
    B_800F7ECC = 0xFF; B_800F2D1C = 0xFF; B_800F7E78 = 0xFF; B_800F4E34 = 0xFF;
    func_80087E20(B_800DBBF0);
    func_80080170(&gDL, B_800DBBF0);
    for (i = 0; i < 4; i++) {
        Scr1_Drifter *d = &B_800DC2E8[i];
        f32 d1, d2, d3, near, k;
        f64 n;
        u8 a;
        if (d->life <= 0) continue;
        if (side ? d->side == 0 : d->side != 0) continue;
        v[0] = info->pos1[0] - d->pos[0]; v[1] = info->pos1[1] - d->pos[1]; v[2] = info->pos1[2] - d->pos[2];
        d1 = func_80087D80(v);
        v[0] = info->pos2[0] - d->pos[0]; v[1] = info->pos2[1] - d->pos[1]; v[2] = info->pos2[2] - d->pos[2];
        d2 = func_80087D80(v);
        if (info->notStarted != 0) d3 = 99999.0f;
        else {
            /* (the original measures fighter 1 again here) */
            v[0] = info->pos1[0] - d->pos[0]; v[1] = info->pos1[1] - d->pos[1]; v[2] = info->pos1[2] - d->pos[2];
            d3 = func_80087D80(v);
        }
        if (d1 < d2 && d1 < d3) near = d1;
        else if (d2 < d1 && d2 < d3) near = d2;
        else near = d3;
        if (700.0 < (f64)near) near = 700.0f;
        n = (f64)near;
        if (n < 350.0) k = 0.0f;
        else k = (f32)((n - 350.0) / 350.0);
        a = (u8)(u32)(d->alpha * 160.0f * k);
        B_800F8760 = a; B_800F5B78 = a;
        a = (u8)(u32)(d->alpha * 160.0f * k);
        B_800F7E58 = a; B_800F2D1D = a;
        func_80085FE4(&gDL, (CharFrame *)OBJ(bank, 19 + d->image), d->pos[0], d->pos[1], d->pos[2], up, right,
                      d->flags);
    }
}

/* 0x8000F99C: HounganIn_Draw - everything is lit up by the lightning */
void func_8000F99C(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    *gDL++ = 0xF8000000; *gDL++ = 0xC8C8FFFF;          /* fog colour */
    if (pass == 0) {
        func_80082994(OBJ(bank, 1), (f32)((f64)B_800DC194 * 0.5 * 0.4));
        func_800831F4(&gDL, OBJ(bank, 1));
        func_80082994(OBJ(bank, 2), (f32)((f64)B_800DC194 * 0.5 * 0.4));
        func_800831F4(&gDL, OBJ(bank, 2));
        func_80082994(OBJ(bank, 0), (f32)((f64)B_800DC194 * 0.5));
        func_8000D4E0((u32 *)OBJ(bank, 0), info, 0);
        func_80082994(OBJ(bank, 22), (f32)((f64)B_800DC194 * 0.5));
        if (0.0 < (f64)*(f32 *)((u8 *)cam + 0x28)) func_800831F4(&gDL, OBJ(bank, 22));
        func_80082994(OBJ(bank, 10), (f32)((f64)B_800DC194 * 0.5));
        func_800831F4(&gDL, OBJ(bank, 10));
        func_80082994(OBJ(bank, 11), (f32)((f64)B_800DC194 * 0.5));
        if (info->v10 == 0) func_8000D2FC(&gDL, info, OBJ(bank, 11));
        else func_8000D2B0(&gDL, info, OBJ(bank, 11));
        func_80082994(OBJ(bank, 3), (f32)((f64)B_800DC194 * 0.5));
        func_800831F4(&gDL, OBJ(bank, 3));
        func_80082994(OBJ(bank, 5), (f32)((f64)B_800DC194 * 0.5));
        func_800831F4(&gDL, OBJ(bank, 5));
        func_80082994(OBJ(bank, 6), (f32)((f64)B_800DC194 * 0.5));
        func_800831F4(&gDL, OBJ(bank, 6));
        func_80082994(OBJ(bank, 4), (f32)((f64)B_800DC194 * 0.5));
        func_800831F4(&gDL, OBJ(bank, 4));
        Scr1_SwingMatrix(B_800DBD00, &B_800DC198[0], D_800AB76C);
        func_80082994(OBJ(bank, 12), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 12));
        Scr1_SwingMatrix(B_800DBD40, &B_800DC198[1], D_800AB76C);
        func_8000D2B0(&gDL, info, OBJ(bank, 12));
        /* the ring-out door, twice */
        func_80087E70(B_800DBBF0, B_800DBD80);
        func_800883F0(B_800DBBF0, (s32)((f64)(B_800DC100 * 65536.0f) / 360.0));
        func_800882C4(B_800DBBF0, D_800AB7EC);
        func_80080170(&gDL, B_800DBBF0);
        func_80082994(OBJ(bank, 13), (f32)((f64)B_800DC194 * 0.5));
        if (info->v10 == 0) func_8000D2FC(&gDL, info, OBJ(bank, 13));
        else func_8000D2B0(&gDL, info, OBJ(bank, 13));
        func_80087E70(B_800DBBF0, B_800DBDC0);
        func_800883F0(B_800DBBF0, (s32)((f64)(B_800DC100 * 65536.0f) / 360.0));
        func_800882C4(B_800DBBF0, D_800AB7EC);
        func_80080170(&gDL, B_800DBBF0);
        if (info->v10 == 0) func_8000D2FC(&gDL, info, OBJ(bank, 13));
        else func_8000D2B0(&gDL, info, OBJ(bank, 13));
        func_80087E70(B_800DBBF0, B_800DBE00);
        func_800883A0(B_800DBBF0, (s32)((f64)vsSin(B_800DBCC0[0]) * 1500.0));
        func_800882C4(B_800DBBF0, D_800AB86C);
        func_80080170(&gDL, B_800DBBF0);
        func_80082994(OBJ(bank, 14), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 14));
        Scr1_SwingMatrix(B_800DBE40, &B_800DC198[2], D_800AB8AC);
        func_80082994(OBJ(bank, 15), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 15));
        Scr1_DrawDrifters(info, bank, 1);
    } else if (pass == 1) {
        u32 *o, m;
        func_80087E20(B_800DBBF0);
        func_80080170(&gDL, B_800DBBF0);
        func_80082994(OBJ(bank, 7), (f32)((f64)B_800DC194 * 0.5 * 0.4));
        o = (u32 *)OBJ(bank, 7);
        if (info->v10 == 0) { o[1] |= 1; m = 0xC8004030; }
        else { o[1] &= ~1u; m = 0xC8004000; }
        o[2] = m; o[3] = m;
        func_800831F4(&gDL, o);
    } else if (pass == 2) {
        func_8007EEF0(&gDL);
        func_80082994(OBJ(bank, 8), (f32)((f64)B_800DC194 * 0.5));
        func_800831F4(&gDL, OBJ(bank, 8));
        func_80082994(OBJ(bank, 9), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 9));
        func_80082994(OBJ(bank, 23), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 23));
        Scr1_SwingMatrix(B_800DBE80, &B_800DC198[3], D_800AB8EC);
        func_80082994(OBJ(bank, 16), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 16));
        Scr1_SwingMatrix(B_800DBEC0, &B_800DC198[4], D_800AB92C);
        func_80082994(OBJ(bank, 17), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 17));
        Scr1_SwingMatrix(B_800DBF00, &B_800DC198[5], D_800AB96C);
        func_80082994(OBJ(bank, 18), (f32)((f64)B_800DC194 * 0.5));
        func_8000D2B0(&gDL, info, OBJ(bank, 18));
        Scr1_SwingMatrix(B_800DBF40, &B_800DC198[6], D_800AB96C);
        func_8000D2B0(&gDL, info, OBJ(bank, 18));
        Scr1_DrawDrifters(info, bank, 0);
    }
}

/* the arena's fog (the same commands the drawing passes start with) */
extern u8 D_800AB260[];                /* per arena: fog r, g, b, (pad) */
extern s16 D_800AB2C8[];               /* per arena: fog near, far */
static void Scr1_Fog(void) {
    s32 a = B_800F7EC8;
    s32 nearZ, range;
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xF8000000;                                      /* gDPSetFogColor */
    g[1] = ((u32)D_800AB260[a * 4] << 24) | ((u32)D_800AB260[a * 4 + 1] << 16) |
           ((u32)D_800AB260[a * 4 + 2] << 8) | 0xFF;
    gDL = g + 4;
    g[2] = 0xBC000008;                                      /* gSPFogPosition */
    nearZ = D_800AB2C8[a * 2];
    range = D_800AB2C8[a * 2 + 1] - nearZ;
    g[3] = ((u32)(128000 / range) << 16) | ((u32)(((500 - nearZ) << 8) / range) & 0xFFFF);
}

/* ---- arena 5 (Houngan's place, outside): snow, lightning, 17 windows lit on and off ---- */
extern s8 B_800DC3A8[17];              /* window lit */
extern s8 B_800DC3C0[17];              /* window just changed (drawn once more) */
extern s16 B_800DC3D8[17];             /* frames until it changes */
extern s32 B_800DC140[2];              /* [0] which picture of the sign (0..2), [1] door angle */
extern f32 D_800AB9EC[17][3];          /* the windows */
extern f32 D_800AB4DC[3];
extern f32 D_800ABAB8[16], D_800ABAF8[16], D_800ABB38[16];   /* matrices (4 x 4 floats) */
void func_80088300(void *m, f32 x, f32 y, f32 z);   /* scale */
void func_8007FD70(u32 **dl, s32 a);

/* 0x80010FD0: HounganOut_Update */
void func_80010FD0(s32 tick, Scr1_Info *info, u8 *bank) {
    s32 i;
    (void)bank;
    if (tick == 0) {
        for (i = 0; i < 17; i++) {
            B_800DC3A8[i] = 0;
            B_800DC3C0[i] = 0;
            B_800DC3D8[i] = (s16)((s32)guRandom() % 512);
        }
        B_800DC140[0] = 0;
        func_8000D558();
        B_800DC140[1] = 0;
        func_8000D94C();
    }
    if (info->state >= 4) {
        for (i = 0; i < 17; i++) {
            B_800DC3D8[i] = (s16)(B_800DC3D8[i] - 1);
            if (B_800DC3D8[i] > 0) continue;
            B_800DC3A8[i] = (s8)(B_800DC3A8[i] == 0);
            B_800DC3C0[i] = 1;
            if (B_800DC3A8[i] != 0) B_800DC3D8[i] = (s16)((s32)guRandom() % 200);
            else B_800DC3D8[i] = (s16)((s32)guRandom() % 100);
        }
    }
    if ((tick & 3) == 0) {
        if (++B_800DC140[0] >= 3) B_800DC140[0] = 0;
    }
    if ((s32)guRandom() % 1024 < 10) {
        func_8000D5A0();
        func_800797EC(B_800F7E6C, 0, 0, 0, (s32)(B_800DC180 * 32767.0f), 0, 1, 1.0f, 1);
    }
    func_8000D728();
    func_8000DA08();
    if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "hont_out1") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DC140[1] = 0x4B;
        else B_800DC140[1] = (s32)((f64)info->ringOutTime * 3.0);
    }
}

/* the door (object 3) turned by B_800DC140[1] degrees about a hinge at (117, 0, 521) */
static void Scr1_HounganDoor(Scr1_Info *info, u8 *bank, s32 mirror) {
    u32 *o;
    func_80087E20(B_800DBBF0);
    if (!mirror) {
        func_80087EE8(B_800DBBF0, -117.0f, 0.0f, -521.0f);
    } else {
        func_80087EE8(B_800DBBF0, 84.0f, 0.0f, -521.0f);
        func_80088300(B_800DBBF0, -1.0f, 1.0f, 1.0f);
    }
    func_800883F0(B_800DBBF0, (s32)((f64)((f32)B_800DC140[1] * 65536.0f) / 360.0));
    func_80088350(B_800DBBF0, 117.0f, 0.0f, 521.0f);
    func_80080170(&gDL, B_800DBBF0);
    func_80082994(OBJ(bank, 3), (f32)((f64)B_800DC194 * 0.75));
    o = (u32 *)OBJ(bank, 3);
    if (!mirror) { o[1] |= 0x2000; o[1] &= ~0x1000u; }
    else { o[1] |= 0x1000; o[1] &= ~0x2000u; }
    func_8000D2B0(&gDL, info, OBJ(bank, 3));
}

/* 0x800112E8: HounganOut_Draw */
void func_800112E8(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    *gDL++ = 0xF8000000; *gDL++ = 0x5A82C8FF;          /* fog colour */
    if (pass == 1) {
        func_80082994(OBJ(bank, 0), (f32)((f64)B_800DC194 * 0.75 * 0.4));
        func_800831F4(&gDL, OBJ(bank, 0));
    } else if (pass == 2) {
        f32 up[3], right[3];
        s32 i;
        func_80082994(OBJ(bank, 1), (f32)((f64)B_800DC194 * 0.75));
        func_800831F4(&gDL, OBJ(bank, 1));
        func_80082994(OBJ(bank, 2), (f32)((f64)B_800DC194 * 0.75));
        func_8000D2B0(&gDL, info, OBJ(bank, 2));
        func_80087E20(B_800DBBF0);
        func_80080170(&gDL, B_800DBBF0);
        up[0] = (f32)((f64)info->v40[0] * 2.5);
        up[1] = (f32)((f64)info->v40[1] * 2.5);
        up[2] = (f32)((f64)info->v40[2] * 2.5);
        right[0] = (f32)((f64)info->v4C[0] * 2.5);
        right[1] = (f32)((f64)info->v4C[1] * 2.5);
        right[2] = (f32)((f64)info->v4C[2] * 2.5);
        for (i = 0; i < 17; i++) {
            if (B_800DC3C0[i] != 0) {
                func_8000D33C(&gDL, info, OBJ(bank, 6), D_800AB9EC[i][0], D_800AB9EC[i][1], D_800AB9EC[i][2],
                              up, right, 0);
                B_800DC3C0[i] = 0;
            } else if (B_800DC3A8[i] != 0) {
                func_8000D33C(&gDL, info, OBJ(bank, 6), D_800AB9EC[i][0], D_800AB9EC[i][1], D_800AB9EC[i][2],
                              up, right, 0);
            }
        }
        /* the flickering sign, twice: pictures 7 + n and 7 + (n + 1) % 3 */
        func_80087E70(B_800DBBF0, D_800ABAF8);
        func_800882C4(B_800DBBF0, D_800ABAB8);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D3D0(&gDL, info, OBJ(bank, 7 + B_800DC140[0]), 0.0f, 0.0f, 0.0f, D_800AB4D0, D_800AB4DC, 0);
        func_80087E70(B_800DBBF0, D_800ABB38);
        func_800882C4(B_800DBBF0, D_800ABAB8);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D3D0(&gDL, info, OBJ(bank, 7 + (B_800DC140[0] + 1) % 3), 0.0f, 0.0f, 0.0f, D_800AB4D0,
                      D_800AB4DC, 0);
        Scr1_HounganDoor(info, bank, 0);
        Scr1_HounganDoor(info, bank, 1);
    } else if (pass == 3) {
        func_8000DD0C((f32 *)cam);                     /* snow */
    }
}

/* ---- arena 6: a lift (the floor rises and falls once the round is under way) ---- */
/* 0x80011990: Lift_Update - moves 5900 frames up, 5900 down, ... */
void func_80011990(s32 tick, Scr1_Info *info, u8 *bank) {
    s32 n = info->v8;
    f32 h;
    f64 hd;
    (void)tick; (void)bank;
    if (n < 121) h = 0.0f;
    else {
        s32 q;
        n -= 120;
        q = n / 5900;
        h = (f32)(n - q * 5900);
        if (q & 1) h = (f32)(5900.0 - (f64)h);
    }
    hd = (f64)h;
    B_800DC100 = (f32)(hd * 0.05 + -287.0);
    B_800DC104 = (f32)(hd * 0.02);
}

/* 0x80011A60: Lift_Draw */
void func_80011A60(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    f32 lift[16], floor_[16], still[16], top[16];
    (void)cam;
    func_80087E20(lift);
    func_80088350(lift, 0.0f, B_800DC100 + B_800DC104, 0.0f);
    func_80087E20(floor_);
    func_80088350(floor_, 0.0f, B_800DC104, 0.0f);
    func_80087E20(still);
    if (pass == 1) {
        func_80080170(&gDL, lift);
        func_800831F4(&gDL, OBJ(bank, 2));
        func_80080170(&gDL, floor_);
        func_8000D2B0(&gDL, info, OBJ(bank, 3));
        func_80080170(&gDL, lift);
        func_800831F4(&gDL, OBJ(bank, 5));
        func_80080170(&gDL, floor_);
        func_800831F4(&gDL, OBJ(bank, 1));
        func_800831F4(&gDL, OBJ(bank, 7));
        func_8000D2B0(&gDL, info, OBJ(bank, 4));
        func_80087E70(top, floor_);
        func_80088350(top, 0.0f, 10.0f, 0.0f);
        func_80080170(&gDL, top);
        func_800831F4(&gDL, OBJ(bank, 6));
        func_80080170(&gDL, still);
        func_800831F4(&gDL, OBJ(bank, 0));
    } else if (pass == 2) {
        func_80080170(&gDL, still);
        func_8000D2B0(&gDL, info, OBJ(bank, 8));
    }
}

/* the working matrix = a turn by B_800DC100 degrees about the hinge at (x, 0, z) */
static void Scr1_Hinge(f32 x, f32 z, s32 angle, f32 x2, f32 z2) {
    func_80087EE8(B_800DBBF0, x, 0.0f, z);
    func_800883F0(B_800DBBF0, angle);
    func_80088350(B_800DBBF0, x2, 0.0f, z2);
    func_80080170(&gDL, B_800DBBF0);
}
#define SCR1_DEGREES(d) ((s32)((f64)((d) * 65536.0f) / 360.0))

/* ---- arena 7 ---- */
/* 0x80011C80: HungOut_Update */
void func_80011C80(s32 tick, Scr1_Info *info, u8 *bank) {
    (void)bank;
    if (tick == 0) B_800DC100 = 0.0f;
    if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "hung_out") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DC100 = 75.0f;
        else B_800DC100 = (f32)((f64)info->ringOutTime * 3.0);
    }
}

/* 0x80011D2C: HungOut_Draw */
void func_80011D2C(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, OBJ(bank, 0));
        func_800831F4(&gDL, OBJ(bank, 2));
        func_800831F4(&gDL, OBJ(bank, 3));
        func_800831F4(&gDL, OBJ(bank, 1));
    } else if (pass == 2) {
        static const u8 objs[] = { 4, 5, 6, 7, 8, 10, 11, 12, 14, 15, 16, 17, 18 };
        u32 i;
        for (i = 0; i < sizeof(objs); i++) func_8000D2B0(&gDL, info, OBJ(bank, objs[i]));
        func_8007FD70(&gDL, 0);
        Scr1_Fog();
        func_800831F4(&gDL, OBJ(bank, 19));
        func_8000D2B0(&gDL, info, OBJ(bank, 20));
        Scr1_Hinge(-4.0f, -33.0f, SCR1_DEGREES(B_800DC100), 4.0f, 33.0f);
        func_8000D2B0(&gDL, info, OBJ(bank, 21));
        func_80087EE8(B_800DBBF0, -4.0f, 0.0f, -33.0f);
        func_80088350(B_800DBBF0, 4.0f, 0.0f, -635.0f);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 21));
    }
}

/* ---- arena 13 (the operating room) ---- */
/* 0x80012150: Operate_Update */
void func_80012150(s32 tick, Scr1_Info *info, u8 *bank) {
    (void)bank;
    if (tick == 0) B_800DC100 = 0.0f;
    if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "operate") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DC100 = -75.0f;
        else B_800DC100 = (f32)((f64)-info->ringOutTime * 3.0);
    }
}

/* 0x80012200: Operate_Draw */
void func_80012200(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    if (pass == 1) {
        func_800831F4(&gDL, OBJ(bank, 0));
    } else if (pass == 2) {
        func_8000D454((u32 *)OBJ(bank, 1), info, 0);
        func_800831F4(&gDL, OBJ(bank, 2));
        func_800831F4(&gDL, OBJ(bank, 3));
        func_8000D2B0(&gDL, info, OBJ(bank, 4));
        func_8000D2B0(&gDL, info, OBJ(bank, 6));
        func_80087E20(B_800DBBF0);
        Scr1_Hinge(632.0f, 90.0f, SCR1_DEGREES(B_800DC100), -632.0f, -90.0f);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
    }
}

/* ---- arena 2 (outside): two ring-out doors, a swaying tree ---- */
extern f32 D_800ABB78[16], D_800ABBB8[16], D_800ABBF8[16], D_800ABC38[16], D_800ABC78[16], D_800ABCB8[16];   /* matrices (4 x 4 floats) */
extern f32 D_800ABCF8[16], D_800ABD38[16];   /* matrices (4 x 4 floats) */

/* 0x80012374: Outside_Update */
void func_80012374(s32 tick, Scr1_Info *info, u8 *bank) {
    (void)bank;
    if (tick == 0) {
        B_800DBCC0[0] = 0; B_800DBCC0[1] = 0; B_800DBCC0[2] = 0;
        func_8000E128();
    }
    if (info->zone != NULL) {
        if (mpStrEqualNoCase((char *)info->zone + 4, "outside1") && info->ringOut != 0) {
            if (25.0 < (f64)info->ringOutTime) B_800DBCC0[0] = 25000;
            else B_800DBCC0[0] = (s32)((f64)info->ringOutTime * 1000.0);
        }
        if (mpStrEqualNoCase((char *)info->zone + 4, "outside2") && info->ringOut != 0) {
            if (25.0 < (f64)info->ringOutTime) B_800DBCC0[1] = 25000;
            else B_800DBCC0[1] = (s32)((f64)info->ringOutTime * 1000.0);
        }
    }
    B_800DBCC0[2] += 0xB6;
    func_8000E168();
}

/* the working matrix = a sway (shear by the wobble) about y = 20 */
static void Scr1_Sway(f64 k) {
    f32 sk[16];
    f32 w;
    func_80087E20(sk);
    w = B_800F84BC;
    sk[4] = (f32)((f64)(B_800F8968[0] * w) * k);
    sk[6] = (f32)((f64)(B_800F8968[1] * w) * k);
    func_80087E20(B_800DBBF0);
    func_80088184(B_800DBBF0, 0.0f, -20.0f, 0.0f);
    func_80088100(B_800DBBF0, sk);
    func_80088184(B_800DBBF0, 0.0f, 20.0f, 0.0f);
    func_80080170(&gDL, B_800DBBF0);
}

static void Scr1_Identity(void) {
    func_80087E20(B_800DBBF0);
    func_80080170(&gDL, B_800DBBF0);
}

/* 0x800124B0: Outside_Draw */
void func_800124B0(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    if (pass == 1) {
        Scr1_Sway(0.04);
        func_800831F4(&gDL, OBJ(bank, 0));
        Scr1_Sway(0.02);
        func_800831F4(&gDL, OBJ(bank, 1));
        Scr1_Identity();
        func_800831F4(&gDL, OBJ(bank, 2));
    } else if (pass == 2) {
        Scr1_Identity();
        func_800831F4(&gDL, OBJ(bank, 3));
        if (info->zone != NULL) {
            if (mpStrEqualNoCase((char *)info->zone + 4, "outside1")) {
                func_80087E70(B_800DBBF0, D_800ABCB8);
                func_800882C4(B_800DBBF0, D_800ABC78);
                func_80080170(&gDL, B_800DBBF0);
                func_80083A64(&gDL, OBJ(bank, 9), 0.0f, 0.0f, 0.0f, D_800AB4D0, D_800AB4DC, 8);
            }
            if (info->zone != NULL && mpStrEqualNoCase((char *)info->zone + 4, "outside2")) {
                func_80087E70(B_800DBBF0, D_800ABD38);
                func_800882C4(B_800DBBF0, D_800ABCF8);
                func_80080170(&gDL, B_800DBBF0);
                func_80083A64(&gDL, OBJ(bank, 10), 0.0f, 0.0f, 0.0f, D_800AB4D0, D_800AB4DC, 8);
            }
        }
        Scr1_Identity();
        func_8000D2B0(&gDL, info, OBJ(bank, 4));
        Scr1_Sway(0.05);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
        Scr1_Tilt(D_800ABB78, B_800DBCC0[0]);
        func_8000D2B0(&gDL, info, OBJ(bank, 6));
        func_80087E70(B_800DBBF0, D_800ABBF8);
        func_800883F0(B_800DBBF0, B_800DBCC0[1]);
        func_800882C4(B_800DBBF0, D_800ABBB8);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 7));
        func_80087E70(B_800DBBF0, D_800ABC38);
        func_8008861C(B_800DBBF0);
        func_80088440(B_800DBBF0, B_800DBCC0[2]);
        func_800882C4(B_800DBBF0, D_800ABC38);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 8));
    }
}

/* ---- arena 15 (the dungeon): four ring-out trapdoors, flames ---- */
typedef struct Scr1_Flame {            /* 0x1C bytes, 60 of them */
    /* 0x00 */ s16 life;
    /* 0x02 */ s16 life0;
    /* 0x04 */ f32 x, z;
    /* 0x0C */ f32 width;
    /* 0x10 */ f32 height;
    /* 0x14 */ f32 grow;               /* height added a frame */
    /* 0x18 */ f32 spread;             /* width added a frame */
} Scr1_Flame;
extern Scr1_Flame *B_800F15E0;
extern f32 B_800DC108, B_800DC10C;     /* (with B_800DC100/104) the trapdoors' angles */
void func_80087774(f32 *m, s32 angle); /* 2D rotation matrix (3x3) */

/* 0x80012A7C: Dungeon_Update */
void func_80012A7C(s32 tick, Scr1_Info *info, u8 *bank) {
    static const char *const names[4] = { "dungeon1", "dungeon2", "dungeon3", "dungeon4" };
    f32 *doors[4];
    s32 i, k;
    (void)bank;
    doors[0] = &B_800DC100; doors[1] = &B_800DC104; doors[2] = &B_800DC108; doors[3] = &B_800DC10C;
    if (tick == 0) {
        B_800F15E0 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 60 * sizeof(Scr1_Flame), 0);
        for (i = 0; i < 60; i++) B_800F15E0[i].life = 0;
        B_800DC10C = 0.0f; B_800DC108 = 0.0f; B_800DC104 = 0.0f; B_800DC100 = 0.0f;
    }
    if (tick < 0) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800F15E0);
        return;
    }
    for (k = 0; k < 6; k++) {
        f32 m[9], v[2], r;
        Scr1_Flame *f;
        for (i = 0; i < 60 && B_800F15E0[i].life != 0; i++) {}
        if (i >= 60) continue;
        func_80087774(m, ((s32)guRandom() % 16384) << 2);
        v[0] = 0.0f; v[1] = 1.0f;
        r = (f32)((s32)guRandom() % 700);
        f = &B_800F15E0[i];
        {
            s16 life = (s16)((s32)guRandom() % 10 + 5);
            f->life0 = life;
            f->life = life;
        }
        f->x = (f32)((f64)(v[0] * m[0] + v[1] * m[3]) + (f64)m[6]);
        f->z = (f32)((f64)(v[0] * m[1] + v[1] * m[4]) + (f64)m[7]);
        f->x = r * f->x;
        f->z = r * f->z;
        f->width = 0.0f;
        f->height = 0.0f;
        f->grow = (f32)((f64)(f32)((s32)guRandom() % 1024) / 28.0);
        {
            f32 t = (f32)(1.0 - (f64)f->grow / 32.0);
            f->spread = (f32)((f64)(t * t) * 16.0 + 2.0);
        }
    }
    for (i = 0; i < 60; i++) {
        Scr1_Flame *f = &B_800F15E0[i];
        if (f->life <= 0) continue;
        f->life = (s16)(f->life - 1);
        f->width = f->width + f->spread;
        f->height = f->height + f->grow;
    }
    D_800AB4E8 = (f32)((f64)(f32)((s32)guRandom() % 1024) / 1024.0);   /* the firelight flicker */
    if (info->zone == NULL) return;
    for (k = 0; k < 4; k++) {
        if (mpStrEqualNoCase((char *)info->zone + 4, names[k]) && info->ringOut != 0) {
            if (25.0 < (f64)info->ringOutTime) *doors[k] = 4551.0f;
            else *doors[k] = (f32)(s32)((f64)(info->ringOutTime * 65536.0f) / 360.0);
        }
    }
}

/* the trapdoor (object 5) turned by 'angle' and dropped by 'drop' */
static void Scr1_Trapdoor(Scr1_Info *info, u8 *bank, s32 angle, f32 x, f32 drop) {
    func_80087E20(B_800DBBF0);
    func_800883F0(B_800DBBF0, angle);
    func_80088350(B_800DBBF0, x, drop, 0.0f);
    func_80082994(OBJ(bank, 5), D_800AB4E8);
    func_80080170(&gDL, B_800DBBF0);
    func_8000D2B0(&gDL, info, OBJ(bank, 5));
}

/* 0x800130F0: Dungeon_Draw */
void func_800130F0(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    *gDL++ = 0xF8000000; *gDL++ = 0xC80A00FF;          /* fog colour */
    if (pass == 1) {
        u32 *o;
        func_80082994(OBJ(bank, 0), D_800AB4E8);
        func_800831F4(&gDL, OBJ(bank, 0));
        o = (u32 *)OBJ(bank, 1);
        o[1] &= ~0x2000u;
        func_80082994(OBJ(bank, 1), D_800AB4E8);
        func_800831F4(&gDL, OBJ(bank, 1));
        func_800831F4(&gDL, OBJ(bank, 2));
    } else if (pass == 2) {
        f32 right[3], up[3];
        s32 i, k;
        func_80082994(OBJ(bank, 3), D_800AB4E8);
        func_8000D454((u32 *)OBJ(bank, 3), info, 0);
        func_80082994(OBJ(bank, 4), D_800AB4E8);
        func_800831F4(&gDL, OBJ(bank, 4));
        func_80082994(OBJ(bank, 5), D_800AB4E8);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
        right[0] = info->v4C[0]; right[1] = info->v4C[1]; right[2] = info->v4C[2];
        right[1] = 0.0f;
        func_80087DD0(right);
        up[0] = D_800AB4D0[0]; up[1] = D_800AB4D0[1]; up[2] = D_800AB4D0[2];   /* (not used) */
        (void)up;
        *gDL++ = 0xE7000000; *gDL++ = 0;
        func_8007FB08(&gDL, 0x205);
        func_8007FB90(&gDL, 0x0C080233, 0x03020233);
        func_8007FBE8(&gDL, 1);
        *gDL++ = 0xB9000002; *gDL++ = 3;
        /* each flame: a flat diamond on the floor (y = -560), its tip rising */
        for (i = 0; i < 60; i++) {
            Scr1_Flame *f = &B_800F15E0[i];
            Vtx16 *v;
            u32 *g;
            if (f->life <= 0) continue;
            v = gVtx;
            v[0].x = (s16)(s32)f->x; v[0].y = -560; v[0].z = (s16)(s32)f->z;
            v[0].flag = 0; v[0].s = 0; v[0].t = 0;
            v[0].r = 255; v[0].g = 255; v[0].b = 0; v[0].a = 255;
            v[1].x = (s16)(s32)f->x; v[1].y = (s16)(s32)(f->height + -560.0f); v[1].z = (s16)(s32)f->z;
            v[1].flag = 0; v[1].s = 0; v[1].t = 0;
            v[1].r = 100; v[1].g = 0; v[1].b = 0; v[1].a = 100;
            v[2].x = (s16)(s32)(f->x - f->width / 2.0f * right[0]); v[2].y = -560;
            v[2].z = (s16)(s32)(f->z - f->width / 2.0f * right[2]);
            v[2].flag = 0; v[2].s = 0; v[2].t = 0;
            v[2].r = 100; v[2].g = 0; v[2].b = 0; v[2].a = 100;
            v[3].x = (s16)(s32)(f->width / 2.0f * right[0] + f->x); v[3].y = -560;
            v[3].z = (s16)(s32)(f->width / 2.0f * right[2] + f->z);
            v[3].flag = 0; v[3].s = 0; v[3].t = 0;
            v[3].r = 100; v[3].g = 0; v[3].b = 0; v[3].a = 100;
            g = gDL;
            gDL = g + 2;
            g[0] = 0x0400103F;                  /* gSPVertex(4 vertices) */
            g[1] = (u32)gVtx;
            gVtx = gVtx + 4;
            gDL = g + 4;
            g[2] = 0xB1000204; g[3] = 0x206;    /* gSP2Triangles */
        }
        {
            u32 *g = gDL;
            gDL = g + 2;
            g[0] = 0xE7000000; g[1] = 0;
            gDL = g + 4;
            g[2] = 0xB9000002; g[3] = 0;
        }
        for (k = 6; k <= 9; k++) {
            func_80082994(OBJ(bank, k), D_800AB4E8);
            func_8000D2B0(&gDL, info, OBJ(bank, k));
        }
        /* the four trapdoors */
        func_80087EE8(B_800DBBF0, 0.0f, B_800DC100, 0.0f);
        func_80082994(OBJ(bank, 5), D_800AB4E8);
        func_80080170(&gDL, B_800DBBF0);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
        Scr1_Trapdoor(info, bank, -0x4000, 0.0f, B_800DC104);
        Scr1_Trapdoor(info, bank, -0x616C, -30.0f, B_800DC108);
        Scr1_Trapdoor(info, bank, -0x7F49, -30.0f, B_800DC10C);
    }
}

/* ---- arena 17: a turning floor that tips towards the fighters, water flowing round it ---- */
extern f32 B_800DC110, B_800DC114, B_800DC11C;
#define B_800DC118 ((&B_800DC114)[1])   /* (no label of its own: the word after B_800DC114) */
extern f32 B_800F5B8C[2];              /* where the floor is pushed down (x, z), eased */
extern f32 B_800F88CC[2];              /* the axis it tips about */
extern f32 D_800ABDB8[16];             /* the floor's height through a turn (16 steps) */
extern f32 D_800ABDAC;
extern f32 D_800ABD78[16];   /* matrices (4 x 4 floats) */
f32  func_80087630(f32 *v);            /* length of a 2D vector */
void func_80081924(u32 **dl, void *texture, s32 a2);

/* 0x800138A8: Tilt_Update */
void func_800138A8(s32 tick, Scr1_Info *info, u8 *bank) {
    s32 a, m, k;
    f32 frac, w, weight, len, tgt[2];
    f64 lim;
    (void)bank;
    if (tick == 0) {
        B_800DBCC0[0] = 0;
        B_800DC104 = 0.0f; B_800DC108 = 0.0f; B_800DC10C = 0.0f;
        B_800DC110 = 0.0f; B_800DC114 = 0.0f; B_800DC118 = 0.0f;
        B_800F5B8C[0] = 0.0f; B_800F5B8C[1] = 0.0f;
        B_800DC11C = 0.0f;
    }
    /* the floor's height: the table, interpolated */
    B_800DBCC0[0] += 10;
    a = B_800DBCC0[0];
    m = a % 65536;
    k = m / 4096;
    frac = (f32)(m - (k << 12)) / 4096.0f;
    w = (f32)(1.0 - (f64)frac);
    B_800DC100 = (f32)((f64)(w * D_800ABDB8[k]) + (f64)D_800ABDB8[(k + 1) % 16] * (1.0 - (f64)w));
    /* half a push for each fighter on the floor */
    weight = 0.0f;
    if (info->pos1[1] <= 0.0f) weight = 0.5f;
    if (info->pos2[1] <= 0.0f) weight = (f32)((f64)weight + 0.5);
    if (info->pos1[1] <= 0.0f && 0.0f < info->pos2[1]) {
        tgt[0] = info->pos1[0]; tgt[1] = info->pos1[2];
    } else if (info->pos2[1] <= 0.0f && 0.0f < info->pos1[1]) {
        tgt[0] = info->pos2[0]; tgt[1] = info->pos2[2];
    } else if (0.0f < info->pos2[1] && 0.0f < info->pos1[1]) {
        tgt[0] = 0.0f; tgt[1] = 0.0f;
    } else {
        tgt[0] = (f32)((f64)(info->pos1[0] + info->pos2[0]) / 2.0);
        tgt[1] = (f32)((f64)(info->pos1[2] + info->pos2[2]) / 2.0);
    }
    B_800F5B8C[0] = (f32)((f64)B_800F5B8C[0] + (f64)(tgt[0] - B_800F5B8C[0]) * 0.01);
    B_800F5B8C[1] = (f32)((f64)B_800F5B8C[1] + (f64)(tgt[1] - B_800F5B8C[1]) * 0.01);
    len = func_80087630(B_800F5B8C);
    lim = 94.0 / (f64)*(f32 *)(D_800AB49C + 0x34);
    if (lim < (f64)len) len = (f32)lim;
    len = (f32)((f64)len / (94.0 / (f64)*(f32 *)(D_800AB49C + 0x34)));
    /* two springs: how far down the middle goes (C108) and how far it tips (C114) */
    B_800DC104 = (f32)((f64)weight * (1.0 - (f64)len));
    B_800DC110 = weight * len;
    B_800DC10C = (f32)((f64)B_800DC10C + (f64)(B_800DC104 - B_800DC108) * 0.01);
    B_800DC108 = B_800DC108 + B_800DC10C;
    B_800DC10C = (f32)((f64)B_800DC10C * 0.95);
    B_800DC118 = (f32)((f64)B_800DC118 + (f64)(B_800DC110 - B_800DC114) * 0.01);
    B_800DC114 = B_800DC114 + B_800DC118;
    B_800DC118 = (f32)((f64)B_800DC118 * 0.95);
    if (len == 0.0f) {
        B_800F88CC[0] = 1.0f; B_800F88CC[1] = 0.0f;
    } else {
        B_800F88CC[0] = -(B_800F5B8C[1] / len);
        B_800F88CC[1] = B_800F5B8C[0] / len;
    }
    /* the water's texture scroll */
    B_800DC11C = (f32)((f64)B_800DC11C + 0.2);
    if (512.0 < (f64)B_800DC11C) B_800DC11C = (f32)((f64)B_800DC11C - 512.0);
    if ((f64)B_800DC11C < 0.0) B_800DC11C = (f32)((f64)B_800DC11C + 512.0);
}

/* 0x80013DBC: Tilt_Draw */
void func_80013DBC(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    f32 turn[16];
    (void)cam; (void)info;
    func_80087EE8(turn, 0.0f, 0.0f, (f32)(0.0 - (f64)B_800DC100));
    func_800883F0(turn, B_800DBCC0[0]);
    if (pass == 1) {
        f32 tip[16];
        func_80080170(&gDL, turn);
        func_800831F4(&gDL, OBJ(bank, 1));
        func_80087E70(B_800DBBF0, D_800ABD78);
        func_80097740(tip, (f32)((f64)B_800DC114 * -7.0), B_800F88CC[0], 0.0f, B_800F88CC[1]);
        func_80088100(B_800DBBF0, tip);
        func_80088184(B_800DBBF0, 0.0f, (f32)(0.0 - (f64)B_800DC108 * 10.0), 0.0f);
        func_80088134(B_800DBBF0, 0.9f, 1.0f, 1.0f);
        func_80088184(B_800DBBF0, 0.0f, (f32)(0.0 - (f64)D_800ABDAC), 0.0f);
        func_80080170(&gDL, B_800DBBF0);
        func_800831F4(&gDL, OBJ(bank, 0));
    } else if (pass == 2) {
        s32 i;
        func_80080170(&gDL, turn);
        func_800831F4(&gDL, OBJ(bank, 2));
        *gDL++ = 0xE7000000; *gDL++ = 0;
        func_80081924(&gDL, OBJ(bank, 3), 1);
        func_8007FB08(&gDL, 0x205);
        func_8007FB90(&gDL, 0x00404A50, 0x00104A50);
        func_8007FBE8(&gDL, 2);
        /* a ring of 32 quads from radius 500 to 4000 around (500, -1621, 0), scrolling */
        for (i = 0; i < 32;) {
            f32 u, x, z;
            f64 ud;
            s16 s;
            Vtx16 *v;
            u32 *g;
            s32 ang;
            u = (f32)((f64)(f32)i * 20.0 + (f64)B_800DC11C);
            if (512.0 < (f64)u) u = (f32)((f64)u - 512.0);
            if ((f64)u < 0.0) u = (f32)((f64)u + 512.0);
            ang = i << 11;
            v = gVtx;
            x = (f32)((f64)vsSin(ang) * 500.0 + 500.0);
            z = (f32)((f64)vsCos(ang) * 500.0 + 0.0);
            v[0].x = (s16)(s32)x; v[0].y = -1621; v[0].z = (s16)(s32)z; v[0].flag = 0;
            ud = (f64)u;
            s = (s16)(s32)(ud * 32.0);
            v[0].s = s; v[0].t = 0;
            v[0].r = 50; v[0].g = 50; v[0].b = 80; v[0].a = 140;
            x = (f32)((f64)vsSin(ang) * 4000.0 + 500.0);
            z = (f32)((f64)vsCos(ang) * 4000.0 + 0.0);
            v = gVtx;
            v[1].x = (s16)(s32)x; v[1].y = -1621; v[1].z = (s16)(s32)z; v[1].flag = 0;
            v[1].s = s; v[1].t = 0x1900;
            v[1].r = 50; v[1].g = 50; v[1].b = 80; v[1].a = 140;
            i++;
            ang = i << 11;
            x = (f32)((f64)vsSin(ang) * 500.0 + 500.0);
            z = (f32)((f64)vsCos(ang) * 500.0 + 0.0);
            v = gVtx;
            v[2].x = (s16)(s32)x; v[2].y = -1621; v[2].z = (s16)(s32)z; v[2].flag = 0;
            s = (s16)(s32)((ud + 20.0) * 32.0);
            v[2].s = s; v[2].t = 0;
            v[2].r = 50; v[2].g = 50; v[2].b = 80; v[2].a = 140;
            x = (f32)((f64)vsSin(ang) * 4000.0 + 500.0);
            z = (f32)((f64)vsCos(ang) * 4000.0 + 0.0);
            v = gVtx;
            v[3].x = (s16)(s32)x; v[3].y = -1621; v[3].z = (s16)(s32)z; v[3].flag = 0;
            v[3].s = s; v[3].t = 0x1900;
            v[3].r = 50; v[3].g = 50; v[3].b = 80; v[3].a = 140;
            g = gDL;
            gDL = g + 2;
            g[0] = 0x0400103F;                  /* gSPVertex(4 vertices) */
            g[1] = (u32)gVtx;
            gVtx = gVtx + 4;
            gDL = g + 4;
            g[2] = 0xB1000206; g[3] = 0x604;    /* gSP2Triangles */
        }
        *gDL++ = 0xE7000000; *gDL++ = 0;
    }
}

/* ---- arena 20 (the toy room): two belts of toys, a conveyor that stops now and then,
   coloured lights, two ring-out doors ---- */
typedef struct Scr1_Toy {              /* 0x48 bytes */
    /* 0x00 */ s8  kind;               /* object number, <= 0: none */
    /* 0x04 */ f32 m[16];              /* its matrix */
    /* 0x44 */ f32 t;                  /* how far along */
} Scr1_Toy;
typedef struct Scr1_Crate {            /* on the conveyor, 100 bytes */
    /* 0x00 */ s32 kind;               /* object number; 0: nothing; -1: unused slot */
    /* 0x04 */ f32 m[16];
    /* 0x44 */ f32 bounce;             /* how high it bounces */
    /* 0x48 */ f32 pos;                /* 0..1 along the belt */
    /* 0x4C */ f32 bounceNow;          /* ... dying away */
    /* 0x50 */ s32 phase;              /* bounce angle */
    /* 0x54 */ f32 lean;               /* leaning when the belt starts and stops */
    /* 0x58 */ f32 leanSpeed;
    /* 0x5C */ f32 weight;             /* (drawing) how much it pulls the rope down: 1 in the middle */
    /* 0x60 */ f32 sag;                /* (drawing) how far down the rope is here */
} Scr1_Crate;
extern u8  B_800DC400[6], B_800DC3FA[6];   /* lights 1: target and current brightness */
extern s16 B_800DC406[6];                  /* ... frames until a new target */
extern u8  B_800DC418[6], B_800DC412[6];   /* lights 2 */
extern s16 B_800DC41E[6];
extern Scr1_Toy B_800DC430[6], B_800DC5E8[9];
extern s16 B_800DC5E0, B_800DC870;         /* frames until the next toy on each belt */
extern s32 B_800DC874;                     /* crates on the conveyor */
extern Scr1_Crate *B_800DC878;
extern f32 B_800DC87C;                     /* the conveyor's movement since the last crate */
extern f32 B_800DC880, B_800DC884;         /* its speed, last frame's speed */
extern s16 B_800DC888;                     /* frames until it starts again */
extern s32 B_800DC88C;
extern f32 D_800ABEF8[16], D_800ABF38[16], D_800ABF78[16], D_800ABFB8[16], D_800ABFF8[16];   /* matrices (4 x 4 floats) */

/* a random turn (0..359 degrees) */
static s32 Scr1_RandomTurn(void) {
    return (s32)((f64)((f32)((s32)guRandom() % 360) * 65536.0f) / 360.0);
}

/* the lights: each eases 12 a frame towards a target of 0, 127 or 254 */
static void Scr1_Lights(u8 *target, u8 *cur, s16 *timer) {
    s32 i;
    for (i = 0; i < 6; i++) {
        timer[i] = (s16)(timer[i] - 1);
        if (timer[i] <= 0) {
            timer[i] = (s16)((s32)guRandom() % 300);
            target[i] = (u8)(((s32)guRandom() % 3) * 127);
        }
        if ((u32)(target[i] - cur[i] + 11) < 23) cur[i] = target[i];
        else {
            if (cur[i] < target[i]) cur[i] = (u8)(cur[i] + 12);
            if (target[i] < cur[i]) cur[i] = (u8)(cur[i] - 12);
        }
    }
}

/* now and then a new toy on a belt, in the first free slot */
static void Scr1_ToyBelt(Scr1_Toy *toys, s32 n, s16 *timer, f64 speed, f64 end) {
    s32 i;
    *timer = (s16)(*timer - 1);
    if (*timer <= 0) {
        *timer = (s16)((s32)guRandom() % 300 + 120);
        for (i = 0; i < n && toys[i].kind > 0; i++) {}
        if (i < n) {
            Scr1_Toy *t = &toys[i];
            f32 sx, sy, sz;
            s32 k;
            k = (s32)guRandom() % 5;
            switch (k) {
            case 0:
                t->kind = 12;
                func_80087E70(t->m, D_800ABEF8);
                func_80088134(t->m, 0.5f, 0.5f, 0.5f);
                break;
            case 1:
                t->kind = 13;
                func_80087E70(t->m, D_800ABF38);
                break;
            case 2:
                guRandom();                                    /* (thrown away) */
                sx = (f32)((f64)(f32)((s32)guRandom() % 3 + 3) / 10.0);
                t->kind = 15;
                func_80087E70(t->m, D_800ABF78);
                func_80088134(t->m, sx, sx, sx);
                func_80088224(t->m, Scr1_RandomTurn());
                break;
            case 3:
            case 4:
                t->kind = (s8)(16 + (k == 4));
                func_80087E70(t->m, (k == 4) ? D_800ABFF8 : D_800ABFB8);
                sx = (f32)((f64)(f32)((s32)guRandom() % 4 + 6) / 10.0);
                sy = (f32)((f64)(f32)((s32)guRandom() % 7 + 3) / 10.0);
                sz = (f32)((f64)(f32)((s32)guRandom() % 4 + 6) / 10.0);
                func_80088134(t->m, sx, sy, sz);
                func_80088224(t->m, Scr1_RandomTurn());
                break;
            }
            t->t = 0.0f;
        }
    }
    for (i = 0; i < n; i++) {
        Scr1_Toy *t = &toys[i];
        if (t->kind <= 0) continue;
        t->t = (f32)((f64)t->t + speed);
        if (end < (f64)t->t) t->kind = -1;
    }
}

/* a crate's size: x and z 0.6..0.9, y 0.3..0.7 */
static void Scr1_CrateScale(Scr1_Crate *c) {
    f32 sx, sy, sz;
    sx = (f32)((f64)(f32)((s32)guRandom() % 4 + 6) / 10.0);
    sy = (f32)((f64)(f32)((s32)guRandom() % 5 + 3) / 10.0);
    sz = (f32)((f64)(f32)((s32)guRandom() % 4 + 6) / 10.0);
    func_80088134(c->m, sx, sy, sz);
    func_80088224(c->m, Scr1_RandomTurn());
    func_80088224(c->m, Scr1_RandomTurn());
}

/* 0x8001447C: ToyRoom_Update */
void func_8001447C(s32 tick, Scr1_Info *info, u8 *bank) {
    s32 i, n;
    if (tick == 0) {
        u8 *o;
        B_800DC10C = 0.0f; B_800DC108 = 0.0f; B_800DC100 = 0.0f; B_800DC104 = 0.0f;
        for (i = 0; i < 6; i++) {
            u8 r = (u8)((s32)guRandom() % 256);
            B_800DC400[i] = r; B_800DC3FA[i] = r;
            B_800DC406[i] = 0;
        }
        for (i = 0; i < 6; i++) {
            u8 r = (u8)((s32)guRandom() % 256);
            B_800DC418[i] = r; B_800DC412[i] = r;
            B_800DC41E[i] = 0;
        }
        for (i = 0; i < 6; i++) B_800DC430[i].kind = -1;
        B_800DC5E0 = (s16)((s32)guRandom() % 300 + 120);
        for (i = 0; i < 9; i++) B_800DC5E8[i].kind = -1;
        B_800DC870 = (s16)((s32)guRandom() % 300 + 120);
        B_800DC878 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 100 * sizeof(Scr1_Crate), 0);
        for (i = 0; i < 100; i++) {
            B_800DC878[i].kind = -1;
            B_800DC874 = 0;
        }
        B_800DC87C = 0.0f;
        B_800DC880 = 0.0025f;
        B_800DC884 = 0.0025f;
        B_800DC88C = 0;
        B_800DC888 = 0;
        o = OBJ(bank, 18);
        *(s16 *)(o + 0xA) = (s16)(*(s16 *)(o + 6) / 2);
        *(s16 *)(o + 0xC) = 0;
    }
    if (tick < 0) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800DC878);
        return;
    }
    /* two texture scrolls */
    B_800DC100 = (f32)((f64)B_800DC100 + 0.01);
    B_800DC104 = (f32)((f64)B_800DC104 + 0.01);
    if (1.0 < (f64)B_800DC100) {
        do { B_800DC100 = (f32)((f64)B_800DC100 - 1.0); } while (1.0 < (f64)B_800DC100);
    }
    if (1.0 < (f64)B_800DC104) {
        do { B_800DC104 = (f32)((f64)B_800DC104 - 1.0); } while (1.0 < (f64)B_800DC104);
    }
    Scr1_Lights(B_800DC400, B_800DC3FA, B_800DC406);
    Scr1_Lights(B_800DC418, B_800DC412, B_800DC41E);
    Scr1_ToyBelt(B_800DC430, 6, &B_800DC5E0, 1.3, 795.0);
    Scr1_ToyBelt(B_800DC5E8, 9, &B_800DC870, 1.3, 1336.0);
    /* the conveyor (at the start it is filled up at once) */
    if (0.0 < (f64)B_800DC880) {
        do {
            f64 sp = (f64)B_800DC880;
            if (sp < 0.0025) {
                B_800DC880 = (f32)(sp + 0.00015);
                if (0.0025 < (f64)B_800DC880) B_800DC880 = 0.0025f;
            }
            B_800DC87C = B_800DC87C + B_800DC880;
            if (0.15 < (f64)B_800DC87C) {
                B_800DC87C = (f32)((f64)B_800DC87C - 0.15);
                if (B_800DC874 < 100) {
                    Scr1_Crate *c;
                    if (B_800DC874 > 0)
                        mpMemMove(B_800DC878 + 1, B_800DC878, (u32)(B_800DC874 * (s32)sizeof(Scr1_Crate)));
                    B_800DC874++;
                    c = &B_800DC878[0];
                    if (++B_800DC88C <= 0) {
                        c->kind = -1;
                        c->bounce = 0.0f;
                    } else {
                        B_800DC88C = 0;
                        switch ((s32)guRandom() % 7) {
                        case 0:
                            c->kind = 16;
                            func_80087E70(c->m, D_800ABFB8);
                            func_80088184(c->m, 0.0f, -80.0f, 0.0f);
                            Scr1_CrateScale(c);
                            c->bounce = 6.0f;
                            break;
                        case 1:
                            c->kind = 17;
                            func_80087E70(c->m, D_800ABFF8);
                            func_80088184(c->m, 0.0f, -70.0f, 0.0f);
                            Scr1_CrateScale(c);
                            c->bounce = 6.0f;
                            break;
                        case 2: {
                            f32 sc = (f32)((f64)(f32)((s32)guRandom() % 3 + 3) / 10.0);
                            c->kind = 15;
                            func_80087E70(c->m, D_800ABF78);
                            func_80088184(c->m, 0.0f, -150.0f, 0.0f);
                            func_80088134(c->m, sc, sc, sc);
                            func_80088224(c->m, Scr1_RandomTurn());
                            c->bounce = 6.0f;
                            break;
                        }
                        default:
                            c->kind = 0;
                            func_80087E20(c->m);
                            c->bounce = 1.0f;
                            break;
                        }
                    }
                    c = &B_800DC878[0];
                    c->pos = B_800DC87C;
                    c->bounceNow = c->bounce / (f32)((s32)guRandom() % 15 + 10);
                    c->phase = 0;
                    c->lean = 0.0f;
                    c->leanSpeed = 0.0f;
                }
            }
            n = B_800DC874;
            for (i = 0; i < n; i++) B_800DC878[i].pos = B_800DC878[i].pos + B_800DC880;
        } while (tick == 0 && (f64)B_800DC874 < 6.666666666666667);
        /* crates off the end; a toy reaching it stops the belt for 120 frames */
        n = B_800DC874;
        while (n > 0) {
            Scr1_Crate *c = &B_800DC878[n - 1];
            if (!(1.0 < (f64)c->pos)) break;
            if (c->kind > 0 && tick >= 2) {
                B_800DC880 = 0.0f;
                B_800DC888 = 120;
            }
            n = --B_800DC874;
        }
    }
    if (B_800DC888 > 0) {
        B_800DC888 = (s16)(B_800DC888 - 1);
        if (B_800DC888 <= 0) B_800DC880 = 0.00015f;
    }
    n = B_800DC874;
    if (n > 0) {
        f32 sp = B_800DC880, last = B_800DC884;
        for (i = 0; i < n; i++) {
            Scr1_Crate *c = &B_800DC878[i];
            f32 v;
            if (sp != last) c->leanSpeed = (f32)((f64)c->leanSpeed + (f64)(sp - last) / 0.025 * 0.25);
            v = (f32)((f64)c->leanSpeed + (f64)c->lean * -0.005);
            c->leanSpeed = (f32)((f64)v * 0.9935);
            c->lean = c->lean + c->leanSpeed;
            c->phase += 600;
            c->bounceNow = (f32)((f64)c->bounceNow * 0.999);
        }
    }
    B_800DC884 = B_800DC880;
    if (info->zone == NULL) return;
    if (mpStrEqualNoCase((char *)info->zone + 4, "toyroom1") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DC108 = 100.0f;
        else B_800DC108 = (f32)((f64)info->ringOutTime * 4.0);
    }
    if (mpStrEqualNoCase((char *)info->zone + 4, "toyboy") && info->ringOut != 0) {
        if (25.0 < (f64)info->ringOutTime) B_800DC10C = 100.0f;
        else B_800DC10C = (f32)((f64)info->ringOutTime * 4.0);
    }
}

extern f32 D_800ABE38[16], D_800ABE78[16], D_800ABEB8[16];   /* matrices (4 x 4 floats) */
extern f32 D_800AC038[9], D_800AC05C[6];       /* the belts' rollers (x) */
extern f32 D_800AC074[8][3], D_800AC0D4[8][3]; /* the belts (8 corners each) */
extern f32 D_800AC134[6][3], D_800AC17C[6][3]; /* the lights */
extern f32 B_800DBC30[16];                     /* a toy's matrix */
extern f32 B_800DBC70[3];                      /* where a crate hangs from the rope */
extern s32 B_800DBC7C, B_800DBC80, B_800DBC84;
void func_800881D4(void *m, s32 angle);

/* a belt: 8 corners from 'corners', scrolled by 'scroll', bobbing by 'scroll' too;
   'dir' = which half scrolls forwards */
static void Scr1_Belt(u8 *bank, f32 (*corners)[3], f32 *scroll, s32 forwardsLow) {
    f32 bob;
    s32 i;
    bob = vsSin((s32)(((f64)*scroll * 6.0 + 7.5) * 65536.0));
    func_80087EE8(B_800DBBF0, 0.0f, bob, 0.0f);
    func_80080170(&gDL, B_800DBBF0);
    *gDL++ = 0xE7000000; *gDL++ = 0;
    func_8007FB08(&gDL, 0x205);
    func_8007FB90(&gDL, 0x00442230, 0x00112230);
    func_8007FBE8(&gDL, 2);
    func_80081924(&gDL, OBJ(bank, 14), 1);
    for (i = 0; i < 8; i++) {
        Vtx16 *v;
        f32 sc, s;
        if (i == 4) {
            func_80087EE8(B_800DBBF0, 0.0f,
                          (f32)((f64)vsSin((s32)(((f64)*scroll * 6.0 + 7.5) * 65536.0)) * 2.0), 0.0f);
            func_80080170(&gDL, B_800DBBF0);
        }
        v = &gVtx[i];
        v->x = (s16)(s32)corners[i][0];
        v->y = (s16)(s32)corners[i][1];
        v->z = (s16)(s32)corners[i][2];
        v->flag = 0;
        sc = *scroll * 64.0f * 32.0f;
        s = (f32)(s32)((f64)corners[i][0] * 0.5 * 32.0);
        if ((i < 4) == forwardsLow) s = s + sc;
        else s = s + -sc;
        v->s = (s16)(s32)s;
        v = &gVtx[i];
        v->t = (s16)(s32)((f64)corners[i][2] * 0.5 * 32.0);
        v->r = 100; v->g = 100; v->b = 128; v->a = 255;
    }
    {
        u32 *g = gDL;
        gDL = g + 2;
        g[0] = 0x0400207F;                  /* gSPVertex(8 vertices) */
        g[1] = (u32)gVtx;
        gVtx = gVtx + 8;
        gDL = g + 4;
        g[2] = 0xB1000204; g[3] = 0x406;
        gDL = g + 6;
        g[4] = 0xB10C080A; g[5] = 0x000E080C;
        gDL = g + 8;
        g[6] = 0xE7000000; g[7] = 0;
    }
    (void)bob;
}

/* the toys riding a belt: they rise on at the start and drop off the end */
static void Scr1_BeltToys(u8 *bank, Scr1_Toy *toys, s32 n, f32 bob, s32 far_) {
    s32 i;
    for (i = 0; i < n; i++) {
        Scr1_Toy *t = &toys[i];
        f32 in, out, x, y;
        f64 ind, notIn;
        if (t->kind <= 0) continue;
        in = 1.0f;
        if (t->t < 50.0f) in = t->t / 50.0f;
        if (!far_) {
            if (761.0 < (f64)t->t) out = (f32)(((f64)t->t - 761.0) / 34.0);
            else out = 0.0f;
            ind = (f64)in;
            notIn = 1.0 - ind;
            x = t->t + -685.0f;
            y = (f32)((f64)(bob * in) * (1.0 - (f64)out) + 26.0 + notIn * -40.0 +
                      (f64)(out * out * out) * -40.0);
            func_80087EE8(B_800DBC30, x, y, 112.0f);
            if (ind < 1.0) func_80088440(B_800DBC30, (s32)(notIn * 16000.0));
            if (0.0 < (f64)out) func_80088440(B_800DBC30, (s32)((f64)out * -16000.0));
        } else {
            if (1324.0 < (f64)t->t) out = (f32)(((f64)t->t - 1324.0) / 12.0);
            else out = 0.0f;
            ind = (f64)in;
            notIn = 1.0 - ind;
            x = 665.0f - t->t;
            y = (f32)((f64)(bob * in) * (1.0 - (f64)out) + 150.0 + notIn * -40.0 + (f64)out * -40.0);
            func_80087EE8(B_800DBC30, x, y, -881.0f);
            if (ind < 1.0) func_80088440(B_800DBC30, (s32)(notIn * -16000.0));
            if (0.0 < (f64)out) func_80088440(B_800DBC30, (s32)((f64)out * 16000.0));
        }
        func_800883F0(B_800DBC30, 0x4000);
        func_800882C4(B_800DBC30, t->m);
        func_80080170(&gDL, B_800DBC30);
        func_800831F4(&gDL, OBJ(bank, t->kind));
    }
}

/* a piece of the rope the crates hang from, from 'a' to B_800DBC70 */
static void Scr1_Rope(Scr1_Info *info, f32 *a) {
    Vtx16 *v = gVtx;
    s32 k;
    for (k = 0; k < 4; k++) {
        f32 *p = (k < 2) ? a : B_800DBC70;
        s32 j;
        v = gVtx;
        for (j = 0; j < 3; j++) {
            f64 d = (f64)info->v40[j];
            (&v[k].x)[j] = (s16)(s32)((f64)p[j] + ((k & 1) ? d + d : d * -2.0));
        }
        v[k].flag = 0; v[k].s = 0; v[k].t = 0;
        v[k].r = 0; v[k].g = 0; v[k].b = 0; v[k].a = 255;
    }
    {
        u32 *g = gDL;
        gDL = g + 2;
        g[0] = 0x0400103F;                  /* gSPVertex(4 vertices) */
        g[1] = (u32)gVtx;
        gVtx = gVtx + 4;
        gDL = g + 4;
        g[2] = 0xE7000000; g[3] = 0;
    }
    func_8007FB08(&gDL, 5);
    func_8007FB90(&gDL, 0x00442078, 0x00112078);
    func_8007FBE8(&gDL, 1);
    {
        u32 *g = gDL;
        gDL = g + 2;
        g[0] = 0xB1000204; g[1] = 0x00040602;
        gDL = g + 4;
        g[2] = 0xE7000000; g[3] = 0;
    }
}

/* the door (object 8) at a place, mirrored or not */
static void Scr1_ToyDoor(Scr1_Info *info, u8 *bank, s32 mirrored) {
    u32 *o = (u32 *)OBJ(bank, 8);
    if (!mirrored) { o[1] |= 0x2000; o[1] &= ~0x1000u; }
    else { o[1] |= 0x1000; o[1] &= ~0x2000u; }
    func_80080170(&gDL, B_800DBBF0);
    func_8000D2B0(&gDL, info, OBJ(bank, 8));
}

/* 0x80016014: ToyRoom_Draw */
void func_80016014(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    f32 scrollA[16], scrollB[16], right[3];
    f64 c;
    (void)cam;
    func_80088068(scrollA, (s32)(B_800DC100 * 65536.0f));
    func_800882C4(scrollA, D_800ABEB8);
    func_80088068(scrollB, (s32)((1.0 - (f64)B_800DC104) * 65536.0));
    func_800882C4(scrollB, D_800ABEB8);
    right[0] = info->v4C[0]; right[1] = info->v4C[1]; right[2] = info->v4C[2];
    right[1] = 0.0f;
    func_80087DD0(right);
    if (pass == 0) {
        s32 i;
        func_800831F4(&gDL, OBJ(bank, 0));
        /* the two ring-out doors (two halves each) */
        func_80087E20(B_800DBBF0);
        func_80087EE8(B_800DBBF0, 0.0f, 0.0f, B_800DC108);
        Scr1_ToyDoor(info, bank, 0);
        func_80087E20(B_800DBBF0);
        func_80087EE8(B_800DBBF0, 0.0f, 0.0f, 739.0f);
        func_80088300(B_800DBBF0, 1.0f, 1.0f, -1.0f);
        func_80088350(B_800DBBF0, 0.0f, 0.0f, B_800DC108);
        Scr1_ToyDoor(info, bank, 1);
        func_80087E20(B_800DBBF0);
        func_80087EE8(B_800DBBF0, B_800DC10C, 0.0f, 257.0f);
        func_800883F0(B_800DBBF0, 0x4000);
        Scr1_ToyDoor(info, bank, 0);
        func_80087E20(B_800DBBF0);
        func_80087EE8(B_800DBBF0, 745.0f - B_800DC10C, 0.0f, 257.0f);
        func_80088300(B_800DBBF0, -1.0f, 1.0f, 1.0f);
        func_800883F0(B_800DBBF0, 0x4000);
        Scr1_ToyDoor(info, bank, 1);
        Scr1_Identity();
        func_8000D454((u32 *)OBJ(bank, 1), info, 0);
        func_800831F4(&gDL, OBJ(bank, 2));
        if ((f64)info->camPt[2] >= -243.0) func_800831F4(&gDL, OBJ(bank, 5));
        func_800831F4(&gDL, OBJ(bank, 3));
        for (i = 0; i < 6; i++) {
            func_80087EE8(B_800DBBF0, D_800AC17C[i][0], D_800AC17C[i][1], D_800AC17C[i][2]);
            func_80088440(B_800DBBF0, (B_800DC412[i] - 110) * -55);
            func_800882C4(B_800DBBF0, D_800ABE78);
            func_80080170(&gDL, B_800DBBF0);
            func_8000D2B0(&gDL, info, OBJ(bank, 10));
        }
        for (i = 0; i < 6; i++) {
            func_80087EE8(B_800DBBF0, D_800AC05C[i], 4.0f, 109.0f);
            func_800882C4(B_800DBBF0, scrollB);
            func_80080170(&gDL, B_800DBBF0);
            func_800831F4(&gDL, OBJ(bank, 11));
        }
        Scr1_Belt(bank, D_800AC0D4, &B_800DC104, 0);
        Scr1_BeltToys(bank, B_800DC430, 6, vsSin((s32)(((f64)B_800DC104 * 6.0 + 7.5) * 65536.0)), 0);
        Scr1_Identity();
    } else if (pass == 1) {
        c = (f64)info->camPt[2];
        if (c < -243.0 && -350.0 <= c) func_800831F4(&gDL, OBJ(bank, 5));
        func_8000D2B0(&gDL, info, OBJ(bank, 4));
        c = (f64)info->camPt[2];
        if (c < -940.0 ||
            (c < -350.0 && -940.0 <= c &&
             (500.0f <= info->camPt[1] || info->v4C[2] <= 0.0f || 0.0f <= info->v4C[0])))
            func_800831F4(&gDL, OBJ(bank, 5));
        func_800831F4(&gDL, OBJ(bank, 12));
    } else if (pass == 2 || pass == 3) {
        if (pass == 2) {
            f32 from[3], hang[16];
            s32 i, j;
            func_800831F4(&gDL, OBJ(bank, 6));
            func_8000D2B0(&gDL, info, OBJ(bank, 7));
            for (i = 0; i < 6; i++) {
                func_80087EE8(B_800DBBF0, D_800AC134[i][0], D_800AC134[i][1], D_800AC134[i][2]);
                func_80088440(B_800DBBF0, (B_800DC3FA[i] - 150) * 60);
                func_800882C4(B_800DBBF0, D_800ABE38);
                func_80080170(&gDL, B_800DBBF0);
                func_8000D2B0(&gDL, info, OBJ(bank, 9));
            }
            for (i = 0; i < 9; i++) {
                func_80087EE8(B_800DBBF0, D_800AC038[i], 128.0f, -884.0f);
                func_800882C4(B_800DBBF0, scrollA);
                func_80080170(&gDL, B_800DBBF0);
                func_800831F4(&gDL, OBJ(bank, 11));
            }
            Scr1_Belt(bank, D_800AC074, &B_800DC100, 1);
            Scr1_BeltToys(bank, B_800DC5E8, 9, vsSin((s32)(((f64)B_800DC100 * 6.0 + 7.5) * 65536.0)), 1);
            /* the conveyor's rope sags under the crates: each one pulls it down near itself */
            for (i = 0; i < B_800DC874; i++) {
                Scr1_Crate *e = &B_800DC878[i];
                f64 w = (f64)e->pos;
                if (w <= 0.5) w = w / 0.5;
                else w = (1.0 - (f64)B_800DC878[i].pos) / 0.5;
                e->weight = __builtin_sqrtf((f32)w);
                e->sag = 0.0f;
            }
            for (i = 0; i < B_800DC874; i++) {
                for (j = 0; j < B_800DC874; j++) {
                    Scr1_Crate *e = &B_800DC878[i];
                    f32 k, s;
                    f64 kd, a;
                    k = (f32)((f64)e->bounce * 9.0 * (f64)e->weight);
                    kd = (f64)k;
                    s = vsSin(e->phase) * 16384.0f;
                    if (s < 0.0f) s = 0.0f - vsSin(e->phase) * 16384.0f;
                    else s = vsSin(e->phase) * 16384.0f;
                    k = (f32)(kd + (1.0 - (f64)s * 0.003) * (f64)e->bounceNow * (f64)e->weight);
                    kd = (f64)k;
                    s = vsSin((s32)(e->lean * 16384.0f));
                    if (s < 0.0f) s = 0.0f - vsSin((s32)(e->lean * 16384.0f));
                    else s = vsSin((s32)(e->lean * 16384.0f));
                    a = 1.0 - (f64)s * 50.0;
                    s = e->leanSpeed;
                    if (s < 0.0f) s = 0.0f - s;
                    k = (f32)(kd + a * (f64)s);
                    B_800DC878[j].sag = k * e->weight * B_800DC878[j].weight + B_800DC878[j].sag;
                }
            }
            from[0] = 753.0f; from[1] = 621.0f; from[2] = -382.0f;
            for (i = 0; i < B_800DC874; i++) {
                Scr1_Crate *e = &B_800DC878[i];
                f32 x = 753.0f - e->pos * 1506.0f;
                f32 z0 = 0.0f;
                func_80087E20(hang);
                func_80088184(hang, x, 621.0f - B_800DC878[i].sag, -382.0f);
                Scr1_Identity();
                B_800DBC7C = 0; B_800DBC80 = 0; B_800DBC84 = 0;
                B_800DBC70[0] = (f32)((f64)(hang[0] * z0 + hang[4] * z0 + hang[8] * z0) + (f64)hang[12]);
                B_800DBC70[1] = (f32)((f64)(hang[1] * z0 + hang[5] * z0 + hang[9] * z0) + (f64)hang[13]);
                B_800DBC70[2] = (f32)((f64)(hang[2] * z0 + hang[6] * z0 + hang[10] * z0) + (f64)hang[14]);
                Scr1_Rope(info, from);
                from[0] = B_800DBC70[0]; from[1] = B_800DBC70[1]; from[2] = B_800DBC70[2];
                if (B_800DC878[i].kind < 0) continue;
                if (B_800DC878[i].kind > 0) {
                    func_80087E20(B_800DBBF0);
                    func_80088184(B_800DBBF0, 0.0f, -38.0f, 0.0f);
                    func_800881D4(B_800DBBF0,
                                  (s32)(vsSin(B_800DC878[i].phase) * 16384.0f * B_800DC878[i].bounceNow));
                    func_80088274(B_800DBBF0, (s32)(B_800DC878[i].lean * 16384.0f));
                    func_80088100(B_800DBBF0, hang);
                    func_800882C4(B_800DBBF0, B_800DC878[i].m);
                    func_80080170(&gDL, B_800DBBF0);
                    func_800831F4(&gDL, OBJ(bank, B_800DC878[i].kind));
                }
                /* the hook */
                func_80087E20(B_800DBBF0);
                func_80088184(B_800DBBF0, 0.0f, 7.0f, 0.0f);
                func_800881D4(B_800DBBF0, (s32)(vsSin(B_800DC878[i].phase) * 16384.0f * B_800DC878[i].bounceNow));
                func_80088274(B_800DBBF0, (s32)(B_800DC878[i].lean * 16384.0f));
                func_80088100(B_800DBBF0, hang);
                func_80080170(&gDL, B_800DBBF0);
                func_80083A64(&gDL, OBJ(bank, 18), 0.0f, 0.0f, 0.0f, D_800AB4D0, right, 0);
            }
            Scr1_Identity();
            B_800DBC70[0] = -753.0f; B_800DBC70[1] = 621.0f; B_800DBC70[2] = -382.0f;
            Scr1_Rope(info, from);
        }
        /* (pass 2 carries on into pass 3's part) */
        c = (f64)info->camPt[2];
        if (c < -350.0 && -940.0 <= c && info->camPt[1] < 500.0f && 0.0f < info->v4C[2] && info->v4C[0] < 0.0f)
            func_800831F4(&gDL, OBJ(bank, 5));
    }
}

/* ---- arena 21: nothing moves ---- */
/* 0x80018400: Static_Update */
void func_80018400(s32 tick, Scr1_Info *info, u8 *bank) {
    (void)tick; (void)info; (void)bank;
}

/* 0x80018408: Static_Draw */
void func_80018408(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    (void)cam;
    if (pass == 1) {
        func_8000D2B0(&gDL, info, OBJ(bank, 0));
        func_800831F4(&gDL, OBJ(bank, 1));
        func_800831F4(&gDL, OBJ(bank, 2));
        func_800831F4(&gDL, OBJ(bank, 5));
        func_800831F4(&gDL, OBJ(bank, 6));
    } else if (pass == 2) {
        func_8000D2B0(&gDL, info, OBJ(bank, 3));
        func_8000D2B0(&gDL, info, OBJ(bank, 4));
        func_800831F4(&gDL, OBJ(bank, 7));
    }
}

/* ---- arena 22: a rope bridge of 15 planks over a river, snow ---- */
typedef struct Scr1_Plank {            /* 0x54 bytes, 15 of them */
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 drop;               /* how far it is pushed down */
    /* 0x08 */ f32 sway;               /* how far it swings sideways (z) */
    /* 0x0C */ s32 tilt;               /* its tilt (angle) */
    /* 0x10 */ f32 a[3];               /* the plank's two ends ... */
    /* 0x1C */ f32 b[3];
    /* 0x28 */ f32 mid[3];             /* ... and its middle */
    /* 0x34 */ f32 u;                  /* 0..1 across the bridge */
    /* 0x38 */ f32 give;               /* how much it can move: 1 in the middle */
    /* 0x3C */ f32 dropTarget, dropNow, dropSpeed;
    /* 0x48 */ f32 tiltTarget, tiltNow, tiltSpeed;
} Scr1_Plank;
extern Scr1_Plank *B_800F5B64;
extern f32 B_800F5D04, B_800F4E48;     /* the fighters' heights last frame */
extern f32 B_800F5FC0[16], B_800F2DD8[16];   /* the river's ripples and their speeds */
#define B_800F5FC4 (&B_800F5FC0[1])
extern f32 B_800DBC88[2], B_800DBC90[2], B_800DBC98[9];
extern f32 D_800AC3C4;                 /* the river's texture scroll */
extern f32 D_800AC304[16][3];          /* the river's corners */
extern f32 D_800AC1C4[16], D_800AC204[16], D_800AC244[16], D_800AC284[16], D_800AC2C4[16];   /* matrices (4 x 4 floats) */

/* 0x800184F0: Bridge_Step - a fighter landing on (or standing on) the bridge at (x, z)
   pushes the planks near him down by 'weight'; 'lastY' > 0: he has just landed */
void func_800184F0(f32 x, f32 z, f32 weight, f32 lastY) {
    f32 u, v, k;
    f64 ud, w;
    s32 i;
    u = (x - -1004.0f) / 1363.0f;
    v = (z - -248.0f) / 370.0f;
    ud = (f64)u;
    if (ud < 0.0 || 1.0 < ud) return;
    if (ud <= 0.5) w = ud / 0.5;
    else w = (1.0 - (f64)u) / 0.5;
    k = __builtin_sqrtf((f32)w);
    for (i = 0; i < 15; i++) {
        Scr1_Plank *p = &B_800F5B64[i];
        f32 d = p->u - u, near_;
        if (d < 0.0f) d = 0.0f - d;
        else d = p->u - u;
        near_ = (f32)(1.0 - (f64)d);
        near_ = near_ * near_;
        p->dropTarget = (f32)((f64)p->dropTarget + (1.0 - (f64)(p->dropTarget / 17.0f)) *
                              (f64)(weight * k * near_));
        p->tiltTarget = (f32)((f64)p->tiltTarget + (1.0 - (f64)(p->tiltTarget / 8192.0f)) *
                              ((f64)weight * ((f64)v - 0.5) * (f64)near_) * 100.0);
    }
    if (0.0f < lastY) B_800DC10C = (f32)((f64)B_800DC10C + -((f64)v - 0.5));
}

/* 0x800187A4: Bridge_Update */
void func_800187A4(s32 tick, Scr1_Info *info, u8 *bank) {
    s32 i;
    (void)bank;
    if (tick == 0) {
        func_8000E128();
        B_800DBCC0[0] = 0;
        B_800F5B64 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 15 * sizeof(Scr1_Plank), 0);
        for (i = 0; i < 15; i++) {
            Scr1_Plank *p = &B_800F5B64[i];
            f64 w;
            p->x = (f32)(i * 97 - 1004);
            p->drop = 0.0f;
            p->tilt = 0;
            p->u = (f32)i / 14.0f;
            w = (f64)p->u;
            if (w <= 0.5) w = w / 0.5;
            else w = (1.0 - (f64)B_800F5B64[i].u) / 0.5;
            p->give = (f32)w;
            p->dropNow = B_800F5B64[i].give;
            p->dropSpeed = 0.0f;
            p->tiltTarget = 0.0f;
            p->tiltNow = 0.0f;
            p->tiltSpeed = 0.0f;
        }
        B_800DC10C = 0.0f; B_800DC110 = 0.0f;
        B_800F5D04 = 0.0f;
        B_800F4E48 = 0.0f;
        func_8000D94C();
        B_800F7D8C = -500.0f;
        for (i = 0; i < 16; i++) { B_800F5FC0[i] = 0.0f; B_800F2DD8[i] = 0.0f; }
    }
    if (tick < 0) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800F5B64);
        return;
    }
    B_800DBCC0[0] += 1000;
    func_8000E128();                   /* (the wobble is reset every frame: it never moves) */
    for (i = 0; i < 15; i++) {
        B_800F5B64[i].dropTarget = 1.0f;
        B_800F5B64[i].tiltTarget = 0.0f;
    }
    if (info->notStarted == 0) {
        if (info->pos1[1] <= 0.0f) func_800184F0(info->pos1[0], info->pos1[2], 17.0f, B_800F5D04);
        if (info->pos2[1] <= 0.0f) func_800184F0(info->pos2[0], info->pos2[2], 17.0f, B_800F4E48);
    }
    B_800F5D04 = info->pos1[1];
    B_800F4E48 = info->pos2[1];
    for (i = 0; i < 15; i++) {
        Scr1_Plank *p = &B_800F5B64[i];
        f32 *m = B_800DBC98;
        p->dropSpeed = (f32)((f64)p->dropSpeed + (f64)(p->dropTarget - p->dropNow) * 0.05);
        p->dropNow = p->dropNow + p->dropSpeed;
        p->dropSpeed = (f32)((f64)p->dropSpeed * 0.9);
        p->tiltSpeed = (f32)((f64)p->tiltSpeed + (f64)(p->tiltTarget - p->tiltNow) * 0.05);
        p->tiltNow = p->tiltNow + p->tiltSpeed;
        p->tiltSpeed = (f32)((f64)p->tiltSpeed * 0.87);
        p->drop = p->dropNow * p->give;
        p->tilt = (s32)(p->tiltNow * p->give);
        p->sway = vsSin((s32)((f64)B_800DC110 * 1000.0)) * 50.0f * B_800F5B64[i].give;
        /* the ends: (+-185, 0) turned by the tilt */
        func_80087774(B_800DBC98, B_800F5B64[i].tilt);
        B_800DBC88[0] = 185.0f; B_800DBC88[1] = 0.0f;
        B_800DBC90[0] = (f32)((f64)(m[0] * 185.0f + m[3] * 0.0f) + (f64)m[6]);
        B_800DBC90[1] = (f32)((f64)(m[1] * 185.0f + m[4] * 0.0f) + (f64)m[7]);
        p->a[0] = p->x;
        p->a[1] = (-14.0f - p->drop) + B_800DBC90[1];
        p->a[2] = (B_800DBC90[0] + -63.0f) + p->sway;
        func_80087774(B_800DBC98, p->tilt);
        B_800DBC88[0] = -185.0f; B_800DBC88[1] = 0.0f;
        B_800DBC90[0] = (f32)((f64)(m[0] * -185.0f + m[3] * 0.0f) + (f64)m[6]);
        B_800DBC90[1] = (f32)((f64)(m[1] * -185.0f + m[4] * 0.0f) + (f64)m[7]);
        p->b[0] = p->x;
        p->b[1] = (-14.0f - p->drop) + B_800DBC90[1];
        p->b[2] = (B_800DBC90[0] + -63.0f) + p->sway;
        p->mid[0] = p->x;
        p->mid[1] = -14.0f - p->drop;
        p->mid[2] = p->sway + -63.0f;
    }
    /* the whole bridge swinging (C110) and its speed (C10C) */
    if (0.0f < B_800DC110) B_800DC10C = (f32)((f64)B_800DC10C - 0.03);
    if (B_800DC110 < 0.0f) B_800DC10C = (f32)((f64)B_800DC10C + 0.03);
    B_800DC10C = (f32)((f64)B_800DC10C * 0.9);
    {
        f32 a = B_800DC110;
        if (a < 0.0f) a = 0.0f - a;
        if ((f64)a < 0.07) {
            f32 b = B_800DC10C;
            if (b < 0.0f) b = 0.0f - b;
            if ((f64)b < 0.07) {
                B_800DC10C = (f32)((f64)B_800DC10C * 0.8);
                B_800DC110 = (f32)((f64)B_800DC110 * 0.8);
            }
        }
        a = B_800DC110;
        if (a < 0.0f) a = 0.0f - a;
        if ((f64)a < 0.01) {
            f32 b = B_800DC10C;
            if (b < 0.0f) b = 0.0f - b;
            if ((f64)b < 0.01) { B_800DC10C = 0.0f; B_800DC110 = 0.0f; }
        }
    }
    B_800DC110 = B_800DC110 + B_800DC10C;
    func_8000DA08();
    D_800AC3C4 = (f32)((f64)D_800AC3C4 - 0.25);
    if ((f64)D_800AC3C4 < 256.0) D_800AC3C4 = (f32)((f64)D_800AC3C4 + 512.0);
    for (i = 0; i < 16; i++) {
        B_800F5FC0[i] = B_800F5FC0[i] + B_800F2DD8[i];
        B_800F2DD8[i] = (f32)((f64)B_800F2DD8[i] * 0.98);
        B_800F5FC0[i] = (f32)((f64)B_800F5FC0[i] * 0.99);
        if ((s32)guRandom() % 512 < 10) {
            s32 r = (s32)guRandom() % 1024;
            B_800F2DD8[i] = (f32)((f64)B_800F2DD8[i] + ((f64)(f32)r / 1024.0 - 0.5));
        }
    }
}

/* one triangle's word for gSP2Triangles, the way the F3DEX macro builds it: 'flag' says
   which vertex comes first (the original passes the loop counter as the flag) */
static u32 Scr1_Tri(s32 v0, s32 v1, s32 v2, s32 flag) {
    s32 a, b, c;
    if (flag == 0) { a = v0; b = v1; c = v2; }
    else if (flag == 1) { a = v1; b = v2; c = v0; }
    else { a = v2; b = v0; c = v1; }
    return ((u32)((a * 2) & 0xFF) << 16) | ((u32)((b * 2) & 0xFF) << 8) | (u32)((c * 2) & 0xFF);
}

/* a white textured quad (a plank's side) from four points, texture s/t per corner */
static void Scr1_PlankQuad(u8 *bank, f32 *p0, f32 dx0, s32 s0, s32 t0, f32 *p1, f32 dx1, s32 s1, s32 t1,
                           f32 *p2, f32 dx2, s32 s2, s32 t2, f32 *p3, f32 dx3, s32 s3, s32 t3, f32 dy) {
    f32 *p[4];
    f32 dx[4];
    s32 s[4], t[4], k;
    u32 *g;
    p[0] = p0; p[1] = p1; p[2] = p2; p[3] = p3;
    dx[0] = dx0; dx[1] = dx1; dx[2] = dx2; dx[3] = dx3;
    s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3;
    t[0] = t0; t[1] = t1; t[2] = t2; t[3] = t3;
    for (k = 0; k < 4; k++) {
        Vtx16 *v = &gVtx[k];
        v->x = (s16)(s32)(p[k][0] + dx[k]);
        v->y = (s16)(s32)(p[k][1] + dy);
        v->z = (s16)(s32)p[k][2];
        v->flag = 0;
        v->s = (s16)s[k];
        v->t = (s16)(t[k] ? ((*(u16 *)(OBJ(bank, 10) + 8) - 1) << 5) : 0);
        v->r = 255; v->g = 255; v->b = 255; v->a = 255;
    }
    g = gDL;
    gDL = g + 2;
    g[0] = 0x0400103F;                      /* gSPVertex(4 vertices) */
    g[1] = (u32)gVtx;
    gVtx = gVtx + 4;
    gDL = g + 4;
    g[2] = 0xB1000204; g[3] = 0x00060402;   /* gSP2Triangles */
    gDL = g + 6;
    g[4] = 0xE7000000; g[5] = 0;
}

/* the textured-quad set-up the planks use */
static void Scr1_PlankMode(u8 *bank) {
    *gDL++ = 0xE7000000; *gDL++ = 0;
    func_80081924(&gDL, OBJ(bank, 10), 1);
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0x0C084000, 0x03024000);
    func_8007FBE8(&gDL, 2);
}

/* a post of the bridge (object 9) at plank i */
static void Scr1_Post(u8 *bank, s32 i) {
    Scr1_Plank *p = &B_800F5B64[i];
    func_80087EE8(B_800DBBF0, p->mid[0], p->mid[1], p->mid[2]);
    func_800883A0(B_800DBBF0, B_800F5B64[i].tilt);
    func_80088300(B_800DBBF0, 1.0f, 1.0f, 1.2f);
    func_800882C4(B_800DBBF0, D_800AC1C4);
    func_80080170(&gDL, B_800DBBF0);
    func_800831F4(&gDL, OBJ(bank, 9));
}

/* one of the four swinging lanterns: (place, object) twice, the second mirrored */
static void Scr1_Lantern(u8 *bank, void *place, s32 obj, s32 angle) {
    func_80087FD0(B_800DBC30, (s32)(vsSin(angle) * 3094.0f));
    func_800882C4(B_800DBC30, place);
    func_80087E70(B_800DBBF0, place);
    func_8008861C(B_800DBBF0);
    func_800882C4(B_800DBBF0, B_800DBC30);
    func_80080170(&gDL, B_800DBBF0);
    func_800831F4(&gDL, OBJ(bank, obj));
    func_80087E70(B_800DBBF0, place);
    func_8008861C(B_800DBBF0);
    func_800882C4(B_800DBBF0, B_800DBC30);
    func_80088134(B_800DBBF0, 1.0f, 1.0f, -1.0f);
    func_80088184(B_800DBBF0, -35.0f, -3.0f, -115.0f);
    func_80080170(&gDL, B_800DBBF0);
    func_800831F4(&gDL, OBJ(bank, obj));
}

/* 0x80019138: Bridge_Draw */
void func_80019138(void *cam, s32 pass, Scr1_Info *info, u8 *bank) {
    f32 sc = (f32)(1.0 / (f64)*(f32 *)(D_800AB49C + 0x34));
    if (pass == 0) {
        /* the river: 8 strips of 2 vertices, the texture scrolling and rippling */
        f32 s = 0.0f;
        s32 i;
        u32 *g;
        *gDL++ = 0xE7000000; *gDL++ = 0;
        func_80081924(&gDL, OBJ(bank, 11), 1);
        func_8007FB08(&gDL, 0x204);
        func_8007FB90(&gDL, 0x0C084000, 0x03024000);
        func_8007FBE8(&gDL, 2);
        for (i = 0; i < 16; i += 2) {
            Vtx16 *v = &gVtx[i];
            v->x = (s16)(s32)(sc * D_800AC304[i][0]);
            v->y = (s16)(s32)(sc * D_800AC304[i][1]);
            v->z = (s16)(s32)(sc * D_800AC304[i][2]);
            v->flag = 0;
            v->s = (s16)(s32)(((f64)(s + D_800AC3C4) + (f64)B_800F5FC0[i] * 0.3) * 32.0);
            v->t = 0;
            v->r = 170; v->g = 210; v->b = 110; v->a = 255;
            v = &gVtx[i];
            v[1].x = (s16)(s32)(sc * D_800AC304[i + 1][0]);
            v[1].y = (s16)(s32)(sc * D_800AC304[i + 1][1]);
            v[1].z = (s16)(s32)(sc * D_800AC304[i + 1][2]);
            v[1].flag = 0;
            v[1].s = (s16)(s32)(((f64)(s + D_800AC3C4) + (f64)B_800F5FC4[i] * 0.3) * 32.0);
            v[1].t = (s16)((*(u16 *)(OBJ(bank, 10) + 8) - 1) << 5);
            v[1].r = 80; v[1].g = 120; v[1].b = 50; v[1].a = 255;
            s = s + 64.0f;
        }
        g = gDL;
        gDL = g + 2;
        g[0] = 0x040040FF;                  /* gSPVertex(16 vertices) */
        g[1] = (u32)gVtx;
        gVtx = gVtx + 16;
        for (i = 0; i < 14; i += 2) {
            g = gDL++;
            if (i == 2 || i == 10) {
                g[0] = 0xB1000000 | Scr1_Tri(i, i + 2, i + 3, i);
                g[1] = Scr1_Tri(i, i + 3, i + 1, i);
            } else {
                g[0] = 0xB1000000 | Scr1_Tri(i, i + 2, i + 1, i);
                g[1] = Scr1_Tri(i + 2, i + 3, i + 1, i + 2);
            }
            gDL = g + 2;
        }
        *gDL++ = 0xE7000000; *gDL++ = 0;
        if ((f64)*(f32 *)((u8 *)cam + 0x28) < 0.0) {
            func_800831F4(&gDL, OBJ(bank, 2));
            func_800831F4(&gDL, OBJ(bank, 1));
        } else {
            func_800831F4(&gDL, OBJ(bank, 1));
            func_800831F4(&gDL, OBJ(bank, 2));
        }
        func_800831F4(&gDL, OBJ(bank, 0));
    } else if (pass == 1) {
        s32 i, nearest = -1, farthest = 9999;
        f32 camX = info->camPt[0];
        /* planks up to the camera's x are drawn from the left, the rest from the right */
        for (i = 0; i < 15; i++) {
            if (B_800F5B64[i].x < camX) nearest = i;
            else if (info->camPt[0] < B_800F5B64[i].x && farthest == 9999) farthest = i;
        }
        *gDL++ = 0xF9000000; *gDL++ = 0x01010101;   /* blend colour */
        Scr1_PlankMode(bank);
        *gDL++ = 0xB9000002; *gDL++ = 1;
        /* the planks' tops */
        for (i = 0; i < 14; i++) {
            Scr1_Plank *p = B_800F5B64;
            Scr1_PlankQuad(bank, p[i].b, 6.0f, 0xFE0, 1, p[i + 1].b, -6.0f, 0, 1,
                           p[i].a, 6.0f, 0xFE0, 0, p[i + 1].a, -6.0f, 0, 0, -2.0f);
        }
        *gDL++ = 0xB9000002; *gDL++ = 0;
        *gDL++ = 0xE7000000; *gDL++ = 0;
        for (i = 0; nearest >= i; i++) {
            Scr1_Post(bank, i);
            if (i >= 14) continue;
            Scr1_Identity();
            Scr1_PlankMode(bank);
            *gDL++ = 0xB9000002; *gDL++ = 1;
            Scr1_PlankQuad(bank, B_800F5B64[i].b, 6.0f, 0, 0, B_800F5B64[i + 1].b, -6.0f, 0xFE0, 0,
                           B_800F5B64[i].a, 6.0f, 0, 1, B_800F5B64[i + 1].a, -6.0f, 0xFE0, 1, 2.0f);
            *gDL++ = 0xB9000002; *gDL++ = 0;
        }
        for (i = 14; !(i < farthest); i--) {
            Scr1_Post(bank, i);
            if (i <= 0) continue;
            Scr1_Identity();
            Scr1_PlankMode(bank);
            *gDL++ = 0xB9000002; *gDL++ = 1;
            Scr1_PlankQuad(bank, B_800F5B64[i].b, -6.0f, 0xFE0, 0, B_800F5B64[i - 1].b, 6.0f, 0, 0,
                           B_800F5B64[i].a, -6.0f, 0xFE0, 1, B_800F5B64[i - 1].a, 6.0f, 0, 1, 2.0f);
            *gDL++ = 0xB9000002; *gDL++ = 0;
        }
        Scr1_Identity();
        func_800831F4(&gDL, OBJ(bank, 3));
        func_800831F4(&gDL, OBJ(bank, 4));
        Scr1_Sway(0.04);
        func_8000D2B0(&gDL, info, OBJ(bank, 5));
        Scr1_Identity();
        func_800831F4(&gDL, OBJ(bank, 6));
        func_8000D2B0(&gDL, info, OBJ(bank, 7));
        Scr1_Lantern(bank, D_800AC204, 12, B_800DBCC0[0]);
        Scr1_Lantern(bank, D_800AC244, 13, B_800DBCC0[0] + 0x2AAA);
        Scr1_Lantern(bank, D_800AC284, 14, B_800DBCC0[0] + 0x5555);
        Scr1_Lantern(bank, D_800AC2C4, 15, B_800DBCC0[0] + 0x8000);
        Scr1_Sway(0.15);
        func_8000D2B0(&gDL, info, OBJ(bank, 8));
    } else if (pass == 3) {
        func_8000DD0C((f32 *)cam);                     /* snow */
    }
}
