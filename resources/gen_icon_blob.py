#!/usr/bin/env python3
"""Packs resources/icons/*/*.c (LVGL v9 C-array images) into one icons.bin blob.

The ESP8266 can only map ~960KB of flash for code+rodata, so the weather icons
live in their own data partition ("icons" in partitions.csv) instead of being
compiled into the app. app/src/IconFs.c serves this blob to LVGL as drive "I:",
and LVGL's bin decoder reads each icon row by row (no full-image RAM buffer).

Layout (all little-endian):
    u32 magic 'WICO', u32 count
    count x { char name[24] (NUL-padded, no extension), u32 offset, u32 size }
    then each icon: a standard LVGL v9 .bin image -- 12-byte lv_image_header_t
    (u8 magic 0x19, u8 cf, u16 flags, u16 w, u16 h, u16 stride, u16 reserved)
    followed by the pixel data. Offsets are from the start of the blob.

Usage: gen_icon_blob.py <output.bin> <icon.c>...
"""
import re
import struct
import sys
from pathlib import Path

BLOB_MAGIC = b"WICO"
NAME_LEN = 24
LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12


def parse_icon(path):
    src = path.read_text()
    name = path.stem

    def field(key):
        m = re.search(r"\." + key + r"\s*=\s*(\w+)", src)
        if not m:
            sys.exit(f"{path}: no .{key} in image descriptor")
        return m.group(1)

    if field("cf") != "LV_COLOR_FORMAT_RGB565":
        sys.exit(f"{path}: expected LV_COLOR_FORMAT_RGB565, got {field('cf')}")
    w, h, stride = int(field("w")), int(field("h")), int(field("stride"))

    body = re.search(r"_map\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not body:
        sys.exit(f"{path}: no pixel map array")
    data = bytes(int(b, 16) for b in re.findall(r"0x([0-9a-fA-F]{2})", body.group(1)))
    if len(data) != stride * h:
        sys.exit(f"{path}: {len(data)} data bytes, expected stride*h = {stride * h}")
    if len(name.encode()) >= NAME_LEN:
        sys.exit(f"{path}: name longer than {NAME_LEN - 1} chars")

    header = struct.pack("<BBHHHHH", LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565, 0, w, h, stride, 0)
    return name, header + data


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out = Path(sys.argv[1])
    icons = sorted((parse_icon(Path(p)) for p in sys.argv[2:]), key=lambda i: i[0])

    index_size = 8 + len(icons) * (NAME_LEN + 8)
    index = BLOB_MAGIC + struct.pack("<I", len(icons))
    payload = b""
    for name, image in icons:
        index += name.encode().ljust(NAME_LEN, b"\0") + struct.pack("<II", index_size + len(payload), len(image))
        payload += image

    out.write_bytes(index + payload)


if __name__ == "__main__":
    main()
