/*
 * CHARFRAM.C (0x80005830-0x80006090): decodes character sprite frames on demand and
 * keeps the last 32 in a cache. A cached frame is freed after 3 calls to
 * CharFrameCache_Update without being asked for again.
 */
#include "char.h"
#include "mpfmem.h"

void RomCopy(void *dst, u32 romAddr, u32 size);

/* BSS (0x800DB870-0x800DBAF0) */
extern CharFrame *sFrameCachePtr[FRAME_CACHE_SIZE];
extern s32 sFrameCacheTimer[FRAME_CACHE_SIZE];
extern CharFileHeader *sFrameCacheData[FRAME_CACHE_SIZE];
extern u32 sFrameCacheRom[FRAME_CACHE_SIZE];
extern s32 sFrameCacheFrame[FRAME_CACHE_SIZE];

/* Reads n bits (n <= 8) LSB-first at bit 'pos' of *p; the caller advances. */
#define PEEK_BITS(p, pos, n) \
    ((pos) < 9 - (n) ? (u32)((p)[0] >> (pos)) : (u32)(((p)[0] >> (pos)) | ((p)[1] << (8 - (pos)))))
#define SKIP_BITS(p, pos, n) do { (pos) += (n); if ((pos) >= 8) { (pos) -= 8; (p)++; } } while (0)

/* 0x80005830 */
void CharFrameCache_Init(void) {
    if (!gCharFrameCacheInitialized) {
        bzero(sFrameCachePtr, sizeof(sFrameCachePtr));
        gCharFrameCacheInitialized = 1;
    }
}

/* 0x80005870 */
CharFrame *LoadCharacterFrame(CharObj *obj, s32 frame) {
    s32 slot = -1;
    s32 i;
    CharFrameEntry *entry;
    u32 handle;
    u8 *raw;
    CharFrame *out;
    u32 *dst;
    u32 *page = NULL;
    const u8 *s;
    s32 pos, remaining, row, pageSet, count, total;
    u32 v, mode = 0, shift = 0, base = 0;

    for (i = 0; i < FRAME_CACHE_SIZE; i++) {
        if (sFrameCachePtr[i] != NULL && sFrameCacheFrame[i] == frame &&
            sFrameCacheData[i] == obj->buf && sFrameCacheRom[i] == obj->romAddr) {
            sFrameCacheTimer[i] = 3;
            return sFrameCachePtr[i];
        }
        if (sFrameCachePtr[i] == NULL) {
            slot = i;
        }
    }
    if (slot < 0) {
        mpFatalError("CHARFRAM.C - Out of frame slots!!!");
    }

    entry = &obj->frames[frame];
    handle = mpLinkedMemoryManager_AllocHndl(&gN64MemoryManager, entry->size, 0);
    raw = mpLinkedMemoryManager_LockHndl(&gN64MemoryManager, handle);
    RomCopy(raw, obj->romAddr + entry->romOffset, entry->size);

    out = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, raw[1] * (raw[0] * 4) + 0x10, 0);
    out->type = 0x43;
    out->pad = 0;
    out->headerSize = 0x10;
    out->unk4 = 0;
    out->width = raw[0];
    out->height = raw[1];
    out->xOffset = raw[2];
    out->yOffset = raw[3];
    out->stride = raw[0] * 4;

    /* colour pass: runs of transparent or indexed pixels, row by row */
    s = raw + 4;
    pos = 0;
    dst = (u32 *)((u8 *)out + out->headerSize);
    row = -1;
    remaining = 0;
    pageSet = 0;
    for (;;) {
        while (remaining <= 0) {
            if (++row >= out->height) {
                goto alpha;
            }
            remaining += raw[0];
            pageSet = 0;
        }
        v = PEEK_BITS(s, pos, 8) & 0xFF;
        s++;
        count = (s32)(v >> 1) + 1;
        if (v & 1) {
            remaining -= count;
            while (count-- > 0) {
                *dst++ = 0;
            }
            continue;
        }
        if (!pageSet) {
            /* first opaque run of the row: 64-colour page, then the index mode */
            page = obj->colours + ((PEEK_BITS(s, pos, 8) & 0xFF) << 6);
            s++;
            mode = PEEK_BITS(s, pos, 3) & 7;
            SKIP_BITS(s, pos, 3);
            pageSet = 1;
            if (mode >= 5) {
                if ((s[0] >> pos) & 1) {
                    mode += 3;
                }
                SKIP_BITS(s, pos, 1);
            }
        }
        remaining -= count;
        if (mode == 0) {
            while (count-- > 0) {
                *dst++ = page[PEEK_BITS(s, pos, 6) & 0x3F];
                SKIP_BITS(s, pos, 6);
            }
        } else if (mode < 5) {
            /* 5-bit indexes */
            switch (mode) {
                case 1: shift = 1; base = 0; break;
                case 2: shift = 1; base = 1; break;
                case 3: shift = 0; base = 0; break;
                case 4: shift = 0; base = 0x20; break;
            }
            while (count-- > 0) {
                *dst++ = page[base + ((PEEK_BITS(s, pos, 5) & 0x1F) << shift)];
                SKIP_BITS(s, pos, 5);
            }
        } else {
            /* 4-bit indexes */
            switch (mode) {
                case 5: shift = 2; base = 0; break;
                case 6: shift = 2; base = 1; break;
                case 7: shift = 2; base = 2; break;
                case 8: shift = 2; base = 3; break;
                case 9: shift = 1; base = 0; break;
                case 10: shift = 1; base = 0x20; break;
            }
            while (count-- > 0) {
                *dst++ = page[base + ((PEEK_BITS(s, pos, 4) & 0xF) << shift)];
                SKIP_BITS(s, pos, 4);
            }
        }
    }

alpha:
    /* alpha pass: 4-bit codes; codes with bit 3 set build up an octal skip count,
       the others set the alpha of the next pixel to 25..200. Rows are padded to an
       even width. */
    total = out->height * ((out->width & 1) ? (s16)(out->width & 0xFFFE) + 2 : out->width);
    s = (u8 *)obj->buf + obj->frames[frame].alphaOffset;
    dst = (u32 *)((u8 *)out + out->headerSize);
    pos = 0;
    i = -1;
    for (;;) {
        u32 skip = 0;
        for (;;) {
            v = PEEK_BITS(s, pos, 4) & 0xF;
            SKIP_BITS(s, pos, 4);
            if (!(v & 8)) {
                break;
            }
            skip = (skip << 3) | (v & 7);
        }
        i = i + 1 + skip;
        if (i >= total) {
            break;
        }
        dst[i] = (dst[i] & ~0xFFu) | (u32)((v + 1) * 25);     /* (the alpha: the low byte) */
    }

    mpLinkedMemoryManager_FreeHndl(&gN64MemoryManager, handle);
    sFrameCachePtr[slot] = out;
    sFrameCacheTimer[slot] = 3;
    sFrameCacheData[slot] = obj->buf;
    sFrameCacheRom[slot] = obj->romAddr;
    sFrameCacheFrame[slot] = frame;
    return sFrameCachePtr[slot];
}

/* 0x80005F90 */
void CharFrameCache_Update(void) {
    s32 i;

    for (i = 0; i < FRAME_CACHE_SIZE; i++) {
        if (sFrameCachePtr[i] != NULL && --sFrameCacheTimer[i] < 0) {
            mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sFrameCachePtr[i]);
            sFrameCachePtr[i] = NULL;
        }
    }
}

/* 0x80006020 */
void CharFrameCache_FreeAll(void) {
    s32 i;

    for (i = 0; i < FRAME_CACHE_SIZE; i++) {
        if (sFrameCachePtr[i] != NULL) {
            mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sFrameCachePtr[i]);
            sFrameCachePtr[i] = NULL;
        }
    }
}
