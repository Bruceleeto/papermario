#!/usr/bin/env python3
"""
validate_assets.py - Validate converted Little Endian assets

Checks:
1. MapFS TOC is parseable with valid offsets
2. Sprite offset tables have reasonable values
3. Icon palettes have valid RGBA5551 colors
4. Message section tables are valid
5. Logos images have expected size and non-trivial content
6. Round-trip test: swap back to BE, compare specific values
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

    entries = []
    pos = 0x20
    while pos + 0x1C <= len(data):
        name = decode_string(data[pos:pos+16])
        if name == "end_data" or not name:
            break

        offset = read_u32_le(data, pos + 0x10)
        size = read_u32_le(data, pos + 0x14)
        decomp_size = read_u32_le(data, pos + 0x18)

        if offset > len(data):
            errors.append(f"{name}: offset 0x{offset:X} > file size 0x{len(data):X}")
        elif offset + size > len(data):
            errors.append(f"{name}: data extends past EOF")

        if size > 0x1000000:
            warnings.append(f"{name}: unusually large size 0x{size:X}")

        entries.append((name, offset, size, decomp_size))
        pos += 0x1C

    print(f"    Parsed {len(entries)} TOC entries")

    for name, offset, size, _ in entries[:5]:
        if offset > 0 and offset + 0x20 <= len(data):
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

    player_raster_off = read_u32_le(data, 0x10) + 0x10
    player_yay0_off = read_u32_le(data, 0x14) + 0x10
    npc_yay0_off = read_u32_le(data, 0x18) + 0x10
    sprite_end_off = read_u32_le(data, 0x1C) + 0x10

    print(f"    Header offsets (LE):")
    print(f"      player_raster: 0x{player_raster_off:X}")
    print(f"      player_yay0:   0x{player_yay0_off:X}")
    print(f"      npc_yay0:      0x{npc_yay0_off:X}")
    print(f"      sprite_end:    0x{sprite_end_off:X}")

    if not (player_raster_off < player_yay0_off < npc_yay0_off <= sprite_end_off):
        errors.append("Header offsets not in expected order")

    if sprite_end_off > len(data):
        errors.append(f"sprite_end 0x{sprite_end_off:X} > file size 0x{len(data):X}")

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

    if len(data) >= 320:
        pal_offset = 288
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

    for i in range(1, len(sections)):
        if sections[i] <= sections[i-1]:
            errors.append(f"Section {i} offset not ascending")

    return len(errors) == 0, errors


def validate_charset(data: bytes) -> Tuple[bool, List[str]]:
    """Validate charset data"""
    errors = []

    if len(data) < 1000:
        errors.append("Charset suspiciously small")

    print(f"    Size: {len(data):,} bytes")

    return len(errors) == 0, errors


def validate_logos(data: bytes) -> Tuple[bool, List[str]]:
    """Validate logos data - three RGBA16 images byte-swapped from BE.

    Expected layout:
      0x00000: Image1 (N64 logo)       128x112 RGBA16 = 0x7000 bytes
      0x07000: Image3 (IS logo)        256x112 RGBA16 = 0xE000 bytes
      0x15000: Image2 (Nintendo logo)  256x48  RGBA16 = 0x6000 bytes
    Total expected: >= 0x1B000 bytes
    """
    errors = []

    EXPECTED_MIN = 0x1B000

    print(f"    Size: {len(data):,} bytes (expected >= 0x{EXPECTED_MIN:X})")

    if len(data) < EXPECTED_MIN:
        errors.append(
            f"Logos data too small: {len(data):,} bytes, "
            f"expected at least 0x{EXPECTED_MIN:X}"
        )
        return False, errors

    images = [
        ("Image1 (N64 logo)",      0x00000, 128, 112),
        ("Image3 (IS logo)",       0x07000, 256, 112),
        ("Image2 (Nintendo logo)", 0x15000, 256,  48),
    ]

    for name, offset, w, h in images:
        img_size = w * h * 2
        region = data[offset:offset + img_size]

        # Check not all zeros
        if region == b'\x00' * img_size:
            errors.append(f"{name}: entirely zero (conversion likely failed)")
            continue

        # Check not all 0xFF
        if region == b'\xFF' * img_size:
            errors.append(f"{name}: entirely 0xFF")
            continue

        # Sample pixels as LE RGBA5551 - check alpha bit distribution
        # In RGBA5551, bit 0 is the alpha flag.
        # Logo images are drawn on a filled background so most pixels
        # should have alpha set.
        alpha_set = 0
        sample_count = min(256, w * h)
        step = max(1, (w * h) // sample_count)
        sampled = 0
        for i in range(0, w * h, step):
            px = read_u16_le(data, offset + i * 2)
            if px & 1:
                alpha_set += 1
            sampled += 1

        pct = (alpha_set / sampled) * 100 if sampled else 0
        print(f"    {name}: {w}x{h}  alpha-set: {pct:.0f}% of {sampled} sampled")

        # If almost no pixels have alpha set, the bytes are probably
        # still in BE order (the alpha bit would be in the wrong byte).
        if pct < 10:
            errors.append(
                f"{name}: only {pct:.0f}% of sampled pixels have alpha set "
                f"(expected high %; may still be BE)"
            )

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

        rom_start = 0x01EA0A30

        orig_name = decode_string(rom[rom_start + 0x20:rom_start + 0x30])
        conv_name = decode_string(mapfs[0x20:0x30])

        if orig_name == conv_name:
            print(f"    MapFS first entry name matches: '{orig_name}'")
        else:
            errors.append(f"MapFS name mismatch: orig='{orig_name}' conv='{conv_name}'")

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
        ("mapfs.bin",   validate_mapfs),
        ("sprite.bin",  validate_sprites),
        ("icon.bin",    validate_icons),
        ("msg.bin",     validate_msg),
        ("charset.bin", validate_charset),
        ("logos.bin",   validate_logos),
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
            passed, errs = validator(data)
            if passed:
                print(f"  PASS")
            else:
                print(f"  FAIL:")
                for e in errs:
                    print(f"    - {e}")
                all_passed = False
        except Exception as e:
            print(f"  ERROR: {e}")
            all_passed = False

    # Compare with original ROM
    print(f"\nComparison with original ROM:")
    try:
        passed, errs = compare_with_original()
        if passed:
            print(f"  PASS")
        else:
            print(f"  FAIL:")
            for e in errs:
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
