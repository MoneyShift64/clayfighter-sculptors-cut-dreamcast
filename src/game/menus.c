/*
 * MENUS.C (0x800582F0-0x8005BF80): the title menu and the options - Game Start / VS Mode /
 * Options, the options list (volumes, difficulty, time limit, ...), the display options and
 * the two players' button set-ups side by side.
 *
 * The menus are data: a Menu is a list of MenuItems, each a label with, optionally, a value
 * (a list of names to cycle through, or a slider) and a function called when it changes.
 * Menus_Draw is the logic (Game_Render calls it every tick), Menus_Frame the picture.
 *
 * The rest of the file (0x8005BF80-0x8005C550) is the "launch" shot COMBAT.C shows when a
 * fighter is knocked clean out of the arena: the arena from below, the fighter flying up into
 * the sky, shaking, then a twinkle.
 */
#include "common.h"
#ifdef TARGET_PORT
#define PORT_NO_BUTTON_CONFIG(label) mpStrEqualNoCase((label), "Button config")
#else
#define PORT_NO_BUTTON_CONFIG(label) 0
#endif
#include "mpfplat.h"
#include "screen.h"

typedef struct MenuItem MenuItem;
typedef struct Menu Menu;

struct MenuItem {                          /* 0x24 bytes */
    /* 0x00 */ const char *label;
    /* 0x04 */ s32 font;                   /* the label's font, -1 = the menu's */
    /* 0x08 */ s32 valueFont;              /* -1 = the label's */
    /* 0x0C */ s32 y;                      /* -1 = under the one before */
    /* 0x10 */ const char **names;         /* the value's names, or NULL */
    /* 0x14 */ s32 *value;                 /* NULL = no value */
    /* 0x18 */ s32 count;                  /* the number of values (a slider's width) */
    /* 0x1C */ s32 slider;
    /* 0x20 */ void (*changed)(Menu *m, MenuItem *it, s32 value, s32 old);
};

struct Menu {                              /* 0x34 bytes */
    /* 0x00 */ s32 id;                     /* 0 title, 1 options, 2 buttons, 3 display */
    /* 0x04 */ MenuItem *items;
    /* 0x08 */ s32 count;
    /* 0x0C */ s32 font;
    /* 0x10 */ s32 x;                      /* the labels' left edge, -1 = centred */
    /* 0x14 */ s32 valueX;                 /* the values' right edge */
    /* 0x18 */ s32 y;
    /* 0x1C */ u8 colour[6], hiColour[6];  /* the labels, plain and under the cursor */
    /* 0x28 */ u8 valueColour[6], valueHiColour[6];
};

typedef struct MenuScreen {                /* 0xA0 bytes */
    /* 0x00 */ u8 *bigFontFile, *bigFont;
    /* 0x08 */ u8 *fontFile1, *font1;
    /* 0x10 */ u8 *fontFile2, *font2;
    /* 0x18 */ u8 *fontFile3, *font3;
    /* 0x20 */ u8 *background;
    /* 0x24 */ Menu *menu;
    /* 0x28 */ Menu *menu2;                /* player 2's buttons, beside player 1's */
    /* 0x2C */ s32 scrollX, scrollY;       /* the background's scroll */
    /* 0x34 */ f32 v34;
    /* 0x38 */ s32 cursor, cursor2;
    /* 0x40 */ s32 leaving;
    /* 0x44 */ s32 next;                   /* the result once faded out */
    /* 0x48 */ s32 result;
    /* 0x4C */ s32 idle;                   /* ticks without a button: the demo after 20s */
    /* 0x50 */ f32 slide;                  /* the menu sliding in, 1 -> 0 */
    /* 0x54 */ f32 wave;                   /* a new menu's letters waving, 1 -> 0 */
    /* 0x58 */ u8 stage[0x20];             /* the logo's 3D stage */
    /* 0x78 */ s32 pulse, pulse2;          /* the cursor's colour pulsing */
    /* 0x80 */ s32 delay;
    /* 0x84 */ f32 fadeSpeed;
    /* 0x88 */ s32 fadeType;
    /* 0x8C */ s32 pad8C;
    /* 0x90 */ u32 lastTick[2];
    /* 0x98 */ s32 playJingle;
    /* 0x9C */ s32 pad9C;
} MenuScreen;

extern MenuScreen *D_800AE9F4;
#define gMenus D_800AE9F4
extern s32 D_800AE9F8;                     /* the last choice on the title menu */
extern Menu D_800AE3CC, D_800AE5B8, D_800AE6D0, D_800AE98C, D_800AE9C0;
#define MENU_TITLE   D_800AE3CC
#define MENU_OPTIONS D_800AE5B8
#define MENU_DISPLAY D_800AE6D0
#define MENU_BUTTONS1 D_800AE98C
#define MENU_BUTTONS2 D_800AE9C0
extern MenuItem D_800AE74C[], D_800AE86C[];  /* the two players' buttons */
extern s32 D_800AE704[6], D_800AE71C[6];   /* ... and what each is set to */
extern void *B_800F8DD0;
extern void *B_800F7E6C;                   /* the menu sounds */
extern u8 B_800F7ECC, B_800F2D1C, B_800F7789, B_800F7770, B_800F7E78, B_800F4E34;
extern u8 B_800F88BC, B_800F88C8;
extern s32 B_800F4E54, B_800F2D18, B_800F8C84;
void func_8002DF30(void *stage);           /* the logo: set up, */
void func_8002E160(void *stage);           /* ... free, */
void func_8002E1F8(void *stage);           /* ... update, */
void func_8002E334(void *stage);           /* ... draw, */
void func_8002EA3C(void *stage);           /* ... start (from the title screen) */
void func_8002E9B0(void *stage);           /* ... and react to a new menu */
void func_80081924(u32 **dl, void *image, s32 mode);
void func_8007A8F4(s32 volume);            /* music volume */
void func_8007AA04(s32 volume);            /* sound volume */
void func_8007A530(void);
void CharMenuSound_Load(s32 bank);
void CharMenuSound_Free(void);
f32  vsSin(s32 angle);

void Menus_Init(s32 fromTitle, s32 fadeType);
void Menus_Shutdown(void);
s32  Menus_Draw(void);
s32  Menus_Frame(void);
void Menu_Input(Menu *m, s32 *cursor, u32 pressed, u32 held, s32 player, s32 *pulse);
void Menu_WaveText(u8 *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                   s32 flags, f32 wave);
void Menu_SetMusicVolume(Menu *m, MenuItem *it, s32 v, s32 old);
void Menu_SetSfxVolume(Menu *m, MenuItem *it, s32 v, s32 old);
void Menu_ResetButtons1(void);
void Menu_SwapButton1(Menu *m, MenuItem *it, s32 v, s32 old);
void Menu_ResetButtons2(void);
void Menu_SwapButton2(Menu *m, MenuItem *it, s32 v, s32 old);

static void Sound(s32 snd, f32 pitch) {
    func_800797EC(B_800F7E6C, 0, 0, snd, 0x7FFF, 0, 0, pitch, 0);
}

static s32 FadeDone(void) {
    ScreenFade *f = &gScreenFade;
    return (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
}

static void SetTexFilter(u32 mode) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xBA000C02;
    g[1] = mode;
}

static void Gfx2(u32 a, u32 b) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = a;
    g[1] = b;
}

/* 0x800582F0 */
void Menus_Init(s32 fromTitle, s32 fadeType) {
    MenuScreen *g;
    u32 size;

    if (gMenus != NULL) return;
    gMenus = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(MenuScreen), 0);
    bzero(gMenus, sizeof(MenuScreen));
    g = gMenus;
    g->bigFontFile = func_80080360(0xF4D8B0);
    gMenus->bigFont = gMenus->bigFontFile + *(s32 *)(gMenus->bigFontFile + 0xC);
    gMenus->fontFile1 = func_80080360(0xF54510);
    gMenus->font1 = gMenus->fontFile1 + *(s32 *)(gMenus->fontFile1 + 0xC);
    gMenus->fontFile2 = func_80080360(0xF53700);
    gMenus->font2 = gMenus->fontFile2 + *(s32 *)(gMenus->fontFile2 + 0xC);
    gMenus->fontFile3 = func_80080360(0xF58F00);
    gMenus->font3 = gMenus->fontFile3 + *(s32 *)(gMenus->fontFile3 + 0xC);
    size = 0xF9CF20 - 0xF9C6F0;
    gMenus->background = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
    RomCopy(gMenus->background, 0xF9C6F0, size);
    g = gMenus;
    g->scrollX = 0;
    g->scrollY = 0;
    g->v34 = 0.0f;
    g->menu = &MENU_TITLE;
    g->cursor = D_800AE9F8;
    g->slide = 1.0f;
    g->result = -1;
    g->wave = 0.0f;
    g->pulse = 0;
    g->pulse2 = 0;
    func_8002DF30(g->stage);
    if (fromTitle) {
        func_8002A340(&gScreenFade, 6);
        B_800F8DD0 = (void *)func_80081804();
        gMenus->fadeSpeed = 0.005f;
        gMenus->playJingle = 1;
        func_8002EA3C(gMenus->stage);
        gMenus->delay = 120;
    } else {
        func_8002A340(&gScreenFade, 1);
        gMenus->fadeSpeed = 0.03f;
    }
    gMenus->fadeType = fadeType;
    gMenus->lastTick[0] = D_800B09B0;
    gMenus->lastTick[1] = D_800B09B4;
    CharMenuSound_Load(0x2A);
}

/* 0x8005854C */
void Menus_Shutdown(void) {
    if (gMenus == NULL) return;
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gMenus->bigFontFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gMenus->fontFile1);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gMenus->fontFile2);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gMenus->fontFile3);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gMenus->background);
    func_8002E160(gMenus->stage);
    if (B_800F8DD0 != NULL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, B_800F8DD0);
        B_800F8DD0 = NULL;
    }
    func_8002AC24(&gScreenFade);
    CharMenuSound_Free();
    func_8007A530();
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gMenus);
    gMenus = NULL;
}

/* 0x80058654: the logic, once a tick */
s32 Menus_Draw(void) {
    MenuScreen *g = gMenus;
    ScreenFade *f = &gScreenFade;

    if (g == NULL) return 0;
    if (g->playJingle) {
        func_800797EC(B_800F7E6C, 0, 0, 2, 0x4FFB, 0, 0, 1.0f, 1);
        gMenus->playJingle = 0;
    }
    g = gMenus;
    g->scrollX++;
    g->v34 = (f32)((f64)g->v34 + 0.01);
    if (1.0 < (f64)g->v34) g->v34 = 1.0f;
    func_8002E1F8(gMenus->stage);
    if (B_800F8C78 != 0 || gMenus->menu != &MENU_TITLE) gMenus->idle = 0;
    g = gMenus;
    if (++g->idle >= 1201 && g->leaving == 0) {        /* left alone: the demo */
        g->leaving = 1;
        func_8002A990(&gScreenFade, g->fadeType != 0);
        f->rgb[2] = 0xFF;
        f->rgb[1] = 0xFF;
        f->rgb[0] = 0xFF;
        gMenus->next = 3;
        gMenus->fadeSpeed = 0.03f;
    }
    g = gMenus;
    if (--g->delay <= 0 && g->leaving == 0) {
        g->delay = 0;
        g->slide = (f32)((f64)g->slide - 0.03);
        if ((f64)g->slide < 0.0) g->slide = 0.0f;
        g = gMenus;
        g->wave = (f32)((f64)g->wave - 0.1);
        if ((f64)g->wave < 0.0) g->wave = 0.0f;
        g = gMenus;
        g->pulse += 0xAAA;
        g->pulse2 += 0xAAA;
        if ((f64)g->slide == 0.0 && g->leaving == 0) {
            if (g->menu2 == NULL) {
                Menu_Input(g->menu, &g->cursor, B_800F8C78, B_800F77D8, (B_800F4E6C & 0x9000) ? 2 : 1,
                           &g->pulse);
            } else {
                Menu_Input(g->menu, &g->cursor, B_800F4E54, B_800F2D18, 1, &g->pulse);
                g = gMenus;
                if (g->menu2 != NULL) Menu_Input(g->menu2, &g->cursor2, B_800F4E6C, B_800F8C84, 2, &g->pulse2);
            }
        }
    }
    func_8002AA14(&gScreenFade, gMenus->fadeSpeed);
    if ((FadeDone() || f->active == 0) && (f64)f->level == 1.0) func_8002AC24(&gScreenFade);
    if (gMenus->leaving != 0 && (FadeDone() || f->active == 0)) gMenus->result = gMenus->next;
    return gMenus->result;
}

/* the fonts, by number */
static u8 *Font(s32 n) {
    switch (n) {
    case 0: return gMenus->bigFont;
    case 1: return gMenus->font1;
    case 3: return gMenus->font3;
    default: return gMenus->font2;
    }
}

static u8 Scale(u8 c, f32 k) { return (u32)((f32)c * k); }

/* the text colours: the six corners of the letters' gradient */
static void SetColours(const u8 *c) {
    B_800F7D94 = c[0]; B_800F7778 = c[0];
    B_800F7789 = c[1]; B_800F7770 = c[1];
    B_800F7E78 = c[2]; B_800F4E34 = c[2];
    B_800F7788 = c[3]; B_800F7E70 = c[3];
    B_800F88BC = c[4]; B_800F88C8 = c[4];
    B_800F7ECC = c[5]; B_800F2D1C = c[5];
}

static void SetColoursPulsed(const u8 *c, f32 k) {
    u8 v;
    v = Scale(c[0], k); B_800F7D94 = v; B_800F7778 = v;
    v = Scale(c[1], k); B_800F7789 = v; B_800F7770 = v;
    v = Scale(c[2], k); B_800F7E78 = v; B_800F4E34 = v;
    v = Scale(c[3], k); B_800F7788 = v; B_800F7E70 = v;
    v = Scale(c[4], k); B_800F88BC = v; B_800F88C8 = v;
    v = Scale(c[5], k); B_800F7ECC = v; B_800F2D1C = v;
}

static void V(Vtx16 *v, s32 x, s32 y, u8 r, u8 g, u8 b, u8 a) {
    v->x = x; v->y = y; v->z = 0; v->flag = 0; v->s = 0; v->t = 0;
    v->r = r; v->g = g; v->b = b; v->a = a;
}

static void Triangles(Vtx16 *v, s32 n, u32 t1, u32 t2) {
    Gfx2(0x04000000 | (n << 10) | (n * 0x10 - 1), (u32)v);
    gVtx = v + n;
    Gfx2(t1, t2);
}

/* a slider: an outline, the filled part shaded in the value's colours, the rest darkened */
static void Slider(MenuItem *it, s32 right, s32 y, u8 *font) {
    Vtx16 *v;
    s32 l, top, bot, r1, fillX;

    l = right - it->count - 1;
    top = y + *(s16 *)(font + 2) - 8;
    bot = top - 7;
    func_8007FB08(&gDL, 0x204);
    func_8007FB90(&gDL, 0x0C084000, 0x03024000);
    func_8007FBE8(&gDL, 1);
    l--; top++; bot--;
    r1 = right - 1;
    v = gVtx;
    V(&v[0], l, top, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[1], right, top, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[2], l, top - 1, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[3], right, top - 1, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[4], right, top, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[5], r1, top, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[6], right, bot, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[7], r1, bot, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[8], l, bot, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[9], right, bot, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[10], l, bot + 1, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[11], right, bot + 1, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[12], l, top, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[13], l + 1, top, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[14], l, bot, 0xFF, 0xFF, 0xFF, 0xFF);
    V(&v[15], l + 1, bot, 0xFF, 0xFF, 0xFF, 0xFF);
    Triangles(v, 16, 0xB1000206, 0x00000604);
    Gfx2(0xB10E080A, 0x000C080E);
    Gfx2(0xB1161012, 0x00141016);
    Gfx2(0xB11E181A, 0x001C181E);
    l++;
    bot++;
    fillX = l + (u16)*it->value + 1;
    v = gVtx;
    V(&v[0], l, top - 1, B_800F7778 >> 2, B_800F7770 >> 2, B_800F4E34 >> 2, 0xFF);
    V(&v[1], fillX, top - 1, B_800F7D94, B_800F7789, B_800F7E78, 0xFF);
    V(&v[2], l, bot, B_800F7788 >> 2, B_800F88BC >> 2, B_800F7ECC >> 2, 0xFF);
    V(&v[3], fillX, bot, B_800F7E70, B_800F88C8, B_800F2D1C, 0xFF);
    Triangles(v, 4, 0xB1000206, 0x00000604);
    Gfx2(0xE7000000, 0);
    if (l + *it->value + 1 < r1) {
        func_8007FB08(&gDL, 0x204);
        func_8007FB90(&gDL, 0x00404240, 0x00104240);
        func_8007FBE8(&gDL, 1);
        v = gVtx;
        V(&v[0], fillX, top - 1, 0, 0, 0, 0x80);
        V(&v[1], r1, top - 1, 0, 0, 0, 0x80);
        V(&v[2], fillX, bot, 0, 0, 0, 0x80);
        V(&v[3], r1, bot, 0, 0, 0, 0x80);
        Triangles(v, 4, 0xB1000206, 0x00000604);
        Gfx2(0xE7000000, 0);
    }
}

/* the letters waving as a new menu comes up: more at the middle of the screen */
static f32 Wave(s32 y) {
    s32 d = (-40 - y >= 0) ? y + 220 : 140 - y;
    f32 w = (f32)((f64)gMenus->wave * ((f64)(f32)d * 0.01));
    if ((f64)w < 0.0) w = 0.0f;
    return w;
}

/* 0x80058B14: the picture, once a frame */
s32 Menus_Frame(void) {
    MenuScreen *g = gMenus;
    Menu *m;
    MenuItem *it;
    f32 up[3], right[3], k, wave, slide, reach, fx, dir;
    u8 *font, *vfont;
    s32 i, x, vx, y, sel, fi;
    Vtx16 *v;

    wave = 0.0f;
    reach = 0.0f;
    slide = reach;
    if (g == NULL) return 0;
    while (gMenus->lastTick[0] == D_800B09B0 && gMenus->lastTick[1] == D_800B09B4) {
        PORT_SPIN();
    }                                                   /* one frame per retrace */
    gMenus->lastTick[0] = D_800B09B0;
    gMenus->lastTick[1] = D_800B09B4;
    func_8007E860(&gDL);
    func_8007E9DC(&gDL);
    func_8007EB48(&gDL);
    func_80081924(&gDL, gMenus->background, 3);        /* the scrolling background */
    Gfx2(0xBA001402, 0x00200000);
    Gfx2(0xE4000000 | ((((B_800F5D00 - 1) * 4) & 0xFFF) << 12) | (((B_800F5BEC - 1) * 4) & 0xFFF), 0);
    Gfx2(0xB4000000, (gMenus->scrollX << 21) | ((gMenus->scrollY << 5) & 0xFFFF));
    Gfx2(0xB3000000, 0x10000400);
    func_8007EEF0(&gDL);
    func_8002E334(gMenus->stage);
    g = gMenus;
    if (g->delay > 0) goto done;
    if (0.5 < (f64)g->slide) slide = (f32)(((f64)g->slide - 0.5) * 300.0);
    if (0.0 < (f64)gMenus->slide) {
        if (0.5 <= (f64)gMenus->slide) reach = 150.0f;
        else reach = (f32)((f64)gMenus->slide * 300.0);
    }
    k = (f32)((f64)vsSin(gMenus->pulse) * 0.2 + 0.8);
    SetTexFilter(0);
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    m = gMenus->menu;
    y = m->y;
    for (i = 0; i < gMenus->menu->count; i++) {
        it = &gMenus->menu->items[i];
        fi = it->font != -1 ? it->font : gMenus->menu->font;
        font = Font(fi);
        vfont = Font(it->valueFont != -1 ? it->valueFont : fi);
        func_80082210(font, &gDL, 0);
        x = gMenus->menu->x;
        if (x == -1) x = -func_80082298(font, it->label) / 2;
        if (it->names == NULL) vx = gMenus->menu->valueX;
        else vx = gMenus->menu->valueX - func_80082298(font, it->names[*it->value]);
        if (it->y != -1) y = it->y;
        if (0.0 < (f64)gMenus->wave) wave = Wave(y);
        g = gMenus;
        sel = g->cursor == i && (f64)g->slide == 0.0;
        if (sel) SetColoursPulsed(g->menu->hiColour, k);
        else {
            SetColours(g->menu->colour);
            if ((B_800F88EC == 1 && mpStrEqualNoCase(it->label, "VS Mode")) || PORT_NO_BUTTON_CONFIG(it->label)) {   /* one controller (the port: no button set-up) */
                B_800F7D94 = 0x80; B_800F7778 = 0x80;
                B_800F7789 = 0x80; B_800F7770 = 0x80;
                B_800F7E78 = 0x80; B_800F4E34 = 0x80;
                B_800F7788 = 0x32; B_800F7E70 = 0x32;
                B_800F88BC = 0x32; B_800F88C8 = 0x32;
                B_800F7ECC = 0x32; B_800F2D1C = 0x32;
            }
        }
        if (0.0 < (f64)gMenus->slide) {                 /* sliding in from the sides */
            Menu_WaveText(font, &gDL, it->label, (i & 1) ? (f32)x + slide : (f32)x - slide, (f32)y, 0, up,
                          right, 8, wave);
            if (!(i & 1)) {
                fx = (f32)x - slide;
                dir = -1.0f;
            } else {
                fx = (f32)(x + func_80082298(font, it->label)) + slide;
                dir = 1.0f;
            }
            func_8007FB08(&gDL, 0x204);
            func_8007FB90(&gDL, 0x00404240, 0x00404240);
            func_8007FBE8(&gDL, 1);
            v = gVtx;
            V(&v[0], (s32)fx, y + *(u16 *)(font + 2), 0xFF, 0xFF, 0xFF, 0xC8);
            V(&v[1], (s32)fx, y, 0x96, 0x64, 0, 0xC8);
            V(&v[2], (s32)(fx + reach * dir), y + *(u16 *)(font + 2), 0xFF, 0xFF, 0xFF, 0);
            V(&v[3], (s32)((f64)fx + ((30.0 < (f64)reach) ? (f64)reach - 30.0 : (f64)reach) * (f64)dir), y,
              0x96, 0x64, 0, 0);
            Triangles(v, 4, 0xB1000204, 0x00040602);
            Gfx2(0xE7000000, 0);
        } else {
            Menu_WaveText(font, &gDL, it->label, (f32)x, (f32)y, 0, up, right, 8, wave);
            if (it->names != NULL || it->slider != 0) {
                g = gMenus;
                if (g->cursor == i && (f64)g->slide == 0.0) SetColoursPulsed(g->menu->valueHiColour, k);
                else SetColours(g->menu->valueColour);
            }
            if (it->names != NULL) {
                func_80082210(vfont, &gDL, 0);
                Menu_WaveText(vfont, &gDL, it->names[*it->value], (f32)vx, (f32)y, 0, up, right, 8, wave);
            }
            if (it->slider != 0) Slider(it, vx, y, font);
        }
        y -= *(s16 *)(font + 2);
    }
    g = gMenus;
    if (g->menu2 == NULL) goto done;                    /* player 2's buttons */
    k = (f32)((f64)vsSin(g->pulse2) * 0.2 + 0.8);
    SetTexFilter(0);
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    y = gMenus->menu2->y;
    for (i = 0; i < gMenus->menu2->count; i++) {
        it = &gMenus->menu2->items[i];
        fi = it->font != -1 ? it->font : gMenus->menu2->font;
        font = fi == 1 ? gMenus->font1 : fi == 0 ? gMenus->bigFont : gMenus->font2;
        vfont = Font(it->valueFont != -1 ? it->valueFont : fi);
        func_80082210(font, &gDL, 0);
        x = gMenus->menu2->x;
        if (x == -1) x = -func_80082298(font, it->label) / 2;
        if (it->names == NULL) vx = gMenus->menu2->valueX;
        else vx = gMenus->menu2->valueX - func_80082298(font, it->names[*it->value]);
        if (it->y != -1) y = it->y;
        if (0.0 < (f64)gMenus->wave) wave = Wave(y);
        g = gMenus;
        if (g->cursor2 == i && (f64)g->slide == 0.0) SetColoursPulsed(g->menu2->hiColour, k);
        else {
            SetColours(g->menu2->colour);
            if (B_800F88EC == 1 && mpStrEqualNoCase(it->label, "VS Mode")) {
                B_800F7D94 = 0x80; B_800F7778 = 0x80;
                B_800F7789 = 0x80; B_800F7770 = 0x80;
                B_800F7E78 = 0x80; B_800F4E34 = 0x80;
                B_800F7788 = 0x32; B_800F7E70 = 0x32;
                B_800F88BC = 0x32; B_800F88C8 = 0x32;
                B_800F7ECC = 0x32; B_800F2D1C = 0x32;
            }
        }
        Menu_WaveText(font, &gDL, it->label, (f32)x, (f32)y, 0, up, right, 8, wave);
        if (it->names != NULL || it->slider != 0) {
            g = gMenus;
            if (g->cursor2 == i && (f64)g->slide == 0.0) SetColoursPulsed(g->menu2->valueHiColour, k);
            else SetColours(g->menu2->valueColour);
        }
        if (it->names != NULL) {
            func_80082210(vfont, &gDL, 0);
            Menu_WaveText(vfont, &gDL, it->names[*it->value], (f32)vx, (f32)y, 0, up, right, 8, wave);
        }
        y -= *(s16 *)(font + 2);
    }
done:
    func_8002AC80(&gScreenFade, &gDL);
    return gMenus->result;
}

/* 0x8005B224: one player's buttons on a menu */
void Menu_Input(Menu *m, s32 *cursor, u32 pressed, u32 held, s32 player, s32 *pulse) {
    MenuScreen *g;
    MenuItem *it;
    s32 changed, old, v;

    if (pressed & 0x800) {                              /* up */
        if (--*cursor < 0) *cursor = m->count - 1;
        Sound(7, 1.0f);
        *pulse = 0;
        if (m->id == 0 && B_800F88EC == 1 && *cursor == 1) *cursor = 0;   /* no VS with one pad */
#ifdef TARGET_PORT
        if (m->id == 1 && *cursor == 7) *cursor = 6;   /* (the port: Button config can't be chosen) */
#endif
    }
    if (pressed & 0x400) {                              /* down */
        if (++*cursor >= m->count) *cursor = 0;
        Sound(7, 1.0f);
        *pulse = 0;
        if (m->id == 0 && B_800F88EC == 1 && *cursor == 1) *cursor = 2;
#ifdef TARGET_PORT
        if (m->id == 1 && *cursor == 7) *cursor = 8;
#endif
    }
    if (pressed & 0x9000) {                             /* A or start */
        Sound(7, 1.5f);
        switch (m->id) {
        case 0:
            switch (*cursor) {
            case 0:                                     /* Game Start */
                g = gMenus;
                g->leaving = 1;
                g->next = player != 1;
                func_8002A990(&gScreenFade, 1);
                gMenus->fadeSpeed = 0.05f;
                D_800AE9F8 = 0;
                return;
            case 1:                                     /* VS Mode */
                g = gMenus;
                g->leaving = 1;
                g->next = 2;
                func_8002A990(&gScreenFade, 1);
                gMenus->fadeSpeed = 0.05f;
                D_800AE9F8 = 1;
                return;
            case 2:                                     /* Options */
                gMenus->menu = &MENU_OPTIONS;
                *cursor = 0;
                goto enter;
            }
            break;
        case 1:
            if (*cursor == 9) {                         /* Exit */
                gMenus->menu = &MENU_TITLE;
                gMenus->menu2 = NULL;
                *cursor = 2;
                goto enter;
            }
#ifdef TARGET_PORT
            if (*cursor == 7) break;                    /* (the port: the attacks are fixed to the
                                                           Dreamcast pad's tap-and-hold layout) */
#endif
            if (*cursor == 7) {                         /* Button config */
                g = gMenus;
                g->menu = &MENU_BUTTONS1;
                *cursor = 0;
                func_8002E9B0(gMenus->stage);
                g = gMenus;
                g->wave = 1.0f;
                g->menu2 = &MENU_BUTTONS2;
                g->cursor2 = 0;
                return;
            }
            if (*cursor == 8) {                         /* Display Options */
                gMenus->menu = &MENU_DISPLAY;
                gMenus->menu2 = NULL;
                *cursor = 0;
                goto enter;
            }
            break;
        case 3:
            if (*cursor == 4) {
                gMenus->menu = &MENU_OPTIONS;
                gMenus->menu2 = NULL;
                *cursor = 8;
                goto enter;
            }
            break;
        case 2:
            if (*cursor == 0) {                         /* Reset */
                if (m == &MENU_BUTTONS1) Menu_ResetButtons1();
                else Menu_ResetButtons2();
            } else if (*cursor == 7) {                  /* Exit */
                gMenus->menu = &MENU_OPTIONS;
                gMenus->menu2 = NULL;
                *cursor = 7;
                gMenus->cursor = 7;
                goto enter;
            }
            break;
        }
    }
    if (pressed & 0x4000) {                             /* B: back */
        switch (m->id) {
        case 1:
            gMenus->menu = &MENU_TITLE;
            *cursor = 2;
            goto enter;
        case 3:
            g = gMenus;
            g->menu = &MENU_OPTIONS;
            g->menu2 = NULL;
            *cursor = 8;
            func_8002E9B0(gMenus->stage);
            g = gMenus;
            g->wave = 1.0f;
            g->menu = &MENU_OPTIONS;
            return;
        case 2:
            if (*cursor == 0 || *cursor == m->count - 1) {
                gMenus->menu = &MENU_OPTIONS;
                gMenus->menu2 = NULL;
                *cursor = 7;
                goto enter;
            }
            break;
        }
    }
    it = &m->items[*cursor];
    if (it->value == NULL) return;
    old = *it->value;
    changed = 0;
    if (m->id == 2) {                                   /* press the button to use */
        if (pressed & 0xC00F) {
            v = 0;
            if (pressed & 2) v = 1;
            if (pressed & 8) v = 2;
            if (pressed & 0x8000) v = 3;
            if (pressed & 4) v = 4;
            if (pressed & 1) v = 5;
            if (*it->value != v) {
                *it->value = v;
                changed = 1;
            }
        }
    } else if (it->slider != 0) {                       /* held left/right */
        if (held & 0x200) {
            if (*it->value > 0) {
                (*it->value)--;
                changed = 1;
            }
        }
        if (held & 0x100) {
            if (*it->value < it->count - 1) {
                (*it->value)++;
                changed = 1;
            }
        }
    } else {                                            /* left/right through the names */
        if (pressed & 0x200) {
            if (--*it->value < 0) *it->value = it->count - 1;
            changed = 1;
        }
        if (pressed & 0x100) {
            if (++*it->value >= it->count) *it->value = 0;
            changed = 1;
        }
    }
    if (!changed) return;
    if (it->changed != NULL) it->changed(m, it, *it->value, old);
    if (it->slider == 0) Sound(7, 1.5f);
    return;
enter:
    func_8002E9B0(gMenus->stage);
    gMenus->wave = 1.0f;
}

/* 0x8005B978: text, each letter scaled by its distance from the middle of the screen */
void Menu_WaveText(u8 *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right,
                   s32 flags, f32 wave) {
    u8 *glyphs = font + *(s32 *)(font + 0x10);
    f32 u[3], r[3], d, sc;
    f64 k;
    s32 i, c, first, adv;
    u8 *gl;

    if ((f64)wave == 0.0) {
        SetTexFilter(0);
        func_80082518(font, dl, s, x, y, z, up, right, flags);
        return;
    }
    SetTexFilter(0x2000);
    if (0.0 < (f64)y) y = (f32)((f64)y + 1.0);
    for (i = 0; (c = (u8)s[i]) != 0; i++) {
        d = 0.0f - x;
        if (d < 0.0f) d = 180.0f - (0.0f - d);
        else d = 180.0f - d;
        sc = (f32)((f64)wave * ((f64)d * 0.01));
        if ((f64)sc < 0.0) sc = 0.0f;
        sc = (f32)((f64)sc * 1.2);
        k = (f64)sc + 1.0;
        u[0] = (f32)((f64)up[0] * k);
        u[1] = (f32)((f64)up[1] * k);
        u[2] = (f32)((f64)up[2] * k);
        r[0] = (f32)((f64)right[0] * k);
        r[1] = (f32)((f64)right[1] * k);
        r[2] = (f32)((f64)right[2] * k);
        first = *(s16 *)(font + 0xC);
        gl = glyphs + (c - first) * 8;
        if (c < first || *(s16 *)(font + 0xE) < c || *(s32 *)gl == 0) {
            adv = *(s16 *)(font + 4);
        } else {
            func_80084CB0(dl, font + *(s32 *)gl,
                          (f32)((f64)x - (f64)((f32)*(s16 *)(gl + 4) * sc) / 2.0),
                          (f32)((f64)y - (f64)((f32)*(s16 *)(gl + 6) * sc) / 2.0),
                          (f32)((f64)sc * 1000.0), u, r, flags);
            adv = *(s16 *)(gl + 4) + *(s16 *)(font + 6);
        }
        x -= (f32)adv * right[0];
        y -= (f32)adv * right[1];
    }
}

/* 0x8005BD68 */
void Menu_SetMusicVolume(Menu *m, MenuItem *it, s32 v, s32 old) {
    (void)m; (void)it; (void)old;
    func_8007A8F4(v * 364);
}

/* 0x8005BD9C: and a blip now and then to hear the new level by */
void Menu_SetSfxVolume(Menu *m, MenuItem *it, s32 v, s32 old) {
    (void)m; (void)it; (void)old;
    func_8007AA04(v * 364);
    if (func_800A4090(((s64)D_800B09B0 << 32) | D_800B09B4, 5) == 0) Sound(7, 1.5f);
}

/* 0x8005BE34 */
void Menu_ResetButtons1(void) {
    D_800AE704[0] = 0; D_800AE704[1] = 1; D_800AE704[2] = 2;
    D_800AE704[3] = 3; D_800AE704[4] = 4; D_800AE704[5] = 5;
}

/* 0x8005BE7C: a button given an action another had: that one gets the old one */
void Menu_SwapButton1(Menu *m, MenuItem *it, s32 v, s32 old) {
    s32 i;
    (void)m;
    if (old == v) return;
    for (i = 1; i < 7; i++) {
        if (&D_800AE74C[i] == it) continue;
        if (*D_800AE74C[i].value == v) *D_800AE74C[i].value = old;
    }
}

/* 0x8005BED8 */
void Menu_ResetButtons2(void) {
    D_800AE71C[0] = 0; D_800AE71C[1] = 1; D_800AE71C[2] = 2;
    D_800AE71C[3] = 3; D_800AE71C[4] = 4; D_800AE71C[5] = 5;
}

/* 0x8005BF20 */
void Menu_SwapButton2(Menu *m, MenuItem *it, s32 v, s32 old) {
    s32 i;
    (void)m;
    if (old == v) return;
    for (i = 1; i < 7; i++) {
        if (&D_800AE86C[i] == it) continue;
        if (*D_800AE86C[i].value == v) *D_800AE86C[i].value = old;
    }
}

/* ---------------------------------------------------------------- the launch shot */

typedef struct LaunchShot {                /* 0x360C bytes */
    /* 0x0000 */ u8 stage[0x35CC];         /* the 3D stage (0x8002EA90) */
    /* 0x35CC */ s32 v35CC;
    /* 0x35D0 */ f32 eye[3];
    /* 0x35DC */ f32 v35DC[3];
    /* 0x35E8 */ f32 pos[3];               /* the fighter, flying */
    /* 0x35F4 */ f32 vel[3];
    /* 0x3600 */ s32 v3600;
    /* 0x3604 */ s32 tick;
    /* 0x3608 */ f32 shake;                /* after 200 ticks: the twinkle, the shaking dying away */
} LaunchShot;

extern LaunchShot *D_800AEA00;
#define gLaunch D_800AEA00
void func_8007FF20(s32 a, f32 nearZ, f32 farZ);
void func_8007FFB8(u32 **dl);
u32  guRandom(void);
extern void *B_800F8768;

/* 0x8005BF80 */
void func_8005BF80(void) {
    LaunchShot *l;

    if (gLaunch != NULL) return;
    gLaunch = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(LaunchShot), 0);
    bzero(gLaunch, sizeof(LaunchShot));
    l = gLaunch;
    l->eye[0] = -1800.0f;
    l->eye[1] = 800.0f;
    l->eye[2] = 200.0f;
    l->v35DC[0] = 0.0f;
    l->v35DC[1] = 200.0f;
    l->v35DC[2] = 200.0f;
    l->pos[0] = 0.0f;
    l->pos[1] = 100.0f;
    l->pos[2] = 200.0f;
    l->vel[0] = -9.0f;
    l->vel[1] = 10.0f;
    l->vel[2] = 0.0f;
    l->v3600 = 0;
    l->shake = 0.0f;
    func_8002EA90(l);
    gLaunch->v35CC = 0;
}

/* 0x8005C060 */
void func_8005C060(void) {
    if (gLaunch == NULL) return;
    func_8002ECBC(gLaunch);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gLaunch);
    gLaunch = NULL;
}

/* 0x8005C0A8: once a tick: the fighter flies up and away (gravity), the camera pulls in;
 * after 200 ticks the twinkle and the shaking */
void func_8005C0A8(void) {
    LaunchShot *l = gLaunch;

    if (l == NULL) return;
    func_8002ED34(l);
    l = gLaunch;
    if (++l->tick >= 200) {
        if (l->tick == 200) {
            l->shake = 1.0f;
            func_800797EC(B_800F8768, 0, 0, 0x1B, 0x7FFF, 0, 0, 1.0f, 1);
        }
        l = gLaunch;
        l->shake = (f32)((f64)l->shake - 0.01);
        if ((f64)l->shake < 0.0) l->shake = 0.0f;
        return;
    }
    l = gLaunch;
    l->pos[0] += l->vel[0];
    l->pos[1] += l->vel[1];
    l->pos[2] += l->vel[2];
    l->vel[1] = (f32)((f64)l->vel[1] - 0.1);
    l->eye[0] = (f32)((f64)l->eye[0] - 0.5);
    l->eye[1] = (f32)((f64)l->eye[1] - 4.0);
}

/* a random shake, -1..1 times 5 times the shake */
static f32 Shake(void) {
    s32 r = (s32)guRandom() % 1024 - 512;
    return (f32)((f64)(f32)r / 512.0 * 5.0 * (f64)gLaunch->shake);
}

/* 0x8005C21C: the picture: the arena, then the fighter's frame facing the camera, turned */
void func_8005C21C(CharFrame *frame, f32 *unused1, f32 *unused2, s32 angle) {
    f32 cam[16], m[16], up[3], right[3], dx, dy;

    (void)unused1; (void)unused2;
    if (gLaunch == NULL) return;
    func_8007E860(&gDL);
    func_8007E9DC(&gDL);
    func_8007E720(&gDL, 0xFF, 0, 0);
    func_8007EEF0(&gDL);
    func_80088654(cam, gLaunch->eye, gLaunch->pos);
    func_8002FD50(gLaunch, cam);
    func_8002FE34(gLaunch, cam);
    func_8007FF20(0x21, 60.0f, 4000.0f);
    func_8007FFB8(&gDL);
    dx = Shake();
    dy = Shake();
    func_80087E70(m, cam);
    func_8008861C(m);
    func_8008004C(&gDL, m);
    func_80087E20(m);
    func_80080170(&gDL, m);
    func_80087E70(m, cam);
    func_80088440(m, angle);
    right[0] = m[0]; right[1] = m[1]; right[2] = m[2];
    up[0] = m[4]; up[1] = m[5]; up[2] = m[6];
    up[0] = (f32)((f64)up[0] + (f64)up[0]);
    up[1] = (f32)((f64)up[1] + (f64)up[1]);
    up[2] = (f32)((f64)up[2] + (f64)up[2]);
    right[0] = (f32)((f64)right[0] + (f64)right[0]);
    right[1] = (f32)((f64)right[1] + (f64)right[1]);
    right[2] = (f32)((f64)right[2] + (f64)right[2]);
    func_80083A64(&gDL, frame, dx * right[0] + gLaunch->pos[0] + dy * up[0],
                  dx * right[1] + gLaunch->pos[1] + dy * up[1],
                  dx * right[2] + gLaunch->pos[2] + dy * up[2], up, right, 0);
    func_8007FF20(0x21, 60.0f, 3600.0f);
    func_8007FFB8(&gDL);
}
