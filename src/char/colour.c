/*
 * Colour conversion (original object 0x80006090-0x80006A00).
 * Costumes change a fighter's colours by converting each palette entry from RGB to
 * hue/lightness/saturation, running a per-costume function on it, and converting back.
 *
 * The original mixes single and double precision; the casts below keep every
 * rounding step where the game has it, so the results are bit-identical.
 */
#include "char.h"

void *func_80080360(u32 assetId);   /* loads a shared asset (still in assembly) */

/* 0x80006090 */
void RecolourPalette(u16 *colours, s32 count, CostumeFunc func) {
    s32 i;
    f32 rgb[3];
    HLS hls;

    for (i = 0; i < count; i++) {
        u16 *c = &colours[i];
        f64 t;
        u32 r, g, b;

        if (!(*c & 1)) {
            continue;   /* transparent */
        }
        rgb[0] = (f32)((f64)(f32)(s32)((*c & 0xF800) >> 8) / 255.0);
        rgb[1] = (f32)((f64)(f32)(s32)((*c & 0x07C0) >> 3) / 255.0);
        rgb[2] = (f32)((f64)(f32)(s32)((*c & 0x003E) << 2) / 255.0);
        RGBtoHLS(rgb, &hls);
        func(&hls);
        HLStoRGB(&hls, rgb);
        t = (f64)rgb[0] * 32.0;
        r = F64_TO_U32(t);
        t = (f64)rgb[1] * 32.0;
        g = F64_TO_U32(t);
        t = (f64)rgb[2] * 32.0;
        b = F64_TO_U32(t);
        *c = (r << 11) | (g << 6) | (b << 1) | 1;
    }
}

/*
 * 0x800062C8: how close 'hue' is to 'centre', from +1 (on it) falling to 0 at
 * 'width' degrees away. Negative when the hue is below the centre.
 */
f32 HueWeight(f32 hue, f32 centre, f32 width) {
    f32 d;

    if (hue > centre) {
        do {
            centre = (f32)((f64)centre + 360.0);
        } while (centre < hue);
    }
    d = centre - hue;
    if ((f64)d > 180.0) {
        d = -(f32)(360.0 - (f64)d);
    }
    if ((f64)d >= 0.0) {
        if ((f64)width < (f64)d) {
            return 0.0f;
        }
    } else {
        if ((f64)width < -(f64)d) {
            return 0.0f;
        }
    }
    if ((f64)d < 0.0) {
        return (f32)(-1.0 - (f64)(d / width));
    }
    return (f32)(1.0 - (f64)(d / width));
}

/* 0x800063EC: rgb in 0-1; out = hue (0-360), saturation, lightness. */
void RGBtoHLS(f32 *rgb, HLS *out) {
    f32 mn, mx, l, s, h;
    f32 rc, gc, bc;

    if (rgb[0] <= rgb[1] && rgb[0] <= rgb[2]) {
        mn = rgb[0];
    } else if (rgb[1] <= rgb[0] && rgb[1] <= rgb[2]) {
        mn = rgb[1];
    } else {
        mn = rgb[2];
    }
    if (rgb[1] <= rgb[0] && rgb[2] <= rgb[0]) {
        mx = rgb[0];
    } else if (rgb[0] <= rgb[1] && rgb[2] <= rgb[1]) {
        mx = rgb[1];
    } else {
        mx = rgb[2];
    }

    l = (f32)((f64)(mn + mx) / 2.0);
    if (mn == mx) {
        out->h = 0.0f;
        out->s = 0.0f;
        out->l = l;
        return;
    }
    if ((f64)l <= 0.5) {
        s = (mx - mn) / (mx + mn);
    } else {
        s = (f32)((f64)(mx - mn) / ((2.0 - (f64)mx) - (f64)mn));
    }
    rc = (mx - rgb[0]) / (mx - mn);
    gc = (mx - rgb[1]) / (mx - mn);
    bc = (mx - rgb[2]) / (mx - mn);
    if (rgb[0] == mx) {
        h = bc - gc;
    } else if (rgb[1] == mx) {
        h = (rc + 2.0f) - bc;
    } else {
        h = (gc + 4.0f) - rc;
    }
    h = (f32)((f64)h * 60.0);
    if ((f64)h < 0.0) {
        h = (f32)((f64)h + 360.0);
    }
    out->h = h;
    out->s = s;
    out->l = l;
}

/* 0x80006678: one RGB channel of the HLS -> RGB conversion. */
f32 HLSValue(f32 n1, f32 n2, f32 hue) {
    if ((f64)hue > 360.0) {
        do {
            hue = (f32)((f64)hue - 360.0);
        } while ((f64)hue > 360.0);
    }
    if ((f64)hue < 0.0) {
        do {
            hue = (f32)((f64)hue + 360.0);
        } while ((f64)hue < 0.0);
    }
    if ((f64)hue < 60.0) {
        return (f32)((f64)n1 + (f64)(n2 - n1) * ((f64)hue / 60.0));
    }
    if ((f64)hue < 180.0) {
        return n2;
    }
    if ((f64)hue < 240.0) {
        return (f32)((f64)n1 + (f64)(n2 - n1) * ((240.0 - (f64)hue) / 60.0));
    }
    return n1;
}

/* 0x800067D0 */
void HLStoRGB(HLS *in, f32 *rgb) {
    f32 r, g, b, m1, m2;

    if ((f64)in->s == 0.0) {
        r = g = b = in->l;
    } else {
        if ((f64)in->l <= 0.5) {
            m2 = (f32)((f64)in->s * (f64)in->l + (f64)in->l);
        } else {
            m2 = (in->l + in->s) - in->l * in->s;
        }
        m1 = (f32)(((f64)in->l + (f64)in->l) - (f64)m2);
        r = HLSValue(m1, m2, (f32)((f64)in->h + 120.0));
        g = HLSValue(m1, m2, in->h);
        b = HLSValue(m1, m2, (f32)((f64)in->h - 120.0));
    }
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}

/* 0x80006930: loads a character's portrait and recolours it for the costume. */
void *LoadCharacterPortrait(s32 slot, s32 costume) {
    u8 *p = func_80080360(D_800AAFF0[slot]);
    u8 *a = p + *(u32 *)(p + 0x0C);
    u8 *b = p + *(u32 *)(p + 0x14);

    *(u16 *)(a + 0x0C) = D_800AB034[slot][1];
    *(u16 *)(a + 0x0A) = D_800AB034[slot][0];
    ApplyCostumeColours((u16 *)(a + 0x10), a[1] + 1, slot, costume);
    /* the second image uses the first one's colour count */
    ApplyCostumeColours((u16 *)(b + 0x10), a[1] + 1, slot, costume);
    return p;
}
