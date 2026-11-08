/*
 * CF3COMBT.C (0x8006B5A0-0x80074ED0) and the fighter helpers just before it
 * (0x800697BC-0x8006B5A0, the computer player): the combat handler that runs a fight.
 * Decompiled a few functions at a time; the rest is still assembly (see docs/FILEMAP.md).
 */
#include "cf3char.h"
#include "mpfplat.h"

/* a point on the arena's path (handler->v134 + arena->pathOffset) */
typedef struct PathPoint {
    f32 x, z;           /* position at distance t */
    f32 t;              /* distance along the path */
    f32 dx, dz;         /* change per unit of distance up to the next point */
} PathPoint;

/* ---------------------------------------------------------------- the computer player */

/* 0x8006B3A0: picks one of up to n moves at random from list[] (move numbers, 0xFF ends the
 * list early; the variant numbers follow at list[n]). Returns 0 if the list is empty. */
s32 cfAI_PickMove(cfCharacter *c, u8 **move, u8 **variant, u8 *list, s32 n) {
    u8 *vars = list + n;
    s32 count = 0, r, j;
    u8 *m;

    if (n > 0) {
        while (list[count] != 0xFF) {
            count++;
            if (count >= n) break;
        }
    }
    if (count <= 0) return 0;
    r = (s32)guRandom() % count;
    m = c->moves;
    for (j = 0; j < list[r]; j++) m += *(s32 *)m;
    *variant = m + *(u16 *)(m + 0x2E) + vars[r] * 0x24;
    *move = m;
    return 1;
}

/* 0x8006B4C4: queues a move as if its motion had just been typed in */
void cfAI_QueueMove(cfCharacter *c, u8 *move, u8 *variant) {
    if (++c->vA28 >= 64) c->vA28 = 0;
    c->a2c[c->vA28].move = move;
    c->a2c[c->vA28].variant = variant;
    c->a2c[c->vA28].timer = 5;
    c->a2c[c->vA28].inputs = -1;
    c->a2c[c->vA28].holdIndex = 0xFF;
    c->a2c[c->vA28].holdButtons = 0;
    if (move[0x30] == 1) c->aiCommand |= 2;     /* a crouching move: hold down */
}

/* 0x8006B350: picks and queues a random move from a list */
s32 cfAI_TryMove(cfCharacter *c, u8 *list, s32 n) {
    u8 *move, *variant;
    if (!cfAI_PickMove(c, &move, &variant, list, n)) return 0;
    cfAI_QueueMove(c, move, variant);
    return 1;
}

/* ---------------------------------------------------------------- set-up */

/* 0x8006B65C */
void cfCombatHandler_Init(CombatHandler *h) {
    mpMutex_Init(&h->mutex);
    h->fighter1 = NULL;
    h->fighter2 = NULL;
    h->v134 = NULL;
    h->numSounds = 0;
    h->dt = 1.0f;
    h->gravity = 0.1f;
    h->friction = 1.0f;
    h->floorY = 0;
    h->minX = -9999;
    h->maxX = 9999;
    h->frameCount = 0;
    h->v2CC0 = 0;
    h->v2CC4 = 0.0f;
    h->v2CC8 = 0.0f;
    h->v2CCC = 0;
}

/* 0x8006B6E4: does nothing */
void func_8006B6E4(void) {
}

/* 0x8006B5A0 / 0x8006B5BC / 0x8006B5F0: small wrappers */


void func_8006B5A0(CombatHandler *h) {
    Fx_UpdateSmoke(h);
}

void func_8006B5BC(CombatHandler *h) {
    func_800720B0(h);
    func_800721C4(h);
    func_800729EC(h);
}

void func_8006B5F0(CombatHandler *h) {
    func_800720B0(h);
    h->fighter1->v24[0] = h->fighter1->v18[0];
    h->fighter1->v24[1] = h->fighter1->v18[1];
    h->fighter1->v24[2] = h->fighter1->v18[2];
    h->fighter2->v24[0] = h->fighter2->v18[0];
    h->fighter2->v24[1] = h->fighter2->v18[1];
    h->fighter2->v24[2] = h->fighter2->v18[2];
}

/* 0x8006C940: remembers where both fighters are on screen */
void func_8006C940(CombatHandler *h) {
    h->v20 = h->fighter1->screenX;
    h->v24 = h->fighter1->screenY;
    h->v28 = h->fighter2->screenX;
    h->v2C = h->fighter2->screenY;
    h->v1C = 1;
}

/* 0x8006CABC: which fighter is on the left (1 or 2) */
void func_8006CABC(CombatHandler *h) {
    h->v18 = (h->fighter1->screenX < h->fighter2->screenX) ? 1 : 2;
}

/* 0x8006E1C4 */
void func_8006E1C4(CombatHandler *h) {
    if (h->v3C->v260 == 0)      h->v2CB4 = 1;
    else if (h->v40->v260 == 0) h->v2CB8 = 1;
    else                        h->v2CB4 = 1;
}

/* ---------------------------------------------------------------- input and sound */

/* 0x8006DD6C: this frame's controller state for both fighters */
void cfCombatHandler_ReadInput(CombatHandler *h, u32 pad1, u32 pad2) {
    if (h->mutex.owner != osGetThreadId(NULL)) {
        osRecvMesg(&h->mutex.mq, NULL, OS_MESG_BLOCK);
        h->mutex.owner = osGetThreadId(NULL);
    }
    h->mutex.count++;
    h->time++;
    h->pressed1 |= (pad1 ^ h->pad1) & pad1;
    h->pad1 = pad1;
    h->pressed2 |= (pad2 ^ h->pad2) & pad2;
    h->pad2 = pad2;
    cfCharacter_ProcessInput(h->fighter1, h->pad1);
    cfCharacter_ProcessInput(h->fighter2, h->pad2);
    if (h->mutex.owner == osGetThreadId(NULL)) {
        if (--h->mutex.count == 0) {
            h->mutex.owner = 0;
            osSendMesg(&h->mutex.mq, NULL, OS_MESG_NOBLOCK);
        }
    }
}

/* 0x8006DE6C: queues a sound (up to 16 a frame). Numbers from 100 up are the fighter's own
 * voice samples (number - 100). */
void func_8006DE6C(CombatHandler *h, s32 sound, s32 volume, cfCharacter *c, const f32 *pos, s32 a5) {
    s32 side = 0;
    SoundReq *q;

    if (h->numSounds >= 16) return;
    if (c != NULL && c->v25C != 0) return;
    if ((u32)sound >= 100) {
        if (sound >= 100) sound -= 100;
        side = (c == h->fighter1) ? 1 : 2;
    }
    h->sounds[h->numSounds].sound = sound;
    h->sounds[h->numSounds].side = side;
    h->sounds[h->numSounds].volume = volume;
    h->sounds[h->numSounds].vC = 0;
    q = &h->sounds[h->numSounds];
    q->hasPos = (pos != NULL);
    if (pos != NULL) {
        h->sounds[h->numSounds].pos[0] = pos[0];
        h->sounds[h->numSounds].pos[1] = pos[1];
        h->sounds[h->numSounds].pos[2] = pos[2];
    }
    h->sounds[h->numSounds].v20 = a5;
    h->numSounds++;
}

/* ---------------------------------------------------------------- effects */

/* 0x8006DFBC: a free burst, or NULL */
FxRing *Fx_NewBurst(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 16; i++) {
        FxRing *r = &h->bursts[i];
        if (r->active) continue;
        r->active = 1;
        r->pos[0] = 0.0f; r->pos[1] = 0.0f; r->pos[2] = 0.0f;
        r->colA[0] = 255; r->colA[1] = 255; r->colA[2] = 255; r->colA[3] = 255;
        r->colB[0] = 255; r->colB[1] = 255; r->colB[2] = 255; r->colB[3] = 255;
        r->v18 = 10.0f;
        r->age = 0.0f;
        r->ageStep = 0.01f;
        r->v24 = 10.0f;
        r->v28 = 0.0f;
        return r;
    }
    return NULL;
}

/* 0x8006E068: a free glow, or NULL */
FxGlow *Fx_NewGlow(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 16; i++) {
        FxGlow *g = &h->glows[i];
        if (g->active) continue;
        g->active = 1;
        g->pos[0] = 0.0f; g->pos[1] = 0.0f; g->pos[2] = 0.0f;
        g->colA[0] = 255; g->colA[1] = 255; g->colA[2] = 255; g->colA[3] = 255;
        g->colB[0] = 255; g->colB[1] = 255; g->colB[2] = 255; g->colB[3] = 255;
        g->v18 = 10.0f;
        g->v1C = 0.0f;
        g->v20 = 0.0f;
        g->v24 = 0.01f;
        return g;
    }
    return NULL;
}

/* 0x8006E108: a free shock-wave ring, or NULL */
FxRing *Fx_NewRing(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 8; i++) {
        FxRing *r = &h->rings[i];
        if (r->active) continue;
        r->active = 1;
        r->pos[0] = 0.0f; r->pos[1] = 0.0f; r->pos[2] = 0.0f;
        r->colA[0] = 255; r->colA[1] = 255; r->colA[2] = 255; r->colA[3] = 150;
        r->colB[0] = 255; r->colB[1] = 255; r->colB[2] = 255; r->colB[3] = 35;
        r->v18 = 15.0f;
        r->age = 0.0f;
        r->ageStep = 0.07f;
        r->v24 = 10.0f;
        r->v28 = 10.0f;
        return r;
    }
    return NULL;
}

/* 0x80071B9C: hit marks blink 5 times, 3 frames each */
void Fx_UpdateMarks(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 4; i++) {
        FxMark *m = &h->marks[i];
        if (!m->active) continue;
        if (--m->v0C > 0) continue;
        if (++m->age >= 5) m->active = 0;
        else h->marks[i].v0C = 3;
    }
}

/* 0x80071C18 */
void Fx_UpdateSparks(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 48; i++) {
        FxSpark *s = &h->sparks[i];
        if (!s->active) continue;
        s->age = (f32)((f64)s->age + 0.14);
        if (1.0 < (f64)s->age) s->active = 0;
    }
}

/* 0x80071C8C: dust grows in 9 steps */
void Fx_UpdateDust(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 6; i++) {
        FxDust *d = &h->dust[i];
        f32 t;
        if (!d->active) continue;
        t = d->size2 - h->dt;
        d->size2 = t;
        if (t <= 0.0f) {
            if (++d->age >= 9) d->active = 0;
            else h->dust[i].size2 = h->dust[i].size2 + h->dust[i].sizeB;
        }
    }
}

/* 0x80071D24 */
void Fx_UpdateDebris(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 32; i++) {
        FxDebris *d = &h->debris[i];
        f32 t;
        if (!d->active) continue;
        t = d->v4 - h->dt;
        d->v4 = t;
        if (t <= 0.0f) d->active = 0;
    }
}

/* 0x80071D84 */
void Fx_UpdateBursts(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 16; i++) {
        FxRing *r = &h->bursts[i];
        if (!r->active) continue;
        r->v24 = r->v24 + r->v28;
        r->age = r->age + r->ageStep;
        if (1.0 < (f64)r->age) r->active = 0;
    }
}

/* 0x80071E04 */
void Fx_UpdateGlows(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 16; i++) {
        FxGlow *g = &h->glows[i];
        if (!g->active) continue;
        g->v18 = g->v18 + g->v1C;
        g->v20 = g->v20 + g->v24;
        if (1.0 < (f64)g->v20) g->active = 0;
    }
}

/* 0x80071E7C */
void Fx_UpdateRings(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 8; i++) {
        FxRing *r = &h->rings[i];
        if (!r->active) continue;
        r->v24 = r->v24 + r->v28;
        r->age = r->age + r->ageStep;
        if (1.0 < (f64)r->age) r->active = 0;
    }
}

/* 0x80071EFC */
void Fx_UpdateFlashes(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 4; i++) {
        FxFlash *f = &h->flashes[i];
        if (!f->active) continue;
        f->age = f->age + f->v14;
        if (1.0 <= (f64)f->age) f->active = 0;
    }
}

/* 0x80071F64: the screen flash after a big impact (v2C70 runs 0 -> 1 over 36 frames) */
void Fx_UpdateImpactFlash(CombatHandler *h) {
    f64 t = (f64)h->v2C70;
    if (!(t < 1.0)) return;
    h->v2C70 = (f32)(t + 1.0 / 36.0);
    if (1.0 < (f64)h->v2C70) h->v2C70 = 1.0f;
    t = (f64)h->v2C70;
    if (t < 0.5) {
        h->v2C74 = (f32)(1.0 - (t + t));
    } else {
        f32 v = (f32)(((f64)h->v2C70 - 0.5) + ((f64)h->v2C70 - 0.5));
        v = v * v;
        h->v2C74 = v * v;
    }
}

/* 0x80072028 */
void Fx_UpdateFade(CombatHandler *h) {
    if (h->v2C84 == 0) return;
    if ((f64)h->v2C88 < 1.0) {
        h->v2C88 = (f32)((f64)h->v2C88 + 0.01);
        if (1.0 < (f64)h->v2C88) h->v2C88 = 1.0f;
    }
    h->v2C8C = (f32)(1.0 - (f64)h->v2C88);
}

/* 0x800704AC: a fighter blocking a hit shakes from side to side */
void func_800704AC(CombatHandler *h) {
    cfCharacter *f;
    if (!(0.0f < h->v2CC4)) return;
    f = h->fighter1;
    if ((u32)(f->seqIndex - 12) < 2) {
        if (h->frameCount & 1) f->screenX = f->screenX + 1.0f;
        else h->fighter1->screenX = h->fighter1->screenX - 1.0f;
    }
    f = h->fighter2;
    if ((u32)(f->seqIndex - 12) < 2) {
        if (h->frameCount & 1) f->screenX = f->screenX + 1.0f;
        else h->fighter2->screenX = h->fighter2->screenX - 1.0f;
    }
}

/* ---------------------------------------------------------------- the arena */

/* 0x80074D40: the point at distance t along the arena's path (wrapping round) */
void cfCombatHandler_PathPoint(CombatHandler *h, f32 t, f32 *out) {
    PathPoint *pts;
    s32 i;

    if ((f64)t < 0.0) {
        f32 len = h->arena->pathLength;
        do {
            t = t + len;
        } while ((f64)t < 0.0);
    }
    if (h->arena->pathLength < t) {
        f32 len = h->arena->pathLength;
        do {
            t = t - len;
        } while (len < t);
    }
    pts = (PathPoint *)(h->v134 + h->arena->pathOffset);
    for (i = 0; i < h->arena->numPath - 1; i++) {
        PathPoint *p = &pts[i];
        if (p->t <= t && t <= pts[i + 1].t) {
            out[0] = (t - p->t) * p->dx + p->x;
            out[1] = (t - p->t) * p->dz + p->z;
            return;
        }
    }
    mpFatalError("Bad path index");
}

/* 0x8006B1B0: the computer player picks a new behaviour (not the same one again) and how
 * long to keep it up */
void cfAI_NewState(cfCharacter *c) {
    s32 old = c->aiState;
    s32 t;

    if (c->def->numAIStates == 0) {
        c->aiState = 0;
    } else {
        u8 *p;
        do {
            p = c->def->aiStates;
            if (c->def->numAIStates != 0) p += (s32)guRandom() % c->def->numAIStates;
            c->aiState = *p;
        } while (*p == old);
    }
    if (c->aiState == 4)      t = (s32)guRandom() % 50 + 20;
    else if (c->aiState == 3) t = (s32)guRandom() % 300 + 150;
    else                      t = (s32)guRandom() % 600 + 300;
    c->aiTimer = (f32)t;
    c->aiIdleTime = 0.0f;
}

/* ------------------------------------------------------------ computer player */

/* chances out of 1000, one per difficulty level (cfCharacter.v264, 0-4) */
extern s32 D_800B0810[5];   /* attack when it may act */
extern s32 D_800B0824[5];   /* start blocking */
extern s32 D_800B0838[5];   /* when hit: drop the guard for a while */
extern s32 D_800B084C[5];   /* chain another move onto its own */
extern s32 D_800B0888[5];   /* break out of the opponent's chain */
extern s32 D_800B089C[5];   /* final round: combo ender on a weak opponent */
extern s32 D_800B08B0[5];   /* go for a finishing move */
extern s32 D_800B08C4[5];   /* random buttons in sequence 0x27 */
extern s32 D_SeqNeutral[63];

#define AI_CHANCE(t) ((s32)guRandom() % 1000 < (t)[c->v264])

/* 0x800697BC: the computer player's thinking for one frame. It sorts the situation into a
 * distance class (0 = close .. 3 = far) and a situation code (own jump direction and the
 * opponent's state), lets its current behaviour (aiState) decide whether it may act and
 * which way to walk (aiCommand), blocks moves and projectiles coming at it, and picks
 * attacks from the CharDef's lists. */
void cfAI_Think(cfCharacter *c) {
    cfCharacter *o = c->opponent;
    CombatHandler *h = c->handler;
    u8 *oMove = NULL;           /* the move the opponent is doing */
    u8 *myMove = NULL;          /* the move this fighter is doing */
    s32 blocking = 0;
    s32 act = 0;                /* the behaviour allows an attack now */
    s32 gap, range = 0, situ, air, fwd, back, oAir, oFwd, oBack, wasBlocking, finish, i;
    f32 v[2], dist, dx, d;

    v[0] = c->v18[0] - o->v18[0];
    v[1] = c->v18[2] - o->v18[2];
    dist = cfVec2_Length(v);
    d = o->screenX - c->screenX;
    if ((f64)d < 0.0) dx = (f32)(0.0 - (f64)d);
    else              dx = o->screenX - c->screenX;

    air = c->screenY < (f32)c->handler->floorY;
    if (air) {
        if (c->facing != 0) { fwd = (f64)c->v470 < 0.0; back = 0.0 < (f64)c->v470; }
        else                { fwd = 0.0 < (f64)c->v470; back = (f64)c->v470 < 0.0; }
    } else {
        fwd = back = 0;
    }
    oAir = o->screenY < (f32)c->handler->floorY;
    if (oAir) {
        if (o->facing != 0) { oFwd = (f64)o->v470 < 0.0; oBack = 0.0 < (f64)o->v470; }
        else                { oFwd = 0.0 < (f64)o->v470; oBack = (f64)o->v470 < 0.0; }
    } else {
        oFwd = oBack = 0;
    }

    if ((f64)dist < 490.0) range = 2;
    if ((f64)dist <= 170.0) range = 1;
    if ((f64)dist <= 140.0) range = 0;
    if (490.0 <= (f64)dist) range = 3;

    if (!air) {
        situ = 0;
    } else {
        if (!fwd && !back) situ = 0x18;
        else { situ = 8; if (back) situ += 0x20; }
        if (3.0 <= (f64)c->v46C) situ += 8;
    }
    if (o->v170 == 0) {
        situ += 1;
    } else if (o->v170 == 2) {
        if (!oFwd && !oBack) situ += 4;
        else { situ += 2; if (oBack) situ += 4; }
        if (3.0 <= (f64)c->v46C) situ += 1;    /* (its own v46C, not the opponent's) */
    }

    switch (c->aiState) {
    case 0: act = 1; break;
    case 1: act = (f64)dist <= 140.0 || air; break;
    case 2: act = (f64)dist <= 170.0 || air; break;
    case 3: act = ((f64)dist <= 450.0 && 320.0 < (f64)dist) || air; break;
    case 4: act = 490.0 <= (f64)dist || air; break;
    case 5: act = 300.0 <= (f64)dist && !air; break;
    case 6:
        if (385.0 < (f64)dist && (f64)dist < 395.0 && !air && o->seqIndex == 0x27) act = 1;
        break;
    case 7: act = air && 3.0 < (f64)c->v46C; break;
    case 8:
        if (c->aiFinisherFrom < h->v2CC0 && h->v2CC0 < c->aiFinisherTo && !air) act = 1;
        break;
    }
    if (air && c->aiState < 5 && 3.0 < (f64)c->v46C && range < 3) act = 1;

    if (o->seqIndex == 0x28 && c->aiTriedFinisher != 0 && c->aiFinisher == NULL) act = 1;
    if (c->seq.flags & 0x10) myMove = c->curMove;
    if (o->seq.flags & 0x10) oMove = o->curMove;
    c->aiCommand = 0;
    if (c->v284 != range || c->v288 != situ) {
        c->v284 = range;
        c->v288 = situ;
    }

    /* the opponent is dizzy: maybe line up a finishing move */
    if (o->seqIndex == 0x28 && c->aiTriedFinisher == 0) {
        if (AI_CHANCE(D_800B08B0)) {
            if (cfAI_PickMove(c, &c->aiFinisher, &c->aiFinisherVariant, c->def->aiFinishers, 16)) {
                s32 t = *(s16 *)(c->aiFinisher + 0x1A);
                c->aiFinisherFrom = t < 0 ? 0 : t + 15;
                t = *(s16 *)(c->aiFinisher + 0x1C);
                c->aiFinisherTo = t < 0 ? 9999 : t - 15;
                c->aiState = 8;
                c->aiTimer = 9999.0f;
                act = 0;
            } else {
                c->aiFinisher = NULL;
                c->aiFinisherVariant = NULL;
            }
        }
        c->aiTriedFinisher = 1;
    }

    c->aiTimer = c->aiTimer - h->dt;
    if ((f64)c->aiTimer < 0.0) cfAI_NewState(c);

    if (c->v280 != 0) {
        c->v280 = 0;
        if (AI_CHANCE(D_800B0838)) {
            c->aiBlockTime = 0.0f;
            c->aiNoBlock = 120.0f;
        }
    }
    wasBlocking = 0.0 < (f64)c->aiBlockTime;
    if (0.0 < (f64)c->aiBlockTime) {
        c->aiBlockTime = c->aiBlockTime - h->dt;
        if ((f64)c->aiBlockTime <= 0.0) c->aiBlockTime = 0.0f;
    }
    if ((f64)c->aiBlockTime <= 0.0 && (f64)c->aiNoBlock <= 0.0 && AI_CHANCE(D_800B0824)) {
        c->aiBlockTime = (f32)((s32)guRandom() % 70 + 30);
    }
    if (0.0 < (f64)c->aiNoBlock) {
        c->aiNoBlock = c->aiNoBlock - h->dt;
        if ((f64)c->aiNoBlock < 0.0) c->aiNoBlock = 0.0f;
    }

    /* behaviour overrides */
    if (c->v42C != 0 && (u32)(c->aiState - 3) < 2) c->aiState = 2;
    if (o->v2C8 == h->v30 - 1) {        /* the opponent needs one more round */
        if (c->v28C == 0 && c->v2C8 < o->v2C8 && (u32)(o->health - 3) < 0x63) {
            c->aiState = 5;
            c->aiTimer = 300.0f;
            c->v28C = 1;
            goto rushed;
        }
        if (c->v290 == 0 && (u32)(o->health - 3) < 0x16 && (c->v2C8 < o->v2C8 || c->health >= 0x8E)) {
            c->aiState = 5;
            c->aiTimer = 300.0f;
            c->v290 = 1;
            goto rushed;
        }
    }
    if (c->aiState == 5 && o->health < 2) {
        c->aiState = 0;
        c->aiTimer = 0.0f;
    }
rushed:
    if (c->aiState == 6 && act) {
        c->aiState = 7;
        c->aiTimer = 9999.0f;
    }
    if ((u32)(c->aiState - 6) < 2 && (f64)o->v2C0 <= 0.0) {
        c->aiState = 0;
        c->aiTimer = 0.0f;
    }
    if (0.0 < (f64)o->v2C0 && c->aiState != 7) {
        c->aiState = 6;
        c->aiTimer = 9999.0f;
    }
    if (air && c->aiState < 5 && c->v2D8 != 0) {
        c->aiState = 2;
        c->aiTimer = 100.0f;
    }
    if (c->v3C != 0 && !((u32)(c->aiState - 1) < 2)) c->aiState = 0;

    /* walking: 4 = forward, 5 = run, 8 = back, 9 = back fast */
    gap = 0;
    switch (c->aiState) {
    case 1:
        if (140.0 < (f64)dist) {
            c->aiCommand = 4;
            if (340.0 <= (f64)dist || ((s32)guRandom() & 0xF) == 0) c->aiCommand |= 1;
            gap = (s32)((f64)dist - 140.0);
        }
        break;
    case 2:
        if (170.0 < (f64)dist) {
            c->aiCommand = 4;
            if (370.0 <= (f64)dist) c->aiCommand = 5;
            gap = (s32)((f64)dist - 170.0);
        }
        break;
    case 0:
    case 3:
        if (c->aiState != 0 || range == 2) {
            if (450.0 < (f64)dist) {
                c->aiCommand = 4;
                if (550.0 <= (f64)dist) c->aiCommand = 5;
            }
            if ((f64)dist <= 320.0) {
                c->aiCommand = 8;
                if ((f64)dist <= 220.0) c->aiCommand = 9;
            }
        }
        break;
    case 4:
        if ((f64)dist < 490.0) {
            c->aiCommand = 8;
            if ((f64)dist <= 390.0) c->aiCommand = 9;
        }
        break;
    case 5:
        if ((f64)dist < 300.0) {
            c->aiCommand = 8;
            if ((f64)dist <= 200.0) c->aiCommand = 9;
        }
        break;
    case 6:
        if ((f64)dist < 390.0) {
            c->aiCommand = 8;
            if ((f64)dist <= 256.66666666666663) c->aiCommand = 9;
        }
        if (390.0 < (f64)dist) {
            c->aiCommand = 4;
            if (590.0 <= (f64)dist) c->aiCommand = 5;
        }
        break;
    case 8:
        if (h->v2CC0 < c->aiFinisherFrom) {
            c->aiCommand = 8;
            if (133.33333333333334 < (f64)(c->aiFinisherFrom - h->v2CC0)) c->aiCommand = 9;
        } else if (c->aiFinisherTo < h->v2CC0) {
            c->aiCommand = 4;
            if (200.0 < (f64)(h->v2CC0 - c->aiFinisherTo)) c->aiCommand = 5;
        }
        break;
    }

    if (act) {
        c->aiIdleTime = 0.0f;
    } else {
        if (c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0) c->aiIdleTime = c->aiIdleTime + h->dt;
        if (100.0 <= (f64)c->aiIdleTime) c->aiState = 0;
    }
    if (390.0 < (f64)gap && !air && h->v54 >= 4) {
        if (c->seqIndex == 6) cfCharacter_PlaySequence(c, 8, -1, 1, (u32)-1);
        c->aiCommand &= ~1;
        c->v2A8 = 1;
    }
    if ((f64)gap == 0.0) c->v2A8 = 0;

    /* blocking */
    if (c->v258 == 0 && c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0 && !wasBlocking) {
        if (oMove != NULL && (f64)dx < 350.0 &&
            (c->screenX < o->screenX ? o->facing != 0 : o->facing == 0)) {
            s32 skip = 0;
            if (c->v264 >= 2 && o->v3EC >= o->numLinks && o->v3F4 < 0 &&
                o->vF68[10] == -1 && o->vF68[12] == -1 && o->vF68[16] == -1) {
                /* the clever levels don't block a move that is past its hitting frames */
                if (!(o->seq.flags & 0x10)) {
                    skip = 1;
                } else {
                    if (o->seq.hdrE < o->seq.frame &&
                        (o->seq.hdrF < o->seq.frame || o->seq.hdrE < o->seq.loopFrame) &&
                        o->seq.loopCount == 1) skip = 1;
                    if (o->seq.loopCount == 0) skip = 1;
                }
            }
            if (!skip) {
                s32 cmd = -1;
                if (air) {
                    if (!(oMove[0x31] & 4)) cmd = 8;
                } else if (!((oMove[0x31] & 1) && (oMove[0x31] & 2))) {
                    if (c->v170 == 1) cmd = (oMove[0x31] & 2) ? 8 : 10;
                    else              cmd = (oMove[0x31] & 1) ? 10 : 8;
                }
                if (cmd >= 0) {
                    c->aiCommand = cmd;
                    blocking = 1;
                }
            }
        }
        for (i = 0; i < 16; i++) {
            CharProjectile *p = &c->opponent->projectiles[i];
            if (p->active == 0 || p->v4 != 0) continue;
            if (!(p->targetDist < (p->targetDot <= 0.0f ? 1.0f * 350.0f : 0.25f * 350.0f))) continue;
            if (air) {
                if (!(p->v78 & 4)) c->aiCommand = 8;
            } else if (c->v170 == 1) {
                c->aiCommand = (p->v78 & 2) ? 8 : 10;
            } else {
                c->aiCommand = (p->v78 & 1) ? 10 : 8;
            }
        }
    }
    if (c->seqIndex == 0x27 && AI_CHANCE(D_800B08C4)) c->aiCommand = (s32)guRandom() % 16;

    /* attacking */
    if (c->aiState == 8 && c->aiTriedFinisher != 0 && c->aiFinisher != NULL) {
        if (c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0 && act) {
            cfAI_QueueMove(c, c->aiFinisher, c->aiFinisherVariant);
            c->aiState = 0;
            c->aiTimer = 0.0f;
        }
        return;
    }
    if (c->aiTriedFinisher == 0 && c->v440 == 0) {
        s32 s = c->opponent->seqIndex;
        if ((u32)(s - 0x29) < 2) return;
        if (c->opponent->health <= 0 && c->opponent->v2C8 >= h->v30 - 1 && s != 0x28) return;
    }
    finish = 0;
    if (c->opponent->health <= 0 && c->opponent->v2C8 >= h->v30 - 1) finish = c->aiTriedFinisher == 0;

    if (c->aiState == 7 && c->seqIndex != 0x12) {
        c->aiCommand = 5;
        return;
    }
    if (c->seqIndex < 0x3F && D_SeqNeutral[c->seqIndex] != 0 && act && !finish) {
        u8 *list = c->def->aiMoves + range * 0x700 + situ * 0x20;
        if (!air && blocking) return;
        if (c->aiState == 5) {
            cfAI_TryMove(c, c->def->aiRushMoves, 4);
            c->aiState = 0;
            c->aiTimer = 20.0f;
            return;
        }
        if (320.0 <= (f64)dist && (f64)dist <= 450.0 && (s32)guRandom() % 1000 < 50) {
            c->aiCommand = 5;
            return;
        }
        if (c->aiState == 7) {
            cfAI_TryMove(c, list, 16);
            c->aiState = 0;
            c->aiTimer = 0.0f;
        } else if (AI_CHANCE(D_800B0810)) {
            cfAI_TryMove(c, list, 16);
        }
        if (air) return;
        if (0.0 < (f64)c->v410) {
            if (c->v414 == 1)      cfAI_TryMove(c, c->def->aiFollowMoves + 0, 2);
            else if (c->v414 == 2) cfAI_TryMove(c, c->def->aiFollowMoves + 4, 2);
            else if (c->v414 == 0) cfAI_TryMove(c, c->def->aiFollowMoves + 8, 2);
        }
        return;
    }
    if (c->opponent->v42C != 0 && (u32)(c->seqIndex - 0x15) < 9 && AI_CHANCE(D_800B0888)) {
        s32 k = c->opponent->v430;
        if (k == 1)      cfAI_TryMove(c, c->def->aiCounterMoves + 4, 2);
        else if (k == 2) cfAI_TryMove(c, c->def->aiCounterMoves + 8, 2);
        else if (k == 0) cfAI_TryMove(c, c->def->aiCounterMoves + 0, 2);
        return;
    }
    if (myMove == NULL || myMove[6] != 0 || c->v2D4 == 0) return;
    if (c->v42C != 0) {
        if (o->v2C8 >= h->v30 - 1 && o->health < 0x29 && AI_CHANCE(D_800B089C)) {
            cfAI_TryMove(c, c->def->aiComboEnders, 16);
            return;
        }
        if (c->v42C != 0 && cfAI_TryMove(c, c->def->aiComboMoves, 16)) return;
    }
    if (AI_CHANCE(D_800B084C) && !air) cfAI_TryMove(c, c->def->aiChainMoves, 16);
}

/* 0x800720B0: where each fighter's shadow goes */
void func_800720B0(CombatHandler *h) {
    cfCharacter *f = h->fighter1;
    f->v18[1] = f->pos[1];
    f = h->fighter1;
    if (f->facing != 0) {
        f->v18[0] = f->pos[0] - h->camDirX * f->v248;
        f = h->fighter1;
        f->v18[2] = f->pos[2] - h->camDirZ * f->v248;
    } else {
        f->v18[0] = h->camDirX * f->v248 + f->pos[0];
        f = h->fighter1;
        f->v18[2] = h->camDirZ * f->v248 + f->pos[2];
    }
    f = h->fighter2;
    f->v18[1] = f->pos[1];
    f = h->fighter2;
    if (f->facing != 0) {
        f->v18[0] = f->pos[0] - h->camDirX * f->v248;
        f = h->fighter2;
        f->v18[2] = f->pos[2] - h->camDirZ * f->v248;
    } else {
        f->v18[0] = h->camDirX * f->v248 + f->pos[0];
        f = h->fighter2;
        f->v18[2] = h->camDirZ * f->v248 + f->pos[2];
    }
}

/* 0x8006C97C: moves the fighters in the 3D world to match their screen positions */
void func_8006C97C(CombatHandler *h) {
    f32 p[2];

    if (h->v4C != 0) {
        if ((u32)(h->v54 - 10) < 2) return;
        cfCombatHandler_PathPoint(h, h->fighter1->screenX, p);
        h->fighter1->pos[0] = p[0];
        h->fighter1->pos[2] = p[1];
        cfCombatHandler_PathPoint(h, h->fighter2->screenX, p);
        h->fighter2->pos[0] = p[0];
        h->fighter2->pos[2] = p[1];
    } else if (h->v1C != 0) {
        cfCharacter *f = h->fighter1;
        f->pos[0] = (f->screenX - h->v20) * h->camDirX + f->pos[0];
        f = h->fighter1;
        f->pos[2] = (f->screenX - h->v20) * h->camDirZ + f->pos[2];
        f = h->fighter2;
        f->pos[0] = (f->screenX - h->v28) * h->camDirX + f->pos[0];
        f = h->fighter2;
        f->pos[2] = (f->screenX - h->v28) * h->camDirZ + f->pos[2];
        h->v1C = 0;
    }
    func_8006CABC(h);
}

/* 0x800728B4: the camera eases towards its target (at least 3 a frame, speeding up by at
 * most 0.8 a frame) */
void func_800728B4(CombatHandler *h) {
    f32 d, step;

    if (h->vBC != 0) return;
    if (h->vD8 == h->vDC) {
        h->vE0 = 0.0f;
        return;
    }
    d = h->vDC - h->vD8;
    if ((f64)d < 0.0) d = (f32)(0.0 - (f64)d);
    else d = h->vDC - h->vD8;
    step = (f32)((f64)d * 0.16);
    if ((f64)step < 3.0) step = 3.0f;
    if (d < step) step = d;
    if (h->vE0 < step && 0.8 < (f64)(step - h->vE0)) step = (f32)((f64)h->vE0 + 0.8);
    h->vE0 = step;
    if (h->vDC < h->vD8) h->vD8 = h->vD8 - step;
    else h->vD8 = step + h->vD8;
}

f32 cfVec3_Length(f32 *vec3);      /* length of a 3D vector (CF3MTX2D area, still in assembly) */

/* two fighters at exactly the same screen x are pushed 0.1 apart (and kept on the path) */
static void Separate(CombatHandler *h) {
    cfCharacter *a = h->fighter1, *b = h->fighter2, *f;

    if (a->screenX == b->screenX) {
        if (a->prevScreenX < b->prevScreenX) {
            a->screenX = (f32)((f64)a->screenX - 0.1);
        } else if (b->prevScreenX < a->prevScreenX) {
            b->screenX = (f32)((f64)b->screenX - 0.1);
        } else if (0.1 < (f64)h->fighter1->screenX) {
            h->fighter1->screenX = (f32)((f64)h->fighter1->screenX - 0.1);
        } else if (0.1 < (f64)h->fighter2->screenX) {
            h->fighter1->screenX = (f32)((f64)h->fighter1->screenX - 0.1);
        } else {
            h->fighter1->screenX = (f32)((f64)h->fighter1->screenX + 0.1);
        }
        if (h->v4C != 0 && h->arena->v3C == 0) {
            if (h->fighter1->screenX < 0.0f) {
                h->fighter2->screenX = (f32)((f64)h->fighter2->screenX + 0.1);
                h->fighter1->screenX = 0.0f;
            }
            f = h->fighter1;
            if (h->arena->pathLength < f->screenX) f->screenX = h->arena->pathLength;
            if (h->fighter2->screenX < 0.0f) {
                h->fighter1->screenX = (f32)((f64)h->fighter1->screenX + 0.1);
                h->fighter2->screenX = 0.0f;
            }
            f = h->fighter2;
            if (h->arena->pathLength < f->screenX) f->screenX = h->arena->pathLength;
        }
    }
}

/* 0x8006C448: keeps the fighters apart and works out the camera: the direction from the left
 * fighter to the right one, the point half-way between them, and (when not following a
 * path) their screen positions either side of it */
void cfCombatHandler_Centre(CombatHandler *h) {
    cfCharacter *a, *b, *f;
    f32 v[3], o[3], len;
    f64 half;

    Separate(h);

    a = h->fighter1;
    b = h->fighter2;
    if (a->pos[0] == b->pos[0] && a->pos[2] == b->pos[2]) {
        if (h->v18 == 1) {
            a->pos[0] = (f32)((f64)a->pos[0] - (f64)h->camDirX * 0.1);
            f = h->fighter1;
        } else {
            f = h->fighter2;
            f->pos[0] = (f32)((f64)f->pos[0] - (f64)h->camDirX * 0.1);
            f = h->fighter2;
        }
        f->pos[2] = (f32)((f64)f->pos[2] - (f64)h->camDirZ * 0.1);
    }

    if (h->v18 == 1) {
        v[0] = h->fighter2->pos[0];
        v[1] = h->fighter2->pos[1];
        v[2] = h->fighter2->pos[2];
        o[0] = h->fighter1->pos[0];
        o[1] = h->fighter1->pos[1];
        o[2] = h->fighter1->pos[2];
    } else {
        v[0] = h->fighter1->pos[0];
        v[1] = h->fighter1->pos[1];
        v[2] = h->fighter1->pos[2];
        o[0] = h->fighter2->pos[0];
        o[1] = h->fighter2->pos[1];
        o[2] = h->fighter2->pos[2];
    }
    v[1] = (f32)h->floorY;
    o[1] = (f32)h->floorY;
    v[0] = v[0] - o[0];
    v[1] = v[1] - o[1];
    v[2] = v[2] - o[2];
    len = cfVec3_Length(v);
    if ((f64)len > 0.0) {
        cfVec3_Normalise(v);
        h->camDirX = v[0];
        h->camDirY = v[1];
        h->camDirZ = v[2];
    }
    half = (f64)len / 2.0;
    h->centre[0] = (f32)((f64)o[0] + (f64)v[0] * half);
    h->centre[1] = (f32)((f64)o[1] + (f64)v[1] * half);
    h->centre[2] = (f32)((f64)o[2] + (f64)v[2] * half);

    if (h->v4C == 0) {
        if (h->v18 == 1) {
            h->fighter1->screenX = (f32)(0.0 - half);
            h->fighter2->screenX = (f32)half;
        } else {
            h->fighter1->screenX = (f32)((f64)len / 2.0);
            h->fighter2->screenX = (f32)(0.0 - (f64)len / 2.0);
        }
        if (h->v1C != 0) func_8006C940(h);
    }
}

/* 0x8006FA2C: the gap between the fighters' body boxes (negative when they overlap) */
s32 cfCombatHandler_Gap(CombatHandler *h) {
    cfCharacter *f;
    s32 l, r;

    Separate(h);
    f = h->fighter1;
    if (f->screenX < h->fighter2->screenX) {
        f32 x = f->screenX;
        l = f->hasBodyRect ? (s32)((f32)f->bodyRect[2] + x) : (s32)h->fighter1->screenX;
        f = h->fighter2;
        r = f->hasBodyRect ? (s32)((f32)f->bodyRect[0] + f->screenX) : (s32)h->fighter2->screenX;
        return r - l;
    }
    f = h->fighter2;
    r = f->hasBodyRect ? (s32)((f32)f->bodyRect[2] + f->screenX) : (s32)h->fighter2->screenX;
    f = h->fighter1;
    l = f->hasBodyRect ? (s32)((f32)f->bodyRect[0] + f->screenX) : (s32)h->fighter1->screenX;
    return l - r;
}

/* one fighter's shadow eases towards where it should be (as the camera does) */
static void EaseShadow(cfCharacter *const *slot) {
    cfCharacter *f = *slot;
    f32 d[3], len, step;

    if (f->v24[0] == f->v18[0] && f->v24[1] == f->v18[1] && f->v24[2] == f->v18[2]) {
        f->v30 = 0.0f;
        return;
    }
    d[0] = f->v18[0];
    d[1] = (*slot)->v18[1];
    d[2] = (*slot)->v18[2];
    d[0] = d[0] - (*slot)->v24[0];
    d[1] = d[1] - (*slot)->v24[1];
    d[2] = d[2] - (*slot)->v24[2];
    len = cfVec3_Length(d);
    d[0] = d[0] / len;
    d[1] = d[1] / len;
    d[2] = d[2] / len;
    step = (f32)((f64)len * 0.16);
    if ((f64)step < 3.0) step = 3.0f;
    if (len < step) step = len;
    f = *slot;
    if (f->v30 < step && 0.8 < (f64)(step - f->v30)) step = (f32)((f64)f->v30 + 0.8);
    (*slot)->v30 = step;
    (*slot)->v24[0] = step * d[0] + (*slot)->v24[0];
    (*slot)->v24[1] = step * d[1] + (*slot)->v24[1];
    (*slot)->v24[2] = step * d[2] + (*slot)->v24[2];
}

/* 0x800721C4 */
void func_800721C4(CombatHandler *h) {
    if (h->vBC != 0) return;
    EaseShadow(&h->fighter1);
    EaseShadow(&h->fighter2);
}

/* moves cur towards target: 16% of the distance a frame, at least 3, never past it, and
 * speeding up by at most 0.8 a frame */
static void Ease3(f32 *cur, const f32 *target, f32 *speed) {
    f32 d[3], len, step;

    if (cur[0] == target[0] && cur[1] == target[1] && cur[2] == target[2]) {
        *speed = 0.0f;
        return;
    }
    d[0] = target[0] - cur[0];
    d[1] = target[1] - cur[1];
    d[2] = target[2] - cur[2];
    len = cfVec3_Length(d);
    d[0] = d[0] / len;
    d[1] = d[1] / len;
    d[2] = d[2] / len;
    step = (f32)((f64)len * 0.16);
    if ((f64)step < 3.0) step = 3.0f;
    if (len < step) step = len;
    if (*speed < step && 0.8 < (f64)(step - *speed)) step = (f32)((f64)*speed + 0.8);
    *speed = step;
    cur[0] = step * d[0] + cur[0];
    cur[1] = step * d[1] + cur[1];
    cur[2] = step * d[2] + cur[2];
}

/* 0x80072568: the camera eases towards where it should be and what it should look at */
void func_80072568(CombatHandler *h) {
    if (h->vBC != 0) return;
    Ease3(h->camEye, h->camEyeTarget, &h->camEyeSpeed);
    Ease3(h->camAt, h->camAtTarget, &h->camAtSpeed);
}

/* 0x8006B6EC: starts a fight: resets the handler, checks the BkgDef (version 4.10), places
 * the two fighters at the arena's start positions, facing each other in their standing
 * sequence, and sets the camera going. */
void cfCombatHandler_Start(CombatHandler *h, cfCharacter *f1, cfCharacter *f2, u8 *bkg, s32 arenaIndex,
                           s32 skipIntro, s32 followPath) {
    f32 p[2];

    h->v4C = followPath;
    h->v7C = 0;
    h->v30 = 2;
    h->v34 = 99.0f;
    h->v38 = 0;
    h->v3C = NULL;
    h->v40 = NULL;
    h->v44 = 2;
    h->v48 = 0;
    h->floorY = 0;
    h->minX = -9999;
    h->maxX = 9999;
    h->dt = ScaleByFrameTime(1.0f);
    h->gravity = ScaleByFrameTime(0.1f);
    h->friction = ScaleByFrameTime(1.0f);
    h->v58 = 0;
    h->v5C = 0;
    h->numSounds = 0;
    mpMemSet(h->smoke, sizeof(h->smoke), 0);
    mpMemSet(h->bits, sizeof(h->bits), 0);
    mpMemSet(h->marks, sizeof(h->marks), 0);
    mpMemSet(h->sparks, sizeof(h->sparks), 0);
    mpMemSet(h->dust, sizeof(h->dust), 0);
    mpMemSet(h->debris, sizeof(h->debris), 0);
    mpMemSet(h->bursts, sizeof(h->bursts), 0);
    mpMemSet(h->glows, sizeof(h->glows), 0);
    mpMemSet(h->rings, sizeof(h->rings), 0);
    mpMemSet(h->flashes, sizeof(h->flashes), 0);
    h->v2C70 = 1.0f;
    h->v2C74 = 1.0f;
    h->v2C78 = 1;
    h->v2C7C = 1;
    h->v2C80 = 1;
    h->v2C84 = 0;
    h->v2C88 = 1.0f;
    h->v2C8C = 1.0f;
    h->shake = 0.0f;
    h->frameCount = 0;
    h->v2CC4 = 0.0f;
    h->v2CC8 = 0.0f;
    h->v6C = 0;
    h->v70 = 0;
    h->v74 = 1;
    h->v78 = 0;

    h->v134 = bkg;
    if (((((const volatile u8 *)bkg)[0] << 8) | ((const volatile u8 *)bkg)[1]) != 0x040A) {   /* two version bytes */
        mpFatalErrorf("CF3COMBT.C - Attempt to initialize with BkgDef version %d.%02d - expecting version %d.%02d",
                      bkg[0], bkg[1], 4, 10);
    }
    h->arenaIndex = arenaIndex;
    h->arena = (ArenaDef *)(h->v134 + *(s32 *)(h->v134 + 0x3C) + arenaIndex * 0x60);
    h->v140 = -1;
    h->v144 = 0;
    h->v148 = 0;
    h->v14C = 0;

    if (h->v4C == 0) {
        f1->pos[0] = h->arena->start1[0];
        f1->pos[2] = h->arena->start1[1];
        f2->pos[0] = h->arena->start2[0];
        f2->pos[2] = h->arena->start2[1];
    } else {
        cfCombatHandler_PathPoint(h, h->arena->startT1, p);
        f1->pos[0] = p[0];
        f1->pos[2] = p[1];
        cfCombatHandler_PathPoint(h, h->arena->startT2, p);
        f2->pos[0] = p[0];
        f2->pos[2] = p[1];
        f1->screenX = h->arena->startT1;
        f2->screenX = h->arena->startT2;
    }
    f1->pos[1] = 0.0f;
    f2->pos[1] = 0.0f;
    h->fighter1 = f1;
    h->fighter2 = f2;
    h->fighter1->v40 = 1;
    h->fighter2->v40 = 2;
    h->fighter1->handler = h;
    h->fighter2->handler = h;
    h->fighter1->screenY = (f32)h->floorY;
    h->fighter2->screenY = (f32)h->floorY;
    h->fighter1->opponent = h->fighter2;
    h->fighter2->opponent = h->fighter1;
    h->v18 = 1;
    cfCharacter_TurnAround(h->fighter2);
    cfCombatHandler_Centre(h);
    func_800720B0(h);
    h->fighter1->v24[0] = h->fighter1->v18[0];
    h->fighter1->v24[1] = h->fighter1->v18[1];
    h->fighter1->v24[2] = h->fighter1->v18[2];
    h->fighter2->v24[0] = h->fighter2->v18[0];
    h->fighter2->v24[1] = h->fighter2->v18[1];
    h->fighter2->v24[2] = h->fighter2->v18[2];
    h->time = 0;
    h->pad1 = 0;
    h->pressed1 = 0;
    h->pad2 = 0;
    h->pressed2 = 0;
    h->v100 = 0.0f;
    h->v104 = 0.0f;
    h->v108 = 0;
    h->vD8 = 0.0f;
    h->vDC = 0.0f;
    h->vE0 = 0.0f;
    h->camEyeTarget[0] = 0.0f;
    h->camEyeTarget[1] = 0.0f;
    h->camEyeTarget[2] = 0.0f;
    h->camAtTarget[0] = 0.0f;
    h->camAtTarget[1] = 0.0f;
    h->camAtTarget[2] = 0.0f;
    h->camEyeSpeed = 0.0f;
    h->camAtSpeed = 0.0f;
    h->vB8 = 0;
    h->vBC = 0;
    func_800729EC(h);
    cfCharacter_PlaySequence(h->fighter1, 0, -1, 1, (u32)-1);
    cfCharacter_PlaySequence(h->fighter2, 0, -1, 1, (u32)-1);
    h->v2CAC = 0;
    h->v2CB0 = 0;
    h->v2CB4 = 0;
    h->v2CB8 = 0;
    h->v50 = -1;
    if (skipIntro != 0) {
        h->v54 = 0;
        h->v68 = 0;
    } else {
        h->v54 = 1;
        h->v100 = 1.0f;
        h->v68 = 1;
    }
}

/* ---------------------------------------------------------------- ring-outs */

/* an arena zone where a fighter can be knocked out of the arena (0x38 bytes) */
typedef struct ArenaZone {
    s16 type;           /* 0 through a wall, 1 up into the air, 2 down a hole */
    u8  pad2[0xE];
    u8  enabled;
    u8  pad11[0xB];
    f32 x, z;           /* centre */
    f32 radius;
    f32 nx, nz;         /* type 0: direction the hit has to come from ... */
    f32 minDot;         /* ... at least this closely */
    f32 pathT;          /* where the zone is along the path */
} ArenaZone;

/* 0x80074330: does attacker a's heavy hit knock v out of the arena through this zone?
 * If so the ring-out starts: v flies off towards the zone's target and the camera follows. */
s32 cfCombatHandler_CheckZone(CombatHandler *h, ArenaZone *zone, cfCharacter *a, cfCharacter *v) {
    f32 dir = v->facing ? -1.0f : 1.0f;
    f32 d[2], tgt[3], p[3];
    s32 seqN = 0;       /* (left unset by the original for unknown zone types) */

    if (!zone->enabled) return 0;
    if (a->hitE4 != 2) return 0;            /* heavy hits only */
    if (v->v334 != 0) return 0;
    if (a->vF50 == 3) return 0;

    switch (zone->type) {
    case 0:
        if (a->v2D4 == 0) return 0;
        d[0] = a->pos[0] - v->pos[0];
        d[1] = a->pos[2] - v->pos[2];
        cfVec2_Normalise(d);
        if (d[0] * zone->nx + d[1] * zone->nz < zone->minDot) return 0;
        tgt[0] = zone->x;
        tgt[1] = 0.0f;
        tgt[2] = zone->z;
        seqN = 0x1F;
        h->zoneSpeed = 10.0f;
        break;
    case 1:
        if ((u32)(v->seqIndex - 0x22) >= 2) return 0;
        seqN = 0x22;
        tgt[0] = v->pos[0];
        tgt[1] = 80.0f;
        tgt[2] = v->pos[2];
        h->zoneSpeed = 20.0f;
        break;
    case 2:
        if (v->seqIndex != 0x37) return 0;
        h->v170 = -470.0f;
        tgt[0] = zone->x;
        tgt[1] = h->v170;
        tgt[2] = zone->z;
        seqN = 0x37;
        h->zoneSpeed = 30.0f;
        break;
    }

    /* a point 90 behind the victim must be inside the zone */
    p[0] = (f32)((f64)v->pos[0] + (f64)h->camDirX * 90.0 * (f64)dir);
    p[1] = v->pos[1];
    p[2] = (f32)((f64)v->pos[2] + (f64)h->camDirZ * 90.0 * (f64)dir);
    d[0] = p[0] - zone->x;
    d[1] = p[2] - zone->z;
    if (zone->radius < cfVec2_Length(d)) return 0;

    if (h->v4C != 0 && zone->type == 0 && (f64)zone->minDot < -0.8) {
        /* on a path: the zone must be behind the victim (away from the attacker) */
        f32 t = zone->pathT;
        for (;;) {
            f64 len = (f64)h->arena->pathLength;
            f64 diff = (f64)(v->screenX - t);
            if (diff < 0.0) diff = 0.0 - diff;
            if (!(len < diff)) break;
            if (t < v->screenX) t = t + h->arena->pathLength;
            else t = t - h->arena->pathLength;
        }
        if (v->screenX < a->screenX && (f64)v->screenX - 45.0 < (f64)t) return 0;
        if (a->screenX < v->screenX && (f64)t < (f64)v->screenX + 45.0) return 0;
    }

    h->v150 = a;
    h->v154 = v;
    h->zoneType = zone->type;
    h->zoneDir[0] = tgt[0] - v->pos[0];
    h->zoneDir[1] = tgt[1] - v->screenY;
    h->zoneDir[2] = tgt[2] - v->pos[2];
    h->zoneDist = cfVec3_Length(h->zoneDir);
    h->zoneDir[0] = h->zoneDir[0] / h->zoneDist;
    h->zoneDir[1] = h->zoneDir[1] / h->zoneDist;
    h->zoneDir[2] = h->zoneDir[2] / h->zoneDist;
    cfCharacter_PlaySequence(v, seqN, 1, 1, (u32)-1);
    v->v478 = 0.0f;
    v->v480 = 0.0f;
    v->v484 = 0.0f;
    v->v488 = 0.0f;
    if (h->zoneType == 2) v->v46C = 0.0f;
    h->v16C = 0;
    if (h->zoneType == 1 && (f32)h->floorY <= v->screenY) {
        s32 i;
        for (i = 0; i < 10; i++) cfCharacter_EmitSmoke(v, v->pos[0], v->pos[1], v->pos[2], v->v40, i % 3);
        cfCharacter_EmitBits(v, (s32)guRandom() % 3 + 1, v->pos[0], v->pos[1], v->pos[2]);
        h->v148 = 1;
        h->v16C = 1;
    }
    h->vB8 = (v == h->fighter1) ? 1 : 2;
    if (h->zoneType == 0) {
        h->v184[0] = zone->x;
        h->v184[1] = 95.0f;
        h->v184[2] = zone->z;
        h->v178[0] = a->pos[0];
        h->v178[1] = 160.0f;
        h->v178[2] = a->pos[2];
        h->vB8 = 5;
        h->v178[0] = (f32)((f64)h->v178[0] - (f64)h->camDirZ * 350.0);
        h->v178[2] = (f32)((f64)h->v178[2] + (f64)h->camDirX * 350.0);
    }
    return 1;
}

/* 0x80074254: checks every zone for both fighters */
void cfCombatHandler_CheckZones(CombatHandler *h) {
    ArenaZone *z = (ArenaZone *)(h->v134 + h->arena->zonesOffset);
    s32 i;

    for (i = 0; i < h->arena->numZones; i++, z++) {
        if (cfCombatHandler_CheckZone(h, z, h->fighter1, h->fighter2) ||
            cfCombatHandler_CheckZone(h, z, h->fighter2, h->fighter1)) {
            h->v140 = i;
            h->v54 = 10;
            h->v154->seq.waitTimer = 0.0f;
            h->v154->v46C = 0.0f;
            h->v154->v48C = -0.8f;
            return;
        }
    }
}

/* 0x80074970: the knocked-out fighter flies towards the zone's target */
void func_80074970(CombatHandler *h) {
    f32 s = ScaleByFrameTime(h->zoneSpeed);
    cfCharacter *v;
    f64 limit;

    switch (h->zoneType) {
    case 0:
        v = h->v154;
        v->pos[0] = s * h->zoneDir[0] + v->pos[0];
        v = h->v154;
        v->pos[2] = s * h->zoneDir[2] + v->pos[2];
        h->zoneDist = h->zoneDist - s;
        if (h->v16C == 0 && (f64)h->zoneDist <= 80.0) {
            h->v148 = 1;
            h->v16C = 1;
        }
        limit = -300.0;
        break;
    case 1:
        v = h->v154;
        v->pos[0] = s * h->zoneDir[0] + v->pos[0];
        v = h->v154;
        v->screenY = s * h->zoneDir[1] + v->screenY;
        v = h->v154;
        v->pos[2] = s * h->zoneDir[2] + v->pos[2];
        if (h->v16C == 0 && (f32)h->floorY <= h->v154->screenY) {
            h->v148 = 1;
            h->v16C = 1;
        }
        h->zoneDist = h->zoneDist - s;
        limit = -100.0;
        break;
    case 2:
        v = h->v154;
        v->pos[0] = s * h->zoneDir[0] + v->pos[0];
        v = h->v154;
        v->screenY = s * h->zoneDir[1] + v->screenY;
        v = h->v154;
        v->pos[2] = s * h->zoneDir[2] + v->pos[2];
        if (h->v16C == 0 && h->v154->screenY <= h->v170) {
            h->v148 = 1;
            h->v16C = 1;
        }
        h->zoneDist = h->zoneDist - s;
        limit = -800.0;
        break;
    default:
        h->v144 = 1;
        return;
    }
    if ((f64)h->zoneDist <= limit) h->v144 = 1;
}

/* 0x80074BA0: after a ring-out */
void func_80074BA0(CombatHandler *h) {
    f32 s = ScaleByFrameTime(h->v1A0);
    f32 floor;

    if (h->zoneType == 0) {
        cfCharacter *a = h->v150;
        a->pos[0] = s * h->v190 + a->pos[0];
        a = h->v150;
        a->pos[2] = s * h->v198 + a->pos[2];
        h->v19C = h->v19C - s;
        if ((f64)h->v19C < 0.0) {
            cfCharacter_PlaySequence(h->v150, 0, -1, 1, (u32)-1);
            h->v54 = 4;
            cfCombatHandler_Centre(h);
        }
        return;
    }
    if (h->zoneType < 0 || h->zoneType >= 3) {
        h->v54 = 4;
        return;
    }
    floor = (f32)h->floorY;
    if (floor <= h->v150->screenY && floor <= h->v154->screenY) {
        h->v54 = 4;
        h->vB8 = 0;
    }
    if (h->v16C == 0 && (f32)h->floorY <= h->v154->screenY) {
        h->v14C = 1;
        h->v16C = 1;
        h->v154->v470 = 4.0f;
        cfCharacter_PlaySequence(h->v154, 0x20, 1, 1, (u32)-1);
        h->v154->v2B4 = 15.0f;
    }
}

/* ---------------------------------------------------------------- smoke and bits */

/* one bouncing piece (a smoke puff or a bit of clay): gravity, spin, bouncing on the floor
 * (the spin settles towards flat), sliding to a stop, bouncing off the arena's walls and
 * falling off its edges */
static void UpdatePiece(CombatHandler *h, s32 *active, f32 *pos, f32 *vel, f32 *angle, f32 *spin,
                        s32 *falling, s32 *resting) {
    f32 old[3];
    s32 j;

    old[0] = pos[0];
    old[1] = pos[1];
    old[2] = pos[2];
    *angle = *angle + *spin;
    vel[1] = (f32)((f64)vel[1] - 0.5);
    pos[0] = pos[0] + vel[0];
    pos[1] = pos[1] + vel[1];
    pos[2] = pos[2] + vel[2];

    if (*falling) {
        if ((f64)pos[1] <= (f64)h->floorY - 150.0) {
            *active = 0;
            return;
        }
    } else {
        f32 floor = (f32)h->floorY;
        if (pos[1] <= floor) {
            f32 bounce = (f32)((f64)vel[1] / -10.0);
            f32 turn;
            f64 t;
            pos[1] = floor;
            vel[1] = (f32)((f64)vel[1] * -0.55);
            if ((f64)vel[1] < 0.6) {
                vel[1] = 0.0f;
                *resting = 1;
            }
            *angle = (f32)((s32)*angle % 360);
            t = (f64)*angle;
            if (90.0 <= t && t <= 270.0)  turn = (f32)(180.0 - t);
            else if (180.0f < *angle)     turn = (f32)(360.0 - (f64)*angle);
            else                          turn = (f32)(0.0 - (f64)*angle);
            *angle = (f32)((f64)*angle + (f64)turn * (1.0 - (f64)(bounce * bounce)));
            *spin = (f32)((f64)(f32)((s32)guRandom() % 2048 - 1024) / 128.0 * (f64)bounce);
        }
    }

    if (*resting) {
        vel[0] = (f32)((f64)vel[0] * 0.95);
        vel[2] = (f32)((f64)vel[2] * 0.95);
        if ((f64)vel[0] < 0.05 && -0.05 < (f64)vel[0] && (f64)vel[2] < 0.05 && -0.05 < (f64)vel[2]) {
            *active = 0;
            return;
        }
    }
    if (*falling) return;
    if (old[0] == pos[0] && old[1] == pos[1] && old[2] == pos[2]) {
        *active = 0;
        return;
    }
    {
        u8 *walls = h->v134 + h->arena->wallsOffset;
        f32 seg[4], hit[2];
        seg[0] = old[0];
        seg[1] = old[2];
        seg[2] = pos[0];
        seg[3] = pos[2];
        for (j = 0; j < h->arena->numWalls; j++) {
            f32 *w = (f32 *)(walls + j * 0x24);
            s32 r = cfSegmentsIntersect(seg, w, hit);
            f32 d[2], len, k;

            if (r == 0 || r == 2) continue;
            if (*((s8 *)w + 0x20) == 0) {
                *falling = 1;
                *resting = 0;
                continue;
            }
            d[0] = pos[0] - old[0];
            d[1] = pos[2] - old[2];
            len = cfVec2_Length(d);
            d[0] = d[0] / len;
            d[1] = d[1] / len;
            k = d[0] * w[6] + d[1] * w[7];
            if ((f64)k < 0.0) k = (f32)(0.0 - (f64)k);
            else k = d[0] * w[6] + d[1] * w[7];
            d[0] = (f32)((f64)(k * w[6]) + (f64)d[0] * (1.0 - (f64)k));
            d[1] = (f32)((f64)(k * w[7]) + (f64)d[1] * (1.0 - (f64)k));
            vel[0] = (f32)((f64)(len * d[0]) * 0.6);
            vel[2] = (f32)((f64)(len * d[1]) * 0.6);
            if ((s32)guRandom() % 1024 < 256) {
                vel[1] = (f32)((f64)vel[1] * 1.25);
                *spin = (f32)((f64)*spin * 1.25);
            }
            pos[0] = hit[0];
            pos[2] = hit[1];
            pos[0] = (f32)((f64)pos[0] + (f64)w[6] * 0.01);
            pos[2] = (f32)((f64)pos[2] + (f64)w[7] * 0.01);
            break;
        }
    }
}

/* 0x80070DDC */
void Fx_UpdateSmoke(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 64; i++) {
        FxSmoke *s = &h->smoke[i];
        if (!s->active) continue;
        UpdatePiece(h, &s->active, s->pos, s->vel, &s->angle, &s->spin, &s->falling, &s->resting);
    }
}

/* 0x800714BC */
void Fx_UpdateBits(CombatHandler *h) {
    s32 i;
    for (i = 0; i < 32; i++) {
        FxBit *b = &h->bits[i];
        if (!b->active) continue;
        UpdatePiece(h, &b->active, b->pos, b->vel, &b->angle, &b->spin, &b->falling, &b->resting);
    }
}

/* ---------------------------------------------------------------- the fighters' bodies */

void func_80092A2C(s32 *a, s32 *b);   /* a = a intersected with b (int rectangles, MPF) */

static void BodyBox(cfCharacter *f, s32 *r) {
    r[0] = f->bodyRect[0];
    r[1] = f->bodyRect[1];
    r[2] = f->bodyRect[2];
    r[3] = f->bodyRect[3];
    r[0] = (s32)((f32)r[0] + f->screenX);
    r[2] = (s32)((f32)r[2] + f->screenX);
    r[1] = (s32)((f32)r[1] + f->screenY);
    r[3] = (s32)((f32)r[3] + f->screenY);
}

/* do b's top or bottom edge fall within a, or does b span a? (one axis: i0/i1) */
static s32 Spans(const s32 *a, const s32 *b, s32 i0, s32 i1) {
    if (!(b[i0] < a[i0] || a[i1] < b[i0])) return 1;
    if (!(b[i1] < a[i0] || a[i1] < b[i1])) return 1;
    return b[i0] < a[i0] && a[i1] < b[i1];
}

/* 0x8006FD3C: overlapping fighters push each other apart (more the faster they were moving);
 * a fighter jumping into the other stops moving sideways */
void cfCombatHandler_PushApart(CombatHandler *h) {
    cfCharacter *f;
    s32 r1[4], r2[4], p1[4], p2[4], box[4];
    f32 d, push, w, half;
    f64 d1, d2;
    s32 leftIs1;

    if (!h->fighter1->hasBodyRect || !h->fighter2->hasBodyRect) return;

    f = h->fighter1;
    d = f->prevScreenX - f->screenX;
    if ((f64)d < 0.0) d1 = 0.0 - (f64)d;
    else d1 = (f64)(h->fighter1->prevScreenX - h->fighter1->screenX);
    f = h->fighter2;
    d = f->prevScreenX - f->screenX;
    if ((f64)d < 0.0) d2 = 0.0 - (f64)d;
    else d2 = (f64)(f->prevScreenX - f->screenX);
    push = (f32)((f64)(f32)(d1 + d2) + 20.0);

    BodyBox(h->fighter1, r1);
    BodyBox(h->fighter2, r2);
    f = h->fighter1;
    if (f->prevHasBodyRect) {
        p1[0] = (s32)((f32)f->prevBodyRect[0] + f->prevScreenX);
        p1[2] = (s32)((f32)f->prevBodyRect[2] + f->prevScreenX);
        p1[1] = (s32)((f32)f->prevBodyRect[1] + f->prevScreenY);
        p1[3] = (s32)((f32)f->prevBodyRect[3] + f->prevScreenY);
    } else {
        p1[0] = r1[0]; p1[1] = r1[1]; p1[2] = r1[2]; p1[3] = r1[3];
    }
    f = h->fighter2;
    if (f->prevHasBodyRect) {
        p2[0] = (s32)((f32)f->prevBodyRect[0] + f->prevScreenX);
        p2[2] = (s32)((f32)f->prevBodyRect[2] + f->prevScreenX);
        /* (sic) fighter 1's previous y */
        p2[1] = (s32)((f32)f->prevBodyRect[1] + h->fighter1->prevScreenY);
        p2[3] = (s32)((f32)f->prevBodyRect[3] + h->fighter1->prevScreenY);
    } else {
        p2[0] = r2[0]; p2[1] = r2[1]; p2[2] = r2[2]; p2[3] = r2[3];
    }

    if (!Spans(r1, r2, 1, 3)) return;
    if (!Spans(r1, r2, 0, 2)) return;

    box[0] = r1[0]; box[1] = r1[1]; box[2] = r1[2]; box[3] = r1[3];
    func_80092A2C(box, r2);
    push = ScaleByFrameTime(push);
    w = (f32)(box[2] - box[0] + 1);
    if (w < push) push = w;
    half = push / 2.0f;

    if (p1[0] < p2[0] && p1[2] < p2[2]) {
        leftIs1 = 1;
    } else if (p2[0] < p1[0] && p2[2] < p1[2]) {
        leftIs1 = 0;
    } else {
        leftIs1 = (p1[0] + p1[2]) / 2 < (p2[0] + p2[2]) / 2;
    }
    if (leftIs1) {
        h->fighter1->screenX = h->fighter1->screenX - half;
        h->fighter2->screenX = half + h->fighter2->screenX;
        f = h->fighter1;
        if ((f64)f->v470 > 0.0 && (f64)f->v46C > 0.0) f->v470 = 0.0f;
        f = h->fighter2;
        if ((f64)f->v470 < 0.0 && (f64)f->v46C > 0.0) f->v470 = 0.0f;
    } else {
        h->fighter2->screenX = h->fighter2->screenX - half;
        h->fighter1->screenX = half + h->fighter1->screenX;
        f = h->fighter1;
        if ((f64)f->v470 < 0.0 && (f64)f->v46C > 0.0) f->v470 = 0.0f;
        f = h->fighter2;
        if ((f64)f->v470 > 0.0 && (f64)f->v46C > 0.0) f->v470 = 0.0f;
    }
}

/* ---------------------------------------------------------------- side-stepping */

/* one fighter side-steps (into or out of the screen) round the other one's starting point.
 * which = 1 or 2 picks the walk sequence (0xA / 0xB) by side. Returns 1 if it moved. */
static s32 SideStep(CombatHandler *h, cfCharacter *const *slot, cfCharacter *const *other, const f32 *me,
                    s32 which, f32 scale) {
    cfCharacter *x = *slot;
    f32 v[3], len, step;
    s32 s;

    if (x->v34 == 0 && x->v38 == 0) {
        if (x->v170 != 2) return 0;
        if (!(x->screenY < (f32)h->floorY)) return 0;
        if (x->v474 == 0.0f) return 0;
    }
    if (x->v170 != 2) {
        s = x->seqIndex;
        if (!((u32)(s - 6) < 2 || (u32)(s - 0xA) < 2 || s == 0)) {
            x->v34 = 0;
            (*slot)->v38 = 0;
            return 0;
        }
    }
    x = *slot;
    step = ScaleByFrameTime(x->v170 == 2 ? x->v474 : (f32)((f64)x->stats[0] * 1.2));
    step = step * scale;

    x = *other;
    v[0] = x->pos[0];
    v[1] = (*other)->pos[1];
    v[2] = (*other)->pos[2];
    v[1] = (f32)h->floorY;
    v[0] = v[0] - me[0];
    v[1] = v[1] - me[1];
    v[2] = v[2] - me[2];
    len = cfVec3_Length(v);
    if (0.1 < (f64)len) {
        f32 zero = 0.0f, c0, c2;
        v[0] = v[0] / len;
        v[1] = v[1] / len;
        v[2] = v[2] / len;
        if ((*slot)->v34 != 0) {
            c0 = v[1] * zero - (-v[2]);
            c2 = -v[0] - v[1] * zero;
        } else {
            c0 = v[1] * zero - v[2];
            c2 = v[0] - v[1] * zero;
        }
        x = *slot;
        x->pos[0] = step * c0 + x->pos[0];
        x = *slot;
        x->pos[2] = step * c2 + x->pos[2];
    }
    x = *slot;
    if (x->seqIndex == 0) {
        s32 seqN;
        if (x->v34 != 0) seqN = (h->v18 != which) ? 0xB : 0xA;
        else             seqN = (h->v18 == which) ? 0xB : 0xA;
        cfCharacter_PlaySequence(*slot, seqN, -1, 1, (u32)-1);
    }
    return 1;
}

/* 0x80070590: side-steps (slower when the fighters are close together) */
void cfCombatHandler_SideSteps(CombatHandler *h) {
    f32 me1[3], me2[3], dist, scale;
    s32 moved;

    if (0.0f < h->v2CC4) return;
    if (0.0f < h->v2CC8) return;
    me1[0] = h->fighter1->pos[0];
    me1[1] = h->fighter1->pos[1];
    me1[2] = h->fighter1->pos[2];
    me2[0] = h->fighter2->pos[0];
    me2[1] = h->fighter2->pos[1];
    me2[2] = h->fighter2->pos[2];
    me1[1] = (f32)h->floorY;
    me2[1] = (f32)h->floorY;
    dist = h->fighter1->screenX - h->fighter2->screenX;
    if (dist < 0.0f) dist = (f32)(0.0 - (f64)dist);
    scale = 1.0f;
    if (dist < 210.0f) scale = dist / 210.0f;
    moved = SideStep(h, &h->fighter1, &h->fighter2, me1, 1, scale);
    if (SideStep(h, &h->fighter2, &h->fighter1, me2, 2, scale)) moved = 1;
    if (moved) cfCombatHandler_Centre(h);
}

/* ---------------------------------------------------------------- arena boundaries */

/* 0x80073900: keeps fighter c inside the arena. Following a path: clamps it to the path's ends
 * (nudging both fighters a little so they never coincide). Otherwise: its move this frame is
 * clipped against the arena's bounding lines, sliding along each line it hits (pushing the
 * opponent back by part of the overlap when they are close); squeezed into a corner, it stays
 * where it was and its push goes to the opponent instead. */
void cfCombatHandler_ClipPlayerAgainstBoundingLines(CombatHandler *h, cfCharacter *c) {
    if (h->v4C != 0) {
        f32 p[2];
        cfCharacter *f;

        if (h->arena->v3C != 0) return;
        p[0] = c->pos[0];
        p[1] = c->pos[2];
        if ((f64)c->screenX < 0.0) {
            c->screenX = 0.0f;
            if (c == h->fighter1) {
                c->screenX = 0.001f;
                f = h->fighter2;
                f->screenX = (f32)((f64)f->screenX + 0.002);
            } else {
                f = h->fighter2;
                f->screenX = (f32)((f64)f->screenX + 0.0015);
                f = h->fighter1;
                f->screenX = (f32)((f64)f->screenX + 0.0025);
            }
            cfCombatHandler_PathPoint(h, c->screenX, p);
            if ((f64)c->v478 != 0.0) {
                f = c->opponent;
                f->v478 = (f32)((f64)f->v478 + -(f64)c->v478);
                c->v478 = 0.0f;
            }
        }
        if (h->arena->pathLength < c->screenX) {
            f32 len = h->arena->pathLength;
            c->screenX = len;
            if (c == h->fighter1) {
                c->screenX = (f32)((f64)len - 0.00125);
                f = h->fighter2;
                f->screenX = (f32)((f64)f->screenX - 0.00225);
            } else {
                f = h->fighter2;
                f->screenX = (f32)((f64)f->screenX - 0.00175);
                f = h->fighter1;
                f->screenX = (f32)((f64)f->screenX - 0.00275);
            }
            cfCombatHandler_PathPoint(h, c->screenX, p);
            if ((f64)c->v478 != 0.0) {
                f = c->opponent;
                f->v478 = (f32)((f64)f->v478 + -(f64)c->v478);
                c->v478 = 0.0f;
            }
        }
        if (p[0] == c->pos[0] && p[1] == c->pos[2]) return;
        c->pos[0] = p[0];
        c->pos[2] = p[1];
        cfCombatHandler_Centre(h);
        return;
    }

    {
        u8 *lines;
        f32 d3[3], k, seg[4], dir[2], firstDir[2], prevN[2], n[2], save[2], hit[2], v[2], off[2], len;
        s32 clips = 0, again, i;

        if (c->prevPos[0] == c->pos[0] && c->prevPos[2] == c->pos[2]) return;
        d3[0] = c->pos[0] - c->opponent->pos[0];
        d3[1] = c->pos[1] - c->opponent->pos[1];
        d3[2] = c->pos[2] - c->opponent->pos[2];
        len = cfVec3_Length(d3);
        if ((f64)len < 30.0) {
            k = (f32)((f64)len / 30.0);
            k = (f32)((f64)k * 0.01);
        } else {
            k = 1.0f;
        }
        lines = h->v134 + h->arena->linesOffset;
        seg[0] = c->prevPos[0];
        seg[1] = c->prevPos[2];
        seg[2] = c->pos[0];
        seg[3] = c->pos[2];
        for (i = 0; i < h->arena->numLines; i++) lines[i * 0x24 + 0x21] = 0;

        do {
            dir[0] = seg[2] - seg[0];
            dir[1] = seg[3] - seg[1];
            len = cfVec2_Length(dir);
            if ((f64)len <= 0.01 && clips == 0) {
                c->pos[0] = c->prevPos[0];
                c->pos[2] = c->prevPos[2];
                c->screenX = c->prevScreenX;
                return;
            }
            if (len == 0.0f) break;
            dir[0] = dir[0] / len;
            dir[1] = dir[1] / len;
            if (clips == 0) {
                firstDir[0] = dir[0];
                firstDir[1] = dir[1];
            }
            save[0] = seg[2];
            save[1] = seg[3];
            seg[2] = (f32)((f64)seg[2] + (f64)dir[0] * 3.0);
            seg[3] = (f32)((f64)seg[3] + (f64)dir[1] * 3.0);
            len = (f32)((f64)len + 3.0);
            if (clips == 1) {
                seg[2] = (f32)((f64)seg[2] + (f64)dir[0] * 5.0);
                seg[3] = (f32)((f64)seg[3] + (f64)dir[1] * 5.0);
                len = (f32)((f64)len + 5.0);
            }

            again = 0;
            for (i = 0; i < h->arena->numLines; i++) {
                f32 *L = (f32 *)(lines + i * 0x24);
                s32 r = cfSegmentsIntersect(seg, L, hit);
                f32 vl, rem, slide, sh;
                cfCharacter *o;

                if (r == 0) continue;
                if (r == 2) {
                    mpFatalError("CF3COMBT.C : cfCombatHandler_ClipPlayerAgainstBoundingLines() - *** Dude, don't know what to do with CF3LIN2D_COLINEAR intersections!");
                }
                if (0.0 <= (f64)(dir[0] * L[6] + dir[1] * L[7])) continue;

                v[0] = hit[0] - seg[0];
                v[1] = hit[1] - seg[1];
                vl = cfVec2_Length(v);
                if ((f64)vl == 0.0) vl = 0.1f;
                v[0] = v[0] / vl;
                v[1] = v[1] / vl;
                rem = len - vl;
                slide = rem * k;
                sh = (f32)((f64)rem * (1.0 - (f64)k));
                o = c->opponent;
                o->pos[0] = o->pos[0] - sh * v[0];
                c->opponent->pos[1] = c->opponent->pos[1];
                o = c->opponent;
                o->pos[2] = o->pos[2] - sh * v[1];
                k = 1.0f;

                if (0.0 <= (f64)(firstDir[0] * L[4] + firstDir[1] * L[5])) {
                    n[0] = L[4];
                    n[1] = L[5];
                } else {
                    n[0] = -L[4];
                    n[1] = -L[5];
                }
                if (clips > 0 && (f64)(n[0] * prevN[0] + n[1] * prevN[1]) < 0.0) {
                    /* squeezed into a corner */
                    c->pos[0] = c->prevPos[0];
                    c->pos[2] = c->prevPos[2];
                    c->screenX = c->prevScreenX;
                    c->v3C = 1;
                    if (c->v47C != 0 || (f64)c->v478 == 0.0 || c->opponent->seq.waitTimer != 0.0f ||
                        c->opponent->v420 != 0) {
                        c->v478 = 0.0f;
                    } else {
                        c->opponent->v478 = (f32)(-(f64)c->v478);
                        c->opponent->v498 = 0.0f;
                        c->opponent->seq.hasVal6C = 0;
                        c->v478 = 0.0f;
                        c->v478 = 0.0f;
                    }
                    cfCombatHandler_Centre(h);
                    return;
                }
                {
                    f32 e0 = slide * n[0] + hit[0];
                    f32 e1 = slide * n[1] + hit[1];
                    prevN[0] = n[0];
                    prevN[1] = n[1];
                    off[0] = (f32)((f64)L[6] * 0.5);
                    off[1] = (f32)((f64)L[7] * 0.5);
                    seg[0] = hit[0] + off[0];
                    seg[1] = hit[1] + off[1];
                    seg[2] = e0 + off[0];
                    seg[3] = e1 + off[1];
                }
                clips++;
                again = 1;
                lines[i * 0x24 + 0x21] = 1;
                break;
            }
        } while (again);

        if (c->pos[0] == save[0] && c->pos[2] == save[1]) {
            c->v3C = 0;
            return;
        }
        c->pos[0] = save[0];
        c->pos[2] = save[1];
        cfCombatHandler_Centre(h);
        c->v3C = 0;
    }
}

/* ---------------------------------------------------------------- the frame */

void cfCharacter_CheckBlock(cfCharacter *c, u32 buttons);
void cfCharacter_ProjectileImpacts(cfCharacter *c);
void cfCharacter_SequenceEffects(cfCharacter *c);
void cfCharacter_SavePrevious(cfCharacter *c);

/* the fighters must never be at exactly the same screen x */
static void Bump(CombatHandler *h) {
    if (h->fighter1->screenX == h->fighter2->screenX) {
        cfCharacter *b = h->fighter2;
        b->screenX = (f32)((f64)b->screenX + 0.0001);
    }
}

static void Lock(CombatHandler *h) {
    if (h->mutex.owner != osGetThreadId(NULL)) {
        osRecvMesg(&h->mutex.mq, NULL, OS_MESG_BLOCK);
        h->mutex.owner = osGetThreadId(NULL);
    }
    h->mutex.count++;
}

static void Unlock(CombatHandler *h) {
    if (h->mutex.owner == osGetThreadId(NULL)) {
        if (--h->mutex.count == 0) {
            h->mutex.owner = 0;
            osSendMesg(&h->mutex.mq, NULL, OS_MESG_NOBLOCK);
        }
    }
}

static void FighterStep(CombatHandler *h, cfCharacter *const *slot) {
    cfCharacter_Update(*slot);
    Bump(h);
    cfCharacter_UpdateProjectiles(*slot);
    cfCharacter_ProjectileImpacts(*slot);
}

static void ApplySeqMove(cfCharacter *const *slot) {
    cfCharacter *f = *slot;
    f->screenX = (f32)f->seq.velX + f->screenX;
    (*slot)->seq.velX = 0;
    f = *slot;
    f->screenY = (f32)f->seq.velY + f->screenY;
    (*slot)->seq.velY = 0;
}

/* 0x8006E1F8: one frame of a fight. Unless a hit has frozen the action (v2CC4 / v2CC8), both
 * fighters are updated (taking turns at going first), then the controls or the computer
 * player, sequence effects, physics, boxes and pushing apart; then path limits, side-steps,
 * arena boundaries, ring-outs, effects, shadows and the camera. */
void cfCombatHandler_Frame(CombatHandler *h) {
    cfCharacter *a, *b;
    f32 t, gap;
    s32 v;

    func_8006C940(h);
    if (h->v2CC4 != 0.0f) {
        t = h->v2CC4 - h->dt;
        h->v2CC4 = t;
        if (t < 0.0f) h->v2CC4 = 0.0f;
    }
    if (h->v2CC8 != 0.0f) {
        h->v2CC4 = 0.0f;
        t = h->v2CC8 - h->dt;
        h->v2CC8 = t;
        if (t < 0.0f) h->v2CC8 = 0.0f;
    }

    if (h->v2CC4 == 0.0f && h->v2CC8 == 0.0f) {
        cfCharacter_SavePrevious(h->fighter1);
        cfCharacter_SavePrevious(h->fighter2);
        if (h->v2CCC != 0) {
            FighterStep(h, &h->fighter1);
            FighterStep(h, &h->fighter2);
        } else {
            FighterStep(h, &h->fighter2);
            FighterStep(h, &h->fighter1);
        }
        h->v2CCC = (h->v2CCC == 0);
        Bump(h);
        h->v2CC0 = cfCombatHandler_Gap(h);
        if (h->v54 == 4) {
            Lock(h);
            Bump(h);
            if (h->fighter1->v260 != 0) cfAI_Think(h->fighter1);
            if (h->fighter2->v260 != 0) cfAI_Think(h->fighter2);
            Bump(h);
            cfCharacter_HandleInput(h->fighter1, h->pad1, h->pressed1);
            cfCharacter_HandleInput(h->fighter2, h->pad2, h->pressed2);
            cfCharacter_CheckBlock(h->fighter1, h->pad1);
            cfCharacter_CheckBlock(h->fighter2, h->pad2);
            h->pressed1 = 0;
            h->pressed2 = 0;
            Unlock(h);
        }
        cfCharacter_SequenceEffects(h->fighter1);
        cfCharacter_SequenceEffects(h->fighter2);
        ApplySeqMove(&h->fighter1);
        ApplySeqMove(&h->fighter2);
        Bump(h);
        cfCharacter_UpdatePhysics(h->fighter1);
        Bump(h);
        cfCharacter_UpdatePhysics(h->fighter2);
        Bump(h);
        cfCharacter_UpdateRects(h->fighter1);
        cfCharacter_UpdateRects(h->fighter2);
        if ((u32)(h->v54 - 10) >= 2) cfCombatHandler_PushApart(h);
        Bump(h);
    }

    if (h->v4C != 0) {
        /* on a path: the fighters can't get further apart than maxGap */
        a = h->fighter1;
        b = h->fighter2;
        if (a->screenX < b->screenX) {
            gap = b->screenX - a->screenX;
            if (h->arena->maxGap < gap) {
                if (a->screenX < a->prevScreenX) a->screenX = a->prevScreenX;
                b = h->fighter2;
                if (b->prevScreenX < b->screenX) b->screenX = b->prevScreenX;
            }
        } else {
            a = h->fighter1;
            b = h->fighter2;
            gap = a->screenX - b->screenX;
            if (h->arena->maxGap < gap) {
                if (b->screenX < b->prevScreenX) b->screenX = b->prevScreenX;
                a = h->fighter1;
                if (a->prevScreenX < a->screenX) a->screenX = a->prevScreenX;
            }
        }
        Bump(h);
        a = h->fighter1;
        b = h->fighter2;
        if (a->screenX < b->screenX) {
            gap = b->screenX - a->screenX;
            if (h->arena->maxGap < gap) {
                a->screenX = (f32)((f64)a->screenX + (f64)(gap - h->arena->maxGap) / 2.0);
                b = h->fighter2;
                b->screenX = (f32)((f64)b->screenX - (f64)(gap - h->arena->maxGap) / 2.0);
            }
        } else {
            a = h->fighter1;
            b = h->fighter2;
            gap = a->screenX - b->screenX;
            if (h->arena->maxGap < gap) {
                b->screenX = (f32)((f64)b->screenX + (f64)(gap - h->arena->maxGap) / 2.0);
                a = h->fighter1;
                a->screenX = (f32)((f64)a->screenX - (f64)(gap - h->arena->maxGap) / 2.0);
            }
        }
        Bump(h);
    }

    func_800704AC(h);
    Bump(h);
    func_8006C97C(h);
    Bump(h);
    if (h->v4C == 0 && (u32)(h->v54 - 10) >= 2) {
        cfCombatHandler_SideSteps(h);
        Bump(h);
    }
    v = h->v54;
    if (v != 10 && v >= 4 && h->fighter1->v428 == 0 && v != 9) {
        Bump(h);
        if (!(h->v54 == 11 && h->fighter1 == h->v150))
            cfCombatHandler_ClipPlayerAgainstBoundingLines(h, h->fighter1);
        if (!(h->v54 == 11 && h->fighter2 == h->v150))
            cfCombatHandler_ClipPlayerAgainstBoundingLines(h, h->fighter2);
    }
    Bump(h);
    func_8006CABC(h);
    Bump(h);
    if ((u32)(h->v54 - 10) >= 2 && h->fighter1->health > 0 && h->fighter2->health > 0) {
        cfCombatHandler_CheckZones(h);
    } else if (h->v2CC4 == 0.0f) {
        if (h->v54 == 10) func_80074970(h);
        if (h->v54 == 11) func_80074BA0(h);
    }
    Bump(h);
    func_8006CABC(h);
    Fx_UpdateSmoke(h);
    Fx_UpdateBits(h);
    Fx_UpdateMarks(h);
    Fx_UpdateSparks(h);
    Fx_UpdateDust(h);
    Fx_UpdateDebris(h);
    Fx_UpdateBursts(h);
    Fx_UpdateGlows(h);
    Fx_UpdateRings(h);
    Fx_UpdateFlashes(h);
    Fx_UpdateImpactFlash(h);
    Fx_UpdateFade(h);
    Bump(h);
    func_800720B0(h);
    func_800721C4(h);
    if (h->v4C != 0) {
        func_8006CABC(h);
        Bump(h);
        cfCombatHandler_Centre(h);
    }
    Bump(h);
    func_800729EC(h);
}

void cfCharacter_CheckAttackHits(cfCharacter *c);
void cfCharacter_CheckProjectileHits(cfCharacter *c);
void cfCharacter_DeliverHit(cfCharacter *c);

/* how strong a fighter's pending hit is when both land at once: 2 for the special states
 * (v420 / v424 / v428 / v434), 1 for v418 or v42C, else 0 */
static s32 TradeRank(cfCharacter *const *slot) {
    cfCharacter *f = *slot;
    if (f->v424 != 0 || f->v428 != 0 || f->v420 != 0 || f->v434 != 0) return 2;
    f = *slot;
    if (f->v418 != 0) return 1;
    return f->v42C != 0 ? 1 : 0;
}

/* both path clamps of the frame (see cfCombatHandler_Frame) */
static void PathLimits(CombatHandler *h, s32 bumpFirst) {
    cfCharacter *a, *b;
    f32 gap;

    if (bumpFirst) Bump(h);
    a = h->fighter1;
    b = h->fighter2;
    if (a->screenX < b->screenX) {
        gap = b->screenX - a->screenX;
        if (h->arena->maxGap < gap) {
            if (a->screenX < a->prevScreenX) a->screenX = a->prevScreenX;
            b = h->fighter2;
            if (b->prevScreenX < b->screenX) b->screenX = b->prevScreenX;
        }
    } else {
        a = h->fighter1;
        b = h->fighter2;
        gap = a->screenX - b->screenX;
        if (h->arena->maxGap < gap) {
            if (b->screenX < b->prevScreenX) b->screenX = b->prevScreenX;
            a = h->fighter1;
            if (a->prevScreenX < a->screenX) a->screenX = a->prevScreenX;
        }
    }
    Bump(h);
    a = h->fighter1;
    b = h->fighter2;
    if (a->screenX < b->screenX) {
        gap = b->screenX - a->screenX;
        if (h->arena->maxGap < gap) {
            a->screenX = (f32)((f64)a->screenX + (f64)(gap - h->arena->maxGap) / 2.0);
            b = h->fighter2;
            b->screenX = (f32)((f64)b->screenX - (f64)(gap - h->arena->maxGap) / 2.0);
        }
    } else {
        a = h->fighter1;
        b = h->fighter2;
        gap = a->screenX - b->screenX;
        if (h->arena->maxGap < gap) {
            b->screenX = (f32)((f64)b->screenX + (f64)(gap - h->arena->maxGap) / 2.0);
            a = h->fighter1;
            a->screenX = (f32)((f64)a->screenX - (f64)(gap - h->arena->maxGap) / 2.0);
        }
    }
    Bump(h);
}

/* 0x8006EDD8: the other kind of frame: no controller input (computer players only), and the
 * hits are worked out here - when both fighters hit each other at once, the stronger kind of
 * hit wins, then the stronger hit, then a coin toss. No hit-freeze countdown either. */
void cfCombatHandler_FrameNoInput(CombatHandler *h) {
    s32 v;

    func_8006C940(h);
    if (h->v2CC4 == 0.0f && h->v2CC8 == 0.0f) {
        cfCharacter *a, *b;

        cfCharacter_SavePrevious(h->fighter1);
        cfCharacter_SavePrevious(h->fighter2);
        Bump(h);
        if (h->fighter1->v260 != 0) cfAI_Think(h->fighter1);
        if (h->fighter2->v260 != 0) cfAI_Think(h->fighter2);
        Bump(h);
        cfCharacter_UpdateProjectiles(h->fighter1);
        cfCharacter_ProjectileImpacts(h->fighter1);
        cfCharacter_SequenceEffects(h->fighter1);
        cfCharacter_UpdateProjectiles(h->fighter2);
        cfCharacter_ProjectileImpacts(h->fighter2);
        cfCharacter_SequenceEffects(h->fighter2);
        Bump(h);
        cfCharacter_UpdatePhysics(h->fighter1);
        Bump(h);
        cfCharacter_UpdatePhysics(h->fighter2);
        Bump(h);
        cfCharacter_UpdateRects(h->fighter1);
        cfCharacter_UpdateRects(h->fighter2);
        if ((u32)(h->v54 - 10) >= 2) cfCombatHandler_PushApart(h);
        Bump(h);
        Bump(h);
        cfCharacter_CheckAttackHits(h->fighter1);
        cfCharacter_CheckAttackHits(h->fighter2);
        cfCharacter_CheckProjectileHits(h->fighter1);
        cfCharacter_CheckProjectileHits(h->fighter2);
        Bump(h);

        a = h->fighter1;
        if (a->hitType != -1 && a->hitByProjectile == 0) {
            b = h->fighter2;
            if (b->hitType != -1 && b->hitByProjectile == 0) {
                s32 ra, rb;
                if (a->screenX == b->screenX) b->screenX = (f32)((f64)b->screenX + 0.0001);
                ra = TradeRank(&h->fighter1);
                rb = TradeRank(&h->fighter2);
                if (rb < ra) {
                    h->fighter2->hitType = -1;
                } else if (ra < rb) {
                    h->fighter1->hitType = -1;
                } else if (h->fighter2->hitE4 < h->fighter1->hitE4) {
                    h->fighter2->hitType = -1;
                } else if (h->fighter1->hitE4 < h->fighter2->hitE4) {
                    h->fighter1->hitType = -1;
                } else if ((s32)guRandom() % 100 < 50) {
                    h->fighter1->hitType = -1;
                } else {
                    h->fighter2->hitType = -1;
                }
            }
        }
        Bump(h);
        cfCharacter_DeliverHit(h->fighter1);
        cfCharacter_DeliverHit(h->fighter2);
        Bump(h);
    }

    if (h->v4C != 0) PathLimits(h, 1);
    Bump(h);
    func_800704AC(h);
    Bump(h);
    func_8006C97C(h);
    Bump(h);
    if (h->v4C == 0 && (u32)(h->v54 - 10) >= 2) cfCombatHandler_SideSteps(h);
    Bump(h);
    v = h->v54;
    if (v != 10 && v >= 4 && h->fighter1->v428 == 0 && v != 9) {
        if (!(h->v54 == 11 && h->fighter1 == h->v150))
            cfCombatHandler_ClipPlayerAgainstBoundingLines(h, h->fighter1);
        if (!(h->v54 == 11 && h->fighter2 == h->v150))
            cfCombatHandler_ClipPlayerAgainstBoundingLines(h, h->fighter2);
    }
    Bump(h);
    func_8006CABC(h);
    Bump(h);
    if ((u32)(h->v54 - 10) >= 2 && h->fighter1->health > 0 && h->fighter2->health > 0) {
        cfCombatHandler_CheckZones(h);
    } else if (h->v2CC4 == 0.0f) {
        if (h->v54 == 10) {
            func_80074970(h);
            Bump(h);
        }
        if (h->v54 == 11) {
            func_80074BA0(h);
            Bump(h);
        }
    }
    Bump(h);
    func_8006CABC(h);
    Bump(h);
    Fx_UpdateSmoke(h);
    Fx_UpdateBits(h);
    Fx_UpdateMarks(h);
    Fx_UpdateSparks(h);
    Fx_UpdateDust(h);
    Fx_UpdateDebris(h);
    Fx_UpdateBursts(h);
    Fx_UpdateGlows(h);
    Fx_UpdateRings(h);
    Fx_UpdateFlashes(h);
    Fx_UpdateImpactFlash(h);
    Fx_UpdateFade(h);
    Bump(h);
    func_800720B0(h);
    func_800721C4(h);
    if (h->v4C != 0) {
        Bump(h);
        func_8006CABC(h);
        cfCombatHandler_Centre(h);
    }
    Bump(h);
    func_800729EC(h);
}

/* 0x8006BB80: after a ring-out the fight carries on in another part of the arena (arena
 * number arenaIndex of the BkgDef): both fighters are placed at zone zoneIndex's entry point
 * (pulled back inside the bounding lines if needed) - the one knocked out flying or falling
 * in, the other one following. */
void cfCombatHandler_EnterArena(CombatHandler *h, u8 *bkg, s32 arenaIndex, s32 zoneIndex) {
    ArenaZone *z;
    u8 *lines;
    f32 c[2], seg[4], hit[2], dir = 1.0f;
    s32 seqA = -1, seqB = -1;  /* (left unset by the original for unknown zone types) */
    s32 i;
    cfCharacter *a, *v;

    mpMemSet(h->smoke, sizeof(h->smoke), 0);
    mpMemSet(h->marks, sizeof(h->marks), 0);
    h->v140 = -1;
    h->v144 = 0;
    h->v148 = 0;
    h->v14C = 0;
    h->v16C = 0;
    h->v134 = bkg;
    if (((((const volatile u8 *)bkg)[0] << 8) | ((const volatile u8 *)bkg)[1]) != 0x040A) {   /* two version bytes */
        mpFatalErrorf("CF3COMBT.C - Attempt to initialize with BkgDef version %d.%02d - expecting version %d.%02d",
                      bkg[0], bkg[1], 4, 10);
    }
    h->arenaIndex = arenaIndex;
    h->arena = (ArenaDef *)(h->v134 + *(s32 *)(h->v134 + 0x3C) + arenaIndex * 0x60);
    h->v140 = -1;
    z = (ArenaZone *)(h->v134 + h->arena->zonesOffset) + zoneIndex;
    h->zoneType = z->type;
    c[0] = z->x;
    c[1] = z->z;
    lines = h->v134 + h->arena->linesOffset;
    seg[0] = z->x;
    seg[1] = z->z;
    seg[2] = (f32)((f64)z->x + (f64)z->nx * ((f64)z->radius * 1.1));
    seg[3] = (f32)((f64)z->z + (f64)z->nz * ((f64)z->radius * 1.1));
    for (i = 0; i < h->arena->numLines; i++) {
        if (cfSegmentsIntersect(seg, (f32 *)(lines + i * 0x24), hit) == 1) {
            c[0] = (f32)((f64)hit[0] + ((f64)z->nx + (f64)z->nx));
            c[1] = (f32)((f64)hit[1] + ((f64)z->nz + (f64)z->nz));
            break;
        }
    }

    a = h->v150;
    a->pos[0] = (f32)((f64)c[0] + -(f64)z->nx);
    h->v150->pos[1] = 0.0f;
    a = h->v150;
    a->pos[2] = (f32)((f64)c[1] + -(f64)z->nz);
    h->v150->prevPos[0] = c[0];
    h->v150->prevPos[1] = (f32)h->floorY;
    h->v150->prevPos[2] = c[1];
    v = h->v154;
    v->pos[0] = (f32)((f64)c[0] + (f64)z->nx * 256.0);
    h->v154->pos[1] = 0.0f;
    v = h->v154;
    v->pos[2] = (f32)((f64)c[1] + (f64)z->nz * 256.0);
    h->v154->prevPos[0] = c[0];
    h->v154->prevPos[1] = (f32)h->floorY;
    h->v154->prevPos[2] = c[1];
    h->v150->screenX = z->pathT;
    h->v154->screenX = (f32)((f64)z->pathT + 10.0);
    h->fighter1->screenY = (f32)h->floorY;
    h->fighter2->screenY = (f32)h->floorY;
    h->v150->v46C = 0.0f;
    h->v150->v470 = 0.0f;
    h->v150->v474 = 0.0f;
    h->v150->v478 = 0.0f;
    h->v150->v480 = 0.0f;
    h->v150->v484 = 0.0f;
    h->v150->v488 = 0.0f;
    h->v150->seq.waitTimer = 0.0f;
    h->v150->screenY = 0.0f;
    h->v154->v470 = 0.0f;
    h->v154->v474 = 0.0f;
    h->v154->v484 = 0.0f;
    h->v154->v488 = 0.0f;
    h->v154->seq.waitTimer = 0.0f;
    h->v154->screenY = 0.0f;
    h->v1A0 = 0.0f;
    cfCombatHandler_Centre(h);

    if (z->type == 0) {
        /* through a wall: the winner walks in after the loser, who lands on the floor */
        dir = h->v154->facing ? -1.0f : 1.0f;
        a = h->v150;
        a->pos[0] = (f32)((f64)c[0] + (f64)z->nx * -30.0);
        h->v150->pos[1] = 0.0f;
        a = h->v150;
        a->pos[2] = (f32)((f64)c[1] + (f64)z->nz * -30.0);
        h->v150->prevPos[0] = c[0];
        h->v150->prevPos[1] = (f32)h->floorY;
        h->v150->prevPos[2] = c[1];
        h->v190 = c[0] - h->v150->pos[0];
        h->v194 = 0.0f;
        h->v198 = c[1] - h->v150->pos[2];
        h->v19C = cfVec3_Length(&h->v190);
        h->v190 = h->v190 / h->v19C;
        h->v194 = h->v194 / h->v19C;
        h->v198 = h->v198 / h->v19C;
        h->v1A0 = (f32)((f64)h->v150->stats[2] * 1.5);
        seqA = 8;
        v = h->v154;
        v->pos[0] = (f32)((f64)c[0] + (f64)z->nx * 180.0);
        h->v154->pos[1] = 0.0f;
        v = h->v154;
        v->pos[2] = (f32)((f64)c[1] + (f64)z->nz * 180.0);
        h->v154->prevPos[0] = c[0];
        h->v154->prevPos[1] = (f32)h->floorY;
        h->v154->prevPos[2] = c[1];
        h->v154->screenY = -80.0f;
        h->v154->v470 = (f32)((f64)dir * -6.0);
        h->v154->v46C = 0.0f;
        h->v154->v48C = -0.8f;
        h->v154->v2B4 = 15.0f;
        seqB = -1;
        h->vB8 = 0;
    } else if (z->type > 0 && z->type < 3) {
        /* from above: both drop in */
        seqA = 0x11;
        h->v150->screenY = -520.0f;
        h->v150->v46C = -0.5f;
        h->v150->seq.waitTimer = 0.0f;
        v = h->v154;
        v->pos[0] = (f32)((f64)c[0] + (f64)z->nx * 70.0);
        h->v154->pos[1] = 0.0f;
        v = h->v154;
        v->pos[2] = (f32)((f64)c[1] + (f64)z->nz * 70.0);
        h->v154->prevPos[0] = c[0];
        h->v154->prevPos[1] = (f32)h->floorY;
        h->v154->prevPos[2] = c[1];
        h->v154->screenY = -470.0f;
        h->v154->v46C = 2.5f;
        h->v154->v470 = 13.0f;
        h->v154->v48C = -2.0f;
        seqB = -1;
        h->vB8 = (h->v154 == h->fighter1) ? 1 : 2;
    }

    for (i = 0; i < 10; i++) {
        v = h->v154;
        cfCharacter_EmitSmoke(h->v154, v->pos[0], v->pos[1], v->pos[2], v->v40, i % 3);
    }
    {
        s32 n = (s32)guRandom() % 3;
        v = h->v154;
        cfCharacter_EmitBits(h->v154, n + 1, v->pos[0], v->pos[1], v->pos[2]);
    }
    cfCombatHandler_Centre(h);
    if (seqA != -1) cfCharacter_PlaySequence(h->v150, seqA, -1, 1, (u32)-1);
    if (seqB != -1) cfCharacter_PlaySequence(h->v154, seqB, -1, 1, (u32)-1);
    func_800720B0(h);
    h->fighter1->v24[0] = h->fighter1->v18[0];
    h->fighter1->v24[1] = h->fighter1->v18[1];
    h->fighter1->v24[2] = h->fighter1->v18[2];
    h->fighter2->v24[0] = h->fighter2->v18[0];
    h->fighter2->v24[1] = h->fighter2->v18[1];
    h->fighter2->v24[2] = h->fighter2->v18[2];
    func_800729EC(h);
    h->v50 = -1;
    h->v54 = 11;
}

/* 0x80074EA0: v scaled by the length of the last frame (gFrameTicks, 32 = one normal frame) */
f32 ScaleByFrameTime(f32 v) {
    extern s32 gFrameTicks;
    return v * (f32)gFrameTicks / 32.0f;
}

/* ---------------------------------------------------------------- the camera */

/* the camera's sideways vector: camDir x (0, 1, 0), written out as the original computes it */
static void CamSide(CombatHandler *h, f32 *side) {
    f32 u = 0.0f;
    side[0] = h->camDirZ - h->camDirY * u;
    side[1] = h->camDirX * u - h->camDirZ * u;
    side[2] = h->camDirY * u - h->camDirX;
}

/* modes 0-2: both fighters in view; the further apart, the further back (and higher) */
static void CamBoth(CombatHandler *h, const f32 *a, const f32 *b, f64 k, s32 lookAt) {
    f32 side[3], mid[3], d[3], zoom;

    CamSide(h, side);
    mid[0] = (f32)((f64)(a[0] + b[0]) / 2.0);
    mid[1] = (f32)((f64)(a[1] + b[1]) / 2.0);
    mid[2] = (f32)((f64)(a[2] + b[2]) / 2.0);
    d[0] = a[0] - b[0];
    d[1] = a[1] - b[1];
    d[2] = a[2] - b[2];
    zoom = (f32)((f64)(f32)((f64)cfVec3_Length(d) / 2.0) * k);
    if ((f64)zoom < 740.0) zoom = 740.0f;
    h->camEye[0] = (f32)((f64)mid[0] + -(f64)side[0] * (f64)zoom);
    h->camEye[1] = (f32)((f64)mid[1] + -(f64)side[1] * (f64)zoom);
    h->camEye[2] = (f32)((f64)mid[2] + -(f64)side[2] * (f64)zoom);
    h->camEye[1] = h->camEye[1] + h->arena->camHeight;
    h->camEye[1] = (f32)((f64)h->camEye[1] + ((f64)zoom - 740.0) / (f64)h->arena->camZoomDiv);
    if (lookAt == 0) {
        h->camAt[0] = mid[0]; h->camAt[1] = mid[1]; h->camAt[2] = mid[2];
    } else if (lookAt == 1) {
        h->camAt[0] = a[0]; h->camAt[1] = a[1]; h->camAt[2] = a[2];
    } else {
        h->camAt[0] = b[0]; h->camAt[1] = b[1]; h->camAt[2] = b[2];
    }
    h->camAt[1] = (f32)((f64)h->camAt[1] + 170.0);
}

/* modes 3 and 4: a close-up of one fighter */
static void CamClose(CombatHandler *h, const f32 *p) {
    f32 side[3], t[3];

    CamSide(h, side);
    t[0] = p[0];
    t[1] = (f32)((f64)p[1] + 127.0);
    t[2] = p[2];
    h->camEye[0] = (f32)((f64)t[0] + -(f64)side[0] * 740.0);
    h->camEye[1] = (f32)((f64)t[1] + -(f64)side[1] * 740.0);
    h->camEye[2] = (f32)((f64)t[2] + -(f64)side[2] * 740.0);
    h->camAt[0] = t[0];
    h->camAt[1] = t[1];
    h->camAt[2] = t[2];
}

/* modes 6 and 7: closer still, a little in front of one fighter */
static void CamFace(CombatHandler *h, const f32 *p, cfCharacter *f) {
    f32 side[3], t[3], dir = 1.0f;

    CamSide(h, side);
    if (f->facing) dir = -1.0f;
    t[0] = (f32)((f64)p[0] + (f64)h->camDirX * 40.0 * (f64)dir);
    t[1] = (f32)((f64)p[1] + 127.0);
    t[2] = (f32)((f64)p[2] + (f64)h->camDirZ * 40.0 * (f64)dir);
    h->camEye[0] = (f32)((f64)t[0] + -(f64)side[0] * 300.0);
    h->camEye[1] = (f32)((f64)t[1] + -(f64)side[1] * 300.0);
    h->camEye[2] = (f32)((f64)t[2] + -(f64)side[2] * 300.0);
    h->camEye[1] = (f32)((f64)h->camEye[1] - 70.0);
    h->camAt[0] = t[0];
    h->camAt[1] = t[1];
    h->camAt[2] = t[2];
}

/* |dz| < |dx| for the "which axis to measure along" choice, as doubles */
static s32 AlongX(f32 dx, f32 dz) {
    f64 ax = (f64)dx < 0.0 ? 0.0 - (f64)dx : (f64)dx;
    f64 az = (f64)dz < 0.0 ? 0.0 - (f64)dz : (f64)dz;
    return az < ax;
}

/* 0x800729EC: where the camera wants to be (by camera mode vB8), then the arena's height
 * limits push its target height up where the view would cut through scenery */
void func_800729EC(CombatHandler *h) {
    f32 a[3], b[3], p[2];
    u8 *ceil;
    f32 seg[4], hit[2], eyeY, zero;
    s32 i;

    if (h->v54 < 2) {
        if (h->v4C == 0) {
            a[0] = h->fighter1->v248 * h->camDirX + h->arena->start1[0];
            a[2] = h->fighter1->v248 * h->camDirZ + h->arena->start1[1];
            b[0] = h->arena->start2[0] - h->fighter2->v248 * h->camDirX;
            b[2] = h->arena->start2[1] - h->fighter2->v248 * h->camDirZ;
        } else {
            cfCombatHandler_PathPoint(h, h->arena->startT1, p);
            a[0] = h->fighter1->v248 * h->camDirX + p[0];
            a[2] = h->fighter1->v248 * h->camDirZ + p[1];
            cfCombatHandler_PathPoint(h, h->arena->startT2, p);
            b[0] = h->fighter2->v248 * h->camDirX + p[0];
            b[2] = h->fighter2->v248 * h->camDirZ + p[1];
        }
        a[1] = 0.0f;
        b[1] = 0.0f;
    } else {
        a[0] = h->fighter1->v24[0];
        a[1] = h->fighter1->v24[1];
        a[2] = h->fighter1->v24[2];
        b[0] = h->fighter2->v24[0];
        b[1] = h->fighter2->v24[1];
        b[2] = h->fighter2->v24[2];
    }
    if (h->vBC != 0) return;

    switch ((u32)h->vB8) {
    case 0: CamBoth(h, a, b, 3.8, 0); break;
    case 1: CamBoth(h, a, b, 3.5, 1); break;
    case 2: CamBoth(h, a, b, 3.5, 2); break;
    case 3: CamClose(h, a); break;
    case 4: CamClose(h, b); break;
    case 5:
        h->camEyeTarget[0] = h->v178[0];
        h->camEyeTarget[1] = h->v178[1];
        h->camEyeTarget[2] = h->v178[2];
        h->camAtTarget[0] = h->v184[0];
        h->camAtTarget[1] = h->v184[1];
        h->camAtTarget[2] = h->v184[2];
        func_80072568(h);
        break;
    case 6: CamFace(h, a, h->fighter1); break;
    case 7: CamFace(h, b, h->fighter2); break;
    default:
        mpFatalError("CF3COMBT.C - *** Bad camera mode!");
        break;
    }

    ceil = h->v134 + h->arena->ceilingsOffset;
    seg[0] = h->camEye[0];
    seg[1] = h->camEye[2];
    seg[2] = h->camAt[0];
    seg[3] = h->camAt[2];
    eyeY = h->camEye[1];
    zero = 0.0f;
    for (i = 0; i < h->arena->numCeilings; i++) {
        f32 *c = (f32 *)(ceil + i * 0x18);
        f32 t, u, hgt, y;

        if (cfSegmentsIntersect(seg, c, hit) != 1) continue;
        if (AlongX(c[2] - c[0], c[3] - c[1])) t = (hit[0] - c[0]) / (c[2] - c[0]);
        else                                 t = (hit[1] - c[1]) / (c[3] - c[1]);
        hgt = (c[5] - c[4]) * t + c[4];
        if (AlongX(seg[2] - seg[0], seg[3] - seg[1])) u = (hit[0] - seg[0]) / (seg[2] - seg[0]);
        else                                         u = (hit[1] - seg[1]) / (seg[3] - seg[1]);
        y = (zero - eyeY) * u + eyeY;
        if (y < hgt) {
            f32 k = (f32)((f64)u + (f64)u);
            if (1.0 < (f64)k) k = 1.0f;
            eyeY = eyeY + (hgt - y) / u * k;
        }
    }
    h->vDC = eyeY - h->camEye[1];
    func_800728B4(h);
}

/* ---------------------------------------------------------------- the round */

/* camera spline helpers (still in assembly) */
void cfCharacter_ReturnToNeutral(cfCharacter *c);

/* round start: a fighter walks in to its start position (seq 0x3C or 6) */
static void WalkIn(CombatHandler *h, cfCharacter *const *slot, f32 *left, s32 which) {
    cfCharacter *f = *slot;
    f32 t, p[2];

    if (f->seqIndex != 0x3C && f->seqIndex != 6) return;
    t = *left - ScaleByFrameTime(f->stats[0]);
    *left = t;
    if (t < 0.0f) *left = 0.0f;
    if (h->v4C == 0) {
        if (which == 1) {
            (*slot)->pos[0] = h->arena->start1[0] - h->camDirX * *left;
            (*slot)->pos[2] = h->arena->start1[1] - h->camDirZ * *left;
        } else {
            (*slot)->pos[0] = h->camDirX * *left + h->arena->start2[0];
            (*slot)->pos[2] = h->camDirZ * *left + h->arena->start2[1];
        }
    } else {
        cfCombatHandler_PathPoint(h, which == 1 ? h->arena->startT1 : h->arena->startT2, p);
        if (which == 1) {
            (*slot)->pos[0] = p[0] - h->camDirX * *left;
            (*slot)->pos[2] = p[1] - h->camDirZ * *left;
        } else {
            (*slot)->pos[0] = h->camDirX * *left + p[0];
            (*slot)->pos[2] = h->camDirZ * *left + p[1];
        }
    }
    f = *slot;
    f->pos[1] = 0.0f;
    if (*left <= f->pos[1]) {
        if ((*slot)->seqIndex == 0x3C) cfCharacter_PlaySequence(*slot, 0x3E, 1, 1, (u32)-1);
        else                           cfCharacter_PlaySequence(*slot, 0, -1, 1, (u32)-1);
    }
}

/* round start: the walk-in (seq 6), or an intro pose (0x3C, then 0x3E) when the fighter has one */
static void StartWalkIn(CombatHandler *h, cfCharacter *const *slot, f32 *left, s32 which) {
    cfCharacter *f;
    if (h->v44 == which) {
        f = *slot;
        if (f->numSeqs >= 0x3D && f->seqOffsets[0x3C] != 0) {
            cfCharacter_PlaySequence(*slot, 0x3C, -1, 1, (u32)-1);
            *left = 200.0f;
        } else {
            cfCharacter_PlaySequence(*slot, 0x3E, 1, 1, (u32)-1);
        }
    } else {
        cfCharacter_PlaySequence(*slot, 6, -1, 1, (u32)-1);
        *left = 200.0f;
    }
}

/* one of the winner's victory sequences (0x38-0x3A) that it really has */
static void VictoryPose(CombatHandler *h) {
    s32 k;
    do {
        do {
            k = (s32)guRandom() % 3 + 0x38;
        } while (!(k < h->v3C->numSeqs));
    } while (h->v3C->seqOffsets[k] == 0);
    cfCharacter_PlaySequence(h->v3C, k, 1, 1, (u32)-1);
}

/* the score for deciding a time-up: 170 a round still to lose (of 10), plus the health left */
static s32 TimeUpScore(cfCharacter *f) {
    return (10 - f->v2C8) * 170 + f->health;
}

static void Winner(CombatHandler *h) {
    h->v5C = (h->v3C == h->fighter1) ? 1 : 2;
    h->v58 = 1;
}

/* 0x8006CAEC: runs one frame of the fight and its round flow (state v54):
 *  0 camera fly-through   1 fighters walk in / intro poses   2 wait (v7C)   3 / 4 fighting
 *  5 fighters too close after a round: step back   6 victory pose   7 / 8 short waits, then the
 *  winner is announced   9 fade out after a knock-out   10 / 11 ring-out (in the frame code)
 *  12 time up: the winner on rounds and health   13 / 14 waits and the victory pose   15 the
 *  loser's sequence 0x3B.  The frames alternate between the two kinds of frame. */
void cfCombatHandler_Run(CombatHandler *h) {
    cfCharacter *f;
    s32 changed;
    f64 t;

    if (h->v54 == 9) h->v4C = 0;
    if (h->v4C != 0) {
        while ((f64)h->fighter1->screenX < 0.0 && (f64)h->fighter2->screenX < 0.0) {
            h->fighter1->screenX = h->fighter1->screenX + h->arena->pathLength;
            h->fighter2->screenX = h->fighter2->screenX + h->arena->pathLength;
        }
        while (h->arena->pathLength < h->fighter1->screenX && h->arena->pathLength < h->fighter2->screenX) {
            h->fighter1->screenX = h->fighter1->screenX - h->arena->pathLength;
            h->fighter2->screenX = h->fighter2->screenX - h->arena->pathLength;
        }
    }
    Bump(h);
    if (h->frameCount & 1) cfCombatHandler_FrameNoInput(h);
    else                   cfCombatHandler_Frame(h);

    /* which fighter is drawn in front */
    if (h->fighter1->v460 != 0 && h->fighter2->v460 == 0) {
        h->fighter1->inFront = 1;
        h->fighter2->inFront = 0;
    }
    if (h->fighter2->v460 != 0 && h->fighter1->v460 == 0) {
        h->fighter2->inFront = 1;
        h->fighter1->inFront = 0;
    }
    if (h->fighter1->v464 != 0 && h->fighter2->v464 == 0) {
        h->fighter1->inFront = 0;
        h->fighter2->inFront = 1;
    }
    if (h->fighter2->v464 != 0 && h->fighter1->v464 == 0) {
        h->fighter2->inFront = 0;
        h->fighter1->inFront = 1;
    }

    /* the round clock */
    t = (f64)h->v34;
    if (1.0 < t && h->v54 == 4 && h->v38 == 0 && h->fighter1->health > 0 && h->fighter2->health > 0) {
        h->v34 = (f32)(t - (f64)h->dt * 0.0074);
        if ((f64)h->v34 <= 1.0) h->v34 = 0.0f;
    }
    if (h->v34 == 0.0f && h->v38 == 0 && h->v54 == 4) h->v54 = 12;
    if ((s32)h->v34 == 15) h->v2CAC = 1;

    changed = (h->v54 != h->v50);
    h->v50 = h->v54;
    switch ((u32)h->v54) {
    case 0: {
        /* the camera flies through the arena along a curve */
        f32 speed = h->v104, look[3], d[3], step;
        u8 *path;
        s32 n;

        if (changed) {
            h->v68 = 0;
            if (h->arena->hasIntro == 0) {
                h->v54++;
                h->stateTimer = 30.0f;
                break;
            }
            h->introPath = h->v134 + h->arena->introOffset;
            func_800729EC(h);
            h->introEye[0] = h->camEye[0];
            h->introEye[1] = h->camEye[1];
            h->introEye[2] = h->camEye[2];
            h->introAt[0] = h->camAt[0];
            h->introAt[1] = h->camAt[1];
            h->introAt[2] = h->camAt[2];
            h->v100 = 0.0001f;
            h->v104 = *(f32 *)(h->introPath + 4);
            /* the curve ends where the fight camera starts */
            n = *(s32 *)h->introPath;
            *(f32 *)(h->introPath + 0x20 + n * 12) = h->introEye[0];
            n = *(s32 *)h->introPath;
            *(f32 *)(h->introPath + 0x24 + n * 12) = h->introEye[1];
            n = *(s32 *)h->introPath;
            *(f32 *)(h->introPath + 0x28 + n * 12) = h->introEye[2];
        }
        if (h->v108 != 0) speed = (f32)((f64)speed + (f64)speed);
        speed = ScaleByFrameTime(speed);
        path = h->introPath;
        Spline_Point((f32 *)(path + 0x20), *(s32 *)path + 1, h->v100, h->camEye, 0);
        path = h->introPath;
        if (*(s8 *)(path + 8) == 0) {
            /* look ahead along the curve */
            step = Spline_Advance((f32 *)(path + 0x20), *(s32 *)path + 1, h->v100, *(f32 *)(path + 4), 0);
            path = h->introPath;
            Spline_Point((f32 *)(path + 0x20), *(s32 *)path + 1, step, look, 0);
            d[0] = look[0] - h->camEye[0];
            d[1] = look[1] - h->camEye[1];
            d[2] = look[2] - h->camEye[2];
            look[0] = (f32)((f64)h->camEye[0] + (f64)d[0] * 500.0);
            look[1] = (f32)((f64)h->camEye[1] + (f64)d[1] * 500.0);
            look[2] = (f32)((f64)h->camEye[2] + (f64)d[2] * 500.0);
        } else if (*(s8 *)(path + 8) == 1) {
            look[0] = h->introAt[0];
            look[1] = h->introAt[1];
            look[2] = h->introAt[2];
        } else {
            look[0] = *(f32 *)(h->introPath + 0xC);
            look[1] = *(f32 *)(h->introPath + 0x10);
            look[2] = *(f32 *)(h->introPath + 0x14);
        }
        {
            f32 start = *(f32 *)(h->introPath + 0x18);
            if (start < h->v100) {
                /* towards the end, turn to look where the fight camera looks */
                f32 k = (f32)((f64)(h->v100 - start) / (1.0 - (f64)start));
                look[0] = (f32)((f64)look[0] * (1.0 - (f64)k) + (f64)(k * h->introAt[0]));
                look[1] = (f32)((f64)look[1] * (1.0 - (f64)k) + (f64)(k * h->introAt[1]));
                look[2] = (f32)((f64)look[2] * (1.0 - (f64)k) + (f64)(k * h->introAt[2]));
            }
        }
        h->camAt[0] = look[0];
        h->camAt[1] = look[1];
        h->camAt[2] = look[2];
        path = h->introPath;
        h->v100 = Spline_Advance((f32 *)(path + 0x20), *(s32 *)path + 1, h->v100, speed, 0);
        if (0.999 <= (f64)h->v100) {
            h->v54++;
            h->stateTimer = 0.0f;
        }
        break;
    }
    case 2:
        if (h->v7C != 0) {
            h->v54++;
            h->stateTimer = 0.0f;
        }
        break;
    case 1:
        if (h->v4C != 0) {
            h->fighter1->screenX = h->arena->startT1;
            h->fighter2->screenX = h->arena->startT2;
        }
        if (changed) {
            StartWalkIn(h, &h->fighter1, &h->v60, 1);
            StartWalkIn(h, &h->fighter2, &h->v64, 2);
        } else {
            h->v68 = 1;
        }
        WalkIn(h, &h->fighter1, &h->v60, 1);
        WalkIn(h, &h->fighter2, &h->v64, 2);
        cfCombatHandler_Centre(h);
        f = h->fighter1;
        if (f->seqIndex == 0x3E && f->seq.seq == NULL) cfCharacter_PlaySequence(h->fighter1, 0, -1, 1, (u32)-1);
        f = h->fighter2;
        if (f->seqIndex == 0x3E && f->seq.seq == NULL) cfCharacter_PlaySequence(h->fighter2, 0, -1, 1, (u32)-1);
        if (h->fighter1->seqIndex == 0 && h->fighter2->seqIndex == 0) h->v54++;
        break;
    case 4:
        if (!changed) break;
        /* fall through */
    case 3:
        cfCharacter_ReturnToNeutral(h->fighter1);
        cfCharacter_ReturnToNeutral(h->fighter2);
        break;
    case 5:
        if (changed) {
            h->v2CD4 = 0;
            cfCharacter_ReturnToNeutral(h->fighter1);
            cfCharacter_ReturnToNeutral(h->fighter2);
        }
        cfCharacter_ClearMoveState(h->fighter1);
        cfCharacter_ClearMoveState(h->fighter2);
        h->fighter1->v2C0 = 0.0f;
        h->fighter2->v2C0 = 0.0f;
        if (h->v2CC0 < 200) {
            /* too close: step back */
            f = h->fighter1;
            if ((f->isThrowSeq == 0 && f->seqIndex == 0) || h->fighter1->seqIndex == 0x27)
                cfCharacter_PlaySequence(h->fighter1, 7, -1, 1, (u32)-1);
            f = h->fighter2;
            if ((f->isThrowSeq == 0 && f->seqIndex == 0) || h->fighter1->seqIndex == 0x27)   /* (sic) */
                cfCharacter_PlaySequence(h->fighter2, 7, -1, 1, (u32)-1);
        } else {
            s32 s1 = h->fighter1->seqIndex, s2;
            if (s1 != 7 && s1 != 0) break;
            s2 = h->fighter2->seqIndex;
            if (s2 != 7 && s2 != 0) break;
            if (s1 != 0) cfCharacter_PlaySequence(h->fighter1, 0, -1, 1, (u32)-1);
            if (h->fighter2->seqIndex != 0) cfCharacter_PlaySequence(h->fighter2, 0, -1, 1, (u32)-1);
            h->v54 = 4;
        }
        break;
    case 6: {
        s32 s;
        cfCharacter_ReturnToNeutral(h->fighter1);
        cfCharacter_ReturnToNeutral(h->fighter2);
        s = h->v3C->seqIndex;
        if ((u32)(s - 0x38) < 2 || s == 0x3A) goto posed;
        if (s != 0) break;
        if (h->v40->seqIndex != 0x25) break;
        VictoryPose(h);
        break;
    }
    case 7:
        if (changed) h->stateTimer = 90.0f;
        h->stateTimer = h->stateTimer - h->dt;
        if (h->stateTimer <= 0.0f) Winner(h);
        break;
    case 8:
        if (changed) h->stateTimer = 120.0f;
        h->stateTimer = h->stateTimer - h->dt;
        if (h->stateTimer <= 0.0f && h->v6C != 0) Winner(h);
        break;
    case 9: {
        cfCharacter *w;
        if (changed) {
            h->v2C84 = 1;
            h->v2C88 = 0.0f;
            h->v78 = 1;
        }
        w = h->v3C;
        if (w->seq.seq == NULL ||
            (w->seq.finished != 0 && w->v3F4 < 0 && !(w->v3EC < w->numLinks))) {
            if ((f32)h->floorY <= h->v3C->screenY) {
                h->v54 = 8;
                break;
            }
        }
        if (h->v70 != 0) h->v54 = 8;
        break;
    }
    case 12:
        if (changed) {
            if (TimeUpScore(h->fighter2) < TimeUpScore(h->fighter1)) {
                h->v3C = h->fighter1;
                h->v40 = h->fighter2;
            } else if (TimeUpScore(h->fighter1) < TimeUpScore(h->fighter2)) {
                h->v3C = h->fighter2;
                h->v40 = h->fighter1;
            } else {
                h->v3C = ((s32)guRandom() % 1000 < 500) ? h->fighter1 : h->fighter2;
                h->v40 = (h->v3C != h->fighter1) ? h->fighter1 : h->fighter2;
            }
            h->v2CB0 = 1;
        }
        cfCharacter_ReturnToNeutral(h->fighter1);
        cfCharacter_ReturnToNeutral(h->fighter2);
        if (h->fighter1->seqIndex == 0 && h->fighter2->seqIndex == 0) h->v54 = 13;
        break;
    case 13:
        if (changed) h->stateTimer = 60.0f;
        goto timer;
    case 14:
        if (changed) {
            VictoryPose(h);
            h->stateTimer = 30.0f;
        }
    timer:
        h->stateTimer = h->stateTimer - h->dt;
        if (h->stateTimer <= 0.0f) h->v54++;
        break;
    case 15:
        if (changed) cfCharacter_PlaySequence(h->v40, 0x3B, -1, 1, (u32)-1);
    posed: {
        cfCharacter *w = h->v3C;
        if (w->seq.seq == NULL || w->seq.finished != 0) {
            if ((f32)h->floorY <= h->v3C->screenY) {
                h->v54 = 7;
                func_8006E1C4(h);
            }
        }
        break;
    }
    default:
        break;
    }
    if (h->v4C != 0) cfCombatHandler_Centre(h);
    h->frameCount++;
}
