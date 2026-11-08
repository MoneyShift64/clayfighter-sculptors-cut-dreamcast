/*
 * MPFCOMP.C, second half - the codecs for compression types 0x20-0x60
 * (original code 0x8008EF40-0x80091C30).
 *
 * None of the game's data uses these types; they are textbook algorithms of
 * the time, adapted by Interplay:
 *   0x20  LZSS (Haruhiko Okumura's LZSS.C, binary search tree). Encoder only,
 *         the decoder in the shipped game is an empty stub (mpfrle_dec.c).
 *   0x30  Burrows-Wheeler block sort (blocks of up to 200000 bytes, sorted
 *         with qsort).
 *   0x40  move-to-front.
 *   0x50  adaptive arithmetic coding (Witten, Neal & Cleary, CACM 1987),
 *         16-bit code values.
 *   0x60  LZW, 9 to 15-bit codes, hashed dictionary (Mark Nelson's LZW.C),
 *         codes packed least-significant bit first.
 *
 * Each encoder takes (dst, src, size), each decoder (dst, src, rawSize), and
 * both return the number of bytes written. Every codec returns 0 at once when
 * the size is 0.
 *
 * The codecs keep their state in the original's BSS variables (B_800E....),
 * and their big work areas come from gN64MemoryManager.
 */
#include "mpfcomp.h"
#include "mpfmem.h"

/* libultra-side qsort (0x800A3C80) */
void func_800A3C80(void *base, u32 count, u32 size, s32 (*compare)(const void *, const void *));

/* the helper functions of the original are real functions here too */
void func_8008F284(void *tree);
void func_8008F2DC(void *tree, s32 r);
void func_8008F490(void *tree, s32 p);
s32  func_8008F770(const u8 *a, const u8 *b, u32 n);
s32  func_8008F7CC(const void *a, const void *b);
void func_8008FD30(void);
void func_8008FD48(s32 bit);
void func_8008FDB8(void);
void func_8008FDE8(s32 bit);
void func_8008FE3C(void);
void func_8008FE5C(s32 symbol, const s32 *cumFreq);
void func_80090038(void);
void func_80090080(void);
void func_80090108(s32 symbol);
void func_80090350(void);
s32  func_80090364(void);
void func_800903D0(void);
s32  func_80090434(const s32 *cumFreq);
void func_800906A8(void);
void func_80090730(s32 symbol);
void func_80090940(void);
void func_800909B4(void);
void func_80090A4C(void);
u32  func_80090AB8(u32 prefix, u32 character);
void func_80090B7C(void);
void func_80090B94(s32 bit);
void func_80090C04(u32 code, s32 numBits);
void func_80090C48(void);
void func_80090EA0(void);
void func_80090F38(void);
void func_80090FA4(void);

/* ======================================================================== */
/* Type 0x20 - LZSS                                                          */
/* ======================================================================== */

#define LZSS_N         4096     /* window size                           */
#define LZSS_F         18       /* longest match                         */
#define LZSS_THRESHOLD 2        /* matches of this length or less are sent as literals */
#define LZSS_NIL       LZSS_N   /* "no node" in the trees                */

/* Work area allocated by mpEncodeType20 (0xD430 bytes). */
typedef struct LZSS_State {
    /* 0x0000 */ u32 textSize;                      /* cleared, never used      */
    /* 0x0004 */ u32 codeSize;                      /* bytes output so far      */
    /* 0x0008 */ u8  textBuf[LZSS_N + LZSS_F - 1];  /* ring buffer + mirror of its first F-1 bytes */
    /* 0x101C */ s32 matchPosition;
    /* 0x1020 */ s32 matchLength;
    /* 0x1024 */ s32 lson[LZSS_N + 1];              /* binary search trees: left children */
    /* 0x5028 */ s32 rson[LZSS_N + 257];            /* right children; 4097..4352 = tree roots per first byte */
    /* 0x942C */ s32 dad[LZSS_N + 1];               /* parents                  */
} LZSS_State;                                      /* 0xD430 */
_Static_assert(sizeof(LZSS_State) == 0xD430, "LZSS_State size");

/* 0x8008F284: LZSS_InitTree - empties all the trees */
void func_8008F284(void *tree) {
    LZSS_State *t = tree;
    s32 i;

    for (i = LZSS_N + 1; i <= LZSS_N + 256; i++) t->rson[i] = LZSS_NIL;
    for (i = 0; i < LZSS_N; i++) t->dad[i] = LZSS_NIL;
}

/*
 * 0x8008F2DC: LZSS_InsertNode - inserts the string at textBuf[r] into its tree
 * and finds the longest match (matchPosition / matchLength). A string that
 * matches all F bytes replaces the old node.
 */
void func_8008F2DC(void *tree, s32 r) {
    LZSS_State *t = tree;
    s32 i, p, cmp;
    const u8 *key;

    cmp = 1;
    key = &t->textBuf[r];
    p = LZSS_N + 1 + key[0];
    t->rson[r] = t->lson[r] = LZSS_NIL;
    t->matchLength = 0;
    for (;;) {
        if (cmp >= 0) {
            if (t->rson[p] != LZSS_NIL) {
                p = t->rson[p];
            } else {
                t->rson[p] = r;
                t->dad[r] = p;
                return;
            }
        } else {
            if (t->lson[p] != LZSS_NIL) {
                p = t->lson[p];
            } else {
                t->lson[p] = r;
                t->dad[r] = p;
                return;
            }
        }
        for (i = 1; i < LZSS_F; i++) {
            if ((cmp = key[i] - t->textBuf[p + i]) != 0) break;
        }
        if (i > t->matchLength) {
            t->matchPosition = p;
            if ((t->matchLength = i) >= LZSS_F) break;
        }
    }
    /* full-length match: node r takes p's place */
    t->dad[r] = t->dad[p];
    t->lson[r] = t->lson[p];
    t->rson[r] = t->rson[p];
    t->dad[t->lson[p]] = r;
    t->dad[t->rson[p]] = r;
    if (t->rson[t->dad[p]] == p) t->rson[t->dad[p]] = r;
    else                         t->lson[t->dad[p]] = r;
    t->dad[p] = LZSS_NIL;
}

/* 0x8008F490: LZSS_DeleteNode - removes node p from its tree */
void func_8008F490(void *tree, s32 p) {
    LZSS_State *t = tree;
    s32 q;

    if (t->dad[p] == LZSS_NIL) return;   /* not in a tree */
    if (t->rson[p] == LZSS_NIL) {
        q = t->lson[p];
    } else if (t->lson[p] == LZSS_NIL) {
        q = t->rson[p];
    } else {
        q = t->lson[p];
        if (t->rson[q] != LZSS_NIL) {
            do {
                q = t->rson[q];
            } while (t->rson[q] != LZSS_NIL);
            t->rson[t->dad[q]] = t->lson[q];
            t->dad[t->lson[q]] = t->dad[q];
            t->lson[q] = t->lson[p];
            t->dad[t->lson[p]] = q;
        }
        t->rson[q] = t->rson[p];
        t->dad[t->rson[p]] = q;
    }
    t->dad[q] = t->dad[p];
    if (t->rson[t->dad[p]] == p) t->rson[t->dad[p]] = q;
    else                         t->lson[t->dad[p]] = q;
    t->dad[p] = LZSS_NIL;
}

/*
 * 0x8008EF40
 * Output: the unpacked size as a little-endian u32, then groups of a flag byte
 * and up to 8 items. Interplay's changes to Okumura's format:
 *   - flag bit 0 = literal byte, 1 = match (inverted; a group starts as 0xFF,
 *     so the unused bits of the last group read as "match");
 *   - a match is a little-endian u16: bits 0-11 = (position - current) & 0xFFF,
 *     bits 12-15 = length - 3;
 *   - the window is not pre-filled with spaces and only the current position
 *     is inserted at the start. The ring buffer comes uninitialised from the
 *     heap, so near the end of the data the choice between equally long
 *     matches can depend on stale heap bytes (the output stays decodable).
 * Returns the bytes written including the 4-byte size (0 when size is 0).
 */
s32 mpEncodeType20(u8 *dst, const u8 *src, s32 size) {
    LZSS_State *t;
    u8 *out;
    u8 codeBuf[17], mask;
    s32 i, len, r, s, lastMatchLength, codeBufPtr;
    u32 inPos = 0;
    u32 code;

    dst[0] = (u8)size;
    dst[1] = (u8)(size >> 8);
    dst[2] = (u8)(size >> 16);
    dst[3] = (u8)(size >> 24);
    out = dst + 4;
    if (size == 0) return 0;

    t = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(LZSS_State), 0);
    t->textSize = 0;
    t->codeSize = 0;
    func_8008F284(t);

    codeBuf[0] = 0xFF;
    mask = 1;
    codeBufPtr = 1;
    s = 0;
    r = LZSS_N - LZSS_F;
    for (len = 0; len < LZSS_F && inPos < (u32)size; len++) {
        t->textBuf[r + len] = src[inPos++];
    }
    func_8008F2DC(t, r);
    do {
        if (t->matchLength > len) t->matchLength = len;
        if (t->matchLength <= LZSS_THRESHOLD) {
            t->matchLength = 1;
            codeBuf[0] &= ~mask;
            codeBuf[codeBufPtr] = t->textBuf[r];
        } else {
            code = ((t->matchPosition - r) & 0xFFF) | ((t->matchLength - (LZSS_THRESHOLD + 1)) << 12);
            codeBuf[codeBufPtr++] = (u8)code;
            codeBuf[codeBufPtr] = (u8)(code >> 8);
        }
        codeBufPtr++;
        if ((mask <<= 1) == 0) {
            for (i = 0; i < codeBufPtr; i++) *out++ = codeBuf[i];
            t->codeSize += codeBufPtr;
            codeBuf[0] = 0xFF;
            mask = 1;
            codeBufPtr = 1;
        }
        lastMatchLength = t->matchLength;
        for (i = 0; i < lastMatchLength && inPos < (u32)size; i++) {
            u8 c;
            func_8008F490(t, s);
            c = src[inPos++];
            t->textBuf[s] = c;
            if (s < LZSS_F - 1) t->textBuf[s + LZSS_N] = c;
            s = (s + 1) & (LZSS_N - 1);
            r = (r + 1) & (LZSS_N - 1);
            func_8008F2DC(t, r);
        }
        while (i++ < lastMatchLength) {
            func_8008F490(t, s);
            s = (s + 1) & (LZSS_N - 1);
            r = (r + 1) & (LZSS_N - 1);
            if (--len) func_8008F2DC(t, r);
        }
    } while (len > 0);
    if (codeBufPtr > 1) {
        for (i = 0; i < codeBufPtr; i++) *out++ = codeBuf[i];
        t->codeSize += codeBufPtr;
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, t);
    return (s32)(out - dst);
}

/* ======================================================================== */
/* Type 0x40 - move-to-front                                                 */
/* ======================================================================== */

/* 0x8008F640: each byte becomes its position in a recently-used list */
s32 mpEncodeType40(u8 *dst, const u8 *src, s32 size) {
    u8 table[256];
    const u8 *end = src + size;
    u8 *out = dst;
    s32 i;
    u8 c;

    if (size == 0) return 0;
    for (i = 0; i < 256; i++) table[i] = (u8)i;
    do {
        c = *src++;
        for (i = 0; i < 256; i++) {
            if (table[i] == c) break;
        }
        *out++ = (u8)i;
        for (; i > 0; i--) table[i] = table[i - 1];
        table[0] = c;
    } while (src < end);
    return (s32)(out - dst);
}

/* 0x8008F6E0 */
s32 mpDecodeType40(u8 *dst, const u8 *src, s32 rawSize) {
    u8 table[256];
    const u8 *end = src + rawSize;   /* packed size = unpacked size */
    u8 *out = dst;
    s32 i;
    u8 c;

    if (rawSize == 0) return 0;
    for (i = 0; i < 256; i++) table[i] = (u8)i;
    do {
        i = *src++;
        c = table[i];
        *out++ = c;
        for (; i > 0; i--) table[i] = table[i - 1];
        table[0] = c;
    } while (src < end);
    return (s32)(out - dst);
}

/* ======================================================================== */
/* Type 0x30 - Burrows-Wheeler block sort                                    */
/* ======================================================================== */

#define BWT_BLOCK 200000        /* bytes per block */
#define BWT_EOB   '?'           /* placeholder output for the end-of-block row */

extern u32 B_800ECFC0;          /* current block length (for the compare)  */
extern const u8 *B_800ECFC4;    /* current block start                     */
#define sBwtLength B_800ECFC0
#define sBwtBlock  B_800ECFC4

/* 0x8008F770: memcmp returning -1 / 0 / 1 */
s32 func_8008F770(const u8 *a, const u8 *b, u32 n) {
    while (n != 0) {
        if (*a < *b) return -1;
        if (*b < *a) return 1;
        a++;
        b++;
        n--;
    }
    return 0;
}

/*
 * 0x8008F7CC: qsort compare of two suffixes of the block (by start index).
 * When one is a prefix of the other, the longer one sorts first, so the empty
 * suffix (index == length) is the last row.
 */
s32 func_8008F7CC(const void *a, const void *b) {
    u32 ia = *(const u32 *)a, ib = *(const u32 *)b;
    u32 lenA = sBwtLength - ia, lenB = sBwtLength - ib;
    s32 r = func_8008F770(sBwtBlock + ia, sBwtBlock + ib, (lenB < lenA) ? lenB : lenA);

    if (r == 0) r = (s32)(lenB - lenA);
    return r;
}

/*
 * 0x8008F838
 * Per block: BE32 rows (= block length + 1), the last column of the sorted
 * rotations (the end-of-block row as '?'), BE32 row of the rotation starting
 * at byte 0 ("primary"), BE32 row of the end-of-block marker.
 * The original keeps its 200001-entry index array (781 KB) on the stack.
 */
s32 mpEncodeType30(u8 *dst, const u8 *src, s32 size) {
    u32 rows[BWT_BLOCK + 1];
    const u8 *end = src + size;
    u8 *out = dst;
    u32 remaining = (u32)size;
    u32 i, primary = 0, eob = 0;

    if (size == 0) return 0;
    do {
        sBwtBlock = src;
        sBwtLength = (remaining > BWT_BLOCK) ? BWT_BLOCK : remaining;
        WRITE_BE32(out, sBwtLength + 1);
        out += 4;
        for (i = sBwtLength + 1; i != 0;) {
            i--;
            rows[i] = i;
        }
        func_800A3C80(rows, sBwtLength + 1, sizeof(u32), func_8008F7CC);
        for (i = 0; i <= sBwtLength; i++) {
            if (rows[i] == 1) primary = i;
            if (rows[i] == 0) {
                eob = i;
                *out = BWT_EOB;
            } else {
                *out = sBwtBlock[rows[i] - 1];
            }
            out++;
        }
        WRITE_BE32(out, primary);
        out += 4;
        WRITE_BE32(out, eob);
        out += 4;
        src += sBwtLength;
        remaining -= sBwtLength;
    } while (src < end);
    return (s32)(out - dst);
}

/* Work area of mpDecodeType30 (0xC3D0C bytes) */
typedef struct BWT_DecodeState {
    /* 0x00000 */ u32 next[BWT_BLOCK + 1];   /* row -> next row in the text */
    /* 0xC3504 */ u32 count[257];            /* per symbol (256 = end-of-block) */
    /* 0xC3908 */ u32 start[257];            /* first sorted row of each symbol */
} BWT_DecodeState;
_Static_assert(sizeof(BWT_DecodeState) == 0xC3D0C, "BWT_DecodeState size");

static inline u32 ReadBE32Advance(const u8 **p) {
    u32 v = READ_BE32(*p);
    *p += 4;
    return v;
}

/*
 * 0x8008FA70
 * Original bug: the loop runs while the read position is below src + rawSize,
 * but each packed block is 13 bytes longer than its unpacked data, so with
 * more than one block (over 200000 bytes) the last block can be skipped.
 * A row count over 200001 is reported but the block is still decoded.
 */
s32 mpDecodeType30(u8 *dst, const u8 *src, s32 rawSize) {
    BWT_DecodeState *st;
    const u8 *end = src + rawSize;
    const u8 *data;
    u8 *out = dst;
    u32 n, primary, eob, i, sum, t, c, j;

    if (rawSize == 0) return 0;
    st = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(BWT_DecodeState), 0);
    do {
        n = ReadBE32Advance(&src);
        if (n > BWT_BLOCK + 1) {
            mpFatalError("BWT Buffer overflow!\n");
        }
        data = src;
        src += n;
        primary = ReadBE32Advance(&src);
        eob = ReadBE32Advance(&src);

        for (i = 0; i < 257; i++) st->count[i] = 0;
        for (i = 0; i < n; i++) {
            if (i == eob) st->count[256]++;
            else          st->count[data[i]]++;
        }
        sum = 0;
        for (i = 0; i < 257; i++) {
            st->start[i] = sum;
            t = st->count[i];
            st->count[i] = 0;
            sum += t;
        }
        for (i = 0; i < n; i++) {
            c = (i == eob) ? 256 : data[i];
            st->next[st->count[c] + st->start[c]] = i;
            st->count[c]++;
        }
        j = primary;
        for (i = 0; i < n - 1; i++) {
            *out++ = data[j];
            j = st->next[j];
        }
    } while (src < end);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, st);
    return (s32)(out - dst);
}

/* ======================================================================== */
/* Type 0x50 - adaptive arithmetic coding                                    */
/* ======================================================================== */

#define AC_TOP          0xFFFF
#define AC_FIRST_QTR    0x4000
#define AC_HALF         0x8000
#define AC_THIRD_QTR    0xC000
#define AC_NUM_CHARS    256
#define AC_EOF_SYMBOL   (AC_NUM_CHARS + 1)  /* 257 */
#define AC_NUM_SYMBOLS  (AC_NUM_CHARS + 1)
#define AC_MAX_FREQ     0x3FFF

/* encoder model and state */
extern s32 B_800ECFD0[AC_NUM_CHARS];            /* char -> symbol index      */
extern u8  B_800ED3D0[AC_NUM_SYMBOLS + 1];      /* symbol index -> char      */
extern s32 B_800ED4E0[AC_NUM_SYMBOLS + 1];      /* symbol frequencies        */
extern s32 B_800ED8F0[AC_NUM_SYMBOLS + 1];      /* cumulative frequencies    */
extern s32 B_800EDCF8;                          /* bit buffer                */
extern s32 B_800EDCFC;                          /* bits free in the buffer   */
extern s32 B_800EDD00;                          /* low                       */
extern s32 B_800EDD04;                          /* high                      */
extern s32 B_800EDD08;                          /* opposite bits to follow   */
extern u8 *B_800EDD0C;                          /* output pointer            */
#define sEncCharToIndex B_800ECFD0
#define sEncIndexToChar B_800ED3D0
#define sEncFreq        B_800ED4E0
#define sEncCumFreq     B_800ED8F0
#define sEncBuffer      B_800EDCF8
#define sEncBitsToGo    B_800EDCFC
#define sEncLow         B_800EDD00
#define sEncHigh        B_800EDD04
#define sEncBitsToFollow B_800EDD08
#define sEncOut         B_800EDD0C

/* decoder model and state (a separate copy) */
extern s32 B_800EDD10[AC_NUM_CHARS];
extern u8  B_800EE110[AC_NUM_SYMBOLS + 1];
extern s32 B_800EE220[AC_NUM_SYMBOLS + 1];
extern s32 B_800EE630[AC_NUM_SYMBOLS + 1];
extern const u8 *B_800EEA38;                    /* input pointer             */
extern s32 B_800EEA3C;                          /* bit buffer                */
extern s32 B_800EEA40;                          /* bits left in the buffer   */
extern s32 B_800EEA44;                          /* garbage bits (cleared, never used) */
extern s32 B_800EEA48;                          /* value                     */
extern s32 B_800EEA4C;                          /* low                       */
extern s32 B_800EEA50;                          /* high                      */
#define sDecCharToIndex B_800EDD10
#define sDecIndexToChar B_800EE110
#define sDecFreq        B_800EE220
#define sDecCumFreq     B_800EE630
#define sDecIn          B_800EEA38
#define sDecBuffer      B_800EEA3C
#define sDecBitsToGo    B_800EEA40
#define sDecGarbageBits B_800EEA44
#define sDecValue       B_800EEA48
#define sDecLow         B_800EEA4C
#define sDecHigh        B_800EEA50

/* 0x8008FD30: start_outputing_bits */
void func_8008FD30(void) {
    sEncBuffer = 0;
    sEncBitsToGo = 8;
}

/* 0x8008FD48: output_bit - bits fill each byte from the top down */
void func_8008FD48(s32 bit) {
    sEncBuffer >>= 1;
    if (bit) sEncBuffer |= 0x80;
    if (--sEncBitsToGo == 0) {
        *sEncOut++ = (u8)sEncBuffer;
        sEncBitsToGo = 8;
    }
}

/* 0x8008FDB8: done_outputing_bits - always writes one more byte */
void func_8008FDB8(void) {
    *sEncOut++ = (u8)(sEncBuffer >> sEncBitsToGo);
}

/* 0x8008FDE8: bit_plus_follow */
void func_8008FDE8(s32 bit) {
    func_8008FD48(bit);
    while (sEncBitsToFollow > 0) {
        func_8008FD48(!bit);
        sEncBitsToFollow--;
    }
}

/* 0x8008FE3C: start_encoding */
void func_8008FE3C(void) {
    sEncLow = 0;
    sEncHigh = AC_TOP;
    sEncBitsToFollow = 0;
}

/* 0x8008FE5C: encode_symbol */
void func_8008FE5C(s32 symbol, const s32 *cumFreq) {
    s32 range = sEncHigh - sEncLow + 1;

    sEncHigh = sEncLow + (range * cumFreq[symbol - 1]) / cumFreq[0] - 1;
    sEncLow = sEncLow + (range * cumFreq[symbol]) / cumFreq[0];
    for (;;) {
        if (sEncHigh < AC_HALF) {
            func_8008FDE8(0);
        } else if (sEncLow >= AC_HALF) {
            func_8008FDE8(1);
            sEncLow -= AC_HALF;
            sEncHigh -= AC_HALF;
        } else if (sEncLow >= AC_FIRST_QTR && sEncHigh < AC_THIRD_QTR) {
            sEncBitsToFollow++;
            sEncLow -= AC_FIRST_QTR;
            sEncHigh -= AC_FIRST_QTR;
        } else {
            break;
        }
        sEncLow = 2 * sEncLow;
        sEncHigh = 2 * sEncHigh + 1;
    }
}

/* 0x80090038: done_encoding */
void func_80090038(void) {
    sEncBitsToFollow++;
    func_8008FDE8((sEncLow < AC_FIRST_QTR) ? 0 : 1);
}

/* Shared model code: start_model / update_model on one of the two copies. */
static inline void AC_StartModel(s32 *charToIndex, u8 *indexToChar, s32 *freq, s32 *cumFreq) {
    s32 i;

    for (i = 0; i < AC_NUM_CHARS; i++) {
        charToIndex[i] = i + 1;
        indexToChar[i + 1] = (u8)i;
    }
    for (i = 0; i <= AC_NUM_SYMBOLS; i++) {
        freq[i] = 1;
        cumFreq[i] = AC_NUM_SYMBOLS - i;
    }
    freq[0] = 0;
}

static inline void AC_UpdateModel(s32 symbol, s32 *charToIndex, u8 *indexToChar, s32 *freq, s32 *cumFreq) {
    s32 i, cum;
    u8 chI, chSymbol;

    if (cumFreq[0] == AC_MAX_FREQ) {
        /* halve all the counts (keeping them non-zero) */
        cum = 0;
        for (i = AC_NUM_SYMBOLS; i >= 0; i--) {
            freq[i] = (freq[i] + 1) / 2;
            cumFreq[i] = cum;
            cum += freq[i];
        }
    }
    for (i = symbol; freq[i] == freq[i - 1]; i--) {
    }
    if (i < symbol) {
        chI = indexToChar[i];
        chSymbol = indexToChar[symbol];
        indexToChar[i] = chSymbol;
        indexToChar[symbol] = chI;
        charToIndex[chI] = symbol;
        charToIndex[chSymbol] = i;
    }
    freq[i]++;
    while (i > 0) {
        i--;
        cumFreq[i]++;
    }
}

/* 0x80090080: start_model (encoder) */
void func_80090080(void) {
    AC_StartModel(sEncCharToIndex, sEncIndexToChar, sEncFreq, sEncCumFreq);
}

/* 0x80090108: update_model (encoder) */
void func_80090108(s32 symbol) {
    AC_UpdateModel(symbol, sEncCharToIndex, sEncIndexToChar, sEncFreq, sEncCumFreq);
}

/* 0x80090274 */
s32 mpEncodeType50(u8 *dst, const u8 *src, s32 size) {
    const u8 *end = src + size;
    s32 symbol;

    if (size == 0) return 0;
    sEncOut = dst;
    func_80090080();
    func_8008FD30();
    func_8008FE3C();
    do {
        symbol = sEncCharToIndex[*src++];
        func_8008FE5C(symbol, sEncCumFreq);
        func_80090108(symbol);
    } while (src < end);
    func_8008FE5C(AC_EOF_SYMBOL, sEncCumFreq);
    func_80090038();
    func_8008FDB8();
    return (s32)(sEncOut - dst);
}

/* 0x80090350: start_inputing_bits */
void func_80090350(void) {
    sDecBitsToGo = 0;
    sDecGarbageBits = 0;
}

/* 0x80090364: input_bit (no end-of-data check: reads on past the packed data) */
s32 func_80090364(void) {
    s32 t;

    if (sDecBitsToGo == 0) {
        sDecBuffer = *sDecIn++;
        sDecBitsToGo = 8;
    }
    t = sDecBuffer;
    sDecBuffer >>= 1;
    sDecBitsToGo--;
    return t & 1;
}

/* 0x800903D0: start_decoding */
void func_800903D0(void) {
    s32 i;

    sDecValue = 0;
    for (i = 1; i <= 16; i++) {
        sDecValue = 2 * sDecValue + func_80090364();
    }
    sDecLow = 0;
    sDecHigh = AC_TOP;
}

/* 0x80090434: decode_symbol */
s32 func_80090434(const s32 *cumFreq) {
    s32 range = sDecHigh - sDecLow + 1;
    s32 cum = ((sDecValue - sDecLow + 1) * cumFreq[0] - 1) / range;
    s32 symbol;

    for (symbol = 1; cumFreq[symbol] > cum; symbol++) {
    }
    sDecHigh = sDecLow + (range * cumFreq[symbol - 1]) / cumFreq[0] - 1;
    sDecLow = sDecLow + (range * cumFreq[symbol]) / cumFreq[0];
    for (;;) {
        if (sDecHigh < AC_HALF) {
            /* nothing */
        } else if (sDecLow >= AC_HALF) {
            sDecValue -= AC_HALF;
            sDecLow -= AC_HALF;
            sDecHigh -= AC_HALF;
        } else if (sDecLow >= AC_FIRST_QTR && sDecHigh < AC_THIRD_QTR) {
            sDecValue -= AC_FIRST_QTR;
            sDecLow -= AC_FIRST_QTR;
            sDecHigh -= AC_FIRST_QTR;
        } else {
            break;
        }
        sDecLow = 2 * sDecLow;
        sDecHigh = 2 * sDecHigh + 1;
        sDecValue = 2 * sDecValue + func_80090364();
    }
    return symbol;
}

/* 0x800906A8: start_model (decoder) */
void func_800906A8(void) {
    AC_StartModel(sDecCharToIndex, sDecIndexToChar, sDecFreq, sDecCumFreq);
}

/* 0x80090730: update_model (decoder) */
void func_80090730(s32 symbol) {
    AC_UpdateModel(symbol, sDecCharToIndex, sDecIndexToChar, sDecFreq, sDecCumFreq);
}

/* 0x8009089C: decodes until the end-of-data symbol (rawSize only checked for 0) */
s32 mpDecodeType50(u8 *dst, const u8 *src, s32 rawSize) {
    u8 *out = dst;
    s32 symbol;

    if (rawSize == 0) return 0;
    sDecIn = src;
    func_800906A8();
    func_80090350();
    func_800903D0();
    for (;;) {
        symbol = func_80090434(sDecCumFreq);
        if (symbol == AC_EOF_SYMBOL) break;
        *out++ = sDecIndexToChar[symbol];
        func_80090730(symbol);
    }
    return (s32)(out - dst);
}

/* ======================================================================== */
/* Type 0x60 - LZW                                                           */
/* ======================================================================== */

#define LZW_TABLE_SIZE  35023   /* hash table entries (a prime)          */
#define LZW_CHUNKS      137     /* the table is allocated as 137 chunks of 256 entries */
#define LZW_END         0x100   /* end of data                           */
#define LZW_GROW        0x101   /* code size grows by one bit            */
#define LZW_RESET       0x102   /* dictionary reset                      */
#define LZW_FIRST_CODE  0x103
#define LZW_MAX_CODE    0x7FFF  /* 15-bit codes                          */

typedef struct LZW_Entry {
    /* 0x0 */ s32 codeValue;    /* -1 = unused (encoder only)            */
    /* 0x4 */ u32 prefixCode;
    /* 0x8 */ u8  appendChar;
} LZW_Entry;                    /* 12 bytes */
_Static_assert(sizeof(LZW_Entry) == 12, "LZW_Entry size");

#define LZW_ENTRY(chunks, i) (&(chunks)[(u32)(i) >> 8][(u32)(i) & 0xFF])

/* encoder */
extern LZW_Entry *B_800EEA60[LZW_CHUNKS];   /* dictionary chunks           */
extern u8 *B_800EEC84;                      /* 35023-byte buffer (allocated, unused by the encoder) */
extern u32 B_800EEC88;                      /* next code                   */
extern s32 B_800EEC8C;                      /* bits per code               */
extern u32 B_800EEC90;                      /* largest code for this size  */
extern s32 B_800EEC94;                      /* bit buffer                  */
extern s32 B_800EEC98;                      /* bits free in the buffer     */
extern u8 *B_800EEC9C;                      /* output pointer              */
#define sLzwEncTable    B_800EEA60
#define sLzwEncSpare    B_800EEC84
#define sLzwEncNextCode B_800EEC88
#define sLzwEncNumBits  B_800EEC8C
#define sLzwEncMaxCode  B_800EEC90
#define sLzwEncBuffer   B_800EEC94
#define sLzwEncBitsToGo B_800EEC98
#define sLzwEncOut      B_800EEC9C

/* decoder */
extern LZW_Entry *B_800EECA0[LZW_CHUNKS];
extern u8 *B_800EEEC4;                      /* decode stack                */
extern u32 B_800EEEC8;                      /* next code                   */
extern u32 B_800EEECC;                      /* largest code (set, never used) */
extern s32 B_800EEED0;                      /* bits per code               */
extern const u8 *B_800EEED4;                /* input pointer               */
extern s32 B_800EEED8;                      /* bit position in *input (0-7) */
#define sLzwDecTable    B_800EECA0
#define sLzwDecStack    B_800EEEC4
#define sLzwDecNextCode B_800EEEC8
#define sLzwDecMaxCode  B_800EEECC
#define sLzwDecNumBits  B_800EEED0
#define sLzwDecIn       B_800EEED4
#define sLzwDecBitPos   B_800EEED8

/* 0x80090940: LZW_InitTable (encoder) */
void func_80090940(void) {
    u32 i;

    for (i = 0; i < LZW_TABLE_SIZE; i++) LZW_ENTRY(sLzwEncTable, i)->codeValue = -1;
    sLzwEncNextCode = LZW_FIRST_CODE;
    sLzwEncNumBits = 9;
    sLzwEncMaxCode = 0x1FF;
}

/* 0x800909B4: allocates the encoder's dictionary */
void func_800909B4(void) {
    u32 i;

    for (i = 0; i < LZW_CHUNKS; i++) {
        sLzwEncTable[i] = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 256 * sizeof(LZW_Entry), 0);
        if (sLzwEncTable[i] == NULL) {
            mpFatalError("Error allocating dictionary space");
        }
    }
    sLzwEncSpare = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, LZW_TABLE_SIZE, 0);
    if (sLzwEncSpare == NULL) {
        mpFatalError("Error allocating decoding stack space");
    }
}

/* 0x80090A4C */
void func_80090A4C(void) {
    u32 i;

    for (i = 0; i < LZW_CHUNKS; i++) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sLzwEncTable[i]);
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sLzwEncSpare);
}

/* 0x80090AB8: LZW_FindMatch - hash slot of (prefix, character): its entry or a free one */
u32 func_80090AB8(u32 prefix, u32 character) {
    u32 index = (character << 7) ^ prefix;
    u32 offset = (index == 0) ? 1 : LZW_TABLE_SIZE - index;
    LZW_Entry *e;

    for (;;) {
        e = LZW_ENTRY(sLzwEncTable, index);
        if (e->codeValue == -1) return index;
        if (e->prefixCode == prefix && e->appendChar == (character & 0xFF)) return index;
        if (index < offset) index = index + LZW_TABLE_SIZE - offset;
        else                index -= offset;
    }
}

/* 0x80090B7C */
void func_80090B7C(void) {
    sLzwEncBuffer = 0;
    sLzwEncBitsToGo = 8;
}

/* 0x80090B94: output_bit - bits fill each byte from the top down */
void func_80090B94(s32 bit) {
    sLzwEncBuffer >>= 1;
    if (bit) sLzwEncBuffer |= 0x80;
    if (--sLzwEncBitsToGo == 0) {
        *sLzwEncOut++ = (u8)sLzwEncBuffer;
        sLzwEncBitsToGo = 8;
    }
}

/* 0x80090C04: output_code - least significant bit first */
void func_80090C04(u32 code, s32 numBits) {
    s32 c = (s32)code;

    while (numBits != 0) {
        func_80090B94(c & 1);
        numBits--;
        c >>= 1;
    }
}

/* 0x80090C48: flushes a partly filled byte */
void func_80090C48(void) {
    if (sLzwEncBitsToGo != 8) {
        *sLzwEncOut++ = (u8)(sLzwEncBuffer >> sLzwEncBitsToGo);
    }
}

/* 0x80090C88 */
s32 mpEncodeType60(u8 *dst, const u8 *src, s32 size) {
    const u8 *end = src + size;
    u32 stringCode, index;
    u8 character;
    LZW_Entry *e;

    if (size == 0) return 0;
    sLzwEncOut = dst;
    func_80090B7C();
    func_800909B4();
    func_80090940();
    stringCode = *src++;
    while (src < end) {
        character = *src++;
        index = func_80090AB8(stringCode, character);
        e = LZW_ENTRY(sLzwEncTable, index);
        if (e->codeValue != -1) {
            stringCode = (u32)e->codeValue;
            continue;
        }
        e->codeValue = (s32)sLzwEncNextCode++;
        e->prefixCode = stringCode;
        e->appendChar = character;
        func_80090C04(stringCode, sLzwEncNumBits);
        stringCode = character;
        if (sLzwEncNextCode > LZW_MAX_CODE) {
            func_80090C04(LZW_RESET, sLzwEncNumBits);
            func_80090940();
        } else if (sLzwEncNextCode > sLzwEncMaxCode) {
            func_80090C04(LZW_GROW, sLzwEncNumBits);
            sLzwEncNumBits++;
            sLzwEncMaxCode = sLzwEncMaxCode * 2 + 1;
        }
    }
    func_80090C04(stringCode, sLzwEncNumBits);
    func_80090C04(LZW_END, sLzwEncNumBits);
    func_80090C48();
    func_80090A4C();
    return (s32)(sLzwEncOut - dst);
}

/* 0x80090EA0: allocates the decoder's dictionary and stack */
void func_80090EA0(void) {
    u32 i;

    for (i = 0; i < LZW_CHUNKS; i++) {
        sLzwDecTable[i] = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 256 * sizeof(LZW_Entry), 0);
        if (sLzwDecTable[i] == NULL) {
            mpFatalError("Error allocating dictionary space");
        }
    }
    sLzwDecStack = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, LZW_TABLE_SIZE, 0);
    if (sLzwDecStack == NULL) {
        mpFatalError("Error allocating decoding stack space");
    }
}

/* 0x80090F38 */
void func_80090F38(void) {
    u32 i;

    for (i = 0; i < LZW_CHUNKS; i++) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sLzwDecTable[i]);
    }
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sLzwDecStack);
}

/* 0x80090FA4 */
void func_80090FA4(void) {
    sLzwDecBitPos = 0;
}

/* Reads numBits (1-15) from the input, least significant bit first. */
static u32 LZW_GetBits(s32 numBits) {
    const u8 *p = sLzwDecIn;
    s32 end = sLzwDecBitPos + numBits;
    u32 v = p[0];

    if (end > 8) v |= (u32)p[1] << 8;
    if (end > 16) v |= (u32)p[2] << 16;
    v = (v >> sLzwDecBitPos) & ((1u << numBits) - 1);
    sLzwDecIn = p + (end >> 3);
    sLzwDecBitPos = end & 7;
    return v;
}

/*
 * The original reads each code size with its own piece of code (a jump table
 * for 1-15 bits). Two quirks kept: the 8-bit case ignores the bit position
 * (takes the whole current byte and restarts at bit 0), and a size outside
 * 1-15 reads nothing and keeps the previous code. Neither can happen with
 * data from mpEncodeType60 (codes are always 9-15 bits).
 */
static u32 LZW_ReadCode(u32 prevCode) {
    u32 v;

    if (sLzwDecNumBits < 1 || sLzwDecNumBits > 15) return prevCode;
    if (sLzwDecNumBits == 8) {
        sLzwDecBitPos = 0;
        v = *sLzwDecIn++;
        return v;
    }
    return LZW_GetBits(sLzwDecNumBits);
}

/* 0x80090FB0: decodes until the end code (rawSize only checked for 0) */
s32 mpDecodeType60(u8 *dst, const u8 *src, s32 rawSize) {
    u8 *out = dst;
    u32 newCode = 0, oldCode, string, count, i;
    u8 character;
    LZW_Entry *e;

    if (rawSize == 0) return 0;
    sLzwDecIn = src;
    func_80090EA0();
    func_80090FA4();
    do {
        for (i = 0; i < LZW_TABLE_SIZE; i++) LZW_ENTRY(sLzwDecTable, i)->codeValue = -1;
        sLzwDecNextCode = LZW_FIRST_CODE;
        sLzwDecNumBits = 9;
        sLzwDecMaxCode = 0x1FF;
        oldCode = LZW_GetBits(9);
        if (oldCode == LZW_END) break;
        character = (u8)oldCode;
        *out++ = (u8)oldCode;
        for (;;) {
            newCode = LZW_ReadCode(newCode);
            if (newCode == LZW_END || newCode == LZW_RESET) break;
            if (newCode == LZW_GROW) {
                sLzwDecNumBits++;
                continue;
            }
            /* unpack the string backwards onto the stack */
            count = 0;
            if (newCode >= sLzwDecNextCode) {
                /* code not in the dictionary yet (the "KwKwK" case) */
                sLzwDecStack[0] = character;
                count = 1;
                string = oldCode;
            } else {
                string = newCode;
            }
            while (string >= 0x100) {
                e = LZW_ENTRY(sLzwDecTable, string);
                sLzwDecStack[count++] = e->appendChar;
                string = e->prefixCode;
            }
            sLzwDecStack[count] = (u8)string;
            character = sLzwDecStack[count];
            for (i = count + 1; i != 0;) {
                i--;
                *out++ = sLzwDecStack[i];
            }
            e = LZW_ENTRY(sLzwDecTable, sLzwDecNextCode);
            e->prefixCode = oldCode;
            e->appendChar = character;
            sLzwDecNextCode++;
            oldCode = newCode;
        }
    } while (newCode != LZW_END && oldCode != LZW_END);
    func_80090F38();
    return (s32)(out - dst);
}
