// src/pc/ld_addrs_stub.c
#include "common.h"
#include "effects.h"
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include "../assets_le/vrom_table.h"

// RSP microcode - dummy addresses
u8 gspF3DZEX2_NoN_PosLight_fifoTextStart[1];
u8 gspF3DZEX2_NoN_PosLight_fifoDataStart[1];


//////////////  I got lazy fix this.
u32 i_spy_VRAM = 0;
u32 i_spy_ROM_START = 0;
u32 i_spy_ROM_END = 0;
u32 pulse_stone_VRAM = 0;
u32 pulse_stone_ROM_START = 0;
u32 pulse_stone_ROM_END = 0;
u32 speech_bubble_VRAM = 0;
u32 speech_bubble_ROM_START = 0;
u32 speech_bubble_ROM_END = 0;
u32 inspect_icon_VRAM = 0;
u32 inspect_icon_ROM_START = 0;
u32 inspect_icon_ROM_END = 0;
u32 world_action_CLASS_VRAM = 0;
u32 create_audio_system_obfuscated_VRAM = 0;
u32 create_audio_system_obfuscated_ROM_START = 0;
u32 create_audio_system_obfuscated_ROM_END = 0;
u32 load_engine_data_obfuscated_VRAM = 0;
u32 load_engine_data_obfuscated_ROM_START = 0;
u32 load_engine_data_obfuscated_ROM_END = 0;
u32 general_heap_create_obfuscated_VRAM = 0;
u32 general_heap_create_obfuscated_ROM_START = 0;
u32 general_heap_create_obfuscated_ROM_END = 0;
u32 battle_heap_create_obfuscated_VRAM = 0;
u32 battle_heap_create_obfuscated_ROM_START = 0;
u32 battle_heap_create_obfuscated_ROM_END = 0;
u32 battle_code_VRAM = 0;
u32 battle_code_ROM_START = 0;
u32 battle_code_ROM_END = 0;
u32 ui_images_filemenu_pause_VRAM = 0;
u32 ui_images_filemenu_pause_ROM_START = 0;
u32 ui_images_filemenu_pause_ROM_END = 0;
u32 btl_states_menus_VRAM = 0;
u32 btl_states_menus_ROM_START = 0;
u32 btl_states_menus_ROM_END = 0;
u32 starpoint_VRAM = 0;
u32 starpoint_ROM_START = 0;
u32 starpoint_ROM_END = 0;
u32 level_up_VRAM = 0;
u32 level_up_ROM_START = 0;
u32 level_up_ROM_END = 0;
u32 dgb_01_smash_bridges_VRAM = 0;
u32 dgb_01_smash_bridges_ROM_START = 0;
u32 dgb_01_smash_bridges_ROM_END = 0;

// Asset ROM addresses
u32 icon_ROM_START = 0;
u32 msg_ROM_START = 0;
u32 audio_ROM_START = 0;
u32 sprite_shading_profiles_ROM_START = 0;
u32 sprite_shading_profiles_data_ROM_START = 0;
u32 entity_model_Signpost_ROM_START = 0;
u32 entity_model_Signpost_ROM_END = 0;

// Battle heap - needs actual memory
HeapNode heap_battleHead;

/////////////////////////////////////////////////

// imgfx animation headers - ROM offsets, stub to NULL
u8* shock_header = NULL;
u8* shiver_header = NULL;
u8* vertical_pipe_curl_header = NULL;
u8* horizontal_pipe_curl_header = NULL;
u8* startle_header = NULL;
u8* flutter_down_header = NULL;
u8* unfurl_header = NULL;
u8* get_in_bed_header = NULL;
u8* spirit_capture_header = NULL;
u8* unused_1_header = NULL;
u8* unused_2_header = NULL;
u8* unused_3_header = NULL;
u8* tutankoopa_gather_header = NULL;
u8* tutankoopa_swirl_2_header = NULL;
u8* tutankoopa_swirl_1_header = NULL;
u8* shuffle_cards_header = NULL;
u8* flip_card_1_header = NULL;
u8* flip_card_2_header = NULL;
u8* flip_card_3_header = NULL;
u8* cymbal_crush_header = NULL;

u32 imgfx_data_ROM_START = 0;
u32 charset_ROM_START = 0;


BackgroundHeader gBackgroundImage;
u8 gBackgroundImage_padding[0x10000];
// Audio RSP
u8 n_aspMainTextStart[1];
u8 n_aspMainDataStart[1];

// Alias from undefined_syms.txt: fx_sun_undeclared = fx_sun
void fx_sun_undeclared(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f) {
    fx_sun(a, b, c, d, e, f);
}
// from undefined_sysms.txt
// gBackgroundImage = 0x80200000;
// gMapShapeData = 0x80210000;

// N64 memory size - 8MB with expansion pak
u32 osMemSize = 0x800000;
s32 osTvType = 1;  // NTSC
s32 osResetType = 0;
s32 osAppNMIBuffer[64];
void* osRomBase = NULL;



void is_debug_init(void) {
    // No-op on Linux
}

void osSyncPrintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void rmonPrintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void is_debug_panic(const char* message, char* file, s32 line) {
    fprintf(stderr, "PANIC - File:%s Line:%d  %s \n", file, line, message);
    abort();
}


s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags) {
    if (MQ_IS_EMPTY(mq)) {
        if (flags == OS_MESG_NOBLOCK) {
            return -1;
        }
        // Blocking mode but empty - just return -1 for now
        // Real implementation would need proper sync
        return -1;
    }

    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    return 0;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags) {
    if (MQ_IS_FULL(mq)) {
        if (flags == OS_MESG_NOBLOCK) {
            return -1;
        }
        return -1;
    }

    mq->msg[(mq->first + mq->validCount) % mq->msgCount] = msg;
    mq->validCount++;
    return 0;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count) {
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msg;
}


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



void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size) {
    const VromEntry* entry;
    u32 offset;
    char path[256];
    FILE* f;

    if (rom_addr >= 0xB0000000) {
        memset(buf_ptr, 0, size);
        return;
    }

    entry = vrom_find(rom_addr);

    if (!entry) {
        // get_asset_offset returns heap buffer pointers as "ROM addresses".
        // These will be above the program's load address (typically 0x08000000+
        // on 32-bit Linux). Real ROM addresses are below 0x02000000.
        if (rom_addr >= 0x08000000 && rom_addr < 0x80000000) {
            memcpy(buf_ptr, (void*)(uintptr_t)rom_addr, size);
            return;
        }
        printf("nuPiReadRom: MISSING asset at ROM 0x%08X size 0x%X\n", rom_addr, size);
        memset(buf_ptr, 0, size);
        return;
    }

    offset = rom_addr - entry->vrom_start;
    snprintf(path, sizeof(path), "assets_le/%s", entry->filename);

    f = fopen(path, "rb");
    if (!f) {
        printf("nuPiReadRom: can't open %s\n", path);
        memset(buf_ptr, 0, size);
        return;
    }

    fseek(f, offset, SEEK_SET);
    fread(buf_ptr, 1, size, f);
    fclose(f);
}


// Decompression may need to fix up later or no op later..
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
