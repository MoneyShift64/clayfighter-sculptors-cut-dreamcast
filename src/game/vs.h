/*
 * VS.C (0x8004E7B0-0x80050380): the VS screen - both fighters' portraits with "VS" and the
 * arena's name over a camera flying along the arena's path, and (in VS mode) "Select Arena":
 * the camera circles the arena and the player steps through the arenas before it flies in.
 */
#ifndef VS_H
#define VS_H

#include "screen.h"

typedef struct ArenaPath {                 /* an arena's VS-screen camera path */
    s32 n;                                 /* number of points: the first is where to look */
    f32 *pts;                              /* n * (x, y, z), z negated */
} ArenaPath;

typedef struct VsScreen {                  /* 0x367C bytes */
    /* 0x0000 */ u8  stage[0x35D0];        /* the 3D stage (func_8002EA90 and friends) */
    /* 0x35D0 */ s32 skip;                 /* the stage's: set when A or Start is pressed */
    /* 0x35D4 */ s32 v35D4;
    /* 0x35D8 */ s32 char1, costume1, char2, costume2;
    /* 0x35E8 */ s32 arena;
    /* 0x35EC */ s32 *arenaOut;            /* where the chosen arena goes */
    /* 0x35F0 */ u8 *portrait1, *image1;   /* fighter 1's portrait file and its picture */
    /* 0x35F8 */ u8 *portrait2, *image2;
    /* 0x3600 */ u8 *frameFile;            /* the portrait frame: file, ... */
    /* 0x3604 */ u8 *frameBack, *frameTop; /* ... the part behind the picture and on top */
    /* 0x360C */ u8 *font2File, *font2;    /* the small font (arena names) */
    /* 0x3614 */ u8 *font1File, *font1;    /* the big font ("VS", "Select Arena") */
    /* 0x361C */ f32 slide;                /* how far apart the portraits are (80 -> 100) */
    /* 0x3620 */ s32 leaving;              /* the closing fade has started */
    /* 0x3624 */ s32 result;               /* -1 while running, 0 when done */
    /* 0x3628 */ ArenaPath *path;
    /* 0x362C */ f32 eye[3];               /* the camera */
    /* 0x3638 */ f32 at[3];                /* ... and where it looks */
    /* 0x3644 */ f32 atFrom[3], atTo[3];   /* where it looks moves from atFrom to atTo */
    /* 0x365C */ s32 n;                    /* the curve the camera flies along */
    /* 0x3660 */ f32 *pts;
    /* 0x3664 */ f32 t;                    /* how far along (0..1) */
    /* 0x3668 */ s32 ownPoints;            /* pts was allocated here */
    /* 0x366C */ s32 selecting;            /* "Select Arena" (camera circling) */
    /* 0x3670 */ s32 angle;
    /* 0x3674 */ f32 height, radius;
} VsScreen;

extern VsScreen *D_800AD630;
#define gVs D_800AD630
extern ArenaPath D_800AD4F8[26];
extern char *D_800AD5C8[26];               /* each arena's "part" name, shown before its name */
extern s8 D_800AB220[26];                  /* each arena's music */
extern f32 B_800E1230[12];                 /* a four-point path made up when the camera is far off */
extern s32 B_800F77D8, B_800F8C78;         /* buttons held, buttons just pressed */
extern void *B_800F7E6C;                   /* the menu sound bank */

const char *func_8000A6E4(s32 arena);      /* an arena's name */


void VS_Init(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 *arena, s32 select);
void VS_Shutdown(void);
s32  VS_Draw(void);
s32  VS_Frame(void);

#endif
