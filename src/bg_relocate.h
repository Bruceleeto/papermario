/**
 * bg_relocate.h - Runtime pointer fixup for PC background files
 * 
 * BG files converted by bg_convert.py have a relocation table appended:
 *   [original data][reloc offsets...][reloc count]
 */

#ifndef BG_RELOCATE_H
#define BG_RELOCATE_H

#include "types.h"

/**
 * Relocate pointers in a background file after loading.
 * 
 * @param bgData    Pointer to loaded BG data
 * @param fileSize  Total size of the file including reloc table
 * @return          Size of BG data without reloc table, or 0 on error
 */
static inline u32 bg_relocate(void* bgData, u32 fileSize) {
    u8* data;
    u32 baseAddr;
    u32 relocCount;
    u32 tableSize;
    u32 dataSize;
    u32* relocTable;
    u32 i;
    
    data = (u8*)bgData;
    baseAddr = (u32)bgData;
    
    if (fileSize < 4) {
        return 0;
    }
    
    /* Read reloc count from last 4 bytes */
    relocCount = *(u32*)(data + fileSize - 4);
    
    /* Sanity check */
    tableSize = (relocCount + 1) * 4;
    if (tableSize > fileSize) {
        return 0;
    }
    
    /* Calculate where reloc table starts */
    dataSize = fileSize - tableSize;
    relocTable = (u32*)(data + dataSize);
    
    /* Apply relocations */
    for (i = 0; i < relocCount; i++) {
        u32 offset = relocTable[i];
        if (offset + 4 <= dataSize) {
            u32* ptr = (u32*)(data + offset);
            *ptr += baseAddr;
        }
    }
    
    return dataSize;
}

#endif /* BG_RELOCATE_H */
