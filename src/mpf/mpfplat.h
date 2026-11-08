/*
 * MPF platform layer: program exit / fatal errors (MPFEXIT.C), memory and string
 * helpers, and Interplay's own printf.
 */
#ifndef MPFPLAT_H
#define MPFPLAT_H

#include <stdarg.h>
#include "common.h"

struct MPF_Mutex;

/* MPFEXIT.C (0x80089790) */
#define MAX_SHUTDOWN_PROCS 20
typedef void (*MPF_ShutDownProc)(void);
void mpAddShutDownProc(MPF_ShutDownProc proc);
void mpExit(void);
void mpExitMsg(const char *msg);
void mpExitf(const char *fmt, ...);
/* mpFatalError / mpFatalErrorf are declared in common.h */
void _ShutDown(void);
void mpHang(s32 code);

/* helpers (0x80089AE0) */
void mpMutex_Init(struct MPF_Mutex *mutex);
void mpMemMove(void *dst, const void *src, u32 len);
void mpMemSet(void *dst, u32 len, s32 value);
void mpStrCpy(char *dst, const char *src);
void mpStrCat(char *dst, const char *src);
s32  mpStrLen(const char *s);
s32  mpStrCmp(const char *a, const char *b);
s32  mpStrCmpNoCase(const char *a, const char *b);
s32  mpStrEqual(const char *a, const char *b);
s32  mpStrEqualNoCase(const char *a, const char *b);

/* printf (0x80089E50) */
void mpSPrintf(char *buf, const char *fmt, ...);
void mpVSPrintf(char *buf, const char *fmt, va_list args);
void mpDebugPrintf(const char *fmt, ...);
void mpNullFunc(void);

#endif
