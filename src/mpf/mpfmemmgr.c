/*
 * MPF_MemoryManager base methods (original object file 0x8008B050-0x8008B350).
 * Error helpers that prefix the manager's name, and generic operations built
 * on the method table (so they work for any memory manager type).
 */
#include <stdarg.h>
#include "mpfmem.h"

/* 0x8008B050 */
void mpMemoryManager_Error(MPF_MemoryManager *mgr, const char *msg) {
    mpFatalErrorf("%s - %s", mgr->name, msg);
}

/* 0x8008B07C */
void mpMemoryManager_Errorf(MPF_MemoryManager *mgr, const char *fmt, ...) {
    char buf[0x808];
    va_list args;

    va_start(args, fmt);
    mpVSPrintf(buf, fmt, args);
    va_end(args);
    mpFatalErrorf("%s - %s", mgr->name, buf);
}

/* 0x8008B0C8 */
void mpMemoryManager_FuncError(MPF_MemoryManager *mgr, const char *func, const char *msg) {
    mpFatalErrorf("%s : %s() - %s", mgr->name, func, msg);
}

/* 0x8008B0FC */
void mpMemoryManager_FuncErrorf(MPF_MemoryManager *mgr, const char *func, const char *fmt, ...) {
    char buf[0x808];
    va_list args;

    va_start(args, fmt);
    mpVSPrintf(buf, fmt, args);
    va_end(args);
    mpFatalErrorf("%s : %s() - %s", mgr->name, func, buf);
}

/* 0x8008B158 - copies a pointer allocation into a new one */
void *mpMemoryManager_DuplicatePtr(MPF_MemoryManager *mgr, void *ptr, s32 noError) {
    u32 size = mgr->sizeOfPtr(mgr, ptr);
    void *copy = mgr->allocPtr(mgr, size, noError);

    if (copy == NULL) return NULL;
    bcopy(ptr, copy, size);
    return copy;
}

/* 0x8008B1DC - copies a handle allocation into a new handle */
u32 mpMemoryManager_DuplicateHndl(MPF_MemoryManager *mgr, u32 handle, s32 noError) {
    u32 size = mgr->sizeOfHndl(mgr, handle);
    u32 copy = mgr->allocHndl(mgr, size, noError);
    void *src, *dst;

    if (copy == 0) return 0;
    src = mgr->lockHndl(mgr, handle);
    dst = mgr->lockHndl(mgr, copy);
    bcopy(src, dst, size);
    mgr->unlockHndl(mgr, copy);
    mgr->unlockHndl(mgr, handle);
    return copy;
}

/*
 * 0x8008B2AC - grows a (locked) handle by `extra` bytes. The handle is
 * unlocked first so it can move, then locked again; returns a pointer to the
 * new space at the end, or NULL.
 */
void *mpMemoryManager_GrowHndl(MPF_MemoryManager *mgr, u32 handle, u32 extra, s32 noError) {
    u32 size;

    mgr->unlockHndl(mgr, handle);
    size = mgr->sizeOfHndl(mgr, handle);
    if (!mgr->resizeHndl(mgr, handle, size + extra, noError)) return NULL;
    return (u8 *)mgr->lockHndl(mgr, handle) + size;
}
