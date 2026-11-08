/*
 * For tools/port_assets.py: unpacks MPF-compressed data (mpDataDecompress, the game's own C
 * from src/mpf) on this computer.
 *   mpf_unpack in.bin offset out.bin     unpacks the stream at offset of in.bin into out.bin
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "mpfcomp.h"

void mpFatalError(const char *msg) { fprintf(stderr, "mpf_unpack: %s\n", msg); exit(1); }
void mpFatalErrorf(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt); fprintf(stderr, "mpf_unpack: "); vfprintf(stderr, fmt, a); fprintf(stderr, "\n"); va_end(a);
    exit(1);
}
void mpDebugPrintf(const char *fmt, ...) { (void)fmt; }
/* the BSS mpfcomp2.c uses (sizes as in the game; pointers get 8 bytes on a 64-bit computer) */
char B_800ECFC0[16] __attribute__((aligned(16)));
char B_800ECFC4[24] __attribute__((aligned(16)));
char B_800ECFD0[2048] __attribute__((aligned(16)));
char B_800ED3D0[16] __attribute__((aligned(16)));
char B_800ED4E0[2080] __attribute__((aligned(16)));
char B_800ED8F0[2064] __attribute__((aligned(16)));
char B_800EDCF8[16] __attribute__((aligned(16)));
char B_800EDCFC[16] __attribute__((aligned(16)));
char B_800EDD00[16] __attribute__((aligned(16)));
char B_800EDD04[16] __attribute__((aligned(16)));
char B_800EDD08[16] __attribute__((aligned(16)));
char B_800EDD0C[16] __attribute__((aligned(16)));
char B_800EDD10[2048] __attribute__((aligned(16)));
char B_800EE110[16] __attribute__((aligned(16)));
char B_800EE220[2080] __attribute__((aligned(16)));
char B_800EE630[2064] __attribute__((aligned(16)));
char B_800EEA38[16] __attribute__((aligned(16)));
char B_800EEA3C[16] __attribute__((aligned(16)));
char B_800EEA40[16] __attribute__((aligned(16)));
char B_800EEA44[16] __attribute__((aligned(16)));
char B_800EEA48[16] __attribute__((aligned(16)));
char B_800EEA4C[16] __attribute__((aligned(16)));
char B_800EEA50[32] __attribute__((aligned(16)));
char B_800EEA60[1096] __attribute__((aligned(16)));
char B_800EEC84[16] __attribute__((aligned(16)));
char B_800EEC88[16] __attribute__((aligned(16)));
char B_800EEC8C[16] __attribute__((aligned(16)));
char B_800EEC90[16] __attribute__((aligned(16)));
char B_800EEC94[16] __attribute__((aligned(16)));
char B_800EEC98[16] __attribute__((aligned(16)));
char B_800EEC9C[16] __attribute__((aligned(16)));
char B_800EECA0[1096] __attribute__((aligned(16)));
char B_800EEEC4[16] __attribute__((aligned(16)));
char B_800EEEC8[16] __attribute__((aligned(16)));
char B_800EEECC[16] __attribute__((aligned(16)));
char B_800EEED0[16] __attribute__((aligned(16)));
char B_800EEED4[16] __attribute__((aligned(16)));
char B_800EEED8[16] __attribute__((aligned(16)));
char gN64MemoryManager[64];
void *mpLinkedMemoryManager_AllocPtr(void *m, u32 n, s32 e) { (void)m; (void)e; return calloc(1, n + 16); }
void mpLinkedMemoryManager_FreePtr(void *m, void *p) { (void)m; free(p); }
void osCreateMesgQueue(void *q, void *m, s32 n) { (void)q; (void)m; (void)n; }
s32 osSendMesg(void *q, void *m, s32 f) { (void)q; (void)m; (void)f; return 0; }
static void Swap(u8 *a, u8 *b, u32 n) { while (n--) { u8 t = *a; *a++ = *b; *b++ = t; } }
void func_800A3C80(void *base, u32 count, u32 size, s32 (*compare)(const void *, const void *)) {
    u8 *b = base, *end, *p, *q;
    u32 gap, gs;
    if (count < 2) return;
    if (count == 2) { if (compare(b, b + size) > 0) Swap(b, b + size, size); return; }
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
void port_yield(void) {}

int main(int argc, char **argv) {
    FILE *f;
    long n, off;
    u8 *in, *out;
    u32 raw;
    s32 got;
    if (argc < 4) { fprintf(stderr, "usage: mpf_unpack in.bin offset out.bin\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    in = malloc(n + 16);
    if (fread(in, 1, n, f) != (size_t)n) return 1;
    fclose(f);
    off = strtol(argv[2], NULL, 0);
    raw = READ_BE32(in + off + 8);
    out = malloc(raw + 64);
    got = mpDataDecompress(out, in + off);
    f = fopen(argv[3], "wb");
    fwrite(out, 1, got, f);
    fclose(f);
    return 0;
}
