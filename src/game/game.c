/*
 * The game flow (0x80000780-0x80003CC0, probably GAME.C): start-up, the attract loop (licence,
 * intro, title menu, biographies and computer-vs-computer fights as demos), the logic thread
 * that reads the controllers, checks the secret codes and runs every screen's logic, each
 * screen's X_Run (set up, frames until it finishes, shut down, report leaked memory), arcade
 * mode (the ladder of opponents, continues, the boss and the endings) and VS mode.
 *
 * Two threads share the screens: the logic thread (Game_Render) calls every screen's X_Draw
 * once a tick, the main thread calls X_Frame once a frame; the lock B_800DB800 keeps them apart.
 */
#include "common.h"
#include "mpfplat.h"
#include "mpfmem.h"
#include "combat.h"
#include "char.h"
#include "cf3char.h"

/* ---- the screens */
void License_Init(s32 noFadeIn);
void License_Shutdown(void);
s32  License_Draw(void);
s32  License_Frame(void);
void Intro_Init(void);
void Intro_Shutdown(void);
s32  Intro_Draw(void);
s32  Intro_Frame(void);
void Intro_Control(void);
void Menus_Init(s32 fromTitle, s32 fadeType);
void Menus_Shutdown(void);
s32  Menus_Draw(void);
s32  Menus_Frame(void);
void CharBio_Init(s32 character, s32 costume, s32 mode);
void CharBio_Shutdown(void);
s32  CharBio_Draw(void);
s32  CharBio_Frame(void);
void CharSel_Init(s32 *char1, s32 *costume1, s32 *char2, s32 *costume2, s32 flags);
void CharSel_Shutdown(void);
s32  CharSel_Draw(void);
s32  CharSel_Frame(void);
void VS_Init(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 *arena, s32 select);
void VS_Shutdown(void);
s32  VS_Draw(void);
s32  VS_Frame(void);
void Continue_Init(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 side);
void Continue_Shutdown(void);
s32  Continue_Draw(void);
s32  Continue_Frame(void);
void Credits_Init(s32 staff, s32 ending);
void Credits_Shutdown(void);
s32  Credits_Draw(void);
s32  Credits_Frame(void);
void ArenaSel_Init(void);
s32  ArenaSel_Draw(void);
void ArenaSel_Frame(void);

/* ---- this file */
void Game_Init(void);
void Game_EndFrame(void);
void Game_MainLoop(void);
void Game_Render(void *arg);
void Game_ResetMatch(GameSettings *g);
s32  License_Run(s32 noFadeIn);
#ifndef PORT_WARNING_SCREEN
#define PORT_WARNING_SCREEN 0               /* (the port) the warning screen: off for now */
#endif
#ifdef TARGET_PORT
void Warning_Init(void);                   /* (the port) WARNING.C */
void Warning_Shutdown(void);
s32  Warning_Draw(void);
s32  Warning_Frame(void);
#if PORT_WARNING_SCREEN
static void Warning_Run(void);
#endif
#endif
s32  Intro_Run(void);
s32  Menus_Run(s32 fromTitle, s32 fadeType);
s32  CharBio_Run(s32 character, s32 costume, s32 mode);
s32  CharSel_Run(s32 *char1, s32 *costume1, s32 *char2, s32 *costume2, s32 flags, s32 keepPicks);
void VS_Run(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 *arena, s32 select);
s32  Continue_Run(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 side);
void Credits_Run(s32 staff, s32 ending);
void ArenaSel_Run(void);
s32  Combat_Run(s32 char1, s32 char2, s32 arena);
s32  Combat_Run2(GameSettings *g);
s32  Game_ArcadeMode(s32 side);
void Game_VSMode(void);

/* ---- elsewhere */
extern MPF_Mutex B_800DB800;               /* the screens' lock */
extern OSMesgQueue B_800D6818;             /* a message every retrace */
extern OSMesg B_800D6810;
extern u8 B_800D6830[];                    /* the logic thread */
extern u32 B_800DB828, B_800DB82C;         /* ticks since the intro started (64-bit) */
extern u8 *D_800AA1C8;                     /* a notice drawn at the bottom of every frame */
extern s32 D_800AA164, D_800AA168;         /* codes: players 1 / 2 played by the computer */
extern s32 D_800AA1A4;                     /* code: leave the attract loop (debug fights) */
extern s32 D_800AA4FC;                     /* VS mode's arena */
extern s8  D_800AB220[];                   /* each arena's music */
extern s32 D_800ADA10[];                   /* each fighter's arena */
extern s32 B_800F8DBC;
extern void *B_800F7E6C, *B_800F1330, *B_800F8768;  /* the sound banks: menus, ... */
extern s32 B_800F88E4;
extern s8  B_800DB830[17], B_800DB848[17]; /* the opponents beaten, from each side */
extern s8  B_800DB859[9];                  /* the order the ladder's opponents come in */
extern s8  D_800C45EC[][9];                /* each fighter's ladder: 8 opponents, then the boss */
extern s32 D_800AA1CC[][12];               /* each fighter's boss fight set-up (GameSettings 0x84-0xB0) */
extern u32 D_800AA1B0[];                   /* the buttons the button set-up's choices stand for */
extern s32 D_800AE408;                     /* the options: difficulty, */
extern s32 D_800AE420;                     /* ... time limit (0 = on), */
extern s32 D_800AE42C;                     /* ... throws (0 = on), */
extern s32 D_800AE438;                     /* ... VS arena select, */
extern s32 D_800AE444;                     /* ... spillits, */
extern s32 D_800AE5EC, D_800AE5F8, D_800AE604, D_800AE610;  /* ... the HUD's parts */
extern s32 D_800AE704[6], D_800AE71C[6];   /* ... and the two players' buttons */
extern const char D_800C44C0[], D_800C44D8[], D_800C44F4[], D_800C4510[], D_800C452C[],
    D_800C4548[], D_800C4564[], D_800C457C[], D_800C4598[], D_800C45D0[], D_800C52A8[];

/* the secret codes: a button sequence typed with Z or L held (per controller) */
typedef struct Code {                      /* 0x24 bytes */
    u32 seq[6];                            /* (0-terminated) */
    s32 entered;
    s32 pos[2];                            /* how far each controller has got */
} Code;
extern Code D_800AA0A4, D_800AA0C8, D_800AA0EC, D_800AA110;

s32  func_80031460(s32 *char1, s32 *char2, s32 *arena);   /* debug fight select */
void func_8000A590(void);
void func_8002C92C(void);
void *func_80078E44(u32 rom, u32 size, u32 end);           /* load a sound bank */
void n64GameSched_CheckinCPU(void);
void func_80080590(u32 **dl, void *image, s32 x, s32 y, s32 a4, s32 a5);
void mpFatalError(const char *msg);

static void Lock(MPF_Mutex *m) {
    if (m->owner == osGetThreadId(NULL)) {
        m->count++;
    } else {
        osRecvMesg(&m->mq, NULL, 1);
        m->owner = osGetThreadId(NULL);
        m->count++;
    }
}

static void Unlock(MPF_Mutex *m) {
    if (m->owner == osGetThreadId(NULL)) {
        if (--m->count == 0) {
            m->owner = 0;
            osSendMesg(&m->mq, NULL, 0);
        }
    }
}

#define FREEMEM() mpLinkedMemoryManager_GetFreeMem(&gN64MemoryManager)

/* 0x80000780: start-up, then the attract loop; after it, debug fights for ever */
void Game_Init(void) {
    GameSettings g;
    s32 char1 = 12, char2 = 12, arena = 2;
    u32 size, b;

    CharMenuSound_Init();
    LoadCharacterCommonDataOnce();
    CharFrameCache_Init();
    func_8000A590();
    func_8002C92C();
    if (!B_800F88EC) {
        size = 0xFC8D20 - 0xFC8660;
        D_800AA1C8 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
        RomCopy(D_800AA1C8, 0xFC8660, size);
    }
    gScreenFade.active = 0;
    gScreenFade.v4 = 0;
    gScreenFade.type = 0;
    gScreenFade.level = 1.0f;
    gScreenFade.speed = 1.0f;
    gScreenFade.rgb[0] = 0;
    gScreenFade.rgb[1] = 0;
    gScreenFade.rgb[2] = 0;
    ((s32 *)&gScreenFade)[6] = 0;
    ((s32 *)&gScreenFade)[7] = 0;
    gScreenFade.v4 = 1;
    B_800F7E6C = func_80078E44(0xD53510, 0xD54AF0 - 0xD53510, 0xD54AF0);
    B_800F1330 = func_80078E44(0xF1EF70, 0xF20530 - 0xF1EF70, 0xF20530);
    B_800F8768 = func_80078E44(0xF040C0, 0xF055C0 - 0xF040C0, 0xF055C0);
    func_8007C5BC(0x2000, 0x100, 0xDAC);
    Game_MainLoop();
    while (func_80031460(&char1, &char2, &arena)) {
        mpDebugPrintf(D_800C44C0, FREEMEM());
        bzero(&g, sizeof(g));
        g.v0 = char1;
        g.v4 = 0;
        g.v8 = (B_800F2D18 & 0x20) ? 4 : -1;           /* Z held: the computer plays */
        g.vC = char2;
        g.v14 = (B_800F8C84 & 0x20) ? 4 : -1;
        g.v10 = 1;
        g.v18 = (s32)guRandom() % 2 + 1;
        g.v1C = arena;
        b = B_800F77D8;
        g.v20 = (b >> 13) & 1;
        g.v24 = (b & 0x4000) ? 99 : -1;
        g.v28 = 0;
        g.v2C = 1;
        g.v30 = (b >> 4) & 1;
        g.showHealth = 1;
        g.showSuper = 1;
        g.showDizzy = 1;
        g.showPortraits = 1;
        g.v44 = 1.0f;
        g.v48 = 1.0f;
        g.v4C = 1;
        g.v50 = ((b >> 4) ^ 1) & 1;
        g.v54 = 1;
        g.v58 = 1;
        g.v5C = 1;
        g.buttons1[0] = 0x4000; g.buttons1[1] = 2; g.buttons1[2] = 8;
        g.buttons1[3] = 0x8000; g.buttons1[4] = 4; g.buttons1[5] = 1;
        g.buttons2[0] = 0x4000; g.buttons2[1] = 2; g.buttons2[2] = 8;
        g.buttons2[3] = 0x8000; g.buttons2[4] = 4; g.buttons2[5] = 1;
        Combat_StartAlt(&g);
        mpDebugPrintf(D_800C44C0, FREEMEM());
    }
}

/* 0x80000AE0: the end of every screen's frame: the notice at the bottom, then hand over */
void Game_EndFrame(void) {
    u32 *dl;
    u8 *img;

    if (D_800AA1C8 != NULL) {
        dl = gDL; gDL = dl + 2; dl[0] = 0xE7000000; dl[1] = 0;
        func_8007EB48(&gDL);
        img = D_800AA1C8;
        *(s16 *)(img + 0xA) = *(s16 *)(img + 6) / 2;
        *(s16 *)(img + 0xC) = *(u16 *)(img + 8) - 1;
        func_80080590(&gDL, img, 0, B_800F5D00 / 2, B_800F5BEC - 18, 0);
        dl = gDL; gDL = dl + 2; dl[0] = 0xE7000000; dl[1] = 0;
    }
    n64GameSched_CheckinCPU();
}

/* 0x80000BC0: the attract loop - licence (every 8th time round), intro (every 4th), the
 * title menu, and if that is left alone a demo: a biography or a computer fight */
void Game_MainLoop(void) {
    s32 seen[17];
    s32 firstTime = 1, skip = 0, lastBio = -1, n, i, r, title, pick, opp, k;

    mpMutex_Init(&B_800DB800);
    osCreateMesgQueue(&B_800D6818, &B_800D6810, 1);
    func_800999D0(B_800D6830, n64NextThreadId(), Game_Render, NULL, &B_800DB800, 20);
    func_8009A0D0(B_800D6830);
    func_800779F4(&B_800D6818);
    for (i = 0; i < 17; i++) seen[i] = 1;
#if defined(TARGET_PORT) && PORT_WARNING_SCREEN
    Warning_Run();                          /* (the port) once, at power-on */
#endif
    for (;;) {
        D_800AA164 = 0;
        D_800AA168 = 0;
        if (D_800AA1A4) break;
        for (n = 0;; n++) {
            title = 0;
            if (!skip) {
                if (n & 7) firstTime = 0;
                else {
#if defined(TARGET_PORT) && PORT_WARNING_SCREEN
                    /* (after the warning screen's fade to black: fading in, as later times) */
                    if (License_Run(0)) skip = firstTime == 0;   /* skipped: no intro */
#else
                    if (License_Run(firstTime)) skip = firstTime == 0;   /* skipped: no intro */
#endif
                    firstTime = 0;
                }
            }
            if (D_800AA1A4) goto out;
            if (!skip && !(n & 3)) title = Intro_Run() == 0;
            if (D_800AA1A4) goto out;
            skip = 0;
            r = Menus_Run(title, n & 1);
            if (r != 3) {
                if ((u32)r < 2) {
                    skip = 1;
                    if (Game_ArcadeMode(r)) skip = 0;
                } else if (r == 2) {
                    Game_VSMode();
                    skip = 1;
                }
                break;
            }
            if (D_800AA1A4) goto out;
            if (!(n & 1)) {                             /* demo: a biography not shown lately */
                gScreenFade.rgb[2] = 0; gScreenFade.rgb[1] = 0; gScreenFade.rgb[0] = 0;
                for (i = 0; i < 17 && seen[i]; i++) {
                }
                if (!(i < 17)) {
                    for (i = 0; i < 17; i++) seen[i] = 0;
                }
                seen[7] = 1;
                do {
                    pick = 0;
                    guRandom();
                    k = (s32)guRandom() % 17;
                    if (k >= 0) {
                        for (;;) {
                            if (!(pick < 17)) pick = 0;
                            if (seen[pick] == 0 && --k <= 0) break;
                            pick++;
                            if (k < 0) break;
                        }
                    }
                } while (pick == lastBio);
                lastBio = pick;
                seen[pick] = 1;
                skip = CharBio_Run(pick, 0, 0);
            } else {                                    /* demo: a computer fight */
                pick = lastBio >= 0 ? lastBio : 0;
                do {
                    opp = (s32)guRandom() % 17;
                } while (opp == pick || opp == 7);
                skip = Combat_Run(pick, opp, D_800ADA10[pick]);
                gScreenFade.rgb[2] = 0; gScreenFade.rgb[1] = 0; gScreenFade.rgb[0] = 0;
            }
            gScreenFade.rgb[2] = 0; gScreenFade.rgb[1] = 0; gScreenFade.rgb[0] = 0;
            if (D_800AA1A4) goto out;
        }
    }
out:
    func_80077AF4(&B_800D6818);
}

/* one controller's step through a secret code */
static void CodeStep(Code *c, s32 p, u32 pressed) {
    if (c->entered) return;
    if (pressed & c->seq[c->pos[p]]) {
        if (c->seq[++c->pos[p]] == 0) {
            c->pos[p] = 0;
            func_800797EC(B_800F7E6C, 0, 0, 0x1A, 0x7FFF, 0, 0, 1.0f, 1);
            c->entered = 1;
        }
    } else {
        c->pos[p] = 0;
    }
}

/* the last code is laid out differently: one flag per controller for "played by the computer" */
static void CodeStep2(u32 *seq, s32 *entered, s32 *pos, u32 pressed) {
    if (*entered) return;
    if (pressed & seq[*pos]) {
        if (seq[++*pos] == 0) {
            *pos = 0;
            func_800797EC(B_800F7E6C, 0, 0, 0x1A, 0x7FFF, 0, 0, 1.0f, 1);
            *entered = 1;
        }
    } else {
        *pos = 0;
    }
}

extern u32 D_800AA134[], D_800AA174[];
extern s32 D_800AA16C, D_800AA170, D_800AA1A8, D_800AA1AC;

/* 0x80000F6C: the logic thread: every retrace, read the controllers, check the codes and run
 * every screen's logic (the ones not running return at once) */
void Game_Render(void *arg) {
    u32 held;

    (void)arg;
    for (;;) {
        osRecvMesg(&B_800D6818, NULL, 1);
        Lock(&B_800DB800);
        func_80076DB8();
        B_800F2D18 |= B_800F8C88;
        held = B_800F2D18;
        B_800F4E54 |= B_800F5BE4;
        B_800F8C84 |= B_800F15E8;
        B_800F4E6C |= B_800F4E60;
        B_800F77D8 |= B_800F7F50;
        B_800F8C78 |= B_800F15DC;
        if (!(held & 0x2020)) {
            D_800AA0A4.pos[0] = 0;
        } else if (!(B_800F2D18 & 0x10)) {
            if (B_800F4E54) {
                CodeStep(&D_800AA0A4, 0, B_800F4E54);
                CodeStep(&D_800AA0C8, 0, B_800F4E54);
                CodeStep(&D_800AA0EC, 0, B_800F4E54);
                CodeStep(&D_800AA110, 0, B_800F4E54);
                CodeStep2(D_800AA134, &D_800AA164, &D_800AA16C, B_800F4E54);
                CodeStep2(D_800AA174, &D_800AA1A4, &D_800AA1A8, B_800F4E54);
            }
            B_800F4E54 = 0;
        }
        if (!(B_800F8C84 & 0x2020)) {
            D_800AA0A4.pos[1] = 0;
        } else if (!(B_800F8C84 & 0x10)) {
            if (B_800F4E6C) {
                CodeStep(&D_800AA0A4, 1, B_800F4E6C);
                CodeStep(&D_800AA0C8, 1, B_800F4E6C);
                CodeStep(&D_800AA0EC, 1, B_800F4E6C);
                CodeStep(&D_800AA110, 1, B_800F4E6C);
                CodeStep2(D_800AA134, &D_800AA168, &D_800AA170, B_800F4E6C);
                CodeStep2(D_800AA174, &D_800AA1A4, &D_800AA1AC, B_800F4E6C);
            }
            B_800F4E6C = 0;
        }
        B_800F77D8 = B_800F2D18 | B_800F8C84;
        B_800F8C78 = B_800F4E54 | B_800F4E6C;
#ifdef TARGET_PORT
        Warning_Draw();
#endif
        License_Draw();
        Intro_Draw();
        CharSel_Draw();
        VS_Draw();
        Continue_Draw();
        Credits_Draw();
        ArenaSel_Draw();
        CharBio_Draw();
        Menus_Draw();
        if (++B_800DB82C == 0) B_800DB828++;
        Unlock(&B_800DB800);
    }
}

/* 0x800019BC: a match with the options' settings */
void Game_ResetMatch(GameSettings *g) {
    s32 i;

    bzero(g, sizeof(*g));
    g->v0 = 0;
    g->v4 = 0;
    g->v8 = D_800AE408;
    g->vC = 0;
    g->v10 = 0;
    g->v14 = D_800AE408;
    g->v18 = 0;
    g->v1C = 0;
    g->v20 = 0;
    g->v24 = D_800AE420 ? -1 : 90;
    g->v28 = D_800AE444;
    g->v2C = D_800AE42C == 0;
    g->v30 = 0;
    g->showHealth = D_800AE5EC != 0;
    g->showSuper = D_800AE604 != 0;
    g->showDizzy = D_800AE5F8 != 0;
    g->showPortraits = D_800AE610 != 0;
    g->v44 = 1.0f;
    g->v48 = 1.0f;
    g->v4C = 1;
    g->v50 = 1;
    g->v54 = 0;
    g->v58 = 1;
    g->v5C = 1;
    for (i = 0; i < 6; i++) g->buttons1[i] = (u16)D_800AA1B0[D_800AE704[i]];
    for (i = 0; i < 6; i++) g->buttons2[i] = (u16)D_800AA1B0[D_800AE71C[i]];
}

/* ---- each screen's run: set up, a frame at a time until it finishes, shut down */

/* 0x80001C04: returns whether it was skipped */
s32 License_Run(s32 noFadeIn) {
    u32 before = FREEMEM(), after;
    s32 r;

    Lock(&B_800DB800);
    License_Init(noFadeIn);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = License_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    License_Shutdown();
    Unlock(&B_800DB800);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C44D8, before - after);
    return r != 0;
}

#if defined(TARGET_PORT) && PORT_WARNING_SCREEN
/* (the port) the warning screen, as License_Run */
static void Warning_Run(void) {
    s32 r;

    Lock(&B_800DB800);
    Warning_Init();
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = Warning_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    Warning_Shutdown();
    Unlock(&B_800DB800);
}
#endif

/* 0x80001E80: returns whether it was skipped */
s32 Intro_Run(void) {
    u32 before, after;
    s32 r;

    func_8007C5BC(0x2000, 0x100, 0xDAC);
    before = mpLinkedMemoryManager_GetUsedMem(&gN64MemoryManager);
    Lock(&B_800DB800);
    Intro_Init();
    Unlock(&B_800DB800);
    B_800DB828 = 0;
    B_800DB82C = 0;
    do {
        Lock(&B_800DB800);
        Intro_Control();
        func_8007C850();
        r = Intro_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    Intro_Shutdown();
    func_8007C5BC(0x2000, 0x100, 0xDAC);
    Unlock(&B_800DB800);
    after = mpLinkedMemoryManager_GetUsedMem(&gN64MemoryManager);
    if (after != before) mpDebugPrintf(D_800C44F4, after - before);
    return r != 0;
}

/* 0x80002138: 0/1 arcade (the player), 2 VS, 3 left alone */
s32 Menus_Run(s32 fromTitle, s32 fadeType) {
    u32 before = FREEMEM(), after;
    s32 r;

    Lock(&B_800DB800);
    Menus_Init(fromTitle, fadeType);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = Menus_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    Menus_Shutdown();
    Unlock(&B_800DB800);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C4510, before - after);
    return r;
}

/* 0x800023BC: returns whether it was skipped */
s32 CharBio_Run(s32 character, s32 costume, s32 mode) {
    u32 before = FREEMEM(), after;
    s32 r;

    Lock(&B_800DB800);
    CharBio_Init(character, costume, mode);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = CharBio_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    CharBio_Shutdown();
    Unlock(&B_800DB800);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C452C, before - after);
    return r == 1;
}

/* 0x8000264C */
s32 CharSel_Run(s32 *char1, s32 *costume1, s32 *char2, s32 *costume2, s32 flags, s32 keepPicks) {
    u32 before = FREEMEM(), after;
    s32 r;

    if (!keepPicks) B_800F8DBC = 0;
    Lock(&B_800DB800);
    CharSel_Init(char1, costume1, char2, costume2, flags);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = CharSel_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    CharSel_Shutdown();
    Unlock(&B_800DB800);
    B_800F8DBC = 1;
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C4548, before - after);
    return r;
}

/* 0x80002914 */
void VS_Run(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 *arena, s32 select) {
    u32 before = FREEMEM(), after;
    s32 r;

    B_800F8DBC = 0;
    Lock(&B_800DB800);
    VS_Init(char1, costume1, char2, costume2, arena, select);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = VS_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    VS_Shutdown();
    Unlock(&B_800DB800);
    B_800F8DBC = 1;
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C4564, before - after);
}

/* 0x80002BE0: 0 continue, 1 game over */
s32 Continue_Run(s32 char1, s32 costume1, s32 char2, s32 costume2, s32 side) {
    u32 before = FREEMEM(), after;
    s32 r;

    Lock(&B_800DB800);
    Continue_Init(char1, costume1, char2, costume2, side);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = Continue_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    Continue_Shutdown();
    Unlock(&B_800DB800);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C457C, before - after);
    return r;
}

/* 0x80002E8C */
void Credits_Run(s32 staff, s32 ending) {
    u32 before = FREEMEM(), after;
    s32 r;

    Lock(&B_800DB800);
    Credits_Init(staff, ending);
    Unlock(&B_800DB800);
    do {
        Lock(&B_800DB800);
        func_8007C850();
        r = Credits_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    } while (r == -1);
    func_8007CCA4();
    Lock(&B_800DB800);
    Credits_Shutdown();
    Unlock(&B_800DB800);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C4598, before - after);
}

/* 0x8000310C: the arena viewer, for ever (nothing calls it) */
void ArenaSel_Run(void) {
    FREEMEM();
    Lock(&B_800DB800);
    ArenaSel_Init();
    Unlock(&B_800DB800);
    for (;;) {
        Lock(&B_800DB800);
        func_8007C850();
        ArenaSel_Frame();
        Game_EndFrame();
        Unlock(&B_800DB800);
    }
}

/* 0x80003290: the attract mode's computer-vs-computer fight */
s32 Combat_Run(s32 char1, s32 char2, s32 arena) {
    GameSettings g;
    u32 before = FREEMEM(), after;

    Game_ResetMatch(&g);
    g.v0 = char1;
    g.vC = char2;
    g.v4 = 0;
    g.v10 = char1 == char2;
    g.v18 = 1;
    g.v1C = arena;
    g.v20 = (s32)guRandom() % 2;
    g.v24 = -1;
    g.v4C = 0;
    func_80077AF4(&B_800D6818);
    Combat_Start(&g, 0, 0);
    func_800779F4(&B_800D6818);
    func_8007C5BC(0x2000, 0x100, 0xDAC);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C45D0, before - after);
    return 0;
}

/* 0x80003394: a fight: 0 over, 1 quit, 2 another player joined in */
s32 Combat_Run2(GameSettings *g) {
    u32 before = FREEMEM(), after;
    s32 r;

    func_80077AF4(&B_800D6818);
    r = Combat_StartAlt(g);
    func_800779F4(&B_800D6818);
    func_8007C5BC(0x2000, 0x100, 0xDAC);
    after = FREEMEM();
    if (after != before) mpDebugPrintf(D_800C45D0, before - after);
    return r;
}

/* 0x80003428: arcade mode. `side` is the player (0 = controller 1). The opponents come in a
 * random order, the boss last; a second player can join in (CharSel result 3, fight result 2)
 * for a two-player fight whose winner carries on. Returns 1 when the game was finished or
 * over, 0 when quit. */
s32 Game_ArcadeMode(s32 side) {
    GameSettings g;
    s32 char1 = -1, costume1 = 0, char2 = -1, costume2 = 0;
    s32 first = 1, pick = 1, keepOrder = 0, joined = 0;
    s32 flags, r, i, j, k, me, myCostume, opp, oppCostume;
    s8 *beaten;

    bzero(B_800DB830, 17);
    bzero(B_800DB848, 17);
top:
    if (joined || pick) {
        if (joined) flags = side ? 0xB : 7;
        else flags = side ? 2 : 1;
        if (first) {
            flags |= 0x10;
            first = 0;
        }
        r = CharSel_Run(&char1, &costume1, &char2, &costume2, flags, 1);
        if (r == 0) return 0;
        if (r == 3) joined = 1;
    }
    pick = 0;
    if (joined) {                                       /* the two players fight */
        Game_ResetMatch(&g);
        g.v0 = char1;
        g.v4 = costume1;
        g.v8 = D_800AA164 ? 4 : -1;
        g.vC = char2;
        g.v10 = costume2;
        g.v14 = D_800AA168 ? 4 : -1;
        g.v18 = side ? 2 : 1;
        g.v1C = (s32)guRandom() % 26;
        g.v20 = (s32)guRandom() % 2;
        g.v58 = 0;
        CharMenuSound_Load(D_800AB220[g.v1C]);
        VS_Run(char1, costume1, char2, costume2, &g.v1C, 0);
        r = Combat_Run2(&g);
        if (r == 1) return 0;
        if (r == 2) mpFatalError(D_800C52A8);
        side = g.v78 != 1;                              /* the winner carries on */
    }
    joined = 0;
    if (!keepOrder) {                                   /* the opponents' order */
        for (i = 0; i < 8; i++) {
            do {
                k = (s32)guRandom() % 8;
                for (j = 0; j < i; j++) {
                    if (B_800DB859[j] == k) break;
                }
            } while (j < i);
            B_800DB859[i] = k;
        }
        B_800DB859[8] = 8;
    }
    keepOrder = 0;
    me = side ? char2 : char1;
    myCostume = side ? costume2 : costume1;
    beaten = side ? B_800DB848 : B_800DB830;
    for (i = 0; i < 9; i++) {
        if (!beaten[D_800C45EC[me][B_800DB859[i]]]) break;
    }
    if ((i == 8 && D_800AE408 == 0) || !(i < 9)) {     /* the end (no boss on the easiest) */
        if (D_800AE408 >= 2) CharBio_Run(me, myCostume, 1);
        Credits_Run(D_800AE408 >= 2, D_800AE408 >= 4);
        return 1;
    }
    opp = D_800C45EC[me][B_800DB859[i]];
    oppCostume = (opp == me && myCostume == 0) ? 1 : 0;
    Game_ResetMatch(&g);
    if (!side) {
        g.v0 = me;
        g.v4 = myCostume;
        g.v8 = -1;
        g.vC = opp;
        g.v10 = oppCostume;
        g.v18 = 2;
        if (D_800AA164) g.v8 = 4;
    } else {
        g.vC = me;
        g.v10 = myCostume;
        g.v14 = -1;
        g.v0 = opp;
        g.v4 = oppCostume;
        g.v18 = 1;
        if (D_800AA168) g.v14 = 4;
    }
    g.v1C = D_800ADA10[opp];
    if (g.v1C == 12 || g.v1C == 20) g.v20 = 0;
    else g.v20 = (s32)guRandom() % 2;
    g.v58 = 0;
    if (opp == D_800C45EC[me][8]) {                     /* the boss: its own set-up */
        g.v84 = D_800AA1CC[me][0];
        g.v88 = D_800AA1CC[me][1];
        g.v8C = D_800AA1CC[me][2];
        g.v90 = D_800AA1CC[me][3];
        g.v94 = D_800AA1CC[me][4];
        g.v98 = D_800AA1CC[me][5];
        g.v9C = D_800AA1CC[me][6];
        g.vA0 = D_800AA1CC[me][7];
        g.vA4 = D_800AA1CC[me][8];
        g.vA8 = D_800AA1CC[me][9];
        g.vAC = D_800AA1CC[me][10];
        g.vB0 = D_800AA1CC[me][11];
        g.v5C = 0;
    }
    CharMenuSound_Load(D_800AB220[g.v1C]);
    VS_Run(g.v0, g.v4, g.vC, g.v10, &g.v1C, 0);
    r = Combat_Run2(&g);
    if (r == 1) return 0;
    if (r == 2) {
        joined = 1;
        goto top;
    }
    if (r != 0) goto top;
    if (g.v78 == (side ? 2 : 1)) {                      /* won */
        beaten[opp] = 1;
        goto top;
    }
    if (Continue_Run(me, myCostume, opp, oppCostume, side ? 2 : 1) == 1) return 1;
    pick = 1;
    keepOrder = 1;
    goto top;
}

/* 0x80003ACC: VS mode, until character select is left */
void Game_VSMode(void) {
    GameSettings g;
    s32 char1 = -1, costume = 0, char2 = -1;

    if (D_800AA4FC < 0) D_800AA4FC = (s32)guRandom() % 26;
    for (;;) {
        D_800AA164 = 0;
        D_800AA168 = 0;
        /* (both players' costumes go to the same place) */
        if (!CharSel_Run(&char1, &costume, &char2, &costume, 0x33, D_800AE438 == 0)) return;
        if (!D_800AE438) {
            D_800AA4FC = (s32)guRandom() % 26;
            CharMenuSound_Load(D_800AB220[D_800AA4FC]);
        }
        VS_Run(char1, 0, char2, costume, &D_800AA4FC, D_800AE438 != 0);
        Game_ResetMatch(&g);
        g.v0 = char1;
        g.v4 = 0;
        g.v8 = D_800AA164 ? D_800AE408 : -1;
        g.vC = char2;
        g.v10 = costume;
        g.v14 = D_800AA168 ? D_800AE408 : -1;
        g.v1C = D_800AA4FC;
        g.v58 = 0;
        Combat_Run2(&g);
    }
}
