/*
 * CONTINUE.C (0x80050380-0x80051D00): "Continue?" after losing in arcade mode - the loser
 * standing at the side, the winner's portrait with one of four taunts, a countdown from 10
 * (any button takes a second off, Start continues) and "Game Over".
 */
#ifndef CONTINUE_H
#define CONTINUE_H

#include "combat.h"

typedef struct ContinueScreen {            /* 0x3544 bytes */
    /* 0x0000 */ s32 char1, costume1;      /* the player's fighter (the one who lost) */
    /* 0x0008 */ s32 char2, costume2;      /* the winner */
    /* 0x0010 */ s32 side;                 /* 2 = player 2 */
    /* 0x0014 */ CharObj *obj;
    /* 0x0018 */ cfCharacter fighter;
    /* 0x34F8 */ u8 *portraitFile, *portrait;
    /* 0x3500 */ u8 *textFontFile, *textFont;   /* the taunt */
    /* 0x3508 */ u8 *countFontFile, *countFont; /* "Continue?" and the count */
    /* 0x3510 */ u8 *bigFontFile, *bigFont;     /* "Game Over" */
    /* 0x3518 */ s32 continued;            /* Start was pressed */
    /* 0x351C */ s32 gaveUp;               /* "Game Over" has been shown */
    /* 0x3520 */ const char *line1, *line2; /* the winner's taunt */
    /* 0x3528 */ s32 count;                /* 10 .. 0 */
    /* 0x352C */ s32 timer;                /* ticks to the next second (60) */
    /* 0x3530 */ s32 chosen;
    /* 0x3534 */ f32 fade;                 /* "Continue?" fading out at 0 */
    /* 0x3538 */ s32 gameOverTimer;        /* -1, or ticks of "Game Over" left (120) */
    /* 0x353C */ s32 result;               /* -1 while running, 0 continue, 1 game over */
    /* 0x3540 */ s32 gameOver;             /* "Game Over" is up */
} ContinueScreen;

typedef struct Taunt { const char *line1, *line2; } Taunt;

extern ContinueScreen *D_800AD868;
#define gContinue D_800AD868
extern s32 D_800AD864;                     /* which of each fighter's four taunts is next */
extern Taunt D_800AD640[][4];              /* the taunts, four per fighter */

void func_80082358(void *font, u32 **dl, const char *s, s32 a3, s32 x, s32 y, s32 a6);  /* text at a pixel position */

void Continue_Init(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 side);
void Continue_Shutdown(void);
s32  Continue_Draw(void);
s32  Continue_Frame(void);
void Continue_DrawFighter(cfCharacter *f, CharObj *obj);

#endif
