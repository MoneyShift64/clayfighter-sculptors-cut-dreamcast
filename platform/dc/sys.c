/*
 * The game's N64 platform functions, rewritten for the port with their original names (the
 * game's C calls these): start-up (was src/game/boot.c), reading the game data (MPFPLAT.C's
 * RomCopy), the controllers, the frame buffers and retrace (N64VIDEO.C), the frame loop
 * (N64GAMSC.C) and sound (N64AUDIO.C - silent for now).
 *
 * The game data: tools/port_assets.py turns the ROM into CFSC.DAT, with every asset the game
 * loads converted for a little-endian CPU (and compressed ones stored unpacked), plus a
 * directory, CFSC.DIR, that says where each ROM range went. RomCopy looks the address up.
 */
#include <stdio.h>
#include <string.h>
#include "port.h"
#include "n64sys.h"
#include "mpfplat.h"

/* ------------------------------------------------------------------ the game data */

typedef struct DirEntry { u32 rom, romEnd, dat, datSize; } DirEntry;   /* CFSC.DIR: little-endian */
static DirEntry *sDir;
static s32 sDirCount;

void port_rom_init(void) {
    u32 n;
    /* the directory is at the start of CFSC.DAT: a count, then the entries */
    plat_data_read(&n, 0, 4);
    sDirCount = (s32)n;
    sDir = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, n * sizeof(DirEntry), 0);
    plat_data_read(sDir, 4, n * sizeof(DirEntry));
}

/* MPFPLAT.C mpSyncROMLoad */
void RomCopy(void *dst, u32 romAddr, u32 size) {
    s32 lo = 0, hi = sDirCount - 1;
    while (lo <= hi) {                          /* the entries are sorted by ROM address */
        s32 mid = (lo + hi) / 2;
        DirEntry *e = &sDir[mid];
        if (romAddr < e->rom) hi = mid - 1;
        else if (romAddr >= e->romEnd) lo = mid + 1;
        else {
            u32 off = romAddr - e->rom;
            if (off + size > e->datSize) {
                if (off >= e->datSize) mpFatalErrorf("RomCopy %08X: past the converted data", romAddr);
                size = e->datSize - off;        /* (the N64 rounded sizes up) */
            }
            plat_data_read(dst, e->dat + off, size);
            return;
        }
    }
    mpFatalErrorf("RomCopy %08X (%X bytes): not in CFSC.DAT", romAddr, size);
}

/* ------------------------------------------------------------------ controllers */

extern s32 D_800B0984, D_800B0988, D_800B098C, D_800B0990;
extern s32 B_800F88EC;
extern s32 D_800B0B08;
extern s32 B_800E13B8[4], B_800E13C8[4], B_800E13D8[4], B_800E13E8[4], B_800E13F8[4], B_800E1408[4],
    B_800E1418[4], B_800E1428[4], B_800E1438[4];
extern s32 B_800F2D18, B_800F4E54, B_800F7F4C, B_800F5B88, B_800F8C88, B_800F5BE4;
extern s32 B_800F8C84, B_800F4E6C, B_800F5B74, B_800F5BAC, B_800F15E8, B_800F4E60;
extern s32 B_800F7D4C, B_800F4E44, B_800F8828, B_800F5D10, B_800F2D44, B_800F4E94;
extern s32 B_800F7A84, B_800F4E88, B_800F7E5C, B_800F2B60, B_800F4E58, B_800F7D50;
extern s32 B_800F7DFC, B_800F8C80, B_800F2E18, B_800F84C0, B_800F2D20, B_800F88F0;
extern s32 B_800F5D1C, B_800F8764, B_800F5D20, B_800F2E24, B_800F1340, B_800F4E84;
extern s32 B_800F8834, B_800F4E78, B_800F4ED8, B_800F7DB4, B_800F5B7C, B_800F8830;
extern s32 B_800F88E4, B_800F7D5C, B_800F7E50, B_800F7784, B_800F84C4, B_800F8818;
extern s32 B_800F77D8, B_800F8C78, B_800F5B94, B_800F133C, B_800F7F50, B_800F15DC;

static u16 sPadButtons[4];
static s8 sPadX[4], sPadY[4];

/* 0x80076B90: find the controllers (port 1 always counts, so the game never stops to ask) */
void func_80076B90(void) {
    s32 present = plat_read_pads(sPadButtons, sPadX, sPadY) | 1, i;
    D_800B0984 = D_800B0988 = D_800B098C = D_800B0990 = -1;
    for (i = 0; i < 4; i++) {
        B_800E13C8[i] = B_800E13D8[i] = B_800E13E8[i] = 0;
        B_800E13F8[i] = B_800E1408[i] = B_800E1418[i] = B_800E1428[i] = B_800E1438[i] = 0;
        B_800E13B8[i] = (present >> i) & 1;
        if (B_800E13B8[i]) {
            B_800F88EC++;
            if (D_800B0984 == -1) D_800B0984 = i;
            else if (D_800B0988 == -1) D_800B0988 = i;
            else if (D_800B098C == -1) D_800B098C = i;
            else if (D_800B0990 == -1) D_800B0990 = i;
        }
    }
}

/* 0x80076DB8: read the controllers (the logic thread, once a tick) - as the original */
void func_80076DB8(void) {
    s32 i, present = plat_read_pads(sPadButtons, sPadX, sPadY);

    for (i = 0; i < 4; i++) {
        s32 b;
        if (B_800E13B8[i] == 0) {
            /* a controller plugged in later becomes the next player */
            if (!((present >> i) & 1)) continue;
            B_800E13B8[i] = 1;
            B_800F88EC++;
            if (D_800B0988 == -1) D_800B0988 = i;
            else if (D_800B098C == -1) D_800B098C = i;
            else if (D_800B0990 == -1) D_800B0990 = i;
        }
        b = sPadButtons[i];
        B_800E13D8[i] = b;
        B_800E13E8[i] = (b ^ B_800E13C8[i]) & b;
        B_800E13C8[i] = B_800E13D8[i];
        B_800E13F8[i] = sPadX[i];
        B_800E1408[i] = sPadY[i];
        B_800E1428[i] = 0;
        if (B_800E13F8[i] < -27) B_800E1428[i] = 0x200;
        if (B_800E13F8[i] >= 28) B_800E1428[i] |= 0x100;
        if (B_800E1408[i] < -27) B_800E1428[i] |= 0x400;
        if (B_800E1408[i] >= 28) B_800E1428[i] |= 0x800;
        B_800E1438[i] = (B_800E1428[i] ^ B_800E1418[i]) & B_800E1428[i];
        B_800E1418[i] = B_800E1428[i];
    }
#define PLAYER(port, held, pressed, sx, sy, dir, dirPressed)                                   \
    if ((port) != -1) {                                                                        \
        held = B_800E13D8[port]; pressed = B_800E13E8[port]; sx = B_800E13F8[port];             \
        sy = B_800E1408[port]; dir = B_800E1428[port]; dirPressed = B_800E1438[port];           \
    } else {                                                                                   \
        held = 0; pressed = 0; sx = 0; sy = 0; dir = 0; dirPressed = 0;                         \
    }
    PLAYER(D_800B0984, B_800F2D18, B_800F4E54, B_800F7F4C, B_800F5B88, B_800F8C88, B_800F5BE4)
    PLAYER(D_800B0988, B_800F8C84, B_800F4E6C, B_800F5B74, B_800F5BAC, B_800F15E8, B_800F4E60)
    PLAYER(D_800B098C, B_800F7D4C, B_800F4E44, B_800F8828, B_800F5D10, B_800F2D44, B_800F4E94)
    PLAYER(D_800B0990, B_800F7A84, B_800F4E88, B_800F7E5C, B_800F2B60, B_800F4E58, B_800F7D50)
#undef PLAYER
#define PORT(i, held, pressed, sx, sy, dir, dirPressed)                                        \
    held = B_800E13D8[i]; pressed = B_800E13E8[i]; sx = B_800E13F8[i];                          \
    sy = B_800E1408[i]; dir = B_800E1428[i]; dirPressed = B_800E1438[i];
    PORT(0, B_800F7DFC, B_800F8C80, B_800F2E18, B_800F84C0, B_800F2D20, B_800F88F0)
    PORT(1, B_800F5D1C, B_800F8764, B_800F5D20, B_800F2E24, B_800F1340, B_800F4E84)
    PORT(2, B_800F8834, B_800F4E78, B_800F4ED8, B_800F7DB4, B_800F5B7C, B_800F8830)
    PORT(3, B_800F88E4, B_800F7D5C, B_800F7E50, B_800F7784, B_800F84C4, B_800F8818)
#undef PORT
    B_800F77D8 = B_800F2D18 | B_800F8C84;
    B_800F8C78 = B_800F4E54 | B_800F4E6C;
    B_800F5B94 = B_800F7F4C + B_800F5B74;
    B_800F133C = B_800F5B88 + B_800F5BAC;
    B_800F7F50 = B_800F8C88 | B_800F15E8;
    B_800F15DC = B_800F5BE4 | B_800F4E60;
    if (B_800F5B94 < -128) B_800F5B94 = -128;
    if (B_800F5B94 >= 128) B_800F5B94 = 127;
    if (B_800F133C < -128) B_800F133C = -128;
    if (B_800F133C >= 128) B_800F133C = 127;
    if ((B_800F77D8 & 0x2030) == 0x2030 && (B_800F8C78 & 0x2030) != 0) {
        if (++D_800B0B08 >= 3) D_800B0B08 = 0;
    }
}

/* ------------------------------------------------------------------ video */

extern s32 B_800F7D90;
extern s16 B_800F5B48[8];
extern f32 B_800F5BC8[4], B_800F7E00[4];

static OSMesgQueue sFrameQ;                 /* the frame loop's own retrace messages */
static OSMesg sFrameMsg;
static u16 *sGrab;                          /* the last frame, for screen grabs */

/* called by ultra.c at every retrace */
void port_on_retrace(void) {
    if (++D_800B09B4 == 0) D_800B09B0++;
    audio_retrace();
    plat_snd_retrace();
}

static void InitVideo(s32 numBuffers) {
    s32 i, size;

    B_800F5D00 = 320;
    B_800F5BEC = 240;
    B_800F7D90 = 2;
    B_800F87B0 = numBuffers;
    B_800F777C = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, numBuffers * 4, 0);
    B_800F7D58 = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, numBuffers * 4, 0);
    size = B_800F5D00 * B_800F5BEC * 2;
    /* the frame buffers: only their addresses matter (the display lists name them) */
    for (i = 0; i < numBuffers; i++) {
        B_800F777C[i] = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 64, 0);
        B_800F7D58[i] = FB_FREE;
    }
    B_800F2D4C = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, 64, 0);
    sGrab = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, size, 0);
    mpMemSet(sGrab, size, 0);

    B_800F5B48[0] = (B_800F5D00 / 2) * 4;
    B_800F5B48[1] = (B_800F5BEC / 2) * 4;
    B_800F5B48[2] = 0x3FF;
    B_800F5B48[3] = 0;
    B_800F5B48[4] = (B_800F5D00 / 2) * 4;
    B_800F5B48[5] = (B_800F5BEC / 2) * 4;
    B_800F5B48[6] = 0;
    B_800F5B48[7] = 0;
    B_800F5BC8[0] = B_800F7E00[0] = 160.0f;
    B_800F5BC8[1] = B_800F7E00[1] = 120.0f;
    B_800F5BC8[2] = 1023.0f;
    B_800F5BC8[3] = B_800F7E00[2] = B_800F7E00[3] = 0.0f;

    osCreateMesgQueue(&sFrameQ, &sFrameMsg, 1);
    port_add_retrace_queue(&sFrameQ);
}

/* 0x800779F4 / 0x80077AF4: a message every retrace / no more */
void func_800779F4(OSMesgQueue *q) { port_add_tick_queue(q); }
void func_80077AF4(OSMesgQueue *q) { port_remove_retrace_queue(q); }

/* 0x80077F10: is a frame still being drawn? (the port draws each one at once) */
s32 func_80077F10(void) { return 0; }

/* 0x80077F74: fill the frame buffers with a colour (only for fatal errors) */
void func_80077F74(s32 r, s32 g, s32 b) { (void)r; (void)g; (void)b; }

/* 0x80078040: debug text (fatal errors) */
void func_80078040(const char *s, s32 x, s32 y, s32 colour) {
    static char last[256];
    (void)x; (void)y; (void)colour;
    if (strncmp(last, s, sizeof(last) - 1) == 0) return;     /* (it is written 80 times) */
    strncpy(last, s, sizeof(last) - 1);
    plat_debug(s);
}

/* osViGetCurrentFramebuffer: the game copies the screen for its transitions */
void *func_80098070(void) {
    plat_gfx_grab(sGrab, B_800F5D00, B_800F5BEC);
    return sGrab;
}

/* ------------------------------------------------------------------ the frame loop (N64GAMSC.C) */

extern s32 B_800E4B60, B_800E4B68, B_800E4B70;   /* display list / matrix / vertex counts */
extern u32 *B_800F89E4;                          /* gDL */
extern void *B_800F5B6C;                         /* the work matrices */
void RomCopy(void *dst, u32 romAddr, u32 size);
extern void *B_800F77D0;                         /* the work vertices */
extern s32 B_800F5B60;
extern void *B_800F882C, *B_800F1334;            /* this frame's frame buffer, the Z buffer */
static u32 *sDL;
static void *sMtx, *sVtx;

static void AllocFrame(void) {
    if (sDL) {
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sDL);
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sMtx);
        mpLinkedMemoryManager_FreePtr(&gN64MemoryManager, sVtx);
    }
    /* (twice the game's sizes: an overrun is reported at the end of the frame, not a
       damaged heap) */
    sDL = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, B_800E4B60 * 16, 0);
    sMtx = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, B_800E4B68 << 7, 0);
    sVtx = mpLinkedMemoryManager_AllocPtr(&gN64MemoryManager, B_800E4B70 << 5, 0);
}

/* 0x8007BF80: start-up */
void func_8007BF80(s32 numBuffers) {
    (void)numBuffers;
    B_800E4B60 = 0x400;
    B_800E4B68 = 0x10;
    B_800E4B70 = 0x40;
    AllocFrame();
}

/* 0x8007C5BC: new sizes for the display list, work matrices and vertices */
void func_8007C5BC(s32 dlSize, s32 mtxCount, s32 vtxCount) {
    if (dlSize == B_800E4B60 && mtxCount == B_800E4B68 && vtxCount == B_800E4B70) return;
    B_800E4B60 = dlSize;
    B_800E4B68 = mtxCount;
    B_800E4B70 = vtxCount;
    AllocFrame();
}

/* 0x8007C850: start a frame */
void func_8007C850(void) {
    B_800F89E4 = sDL;
    B_800F5B6C = sMtx;
    B_800F77D0 = sVtx;
    B_800F5B60 = 0;
    B_800F882C = B_800F777C[0];
    B_800F1334 = B_800F2D4C;
}

#define port_heapcheck_at(where) ((void)0)
void port_heapcheck(void) {}

/* 0x8007CA88: the frame's display list is finished: draw it, show it, wait for the retrace */
void n64GameSched_CheckinCPU(void) {
    s32 used;

    port_heapcheck_at("the frame built");
    B_800F89E4[0] = 0xE9000000;            /* gDPFullSync */
    B_800F89E4[1] = 0;
    B_800F89E4[2] = 0xB8000000;            /* gSPEndDisplayList */
    B_800F89E4[3] = 0;
    B_800F89E4 += 4;
    used = (s32)((u8 *)B_800F89E4 - (u8 *)sDL) >> 3;
    if (B_800E4B60 < used)
        mpFatalErrorf("N64GAMSC.C : n64GameSched_CheckinCPU() - Command list has been overrun!  Size is %d commands, max is %d commands", used, B_800E4B60);
    used = (s32)((u8 *)B_800F5B6C - (u8 *)sMtx) >> 6;
    if (B_800E4B68 < used)
        mpFatalErrorf("N64GAMSC.C : n64GameSched_CheckinCPU() - RSP work matrices have been overrun!  Used %d, max is %d", used, B_800E4B68);
    used = (s32)((u8 *)B_800F77D0 - (u8 *)sVtx) >> 4;
    if (B_800E4B70 < used)
        mpFatalErrorf("N64GAMSC.C : n64GameSched_CheckinCPU() - RSP work vertices have been overrun!  Used %d, max is %d", used, B_800E4B70);
    plat_mark(0);
    gfx_run(sDL);
    port_heapcheck_at("the frame drawn");
    /* one frame per retrace at most (the N64 showed a frame on the retrace after it finished) */
    osRecvMesg(&sFrameQ, NULL, OS_MESG_BLOCK);
    plat_mark(1);
}

/* 0x8007CCA4: wait until all the graphics are done (they always are) */
void func_8007CCA4(void) {}

/* (sound: port/audio.c) */

/* ------------------------------------------------------------------ start-up (was boot.c) */

void n64MemoryManager_Init(u32 start, u32 end, s32 numHandles);
void func_8007FF20(s32 fovy, f32 nearZ, f32 farZ);
void Game_Init(void);
u32  func_800980B0(void);

#define HEAP_SIZE (6 * 1024 * 1024)
static u8 sHeap[HEAP_SIZE] __attribute__((aligned(64)));

void port_hang(s32 code) {
    char buf[160];
    snprintf(buf, sizeof(buf), "the game stopped (%d)", (int)code);
    plat_debug(buf);
    plat_exit(code);
}

void port_main(void) {
    s32 n;

    port_threads_init();
    mpMemSet(sHeap, HEAP_SIZE, 0);
    n64MemoryManager_Init((u32)sHeap, (u32)sHeap + HEAP_SIZE, 0x300);
    port_rom_init();
    gfx_init();
    InitVideo(3);
    func_800786B0(22050, 16, 8, 1);
    func_8007BF80(3);
    func_80076B90();
    func_8007FF20(0x21, 60.0f, 2600.0f);
    for (n = (s32)func_800980B0() - 1; n > 0; n--) guRandom();
    Game_Init();
    mpExitf("Terminated with %ld available heap", mpLinkedMemoryManager_GetFreeMem(&gN64MemoryManager));
}
