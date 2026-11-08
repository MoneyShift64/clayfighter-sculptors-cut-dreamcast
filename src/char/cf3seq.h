/*
 * CF3SEQ.C: fighter animation sequences.
 *
 * A sequence is a 16-byte header followed by a byte-code stream. Each game frame
 * the interpreter runs commands until an end-of-frame marker (0xFE): sprite layers to
 * draw, attack and block rectangles, movement, and flags the fight code reacts to.
 * Waits (timers, landing, waiting for the fight code) can hold the sequence on its
 * current frame. 0xFF ends the sequence (it then loops if a loop count is left).
 */
#ifndef CF3SEQ_H
#define CF3SEQ_H

#include "common.h"

#define SEQ_MAX_LAYERS  2
#define SEQ_MAX_ATTACKS 4
#define SEQ_MAX_BLOCKS  4
#define SEQ_STACK_SIZE  10

typedef struct SeqLayer {       /* one sprite drawn this frame */
    /* 0x0 */ u16 frame;        /* frame number in the character file */
    /* 0x2 */ s16 x, y;
    /* 0x6 */ u8  flags;        /* 1 = mirrored, 2 = command bit 0x40 */
    /* 0x7 */ u8  pad;
} SeqLayer;

typedef struct SeqRect {
    s16 left, top, right, bottom;
} SeqRect;

typedef struct SeqPair {        /* "set" flag + two values (commands 0xA0-0xA7) */
    s32 set;
    s16 a, b;
} SeqPair;

typedef struct CharSeq {
    /* 0x000 */ u8  *seq;               /* NULL = not running */
    /* 0x004 */ s32  frame;             /* frames run so far */
    /* 0x008 */ u8  *ptr;               /* start of the next frame's commands */
    /* 0x00C */ s32  loopFrame;         /* loop point (command 0xB6) */
    /* 0x010 */ u8  *loopPtr;
    /* 0x014 */ s32  loopCount;         /* -1 = for ever */
    /* 0x018 */ s32  holdFrame;         /* last frame command 0xB7 ran on */
    /* 0x01C */ s32  loopsDone;
    /* 0x020 */ s32  hold;              /* 0xB7: go back to the loop point */
    /* 0x024 */ s32  finished;          /* reached 0xFF at least once */
    /* 0x028 */ s32  depth;             /* frame history (used to play backwards) */
    /* 0x02C */ u8  *stack[SEQ_STACK_SIZE];
    /* 0x054 */ s32  reverse;           /* playing backwards through the history */
    /* 0x058 */ s32  hasVal5C;
    /* 0x05C */ f32  val5C;             /* header +2 / 100 */
    /* 0x060 */ s32  hasVal64;
    /* 0x064 */ f32  val64;             /* header +8 / 100 */
    /* 0x068 */ s32  hasVal6C;
    /* 0x06C */ f32  val6C;             /* header +4 / 100 */
    /* 0x070 */ s32  hasVal74;
    /* 0x074 */ f32  val74;             /* header +6 / 100 */
    /* 0x078 */ s32  hdrA;              /* header bytes 0xA-0xD */
    /* 0x07C */ s32  flag800;           /* header flags bit 11 */
    /* 0x080 */ s32  hdrB;
    /* 0x084 */ s32  hdrC;
    /* 0x088 */ s32  hdrD;
    /* 0x08C */ u16  flags;             /* header flags */
    /* 0x08E */ u16  pad8E;
    /* 0x090 */ s32  hdrE;
    /* 0x094 */ s32  hdrF;
    /* 0x098 */ s32  mirrored;          /* facing left: x and rectangles are flipped */
    /* 0x09C */ u16  speed;             /* added to 'time' per frame; 0x100 = 1 frame */
    /* 0x09E */ s16  time;              /* > 0: wait before running the next frame */
    /* 0x0A0 */ s32  fixedSpeed;
    /* 0x0A4 */ u16  fixedStep;
    /* 0x0A6 */ u16  padA6;
    /* 0x0A8 */ s32  numLayers;
    /* 0x0AC */ SeqLayer layers[SEQ_MAX_LAYERS];
    /* 0x0BC */ s32  numAttacks;
    /* 0x0C0 */ SeqRect attacks[SEQ_MAX_ATTACKS];
    /* 0x0E0 */ s16  attackA[SEQ_MAX_ATTACKS];   /* negated when mirrored */
    /* 0x0E8 */ s16  attackB[SEQ_MAX_ATTACKS];
    /* 0x0F0 */ s32  numBlocks;
    /* 0x0F4 */ SeqRect blocks[SEQ_MAX_BLOCKS];
    /* 0x114 */ s32  velX;
    /* 0x118 */ s32  velY;
    /* 0x11C */ SeqPair pairs[8];       /* commands 0xA0-0xA7 */
    /* 0x15C */ s32  flag20;            /* header flags & 0x20 */
    /* 0x160 */ s32  cmdB1;
    /* 0x164 */ s32  cmdB0;
    /* 0x168 */ s32  cmdB0Value;
    /* 0x16C */ s32  cmdB2Value;        /* -1 if the value was negative */
    /* 0x170 */ s32  cmdB2Negative;     /* minus the value (-1 = none) */
    /* 0x174 */ s32  hasRect95;
    /* 0x178 */ SeqRect rect95;
    /* 0x180 */ s32  cmdB3;
    /* 0x184 */ f32  cmdB3Value;
    /* 0x188 */ s32  cmdB4;
    /* 0x18C */ f32  cmdB4Value;        /* /100 */
    /* 0x190 */ f32  waitTimer;         /* 0xB5 (with 0xBB: wait until <= 0) */
    /* 0x194 */ s32  cmdB5;
    /* 0x198 */ s32  waitReady;         /* 0xBA: wait until 'ready' */
    /* 0x19C */ s32  ready;             /* set by the fight code */
    /* 0x1A0 */ s32  cmdBC;
    /* 0x1A4 */ s32  waitA;             /* 0xB8: wait until valA >= valB */
    /* 0x1A8 */ f32  waitAValue;
    /* 0x1AC */ f32  cmdBCValue;
    /* 0x1B0 */ f32  waitATarget;       /* set by the fight code */
    /* 0x1B4 */ s32  cmdBD;
    /* 0x1B8 */ s32  waitB;             /* 0xB9 */
    /* 0x1BC */ f32  waitBValue;
    /* 0x1C0 */ f32  cmdBDValue;
    /* 0x1C4 */ f32  waitBTarget;       /* set by the fight code */
    /* 0x1C8 */ s32  waitTimerActive;   /* 0xBB */
    /* 0x1CC */ s32  cmdBE;
    /* 0x1D0 */ f32  cmdBEValue;
    /* 0x1D4 */ s32  cmdBF;
    /* 0x1D8 */ f32  cmdBFValue;
    /* 0x1DC */ s32  cmdC0;
    /* 0x1E0 */ s32  cmdC0Value;
    /* 0x1E4 */ s32  cmdC4;
    /* 0x1E8 */ f32  cmdC4Value;
    /* 0x1EC */ s32  cmdC5;
    /* 0x1F0 */ f32  cmdC5Value;
    /* 0x1F4 */ s32  cmdC8;
    /* 0x1F8 */ f32  cmdC8Value;
    /* 0x1FC */ s32  cmdC9;
} CharSeq;                              /* 0x200 bytes */

void UpdateCharacterAnimation(CharSeq *s);
void CharSeq_Stop(CharSeq *s);
void CharSeq_Start(CharSeq *s, u8 *seq, s32 loops, u32 speed);
void CharSeq_RunUntil(CharSeq *s, s32 frame);
void CharSeq_Reverse(CharSeq *s);

f32 ScaleByFrameTime(f32 v);   /* v * gFrameTicks / 32 (src/char/cf3combt.c) */

#endif
