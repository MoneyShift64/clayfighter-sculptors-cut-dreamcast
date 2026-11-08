/*
 * Interplay MPF engine - memory managers.
 *
 *   MPF_MemoryManager        abstract base: a name and a table of methods
 *   MPF_LinkedMemoryManager  the implementation the game uses (MPFLNKMM.C)
 *
 * The linked memory manager carves a memory pool into blocks kept in a doubly
 * linked list, each with a 16-byte header. Allocations are either fixed
 * pointers (allocated from the bottom of the pool) or relocatable "handles"
 * (allocated from the top), which Pack() can move to defragment memory while
 * they are unlocked. Every call is protected by a recursive thread lock.
 */
#ifndef MPFMEM_H
#define MPFMEM_H

#include <stdarg.h>
#include "common.h"
#include "ultra64_min.h"

/* Block header, 16 bytes, placed in front of every block in the pool. */
#define MPF_NODE_FREE 0x4652    /* 'FR' */
#define MPF_NODE_USED 0x5553    /* 'US' */
typedef struct MPF_LinkedMemoryNode {
    /* 0x00 */ u16 magic;                       /* MPF_NODE_FREE / MPF_NODE_USED */
    /* 0x02 */ u16 handle;                      /* owning handle (used blocks)   */
    /* 0x04 */ u32 size;                        /* whole block, header included  */
    /* 0x08 */ struct MPF_LinkedMemoryNode *next;
    /* 0x0C */ struct MPF_LinkedMemoryNode *prev;
} MPF_LinkedMemoryNode;

/* Handle table entry, 8 bytes. Handle 0 is never used. */
#define MPF_LOCK_POINTER 0xFF   /* fixed allocation from AllocPtr (never moves) */
#define MPF_LOCK_MAX     0xFE
typedef struct MPF_LinkedMemMgrHandleEntry {
    /* 0x00 */ void *ptr;                       /* data (just after the header)  */
    /* 0x04 */ u16 unk4;                        /* always 0                      */
    /* 0x06 */ u8 slack;                        /* unused bytes at the block end */
    /* 0x07 */ u8 lockCount;                    /* 0 = movable                   */
} MPF_LinkedMemMgrHandleEntry;

/* Recursive lock (MPFEXIT.C area: mpMutex_Init at 0x80089AE0) */
typedef struct MPF_Mutex {
    /* 0x00 */ OSMesgQueue mq;
    /* 0x18 */ OSMesg msg;
    /* 0x1C */ OSId owner;
    /* 0x20 */ s32 count;
} MPF_Mutex;

typedef struct MPF_MemoryManager MPF_MemoryManager;
struct MPF_MemoryManager {
    /* 0x00 */ char *name;
    /* 0x04 */ void *(*allocPtr)(MPF_MemoryManager *mgr, u32 size, s32 noError);
    /* 0x08 */ u32   (*allocHndl)(MPF_MemoryManager *mgr, u32 size, s32 noError);
    /* 0x0C */ void  (*freePtr)(MPF_MemoryManager *mgr, void *ptr);
    /* 0x10 */ void  (*freeHndl)(MPF_MemoryManager *mgr, u32 handle);
    /* 0x14 */ u32   (*getFreeMem)(MPF_MemoryManager *mgr);
    /* 0x18 */ u32   (*getUsedMem)(MPF_MemoryManager *mgr);
    /* 0x1C */ s32   (*getNumFreeBlocks)(MPF_MemoryManager *mgr);
    /* 0x20 */ void  (*pack)(MPF_MemoryManager *mgr);
    /* 0x24 */ u32   (*sizeOfPtr)(MPF_MemoryManager *mgr, void *ptr);
    /* 0x28 */ u32   (*sizeOfHndl)(MPF_MemoryManager *mgr, u32 handle);
    /* 0x2C */ void *(*duplicatePtr)(MPF_MemoryManager *mgr, void *ptr, s32 noError);
    /* 0x30 */ u32   (*duplicateHndl)(MPF_MemoryManager *mgr, u32 handle, s32 noError);
    /* 0x34 */ s32   (*resizePtr)(MPF_MemoryManager *mgr, void *ptr, u32 size, s32 noError);
    /* 0x38 */ s32   (*resizeHndl)(MPF_MemoryManager *mgr, u32 handle, u32 size, s32 noError);
    /* 0x3C */ void *(*growHndl)(MPF_MemoryManager *mgr, u32 handle, u32 extra, s32 noError);
    /* 0x40 */ void *(*lockHndl)(MPF_MemoryManager *mgr, u32 handle);
    /* 0x44 */ void  (*unlockHndl)(MPF_MemoryManager *mgr, u32 handle);
    /* 0x48 */ u8    (*getHndlIndexedByte)(MPF_MemoryManager *mgr, u32 handle, s32 index);
    /* 0x4C */ s16   (*getHndlIndexedInt16)(MPF_MemoryManager *mgr, u32 handle, s32 index);
    /* 0x50 */ s32   (*getHndlIndexedInt32)(MPF_MemoryManager *mgr, u32 handle, s32 index);
};

typedef struct MPF_LinkedMemoryManager {
    /* 0x00 */ MPF_MemoryManager base;
    /* 0x54 */ s32 initialized;
    /* 0x58 */ u32 granularity;                 /* 1, 2, 4, 8 or 16              */
    /* 0x5C */ u32 granMask;                    /* granularity - 1               */
    /* 0x60 */ u32 granInvMask;                 /* ~granMask                     */
    /* 0x64 */ s32 lastHandle;                  /* where the free-handle search resumes */
    /* 0x68 */ MPF_LinkedMemoryNode *first;
    /* 0x6C */ MPF_LinkedMemoryNode *last;
    /* 0x70 */ s32 numHandles;
    /* 0x74 */ MPF_LinkedMemMgrHandleEntry *handles;
    /* 0x78 */ u32 handleTableSize;             /* bytes, rounded to granularity */
    /* 0x7C */ MPF_Mutex mutex;
} MPF_LinkedMemoryManager;                      /* 0xA0 bytes */

/* MPFLNKMM.C */
void  mpLinkedMemoryManager_Construct(MPF_LinkedMemoryManager *mgr);
void  mpLinkedMemoryManager_Destruct(MPF_LinkedMemoryManager *mgr);
void  mpLinkedMemoryManager_Init(MPF_LinkedMemoryManager *mgr, void *pool, u32 poolSize,
                                 s32 numHandles, u32 granularity);
void  mpLinkedMemoryManager_AddMemBlock(MPF_LinkedMemoryManager *mgr, void *block, u32 size);
void  mpLinkedMemoryManager_Shutdown(MPF_LinkedMemoryManager *mgr);
void  mpLinkedMemoryManager_Dump(MPF_LinkedMemoryManager *mgr);
void *mpLinkedMemoryManager_AllocPtr(MPF_LinkedMemoryManager *mgr, u32 size, s32 noError);
u32   mpLinkedMemoryManager_AllocHndl(MPF_LinkedMemoryManager *mgr, u32 size, s32 noError);
void  mpLinkedMemoryManager_FreePtr(MPF_LinkedMemoryManager *mgr, void *ptr);
void  mpLinkedMemoryManager_FreeHndl(MPF_LinkedMemoryManager *mgr, u32 handle);
u32   mpLinkedMemoryManager_GetFreeMem(MPF_LinkedMemoryManager *mgr);
u32   mpLinkedMemoryManager_GetUsedMem(MPF_LinkedMemoryManager *mgr);
s32   mpLinkedMemoryManager_GetNumFreeBlocks(MPF_LinkedMemoryManager *mgr);
void  mpLinkedMemoryManager_Pack(MPF_LinkedMemoryManager *mgr);
u32   mpLinkedMemoryManager_SizeOfPtr(MPF_LinkedMemoryManager *mgr, void *ptr);
u32   mpLinkedMemoryManager_SizeOfHndl(MPF_LinkedMemoryManager *mgr, u32 handle);
s32   mpLinkedMemoryManager_ResizePtr(MPF_LinkedMemoryManager *mgr, void *ptr, u32 size, s32 noError);
s32   mpLinkedMemoryManager_ResizeHndl(MPF_LinkedMemoryManager *mgr, u32 handle, u32 size, s32 noError);
void *mpLinkedMemoryManager_LockHndl(MPF_LinkedMemoryManager *mgr, u32 handle);
void  mpLinkedMemoryManager_UnlockHndl(MPF_LinkedMemoryManager *mgr, u32 handle);
u8    mpLinkedMemoryManager_GetHndlIndexedByte(MPF_LinkedMemoryManager *mgr, u32 handle, s32 index);
s16   mpLinkedMemoryManager_GetHndlIndexedInt16(MPF_LinkedMemoryManager *mgr, u32 handle, s32 index);
s32   mpLinkedMemoryManager_GetHndlIndexedInt32(MPF_LinkedMemoryManager *mgr, u32 handle, s32 index);

/* MPF_MemoryManager base methods (0x8008B050-0x8008B350) */
void  mpMemoryManager_Error(MPF_MemoryManager *mgr, const char *msg);
void  mpMemoryManager_Errorf(MPF_MemoryManager *mgr, const char *fmt, ...);
void  mpMemoryManager_FuncError(MPF_MemoryManager *mgr, const char *func, const char *msg);
void  mpMemoryManager_FuncErrorf(MPF_MemoryManager *mgr, const char *func, const char *fmt, ...);
void *mpMemoryManager_DuplicatePtr(MPF_MemoryManager *mgr, void *ptr, s32 noError);
u32   mpMemoryManager_DuplicateHndl(MPF_MemoryManager *mgr, u32 handle, s32 noError);
void *mpMemoryManager_GrowHndl(MPF_MemoryManager *mgr, u32 handle, u32 extra, s32 noError);

/* the game's global memory manager (N64 side) */
extern MPF_LinkedMemoryManager gN64MemoryManager;
void  n64MemoryManager_Init(u32 start, u32 end, s32 numHandles);

/* MPF platform helpers (mpfplat.c, mpfprint.c) */
#include "mpfplat.h"

#endif /* MPFMEM_H */
