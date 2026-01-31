// src/pc/ld_addrs_stub.c
#include "common.h"
#include <time.h>


// RSP microcode - dummy addresses
u8 gspF3DZEX2_NoN_PosLight_fifoTextStart[1];
u8 gspF3DZEX2_NoN_PosLight_fifoDataStart[1];

BackgroundHeader gBackgroundImage;

// Audio RSP
u8 n_aspMainTextStart[1];
u8 n_aspMainDataStart[1];


// from undefined_sysms.txt
// gBackgroundImage = 0x80200000;
// gMapShapeData = 0x80210000;

// N64 memory size - 8MB with expansion pak
u32 osMemSize = 0x800000;
s32 osTvType = 1;  // NTSC
s32 osResetType = 0;
s32 osAppNMIBuffer[64];
void* osRomBase = NULL;


void __osSetCompare(u32 value) {
}

u32 osGetCount(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32)(ts.tv_sec * 46875000 + ts.tv_nsec / 21);
}

void __osRestoreInt(u32 mask) {
    // No-op on PC
}

u32 __osDisableInt(void) {
    return 0;
}


// Cache operations - no-op on PC
void osWritebackDCacheAll(void) {
}

void osWritebackDCache(void* addr, s32 size) {
}

void osInvalICache(void* addr, s32 size) {
}


// Cache
void osInvalDCache(void* addr, s32 size) {
}

// Interrupt mask
u32 osSetIntMask(u32 mask) {
    return 0;
}

void __osEnqueueThread(OSThread** queue, OSThread* thread) {
}

OSThread* __osPopThread(OSThread** queue) {
    return NULL;
}

void __osDispatchThread(void) {
}

void __osEnqueueAndYield(OSThread** queue) {
}


// Thread cleanup
void __osCleanupThread(void) {
}

// Coprocessor register access
u32 __osGetSR(void) {
    return 0;
}

void __osSetSR(u32 value) {
}

u32 __osSetFpcCsr(u32 value) {
    return 0;
}

void __osSetWatchLo(u32 value) {
}

u32 __osGetCause(void) {
    return 0;
}

// Exception handling
void __osExceptionPreamble(void) {
}

// TLB
void osUnmapTLBAll(void) {
}

void osMapTLBRdb(void) {
}

void* __osProbeTLB(void* addr) {
    return addr;
}


// TLB
void osMapTLB(s32 index, OSPageMask pm, void* vaddr, u32 evenpaddr, u32 oddpaddr, s32 asid) {
}

void osUnmapTLB(s32 index) {
}








// Decompression may need to fix up later.
void decode_yay0(void* src, void* dst) {
    u8* srcPtr = (u8*)src;
    u8* dstPtr = (u8*)dst;

    u32 decompSize = (srcPtr[4] << 24) | (srcPtr[5] << 16) | (srcPtr[6] << 8) | srcPtr[7];
    u32 linkOffset = (srcPtr[8] << 24) | (srcPtr[9] << 16) | (srcPtr[10] << 8) | srcPtr[11];
    u32 chunkOffset = (srcPtr[12] << 24) | (srcPtr[13] << 16) | (srcPtr[14] << 8) | srcPtr[15];

    u8* linkPtr = srcPtr + linkOffset;
    u8* chunkPtr = srcPtr + chunkOffset;
    u8* ctrlPtr = srcPtr + 16;

    u8* dstEnd = dstPtr + decompSize;

    u32 ctrl = 0;
    s32 ctrlBits = 0;

    while (dstPtr < dstEnd) {
        if (ctrlBits == 0) {
            ctrl = (ctrlPtr[0] << 24) | (ctrlPtr[1] << 16) | (ctrlPtr[2] << 8) | ctrlPtr[3];
            ctrlPtr += 4;
            ctrlBits = 32;
        }

        if (ctrl & 0x80000000) {
            // Direct copy
            *dstPtr++ = *chunkPtr++;
        } else {
            // Backreference
            u16 link = (linkPtr[0] << 8) | linkPtr[1];
            linkPtr += 2;

            s32 dist = link & 0xFFF;
            s32 len = link >> 12;

            if (len == 0) {
                len = *chunkPtr++ + 0x12;
            } else {
                len += 2;
            }

            u8* backPtr = dstPtr - dist - 1;
            while (len-- > 0) {
                *dstPtr++ = *backPtr++;
            }
        }

        ctrl <<= 1;
        ctrlBits--;
    }
}
