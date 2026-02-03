/**
 * shape_relocate.h - Runtime pointer fixup for PC shape files
 *
 * Shape files converted by shape_convert.py have a relocation table appended:
 *   [original data][reloc offsets...][reloc count]
 *
 * This function reads the table and adds the load address to each pointer field.
 */

#ifndef SHAPE_RELOCATE_H
#define SHAPE_RELOCATE_H

#include "types.h"

/**
 * Relocate pointers in a shape file after loading.
 *
 * @param shapeData  Pointer to loaded shape data
 * @param fileSize   Total size of the file including reloc table
 * @return           Size of shape data without reloc table, or 0 on error
 */
static inline u32 shape_relocate(void* shapeData, u32 fileSize) {
    u8* data;
    u32 baseAddr;
    u32 relocCount;
    u32 tableSize;
    u32 dataSize;
    u32* relocTable;
    u32 i;

    data = (u8*)shapeData;
    baseAddr = (u32)shapeData;

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

#endif // SHAPE_RELOCATE_H
