/*
 * CF3CHAR.C: the fighter during a fight (cfCharacter), plus the structs it shares with
 * the combat handler (CF3COMBT.C). The structs themselves are generated from
 * cf3char_fields.txt by tools/gen_struct.py, with every field offset checked.
 */
#ifndef CF3CHAR_H
#define CF3CHAR_H

#include "common.h"
#include "cf3seq.h"
#include "mpfmem.h"

typedef struct Matrix2D { f32 m[3][3]; } Matrix2D;
typedef struct cfCharacter cfCharacter;

#include "cf3char_structs.h"

#define CHAR_NUM_INPUTS      112
#define CHAR_NUM_PROJECTILES 16
#define CHAR_NUM_PROJSEQS    10

/* CF3CHAR.C */
void cfCharacter_Construct(cfCharacter *c);
void cfCharacter_Destruct(cfCharacter *c);
void cfCharacter_Init(cfCharacter *c, CharDef *def);
void cfCharacter_TurnAround(cfCharacter *c);
void cfCharacter_SavePrevious(cfCharacter *c);
void cfCharacter_ReturnToNeutral(cfCharacter *c);
void cfCharacter_ClearMoveState(cfCharacter *c);
s32  cfCharacter_SideOfOpponent(cfCharacter *c);
void cfCharacter_ClampToArena(cfCharacter *c);
void cfCharacter_UpdatePhysics(cfCharacter *c);
void cfCharacter_Update(cfCharacter *c);
void cfCharacter_ProcessInput(cfCharacter *c, u32 buttons);
void cfCharacter_UpdateProjectiles(cfCharacter *c);
void cfCharacter_HandleInput(cfCharacter *c, u32 buttons, u32 buttons2);
void cfCharacter_HitReaction(cfCharacter *c, s32 type, s32 strength, s32 kind, s32 damage, s32 sound,
                             f32 pushX, f32 pushY, f32 hitX, f32 hitY, f32 scale, s32 byProjectile);
void cfCharacter_PlaySequence(cfCharacter *c, s32 index, s32 loops, s32 reset, u32 speed);
void cfCharacter_StopSequence(cfCharacter *c);
void cfCharacter_UpdateMatrix(cfCharacter *c);
void cfCharacter_ProjectileImpacts(cfCharacter *c);
void cfCharacter_ApplyProjectileSeq(cfCharacter *c, CharProjectile *p);
void cfCharacter_AimProjectile(cfCharacter *c, CharProjectile *p);
s32  cfCharacter_PickSound(cfCharacter *c, s32 sound);
u8  *cfCharacter_GetRecord(cfCharacter *c, s32 n);
u32  cfCharacter_ButtonsToCommand(cfCharacter *c, u32 buttons);
s32  cfCharacter_MatchInput(cfCharacter *c, u32 command, u32 buttons, s32 *index, u32 *time,
                            u32 window, s32 allowUsed);
s32  cfCharacter_FindInput(cfCharacter *c, u32 buttons, s32 *index, u32 *time, u32 window);
s32  cfCharacter_InputHeld(cfCharacter *c, u32 buttons, s32 duration, s32 index);
s32  cfCharacter_AllocProjectile(cfCharacter *c);
void cfCharacter_FreeProjectile(cfCharacter *c, s32 i);
s32  cfCharacter_AllocProjSeq(cfCharacter *c);
void cfCharacter_SpawnSpark(cfCharacter *c, f32 x, f32 y, f32 z, f32 size);
void cfCharacter_SpawnDust(cfCharacter *c, f32 x, f32 y, f32 z, f32 size);
void cfCharacter_SpawnDebris(cfCharacter *c, f32 x, f32 y, f32 z);
void cfCharacter_SpawnFlash(cfCharacter *c, f32 x, f32 y, f32 z);
void cfCharacter_ResetCounters(cfCharacter *c);
void cfCharacter_EmitBits(cfCharacter *c, s32 count, f32 x, f32 y, f32 z);
void cfCharacter_EmitSmoke(cfCharacter *c, f32 x, f32 y, f32 z, s32 a4, s32 size);
void cfCharacter_SequenceEffects(cfCharacter *c);
void cfCharacter_StartMove(cfCharacter *c, u8 *move, u8 *variant);
void cfCharacter_CheckBlock(cfCharacter *c, u32 buttons);

void cfCharacter_ApplySeqFrame(cfCharacter *c);
void cfCharacter_UpdateRects(cfCharacter *c);

/* elsewhere in the game (still in assembly) */
void func_8006DE6C(CombatHandler *h, s32 sound, s32 volume, cfCharacter *c, const f32 *pos, s32 a5);
f32  cfVec2_Length(f32 *vec2);                              /* length of a 2D vector */
void cfVec3_Normalise(f32 *vec3);                              /* normalise a 3D vector */
void cfMatrix2D_SetTranslate(Matrix2D *m, f32 x, f32 y);
void cfMatrix2D_PreTranslate(Matrix2D *m, f32 x, f32 y);
void cfMatrix2D_PreRotate(Matrix2D *m, f32 angle);
u32  guRandom(void);
/* CF3COMBT.C (src/char/cf3combt.c) */
s32  cfAI_PickMove(cfCharacter *c, u8 **move, u8 **variant, u8 *list, s32 n);
void cfAI_QueueMove(cfCharacter *c, u8 *move, u8 *variant);
s32  cfAI_TryMove(cfCharacter *c, u8 *list, s32 n);
void cfCombatHandler_Init(CombatHandler *h);
void cfAI_NewState(cfCharacter *c);
void cfAI_Think(cfCharacter *c);
void cfCombatHandler_Centre(CombatHandler *h);
s32  cfCombatHandler_Gap(CombatHandler *h);
void func_8006C940(CombatHandler *h);
void func_800720B0(CombatHandler *h);
void func_8006C97C(CombatHandler *h);
void func_800728B4(CombatHandler *h);
void func_800721C4(CombatHandler *h);
void func_80072568(CombatHandler *h);
void cfCombatHandler_CheckZones(CombatHandler *h);
void func_80074970(CombatHandler *h);
void func_80074BA0(CombatHandler *h);
void cfVec2_Normalise(f32 *vec2);
s32  cfSegmentsIntersect(f32 *seg, f32 *wall, f32 *out);   /* does the move seg[0..3] cross the wall? 0 no, 1 yes, 2 (ignored) */
void Fx_UpdateSmoke(CombatHandler *h);
void cfCombatHandler_PushApart(CombatHandler *h);
void cfCombatHandler_SideSteps(CombatHandler *h);
void cfCombatHandler_Frame(CombatHandler *h);
void cfCombatHandler_FrameNoInput(CombatHandler *h);
void cfCombatHandler_Run(CombatHandler *h);
void func_8006E1C4(CombatHandler *h);
void cfCombatHandler_EnterArena(CombatHandler *h, u8 *bkg, s32 arenaIndex, s32 zoneIndex);
void cfCombatHandler_ClipPlayerAgainstBoundingLines(CombatHandler *h, cfCharacter *c);
void Fx_UpdateBits(CombatHandler *h);
void func_800729EC(CombatHandler *h);
void cfCombatHandler_Start(CombatHandler *h, cfCharacter *f1, cfCharacter *f2, u8 *bkg, s32 arenaIndex,
                           s32 skipIntro, s32 followPath);
void func_8006CABC(CombatHandler *h);
void cfCombatHandler_ReadInput(CombatHandler *h, u32 pad1, u32 pad2);
void cfCombatHandler_PathPoint(CombatHandler *h, f32 t, f32 *out);
void Fx_UpdateMarks(CombatHandler *h);
void Fx_UpdateSparks(CombatHandler *h);
void Fx_UpdateDust(CombatHandler *h);
void Fx_UpdateDebris(CombatHandler *h);
void Fx_UpdateBursts(CombatHandler *h);
void Fx_UpdateGlows(CombatHandler *h);
void Fx_UpdateRings(CombatHandler *h);
void Fx_UpdateFlashes(CombatHandler *h);
void Fx_UpdateImpactFlash(CombatHandler *h);
void Fx_UpdateFade(CombatHandler *h);
FxRing *Fx_NewRing(CombatHandler *h);   /* free shock-wave ring (8 at 0x2AB0), or NULL */
FxRing *Fx_NewBurst(CombatHandler *h);  /* free burst (16 at 0x2570), or NULL */
FxGlow *Fx_NewGlow(CombatHandler *h);   /* free glow (16 at 0x2830), or NULL */
void mpRectNormalize(SeqRect *r);
void mpRectIntersect(SeqRect *a, SeqRect *b);   /* a = a ∩ b */
f32  vsSin(s32 angle);                                      /* 0x10000 = full turn */
f32  vsCos(s32 angle);


/* CF3MTX2D.C (src/char/cf3mtx2d.c) */
void cfMatrix2D_Identity(Matrix2D *m);
void cfMatrix2D_Copy(Matrix2D *dst, Matrix2D *src);
void cfMatrix2D_SetScale(Matrix2D *m, f32 sx, f32 sy);
void cfMatrix2D_SetRotate(Matrix2D *m, f32 angle);
void cfMatrix2D_Concat(Matrix2D *a, Matrix2D *b, Matrix2D *out);
void cfMatrix2D_Multiply(Matrix2D *a, Matrix2D *b);
void cfMatrix2D_Scale(Matrix2D *m, f32 sx, f32 sy);
void cfMatrix2D_Translate(Matrix2D *m, f32 x, f32 y);
void cfMatrix2D_Rotate(Matrix2D *m, f32 angle);
void cfMatrix2D_PreMultiply(Matrix2D *a, Matrix2D *b);
void cfMatrix2D_PreScale(Matrix2D *m, f32 sx, f32 sy);
f32  cfMatrix2D_Determinant(Matrix2D *m);
void cfMatrix2D_Transpose(Matrix2D *src, Matrix2D *dst);
void cfMatrix2DInvert(Matrix2D *src, Matrix2D *dst);
void cfMatrix2D_TransposeSelf(Matrix2D *m);
void cfMatrix2D_InvertSelf(Matrix2D *m);
void cfMatrix2D_Init(Matrix2D *m);
f32  cfVec3_Length(f32 *v);

/* curves (src/char/spline.c) */
void Spline_Knots(s32 *knots, s32 n, s32 k);
f32  Spline_Basis(s32 i, s32 k, s32 *knots, f32 u);
void Spline_Eval(s32 *knots, s32 n, s32 k, f32 u, f32 *pts, f32 *out);
void Spline_Point(f32 *pts, s32 n, f32 t, f32 *out, s32 wrap);     /* point at t (0..1) on the curve through n points */
f32  Spline_Advance(f32 *pts, s32 n, f32 t, f32 step, s32 wrap);   /* t moved on by about 'step' units along it */
s32  n64NextThreadId(void);                  /* a new thread id (counts up) */

#endif
