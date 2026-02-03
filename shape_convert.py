#!/usr/bin/env python3
"""
shape_convert.py - Convert Paper Mario shape files to LE with relocation table

Structures (from model.h):

ShapeFileHeader (0x20 bytes):
  0x00: ModelNode* root
  0x04: Vtx_t* vertexTable
  0x08: char** modelNames
  0x0C: char** colliderNames
  0x10: char** zoneNames
  0x14: pad[0xC]

ModelNode (0x14 bytes):
  0x00: s32 type
  0x04: ModelDisplayData* displayData
  0x08: s32 numProperties
  0x0C: ModelNodeProperty* propertyList
  0x10: ModelGroupData* groupData

ModelGroupData (0x14 bytes):
  0x00: Mtx* transformMatrix
  0x04: Lightsn* lightingGroup
  0x08: s32 numLights
  0x0C: s32 numChildren
  0x10: ModelNode** childList

ModelDisplayData (0x08 bytes):
  0x00: Gfx* displayList
  0x04: pad

ModelNodeProperty (0x0C bytes):
  0x00: s32 key
  0x04: s32 dataType
  0x08: union data (s32/f32/void*)
"""

import struct
from typing import List, Set

BASE_ADDR = 0x80210000

def read_u32_be(data: bytes, off: int) -> int:
    return struct.unpack(">I", data[off:off+4])[0]

def read_u16_be(data: bytes, off: int) -> int:
    return struct.unpack(">H", data[off:off+2])[0]

def write_u32_le(data: bytearray, off: int, val: int):
    struct.pack_into("<I", data, off, val & 0xFFFFFFFF)

def swap16(data: bytearray, off: int):
    if off + 1 < len(data):
        data[off], data[off+1] = data[off+1], data[off]

def swap32(data: bytearray, off: int):
    if off + 3 < len(data):
        data[off], data[off+1], data[off+2], data[off+3] = \
            data[off+3], data[off+2], data[off+1], data[off]

def swap16_range(data: bytearray, start: int, count: int):
    for i in range(count):
        swap16(data, start + i * 2)

def swap32_range(data: bytearray, start: int, count: int):
    for i in range(count):
        swap32(data, start + i * 4)

def is_valid_ptr(val: int, file_size: int) -> bool:
    if val < BASE_ADDR:
        return False
    offset = val - BASE_ADDR
    return offset < file_size

class ShapeConverter:
    def __init__(self, data: bytes):
        self.orig = data
        self.data = bytearray(data)
        self.size = len(data)
        self.relocs: List[int] = []
        self.reloc_set: Set[int] = set()
        self.visited_nodes: Set[int] = set()

    def add_reloc(self, offset: int):
        if offset not in self.reloc_set and offset + 4 <= self.size:
            self.relocs.append(offset)
            self.reloc_set.add(offset)

    def convert_ptr(self, offset: int) -> int:
        """Convert pointer at offset to file offset, record reloc, swap to LE. Returns file offset or 0."""
        if offset + 4 > self.size:
            return 0
        val = read_u32_be(self.orig, offset)
        if is_valid_ptr(val, self.size):
            file_off = val - BASE_ADDR
            self.add_reloc(offset)
            write_u32_le(self.data, offset, file_off)
            return file_off
        else:
            swap32(self.data, offset)
            return 0

    def process_string_list(self, offset: int):
        """Process null-terminated array of string pointers"""
        if offset == 0 or offset >= self.size:
            return
        pos = offset
        while pos + 4 <= self.size:
            val = read_u32_be(self.orig, pos)
            if val == 0:
                swap32(self.data, pos)
                break
            self.convert_ptr(pos)
            pos += 4

    def process_display_list(self, offset: int):
        """Process Gfx commands until G_ENDDL"""
        if offset == 0 or offset >= self.size:
            return
        pos = offset
        max_cmds = 0x10000
        while pos + 8 <= self.size and max_cmds > 0:
            cmd = self.orig[pos]
            word1 = read_u32_be(self.orig, pos + 4)

            swap32(self.data, pos)

            # Commands with pointers in word1: G_VTX, G_DL, G_MTX, etc.
            if cmd in (0x01, 0x06, 0xDA, 0xDB, 0xD9, 0xDE) and is_valid_ptr(word1, self.size):
                self.convert_ptr(pos + 4)
            else:
                swap32(self.data, pos + 4)

            if cmd == 0xDF:  # G_ENDDL
                break
            pos += 8
            max_cmds -= 1

    def process_display_data(self, offset: int):
        """Process ModelDisplayData (0x08 bytes)"""
        if offset == 0 or offset >= self.size:
            return
        if offset + 8 > self.size:
            return

        # 0x00: Gfx* displayList
        gfx_off = self.convert_ptr(offset + 0x00)
        # 0x04: pad
        swap32(self.data, offset + 0x04)

        if gfx_off > 0:
            self.process_display_list(gfx_off)

    def process_property_list(self, offset: int, count: int):
        """Process array of ModelNodeProperty (0x0C each)"""
        if offset == 0 or offset >= self.size or count == 0:
            return

        for i in range(count):
            prop_off = offset + i * 0x0C
            if prop_off + 0x0C > self.size:
                break

            # 0x00: s32 key
            swap32(self.data, prop_off + 0x00)
            # 0x04: s32 dataType
            data_type = read_u32_be(self.orig, prop_off + 0x04)
            swap32(self.data, prop_off + 0x04)
            # 0x08: union data - could be pointer if dataType indicates
            # For now just swap as u32; most properties are ints/floats
            val = read_u32_be(self.orig, prop_off + 0x08)
            if is_valid_ptr(val, self.size):
                self.convert_ptr(prop_off + 0x08)
            else:
                swap32(self.data, prop_off + 0x08)

    def process_group_data(self, offset: int):
        """Process ModelGroupData (0x14 bytes)"""
        if offset == 0 or offset >= self.size or offset in self.visited_nodes:
            return
        self.visited_nodes.add(offset)

        if offset + 0x14 > self.size:
            return

        # Read before swapping
        num_children = read_u32_be(self.orig, offset + 0x0C)
        child_list_val = read_u32_be(self.orig, offset + 0x10)

        # 0x00: Mtx* transformMatrix
        mtx_off = self.convert_ptr(offset + 0x00)
        # 0x04: Lightsn* lightingGroup
        self.convert_ptr(offset + 0x04)
        # 0x08: s32 numLights
        swap32(self.data, offset + 0x08)
        # 0x0C: s32 numChildren
        swap32(self.data, offset + 0x0C)
        # 0x10: ModelNode** childList
        child_list_off = self.convert_ptr(offset + 0x10)

        # Swap matrix data (Mtx = 0x40 bytes, 16 x s32 in fixed point)
        if mtx_off > 0 and mtx_off + 0x40 <= self.size:
            swap32_range(self.data, mtx_off, 16)

        # Process child list - array of ModelNode pointers
        if child_list_off > 0 and num_children > 0:
            for i in range(num_children):
                child_ptr_off = child_list_off + i * 4
                if child_ptr_off + 4 > self.size:
                    break
                child_node_val = read_u32_be(self.orig, child_ptr_off)
                child_node_off = self.convert_ptr(child_ptr_off)
                if child_node_off > 0:
                    self.process_model_node(child_node_off)

    def process_model_node(self, offset: int):
        """Process ModelNode (0x14 bytes)"""
        if offset == 0 or offset >= self.size or offset in self.visited_nodes:
            return
        self.visited_nodes.add(offset)

        if offset + 0x14 > self.size:
            return

        # Read before swapping
        num_props = read_u32_be(self.orig, offset + 0x08)

        # 0x00: s32 type
        swap32(self.data, offset + 0x00)
        # 0x04: ModelDisplayData* displayData
        display_off = self.convert_ptr(offset + 0x04)
        # 0x08: s32 numProperties
        swap32(self.data, offset + 0x08)
        # 0x0C: ModelNodeProperty* propertyList
        prop_off = self.convert_ptr(offset + 0x0C)
        # 0x10: ModelGroupData* groupData
        group_off = self.convert_ptr(offset + 0x10)

        # Process sub-structures
        if display_off > 0:
            self.process_display_data(display_off)
        if prop_off > 0 and num_props > 0:
            self.process_property_list(prop_off, num_props)
        if group_off > 0:
            self.process_group_data(group_off)

    def process_header(self):
        """Process ShapeFileHeader (0x20 bytes)"""
        if self.size < 0x20:
            return

        # 0x00: ModelNode* root
        root_off = self.convert_ptr(0x00)
        # 0x04: Vtx_t* vertexTable
        self.convert_ptr(0x04)
        # 0x08: char** modelNames
        model_names_off = self.convert_ptr(0x08)
        # 0x0C: char** colliderNames
        collider_names_off = self.convert_ptr(0x0C)
        # 0x10: char** zoneNames
        zone_names_off = self.convert_ptr(0x10)
        # 0x14-0x1F: padding
        swap32_range(self.data, 0x14, 3)

        # Process string lists
        if model_names_off > 0:
            self.process_string_list(model_names_off)
        if collider_names_off > 0:
            self.process_string_list(collider_names_off)
        if zone_names_off > 0:
            self.process_string_list(zone_names_off)

        # Process model tree
        if root_off > 0:
            self.process_model_node(root_off)

    def convert(self) -> bytes:
        self.process_header()
        self.relocs.sort()

        out = bytearray(self.data)
        while len(out) % 4 != 0:
            out.append(0)

        for off in self.relocs:
            out.extend(struct.pack("<I", off))
        out.extend(struct.pack("<I", len(self.relocs)))

        return bytes(out)


def convert_shape_le(data: bytes, debug: bool = False) -> bytes:
    if len(data) < 0x20:
        return data

    converter = ShapeConverter(data)
    result = converter.convert()

    if debug:
        print(f"    Shape: {len(data)} bytes, {len(converter.relocs)} relocations")
        for i, off in enumerate(converter.relocs[:10]):
            orig_val = read_u32_be(data, off)
            print(f"      reloc[{i}]: offset 0x{off:04X}, orig 0x{orig_val:08X} -> 0x{orig_val - BASE_ADDR:04X}")
        if len(converter.relocs) > 10:
            print(f"      ... and {len(converter.relocs) - 10} more")

    return result


if __name__ == "__main__":
    import sys
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.bin>")
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        data = f.read()

    result = convert_shape_le(data, debug=True)

    with open(sys.argv[2], 'wb') as f:
        f.write(result)

    orig_size = len(data)
    new_size = len(result)
    reloc_count = struct.unpack("<I", result[-4:])[0]
    print(f"Converted: {orig_size} -> {new_size} bytes")
    print(f"Relocations: {reloc_count}")
