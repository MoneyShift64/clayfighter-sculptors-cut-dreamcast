/*
 * RLE encoders (original object file 0x80091C30-0x800922E0, part of MPFCOMP).
 *
 * Packed format (all widths): a control byte c, then
 *   c & 0x80, c != 0xFF : one unit, repeated (c & 0x7F) + 1 times
 *   c < 0x80            : c + 1 units copied as they are
 *   c == 0xFF           : end of data
 * A unit is 4 bits (RLE4, packed into a bit stream), or 1, 2, 3 or 4 bytes.
 *
 * Original quirks kept: a repeat run is only started after a minimum number of
 * matching units (RLE4: 7, RLE8: 4, RLE16/24: 3, RLE32: 2), runs stop at 127
 * units, and literal blocks at 128 units.
 * The 16/24/32-bit encoders in the original store units with unaligned
 * halfword/word writes (which would fault on the N64 and show these were never
 * used on the console); here units are copied byte by byte, producing the same
 * output.
 */
#include "mpfcomp.h"

#define RLE_MAX_RUN 0x7E   /* count field limit for repeat runs (+1 = 127 units) */
#define RLE_MAX_LIT 0x7F   /* count field limit for literal blocks (+1 = 128)    */

static inline s32 memcmp_units(const u8 *a, const u8 *b, s32 unit) {
    s32 k;
    for (k = 0; k < unit; k++) {
        if (a[k] != b[k]) return 1;
    }
    return 0;
}

/*
 * Shared implementation for the byte-unit encoders.
 * `unit` = bytes per unit, `minExtra` = matching units needed after the
 * current one before a repeat run is started.
 */
static s32 mpEncodeRLEBytes(u8 *dst, const u8 *src, s32 size, s32 unit, s32 minExtra) {
    u8 *out = dst;
    u8 *lit = NULL;         /* control byte of the current literal block */
    s32 n, k;

    while (size > 0) {
        const u8 *cur = src;
        s32 isRun = 0;

        src += unit;
        size -= unit;

        if (size >= minExtra * unit) {
            isRun = 1;
            for (k = 0; k < minExtra; k++) {
                if (memcmp_units(src + k * unit, cur, unit) != 0) { isRun = 0; break; }
            }
        }

        if (isRun) {
            lit = NULL;
            size -= minExtra * unit;
            n = minExtra;
            if (size > 0) {
                for (;;) {
                    if (memcmp_units(src + n * unit, cur, unit) != 0) break;
                    n++;
                    size -= unit;
                    if (n >= RLE_MAX_RUN) break;
                    if (size <= 0) break;
                }
            }
            src += n * unit;
            *out++ = (u8)(n | 0x80);
            for (k = 0; k < unit; k++) *out++ = cur[k];
        } else {
            if (lit != NULL && *lit >= RLE_MAX_LIT) {
                lit = NULL;
            }
            if (lit == NULL) {
                lit = out++;
                *lit = 0xFF;            /* becomes 0 (= 1 unit) below */
            }
            for (k = 0; k < unit; k++) *out++ = cur[k];
            (*lit)++;
        }
    }
    *out++ = 0xFF;
    return (s32)(out - dst);
}

/* 0x80091ECC */
s32 mpEncodeRLE8(u8 *dst, const u8 *src, s32 size) {
    return mpEncodeRLEBytes(dst, src, size, 1, 3);
}

/* 0x80091FCC */
s32 mpEncodeRLE16(u8 *dst, const u8 *src, s32 size) {
    return mpEncodeRLEBytes(dst, src, size, 2, 2);
}

/* 0x800920C8 */
s32 mpEncodeRLE24(u8 *dst, const u8 *src, s32 size) {
    return mpEncodeRLEBytes(dst, src, size, 3, 2);
}

/* 0x800921EC */
s32 mpEncodeRLE32(u8 *dst, const u8 *src, s32 size) {
    return mpEncodeRLEBytes(dst, src, size, 4, 1);
}

/*
 * 0x80091C30
 * Same scheme on 4-bit values (`size` is in bytes; each byte is two values,
 * low nibble first). Control bytes and values are written to a bit stream.
 */
s32 mpEncodeRLE4(u8 *dst, const u8 *src, s32 size) {
    s32 remaining = size * 2;   /* nibbles left */
    s32 inPos = 0;              /* bit position in src */
    s32 outPos = 0;             /* bit position in dst */
    s32 litPos = -1;            /* bit position of the literal control byte */
    s32 n, k;
    u32 value;

    while (remaining > 0) {
        s32 isRun = 0;

        value = mpReadBits(src, inPos, 4);
        remaining--;
        inPos += 4;

        if (remaining >= 6) {
            isRun = 1;
            for (k = 0; k < 6; k++) {
                if (mpReadBits(src, inPos + k * 4, 4) != (value & 0xFF)) { isRun = 0; break; }
            }
        }

        if (isRun) {
            litPos = -1;
            remaining -= 6;
            n = 6;
            if (remaining > 0) {
                for (;;) {
                    if (mpReadBits(src, inPos + n * 4, 4) != (value & 0xFF)) break;
                    n++;
                    remaining--;
                    if (n >= RLE_MAX_RUN) break;
                    if (remaining <= 0) break;
                }
            }
            inPos += n * 4;
            mpWriteBits(dst, outPos, 8, (n | 0x80) & 0xFF);
            outPos += 8;
            mpWriteBits(dst, outPos, 4, value & 0xFF);
            outPos += 4;
        } else {
            if (litPos != -1 && (s32)mpReadBits(dst, litPos, 8) >= RLE_MAX_LIT) {
                litPos = -1;
            }
            if (litPos == -1) {
                litPos = outPos;
                mpWriteBits(dst, outPos, 8, 0xFF);
                outPos += 8;
            }
            mpWriteBits(dst, outPos, 4, value & 0xFF);
            outPos += 4;
            mpWriteBits(dst, litPos, 8, (mpReadBits(dst, litPos, 8) + 1) & 0xFF);
        }
    }
    mpWriteBits(dst, outPos, 8, 0xFF);
    outPos += 8;
    /* bytes used, rounded up */
    return (outPos & 7) ? ((outPos & ~7) + 8) / 8 : outPos / 8;
}
