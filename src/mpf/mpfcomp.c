/*
 * MPFCOMP.C - compression front end (0x8008EA70-0x8008EF40)
 *
 * The codecs for types 0x20-0x60 (0x8008EF40-0x80091C30) are in mpfcomp2.c.
 */
#include "mpfcomp.h"

/* global (not static) so they replace the original functions in the build */
void mpDataCompress_Error(const char *msg);
void mpDataDecompress_Error(const char *msg);

/*
 * 0x8008EA70
 * Packs `size` bytes from `src` into `dst` (header + packed data) using the
 * given compression type. Returns the total size written, header included.
 */
s32 mpDataCompress(void *dst, const void *src, s32 size, u16 type) {
    u8 *hdr = (u8 *)dst;
    u8 *out;
    s32 total;

    hdr[0] = (u8)type;
    hdr[1] = 0;
    WRITE_BE16(hdr + 2, MPF_HEADER_SIZE);
    WRITE_BE32(hdr + 4, 0);
    WRITE_BE32(hdr + 8, (u32)size);
    total = MPF_HDR_HEADERSIZE(hdr);
    out = hdr + total;

    switch (type) {
        case MPF_COMP_NONE:
            total += size;
            bcopy(src, out, size);
            WRITE_BE32(hdr + 4, (u32)total);
            return total;
        case MPF_COMP_RLE_4_BIT:
            total += mpEncodeRLE4(out, src, size);
            break;
        case MPF_COMP_RLE_8_BIT:
            total += mpEncodeRLE8(out, src, size);
            break;
        case MPF_COMP_RLE_16_BIT:
            if (size % 2 != 0) {
                mpDataCompress_Error("Data size must be multiple of 2 for MPF_COMP_RLE_16_BIT");
            }
            total += mpEncodeRLE16(out, src, size);
            break;
        case MPF_COMP_RLE_24_BIT:
            if ((u32)size % 3 != 0) {
                mpDataCompress_Error("Data size must be multiple of 3 for MPF_COMP_RLE_24_BIT");
            }
            total += mpEncodeRLE24(out, src, size);
            break;
        case MPF_COMP_RLE_32_BIT:
            if (size % 4 != 0) {
                mpDataCompress_Error("Data size must be multiple of 4 for MPF_COMP_RLE_32_BIT");
            }
            total += mpEncodeRLE32(out, src, size);
            break;
        case MPF_COMP_TYPE_20:
            total += mpEncodeType20(out, src, size);
            break;
        case MPF_COMP_TYPE_30:
            total += mpEncodeType30(out, src, size);
            break;
        case MPF_COMP_TYPE_40:
            total += mpEncodeType40(out, src, size);
            break;
        case MPF_COMP_TYPE_50:
            total += mpEncodeType50(out, src, size);
            break;
        case MPF_COMP_TYPE_60:
            total += mpEncodeType60(out, src, size);
            break;
        default:
            mpDataCompress_Error("Undefined compression type specified");
            break;
    }
    WRITE_BE32(hdr + 4, (u32)total);
    return total;
}

/*
 * 0x8008EC80
 * Unpacks the MPF block at `src` into `dst`. Returns the unpacked size; a
 * size that differs from the header is a fatal error.
 */
s32 mpDataDecompress(void *dst, const void *src) {
    const u8 *hdr = (const u8 *)src;
    const u8 *data = hdr + MPF_HDR_HEADERSIZE(hdr);
    u32 rawSize = MPF_HDR_RAWSIZE(hdr);
    s32 result = 0;

    switch (MPF_HDR_TYPE(hdr)) {
        case MPF_COMP_NONE:
            result = (s32)rawSize;
            bcopy(data, dst, result);
            break;
        case MPF_COMP_RLE_4_BIT:  result = mpDecodeRLE4(dst, data, rawSize);   break;
        case MPF_COMP_RLE_8_BIT:  result = mpDecodeRLE8(dst, data, rawSize);   break;
        case MPF_COMP_RLE_16_BIT: result = mpDecodeRLE16(dst, data, rawSize);  break;
        case MPF_COMP_RLE_24_BIT: result = mpDecodeRLE24(dst, data, rawSize);  break;
        case MPF_COMP_RLE_32_BIT: result = mpDecodeRLE32(dst, data, rawSize);  break;
        case MPF_COMP_TYPE_20:    result = mpDecodeType20(dst, data, rawSize); break;
        case MPF_COMP_TYPE_30:    result = mpDecodeType30(dst, data, rawSize); break;
        case MPF_COMP_TYPE_40:    result = mpDecodeType40(dst, data, rawSize); break;
        case MPF_COMP_TYPE_50:    result = mpDecodeType50(dst, data, rawSize); break;
        case MPF_COMP_TYPE_60:    result = mpDecodeType60(dst, data, rawSize); break;
        default:
            mpDataDecompress_Error("Undefined compression type");
            break;
    }
    if (rawSize != (u32)result) {
        mpDataDecompress_Error("Error decompressing data");
    }
    return result;
}

/*
 * 0x8008EDE8
 * Largest possible packed size (header included) for `size` bytes of data.
 * The divisions by 127 match the longest repeat/literal block of the RLE types.
 */
u32 mpCalculateWorstCase(u32 size, u16 type) {
    switch (type) {
        case MPF_COMP_NONE:
        case MPF_COMP_TYPE_40:
            return size + 12;
        case MPF_COMP_RLE_8_BIT:
            return size + size / 127 + 14;
        case MPF_COMP_RLE_16_BIT:
            return size + (size >> 1) / 127 + 14;
        case MPF_COMP_RLE_24_BIT:
            return size + size / 381 + 14;
        case MPF_COMP_RLE_32_BIT:
            return size + (size >> 2) / 127 + 14;
        case MPF_COMP_RLE_4_BIT:
            return size + (size << 1) / 127 + 14;
        case MPF_COMP_TYPE_20:
        case MPF_COMP_TYPE_30:
        case MPF_COMP_TYPE_50:
        case MPF_COMP_TYPE_60:
            return size * 2 + 12;
        default:
            mpFatalError("MPFCOMP.C : mpCalculateWorstCase() - Undefined compressed type specified");
            return 0;
    }
}

/* 0x8008EEF8 */
void mpDataCompress_Error(const char *msg) {
    mpFatalErrorf("MPFCOMP.C : mpDataCompress() - %s", msg);
}

/* 0x8008EF1C */
void mpDataDecompress_Error(const char *msg) {
    mpFatalErrorf("MPFCOMP.C : mpDataDecompress() - %s", msg);
}
