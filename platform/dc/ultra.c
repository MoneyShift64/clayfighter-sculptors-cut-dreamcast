/*
 * The parts of libultra (the N64 SDK) that the game's C still calls.
 *
 * Threads work the N64 way: only one runs at a time, always the highest-priority one that
 * isn't waiting, and it keeps running until it waits for a message or wakes a thread with a
 * higher priority. Each game thread is a real (POSIX) thread, but they hand over to each
 * other so only one is ever running game code. That keeps every timing assumption the game
 * makes, without races it never had on the N64.
 *
 * The vertical retrace "interrupt" is checked for whenever a thread calls one of these
 * functions: each new retrace (from plat_vblank_count) sends a message to the queues that
 * asked for one, which wakes the logic threads.
 */
#include <pthread.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include "port.h"
#ifdef __sh__
#include <sh4zam/shz_sh4zam.h>
#define sinf shz_sinf
#define cosf shz_cosf
#define sqrtf shz_sqrtf
#endif

/* ------------------------------------------------------------------ threads */

enum { T_FREE, T_STOPPED, T_READY, T_WAITRECV, T_WAITSEND, T_DONE };

typedef struct PThread {
    void *os;                       /* the game's OSThread (it is only used as a key) */
    s32 id, pri, state;
    u32 seq;                        /* when it became ready (first come, first served) */
    OSMesgQueue *wq;                /* the queue it waits on */
    void (*entry)(void *);
    void *arg;
    s32 started;
    pthread_t th;
    pthread_cond_t cv;
} PThread;

#define MAX_THREADS 16
static PThread sThreads[MAX_THREADS];
static pthread_mutex_t sK = PTHREAD_MUTEX_INITIALIZER;
static PThread *sCur;               /* the one running */
static u32 sSeq;
static u32 sLastVbl;
static OSMesgQueue *sRetraceQ[8];
u32 gPortRetraces;

void port_on_retrace(void);         /* sys.c: the retrace counter etc. */

static PThread *Find(void *os) {
    s32 i;
    for (i = 0; i < MAX_THREADS; i++)
        if (sThreads[i].state != T_FREE && sThreads[i].os == os) return &sThreads[i];
    return NULL;
}

static PThread *Best(void) {
    PThread *b = NULL;
    s32 i;
    for (i = 0; i < MAX_THREADS; i++) {
        PThread *t = &sThreads[i];
        if (t->state != T_READY) continue;
        if (b == NULL || t->pri > b->pri || (t->pri == b->pri && (s32)(t->seq - b->seq) < 0)) b = t;
    }
    return b;
}

static void MakeReady(PThread *t) {
    t->state = T_READY;
    t->wq = NULL;
    t->seq = ++sSeq;
}

/* wake the highest-priority thread waiting in state on q */
static void WakeOne(OSMesgQueue *q, s32 state) {
    PThread *b = NULL;
    s32 i;
    for (i = 0; i < MAX_THREADS; i++) {
        PThread *t = &sThreads[i];
        if (t->state == state && t->wq == q && (b == NULL || t->pri > b->pri)) b = t;
    }
    if (b) MakeReady(b);
}

/* a message from an "interrupt": never blocks, never switches */
static void SendNoSwitch(OSMesgQueue *q, OSMesg m) {
    if (q->validCount >= q->msgCount) return;
    q->msg[(q->first + q->validCount) % q->msgCount] = m;
    q->validCount++;
    WakeOne(q, T_WAITRECV);
}

/* A game logic thread (the screens', the fight's) waits for a retrace message and does one
   tick. On the N64 it ran at every retrace while the graphics chip drew; here the drawing is
   done with the game's lock held, so when a frame takes two retraces the second one found its
   queue still full and was lost: the game ran slower than real time (slow motion, the intro
   behind its music). Such a queue keeps a count of the retraces it missed and is refilled as
   its thread takes each message, so the logic catches up (as many ticks as retraces). */
#define LOST(n) ((void)0)
static u8 sRetraceTicks[8];             /* the queue gets the missed retraces too */
static u8 sRetraceOwed[8];
#define MAX_OWED 4

static void Retrace(void) {
    s32 i;
    gPortRetraces++;
    port_on_retrace();
    for (i = 0; i < 8; i++)
        if (sRetraceQ[i]) {
            if (sRetraceTicks[i] && sRetraceQ[i]->validCount >= sRetraceQ[i]->msgCount) {
                if (sRetraceOwed[i] < MAX_OWED) sRetraceOwed[i]++; else LOST(1);
            } else SendNoSwitch(sRetraceQ[i], NULL);
        }
}

/* a message taken from q: if it is a retrace queue owed a retrace, the next one */
static void RetraceRefill(OSMesgQueue *q) {
    s32 i;
    for (i = 0; i < 8; i++)
        if (sRetraceQ[i] == q && sRetraceOwed[i]) { sRetraceOwed[i]--; SendNoSwitch(q, NULL); return; }
}

static void PollIrq(void) {
    u32 now = plat_vblank_count();
    if (now - sLastVbl > 3) { LOST(now - sLastVbl - 3); sLastVbl = now - 3; }     /* after a long load: don't catch up */
    while (sLastVbl != now) {
        sLastVbl++;
        Retrace();
    }
}

static void *ThreadMain(void *p);
void port_heapcheck(void);

/* sCur is self. Run whoever should run now; come back when self is chosen again. */
static void Schedule(PThread *self) {
    PThread *b;

    for (;;) {
        b = Best();
        if (b == NULL) {                /* everybody waits: idle until the next retrace */
            u32 v = sLastVbl;
            pthread_mutex_unlock(&sK);
            plat_wait_vblank(v);
            pthread_mutex_lock(&sK);
            PollIrq();
            continue;
        }
        if (self->state == T_READY && b->pri <= self->pri) b = self;
        break;
    }
    if (b == self) return;
    port_heapcheck();
    if (self->state == T_READY) self->seq = ++sSeq;     /* preempted: to the back of its level */
    sCur = b;
    if (!b->started) {
        pthread_attr_t a;
        b->started = 1;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, 256 * 1024);
        pthread_create(&b->th, &a, ThreadMain, b);
        pthread_attr_destroy(&a);
    } else {
        pthread_cond_signal(&b->cv);
    }
    if (self->state == T_DONE) return;  /* a thread that has ended just hands over */
    while (sCur != self) pthread_cond_wait(&self->cv, &sK);
}

static void *ThreadMain(void *p) {
    PThread *t = p;
    pthread_mutex_lock(&sK);
    while (sCur != t) pthread_cond_wait(&t->cv, &sK);
    pthread_mutex_unlock(&sK);
    t->entry(t->arg);
    pthread_mutex_lock(&sK);
    t->state = T_DONE;
    Schedule(t);
    pthread_mutex_unlock(&sK);
    return NULL;
}

/* switch if a higher-priority thread is ready */
static void Preempt(void) {
    PThread *b = Best();
    if (b && b != sCur && b->pri > sCur->pri) Schedule(sCur);
}

void port_threads_init(void) {
    PThread *t = &sThreads[0];
    pthread_cond_init(&t->cv, NULL);
    t->os = (void *)1;
    t->id = 6;                      /* the N64 main thread's id and priority */
    t->pri = 10;
    t->started = 1;
    t->th = pthread_self();
    MakeReady(t);
    sCur = t;
    sLastVbl = plat_vblank_count();
}

static void AddRetraceQueue(OSMesgQueue *q, s32 ticks) {
    s32 i;
    pthread_mutex_lock(&sK);
    for (i = 0; i < 8; i++)
        if (sRetraceQ[i] == NULL) { sRetraceQ[i] = q; sRetraceTicks[i] = (u8)ticks; sRetraceOwed[i] = 0; break; }
    pthread_mutex_unlock(&sK);
}
void port_add_retrace_queue(OSMesgQueue *q) { AddRetraceQueue(q, 0); }
/* a game logic thread's queue: no retrace lost (see Retrace) */
void port_add_tick_queue(OSMesgQueue *q) { AddRetraceQueue(q, 1); }

void port_remove_retrace_queue(OSMesgQueue *q) {
    s32 i;
    pthread_mutex_lock(&sK);
    for (i = 0; i < 8; i++) if (sRetraceQ[i] == q) { sRetraceQ[i] = NULL; sRetraceOwed[i] = 0; break; }
    pthread_mutex_unlock(&sK);
}

/* in a loop that waits for something another thread or the retrace does */
void port_yield(void) {
    u32 v;
    pthread_mutex_lock(&sK);
    v = sLastVbl;
    PollIrq();
    if (sLastVbl == v) {            /* nothing new: wait for the next retrace */
        pthread_mutex_unlock(&sK);
        plat_wait_vblank(v);
        pthread_mutex_lock(&sK);
        PollIrq();
    }
    Preempt();
    pthread_mutex_unlock(&sK);
}

/* 0x800999D0 osCreateThread */
void func_800999D0(void *os, s32 id, void (*entry)(void *), void *arg, void *sp, s32 pri) {
    PThread *t;
    s32 i;
    (void)sp;
    pthread_mutex_lock(&sK);
    t = Find(os);
    if (t == NULL || t->state != T_DONE) {
        for (i = 0; i < MAX_THREADS && sThreads[i].state != T_FREE && sThreads[i].state != T_DONE; i++) {
        }
        if (i == MAX_THREADS) { pthread_mutex_unlock(&sK); mpFatalError("port: too many threads"); }
        t = &sThreads[i];
    }
    if (t->started && t->state == T_DONE) pthread_join(t->th, NULL);
    else if (!t->started) pthread_cond_init(&t->cv, NULL);
    t->os = os;
    t->id = id;
    t->pri = pri;
    t->entry = entry;
    t->arg = arg;
    t->started = 0;
    t->state = T_STOPPED;
    pthread_mutex_unlock(&sK);
}

/* 0x8009A0D0 osStartThread */
void func_8009A0D0(void *os) {
    PThread *t;
    pthread_mutex_lock(&sK);
    t = Find(os);
    if (t && t->state == T_STOPPED) {
        MakeReady(t);
        PollIrq();
        Preempt();
    }
    pthread_mutex_unlock(&sK);
}

/* 0x8009A000 osSetThreadPri */
void func_8009A000(void *os, s32 pri) {
    PThread *t;
    pthread_mutex_lock(&sK);
    t = os ? Find(os) : sCur;
    if (t) t->pri = pri;
    Preempt();
    pthread_mutex_unlock(&sK);
}

OSId osGetThreadId(OSThread *os) {
    PThread *t = os ? Find(os) : sCur;
    return t ? t->id : -1;
}

void osCreateMesgQueue(OSMesgQueue *q, OSMesg *msg, s32 count) {
    q->mtqueue = NULL;
    q->fullqueue = NULL;
    q->validCount = 0;
    q->first = 0;
    q->msgCount = count;
    q->msg = msg;
}

s32 osSendMesg(OSMesgQueue *q, OSMesg m, s32 flag) {
    PThread *self;
    pthread_mutex_lock(&sK);
    self = sCur;
    PollIrq();
    while (q->validCount >= q->msgCount) {
        if (flag != OS_MESG_BLOCK) { pthread_mutex_unlock(&sK); return -1; }
        self->state = T_WAITSEND;
        self->wq = q;
        Schedule(self);
    }
    q->msg[(q->first + q->validCount) % q->msgCount] = m;
    q->validCount++;
    WakeOne(q, T_WAITRECV);
    Preempt();
    pthread_mutex_unlock(&sK);
    return 0;
}

s32 osRecvMesg(OSMesgQueue *q, OSMesg *m, s32 flag) {
    PThread *self;
    pthread_mutex_lock(&sK);
    self = sCur;
    PollIrq();
    while (q->validCount == 0) {
        if (flag != OS_MESG_BLOCK) { pthread_mutex_unlock(&sK); return -1; }
        self->state = T_WAITRECV;
        self->wq = q;
        Schedule(self);
    }
    if (m) *m = q->msg[q->first];
    q->first = (q->first + 1) % q->msgCount;
    q->validCount--;
    RetraceRefill(q);
    WakeOne(q, T_WAITSEND);
    Preempt();
    pthread_mutex_unlock(&sK);
    return 0;
}

/* ------------------------------------------------------------------ gu: matrices */

static void IdentF(f32 mf[4][4]) {
    s32 i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) mf[i][j] = (i == j) ? 1.0f : 0.0f;
}

/* 0x80096D30 guMtxF2L: floats to the RSP's 16.16 fixed point (integer halves, then fractions) */
void func_80096D30(f32 mf[4][4], void *mtx) {
    u32 *ai = mtx, *af = (u32 *)mtx + 8;
    s32 i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 2; j++) {
            s32 e1 = (s32)(mf[i][j * 2] * 65536.0f);
            s32 e2 = (s32)(mf[i][j * 2 + 1] * 65536.0f);
            *ai++ = ((u32)e1 & 0xFFFF0000u) | (((u32)e2 >> 16) & 0xFFFF);
            *af++ = (((u32)e1 << 16) & 0xFFFF0000u) | ((u32)e2 & 0xFFFF);
        }
}

/* 0x80096FB0 guMtxCatF: out = a * b */
void func_80096FB0(f32 a[4][4], f32 b[4][4], f32 out[4][4]) {
    f32 t[4][4];
    s32 i, j, k;
#ifdef __sh__
    shz_xmtrx_load_unaligned_4x4(&b[0][0]);              /* (each row of a through b, on the matrix unit) */
    for (i = 0; i < 4; i++) {
        shz_vec4_t v = shz_xmtrx_transform_vec4((shz_vec4_t){ .x = a[i][0], .y = a[i][1], .z = a[i][2], .w = a[i][3] });
        t[i][0] = v.x; t[i][1] = v.y; t[i][2] = v.z; t[i][3] = v.w;
    }
    (void)k;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) out[i][j] = t[i][j];
    return;
#endif
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            f32 s = 0.0f;
            for (k = 0; k < 4; k++) s += a[i][k] * b[k][j];
            t[i][j] = s;
        }
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) out[i][j] = t[i][j];
}

/* 0x8009708C guMtxXFMF: a point through a matrix */
void func_8009708C(f32 mf[4][4], f32 x, f32 y, f32 z, f32 *ox, f32 *oy, f32 *oz) {
#ifdef __sh__
    *ox = shz_dot8f(mf[0][0], mf[1][0], mf[2][0], mf[3][0], x, y, z, 1.0f);   /* (FIPR) */
    *oy = shz_dot8f(mf[0][1], mf[1][1], mf[2][1], mf[3][1], x, y, z, 1.0f);
    *oz = shz_dot8f(mf[0][2], mf[1][2], mf[2][2], mf[3][2], x, y, z, 1.0f);
#else
    *ox = mf[0][0] * x + mf[1][0] * y + mf[2][0] * z + mf[3][0];
    *oy = mf[0][1] * x + mf[1][1] * y + mf[2][1] * z + mf[3][1];
    *oz = mf[0][2] * x + mf[1][2] * y + mf[2][2] * z + mf[3][2];
#endif
}

/* 0x80097140 guOrthoF */
void func_80097140(f32 mf[4][4], f32 l, f32 r, f32 b, f32 t, f32 n, f32 f, f32 scale) {
    s32 i, j;
    IdentF(mf);
    mf[0][0] = 2.0f / (r - l);
    mf[1][1] = 2.0f / (t - b);
    mf[2][2] = -2.0f / (f - n);
    mf[3][0] = -(r + l) / (r - l);
    mf[3][1] = -(t + b) / (t - b);
    mf[3][2] = -(f + n) / (f - n);
    mf[3][3] = 1.0f;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) mf[i][j] *= scale;
}

/* 0x800973C0 guPerspectiveF */
void func_800973C0(f32 mf[4][4], u16 *perspNorm, f32 fovy, f32 aspect, f32 n, f32 f, f32 scale) {
    f32 cot;
    s32 i, j;
    IdentF(mf);
    fovy = (f32)(fovy * (3.1415926 / 180.0));
    cot = cosf(fovy / 2.0f) / sinf(fovy / 2.0f);
    mf[0][0] = cot / aspect;
    mf[1][1] = cot;
    mf[2][2] = (n + f) / (n - f);
    mf[2][3] = -1.0f;
    mf[3][2] = (2.0f * n * f) / (n - f);
    mf[3][3] = 0.0f;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) mf[i][j] *= scale;
    if (perspNorm) {
        if (n + f <= 2.0f) *perspNorm = 65535;
        else {
            *perspNorm = (u16)(s32)((2.0 * 65536.0) / (n + f));
            if (*perspNorm == 0) *perspNorm = 1;
        }
    }
}

/* 0x80097740 guRotateF: a rotation of angle degrees about the axis */
void func_80097740(f32 mf[4][4], f32 a, f32 x, f32 y, f32 z) {
    f32 sine, cosine, ab, bc, ca, t, len;
    len = sqrtf(x * x + y * y + z * z);
    if (len != 0.0f) { len = 1.0f / len; x *= len; y *= len; z *= len; }
    a = (f32)(a * (3.1415926 / 180.0));
    sine = sinf(a);
    cosine = cosf(a);
    t = 1.0f - cosine;
    ab = x * y * t;
    bc = y * z * t;
    ca = z * x * t;
    IdentF(mf);
    t = x * x; mf[0][0] = t + cosine * (1.0f - t); mf[2][1] = bc - x * sine; mf[1][2] = bc + x * sine;
    t = y * y; mf[1][1] = t + cosine * (1.0f - t); mf[2][0] = ca + y * sine; mf[0][2] = ca - y * sine;
    t = z * z; mf[2][2] = t + cosine * (1.0f - t); mf[1][0] = ab - z * sine; mf[0][1] = ab + z * sine;
}

/* ------------------------------------------------------------------ the rest */

extern u32 D_800C0F00;              /* guRandom's seed (in the game's data) */

u32 guRandom(void) {
    u32 x = D_800C0F00 << 2;
    x = ((x + 2) * (x + 3)) >> 2;
    D_800C0F00 = x;
    return x;
}

/* 0x800A4090: 64-bit signed remainder */
s64 func_800A4090(s64 a, s64 b) {
    return b ? a % b : 0;
}

/* 0x800A3C80: the library's qsort (a Shell sort; kept as it was so equal items end up
   in the same order) */
static void Swap(u8 *a, u8 *b, u32 n) {
    while (n--) { u8 t = *a; *a++ = *b; *b++ = t; }
}

void func_800A3C80(void *base, u32 count, u32 size, s32 (*compare)(const void *, const void *)) {
    u8 *b = base, *end, *p, *q;
    u32 gap, gs;

    if (count < 2) return;
    if (count == 2) {
        if (compare(b, b + size) > 0) Swap(b, b + size, size);
        return;
    }
    gap = 1;
    while (gap < count) gap = gap * 2 + 1;
    end = b + count * size;
    for (gap >>= 1; gap != 0; gap >>= 1) {
        gs = gap * size;
        for (p = b + gs; p < end; p += size)
            for (q = p - gs; q >= b && q < end; q -= gs) {
                if (compare(q, q + gs) <= 0) break;
                Swap(q, q + gs, size);
            }
    }
}


/* (empty on the N64; the port's PC build prints it) */
void osSyncPrintf(const char *fmt, ...) {
    char buf[512];
    va_list a;
#ifdef __sh__
    (void)fmt; (void)buf; (void)a;
    return;                                 /* (the console build: nothing, as the N64) */
#endif
    va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    plat_debug(buf);
}

/* 0x800980B0 osViGetCurrentLine (only used for a random start) */
u32 func_800980B0(void) {
    return plat_vblank_count() & 0xFF;
}

