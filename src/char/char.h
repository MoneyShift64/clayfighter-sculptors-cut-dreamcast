/*
 * Character loading, sprite frames and costume colours
 * (original objects 0x80003CC0-0x80005830, CHARFRAM.C 0x80005830-0x80006090 and
 *  the colour file 0x80006090-0x80006A00).
 */
#ifndef CHAR_H
#define CHAR_H

#include "common.h"

#define NUM_CHARACTERS   17
#define NUM_COSTUMES      4
#define FRAME_CACHE_SIZE 32

/* Hue (degrees, 0-360), saturation and lightness (0-1). */
typedef struct HLS {
    f32 h, s, l;
} HLS;

typedef void (*CostumeFunc)(HLS *hls);

/* Character file header: 16 big-endian words at the start of every file. */
typedef struct CharFileHeader {
    /* 0x00 */ u16 magic;          /* 0x0446 */
    /* 0x02 */ s16 numFrames;
    /* 0x04 */ u32 movesOffset;    /* offsets are into the unpacked buffer */
    /* 0x08 */ u32 frameTableOffset;
    /* 0x0C */ u32 colourPages;    /* pages of 64 colours */
    /* 0x10 */ u32 coloursOffset;  /* 16-bit (RGBA5551) colours */
    /* 0x14 */ u32 unk14;
    /* 0x18 */ u32 bankOffset;     /* sound bank (0 = none) */
    /* 0x1C */ u32 bankRomOffset;  /* its sample data, from the start of the file */
    /* 0x20 */ u32 packedOffset;   /* MPF-compressed block, from the start of the file */
    /* 0x24 */ u32 packedSize;
    /* 0x28 */ s32 packPasses;
    /* 0x2C */ u32 fixedPages;     /* last pages, never recoloured by costumes */
    /* 0x30 */ u32 unk30[4];
} CharFileHeader;

typedef struct CharFrameEntry {
    u32 romOffset;   /* frame stream, from the start of the file (read from ROM) */
    u32 size;
    u32 alphaOffset; /* alpha stream, in the unpacked buffer (rebased at load) */
} CharFrameEntry;

/* A loaded character (0x40 bytes). */
typedef struct CharObj {
    /* 0x00 */ s32 slot;
    /* 0x04 */ s32 costume;
    /* 0x08 */ CharFileHeader *buf;     /* header + unpacked data */
    /* 0x0C */ u8 *moves;
    /* 0x10 */ u32 romAddr;
    /* 0x14 */ CharFrameEntry *frames;
    /* 0x18 */ u32 *colours;            /* RGBA32 dictionary */
    /* 0x1C */ void *bank;
    /* 0x20 */ u8 *lightPalettes[4];    /* 0x208 bytes each: 8-byte header + 256 RGBA5551 */
    /* 0x30 */ s32 lightValues[4];
} CharObj;

/* A decoded frame: this header, then w*h RGBA32 pixels. */
typedef struct CharFrame {
    /* 0x00 */ u8  type;      /* 0x43 */
    /* 0x01 */ u8  pad;
    /* 0x02 */ u16 headerSize; /* 0x10 */
    /* 0x04 */ u16 unk4;
    /* 0x06 */ s16 width;
    /* 0x08 */ s16 height;
    /* 0x0A */ s16 xOffset;
    /* 0x0C */ s16 yOffset;
    /* 0x0E */ s16 stride;    /* bytes per row */
} CharFrame;

/* Per-character base lighting colour, one per palette (20 bytes). */
typedef struct CharLight {
    f32 h, s, l;
    f32 lightAdd;
    s32 value;
} CharLight;

/* ---- data still in assembly ---- */
extern u32 D_CharacterRomTable[NUM_CHARACTERS];
extern u32 D_CharacterRomEndTable[NUM_CHARACTERS];
extern const char *D_CharacterNames[NUM_CHARACTERS];
extern CostumeFunc D_CharacterColourVariants[NUM_CHARACTERS][NUM_COSTUMES];
extern u8 gCharacterAvailable[NUM_CHARACTERS];
extern s32 gCharCommonDataLoaded;
extern s32 gCharFrameCacheInitialized;
extern u8 D_800AA880[256];             /* lighting saturation ramp */
extern u8 D_800AA980[256];             /* lighting lightness ramp */
extern CharLight D_800AAA80[NUM_CHARACTERS * 4];
extern u32 D_800AAFF0[NUM_CHARACTERS]; /* portrait asset ids */
extern u16 D_800AB034[NUM_CHARACTERS][2];

/* ---- CHARLOAD (0x80003CC0) ---- */
void CharMenuSound_Init(void);
void CharMenuSound_Load(s32 index);
void CharMenuSound_Free(void);
void LoadCharacterCommonDataOnce(void);
CharObj *LoadCharacter(s32 slot, s32 costume);
void FreeCharacter(CharObj *obj);
void ApplyCostumeColours(u16 *colours, s32 count, s32 slot, s32 costume);
const char *GetCharacterName(s32 slot);
void LoadCharacterCommonData(void);

/* ---- CHARFRAM.C ---- */
void CharFrameCache_Init(void);
CharFrame *LoadCharacterFrame(CharObj *obj, s32 frame);
void CharFrameCache_Update(void);
void CharFrameCache_FreeAll(void);

/* ---- colours ---- */
void RecolourPalette(u16 *colours, s32 count, CostumeFunc func);
f32  HueWeight(f32 hue, f32 centre, f32 width);
void RGBtoHLS(f32 *rgb, HLS *out);
f32  HLSValue(f32 n1, f32 n2, f32 hue);
void HLStoRGB(HLS *in, f32 *rgb);
void *LoadCharacterPortrait(s32 slot, s32 costume);

/* float -> unsigned int as the game's compiler does it */
#define F64_TO_U32(d) ((d) >= 2147483648.0 ? ((u32)(s32)((d) - 2147483648.0) | 0x80000000u) : (u32)(s32)(d))

#endif
