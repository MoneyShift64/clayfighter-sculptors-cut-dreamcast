/*
 * CF3CHAR.C (0x8005DDC0-0x80069760): the fighter during a fight - all 45 functions.
 * Per frame the fight code calls cfCharacter_HandleInput (controls, special moves),
 * cfCharacter_Update (timers, animation, round states), cfCharacter_UpdatePhysics,
 * cfCharacter_UpdateProjectiles and the hit checks, which end in cfCharacter_HitReaction.
 */
#include "cf3char.h"
#include "mpfplat.h"

extern u8  D_SeqDefaultV170[63];
extern u8  D_SeqDefaultPriority[63];
extern s32 D_SeqDefaultV178[63];
extern s32 sSparkAngle;

/* 0x8005DDC0 */
void cfCharacter_Construct(cfCharacter *c) {
    c->pos[0] = 0.0f;
    c->pos[1] = 0.0f;
    c->pos[2] = 0.0f;
    c->v34 = 0;
    c->v38 = 0;
    c->screenY = 0.0f;
    c->screenX = 0.0f;
    c->prevScreenY = 0.0f;
    c->prevScreenX = 0.0f;
    c->facing = 0;
    c->inFront = 0;
    c->numBodyRects = 0;
    c->numAttacks = 0;
    c->numBlocks = 0;
    c->hasBodyRect = 0;
    c->prevHasBodyRect = 0;
    c->def = NULL;
    mpStrCpy(c->name, "Unnamed");
    mpStrCpy(c->shortName, "Unnamed");
    c->stats[0] = 2.0f;
    c->stats[1] = 1.5f;
    c->stats[2] = 3.0f;
    c->stats[3] = -3.0f;
    c->stats[4] = 6.0f;
    c->handler = NULL;
    c->opponent = NULL;
    c->v2B4 = 0.0f;
    c->v2B8 = 0.0f;
    c->health = 170;
    c->v2C8 = 0;
    c->hitType = -1;
    c->v46C = 0.0f;
    c->v470 = 0.0f;
    c->v474 = 0.0f;
    c->v478 = 0.0f;
    c->v47C = 0;
    c->v480 = 0.0f;
    c->v48C = 0.0f;
    c->v490 = 0.0f;
    c->v498 = 0.0f;
    c->v4A4 = 0.0f;
    c->v49C = 0.0f;
    c->v4A0 = 0.0f;
    c->v4A8 = 0.0f;
    c->v4AC = 0.0f;
    c->angle = 0.0f;
    c->seqIndex = 0;
    CharSeq_Stop(&c->seq);
}

/* 0x8005DF10 */
void cfCharacter_Destruct(cfCharacter *c) {
    (void)c;
}

#define DEF_PTR(c, off) ((u8 *)(c)->def + (off))

/* 0x8005DF18: sets a fighter up from its CharDef (the move data in its file) */
void cfCharacter_Init(cfCharacter *c, CharDef *def) {
    s32 i;

    c->handler = NULL;
    c->opponent = NULL;
    c->v4E0 = 0;
    c->inputHead = 0;
    mpMemSet(c->inputs, sizeof(c->inputs), 0);
    c->vA28 = 0;
    mpMemSet(c->a2c, sizeof(c->a2c), 0);
    c->vF3C = 0;
    c->vF44 = 0;
    c->vF48 = 1;
    c->vF2C = 0;
    for (i = 0; i < 18; i++) {
        c->vF68[i] = -1;
    }
    c->vFB0 = -1.0f;
    c->v3F4 = -1;
    c->numLinks = 0;
    c->def = def;
    if (((((const volatile u8 *)def)[0] << 8) | ((const volatile u8 *)def)[1]) != 0x0521) {   /* two bytes (read one at a time: the same on either byte order) */
        mpFatalErrorf("CF3CHAR.C - Attempt to initialize character with CharDef version %d.%02d - expecting version %d.%02d",
                      def->versionMajor, def->versionMinor, 5, 0x21);
    }
    if (c->def->bigEndian != 1) {
        mpFatalErrorf("CF3CHAR.C - Attempt to initialize character with little-endian CharDef.  Expecting big-endian");
    }
    c->v24[0] = c->pos[0];
    c->v24[1] = c->pos[1];
    c->v24[2] = c->pos[2];
    c->v18[0] = c->pos[0];
    c->v18[1] = c->pos[1];
    c->v18[2] = c->pos[2];
    c->v3C = 0;
    c->v48C = 0.0f;
    c->v490 = 0.0f;
    c->v498 = 0.0f;
    c->v4D8 = 0;
    c->v4DC = 0;
    c->v31C = 0;
    c->v320 = 0;
    c->v324 = -1;
    c->v328 = -1;
    c->v188 = -1;
    c->v438 = 0;
    c->v440 = 0;
    c->v444 = 0;
    c->v448 = 0;
    c->v44C = 0;
    c->v454 = -1;
    c->v450 = 0;
    c->v458 = 0;
    c->v45C = 0;
    c->hitByProjectile = 0;
    c->v460 = 0;
    c->v464 = 0;
    c->v468 = 0;
    c->numSeqs = c->def->numSeqs;
    c->seqOffsets = (u16 *)DEF_PTR(c, c->def->seqOffsetsOffset);
    c->num1E0 = c->def->num10;
    c->table1E4 = DEF_PTR(c, c->def->offset14);
    c->numMoves = c->def->num18;
    c->moves = DEF_PTR(c, c->def->offset1C);
    c->num1F0 = c->def->num20;
    c->table1F4 = DEF_PTR(c, c->def->offset24);
    c->num1F8 = c->def->num28;
    c->table1FC = DEF_PTR(c, c->def->offset2C);
    c->v2BC = 0;
    c->v2C0 = 0.0f;
    c->v2CC = 0;
    c->v194 = 0.0f;
    c->v198 = 0;
    c->v19C = 0;
    c->v1A0 = 9999.0f;
    mpStrCpy(c->name, c->def->name);
    mpStrCpy(c->shortName, c->def->shortName);
    c->stats[0] = c->def->stats[0];
    c->stats[1] = c->def->stats[1];
    c->stats[2] = (f32)((f64)c->stats[0] * 1.5);
    for (i = 0; i < 7; i++) {
        c->stats[3 + i] = c->def->stats[2 + i];
    }
    c->v248 = 0;
    c->v32C = 0;
    c->isThrowSeq = 0;
    c->v408 = 0;
    c->v410 = 0.0f;
    c->v418 = 0;
    c->v41C = 0;
    c->v420 = 0;
    c->v424 = 0;
    c->v428 = 0;
    c->v42C = 0;
    c->v43C = 0;
    c->v2DC = 0;
    c->v18[0] = 0.0f;
    c->v18[1] = 0.0f;
    c->v18[2] = 0.0f;
    mpMemSet(c->projectiles, sizeof(c->projectiles), 0);
    mpMemSet(c->projSeqUsed, sizeof(c->projSeqUsed), 0);
    for (i = 0; i < CHAR_NUM_PROJSEQS; i++) {
        CharSeq_Stop(&c->projSeqs[i]);
    }
    c->v190 = -1;
    c->buttonMap[0] = 0x4000;
    c->buttonMap[1] = 2;
    c->buttonMap[2] = 8;
    c->buttonMap[3] = 0x8000;
    c->buttonMap[4] = 4;
    c->buttonMap[5] = 1;
    cfCharacter_ResetCounters(c);
    c->v258 = 0;
    c->v25C = 0;
    for (i = 0; i < 12; i += 2) {
        c->bounds[i] = 9999.0f;
        c->bounds[i + 1] = -9999.0f;
    }
}

/* 0x800602BC */
void cfCharacter_TurnAround(cfCharacter *c) {
    s32 facing = (c->facing == 0);

    c->facing = facing;
    c->seq.mirrored = facing;
}

/* 0x800602D0 */
void cfCharacter_SavePrevious(cfCharacter *c) {
    c->prevPos[0] = c->pos[0];
    c->prevPos[1] = c->pos[1];
    c->prevPos[2] = c->pos[2];
    c->prevScreenX = c->screenX;
    c->prevScreenY = c->screenY;
    c->prevHasBodyRect = c->hasBodyRect;
    c->prevBodyRect[0] = c->bodyRect[0];
    c->prevBodyRect[1] = c->bodyRect[1];
    c->prevBodyRect[2] = c->bodyRect[2];
    c->prevBodyRect[3] = c->bodyRect[3];
}

/* 0x80061D78 */
void cfCharacter_ReturnToNeutral(cfCharacter *c) {
    s32 s;

    c->v34 = 0;
    c->v38 = 0;
    s = c->seqIndex;
    if (s == 6 || s == 7 || s == 8 || s == 10 || s == 11 || s == 2 || s == 12 || s == 0x27) {
        cfCharacter_PlaySequence(c, 0, -1, 1, 0xFFFFFFFF);
        s = c->seqIndex;
    }
    if (s == 1 || s == 5 || s == 13) {
        cfCharacter_PlaySequence(c, 3, 1, 1, 0xFFFFFFFF);
    }
    cfCharacter_ClearMoveState(c);
}

/* 0x80061E6C */
void cfCharacter_ClearMoveState(cfCharacter *c) {
    s32 i;

    if (c->seqIndex == 1) {
        cfCharacter_PlaySequence(c, 3, 1, 1, 0xFFFFFFFF);
    }
    if (c->seqIndex == 2) {
        cfCharacter_PlaySequence(c, 0, -1, 1, 0xFFFFFFFF);
    }
    for (i = 0; i < 64; i++) {
        c->a2c[i].timer = 0;
    }
}

/* 0x80065920: -1 if this fighter is left of the opponent, 1 if right */
s32 cfCharacter_SideOfOpponent(cfCharacter *c) {
    cfCharacter *o;
    s32 l, r, ol, or;

    if (c->hasBodyRect) {
        l = (s32)((f32)c->bodyRect[0] + c->screenX);
        r = (s32)((f32)c->bodyRect[2] + c->screenX);
    } else {
        l = r = (s32)c->screenX;
    }
    o = c->opponent;
    if (o->hasBodyRect) {
        ol = (s32)((f32)o->bodyRect[0] + o->screenX);
        or = (s32)((f32)o->bodyRect[2] + o->screenX);
    } else {
        ol = or = (s32)c->opponent->screenX;
    }
    if (l < ol && r < or) {
        return -1;
    }
    if (ol < l && or < r) {
        return 1;
    }
    return ((l + r) / 2 < (ol + or) / 2) ? -1 : 1;
}

/* 0x80065A50 */
void cfCharacter_ClampToArena(cfCharacter *c) {
    CombatHandler *h = c->handler;

    if (c->screenX < (f32)h->minX) {
        c->screenX = (f32)h->minX;
        c->v470 = 0.0f;
        h = c->handler;
    }
    if ((f32)h->maxX < c->screenX) {
        c->screenX = (f32)h->maxX;
        c->v470 = 0.0f;
    }
}


/* 0x80066948: starts sequence 'index' (loops: -1 = for ever; reset: also clear the
   combo / move state; speed: fixed speed or -1) */
void cfCharacter_PlaySequence(cfCharacter *c, s32 index, s32 loops, s32 reset, u32 speed) {
    cfCharacter *o;

    if (index >= c->numSeqs || c->seqOffsets[index] == 0) {
        mpFatalErrorf("CF3CHAR.C - Attempt to play unavailable sequence (%d)", index);
    }
    c->v4A4 = 0.0f;
    c->v49C = 0.0f;
    c->v4A0 = 0.0f;
    c->v4A8 = 0.0f;
    c->v4AC = 0.0f;
    c->angle = 0.0f;
    c->v4D8 = 0;
    c->v494 = 0;
    c->v318 = 0;
    if (reset) {
        c->numLinks = 0;
        c->v3EC = 0;
        c->v3F0 = 0;
        c->vF68[1] = -1;
        c->vF68[3] = -1;
        c->vF68[5] = -1;
        c->vF68[8] = -1;
        c->vF68[11] = -1;
        c->vF68[13] = -1;
        c->vF68[17] = -1;
        c->v3F4 = -1;
        c->v460 = 0;
        c->v464 = 0;
    }
    CharSeq_Start(&c->seq, (u8 *)c->def + c->seqOffsets[index], loops, speed);
    c->seqIndex = index;
    cfCharacter_ApplySeqFrame(c);
    if (index < 63) {
        c->v170 = D_SeqDefaultV170[index];
        c->v174 = 0;
        c->v17C = 1;
        c->priority = D_SeqDefaultPriority[index];
        c->v178 = D_SeqDefaultV178[index];
        c->v184 = 0;
        c->v190 = -1;
        c->v188 = -1;
    } else {
        c->v170 = c->vF3C;
        c->v174 = c->vF40;
        c->v17C = c->vF48;
        c->priority = c->vF44;
        c->v178 = 0;
        c->v184 = c->vF4C;
        c->v190 = c->vF54;
        c->v188 = c->vF60;
        c->v18C = c->vF64;
    }
    c->v198 = 0;
    if (c->seq.hdrA >= 0) {
        c->v190 = c->seq.hdrA;
    }
    c->vF68[0] = -1;
    c->vF68[2] = -1;
    c->vF68[10] = -1;
    c->vF68[12] = -1;
    c->vF68[14] = -1;
    c->vF68[15] = -1;
    c->vF68[16] = -1;
    c->vFB0 = -1.0f;

    /* who is drawn in front */
    o = c->opponent;
    if (o != NULL) {
        s32 front;

        if (c->v460 == o->v460) {
            if (c->v464 == o->v464) {
                front = !(c->priority < o->priority);
            } else {
                front = (c->v464 == 0);
            }
        } else {
            front = (c->v460 != 0);
        }
        if (front) {
            c->inFront = 1;
            c->opponent->inFront = 0;
        } else {
            c->inFront = 0;
            c->opponent->inFront = 1;
        }
    }
    c->v32C = 0;
    c->isThrowSeq = (index == 0x1F || index == 0x20 || index == 0x21 || index == 0x25 || index == 0x26);
    c->v334 = 0;
    c->v3F8 = 0;
    c->v400 = 0;
    c->v41C = 0;
    c->v2D0 = 0;
}

/* 0x80066C0C */
void cfCharacter_StopSequence(cfCharacter *c) {
    c->seq.seq = NULL;
}

/* 0x80067590 */
void cfCharacter_UpdateMatrix(cfCharacter *c) {
    f32 a = c->v49C + c->v4A0;

    c->angle = a;
    if (c->v334 && c->seqIndex != 0x2C) {
        c->angle = a + c->opponent->v49C;
    }
    cfMatrix2D_SetTranslate(&c->matrix, c->v4A8, c->v4AC);
    cfMatrix2D_PreRotate(&c->matrix, (f32)(0.0 - (f64)c->angle));
    cfMatrix2D_PreTranslate(&c->matrix, 0.0f - c->v4A8, 0.0f - c->v4AC);
}

/* 0x8005FE6C: projectiles whose sequence flagged an impact (command 0xA5) make a spark */
void cfCharacter_ProjectileImpacts(cfCharacter *c) {
    s32 i;

    for (i = 0; i < CHAR_NUM_PROJECTILES; i++) {
        CharProjectile *p = &c->projectiles[i];
        CharSeq *s;
        f32 x, y, z;

        if (!p->active) {
            continue;
        }
        s = &c->projSeqs[p->seqSlot];
        if (!s->pairs[5].set) {
            continue;
        }
        x = (f32)s->pairs[5].a * c->handler->camDirX + p->posX;
        y = (f32)((f64)p->posY + -(f64)(f32)s->pairs[5].b);
        z = (f32)s->pairs[5].a * c->handler->camDirZ + p->posZ;
        cfCharacter_SpawnSpark(c, x, y, z, 150.0f);
        s->pairs[5].set = 0;
    }
}

/* 0x8006837C: takes velocity, values and sound from the projectile's sequence */
void cfCharacter_ApplyProjectileSeq(cfCharacter *c, CharProjectile *p) {
    CharSeq *s = &c->projSeqs[p->seqSlot];
    s32 sound;

    if (c->handler == NULL) {
        return;
    }
    if (s->pairs[2].set) {
        p->velX = (f32)s->pairs[2].a;
        p->velY = (f32)s->pairs[2].b;
    }
    if (s->cmdB3) {
        p->angle = s->cmdB3Value;
    }
    if (s->cmdB4) {
        p->v18 = s->cmdB4Value;
    }
    sound = s->cmdB2Value;
    if (sound >= 0) {
        sound = cfCharacter_PickSound(c, sound);
        if (sound >= 0) {
            func_8006DE6C(c->handler, sound, 0x7FFF, c, 0, 1);
        }
        s->cmdB2Value = -1;
    }
}

/* 0x8006845C: direction from the projectile to the opponent */
void cfCharacter_AimProjectile(cfCharacter *c, CharProjectile *p) {
    f32 dx = c->handler->camDirX;
    f32 dz = c->handler->camDirZ;
    f32 d;

    if (c->opponent->facing) {
        dx = -dx;
        dz = -dz;
    }
    p->targetDirX = c->opponent->pos[0] - p->posX;
    p->targetDirZ = c->opponent->pos[2] - p->posZ;
    d = cfVec2_Length(&p->targetDirX);
    p->targetDist = d;
    if (d == 0.0f) {
        p->targetDirX = dx;
        p->targetDirZ = dz;
    } else {
        p->targetDirX = p->targetDirX / p->targetDist;
        p->targetDirZ = p->targetDirZ / p->targetDist;
    }
    p->targetDot = p->targetDirX * dx + p->targetDirZ * dz;
}

/* 0x80068550: sounds 9000-9013 pick at random from one of the fighter's 14 sound sets.
   Sounds 9002 and 9003 use each other's set (the game's switch has them swapped). */
static const u8 sSoundSetOrder[14] = { 0, 1, 3, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };

s32 cfCharacter_PickSound(cfCharacter *c, s32 sound) {
    s8 *set;
    s32 k, r;

    if (sound < 9000) {
        return sound;
    }
    k = sound - 9000;
    if ((u32)k >= 14) {
        return -1;
    }
    k = sSoundSetOrder[k];
    set = &c->def->soundSets[k * 8];
    if (set[0] <= 0) {
        return -1;
    }
    r = (s32)guRandom();
    return set[1 + r % c->def->soundSets[k * 8]] + 100;
}

/* 0x800687DC: the n-th size-prefixed record */
u8 *cfCharacter_GetRecord(cfCharacter *c, s32 n) {
    u8 *p = c->moves;
    s32 i;

    for (i = 0; i < n; i++) {
        p += *(s32 *)p;
    }
    return p;
}

/*
 * 0x80068D0C: controller buttons -> command bits. Left/right become "towards" (4) and
 * "away" (8) depending on which side the opponent is.
 *   1 up  2 down  4 towards  8 away  0x10 / 0x20 / 0x1000 other directions,
 *   0x40 0x80 0x100 0x200 0x400 0x800 the six attack buttons
 */
u32 cfCharacter_ButtonsToCommand(cfCharacter *c, u32 b) {
    u32 r = (b & 0x800) ? 1 : 0;
    cfCharacter *o;

    if (b & 0x400) r |= 2;
    if (b & 0x20) r |= 0x10;
    if (b & 0x10) r |= 0x20;
    if (b & 0x2000) r |= 0x1000;
    o = c->opponent;
    if (o == NULL || c->screenX < o->screenX) {
        if (b & 0x100) r |= 4;
        if (b & 0x200) r |= 8;
    } else {
        if (b & 0x200) r |= 4;
        if (b & 0x100) r |= 8;
    }
    if (r & 1) r &= ~2;
    if (r & 4) r &= ~8;
    if (b & 0x4000) r |= 0x40;
    if (b & 0x2) r |= 0x80;
    if (b & 0x8) r |= 0x100;
    if (b & 0x8000) r |= 0x200;
    if (b & 0x4) r |= 0x400;
    if (b & 0x1) r |= 0x800;
    return r;
}

#define PREV_INPUT(i) do { if (--(i) < 0) (i) = CHAR_NUM_INPUTS - 1; } while (0)

/*
 * 0x80068E0C: looks back through the input history from *index for an entry matching
 * 'command' within 'window' ticks of *time. Bit 0x8000 in command = any of the bits;
 * otherwise the bits are collected across entries. 'buttons' (bit 0x8000 = any) must
 * also be held. Returns 1 on a match, updating *index and *time.
 */
s32 cfCharacter_MatchInput(cfCharacter *c, u32 command, u32 buttons, s32 *index, u32 *time,
                           u32 window, s32 allowUsed) {
    s32 idx = *index;
    u32 lastTime = *time;
    u32 now = *time;
    u32 need = command & 0x7FFF;
    u32 held = buttons & 0xFFFF;
    s32 result = 1;

    do {
        CharInput *e = &c->inputs[idx];
        u32 cmd;

        if (!(now - e->time < window)) {
            break;
        }
        lastTime = e->time;
        if (held != 0) {
            if (buttons & 0x8000) {
                if (!(buttons & e->buttons)) {
                    result = 0;
                    goto out;
                }
            } else if ((buttons & e->buttons) != held) {
                result = 0;
                goto out;
            }
        }
        if (!(((e->flags ^ 1) & 1) | (allowUsed != 0))) {
            PREV_INPUT(idx);
            continue;
        }
        cmd = c->inputs[idx].command;
        if (command & 0x8000) {
            if (!(cmd & 0x1FF0) && (c->inputs[idx].flags & 1)) {
                PREV_INPUT(idx);
                continue;
            }
            if (command & cmd) {
                *index = idx;
                *time = lastTime;
                return result;
            }
        } else {
            u32 left;

            if (cmd & 0x1FF0) {
                need &= ~cmd;
                left = need & 0xFFFF;
            } else {
                if ((c->inputs[idx].flags & 1) || (cmd & 0xF) != (command & 0xF)) {
                    PREV_INPUT(idx);
                    continue;
                }
                need &= 0xFFF0;
                left = need;
            }
            if (left == 0) {
                *index = idx;
                *time = lastTime;
                return result;
            }
        }
        PREV_INPUT(idx);
    } while (idx != c->inputHead);
    result = 0;
out:
    *index = idx;
    *time = lastTime;
    return result;
}

/* 0x80069004: finds buttons held (bit 0x8000 = any of them) within 'window' ticks */
s32 cfCharacter_FindInput(cfCharacter *c, u32 buttons, s32 *index, u32 *time, u32 window) {
    s32 idx = *index;
    u32 lastTime = *time;
    u32 now;
    s32 any = (buttons & 0xFFFF) >> 15;
    u32 m = buttons & 0x7FFF;
    s32 result;

    if (m == 0) {
        return 1;
    }
    now = *time;
    do {
        CharInput *e = &c->inputs[idx];

        if (!(now - e->time < window)) {
            break;
        }
        lastTime = e->time;
        if (any) {
            if (m & c->inputs[idx].buttons) {
                result = 1;
                goto out;
            }
        } else if ((m & c->inputs[idx].buttons) == m) {
            result = 1;
            goto out;
        }
        PREV_INPUT(idx);
    } while (idx != c->inputHead);
    result = 0;
out:
    *index = idx;
    *time = lastTime;
    return result;
}

/* 0x800690D0: were the buttons held for 'duration' ticks back from entry 'index'? */
s32 cfCharacter_InputHeld(cfCharacter *c, u32 buttons, s32 duration, s32 index) {
    s32 any = (buttons & 0xFFFF) >> 15;
    u32 m = buttons & 0x7FFF;
    u32 acc, t;
    s32 i = index;

    if (m == 0 || duration <= 0) {
        return 1;
    }
    acc = m;
    t = c->inputs[i].time;
    do {
        acc = c->inputs[i].buttons & acc;
        if (any) {
            if (acc == 0) {
                return 0;
            }
        } else if ((acc & 0xFFFF) != m) {
            return 0;
        }
        duration -= t - c->inputs[i].time;
        t = c->inputs[i].time;
        if (duration <= 0) {
            break;
        }
        PREV_INPUT(i);
    } while (i != index);
    return 1;
}

/* 0x80069180 */
s32 cfCharacter_AllocProjectile(cfCharacter *c) {
    s32 i;

    for (i = 0; i < CHAR_NUM_PROJECTILES; i++) {
        if (!c->projectiles[i].active) {
            c->projectiles[i].active = 1;
            return i;
        }
    }
    return -1;
}

/* 0x800691E4 */
void cfCharacter_FreeProjectile(cfCharacter *c, s32 i) {
    CharProjectile *p = &c->projectiles[i];

    if (p->seqSlot != -1) {
        c->projSeqUsed[p->seqSlot] = 0;
        p->seqSlot = -1;
    }
    c->projectiles[i].active = 0;
}

/* 0x80069228 */
s32 cfCharacter_AllocProjSeq(cfCharacter *c) {
    s32 i;

    for (i = 0; i < CHAR_NUM_PROJSEQS; i++) {
        if (!c->projSeqUsed[i]) {
            c->projSeqUsed[i] = 1;
            return i;
        }
    }
    return -1;
}

/* 0x8006927C: one spark in a random direction (each new one turns further round) */
void cfCharacter_SpawnSpark(cfCharacter *c, f32 x, f32 y, f32 z, f32 size) {
    s32 i;

    for (i = 0; i < 48; i++) {
        FxSpark *s = &c->handler->sparks[i];
        s32 a;

        if (s->active) {
            continue;
        }
        s->active = 1;
        s->pos[0] = x;
        s->pos[1] = y;
        s->pos[2] = z;
        sSparkAngle = ((s32)guRandom() % 90 + sSparkAngle) % 360;
        a = (s32)((f64)((f32)sSparkAngle * 65536.0f) / 360.0);
        s->dirX = vsSin(a);
        a = (s32)((f64)((f32)sSparkAngle * 65536.0f) / 360.0);
        s->dirY = vsCos(a);
        s->rgb[0] = 0xFF;
        s->rgb[1] = 0xFF;
        s->rgb[2] = 0xFF;
        s->age = 0;
        s->size = size;
        return;
    }
}

/* 0x80069424 */
void cfCharacter_SpawnDust(cfCharacter *c, f32 x, f32 y, f32 z, f32 size) {
    s32 i;

    for (i = 0; i < 6; i++) {
        FxDust *d = &c->handler->dust[i];
        f32 s2;

        if (d->active) {
            continue;
        }
        d->active = 1;
        d->variant = (s32)guRandom() % 3;
        d->age = 0;
        d->pos[0] = x;
        d->pos[1] = y;
        d->pos[2] = z;
        d->size = size;
        s2 = (f32)((f64)size + (f64)size);
        d->sizeB = s2;
        d->size2 = s2;
        return;
    }
}

/* 0x80069514: fills every free debris slot (all 32 when none are in use) */
void cfCharacter_SpawnDebris(cfCharacter *c, f32 x, f32 y, f32 z) {
    s32 i;

    for (i = 0; i < 32; i++) {
        FxDebris *d = &c->handler->debris[i];
        f32 a, speed;
        s32 ang;

        if (d->active) {
            continue;
        }
        a = (f32)((s32)guRandom() % 360);
        speed = (f32)((s32)guRandom() % 1000);
        speed = speed / 1000.0f;
        speed = (f32)((f64)speed + 1.2);
        d->active = 1;
        d->v4 = 36.0f;
        ang = (s32)((f64)(a * 65536.0f) / 360.0);
        d->velX = vsSin(ang) * speed;
        d->velY = vsCos(ang) * speed;
        d->pos[0] = x;
        d->pos[1] = y;
        d->pos[2] = z;
    }
}

/* 0x800696E4: like debris, fills every free flash slot */
void cfCharacter_SpawnFlash(cfCharacter *c, f32 x, f32 y, f32 z) {
    s32 i;

    for (i = 0; i < 4; i++) {
        FxFlash *f = &c->handler->flashes[i];

        if (f->active) {
            continue;
        }
        f->active = 1;
        f->pos[0] = x;
        f->pos[1] = y;
        f->pos[2] = z;
        f->age = 0;
        f->v14 = 1.0f / 36.0f;
    }
}

/* 0x80069760 (first function of the next object file) */
void cfCharacter_ResetCounters(cfCharacter *c) {

    c->v260 = 0;
    c->v264 = 2;
    c->aiCommand = 0;
    c->aiState = 0;
    c->aiTimer = 0.0f;
    c->aiIdleTime = 0.0f;
    c->aiBlockTime = 0.0f;
    c->aiNoBlock = 0.0f;
    c->v280 = 0;
    c->v284 = -1;
    c->v288 = -1;
    c->v28C = 0;
    c->v290 = 0;
    c->aiTriedFinisher = 0;
    c->aiFinisher = NULL;
    c->aiFinisherVariant = NULL;
    c->aiFinisherFrom = 0;
    c->aiFinisherTo = 9999;
    c->v2A8 = 0;
}

/* ---------------------------------------------------------------- rectangles */

/* Moves a rectangle so its centre follows the fighter's rotation matrix (the
   rectangle itself stays axis-aligned). */
static void RotateRect(cfCharacter *c, SeqRect *r) {
    f32 cx = (f32)((r->right + r->left) / 2);
    f32 cy = (f32)((r->bottom + r->top) / 2);
    f32 nx = (f32)((f64)(cx * c->matrix.m[0][0] + cy * c->matrix.m[1][0]) + (f64)c->matrix.m[2][0]);
    f32 ny = (f32)((f64)(cx * c->matrix.m[0][1] + cy * c->matrix.m[1][1]) + (f64)c->matrix.m[2][1]);

    r->left = (s32)((f32)r->left + (nx - cx));
    r->right = (s32)((f32)r->right + (nx - cx));
    r->top = (s32)((f32)r->top + (ny - cy));
    r->bottom = (s32)((f32)r->bottom + (ny - cy));
}

static void RotatePoint(cfCharacter *c, s16 *x, s16 *y) {
    f32 px = (f32)*x;
    f32 py = (f32)*y;
    f32 nx = (f32)((f64)(px * c->matrix.m[0][0] + py * c->matrix.m[1][0]) + (f64)c->matrix.m[2][0]);
    f32 ny = (f32)((f64)(px * c->matrix.m[0][1] + py * c->matrix.m[1][1]) + (f64)c->matrix.m[2][1]);

    *x = (s32)nx;
    *y = (s32)ny;
}

/* the body rectangle comes from sequence command 0x95, unless switched off (v258) */
static void SetBodyRect(cfCharacter *c) {
    s32 has = 0;

    if (c->seq.hasRect95) {
        has = (c->v258 == 0);
    }
    c->hasBodyRect = has;
    if (has) {
        c->bodyRect[0] = c->seq.rect95.left;
        c->bodyRect[1] = c->seq.rect95.top;
        c->bodyRect[2] = c->seq.rect95.right;
        c->bodyRect[3] = c->seq.rect95.bottom;
        if ((f64)c->angle != 0.0) {
            RotateRect(c, (SeqRect *)c->bodyRect);
        }
    } else {
        c->bodyRect[0] = 0;
        c->bodyRect[1] = 0;
        c->bodyRect[2] = -1;
        c->bodyRect[3] = -1;
    }
}

/*
 * 0x80065AA8: builds this frame's hurt boxes from the rectangles stored with each
 * sprite frame (flipped and offset like the sprite layer), keeps the overall extent
 * of each rectangle type in bounds[], and copies the sequence's attack and block
 * rectangles - all moved with the fighter's rotation when it is rotated.
 */
void cfCharacter_UpdateRects(cfCharacter *c) {
    s32 i, j, n;

    for (i = 0; i < 12; i += 2) {
        c->bounds[i] = 9999.0f;
        c->bounds[i + 1] = -9999.0f;
    }
    c->numBodyRects = 0;
    for (i = 0; i < c->seq.numLayers; i++) {
        s32 frame = (s16)c->seq.layers[i].frame;
        u16 off;
        s16 *p;

        if (!(frame < c->num1E0)) {
            mpFatalError("CF3CHAR.C - cfCharacter_UpdateRects() *** Illegal frame number");
        }
        off = ((u16 *)c->table1E4)[frame];
        if (off == 0) {
            continue;
        }
        p = (s16 *)((u8 *)c->def + off);
        n = p[0];
        if (n == 0) {
            continue;
        }
        p++;
        if (!(c->numBodyRects + n < 17)) {
            mpFatalError("Maximum body rectangles exceeded!!!");
        }
        for (j = 0; j < n; j++, p += 4) {
            SeqLayer *l = &c->seq.layers[i];
            SeqRect *r;
            f32 *b;
            s32 type;

            type = p[0] & 0xC000;
            if (type == 0x4000) {
                c->bodyType[c->numBodyRects] = 1;
            } else if (type == 0x8000) {
                c->bodyType[c->numBodyRects] = 2;
            } else {
                c->bodyType[c->numBodyRects] = 0;
            }
            r = &c->bodyRects[c->numBodyRects];
            r->left = p[0];
            if (r->left & 0x2000) {            /* 14-bit signed x */
                r->left |= 0xC000;
            } else {
                r->left &= 0x3FFF;
            }
            if (l->flags & 2) {
                r->top = -p[1];
                r->bottom = -p[3];
            } else {
                r->top = p[1];
                r->bottom = p[3];
            }
            if (l->flags & 1) {
                r->left = -r->left;
                r->right = -p[2];
            } else {
                r->right = p[2];
            }
            mpRectNormalize(r);
            r->left += l->x;
            r->right += l->x;
            r->top += l->y;
            r->bottom += l->y;

            type = p[0] & 0xC000;
            b = (type == 0x4000) ? &c->bounds[4] : (type == 0x8000) ? &c->bounds[8] : &c->bounds[0];
            if ((f32)r->top < b[0]) b[0] = (f32)r->top;
            if (b[1] < (f32)r->bottom) b[1] = (f32)r->bottom;
            if ((f32)r->left < b[2]) b[2] = (f32)r->left;
            if (b[3] < (f32)r->right) b[3] = (f32)r->right;

            if ((f64)c->angle != 0.0) {
                RotateRect(c, r);
            }
            c->numBodyRects++;
        }
    }
    c->bounds[0] = -c->bounds[0];
    c->bounds[1] = -c->bounds[1];
    c->bounds[4] = -c->bounds[4];
    c->bounds[5] = -c->bounds[5];
    c->bounds[8] = -c->bounds[8];
    c->bounds[9] = -c->bounds[9];

    c->numAttacks = c->seq.numAttacks;
    if ((f64)c->angle == 0.0) {
        bcopy(c->seq.attacks, c->attacks, c->numAttacks * 8);
        bcopy(c->seq.attackA, c->attackA, c->numAttacks * 2);
        bcopy(c->seq.attackB, c->attackB, c->numAttacks * 2);
    } else {
        for (i = 0; i < c->numAttacks; i++) {
            c->attacks[i] = c->seq.attacks[i];
            c->attackA[i] = c->seq.attackA[i];
            c->attackB[i] = c->seq.attackB[i];
            RotateRect(c, &c->attacks[i]);
            RotatePoint(c, &c->attackA[i], &c->attackB[i]);
        }
    }
    c->numBlocks = c->seq.numBlocks;
    if ((f64)c->angle == 0.0) {
        bcopy(c->seq.blocks, c->blocks, c->numBlocks * 8);
    } else {
        for (i = 0; i < c->numBlocks; i++) {
            c->blocks[i] = c->seq.blocks[i];
            RotateRect(c, &c->blocks[i]);
        }
    }
    SetBodyRect(c);
}

/* ---------------------------------------------------------------- sequence frame */

extern s32 D_GrabbedSeqs[];     /* 0x800B07F0: opponent sequence for each grab type */

/* a projectile's launch record (0x44 bytes, indexed by v190) */
#define PROJ_REC(c) ((u8 *)(c)->table1FC + (c)->v190 * 0x44)
#define REC_S16(c, o) (*(s16 *)(PROJ_REC(c) + (o)))
#define REC_F32(c, o) (*(f32 *)(PROJ_REC(c) + (o)))

static void LaunchProjectile(cfCharacter *c) {
    s32 i, k;
    CharProjectile *p;
    CharSeq *s;

    i = cfCharacter_AllocProjectile(c);
    if (i == -1) {
        return;
    }
    k = cfCharacter_AllocProjSeq(c);
    if (k == -1) {
        cfCharacter_FreeProjectile(c, i);
        return;
    }
    p = &c->projectiles[i];
    s = &c->projSeqs[k];
    p->v4 = 0;
    p->facing = c->facing;
    p->facing2 = c->facing;
    p->seqSlot = k;
    p->angle = 0.0f;
    p->v18 = 0.0f;
    p->velX = 0.0f;
    p->velY = 0.0f;
    p->v48 = 0;
    s->mirrored = c->facing;
    CharSeq_Start(s, (u8 *)c->def + c->seqOffsets[REC_S16(c, 0)], -1, 0xFFFFFFFF);
    cfCharacter_ApplyProjectileSeq(c, p);
    cfMatrix2D_SetTranslate(&p->matrix, p->velX, p->velY);
    cfMatrix2D_PreRotate(&p->matrix, (f32)(0.0 - (f64)p->angle));
    cfMatrix2D_PreTranslate(&p->matrix, (f32)(0.0 - (f64)p->velX), (f32)(0.0 - (f64)p->velY));
    p->v50 = (f32)REC_S16(c, 0x2);
    p->v54 = REC_S16(c, 0x4);
    p->v58 = REC_S16(c, 0x6);
    p->v5C = REC_S16(c, 0x6);           /* (sic) the same field again */
    p->v60 = REC_S16(c, 0xA);
    p->v64 = (f32)REC_S16(c, 0xC);
    p->v68 = REC_S16(c, 0xE);
    p->v6C = REC_S16(c, 0x10);
    p->v70 = REC_S16(c, 0x12);
    p->v74 = (f32)((f64)REC_S16(c, 0x14) + (f64)REC_S16(c, 0x14));
    p->v78 = PROJ_REC(c)[0x1F];
    p->posX = (f32)c->seq.pairs[1].a * c->handler->camDirX + c->pos[0];
    p->posY = -(((f32)c->seq.pairs[1].b + c->screenY) + c->handler->centre[1]);
    p->posZ = (f32)c->seq.pairs[1].a * c->handler->camDirZ + c->pos[2];
    p->dirX = c->handler->camDirX;
    p->dirZ = c->handler->camDirZ;
    if (c->facing) {
        p->dirX = -p->dirX;
        p->dirZ = -p->dirZ;
    }
    p->v90 = 0;
    p->v94 = 0;
    p->v98 = REC_F32(c, 0x20);
    p->v9C = -REC_F32(c, 0x24);
    p->vA0 = REC_F32(c, 0x28);
    p->vA4 = REC_F32(c, 0x2C);
    p->vA8 = REC_F32(c, 0x30);
    p->vAC = 99999.0f;
    p->vB0 = REC_S16(c, 0x16);
    p->vB4 = REC_S16(c, 0x18);
    p->vB8 = REC_S16(c, 0x1A);
    p->vBC = (s8)PROJ_REC(c)[0x1C];
    p->vC0 = (s8)PROJ_REC(c)[0x1D];
    p->vC4 = (s8)PROJ_REC(c)[0x1E];
    p->vC8 = 1;
    p->vCC = REC_F32(c, 0x34);
    p->vD0 = REC_F32(c, 0x38);
    p->vD4 = 0.0f;
    p->vD8 = REC_F32(c, 0x3C);
    p->vDC = REC_F32(c, 0x40);
    if (0.0 < (f64)p->vDC) {
        p->vDC = (f32)((f64)p->vDC + 20.0);
    }
    cfCharacter_AimProjectile(c, p);
}

static void ClearMotion(cfCharacter *c) {
    c->v46C = 0.0f;
    c->v470 = 0.0f;
    c->v474 = 0.0f;
    c->v478 = 0.0f;
    c->v480 = 0.0f;
    c->v484 = 0.0f;
    c->v488 = 0.0f;
    c->v490 = 0.0f;
    c->v48C = 0.0f;
}

/*
 * 0x80067658: applies what the current sequence frame asked for - movement values,
 * rotation, body rectangle, a sound, launching a projectile, and holding the opponent
 * during a grab (command 0xA0 gives the hold point; the opponent is put into the grab
 * sequence chosen by command 0xC0).
 */
void cfCharacter_ApplySeqFrame(cfCharacter *c) {
    CombatHandler *h = c->handler;
    cfCharacter *o;
    s32 sound;

    if (h == NULL) {
        return;
    }
    if (h->vBC && !c->seq.pairs[0].set && !c->opponent->seq.pairs[0].set && !c->v428 && !c->opponent->v428) {
        h->vBC = 0;
    }
    if (c->seq.numAttacks > 0) {
        c->v3F0++;
    }
    if (c->seq.cmdB5) {
        c->v484 = c->seq.cmdBEValue;
        c->v488 = c->seq.cmdBFValue;
    }
    if (c->seq.cmdB1 || (c->seq.cmdB0 && c->seq.cmdB0Value != 10)) {
        if (c->v428) {
            c->handler->v70 = 1;
        }
    }
    if (c->seq.cmdB0 || (c->seq.cmdC0 && c->seq.cmdC0Value == 10)) {
        if (c->v428) {
            c->opponent->v468 = 1;
            c->seq.cmdC0 = 0;
        }
    }
    c->v2D4 = 0;
    c->v2D8 = 0;
    if (c->seq.pairs[2].set) {
        c->v4A8 = (f32)c->seq.pairs[2].a;
        c->v4AC = (f32)c->seq.pairs[2].b;
    }
    if (c->seq.cmdB3) {
        c->v49C = c->seq.cmdB3Value;
    }
    if (c->seq.cmdB4) {
        c->v4A4 = c->seq.cmdB4Value;
    }
    cfCharacter_UpdateMatrix(c);
    SetBodyRect(c);

    if (c->seq.cmdBC) {
        if ((f64)c->seq.cmdBCValue == 0.0) {
            c->v480 = 0.0f;
        } else {
            c->v480 = c->seq.cmdBCValue;
            c->seq.cmdBCValue = 0.0f;
        }
        c->v470 = 0.0f;
    }
    if (c->seq.cmdBD) {
        if (c->v4D8) {
            c->v46C = c->v46C + c->stats[3];
            c->v4D8 = 0;
        } else if ((f64)c->seq.cmdBDValue == 0.0) {
            c->v46C = 0.0f;
        } else {
            c->v46C = c->v46C + c->seq.cmdBDValue;
        }
        if (c->seq.cmdBE) {
            c->v470 = c->seq.cmdBEValue;
        }
        c->seq.cmdBDValue = 0.0f;
    }
    sound = c->seq.cmdB2Value;
    if (sound >= 0) {
        sound = cfCharacter_PickSound(c, sound);
        if (sound >= 0) {
            func_8006DE6C(c->handler, sound, 0x7FFF, c, 0, 1);
        }
        c->seq.cmdB2Value = -1;
    }
    if (c->seq.pairs[1].set && c->v190 >= 0 && c->v190 < c->num1F8) {
        LaunchProjectile(c);
    }

    if (c->seq.pairs[0].set && c->seqIndex != 0x2C) {
        o = c->opponent;
        if (o->facing != c->facing) {
            cfCharacter_TurnAround(o);
        }
        if (c->seq.cmdC0 || c->opponent->seqIndex != D_GrabbedSeqs[c->seq.cmdC0Value]) {
            cfCharacter_PlaySequence(c->opponent, D_GrabbedSeqs[c->seq.cmdC0Value], -1, 1, 0xFFFFFFFF);
        }
        c->opponent->v334 = 1;
        o = c->opponent;
        if (o->seqIndex == 0x2C) {
            c->screenX = ((f32)o->seq.pairs[0].a + o->screenX) - (f32)c->seq.pairs[0].a;
            c->screenY = ((f32)c->opponent->seq.pairs[0].b + c->opponent->screenY) - (f32)c->seq.pairs[0].b;
            ClearMotion(c);
            c->v334 = 1;
        } else {
            o->screenX = (f32)c->seq.pairs[0].a + c->screenX;
            c->opponent->screenY = (f32)c->seq.pairs[0].b + c->screenY;
            c->v334 = 0;
        }
        ClearMotion(c->opponent);
        c->opponent->v4A0 = c->seq.cmdC8Value;
        cfCharacter_UpdateMatrix(c->opponent);
        if (c->seq.cmdC9) {
            /* throw: let go with a push */
            o = c->opponent;
            o->v334 = 0;
            c->opponent->v46C = c->opponent->v46C + c->vF58;
            c->opponent->v470 = c->facing ? (f32)(0.0 - (f64)c->vF5C) : c->vF5C;
            c->opponent->v48C = c->opponent->v46C / 2.0f;
            c->opponent->v494 = 1;
            c->opponent->v318 = c->vF4C;
        }
    } else {
        c->opponent->v334 = 0;
    }
    if (c->seq.pairs[0].set && c->opponent->v31C) {
        c->v320 = 1;
    }
}

/* ---------------------------------------------------------------- particles */

/* signed random number in -1024..1023 */
#define RAND_PM1024() ((s32)guRandom() % 2048 - 1024)

/* 0x80066C14: 'count' bits of debris from the fighter's debris table (num1F0 kinds) */
void cfCharacter_EmitBits(cfCharacter *c, s32 count, f32 x, f32 y, f32 z) {
    s32 n, i;

    if (c->num1F0 <= 0 || count <= 0) {
        return;
    }
    for (n = 0; n < count; n++) {
        FxBit *b;
        u8 *rec;

        for (i = 0; i < 32; i++) {
            if (!c->handler->bits[i].active) {
                break;
            }
        }
        if (i >= 32) {
            return;
        }
        b = &c->handler->bits[i];
        rec = c->table1F4;
        if (c->num1F0 * 100 != 0) {
            rec += (s32)guRandom() % (c->num1F0 * 100) / 100 * 16;
        }
        b->active = 1;
        b->v4 = c->v40;
        b->rec = rec;
        b->vC = *(s16 *)rec;
        b->pos[0] = x;
        b->pos[1] = y;
        b->pos[2] = z;
        b->vel[0] = (f32)((f64)(f32)RAND_PM1024() + 0.5);
        b->vel[1] = (f32)((f64)(f32)RAND_PM1024() + 0.5);
        b->vel[2] = (f32)((f64)(f32)RAND_PM1024() + 0.5);
        cfVec3_Normalise(b->vel);
        b->vel[0] = (f32)((s32)guRandom() % 35) * b->vel[0];
        b->vel[1] = (f32)((s32)guRandom() % 50) * b->vel[1];
        b->vel[2] = (f32)((s32)guRandom() % 35) * b->vel[2];
        b->angle = 0.0f;
        b->spin = (f32)((f64)(f32)RAND_PM1024() / 64.0);
        b->falling = 0;
        b->resting = 0;
        /* never too slow sideways */
        if ((f64)b->vel[0] < 0.13 && 0.0 <= (f64)b->vel[0]) b->vel[0] = 0.13f;
        if (-0.13 < (f64)b->vel[0] && (f64)b->vel[0] <= 0.0) b->vel[0] = -0.13f;
        if ((f64)b->vel[2] < 0.13 && 0.0 <= (f64)b->vel[2]) b->vel[2] = 0.13f;
        if (-0.13 < (f64)b->vel[2] && (f64)b->vel[2] <= 0.0) b->vel[2] = -0.13f;
    }
}

/* 0x800670EC: one smoke puff (the first free one of 64) */
void cfCharacter_EmitSmoke(cfCharacter *c, f32 x, f32 y, f32 z, s32 a4, s32 size) {
    f32 scale = (f32)(size + 1);
    s32 i;

    for (i = 0; i < 64; i++) {
        FxSmoke *s = &c->handler->smoke[i];
        f64 k;

        if (s->active) {
            continue;
        }
        guRandom();                         /* (result unused in the original too) */
        s->active = 1;
        s->v4 = a4;
        s->v8 = -1;
        s->vC = (s32)guRandom() % 500 / 100;
        s->v10 = scale;
        s->pos[0] = x;
        s->pos[1] = y;
        s->pos[2] = z;
        s->vel[0] = (f32)((f64)(f32)RAND_PM1024() + 0.5);
        s->vel[1] = (f32)((f64)(f32)RAND_PM1024() + 0.5);
        s->vel[2] = (f32)((f64)(f32)RAND_PM1024() + 0.5);
        cfVec3_Normalise(s->vel);
        k = 4.0 - (f64)scale;
        s->vel[0] = (f32)((f64)s->vel[0] * (k * (f64)((s32)guRandom() % 10)));
        s->vel[1] = (f32)((f64)s->vel[1] * (k * (f64)((s32)guRandom() % 16)));
        s->vel[2] = (f32)((f64)s->vel[2] * ((4.0 - (f64)scale) * (f64)((s32)guRandom() % 10)));
        s->angle = 0.0f;
        s->spin = (f32)((f64)(f32)RAND_PM1024() / 64.0);
        s->falling = 0;
        s->resting = 0;
        if ((f64)s->vel[0] < 0.13 && 0.0 <= (f64)s->vel[0]) s->vel[0] = 0.13f;
        if (-0.13 < (f64)s->vel[0] && (f64)s->vel[0] <= 0.0) s->vel[0] = -0.13f;
        if ((f64)s->vel[2] < 0.13 && 0.0 <= (f64)s->vel[2]) s->vel[2] = 0.13f;
        if (-0.13 < (f64)s->vel[2] && (f64)s->vel[2] <= 0.0) s->vel[2] = -0.13f;
        return;
    }
}

/* where a sequence value pair (commands 0xA4/0xA5) is in the arena */
static void PairToWorld(cfCharacter *c, SeqPair *pr, f32 *x, f32 *y, f32 *z) {
    *x = (f32)pr->a * c->handler->camDirX + c->pos[0];
    *y = (f32)(-(f64)((f32)pr->b + c->handler->centre[1]) - (f64)c->screenY);
    *z = (f32)pr->a * c->handler->camDirZ + c->pos[2];
}

/* 0x8005FF94: big impact (command 0xA4) and spark burst (0xA5) asked for by the sequence */
void cfCharacter_SequenceEffects(cfCharacter *c) {
    f32 x, y, z;
    s32 i;

    if (c->v25C) {
        return;
    }
    if (c->seq.pairs[4].set) {
        FxGlow *g;

        PairToWorld(c, &c->seq.pairs[4], &x, &y, &z);
        c->handler->v2C70 = 0;
        c->handler->v2C78 = 1;
        c->handler->v2C7C = 1;
        c->handler->v2C80 = 0;
        for (i = 0; i < 16; i++) {
            cfCharacter_SpawnDebris(c, x, y, z);
        }
        cfCharacter_SpawnFlash(c, x, y, z);
        g = Fx_NewGlow(c->handler);
        if (g != NULL) {
            g->pos[0] = x;
            g->pos[1] = y;
            g->pos[2] = z;
            g->colA[0] = 200; g->colA[1] = 200; g->colA[2] = 255; g->colA[3] = 255;
            g->colB[0] = 0;   g->colB[1] = 0;   g->colB[2] = 255; g->colB[3] = 0;
            g->v18 = 0.0f;
            g->v1C = 5.0f;
            g->v24 = 1.0f / 36.0f;
        }
        func_8006DE6C(c->handler, 0x17, 0x7FFF, c, 0, 1);
        c->handler->v2CC8 = 18.0f;
        c->seq.pairs[4].set = 0;
    }
    if (c->seq.pairs[5].set) {
        PairToWorld(c, &c->seq.pairs[5], &x, &y, &z);
        cfCharacter_SpawnSpark(c, x, y, z, 150.0f);
        cfCharacter_SpawnSpark(c, x, y, z, 140.0f);
        cfCharacter_SpawnSpark(c, x, y, z, 130.0f);
        cfCharacter_SpawnSpark(c, x, y, z, 100.0f);
        cfCharacter_SpawnSpark(c, x, y, z, 80.0f);
        c->seq.pairs[5].set = 0;
    }
}

/* ---------------------------------------------------------------- moves */

#define MOVE_S8(p, o)  ((s8)(p)[o])
#define MOVE_S16(p, o) (*(s16 *)((p) + (o)))
#define MOVE_F32(p, o) (*(f32 *)((p) + (o)))

/* push strength: -1..-4 pick one of the variant's four values, otherwise the move's own */
static f32 MovePush(u8 *move, u8 *var, s32 off) {
    s32 k = *(s32 *)(move + off);

    if (k == -1) return MOVE_F32(var, 0x14);
    if (k == -2) return MOVE_F32(var, 0x18);
    if (k == -3) return MOVE_F32(var, 0x1C);
    if (k == -4) return MOVE_F32(var, 0x20);
    return MOVE_F32(move, off);
}

/*
 * 0x80068810: starts a move (a record from the CharDef move list) in one of its
 * variants (NULL = the default variant): copies the move's properties into the
 * fighter, plays its sequence, and sets up the combo links that may follow it.
 */
void cfCharacter_StartMove(cfCharacter *c, u8 *move, u8 *var) {
    s32 noVar = (var == NULL);
    s32 i, v;
    u16 w;

    if (c->handler->v54 != 9) {
        c->handler->vBC = MOVE_S8(move, 0x10);
    }
    if (var == NULL) {
        var = move + *(u16 *)(move + 0x2E);
    }
    if (!noVar) {
        v = (move[6] == 1);
        c->v418 = v;
        c->v41C = v;
        c->v420 = (move[6] == 2);
        c->v424 = MOVE_S8(move, 0xA);
        c->v428 = (move[6] == 3);
        v = (MOVE_S8(move, 0xB) >= 0);
        c->v42C = v;
        if (v) {
            c->v430 = MOVE_S8(move, 0xB) - 1;
            if (c->v430 < 0) {
                c->v430 = 2;
            }
        }
        c->v434 = (MOVE_S8(move, 0xE) >= 0);
        c->v2DC = 0;
        w = *(u16 *)var;
        if (w & 0x900) {
            c->vF60 = 2;
        } else if (w & 0x480) {
            c->vF60 = 1;
        } else if (w & 0x240) {
            c->vF60 = 0;
        } else {
            c->vF60 = -1;
        }
        if (w & 0x1C0) {
            c->vF64 = 0;
        } else {
            c->vF64 = (w & 0xE00) ? 1 : -1;
        }
        c->vF38 = MOVE_S8(move, 0xD);
    }
    c->vF3C = move[0x30];
    c->vF40 = move[0x31];
    c->vF44 = move[6] + 0x32;
    c->vF48 = var[0xF];
    if (!noVar) {
        c->vF50 = move[6];
    }
    c->vF4C = MOVE_S8(var, 0x10) ? var[0x11] : 0;
    c->vF54 = MOVE_S16(var, 0x12);
    c->vF58 = MovePush(move, var, 0xCC);
    c->vF5C = MovePush(move, var, 0xD0);
    c->curMove = move;
    c->curVariant = var;
    cfCharacter_PlaySequence(c, MOVE_S16(var, 2), 1, 1, MOVE_S16(var, 4));
    if (MOVE_S8(move, 0xB) >= 0) {
        c->v410 = 0.0f;
    }
    if (move[6] == 3) {
        c->handler->v3C = c;
        c->handler->v40 = c->opponent;
        c->handler->v54 = 9;
    }
    if (var[0xC]) {
        CharSeq_RunUntil(&c->seq, var[0xC]);
        cfCharacter_ApplySeqFrame(c);
    }
    v = MOVE_S16(var, 6);
    if (v != -0x8000) {
        f32 f = (f32)((f64)(f32)v / 100.0);
        if (c->facing) {
            f = -f;
        }
        if ((f64)f == 0.0) {
            c->v480 = 0.0f;
        } else {
            c->v480 = f;
        }
    }
    v = MOVE_S16(var, 8);
    if (v != -0x8000) {
        c->v3F8 = 1;
        c->v3FC = (f32)((f64)(f32)MOVE_S16(var, 8) / 100.0);
    }
    v = MOVE_S16(var, 0xA);
    if (v != -0x8000) {
        c->v400 = 1;
        c->v404 = (f32)((f64)(f32)MOVE_S16(var, 0xA) / 100.0);
    }
    c->numLinks = MOVE_S8(move, 0x56);
    for (i = 0; i < c->numLinks; i++) {
        c->linkA[i] = MOVE_S16(move, 0x58 + i * 2);
        c->linkB[i] = MOVE_S8(move, 0x6E + i);
        c->linkC[i] = MOVE_S8(move, 0x7A + i);
        c->linkD[i] = *(u16 *)(move + 0x86 + i * 2);
        c->linkE[i] = *(u16 *)(move + 0xB2 + i * 2);
    }
    c->v3F4 = MOVE_S16(move, 0xC8);
    c->vF68[0] = MOVE_S16(move, 0x32);
    c->vF68[1] = MOVE_S16(move, 0x36);
    c->vF68[2] = MOVE_S16(move, 0x34);
    c->vF68[3] = MOVE_S16(move, 0x38);
    c->vF68[4] = MOVE_S16(move, 0x3E);
    c->vF68[5] = MOVE_S16(move, 0x42);
    c->vF68[6] = move[0x3A];
    c->vF68[7] = MOVE_S16(move, 0x3C);
    c->vF68[8] = MOVE_S16(move, 0x40);
    c->vF68[9] = move[0x3B];
    c->vF68[10] = MOVE_S16(move, 0x44);
    c->vF68[11] = MOVE_S16(move, 0x48);
    c->vF68[12] = MOVE_S16(move, 0x46);
    c->vF68[13] = MOVE_S16(move, 0x4A);
    c->vF68[14] = MOVE_S16(move, 0x4C);
    c->vF68[15] = MOVE_S16(move, 0x4E);
    c->vF68[16] = MOVE_S16(move, 0x50);
    c->vF68[17] = MOVE_S16(move, 0x54);
    c->vFB0 = (f32)MOVE_S16(move, 0x52);
    c->v460 = MOVE_S8(move, 0xCA);
    c->v464 = MOVE_S8(move, 0xCB);
    if (MOVE_S16(move, 0x1E)) {
        c->v1A0 = 0.0f;
    }
}

/* ---------------------------------------------------------------- blocking */

/*
 * 0x8006267C: holding "away" blocks - standing (sequence 12), crouching (13) or in the
 * air (14) - but only when the opponent is attacking within 350 units or one of its
 * projectiles is coming close. Switches between standing and crouching block.
 */
void cfCharacter_CheckBlock(cfCharacter *c, u32 buttons) {
    cfCharacter *o;
    f32 d, dist;
    u32 cmd;
    s32 s, seq, i;

    if (c->v258 || c->v4D8) {
        return;
    }
    o = c->opponent;
    d = c->screenX - o->screenX;
    if ((f64)d < 0.0) {
        dist = (f32)(0.0 - (f64)d);
    } else {
        dist = c->screenX - o->screenX;
    }
    if (c->v260) {
        cmd = c->aiCommand;
    } else {
        cmd = cfCharacter_ButtonsToCommand(c, buttons);
    }
    cmd &= -(u32)(c->v2BC < 200);

    s = c->seqIndex;
    if (s == 12 || s == 13) {
        if (!(c->v478 == 0.0f)) {
            return;
        }
        if (s == 12 && (cmd & 2)) {
            cfCharacter_PlaySequence(c, 13, -1, 1, 0xFFFFFFFF);
            s = c->seqIndex;
        }
        if (s == 13 && !(cmd & 2)) {
            cfCharacter_PlaySequence(c, 12, -1, 1, 0xFFFFFFFF);
        }
        return;
    }
    if (!(cmd & 8)) {
        return;
    }
    if (!(c->screenY == (f32)c->handler->floorY)) {
        s = c->seqIndex;
        if (!(s == 17 || s == 18 || s == 19)) {
            return;
        }
        seq = 14;
    } else {
        s = c->seqIndex;
        if (!(s == 0 || s == 4 || s == 3 || s == 6 || s == 7 || s == 8 || s == 1 || s == 5 || s == 2 ||
              (s == 20 && c->v3C))) {
            return;
        }
        seq = (cmd & 2) ? 13 : 12;
    }
    if (((c->opponent->seq.flags >> 4) & 1) & (dist < 350.0f)) {
        cfCharacter_PlaySequence(c, seq, -1, 1, 0xFFFFFFFF);
        return;
    }
    o = c->opponent;
    for (i = 0; i < CHAR_NUM_PROJECTILES; i++) {
        CharProjectile *p = &o->projectiles[i];
        f32 lim;

        if (!p->active || p->v4) {
            continue;
        }
        lim = (p->targetDot <= 0.0f) ? 1.0f * 350.0f : 0.25f * 350.0f;
        if (p->targetDist < lim) {
            cfCharacter_PlaySequence(c, seq, -1, 1, 0xFFFFFFFF);
            return;
        }
    }
}

/* ---------------------------------------------------------------- hits */

/* CF3CHAR tables used by the hit reaction */
extern s32 D_SeqNeutral[63];     /* 0x800CCC3C: neutral sequences (standing, walking, crouching, jumping) */
extern f32 D_EscapeStun[];       /* 0x800CD57C: attacker's stun after a guard escape, by hit strength */

static void SetFx(FxRing *r, const f32 *p) { r->pos[0] = p[0]; r->pos[1] = p[1]; r->pos[2] = p[2]; }
static void SetGlowPos(FxGlow *g, const f32 *p) { g->pos[0] = p[0]; g->pos[1] = p[1]; g->pos[2] = p[2]; }
static void SetCol(u8 *col, s32 r, s32 g, s32 b) { col[0] = r; col[1] = g; col[2] = b; }

/* ------------------------------------------------------------------------
 * cfCharacter_HitReaction (0x80063E4C): this fighter has been hit.
 *   type      -1 = nothing, 0 = blocked, 1 = high, 2 = low, 3 = from behind / overhead
 *   strength  0 / 1 / 2 (light, medium, heavy)
 *   kind      special reactions: 1-5, 8-11 pick fixed "hurt" sequences, 7 and 15 are
 *             pushes without a reaction (15 = landing from a slam)
 *   damage    before the +25% applied here
 *   sound     impact sound for the attacker (-1 = none)
 *   pushX/Y   knock-back, hitX/Y where the hit landed (screen), scale = hit-stop scale
 * Steps: a quick back-back tap in some sequences escapes the hit (no damage, attacker is
 * stunned); damage, combo counters and the dizzy meter; impact sounds; smoke, sparks,
 * debris, glows and bursts; knock-back; the hurt / knock-down sequence; the voice.
 * ---------------------------------------------------------------------- */
void cfCharacter_HitReaction(cfCharacter *c, s32 type, s32 strength, s32 kind, s32 damage, s32 sound,
                             f32 pushX, f32 pushY, f32 hitX, f32 hitY, f32 scale, s32 byProjectile) {
    s32 escaped = 0;
    s32 fromBehind = 0;
    s32 seqN = 0;
    f32 p[3];
    cfCharacter *o;
    CombatHandler *h;
    FxRing *b;
    FxGlow *g;
    s32 i;

    if (type == -1) return;
    damage += damage / 4;

    o = c->opponent;
    if (o->v440 == 0 && o->v424 == 0) func_8006DE6C(c->handler, -1, 0, c, 0, 1);

    /* escape: back, back within 12 then 25 ticks */
    if (c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0 && D_SeqDefaultV170[c->seqIndex] != 2 &&
        type != 0 && kind != 7 && kind != 15) {
        s32 index = c->inputHead;
        u32 now = c->handler->time;
        u32 dir;
        MPF_Mutex *m;

        if (type == 3) { fromBehind = 1; dir = 2; }
        else           { fromBehind = 0; dir = 4; }

        m = &c->handler->mutex;
        if (m->owner != osGetThreadId(NULL)) {
            osRecvMesg(&c->handler->mutex.mq, NULL, OS_MESG_BLOCK);
            c->handler->mutex.owner = osGetThreadId(NULL);
        }
        c->handler->mutex.count++;
        if (cfCharacter_MatchInput(c, dir & 0xFFFF, 0, &index, &now, 12, 0)) {
            index--;
            if (index < 0) index = 111;
            if (!cfCharacter_MatchInput(c, dir & 0xFFFF, 0, &index, &now, 25, 0)) {
                escaped = 1;
                damage = 0;
                type = 0;
                pushX = 0.0f;
            }
        }
        m = &c->handler->mutex;
        if (m->owner == osGetThreadId(NULL)) {
            if (--c->handler->mutex.count == 0) {
                c->handler->mutex.owner = 0;
                osSendMesg(&c->handler->mutex.mq, NULL, OS_MESG_NOBLOCK);
            }
        }
    }

    if (c->opponent->v424 != 0) c->v31C = 0;

    if (!escaped) {
        if (c->opponent->v424 != 0) {
            s32 r = (s32)guRandom();
            o = c->opponent;
            o->v440 = r % 15 + o->v440;
        }
        c->opponent->v440 += 1;
        if (c->v428 == 0) {
            c->v2CC += 5;
            o = c->opponent;
            if (o->v420 == 0) o->v2CC = strength * 10 + o->v2CC;
        }
    } else if (strength == 0) {
        c->v2CC += 5;
    } else {
        c->v2CC = strength * 10 + c->v2CC;
    }
    if (c->v2CC > 300) c->v2CC = 300;
    if (c->opponent->v2CC > 300) c->opponent->v2CC = 300;

    if (!escaped) c->v280 = 1;
    if (kind != 15) c->v46C = 0.0f;

    c->health -= damage;
    if (c->health < 0) c->health = 0;
    if (type == 0 && c->health <= 0) c->health = 1;     /* blocked hits never knock out */

    /* dizzy meter */
    if (c->seqIndex == 0x27) c->v2BC = 0;
    if ((f64)c->v2C0 > 0.0 && c->v2BC < 200) {
        c->v2C0 = 0.0f;
    } else {
        if (c->opponent->v420 == 0 && c->v334 == 0 && kind != 15 && type != 0) {
            c->v2BC = damage * 3 + c->v2BC;
            if (c->v2BC > 200) c->v2BC = 200;
        }
        if (c->v2BC >= 200 && c->health > 0) c->v2C0 = 100.0f;
    }

    /* impact sounds */
    if (damage != 0 || (f64)scale != 0.0 || c->handler->v54 == 9) {
        if (kind == 15) {
            func_8006DE6C(c->handler, 9, 0x5FFF, NULL, 0, 1);
        } else {
            s32 snd;
            if (c->health <= 0)  snd = c->opponent->v18C ? strength + 16 : strength + 8;
            else if (escaped)    snd = 11;
            else if (type == 0)  snd = 7;
            else                 snd = c->opponent->v18C ? strength + 16 : strength + 8;

            if (sound == -1 || escaped) {
                func_8006DE6C(c->handler, snd, 0x7FFF, NULL, 0, 1);
            } else if (type == 0) {
                func_8006DE6C(c->handler, snd, 0x7FFF, NULL, 0, 1);
                func_8006DE6C(c->handler, sound, 0x3FFF, c->opponent, 0, 1);
            } else {
                func_8006DE6C(c->handler, sound, 0x7FFF, c->opponent, 0, 1);
            }
        }
    }

    /* where the effects go (world space) */
    if (byProjectile) {
        p[0] = c->opponent->hitPos[0];
        p[1] = c->opponent->hitPos[1];
        p[2] = c->opponent->hitPos[2];
    } else {
        p[0] = (hitX - c->screenX) * c->handler->camDirX + c->pos[0];
        p[1] = -(hitY + c->handler->centre[1]);
        p[2] = (hitX - c->screenX) * c->handler->camDirZ + c->pos[2];
    }

    if (type != 0 && (damage != 0 || (f64)scale != 0.0 || c->handler->v54 == 9)) {
        cfCharacter_EmitSmoke(c, p[0], p[1], p[2], c->v40, 0);
        if (c->handler->v48 > 0) cfCharacter_EmitSmoke(c, p[0], p[1], p[2], c->v40, 0);
        if (strength > 0) {
            cfCharacter_EmitSmoke(c, p[0], p[1], p[2], c->v40, 1);
            if (c->handler->v48 > 0) cfCharacter_EmitSmoke(c, p[0], p[1], p[2], c->v40, 1);
        }
        if (strength >= 2) {
            cfCharacter_EmitSmoke(c, p[0], p[1], p[2], c->v40, 2);
            if (c->handler->v48 > 0) cfCharacter_EmitSmoke(c, p[0], p[1], p[2], c->v40, 2);
        }
        if (strength == 2 && (s32)guRandom() % 1024 < 768) {
            if (c->handler->v48 == 0) cfCharacter_EmitBits(c, 1, p[0], p[1], p[2]);
            else cfCharacter_EmitBits(c, (s32)guRandom() % 2 + 1, p[0], p[1], p[2]);
        }
        if (c->opponent->v420 != 0) {
            if (c->handler->v48 == 0) cfCharacter_EmitBits(c, 1, p[0], p[1], p[2]);
            else cfCharacter_EmitBits(c, (s32)guRandom() % 3 + 1, p[0], p[1], p[2]);
        }
        if (kind == 15) goto after_marks;

        cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 150.0f);
        cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 140.0f);
        cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 130.0f);
        cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 100.0f);
        cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 80.0f);
        if (strength > 0) {
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 150.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 140.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 130.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 100.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 80.0f);
        }
        if (strength >= 2) {
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 170.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 160.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 150.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 120.0f);
            cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 100.0f);
        }
        o = c->opponent;
        if (o->v418 != 0 || o->v420 != 0 || o->v424 != 0) {
            for (i = 0; i < 5; i++) cfCharacter_SpawnSpark(c, p[0], p[1], p[2], 180.0f);
        }
        if (strength >= 2) {
            b = Fx_NewBurst(c->handler);
            if (b != NULL) {
                SetFx(b, p);
                SetCol(b->colA, 255, 255, 0); b->colA[3] = 90;
                SetCol(b->colB, 255, 0, 0);   b->colB[3] = 0;
                b->v18 = (f32)((s32)guRandom() % 16 + 28);
                b->v24 = 10.0f;
                b->v28 = 5.0f;
                b->ageStep = 0.06f;
            }
        }
        if (strength == 1) {
            b = Fx_NewBurst(c->handler);
            if (b != NULL) {
                SetFx(b, p);
                SetCol(b->colA, 200, 255, 200); b->colA[3] = 120;
                SetCol(b->colB, 0, 255, 0);     b->colB[3] = 0;
                b->v18 = (f32)((s32)guRandom() % 10 + 15);
                b->v24 = 20.0f;
                b->v28 = 7.5f;
                b->ageStep = 0.07f;
            }
        }
        if (c->opponent->v420 != 0) {
            g = Fx_NewGlow(c->handler);
            if (g != NULL) {
                SetGlowPos(g, p);
                SetCol(g->colA, 150, 150, 255); g->colA[3] = 90;
                SetCol(g->colB, 0, 0, 255);     g->colB[3] = 0;
                g->v18 = 200.0f;
                g->v1C = 15.0f;
                g->v24 = 0.15f;
            }
        }
        if (c->opponent->v424 != 0) {
            g = Fx_NewGlow(c->handler);
            if (g != NULL) {
                SetGlowPos(g, p);
                SetCol(g->colA, 255, 150, 150); g->colA[3] = 90;
                SetCol(g->colB, 255, 0, 0);     g->colB[3] = 0;
                g->v18 = 200.0f;
                g->v1C = 15.0f;
                g->v24 = 0.15f;
            }
        }
        if (c->opponent->v434 != 0) {
            g = Fx_NewGlow(c->handler);
            if (g != NULL) {
                SetGlowPos(g, p);
                SetCol(g->colA, 255, 150, 150); g->colA[3] = 140;
                SetCol(g->colB, 255, 0, 0);     g->colB[3] = 0;
                g->v18 = 200.0f;
                g->v1C = 16.0f;
                g->v24 = 0.1f;
            }
        }
    }

    if (kind != 15) {
        for (i = 0; i < 4; i++) {
            FxMark *mk = &c->handler->marks[i];
            if (mk->active) continue;
            mk->active = 1;
            c->handler->marks[i].strength = type ? strength + 1 : 0;
            c->handler->marks[i].age = 0;
            c->handler->marks[i].v0C = 3;
            c->handler->marks[i].pos[0] = p[0];
            c->handler->marks[i].pos[1] = p[1];
            c->handler->marks[i].pos[2] = p[2];
            break;
        }
        if (c->opponent->v420 != 0) cfCharacter_SpawnDust(c, p[0], p[1], p[2], 1.5f);
        if (c->opponent->v424 != 0) cfCharacter_SpawnDust(c, p[0], p[1], p[2], 1.5f);
    }
after_marks:

    if (type == 0 && !escaped) {
        /* blocked: blue flash */
        g = Fx_NewGlow(c->handler);
        if (g != NULL) {
            SetGlowPos(g, p);
            SetCol(g->colA, 100, 100, 255);
            SetCol(g->colB, 50, 50, 255);
            g->v18 = 5.0f;
            g->v1C = 2.25f;
            g->v24 = 0.1f;
        }
        b = Fx_NewBurst(c->handler);
        if (b != NULL) {
            SetFx(b, p);
            b->colB[0] = 50; b->colA[0] = 50;
            b->colB[1] = 50; b->colA[1] = 50;
            b->colB[2] = 255; b->colA[2] = 255;
            b->v18 = 5.0f;
            b->v24 = 13.0f;
            b->v28 = 2.75f;
            b->ageStep = 0.1f;
        }
    }
    if (escaped) {
        b = Fx_NewBurst(c->handler);
        if (b != NULL) {
            SetFx(b, p);
            SetCol(b->colA, 130, 130, 255);
            SetCol(b->colB, 230, 230, 255); b->colB[3] = 50;
            b->v18 = 20.0f;
            b->v24 = 5.0f;
            b->v28 = 1.75f;
            b->ageStep = 0.05f;
        }
        b = Fx_NewBurst(c->handler);
        if (b != NULL) {
            SetFx(b, p);
            SetCol(b->colA, 130, 130, 255);
            SetCol(b->colB, 240, 240, 255); b->colB[3] = 0;
            b->v18 = 10.0f;
            b->v24 = 2.5f;
            b->v28 = 3.75f;
            b->ageStep = 0.05f;
        }
        b = Fx_NewBurst(c->handler);
        if (b != NULL) {
            SetFx(b, p);
            SetCol(b->colA, 130, 130, 255);
            SetCol(b->colB, 250, 250, 255); b->colB[3] = 0;
            b->v24 = 1.0f;
            b->v28 = 4.75f;
            b->ageStep = 0.05f;
        }
    }

    /* knock-back */
    if (type == 0) pushY = 0.0f;
    if ((f64)pushY != 0.0) {
        c->v46C = pushY + c->v46C;
        c->v470 = pushX;
    } else if (c->screenY < (f32)c->handler->floorY) {
        c->v46C = (f32)((f64)c->v46C + -1.9);
        c->v470 = c->opponent->facing ? -6.0f : 6.0f;
    } else {
        c->v46C = pushY + c->v46C;
        c->v478 = pushX;
    }
    if (c->v3C != 0 && c->screenY < (f32)c->handler->floorY && (f64)c->v478 < 25.0) {
        c->v478 = c->facing == 0 ? -25.0f : 25.0f;
    }
    c->v47C = byProjectile;
    c->v480 = 0.0f;
    if (c->opponent->v434 != 0) c->handler->v2CC4 = 21.0f;
    else c->handler->v2CC4 = scale;
    if (c->health <= 0) {
        c->v2B4 = 30.0f;
        h = c->handler;
        h->v2CC4 = (f32)((f64)h->v2CC4 * 1.2);
        if (c->v43C == 0) {
            c->v2C8++;
            c->v43C = 1;
        }
    }
    if (kind == 7 || kind == 15) {
        c->v478 = pushX;
        c->handler->v2CC4 = scale;
        goto finish;
    }

    /* the hurt / knock-down sequence */
    if (type != 0) {
        if (c->screenY < (f32)(c->handler->floorY - 10)) {
            /* in the air */
            if (c->v31C != 0)   seqN = 0x21;
            else if (kind == 3) { seqN = 0x21; c->v2B4 = 0.01f; }
            else if (kind == 4) seqN = 0x37;
            else if (kind == 5) seqN = 0x36;
            else                seqN = 0x1E;
            if (c->health <= 0) seqN = 0x21;
        } else if (c->v170 == 1) {
            /* crouching */
            switch (kind) {
            case 1:  seqN = 0x23; break;
            case 2:  seqN = 0x24; break;
            case 3:  seqN = 0x20; c->v2B4 = 0.01f; break;
            case 4:  seqN = 0x37; break;
            case 5:  seqN = 0x36; break;
            case 8:  seqN = 0x31; break;
            case 9:  seqN = 0x32; break;
            case 10: seqN = 0x33; break;
            case 11: seqN = 0x34; break;
            default: seqN = strength + 0x1B; break;
            }
        } else {
            switch (kind) {
            case 1:  seqN = 0x22; break;
            case 2:  seqN = 0x24; break;
            case 3:  seqN = 0x1F; c->v2B4 = 0.01f; break;
            case 4:  seqN = 0x37; break;
            case 5:  seqN = 0x36; break;
            case 8:  seqN = 0x31; break;
            case 9:  seqN = 0x32; break;
            case 10: seqN = 0x33; break;
            case 11: seqN = 0x34; break;
            default: seqN = (type == 1) ? strength + 0x15 : strength + 0x18; break;
            }
        }
        if (c->v31C != 0) {
            if ((u32)(seqN - 0x15) < 3 || (u32)(seqN - 0x18) < 3 || (u32)(seqN - 0x1B) < 3 ||
                seqN == 0x24 || (u32)(seqN - 0x22) < 2) {
                seqN = 0x20;
            }
        }
        if (c->health <= 0 &&
            ((f64)c->v46C < 0.0 || c->v2C8 < c->handler->v30) &&
            c->opponent->v424 == 0) {
            if (c->v170 == 1)      seqN = 0x20;
            else if (c->v170 == 2) seqN = 0x21;
            else                   seqN = 0x1F;
        }
        if ((u32)(seqN - 0x31) < 2 || seqN == 0x33 || seqN == 0x34) {
            cfCharacter_PlaySequence(c, seqN, -1, 1, (u32)-1);
        } else {
            cfCharacter_PlaySequence(c, seqN, 1, 1, (u32)-1);
        }
        if (seqN == 0x21 || seqN == 0x36 || seqN == 0x37) {
            c->v48C = (f32)((f64)c->v46C * 0.5);
            c->v494 = 1;
        }
    }

    if (escaped) {
        cfCharacter_PlaySequence(c, fromBehind ? 0x10 : 0xF, 1, 1, (u32)-1);
        c->opponent->v194 = D_EscapeStun[strength];
        c->opponent->v46C = 0.0f;
    }

    if (c->screenY < (f32)c->handler->floorY && (f64)c->v46C > 0.0 && c->v3C != 0) c->v198 = 1;

    if (type == 0) {
        if (strength == 0)      c->v19C = 11.0f;
        else if (strength == 1) c->v19C = 13.0f;
        else if (strength == 2) c->v19C = 15.0f;
    }

    /* the voice: a random sound from one of the character's sound sets */
    {
        s32 voice = -1;
        s32 set;
        if (escaped)                              set = 0x38;
        else if (type == 0 && c->health > 0)      set = 0x30;
        else if (c->health <= 0 || strength >= 2) set = 8;
        else                                      set = 0;
        if (c->def->soundSets[set] > 0) {
            s32 r = (s32)guRandom();
            voice = c->def->soundSets[set + 1 + r % c->def->soundSets[set]];
        }
        if (voice >= 0) func_8006DE6C(c->handler, voice + 100, 0x6665, c, 0, 0);
    }

finish:
    if (c->health <= 0 && c->v2C8 >= c->handler->v30 && c->v31C != 0) {
        o = c->opponent;
        if (o->v328 == o->v438) c->v2B4 = 0.1f;
        else c->v2B4 = 9999.0f;
    }
}

/* 0x800639E4: delivers the hit this fighter landed (combo damage scaling), and starts the
   follow-up move or sequence the move asked for on a hit or on a block */
void cfCharacter_DeliverHit(cfCharacter *c) {
    s32 oppHealth;
    cfCharacter *o;

    if (c->hitType == -1 || c->hitKind == 6) {
        goto end;
    }
    oppHealth = c->opponent->health;
    if (c->hitKind != 15 && !c->hitByProjectile) {
        if (c->v424) {
            c->hitDamage = 255;
        } else if (c->v440 > 0 && c->hitDamage > 0) {
            /* each hit in a combo does less: 1 - hits/35, at least 0.4 */
            f32 k = (f32)(1.0 - (f64)(f32)c->v440 / 35.0);
            s32 d;

            if ((f64)k < 0.6) {
                k = 0.4f;
            }
            d = (s32)((f32)c->hitDamage * k);
            c->hitDamage = d;
            if (d <= 0) {
                c->hitDamage = 1;
            }
        }
    }
    cfCharacter_HitReaction(c->opponent, c->hitType, c->hitE4, c->hitKind, c->hitDamage, c->hitF0, c->hitPushX,
                  c->hitPushY, c->hitFC, c->hit300, c->hit310, c->hitByProjectile);
    if (oppHealth > 0 && c->opponent->health <= 0 && !c->opponent->v31C) {
        c->v324 = c->vF2C;
        c->v328 = c->v438;
    }
    if (c->opponent->v31C && c->v438 != c->v328) {
        c->v320 = 1;
    }
    if (c->hitKind == 15 || c->hitByProjectile) {
        goto end;
    }
    if (c->screenY < (f32)c->handler->floorY && 0.0f < c->v46C && !c->vF50) {
        c->v408 = 1;
        c->v40C = c->v188;
        c->v410 = 10.0f;
        c->v414 = c->v188;
    }
    if (c->hitType == 0) {
        /* blocked */
        if (c->vF68[2] != -1) {
            cfCharacter_PlaySequence(c, c->vF68[2], 1, 1, 0xFFFFFFFF);
            c->v41C = c->v418;
        }
        if (c->vF68[7] != -1 && c->vF68[9] == c->v3F0) {
            cfCharacter_PlaySequence(c, c->vF68[7], 1, 1, 0xFFFFFFFF);
            c->v41C = c->v418;
        }
        if (c->vF68[3] != -1) {
            cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[3]), NULL);
            c->v41C = c->v418;
        }
        if (c->vF68[8] != -1 && c->vF68[9] == c->v3F0) {
            cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[8]), NULL);
            c->v41C = c->v418;
        }
    } else {
        if (c->vF68[0] != -1 && c->v194 == 0.0f) {
            cfCharacter_PlaySequence(c, c->vF68[0], 1, 1, 0xFFFFFFFF);
            c->v41C = c->v418;
        }
        if (c->vF68[4] != -1 && c->vF68[6] == c->v3F0) {
            cfCharacter_PlaySequence(c, c->vF68[4], 1, 1, 0xFFFFFFFF);
            c->v41C = c->v418;
        }
        if (c->vF68[1] != -1 && c->v194 == 0.0f) {
            cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[1]), NULL);
            c->v41C = c->v418;
        }
        if (c->vF68[5] != -1 && c->vF68[6] == c->v3F0) {
            cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[5]), NULL);
            c->v41C = c->v418;
        }
    }
end:
    /* a double knock-out with rounds to spare: both get 1 health back */
    if (c->health <= 0) {
        o = c->opponent;
        if (o->health <= 0 && !(c->v2C8 < c->handler->v30) && !(o->v2C8 < c->handler->v30)) {
            c->health = 1;
            c->opponent->health = 1;
            c->v2C8--;
            c->opponent->v2C8--;
        }
    }
}

static f32 HalfWidth(f32 lo, f32 hi, f64 scale) {
    f64 d;

    if ((f64)(hi - lo) < 0.0) {
        d = 0.0 - (f64)(hi - lo);
    } else {
        d = (f64)(hi - lo);
    }
    return (f32)(d / 2.0 * scale);
}

static f32 CentreAlong(f32 base, f32 lo, f32 hi, f32 cam) {
    return (f32)((f64)base + (f64)(lo + hi) / 2.0 * (f64)cam);
}

/*
 * 0x80063270: tests this fighter's projectiles against the opponent's three body
 * volumes (from UpdateRects' bounds: high, low and the third type). A hit sets up
 * hitType (1/2/3, or 0 when blocked) and the damage for DeliverHit.
 */
void cfCharacter_CheckProjectileHits(cfCharacter *c) {
    cfCharacter *o;
    s32 m, i;
    f32 ny, aTop, aBot, bTop, bBot, cTop, cBot, wA, wB, wC;
    f32 xA, zA, xB, zB, xC, zC;

    if (c->v258) {
        return;
    }
    m = c->handler->v54;
    if (m != 4 && m != 9 && m != 8 && m != 5) {
        return;
    }
    if (!c->v424) {
        o = c->opponent;
        if (o->seq.flag20 || o->seq.cmdB1 || o->v178) {
            return;
        }
    }
    if (c->v334) {
        return;
    }
    o = c->opponent;
    if (o->seq.pairs[0].set) {
        return;
    }
    ny = (f32)(0.0 - (f64)o->screenY);
    aTop = ny + o->bounds[0];
    aBot = ny + o->bounds[1];
    bTop = ny + o->bounds[4];
    bBot = ny + o->bounds[5];
    cTop = ny + o->bounds[8];
    cBot = ny + o->bounds[9];
    wA = HalfWidth(c->opponent->bounds[2], c->opponent->bounds[3], 0.9);
    wB = HalfWidth(c->opponent->bounds[6], c->opponent->bounds[7], 0.9);
    wC = HalfWidth(c->opponent->bounds[10], c->opponent->bounds[11], 0.9);
    o = c->opponent;
    xA = CentreAlong(o->pos[0], o->bounds[2], o->bounds[3], c->handler->camDirX);
    zA = CentreAlong(o->pos[2], o->bounds[2], o->bounds[3], c->handler->camDirZ);
    xB = CentreAlong(o->pos[0], o->bounds[6], o->bounds[7], c->handler->camDirX);
    zB = CentreAlong(o->pos[2], o->bounds[6], o->bounds[7], c->handler->camDirZ);
    xC = CentreAlong(o->pos[0], o->bounds[10], o->bounds[11], c->handler->camDirX);
    zC = CentreAlong(o->pos[2], o->bounds[10], o->bounds[11], c->handler->camDirZ);

    for (i = 0; i < CHAR_NUM_PROJECTILES; i++) {
        CharProjectile *p = &c->projectiles[i];
        f32 r, d[2];
        s32 blocked, s;

        if (!p->active || p->v4) {
            continue;
        }
        if (!(0.0 < (f64)p->vDC)) {
            continue;
        }
        r = p->vDC;
        if (p->posY < cTop + r && cBot - r < p->posY) {
            d[0] = xC - p->posX;
            d[1] = zC - p->posZ;
            if (!((f64)((cfVec2_Length(d) - wC) - p->vDC) < 0.0)) {
                continue;
            }
            c->hitType = 3;
        } else if (p->posY < aTop + r && aBot - r < p->posY) {
            d[0] = xA - p->posX;
            d[1] = zA - p->posZ;
            if (!((f64)((cfVec2_Length(d) - wA) - p->vDC) < 0.0)) {
                continue;
            }
            c->hitType = 1;
        } else {
            r = p->vDC;
            if (!(p->posY < bTop + r) || !(bBot - r < p->posY)) {
                continue;
            }
            d[0] = xB - p->posX;
            d[1] = zB - p->posZ;
            if (!((f64)((cfVec2_Length(d) - wB) - p->vDC) < 0.0)) {
                continue;
            }
            c->hitType = 2;
        }

        /* blocking: flags in v78 make the projectile pass some blocks */
        blocked = 0;
        o = c->opponent;
        if ((p->v78 & 2) && o->seqIndex == 13) {
            p->v48 = 1;
        } else if ((p->v78 & 1) && c->opponent->seqIndex == 12) {
            p->v48 = 1;
        } else if ((p->v78 & 4) && c->opponent->seqIndex == 14) {
            p->v48 = 1;
        } else {
            s = c->opponent->seqIndex;
            if (s == 12 || s == 13 || s == 14) {
                blocked = 1;
            }
            p->v48 = 1;
        }
        p->blocked = blocked;
        if (blocked) {
            c->hitType = 0;
        }
        c->hitByProjectile = 1;
        c->hitE4 = 2;
        c->hitKind = p->vB0;
        c->hitPos[0] = p->posX;
        c->hitPos[1] = p->posY;
        c->hitPos[2] = p->posZ;
        c->hitPushX = p->vCC;
        if (p->facing2) {
            c->hitPushX = -p->vCC;
        }
        c->hitPushY = p->vD0;
        c->hitDamage = p->vB4;
        if (blocked && p->vB4 != 0) {
            s32 dmg = (s32)((f64)(f32)c->hitDamage * 0.2);
            c->hitDamage = dmg;
            if (dmg == 0) {
                c->hitDamage = 1;
            }
        }
        c->hitF0 = -1;
        c->hit310 = p->vD4;
    }
}

/* ---------------------------------------------------------------- input */

static void PushInput(cfCharacter *c, u32 command, u32 buttons, s32 used) {
    CharInput *e;

    if (++c->inputHead >= CHAR_NUM_INPUTS) {
        c->inputHead = 0;
    }
    e = &c->inputs[c->inputHead];
    e->command = command;
    e->buttons = buttons;
    e->flags = used;
    e->time = c->handler->time;
}

#define MV_U16(m, o) (*(u16 *)((m) + (o)))

/*
 * 0x80061F08: records this frame's presses and releases in the input history and looks
 * for special-move motions: each move variant gives the last input to look for, and the
 * move a list of earlier inputs that must come before it within 16 (or 24) ticks, plus
 * optionally buttons that must have been held. Recognised moves are queued in a2c[].
 * (Runs under the combat handler's lock - which, as in the original, stays taken when
 * nothing new was pressed or released.)
 */
void cfCharacter_ProcessInput(cfCharacter *c, u32 buttons) {
    MPF_Mutex *m;
    u32 cmd, prev, pressed, released, pDir, rDir, pBtn, rBtn;
    s32 recorded = 0;
    s32 k;
    u8 *mv;

    m = &c->handler->mutex;
    if (m->owner != osGetThreadId(NULL)) {
        osRecvMesg(&c->handler->mutex.mq, NULL, OS_MESG_BLOCK);
        c->handler->mutex.owner = osGetThreadId(NULL);
    }
    c->handler->mutex.count++;

    if (c->v260) {
        cmd = (u16)c->aiCommand;
    } else {
        u32 b = buttons & 0xFFFF3FF0;
        if (buttons & c->buttonMap[0]) b |= 0x4000;
        if (buttons & c->buttonMap[1]) b |= 0x2;
        if (buttons & c->buttonMap[2]) b |= 0x8;
        if (buttons & c->buttonMap[3]) b |= 0x8000;
        if (buttons & c->buttonMap[4]) b |= 0x4;
        if (buttons & c->buttonMap[5]) b |= 0x1;
        cmd = cfCharacter_ButtonsToCommand(c, b);
    }
    prev = c->v4E0;
    pressed = cmd & (cmd ^ prev);
    released = cmd ^ (prev | cmd);
    c->v4E0 = cmd;
    if (0.0 < (f64)c->v2C0 && (pressed & 0xFFFF)) {
        /* mashing buttons gets you out of a dizzy state faster */
        c->v2C0 = (f32)((f64)c->v2C0 - 1.5);
        if ((f64)c->v2C0 < 0.0) {
            c->v2C0 = 0.0f;
        }
        c->seq.speed -= 0x20;
        if ((s16)c->seq.speed < 0x50) {
            c->seq.speed = 0x50;
        }
    }
    pDir = pressed & 0xF;
    rDir = released & 0xF;
    pBtn = pressed & 0x1FF0;
    rBtn = released & 0x1FF0;
    if (rDir) {
        PushInput(c, rDir, rBtn | (cmd | rDir), 1);
    }
    if (pDir || (rDir && (cmd & 0xF))) {
        PushInput(c, cmd & 0xF, cmd, 0);
        recorded = 1;
    }
    if (rBtn) {
        PushInput(c, rBtn, rBtn | (cmd | rDir), 1);
        recorded = 1;
    }
    if (pBtn) {
        PushInput(c, pBtn, cmd, 0);
        recorded = 1;
    }
    if (!recorded) {
        return;
    }

    mv = c->moves;
    for (k = 0; k < c->numMoves; k++, mv += *(s32 *)mv) {
        u8 *var = mv + MV_U16(mv, 0x2E);
        s32 j;

        for (j = 0; j < *(s16 *)(mv + 0x2C); j++, var += 0x24) {
            s32 idx, n, ok;
            u32 t, code, need;
            u8 holdIndex = 0xFF;
            u16 held = 0;

            if (MV_U16(var, 0) == 0) {
                continue;
            }
            idx = c->inputHead;
            t = c->handler->time;
            if (!cfCharacter_MatchInput(c, MV_U16(var, 0), MV_U16(mv, 0x20), &idx, &t, 16, *(s16 *)(mv + 0x28) > 0)) {
                continue;
            }
            /* found the final input: now the motion before it, newest first */
            code = idx | 0xFFFFFF00;
            need = MV_U16(var, 0) & 0x7FFF;
            ok = 1;
            for (n = *(s16 *)(mv + 0x28) - 1; n >= 0; n--) {
                u16 step = MV_U16(mv + MV_U16(mv, 0x2A), n * 2);
                u32 window = 16;

                if ((need & 0xFFFF & 0xFFFF7FFF) & step) {
                    if (--idx < 0) {
                        idx = 111;
                        window = 24;
                    }
                }
                if (*(s16 *)(mv + 0x28) != 1) {
                    window = 24;
                }
                if (!cfCharacter_MatchInput(c, step, MV_U16(mv, 0x20), &idx, &t, window, 1)) {
                    ok = 0;
                    break;
                }
                if ((code & 0xFF000000) == 0xFF000000) {
                    code = (code << 8) | idx;
                }
                need = step & 0x7FFF;
            }
            if (!ok) {
                break;
            }
            if (MV_U16(mv, 0x24) && *(s16 *)(mv + 0x26) > 0) {
                ok = 0;
                if (cfCharacter_FindInput(c, MV_U16(mv, 0x24), &idx, &t, 16)) {
                    ok = cfCharacter_InputHeld(c, MV_U16(mv, 0x24), *(s16 *)(mv + 0x26), idx);
                    holdIndex = (u8)idx;
                    held = MV_U16(mv, 0x24) & 0x7FFF;
                }
                if (!ok) {
                    break;
                }
            }
            if (++c->vA28 >= 64) {
                c->vA28 = 0;
            }
            c->a2c[c->vA28].move = mv;
            c->a2c[c->vA28].variant = var;
            c->a2c[c->vA28].timer = 5;
            c->a2c[c->vA28].inputs = code;
            c->a2c[c->vA28].holdIndex = holdIndex;
            c->a2c[c->vA28].holdButtons = held;
            break;
        }
    }

    m = &c->handler->mutex;
    if (m->owner == osGetThreadId(NULL)) {
        if (--c->handler->mutex.count == 0) {
            c->handler->mutex.owner = 0;
            osSendMesg(&c->handler->mutex.mq, NULL, OS_MESG_NOBLOCK);
        }
    }
}

extern f32 D_HitStrengthScale[];   /* 0x800CD570, by hit strength */

/* a rectangle moved to screen position */
static void RectAt(SeqRect *out, SeqRect *in, f32 x, f32 y) {
    *out = *in;
    out->left = (s32)((f32)out->left + x);
    out->right = (s32)((f32)out->right + x);
    out->top = (s32)((f32)out->top + y);
    out->bottom = (s32)((f32)out->bottom + y);
}

/* do b and a overlap on one axis? (b's lo or hi edge inside a, or b spanning a) */
static s32 Overlap(s16 aLo, s16 aHi, s16 bLo, s16 bHi) {
    if (!(bLo < aLo || aHi < bLo)) return 1;
    if (!(bHi < aLo || aHi < bHi)) return 1;
    return (bLo < aLo) && (aHi < bHi);
}

/*
 * 0x8006299C: tests this frame's attack rectangles against the opponent's hurt boxes.
 * The attack overlapping the most area wins; the box types it touched decide the hit
 * height (1 = high, 2 = middle, 3 = low; 0 = blocked) and the move decides damage and
 * push. Command 0xA3 gives an explicit hit point instead (e.g. throws).
 */
void cfCharacter_CheckAttackHits(cfCharacter *c) {
    cfCharacter *o;
    s32 a, b, best, m, s;

    if (c->v258) {
        return;
    }
    c->hitType = -1;
    if (!c->v424) {
        m = c->handler->v54;
        if (m != 4 && m != 9 && m != 8 && m != 5) {
            return;
        }
        if (!(c->seq.flags & 0x10) || c->v334 || c->opponent->seq.pairs[0].set) {
            return;
        }
    }
    if (c->v2D4) {
        return;
    }
    if (c->seq.pairs[3].set) {
        c->hitByProjectile = 0;
        c->hitFC = (f32)c->seq.pairs[3].a + c->screenX;
        c->hit300 = (f32)c->seq.pairs[3].b + c->screenY;
        c->hitType = 2;
        c->hitE4 = c->v17C - 1;
        if (c->hitE4 < 0) {
            c->hitE4 = 0;
        }
        c->hitKind = 7;
        c->hitPushX = 0.0f;
        c->hitPushY = 0.0f;
        c->hitDamage = c->seq.cmdB0 ? c->seq.cmdB0Value : c->v184;
        c->hitF0 = c->seq.cmdB2Negative;
        c->hit310 = c->seq.cmdC5 ? c->seq.cmdC5Value : D_HitStrengthScale[c->hitE4];
        c->v2D0 = 1;
        c->v2D4 = 1;
        c->v2DC = 1;
        return;
    }
    best = 0;
    if (!c->v424) {
        o = c->opponent;
        if (o->seq.flag20 || o->seq.cmdB1 || o->v178) {
            return;
        }
    }
    for (a = 0; a < c->numAttacks; a++) {
        SeqRect A;
        s32 blocked = 0, sum0 = 0, sum1 = 0, sum2 = 0, total;

        RectAt(&A, &c->attacks[a], c->screenX, c->screenY);
        if ((c->v174 & 2) && c->opponent->seqIndex == 13) {
        } else if ((c->v174 & 1) && c->opponent->seqIndex == 12) {
        } else if ((c->v174 & 4) && c->opponent->seqIndex == 14) {
        } else {
            s = c->opponent->seqIndex;
            if ((s == 12 || s == 13 || s == 14) && !c->v424) {
                blocked = 1;
            }
        }
        for (b = 0; b < c->opponent->numBodyRects; b++) {
            SeqRect B, U;
            s32 area, t;

            RectAt(&B, &c->opponent->bodyRects[b], c->opponent->screenX, c->opponent->screenY);
            if (!Overlap(A.top, A.bottom, B.top, B.bottom) || !Overlap(A.left, A.right, B.left, B.right)) {
                continue;
            }
            U = A;
            mpRectIntersect(&U, &B);
            area = (s16)(U.right - U.left + 1) * (s16)(U.bottom - U.top + 1);
            t = c->opponent->bodyType[b];
            if (t == 0) {
                sum0 += area;
            } else if (t == 2) {
                sum2 += area;
            } else {
                sum1 += area;
            }
        }
        if (c->v424) {
            sum0++;
            sum2++;
            sum1++;
        }
        if (blocked) {
            if (sum0) sum0 = 1;
            if (sum1) sum1 = 1;
            if (sum2) sum2 = 1;
        }
        total = sum0 + sum1 + sum2;
        if (!(best < total)) {
            continue;
        }
        best = total;
        c->hitFC = (f32)c->attackA[a] + c->screenX;
        c->hit300 = (f32)c->attackB[a] + c->screenY;
        if (blocked) {
            c->hitType = 0;
        } else if (sum1 < sum0 || sum2 < sum0) {
            c->hitType = 1;
        } else {
            c->hitType = (sum0 < sum2 || sum1 < sum2) ? 3 : 2;
        }
        c->hitByProjectile = 0;
        c->hitE4 = c->v17C - 1;
        if (c->hitE4 < 0) {
            c->hitE4 = 0;
        }
        c->hitKind = c->seq.flags & 0xF;
        if (c->v3F8) {
            c->hitPushX = c->v3FC;
        } else if (c->seq.cmdC4) {
            c->hitPushX = c->seq.cmdC4Value;
        } else if (c->seq.hasVal5C) {
            c->hitPushX = c->seq.val5C;
        } else if (c->hitKind != 0) {
            c->hitPushX = 0.0f;
        } else if (c->hitE4 == 0) {
            c->hitPushX = c->stats[5];
        } else if (c->hitE4 == 1) {
            c->hitPushX = c->stats[6];
        } else {
            c->hitPushX = c->stats[7];
        }
        if (c->v400) {
            c->hitPushY = c->v404;
        } else {
            c->hitPushY = 0.0f;
        }
        if (cfCharacter_SideOfOpponent(c) > 0) {
            c->hitPushX = -c->hitPushX;
        }
        c->hitDamage = c->seq.cmdB0 ? c->seq.cmdB0Value : c->v184;
        if (blocked && c->hitDamage > 0) {
            if (!c->vF50) {
                c->hitDamage = 0;
            } else {
                s32 d = (s32)((f64)(f32)c->hitDamage * 0.2);
                c->hitDamage = d;
                if (d == 0) {
                    c->hitDamage = 1;
                }
            }
        }
        c->hitF0 = c->seq.cmdB2Negative;
        c->hit310 = c->seq.cmdC5 ? c->seq.cmdC5Value : D_HitStrengthScale[c->hitE4];
        c->v2D0 = 1;
        c->v2D4 = 1;
        c->v2D8 = !blocked;
        c->v2DC = !blocked;
    }
    if (c->hitType == 0 && (c->seq.flags & 0x200)) {
        CharSeq_Reverse(&c->seq);   /* blocked: the attack bounces back */
    }
}

/* ------------------------------------------------------------------------
 * cfCharacter_UpdatePhysics (0x80060324): one frame of movement.
 *   - while grabbed (v334) the fighter is pinned to the opponent's grab point;
 *   - spinning (v4A4) turns the sprite, kept within +-360 degrees;
 *   - a timed slide (seq.waitTimer > 0) just adds v484/v488;
 *   - otherwise: fall with vertical speed v46C (x7), land on the floor (with
 *     bounce v48C, sound, debris, shock-wave rings, and screen shake / damage
 *     for a slam, v318), push the opponent at the arena edges, walk/dash
 *     speeds for sequences 6/7/8, gravity while in the air and friction on
 *     the horizontal speeds v478/v480.
 * ---------------------------------------------------------------------- */
void cfCharacter_UpdatePhysics(cfCharacter *c) {
    f32 oldY = c->screenY;
    s32 bounced = 0;
    s32 airborne;
    f32 gravity, friction, impact, volume, floor;
    CombatHandler *h;
    FxRing *r;

    if (c->v468 != 0) return;
    if (c->v194 > 0.0f) return;

    if (c->v334 != 0) {
        cfCharacter *o;
        if (c->seqIndex == 0x2C) return;
        o = c->opponent;
        if (o->seqIndex == 0x2C) return;
        c->screenX = (f32)o->seq.pairs[0].a + o->screenX;
        o = c->opponent;
        c->screenY = (f32)o->seq.pairs[0].b + o->screenY;
        cfCharacter_UpdateMatrix(c);
        return;
    }

    if (c->v4A4 != 0.0f) {
        c->v49C = ScaleByFrameTime(c->v4A4) + c->v49C;
        while ((f64)c->v49C > 360.0) c->v49C = (f32)((f64)c->v49C - 360.0);
        while ((f64)c->v49C < -360.0) c->v49C = (f32)((f64)c->v49C + 360.0);
        cfCharacter_UpdateMatrix(c);
    }

    if (c->seq.waitTimer > 0.0f) {
        c->screenX = ScaleByFrameTime(c->v484) + c->screenX;
        c->screenY = ScaleByFrameTime(c->v488) + c->screenY;
        return;
    }

    gravity  = c->seq.hasVal74 ? c->seq.val74 : c->handler->gravity;
    friction = c->seq.hasVal6C ? c->seq.val6C : c->handler->friction;
    if (c->v498 != 0.0f) friction = c->v498;

    c->screenY = ScaleByFrameTime((f32)((f64)c->v46C * 7.0)) + c->screenY;
    floor = (f32)c->handler->floorY;
    if (floor <= c->screenY) {
        if (oldY < floor) {
            /* just landed */
            c->v4DC = 1;
            if (c->v408 != 0 && c->v40C != -1) {
                c->v410 = 10.0f;
                c->v414 = c->v188;
            }
            c->v408 = 0;

            impact = (f32)((f64)c->v46C / 2.5);
            if ((f64)impact > 1.0) impact = 1.0f;
            volume = impact * 32767.0f;

            if (c->v48C != 0.0f) {
                /* bounce */
                c->v46C = c->v48C;
                c->v490 = c->v48C;
                c->v48C = (f32)((f64)c->v48C * 0.3);
                if (-0.6 < (f64)c->v48C) c->v48C = 0.0f;
                bounced = 1;
                func_8006DE6C(c->handler, 13, (s32)volume, c, 0, 1);
                if ((s32)guRandom() % 1024 < 512) {
                    s32 n = (s32)guRandom() % 3;
                    cfCharacter_EmitBits(c, n + 1, c->pos[0], c->pos[1], c->pos[2]);
                }
                if (c->v318 != 0) {
                    /* slammed into the floor: take the damage now */
                    cfCharacter_HitReaction(c, 2, 2, 15, c->v318, -1, 0.0f, 0.0f, c->screenX, c->screenY, 0.0f, 0);
                    r = Fx_NewRing(c->handler);
                    if (r != NULL) {
                        r->pos[0] = c->pos[0];
                        r->pos[1] = 0.0f;
                        r->pos[2] = c->pos[2];
                        r->v24 = 5.0f;
                    }
                    r = Fx_NewRing(c->handler);
                    if (r != NULL) {
                        r->pos[0] = c->pos[0];
                        r->pos[1] = 0.0f;
                        r->pos[2] = c->pos[2];
                        r->v24 = 50.0f;
                    }
                    h = c->handler;
                    h->shake = impact + h->shake;
                } else {
                    r = Fx_NewRing(c->handler);
                    if (r != NULL) {
                        r->pos[0] = c->pos[0];
                        r->pos[1] = 0.0f;
                        r->pos[2] = c->pos[2];
                        r->v24 = 5.0f;
                    }
                }
                if (c->v494 != 0) cfCharacter_PlaySequence(c, 0x35, 1, 1, (u32)-1);
            } else if ((f64)c->v490 != 0.0) {
                /* last bounce finished: keep sliding */
                c->v490 = 0.0f;
                c->v480 = c->v470;
                c->v46C = 0.0f;
                c->v470 = 0.0f;
                c->v474 = 0.0f;
                c->v498 = 0.32f;
                friction = 0.32f;
                func_8006DE6C(c->handler, 13, (s32)volume, c, 0, 1);
            } else if ((f32)c->handler->floorY < c->screenY) {
                c->v46C = 0.0f;
                c->v470 = 0.0f;
                c->v474 = 0.0f;
                func_8006DE6C(c->handler, 12, 0x7FFF, c, 0, 1);
            }
        }
        c->screenY = (f32)c->handler->floorY;
    }

    cfCharacter_ClampToArena(c);

    c->screenX = ScaleByFrameTime(c->v478) + c->screenX;
    h = c->handler;
    if (c->screenX < (f32)h->minX) {
        c->opponent->screenX = ((f32)h->minX - c->screenX) + c->opponent->screenX;
        h = c->handler;
    }
    if ((f32)h->maxX < c->screenX) {
        c->opponent->screenX = c->opponent->screenX - (c->screenX - (f32)h->maxX);
    }

    c->screenX = ScaleByFrameTime(c->v480) + c->screenX;
    if (c->handler->v54 >= 4) {
        if (c->seqIndex == 6) {
            if (c->facing == 0) c->screenX = ScaleByFrameTime(c->stats[0]) + c->screenX;
            else                c->screenX = c->screenX - ScaleByFrameTime(c->stats[0]);
        }
        if (c->seqIndex == 8) {
            if (c->facing == 0) c->screenX = ScaleByFrameTime((f32)((f64)c->stats[0] * 3.0)) + c->screenX;
            else                c->screenX = c->screenX - ScaleByFrameTime((f32)((f64)c->stats[0] * 3.0));
        }
        if (c->seqIndex == 7) {
            if (c->facing == 0) c->screenX = c->screenX - ScaleByFrameTime(c->stats[1]);
            else                c->screenX = ScaleByFrameTime(c->stats[1]) + c->screenX;
        }
    }

    airborne = 0;
    if (c->screenY < (f32)c->handler->floorY) {
        c->screenX = ScaleByFrameTime(c->v470) + c->screenX;
        if (c->screenY < (f32)c->handler->floorY) airborne = 1;
    }
    if (airborne) {
        c->v46C = gravity + c->v46C;
        if (5.0 < (f64)c->v46C) c->v46C = 5.0f;
    } else if (!bounced) {
        c->v46C = 0.0f;
    }

    if (c->v478 < 0.0f) {
        c->v478 = friction + c->v478;
        if (0.0f < c->v478) c->v478 = 0.0f;
    }
    if (0.0f < c->v478) {
        c->v478 = c->v478 - friction;
        if (c->v478 < 0.0f) c->v478 = 0.0f;
    }
    if (c->v480 < 0.0f) {
        c->v480 = friction + c->v480;
        if (0.0f < c->v480) c->v480 = 0.0f;
    }
    if (0.0f < c->v480) {
        c->v480 = c->v480 - friction;
        if (c->v480 < 0.0f) c->v480 = 0.0f;
    }
    if (c->v480 == 0.0f) c->v498 = 0.0f;
    c->seq.waitATarget = c->v480;
}

/* ------------------------------------------------------------------------
 * cfCharacter_Update (0x8005E358): the per-frame update of a fighter.
 * Timers (stun v194, v19C, v410, dizziness v2C0/v2BC, knock-down v2B4/v2B8), the queued
 * sequences and moves from the move record (vF68[10..17]), the animation itself, landing
 * from a jump, dizziness, combo links (linkA..E), getting up after a knock-down, turning to
 * face the opponent, and the end-of-round sequences (0x25-0x2B).
 * ---------------------------------------------------------------------- */
void cfCharacter_Update(cfCharacter *c) {
    CombatHandler *h = c->handler;
    cfCharacter *o;
    s32 s, frame, hadAttacks;
    f32 t;

    if (h == NULL) {
        UpdateCharacterAnimation(&c->seq);
        return;
    }
    if ((u32)(h->v54 - 10) < 2) {
        frame = c->seq.frame;
        UpdateCharacterAnimation(&c->seq);
        if (frame != c->seq.frame) cfCharacter_ApplySeqFrame(c);
        return;
    }

    c->v1A0 = (f32)((f64)c->v1A0 + ((f64)c->handler->dt + (f64)c->handler->dt));

    /* combo counters move to the "last combo" slots */
    s = c->seqIndex;
    o = c->opponent;
    if (((D_SeqNeutral[s] != 0 || D_SeqDefaultV178[s] != 0 || (u32)(s - 0x25) < 2 || s == 0x27) &&
         o->v440 != 0 && o->v424 == 0) ||
        c->seqIndex == 0x2A || c->seqIndex == 0x28 || c->seqIndex == 0x25) {
        c->opponent->v448 = c->opponent->v440;
        c->opponent->v44C = c->opponent->v444;
        c->opponent->v440 = 0;
        c->opponent->v444 = 0;
        c->opponent->v45C = 0;
    }
    if (c->v440 == 0) c->v444 = 0;
    if (c->v448 > 0) {
        if (c->v448 < 3) {
            c->v448 = 0;
        } else {
            if (c->v450 == 0) c->v454 = c->v448;
            c->v448 = 0;
        }
        c->v44C = 0;
        c->v450 = 0;
    }

    c->v19C = c->v19C - c->handler->dt;
    if ((f64)c->v19C < 0.0) c->v19C = 0.0f;

    if ((u32)(c->seqIndex - 0x2D) < 2) {
        mpDebugPrintf("health = %d\n", c->health);
        if (c->health <= 0 && c->opponent->v428 == 0) cfCharacter_PlaySequence(c, 0x20, 1, 1, (u32)-1);
    }

    if (c->v194 != 0.0f) {
        t = c->v194 - c->handler->dt;
        c->v194 = t;
        if (t < 0.0f) c->v194 = 0.0f;
    }
    if (0.0f < c->v194) return;            /* stunned */

    if (c->seqIndex == 0) c->v43C = 0;
    if (D_SeqNeutral[c->seqIndex] != 0) {
        if (c->seq.frame >= 2) c->v438++;
        if (D_SeqNeutral[c->seqIndex] != 0 &&
            (c->opponent->seqIndex == 0x2B || (c->v320 != 0 && c->v428 == 0))) {
            c->handler->v3C = c;
            c->handler->v40 = c->opponent;
            c->handler->v54 = 6;
        }
    }

    if ((f64)c->v410 > 0.0) {
        c->v410 = c->v410 - c->handler->dt;
        if ((f64)c->v410 < 0.0) c->v410 = 0.0f;
    }

    /* sequences / moves queued by the move record, by time */
    if (c->vF68[14] > 0 && c->vF68[10] != -1 && c->vF68[14] < c->handler->v2CC0) {
        cfCharacter_PlaySequence(c, c->vF68[10], 1, 1, (u32)-1);
        c->v41C = c->v418;
    }
    if (c->vF68[15] > 0 && c->vF68[12] != -1 && c->handler->v2CC0 < c->vF68[15]) {
        cfCharacter_PlaySequence(c, c->vF68[12], 1, 1, (u32)-1);
        c->v41C = c->v418;
    }
    if (c->vF68[14] > 0 && c->vF68[11] != -1 && c->vF68[14] < c->handler->v2CC0) {
        cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[11]), NULL);
        c->v41C = c->v418;
    }
    if (c->vF68[15] > 0 && c->vF68[13] != -1 && c->handler->v2CC0 < c->vF68[15]) {
        cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[13]), NULL);
        c->v41C = c->v418;
    }
    if (0.0f < c->vFB0) {
        c->vFB0 = c->vFB0 - c->handler->dt;
        if ((f64)c->vFB0 <= 0.0) {
            c->vFB0 = -1.0f;
            if (c->vF68[16] != -1) {
                cfCharacter_PlaySequence(c, c->vF68[16], 1, 1, (u32)-1);
                c->v41C = c->v418;
            }
            if (c->vF68[17] != -1) {
                cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->vF68[17]), NULL);
                c->v41C = c->v418;
            }
        }
    }

    if (c->seq.waitTimer != 0.0f) {
        t = c->seq.waitTimer - c->handler->dt;
        c->seq.waitTimer = t;
        if (t <= 0.0f) {
            c->seq.waitTimer = 0.0f;
            c->v470 = c->v484;
        }
    }

    /* the animation */
    c->seq.ready = ((f32)c->handler->floorY <= c->screenY) ? 1 : 0;
    c->seq.waitBTarget = c->v46C;
    hadAttacks = c->seq.numAttacks != 0;
    frame = c->seq.frame;
    UpdateCharacterAnimation(&c->seq);
    if (frame != c->seq.frame) {
        cfCharacter_ApplySeqFrame(c);
        c->v32C = hadAttacks;
    }

    /* landed after being hit while crouching */
    if (c->v4DC != 0) {
        s = c->seqIndex;
        if (c->v170 == 2 && c->health > 0 && s != 0x25 && s != 0x1F &&
            (u32)(s - 0x20) >= 2 && (u32)(s - 0x26) >= 2) {
            cfCharacter_PlaySequence(c, 0x14, 1, 1, (u32)-1);
        }
        c->v4DC = 0;
    }

    /* dizzy */
    if ((f64)c->v2C0 > 0.0) {
        cfCharacter_ClearMoveState(c);
        s = c->seqIndex;
        if ((u32)s < 2 || (u32)(s - 12) < 2 || (u32)(s - 6) < 2 || s == 8 || s == 4 || s == 5) {
            if (c->seqIndex != 0x27) cfCharacter_PlaySequence(c, 0x27, -1, 1, (u32)-1);
        } else if (c->seqIndex == 0x27) {
            c->v2C0 = c->v2C0 - c->handler->dt;
            if ((f64)c->v2C0 < 0.0) c->v2C0 = 0.0f;
        }
    } else if (c->seqIndex == 0x27) {
        cfCharacter_PlaySequence(c, 0, -1, 1, (u32)-1);
        c->v2BC = 0;
    }
    s = c->v2BC;
    if (s > 0 && !((f64)c->v2C0 > 0.0) && c->seqIndex != 0x27 && (c->handler->frameCount & 3) == 0 && s != 200) {
        c->v2BC--;
    }

    /* combo links: chained sequences */
    if (c->v3EC < c->numLinks) {
        s32 i;
        if (c->seq.seq != NULL) goto landed;
        i = c->v3EC;
        cfCharacter_PlaySequence(c, c->linkA[i], 1, 0, (u32)c->linkC[i]);
        CharSeq_RunUntil(&c->seq, c->linkB[c->v3EC]);
        cfCharacter_ApplySeqFrame(c);
        c->v41C = c->v418;
        if (c->linkD[c->v3EC] != -0x8000) {
            f32 v = (f32)((f64)(f32)c->linkD[c->v3EC] / 100.0);
            if (c->facing != 0) v = -v;
            c->v480 = ((f64)v != 0.0) ? v : 0.0f;
        }
        if (c->linkE[c->v3EC] != -0x8000) {
            c->v3F8 = 1;
            c->v3FC = (f32)((f64)(f32)c->linkE[c->v3EC] / 100.0);
        }
        c->v3EC++;
    }
    if (c->seq.seq == NULL) {
        if (c->v3F4 >= 0) cfCharacter_StartMove(c, cfCharacter_GetRecord(c, c->v3F4), NULL);
        if (c->seq.seq == NULL && c->v418 != 0 && c->v2DC != 0 &&
            (u32)(c->opponent->seqIndex - 0x22) >= 2) {
            c->v410 = 10.0f;
            c->v414 = c->v188;
        }
    }

landed:
    /* a sequence finished on the floor: get up / fall / stand */
    if (c->screenY == (f32)c->handler->floorY && c->seq.seq == NULL) {
        s = c->seqIndex;
        if ((u32)(s - 0x29) < 2) {
            s32 voice = -1;
            cfCharacter_PlaySequence(c, 0x28, -1, 1, (u32)-1);
            c->v2B4 = 9999.0f;
            c->v2BC = 200;
            c->v31C = 1;
            c->v2B8 = 180.0f;
            if (c->def->soundSets[0x18] > 0) {
                s32 r = (s32)guRandom();
                voice = c->def->soundSets[0x19 + r % c->def->soundSets[0x18]];
            }
            if (voice >= 0) func_8006DE6C(c->handler, voice + 100, 0x7FFF, c, 0, 0);
        } else if ((u32)(s - 0x1F) < 2 || s == 0x21 || s == 0x2B || (u32)(s - 0x35) < 2 || s == 0x37) {
            cfCharacter_PlaySequence(c, 0x25, -1, 1, (u32)-1);
        } else if (c->health <= 0 && c->v2C8 >= c->handler->v30) {
            cfCharacter_PlaySequence(c, 0x2A, 1, 1, (u32)-1);
            c->v31C = 1;
            c->v2B4 = 9999.0f;
        } else if (c->v170 == 1) {
            cfCharacter_PlaySequence(c, 1, -1, 1, (u32)-1);
        } else {
            cfCharacter_PlaySequence(c, 0, -1, 1, (u32)-1);
        }
    }

    /* turn round when the opponent has crossed over */
    o = c->opponent;
    if (o != NULL) {
        s32 crossed = (c->facing == 0) ? (o->screenX < c->screenX) : (c->screenX < c->opponent->screenX);
        if (crossed) {
            s = c->seqIndex;
            if (s == 0 || s == 6 || (u32)(s - 7) < 2 || (u32)(s - 0xA) < 2 || s == 1) {
                cfCharacter_TurnAround(c);
                cfCharacter_PlaySequence(c, (c->v170 == 1) ? 5 : 4, 1, 1, (u32)-1);
                c->v34 = 0;
                c->v38 = 0;
            }
        }
    }

    /* lying down after a knock-down */
    if (c->seqIndex == 0x25) {
        if (c->health <= 0 && c->v2C8 < c->handler->v30) c->handler->v54 = 5;
        if (c->seqIndex == 0x25) {
            t = c->v2B4 - c->handler->dt;
            c->v2B4 = t;
            if (t < 0.0f) c->v2B4 = 0.0f;
            if (c->v2B4 <= 0.0f) {
                if (c->health <= 0 && c->v2C8 < c->handler->v30) {
                    c->health = 170;
                    c->v2BC = 0;
                }
                c->v2B4 = 0.0f;
                cfCharacter_PlaySequence(c, 0x26, 1, 1, (u32)-1);
                c->v31C = 0;
                if (c->health <= 0 && c->v2C8 >= c->handler->v30) {
                    cfCharacter_PlaySequence(c, 0x29, 1, 1, (u32)-1);
                    c->v31C = 1;
                    c->v2B4 = 9999.0f;
                }
            }
        }
    }
    if (c->seqIndex == 0x28) {
        t = c->v2B8 - c->handler->dt;
        c->v2B8 = t;
        if (t < 0.0f) c->v2B8 = 0.0f;
        if (c->v2B8 <= 0.0f && c->opponent->v428 == 0) {
            cfCharacter_TurnAround(c);
            cfCharacter_PlaySequence(c, 0x2B, 1, 1, (u32)-1);
            c->v2B4 = 9999.0f;
        }
    }
}

/* arena helpers (still in assembly) */

/* restart a projectile's sequence */
static void ProjSeq(cfCharacter *c, CharProjectile *p, CharSeq *s, s32 index, s32 loops) {
    CharSeq_Start(s, (u8 *)c->def + c->seqOffsets[index], loops, 0xFFFFFFFF);
    p->velX = 0.0f;
    p->velY = 0.0f;
    p->angle = 0.0f;
    p->v18 = 0.0f;
    cfCharacter_ApplyProjectileSeq(c, p);
}

/* ------------------------------------------------------------------------
 * cfCharacter_UpdateProjectiles (0x8005F1D4): moves this fighter's projectiles.
 * Each one animates every other frame; while flying (state 0) it can time out (v50, v64),
 * change sequence by distance (v70/v74), cancel against an opponent's projectile, bounce on
 * the floor (losing speed vA4, with a sound) and bounce off or stop at the arena's walls.
 * A projectile that has hit something (v48) switches to its hit or blocked sequence.
 * ---------------------------------------------------------------------- */
void cfCharacter_UpdateProjectiles(cfCharacter *c) {
    u8 *walls = c->handler->v134 + c->handler->arena->wallsOffset;
    s32 moving = 0;     /* the original leaves this unset for states other than 0-2 */
    s32 i, j;

    for (i = 0; i < 16; i++) {
        CharProjectile *p = &c->projectiles[i];
        CharSeq *s;
        f32 old[3];

        if (!p->active) continue;
        s = &c->projSeqs[p->seqSlot];
        if ((c->handler->frameCount & 1) == 0) {
            s32 frame = s->frame;
            UpdateCharacterAnimation(s);
            if (frame != s->frame) cfCharacter_ApplyProjectileSeq(c, p);
        }
        old[0] = p->posX;
        old[1] = p->posY;
        old[2] = p->posZ;
        cfCharacter_AimProjectile(c, p);

        if (p->v4 == 1) {
            if (s->seq == NULL) {
                cfCharacter_FreeProjectile(c, i);
                continue;
            }
            moving = (p->vC4 == 0);
        } else if (p->v4 == 2) {
            moving = (p->vC4 == 0);
        } else if (p->v4 == 0) {
            if (c->handler->v54 == 5 || c->handler->v54 == 12) {
                if (p->v58 >= 0) {
                    p->v4 = 1;
                    ProjSeq(c, p, s, p->v58, 1);
                } else {
                    s->seq = NULL;
                }
            }
            if (p->v60 >= 0) {
                p->v64 = p->v64 - c->handler->dt;
                if ((f64)p->v64 <= 0.0) {
                    ProjSeq(c, p, s, p->v60, 1);
                    p->v60 = -1;
                }
            }
            if (p->v70 >= 0 && p->targetDist < p->v74) {
                ProjSeq(c, p, s, p->v70, 1);
                p->v70 = -1;
            }
            p->v50 = p->v50 - c->handler->dt;
            if ((f64)p->v50 <= 0.0) {
                p->v50 = 0.0f;
                if (p->v54 >= 0) {
                    p->v4 = 1;
                    ProjSeq(c, p, s, p->v54, 1);
                } else {
                    s->seq = NULL;
                }
            }
            moving = 1;
        }

        /* two flying projectiles close together cancel out */
        if (p->v4 == 0) {
            for (j = 0; j < 16; j++) {
                cfCharacter *o = c->opponent;
                CharProjectile *q = &o->projectiles[j];
                CharSeq *qs;
                f32 dx, dy, dz;

                if (!q->active || q->v4 != 0) continue;
                qs = &o->projSeqs[q->seqSlot];
                dx = q->posX - p->posX;
                dy = q->posY - p->posY;
                dz = q->posZ - p->posZ;
                if (dx * dx + dy * dy + dz * dz <= 6000.0f) {
                    if (p->v58 >= 0) {
                        p->v4 = 1;
                        ProjSeq(c, p, s, p->v58, 1);
                    } else {
                        s->seq = NULL;
                    }
                    if (q->v58 >= 0) {
                        q->v4 = 1;
                        ProjSeq(c->opponent, q, qs, q->v58, 1);
                    } else {
                        qs->seq = NULL;
                    }
                }
            }
        }

        /* it hit something */
        if (p->v48 != 0) {
            s32 idx;
            if (p->vC8 != 0) {
                p->v4 = 1;
                if (p->v54 >= 0) ProjSeq(c, p, s, p->v54, 1);
                else s->seq = NULL;
            }
            idx = p->blocked ? p->v5C : p->v58;
            if (idx < 0) {
                p->v48 = 0;
            } else {
                p->v4 = 1;
                ProjSeq(c, p, s, idx, 1);
                p->v48 = 0;
            }
        }

        if (s->seq == NULL) {
            cfCharacter_FreeProjectile(c, i);
            continue;
        }
        p->angle = p->angle + p->v18;
        cfMatrix2D_SetTranslate(&p->matrix, p->velX, p->velY);
        cfMatrix2D_PreRotate(&p->matrix, (f32)(0.0 - (f64)p->angle));
        cfMatrix2D_PreTranslate(&p->matrix, (f32)(0.0 - (f64)p->velX), (f32)(0.0 - (f64)p->velY));
        if (!moving) continue;

        {
            f32 sx = ScaleByFrameTime(p->v98);
            f32 sy = ScaleByFrameTime(p->v9C);
            p->posX = sx * p->dirX + p->posX;
            p->posY = sy + p->posY;
            p->posZ = sx * p->dirZ + p->posZ;
        }
        if (p->v94 == 0) p->v9C = p->v9C - p->vA0;

        if (p->v90 != 0) {
            /* falling out of the arena */
            if ((f64)p->posY < -150.0) {
                cfCharacter_FreeProjectile(c, i);
                continue;
            }
        } else if (p->posY < 0.0f) {
            /* bounce on the floor */
            f32 vol = (f32)((f64)p->v9C / -15.0);
            f32 t;
            if (1.0 < (f64)vol) vol = 1.0f;
            if ((f64)vol < 0.0) vol = 0.0f;
            p->v9C = (f32)((f64)p->v9C * (0.0 - (f64)p->vA4));
            p->posY = 0.0f;
            t = p->v9C;
            if (t < p->vA0 || t < p->vA4 || (p->vAC < t && (f64)p->vA4 < 1.0)) {
                p->v9C = 0.0f;
                p->v94 = 1;
            }
            p->vAC = p->v9C;
            if (p->vB8 >= 0) {
                s32 snd = cfCharacter_PickSound(c, p->vB8);
                if (snd >= 0) func_8006DE6C(c->handler, snd, (s32)(vol * 32767.0f), c, 0, 1);
            }
            if (p->v4 == 0 && p->v6C >= 0) ProjSeq(c, p, s, p->v6C, 1);
        }

        if (p->v94 != 0) {
            /* rolling along the floor */
            p->v98 = p->v98 * p->vA8;
            if ((f64)p->v98 < 0.05 && -0.05 < (f64)p->v98) p->v98 = 0.0f;
            if (p->vC0 != 0) {
                p->angle = 0.0f;
                p->v18 = 0.0f;
            }
        }

        if (old[0] == p->posX && old[1] == p->posY && old[2] == p->posZ) continue;

        /* the arena's walls */
        {
            f32 seg[4], hit[2];
            seg[0] = old[0];
            seg[1] = old[2];
            seg[2] = p->posX;
            seg[3] = p->posZ;
            for (j = 0; j < c->handler->arena->numWalls; j++) {
                f32 *w = (f32 *)(walls + j * 0x24);
                s32 r = cfSegmentsIntersect(seg, w, hit);
                f32 d[2], len, k;

                if (r == 0 || r == 2) continue;
                if (*((s8 *)w + 0x20) == 0) {
                    /* an edge: fall off */
                    p->v90 = 1;
                    p->v94 = 0;
                    continue;
                }
                if (p->vBC == 0) {
                    /* stops at the wall */
                    p->v98 = 0.0f;
                    if (p->v4 == 0 && p->v68 >= 0) {
                        ProjSeq(c, p, s, p->v68, (p->vBC != 0) ? -1 : 1);
                        continue;
                    }
                    if (p->vBC == 0) cfCharacter_FreeProjectile(c, i);
                    continue;
                }
                /* bounces off: reflect the direction about the wall's normal */
                d[0] = p->posX - old[0];
                d[1] = p->posZ - old[2];
                len = cfVec2_Length(d);
                d[0] = d[0] / len;
                d[1] = d[1] / len;
                k = d[0] * w[6] + d[1] * w[7];
                if ((f64)k < 0.0) k = (f32)(0.0 - (f64)k);
                else k = d[0] * w[6] + d[1] * w[7];
                d[0] = (f32)((f64)(k * w[6]) + (f64)d[0] * (1.0 - (f64)k));
                d[1] = (f32)((f64)(k * w[7]) + (f64)d[1] * (1.0 - (f64)k));
                p->dirX = len * d[0] * p->vA4;
                p->dirZ = len * d[1] * p->vA4;
                cfVec2_Normalise(&p->dirX);
                p->v98 = p->v98 * p->vA4;
                p->posX = hit[0];
                p->posZ = hit[1];
                p->posX = (f32)((f64)p->posX + (f64)w[6] * 0.01);
                p->posZ = (f32)((f64)p->posZ + (f64)w[7] * 0.01);
                break;
            }
        }
    }
}

/* take / give back the fight's input lock (as ProcessInput does) */
static void LockInput(cfCharacter *c) {
    MPF_Mutex *m = &c->handler->mutex;
    if (m->owner != osGetThreadId(NULL)) {
        osRecvMesg(&c->handler->mutex.mq, NULL, OS_MESG_BLOCK);
        c->handler->mutex.owner = osGetThreadId(NULL);
    }
    c->handler->mutex.count++;
}
static void UnlockInput(cfCharacter *c) {
    MPF_Mutex *m = &c->handler->mutex;
    if (m->owner == osGetThreadId(NULL)) {
        if (--c->handler->mutex.count == 0) {
            c->handler->mutex.owner = 0;
            osSendMesg(&c->handler->mutex.mq, NULL, OS_MESG_NOBLOCK);
        }
    }
}

/* ------------------------------------------------------------------------
 * cfCharacter_HandleInput (0x80060CAC): turns this frame's controls into actions.
 * Command bits (cfCharacter_ButtonsToCommand): 1 up, 2 down, 4 towards, 8 away,
 * 0x10 / 0x20 into / out of the screen.
 *   On the floor: stand / crouch, walk, dash (towards twice), walk back (blocks), jumps
 *   (straight, forwards, backwards, with a side-step component).
 *   Then the queue of recognised special moves (a2c, newest first): the first one whose
 *   conditions all hold (stance, direction, timing, meter, round state, combo rules...)
 *   is started and the rest of the queue is dropped.
 * ---------------------------------------------------------------------- */
void cfCharacter_HandleInput(cfCharacter *c, u32 buttons, u32 buttons2) {
    cfCharacter *o;
    u32 cmd;
    f32 dist, d;
    s32 s, stance, idx, n;

    if (c->v2BC >= 200) return;            /* dizzy */
    c->v34 = 0;
    c->v38 = 0;
    if (c->v4D8 != 0) return;

    o = c->opponent;
    d = c->screenX - o->screenX;
    if ((f64)d < 0.0) dist = (f32)(0.0 - (f64)d);
    else dist = c->screenX - o->screenX;

    if (c->v260 != 0) {
        cmd = c->aiCommand;             /* computer player */
    } else {
        cmd = cfCharacter_ButtonsToCommand(c, buttons);
        cfCharacter_ButtonsToCommand(c, buttons2);
    }

    if (c->screenY == (f32)c->handler->floorY) {
        /* blocking (seq 12/13): let go when nothing is coming */
        if ((u32)(c->seqIndex - 12) < 2 && c->v478 == 0.0f && (f64)c->v19C == 0.0) {
            s32 threat = 0;
            if (cmd & 8) {
                if (((c->opponent->seq.flags >> 4) & 1) && dist < 370.0f) {
                    threat = 1;
                } else {
                    s32 j;
                    o = c->opponent;
                    for (j = 0; j < 16; j++) {
                        CharProjectile *q = &o->projectiles[j];
                        f32 lim;
                        if (!q->active || q->v4 != 0) continue;
                        lim = (q->targetDot <= 0.0f ? 1.0f : 0.25f) * 370.0f;
                        if (q->targetDist < lim) {
                            threat = 1;
                            break;
                        }
                    }
                }
            }
            if (!threat) cfCharacter_PlaySequence(c, (cmd & 2) ? 1 : 0, -1, 1, (u32)-1);
        }

        /* crouch / stand */
        if (cmd & 2) {
            s = c->seqIndex;
            if (s == 0 || s == 6 || (u32)(s - 7) < 2 || (u32)(s - 0xA) < 2)
                cfCharacter_PlaySequence(c, 2, 1, 1, (u32)-1);
        } else if (c->seqIndex == 1) {
            cfCharacter_PlaySequence(c, 3, 1, 1, (u32)-1);
        }
        if (c->seqIndex == 2 && !(cmd & 2)) cfCharacter_PlaySequence(c, 0, -1, 1, (u32)-1);
        if (c->seqIndex == 3 && (cmd & 2))  cfCharacter_PlaySequence(c, 1, -1, 1, (u32)-1);

        /* walk / dash towards */
        if (cmd & 4) {
            s = c->seqIndex;
            if (s == 0 || s == 7 || (u32)(s - 0xA) < 2) {
                s32 head = c->inputHead;
                u32 now = c->handler->time;
                s32 seqN = 6;
                if (c->v260 != 0) {
                    if (c->v2A8 != 0) seqN = 8;
                } else {
                    LockInput(c);
                    if (cfCharacter_MatchInput(c, 4, 0, &head, &now, 12, 0)) {
                        head--;
                        if (head < 0) head = 111;
                        if (cfCharacter_MatchInput(c, 4, 0, &head, &now, 12, 0)) seqN = 8;
                    }
                    UnlockInput(c);
                }
                cfCharacter_PlaySequence(c, seqN, -1, 1, (u32)-1);
            }
        }
        /* walk away */
        if (cmd & 8) {
            s = c->seqIndex;
            if (s == 0 || s == 6 || s == 8 || s == 10 || s == 11)
                cfCharacter_PlaySequence(c, 7, -1, 1, (u32)-1);
        }
        s = c->seqIndex;
        if ((s == 6 || s == 8) && !(cmd & 4)) cfCharacter_PlaySequence(c, 0, -1, 1, (u32)-1);
        if (c->seqIndex == 7 && !(cmd & 8))   cfCharacter_PlaySequence(c, 0, -1, 1, (u32)-1);
        if ((u32)(c->seqIndex - 0xA) < 2 && !(cmd & 0x30)) cfCharacter_PlaySequence(c, 0, -1, 1, (u32)-1);

        /* jump */
        if (cmd & 1) {
            s32 ok = 0;
            s = c->seqIndex;
            if (s == 0 || s == 6 || (u32)(s - 7) < 2 || (u32)(s - 0xA) < 2 || (u32)(s - 3) < 2 || s == 2) {
                ok = 1;
            } else if (s == 12 && (f64)c->v478 == 0.0 && (f64)c->v19C == 0.0) {
                ok = 1;
            }
            if (ok) {
                c->v470 = 0.0f;
                c->v474 = 0.0f;
                if (cmd & 8) {
                    f32 t;
                    cfCharacter_PlaySequence(c, 0x13, 1, 1, (u32)-1);
                    t = 0.0f - c->stats[4];
                    c->v470 = t;
                    if (c->facing != 0) c->v470 = 0.0f - t;
                } else if (cmd & 4) {
                    f32 t;
                    cfCharacter_PlaySequence(c, 0x12, 1, 1, (u32)-1);
                    t = c->stats[4];
                    c->v470 = t;
                    if (c->facing != 0) c->v470 = 0.0f - t;
                } else {
                    cfCharacter_PlaySequence(c, 0x11, 1, 1, (u32)-1);
                }
                c->v4D8 = 1;
                if (cmd & 0x10)      c->v474 = 0.0f - c->stats[4];
                else if (cmd & 0x20) c->v474 = c->stats[4];
                return;
            }
        }
    }

    /* the queue of recognised special moves */
    stance = ((u32)c->v170 < 2) ? (s32)((cmd >> 1) & 1) : c->v170;
    idx = c->vA28;
    for (n = 0; n < 64; n++, idx--) {
        CharA2C *e;
        u8 *mv, *var;
        s32 breaksCombo = 0, canCancel = 0, k, used;
        u32 b;

        if (idx < 0) idx = 63;
        e = &c->a2c[idx];
        if (e->timer <= 0) continue;
        e->timer--;
        mv = e->move;
        var = e->variant;

        if ((s8)mv[0xF] != 0) {
            if (c->handler->v74 == 0) continue;
            if (c->opponent->seqIndex < 0x3F && D_SeqDefaultV178[c->seqIndex] != 0) continue;
            o = c->opponent;
            if (o->v178 != 0) continue;
            if (o->seqIndex >= 0x15 && o->seqIndex >= 0x24) continue;   /* (sic) */
            if ((f64)o->v19C > 0.0) continue;
            if (c->v258 != 0) continue;
        }

        if ((s8)mv[0xE] >= 0) {
            /* a combo breaker: only while being hit by the given combo */
            s32 head;
            u32 now;
            o = c->opponent;
            if (o->v42C == 0) continue;
            if (o->v430 != (s8)mv[0xE]) continue;
            if (c->health <= 0) continue;
            if ((u32)(c->seqIndex - 0x15) >= 9) continue;
            head = c->inputHead;
            now = c->handler->time;
            if (cfCharacter_MatchInput(c, ((~*(u16 *)var) & 0x1FF0) | 0x8000, 0, &head, &now, 16, 1)) {
                e->timer = 0;
                continue;
            }
            breaksCombo = 1;
        } else {
            if (mv[6] == 1) {
                if (!(c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0)) {
                    if (c->vF38 != 0) continue;
                    if (c->seqIndex >= 0x3F && c->vF50 > 0) continue;
                    canCancel = 1;
                    if (c->v32C == 0 && c->v2D4 == 0) continue;
                }
            } else {
                if (!(c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0)) {
                    if (c->vF38 != 0) continue;
                    if (mv[6] == 0) continue;
                    if (c->seqIndex >= 0x3F && c->vF50 > 0) continue;
                    if (c->v32C == 0 && c->v2D4 == 0) continue;
                    if (c->screenY < (f32)c->handler->floorY) continue;
                    canCancel = 1;
                }
                if (mv[6] == 2 && c->v2CC < 100) continue;                      /* needs the meter */
                if (mv[6] == 3 && c->opponent->seqIndex != 0x28) continue;      /* opponent dizzy */
            }
            if (c->v478 != 0.0f) continue;
        }

        /* time window */
        if (*(s16 *)(mv + 0x1A) != -1 && c->handler->v2CC0 < *(s16 *)(mv + 0x1A)) {
            e->timer = 0;
            continue;
        }
        if (*(s16 *)(mv + 0x1C) != -1 && *(s16 *)(mv + 0x1C) < c->handler->v2CC0) {
            e->timer = 0;
            continue;
        }

        /* stance: 0 standing, 1 crouching, 2 in the air */
        if ((s8)mv[0x12] <= 0) continue;
        for (k = 0; k < (s8)mv[0x12]; k++) {
            u8 st = mv[0x13 + k];
            if (st == stance) break;
            if (st == 2 && c->screenY < (f32)(c->handler->floorY - 10)) break;
        }
        if (k >= (s8)mv[0x12]) continue;

        if ((s8)mv[0xB] >= 0) {
            if ((f64)c->v410 <= 0.0) continue;
            if (c->v414 != (s8)mv[0xB]) continue;
        }
        if ((s8)mv[0xA] != 0) {
            /* finishing move: last round, opponent low on health and still standing */
            o = c->opponent;
            if (o->v2C8 < c->handler->v30 - 1) continue;
            if (o->health >= 41) continue;
            if (o->seqIndex == 0x28 || o->seqIndex == 0x2B || o->seqIndex == 0x2A) continue;
            if (c->v440 <= 0) continue;
        }
        if ((s8)mv[0xC] != 0) {
            f32 floor = (f32)c->handler->floorY;
            if (c->screenY < floor) continue;
            if (c->opponent->screenY < floor) continue;
        }
        if (*(s16 *)(mv + 0x1E) != 0 && c->v1A0 < (f32)*(s16 *)(mv + 0x1E)) continue;
        if ((s8)mv[9] != 0) {
            if (!canCancel) continue;
            if (c->v42C == 0) continue;
            if (c->v45C >= 3) continue;
        }

        /* in the air: direction of travel (1 forwards, 2 backwards, 0 none) */
        if ((s8)mv[0x16] > 0 && mv[0x13] == 2) {
            u32 dir = 0;
            if (c->v470 != 0.0f) {
                if (0.0f < c->v470) dir = c->facing ? 2 : 1;
                else                dir = c->facing ? 1 : 2;
            }
            for (k = 0; k < (s8)mv[0x16]; k++) {
                if (mv[0x17 + k] == dir) break;
            }
            if (k >= (s8)mv[0x16]) continue;
        }

        /* use up the motion's inputs */
        used = 0;
        b = (u32)e->inputs & 0xFF;
        while (b != 0xFF) {
            if (c->inputs[b].command == 0) {
                e->timer = 0;
                used = 1;
                break;
            }
            c->inputs[b].command = 0;
            e->inputs = (s32)(((u32)e->inputs >> 8) | 0xFF000000);
            b = (u32)e->inputs & 0xFF;
        }
        if (used) continue;
        if (e->holdIndex != 0xFF) c->inputs[e->holdIndex].buttons &= ~e->holdButtons;

        /* face the opponent and go */
        if ((f32)c->handler->floorY <= c->screenY) {
            if (c->screenX < c->opponent->screenX) {
                if (c->facing != 0) cfCharacter_TurnAround(c);
            } else {
                if (c->facing == 0) cfCharacter_TurnAround(c);
            }
        }
        c->vF2C++;
        cfCharacter_StartMove(c, mv, var);
        c->v444++;
        if (breaksCombo) {
            c->opponent->v440 = 0;
            c->opponent->v444 = 0;
            c->seq.flag20 = 1;
            c->v458 = 1;
        }
        if (canCancel && var[0xD] >= 2) {
            CharSeq_RunUntil(&c->seq, var[0xD]);
            cfCharacter_ApplySeqFrame(c);
        }
        if ((s8)mv[9] != 0)    c->v45C++;
        if ((s8)mv[0xF] != 0)  c->v450 = 1;
        if ((s8)mv[0x10] != 0) c->handler->vBC = 1;
        if (c->opponent->v198 != 0) c->opponent->v178 = 1;
        if (mv[6] == 1) {
            c->v2CC += 16;
            if (c->v2CC > 300) c->v2CC = 300;
        }
        if (c->v434 != 0) c->v2CC += 40;
        if (c->v420 != 0) c->v2CC -= 100;

        /* drop the rest of the queue */
        for (; n < 64; n++) {
            c->a2c[idx].timer = 0;
            idx--;
            if (idx < 0) idx = 63;
        }
        break;
    }

    s = c->seqIndex;
    if ((s == 0 || s == 6 || (u32)(s - 7) < 2 || (u32)(s - 0xA) < 2) && (cmd & 0x30)) {
        if (buttons & 0x20) c->v34 = 1;
        else c->v38 = 1;
    }
}
