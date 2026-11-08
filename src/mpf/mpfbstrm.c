/*
 * MPFBSTRM.C - bit stream access (0x80092B80-0x80093110)
 *
 * Bits are packed least-significant first: bit 0 of the stream is bit 0 of
 * byte 0. Fields of 1-8 bits may straddle two bytes. The original code has a
 * separate case for each field width (jump tables at 0x800CFB80 / 0x800CFBD8);
 * this version handles all widths with the same arithmetic.
 */
#include "mpfcomp.h"

/* 0x80092B80 */
u32 mpReadBits(const u8 *buf, s32 bitPos, s32 numBits) {
    const u8 *p;
    s32 shift;
    u32 value;

    if (numBits < 1 || numBits > 8) {
        mpFatalErrorf("MPFBSTRM.C : mpReadBits() - Illegal bit size (%d)", numBits);
        return 0;
    }
    p = buf + (bitPos >> 3);
    shift = bitPos & 7;
    value = p[0] >> shift;
    if (shift > 8 - numBits) {
        /* the field continues into the next byte */
        value |= (u32)p[1] << (8 - shift);
    }
    return value & ((1u << numBits) - 1);
}

/* 0x80092DF4 */
void mpWriteBits(u8 *buf, s32 bitPos, s32 numBits, u32 value) {
    u8 *p;
    s32 shift;
    u32 mask;

    if (numBits < 1 || numBits > 8) {
        mpFatalErrorf("MPFBSTRM.C : mpWriteBits() - Illegal bit size (%d)", numBits);
        return;
    }
    p = buf + (bitPos >> 3);
    shift = bitPos & 7;
    mask = (1u << numBits) - 1;
    value &= mask;
    p[0] = (u8)((p[0] & ~(mask << shift)) | (value << shift));
    if (shift > 8 - numBits) {
        p[1] = (u8)((p[1] & ~(mask >> (8 - shift))) | (value >> (8 - shift)));
    }
}
