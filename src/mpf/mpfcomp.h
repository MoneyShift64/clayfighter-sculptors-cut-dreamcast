/*
 * Interplay MPF engine - data compression (MPFCOMP.C) and bit streams (MPFBSTRM.C).
 */
#ifndef MPFCOMP_H
#define MPFCOMP_H

#include "common.h"

/* Compression types (first byte of MPF_CompressedDataHeader) */
#define MPF_COMP_NONE        0x00
#define MPF_COMP_RLE_8_BIT   0x10
#define MPF_COMP_RLE_16_BIT  0x11
#define MPF_COMP_RLE_24_BIT  0x12
#define MPF_COMP_RLE_32_BIT  0x13
#define MPF_COMP_RLE_4_BIT   0x14
#define MPF_COMP_TYPE_20     0x20   /* LZSS; encoder only, the decoder is an empty stub */
#define MPF_COMP_TYPE_30     0x30   /* Burrows-Wheeler block sort                 */
#define MPF_COMP_TYPE_40     0x40   /* move-to-front                              */
#define MPF_COMP_TYPE_50     0x50   /* adaptive arithmetic coding                 */
#define MPF_COMP_TYPE_60     0x60   /* LZW, 9-15 bit codes                        */

/*
 * MPF_CompressedDataHeader - 12 bytes, big-endian in the ROM:
 *   +0 u8  type
 *   +1 u8  (unused, 0)
 *   +2 u16 header size (offset of the packed data, always 12)
 *   +4 u32 total size (header + packed data)
 *   +8 u32 unpacked size
 * Use the MPF_HDR_* accessors so the code works on any byte order.
 */
#define MPF_HEADER_SIZE 12
#define MPF_HDR_TYPE(h)        (((const u8 *)(h))[0])
#define MPF_HDR_HEADERSIZE(h)  READ_BE16((const u8 *)(h) + 2)
#define MPF_HDR_TOTALSIZE(h)   READ_BE32((const u8 *)(h) + 4)
#define MPF_HDR_RAWSIZE(h)     READ_BE32((const u8 *)(h) + 8)

/* MPFBSTRM.C */
u32  mpReadBits(const u8 *buf, s32 bitPos, s32 numBits);
void mpWriteBits(u8 *buf, s32 bitPos, s32 numBits, u32 value);

/* MPFCOMP.C */
s32  mpDataCompress(void *dst, const void *src, s32 size, u16 type);
s32  mpDataDecompress(void *dst, const void *src);
u32  mpCalculateWorstCase(u32 size, u16 type);

/* RLE encoders / decoders (each returns the number of bytes written) */
s32  mpEncodeRLE4(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeRLE8(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeRLE16(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeRLE24(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeRLE32(u8 *dst, const u8 *src, s32 size);
s32  mpDecodeRLE4(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeRLE8(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeRLE16(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeRLE24(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeRLE32(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeType20(u8 *dst, const u8 *src, s32 rawSize);

/* MPFCOMP.C second half, in mpfcomp2.c (not used by any data in the ROM) */
s32  mpEncodeType20(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeType30(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeType40(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeType50(u8 *dst, const u8 *src, s32 size);
s32  mpEncodeType60(u8 *dst, const u8 *src, s32 size);
s32  mpDecodeType30(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeType40(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeType50(u8 *dst, const u8 *src, s32 rawSize);
s32  mpDecodeType60(u8 *dst, const u8 *src, s32 rawSize);

#endif /* MPFCOMP_H */
