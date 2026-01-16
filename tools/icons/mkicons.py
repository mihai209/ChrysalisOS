#!/usr/bin/env python3
from PIL import Image
import struct
import sys

ICONS_MAGIC = 0x4E4F4349  # 'ICON'
VERSION = 1

icons = [
    (0,  "start.png"),
    (1,  "term.png"),
    (2,  "files.png"),
    (3,  "img.png"),
    (4,  "note.png"),
    (5,  "paint.png"),
    (6,  "calc.png"),
    (7,  "clock.png"),  
    (8,  "calc.png"),     
    (9,  "task.png"),
    (10, "info.png"),
    (11, "3D.png"),
    (12, "mine.png"),
    (13, "net.webp"),
    (14, "x0.png"),
    (15, "run.png"),
]

entries = []
pixel_blobs = []
offset = 0

for icon_id, path in icons:
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    pixels = img.tobytes()  # RGBA8888

    entries.append((icon_id, w, h, offset))
    pixel_blobs.append(pixels)
    offset += len(pixels)

with open("icons.mod", "wb") as f:
    # header
    f.write(struct.pack("<IHH", ICONS_MAGIC, VERSION, len(entries)))

    # entries
    entry_offset = 0
    data_offset = 8 + len(entries) * 10  # header + entries
    cur = data_offset

    for (icon_id, w, h, off), blob in zip(entries, pixel_blobs):
        f.write(struct.pack("<HHHI", icon_id, w, h, cur))
        cur += len(blob)

    # pixel data
    for blob in pixel_blobs:
        f.write(blob)

print("icons.mod generated")
