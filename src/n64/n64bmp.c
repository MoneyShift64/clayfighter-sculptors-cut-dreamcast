/*
 * The shared asset loader (0x80080360) and N64BMP.C (0x80080590-0x80082210): 2D bitmaps.
 *
 * A bitmap (N64Bitmap, n64gfx.h) is a 16-byte header, then for colour-indexed formats the
 * palette, then the texels. func_80080590 draws one as screen rectangles (copy mode when it
 * can), in bands of rows that fit the 4 KB texture memory; func_80081924 loads a whole one as
 * a texture for 3D drawing; mpDecodeBitmapTo32Bit turns a CI8 one into RGBA32;
 * func_80081804 copies the screen into a new RGBA16 bitmap.
 */
#include "n64gfx.h"
#include "mpfcomp.h"

/* 0x80080360: Asset_Load - loads an asset file from the ROM. Its 12-byte header says how
 * many times it is compressed (byte 2; 0 = stored) and its size (word 2); the result is the
 * header followed by the unpacked data. */
u8 *func_80080360(u32 romAddr) {
    MPF_LinkedMemoryManager *mgr = &gN64MemoryManager;
    u8 hdr[12];
    u8 *tmp, *buf;
    u32 handle, next;
    u8 *packed, *unpacked;
    s32 i;

    tmp = mpLinkedMemoryManager_AllocPtr(mgr, 12, 0);
    RomCopy(tmp, romAddr, 12);
    bcopy(tmp, hdr, 12);
    mpLinkedMemoryManager_FreePtr(mgr, tmp);
    if ((s8)hdr[2] == 0) {
        buf = mpLinkedMemoryManager_AllocPtr(mgr, READ_BE32(&hdr[8]), 0);
        RomCopy(buf, romAddr, READ_BE32(&hdr[8]));
        return buf;
    }

    /* read the packed data, then undo all but the last compression pass */
    mpLinkedMemoryManager_Pack(mgr);
    handle = mpLinkedMemoryManager_AllocHndl(mgr, READ_BE32(&hdr[8]) - 12, 0);
    RomCopy(mpLinkedMemoryManager_LockHndl(mgr, handle), romAddr + 12, READ_BE32(&hdr[8]) - 12);
    mpLinkedMemoryManager_UnlockHndl(mgr, handle);
    for (i = 0; i < (s8)hdr[2] - 1; i++) {
        packed = mpLinkedMemoryManager_LockHndl(mgr, handle);
        next = mpLinkedMemoryManager_AllocHndl(mgr, READ_BE32(packed + 8), 0);   /* unpacked size */
        unpacked = mpLinkedMemoryManager_LockHndl(mgr, next);
        mpDataDecompress(unpacked, mpLinkedMemoryManager_LockHndl(mgr, handle));
        mpLinkedMemoryManager_UnlockHndl(mgr, next);
        mpLinkedMemoryManager_FreeHndl(mgr, handle);
        handle = next;
        mpLinkedMemoryManager_Pack(mgr);
    }

    /* the last pass straight into the result, after the header */
    packed = mpLinkedMemoryManager_LockHndl(mgr, handle);
    buf = mpLinkedMemoryManager_AllocPtr(mgr, READ_BE32(packed + 8) + 12, 0);
    mpDataDecompress(buf + 12, mpLinkedMemoryManager_LockHndl(mgr, handle));
    mpLinkedMemoryManager_FreeHndl(mgr, handle);
    bcopy(hdr, buf, 12);
    return buf;
}

/* 0x80080590: Bitmap_Draw - draws bmp with its hot spot at screen x, y, clipped to clip
 * ({x0, y0, x1, y1}, NULL = the screen). flags: 1 mirror, 2 flip (clipping only), 4 the
 * top left corner at x, y instead of the hot spot, 0x20 don't load the texture (already
 * loaded), 0xF00 the palette number for colour-indexed bitmaps. */
void func_80080590(u32 **dl, N64Bitmap *bmp, s32 *clip, s32 x, s32 y, s32 flags) {
    s32 cl[4];
    s32 x0, y0, x1, y1;          /* screen rectangle */
    s32 s0, t0, s1, t1;          /* texels drawn */
    s32 dsdx, copy;
    u32 addr;
    s32 texW, w, rows, lineBytes, bandRows, band, row, ty;
    u32 tmem;
    u32 pal = 0;                 /* (the original leaves it unset for 16/32-bit bitmaps; unused then) */
    u32 timgCI4, timgCI8, tileW;
    s32 xEnd;

    if (clip != NULL) {
        cl[0] = clip[0]; cl[1] = clip[1]; cl[2] = clip[2]; cl[3] = clip[3];
        func_80092988(cl, 0, 0, B_800F5D00 - 1, B_800F5BEC - 1);
    } else {
        cl[0] = 0;
        cl[1] = 0;
        cl[2] = B_800F5D00 - 1;
        cl[3] = B_800F5BEC - 1;
    }
    s0 = 0;
    t0 = 0;
    s1 = bmp->width - 1;
    t1 = bmp->height - 1;

    if (flags & 4) {
        x0 = (flags & 1) ? -(bmp->width - 1) : 0;
        x1 = x0 - 1 + bmp->width;
        y0 = (flags & 2) ? -(bmp->height - 1) : 0;
    } else {
        x0 = (flags & 1) ? bmp->xHot + 1 - bmp->width : -bmp->xHot;
        x1 = x0 - 1 + bmp->width;
        y0 = (flags & 2) ? bmp->yHot + 1 - bmp->height : -bmp->yHot;
    }
    y1 = y0 - 1 + bmp->height;
    x0 += x; x1 += x;
    y0 += y; y1 += y;

    /* clip, moving the texels drawn with it */
    if (x0 < cl[0]) {
        if (flags & 1) s1 -= cl[0] - x0;
        else           s0 += cl[0] - x0;
        x0 = cl[0];
    }
    if (y0 < cl[1]) {
        if (flags & 2) t1 -= cl[1] - y0;
        else           t0 += cl[1] - y0;
        y0 = cl[1];
    }
    if (cl[2] < x1) {
        if (flags & 1) s0 += x1 - cl[2];
        else           s1 -= x1 - cl[2];
        x1 = cl[2];
    }
    if (cl[3] < y1) {
        if (flags & 2) t0 += y1 - cl[3];
        else           t1 -= y1 - cl[3];
        y1 = cl[3];
    }
    if (x1 < x0 || y1 < y0 || s1 < s0 || t1 < t0) return;

    /* copy mode (4 texels a clock) for unmirrored, non-32-bit bitmaps with smooth alpha */
    if ((bmp->format & 0xF) != 3 && !(flags & 1) && (bmp->format & 0x40)) {
        DL_PUSH(dl, 0xBA001402, 0x00200000);             /* copy mode */
        DL_PUSH(dl, 0xB900031D, 0);
        dsdx = 4;
        copy = 1;
    } else {
        DL_PUSH(dl, 0xBA001402, 0);                      /* 1 cycle */
        dsdx = 1;
        copy = 0;
    }

    if (bmp->format & 0x60) {
        if (bmp->format & 0x40) {
            DL_PUSH(dl, 0xB900031D, 0x00504240);
            DL_PUSH(dl, 0xB9000002, 1);                  /* alpha compare: threshold */
            DL_PUSH(dl, 0xF9000000, 0x80808001);         /* blend colour (the threshold) */
        } else {
            DL_PUSH(dl, 0xB9000002, 1);
            if ((bmp->format & 0xF) != 2) DL_PUSH(dl, 0xF9000000, 0x80808080);
            DL_PUSH(dl, 0xB900031D, 0x0C087008);
        }
    } else {
        DL_PUSH(dl, 0xB900031D, 0x0F0A4000);
        DL_PUSH(dl, 0xB9000002, 0);
    }

    addr = ((u32)bmp + bmp->dataOffset + bmp->stride * t0) & 0x1FFFFFFF;
    texW = bmp->stride << 1;                             /* texels per row in memory */
    if ((bmp->format & 0xF) != 0) texW = (s32)bmp->stride / D_800B0D60[bmp->format & 0xF];

    w = s1 - s0 + 1;
    rows = t1 - t0 + 1;
    ty = y0;
    /* bytes of a texture line, rounded up to 8 */
    if ((bmp->format & 0xF) == 0) {
        lineBytes = (w & 1) ? ((w & ~1) + 2) >> 1 : w / 2;
        if (lineBytes & 7) lineBytes = (lineBytes & ~7) + 8;
    } else {
        lineBytes = w * D_800B0D60[bmp->format & 0xF];
        if (lineBytes & 7) lineBytes = (lineBytes & ~7) + 8;
    }

    if ((bmp->format & 0xF) < 2) {
        if (bmp->format & 0x10) {                        /* load the palette */
            DL_PUSH(dl, 0xFD100000, (u32)bmp + 0x10);
            DL_PUSH(dl, 0xE8000000, 0);
            DL_PUSH(dl, 0xF5000100, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF0000000, ((u32)bmp->palCount << 14) | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        }
        pal = ((u32)flags & 0xF00) >> 8;
        DL_PUSH(dl, 0xBA000E02, 0x8000);                 /* TLUT on */
        tmem = 0x800;                                    /* (half the texture memory) */
    } else {
        DL_PUSH(dl, 0xBA000E02, 0);
        tmem = 0xF00;
    }
    bandRows = (s32)(tmem / (u32)lineBytes);

    timgCI4 = 0xFD480000 | ((((u32)(texW >> 1)) - 1) & 0xFFF);
    timgCI8 = 0xFD480000 | (((u32)texW - 1) & 0xFFF);
    tileW = (u32)texW - 1;
    pal = (pal & 0xF) << 20;
    xEnd = x1 - (copy - 1);                              /* copy mode rectangles include the end */
    for (row = 0; row < rows; row += bandRows) {
        u32 lrt;
        band = rows - row;
        if (bandRows < band) band = bandRows;
        lrt = (((u32)band - 1) << 2) & 0xFFF;
        if (!(flags & 0x20)) {
            switch (bmp->format & 0xF) {
            case 0: {                                    /* CI4, loaded as CI8 */
                u32 tw = ((((u32)((s1 - s0 + 1) >> 1) + 7) >> 3) & 0x1FF) << 9;
                DL_PUSH(dl, timgCI4, addr);
                DL_PUSH(dl, 0xF5480000 | tw, 0x07000000);
                DL_PUSH(dl, 0xE6000000, 0);
                DL_PUSH(dl, 0xF4000000 | ((((u32)s0 << 1) & 0xFFF) << 12),
                            ((((u32)s1 << 1) & 0xFFF) << 12) | lrt | 0x07000000);
                DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
                DL_PUSH(dl, 0xF5400000 | tw, pal);
                break;
            }
            case 1: {                                    /* CI8 */
                u32 tw = ((((u32)(s1 - s0 + 8)) >> 3) & 0x1FF) << 9;
                DL_PUSH(dl, timgCI8, addr);
                DL_PUSH(dl, 0xF5480000 | tw, 0x07000000);
                DL_PUSH(dl, 0xE6000000, 0);
                DL_PUSH(dl, 0xF4000000 | ((((u32)s0 << 2) & 0xFFF) << 12),
                            ((((u32)s1 << 2) & 0xFFF) << 12) | lrt | 0x07000000);
                DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
                DL_PUSH(dl, 0xF5480000 | tw, pal);
                break;
            }
            case 2: {                                    /* RGBA16 */
                u32 tw = ((((u32)((s1 - s0) * 2 + 9)) >> 3) & 0x1FF) << 9;
                DL_PUSH(dl, 0xFD100000 | (tileW & 0xFFF), addr);
                DL_PUSH(dl, 0xF5100000 | tw, 0x07000000);
                DL_PUSH(dl, 0xE6000000, 0);
                DL_PUSH(dl, 0xF4000000 | ((((u32)s0 << 2) & 0xFFF) << 12),
                            ((((u32)s1 << 2) & 0xFFF) << 12) | lrt | 0x07000000);
                DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
                DL_PUSH(dl, 0xF5100000 | tw, 0);
                break;
            }
            case 3: {                                    /* RGBA32 */
                u32 tw = ((((u32)((s1 - s0) * 2 + 9)) >> 3) & 0x1FF) << 9;
                DL_PUSH(dl, 0xFD180000 | (tileW & 0xFFF), addr);
                DL_PUSH(dl, 0xF5180000 | tw, 0x07000000);
                DL_PUSH(dl, 0xE6000000, 0);
                DL_PUSH(dl, 0xF4000000 | ((((u32)s0 << 2) & 0xFFF) << 12),
                            ((((u32)s1 << 2) & 0xFFF) << 12) | lrt | 0x07000000);
                DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
                DL_PUSH(dl, 0xF5180000 | tw, 0);
                break;
            }
            default:
                goto rect;
            }
            DL_PUSH(dl, 0xF2000000 | ((((u32)s0 << 2) & 0xFFF) << 12),
                        ((((u32)s1 << 2) & 0xFFF) << 12) | lrt);   /* tile size */
        }
    rect:
        /* texture rectangle; mirrored ones step backwards from s1 */
        DL_PUSH(dl, 0xE4000000 | ((((u32)xEnd << 2) & 0xFFF) << 12) | ((((u32)(ty + band - copy)) << 2) & 0xFFF),
                    ((((u32)x0 << 2) & 0xFFF) << 12) | ((((u32)(row + y0)) << 2) & 0xFFF));
        if (flags & 1) {
            DL_PUSH(dl, 0xB4000000, (u32)s1 << 21);
            DL_PUSH(dl, 0xB3000000, ((u32)-dsdx << 26) | 0x400);
        } else {
            DL_PUSH(dl, 0xB4000000, (u32)s0 << 21);
            DL_PUSH(dl, 0xB3000000, ((u32)dsdx << 26) | 0x400);
        }
        addr += bmp->stride * band;
        ty += band;
    }
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x8008144C: Bitmap_LoadPalette - loads a palette (count byte, then the colours at +8)
 * into the texture memory's palette half at entry 'start' */
void func_8008144C(u32 **dl, u8 *palette, s32 start) {
    DL_PUSH(dl, 0xFD100000, (u32)(palette + 8));
    DL_PUSH(dl, 0xE8000000, 0);
    DL_PUSH(dl, 0xF5000000 | (((u32)((start >> 2) + 0x100)) & 0x1FF), 0x07000000);
    DL_PUSH(dl, 0xE6000000, 0);
    DL_PUSH(dl, 0xF0000000, ((u32)palette[0] << 14) | 0x07000000);
    DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
}

/* 0x80081504: a CI8 bitmap -> a new RGBA32 one (alpha from its 1-bit alpha flag, or from its
 * run-length coded alpha channel). Other formats stop the game. */
N64Bitmap *mpDecodeBitmapTo32Bit(N64Bitmap *bmp) {
    u32 format = 0x43;
    s32 stride;
    N64Bitmap *out;
    u8 *src;
    u32 *dst;
    s32 x, y;

    if (!(bmp->format & 0x40) && bmp->alphaOffset == 0) {
        format = ((bmp->format & 0x20) ? 0x23 : 0) | 3;
    }
    if ((u16)bmp->width & 1) stride = ((s16)((u16)bmp->width & 0xFFFE) << 2) + 8;
    else                     stride = bmp->width << 2;
    out = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, bmp->height * stride + 0x10, 0);
    out->format = format;
    out->palCount = 0;
    out->dataOffset = 0x10;
    out->alphaOffset = 0;
    out->width = bmp->width;
    out->height = bmp->height;
    out->xHot = bmp->xHot;
    out->yHot = bmp->yHot;
    out->stride = stride;

    if ((bmp->format & 0xF) != 1) {
        mpFatalError("N64BMP.C : mpDecodeBitmapTo32Bit() - *** Unsupported source bitmap type");
    } else {
        src = (u8 *)bmp + bmp->dataOffset;
        dst = (u32 *)((u8 *)out + out->dataOffset);
        for (y = 0; y < bmp->height; y++) {
            for (x = 0; x < bmp->width; x++) {
                u32 c = bmp->palette[src[x]];
                u32 rgb = ((c & 0xF800) << 16) | ((c & 0x7C0) << 13) | ((c & 0x3E) << 10);
                if (bmp->format & 0x20) dst[x] = rgb | D_800B0D70[c & 1];
                else                    dst[x] = rgb | 0xFF;
            }
            src += bmp->stride;
            dst = (u32 *)((u8 *)dst + (out->stride & 0xFFFC));
        }
    }

    if (bmp->alphaOffset != 0) {
        /* runs of 4-bit codes: 1xxx = 3 more bits of a skip count, 0xxx = alpha (x+1) * 25 */
        s32 bit = 0, pixel = -1, total, skip, w;
        u8 *a = (u8 *)bmp + bmp->alphaOffset;
        u8 *pixels = (u8 *)out + out->dataOffset;
        u32 code, v;

        w = ((u16)out->width & 1) ? (s16)((u16)out->width & 0xFFFE) + 2 : out->width;
        total = out->height * w;
        for (;;) {
            skip = 0;
            for (;;) {
                if (bit < 5) v = (u32)a[0] >> bit;
                else         v = ((u32)a[0] >> bit) | ((u32)a[1] << (8 - bit));
                code = v & 0xF;
                bit += 4;
                if (bit >= 8) {
                    bit -= 8;
                    a++;
                }
                if (!(code & 8)) break;
                skip = (skip << 3) | (code & 7);
            }
            pixel = pixel + 1 + skip;
            if (pixel >= total) break;
#ifdef TARGET_PORT
            /* (the alpha is the word's low byte: the 4th byte on the N64, the 1st here) */
            ((u32 *)pixels)[pixel] = (((u32 *)pixels)[pixel] & ~0xFFu) | (u32)((code + 1) * 25);
#else
            pixels[pixel * 4 + 3] = (code + 1) * 25;
#endif
        }
    }
    return out;
}

/* 0x80081804: Screen_Grab - a copy of the screen as a new RGBA16 bitmap (every pixel opaque).
 * Returns the N64Bitmap * (combat.h declares it returning s32). */
s32 func_80081804(void) {
    N64Bitmap *b;
    u32 *px;
    s32 i, n;

    b = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, B_800F5D00 * B_800F5BEC * 2 + 0x10, 0);
    b->format = 2;
    b->palCount = 0;
    b->dataOffset = 0x10;
    b->alphaOffset = 0;
    b->width = (s16)B_800F5D00;
    b->height = (s16)B_800F5BEC;
    b->xHot = 0;
    b->yHot = 0;
    b->stride = (u16)(B_800F5D00 << 1);
    bcopy(func_80098070(), (u8 *)b + 0x10, B_800F5D00 * B_800F5BEC * 2);
    n = B_800F5D00 * B_800F5BEC / 2;
    px = (u32 *)((u8 *)b + 0x10);
    for (i = 0; i < n; i++) px[i] |= 0x00010001;
    return (s32)b;
}

/* the tile settings func_80081924 uses: clamp (cm 2) and masks */
static u32 Tile_W1(N64Bitmap *bmp, u32 cmt, u32 cms, u32 noClampMaskT, u32 noClampMaskS) {
    u32 w = (cmt & 3) << 18;
    u32 s = (cms & 3) << 8;
    if (cmt != 0) w |= (((u32)(bmp->height - 1) & 0xF) << 14) | s;
    else          w |= s | noClampMaskT;
    if (cms != 0) return w | (((u32)(bmp->width - 1) & 0xF) << 4);
    return w | noClampMaskS;
}

/* 0x80081924: Bitmap_LoadTexture - the whole bitmap as tile 0 for 3D drawing; mode bit 0
 * clear: clamp at the edges */
void func_80081924(u32 **dl, N64Bitmap *bmp, s32 mode) {
    u32 cms = ((mode & 1) == 0) << 1;
    u32 cmt = ((mode & 1) == 0) << 1;
    u32 tw, maskT;

    if ((bmp->format & 0xF) < 2) {
        if (bmp->format & 0x10) {
            DL_PUSH(dl, 0xFD100000, (u32)bmp + 0x10);
            DL_PUSH(dl, 0xE8000000, 0);
            DL_PUSH(dl, 0xF5000100, 0x07000000);
            DL_PUSH(dl, 0xE6000000, 0);
            DL_PUSH(dl, 0xF0000000, ((u32)bmp->palCount << 14) | 0x07000000);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        }
        DL_PUSH(dl, 0xBA000E02, 0x8000);
    }
    switch (bmp->format & 0xF) {
    case 0:                                              /* CI4 (loaded as CI8) */
        tw = ((((u32)((s16)bmp->width >> 1) + 7) >> 3) & 0x1FF) << 9;
        maskT = 0x18000;
        DL_PUSH(dl, 0xFD480000 | (((u32)(((s16)bmp->width >> 1) - 1)) & 0xFFF), (u32)bmp + bmp->dataOffset);
        DL_PUSH(dl, 0xF5480000 | tw, 0x07000000 | Tile_W1(bmp, cmt, cms, maskT, 0x60));
        DL_PUSH(dl, 0xE6000000, 0);
        DL_PUSH(dl, 0xF4000000, ((((u32)(bmp->width - 1) << 1) & 0xFFF) << 12)
                                | ((((u32)(bmp->height - 1)) << 2) & 0xFFF) | 0x07000000);
        DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        DL_PUSH(dl, 0xF5400000 | tw, Tile_W1(bmp, cmt, cms, maskT, 0x60));
        break;
    case 1:                                              /* CI8 */
        tw = ((((u32)(bmp->width + 7)) >> 3) & 0x1FF) << 9;
        maskT = 0x14000;
        DL_PUSH(dl, 0xFD480000 | (((u32)(bmp->width - 1)) & 0xFFF), (u32)bmp + bmp->dataOffset);
        DL_PUSH(dl, 0xF5480000 | tw, 0x07000000 | Tile_W1(bmp, cmt, cms, maskT, 0x60));
        DL_PUSH(dl, 0xE6000000, 0);
        DL_PUSH(dl, 0xF4000000, ((((u32)(bmp->width - 1) << 2) & 0xFFF) << 12)
                                | ((((u32)(bmp->height - 1)) << 2) & 0xFFF) | 0x07000000);
        DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        DL_PUSH(dl, 0xF5480000 | tw, Tile_W1(bmp, cmt, cms, maskT, 0x60));
        break;
    case 2:                                              /* RGBA16 */
        tw = ((((u32)(bmp->width * 2 + 7)) >> 3) & 0x1FF) << 9;
        maskT = 0x14000;
        DL_PUSH(dl, 0xFD100000 | (((u32)(bmp->width - 1)) & 0xFFF), (u32)bmp + bmp->dataOffset);
        DL_PUSH(dl, 0xF5100000 | tw, 0x07000000 | Tile_W1(bmp, cmt, cms, maskT, 0x60));
        DL_PUSH(dl, 0xE6000000, 0);
        DL_PUSH(dl, 0xF4000000, ((((u32)(bmp->width - 1) << 2) & 0xFFF) << 12)
                                | ((((u32)(bmp->height - 1)) << 2) & 0xFFF) | 0x07000000);
        DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        DL_PUSH(dl, 0xF5100000 | tw, Tile_W1(bmp, cmt, cms, maskT, 0x60));
        break;
    case 3:                                              /* RGBA32 */
        tw = ((((u32)(bmp->width * 2 + 7)) >> 3) & 0x1FF) << 9;
        maskT = 0x14000;
        DL_PUSH(dl, 0xFD180000 | (((u32)(bmp->width - 1)) & 0xFFF), (u32)bmp + bmp->dataOffset);
        DL_PUSH(dl, 0xF5180000 | tw, 0x07000000 | Tile_W1(bmp, cmt, cms, maskT, 0x50));
        DL_PUSH(dl, 0xE6000000, 0);
        DL_PUSH(dl, 0xF4000000, ((((u32)(bmp->width - 1) << 2) & 0xFFF) << 12)
                                | ((((u32)(bmp->height - 1)) << 2) & 0xFFF) | 0x07000000);
        DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        DL_PUSH(dl, 0xF5180000 | tw, Tile_W1(bmp, cmt, cms, maskT, 0x50));
        break;
    default:
        return;
    }
    DL_PUSH(dl, 0xF2000000, ((((u32)(bmp->width - 1) << 2) & 0xFFF) << 12)
                            | ((((u32)(bmp->height - 1)) << 2) & 0xFFF));
}
