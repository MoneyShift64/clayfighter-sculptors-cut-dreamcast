/*
 * CF3SEQ.C (0x80075A60-0x80076A50): the fighter animation sequence interpreter.
 * See cf3seq.h for the sequence state.
 *
 * Command bytes (operands are big-endian; the size includes the command byte):
 *   0x00-0x7F  sprite layer (6): frame = (b & 0x1F) << 8 | next byte, s16 x, s16 y;
 *              bit 0x20 = mirrored, bit 0x40 = flag 2
 *   0x80 (4) speed = u16 / 2          0x81 (4) x velocity    0x82 (4) y velocity
 *   0x93 (14) attack rectangle + 2 values    0x94 (10) block rectangle
 *   0x95 (10) extra rectangle         0xA0-0xA7 (6) value pairs
 *   0xB0 (2) 0xB1 (2) 0xB2 (4) 0xB3 (4) 0xB4 (4) flags/values for the fight code
 *   0xB5 (4) wait timer   0xB6 (2) set loop point   0xB7 (2) loop back (count)
 *   0xB8/0xB9 (4) wait for a value   0xBA (2) wait for 'ready'   0xBB (2) wait for timer
 *   0xBC-0xBF, 0xC4, 0xC5 (4) values / 100   0xC0 (2) signed byte   0xC8 (4) 0xC9 (4)
 *   0xFE (2) end of frame    0xFF end of sequence
 */
#include "cf3seq.h"

#define CMD_S16(p, o) (*(s16 *)((p) + (o)))
#define CMD_U16(p, o) (*(u16 *)((p) + (o)))
#define PERCENT(v) ((f32)((f64)(f32)(s32)(v) / 100.0))

static void FlipRect(SeqRect *r) {
    s16 right = -r->right;

    r->right = -r->left;
    r->left = right;
}

/* 0x80075A60 */
void CharSeq_Stop(CharSeq *s) {
    s->seq = NULL;
    s->mirrored = 0;
}

/* 0x80075A6C: loops = how many times to play (-1 = for ever, 0 = don't start);
   speed = fixed playback speed, or -1 to let the sequence set it */
void CharSeq_Start(CharSeq *s, u8 *seq, s32 loops, u32 speed) {
    s->seq = NULL;
    if (loops == 0) {
        return;
    }
    s->seq = seq;
    s->loopCount = loops;
    s->holdFrame = -1;
    s->hold = 0;
    s->finished = 0;
    s->loopsDone = 0;
    s->depth = 0;
    s->reverse = 0;
    s->frame = 0;
    s->ptr = s->seq + 0x10;
    s->loopFrame = 0;
    s->loopPtr = s->seq + 0x10;
    s->flags = CMD_U16(s->seq, 0);
    s->hdrE = (s8)s->seq[0xE];
    s->hdrF = (s8)s->seq[0xF];

    s->hasVal5C = (s->flags >> 6) & 1;
    if (s->hasVal5C) {
        s->val5C = PERCENT(CMD_S16(s->seq, 2));
    } else {
        s->val5C = 0.0f;
    }
    s->hasVal64 = (s->flags >> 10) & 1;
    if (s->hasVal64) {
        s->val64 = PERCENT(CMD_S16(s->seq, 8));
    } else {
        s->val64 = 0.0f;
    }
    s->hasVal6C = (s->flags >> 7) & 1;
    if (s->hasVal6C) {
        s->val6C = PERCENT(CMD_S16(s->seq, 4));
    } else {
        s->val6C = 0.0f;
    }
    s->hasVal74 = (s->flags >> 8) & 1;
    if (s->hasVal74) {
        s->val74 = PERCENT(CMD_S16(s->seq, 6));
    } else {
        s->val74 = 0.0f;
    }
    s->hdrA = (s8)s->seq[0xA];
    s->flag800 = (s->flags >> 11) & 1;
    s->hdrB = s->seq[0xB];
    s->hdrC = s->seq[0xC];
    s->hdrD = s->seq[0xD];

    s->speed = 0x100;
    s->time = 0;
    s->numLayers = 0;
    s->numAttacks = 0;
    s->numBlocks = 0;
    s->velX = 0;
    s->velY = 0;
    s->pairs[0].set = 0;
    s->pairs[0].a = 0;
    s->pairs[0].b = 0;
    s->pairs[1].set = 0;
    s->pairs[2].set = 0;
    s->flag20 = s->flags & 0x20;
    s->cmdB1 = 0;
    s->hasRect95 = 0;
    s->waitTimer = 0.0f;
    s->cmdB5 = 0;
    s->waitReady = 0;
    s->ready = 1;
    s->waitA = 0;
    s->cmdBCValue = 0.0f;
    s->waitATarget = 0.0f;
    s->waitB = 0;
    s->cmdBDValue = 0.0f;
    s->waitBTarget = 0.0f;
    s->waitTimerActive = 0;
    s->cmdBE = 0;
    s->cmdBEValue = 0.0f;
    s->cmdBF = 0;
    s->cmdBFValue = 0.0f;
    s->cmdC0Value = 0;
    s->cmdC8Value = 0.0f;
    s->fixedSpeed = (speed != 0xFFFFFFFF);
    if (s->fixedSpeed) {
        if (speed == 0) {
            __builtin_trap();       /* the original divides by zero here */
        }
        s->fixedStep = (0xC800 / speed) >> 1;
    }
    UpdateCharacterAnimation(s);
}

/* 0x80075D18: fast-forwards to 'frame' (stops early if the sequence loops) */
void CharSeq_RunUntil(CharSeq *s, s32 frame) {
    s32 loops = s->loopCount;

    while (s->seq != NULL && s->frame < frame) {
        if (s->loopCount != loops) {
            return;
        }
        s->waitA = 0;
        s->waitB = 0;
        s->waitTimerActive = 0;
        s->waitReady = 0;
        s->time = 0;
        UpdateCharacterAnimation(s);
    }
}

/* 0x80075D98: play back through the frame history (e.g. pulling a move back) */
void CharSeq_Reverse(CharSeq *s) {
    if (s->reverse) {
        return;
    }
    if (s->depth < 2) {
        s->seq = NULL;
        return;
    }
    s->depth--;
    s->reverse = 1;
    s->waitA = 0;
    s->waitB = 0;
    s->waitReady = 0;
    s->waitTimerActive = 0;
    s->time = 0;
}

/* back to the loop point; returns 0 if the sequence is over */
static s32 LoopBack(CharSeq *s) {
    s->frame = s->loopFrame;
    s->ptr = s->loopPtr;
    s->loopsDone++;
    return s->ptr[0] != 0xFF;
}

/* 0x80075DEC: runs one game frame of the sequence */
void UpdateCharacterAnimation(CharSeq *s) {
    u8 *p;
    u8 b;
    s32 i;

    if (s->seq == NULL) {
        return;
    }

    /* waits that hold the sequence on its current frame */
    if (s->waitA) {
        if (s->mirrored) {
            if (!(-(f64)s->waitATarget <= (f64)s->waitAValue)) {
                return;
            }
        } else if (!(s->waitATarget <= s->waitAValue)) {
            return;
        }
        s->waitA = 0;
        s->time = 0;
    }
    if (s->waitB) {
        if (!(s->waitBValue < s->waitBTarget)) {
            if (!s->ready || !(0.0f <= s->waitBTarget) || !(0.0f <= s->waitBValue)) {
                return;
            }
        }
        s->waitB = 0;
        s->time = 0;
    }
    if (s->waitTimerActive) {
        if (!((f64)s->waitTimer <= 0.0)) {
            return;
        }
        s->waitTimerActive = 0;
        s->waitTimer = 0.0f;
    }
    if (s->waitReady) {
        if (!s->ready) {
            return;
        }
        s->waitReady = 0;
        s->time = 0;
    }
    if (s->time > 0) {
        f32 elapsed = ScaleByFrameTime(256.0f);
        s->time = (s32)((f32)s->time - elapsed);
        if (s->time > 0) {
            return;
        }
    }

    /* frame history: remember this frame, or step back when reversing */
    if (!s->reverse) {
        if (s->depth < SEQ_STACK_SIZE) {
            s->stack[s->depth++] = s->ptr;
        }
    } else if (s->depth == 0) {
        s->seq = NULL;
    } else {
        s->depth--;
        s->ptr = s->stack[s->depth];
    }

    s->waitReady = 0;
    s->waitB = 0;
    s->waitA = 0;
    s->waitTimerActive = 0;

    if (s->ptr[0] == 0xFF) {
        s->finished = 1;
        if (s->loopCount != -1) {
            s->loopCount--;
            if (s->loopCount <= 0) {
                s->seq = NULL;
                return;
            }
        }
        if (!LoopBack(s)) {
            return;
        }
    }
    if (s->hold) {
        if (s->loopCount != -1) {
            s->loopCount--;
            if (s->loopCount <= 0) {
                goto run;
            }
        }
        if (!LoopBack(s)) {
            return;
        }
    }

run:
    s->hold = 0;
    s->numLayers = 0;
    s->numAttacks = 0;
    s->numBlocks = 0;
    s->velX = 0;
    s->velY = 0;
    s->pairs[0].set = 0;
    for (i = 1; i < 8; i++) {
        s->pairs[i].set = 0;
    }
    s->cmdB1 = 0;
    s->cmdB0 = 0;
    s->cmdB0Value = 0;
    s->cmdB2Value = -1;
    s->cmdB2Negative = -1;
    s->cmdB3 = 0;
    s->cmdB4 = 0;
    s->cmdBC = 0;
    s->cmdBD = 0;
    s->cmdBCValue = 0.0f;
    s->cmdBDValue = 0.0f;
    s->cmdBE = 0;
    s->cmdBEValue = 0.0f;
    s->cmdBF = 0;
    s->cmdBFValue = 0.0f;
    s->cmdB5 = 0;
    s->cmdC0 = 0;
    s->cmdC4 = 0;
    s->cmdC5 = 0;
    s->cmdC8 = 0;
    s->cmdC9 = 0;

    p = s->ptr;
    for (;;) {
        b = p[0];
        if (!(b & 0x80)) {
            SeqLayer *l;

            if (s->numLayers >= SEQ_MAX_LAYERS) {
                mpFatalError("CF3SEQ.C - Too many frames in state!");
            }
            l = &s->layers[s->numLayers];
            l->frame = (b & 0x1F) << 8;
            l->frame += p[1];
            l->x = CMD_U16(p, 2);
            l->y = CMD_U16(p, 4);
            l->flags = 0;
            if (b & 0x20) {
                l->flags |= 1;
            }
            if (b & 0x40) {
                l->flags |= 2;
            }
            if (s->mirrored) {
                l->flags ^= 1;
                l->x = -l->x;
            }
            s->numLayers++;
            p += 6;
            continue;
        }
        if (b == 0xFE) {
            p += 2;
            s->ptr = p;
            s->frame++;
            break;
        }
        switch (b) {
            case 0x80:
                if (!s->reverse) {
                    s->speed = CMD_U16(p, 2) >> 1;
                }
                p += 4;
                break;
            case 0x81: {
                s32 dir = s->mirrored ? -1 : 1;
                if (s->reverse) {
                    dir = -dir;
                }
                s->velX = CMD_S16(p, 2) * dir;
                p += 4;
                break;
            }
            case 0x82:
                s->velY = CMD_S16(p, 2) * (s->reverse ? -1 : 1);
                p += 4;
                break;
            case 0x93:
                if (s->numAttacks >= SEQ_MAX_ATTACKS) {
                    mpFatalError("CF3SEQ.C - Too many attack rectangles in state!");
                }
                s->attacks[s->numAttacks].left = CMD_U16(p, 2);
                s->attacks[s->numAttacks].top = CMD_U16(p, 4);
                s->attacks[s->numAttacks].right = CMD_U16(p, 6);
                s->attacks[s->numAttacks].bottom = CMD_U16(p, 8);
                s->attackA[s->numAttacks] = CMD_U16(p, 10);
                s->attackB[s->numAttacks] = CMD_U16(p, 12);
                if (s->mirrored) {
                    FlipRect(&s->attacks[s->numAttacks]);
                    s->attackA[s->numAttacks] = -s->attackA[s->numAttacks];
                }
                s->numAttacks++;
                p += 14;
                break;
            case 0x94:
                if (s->numBlocks >= SEQ_MAX_BLOCKS) {
                    mpFatalError("CF3SEQ.C - Too many block rectangles in state!");
                }
                s->blocks[s->numBlocks].left = CMD_U16(p, 2);
                s->blocks[s->numBlocks].top = CMD_U16(p, 4);
                s->blocks[s->numBlocks].right = CMD_U16(p, 6);
                s->blocks[s->numBlocks].bottom = CMD_U16(p, 8);
                if (s->mirrored) {
                    FlipRect(&s->blocks[s->numBlocks]);
                }
                s->numBlocks++;
                p += 10;
                break;
            case 0x95:
                s->rect95.left = CMD_U16(p, 2);
                s->rect95.top = CMD_U16(p, 4);
                s->rect95.right = CMD_U16(p, 6);
                s->rect95.bottom = CMD_U16(p, 8);
                if (s->mirrored) {
                    FlipRect(&s->rect95);
                }
                s->hasRect95 = 1;
                p += 10;
                break;
            case 0xA0: case 0xA1: case 0xA2: case 0xA3:
            case 0xA4: case 0xA5: case 0xA6: case 0xA7: {
                SeqPair *pr = &s->pairs[b - 0xA0];
                pr->set = 1;
                pr->a = CMD_U16(p, 2);
                pr->b = CMD_U16(p, 4);
                if (s->mirrored) {
                    pr->a = -pr->a;
                }
                p += 6;
                break;
            }
            case 0xB0:
                s->cmdB0 = 1;
                s->cmdB0Value = p[1];
                p += 2;
                break;
            case 0xB1:
                s->cmdB1 = 1;
                p += 2;
                break;
            case 0xB2:
                s->cmdB2Value = CMD_S16(p, 2);
                if (s->cmdB2Value < 0) {
                    s->cmdB2Negative = -s->cmdB2Value;
                    s->cmdB2Value = -1;
                }
                p += 4;
                break;
            case 0xB3:
                s->cmdB3 = 1;
                s->cmdB3Value = (f32)CMD_S16(p, 2);
                if (s->mirrored) {
                    s->cmdB3Value = 0.0f - s->cmdB3Value;
                }
                p += 4;
                break;
            case 0xB4:
                s->cmdB4 = 1;
                s->cmdB4Value = PERCENT(CMD_S16(p, 2));
                if (s->mirrored) {
                    s->cmdB4Value = 0.0f - s->cmdB4Value;
                }
                p += 4;
                break;
            case 0xB5:
                s->waitTimer = (f32)(s32)CMD_U16(p, 2);
                s->cmdB5 = 1;
                p += 4;
                break;
            case 0xB6:
                s->loopFrame = s->frame;
                s->loopPtr = s->ptr;
                p += 2;
                break;
            case 0xB7:
                s->hold = 1;
                if (s->holdFrame < s->frame) {
                    s->holdFrame = s->frame;
                    if ((u32)s->loopCount < 2) {
                        s->loopCount = p[1];
                        if (p[1] == 0) {
                            s->loopCount = 1;
                        }
                    }
                }
                p += 2;
                break;
            case 0xB8:
                s->waitAValue = PERCENT(CMD_S16(p, 2));
                s->waitA = 1;
                p += 4;
                break;
            case 0xB9:
                s->waitBValue = PERCENT(CMD_S16(p, 2));
                s->waitB = 1;
                p += 4;
                break;
            case 0xBA:
                s->waitReady = 1;
                p += 2;
                break;
            case 0xBB:
                s->waitTimerActive = 1;
                p += 2;
                break;
            case 0xBC:
                s->cmdBC = 1;
                s->cmdBCValue = PERCENT(CMD_S16(p, 2));
                if (s->mirrored) {
                    s->cmdBCValue = 0.0f - s->cmdBCValue;
                }
                p += 4;
                break;
            case 0xBD:
                s->cmdBD = 1;
                s->cmdBDValue = PERCENT(CMD_S16(p, 2));
                p += 4;
                break;
            case 0xBE:
                s->cmdBE = 1;
                s->cmdBEValue = PERCENT(CMD_S16(p, 2));
                if (s->mirrored) {
                    s->cmdBEValue = 0.0f - s->cmdBEValue;
                }
                p += 4;
                break;
            case 0xBF:
                s->cmdBF = 1;
                s->cmdBFValue = PERCENT(CMD_S16(p, 2));
                p += 4;
                break;
            case 0xC0:
                s->cmdC0 = 1;
                s->cmdC0Value = (s8)p[1];
                p += 2;
                break;
            case 0xC4:
                s->cmdC4 = 1;
                s->cmdC4Value = PERCENT(CMD_S16(p, 2));
                p += 4;
                break;
            case 0xC5:
                s->cmdC5 = 1;
                s->cmdC5Value = PERCENT(CMD_S16(p, 2));
                p += 4;
                break;
            case 0xC8:
                s->cmdC8 = 1;
                s->cmdC8Value = (f32)CMD_S16(p, 2);
                if (s->mirrored) {
                    s->cmdC8Value = -s->cmdC8Value;
                }
                p += 4;
                break;
            case 0xC9:
                s->cmdC9 = 1;
                p += 4;
                break;
            default:
                /* the original doesn't skip the bad byte, so if the error returns it
                   loops here for ever */
                mpFatalError("CF3SEQ.C - Undefined sequence command");
                break;
        }
    }

    /* how long until the next frame */
    if (s->fixedSpeed) {
        s->time += s->fixedStep;
    } else {
        s->time += s->speed;
    }
    if (s->time < 0) {
        s->time = 0;
    }
}
