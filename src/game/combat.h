/* COMBAT.C (0x80031FE0-0x80048150): the fight screen. */
#ifndef COMBAT_H
#define COMBAT_H

#include "common.h"
#include "cf3char.h"
#include "char.h"

/* ---- a line of on-screen text whose letters can move separately (0x290 bytes) ---- */
typedef struct HudText {
    /* 0x000 */ s32 len;
    /* 0x004 */ char text[32];
    /* 0x024 */ f32 x[31];          /* each letter's x offset */
    /* 0x0A0 */ f32 y[31];           /* ... and y offset */
    /* 0x11C */ f32 dir[31][2];     /* direction each letter flies off in */
    /* 0x214 */ s32 spin[31];       /* angle each letter spins to */
} HudText;

/* ---- "Combo Breaker!" banner (0x55C bytes) ---- */
typedef struct ComboBanner {
    /* 0x000 */ s32 active;
    /* 0x004 */ s32 v4;
    /* 0x008 */ HudText line1;
    /* 0x298 */ HudText line2;
    /* 0x528 */ f32 t;              /* 0..1 within the current state */
    /* 0x52C */ s32 state;          /* 0 = in, 1 = hold, 2 = out, 3 = done */
    /* 0x530 */ f32 alpha;
    /* 0x534 */ f32 scale;
    /* 0x538 */ s32 v538;
    /* 0x53C */ s32 v53C;
    /* 0x540 */ u8  rgb1[3];
    /* 0x543 */ u8  rgb2[3];
    /* 0x548 */ f32 v548;
    /* 0x54C */ s32 v54C;
    /* 0x550 */ s32 v550;
    /* 0x554 */ f32 v554;
    /* 0x558 */ s32 v558;
} ComboBanner;

/* ---- full-screen fade (0x800F8DB8, shared by every screen) ---- */
typedef struct ScreenFade {
    /* 0x00 */ s32 active;
    /* 0x04 */ s32 v4;
    /* 0x08 */ s32 type;
    /* 0x0C */ f32 level;
    /* 0x10 */ f32 speed;
    /* 0x14 */ u8  rgb[3];
} ScreenFade;

/* ---- globals (names for the disassembly's labels) ---- */
extern CharObj *B_800DCA80, *B_800DCA84;           /* the two loaded characters */
extern cfCharacter *B_800DCA98, *B_800DCA9C;       /* the two fighters */
extern CombatHandler *B_800DCAA0;                  /* the combat handler */
#define gCombatChar1   B_800DCA80
#define gCombatChar2   B_800DCA84
#define gCombatFighter1 B_800DCA98
#define gCombatFighter2 B_800DCA9C
#define gCombat        B_800DCAA0

extern s32 B_800E0E30, B_800E0E34, B_800E0E38, B_800E0E40, B_800E0E44;
#define gCombatArcade    B_800E0E30    /* 1: arcade mode (Combat_Start) */
#define gCombatTimeLimit B_800E0E34
#define gCombatSolo      B_800E0E40    /* 1: only one fighter is loaded (func_80032028) */

extern ScreenFade B_800F8DB8;
#define gScreenFade B_800F8DB8
extern s32 B_800E0CE8;

extern void *B_800F1330;                           /* the combat sound bank */
extern void *B_800F8768;                           /* the general sound bank */
extern void *D_800ACCFC;                           /* the big font */
/* ---- the game settings (0x800F5BC4 points at them) ---- */
typedef struct GameSettings {
    /* 0x00 */ s32 v0;                /* fighter 1: character */
    /* 0x04 */ s32 v4;                /* ... costume */
    /* 0x08 */ s32 v8;                /* ... computer player's level, -1 = human */
    /* 0x0C */ s32 vC;                /* fighter 2: character */
    /* 0x10 */ s32 v10;               /* ... costume */
    /* 0x14 */ s32 v14;               /* ... computer player's level, -1 = human */
    /* 0x18 */ s32 v18;
    /* 0x1C */ s32 v1C;               /* arena */
    /* 0x20 */ s32 v20;               /* part of the arena to start in */
    /* 0x24 */ s32 v24;               /* round time */
    /* 0x28 */ s32 v28;
    /* 0x2C */ s32 v2C;
    /* 0x30 */ s32 v30;               /* debug: fighter 1 starts on 1 health */
    /* 0x34 */ s32 showHealth;        /* the HUD's parts: health bars */
    /* 0x38 */ s32 showSuper;         /* ... super meters */
    /* 0x3C */ s32 showDizzy;         /* ... dizzy bars */
    /* 0x40 */ s32 showPortraits;     /* ... portraits */
    /* 0x44 */ f32 v44;               /* fighters' scale */
    /* 0x48 */ f32 v48;               /* sound pitch */
    /* 0x4C */ s32 v4C, v50;
    /* 0x54 */ s32 v54;               /* debug keys */
    /* 0x58 */ s32 v58, v5C;
    /* 0x60 */ u16 buttons1[6];       /* fighter 1's button mapping (0 = default) */
    /* 0x6C */ u16 buttons2[6];
    /* 0x78 */ s32 v78, v7C, v80;     /* results: winner, arena, arena part */
    /* 0x84 */ s32 v84, v88, v8C, v90, v94, v98, v9C, vA0, vA4, vA8, vAC, vB0;
} GameSettings;
extern GameSettings *B_800F5BC4;
#define gGame B_800F5BC4

/* ---- functions elsewhere (still assembly) ---- */
s32  Combat_Main(GameSettings *g);
void func_8002A990(ScreenFade *f, s32 type);                  /* start a fade */
s32  func_80082298(void *font, const char *s);                /* text width */
void func_800797EC(void *bank, s32 a1, s32 a2, s32 sound, s32 volume, s32 a5, s32 a6, f32 pitch, s32 a8);
s32  n64PlaySFX(void *bank, s32 a1, s32 a2, s32 sound);
void func_8007A0BC(void *bank);
void func_800893F8(void *light, f32 *pos, f32 *normal, f32 *r, f32 *g, f32 *b);   /* a light at a point */
extern s32 D_800AB4A4;   /* number of lights */
extern u8 *D_800AB4A8;   /* lights, 0x3C bytes each */
extern u8 B_800F7788, B_800F7E70, B_800F7D94, B_800F7778;
extern u8 B_800F88BC, B_800F88C8, B_800F7789, B_800F7770;
extern u8 B_800F7ECC, B_800F2D1C, B_800F7E78, B_800F4E34;

/* ---- the game camera (VSLMTX.C) ---- */
extern u8 B_800F8770[];                    /* the camera object */
#define gCamera ((void *)B_800F8770)
extern f32 B_800F5CCC[3];                  /* camera eye */
extern f32 B_800F7A70[3];                  /* camera target */
extern f32 B_800F87A4;                     /* camera +0x34 (roll?) */
extern f32 B_800F5CDC;
extern s32 B_800E0CFC;                     /* screen shake strength */
void func_80088654(void *cam, f32 *eye, f32 *at);       /* look from eye at at */
void func_80088350(void *cam, f32 x, f32 y, f32 z);     /* move */
void func_800883A0(void *cam, s32 a);                    /* turn */
void func_800883F0(void *cam, s32 a);                    /* tilt */
void func_80088440(void *cam, s32 a);
extern s32 B_800F88E4;                     /* controller 1 buttons */
extern s32 B_800F7E50, B_800F7784;         /* controller 1 stick x, y */

/* ---- the health / dizzy / super bars ---- */
typedef struct HudMeterSeg {               /* one third of a super meter (0x1C bytes) */
    /* 0x00 */ s32 w, h;                   /* size (13 x 3) */
    /* 0x08 */ s32 fill;                   /* 0..100 */
    /* 0x0C */ s32 vC;
    /* 0x10 */ s32 x, y;                   /* where it is drawn */
    /* 0x18 */ f32 flash;                  /* 0 when it fills up, rises back to 1 */
} HudMeterSeg;
extern HudMeterSeg B_800F5AF0[3], B_800F88F8[3];   /* fighter 1's, fighter 2's */
extern s32 B_800E0CB8;                     /* bars sliding in: 120 = in place */
extern f32 B_800E0CA4;                     /* bars fading in */
extern s32 B_800E0CA8, B_800E0CAC;         /* health shown */
extern s32 B_800E0CB0, B_800E0CB4;         /* set when a fighter is on full health after a round */
extern s32 B_800E0CC0, B_800E0CC4;         /* 200 - dizziness shown */
extern s32 B_800E0CC8, B_800E0CCC;         /* super meter shown */
extern s32 B_800F4E30;                     /* a pulsing angle */
extern f32 B_800F7A7C;                     /* its sine */
extern s32 D_800ACD18, D_800ACD1C;         /* the last two combo announcer sounds */
extern s32 B_800F5D00, B_800F5BEC;          /* the screen's width and height */
extern u8 D_800ACD20[];                    /* the players' colours (r, g, b) */

/* ---- the combat logic thread ---- */
#include "ultra64_min.h"
#include "mpfmem.h"
extern OSMesgQueue B_800DCAB0;             /* one message a frame */
extern MPF_Mutex B_800E0C80;               /* guards the combat state against the drawing */
extern s32 B_800E0E20, B_800E0E24, B_800E0E28, B_800E0E2C;
extern s32 B_800F7EC0, B_800F7A80, B_800F88D8, B_800F2E1C;
extern s32 B_800F2D18, B_800F8C88, B_800F4E54, B_800F5BE4, B_800F8C84, B_800F15E8;
extern s32 B_800F4E6C, B_800F4E60, B_800F77D8, B_800F7F50, B_800F8C78, B_800F15DC;
extern f32 B_800F7EC4;
extern s32 B_800F7E64, B_800F84C8, B_800F5BE8, B_800F8C7C, B_800F7D98, B_800F4E8C, B_800F7E60, B_800F7E74;
extern s32 B_800F8CA8, B_800F7D54;
extern s32 B_800F7774, B_800F5B68, B_800F4E7C;
extern s32 D_800AA164, D_800AA168;
extern s32 B_800E0CF8, B_800E0CEC, B_800E0CF0, B_800E0CF4;
extern s32 B_800F88E0, B_800F4E80;
extern s32 B_800F5BD8;
extern f32 B_800F5B84, B_800F7780, B_800F4E90;
extern s32 B_800F7ED0, B_800F7D5C, B_800F88EC;
extern s32 B_800E1210, B_800E1214;
extern s32 D_800ACD0C, D_800ACD10, D_800ACD14;
extern HudText B_800F89E8, B_800F77E0, B_800F5D30;
extern u8 B_800F7E80[], B_800F7E10[], B_800F2D50[];   /* matrices (VSLMTX.C) */
extern s32 B_800F5BA8, B_800F7D20, B_800F5B5C;
extern f32 B_800F5BE0, B_800F5D14, B_800F15F0;
extern s32 B_800F4E64, B_800F1344, B_800F4E38;
extern void *B_800F4E74;
extern s32 B_800F8C8C;
extern ComboBanner B_800F7F60, B_800F1600;
extern ScreenFade B_800E0E00;
extern s8 D_800AB23C[];
void func_8005C0A8(void);
void func_80076DB8(void);
void func_8000B894(CombatHandler *h);
void func_8000A71C(s32 a0, s32 a1, s32 a2, s32 *a3, s32 *a4, s32 *a5);
void func_80087E20(void *m);                             /* identity */
void func_80087E98(void *m, f32 x, f32 y, f32 z);         /* set scale */
void func_80087EE8(void *m, f32 x, f32 y, f32 z);
void func_80088134(void *m, f32 x, f32 y, f32 z);         /* scale */
void func_80088184(void *m, f32 x, f32 y, f32 z);         /* translate */
void func_80088274(void *m, s32 angle);                  /* rotate */
void func_8002AA14(ScreenFade *f, f32 speed);
void func_8002AC24(ScreenFade *f);
s32  func_8007A3B8(void *bank);

/* ---- set-up and tear-down (Combat_Main) ---- */
extern s32 B_800DCA88, B_800DCA8C, B_800DCA90, B_800DCA94;   /* characters and costumes loaded */
extern s32 B_800DCAA4, B_800E0CBC;
extern OSMesg B_800DCAA8;
extern u8 B_800DCAC8[];                    /* the logic thread */
extern u8 B_800E0D00[], B_800E0D40[], B_800E0D80[], B_800E0DC0[];   /* matrices */
extern u8 *B_800F5D18, *B_800F5CE0, *B_800F2D24, *B_800F2D48, *B_800F5B70;
extern u32 B_800F88B8, B_800F2E20;          /* portrait handles */
extern s32 B_800F15D8, D_800ACD08, D_800AB4AC;
extern u8 *D_800ACCF0, *D_800ACCF4, *D_800ACCF8, *D_800ACD00, *D_800ACD04;
extern u8 *D_800AB49C;                     /* the arena's BkgDef */
extern s8 D_800AB220[];
extern volatile u32 D_800B09B0, D_800B09B4; /* retrace counter */
void RomCopy(void *dst, u32 romAddr, u32 size);
void func_8007CCA4(void);
void func_8007C5BC(s32 a0, s32 a1, s32 a2);
void func_80087F38(void *m, s32 a);
void func_80088224(void *m, s32 a);
u8  *func_80080360(u32 romAddr);           /* load an asset */
void func_8000AF40(s32 arena);
void func_8000B704(void);
void func_8002A340(ScreenFade *f, s32 type);
void func_800779F4(OSMesgQueue *q);
void func_80077AF4(OSMesgQueue *q);
void func_8005C060(void);
void func_8007A530(void);
void func_8006B6E4(CombatHandler *h);
void func_800999D0(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri);   /* osCreateThread */
void func_8009A0D0(void *t);                                                            /* osStartThread */

/* ---- drawing ---- */
extern u32 *B_800F89E4;                    /* the display list being built */
#define gDL B_800F89E4
extern f32 B_800F4E70, D_800AB4E8;
extern s32 B_800E0E18, B_800F5D0C;
extern f32 B_800E0CD0[3], B_800E0CDC[3];
extern CharFrame *B_800F1338;              /* the "launch" frame shown full screen */
extern u8 D_800ADADC[];                    /* taunt text colours, 6 per character */
extern void *B_800E0E3C, *B_800E0E48;      /* per-frame drawing callbacks */
s32  func_80077F10(void);
s32  func_80081804(void);
void func_8005BF80(void);
void func_8007C850(void);
void func_8007E860(u32 **dl);
void func_8007E9DC(u32 **dl);
void func_8007E720(u32 **dl, s32 a, s32 b, s32 c);
void func_8007EEF0(u32 **dl);
void func_8007EB48(u32 **dl);
void func_8007F94C(u32 **dl);
void func_8007F3D4(u32 **dl, s32 *rect, s32 a2, s32 a3, s32 a4, s32 alpha);
void func_8005C21C(CharFrame *f, f32 *up, f32 *right, s32 a3);
void func_8000B908(void);
void func_8000B998(void *cam, CombatHandler *h);
void func_8000BCA8(void *cam, CombatHandler *h);
void func_8000BE80(void *cam, CombatHandler *h);
void func_8000C07C(void *cam, CombatHandler *h);
void func_80087E70(void *m, void *cam);
void func_8008861C(void *m);
void func_8008004C(u32 **dl, void *m);
void func_80080170(u32 **dl, void *m);
void func_800801F4(f32 *a, f32 *b);
void func_80088068(void *m, s32 angle);
void func_80088100(void *m, void *m2);
void func_80084CB0(u32 **dl, void *sprite, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void func_80083A64(u32 **dl, void *image, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void func_80082210(void *font, u32 **dl, s32 mode);
void func_80082518(void *font, u32 **dl, const char *s, f32 x, f32 y, s32 a5, f32 *up, f32 *right, s32 flags);
void func_800824CC(void *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void func_8002AC80(ScreenFade *f, u32 **dl);
void Game_EndFrame(void);
void Hud_Draw(void);
void Hud_Bevel(s32 *rect, s32 r, s32 g, s32 b, s32 a, s32 raised);
void Hud_DrawMeterSeg(HudMeterSeg *m, s32 right);
void Hud_Gradient(s32 x0, s32 y0, s32 r0, s32 g0, s32 b0, s32 a0, s32 x1, s32 y1, s32 r1, s32 g1, s32 b1, s32 a1,
                  s32 across);
void ComboBanner_Draw(ComboBanner *b, f32 x, f32 dir);
void func_80082564(void *font, u32 **dl, const char *s, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags); /* (see-through) */
void Combat_DrawPause(void);
extern HudText B_800F84D0, B_800F1348, B_800F7A90;  /* the pause menu: title, first and second option */

/* ---- fighters and after-images ---- */
typedef struct AfterImage {               /* a fading copy of a fighter's sprite (0x3C bytes) */
    /* 0x00 */ s8  timer;                 /* 16 -> 0 */
    /* 0x01 */ u8  flags;                 /* mirroring */
    /* 0x02 */ u8  colour[4][3];          /* the four corners' light colours */
    /* 0x0E */ u8  pad0E[2];
    /* 0x10 */ f32 pos[3];
    /* 0x1C */ f32 up[3];
    /* 0x28 */ f32 right[3];
    /* 0x34 */ CharObj *obj;
    /* 0x38 */ s32 frame;
} AfterImage;
extern AfterImage B_800E0E50[8], B_800E1030[8];   /* fighter 1's, fighter 2's */
extern u8 B_800F5B78, B_800F8760, B_800F2D1D, B_800F7E58;   /* the corners' alpha */
void func_80087DD0(f32 *v);                                  /* normalise */
void func_800836B0(CharFrame *f, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags,
                   f32 *c0, f32 *c1, f32 *c2, f32 *c3);       /* draw a sprite; gives its corners */
void func_80085FE4(u32 **dl, CharFrame *f, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
s64  func_800A4090(s64 a, s64 b);                            /* 64-bit remainder */

/* ---- COMBAT.C ---- */
void Fighter_DrawLit(CharFrame *f, f32 x, f32 y, f32 z, f32 *up, f32 *right, s32 flags);
void AfterImage_Capture(cfCharacter *c, CharObj *obj, AfterImage *a);
void AfterImages_Draw(AfterImage *a);
void AfterImages_Update(void);
void Combat_DrawFighters(void);
void Fighter_DrawShadow(cfCharacter *c, CharObj *obj);
void Fighter_DrawProjectiles(cfCharacter *c, CharObj *obj, s32 layer);
void Fighter_Draw(cfCharacter *c, CharObj *obj, s32 layer);
extern s32 B_800F7EC8;
extern f32 B_800F8790[3];                  /* the camera's forward direction (gCamera + 0x20) */
void func_8008144C(u32 **dl, u8 *palette, s32 a2);           /* load a palette */
void func_80087FD0(void *m, s32 angle);                      /* rotation about y */
void func_80097740(void *m, f32 angle, f32 ax, f32 ay, f32 az); /* rotation about an axis */
void func_8009708C(void *m, f32 x, f32 y, f32 z, f32 *ox, f32 *oy, f32 *oz);   /* transform a point */
typedef struct Vtx16 {                    /* an N64 vertex (Vtx_t) */
    s16 x, y, z;
    u16 flag;
    s16 s, t;
    u8  r, g, b, a;
} Vtx16;
extern Vtx16 *B_800F77D0;                  /* where this frame's vertices go */
#define gVtx B_800F77D0
void func_8007FB08(u32 **dl, s32 geometryMode);
void func_8007FB90(u32 **dl, u32 mode1, u32 mode2);          /* render mode */
void func_8007FBE8(u32 **dl, s32 combine);
void Fx_DrawDebris(void);
void Fx_DrawGlows(void);
void Fx_DrawBursts(void);
void Fx_DrawFlashes(void);
void Fx_DrawSparks(void);
void Fx_DrawRings(void);
void Fx_DrawSmokeFlat(void);
void Fx_DrawBitsFlat(void);
void Fx_DrawSmokeInAir(void);
void Fx_DrawSmokeLanded(void);
void Fx_DrawBitsInAir(void);
void Fx_DrawBitsLanded(void);                     /* arena number (15: the dark one) */
extern f32 B_800F8780[3];                  /* the camera's up direction (gCamera + 0x10) */
void Combat_DrawLoop(void);
void Combat_LogicThread(void *arg);
void Combat_SetCamera(void);
void Combat_DebugCamera(void);
void Combat_UpdateBars(void);
void ComboBanner_ShowHits(ComboBanner *b, s32 hits);
s32  Combat_Start(GameSettings *g, s32 a1, s32 a2);
s32  func_80032028(GameSettings *g, s32 a1, s32 a2);
s32  Combat_StartAlt(GameSettings *g);
void Combat_FadeOut(void);
void HudText_Set(HudText *t, void *font, const char *s, s32 align);
void ComboBanner_Start(ComboBanner *b);
void ComboBanner_Update(ComboBanner *b);
void Combat_PlaySounds(void);
void Combat_SetLightColour(f32 x, f32 y, f32 z);
void func_80038574(void);
void func_8003A134(void *f, void *c);
void func_80041CD4(void);
void func_80041CE0(void);
void func_80041CEC(void);

#endif
