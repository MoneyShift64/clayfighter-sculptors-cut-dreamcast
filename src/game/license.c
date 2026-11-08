/*
 * LICENSE.C (0x80048150-0x80048820): the licence screen shown at power-on - the licence
 * picture and the animated logo (the text/logo code at 0x8002C9A0), A or Start to skip it
 * once it has faded in.
 *
 * License_Draw is the logic (every tick), License_Frame the picture.
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"

typedef struct LicenseScreen {             /* 0x1970 bytes */
    /* 0x0000 */ u8 logo[0x194C];          /* the animated logo (0x8002C9A0-0x8002D680) */
    /* 0x194C */ u8 *picture;
    /* 0x1950 */ s32 skipped;
    /* 0x1954 */ s32 ending;
    /* 0x1958 */ s32 result;               /* -1 while running; 0 at the end, 1 skipped */
    /* 0x195C */ s32 tick;
    /* 0x1960 */ s32 noFadeIn;
    /* 0x1964 */ s32 pad;
    /* 0x1968 */ u32 lastTick[2];
} LicenseScreen;

extern LicenseScreen *D_800ACD30;
#define gLicense D_800ACD30
extern s32 B_800F8DD0;
void func_8002C9A0(void *logo);            /* the logo: set up, */
void func_8002CB44(void *logo);            /* ... free, */
void func_8002CBCC(void *logo);            /* ... update, */
void func_8002D680(void *logo);            /* ... draw */
void func_80080590(u32 **dl, void *image, s32 x, s32 y, s32 a4, s32 a5);

void License_Init(s32 noFadeIn);
void License_Shutdown(void);
s32  License_Draw(void);
s32  License_Frame(void);

static s32 FadeOver(void) {                /* finished, or no fade at all */
    ScreenFade *f = &gScreenFade;
    s32 done = (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
    return done || f->active == 0;
}

/* 0x80048150 */
void License_Init(s32 noFadeIn) {
    LicenseScreen *l;
    u32 size;
    s32 cap;

    if (gLicense != NULL) return;
    gLicense = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(LicenseScreen), 0);
    bzero(gLicense, sizeof(LicenseScreen));
    size = 0xFA9DD0 - 0xFA6AC0;
    gLicense->picture = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
    RomCopy(gLicense->picture, 0xFA6AC0, size);
    l = gLicense;
    *(u16 *)(l->picture + 0xA) = *(u16 *)(l->picture + 6) - 1;
    *(u16 *)(l->picture + 0xC) = *(u16 *)(l->picture + 8) - 1;
    l->result = -1;
    l->noFadeIn = noFadeIn;
    if (!noFadeIn) {
        cap = 0;
        if ((f64)gScreenFade.level == 1.0) cap = func_80081804();
        func_8002A340(&gScreenFade, 1);
        B_800F8DD0 = cap;
    }
    func_8002C9A0(gLicense);
    gLicense->lastTick[0] = D_800B09B0;
    gLicense->lastTick[1] = D_800B09B4;
}

/* 0x800482A8 */
void License_Shutdown(void) {
    if (gLicense == NULL) return;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gLicense->picture);
    func_8002CB44(gLicense);
    func_8002AC24(&gScreenFade);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gLicense);
    gLicense = NULL;
}

/* 0x80048310: the logic, once a tick */
s32 License_Draw(void) {
    LicenseScreen *l = gLicense;
    f32 level;

    if (l == NULL) return 0;
    if ((B_800F77D8 & 0x9000) && l->skipped == 0 && l->ending == 0 &&
        (l->tick >= 121 || (l->noFadeIn == 0 && FadeOver()))) {
        level = gScreenFade.level;                      /* skip: fade out from where it is */
        func_8002A990(&gScreenFade, 1);
        gScreenFade.level = level;
        gLicense->skipped = 1;
    }
    l = gLicense;
    if (++l->tick >= 241 && FadeOver() && gLicense->skipped == 0 && gLicense->ending == 0) {
        func_8002A990(&gScreenFade, 1);
        gLicense->ending = 1;
    }
    func_8002CBCC(gLicense);
    if (gLicense->skipped != 0 && FadeOver()) gLicense->result = 1;
    if (gLicense->ending != 0 && FadeOver()) gLicense->result = 0;
    func_8002AA14(&gScreenFade, 0.03f);
    if (gScreenFade.active != 0 && (f64)gScreenFade.level == 1.0) func_8002AC24(&gScreenFade);
    return gLicense->result;
}

/* 0x80048694: the picture, once a frame */
s32 License_Frame(void) {
    f32 m[16];

    if (gLicense == NULL) return 0;
    while (gLicense->lastTick[0] == D_800B09B0 && gLicense->lastTick[1] == D_800B09B4) {
        PORT_SPIN();
    }                                                   /* one frame per retrace */
    gLicense->lastTick[0] = D_800B09B0;
    gLicense->lastTick[1] = D_800B09B4;
    func_8007E860(&gDL);
    func_8007E720(&gDL, 0, 0, 0);
    func_8007EB48(&gDL);
    func_80080590(&gDL, gLicense->picture, 0, 300, B_800F88EC != 0 ? 0xDC : 0xCD, 0);
    func_80087EE8(m, 50.0f, -40.0f, 400.0f);
    func_8008861C(m);
    func_8008004C(&gDL, m);
    func_8007EEF0(&gDL);
    func_8002D680(gLicense);
    func_8002AC80(&gScreenFade, &gDL);
    return gLicense->result;
}
