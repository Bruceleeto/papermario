#!/usr/bin/env python3
"""
validate_assets.py - Validate converted Little Endian assets

Checks:
1. MapFS TOC is parseable with valid offsets
2. Sprite offset tables have reasonable values
3. Icon palettes have valid RGBA5551 colors
4. Message section tables are valid
5. Round-trip test: swap back to BE, compare specific values
"""

import struct
from pathlib import Path
from typing import List, Tuple

ASSETS_DIR = Path("assets_le")
ROM_PATH = Path("ver/us/build/papermario.z64")

# =============================================================================
# Utility
# =============================================================================

def read_u32_le(data: bytes, offset: int) -> int:
    return struct.unpack("<I", data[offset:offset+4])[0]

def read_u16_le(data: bytes, offset: int) -> int:
    return struct.unpack("<H", data[offset:offset+2])[0]

def read_i32_le(data: bytes, offset: int) -> int:
    return struct.unpack("<i", data[offset:offset+4])[0]

def read_u32_be(data: bytes, offset: int) -> int:
    return struct.unpack(">I", data[offset:offset+4])[0]

def read_u16_be(data: bytes, offset: int) -> int:
    return struct.unpack(">H", data[offset:offset+2])[0]

def decode_string(data: bytes) -> str:
    end = data.find(0)
    if end == -1:
        end = len(data)
    return data[:end].decode('ascii', errors='ignore')

# =============================================================================
# Validators
# =============================================================================

def validate_mapfs(data: bytes) -> Tuple[bool, List[str]]:
    """Validate MapFS TOC structure"""
    errors = []
    warnings = []

    if len(data) < 0x40:
        return False, ["MapFS too small"]

    # Parse TOC entries
    entries = []
    pos = 0x20
    while pos + 0x1C <= len(data):
        name = decode_string(data[pos:pos+16])
        if name == "end_data" or not name:
            break

        offset = read_u32_le(data, pos + 0x10)
        size = read_u32_le(data, pos + 0x14)
        decomp_size = read_u32_le(data, pos + 0x18)

        # Validate
        if offset > len(data):
            errors.append(f"{name}: offset 0x{offset:X} > file size 0x{len(data):X}")
        elif offset + size > len(data):
            errors.append(f"{name}: data extends past EOF")

        if size > 0x1000000:  # >16MB is suspicious
            warnings.append(f"{name}: unusually large size 0x{size:X}")

        entries.append((name, offset, size, decomp_size))
        pos += 0x1C

    print(f"    Parsed {len(entries)} TOC entries")

    # Spot check: first few entries should have valid data
    for name, offset, size, _ in entries[:5]:
        if offset > 0 and offset + 0x20 <= len(data):
            # Check if data region looks valid (not all zeros or all 0xFF)
            sample = data[0x20 + offset:0x20 + offset + 0x20]
            if sample == b'\x00' * 0x20:
                warnings.append(f"{name}: data region is all zeros")
            elif sample == b'\xFF' * 0x20:
                warnings.append(f"{name}: data region is all 0xFF")

    for w in warnings:
        print(f"    WARNING: {w}")

    return len(errors) == 0, errors

def validate_sprites(data: bytes) -> Tuple[bool, List[str]]:
    """Validate sprite offset tables"""
    errors = []

    if len(data) < 0x20:
        return False, ["Sprite data too small"]

    # Read header (now LE)
    player_raster_off = read_u32_le(data, 0x10) + 0x10
    player_yay0_off = read_u32_le(data, 0x14) + 0x10
    npc_yay0_off = read_u32_le(data, 0x18) + 0x10
    sprite_end_off = read_u32_le(data, 0x1C) + 0x10

    print(f"    Header offsets (LE):")
    print(f"      player_raster: 0x{player_raster_off:X}")
    print(f"      player_yay0:   0x{player_yay0_off:X}")
    print(f"      npc_yay0:      0x{npc_yay0_off:X}")
    print(f"      sprite_end:    0x{sprite_end_off:X}")

    # Validate offsets are in order
    if not (player_raster_off < player_yay0_off < npc_yay0_off <= sprite_end_off):
        errors.append("Header offsets not in expected order")

    if sprite_end_off > len(data):
        errors.append(f"sprite_end 0x{sprite_end_off:X} > file size 0x{len(data):X}")

    # Check player raster table header
    if player_raster_off + 12 <= len(data):
        idx_off = read_u32_le(data, player_raster_off)
        info_off = read_u32_le(data, player_raster_off + 4)
        ci4_off = read_u32_le(data, player_raster_off + 8)

        print(f"    Player raster table:")
        print(f"      idx_ranges: 0x{idx_off:X}")
        print(f"      raster_info: 0x{info_off:X}")
        print(f"      ci4_data: 0x{ci4_off:X}")

        if not (idx_off < info_off < ci4_off):
            errors.append("Player raster sub-offsets not in order")

    return len(errors) == 0, errors

def validate_icons(data: bytes) -> Tuple[bool, List[str]]:
    """Validate icon data"""
    errors = []

    # Sample some 16-bit values that should be valid RGBA5551 colors
    # In a palette, values should generally not be 0x0000 for all entries

    # Check first potential palette region (after first CI4 raster)
    # Assuming 24x24 CI4 icon = 288 bytes raster, then 32 byte palette
    if len(data) >= 320:
        pal_offset = 288  # After first 24x24 CI4 raster
        nonzero = 0
        for i in range(16):
            color = read_u16_le(data, pal_offset + i * 2)
            if color != 0:
                nonzero += 1

        print(f"    First palette: {nonzero}/16 non-zero colors")

        if nonzero == 0:
            errors.append("First palette is all zeros (likely not swapped correctly)")

    return len(errors) == 0, errors

def validate_msg(data: bytes) -> Tuple[bool, List[str]]:
    """Validate message section table"""
    errors = []

    if len(data) < 8:
        return False, ["Message data too small"]

    # Read section offsets (should be reasonable values after LE conversion)
    sections = []
    pos = 0
    while pos + 4 <= len(data):
        off = read_u32_le(data, pos)
        if off == 0:
            break
        if off >= len(data):
            errors.append(f"Section offset 0x{off:X} >= file size")
            break
        sections.append(off)
        pos += 4

    print(f"    Found {len(sections)} message sections")

    # Validate section offsets are ascending
    for i in range(1, len(sections)):
        if sections[i] <= sections[i-1]:
            errors.append(f"Section {i} offset not ascending")

    return len(errors) == 0, errors

def validate_charset(data: bytes) -> Tuple[bool, List[str]]:
    """Validate charset data"""
    errors = []

    # Charset is CI4 rasters + palettes
    # Just check size is reasonable
    if len(data) < 1000:
        errors.append("Charset suspiciously small")

    print(f"    Size: {len(data):,} bytes")

    return len(errors) == 0, errors

def compare_with_original() -> Tuple[bool, List[str]]:
    """Compare specific values with original ROM to verify conversion"""
    errors = []

    if not ROM_PATH.exists():
        return True, []  # Skip if no ROM

    rom = ROM_PATH.read_bytes()

    # MapFS comparison
    mapfs_path = ASSETS_DIR / "mapfs.bin"
    if mapfs_path.exists():
        mapfs = mapfs_path.read_bytes()

        # Original MapFS ROM offset (from vrom_table)
        rom_start = 0x01EA0A30

        # Check first TOC entry name matches
        orig_name = decode_string(rom[rom_start + 0x20:rom_start + 0x30])
        conv_name = decode_string(mapfs[0x20:0x30])

        if orig_name == conv_name:
            print(f"    MapFS first entry name matches: '{orig_name}'")
        else:
            errors.append(f"MapFS name mismatch: orig='{orig_name}' conv='{conv_name}'")

        # Check offset value is byte-swapped correctly
        orig_offset = read_u32_be(rom, rom_start + 0x30)
        conv_offset = read_u32_le(mapfs, 0x30)

        if orig_offset == conv_offset:
            print(f"    MapFS first offset matches: 0x{orig_offset:X}")
        else:
            errors.append(f"Offset mismatch: BE=0x{orig_offset:X} LE=0x{conv_offset:X}")

    return len(errors) == 0, errors

# =============================================================================
# Main
# =============================================================================

def main():
    print("Asset Validation")
    print("=" * 50)

    all_passed = True

    validators = [
        ("mapfs.bin", validate_mapfs),
        ("sprite.bin", validate_sprites),
        ("icon.bin", validate_icons),
        ("msg.bin", validate_msg),
        ("charset.bin", validate_charset),
    ]

    for filename, validator in validators:
        path = ASSETS_DIR / filename
        print(f"\n{filename}:")

        if not path.exists():
            print(f"  SKIP: File not found")
            continue

        data = path.read_bytes()
        print(f"  Size: {len(data):,} bytes")

        try:
            passed, errors = validator(data)
            if passed:
                print(f"  PASS")
            else:
                print(f"  FAIL:")
                for e in errors:
                    print(f"    - {e}")
                all_passed = False
        except Exception as e:
            print(f"  ERROR: {e}")
            all_passed = False

    # Compare with original ROM
    print(f"\nComparison with original ROM:")
    try:
        passed, errors = compare_with_original()
        if passed:
            print(f"  PASS")
        else:
            print(f"  FAIL:")
            for e in errors:
                print(f"    - {e}")
            all_passed = False
    except Exception as e:
        print(f"  ERROR: {e}")

    print("\n" + "=" * 50)
    if all_passed:
        print("All validations PASSED")
        return 0
    else:
        print("Some validations FAILED")
        return 1

if __name__ == "__main__":
    exit(main())
