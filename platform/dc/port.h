/*
 * The Dreamcast port's own layer: what replaces libultra and the game's N64 platform code
 * (src/n64/n64plat.c, n64video.c, n64sched.c, n64audio.c and src/game/boot.c).
 *
 * port/ultra.c   libultra: threads and message queues, the gu matrix maths, guRandom, ...
 * port/sys.c     the game's platform functions with their original names: reading the
 *                converted ROM data, controllers, the frame loop, sound
 * port/gfx.c     runs the game's N64 display lists (F3DEX) and draws them with plat_gfx_*
 * dc/plat_dc.c   the Dreamcast side of plat_* (KallistiOS: PowerVR, maple, AICA, the disc)
 * host/plat_host.c  the same for the PC test build (software drawing to PNG files)
 */
#ifndef PORT_H
#define PORT_H

#include "common.h"
#include "ultra64_min.h"

/* ---- what the platforms provide (dc/plat_dc.c, host/plat_host.c) */
void plat_init(void);
u32  plat_vblank_count(void);               /* retraces since start-up (60 a second) */
void plat_wait_vblank(u32 count);           /* wait until plat_vblank_count() > count */
s32  plat_read_pads(u16 buttons[4], s8 sx[4], s8 sy[4]);   /* N64 button bits; returns the ports present */
void plat_data_read(void *dst, u32 offset, u32 size);   /* from the converted data file (CFSC.DAT) */
u32  plat_data_size(void);
void plat_debug(const char *s);
void plat_exit(s32 code);
void plat_mark(s32 point);                  /* (timing) 0: the game's frame built, 1: next frame starts */

/* drawing: screen-space triangles, textures and fills (gfx.c turns display lists into these).
   x, y in N64 pixels (320 x 240); z 0 (near) .. 1 (far); q = 1/w (perspective);
   u, v over the texture (0..1 = its width / height); colour = texel * argb + oargb */
typedef struct GfxVert {
    f32 x, y, z, q;
    f32 u, v;
    u32 argb;
    u32 oargb;
} GfxVert;
typedef struct GfxTex {
    u32 key;                  /* identifies the texture's contents */
    u16 *texels;              /* w x h, TEXFMT_* (filled only when the platform doesn't know key) */
    s32 w, h;                 /* powers of two, 8..1024 */
    s32 fmt;
    s32 wrapS, wrapT;         /* WRAP_* */
    s32 filter;               /* bilinear */
} GfxTex;
enum { TEXFMT_1555, TEXFMT_4444 };
enum { WRAP_CLAMP, WRAP_REPEAT, WRAP_MIRROR };
enum { BLEND_OPAQUE, BLEND_CUTOUT, BLEND_ALPHA, BLEND_ADD };

void plat_gfx_begin(void);
s32  plat_tex_known(u32 key);
/* n / 3 triangles; tex NULL = untextured; offset: oargb is used; zcmp GFX_ZCMP_SCENERY (the
   Dreamcast only): not depth-tested, but part of the arena (drawn before what stands in it) */
#define GFX_ZCMP_SCENERY 2
/* the port's marks in the display list (G_SPNOOP's second word) around the arena's passes */
#define GFX_MARK_SCENERY_ON  0x53434E31u
#define GFX_MARK_SCENERY_OFF 0x53434E30u
void plat_gfx_draw(const GfxVert *v, s32 n, const GfxTex *tex, s32 blend, s32 zcmp, s32 zwrite, s32 offset);
void plat_gfx_fill(s32 x0, s32 y0, s32 x1, s32 y1, u32 argb);   /* x1, y1 not included */
void plat_gfx_clear_depth(void);
void plat_gfx_end(void);                                        /* show the frame */
void plat_gfx_grab(u16 *dst, s32 w, s32 h);                     /* the last frame, N64 RGBA5551 */

/* sound: the platform plays waves (port/audio.c does the rest); see tools/port_sound.py */
typedef struct PWave {
    u32 off, bytes;           /* the data: from the start of its bank file */
    u32 samples, loopStart, loopEnd;
    s32 loopCount;            /* 0: plays once; else loops from loopStart to loopEnd for ever */
    u32 fmt;                  /* SND_* */
} PWave;
enum { SND_PCM16, SND_PCM8, SND_ADPCM, SND_STREAM };   /* (SND_STREAM: ADPCM, too long for one buffer) */
void plat_snd_init(void);
void plat_snd_lock(void);
void plat_snd_unlock(void);
u32  plat_sound_read(void *dst, u32 offset, u32 size);   /* from SOUND.DAT; the bytes read */
void plat_voice_start(s32 v, const PWave *w, const u8 *data, f32 hz, f32 vol, f32 pan);   /* vol, pan 0..1 */
void plat_voice_set(s32 v, f32 hz, f32 vol, f32 pan);
void plat_voice_stop(s32 v);
void plat_snd_forget(const void *file, u32 size);   /* a bank file is about to be freed */
void plat_snd_retrace(void);               /* once a retrace (the PC build ticks and mixes here) */

/* ---- port/audio.c */
void audio_tick(f32 dt);                    /* the music, envelopes and voices: dt seconds on */
void audio_retrace(void);                   /* once a retrace */

/* ---- port/ultra.c */
void port_threads_init(void);
void port_add_retrace_queue(OSMesgQueue *q);
void port_add_tick_queue(OSMesgQueue *q);        /* (a logic thread's: missed retraces kept) */
void port_remove_retrace_queue(OSMesgQueue *q);
void port_yield(void);                      /* in loops that wait for another thread */
extern u32 gPortRetraces;

/* ---- port/sys.c */
void port_main(void);
void port_rom_init(void);

/* ---- port/gfx.c */
void gfx_run(u32 *dl);
void gfx_init(void);

#endif
