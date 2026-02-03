#!/usr/bin/env python3
"""
bg_convert.py - Convert Paper Mario BG files to LE with relocation table

BackgroundHeader (0x10 bytes):
  0x00: IMG_PTR raster   (0x80200000-based pointer)
  0x04: PAL_PTR palette  (0x80200000-based pointer)
  0x08: u16 startX
  0x0A: u16 startY
  0x0C: u16 width
  0x0E: u16 height

Some BGs have multiple palette variants (pal_count headers back-to-back).
"""

import struct
from typing import List

BASE_ADDR = 0x80200000

def read_u32_be(data: bytes, off: int) -> int:
    return struct.unpack(">I", data[off:off+4])[0]

def write_u32_le(data: bytearray, off: int, val: int):
    struct.pack_into("<I", data, off, val & 0xFFFFFFFF)

def swap16(data: bytearray, off: int):
    if off + 1 < len(data):
        data[off], data[off+1] = data[off+1], data[off]

def swap16_range(data: bytearray, start: int, count: int):
    for i in range(count):
        swap16(data, start + i * 2)

def is_valid_ptr(val: int, file_size: int) -> bool:
    if val < BASE_ADDR:
        return False
    offset = val - BASE_ADDR
    return offset < file_size


def convert_bg_le(data: bytes, pal_count: int = 1, debug: bool = False) -> bytes:
    """Convert a BE background file to LE with relocation table.
    
    Args:
        data: Big-endian BG file bytes
        pal_count: Number of palette variants (headers)
        debug: Print debug info
        
    Returns:
        Little-endian BG file with reloc table appended
    """
    if len(data) < 0x10:
        return data
    
    out = bytearray(data)
    relocs: List[int] = []
    
    for i in range(pal_count):
        header_off = i * 0x10
        if header_off + 0x10 > len(data):
            break
        
        # Read pointers before modifying
        raster_ptr = read_u32_be(data, header_off + 0x00)
        palette_ptr = read_u32_be(data, header_off + 0x04)
        
        # Convert raster pointer to offset
        if is_valid_ptr(raster_ptr, len(data)):
            raster_off = raster_ptr - BASE_ADDR
            relocs.append(header_off + 0x00)
            write_u32_le(out, header_off + 0x00, raster_off)
        else:
            write_u32_le(out, header_off + 0x00, raster_ptr)
        
        # Convert palette pointer to offset
        if is_valid_ptr(palette_ptr, len(data)):
            palette_off = palette_ptr - BASE_ADDR
            relocs.append(header_off + 0x04)
            write_u32_le(out, header_off + 0x04, palette_off)
        else:
            write_u32_le(out, header_off + 0x04, palette_ptr)
        
        # Swap u16 fields: startX, startY, width, height
        swap16(out, header_off + 0x08)
        swap16(out, header_off + 0x0A)
        swap16(out, header_off + 0x0C)
        swap16(out, header_off + 0x0E)
        
        # Swap palette data (256 colors, RGBA16 = 2 bytes each)
        if is_valid_ptr(palette_ptr, len(data)):
            pal_off = palette_ptr - BASE_ADDR
            if pal_off + 512 <= len(data):
                swap16_range(out, pal_off, 256)
    
    if debug:
        print(f"    BG: {len(data)} bytes, {pal_count} variants, {len(relocs)} relocations")
        for i, off in enumerate(relocs):
            orig_val = read_u32_be(data, off)
            print(f"      reloc[{i}]: offset 0x{off:04X}, orig 0x{orig_val:08X} -> 0x{orig_val - BASE_ADDR:04X}")
    
    # Append relocation table
    while len(out) % 4 != 0:
        out.append(0)
    
    for off in relocs:
        out.extend(struct.pack("<I", off))
    out.extend(struct.pack("<I", len(relocs)))
    
    return bytes(out)


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.bin> [pal_count]")
        sys.exit(1)
    
    pal_count = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    
    with open(sys.argv[1], 'rb') as f:
        data = f.read()
    
    result = convert_bg_le(data, pal_count, debug=True)
    
    with open(sys.argv[2], 'wb') as f:
        f.write(result)
    
    print(f"Converted: {len(data)} -> {len(result)} bytes")
