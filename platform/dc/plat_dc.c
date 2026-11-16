/*
 * plat_dc.c - the Dreamcast side of the port (KallistiOS): the game data from the disc,
 * controllers, the vertical blank, and the PowerVR backend that draws the triangles
 * port/gfx.c makes from the game's display lists.
 *
 * Draw order: the N64 draws in display-list order; the PowerVR sorts by list. Everything
 * goes in the translucent list with sorting off, so it is drawn in the order it comes.
 * Depth: polygons the game draws with the depth test compare 1/w (GEQUAL); the others always
 * pass, so they simply land in order, as on the N64; each writes the depth buffer if the game
 * says so. The depth buffer is cleared to "far" at the start of a frame and when the game
 * clears its Z buffer.
 */
#include <kos.h>
#include <dc/pvr.h>
#include <dc/vblank.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/maple/vmu.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/stream.h>
#include <dc/sound/aica_comm.h>
#include <dc/spu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include "port.h"

/* ------------------------------------------------------------------ the game data, the clock */

static file_t sData = -1;
static u32 sDataSize;
static volatile u32 sVblanks;

static void VblankCount(uint32_t code, void *data) {
    (void)code; (void)data;
    sVblanks++;
}

void plat_init(void) {}
u32 plat_vblank_count(void) { return sVblanks; }
void plat_wait_vblank(u32 count) {
    while ((s32)(sVblanks - count) <= 0) thd_pass();
}

/* reads from the disc go through a cache of 32 KB blocks (the fighters' animation frames are
   read a few at a time all through a fight; the disc is slow to seek). 1 MB of them: with 2 MB
   the memory left over ran out when a fight's sound banks were loaded (a white screen) */
#define BLK 32768
#define NBLK 32
static struct { u32 blk; u32 used; u8 *data; } sCache[NBLK];
static u32 sCacheClock;
static u8 sBounce[128 * 1024] __attribute__((aligned(32)));

static const u8 *Block(u32 blk) {
    s32 i, old = 0;
    for (i = 0; i < NBLK; i++) {
        if (sCache[i].data && sCache[i].blk == blk) { sCache[i].used = ++sCacheClock; return sCache[i].data; }
        if (sCache[i].used < sCache[old].used) old = i;
    }
    if (!sCache[old].data) sCache[old].data = memalign(32, BLK);
    sCache[old].blk = blk;
    sCache[old].used = ++sCacheClock;
    fs_seek(sData, (off_t)blk * BLK, SEEK_SET);
    fs_read(sData, sCache[old].data, BLK);
    return sCache[old].data;
}

void plat_data_read(void *dst, u32 offset, u32 size) {
    u8 *d = dst;
    if (sData < 0) return;
    if (size >= 4 * BLK) {
        /* big files: straight from the disc, through a buffer that starts on a sector and on
           32 bytes (the disc driver's fast way; into the game's memory it read a sector at a
           time: 0.1 MB a second, seconds of loading) */
        u32 at = offset & ~2047u, skip = offset - at;
        fs_seek(sData, at, SEEK_SET);
        while (size) {
            u32 want = (skip + size + 2047) & ~2047u, n, c;
            if (want > sizeof(sBounce)) want = sizeof(sBounce);
            n = (u32)fs_read(sData, sBounce, want);
            if (n <= skip) break;
            c = n - skip < size ? n - skip : size;
            memcpy(d, sBounce + skip, c);
            d += c; size -= c; skip = 0;
        }
        return;
    }
    while (size) {
        u32 o = offset % BLK, n = BLK - o;
        if (n > size) n = size;
        memcpy(d, Block(offset / BLK) + o, n);
        d += n; offset += n; size -= n;
    }
}
u32 plat_data_size(void) { return sDataSize; }


void plat_debug(const char *s) { printf("%s\n", s); }
void plat_exit(s32 code) {
    printf("exit %d\n", (int)code);
    arch_exit();
}

/* ------------------------------------------------------------------ controllers */

/* Dreamcast -> N64 buttons (the game's six attacks, as its manual and button set-up name them):
 *   X -> B (wobbly punch), Y tapped -> C-left (fair punch), Y held -> C-up (brutal punch),
 *   A -> A (wobbly kick), B tapped -> C-down (fair kick), B held -> C-right (brutal kick),
 *   left trigger -> L, right trigger -> R (the side-steps), Start -> Start, d-pad -> d-pad,
 *   analogue stick -> stick. (The N64's Z has no button: only a leftover camera uses it.)
 * A tap is sent when the button is let go; one held for HOLD_MS is the brutal attack, sent
 * then and for as long as it stays held.
 * A + B + X + Y + Start together: back to the Dreamcast's menu (the standard soft reset). */
#define HOLD_MS 133                     /* how long a hold is (8 frames) */
#define TAP_MS 50                       /* how long a tap is shown to the game (3 frames) */
/* a tap-or-hold button: what it gives the game now (tap: the first bit, hold: the second) */
typedef struct TapHold { u64 down, tapUntil; s32 held; } TapHold;
static u16 TapOrHold(TapHold *t, s32 pressed, u64 now, u16 tapBit, u16 holdBit) {
    if (pressed) {
        if (t->down == 0) t->down = now ? now : 1;
        if (now - t->down >= HOLD_MS) t->held = 1;
        if (t->held) return holdBit;
    } else if (t->down) {
        if (!t->held) t->tapUntil = now + TAP_MS;            /* let go in time: a tap */
        t->down = 0; t->held = 0;
    }
    return now < t->tapUntil ? tapBit : 0;
}
static TapHold sTapY[4], sTapB[4];
static void VmuUpdate(void);
/* the secret codes, typed with quick presses only (on the N64 four C buttons; here two buttons
   with two each): X is B, A is A, Y is C-left and C-up, B is C-down and C-right */
static u16 sCodeButtons[4];
u16 plat_code_buttons(s32 port) { return (port >= 0 && port < 4) ? sCodeButtons[port] : 0; }

s32 plat_read_pads(u16 buttons[4], s8 sx[4], s8 sy[4]) {
    s32 i, present = 0;
    u64 now = timer_ms_gettime64();
    for (i = 0; i < 4; i++) {
        maple_device_t *dev = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st;
        u16 b = 0;
        buttons[i] = 0; sx[i] = 0; sy[i] = 0; sCodeButtons[i] = 0;
        if (dev == NULL || (st = (cont_state_t *)maple_dev_status(dev)) == NULL) continue;
        present |= 1 << dev->port;
        if ((st->buttons & (CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START)) ==
            (CONT_A | CONT_B | CONT_X | CONT_Y | CONT_START)) arch_exit();
        if (st->buttons & CONT_A) b |= 0x8000;                                   /* A */
        if (st->buttons & CONT_X) b |= 0x4000;                                   /* B */
        b |= TapOrHold(&sTapY[dev->port], (st->buttons & CONT_Y) != 0, now, 0x0002, 0x0008);  /* C-left / C-up */
        b |= TapOrHold(&sTapB[dev->port], (st->buttons & CONT_B) != 0, now, 0x0004, 0x0001);  /* C-down / C-right */
        if (st->ltrig > 64) b |= 0x0020;                                         /* L */
        if (st->rtrig > 64) b |= 0x0010;                                         /* R */
        if (st->buttons & CONT_START) b |= 0x1000;
        if (st->buttons & CONT_DPAD_UP) b |= 0x0800;
        if (st->buttons & CONT_DPAD_DOWN) b |= 0x0400;
        if (st->buttons & CONT_DPAD_LEFT) b |= 0x0200;
        if (st->buttons & CONT_DPAD_RIGHT) b |= 0x0100;
        buttons[dev->port] = b;
        sCodeButtons[dev->port] = (u16)(((st->buttons & CONT_A) ? 0x8000 : 0) | ((st->buttons & CONT_X) ? 0x4000 : 0) |
                                        ((st->buttons & CONT_Y) ? 0x000A : 0) | ((st->buttons & CONT_B) ? 0x0005 : 0));
        sx[dev->port] = (s8)(st->joyx * 80 / 128);
        sy[dev->port] = (s8)(-st->joyy * 80 / 128);
    }
    VmuUpdate();
    return present;
}

/* ------------------------------------------------------------------ the VMU's screen */

/* in a fight, each human player's VMU shows their own health as a bar, with the round's
   timer; the rest of the time the ClayFighter logo scrolls across. Sent only when it changes, at most ten times a second, and never
   waited for (KallistiOS queues it; a busy controller is tried again later) */
static struct { s32 health, timer; u64 at; s32 sent; } sVmu[4];
static u64 sVmuLast;
static const char *const sVmuGlyph[] = {   /* 3 x 5: 0-9, then Y O U */
    "111101101101111", "010110010010111", "111001111100111", "111001111001111", "101101111001001",
    "111100111001111", "111100111101111", "111001010010010", "111101111101111", "111101111001111",
    "101101010010010", "111101101101111", "101101101101111" };

static const u8 sVmuLogo[32][12] = {   /* 93 x 32, 1 = on */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00},
    {0x0C, 0x30, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00},
    {0x1F, 0x3C, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00},
    {0x1F, 0x3E, 0x00, 0x00, 0x01, 0xFF, 0xC0, 0x00, 0xF0, 0x00, 0x00, 0x00},
    {0x1F, 0x9E, 0x06, 0x00, 0x01, 0xFF, 0xC0, 0x00, 0xE0, 0x00, 0x00, 0x00},
    {0x18, 0x1E, 0x07, 0x00, 0x01, 0xE1, 0x00, 0x01, 0xC0, 0x38, 0x00, 0x00},
    {0x18, 0x1E, 0x07, 0xE2, 0x01, 0xE0, 0x00, 0x03, 0xC0, 0x70, 0x00, 0x00},
    {0x18, 0x1E, 0x0F, 0xE7, 0x01, 0xF0, 0x00, 0x03, 0xE3, 0xF1, 0xC0, 0x00},
    {0x1C, 0x0E, 0x0E, 0x77, 0x01, 0xF8, 0x03, 0x83, 0xFF, 0xFB, 0xEC, 0x00},
    {0x1C, 0x0E, 0x0E, 0x77, 0x01, 0xF9, 0xC7, 0x87, 0xFF, 0xFF, 0xFF, 0xC0},
    {0x1C, 0x0E, 0x0C, 0x73, 0x89, 0xFB, 0xCF, 0x3F, 0xFF, 0x8F, 0xFF, 0xC0},
    {0x0C, 0x06, 0x0C, 0x33, 0x9C, 0xE0, 0x1E, 0x7F, 0x3F, 0x8F, 0xFC, 0xC0},
    {0x0E, 0x07, 0x3E, 0x7B, 0xFC, 0xE1, 0x9C, 0x7E, 0x3F, 0x0F, 0x8C, 0x00},
    {0x0E, 0x07, 0xFF, 0xF9, 0xF8, 0xE3, 0xDC, 0xFE, 0x3F, 0x0E, 0x0E, 0x00},
    {0x07, 0x1F, 0xF7, 0xF9, 0xF0, 0xE3, 0x9F, 0xFE, 0x7F, 0xFE, 0x0E, 0x00},
    {0x07, 0x3F, 0xE3, 0xF1, 0xE0, 0x63, 0x9F, 0xCE, 0xFF, 0xFF, 0xCE, 0x00},
    {0x03, 0xFB, 0xC1, 0x80, 0xC0, 0x63, 0x8F, 0xC4, 0x67, 0xC7, 0xE6, 0x00},
    {0x01, 0xF0, 0x00, 0x01, 0xC0, 0x73, 0x83, 0xC0, 0x00, 0x03, 0x86, 0x00},
    {0x01, 0xF0, 0x00, 0x01, 0x80, 0x21, 0x83, 0x80, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};
#define VMU_LOGO_W 93

void plat_vmu_fight(s32 port, s32 health, s32 timer) {
    if (port < 0 || port > 3) return;
    sVmu[port].health = health < 0 ? 0 : (health > 170 ? 170 : health);
    sVmu[port].timer = timer;
    sVmu[port].at = timer_ms_gettime64();
}

static void VmuPixel(u8 *b, s32 x, s32 y) { b[y * 6 + (x >> 3)] |= (u8)(0x80 >> (x & 7)); }
static void VmuGlyph(u8 *b, s32 g, s32 x, s32 y) {
    s32 i;
    for (i = 0; i < 15; i++) if (sVmuGlyph[g][i] == '1') VmuPixel(b, x + i % 3, y + i / 3);
}

static void VmuUpdate(void) {
    u64 now = timer_ms_gettime64();
    s32 port, unit;
    if (now - sVmuLast < 100) return;
    sVmuLast = now;
    for (port = 0; port < 4; port++) {
        s32 active = sVmu[port].at != 0 && now - sVmu[port].at < 500;
        s32 scroll = (s32)((now / 100) * 2 % (VMU_LOGO_W + 48));       /* 20 dots a second */
        s32 key = active ? 1 + sVmu[port].health * 128 + (sVmu[port].timer & 127) : -1 - scroll;
        u32 bits[48];                         /* (4-byte aligned, as vmu_draw_lcd_rotated wants) */
        u8 *b = (u8 *)bits;
        s32 ok = 1, any = 0;
        if (key == sVmu[port].sent) continue;
        memset(bits, 0, sizeof(bits));
        if (active) {
            s32 x, y, fill = sVmu[port].health * 42 / 170;
            VmuGlyph(b, 10, 2, 2); VmuGlyph(b, 11, 6, 2); VmuGlyph(b, 12, 10, 2);       /* YOU */
            if (sVmu[port].timer >= 0) {
                VmuGlyph(b, (sVmu[port].timer / 10) % 10, 39, 2);
                VmuGlyph(b, sVmu[port].timer % 10, 43, 2);
            }
            for (x = 1; x < 47; x++) { VmuPixel(b, x, 10); VmuPixel(b, x, 29); }       /* the frame */
            for (y = 10; y < 30; y++) { VmuPixel(b, 1, y); VmuPixel(b, 46, y); }
            for (x = 3; x < 3 + fill; x++) for (y = 12; y < 28; y++) VmuPixel(b, x, y);
        } else {
            s32 x, y;
            for (y = 0; y < 32; y++)
                for (x = 0; x < 48; x++) {
                    s32 lx = x - 48 + scroll;
                    if (lx >= 0 && lx < VMU_LOGO_W && (sVmuLogo[y][lx >> 3] & (0x80 >> (lx & 7)))) VmuPixel(b, x, y);
                }
        }
        for (unit = 1; unit <= 2; unit++) {
            maple_device_t *dev = maple_enum_dev(port, unit);
            if (dev == NULL || !(dev->info.functions & MAPLE_FUNC_LCD)) continue;
            any = 1;
            if (vmu_draw_lcd_rotated(dev, bits) != MAPLE_EOK) ok = 0;
        }
        if (!any || ok) sVmu[port].sent = key;
    }
}

/* ------------------------------------------------------------------ sound (the AICA) */

/* port/audio.c plays the game's music and sounds; here its voices are AICA channels. The
   waves wait in main memory (each bank file, as SOUND.DAT has it) and are copied into the
   AICA's own memory when first played, the ones not played for longest making room. Waves too
   long for one channel buffer are streamed (KallistiOS's sound streams). A thread runs the
   player every few milliseconds. */
#define SND_VOICES 24
#define SND_GAIN 160.0f                     /* full volume (255 is the AICA's most): room for many voices */
#define SPU_SLOTS 256
typedef struct SpuWave { const u8 *data; u32 addr, size, used; s32 pins; } SpuWave;
static SpuWave sSpu[SPU_SLOTS];
static s32 sChn[SND_VOICES];                /* the AICA channel of each voice */
static SpuWave *sVoiceWave[SND_VOICES];
static u32 sSpuClock;
static mutex_t sSndLock;
static file_t sSnd = -1;
static s32 sSndOn;

/* streams: the long waves */
#define SND_STREAMS 2
typedef struct SStream { snd_stream_hnd_t h; s32 voice; const u8 *data; u32 bytes, pos; } SStream;
static SStream sStream[SND_STREAMS];
static s32 sVoiceStream[SND_VOICES];        /* the stream + 1 a voice plays, 0 none */
static u8 sStreamBuf[SND_STREAMS][16384] __attribute__((aligned(32)));

/* (through an aligned buffer from a sector's start, as plat_data_read: the disc driver's fast way;
   into the game's memory it read a sector at a time, each fighter's sounds a second or two) */
static u8 sSndBounce[64 * 1024] __attribute__((aligned(32)));
u32 plat_sound_read(void *dst, u32 offset, u32 size) {
    u8 *d = dst;
    u32 at = offset & ~2047u, skip = offset - at, got = 0;
    if (sSnd < 0) sSnd = fs_open("/cd/SOUND.DAT", O_RDONLY);
    if (sSnd < 0) return 0;
    fs_seek(sSnd, at, SEEK_SET);
    while (size) {
        u32 want = (skip + size + 2047) & ~2047u, n, c;
        if (want > sizeof(sSndBounce)) want = sizeof(sSndBounce);
        n = (u32)fs_read(sSnd, sSndBounce, want);
        if ((s32)n <= (s32)skip) break;
        c = n - skip < size ? n - skip : size;
        memcpy(d, sSndBounce + skip, c);
        d += c; size -= c; got += c; skip = 0;
    }
    return got;
}
void plat_snd_lock(void) { if (sSndOn) mutex_lock(&sSndLock); }
void plat_snd_unlock(void) { if (sSndOn) mutex_unlock(&sSndLock); }
void plat_snd_retrace(void) {}

static void *StreamFeed(snd_stream_hnd_t h, int req, int *got) {
    s32 i;
    for (i = 0; i < SND_STREAMS; i++)
        if (sStream[i].h == h) {
            SStream *st = &sStream[i];
            u32 n = (u32)req, left = st->pos < st->bytes ? st->bytes - st->pos : 0;
            if (n > sizeof(sStreamBuf[0])) n = sizeof(sStreamBuf[0]);
            if (n > left) { memcpy(sStreamBuf[i], st->data + st->pos, left); memset(sStreamBuf[i] + left, 0x00, n - left); }
            else memcpy(sStreamBuf[i], st->data + st->pos, n);
            st->pos += n;
            *got = (int)n;
            return sStreamBuf[i];
        }
    *got = 0;
    return NULL;
}

static void *AudioThread(void *arg) {
    u64 last = timer_us_gettime64();
    s32 i;
    (void)arg;
    for (;;) {
        u64 now;
        thd_sleep(4);
        now = timer_us_gettime64();
        audio_tick((f32)(now - last) * 1e-6f);
        last = now;
        mutex_lock(&sSndLock);
        for (i = 0; i < SND_STREAMS; i++) if (sStream[i].voice >= 0) snd_stream_poll(sStream[i].h);
        mutex_unlock(&sSndLock);
    }
    return NULL;
}

void plat_snd_init(void) {
    s32 i;
    snd_stream_init();
    for (i = 0; i < SND_STREAMS; i++) { sStream[i].h = snd_stream_alloc(StreamFeed, sizeof(sStreamBuf[0])); sStream[i].voice = -1; }
    for (i = 0; i < SND_VOICES; i++) sChn[i] = snd_sfx_chn_alloc();
    mutex_init(&sSndLock, MUTEX_TYPE_RECURSIVE);
    sSndOn = 1;
    thd_create(1, AudioThread, NULL);
}

/* a rate for the AICA driver that it turns into the nearest pitch it can play (it rounds down:
   up to 1.7 cents flat, and the whole-hertz rate as much again for low notes) */
static f32 PanHi(f32 pan) { f32 l = cosf(pan * 1.5707963f), r = sinf(pan * 1.5707963f); return l > r ? l : r; }

static f32 AicaHz(f32 hz) {
    u32 base = 5644800, fns;
    s32 oct = 7;
    if (hz < 100.0f) hz = 100.0f;
    if (hz > 176400.0f) hz = 176400.0f;
    while (hz < (f32)base && oct > -8) { base >>= 1; oct--; }
    fns = (u32)(hz * 1024.0f / (f32)base + 0.5f);          /* the nearest step, 1024..2048 */
    return (f32)(((u64)fns * base + 1023) / 1024);           /* (the smallest whole rate giving it) */
}

static void Chan(s32 v, u32 cmdBits, u32 base, u32 type, u32 len, u32 loop, u32 ls, u32 le, f32 hz, f32 vol, f32 pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, ch);
    s32 iv, ip;
    if (sChn[v] < 0) return;
    {   /* the N64's equal-power pan (both sides at 0.71 in the middle) as the AICA's: the louder
           side's level, the other side lowered in 3 dB steps */
        f32 l = cosf(pan * 1.5707963f), r = sinf(pan * 1.5707963f), hi = l > r ? l : r, lo = l > r ? r : l;
        s32 k = lo <= 0.0f ? 15 : (s32)(-20.0f * log10f(lo / hi) / 3.0f + 0.5f);
        if (k > 15) k = 15;
        ip = l >= r ? 0x7F - 8 * k : 0x80 + 8 * k;
        iv = (s32)(vol * hi * SND_GAIN + 0.5f);
    }
    hz = AicaHz(hz);
    cmd->cmd = AICA_CMD_CHAN; cmd->timestamp = 0; cmd->size = AICA_CMDSTR_CHANNEL_SIZE; cmd->cmd_id = sChn[v];
    ch->cmd = cmdBits; ch->base = base; ch->type = type; ch->length = len; ch->loop = loop;
    ch->loopstart = ls; ch->loopend = le;
    ch->freq = (u32)hz;
    ch->vol = iv < 0 ? 0 : iv > 255 ? 255 : iv;
    ch->pan = ip < 0 ? 0 : ip > 255 ? 255 : ip;
    snd_sh4_to_aica(tmp, cmd->size);
}

/* the wave in the AICA's memory (copied there if it isn't) */
static SpuWave *SpuGet(const PWave *w, const u8 *data) {
    s32 i, freeSlot = -1;
    u32 addr;
    for (i = 0; i < SPU_SLOTS; i++) {
        if (sSpu[i].data == data) { sSpu[i].used = ++sSpuClock; return &sSpu[i]; }
        if (sSpu[i].data == NULL && freeSlot < 0) freeSlot = i;
    }
    for (;;) {
        addr = freeSlot >= 0 ? snd_mem_malloc(w->bytes) : 0;
        if (addr) break;
        {   /* room: the wave not played for longest (and not playing) */
            s32 old = -1;
            for (i = 0; i < SPU_SLOTS; i++)
                if (sSpu[i].data && sSpu[i].pins == 0 && (old < 0 || sSpu[i].used < sSpu[old].used)) old = i;
            if (old < 0) return NULL;
            snd_mem_free(sSpu[old].addr);
            sSpu[old].data = NULL;
            if (freeSlot < 0) freeSlot = old;
        }
    }
    spu_memload(addr, data, (w->bytes + 3) & ~3u);
    sSpu[freeSlot].data = data; sSpu[freeSlot].addr = addr; sSpu[freeSlot].size = w->bytes;
    sSpu[freeSlot].used = ++sSpuClock; sSpu[freeSlot].pins = 0;
    return &sSpu[freeSlot];
}

static void VoiceLetGo(s32 v) {
    if (sVoiceWave[v]) { sVoiceWave[v]->pins--; sVoiceWave[v] = NULL; }
    if (sVoiceStream[v]) {
        SStream *st = &sStream[sVoiceStream[v] - 1];
        snd_stream_stop(st->h);
        st->voice = -1;
        sVoiceStream[v] = 0;
    }
}

void plat_voice_start(s32 v, const PWave *w, const u8 *data, f32 hz, f32 vol, f32 pan) {
    SpuWave *sw;
    if (v < 0 || v >= SND_VOICES) return;
    VoiceLetGo(v);
    if (w->fmt == SND_STREAM) {
        s32 i;
        for (i = 0; i < SND_STREAMS; i++)
            if (sStream[i].voice < 0) {
                SStream *st = &sStream[i];
                st->voice = v; st->data = data; st->bytes = w->bytes; st->pos = 0;
                sVoiceStream[v] = i + 1;
                snd_stream_start_adpcm(st->h, (uint32_t)hz, 0);
                /* (after the start, which sets the channel to full volume: before it, a long
                   sound kept that, twice too loud, until its volume next changed: the intro's
                   tune, the title music's cymbal) */
                snd_stream_volume(st->h, (int)(vol * PanHi(pan) * SND_GAIN));   /* (in the middle: at 0.71, as the N64) */
                return;
            }
        return;
    }
    sw = SpuGet(w, data);
    if (sw == NULL) return;
    sw->pins++;
    sVoiceWave[v] = sw;
    Chan(v, AICA_CH_CMD_START, sw->addr,
         w->fmt == SND_PCM16 ? AICA_SM_16BIT : w->fmt == SND_PCM8 ? AICA_SM_8BIT : AICA_SM_ADPCM,
         w->samples, w->loopCount != 0, w->loopCount ? w->loopStart : 0, w->loopCount ? w->loopEnd : w->samples,
         hz, vol, pan);
}
void plat_voice_set(s32 v, f32 hz, f32 vol, f32 pan) {
    if (v < 0 || v >= SND_VOICES) return;
    if (sVoiceStream[v]) { snd_stream_volume(sStream[sVoiceStream[v] - 1].h, (int)(vol * PanHi(pan) * SND_GAIN)); return; }
    if (sVoiceWave[v] == NULL) return;
    Chan(v, AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_VOL | AICA_CH_UPDATE_SET_PAN,
         0, 0, 0, 0, 0, 0, hz, vol, pan);
}
void plat_voice_stop(s32 v) {
    if (v < 0 || v >= SND_VOICES) return;
    if (sVoiceWave[v]) Chan(v, AICA_CH_CMD_STOP, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.5f);
    VoiceLetGo(v);
}
void plat_snd_forget(const void *file, u32 size) {
    s32 i;
    const u8 *a = (const u8 *)file, *b = a + size;
    for (i = 0; i < SND_VOICES; i++)
        if ((sVoiceWave[i] && sVoiceWave[i]->data >= a && sVoiceWave[i]->data < b) ||
            (sVoiceStream[i] && sStream[sVoiceStream[i] - 1].data >= a && sStream[sVoiceStream[i] - 1].data < b))
            plat_voice_stop(i);
    for (i = 0; i < SPU_SLOTS; i++)
        if (sSpu[i].data >= a && sSpu[i].data < b) { snd_mem_free(sSpu[i].addr); sSpu[i].data = NULL; }
}

/* ------------------------------------------------------------------ textures */

#define TEX_SLOTS 2048
typedef struct TexEntry {
    u32 key;
    pvr_ptr_t ptr;
    u16 w, h, fmt;
    s32 used;
} TexEntry;
static TexEntry sTex[TEX_SLOTS];
static s32 sTexCount, sFrameNo;
static u32 sSpread[1024];
static u16 sTw[1024 * 512] __attribute__((aligned(32)));

static TexEntry *TexFind(u32 key) {
    u32 i = (key * 2654435761u) & (TEX_SLOTS - 1);
    while (sTex[i].ptr != NULL) {
        if (sTex[i].key == key) return &sTex[i];
        i = (i + 1) & (TEX_SLOTS - 1);
    }
    return NULL;
}

s32 plat_tex_known(u32 key) {
    TexEntry *e = TexFind(key);
    if (e) { e->used = sFrameNo; return 1; }
    return 0;
}

/* free the textures not drawn this frame or the last */
static s32 TexEvict(void) {
    static TexEntry keep[TEX_SLOTS];
    s32 i, n = 0, freed = 0;
    for (i = 0; i < TEX_SLOTS; i++) {
        if (sTex[i].ptr == NULL) continue;
        if (sTex[i].used >= sFrameNo - 1) keep[n++] = sTex[i];
        else { pvr_mem_free(sTex[i].ptr); freed++; }
        sTex[i].ptr = NULL;
    }
    for (i = 0; i < n; i++) {
        u32 j = (keep[i].key * 2654435761u) & (TEX_SLOTS - 1);
        while (sTex[j].ptr != NULL) j = (j + 1) & (TEX_SLOTS - 1);
        sTex[j] = keep[i];
    }
    sTexCount = n;
    return freed;
}

static s32 Log2(u32 v) { s32 n = 0; while ((1u << n) < v) n++; return n; }

static TexEntry *TexUpload(const GfxTex *t) {
    u32 w = t->w, h = t->h, x, y, m = w < h ? w : h, lm = Log2(m), i;
    pvr_ptr_t p;
    TexEntry *e;
    if (sTexCount >= TEX_SLOTS * 3 / 4 && TexEvict() == 0) return NULL;
    p = pvr_mem_malloc(w * h * 2);
    if (p == NULL && (TexEvict() == 0 || (p = pvr_mem_malloc(w * h * 2)) == NULL)) {
        return NULL;
    }
    /* twiddled (the PowerVR's fast order): x and y bits interleaved, square blocks */
    for (y = 0; y < h; y++) {
        const u16 *src = t->texels + y * w;
        u32 ybits = sSpread[y & (m - 1)], ybase = (w > h) ? 0 : (y >> lm) << (2 * lm);
        for (x = 0; x < w; x++) {
            u32 idx = ybits | (sSpread[x & (m - 1)] << 1);
            idx += (w > h) ? (x >> lm) << (2 * lm) : ybase;
            sTw[idx] = src[x];
        }
    }
    {   /* plain 32-bit writes (pvr_txr_load uses the store queues, which carry the polygons) */
        volatile u32 *d = (volatile u32 *)p;
        const u32 *src32 = (const u32 *)sTw;
        u32 k, nw = w * h / 2;
        for (k = 0; k < nw; k++) d[k] = src32[k];
    }
    i = (t->key * 2654435761u) & (TEX_SLOTS - 1);
    while (sTex[i].ptr != NULL) i = (i + 1) & (TEX_SLOTS - 1);
    e = &sTex[i];
    e->key = t->key; e->ptr = p; e->w = w; e->h = h; e->fmt = t->fmt; e->used = sFrameNo;
    sTexCount++;
    return e;
}

/* ------------------------------------------------------------------ polygons */

static u32 sDrawUs, sTexUs;

/* Which list: the PowerVR draws its solid list fastest (it works out what is seen first and
   draws each pixel once), then the punch-through list (solid, with see-through texels), then
   the translucent list, in the order given (sorting is off). The N64 draws in display-list
   order, so a polygon only goes in the solid or punch-through list when the order can't
   matter: it is depth-tested and writes depth (what's in front wins whatever the order), or it
   is a solid background drawn before anything depth-tested. Everything else goes in the
   translucent list in order, after the solid ones, as before. Once the game clears its depth
   buffer with 3D already drawn, the rest of the frame is all translucent list (in order).
   The solid and punch-through lists are kept in memory and sent at the end of the frame. */
#ifndef PVR_PT_ALPHA_REF
#define PVR_PT_ALPHA_REF 0x011c         /* older KallistiOS: the punch-through alpha reference register */
#endif
enum { LIST_TR, LIST_OP, LIST_PT };
#define LIST_BYTES (384 * 1024)
static pvr_dr_state_t sDr;              /* (the translucent list goes straight to the PowerVR) */
static u32 sListBuf[2][LIST_BYTES / 4] __attribute__((aligned(32)));
static u32 sListN[2];                   /* bytes in the solid and punch-through lists */
static s32 sPhase;                      /* 0 before anything depth-tested, 1 after, 2 after a depth clear */
static s32 sTrUsed;                     /* a background has gone in the translucent list */
#define BG_Z 1e-12f                     /* the depth of "nothing drawn yet" (1/w: far) */
#define BG_SCALE 1e-4f                  /* backgrounds in the translucent list: their depths (1/w) scaled
                                           down, behind anything 3D (whose 1/w is above 0.0002) but well
                                           in front of BG_Z (the real PowerVR's depths aren't exact). Each
                                           corner keeps its own depth: the PowerVR corrects the texture
                                           for perspective by it (one depth for all bent the floors) */

static void *Out(s32 list) {
    if (list == LIST_TR) return pvr_dr_target(sDr);
    if (sListN[list - 1] + 32 > LIST_BYTES) {
        return NULL;
    }
    return (u8 *)sListBuf[list - 1] + sListN[list - 1];
}
static void Commit(s32 list, void *p) {
    if (list == LIST_TR) pvr_dr_commit(p);
    else sListN[list - 1] += 32;
}

static s32 Header(s32 list, TexEntry *te, const GfxTex *t, s32 blend, s32 cmp, s32 zwrite, s32 offset) {
    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t *hdr;
    s32 pl = list == LIST_OP ? PVR_LIST_OP_POLY : list == LIST_PT ? PVR_LIST_PT_POLY : PVR_LIST_TR_POLY;
    if (te) {
        pvr_poly_cxt_txr(&cxt, pl,
                         (te->fmt == TEXFMT_4444 ? PVR_TXRFMT_ARGB4444 : PVR_TXRFMT_ARGB1555) | PVR_TXRFMT_TWIDDLED,
                         te->w, te->h, te->ptr, t->filter ? PVR_FILTER_BILINEAR : PVR_FILTER_NEAREST);
        cxt.txr.env = PVR_TXRENV_MODULATEALPHA;
        cxt.txr.uv_clamp = (t->wrapS == WRAP_CLAMP ? PVR_UVCLAMP_U : 0) | (t->wrapT == WRAP_CLAMP ? PVR_UVCLAMP_V : 0);
        cxt.txr.uv_flip = (t->wrapS == WRAP_MIRROR ? PVR_UVFLIP_U : 0) | (t->wrapT == WRAP_MIRROR ? PVR_UVFLIP_V : 0);
        if (offset) cxt.gen.specular = PVR_SPECULAR_ENABLE;
    } else {
        pvr_poly_cxt_col(&cxt, pl);
    }
    cxt.gen.culling = PVR_CULLING_NONE;
    cxt.depth.comparison = cmp;
    cxt.depth.write = zwrite ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    if (list == LIST_TR) {
        if (blend == BLEND_OPAQUE) {
            cxt.blend.src = PVR_BLEND_ONE;
            cxt.blend.dst = PVR_BLEND_ZERO;
        } else if (blend == BLEND_ADD) {
            cxt.blend.src = PVR_BLEND_SRCALPHA;
            cxt.blend.dst = PVR_BLEND_ONE;
        }
    }
    hdr = (pvr_poly_hdr_t *)Out(list);
    if (hdr == NULL) return 0;
    pvr_poly_compile(hdr, &cxt);
    Commit(list, hdr);
    return 1;
}

/* Triangles joined into strips: the N64 sends lists of triangles, and most come one after another
   sharing an edge. A triangle that shares the last two corners of the strip so far adds one
   vertex to it (no culling here, so which way round doesn't matter). Fewer vertices to send,
   and far fewer entries in the PowerVR's per-tile object lists (each strip is one entry, not
   each triangle): those lists running out dropped whole objects on the console. */
#define SEQ_MAX 3072
static const GfxVert *sSeq[SEQ_MAX];
static u8 sEol[SEQ_MAX];
static inline s32 Same(const GfxVert *a, const GfxVert *b) {
    /* (as words: the SH-4 compares integers far faster than floats; the same vertex is the same
       bits, and x and y differ first for nearly all others) */
    const u32 *p = (const u32 *)a, *q = (const u32 *)b;
    return p[0] == q[0] && p[1] == q[1] && p[3] == q[3] && p[4] == q[4] && p[5] == q[5] &&
           p[6] == q[6] && p[7] == q[7];
}
/* t's corner not one of a, b (-1 if t doesn't have both a and b) */
static s32 Third(const GfxVert *t, const GfxVert *a, const GfxVert *b) {
    s32 k, hit = 0, other = -1;
    for (k = 0; k < 3; k++) {
        if ((!(hit & 1) && Same(&t[k], a))) hit |= 1;
        else if ((!(hit & 2) && Same(&t[k], b))) hit |= 2;
        else other = k;
    }
    return hit == 3 ? other : -1;
}
static s32 Strips(const GfxVert *v, s32 n) {
    s32 i = 0, m = 0;
    if (n > SEQ_MAX) n = SEQ_MAX;
    while (i + 3 <= n) {
        const GfxVert *t = &v[i];
        s32 r = 0, k;
        if (i + 6 <= n) {                    /* start with the edge the next one shares last */
            for (k = 0; k < 3; k++)
                if (Third(&v[i + 3], &t[(k + 1) % 3], &t[(k + 2) % 3]) >= 0) { r = k; break; }
        }
        sSeq[m] = &t[r]; sEol[m++] = 0;
        sSeq[m] = &t[(r + 1) % 3]; sEol[m++] = 0;
        sSeq[m] = &t[(r + 2) % 3]; sEol[m++] = 0;
        i += 3;
        while (i + 3 <= n) {
            s32 c = Third(&v[i], sSeq[m - 2], sSeq[m - 1]);
            if (c < 0) break;
            sSeq[m] = &v[i + c]; sEol[m++] = 0;
            i += 3;
        }
        sEol[m - 1] = 1;
    }
    return m;
}

void plat_gfx_draw(const GfxVert *v, s32 n, const GfxTex *tex, s32 blend, s32 zcmp, s32 zwrite, s32 offset) {
    TexEntry *te = NULL;
    f32 zscale = 1.0f;
    s32 cmp, i, m, list;

    u64 t0 = timer_us_gettime64();
    if (tex) {
        te = TexFind(tex->key);
        if (te == NULL) {
            u64 t1 = timer_us_gettime64();
            te = TexUpload(tex);
            sTexUs += (u32)(timer_us_gettime64() - t1);
        }
        if (te == NULL) return;
        te->used = sFrameNo;
    }
    /* as the N64: depth-tested polygons compare 1/w (GEQUAL); the others are simply drawn in
       order (in the translucent list, which isn't sorted) */
    if (zcmp == GFX_ZCMP_SCENERY) {
        /* arena scenery drawn without depth: on the N64 it is painted over what came before it
           and under anything depth-tested drawn after it. Before anything depth-tested it is a
           background (below); after, the translucent list (drawn after the solid one) would put
           it over the furniture drawn later (the Doghouse's walls, once its door was on the
           screen), so there it is tested at its own depth, a little in front (to stay over
           what it lies on) */
        if (sPhase == 1) {
            if (!Header(LIST_TR, te, tex, blend, PVR_DEPTHCMP_GEQUAL, 0, offset)) { sDrawUs += (u32)(timer_us_gettime64() - t0); return; }
            zscale = 1.01f;
            list = LIST_TR;
            goto send;
        }
        zcmp = 0;
    }
    cmp = zcmp ? PVR_DEPTHCMP_GEQUAL : PVR_DEPTHCMP_ALWAYS;
    list = LIST_TR;
    if (sPhase < 2) {
        if (zcmp && zwrite && blend == BLEND_OPAQUE) list = LIST_OP;
        else if (zcmp && zwrite && blend == BLEND_CUTOUT) list = LIST_PT;
        else if (!zcmp && sPhase == 0) {
            /* a background (nothing depth-tested has come yet): solid ones in the solid list
               (until a see-through one has come, to keep their order); the others where
               nothing else is drawn, in order */
            if (blend == BLEND_OPAQUE && !sTrUsed) { list = LIST_OP; zwrite = 0; }
            else { cmp = PVR_DEPTHCMP_GEQUAL; zscale = BG_SCALE; zwrite = 0; sTrUsed = 1; }
        }
        if (zcmp) sPhase = 1;
    }
    /* (in the translucent list, see-through texels would still write depth: they don't) */
    if (list == LIST_TR && blend == BLEND_CUTOUT) zwrite = 0;
    if (!Header(list, te, tex, blend, cmp, zwrite, offset)) { sDrawUs += (u32)(timer_us_gettime64() - t0); return; }
send:
    m = Strips(v, n);
    for (i = 0; i < m; i++) {
        pvr_vertex_t *o = (pvr_vertex_t *)Out(list);
        const GfxVert *s = sSeq[i];
        if (o == NULL) {                     /* (a list in memory full: end the strip being sent) */
            if (i > 0 && !sEol[i - 1])
                ((pvr_vertex_t *)((u8 *)sListBuf[list - 1] + sListN[list - 1] - 32))->flags = PVR_CMD_VERTEX_EOL;
            break;
        }
        o->flags = sEol[i] ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        o->x = s->x * 2.0f;                  /* 320 x 240 -> 640 x 480 */
        o->y = s->y * 2.0f;
        o->z = s->q * zscale;
        o->u = s->u;
        o->v = s->v;
        o->argb = s->argb;
        o->oargb = s->oargb;
        Commit(list, o);
    }
    sDrawUs += (u32)(timer_us_gettime64() - t0);
}

void plat_gfx_fill(s32 x0, s32 y0, s32 x1, s32 y1, u32 argb) {
    GfxVert q[6];
    static const u8 cx[6] = {0, 1, 1, 0, 1, 0}, cy[6] = {0, 0, 1, 0, 1, 1};
    s32 i;
    for (i = 0; i < 6; i++) {
        q[i].x = (f32)(cx[i] ? x1 : x0);
        q[i].y = (f32)(cy[i] ? y1 : y0);
        q[i].z = 0; q[i].q = 1; q[i].u = q[i].v = 0;
        q[i].argb = argb; q[i].oargb = 0;
    }
    plat_gfx_draw(q, 6, NULL, (argb >> 24) == 255 ? BLEND_OPAQUE : BLEND_ALPHA, 0, 0, 0);
}

/* the depth buffer back to "far": a whole-screen polygon that writes only depth */
void plat_gfx_clear_depth(void) {
    static const u8 cx[6] = {0, 1, 1, 0, 1, 0}, cy[6] = {0, 0, 1, 0, 1, 1};
    s32 i;
    if (sPhase == 0) return;                 /* (nothing depth-tested yet: nothing to clear) */
    sPhase = 2;                              /* from here on, all in order in the translucent list */
    Header(LIST_TR, NULL, NULL, BLEND_ALPHA, PVR_DEPTHCMP_ALWAYS, 1, 0);
    for (i = 0; i < 6; i++) {
        pvr_vertex_t *o = (pvr_vertex_t *)Out(LIST_TR);
        if (o == NULL) return;
        o->flags = (i % 3 == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        o->x = cx[i] ? 640.0f : 0.0f; o->y = cy[i] ? 480.0f : 0.0f; o->z = BG_Z;
        o->u = o->v = 0; o->argb = 0; o->oargb = 0;
        Commit(LIST_TR, o);
    }
}

static u64 sT[6];                    /* frame start, frame built, wait start, wait end, end */
void plat_mark(s32 point) { (void)point; }

void plat_gfx_begin(void) {
    sT[2] = timer_us_gettime64();
    pvr_wait_ready();
    sT[3] = timer_us_gettime64();
    sFrameNo++;
    sPhase = 0;
    sTrUsed = 0;
    sListN[0] = sListN[1] = 0;
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    pvr_dr_init(&sDr);
    {   /* the solid list starts with the whole screen black at "far" (writing depth), as cleared */
        static const u8 cx[6] = {0, 1, 1, 0, 1, 0}, cy[6] = {0, 0, 1, 0, 1, 1};
        s32 i;
        Header(LIST_OP, NULL, NULL, BLEND_OPAQUE, PVR_DEPTHCMP_ALWAYS, 1, 0);
        for (i = 0; i < 6; i++) {
            pvr_vertex_t *o = (pvr_vertex_t *)Out(LIST_OP);
            o->flags = (i % 3 == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            o->x = cx[i] ? 640.0f : 0.0f; o->y = cy[i] ? 480.0f : 0.0f; o->z = BG_Z;
            o->u = o->v = 0; o->argb = 0xFF000000; o->oargb = 0;
            Commit(LIST_OP, o);
        }
    }
}

void plat_gfx_end(void) {
    sT[4] = timer_us_gettime64();
    pvr_dr_finish();
    pvr_list_finish();
    /* the solid and punch-through lists, kept until now */
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(sListBuf[0], sListN[0]);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_PT_POLY);
    if (sListN[1]) pvr_prim(sListBuf[1], sListN[1]);
    pvr_list_finish();
    pvr_scene_finish();
}

/* the frame on screen, as N64 RGBA5551 at 320 x 240 (the transitions copy it): read where
   KallistiOS's own screenshots read it (vram_s, the frame being shown, line by line). The
   address pvr_get_front_buffer() gives is for the texture view of video memory, where the
   frame isn't laid out line by line (it read as noise). */
void plat_gfx_grab(u16 *dst, s32 w, s32 h) {
    const u16 *fb = (const u16 *)vram_s;
    s32 x, y;
    if (fb == NULL) { memset(dst, 0, w * h * 2); return; }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            u16 c = fb[(y * 2) * 640 + x * 2];          /* RGB565 */
            dst[y * w + x] = (u16)((c & 0xFFC0) | ((c & 0x1F) << 1) | 1);
        }
}

/* ------------------------------------------------------------------ start */

static void *GameMain(void *arg) {
    (void)arg;
    port_main();
    return NULL;
}

int main(int argc, char **argv) {
    pvr_init_params_t params = {
        { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
        512 * 1024, 0, 0, 1, 3
    };
    u32 v, b;
    (void)argc; (void)argv;

    params.autosort_disabled = 1;
    for (v = 0; v < 1024; v++) {
        u32 s = 0;
        for (b = 0; b < 10; b++) s |= ((v >> b) & 1) << (2 * b);
        sSpread[v] = s;
    }
    pvr_init(&params);
    PVR_SET(PVR_PT_ALPHA_REF, 0x80);           /* punch-through: texels at least half opaque */
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    vblank_handler_add(VblankCount, NULL);
    sData = fs_open("/cd/CFSC.DAT", O_RDONLY);
    if (sData < 0) {
        printf("CFSC.DAT not found on the disc\n");
        return 1;
    }
    sDataSize = fs_total(sData);
    {   /* the game's main thread, with a big stack (the N64's was 2 MB) */
        pthread_t t;
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 512 * 1024);
        pthread_create(&t, &a, GameMain, NULL);
        pthread_join(t, NULL);
    }
    return 0;
}
