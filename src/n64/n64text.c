/*
 * Text (0x80082210-0x80082780): fonts are an N64 font header (Font, n64gfx.h), palettes and
 * a glyph table of N64Bitmaps. Letters without a glyph are as wide as a space, a tab is three
 * spaces; every letter is followed by the font's spacing (text width: except the last).
 *
 * A line is drawn either in pixels (func_80082358, 2D bitmaps) or in 3D: func_800825B0 walks
 * along the 'right' vector (backwards: x -= width * right) and draws each letter with one of
 * the three sprite drawers: func_800824CC (func_80083A64), func_80082518 (func_80084CB0) or
 * func_80082564 (func_80085FE4).
 */
#include "n64gfx.h"

#define GLYPHS(f) ((FontGlyph *)((u8 *)(f) + (f)->glyphsOffset))

/* 0x80082210: Font_SetPalette - loads palette number pal of the font */
void func_80082210(void *fontp, u32 **dl, s32 pal) {
    Font *font = fontp;
    u32 off;

    if (pal < font->numPalettes) {
        off = ((u32 *)((u8 *)font + font->palettesOffset))[pal];
        DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
        func_8008144C(dl, (u8 *)font + off, 0);
        DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
    }
}

/* 0x80082298: Font_TextWidth - in pixels */
s32 func_80082298(void *fontp, const char *s) {
    Font *font = fontp;
    FontGlyph *glyphs = GLYPHS(font);
    s32 w = 0;
    s32 i = 0;
    u32 c;

    for (c = (u8)s[0]; c != 0; c = (u8)s[i]) {
        if ((s32)c >= font->firstChar && (s32)c <= font->lastChar && glyphs[c - font->firstChar].offset != 0) {
            w += glyphs[c - font->firstChar].width;
            if (s[i + 1] != 0) w += font->spacing;
        } else if (c == '\t') {
            w += font->spaceWidth * 3;
        } else {
            w += font->spaceWidth;
        }
        i++;
    }
    return w;
}

/* 0x80082358: Font_DrawText2D - text at pixel x, y (the letters' hot spots), clipped to
 * clip; flags as for func_80080590 */
void func_80082358(Font *font, u32 **dl, const char *s, s32 *clip, s32 x, s32 y, s32 flags) {
    FontGlyph *glyphs = GLYPHS(font);
    s32 i = 0;
    u32 c;

    for (c = (u8)s[0]; c != 0; c = (u8)s[i]) {
        if ((s32)c >= font->firstChar && (s32)c <= font->lastChar && glyphs[c - font->firstChar].offset != 0) {
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            func_80080590(dl, (N64Bitmap *)((u8 *)font + glyphs[c - font->firstChar].offset), clip, x, y, flags);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            x += glyphs[c - font->firstChar].width + font->spacing;
        } else if (c == '\t') {
            x += font->spaceWidth * 2 + font->spaceWidth;
        } else {
            x += font->spaceWidth;
        }
        i++;
    }
}

/* 0x800825B0: Font_DrawText3D - text from x, y, z along -right, each letter drawn with draw */
void func_800825B0(Font *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                   s32 flags, GlyphDrawFn draw) {
    FontGlyph *glyphs = GLYPHS(font);
    s32 i = 0;
    s32 adv;
    f32 a;
    u32 c;

    for (c = (u8)s[0]; c != 0; c = (u8)s[i]) {
        if ((s32)c >= font->firstChar && (s32)c <= font->lastChar && glyphs[c - font->firstChar].offset != 0) {
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            draw(dl, (u8 *)font + glyphs[c - font->firstChar].offset, x, y, z, up, right, flags);
            DL_PUSH(dl, G_RDPPIPESYNC_W0, 0);
            adv = glyphs[c - font->firstChar].width + font->spacing;
        } else if (c == '\t') {
            adv = font->spaceWidth * 2 + font->spaceWidth;
        } else {
            adv = font->spaceWidth;
        }
        a = (f32)adv;
        x = x - a * right[0];
        y = y - a * right[1];
        i++;
        z = z - a * right[2];
    }
}

/* 0x800824CC: Font_DrawText (letters with func_80083A64) */
void func_800824CC(void *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    func_800825B0(font, dl, s, x, y, z, up, right, flags, (GlyphDrawFn)func_80083A64);
}

/* 0x80082518: Font_DrawTextLit (letters with func_80084CB0). combat.h declares the z
 * argument as an s32 (callers pass 0); it is the float z, passed on unchanged. */
void func_80082518(void *font, u32 **dl, const char *s, f32 x, f32 y, s32 z, f32 *up, f32 *right, s32 flags) {
    union { s32 i; f32 f; } zz;
    zz.i = z;
    func_800825B0(font, dl, s, x, y, zz.f, up, right, flags, (GlyphDrawFn)func_80084CB0);
}

/* 0x80082564: Font_DrawTextSeeThrough (letters with func_80085FE4) */
void func_80082564(void *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags) {
    func_800825B0(font, dl, s, x, y, z, up, right, flags, (GlyphDrawFn)func_80085FE4);
}
