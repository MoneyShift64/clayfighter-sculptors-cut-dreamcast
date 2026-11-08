/*
 * ClayFighter: Sculptor's Cut - common types and helpers for the decompiled C.
 *
 * This is a *functional* decompilation: the C does the same job as the original
 * code but is not expected to compile to identical bytes. It is written to build
 * three ways: for the N64 (MIPS, big-endian), for the host test programs, and later
 * for the Dreamcast port (SH-4, little-endian).
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

typedef signed char        s8;
typedef unsigned char      u8;
typedef signed short       s16;
typedef unsigned short     u16;
typedef signed int         s32;
typedef unsigned int       u32;
typedef signed long long   s64;
typedef unsigned long long u64;
typedef float              f32;
#if defined(TARGET_PORT) && (defined(__sh__) || defined(PORT_F64_FLOAT))
/* the Dreamcast: the SH4 is fast at single precision and slow to switch to double; the game
   keeps no doubles (they are only in its arithmetic), so they are floats here */
typedef float              f64;
#else
typedef double             f64;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

/*
 * Game data in the ROM is big-endian. On the N64 these are plain loads; on a
 * little-endian machine (host tests, Dreamcast) they swap the bytes.
 */
static inline u16 READ_BE16(const void *p) {
    const u8 *b = (const u8 *)p;
    return (u16)((b[0] << 8) | b[1]);
}
static inline u32 READ_BE32(const void *p) {
    const u8 *b = (const u8 *)p;
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}
static inline void WRITE_BE16(void *p, u16 v) {
    u8 *b = (u8 *)p;
    b[0] = (u8)(v >> 8); b[1] = (u8)v;
}
static inline void WRITE_BE32(void *p, u32 v) {
    u8 *b = (u8 *)p;
    b[0] = (u8)(v >> 24); b[1] = (u8)(v >> 16); b[2] = (u8)(v >> 8); b[3] = (u8)v;
}

/* libultra memory helpers. On the N64 these are libultra's own functions;
   everywhere else they map to the standard C library. */
#ifdef TARGET_N64
void bcopy(const void *src, void *dst, int len);
void bzero(void *dst, int len);
#else
#include <string.h>
#define bcopy(src, dst, len) memmove((dst), (src), (size_t)(len))
#define bzero(dst, len)      memset((dst), 0, (size_t)(len))
#endif

/* in loops that wait for another thread or the retrace: nothing on the N64 (an interrupt
   gets the other thread going); the ports run one thread at a time, so this lets the others go */
#ifdef TARGET_PORT
void port_yield(void);
#define PORT_SPIN() port_yield()
#elif defined(CFSC_TRACE_BUILD)
#define PORT_SPIN() break       /* (the emulator tests: nothing to wait for) */
#else
#define PORT_SPIN() ((void)0)
#endif

/* MPFEXIT.C - error handlers (do not return in practice) */
void mpFatalError(const char *msg);
void mpFatalErrorf(const char *fmt, ...);

#endif /* COMMON_H */
