/*
 * MPFLNKMM.C - linked-list memory manager (0x8008B350-0x8008E9C0)
 *
 * Functional decompilation. Control flow follows the original closely,
 * including what happens after an error is reported: the game's fatal error
 * screen can return (debug "continue"), so the code carries on as the
 * original does.
 */
#include "mpfmem.h"

#define MGR(m) (&(m)->base)
#define NODE_OK(n) ((n)->magic == MPF_NODE_USED || (n)->magic == MPF_NODE_FREE)

static void mpLinkedMemoryManager_CheckInit(MPF_LinkedMemoryManager *mgr) {
    if (!mgr->initialized) {
        mpFatalErrorf("MPFLNKMM.C - Attempt to use MPF_LinkedMemoryManager \"%s\" while not initialized",
                      mgr->base.name);
    }
}

/* The recursive lock is written out inline in every function of the original. */
static inline void Lock(MPF_LinkedMemoryManager *mgr) {
    if (mgr->mutex.owner != osGetThreadId(NULL)) {
        osRecvMesg(&mgr->mutex.mq, NULL, OS_MESG_BLOCK);
        mgr->mutex.owner = osGetThreadId(NULL);
    }
    mgr->mutex.count++;
}

static inline void Unlock(MPF_LinkedMemoryManager *mgr) {
    if (mgr->mutex.owner == osGetThreadId(NULL)) {
        if (--mgr->mutex.count == 0) {
            mgr->mutex.owner = 0;
            osSendMesg(&mgr->mutex.mq, NULL, OS_MESG_NOBLOCK);
        }
    }
}

static inline u32 RoundUp(MPF_LinkedMemoryManager *mgr, u32 size) {
    if (size & mgr->granMask) {
        size = (size & mgr->granInvMask) + mgr->granularity;
    }
    return size;
}

/* Real (global) functions so they replace the original's FindFreeHandle /
   InsertUsedNodeLow; the other helpers were always inlined into their callers. */
s32 mpLinkedMemoryManager_FindFreeHandle(MPF_LinkedMemoryManager *mgr);
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_FindFirstFit(MPF_LinkedMemoryManager *mgr, u32 size);
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_FindLastFit(MPF_LinkedMemoryManager *mgr, u32 size);
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_MakeNodeFree(MPF_LinkedMemoryManager *mgr, MPF_LinkedMemoryNode *node);
static s32 mpLinkedMemoryManager_ResizeNode(MPF_LinkedMemoryManager *mgr, MPF_LinkedMemoryNode *node, u32 size);
MPF_LinkedMemoryNode *mpLinkedMemoryManager_InsertUsedNodeLow(MPF_LinkedMemoryManager *mgr,
        MPF_LinkedMemoryNode *node, u32 size, const void *src, u32 srcLen);
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_InsertUsedNodeHigh(MPF_LinkedMemoryManager *mgr,
        MPF_LinkedMemoryNode *node, u32 size, const void *src, u32 srcLen);

/* 0x8008B350 */
void mpLinkedMemoryManager_Construct(MPF_LinkedMemoryManager *mgr) {
    static char defaultName[] = "Unnamed LinkedMemMgr";
    MPF_MemoryManager *b = &mgr->base;

    b->name = defaultName;
    b->allocPtr = (void *)mpLinkedMemoryManager_AllocPtr;
    b->allocHndl = (void *)mpLinkedMemoryManager_AllocHndl;
    b->freePtr = (void *)mpLinkedMemoryManager_FreePtr;
    b->freeHndl = (void *)mpLinkedMemoryManager_FreeHndl;
    b->getFreeMem = (void *)mpLinkedMemoryManager_GetFreeMem;
    b->getUsedMem = (void *)mpLinkedMemoryManager_GetUsedMem;
    b->getNumFreeBlocks = (void *)mpLinkedMemoryManager_GetNumFreeBlocks;
    b->pack = (void *)mpLinkedMemoryManager_Pack;
    b->sizeOfPtr = (void *)mpLinkedMemoryManager_SizeOfPtr;
    b->sizeOfHndl = (void *)mpLinkedMemoryManager_SizeOfHndl;
    b->duplicatePtr = mpMemoryManager_DuplicatePtr;
    b->duplicateHndl = mpMemoryManager_DuplicateHndl;
    b->resizePtr = (void *)mpLinkedMemoryManager_ResizePtr;
    b->resizeHndl = (void *)mpLinkedMemoryManager_ResizeHndl;
    b->growHndl = mpMemoryManager_GrowHndl;
    b->lockHndl = (void *)mpLinkedMemoryManager_LockHndl;
    b->unlockHndl = (void *)mpLinkedMemoryManager_UnlockHndl;
    b->getHndlIndexedByte = (void *)mpLinkedMemoryManager_GetHndlIndexedByte;
    b->getHndlIndexedInt16 = (void *)mpLinkedMemoryManager_GetHndlIndexedInt16;
    b->getHndlIndexedInt32 = (void *)mpLinkedMemoryManager_GetHndlIndexedInt32;
    mgr->initialized = 0;
    mgr->granularity = 0;
    mgr->granMask = 0;
    mgr->granInvMask = ~0u;
    mgr->lastHandle = 0;
    mgr->numHandles = 0;
    mgr->handles = NULL;
    mgr->handleTableSize = 0;
}

/* 0x8008B47C */
void mpLinkedMemoryManager_Destruct(MPF_LinkedMemoryManager *mgr) {
    mpLinkedMemoryManager_Shutdown(mgr);
}

/* 0x8008B498 */
void mpLinkedMemoryManager_Init(MPF_LinkedMemoryManager *mgr, void *pool, u32 poolSize,
                                s32 numHandles, u32 granularity) {
    MPF_LinkedMemoryNode *node;
    u32 tableSize;

    if (mgr->initialized) {
        mpFatalErrorf("MPFLNKMM.C : mpLinkedMemoryManager_Init() - LinkedMemoryManager initialized twice");
    }
    mpMutex_Init(&mgr->mutex);
    mgr->granularity = granularity;
    switch (granularity) {
        case 1:  mgr->granMask = 0;   break;
        case 2:  mgr->granMask = 1;   break;
        case 4:  mgr->granMask = 3;   break;
        case 8:  mgr->granMask = 7;   break;
        case 16: mgr->granMask = 15;  break;
        default:
            mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_Init() - Granularity must be 1, 2, 4, 8, or 16");
            break;
    }
    mgr->granInvMask = ~mgr->granMask;
    if (numHandles > 0xFFFF) {
        mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_Init() - Number of handles exceeds maximum (65535)");
    }
    tableSize = (u32)numHandles * 8;
    mgr->handleTableSize = tableSize;
    if (tableSize & mgr->granMask) {
        mgr->handleTableSize = (tableSize & mgr->granInvMask) + mgr->granularity;
    }
    if (poolSize < mgr->handleTableSize + 16) {
        mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_Init() - Initial memory pool is not large enough to include handle table");
    }
    mgr->numHandles = numHandles;
    mgr->handles = pool;
    bzero(pool, numHandles * 8);

    node = (MPF_LinkedMemoryNode *)((u8 *)pool + mgr->handleTableSize);
    mgr->first = node;
    mgr->last = node;
    mgr->first->magic = MPF_NODE_FREE;
    mgr->first->handle = 0;
    mgr->first->size = (poolSize - mgr->handleTableSize) & mgr->granInvMask;
    mgr->first->next = NULL;
    mgr->first->prev = NULL;
    mgr->initialized = 1;
}

/* 0x8008B650 - adds another (separate) region of memory to the pool */
void mpLinkedMemoryManager_AddMemBlock(MPF_LinkedMemoryManager *mgr, void *block, u32 size) {
    MPF_LinkedMemoryNode *node = block;
    MPF_LinkedMemoryNode *cur, *after = NULL;

    mpLinkedMemoryManager_CheckInit(mgr);
    node->magic = MPF_NODE_USED;
    node->handle = 0;
    node->size = size & mgr->granInvMask;

    /* find the last node below the new block */
    for (cur = mgr->first; cur != NULL && cur < node; cur = cur->next) {
        after = cur;
    }
    if (after == NULL) {
        node->prev = NULL;
        node->next = mgr->first;
        mgr->first->prev = node;
        mgr->first = node;
    } else if (node->prev = after, after == mgr->last) {
        node->next = NULL;
        mgr->last->next = node;
        mgr->last = node;
    } else {
        node->next = after->next;
        after->next->prev = node;
        node->prev->next = node;
    }
    if (node->prev != NULL && node < (MPF_LinkedMemoryNode *)((u8 *)node->prev + node->prev->size)) {
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_AddMemBlock", "Attempt to add overlapping memory");
    }
    if (node->next != NULL && node->next < (MPF_LinkedMemoryNode *)((u8 *)node + node->size)) {
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_AddMemBlock", "Attempt to add overlapping memory");
    }
    mpLinkedMemoryManager_MakeNodeFree(mgr, node);
}

/* 0x8008B7C0 */
void mpLinkedMemoryManager_Shutdown(MPF_LinkedMemoryManager *mgr) {
    if (mgr->initialized) {
        mgr->handles = NULL;
        mgr->first = NULL;
        mgr->last = NULL;
        mgr->initialized = 0;
    }
}

/* 0x8008B7E4 - debug listing of the block list (the print function is empty in this build) */
void mpLinkedMemoryManager_Dump(MPF_LinkedMemoryManager *mgr) {
    MPF_LinkedMemoryNode *node;
    u32 used, freeMem;
    s32 frags;

    for (node = mgr->first; node != NULL; node = node->next) {
        if (!NODE_OK(node)) return;
        mpDebugPrintf("%08x  ", node);
        mpDebugPrintf("%06x  ", node->size);
        if (node->magic != MPF_NODE_FREE) {
            mpDebugPrintf("%04x  ", node->handle);
            if (node->magic != MPF_NODE_FREE) {
                mpDebugPrintf("%02x  ", mgr->handles[node->handle].lockCount);
                if (node->magic != MPF_NODE_FREE) {
                    mpDebugPrintf("%04x  ", mgr->handles[node->handle].unk4);
                }
            }
        }
    }
    used = mpLinkedMemoryManager_GetUsedMem(mgr);
    freeMem = mpLinkedMemoryManager_GetFreeMem(mgr);
    frags = mpLinkedMemoryManager_GetNumFreeBlocks(mgr) - 1;
    if (frags < 0) frags = 0;
    mpDebugPrintf("Total = %d, Used = %d, Free = %d, Frags = %d\n", used + freeMem, used, freeMem, frags);
}

/* Sets up the handle entry for a freshly placed block. */
static inline void SetHandle(MPF_LinkedMemoryManager *mgr, s32 h, MPF_LinkedMemoryNode *node,
                             u32 dataSize, u8 lockCount) {
    MPF_LinkedMemMgrHandleEntry *e = &mgr->handles[h];
    node->handle = h;
    e->ptr = node + 1;
    e->unk4 = 0;
    e->slack = (u8)(node->size - (dataSize + 16));
    e->lockCount = lockCount;
}

/* 0x8008B934 - fixed allocation (taken from the bottom of the pool) */
void *mpLinkedMemoryManager_AllocPtr(MPF_LinkedMemoryManager *mgr, u32 size, s32 noError) {
    s32 h;
    u32 need;
    MPF_LinkedMemoryNode *node;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    h = mpLinkedMemoryManager_FindFreeHandle(mgr);
    if (h == 0) {
        Unlock(mgr);
        if (noError) return NULL;
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_AllocPtr", "Out of handles");
    }
    need = RoundUp(mgr, size + 16);
    node = mpLinkedMemoryManager_FindFirstFit(mgr, need);
    if (node == NULL) {
        mpLinkedMemoryManager_Pack(mgr);
        node = mpLinkedMemoryManager_FindFirstFit(mgr, need);
        if (node == NULL) {
            Unlock(mgr);
            if (noError) return NULL;
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_AllocPtr", "Out of memory");
        }
    }
    node = mpLinkedMemoryManager_InsertUsedNodeLow(mgr, node, need, NULL, 0);
    SetHandle(mgr, h, node, size, MPF_LOCK_POINTER);
    Unlock(mgr);
    return node + 1;
}

/* 0x8008BB8C - relocatable allocation (taken from the top of the pool) */
u32 mpLinkedMemoryManager_AllocHndl(MPF_LinkedMemoryManager *mgr, u32 size, s32 noError) {
    s32 h;
    u32 need;
    MPF_LinkedMemoryNode *node;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    h = mpLinkedMemoryManager_FindFreeHandle(mgr);
    if (h == 0) {
        Unlock(mgr);
        if (noError) return 0;
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_AllocHndl", "Out of handles");
    }
    need = RoundUp(mgr, size + 16);
    node = mpLinkedMemoryManager_FindLastFit(mgr, need);
    if (node == NULL) {
        mpLinkedMemoryManager_Pack(mgr);
        node = mpLinkedMemoryManager_FindLastFit(mgr, need);
        if (node == NULL) {
            Unlock(mgr);
            if (noError) return 0;
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_AllocHndl", "Out of memory");
        }
    }
    node = mpLinkedMemoryManager_InsertUsedNodeHigh(mgr, node, need, NULL, 0);
    SetHandle(mgr, h, node, size, 0);
    Unlock(mgr);
    return h;
}

/* Checks that `ptr` came from AllocPtr/AllocHndl of this manager. */
static inline s32 PtrIsValid(MPF_LinkedMemoryManager *mgr, void *ptr) {
    MPF_LinkedMemoryNode *node = (MPF_LinkedMemoryNode *)ptr - 1;
    return node->magic == MPF_NODE_USED && (s32)node->handle < mgr->numHandles &&
           mgr->handles[node->handle].ptr == ptr;
}

/*
 * Handle checks shared by the handle functions. Reports the first problem
 * found (with the lock released first, as in the original) and carries on.
 */
static void CheckHandle(MPF_LinkedMemoryManager *mgr, u32 h, const char *func,
                        const char *invalidMsg, const char *nullMsg, const char *emptyMsg) {
    MPF_LinkedMemoryNode *node;

    if (h >= (u32)mgr->numHandles) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), func, invalidMsg);
    }
    if (h == 0) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), func, nullMsg);
    }
    if (mgr->handles[h].ptr == NULL) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), func, emptyMsg);
    }
    node = (MPF_LinkedMemoryNode *)mgr->handles[h].ptr - 1;
    if (node->magic != MPF_NODE_USED || node->handle != h) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), func, "Handle points to bad memory node.  Possible corrupted memory list");
    }
}

static inline void ClearHandle(MPF_LinkedMemoryManager *mgr, u32 h) {
    mgr->handles[h].ptr = NULL;
    mgr->handles[h].unk4 = 0;
    mgr->handles[h].slack = 0;
    mgr->handles[h].lockCount = 0;
}

/* 0x8008BDE0 */
void mpLinkedMemoryManager_FreePtr(MPF_LinkedMemoryManager *mgr, void *ptr) {
    MPF_LinkedMemoryNode *node;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    if (ptr == NULL) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_FreePtr", "Attempt to free NULL pointer");
    }
    node = (MPF_LinkedMemoryNode *)ptr - 1;
    if (!PtrIsValid(mgr, ptr)) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_FreePtr", "Attempt to free invalid pointer");
    }
    ClearHandle(mgr, node->handle);
    mpLinkedMemoryManager_MakeNodeFree(mgr, node);
    Unlock(mgr);
}

/* 0x8008BFCC */
void mpLinkedMemoryManager_FreeHndl(MPF_LinkedMemoryManager *mgr, u32 handle) {
    MPF_LinkedMemoryNode *node;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_FreeHndl", "Attempt to free invalid handle",
                "Attempt to free NULL handle", "Attempt to free empty handle");
    node = (MPF_LinkedMemoryNode *)mgr->handles[handle].ptr - 1;
    ClearHandle(mgr, handle);
    mpLinkedMemoryManager_MakeNodeFree(mgr, node);
    Unlock(mgr);
}

/* 0x8008C254 - free bytes (block headers not counted) */
u32 mpLinkedMemoryManager_GetFreeMem(MPF_LinkedMemoryManager *mgr) {
    MPF_LinkedMemoryNode *node;
    u32 total = 0;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    for (node = mgr->first; node != NULL; node = node->next) {
        if (node->magic == MPF_NODE_FREE) {
            total = total - 16 + node->size;
        } else if (node->magic != MPF_NODE_USED) {
            Unlock(mgr);
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_GetFreeMem", "Memory list corrupted");
        }
    }
    Unlock(mgr);
    return total;
}

/* 0x8008C3B4 - used bytes: handle table, used blocks and the headers of free blocks */
u32 mpLinkedMemoryManager_GetUsedMem(MPF_LinkedMemoryManager *mgr) {
    MPF_LinkedMemoryNode *node;
    u32 total;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    total = mgr->handleTableSize;
    for (node = mgr->first; node != NULL; node = node->next) {
        if (node->magic == MPF_NODE_USED) {
            total += node->size;
        } else if (node->magic == MPF_NODE_FREE) {
            total += 16;            /* a free block's header still takes space */
        } else {
            Unlock(mgr);
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_GetUsedMem", "Memory list corrupted");
        }
    }
    Unlock(mgr);
    return total;
}

/* 0x8008C51C */
s32 mpLinkedMemoryManager_GetNumFreeBlocks(MPF_LinkedMemoryManager *mgr) {
    MPF_LinkedMemoryNode *node;
    s32 count = 0;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    for (node = mgr->first; node != NULL; node = node->next) {
        if (node->magic == MPF_NODE_FREE) {
            count++;
        } else if (node->magic != MPF_NODE_USED) {
            Unlock(mgr);
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_GetNumFreeBlocks", "Memory list corrupted");
        }
    }
    Unlock(mgr);
    return count;
}

/* Steps back from `node` to the previous used block, skipping free ones. */
static MPF_LinkedMemoryNode *PrevUsed(MPF_LinkedMemoryManager *mgr, MPF_LinkedMemoryNode *node) {
    while (node != NULL && node->magic != MPF_NODE_USED) {
        if (node->magic != MPF_NODE_FREE) {
            Unlock(mgr);
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_Pack", "Memory list corrupted");
        }
        node = node->prev;
    }
    return node;
}

/*
 * 0x8008C678 - defragments memory: from the top down, every unlocked handle
 * block is moved into the highest free block above it that is big enough,
 * or slid up into an adjacent free gap.
 */
void mpLinkedMemoryManager_Pack(MPF_LinkedMemoryManager *mgr) {
    MPF_LinkedMemoryNode *node, *nextUsed, *dest, *moved, *f;
    u32 need, dataSize;
    u16 h;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    node = PrevUsed(mgr, mgr->last);
    while (node != NULL) {
        nextUsed = PrevUsed(mgr, node->prev);
        h = node->handle;
        if (mgr->handles[h].lockCount != 0) {
            node = nextUsed;
            continue;
        }
        need = RoundUp(mgr, node->size - mgr->handles[h].slack);
        dataSize = node->size - (mgr->handles[h].slack + 16);

        /* highest free block above this one that fits */
        dest = NULL;
        for (f = mgr->last; f != NULL && node < f; f = f->prev) {
            if (f->magic == MPF_NODE_FREE && f->size >= need) {
                dest = f;
                break;
            }
            if (f->magic != MPF_NODE_FREE && f->magic != MPF_NODE_USED) {
                Unlock(mgr);
                mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_Pack", "Memory list corrupted");
            }
        }

        if (dest != NULL) {
            moved = mpLinkedMemoryManager_InsertUsedNodeHigh(mgr, dest, need, node + 1, dataSize);
            moved->handle = h;
            mgr->handles[h].ptr = moved + 1;
            mgr->handles[h].slack = (u8)(moved->size - (dataSize + 16));
            mpLinkedMemoryManager_MakeNodeFree(mgr, node);
        } else {
            MPF_LinkedMemoryNode *nx = node->next;
            s32 slide = 0;
            if (nx != NULL && nx->magic == MPF_NODE_FREE &&
                (MPF_LinkedMemoryNode *)((u8 *)node + node->size) == nx) {
                slide = 1;      /* free space directly above */
            } else if (need < node->size && node->prev != NULL &&
                       (MPF_LinkedMemoryNode *)((u8 *)node->prev + node->prev->size) == node) {
                slide = 1;      /* spare space to hand back to the block below */
            }
            if (slide) {
                f = mpLinkedMemoryManager_MakeNodeFree(mgr, node);
                moved = mpLinkedMemoryManager_InsertUsedNodeHigh(mgr, f, need, node + 1, dataSize);
                moved->handle = h;
                mgr->handles[h].ptr = moved + 1;
                mgr->handles[h].slack = (u8)(moved->size - (dataSize + 16));
            }
        }
        node = nextUsed;
    }
    Unlock(mgr);
}

/* 0x8008CA9C */
u32 mpLinkedMemoryManager_SizeOfPtr(MPF_LinkedMemoryManager *mgr, void *ptr) {
    MPF_LinkedMemoryNode *node;
    u32 size;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    if (ptr == NULL) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_SizeOfPtr", "Attempt to take size of NULL pointer");
    }
    node = (MPF_LinkedMemoryNode *)ptr - 1;
    if (!PtrIsValid(mgr, ptr)) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_SizeOfPtr", "Attempt to take size of invalid pointer");
    }
    size = node->size - (mgr->handles[node->handle].slack + 16);
    Unlock(mgr);
    return size;
}

/* 0x8008CC50 */
u32 mpLinkedMemoryManager_SizeOfHndl(MPF_LinkedMemoryManager *mgr, u32 handle) {
    MPF_LinkedMemoryNode *node;
    u32 size;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_SizeOfHndl", "Attempt to take size of invalid handle",
                "Attempt to take size of NULL handle", "Attempt to take size of empty handle");
    node = (MPF_LinkedMemoryNode *)mgr->handles[handle].ptr - 1;
    size = node->size - (mgr->handles[node->handle].slack + 16);
    Unlock(mgr);
    return size;
}

/* 0x8008CEBC */
s32 mpLinkedMemoryManager_ResizePtr(MPF_LinkedMemoryManager *mgr, void *ptr, u32 size, s32 noError) {
    MPF_LinkedMemoryNode *node;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    if (ptr == NULL) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_ResizePtr", "Attempt to resize NULL pointer");
    }
    node = (MPF_LinkedMemoryNode *)ptr - 1;
    if (!PtrIsValid(mgr, ptr)) {
        Unlock(mgr);
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_ResizePtr", "Attempt to resize invalid pointer");
    }
    if (!mpLinkedMemoryManager_ResizeNode(mgr, node, size)) {
        mpLinkedMemoryManager_Pack(mgr);
        if (!mpLinkedMemoryManager_ResizeNode(mgr, node, size)) {
            Unlock(mgr);
            if (noError) return 0;
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_ResizePtr",
                                      "Insufficient adjacent free memory to resize pointer");
        }
    }
    Unlock(mgr);
    return 1;
}

/* 0x8008D0F4 - resizes in place, or moves an unlocked handle to a new block */
s32 mpLinkedMemoryManager_ResizeHndl(MPF_LinkedMemoryManager *mgr, u32 handle, u32 size, s32 noError) {
    MPF_LinkedMemoryNode *node;
    u32 newHandle;
    void *src, *dst;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_ResizeHndl", "Attempt to resize invalid handle",
                "Attempt to resize NULL handle", "Attempt to resize empty handle");
    node = (MPF_LinkedMemoryNode *)mgr->handles[handle].ptr - 1;
    if (!mpLinkedMemoryManager_ResizeNode(mgr, node, size)) {
        if (mgr->handles[handle].lockCount != 0) {
            Unlock(mgr);
            if (noError) return 0;
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_ResizeHndl",
                                      "Insufficient adjacent free memory to resize locked handle");
        }
        newHandle = mpLinkedMemoryManager_AllocHndl(mgr, size, noError);
        if (newHandle == 0) {
            Unlock(mgr);
            return 0;
        }
        src = mpLinkedMemoryManager_LockHndl(mgr, handle);
        dst = mpLinkedMemoryManager_LockHndl(mgr, newHandle);
        bcopy(src, dst, mpLinkedMemoryManager_SizeOfHndl(mgr, handle));
        mpLinkedMemoryManager_UnlockHndl(mgr, newHandle);
        mpLinkedMemoryManager_UnlockHndl(mgr, handle);
        mpLinkedMemoryManager_FreeHndl(mgr, handle);
        /* the new block takes over the old handle number */
        bcopy(&mgr->handles[newHandle], &mgr->handles[handle], sizeof(MPF_LinkedMemMgrHandleEntry));
        bzero(&mgr->handles[newHandle], sizeof(MPF_LinkedMemMgrHandleEntry));
        ((MPF_LinkedMemoryNode *)mgr->handles[handle].ptr - 1)->handle = handle;
    }
    Unlock(mgr);
    return 1;
}

/* 0x8008D4D4 */
void *mpLinkedMemoryManager_LockHndl(MPF_LinkedMemoryManager *mgr, u32 handle) {
    MPF_LinkedMemoryNode *node;
    MPF_LinkedMemMgrHandleEntry *e;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_LockHndl", "Attempt to lock invalid handle",
                "Attempt to lock NULL handle", "Attempt to lock empty handle");
    node = (MPF_LinkedMemoryNode *)mgr->handles[handle].ptr - 1;
    e = &mgr->handles[handle];
    if (e->lockCount != MPF_LOCK_POINTER) {
        if (e->lockCount == MPF_LOCK_MAX) {
            Unlock(mgr);
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_LockHndl",
                                      "Lock count overflow!  Maximum count is 254");
        }
        mgr->handles[handle].lockCount++;
    }
    Unlock(mgr);
    return node + 1;
}

/* 0x8008D7A8 */
void mpLinkedMemoryManager_UnlockHndl(MPF_LinkedMemoryManager *mgr, u32 handle) {
    MPF_LinkedMemMgrHandleEntry *e;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_UnlockHndl", "Attempt to unlock invalid handle",
                "Attempt to unlock NULL handle", "Attempt to unlock empty handle");
    e = &mgr->handles[handle];
    if (e->lockCount != MPF_LOCK_POINTER && e->lockCount != 0) {
        e->lockCount--;
    }
    Unlock(mgr);
}

/* 0x8008DA14 */
u8 mpLinkedMemoryManager_GetHndlIndexedByte(MPF_LinkedMemoryManager *mgr, u32 handle, s32 index) {
    u8 v;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_GetHndlIndexedByte", "Attempt to index into invalid handle",
                "Attempt to index into NULL handle", "Attempt to index into empty handle");
    v = ((u8 *)mgr->handles[handle].ptr)[index];
    Unlock(mgr);
    return v;
}

/* 0x8008DC7C */
s16 mpLinkedMemoryManager_GetHndlIndexedInt16(MPF_LinkedMemoryManager *mgr, u32 handle, s32 index) {
    s16 v;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_GetHndlIndexedInt16", "Attempt to index into invalid handle",
                "Attempt to index into NULL handle", "Attempt to index into empty handle");
    v = ((s16 *)mgr->handles[handle].ptr)[index];
    Unlock(mgr);
    return v;
}

/* 0x8008DEEC */
s32 mpLinkedMemoryManager_GetHndlIndexedInt32(MPF_LinkedMemoryManager *mgr, u32 handle, s32 index) {
    s32 v;

    mpLinkedMemoryManager_CheckInit(mgr);
    Lock(mgr);
    CheckHandle(mgr, handle, "mpLinkedMemoryManager_GetHndlIndexedInt32", "Attempt to index into invalid handle",
                "Attempt to index into NULL handle", "Attempt to index into empty handle");
    v = ((s32 *)mgr->handles[handle].ptr)[index];
    Unlock(mgr);
    return v;
}

/* 0x8008E158 - next unused handle after the last one handed out (1..numHandles-1) */
s32 mpLinkedMemoryManager_FindFreeHandle(MPF_LinkedMemoryManager *mgr) {
    s32 h = mgr->lastHandle + 1;
    s32 tried;

    for (tried = 0; tried < mgr->numHandles; h++) {
        if (h >= mgr->numHandles) h = 1;
        tried++;
        if (mgr->handles[h].ptr == NULL) {
            mgr->lastHandle = h;
            return h;
        }
    }
    return 0;
}

/* 0x8008E1B8 */
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_FindFirstFit(MPF_LinkedMemoryManager *mgr, u32 size) {
    MPF_LinkedMemoryNode *node;

    for (node = mgr->first; node != NULL; node = node->next) {
        if (node->magic == MPF_NODE_FREE) {
            if (node->size >= size) return node;
        } else if (node->magic != MPF_NODE_USED) {
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_FindFirstFit", "Memory list corrupted");
        }
    }
    return NULL;
}

/* 0x8008E260 */
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_FindLastFit(MPF_LinkedMemoryManager *mgr, u32 size) {
    MPF_LinkedMemoryNode *node;

    for (node = mgr->last; node != NULL; node = node->prev) {
        if (node->magic == MPF_NODE_FREE) {
            if (node->size >= size) return node;
        } else if (node->magic != MPF_NODE_USED) {
            mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_FindLastFit", "Memory list corrupted");
        }
    }
    return NULL;
}

/*
 * 0x8008E308 - marks a block free and merges it with free neighbours. If the
 * block below is in use and has slack, that slack is reclaimed by moving this
 * block's header down. Returns the resulting free block.
 */
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_MakeNodeFree(MPF_LinkedMemoryManager *mgr,
                                                                MPF_LinkedMemoryNode *node) {
    MPF_LinkedMemoryNode *prev, *next, *result;
    u32 pad;

    node->magic = MPF_NODE_FREE;
    node->handle = 0;
    result = node;
    prev = node->prev;
    next = node->next;

    if (!NODE_OK(node) || (prev != NULL && !NODE_OK(prev)) || (next != NULL && !NODE_OK(next))) {
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_MakeNodeFree", "Memory list corrupted");
    }
    /* merge with the free block above */
    if (next != NULL && next->magic == MPF_NODE_FREE &&
        (MPF_LinkedMemoryNode *)((u8 *)node + node->size) == next) {
        node->next = next->next;
        node->size += next->size;
        if (node->next == NULL) mgr->last = node;
        else node->next->prev = node;
    }
    if (prev == NULL || (MPF_LinkedMemoryNode *)((u8 *)prev + prev->size) != node) {
        return result;
    }
    if (prev->magic == MPF_NODE_FREE) {
        /* merge into the free block below */
        prev->next = node->next;
        prev->size += node->size;
        if (prev->next == NULL) mgr->last = prev;
        else prev->next->prev = prev;
        return prev;
    }
    /* block below is in use: take back its slack */
    pad = mgr->handles[prev->handle].slack & mgr->granInvMask;
    if (pad != 0) {
        MPF_LinkedMemoryNode *moved = (MPF_LinkedMemoryNode *)((u8 *)node - pad);
        mpMemMove(moved, node, 16);
        moved->size = pad + moved->size;
        prev->size -= pad;
        mgr->handles[prev->handle].slack -= pad;
        if (moved->next == NULL) mgr->last = moved;
        else moved->next->prev = moved;
        if (moved->prev == NULL) mgr->first = moved;
        else moved->prev->next = moved;
        result = moved;
    }
    return result;
}

/*
 * 0x8008E528 - resizes a used block in place (growing into a free block
 * directly above if needed) and returns spare space of 16+ bytes as a new
 * free block. Returns 0 if there isn't room.
 */
static s32 mpLinkedMemoryManager_ResizeNode(MPF_LinkedMemoryManager *mgr, MPF_LinkedMemoryNode *node, u32 size) {
    MPF_LinkedMemoryNode *next, *split, *nx;
    u32 need, rem;

    if (!NODE_OK(node) || (node->next != NULL && !NODE_OK(node->next)) ||
        (node->prev != NULL && !NODE_OK(node->prev))) {
        mpMemoryManager_FuncError(MGR(mgr), "mpLinkedMemoryManager_MakeNodeFree", "Memory list corrupted");
    }
    need = RoundUp(mgr, size + 16);

    if (node->size < need) {
        next = node->next;
        if (next == NULL) return 0;
        if (next->magic != MPF_NODE_FREE) return 0;
        if ((MPF_LinkedMemoryNode *)((u8 *)node + node->size) != next) return 0;
        if (node->size + next->size < need) return 0;
        node->size += node->next->size;
        node->next = node->next->next;
        if (node->next != NULL) node->next->prev = node;
        else mgr->last = node;
    }

    rem = node->size - need;
    if (rem < 16) {
        need = node->size;          /* keep the whole block */
    } else {
        split = (MPF_LinkedMemoryNode *)((u8 *)node + need);
        split->magic = MPF_NODE_FREE;
        split->handle = 0;
        split->size = rem;
        split->next = node->next;
        split->prev = node;
        node->next = split;
        if (split->next == NULL) mgr->last = split;
        else split->next->prev = split;

        nx = split->next;
        if (nx != NULL && nx->magic == MPF_NODE_FREE &&
            (MPF_LinkedMemoryNode *)((u8 *)split + split->size) == nx) {
            split->size += nx->size;
            split->next = split->next->next;
            if (split->next != NULL) split->next->prev = split;
            else mgr->last = split;
        }
    }
    node->size = need;
    mgr->handles[node->handle].slack = (u8)(need - (size + 16));
    return 1;
}

/* 0x8008E750 - turns the bottom `size` bytes of a free block into a used block */
MPF_LinkedMemoryNode *mpLinkedMemoryManager_InsertUsedNodeLow(MPF_LinkedMemoryManager *mgr,
        MPF_LinkedMemoryNode *node, u32 size, const void *src, u32 srcLen) {
    MPF_LinkedMemoryNode *used = node, *rest;
    u32 rem;

    if (src != NULL) {
        mpMemMove(used + 1, src, srcLen);
    }
    rem = node->size - size;
    if (rem != 0) {
        if (rem >= 16) {
            rest = (MPF_LinkedMemoryNode *)((u8 *)node + size);
            rest->magic = MPF_NODE_FREE;
            rest->handle = 0;
            rest->size = rem;
            rest->next = used->next;
            rest->prev = used;
            used->next = rest;
            if (rest->next == NULL) mgr->last = rest;
            else rest->next->prev = rest;
        } else {
            if ((s32)rem < 0) {
                mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_InsertUsedNodeLow() - *** Free node given is too small");
            }
            size = node->size;      /* too small to split: use the whole block */
        }
    }
    used->magic = MPF_NODE_USED;
    used->handle = 0;
    used->size = size;
    return used;
}

/* 0x8008E820 - turns the top `size` bytes of a free block into a used block */
static MPF_LinkedMemoryNode *mpLinkedMemoryManager_InsertUsedNodeHigh(MPF_LinkedMemoryManager *mgr,
        MPF_LinkedMemoryNode *node, u32 size, const void *src, u32 srcLen) {
    MPF_LinkedMemoryNode *used = node;
    MPF_LinkedMemoryNode *next = node->next;
    MPF_LinkedMemoryNode *prev = node->prev;
    u32 rem = node->size - size;

    if (rem != 0) {
        if (rem >= 16) {
            used = (MPF_LinkedMemoryNode *)((u8 *)node + rem);
            node->size = rem;
            prev = node;
            node->next = used;
            if (next != NULL) next->prev = used;
            else mgr->last = used;
        } else {
            if ((s32)rem < 0) {
                mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_InsertUsedNodeHigh() - *** Free node given is too small");
            }
            if (prev != NULL && (MPF_LinkedMemoryNode *)((u8 *)prev + prev->size) == used) {
                /* hand the few spare bytes to the used block below as slack */
                if (prev->magic != MPF_NODE_USED) {
                    mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_InsertUsedNodeHigh() - *** Unexpected free node found");
                }
                used = (MPF_LinkedMemoryNode *)((u8 *)used + rem);
                if (next != NULL) next->prev = used;
                else mgr->last = used;
                prev->next = used;
                prev->size = rem + prev->size;
                mgr->handles[prev->handle].slack += rem;
                if (mgr->handles[prev->handle].slack >= 16) {
                    mpFatalError("MPFLNKMM.C : mpLinkedMemoryManager_InsertUsedNodeHigh() - *** Dude, pad size has accumulated beyond size of MPF_LinkedMemoryNode");
                }
            } else {
                size = node->size;  /* use the whole block */
            }
        }
    }
    if (src != NULL) {
        mpMemMove(used + 1, src, srcLen);
    }
    used->magic = MPF_NODE_USED;
    used->handle = 0;
    used->size = size;
    used->next = next;
    used->prev = prev;
    return used;
}
