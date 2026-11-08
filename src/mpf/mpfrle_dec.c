/*
 * RLE decoders (original object files 0x800922E0-0x800926F0 and the
 * one-function file 0x800926F0-0x80092700), part of MPFCOMP.
 * See mpfrle_enc.c for the packed format.
 *
 * `rawSize` (the unpacked size from the header) is passed by mpDataDecompress
 * but not used: the decoders stop at the 0xFF end marker. Each returns the
 * number of bytes written so the caller can check it against the header.
 */
#include "mpfcomp.h"

/* 0x800922E0 - 4-bit values in a bit stream (used by the 16-colour textures) */
s32 mpDecodeRLE4(u8 *dst, const u8 *src, s32 rawSize) {
    s32 inPos = 0, outPos = 0;
    u32 c, value;
    s32 count;

    (void)rawSize;
    for (;;) {
        c = mpReadBits(src, inPos, 8);
        inPos += 8;
        if (c & 0x80) {
            if ((c & 0xFF) == 0xFF) break;
            count = c & 0x7F;
            value = mpReadBits(src, inPos, 4);
            inPos += 4;
            do {
                mpWriteBits(dst, outPos, 4, value & 0xFF);
                outPos += 4;
            } while (count-- != 0);
        } else {
            count = c & 0xFF;
            do {
                mpWriteBits(dst, outPos, 4, mpReadBits(src, inPos, 4));
                inPos += 4;
                outPos += 4;
            } while (count-- != 0);
        }
    }
    /* bytes written, rounded up */
    return (outPos & 7) ? ((outPos & ~7) + 8) / 8 : outPos / 8;
}

/* Shared implementation for the byte-unit decoders. */
static s32 mpDecodeRLEBytes(u8 *dst, const u8 *src, s32 unit) {
    u8 *out = dst;
    u8 c;
    s32 count, k;

    for (;;) {
        c = *src++;
        if (c & 0x80) {
            if (c == 0xFF) break;
            count = c & 0x7F;
            do {
                for (k = 0; k < unit; k++) *out++ = src[k];
            } while (count-- != 0);
            src += unit;
        } else {
            count = c;
            do {
                for (k = 0; k < unit; k++) *out++ = *src++;
            } while (count-- != 0);
        }
    }
    return (s32)(out - dst);
}

/* 0x80092434 */
s32 mpDecodeRLE8(u8 *dst, const u8 *src, s32 rawSize) {
    (void)rawSize;
    return mpDecodeRLEBytes(dst, src, 1);
}

/* 0x800924B0 */
s32 mpDecodeRLE16(u8 *dst, const u8 *src, s32 rawSize) {
    (void)rawSize;
    return mpDecodeRLEBytes(dst, src, 2);
}

/* 0x8009254C */
s32 mpDecodeRLE24(u8 *dst, const u8 *src, s32 rawSize) {
    (void)rawSize;
    return mpDecodeRLEBytes(dst, src, 3);
}

/* 0x80092608 */
s32 mpDecodeRLE32(u8 *dst, const u8 *src, s32 rawSize) {
    (void)rawSize;
    return mpDecodeRLEBytes(dst, src, 4);
}

/*
 * 0x800926F0 - decoder for type 0x20 is an empty stub in the shipped game: it
 * always returns 0, so mpDataDecompress reports an error for such data. (The
 * matching encoder, mpEncodeType20, does exist.)
 */
s32 mpDecodeType20(u8 *dst, const u8 *src, s32 rawSize) {
    (void)dst; (void)src; (void)rawSize;
    return 0;
}
