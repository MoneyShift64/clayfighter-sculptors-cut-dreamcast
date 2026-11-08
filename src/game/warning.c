/*
 * (the port) WARNING.C: a screen shown once at power-on, before the licence screen: this port
 * is free, and whoever sold it to someone sold what isn't theirs. Drawn with the game's own
 * fonts (from the ROM, as the other screens load them), A or Start to go on once it has
 * faded in; it goes on by itself after a while.
 *
 * Warning_Draw is the logic (every tick), Warning_Frame the picture, as LICENSE.C's.
 * Off for now: PORT_WARNING_SCREEN (game.c) 1 shows it.
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"
#include "n64gfx.h"

void func_80082358(Font *font, u32 **dl, const char *s, s32 *clip, s32 x, s32 y, s32 flags);

#ifdef TARGET_PORT

typedef struct WarningScreen {
    u8 *file[3];                           /* the fonts' files: big, menu, text */
    Font *big, *menu, *text;
    s32 skipped, ending, result, tick;
    u32 lastTick[2];
    char lines[12][64];                    /* the paragraph, broken into lines */
    s32 nLines;
} WarningScreen;

static WarningScreen *sWarning;

#define SCREEN_W 320
#define SCREEN_H 240
#define TEXT_W   304
#define MIN_TICKS 60                       /* (a second) before A / Start goes on */
#define MAX_TICKS 900                      /* (15 seconds) before it goes on by itself */

static const char sPara[] =
    "This is a non-commercial preservation project which is available to download freely from "
    "GitHub. No game data is included with the download. You are required to supply your own "
    "ROM data to build the Dreamcast port.";

static s32 FadeOver(void) {                /* finished, or no fade at all */
    ScreenFade *f = &gScreenFade;
    s32 done = (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
    return done || f->active == 0;
}

static Font *FontOf(u8 *file) { return (Font *)(file + *(s32 *)(file + 0xC)); }

/* the big font's apostrophe sits on the line, like a comma (the game never uses it): up to
   the top of the capitals */
static void RaiseApostrophe(Font *f) {
    FontGlyph *g = (FontGlyph *)((u8 *)f + f->glyphsOffset);
    s32 c = '\'';
    if (c < f->firstChar || c > f->lastChar || g[c - f->firstChar].offset == 0) return;
    *(s16 *)((u8 *)f + g[c - f->firstChar].offset + 0xC) += 22;
}

/* the paragraph into lines no wider than TEXT_W */
static void Wrap(WarningScreen *w) {
    char cur[64], test[64];
    const char *p = sPara;
    s32 n = 0;

    cur[0] = 0;
    while (*p) {
        char word[48];
        s32 k = 0;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && k < 47) word[k++] = *p++;
        word[k] = 0;
        if (k == 0) break;
        if (cur[0]) { mpSPrintf(test, "%s %s", cur, word); }
        else { mpSPrintf(test, "%s", word); }
        if (cur[0] && func_80082298(w->text, test) > TEXT_W) {
            if (n < 12) { mpSPrintf(w->lines[n], "%s", cur); n++; }
            mpSPrintf(cur, "%s", word);
        } else {
            mpSPrintf(cur, "%s", test);
        }
    }
    if (cur[0] && n < 12) { mpSPrintf(w->lines[n], "%s", cur); n++; }
    w->nLines = n;
}

void Warning_Init(void) {
    WarningScreen *w;

    if (sWarning != NULL) return;
    sWarning = w = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(WarningScreen), 0);
    bzero(w, sizeof(WarningScreen));
    w->file[0] = func_80080360(0xF4D8B0);
    w->file[1] = func_80080360(0xF54510);
    w->file[2] = func_80080360(0xF56270);
    w->big = FontOf(w->file[0]);
    w->menu = FontOf(w->file[1]);
    w->text = FontOf(w->file[2]);
    RaiseApostrophe(w->big);
    Wrap(w);
    w->result = -1;
    func_8002A340(&gScreenFade, 1);         /* fade in */
    w->lastTick[0] = D_800B09B0;
    w->lastTick[1] = D_800B09B4;
}

void Warning_Shutdown(void) {
    s32 i;
    if (sWarning == NULL) return;
    for (i = 0; i < 3; i++) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sWarning->file[i]);
    func_8002AC24(&gScreenFade);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sWarning);
    sWarning = NULL;
}

/* the logic, once a tick */
s32 Warning_Draw(void) {
    WarningScreen *w = sWarning;
    f32 level;

    if (w == NULL) return 0;
    if ((B_800F77D8 & 0x9000) && !w->skipped && !w->ending && w->tick >= MIN_TICKS && FadeOver()) {
        level = gScreenFade.level;          /* A / Start: fade out from where it is */
        func_8002A990(&gScreenFade, 1);
        gScreenFade.level = level;
        w->skipped = 1;
    }
    if (++w->tick >= MAX_TICKS && FadeOver() && !w->skipped && !w->ending) {
        func_8002A990(&gScreenFade, 1);
        w->ending = 1;
    }
    if ((w->skipped || w->ending) && FadeOver()) w->result = 0;
    func_8002AA14(&gScreenFade, 0.03f);
    if (gScreenFade.active != 0 && (f64)gScreenFade.level == 1.0) func_8002AC24(&gScreenFade);
    return w->result;
}

static void SetColours(const u8 *c) {      /* (as CONTINUE.C's: the lit letters' colours) */
    B_800F7D94 = c[0]; B_800F7778 = c[0];
    B_800F7789 = c[1]; B_800F7770 = c[1];
    B_800F7E78 = c[2]; B_800F4E34 = c[2];
    B_800F7788 = c[3]; B_800F7E70 = c[3];
    B_800F88BC = c[4]; B_800F88C8 = c[4];
    B_800F7ECC = c[5]; B_800F2D1C = c[5];
}

static void SetTexFilter(u32 mode) { DL_PUSH(&gDL, 0xBA000C02, mode); }

static void Centred(Font *f, s32 pal, const char *s, s32 y) {
    func_80082210(f, &gDL, pal);
    func_80082358(f, &gDL, s, 0, (SCREEN_W - func_80082298(f, s)) / 2, y, 0);
}

/* the picture, once a frame */
s32 Warning_Frame(void) {
    WarningScreen *w = sWarning;
    s32 i;

    if (w == NULL) return 0;
    while (w->lastTick[0] == D_800B09B0 && w->lastTick[1] == D_800B09B4) {
        PORT_SPIN();
    }                                       /* one frame per retrace */
    w->lastTick[0] = D_800B09B0;
    w->lastTick[1] = D_800B09B4;
    func_8007E860(&gDL);
    func_8007E720(&gDL, 10, 10, 36);        /* a dark blue */
    func_8007EB48(&gDL);
    Centred(w->menu, 0, "IF YOU BOUGHT THIS GAME FROM", 20);
    Centred(w->menu, 0, "EBAY OR VINTED, ETC...", 35);
    Centred(w->big, 1, "YOU'VE BEEN", 74);
    Centred(w->big, 1, "RIPPED OFF!", 106);
    {   /* in red: the menu font's palettes are white, coloured as it is drawn (in 3D, as the
           Continue screen's words; the 2D letters are copied as they are) */
        static const u8 red[6] = { 255, 40, 40, 255, 0, 0 };
        f32 up[3] = { 0.0f, 1.0f, 0.0f }, right[3] = { -1.0f, 0.0f, 0.0f };
        const char *s = "REPORT THE SELLER!";
        func_8007E9DC(&gDL);
        func_8007EEF0(&gDL);
        func_8007F94C(&gDL);
        func_8007FB08(&gDL, 0x204);
        func_8007FB90(&gDL, 0xC084000, 0x3024000);
        func_8007FBE8(&gDL, 1);
        SetTexFilter(0);
        func_80082210(w->menu, &gDL, 0);
        SetColours(red);
        func_80082518(w->menu, &gDL, s, (f32)(-func_80082298(w->menu, s) / 2), (f32)(SCREEN_H / 2 - 125), 0, up, right, 8);
        SetTexFilter(0x2000);
        func_8007EB48(&gDL);
    }
    for (i = 0; i < w->nLines; i++) Centred(w->text, 0, w->lines[i], 143 + i * 12);
    func_8002AC80(&gScreenFade, &gDL);
    return w->result;
}

#endif
