#!/usr/bin/env python3
"""
convert_assets_le.py - Convert Paper Mario N64 assets to Little Endian for PC

Usage:
    ./configure us
    ninja
    python3 convert_assets_le.py

Outputs:
    assets_le/*.bin - Little-endian asset files (uncompressed)
    assets_le/vrom_table.h - C header mapping ROM addresses to files
"""

import struct
import re
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Any
import sys
from shape_convert import convert_shape_le
from bg_convert import convert_bg_le
try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False

try:
    import crunch64
    HAS_CRUNCH64 = True
except ImportError:
    HAS_CRUNCH64 = False


# =============================================================================
# Configuration
# =============================================================================

BUILD_DIR = Path("ver/us/build")
ROM_PATH = BUILD_DIR / "papermario.z64"
MAP_PATH = BUILD_DIR / "papermario.map"
SPLAT_EXT_DIR = Path("tools/splat_ext")
OUT_DIR = Path("assets_le")
SPLIT_MAPFS = True

# =============================================================================
# Utility Functions
# =============================================================================

def read_u32_be(data: bytes, offset: int) -> int:
    return struct.unpack(">I", data[offset:offset+4])[0]

def read_u16_be(data: bytes, offset: int) -> int:
    return struct.unpack(">H", data[offset:offset+2])[0]

def read_i32_be(data: bytes, offset: int) -> int:
    return struct.unpack(">i", data[offset:offset+4])[0]

def read_i16_be(data: bytes, offset: int) -> int:
    return struct.unpack(">h", data[offset:offset+2])[0]

def write_u32_le(data: bytearray, offset: int, value: int):
    struct.pack_into("<I", data, offset, value)

def write_u16_le(data: bytearray, offset: int, value: int):
    struct.pack_into("<H", data, offset, value)

def swap16(data: bytearray, offset: int):
    if offset + 1 < len(data):
        data[offset], data[offset+1] = data[offset+1], data[offset]

def swap32(data: bytearray, offset: int):
    if offset + 3 < len(data):
        data[offset], data[offset+1], data[offset+2], data[offset+3] = \
            data[offset+3], data[offset+2], data[offset+1], data[offset]

def swap16_range(data: bytearray, start: int, count: int):
    for i in range(count):
        swap16(data, start + i * 2)

def swap32_range(data: bytearray, start: int, count: int):
    for i in range(count):
        swap32(data, start + i * 4)

def read_offset_list_be(data: bytes, offset: int) -> List[int]:
    result = []
    while offset + 4 <= len(data):
        val = read_i32_be(data, offset)
        if val == -1:
            break
        result.append(val)
        offset += 4
    return result

def swap_offset_list(data: bytearray, offset: int) -> int:
    count = 0
    while offset + 4 <= len(data):
        val = read_i32_be(data, offset)
        swap32(data, offset)
        offset += 4
        if val == -1:
            break
        count += 1
    return count

def decompress_yay0(data: bytes) -> bytes:
    if not HAS_CRUNCH64:
        raise RuntimeError("crunch64 required for YAY0 decompression")
    if len(data) < 4 or data[0:4] != b'Yay0':
        raise ValueError("Not YAY0 data")
    return crunch64.yay0.decompress(data)

def is_yay0(data: bytes) -> bool:
    return len(data) >= 4 and data[0:4] == b'Yay0'

def load_yaml(path: Path) -> Any:
    if not HAS_YAML:
        raise RuntimeError("PyYAML required. Install: pip install pyyaml")
    with open(path, 'r') as f:
        return yaml.safe_load(f)

def decode_null_terminated_ascii(data: bytes) -> str:
    end = data.find(0)
    if end == -1:
        end = len(data)
    return data[:end].decode('ascii', errors='ignore')

# =============================================================================
# Map File Parser
# =============================================================================

def parse_map_file(map_path: Path) -> Dict[str, int]:
    symbols = {}
    with open(map_path, 'r') as f:
        for line in f:
            line = line.strip()
            match = re.match(r'^\s*(0x[0-9a-fA-F]+)\s+(\w+)', line)
            if match:
                symbols[match.group(2)] = int(match.group(1), 16)
                continue
            match = re.match(r'^\s*(\w+)\s*=\s*(0x[0-9a-fA-F]+)', line)
            if match:
                symbols[match.group(1)] = int(match.group(2), 16)
    return symbols

def get_segment_bounds(symbols: Dict[str, int], names: List[str]) -> Optional[Tuple[int, int, str]]:
    for name in names:
        for prefix in ['', '_']:
            start_sym = f"{prefix}{name}_ROM_START"
            end_sym = f"{prefix}{name}_ROM_END"
            if start_sym in symbols and end_sym in symbols:
                return (symbols[start_sym], symbols[end_sym], name)
    return None

# =============================================================================
# MapFS Configuration Loader
# =============================================================================

class MapFSConfig:
    def __init__(self):
        self.files: Dict[str, Dict[str, Any]] = {}
        self._load()

    def _load(self):
        config_path = SPLAT_EXT_DIR / "mapfs.yaml"
        if not config_path.exists():
            print(f"    Warning: {config_path} not found")
            return

        mapfs_cfg = load_yaml(config_path)
        for entry in mapfs_cfg:
            if isinstance(entry, dict):
                name = entry["name"]
                self.files[name] = entry.copy()
            else:
                name = str(entry)
                self.files[name] = {"name": name}

        print(f"    Loaded {len(self.files)} entries from mapfs.yaml")

    def get(self, name: str) -> Optional[Dict[str, Any]]:
        return self.files.get(name)

    def get_pal_count(self, name: str) -> int:
        entry = self.get(name)
        if entry:
            return entry.get("pal_count", 1)
        return 1

    def get_textures(self, name: str) -> Optional[List]:
        entry = self.get(name)
        if entry:
            return entry.get("textures")
        return None

    def exists(self, name: str) -> bool:
        return name in self.files

_mapfs_config: Optional[MapFSConfig] = None

def get_mapfs_config() -> MapFSConfig:
    global _mapfs_config
    if _mapfs_config is None:
        _mapfs_config = MapFSConfig()
    return _mapfs_config

# =============================================================================
# Icon Configuration Loader
# =============================================================================

class IconConfig:
    def __init__(self):
        self.icons: List[Tuple[str, str, int, int]] = []
        self._load()

    def _load(self):
        config_path = SPLAT_EXT_DIR / "icon.yaml"
        if not config_path.exists():
            print(f"    Warning: {config_path} not found")
            return

        icons_cfg = load_yaml(config_path)
        for icon in icons_cfg:
            fmt = icon[0]
            name = icon[1]
            w = int(icon[2])
            h = int(icon[3])
            self.icons.append((fmt, name, w, h))

        print(f"    Loaded {len(self.icons)} icons from icon.yaml")

_icon_config: Optional[IconConfig] = None

def get_icon_config() -> IconConfig:
    global _icon_config
    if _icon_config is None:
        _icon_config = IconConfig()
    return _icon_config

# =============================================================================
# Sprite Converter - Decompresses YAY0 and converts to LE
# =============================================================================

def convert_animation_component(data: bytearray, comp_offset: int):
    if comp_offset + 12 > len(data):
        return

    cmd_offset = read_u32_be(data, comp_offset)
    cmd_size = read_u16_be(data, comp_offset + 4)

    swap32(data, comp_offset)
    swap16(data, comp_offset + 4)
    swap16(data, comp_offset + 6)
    swap16(data, comp_offset + 8)
    swap16(data, comp_offset + 10)

    if cmd_offset + cmd_size <= len(data):
        swap16_range(data, cmd_offset, cmd_size // 2)

def convert_npc_sprite(data: bytearray):
    if len(data) < 16:
        return

    img_list_off = read_u32_be(data, 0x00)
    pal_list_off = read_u32_be(data, 0x04)

    img_offsets = read_offset_list_be(data, img_list_off)
    pal_offsets = read_offset_list_be(data, pal_list_off)
    anim_offsets = read_offset_list_be(data, 0x10)

    swap32_range(data, 0x00, 4)

    swap_offset_list(data, img_list_off)
    swap_offset_list(data, pal_list_off)
    swap_offset_list(data, 0x10)

    for img_off in img_offsets:
        if img_off + 8 <= len(data):
            swap32(data, img_off)

    for pal_off in pal_offsets:
        if pal_off + 32 <= len(data):
            swap16_range(data, pal_off, 16)

    for anim_off in anim_offsets:
        comp_offsets = read_offset_list_be(data, anim_off)
        swap_offset_list(data, anim_off)
        for comp_off in comp_offsets:
            convert_animation_component(data, comp_off)

def convert_player_sprite(data: bytearray):
    if len(data) < 16:
        return

    raster_list_off = read_u32_be(data, 0x00)
    pal_list_off = read_u32_be(data, 0x04)

    raster_offsets = read_offset_list_be(data, raster_list_off)
    pal_offsets = read_offset_list_be(data, pal_list_off)
    anim_offsets = read_offset_list_be(data, 0x10)

    swap32_range(data, 0x00, 4)

    swap_offset_list(data, raster_list_off)
    swap_offset_list(data, pal_list_off)
    swap_offset_list(data, 0x10)

    for raster_off in raster_offsets:
        if raster_off + 8 <= len(data):
            swap32(data, raster_off)

    for pal_off in pal_offsets:
        if pal_off + 32 <= len(data):
            swap16_range(data, pal_off, 16)

    for anim_off in anim_offsets:
        comp_offsets = read_offset_list_be(data, anim_off)
        swap_offset_list(data, anim_off)
        for comp_off in comp_offsets:
            convert_animation_component(data, comp_off)

def convert_sprites_segment(data: bytes) -> bytes:
    """Convert sprite segment - decompress all YAY0 sprites and convert to LE"""

    if len(data) < 0x20:
        return data

    # Parse header (all offsets are relative to 0x10)
    player_raster_off = read_u32_be(data, 0x10) + 0x10
    player_yay0_off = read_u32_be(data, 0x14) + 0x10
    npc_yay0_off = read_u32_be(data, 0x18) + 0x10
    sprite_end_off = read_u32_be(data, 0x1C) + 0x10

    print(f"    Header: raster=0x{player_raster_off:X} player=0x{player_yay0_off:X} npc=0x{npc_yay0_off:X} end=0x{sprite_end_off:X}")

    # Copy and convert player raster table
    player_raster_data = bytearray(data[player_raster_off:player_yay0_off])

    if len(player_raster_data) >= 12:
        idx_ranges_off = read_u32_be(player_raster_data, 0)
        raster_info_off = read_u32_be(player_raster_data, 4)
        ci4_data_off = read_u32_be(player_raster_data, 8)

        swap32_range(player_raster_data, 0, 3)

        pos = idx_ranges_off
        while pos + 4 <= raster_info_off:
            swap32(player_raster_data, pos)
            pos += 4

        pos = raster_info_off
        while pos + 4 <= ci4_data_off:
            swap32(player_raster_data, pos)
            pos += 4

    # Read player YAY0 offset table (14 player sprites)
    player_yay0_offsets = []
    for i in range(14):
        off = read_u32_be(data, player_yay0_off + i * 4)
        player_yay0_offsets.append(off)

    # Read NPC YAY0 offset table
    npc_yay0_offsets = []
    pos = npc_yay0_off
    while pos + 4 <= sprite_end_off:
        off = read_u32_be(data, pos)
        if off == 0:
            break
        npc_yay0_offsets.append(off)
        pos += 4

    print(f"    Found {len(player_yay0_offsets)} player sprites, {len(npc_yay0_offsets)} NPC sprites")

    # Decompress and convert player sprites
    player_sprites = []
    for i, off in enumerate(player_yay0_offsets):
        if off == 0:
            player_sprites.append(b'')
            continue

        # Find end of this sprite's compressed data
        next_off = None
        for j in range(i + 1, len(player_yay0_offsets)):
            if player_yay0_offsets[j] != 0:
                next_off = player_yay0_off + player_yay0_offsets[j]
                break
        if next_off is None:
            next_off = npc_yay0_off

        yay0_start = player_yay0_off + off
        yay0_data = data[yay0_start:next_off]

        if is_yay0(yay0_data):
            try:
                decompressed = bytearray(decompress_yay0(yay0_data))
                convert_player_sprite(decompressed)
                player_sprites.append(bytes(decompressed))
            except Exception as e:
                print(f"      Warning: Player sprite {i}: {e}")
                player_sprites.append(b'')
        else:
            player_sprites.append(b'')

    # Decompress and convert NPC sprites
    npc_sprites = []
    for i, off in enumerate(npc_yay0_offsets):
        if off == 0:
            npc_sprites.append(b'')
            continue

        # Find end of this sprite's compressed data
        next_off = None
        for j in range(i + 1, len(npc_yay0_offsets)):
            if npc_yay0_offsets[j] != 0:
                next_off = npc_yay0_off + npc_yay0_offsets[j]
                break
        if next_off is None:
            next_off = sprite_end_off

        yay0_start = npc_yay0_off + off
        yay0_data = data[yay0_start:next_off]

        if is_yay0(yay0_data):
            try:
                decompressed = bytearray(decompress_yay0(yay0_data))
                convert_npc_sprite(decompressed)
                npc_sprites.append(bytes(decompressed))
            except Exception as e:
                print(f"      Warning: NPC sprite {i}: {e}")
                npc_sprites.append(b'')
        else:
            npc_sprites.append(b'')

    # Build output file
    out = bytearray()

    # Reserve space for header (0x20 bytes)
    out.extend(b'\x00' * 0x20)

    # Write player raster table
    new_player_raster_off = len(out) - 0x10
    out.extend(player_raster_data)

    # Align to 4 bytes
    while len(out) % 4 != 0:
        out.append(0)

    # Write player sprite offset table
    new_player_yay0_off = len(out) - 0x10
    player_sprite_table_pos = len(out)
    out.extend(b'\x00' * (14 * 4))

    # Write player sprite data and update offsets
    # Offsets are relative to the table position
    for i, sprite_data in enumerate(player_sprites):
        if len(sprite_data) > 0:
            offset = len(out) - player_sprite_table_pos
            write_u32_le(out, player_sprite_table_pos + i * 4, offset)
            out.extend(sprite_data)
            while len(out) % 4 != 0:
                out.append(0)
        else:
            write_u32_le(out, player_sprite_table_pos + i * 4, 0)

    # Align to 16 bytes before NPC section
    while len(out) % 16 != 0:
        out.append(0)

    # Write NPC sprite offset table
    new_npc_yay0_off = len(out) - 0x10
    npc_sprite_table_pos = len(out)
    out.extend(b'\x00' * ((len(npc_yay0_offsets) + 1) * 4))

    # Write NPC sprite data and update offsets
    # Offsets are relative to the table position
    for i, sprite_data in enumerate(npc_sprites):
        if len(sprite_data) > 0:
            offset = len(out) - npc_sprite_table_pos
            write_u32_le(out, npc_sprite_table_pos + i * 4, offset)
            out.extend(sprite_data)
            while len(out) % 4 != 0:
                out.append(0)
        else:
            write_u32_le(out, npc_sprite_table_pos + i * 4, 0)

    # Write sentinel for NPC table
    write_u32_le(out, npc_sprite_table_pos + len(npc_yay0_offsets) * 4, 0)

    # Update header with new offsets
    new_end_off = len(out) - 0x10
    write_u32_le(out, 0x10, new_player_raster_off)
    write_u32_le(out, 0x14, new_player_yay0_off)
    write_u32_le(out, 0x18, new_npc_yay0_off)
    write_u32_le(out, 0x1C, new_end_off)

    print(f"    Output: {len(out):,} bytes (was {len(data):,} compressed)")

    return bytes(out)

# =============================================================================
# MapFS Converter
# =============================================================================

def convert_hit_data(data: bytearray):
    """Convert hit/collision data from BE to LE."""
    if len(data) < 8:
        return

    collision_offset = read_u32_be(data, 0)
    zone_offset      = read_u32_be(data, 4)
    swap32(data, 0)
    swap32(data, 4)

    for section_offset in (collision_offset, zone_offset):
        if section_offset == 0 or section_offset + 0x18 > len(data):
            continue

        hdr = section_offset

        num_colliders    = read_i16_be(data, hdr + 0x00)
        colliders_offset = read_i32_be(data, hdr + 0x04)
        num_vertices     = read_i16_be(data, hdr + 0x08)
        vertices_offset  = read_i32_be(data, hdr + 0x0C)
        bb_data_size     = read_i16_be(data, hdr + 0x10)
        bb_offset        = read_i32_be(data, hdr + 0x14)

        swap16(data, hdr + 0x00)
        swap32(data, hdr + 0x04)
        swap16(data, hdr + 0x08)
        swap32(data, hdr + 0x0C)
        swap16(data, hdr + 0x10)
        swap32(data, hdr + 0x14)

        if bb_offset > 0 and bb_offset < len(data) and bb_data_size > 0:
            swap32_range(data, bb_offset, bb_data_size)

        if vertices_offset > 0 and vertices_offset < len(data) and num_vertices > 0:
            swap16_range(data, vertices_offset, num_vertices * 3)

        if colliders_offset > 0 and colliders_offset < len(data) and num_colliders > 0:
            for c in range(num_colliders):
                cpos = colliders_offset + c * 0x0C
                if cpos + 0x0C > len(data):
                    break

                num_triangles    = read_i16_be(data, cpos + 0x06)
                triangles_offset = read_i32_be(data, cpos + 0x08)

                swap16(data, cpos + 0x00)
                swap16(data, cpos + 0x02)
                swap16(data, cpos + 0x04)
                swap16(data, cpos + 0x06)
                swap32(data, cpos + 0x08)

                if (triangles_offset > 0 and triangles_offset < len(data)
                        and num_triangles > 0):
                    swap32_range(data, triangles_offset, num_triangles)

def convert_tex_data(data: bytearray):
    pos = 0
    tex_idx = 0

    print(f"    [convert_tex_data] total size=0x{len(data):X} ({len(data)} bytes)")

    while pos + 48 <= len(data):
        name_end = data[pos:pos+32].find(0)
        if name_end <= 0:
            print(f"    [tex {tex_idx}] pos=0x{pos:X}: no valid name (name_end={name_end}), stopping")
            break

        try:
            name = data[pos:pos+name_end].decode('ascii')
            if not all(c.isprintable() or c.isspace() for c in name):
                print(f"    [tex {tex_idx}] pos=0x{pos:X}: non-printable name, stopping")
                break
        except:
            print(f"    [tex {tex_idx}] pos=0x{pos:X}: decode error, stopping")
            break

        # Read header fields BEFORE swapping
        aux_w = read_u16_be(data, pos + 32)
        main_w = read_u16_be(data, pos + 34)
        aux_h = read_u16_be(data, pos + 36)
        main_h = read_u16_be(data, pos + 38)
        extra_tiles = data[pos + 41]
        fmts = data[pos + 43]
        depths = data[pos + 44]

        main_fmt = fmts & 0xF
        main_depth = depths & 0xF
        aux_fmt = (fmts >> 4) & 0xF
        aux_depth = (depths >> 4) & 0xF

        # Swap the 4 u16 header fields
        swap16_range(data, pos + 32, 4)

        def raster_size(w, h, depth):
            if depth == 0: return w * h // 2
            elif depth == 1: return w * h
            elif depth == 2: return w * h * 2
            else: return w * h * 4

        def palette_size(fmt, depth):
            if fmt == 2:
                return 32 if depth == 0 else 512
            return 0

        raster_start = pos + 48
        main_raster_sz = raster_size(main_w, main_h, main_depth)
        main_pal_sz = palette_size(main_fmt, main_depth)

        print(f"    [tex {tex_idx}] pos=0x{pos:X} name='{name}' "
              f"main={main_w}x{main_h} aux={aux_w}x{aux_h} "
              f"extra_tiles={extra_tiles} fmt={main_fmt} depth={main_depth} "
              f"aux_fmt={aux_fmt} aux_depth={aux_depth} "
              f"raster_sz=0x{main_raster_sz:X} pal_sz=0x{main_pal_sz:X}")

        # Swap main raster
        if main_fmt == 0:
            if main_depth == 2:
                swap16_range(data, raster_start, main_w * main_h)
            elif main_depth == 3:
                swap32_range(data, raster_start, main_w * main_h)
        if main_fmt == 3 and main_depth == 2:
            swap16_range(data, raster_start, main_w * main_h)

        # Handle mipmaps
        if extra_tiles == 1:
            mipmap_pos = raster_start + main_raster_sz
            divisor = 2
            mm_count = 0
            while main_w // divisor >= (16 >> main_depth) and main_h // divisor > 0:
                mm_w = main_w // divisor
                mm_h = main_h // divisor
                mm_sz = raster_size(mm_w, mm_h, main_depth)
                if main_fmt == 0 and main_depth == 2:
                    swap16_range(data, mipmap_pos, mm_w * mm_h)
                elif main_fmt == 0 and main_depth == 3:
                    swap32_range(data, mipmap_pos, mm_w * mm_h)
                elif main_fmt == 3 and main_depth == 2:
                    swap16_range(data, mipmap_pos, mm_w * mm_h)
                mipmap_pos += mm_sz
                divisor *= 2
                mm_count += 1

            # Include mipmaps in main_raster_sz
            main_raster_sz = mipmap_pos - raster_start
            print(f"           mipmaps={mm_count} updated raster_sz=0x{main_raster_sz:X}")

            # Palette after mipmaps
            if main_fmt == 2:
                pal_colors = 16 if main_depth == 0 else 256
                swap16_range(data, mipmap_pos, pal_colors)
                main_pal_sz = pal_colors * 2
        else:
            # Non-mipmap: palette right after main raster
            if main_fmt == 2:
                pal_start = raster_start + main_raster_sz
                pal_colors = 16 if main_depth == 0 else 256
                swap16_range(data, pal_start, pal_colors)

        aux_start = raster_start + main_raster_sz + main_pal_sz

        if extra_tiles == 2:
            # SHARED_AUX: aux is already in main_raster_sz (mainH covers both halves)
            print(f"           shared_aux: no extra advancement")
            pass
        elif extra_tiles == 3:
            aux_raster_sz = raster_size(aux_w, aux_h, aux_depth)
            aux_pal_sz = palette_size(aux_fmt, aux_depth)

            if aux_fmt == 0 and aux_depth == 2:
                swap16_range(data, aux_start, aux_w * aux_h)
            elif aux_fmt == 0 and aux_depth == 3:
                swap32_range(data, aux_start, aux_w * aux_h)
            elif aux_fmt == 3 and aux_depth == 2:
                swap16_range(data, aux_start, aux_w * aux_h)

            if aux_fmt == 2:
                pal_colors = 16 if aux_depth == 0 else 256
                swap16_range(data, aux_start + aux_raster_sz, pal_colors)

            aux_start += aux_raster_sz + aux_pal_sz
            print(f"           indep_aux: raster=0x{aux_raster_sz:X} pal=0x{aux_pal_sz:X}")

        total_size = aux_start - pos
        print(f"           total_size=0x{total_size:X} next_pos=0x{aux_start:X}")

        pos = aux_start
        tex_idx += 1
        if pos <= raster_start:
            print(f"    [tex] ERROR: pos went backwards! pos=0x{pos:X} <= raster_start=0x{raster_start:X}")
            break

    print(f"    [convert_tex_data] processed {tex_idx} textures, final pos=0x{pos:X}")


def convert_bg_data(data: bytearray, pal_count: int = 1):
    if len(data) < 16:
        return

    for i in range(pal_count):
        header_off = i * 0x10
        if header_off + 0x10 > len(data):
            break

        raster_off_raw = read_u32_be(data, header_off)
        pal_off_raw = read_u32_be(data, header_off + 4)

        raster_off = raster_off_raw - 0x80200000 if raster_off_raw >= 0x80200000 else raster_off_raw
        pal_off = pal_off_raw - 0x80200000 if pal_off_raw >= 0x80200000 else pal_off_raw

        swap32_range(data, header_off, 3)
        swap16_range(data, header_off + 12, 2)

        if pal_off + 512 <= len(data):
            swap16_range(data, pal_off, 256)

def convert_party_data(data: bytearray):
    if len(data) >= 512:
        swap16_range(data, 0, 256)

def convert_title_data(data: bytearray, textures: List):
    if not textures:
        return

    for tex in textures:
        pos = tex[0]
        imgtype = tex[1]

        if imgtype == "pal":
            continue

        w = tex[3]
        h = tex[4]

        if imgtype == "rgba16":
            if pos + w * h * 2 <= len(data):
                swap16_range(data, pos, w * h)
        elif imgtype == "rgba32":
            if pos + w * h * 4 <= len(data):
                swap32_range(data, pos, w * h)
        elif imgtype == "ia16":
            if pos + w * h * 2 <= len(data):
                swap16_range(data, pos, w * h)
        elif imgtype in ("ci4", "ci8"):
            pal_entry = next((t for t in textures if t[1] == "pal" and t[2] == tex[2]), None)
            if pal_entry:
                pal_pos = pal_entry[0]
                pal_size = 16 if imgtype == "ci4" else 256
                if pal_pos + pal_size * 2 <= len(data):
                    swap16_range(data, pal_pos, pal_size)

def convert_mapfs_entry(data: bytes, name: str, config: MapFSConfig) -> bytes:
    # Shape files use the dedicated converter with relocation table
    if name.endswith("_shape"):
        return convert_shape_le(data, debug=False)

    # BG files use the dedicated converter with relocation table
    if name.endswith("_bg"):
        pal_count = config.get_pal_count(name)
        return convert_bg_le(data, pal_count)

    out = bytearray(data)

    if name.endswith("_hit"):
        convert_hit_data(out)
    elif name.endswith("_tex"):
        convert_tex_data(out)
    elif name.startswith("party_"):
        convert_party_data(out)
    elif name == "title_data":
        textures = config.get_textures(name)
        if textures:
            convert_title_data(out, textures)

    return bytes(out)

def convert_mapfs_segment(data: bytes) -> bytes:
    config = get_mapfs_config()

    if len(data) < 0x40:
        return data

    entries = []
    pos = 0x20
    while pos + 0x1C <= len(data):
        name_bytes = data[pos:pos+16]
        name = decode_null_terminated_ascii(name_bytes)

        if name == "end_data" or not name:
            break

        offset = read_u32_be(data, pos + 0x10)
        size = read_u32_be(data, pos + 0x14)
        decomp_size = read_u32_be(data, pos + 0x18)

        entries.append({
            'name': name,
            'offset': offset,
            'size': size,
            'decomp_size': decomp_size,
        })
        pos += 0x1C

    print(f"    Found {len(entries)} entries")

    if SPLIT_MAPFS:
        maps_dir = OUT_DIR / "maps"
        maps_dir.mkdir(parents=True, exist_ok=True)

        index_entries = []

        for entry in entries:
            data_start = 0x20 + entry['offset']
            data_end = data_start + entry['size']
            entry_data = data[data_start:data_end]

            is_compressed = entry['size'] != entry['decomp_size']
            if is_compressed and is_yay0(entry_data):
                try:
                    entry_data = decompress_yay0(entry_data)
                except Exception as e:
                    print(f"      Warning: {entry['name']}: {e}")

            entry_data = convert_mapfs_entry(entry_data, entry['name'], config)

            out_path = maps_dir / f"{entry['name']}.bin"
            out_path.write_bytes(entry_data)

            index_entries.append((entry['name'], len(entry_data)))

        write_mapfs_index(maps_dir, index_entries)

        print(f"    Wrote {len(entries)} individual files to {maps_dir}/")

        return b''

    else:
        out = bytearray()
        out.extend(data[0:0x20])

        toc_start = len(out)
        toc_size = (len(entries) + 1) * 0x1C
        out.extend(b'\x00' * toc_size)

        current_data_pos = toc_start + toc_size

        for i, entry in enumerate(entries):
            data_start = 0x20 + entry['offset']
            data_end = data_start + entry['size']
            entry_data = data[data_start:data_end]

            is_compressed = entry['size'] != entry['decomp_size']
            if is_compressed and is_yay0(entry_data):
                try:
                    entry_data = decompress_yay0(entry_data)
                except Exception as e:
                    print(f"      Warning: {entry['name']}: {e}")

            entry_data = convert_mapfs_entry(entry_data, entry['name'], config)

            toc_pos = toc_start + i * 0x1C
            name_bytes = entry['name'].encode('ascii')[:15].ljust(16, b'\x00')
            out[toc_pos:toc_pos+16] = name_bytes
            struct.pack_into("<III", out, toc_pos + 0x10,
                            current_data_pos - 0x20, len(entry_data), len(entry_data))

            out.extend(entry_data)
            current_data_pos += len(entry_data)

            padding = (16 - (len(out) % 16)) % 16
            out.extend(b'\x00' * padding)
            current_data_pos += padding

        toc_pos = toc_start + len(entries) * 0x1C
        out[toc_pos:toc_pos+16] = b'end_data\x00\x00\x00\x00\x00\x00\x00\x00'
        struct.pack_into("<III", out, toc_pos + 0x10, 0, 0, 0)

        return bytes(out)

def write_mapfs_index(maps_dir: Path, entries: List[Tuple[str, int]]):
    header = """// Auto-generated map asset index
#ifndef MAP_ASSETS_H
#define MAP_ASSETS_H

typedef struct {
    const char* name;
    u32 size;
} MapAssetEntry;

static const MapAssetEntry gMapAssets[] = {
"""

    with open(maps_dir / "map_assets.h", 'w') as f:
        f.write(header)
        for name, size in entries:
            f.write(f'    {{ "{name}", 0x{size:X} }},\n')
        f.write("""    { NULL, 0 }
};

#define MAP_ASSET_COUNT %d

#endif // MAP_ASSETS_H
""" % len(entries))

# =============================================================================
# Message Converter
# =============================================================================

def convert_msg_segment(data: bytes) -> bytes:
    out = bytearray(data)

    section_offsets = []
    pos = 0
    while pos + 4 <= len(data):
        off = read_u32_be(data, pos)
        swap32(out, pos)
        if off == 0:
            break
        section_offsets.append(off)
        pos += 4

    for section_off in section_offsets:
        if section_off >= len(data):
            continue
        pos = section_off
        while pos + 4 <= len(data):
            off = read_u32_be(data, pos)
            swap32(out, pos)
            if off == section_off:
                break
            pos += 4

    return bytes(out)

# =============================================================================
# Charset Converter
# =============================================================================

def convert_charset_segment(data: bytes) -> bytes:
    out = bytearray(data)

    if len(data) > 0x1000:
        estimated_raster_end = (len(data) // 128) * 128
        if estimated_raster_end > len(data) - 0x1000:
            estimated_raster_end = len(data) - 0x1000

        palette_start = (estimated_raster_end + 15) & ~15

        for pos in range(palette_start, len(data) - 1, 2):
            swap16(out, pos)

    return bytes(out)

# =============================================================================
# Icon Converter
# =============================================================================

def convert_icon_segment(data: bytes) -> bytes:
    config = get_icon_config()
    out = bytearray(data)

    if not config.icons:
        print("    Warning: No icon config, using heuristic")
        return convert_icon_segment_heuristic(data)

    print(f"    Processing {len(config.icons)} icons")

    pos = 0
    for fmt, name, w, h in config.icons:
        if fmt == "solo":
            raster_size = w * h // 2
            pal_start = pos + raster_size

            if pal_start + 32 <= len(data):
                swap16_range(out, pal_start, 16)

            pos += raster_size + 32

        elif fmt == "pair":
            raster_size = w * h // 2
            pal1_start = pos + raster_size
            pal2_start = pal1_start + 32

            if pal1_start + 32 <= len(data):
                swap16_range(out, pal1_start, 16)
            if pal2_start + 32 <= len(data):
                swap16_range(out, pal2_start, 16)

            pos += raster_size + 64

        elif fmt == "rgba16":
            raster_size = w * h * 2

            if pos + raster_size <= len(data):
                swap16_range(out, pos, w * h)

            pos += raster_size

    return bytes(out)

def convert_icon_segment_heuristic(data: bytes) -> bytes:
    out = bytearray(data)

    pos = 0
    while pos + 32 <= len(data):
        nonzero = sum(1 for i in range(0, 32, 2)
                     if data[pos+i] != 0 or data[pos+i+1] != 0)
        if nonzero >= 8:
            swap16_range(out, pos, 16)
        pos += 32

    return bytes(out)

# =============================================================================
# Logos Converter - Raw RGBA16 textures, just byte-swap
# =============================================================================

def convert_logos_segment(data: bytes) -> bytes:
    """Convert logos segment - raw RGBA16 texture data, swap every 16-bit word."""
    out = bytearray(data)

    total_pixels = len(data) // 2
    print(f"    Swapping {total_pixels} pixels ({len(data):,} bytes, RGBA16)")

    swap16_range(out, 0, total_pixels)

    if len(data) >= 0x1B000:
        print(f"    Image1 (N64 logo):      0x00000 - 0x07000  128x112")
        print(f"    Image3 (IS logo):        0x07000 - 0x15000  256x112")
        print(f"    Image2 (Nintendo logo):  0x15000 - 0x1B000  256x48")

    return bytes(out)

# =============================================================================
# VROM Table Generator
# =============================================================================

def generate_vrom_table(segments: List[Tuple[str, int, int, int]], out_path: Path):
    header = """// Auto-generated by convert_assets_le.py
// Maps N64 ROM addresses to PC asset files

#ifndef VROM_TABLE_H
#define VROM_TABLE_H

#include "types.h"

typedef struct {
    u32 vrom_start;
    u32 vrom_end;
    u32 pc_size;
    const char* filename;
} VromEntry;

static const VromEntry gVromTable[] = {
"""

    footer = """    { 0, 0, 0, NULL }
};

// Find VROM entry containing address
static inline const VromEntry* vrom_find(u32 addr) {
    int i;
    for (i = 0; gVromTable[i].filename; i++) {
        if (addr >= gVromTable[i].vrom_start && addr < gVromTable[i].vrom_end)
            return &gVromTable[i];
    }
    return NULL;
}

#endif // VROM_TABLE_H
"""

    with open(out_path, 'w') as f:
        f.write(header)
        for name, start, end, pc_size in segments:
            f.write(f'    {{ 0x{start:08X}, 0x{end:08X}, 0x{pc_size:X}, "{name}.bin" }},\n')
        f.write(footer)

# =============================================================================
# Main
# =============================================================================

CONVERTERS = {
    'sprite': (convert_sprites_segment, ['pm_sprites', 'sprite', 'sprites']),
    'mapfs': (convert_mapfs_segment, ['pm_map_data', 'mapfs', 'map_data']),
    'msg': (convert_msg_segment, ['pm_msg', 'msg', 'message']),
    'charset': (convert_charset_segment, ['charset', 'pm_charset']),
    'icon': (convert_icon_segment, ['pm_icons', 'icon', 'icons']),
    'logos': (convert_logos_segment, ['logos']),
}

SEGMENTS = ['sprite', 'mapfs', 'msg', 'charset', 'icon', 'logos']

def main():
    print("Paper Mario Asset Converter (BE -> LE)")
    print("=" * 50)

    errors = []
    if not ROM_PATH.exists():
        errors.append(f"ROM not found: {ROM_PATH}")
    if not MAP_PATH.exists():
        errors.append(f"Map file not found: {MAP_PATH}")
    if not HAS_CRUNCH64:
        errors.append("crunch64 not installed: pip install crunch64")
    if not HAS_YAML:
        errors.append("PyYAML not installed: pip install pyyaml")

    if errors:
        for e in errors:
            print(f"ERROR: {e}")
        print("\nRun './configure us && ninja' first to build the ROM")
        return 1

    OUT_DIR.mkdir(exist_ok=True)

    print(f"\nParsing {MAP_PATH}...")
    symbols = parse_map_file(MAP_PATH)
    print(f"  Found {len(symbols)} symbols")

    print(f"\nLoading {ROM_PATH}...")
    rom_data = ROM_PATH.read_bytes()
    print(f"  {len(rom_data):,} bytes")

    converted = []

    for seg_name in SEGMENTS:
        converter, sym_names = CONVERTERS[seg_name]
        bounds = get_segment_bounds(symbols, sym_names)
        if not bounds:
            print(f"\nWARNING: Segment '{seg_name}' not found (tried: {', '.join(sym_names)})")
            continue

        start, end, matched_name = bounds
        size = end - start
        print(f"\n{seg_name} ({matched_name}): 0x{start:08X} - 0x{end:08X} ({size:,} bytes)")

        seg_data = rom_data[start:end]

        try:
            result = converter(seg_data)
            if len(result) > 0:
                print(f"  Output: {len(result):,} bytes")
                out_path = OUT_DIR / f"{seg_name}.bin"
                out_path.write_bytes(result)
                converted.append((seg_name, start, end, len(result)))
                print(f"  Wrote: {out_path}")
            else:
                print(f"  Output: individual files (see subdirectory)")
        except Exception as e:
            print(f"  ERROR: {e}")
            import traceback
            traceback.print_exc()
            result = seg_data
            out_path = OUT_DIR / f"{seg_name}.bin"
            out_path.write_bytes(result)
            converted.append((seg_name, start, end, len(result)))

    # =================================================================
    # Extract all remaining ROM segments into the VROM table.
    # These are segments like title_bg_1, entity models, etc. that
    # the game DMA's from but aren't one of the 6 converter types.
    # Data that is only single bytes (CI8 rasters) works as-is.
    # Palette segments (_pal) get their u16s swapped.
    # =================================================================
    print(f"\nExtracting remaining ROM segments...")

    paired = {}
    for sym, addr in symbols.items():
        if sym.endswith("_ROM_START"):
            base = sym[:-len("_ROM_START")]
            paired.setdefault(base, {})["start"] = addr
        elif sym.endswith("_ROM_END"):
            base = sym[:-len("_ROM_END")]
            paired.setdefault(base, {})["end"] = addr

    already_done = {s[0] for s in converted}
    already_done.add('mapfs')

    extra_count = 0
    for base, addrs in sorted(paired.items()):
        if "start" not in addrs or "end" not in addrs:
            continue
        start = addrs["start"]
        end = addrs["end"]
        size = end - start
        if size <= 0 or base in already_done:
            continue
        if start >= len(rom_data) or end > len(rom_data):
            continue
        if start >= 0x80000000:
            continue

        seg_data = bytearray(rom_data[start:end])

        # Palette segments are pure u16 RGBA data - swap them
        if base.endswith("_pal"):
            swap16_range(seg_data, 0, len(seg_data) // 2)

        out_path = OUT_DIR / f"{base}.bin"
        out_path.write_bytes(bytes(seg_data))
        converted.append((base, start, end, size))
        extra_count += 1

    print(f"  Extracted {extra_count} additional segments")

    # Now the VROM table includes everything
    print(f"\nGenerating vrom_table.h ({len(converted)} entries)...")
    generate_vrom_table(converted, OUT_DIR / "vrom_table.h")

    print(f"Generating pc_rom_addrs.ld...")
    generate_linker_symbols(converted, symbols, OUT_DIR / "pc_rom_addrs.ld")

    print("\n" + "=" * 50)
    print(f"Done! Output directory: {OUT_DIR}/")
    print("\nGenerated files:")
    for name, _, _, size in converted:
        print(f"  {name}.bin ({size:,} bytes)")
    print(f"  vrom_table.h")

    return 0


def generate_linker_symbols(segments: List[Tuple[str, int, int, int]], symbols: Dict[str, int], out_path: Path):
    """Generate linker script with absolute symbols for all _ROM_START/_ROM_END pairs"""
    with open(out_path, 'w') as f:
        f.write("/* Auto-generated by convert_assets_le.py */\n")
        f.write("/* Absolute symbols so address-of == value, matching N64 linker behavior */\n\n")

        for name, start, end, pc_size in segments:
            f.write(f"{name}_ROM_START = 0x{start:08X};\n")
            f.write(f"{name}_ROM_END = 0x{end:08X};\n")

        f.write("\n/* Additional segments from map file */\n")
        emitted = {s[0] for s in segments}
        paired = {}
        for sym, addr in symbols.items():
            if sym.endswith("_ROM_START"):
                base = sym[:-len("_ROM_START")]
                paired.setdefault(base, {})["start"] = addr
            elif sym.endswith("_ROM_END"):
                base = sym[:-len("_ROM_END")]
                paired.setdefault(base, {})["end"] = addr

        for base, addrs in sorted(paired.items()):
            if base in emitted:
                continue
            if "start" in addrs and "end" in addrs:
                f.write(f"{base}_ROM_START = 0x{addrs['start']:08X};\n")
                f.write(f"{base}_ROM_END = 0x{addrs['end']:08X};\n")


if __name__ == "__main__":
    sys.exit(main())
