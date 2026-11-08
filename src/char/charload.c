/*
 * Character loading (original object 0x80003CC0-0x80005830, minus the costume
 * functions, which are in costume.c).
 *
 * A character file is a 0x40-byte header followed by an MPF-compressed block. The
 * game unpacks it into one buffer (header + data), recolours it for the costume,
 * builds an RGBA32 colour dictionary for the frame decoder and four lighting
 * palettes, and returns a CharObj describing it all.
 */
#include "char.h"
#include "mpfmem.h"
#include "mpfcomp.h"

void RomCopy(void *dst, u32 romAddr, u32 size);
void alBnkfNew(void *bank, u32 romSamples);

/* still in assembly */
void  func_8007CCA4(void);                       /* takes a global lock */
void *func_80078E44(u32 romAddr, u32 size, u32 romEnd);
void  func_800794CC(void *sound, s32 arg);
void  func_800795B4(void *data);
void  func_800796C0(void);

extern void *B_800F4E74;              /* menu sound player */
extern void *D_800AA658;              /* current menu sound data */
extern u32 D_MenuFiles[];             /* ROM addresses: where each file starts ... */
extern u32 D_800AA5AC[];              /* ... and ends (a table of its own: the last one has no next) */
extern volatile u32 D_800B09B0[2];    /* audio frame counter pair */

/* 0x80003CC0 */
void CharMenuSound_Init(void) {
    B_800F4E74 = func_80078E44(0xE221A0, 0xE26FF0 - 0xE221A0, 0xE26FF0);
    func_800794CC(B_800F4E74, 0);
}

/* 0x80003D0C */
void CharMenuSound_Load(s32 index) {
    u32 size;

    CharMenuSound_Free();
    size = D_800AA5AC[index] - D_MenuFiles[index];
    if (size & 1) {
        size = (size & ~1) + 2;
    }
    D_800AA658 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
    size = D_800AA5AC[index] - D_MenuFiles[index];
    if (size & 1) {
        size = (size & ~1) + 2;
    }
    RomCopy(D_800AA658, D_MenuFiles[index], size);
    func_800795B4(D_800AA658);
}

/* 0x80003E18 */
void CharMenuSound_Free(void) {
    u32 a, b;

    if (D_800AA658 != NULL) {
        func_800796C0();
        /* wait for the audio thread to move on twice, so it has let go of the data */
        a = D_800B09B0[0];
        b = D_800B09B0[1];
        while (a == D_800B09B0[0] && b == D_800B09B0[1]) {
            PORT_SPIN();
        }
        a = D_800B09B0[0];
        b = D_800B09B0[1];
        while (a == D_800B09B0[0] && b == D_800B09B0[1]) {
            PORT_SPIN();
        }
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, D_800AA658);
        D_800AA658 = NULL;
    }
}

/* 0x80003EC0 */
void LoadCharacterCommonDataOnce(void) {
    if (!gCharCommonDataLoaded) {
        LoadCharacterCommonData();
        gCharCommonDataLoaded = 1;
    }
}

/* 0x80003EF8 */
CharObj *LoadCharacter(s32 slot, s32 costume) {
    MPF_LinkedMemoryManager *mgr = &gN64MemoryManager;
    CharFileHeader hdr;
    CharObj *obj;
    CharFileHeader *buf;
    void *tmp;
    u32 handle, next;
    u8 *packed, *unpacked;
    s32 i, j;
    CostumeFunc *func;
    u16 *colours16;
    u32 *colours;

    func_8007CCA4();
    CharFrameCache_FreeAll();
    mpLinkedMemoryManager_Pack(mgr);
    LoadCharacterCommonDataOnce();

    obj = mpLinkedMemoryManager_AllocPtr(mgr, sizeof(CharObj), 0);
    bzero(obj, sizeof(CharObj));

    tmp = mpLinkedMemoryManager_AllocPtr(mgr, sizeof(CharFileHeader), 0);
    RomCopy(tmp, D_CharacterRomTable[slot], sizeof(CharFileHeader));
    bcopy(tmp, &hdr, sizeof(CharFileHeader));
    mpLinkedMemoryManager_FreePtr(mgr, tmp);
    mpLinkedMemoryManager_Pack(mgr);

    /* read the packed block, then undo all but the last compression pass */
    handle = mpLinkedMemoryManager_AllocHndl(mgr, hdr.packedSize, 0);
    packed = mpLinkedMemoryManager_LockHndl(mgr, handle);
    RomCopy(packed, D_CharacterRomTable[slot] + hdr.packedOffset, hdr.packedSize);
    mpLinkedMemoryManager_UnlockHndl(mgr, handle);
    for (i = 0; i < hdr.packPasses - 1; i++) {
        packed = mpLinkedMemoryManager_LockHndl(mgr, handle);
        next = mpLinkedMemoryManager_AllocHndl(mgr, READ_BE32(packed + 8), 0);   /* raw size */
        unpacked = mpLinkedMemoryManager_LockHndl(mgr, next);
        mpDataDecompress(unpacked, mpLinkedMemoryManager_LockHndl(mgr, handle));
        mpLinkedMemoryManager_UnlockHndl(mgr, next);
        mpLinkedMemoryManager_FreeHndl(mgr, handle);
        handle = next;
        mpLinkedMemoryManager_Pack(mgr);
    }

    /* last pass straight into the character buffer, after a copy of the header */
    packed = mpLinkedMemoryManager_LockHndl(mgr, handle);
    buf = mpLinkedMemoryManager_AllocPtr(mgr, READ_BE32(packed + 8) + 0x40, 0);
    mpDataDecompress((u8 *)buf + 0x40, mpLinkedMemoryManager_LockHndl(mgr, handle));
    mpLinkedMemoryManager_FreeHndl(mgr, handle);
    bcopy(&hdr, buf, sizeof(CharFileHeader));

    /* costume colours (the last 'fixedPages' pages are left alone) */
    func = &D_CharacterColourVariants[slot][costume];
    if (*func != NULL) {
        RecolourPalette((u16 *)((u8 *)buf + buf->coloursOffset),
                        (buf->colourPages - buf->fixedPages) << 6, *func);
    }

    if (buf->bankOffset != 0) {
        alBnkfNew((u8 *)buf + buf->bankOffset, D_CharacterRomTable[slot] + buf->bankRomOffset);
    }

    /* RGBA5551 -> RGBA32 dictionary for the frame decoder */
    colours16 = (u16 *)((u8 *)buf + buf->coloursOffset);
    colours = mpLinkedMemoryManager_AllocPtr(mgr, buf->colourPages << 8, 0);
    for (i = 0; i < (s32)(buf->colourPages << 6); i++) {
        u32 c = colours16[i];
        colours[i] = ((c & 0xF800) << 16) | ((c & 0x07C0) << 13) | ((c & 0x003E) << 10) | 0xFF;
    }

    obj->slot = slot;
    obj->costume = costume;
    obj->buf = buf;
    obj->moves = (u8 *)buf + buf->movesOffset;
    obj->romAddr = D_CharacterRomTable[slot];
    obj->frames = (CharFrameEntry *)((u8 *)buf + buf->frameTableOffset);
    obj->colours = colours;
    obj->bank = buf->bankOffset != 0 ? (u8 *)buf + buf->bankOffset : NULL;

    /* alpha streams are stored relative to the packed block; point them into buf */
    for (i = 0; i < obj->buf->numFrames; i++) {
        obj->frames[i].alphaOffset = obj->frames[i].alphaOffset + 0x40 - obj->buf->packedOffset;
    }

    /* four lighting palettes: ramps of one base colour, adjusted for the costume */
    for (i = 0; i < 4; i++) {
        CharLight *light = &D_800AAA80[slot * 4 + i];
        u8 *pal = mpLinkedMemoryManager_AllocPtr(mgr, 0x208, 0);
        u16 *out;
        HLS base, hls;
        f32 rgb[3];

        obj->lightPalettes[i] = pal;
        pal[0] = 0xFF;
        base.h = light->h;
        base.s = light->s;
        base.l = light->l;
        if (*func != NULL) {
            (*func)(&base);
        }
        out = (u16 *)(pal + 8);
        out[0] = 0;
        for (j = 1; j < 256; j++) {
            f64 t;
            u32 r, g, b;

            hls.h = base.h;
            hls.s = (f32)((f64)(f32)D_800AA880[j] / 255.0 * (f64)base.s);
            hls.l = (f32)((f64)(f32)D_800AA980[j] / 255.0 * (f64)base.l);
            hls.l = hls.l + light->lightAdd;
            if ((f64)hls.s > 1.0) {
                hls.s = 1.0f;
            }
            if ((f64)hls.l > 1.0) {
                hls.l = 1.0f;
            }
            if ((f64)hls.l < 0.0) {
                hls.l = 0.0f;
            }
            HLStoRGB(&hls, rgb);
            t = (f64)rgb[0] * 32.0;
            r = F64_TO_U32(t);
            t = (f64)rgb[1] * 32.0;
            g = F64_TO_U32(t);
            t = (f64)rgb[2] * 32.0;
            b = F64_TO_U32(t);
            out[j] = (r << 11) | (g << 6) | (b << 1) | 1;
        }
        obj->lightValues[i] = light->value;
    }
    return obj;
}

/* 0x80004644 */
void FreeCharacter(CharObj *obj) {
    s32 i;

    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, obj->buf);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, obj->colours);
    for (i = 0; i < 4; i++) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, obj->lightPalettes[i]);
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, obj);
}

/* 0x800046C8 */
void ApplyCostumeColours(u16 *colours, s32 count, s32 slot, s32 costume) {
    CostumeFunc func = D_CharacterColourVariants[slot][costume];

    if (func != NULL) {
        RecolourPalette(colours, count, func);
    }
}

/* 0x80004708 */
const char *GetCharacterName(s32 slot) {
    if (slot < NUM_CHARACTERS) {
        return D_CharacterNames[slot];
    }
    return "<Illegal Character Index>";
}

/*
 * 0x80004734: empty character slots hold a file named "nullchar". They are pointed
 * at the first real character's file and marked unavailable.
 */
void LoadCharacterCommonData(void) {
    s32 real = -1;
    s32 i;
    char *name = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 9, 0);

    mpMemSet(name, mpLinkedMemoryManager_SizeOfPtr(&gN64MemoryManager, name), 0);
    for (i = 0; i < NUM_CHARACTERS; i++) {
        RomCopy(name, D_CharacterRomTable[i], 8);
        if (!mpStrEqualNoCase(name, "nullchar")) {
            real = i;
            break;
        }
    }
    for (i = 0; i < NUM_CHARACTERS; i++) {
        RomCopy(name, D_CharacterRomTable[i], 8);
        if (mpStrEqualNoCase(name, "nullchar")) {
            D_CharacterRomTable[i] = D_CharacterRomTable[real];
            D_CharacterRomEndTable[i] = D_CharacterRomEndTable[real];
            gCharacterAvailable[i] = 0;
        } else {
            gCharacterAvailable[i] = 1;
        }
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, name);
}
