/*
 * CHARBIO.C (0x800569D0-0x800582F0): a fighter's biography - the fighter's arena slowly
 * flown through, the portrait faded in at the side, and the text scrolling up a line at a
 * time, each line word-wrapped to 260 pixels and justified (letter spacing first, then the
 * gaps between words) when it was broken.
 *
 * CharBio_Draw is the logic (Game_Render calls it every tick); CharBio_Frame builds the
 * picture once a frame (CharBio_Run's loop).
 */
#include "common.h"
#include "mpfplat.h"
#include "screen.h"
#include "combat.h"
#include "char.h"

typedef struct BioLine {                   /* a line of text on the screen (0x28 bytes) */
    /* 0x00 */ const char *text;           /* NULL = free */
    /* 0x04 */ s32 len;
    /* 0x08 */ s32 numWords;
    /* 0x0C */ char **words;
    /* 0x10 */ s32 *widths;
    /* 0x14 */ s32 *xs;
    /* 0x18 */ s32 total;                  /* the words' width together */
    /* 0x1C */ s32 spacing;                /* letter spacing */
    /* 0x20 */ s32 broken;                 /* the line was wrapped (so it is justified) */
    /* 0x24 */ s32 y;                      /* -140 (bottom) up to 141 */
} BioLine;

typedef struct BioCam {                    /* each arena's fly-through (0x40 bytes) */
    f32 pos[3], dpos[3], at[3], dat[3];
    f32 dist, ddist;
    s32 angle, dangle;
} BioCam;

typedef struct BioScreen {                 /* 0x834 bytes */
    /* 0x000 */ s32 character, arena;
    /* 0x008 */ s32 result;                /* -1 while running; 0 at the end, 1 skipped */
    /* 0x00C */ s32 tick;
    /* 0x010 */ s32 finished;              /* the text has all scrolled away */
    /* 0x014 */ s32 skipped;
    /* 0x018 */ s32 mode;                  /* 1 = the ending text, can't be skipped */
    /* 0x01C */ f32 fadeSpeed;
    /* 0x020 */ s32 angle;
    /* 0x024 */ f32 dist;
    /* 0x028 */ f32 pos[3], at[3];
    /* 0x040 */ u8 *portraitFile, *portrait;
    /* 0x048 */ f32 alpha;                 /* the portrait fading in */
    /* 0x04C */ u8 *bigFontFile, *bigFont;
    /* 0x054 */ u8 *fontFile, *font;
    /* 0x05C */ const char *text;          /* the rest of the text */
    /* 0x060 */ s32 delay;                 /* ticks before the text starts */
    /* 0x064 */ BioLine lines[50];
} BioScreen;

extern BioScreen *D_800AE35C;
#define gBio D_800AE35C
extern s32 D_800ADA10[];                   /* each fighter's arena */
extern BioCam D_800ADC54[];
extern const char *D_800AE2D4[], *D_800AE318[];  /* the biographies, the endings */
extern f32 D_800ADB44[][4];                /* each fighter's portrait colour */
extern f32 D_800ADA54[][2];                /* ... and position */
extern s32 B_800F8DD0;

void CharBio_Init(s32 character, s32 costume, s32 mode);
void CharBio_Shutdown(void);
s32  CharBio_Draw(void);
s32  CharBio_Frame(void);

static void SetTexFilter(u32 mode) {
    u32 *g = gDL;
    gDL = g + 2;
    g[0] = 0xBA000C02;
    g[1] = mode;
}

static s32 FadeDone(void) {
    ScreenFade *f = &gScreenFade;
    return (0.0 <= (f64)f->speed) ? (1.0 <= (f64)f->level) : ((f64)f->level <= 0.0);
}

#define SPACING(font) (*(s16 *)((font) + 6))
#define GAP(font) (*(s16 *)((font) + 4))

/* 0x800569D0 */
void CharBio_Init(s32 character, s32 costume, s32 mode) {
    BioScreen *b;
    BioCam *c;
    u8 *img;
    s32 cap;

    if (gBio != NULL) return;
    gBio = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, sizeof(BioScreen), 0);
    bzero(gBio, sizeof(BioScreen));
    b = gBio;
    b->character = character;
    b->arena = D_800ADA10[character];
    b->mode = mode;
    c = &D_800ADC54[b->arena];
    b->angle = c->angle;
    b->dist = D_800ADC54[b->arena].dist;
    b->pos[0] = D_800ADC54[b->arena].pos[0];
    b->pos[1] = D_800ADC54[b->arena].pos[1];
    b->pos[2] = D_800ADC54[b->arena].pos[2];
    b->at[0] = D_800ADC54[b->arena].at[0];
    b->at[1] = D_800ADC54[b->arena].at[1];
    b->at[2] = D_800ADC54[b->arena].at[2];
    b->portraitFile = LoadCharacterPortrait(b->character, costume);
    b = gBio;
    b->portrait = img = b->portraitFile + *(s32 *)(b->portraitFile + 0x14);
    *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
    *(s16 *)(b->portrait + 0xC) = *(s16 *)(b->portrait + 8) / 2;
    b->alpha = 0.0f;
    b->bigFontFile = func_80080360(0xF4D8B0);
    gBio->bigFont = gBio->bigFontFile + *(s32 *)(gBio->bigFontFile + 0xC);
    gBio->fontFile = func_80080360(0xF56270);
    b = gBio;
    b->font = b->fontFile + *(s32 *)(b->fontFile + 0xC);
    if (mode != 0) b->text = D_800AE318[b->character];
    else gBio->text = D_800AE2D4[gBio->character];
    b = gBio;
    b->delay = 120;
    func_8000AF40(b->arena);
    gBio->result = -1;
    cap = 0;
    if ((f64)gScreenFade.level == 1.0 || gScreenFade.type == 0) cap = func_80081804();
    func_8002A340(&gScreenFade, cap ? 2 : 1);
    B_800F8DD0 = cap;
    gBio->fadeSpeed = (gScreenFade.type != 1) ? 0.03f : 0.02f;
    CharMenuSound_Load(6);
}

static void FreeLine(BioLine *l) {
    s32 j;
    for (j = 0; j < l->numWords; j++) mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, l->words[j]);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, l->words);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, l->widths);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, l->xs);
}

/* 0x80056CE0 */
void CharBio_Shutdown(void) {
    s32 i;

    if (gBio == NULL) return;
    func_8000B704();
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gBio->portraitFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gBio->bigFontFile);
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gBio->fontFile);
    for (i = 0; i < 50; i++) {
        if (gBio->lines[i].text == NULL) continue;
        gBio->lines[i].text = NULL;
        FreeLine(&gBio->lines[i]);
    }
    func_8002AC24(&gScreenFade);
    CharMenuSound_Free();
    func_8007A530();
    mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, gBio);
    gBio = NULL;
}

/* a word of a line, copied into its own string */
static void AddWord(BioLine *l, s32 k, s32 start, s32 n) {
    l->words[k] = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, n + 1, 0);
    bcopy(l->text + start, l->words[k], n);
    l->words[k][n] = 0;
}

/* the next line of the text: as many words as fit in 260 pixels, then split into words and
 * laid out - a wrapped line is spread to the full width (letter spacing up to a third of the
 * word gap, no more than half of it), the last line of a paragraph is not */
static void NewLine(BioLine *l) {
    const char *p, *brk;
    char c[2];
    s32 w, words, inWord, cw, k, i, start, total, gap, x, s, s0;

    c[0] = 0;
    c[1] = 0;
    while (*gBio->text == ' ') gBio->text++;
    if (*gBio->text == 0) return;
    p = brk = gBio->text;
    w = words = inWord = 0;
    l->broken = 0;
    for (;;) {
        c[0] = *p++;
        if (c[0] == ' ') {
            words += inWord;
            brk = p;
        }
        if (c[0] == 0 || c[0] == '\n') {
            words += inWord;
            break;
        }
        inWord = c[0] != ' ';
        cw = func_80082298(gBio->font, c);
        if (c[0] != ' ' && c[0] != '\t') cw += SPACING(gBio->font);
        w += cw;
        if (!(w < 0x104)) {
            p = brk;
            l->broken = 1;
            break;
        }
    }
    l->text = gBio->text;
    l->len = p - gBio->text;
    l->numWords = words;
    l->y = -140;
    while (l->text[l->len - 1] == '\n' || l->text[l->len - 1] == ' ') l->len--;
    l->words = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, words * 4, 0);
    l->widths = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, words * 4, 0);
    l->xs = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, words * 4, 0);
    start = 0;
    inWord = 0;
    k = 0;
    for (i = 0; i < l->len; i++) {
        if (l->text[i] == ' ') {
            if (inWord) AddWord(l, k++, start, i - start);
        } else if (!inWord) start = i;
        inWord = l->text[i] != ' ';
    }
    if (inWord) AddWord(l, k++, start, i - start);
    if (k != l->numWords) mpFatalErrorf("numWords(%d) != pLine->numWords(%d)\n", k);
    if (k >= 2 && l->broken != 0) {
        s0 = SPACING(gBio->font);
        s = s0 - 1;
        do {
            SPACING(gBio->font) = ++s;
            total = 0;
            for (i = 0; i < l->numWords; i++) total += func_80082298(gBio->font, l->words[i]);
            gap = (0x104 - total) / (k - 1);
        } while (GAP(gBio->font) * 3 < gap);
        if (gap / 2 < s) s = gap / 2;
        SPACING(gBio->font) = s;
        l->spacing = s;
        total = 0;
        for (i = 0; i < l->numWords; i++) {
            l->widths[i] = func_80082298(gBio->font, l->words[i]);
            total += l->widths[i];
        }
        l->total = total;
        gap = (0x104 - total) / (k - 1);
        x = 0;
        for (i = 0; i < k; i++) {
            l->xs[i] = x;
            x += gap + l->widths[i];
        }
        SPACING(gBio->font) = s0;
    } else {
        x = total = 0;
        for (i = 0; i < k; i++) {
            l->widths[i] = func_80082298(gBio->font, l->words[i]);
            total += l->widths[i];
            l->xs[i] = x;
            x += GAP(gBio->font) + l->widths[i];
        }
        l->total = total;
        l->spacing = SPACING(gBio->font);
    }
    if (p == gBio->text) mpFatalError("CHARBIO.C - Text formatting error!");
    gBio->text = p;
}

/* 0x80056EA8: the logic, once a tick */
s32 CharBio_Draw(void) {
    BioScreen *b = gBio;
    BioCam *c;
    ScreenFade *f = &gScreenFade;
    s32 i, freeLine, lowest, moved;
    f32 level;

    if (b == NULL) return 0;
    b->tick++;
    if (b->mode == 0 && (B_800F8C78 & 0x9000) &&
        (FadeDone() || f->active == 0 || (0.0 < (f64)f->speed && 0.5 <= (f64)f->level))) {
        level = f->level;                               /* skip: fade out from where it is */
        func_8002A990(&gScreenFade, 1);
        gBio->fadeSpeed = 0.04f;
        f->level = level;
        gBio->skipped = 1;
    }
    b = gBio;
    c = &D_800ADC54[b->arena];
    b->angle += c->dangle;
    b->dist += D_800ADC54[b->arena].ddist;
    b->pos[0] += D_800ADC54[b->arena].dpos[0];
    b->pos[1] += D_800ADC54[b->arena].dpos[1];
    b->pos[2] += D_800ADC54[b->arena].dpos[2];
    b->at[0] += D_800ADC54[b->arena].dat[0];
    b->at[1] += D_800ADC54[b->arena].dat[1];
    b->at[2] += D_800ADC54[b->arena].dat[2];
    b->alpha = (f32)((f64)b->alpha + 0.003);
    if (1.0 < (f64)b->alpha) b->alpha = 1.0f;
    func_8000B894(NULL);
    b = gBio;
    if (--b->delay < 0) b->delay = 0;
    if (gBio->delay <= 0) {
        freeLine = -1;
        lowest = 9999;
        for (i = 0; i < 50; i++) {
            BioLine *l = &gBio->lines[i];
            if (l->text == NULL) freeLine = i;
            else {
                if (l->y < lowest) lowest = l->y;
                if (!(gBio->lines[i].y < 0x8D)) gBio->lines[i].text = NULL;
            }
        }
        if (!(lowest + 0x8C < 0x15) && freeLine >= 0) NewLine(&gBio->lines[freeLine]);
    }
    moved = 0;
    for (i = 0; i < 50; i++) {                          /* scroll every fourth tick */
        BioLine *l = &gBio->lines[i];
        if (l->text == NULL) continue;
        moved = 1;
        if (gBio->tick & 3) continue;
        if (++l->y < 0x8D) continue;
        l->text = NULL;
        FreeLine(&gBio->lines[i]);
    }
    b = gBio;
    if (!moved && b->delay <= 0 && b->skipped == 0 && b->finished == 0) {
        func_8002A990(&gScreenFade, 1);
        gBio->fadeSpeed = 0.02f;
        gBio->finished = 1;
    }
    func_8002AA14(&gScreenFade, gBio->fadeSpeed);
    if (f->active != 0 && FadeDone()) {
        if ((f64)f->level == 1.0) func_8002AC24(&gScreenFade);
        else {
            if (gBio->finished != 0) gBio->result = 0;
            if (gBio->skipped != 0) gBio->result = 1;
        }
    }
    return gBio->result;
}

static u8 Channel(f32 c) { return (u32)((f64)(c * gBio->alpha) * 255.0); }

/* 0x80057B00: the picture, once a frame */
s32 CharBio_Frame(void) {
    f32 m[16], cam[16], zero[3], eye[3], up[3], right[3];
    s32 i, j;
    s16 s0;
    u8 v;

    if (gBio == NULL) return 0;
    func_80087EE8(m, 0.0f, 0.0f, gBio->dist);
    func_80088224(m, gBio->angle);
    func_80088184(m, gBio->pos[0], gBio->pos[1], gBio->pos[2]);
    zero[0] = 0.0f; zero[1] = 0.0f; zero[2] = 0.0f;
    eye[0] = (f32)((f64)(m[0] * zero[0] + m[4] * zero[0] + m[8] * zero[0]) + (f64)m[12]);
    eye[1] = (f32)((f64)(m[1] * zero[0] + m[5] * zero[0] + m[9] * zero[0]) + (f64)m[13]);
    eye[2] = (f32)((f64)(m[2] * zero[0] + m[6] * zero[0] + m[10] * zero[0]) + (f64)m[14]);
    func_80088654(cam, eye, gBio->at);
    func_8007E860(&gDL);
    func_8000B908();
    func_8007EEF0(&gDL);
    func_8000B998(cam, NULL);
    func_8000BCA8(cam, NULL);
    func_8000BE80(cam, NULL);
    func_8000C07C(cam, NULL);
    func_8007F94C(&gDL);
    up[0] = zero[0]; up[1] = 1.2f; up[2] = zero[0];
    right[0] = -1.2f; right[1] = zero[0]; right[2] = zero[0];
    v = Channel(D_800ADB44[gBio->character][0]);
    B_800F7788 = v; B_800F7E70 = v; B_800F7D94 = v; B_800F7778 = v;
    v = Channel(D_800ADB44[gBio->character][1]);
    B_800F88BC = v; B_800F88C8 = v; B_800F7789 = v; B_800F7770 = v;
    v = Channel(D_800ADB44[gBio->character][2]);
    B_800F7ECC = v; B_800F2D1C = v; B_800F7E78 = v; B_800F4E34 = v;
    v = Channel(D_800ADB44[gBio->character][3]);
    B_800F7E58 = v; B_800F2D1D = v; B_800F8760 = v; B_800F5B78 = v;
    func_80085FE4(&gDL, (CharFrame *)gBio->portrait, D_800ADA54[gBio->character][0] + -85.0f,
                  D_800ADA54[gBio->character][1] + -15.0f, 0, up, right, 9);
    func_80082210(gBio->font, &gDL, 0);
    func_8007EEF0(&gDL);
    func_8007F94C(&gDL);
    SetTexFilter(0);
    up[0] = 0.0f; up[1] = 1.0f; up[2] = 0.0f;
    right[0] = -1.0f; right[1] = 0.0f; right[2] = 0.0f;
    for (i = 0; i < 50; i++) {
        BioLine *l = &gBio->lines[i];
        const u8 *col;
        if (l->text == NULL) continue;
        s0 = SPACING(gBio->font);
        SPACING(gBio->font) = (s16)l->spacing;
        col = &D_800ADADC[gBio->character * 6];
        B_800F7D94 = col[0]; B_800F7778 = col[0];
        B_800F7789 = col[1]; B_800F7770 = col[1];
        B_800F7E78 = col[2]; B_800F4E34 = col[2];
        B_800F7788 = col[3]; B_800F7E70 = col[3];
        B_800F88BC = col[4]; B_800F88C8 = col[4];
        B_800F7ECC = col[5]; B_800F2D1C = col[5];
        for (j = 0; j < l->numWords; j++)
            func_80082518(gBio->font, &gDL, l->words[j], (f32)(l->xs[j] - 130), (f32)l->y, 0, up, right, 8);
        SPACING(gBio->font) = s0;
    }
    SetTexFilter(0x2000);
    func_8002AC80(&gScreenFade, &gDL);
    return gBio->result;
}
