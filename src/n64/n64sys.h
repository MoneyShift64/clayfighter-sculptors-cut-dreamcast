/*
 * The game's own N64 platform layer (0x80076A70-0x8007E720): cartridge DMA and controllers
 * (MPFPLAT.C), video (N64VIDEO.C), sound (N64AUDIO.C), the RSP scheduler (N64RSPSC.C) and the
 * game's graphics scheduler (N64GAMSC.C).
 *
 * All of it sits on top of libultra (Nintendo's SDK: threads, message queues, PI/SI/VI/AI, the
 * audio library al*, osSp*), which stays in assembly and is replaced as a whole on the Dreamcast.
 * libultra functions keep their ROM symbol names (func_8009xxxx); the comment says which
 * libultra 2.0 function each one is.
 */
#ifndef N64SYS_H
#define N64SYS_H

#include "common.h"
#include "ultra64_min.h"
#include "mpfmem.h"

/* ------------------------------------------------------------------ libultra */

typedef struct OSContStatus { u16 type; u8 status; u8 errnum; } OSContStatus;     /* 4 bytes */
typedef struct OSContPad { u16 button; s8 stick_x; s8 stick_y; u8 errnum; } OSContPad;  /* 6 */
#define CONT_NO_RESPONSE_ERROR 0x8

typedef struct OSIoMesg { u8 raw[0x18]; } OSIoMesg;         /* a PI DMA request */

/* an RSP task (libultra OSTask, 0x40 bytes) */
typedef struct OSTask {
    /* 0x00 */ u32 type;                 /* 1 = graphics, 2 = audio */
    /* 0x04 */ u32 flags;
    /* 0x08 */ void *ucode_boot;
    /* 0x0C */ u32 ucode_boot_size;
    /* 0x10 */ void *ucode;
    /* 0x14 */ u32 ucode_size;
    /* 0x18 */ void *ucode_data;
    /* 0x1C */ u32 ucode_data_size;
    /* 0x20 */ void *dram_stack;
    /* 0x24 */ u32 dram_stack_size;
    /* 0x28 */ void *output_buff;
    /* 0x2C */ void *output_buff_size;   /* (the end of the buffer) */
    /* 0x30 */ void *data_ptr;           /* display list / audio command list */
    /* 0x34 */ u32 data_size;
    /* 0x38 */ void *yield_data_ptr;
    /* 0x3C */ u32 yield_data_size;
} OSTask;

/* libultra: PI, SI, VI, timers, caches, threads */
void  func_800989B0(s32 pri, OSMesgQueue *cmdQ, OSMesg *cmdBuf, s32 cmdMsgCnt); /* osCreatePiManager */
s32   func_800991C0(OSIoMesg *mb, s32 pri, s32 direction, u32 devAddr, void *vAddr, u32 nbytes,
                    OSMesgQueue *mq);                                          /* osPiStartDma */
void  func_800998F0(void *p, s32 len);                                         /* osWritebackDCache */
void  func_80099700(void *p, s32 len);                                         /* osInvalDCache */
void  func_80099970(void);                                                     /* osWritebackDCacheAll */
void  func_80099F60(s32 event, OSMesgQueue *mq, OSMesg msg);                    /* osSetEventMesg */
s32   func_8009A7D0(OSMesgQueue *mq, u8 *bitpattern, OSContStatus *status);    /* osContInit */
s32   func_8009A5F0(OSMesgQueue *mq);                                          /* osContStartReadData */
void  func_8009A678(OSContPad *pads);                                          /* osContGetReadData */
u64   func_80099AE0(void);                                                     /* osGetTime */
void  func_800980C0(s32 pri);                                                  /* osCreateViManager */
void *func_80098070(void);                                                     /* osViGetCurrentFramebuffer */
void  func_80098450(void *mode);                                               /* osViSetMode */
void  func_80098950(s32 black);                                                /* osViBlack */
void  func_800984A0(u32 features);                                             /* osViSetSpecialFeatures */
void  func_80098610(void *fb);                                                 /* osViSwapBuffer */
void  func_800983F0(OSMesgQueue *mq, OSMesg msg, u32 retraceCount);            /* osViSetEvent */
void  func_800999D0(void *t, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri); /* osCreateThread */
void  func_8009A0D0(void *t);                                                  /* osStartThread */
u32   func_8009A530(void *p);                                                  /* osVirtualToPhysical */
void  func_80097DC0(OSTask *t);                                                /* osSpTaskLoad */
void  func_80097FCC(OSTask *t);                                                /* osSpTaskStartGo */
void  func_80098000(void);                                                     /* osSpTaskYield */
s32   func_80098020(OSTask *t);                                                /* osSpTaskYielded */
u64   func_800A4150(u64 a, u64 b);                                             /* __udivdi3 */
u32   guRandom(void);
s32   n64NextThreadId(void);                     /* a new thread id (0x80076A50, in spline.c) */

/* libultra: audio */
typedef struct ALHeap { u8 *base; u8 *cur; s32 len; s32 count; } ALHeap;
typedef struct ALLink { struct ALLink *next, *prev; } ALLink;
void  func_80093490(ALHeap *hp, u8 *base, s32 len);                            /* alHeapInit */
void *func_800934D0(const char *file, s32 line, ALHeap *hp, s32 num, s32 size); /* alHeapDBAlloc */
void  func_8009343C(void *ln, void *to);                                       /* alLink */
void  func_8009345C(void *ln);                                                 /* alUnlink */
s32   func_80097BF0(u32 frequency);                                            /* osAiSetFrequency */
s32   func_80097D20(void *buf, u32 size);                                      /* osAiSetNextBuffer */
void  func_800933E0(void *globals, void *config);                              /* alInit */
void  alBnkfNew(void *file, u32 table);
void *func_800960EC(void *cmdList, s32 *cmdLen, s16 *outBuf, s32 outLen);      /* alAudioFrame */
/* the compressed-sequence player */
void  func_800940D0(void *seqp, void *config);                                 /* alCSPNew */
void  func_80094010(void *seqp, void *bank);                                   /* alCSPSetBank */
void  func_80093520(void *seq, void *data);                                    /* alCSeqNew */
void  func_80094040(void *seqp, void *seq);                                    /* alCSPSetSeq */
void  func_80094070(void *seqp, s16 vol);                                      /* alCSPSetVol */
void  func_80093FE0(void *seqp);                                               /* alCSPPlay */
void  func_800940A0(void *seqp);                                               /* alCSPStop */
/* the sound player */
void  func_800953F0(void *sndp, void *config);                                 /* alSndpNew */
s16   func_80095B40(void *sndp, void *sound);                                  /* alSndpAllocate */
void  func_80095C00(void *sndp, s16 id);                                       /* alSndpDeallocate */
void  func_80095C50(void *sndp, s16 id);                                       /* alSndpSetSound */
void  func_80095C60(void *sndp);                                               /* alSndpPlay */
void  func_80095CC0(void *sndp);                                               /* alSndpStop */
s32   func_80095D10(void *sndp);                                               /* alSndpGetState (0 = stopped) */
void  func_80095D30(void *sndp, f32 pitch);                                    /* alSndpSetPitch */
void  func_80095D80(void *sndp, s16 vol);                                      /* alSndpSetVol */
void  func_80095DD0(void *sndp, u8 pan);                                       /* alSndpSetPan */

/* libultra's sound bank structures (alBnkfNew turns their offsets into pointers) */
typedef struct ALEnvelope { s32 attackTime, decayTime, releaseTime; u8 attackVolume, decayVolume; } ALEnvelope;
typedef struct ALKeyMap { u8 velocityMin, velocityMax, keyMin, keyMax, keyBase; s8 detune; } ALKeyMap;
typedef struct ALSound { ALEnvelope *envelope; ALKeyMap *keyMap; void *wavetable; u8 samplePan, sampleVolume, flags; } ALSound;
typedef struct ALInstrument {
    u8 volume, pan, priority, flags;
    u8 tremType, tremRate, tremDepth, tremDelay;
    u8 vibType, vibRate, vibDepth, vibDelay;
    s16 bendRange;
    s16 soundCount;
    ALSound *soundArray[1];
} ALInstrument;
typedef struct ALBank { s16 instCount; u8 flags, pad; s32 sampleRate; ALInstrument *percussion; ALInstrument *instArray[1]; } ALBank;
typedef struct ALBankFile { s16 revision; s16 bankCount; ALBank *bankArray[1]; } ALBankFile;

/* ------------------------------------------------------------------ MPFPLAT.C: DMA, controllers */
void RomCopy(void *dst, u32 romAddr, u32 size);
void func_80076A70(void);
void func_80076B90(void);
void func_80076BC4(void);
void func_80076DB8(void);

/* ------------------------------------------------------------------ N64VIDEO.C */
extern s32 B_800F5D00, B_800F5BEC;         /* the screen's width and height */
extern s32 B_800F87B0;                     /* number of frame buffers */
extern u16 **B_800F777C;                   /* the frame buffers */
extern s32 *B_800F7D58;                    /* each frame buffer's state (FB_...) */
extern void *B_800F2D4C;                   /* the Z buffer */
extern u64 B_800F88C0;                     /* osGetTime() at the last vertical retrace */
extern volatile u32 D_800B09B0, D_800B09B4; /* vertical retrace counter (64-bit: high, low) */

enum { FB_FREE = 0, FB_DRAWING = 1, FB_READY = 2, FB_SHOWN = 3 };

void n64InitVideo(s32 viMode, s32 numBuffers, u32 fbAddr, u32 zbAddr);
void func_800779F4(OSMesgQueue *q);        /* get a message every vertical retrace */
void func_80077AF4(OSMesgQueue *q);        /* ... no longer */
s32  func_80077BEC(void);                  /* the next frame buffer to draw in */
s32  func_80077CE4(s32 fb, s32 wait);      /* claim a free frame buffer for drawing */
void func_80077E30(s32 fb);                /* a frame buffer is finished: show it */
s32  func_80077F10(void);                  /* is any frame buffer still being drawn or waiting? */
void func_80077F74(s32 r, s32 g, s32 b);   /* fill every frame buffer with a colour */
void func_80078040(const char *s, s32 x, s32 y, s32 colour); /* debug text into every frame buffer */
void func_80078138(s32 c, s32 x, s32 y, u16 *fb, s32 colour); /* one character */
void func_800782B0(void);                  /* swap in the next ready frame buffer */
void func_800784BC(void *arg);             /* the retrace thread */

/* ------------------------------------------------------------------ N64AUDIO.C */
/* one playing (or stopping) sound effect: 0x14 bytes */
typedef struct SfxSlot {
    /* 0x00 */ s16 id;                     /* sound player id, -1 = free */
    /* 0x04 */ ALBankFile *bankFile;
    /* 0x08 */ u32 serial;                 /* order of starting (the oldest is stolen) */
    /* 0x0C */ s32 looped;                 /* the sound's envelope has a release (keeps playing) */
    /* 0x10 */ u32 frame;                  /* audio frame it was started in */
} SfxSlot;

/* an audio task for the RSP: 0x58 bytes */
typedef struct N64AudioTask {
    /* 0x00 */ OSTask t;
    /* 0x40 */ OSMesgQueue *doneQ;         /* (never set: no reply) */
    /* 0x44 */ s32 v44;
    /* 0x48 */ u64 startTime;
    /* 0x50 */ u8 pad50[8];
} N64AudioTask;

void  func_800786B0(u32 frequency, s32 maxVoices, s32 maxSounds, s32 fxType); /* audio start-up */
void *func_80078E44(u32 romAddr, u32 size, u32 romTable);  /* load a sound bank */
void  n64FreeSampleBank(void *bank);
void  func_800794CC(ALBankFile *bankFile, s32 bank);       /* the music's instrument bank */
void  func_800795B4(void *seqData);                        /* play a tune */
void  func_800796C0(void);                                 /* stop the music */
void  func_800797EC(ALBankFile *bf, s32 bank, s32 inst, s32 sound, s32 volume, s32 pan, s32 restart,
                    f32 pitch, s32 randomPitch);           /* play a sound effect */
void  func_80079D70(ALBankFile *bf, s32 bank, s32 inst, s32 sound, s32 volume, s32 pan); /* ... at music volume */
void  func_8007A0BC(ALBankFile *bf);                       /* stop a bank's looped sounds */
s32   n64PlaySFX(ALBankFile *bf, s32 bank, s32 inst, s32 sound); /* is that sound a looped one? */
s32   func_8007A3B8(ALBankFile *bf);                       /* is a looped sound of the bank playing? */
void  func_8007A530(void);                                 /* stop every sound */
s32   func_8007A710(void);                                 /* stop one sound; 0 if none was playing */
void  func_8007A8F4(s32 volume);                           /* music volume */
void  func_8007AA04(s32 volume);                           /* sound effects volume */
void  func_8007AB70(f32 scale);                            /* master volume */
void  func_8007AC68(void *arg);                            /* the audio thread */
void  CycleAudio(void);
s32   func_8007AFC8(void);                                 /* a free sound slot (stealing the oldest) */
void  func_8007B144(void);                                 /* free the slots of finished sounds */
void *func_8007B294(void *state);                          /* the synthesizer's DMA set-up */
s32   SynVoice_CallBack(s32 addr, s32 len, void *state);   /* sample data DMA */
void  func_8007B548(void);                                 /* recycle old DMA buffers */

/* ------------------------------------------------------------------ N64RSPSC.C */
/* a graphics task: 0x268 bytes */
typedef struct N64GfxTask {
    /* 0x000 */ OSTask t;
    /* 0x040 */ s32 waitForDP;             /* finished only when the RDP is done too */
    /* 0x044 */ OSMesgQueue *doneQ;        /* gets the task when it is finished */
    /* 0x048 */ struct N64GfxTask *next;   /* the queue of tasks waiting for the RSP */
    /* 0x04C */ s32 numRuns;               /* times started (first start and every resume after a yield) */
    /* 0x050 */ u64 startTime[32];
    /* 0x150 */ u64 yieldTime[32];
    /* 0x250 */ u64 dpStartTime;
    /* 0x258 */ u64 dpEndTime;
    /* 0x260 */ s32 dpDone;
    /* 0x264 */ s32 v264;
} N64GfxTask;

void func_8007B600(void);                  /* RSP scheduler start-up */
void func_8007B7F4(N64GfxTask *t);         /* queue a graphics task */
void func_8007B904(N64AudioTask *t);       /* run an audio task (it interrupts the graphics) */
void func_8007BA18(void);                  /* start the next task */
void RSPThreadProc(void *arg);
void func_8007BDAC(void *arg);             /* the RDP thread */
void func_8007BF20(void *arg);
void func_8007BF50(void *arg);             /* the reset (pre-NMI) thread */

/* ------------------------------------------------------------------ N64GAMSC.C */
enum { DL_FREE = 0, DL_BUILDING = 1, DL_BUILT = 2, DL_RUNNING = 3, DL_DONE = 4 };
extern u32 *B_800F89E4;                    /* the display list being built (gDL) */
extern void *B_800F77D0;                   /* the vertex buffer (gVtx) */

void func_8007BF80(s32 numBuffers);        /* graphics scheduler start-up */
void func_8007C5BC(s32 dlSize, s32 mtxCount, s32 vtxCount); /* resize the display-list buffers */
void func_8007C850(void);                  /* wait for a free display-list buffer and start a frame */
void n64GameSched_CheckinCPU(void);        /* the frame's display list is finished */
void func_8007CCA4(void);                  /* wait until all the graphics are done */
void func_8007CDD4(void);                  /* performance bars */
void func_8007D620(void);                  /* performance bars (other scale) */
void func_8007E034(void);                  /* send built display lists, show finished frames */
void func_8007E404(void *arg);             /* its retrace thread */
void GFXRSPThreadProc(void *arg);          /* graphics task finished */

/* ------------------------------------------------------------------ the recursive lock (MPF_Mutex) */
static inline void N64_Lock(MPF_Mutex *m) {
    if (m->owner == osGetThreadId(NULL)) {
        m->count++;
    } else {
        osRecvMesg(&m->mq, NULL, OS_MESG_BLOCK);
        m->owner = osGetThreadId(NULL);
        m->count++;
    }
}
static inline void N64_Unlock(MPF_Mutex *m) {
    if (m->owner == osGetThreadId(NULL)) {
        if (--m->count == 0) {
            m->owner = 0;
            osSendMesg(&m->mq, NULL, OS_MESG_NOBLOCK);
        }
    }
}

/* waits for the next vertical retrace (the counter D_800B09B0/B4 to change); its own function so
   the tests can stand in for it */
void N64_WaitRetrace(void);

#endif /* N64SYS_H */
