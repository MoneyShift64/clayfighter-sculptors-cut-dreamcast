/*
 * MPFEXIT.C (0x80089790-0x80089AE0): shut-down procedures, exit and fatal errors.
 *
 * A fatal error prints its message on screen for 10 rounds, runs the shut-down
 * procedures (last registered first) and then hangs. A second fatal error raised while
 * shutting down prints both messages and hangs straight away.
 */
#include "mpfplat.h"

/* still in assembly */
void osSyncPrintf(const char *fmt, ...);             /* empty in this build */
void func_80077F74(s32 r, s32 g, s32 b);              /* debug text colour */
void func_80078040(const char *s, s32 x, s32 y, s32 arg3); /* debug text */

/* data / BSS */
extern s32 sShutDownCalled;
extern s32 sNumShutDownProcs;
extern const char *sFatalMessage;
extern MPF_ShutDownProc sShutDownProcs[MAX_SHUTDOWN_PROCS];

/* 0x80089790 */
void mpAddShutDownProc(MPF_ShutDownProc proc) {
    if (sNumShutDownProcs == MAX_SHUTDOWN_PROCS) {
        mpFatalErrorf("MPFEXIT.C - Maximum number of shut down procedures exceeded (%d)", MAX_SHUTDOWN_PROCS);
    }
    sShutDownProcs[sNumShutDownProcs++] = proc;
}

/* 0x800897F4 */
void mpExit(void) {
    _ShutDown();
    mpHang(0);
}

/* 0x80089818 */
void mpExitMsg(const char *msg) {
    _ShutDown();
    osSyncPrintf("\n");
    osSyncPrintf(msg);
    osSyncPrintf("\n");
    mpHang(0);
}

/* 0x80089864 */
void mpExitf(const char *fmt, ...) {
    char buf[0x200];
    va_list args;

    va_start(args, fmt);
    mpVSPrintf(buf, fmt, args);
    va_end(args);
    mpExitMsg(buf);
}

/* 0x800898A0 */
void mpFatalError(const char *msg) {
    s32 i, j;

    for (i = 0; i < 10; i++) {
        func_80077F74(0xFF, 0xFF, 0xFF);
        for (j = 0; j < 8; j++) {
            func_80078040(msg, 32, 32, 0);
        }
    }
    if (sFatalMessage != NULL) {
        osSyncPrintf("\nFATAL ERROR: ");
        osSyncPrintf(sFatalMessage);
        osSyncPrintf("\n\n");
        osSyncPrintf("RECURSIVE FATAL: ");
        osSyncPrintf(msg);
        osSyncPrintf("\n");
        mpHang(1);
    }
    sFatalMessage = msg;
    _ShutDown();
    osSyncPrintf("\nFATAL ERROR: ");
    osSyncPrintf(msg);
    osSyncPrintf("\n");
    mpHang(1);
}

/* 0x80089A1C */
void mpFatalErrorf(const char *fmt, ...) {
    char buf[0x200];
    va_list args;

    va_start(args, fmt);
    mpVSPrintf(buf, fmt, args);
    va_end(args);
    mpFatalError(buf);
}

/* 0x80089A58 */
void _ShutDown(void) {
    s32 i;

    if (sShutDownCalled) {
        mpFatalError("MPFEXIT.C - _ShutDown() called twice");
    }
    sShutDownCalled = 1;
    for (i = sNumShutDownProcs - 1; i >= 0; i--) {
        sShutDownProcs[i]();
    }
}

/* 0x80089AD8: the end of the program - loops forever (kept out of line, as in the
   original, so there is one place to break on) */
__attribute__((noinline)) void mpHang(s32 code) {
#ifdef TARGET_N64
    __asm__ volatile("" : : "r"(code));   /* keep the code in $a0 for a debugger */
#elif defined(TARGET_PORT)
    void port_hang(s32 code);
    port_hang(code);
#else
    (void)code;
#endif
    for (;;) {
    }
}
