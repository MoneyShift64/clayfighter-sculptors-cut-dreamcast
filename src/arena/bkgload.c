/*
 * The arena ("background") tables, loading and runtime (0x8000A590-0x8000D2B0).
 *
 *   func_8000A7A4  fills the "nullbkg" (unused) arena slots with the first real arena
 *   func_8000A984  reads every arena's definition once to build the name table and the zone
 *                  links between arenas (where a fighter knocked through a zone lands)
 *   func_8000AF40  loads an arena (its BKG bank and definition), lights, the 3D ground, the
 *                  projection, and starts its script; func_8000B704 frees it
 *   func_8000B894  the per-tick update (the arena's script)
 *   func_8000B908  clears the screen to the arena's colour
 *   func_8000B998 / BCA8 / BE80 / C07C  the four drawing passes (each gets the camera and the
 *                  combat handler, NULL outside a fight)
 *   func_8000C2B8 / C9C8  helpers for the scripts: draw a model / a sprite, hiding what is
 *                  behind fighter 1 as seen along the fight line
 *   func_8000CEDC  fills in the ArenaContext the scripts get
 *
 * See arena.h for the data formats.
 */
#include "common.h"
#include "mpfmem.h"
#include "mpfplat.h"
#include "combat.h"
#include "n64gfx.h"
#include "arena.h"

/* ---- the arena tables (game data) ---- */
extern s32 D_800AB258;                    /* the nullbkg slots have been filled in */
extern s32 D_800AB25C;                    /* the name / zone tables have been built */
extern u8  D_800AB260[NUM_ARENAS][4];     /* clear / fog colour r, g, b; [3] non-zero: clear the screen */
extern s16 D_800AB2C8[NUM_ARENAS][2];     /* fog: near, far (0 = no fog) */
extern f32 D_800AB330[NUM_ARENAS];        /* far clipping distance */
extern s8  D_800AB398[NUM_ARENAS][2];     /* -> B_800F5D0C, B_800F5BD8 */
extern ArenaUpdateFn D_800AB3CC[NUM_ARENAS];   /* the arenas' scripts: update */
extern ArenaDrawFn   D_800AB434[NUM_ARENAS];   /* ... and drawing */
extern s32 D_800AB4B0;                    /* ticks with handler->v54 >= 4 */
extern s32 D_800AB4B4;                    /* a zone knock-out is going on */
extern s32 D_800AB4B8;                    /* ... it started in state 10 */
extern f32 D_800AB4BC;                    /* ... time since it started */
extern s32 D_800AB4C0;                    /* ... handler->v148 was set during it */
extern f32 D_800AB4C4;                    /* ... time since then */
extern const char D_800C5A80[];           /* "<Illegal Background Index>" */
extern const char D_800C5A9C[];           /* "nullbkg" */
extern const char D_800C5AA4[];           /* "" */

/* ---- built by func_8000A7A4 / func_8000A984 (BSS) ---- */
extern s16  B_800DBAF0[NUM_ARENAS];       /* first part of each arena (in B_800DBB24) */
extern s16 *B_800DBB24;                   /* per part (all arenas): its first zone (in B_800DBB28) */
extern s8  *B_800DBB28;                   /* per zone: where it leads (arena, part, zone) or -1s */
extern char *B_800DBB2C;                  /* all the arenas' names */
extern s32  B_800DBB30[NUM_ARENAS];       /* each arena's name in B_800DBB2C */
extern u8   B_800F2D28[NUM_ARENAS];       /* 1: a real arena, 0: a nullbkg slot */

/* ---- the current arena ---- */
extern s32 B_800F15E4;                    /* ticks since it was loaded */
extern f32 B_800DBBA0, B_800DBBA4, B_800DBBA8;   /* the light colour the models were last tinted with */
extern u8  B_800DBBB0[];                  /* the drawing passes' matrix */
extern f32 B_800F5B84, B_800F7780, B_800F4E90;   /* (combat.h) the light colour r, g, b */

/* the arena's 3D ground (ArenaGround in src/arena/arenadraw.c; only the fields set here) */
typedef struct ArenaGroundSettings {
    /* 0x00 */ u8  pad00[0x54];
    /* 0x54 */ u8  glowRgb[4];
    /* 0x58 */ u8  nearRgb[4];
    /* 0x5C */ f32 glowRadius;
    /* 0x60 */ u8  pad60[0x14];
    /* 0x74 */ s32 sun;
    /* 0x78 */ f32 glowDist;
    /* 0x7C */ u8  rimRgb[4];
    /* 0x80 */ u8  centreRgb[4];
    /* 0x84 */ f32 floorTexScale;
    /* 0x88 */ s32 floorScroll[2];
    /* 0x90 */ s32 floorScrollSpeed[2];
    /* 0x98 */ u8  overlayRgb[4];
    /* 0x9C */ u8  pad9C[0x14];
    /* 0xB0 */ s32 shadows;
    /* 0xB4 */ u8  backRgb[4][4];
    /* 0xC4 */ f32 sunPos[3];
} ArenaGroundSettings;
extern ArenaGroundSettings B_800F5BF0;

void func_800245B0(void *g, s32 mode, s32 angle, s32 domeTex, s32 floorTex, s32 overlayTex);
void func_80024904(void *g);
void func_80024994(void *g, u32 **dl, void *m);
void func_80028490(void *g, u32 **dl, void *cam);

static void Rgb(u8 *c, u8 r, u8 g, u8 b) {
    c[0] = r;
    c[1] = g;
    c[2] = b;
}

/* the BkgBank's object i */
#define BANK_OBJECT(bank, i) ((u8 *)(bank) + (bank)->objects[i].offset)

/* 0x8000A590: builds the arena tables if that hasn't been done yet */
void func_8000A590(void) {
    if (!D_800AB258) func_8000A7A4();
    if (!D_800AB25C) func_8000A984();
}

/* 0x8000A5D4: Arena_LoadBank - unpacks an arena's BKG bank */
BkgBank *func_8000A5D4(s32 arena) {
    if (!D_800AB258) func_8000A7A4();
    if (!D_800AB25C) func_8000A984();
    return PrepareBkgBank(D_800AB150[arena]);
}

/* 0x8000A634: Arena_LoadDef - loads an arena's definition file */
u8 *func_8000A634(s32 arena) {
    u8 *def;
    if (!D_800AB258) func_8000A7A4();
    if (!D_800AB25C) func_8000A984();
    def = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager,
                                         D_ArenaTextureFiles[arena] - D_ArenaGeometryFiles[arena], 0);
    RomCopy(def, D_ArenaGeometryFiles[arena], mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, def));
    return def;
}

/* 0x8000A6E4: Arena_Name */
const char *func_8000A6E4(s32 arena) {
    if (arena < NUM_ARENAS) return B_800DBB2C + B_800DBB30[arena];
    return D_800C5A80;
}

/* 0x8000A71C: Arena_ZoneLink - where zone 'zone' of part 'part' of an arena leads: another
   arena's part and zone, or (no link) the same ones */
void func_8000A71C(s32 arena, s32 part, s32 zone, s32 *toArena, s32 *toPart, s32 *toZone) {
    s8 *link = B_800DBB28 + (B_800DBB24[B_800DBAF0[arena] + part] + zone) * 3;
    if (link[0] == -1) {
        *toArena = arena;
        *toPart = part;
        *toZone = zone;
    } else {
        *toArena = link[0];
        *toPart = link[1];
        *toZone = link[2];
    }
}

/* 0x8000A7A4: Arena_FillNullSlots - the unused arena slots (their bank starts with the name
   "nullbkg") get the files of the first real arena */
void func_8000A7A4(void) {
    s32 first = -1;
    s32 i;
    char *name = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 8, 0);
    mpMemSet(name, mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, name), 0);

    i = 0;
    do {
        RomCopy(name, D_800AB150[i], 7);
        if (!mpStrEqualNoCase(name, D_800C5A9C)) {
            first = i;     /* (stays -1 if every slot is empty) */
            break;
        }
        i++;
    } while (i < NUM_ARENAS);

    for (i = 0; i < NUM_ARENAS; i++) {
        RomCopy(name, D_800AB150[i], 7);
        if (mpStrEqualNoCase(name, D_800C5A9C)) {
            D_800AB150[i] = D_800AB150[first];
            D_800AB1B8[i] = D_800AB1B8[first];
            D_ArenaGeometryFiles[i] = D_ArenaGeometryFiles[first];
            D_ArenaTextureFiles[i] = D_ArenaTextureFiles[first];
            B_800F2D28[i] = 0;
        } else {
            B_800F2D28[i] = 1;
        }
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, name);
    D_800AB258 = 1;
}

/* the parts (ArenaDef, 0x60 bytes) and zones (0x38 bytes) of an arena definition file */
#define DEF_NUM_PARTS(d)  (*(s32 *)((d) + 0x38))
#define DEF_PARTS(d)      ((ArenaDef *)((d) + *(s32 *)((d) + 0x3C)))
#define DEF_NAME(d)       ((char *)(d) + 8)
#define ZONE_SIZE         0x38
#define ZONE_NAME(z)      ((char *)(z) + 0x04)
#define ZONE_TARGET(z)    ((char *)(z) + 0x10)  /* the zone it leads to, "" = none */

/* 0x8000A984: Arena_BuildTables - loads every arena's definition to make the name table and
   the zone links (a zone leads to the first zone, in any arena, whose name is its target) */
void func_8000A984(void) {
    u32 hndl[NUM_ARENAS];
    u8 *def[NUM_ARENAS];
    s32 i, j, k, total;

    if (!D_800AB258) func_8000A7A4();
    mpLinkedMemoryManager_Pack(&gN64MemoryManager);
    for (i = 0; i < NUM_ARENAS; i++) {
        hndl[i] = mpLinkedMemoryManager_AllocHndl(&gN64MemoryManager,
                                                  D_ArenaTextureFiles[i] - D_ArenaGeometryFiles[i], 0);
        def[i] = mpLinkedMemoryManager_LockHndl(&gN64MemoryManager, hndl[i]);
        RomCopy(def[i], D_ArenaGeometryFiles[i], mpLinkedMemoryManager_SizeOfHndl(&gN64MemoryManager, hndl[i]));
    }

    /* the names */
    total = 0;
    for (i = 0; i < NUM_ARENAS; i++) {
        B_800DBB30[i] = total;
        total = total + 1 + mpStrLen(DEF_NAME(def[i]));
    }
    B_800DBB2C = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, total, 0);
    for (i = 0; i < NUM_ARENAS; i++) mpStrCpy(B_800DBB2C + B_800DBB30[i], DEF_NAME(def[i]));

    /* the parts */
    total = 0;
    for (i = 0; i < NUM_ARENAS; i++) {
        B_800DBAF0[i] = (s16)total;
        total += DEF_NUM_PARTS(def[i]);
    }
    B_800DBB24 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, total * 2, 0);
    bzero(B_800DBB24, mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, B_800DBB24));

    /* the zones */
    total = 0;
    for (i = 0; i < NUM_ARENAS; i++) {
        ArenaDef *part = DEF_PARTS(def[i]);
        for (j = 0; j < DEF_NUM_PARTS(def[i]); j++, part++) {
            B_800DBB24[B_800DBAF0[i] + j] = (s16)total;
            total += part->numZones;
        }
    }
    B_800DBB28 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, total * 3, 0);
    bzero(B_800DBB28, mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, B_800DBB28));

    /* the links */
    for (i = 0; i < NUM_ARENAS; i++) {
        ArenaDef *part = DEF_PARTS(def[i]);
        for (j = 0; j < DEF_NUM_PARTS(def[i]); j++, part++) {
            u8 *zone = def[i] + part->zonesOffset;
            for (k = 0; k < part->numZones; k++, zone += ZONE_SIZE) {
                s8 *link = B_800DBB28 + (B_800DBB24[B_800DBAF0[i] + j] + k) * 3;
                s32 found = 0, toArena = 0, toPart = 0, toZone = 0;
                if (!mpStrEqualNoCase(ZONE_TARGET(zone), D_800C5AA4)) {
                    s32 a, p, z;
                    for (a = 0; a < NUM_ARENAS && !found; a++) {
                        ArenaDef *ap = DEF_PARTS(def[a]);
                        for (p = 0; p < DEF_NUM_PARTS(def[a]) && !found; p++, ap++) {
                            u8 *az = def[a] + ap->zonesOffset;
                            for (z = 0; z < ap->numZones && !found; z++, az += ZONE_SIZE) {
                                if (mpStrEqualNoCase(ZONE_TARGET(zone), ZONE_NAME(az))) {
                                    toArena = a;
                                    toPart = p;
                                    toZone = z;
                                    found = 1;
                                }
                            }
                        }
                    }
                }
                if (found) {
                    link[0] = (s8)toArena;
                    link[1] = (s8)toPart;
                    link[2] = (s8)toZone;
                } else {
                    link[0] = link[1] = link[2] = -1;
                }
            }
        }
    }
    D_800AB25C = 1;
    for (i = 0; i < NUM_ARENAS; i++) mpLinkedMemoryManager_FreeHndl(&gN64MemoryManager, hndl[i]);
    mpLinkedMemoryManager_Pack(&gN64MemoryManager);
}

/* 0x8000AF40: Arena_Load */
void func_8000AF40(s32 arena) {
    ArenaContext ctx;
    s32 i;
    u8 *src;
    ArenaGroundSettings *g = &B_800F5BF0;

    if (D_800AB4A0 != NULL || D_800AB49C != NULL) func_8000B704();
    D_800AB4A0 = func_8000A5D4(arena);
    D_800AB49C = func_8000A634(arena);
    B_800F15E4 = 0;

    /* the lights (0x3C bytes each; the range at +0x28 is made 1.35 times longer) */
    D_800AB4A4 = *(s32 *)(D_800AB49C + 0x40);
    D_800AB4A8 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, D_800AB4A4 * 0x3C, 0);
    src = D_800AB49C + *(s32 *)(D_800AB49C + 0x44);
    for (i = 0; i < D_800AB4A4; i++, src += 0x3C) {
        u8 *l = D_800AB4A8 + i * 0x3C;
        s32 k;
        *(s32 *)l = src[0];
        for (k = 4; k < 0x3C; k += 4) {
            if (k == 0x28) *(f32 *)(l + k) = (f32)((f64)*(f32 *)(src + k) * 1.35);
            else *(f32 *)(l + k) = *(f32 *)(src + k);
        }
    }

    /* the 3D ground */
    switch ((u32)arena) {
    case 2:
        func_800245B0(g, 1, 0, 0, 0, 0);
        g->shadows = 1;
        break;
    case 5:
        func_800245B0(g, 1, 0, 3, 0, 1);
        Rgb(g->glowRgb, 50, 50, 50);
        g->glowRadius = 0.9f;
        Rgb(g->nearRgb, 255, 0, 0);
        g->sun = 0;
        Rgb(g->rimRgb, 20, 20, 20);
        Rgb(g->centreRgb, 100, 0, 0);
        Rgb(g->overlayRgb, 70, 70, 70);
        Rgb(g->backRgb[0], 200, 150, 150);
        Rgb(g->backRgb[1], 180, 120, 120);
        Rgb(g->backRgb[2], 0, 0, 0);
        Rgb(g->backRgb[3], 0, 0, 0);
        break;
    case 12:
        func_800245B0(g, 1, 0x4000, 0, 0, 2);
        g->shadows = 1;
        break;
    case 14:
        func_800245B0(g, 2, 0x8E38, 0, 0, -1);
        g->floorScrollSpeed[0] = 0;
        g->floorScrollSpeed[1] = 0;
        g->floorTexScale = 0.8f;
        break;
    case 19:
        func_800245B0(g, 1, 0x8000, 1, 0, 0);
        Rgb(g->glowRgb, 110, 110, 110);
        g->shadows = 1;
        break;
    case 21:
        func_800245B0(g, 1, 0x4000, 2, 0, 3);
        Rgb(g->glowRgb, 110, 110, 110);
        Rgb(g->backRgb[2], 50, 150, 50);
        Rgb(g->backRgb[3], 15, 25, 15);
        break;
    case 22:
        func_800245B0(g, 3, 0x8AAA, 0, -1, -1);
        g->glowRadius = 1.0f;
        break;
    case 23:
        func_800245B0(g, 2, 0x4000, 0, 0, -1);
        g->floorScrollSpeed[0] = 0;
        g->floorScrollSpeed[1] = 0;
        g->floorTexScale = 0.8f;
        Rgb(g->centreRgb, 0, 90, 10);
        Rgb(g->nearRgb, 0, 150, 20);
        g->sun = 0;
        break;
    case 24:
        func_800245B0(g, 2, 0x4000, 0, 0, -1);
        Rgb(g->glowRgb, 90, 90, 90);
        g->glowRadius = 1.3f;
        g->glowDist = 0.4f;
        Rgb(g->nearRgb, 255, 255, 255);
        g->sun = 0;
        Rgb(g->rimRgb, 80, 80, 80);
        Rgb(g->centreRgb, 150, 150, 150);
        g->floorScrollSpeed[0] = 0;
        g->floorScrollSpeed[1] = 0;
        g->floorTexScale = 0.8f;
        g->sunPos[0] = 0.0f;
        g->sunPos[1] = 100.0f;
        g->sunPos[2] = 500.0f;
        break;
    case 25:
        func_800245B0(g, 2, 0, 0, 0, -1);
        Rgb(g->glowRgb, 0, 20, 50);
        Rgb(g->nearRgb, 0, 20, 50);
        g->sun = 0;
        Rgb(g->rimRgb, 0, 20, 50);
        Rgb(g->centreRgb, 0, 20, 50);
        g->floorScrollSpeed[0] = 0;
        g->floorScrollSpeed[1] = 0;
        g->floorTexScale = 0.8f;
        break;
    case 0: case 1: case 3: case 4: case 6: case 7: case 8: case 9: case 10: case 11:
    case 13: case 15: case 16: case 17: case 18: case 20:
        func_800245B0(g, 0, 0, -1, -1, -1);     /* no ground */
        break;
    default:
        break;
    }

    /* the models, scaled to the arena */
    for (i = 0; i < D_800AB4A0->numObjects; i++) {
        u8 *obj = BANK_OBJECT(D_800AB4A0, i);
        if (BKG_OBJECT_TYPE(obj) == BKG_TYPE_MODEL) {
            func_80082780((GfxModel *)obj, (f32)(1.0 / (f64)*(f32 *)(D_800AB49C + 0x34)));
            func_80082B5C((GfxModel *)obj);
        }
    }

    B_800F7EC8 = arena;
    func_8007FF20(0x21, 40.0f,
                  (f32)((f64)D_800AB330[arena] * (1.0 / (f64)*(f32 *)(D_800AB49C + 0x34)) * 1.27));
    B_800F5D0C = D_800AB398[B_800F7EC8][0];
    B_800F5BD8 = D_800AB398[B_800F7EC8][1];
    B_800F5B84 = B_800F7780 = B_800F4E90 = 1.0f;
    B_800DBBA0 = B_800DBBA4 = B_800DBBA8 = 1.0f;

    if (D_800AB3CC[arena] != NULL) {
        func_8000CEDC(&ctx, NULL, NULL, 1);
        D_800AB3CC[arena](0, &ctx, D_800AB4A0);
    }
    B_800F15E4 = 1;
}

/* 0x8000B704: Arena_Free */
void func_8000B704(void) {
    ArenaContext ctx;
    s32 i;

    if (D_800AB4A0 != NULL || D_800AB49C != NULL) {
        if (D_800AB3CC[B_800F7EC8] != NULL) {
            func_8000CEDC(&ctx, NULL, NULL, 1);
            D_800AB3CC[B_800F7EC8](-1, &ctx, D_800AB4A0);
        }
    }
    if (D_800AB4A0 != NULL) {
        for (i = 0; i < D_800AB4A0->numObjects; i++) {
            u8 *obj = BANK_OBJECT(D_800AB4A0, i);
            if (BKG_OBJECT_TYPE(obj) == BKG_TYPE_MODEL) func_80082D60((GfxModel *)obj);
        }
        if (D_800AB4A0 != NULL) {
            mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800AB4A0);
            D_800AB4A0 = NULL;
        }
    }
    if (D_800AB49C != NULL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800AB49C);
        D_800AB49C = NULL;
    }
    if (D_800AB4A8 != NULL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800AB4A8);
        D_800AB4A8 = NULL;
        D_800AB4A4 = 0;
    }
    func_80024904(&B_800F5BF0);
}

/* 0x8000B894: Arena_Update - runs the arena's script once */
void func_8000B894(CombatHandler *h) {
    ArenaContext ctx;
    func_8000CEDC(&ctx, h, NULL, 1);
    if (D_800AB3CC[B_800F7EC8] != NULL) D_800AB3CC[B_800F7EC8](B_800F15E4, &ctx, D_800AB4A0);
    B_800F15E4++;
}

/* 0x8000B908: Arena_Clear - clears the screen to the arena's colour (arena 9: black) */
void func_8000B908(void) {
    s32 a = B_800F7EC8;
    if (D_800AB260[a][3]) {
        if (a == 9) func_8007E720(&gDL, 0, 0, 0);
        else func_8007E720(&gDL, D_800AB260[B_800F7EC8][0], D_800AB260[B_800F7EC8][1], D_800AB260[B_800F7EC8][2]);
    }
}

/* the four passes start the same way: the camera matrix, then the identity model matrix */
static void Arena_BeginPass(void *cam) {
    func_80087E70(B_800DBBB0, cam);
    func_8008861C(B_800DBBB0);
}

#ifdef TARGET_PORT
/* (the port) marks in the display list around the arena's drawing (G_SPNOOP; port.h's
   GFX_MARK_SCENERY_ON / _OFF): the Dreamcast sorts the arena's undepth-tested parts by them */
#define ARENA_MARK(on) do { gDL[0] = 0; gDL[1] = (on) ? 0x53434E31u : 0x53434E30u; gDL += 2; } while (0)
#else
#define ARENA_MARK(on) ((void)0)
#endif

static void Arena_LoadMatrices(void) {
    func_8008004C(&gDL, B_800DBBB0);
    func_80087E20(B_800DBBB0);
    func_80080170(&gDL, B_800DBBB0);
}

/* gDPSetFogColor + gSPFogPosition(near, far), for the arenas with fog */
static void Arena_Fog(void) {
    s32 a = B_800F7EC8;
    if (D_800AB2C8[a][0] != 0) {
        u32 *dl = gDL;
        s32 nearZ = D_800AB2C8[a][0];
        s32 range = D_800AB2C8[a][1] - nearZ;
        gDL = dl + 4;
        dl[0] = 0xF8000000;
        dl[1] = ((u32)D_800AB260[a][0] << 24) | ((u32)D_800AB260[a][1] << 16) | ((u32)D_800AB260[a][2] << 8) | 0xFF;
        dl[2] = 0xBC000008;
        dl[3] = ((u32)(128000 / range) << 16) | ((u32)(((500 - nearZ) << 8) / range) & 0xFFFF);
    }
}

/* 0x8000B998: Arena_DrawPass0 - the ground, then (re-tinting the models if the light colour
   changed) the script's pass 0 */
void func_8000B998(void *cam, CombatHandler *h) {
    ArenaContext ctx;
    s32 i;

    func_8000CEDC(&ctx, h, cam, 0);
    ARENA_MARK(1);
    Arena_BeginPass(cam);
    func_80024994(&B_800F5BF0, &gDL, B_800DBBB0);
    Arena_LoadMatrices();
    if (!(B_800F5B84 == B_800DBBA0 && B_800F7780 == B_800DBBA4 && B_800F4E90 == B_800DBBA8)) {
        for (i = 0; i < D_800AB4A0->numObjects; i++) {
            u8 *obj = BANK_OBJECT(D_800AB4A0, i);
            if (BKG_OBJECT_TYPE(obj) == BKG_TYPE_MODEL) {
                func_80082ED8((GfxModel *)obj, B_800F5B84, B_800F7780, B_800F4E90);
            }
        }
        B_800DBBA0 = B_800F5B84;
        B_800DBBA4 = B_800F7780;
        B_800DBBA8 = B_800F4E90;
    }
    Arena_Fog();
    if (D_800AB434[B_800F7EC8] != NULL) D_800AB434[B_800F7EC8](cam, 0, &ctx, D_800AB4A0);
    ARENA_MARK(0);
}

/* 0x8000BCA8: Arena_DrawPass1 */
void func_8000BCA8(void *cam, CombatHandler *h) {
    ArenaContext ctx;
    func_8000CEDC(&ctx, h, cam, 0);
    ARENA_MARK(1);
    Arena_BeginPass(cam);
    Arena_LoadMatrices();
    Arena_Fog();
    if (D_800AB434[B_800F7EC8] != NULL) D_800AB434[B_800F7EC8](cam, 1, &ctx, D_800AB4A0);
    ARENA_MARK(0);
}

/* 0x8000BE80: Arena_DrawPass2 - the script's pass 2, or (no script) the bank's first model */
void func_8000BE80(void *cam, CombatHandler *h) {
    ArenaContext ctx;
    func_8000CEDC(&ctx, h, cam, 0);
    ARENA_MARK(1);
    Arena_BeginPass(cam);
    Arena_LoadMatrices();
    Arena_Fog();
    if (D_800AB434[B_800F7EC8] != NULL) D_800AB434[B_800F7EC8](cam, 2, &ctx, D_800AB4A0);
    else func_800831F4(&gDL, (GfxModel *)BANK_OBJECT(D_800AB4A0, 0));
    ARENA_MARK(0);
}

/* 0x8000C07C: Arena_DrawPass3 - the script's pass 3, then the ground's top layer in arenas
   2 and 12 (only while handler->v54 == 0) and 24 */
void func_8000C07C(void *cam, CombatHandler *h) {
    ArenaContext ctx;
    func_8000CEDC(&ctx, h, cam, 0);
    Arena_BeginPass(cam);
    Arena_LoadMatrices();
    Arena_Fog();
    if (D_800AB434[B_800F7EC8] != NULL) D_800AB434[B_800F7EC8](cam, 3, &ctx, D_800AB4A0);
    switch (B_800F7EC8) {
    case 2:
    case 12:
        if (!ctx.idle) break;
        /* fall through */
    case 24:
        func_80028490(&B_800F5BF0, &gDL, cam);
        break;
    }
}

/* a point through the model matrix (only x and z are needed) */
static void Arena_ModelPointXZ(const f32 *p, f32 *x, f32 *z) {
    f32 (*m)[4] = B_800F7DB8;
    *x = (f32)((f64)(p[0] * m[0][0] + p[1] * m[1][0] + p[2] * m[2][0]) + (f64)m[3][0]);
    *z = (f32)((f64)(p[0] * m[0][2] + p[1] * m[1][2] + p[2] * m[2][2]) + (f64)m[3][2]);
}

/* which side of the line through fighter 1 along the fight direction a point is: < 0 behind */
static s32 Arena_Behind(ArenaContext *ctx, f32 x, f32 z) {
    f32 d = (ctx->fighter1[2] - z) * ctx->dirX - (ctx->fighter1[0] - x) * ctx->dirZ;
    return (f64)d < 0.0;
}

/* 0x8000C2B8: Arena_DrawModelCut - in a fight, draws a model without the cells (with flag set)
   that have three or four corners behind fighter 1's line */
void func_8000C2B8(BkgModel *m, ArenaContext *ctx) {
    s32 cellSize, n, i, j;
    u8 *cells;
    f32 *points;

    if (!ctx->inFight) return;
    cellSize = m->subsPerCell * 20 + 0x18;
    cells = (u8 *)m + m->cellsOffset;
    points = (f32 *)((u8 *)m + m->pointsOffset);
    n = m->gridX * m->gridY * m->gridZ;
    for (i = 0; i < n; i++) {
        BkgCell *c = (BkgCell *)(cells + i * cellSize);
        static const u8 order[4] = { 0, 3, 2, 1 };
        s32 behind = 0;
        if (c->flag == 0) continue;
        for (j = 0; j < 4; j++) {
            f32 x, z;
            Arena_ModelPointXZ(&points[c->corner[order[j]] * 3], &x, &z);
            behind += Arena_Behind(ctx, x, z);
        }
        c->hidden = (behind >= 3);
    }
    func_800831F4(&gDL, (GfxModel *)m);
    for (i = 0; i < n; i++) ((BkgCell *)(cells + i * cellSize))->hidden = 0;
}

/* 0x8000C9C8: Arena_DrawSpriteCut - in a fight, draws a sprite unless three or four of its
   corners are behind fighter 1's line */
void func_8000C9C8(void *frame, ArenaContext *ctx, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    f32 corner[4][4];
    s32 behind = 0, j;

    if (!ctx->inFight) return;
    func_800836B0((CharFrame *)frame, x, y, z, up, right, flags, corner[0], corner[1], corner[2], corner[3]);
    for (j = 0; j < 4; j++) {
        f32 cx, cz;
        Arena_ModelPointXZ(corner[j], &cx, &cz);
        behind += Arena_Behind(ctx, cx, cz);
    }
    if (behind < 3) func_80083A64(&gDL, frame, x, y, z, up, right, flags);
}

/* 0x8000CEDC: Arena_GetContext - what the scripts are told: the fight state, the fighters, the
   camera (cam: its matrix, may be NULL) and the zone knock-out; 'advance' moves the knock-out
   timers on (the update does, the drawing passes don't) */
void func_8000CEDC(ArenaContext *ctx, CombatHandler *h, f32 *cam, s32 advance) {
    if (h == NULL) {
        /* (these defaults don't line up with the fields as a fight fills them in) */
        ctx->inFight = 0;
        ctx->idle = 1;
        ctx->lateTicks = 0;
        ctx->state = 0;
        ctx->part = 0;
        ctx->fighter1[0] = 0; ctx->fighter1[1] = 0; ctx->fighter1[2] = 0;
        ctx->fighter2[0] = 1.0f; ctx->fighter2[1] = 0; ctx->fighter2[2] = 0;
        ctx->camPos[0] = 0; ctx->camPos[1] = 1.0f; ctx->camPos[2] = 1.0f;
        ctx->dirX = 1.0f; ctx->dirZ = 0;
        ctx->camUp[0] = 0; ctx->camUp[1] = 1.0f; ctx->camUp[2] = 0;
        ctx->camRight[0] = 1.0f; ctx->camRight[1] = 0; ctx->camRight[2] = 0;
        ctx->zone = NULL;
        ctx->zoneFromState10 = 0;
        ctx->zoneTime = 0.0f;
        ctx->zoneFlag = 0;
        ctx->zoneFlagTime = 0.0f;
        D_800AB4B0 = 0;
        D_800AB4B4 = 0;
        D_800AB4B8 = 0;
        D_800AB4BC = 0.0f;
        D_800AB4C0 = 0;
        D_800AB4C4 = 0.0f;
        return;
    }
    ctx->inFight = 1;
    ctx->idle = (h->v54 == 0);
    ctx->lateTicks = D_800AB4B0;
    ctx->state = h->v54;
    ctx->part = h->arenaIndex;
    ctx->fighter1[0] = h->fighter1->v18[0];
    ctx->fighter1[1] = -h->fighter1->screenY;
    ctx->fighter1[2] = h->fighter1->v18[2];
    ctx->fighter2[0] = h->fighter2->v18[0];
    ctx->fighter2[1] = -h->fighter2->screenY;
    ctx->fighter2[2] = h->fighter2->v18[2];
    if (cam != NULL) {
        /* the camera's position: (0, 0, 0) through its matrix (the original multiplies all
           three rows of x by the vector's x) */
        f32 v[3] = { 0, 0, 0 };
        ctx->camPos[0] = (f32)((f64)(cam[0] * v[0] + cam[4] * v[0] + cam[8] * v[0]) + (f64)cam[12]);
        ctx->camPos[1] = (f32)((f64)(v[0] * cam[1] + v[1] * cam[5] + v[2] * cam[9]) + (f64)cam[13]);
        ctx->camPos[2] = (f32)((f64)(v[0] * cam[2] + v[1] * cam[6] + v[2] * cam[10]) + (f64)cam[14]);
    } else {
        ctx->camPos[0] = 0.0f;
        ctx->camPos[1] = 0.0f;
        ctx->camPos[2] = 0.0f;
    }
    ctx->dirX = h->camDirX;
    ctx->dirZ = h->camDirZ;
    if (cam != NULL) {
        ctx->camUp[0] = cam[4]; ctx->camUp[1] = cam[5]; ctx->camUp[2] = cam[6];
        ctx->camRight[0] = cam[0]; ctx->camRight[1] = cam[1]; ctx->camRight[2] = cam[2];
    } else {
        ctx->camUp[0] = 0; ctx->camUp[1] = 1.0f; ctx->camUp[2] = 0;
        ctx->camRight[0] = 1.0f; ctx->camRight[1] = 0; ctx->camRight[2] = 0;
    }
    if (h->v54 >= 4) D_800AB4B0++;

    if (h->v140 != -1) {
        ctx->zone = h->v134 + h->arena->zonesOffset + h->v140 * 0x38;
        if (!D_800AB4B4) {
            D_800AB4B8 = (h->v54 == 10);
            D_800AB4BC = 0.0f;
            D_800AB4C0 = 0;
            D_800AB4C4 = 0.0f;
        }
        if (h->v148) D_800AB4C0 = 1;
        D_800AB4B4 = 1;
    } else {
        D_800AB4B4 = 0;
        ctx->zone = NULL;
    }
    ctx->zoneFromState10 = D_800AB4B8;
    ctx->zoneTime = D_800AB4BC;
    ctx->zoneFlag = D_800AB4C0;
    if (advance) D_800AB4BC = D_800AB4BC + h->dt;
    if (D_800AB4C0 && advance) D_800AB4C4 = D_800AB4C4 + h->dt;
    ctx->zoneFlagTime = D_800AB4C4;
}
