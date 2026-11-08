/*
 * N64 memory manager setup (original object file 0x8008E9C0-0x8008EA70).
 * The game keeps one global MPF_LinkedMemoryManager for all its memory.
 */
#include "mpfmem.h"

/* In the game's data (0x800C0E30 / 0x800C0ED0): the manager is statically
   initialised there, so it stays a data symbol. */
extern s32 gN64MemoryManagerInitialized;

/* 0x8008E9C0 */
void n64MemoryManager_Init(u32 start, u32 end, s32 numHandles) {
    if (gN64MemoryManagerInitialized) return;

    mpLinkedMemoryManager_Construct(&gN64MemoryManager);
    mpStrCpy(gN64MemoryManager.base.name, "N64MemoryManager");
    if (start & 0xF) {
        start = (start & ~0xFu) + 0x10;
    }
    end &= ~0xFu;
    mpLinkedMemoryManager_Init(&gN64MemoryManager, (void *)start, end - start, numHandles, 16);
    gN64MemoryManagerInitialized = 1;
}
