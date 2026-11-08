/*
 * The arena ("background", BKG) core: 0x80006A00-0x8000D2B0.
 *
 *   bkgbank.c  PrepareBkgBank: turns an arena's packed "BKG bank" (ROM) into ready-to-run N64
 *              display lists, vertices and textures.
 *   bkgload.c  the arena tables ("nullbkg" slots, zone links between arenas), loading/freeing
 *              the current arena, its per-tick update and the four drawing passes.
 *
 * Every arena has two files in the ROM:
 *   - the arena definition (D_ArenaGeometryFiles[i] .. D_ArenaTextureFiles[i]): a BkgDef header
 *     followed by ArenaDefs (the playable parts of the arena, see cf3char_structs.h), zones,
 *     lights, ...; loaded as is into D_800AB49C;
 *   - the BKG bank (D_800AB150[i] .. D_800AB1B8[i]): packed models and textures, turned into a
 *     BkgBank by PrepareBkgBank (D_800AB4A0).
 *
 * All offsets in the files are from the start of the file they are in.
 *
 * Other engineers (arena scripts, arena renderer) read this header; the structures below are
 * what this part of the code works out about the formats.
 */
#ifndef ARENA_H
#define ARENA_H

#include "common.h"

struct CombatHandler;

/* ======================================================================================
 * The packed BKG bank in the ROM
 * ====================================================================================== */

/* header, 0x3C bytes, followed by BkgFileObject[numObjects] */
typedef struct BkgFileHeader {
    /* 0x00 */ u32 v0;
    /* 0x04 */ s32 numObjects;
    /* 0x08 */ u32 fileSize;          /* bytes to load from the ROM */
    /* 0x0C */ u32 outSize;           /* size of the BkgBank built from it */
    /* 0x10 */ s32 numTextures;
    /* 0x14 */ u32 texturesOffset;    /* BkgFileTexture[numTextures] */
    /* 0x18 */ s32 numPositions;      /* shared vertex positions: 3 x 11-bit signed, packed */
    /* 0x1C */ u32 positionsOffset;
    /* 0x20 */ s32 numTexCoords;      /* shared texture coordinates: 2 x 11-bit signed, packed (<< 5) */
    /* 0x24 */ u32 texCoordsOffset;
    /* 0x28 */ s32 numColours;        /* shared vertex colours: 4 x 5-bit, packed (<< 3, 0xF8 -> 0xFF) */
    /* 0x2C */ u32 coloursOffset;
    /* 0x30 */ s32 bias[3];           /* added to every position (16-bit) */
} BkgFileHeader;

/* one object of the bank (12 bytes) */
typedef struct BkgFileObject {
    /* 0x00 */ s8  packed;            /* non-zero: the object is just mpDataDecompress'ed */
    /* 0x01 */ u8  pad01[3];
    /* 0x04 */ u32 srcOffset;         /* in the file: a BkgFileModel (or packed data) */
    /* 0x08 */ u32 dstOffset;         /* in the BkgBank */
} BkgFileObject;

/* a texture (20 bytes): a colour-indexed image with its palette in front of it */
typedef struct BkgFileTexture {
    /* 0x00 */ u8  is8bit;            /* 0: CI4 (16-colour palette), 1: CI8 (256 colours) */
    /* 0x01 */ u8  pad01[3];
    /* 0x04 */ s16 width, height;
    /* 0x08 */ s8  maskS, maskT;      /* used when the texture repeats */
    /* 0x0A */ s8  wrapS, wrapT;      /* non-zero: repeat, 0: clamp */
    /* 0x0C */ u32 srcOffset;         /* packed (mpDataDecompress) palette + texels in the file */
    /* 0x10 */ u32 dstOffset;         /* where they are unpacked in the BkgBank */
} BkgFileTexture;

/* a render mode the command stream can select (12 bytes) */
typedef struct BkgFileMode {
    u32 renderMode1, renderMode2;     /* G_SETOTHERMODE_L words, ORed together */
    u32 combine;                      /* 0..5: one of the six colour combiner settings */
} BkgFileMode;

/* a model: a grid of cells, each with a number of display lists ("subs") */
typedef struct BkgFileModel {
    /* 0x00 */ u8  v0;
    /* 0x01 */ u8  pad01[3];
    /* 0x04 */ u32 v4[4];             /* copied to the BkgModel as is */
    /* 0x14 */ BkgFileMode modes[5];  /* mode 0 is the default, restored at the end of a list */
    /* 0x50 */ s32 gridX, gridY, gridZ;   /* cells (x fastest) */
    /* 0x5C */ s32 subsPerCell;
    /* 0x60 */ u32 cellsOffset;       /* BkgFileCell[grid] (each 0x14 + subsPerCell * 8 bytes) */
    /* 0x64 */ u32 outCellsOffset;    /* -> BkgCell[grid] in the BkgBank */
    /* 0x68 */ u32 outDataOffset;     /* where the cells' display lists and vertices go */
    /* 0x6C */ s32 numMaterials;
    /* 0x70 */ u32 materialTexOffset; /* s16 texture index per material */
    /* 0x74 */ u32 outMaterialsOffset;/* -> BkgMaterial[numMaterials] */
    /* 0x78 */ u32 outMaterialDLOffset; /* the materials' display lists, 0x80 bytes each */
    /* 0x7C */ s32 numPoints;
    /* 0x80 */ u32 pointsOffset;      /* u16 position index per point */
    /* 0x84 */ u32 outPointsOffset;   /* -> f32[3] per point */
    /* 0x88 */ s32 hasVertexBytes;    /* non-zero: one extra byte per vertex follows the vertices */
} BkgFileModel;

typedef struct BkgFileCell {
    /* 0x00 */ u16 bounds[9];         /* reordered into BkgCell.bounds */
    /* 0x12 */ u8  pad12;
    /* 0x13 */ u8  flag;
    /* 0x14 */ struct BkgFileSub {
        s16 numCommands;              /* display list length, checked after unpacking */
        s16 numVertices;
        u16 commandsOffset;           /* bit stream (from the BkgFileModel) */
        u16 verticesOffset;           /* bit stream: 12-bit position, 11-bit texcoord, 11-bit colour */
    } subs[1];                        /* [subsPerCell] */
} BkgFileCell;

/* ======================================================================================
 * The BkgBank (D_800AB4A0) built by PrepareBkgBank
 * ====================================================================================== */

typedef struct BkgBankEntry {
    u32 offset;                       /* from the start of the BkgBank */
    u32 v4;                           /* 0 */
} BkgBankEntry;

typedef struct BkgBank {
    /* 0x00 */ u8  type, v1, v2;      /* 1, 0, 0 */
    /* 0x04 */ s32 numObjects;
    /* 0x08 */ u32 size;
    /* 0x0C */ BkgBankEntry objects[1];   /* [numObjects] */
} BkgBank;

/* the first word of a model object: bytes 0, 3 */
#define BKG_OBJECT_TYPE(p)  (((u32)((const u8 *)(p))[0] << 24) | ((u32)((const u8 *)(p))[1] << 16))   /* (bytes: any byte order) */
#define BKG_TYPE_MODEL      0x00030000

/* a model (header written by PrepareBkgBank; offsets from the model itself) */
typedef struct BkgModel {
    /* 0x00 */ u8  v0;                /* 0 */
    /* 0x01 */ u8  type;              /* 3 */
    /* 0x02 */ u8  v2;
    /* 0x03 */ u8  pad03;
    /* 0x04 */ u32 v4[4];
    /* 0x14 */ u8  gridX, gridY, gridZ;
    /* 0x17 */ u8  pad17;
    /* 0x18 */ u16 cellsOffset;       /* BkgCell[gridX * gridY * gridZ] */
    /* 0x1A */ u16 subsPerCell;
    /* 0x1C */ u16 numMaterials;
    /* 0x1E */ u16 pad1E;
    /* 0x20 */ u16 pointsOffset;      /* f32[3] per point */
    /* 0x22 */ u16 numPoints;
    /* 0x24 */ u16 dataOffset;
    /* 0x26 */ u16 pad26;
    /* 0x28 */ u16 numMaterials2;
    /* 0x2A */ u16 materialsOffset;   /* BkgMaterial[] */
    /* 0x2C */ u32 v2C, v30;          /* 0 */
} BkgModel;

typedef struct BkgMaterial {
    u32 v0;                           /* 0 */
    u32 dl;                           /* physical address of its display list (texture set-up) */
} BkgMaterial;

typedef struct BkgSub {               /* one display list of a cell (20 bytes) */
    /* 0x00 */ s32 dlOffset;          /* from the model, -1 = empty */
    /* 0x04 */ s32 numVertices;
    /* 0x08 */ s32 vtxOffset;
    /* 0x0C */ s32 bytesOffset;       /* the extra byte per vertex, 0 = none */
    /* 0x10 */ s32 v10;
} BkgSub;

typedef struct BkgCell {              /* 0x18 + subsPerCell * 20 bytes */
    /* 0x00 */ u16 corner[4];         /* point indices of the cell's corners (0, 3, 2, 1 of the file) */
    /* 0x08 */ u16 bounds[5];
    /* 0x12 */ u16 hidden;            /* set by func_8000C2B8 while it draws the model */
    /* 0x14 */ s8  flag;              /* 0: never hidden */
    /* 0x15 */ u8  pad15[3];
    /* 0x18 */ BkgSub subs[1];
} BkgCell;

/* ======================================================================================
 * What the arena scripts get every update / drawing pass (func_8000CEDC, 0x6C bytes)
 * ====================================================================================== */
typedef struct ArenaContext {
    /* 0x00 */ s32 inFight;           /* 1: there is a combat handler */
    /* 0x04 */ s32 idle;              /* handler->v54 == 0 (1 outside a fight) */
    /* 0x08 */ s32 lateTicks;         /* D_800AB4B0: ticks with handler->v54 >= 4 */
    /* 0x0C */ s32 state;             /* handler->v54 */
    /* 0x10 */ s32 part;              /* handler->arenaIndex */
    /* 0x14 */ f32 fighter1[3];       /* x, -screenY, z (v18[0], -screenY, v18[2]) */
    /* 0x20 */ f32 fighter2[3];
    /* 0x2C */ f32 camPos[3];         /* the camera matrix's translation */
    /* 0x38 */ f32 dirX, dirZ;        /* handler->camDirX, camDirZ */
    /* 0x40 */ f32 camUp[3];          /* camera matrix row 1 */
    /* 0x4C */ f32 camRight[3];       /* camera matrix row 0 */
    /* 0x58 */ void *zone;            /* the zone a fighter is being knocked through, or NULL */
    /* 0x5C */ s32 zoneFromState10;   /* D_800AB4B8 */
    /* 0x60 */ f32 zoneTime;          /* D_800AB4BC: time since the knock-out started */
    /* 0x64 */ s32 zoneFlag;          /* D_800AB4C0: handler->v148 was set */
    /* 0x68 */ f32 zoneFlagTime;      /* D_800AB4C4 */
} ArenaContext;

/* the arena script functions (D_800AB3CC: update, D_800AB434: draw) */
typedef void (*ArenaUpdateFn)(s32 tick, ArenaContext *ctx, BkgBank *bank);
typedef void (*ArenaDrawFn)(void *cam, s32 pass, ArenaContext *ctx, BkgBank *bank);

#define NUM_ARENAS 26

/* ---- globals ---- */
extern BkgBank *D_800AB4A0;              /* the current arena's BkgBank */
extern u8 *D_800AB49C;                   /* the current arena's definition file */
extern u32 D_800AB150[NUM_ARENAS];       /* BKG bank ROM start per arena */
extern u32 D_800AB1B8[NUM_ARENAS];       /* ... and end */
extern u32 D_ArenaGeometryFiles[NUM_ARENAS];  /* arena definition file ROM start */
extern u32 D_ArenaTextureFiles[NUM_ARENAS];   /* ... and end (= BKG bank start) */

/* ---- functions ---- */
BkgBank *PrepareBkgBank(u32 romAddr);
void  func_8000A590(void);
BkgBank *func_8000A5D4(s32 arena);
u8   *func_8000A634(s32 arena);
const char *func_8000A6E4(s32 arena);
void  func_8000A71C(s32 arena, s32 part, s32 zone, s32 *toArena, s32 *toPart, s32 *toZone);
void  func_8000A7A4(void);
void  func_8000A984(void);
void  func_8000AF40(s32 arena);
void  func_8000B704(void);
void  func_8000C2B8(BkgModel *m, ArenaContext *ctx);
void  func_8000CEDC(ArenaContext *ctx, struct CombatHandler *h, f32 *cam, s32 advance);

#endif
