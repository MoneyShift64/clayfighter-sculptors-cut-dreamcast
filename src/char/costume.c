/*
 * Costume colour functions (0x800048C0-0x80005830), one per fighter and costume,
 * listed in the table D_CharacterColourVariants (0x800AA770). Each adjusts one
 * colour given as hue (degrees), saturation and lightness.
 *
 * Literals ending in 'f' are single-precision steps in the original; the rest are
 * done in double precision and rounded back to float on store.
 */
#include "char.h"

#define IN_RANGE(x, lo, hi) ((lo) < (x) && (x) < (hi))

static f32 AbsWeight(f32 w) {
    if ((f64)w <= 0.0) {
        w = (f32)(0.0 - (f64)w);
    }
    return w;
}

/* 0x800048C0: Frosty, costume 0 - darkens greens and turns them blue */
void Costume_Frosty_0(HLS *c) {
    f32 w = AbsWeight(HueWeight(c->h, 120.0f, 60.0f));

    c->l = c->l * (1.0 - w * 0.3);
    if (IN_RANGE((f64)c->h, 60.0, 180.0)) {
        c->h = c->h + 125.0;
    }
}

/* 0x80004998 */
void Costume_Frosty_1(HLS *c) {
    f32 w = HueWeight(c->h, 120.0f, 60.0f);

    c->l = c->l * (1.0 - w * 0.4);
}

/* 0x800049FC */
void Costume_Blob_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 60.0, 180.0)) {
        c->h = c->h + 120.0;
    }
}

/* 0x80004A50 */
void Costume_Bonker_1(HLS *c) {
    f32 w = AbsWeight(HueWeight(c->h, 40.0f, 40.0f));

    c->h = c->h - w * 40.0;
    c->l = c->l - w * 0.05;
}

/* 0x80004AF8 */
void Costume_BoogerMan_1(HLS *c) {
    f32 w = AbsWeight(HueWeight(c->h, 120.0f, 60.0f));

    c->s = c->s * (w * 0.15 + 1.0);
    if ((f64)c->s > 1.0) {
        c->s = 1.0f;
    }
    if (IN_RANGE((f64)c->h, 70.0, 180.0)) {
        c->h = c->h - 76.0;
    }
}

/* 0x80004BF8 */
void Costume_DrKiln_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 120.0, 240.0)) {
        c->h = c->h + 150.0;
    }
    c->h = c->h + 20.0f;
}

/* 0x80004C60 */
void Costume_EarthwormJim_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 180.0, 300.0)) {
        c->h = c->h + 135.0;
    }
}

/* 0x80004CB4 */
void Costume_HighFive_1(HLS *c) {
    c->l = c->l - 0.2;
    if ((f64)c->l < 0.0) {
        c->l = 0.0f;
    }
    c->h = c->h - 2.0;
    c->s = c->s * 0.8;
}

/* 0x80004D30 */
void Costume_NullChar_1(HLS *c) {
    f32 w = AbsWeight(HueWeight(c->h, 240.0f, 40.0f));

    c->h = c->h + w * 120.0;
    c->l = c->l - w * 0.05;
    if ((f64)c->l < 0.0) {
        c->l = 0.0f;
    }
    c->h = c->h - 10.0f;
    if (IN_RANGE((f64)c->h, 100.0, 140.0)) {
        c->h = c->h + 120.0;
    }
    c->l = c->l * 0.7;
}

/* 0x80004E70 */
void Costume_Houngan_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 50.0, 180.0)) {
        c->h = c->h + 140.0;
    }
    if (IN_RANGE((f64)c->h, 20.0, 50.0)) {
        c->h = c->h - 20.0;
    }
}

/* 0x80004F08 */
void Costume_Ickybod_1(HLS *c) {
    if (!IN_RANGE((f64)c->h, 0.0, 90.0) && !((f64)c->h > 330.0)) {
        c->l = c->l * 0.7;
        c->s = c->s * 1.2;
        if ((f64)c->s > 1.0) {
            c->s = 1.0f;
        }
    }
    if (IN_RANGE((f64)c->h, 200.0, 360.0)) {
        c->h = c->h + 90.0f;
    }
}

/* the second half of both Kung Pow costumes */
#define KUNGPOW_GREENS(c)                       \
    if (IN_RANGE((f64)(c)->h, 90.0, 180.0)) {   \
        (c)->h = (c)->h + 60.0f;                \
        (c)->s = (c)->s * 0.08;                 \
        (c)->l = (c)->l - 0.35;                 \
        if ((f64)(c)->l < 0.0) {                \
            (c)->l = 0.0f;                      \
        }                                       \
        (c)->l = (c)->l * 1.8;                  \
        if ((f64)(c)->l > 1.0) {                \
            (c)->l = 1.0f;                      \
        }                                       \
    }

/* 0x8000501C */
void Costume_KungPow_0(HLS *c) {
    if (IN_RANGE((f64)c->h, 180.0, 270.0)) {
        c->h = c->h + 220.0;
        c->s = c->s * 0.1;
        c->l = c->l * 1.4;
        if ((f64)c->l > 1.0) {
            c->l = 1.0f;
        }
    }
    KUNGPOW_GREENS(c)
}

/* 0x800051C4 */
void Costume_KungPow_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 200.0, 250.0)) {
        c->h = c->h + 137.0;
        c->l = c->l * 0.55;
        c->s = c->s * 1.7;
        if ((f64)c->s >= 1.0) {
            c->s = 1.0f;
        }
    }
    KUNGPOW_GREENS(c)
}

/* 0x8000536C */
void Costume_LadyLiberty_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 80.0, 240.0)) {
        c->h = c->h + 90.0;
        c->s = c->s * 0.6;
        c->l = c->l * (c->l + 0.4);
        c->l = c->l + 0.1;
        if (1.0 - c->s < (f64)c->l) {
            c->l = 1.0 - c->s;
        }
    }
}

/* 0x80005440 */
void Costume_Lockjaw_1(HLS *c) {
    if ((f64)c->s > 0.1) {
        c->l = c->l * 0.6;
        c->s = c->s * 0.7;
    } else {
        c->l = c->l * 1.2;
        if (1.0 - c->s < (f64)c->l) {
            c->l = 1.0 - c->s;
        }
    }
    c->h = c->h + 7.0;
}

/* 0x80005508 */
void Costume_SumoSanta_1(HLS *c) {
    if ((f64)c->h > 300.0 || ((f64)c->h < 60.0 && (f64)c->s > 0.65)) {
        c->h = c->h + 60.0;
    } else if (IN_RANGE((f64)c->h, 90.0, 180.0)) {
        c->h = c->h - 60.0;
    }
    c->s = c->s * 1.1;
    if ((f64)c->s > 1.0) {
        c->s = 1.0f;
    }
    c->l = c->l * 0.9;
}

/* 0x8000562C */
void Costume_Taffy_1(HLS *c) {
    if (IN_RANGE((f64)c->h, 30.0, 90.0) && (f64)c->s > 0.4) {
        c->h = c->h - 60.0;
        c->s = c->s * 0.8;
        c->l = c->l * 1.1;
        if ((f64)c->l > 1.0) {
            c->l = 1.0f;
        }
    }
}

/* 0x80005704 */
void Costume_THoppy_1(HLS *c) {
    if ((((f64)c->h >= 0.0 && (f64)c->h < 90.0) || c->h > 330.0f) && (f64)c->s > 0.15) {
        c->l = c->l * 0.85;
        c->h = c->h - 30.0;
    }
    if (IN_RANGE((f64)c->h, 90.0, 180.0)) {
        c->h = c->h - 120.0;
    }
}

/* 0x8000580C */
void Costume_Zappa_1(HLS *c) {
    c->h = c->h - 40.0f;
}
