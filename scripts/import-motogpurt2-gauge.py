#!/usr/bin/env python3
"""Convert the supplied MotoGP URT2 PC gauge without bundling donor archives.

Requires Pillow. The ARK tables, LZSS streams, texture sizes and glyph ranges
are validated before emitting the self-contained classic-gauge resource.
"""

import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct

from PIL import Image


def unpack(data, offset, fmt):
    size = struct.calcsize(fmt)
    if offset < 0 or offset + size > len(data):
        raise ValueError("Truncated MotoGP URT2 resource")
    return struct.unpack_from(fmt, data, offset)


def lzss(data, size):
    if not 0 < size <= 16 * 1024 * 1024:
        raise ValueError("Invalid ARK decoded size")
    ring = bytearray(4096)
    cursor, offset = 0xFEE, 0
    result = bytearray()
    while len(result) < size:
        flags, = unpack(data, offset, "<B")
        offset += 1
        for bit in range(8):
            if flags & (1 << bit):
                value, = unpack(data, offset, "<B")
                offset += 1
                values = [value]
            else:
                first, second = unpack(data, offset, "<BB")
                offset += 2
                source = first | ((second & 0xF0) << 4)
                count = (second & 15) + 3
                if len(result) + count > size:
                    raise ValueError("ARK match exceeds decoded size")
                # Copy sequentially: source matches may overlap their output.
                values = None
                for index in range(count):
                    value = ring[(source + index) & 4095]
                    result.append(value)
                    ring[cursor] = value
                    cursor = (cursor + 1) & 4095
            if values is not None:
                for value in values:
                    result.append(value)
                    ring[cursor] = value
                    cursor = (cursor + 1) & 4095
            if len(result) >= size:
                break
    if len(result) != size or offset != len(data):
        raise ValueError("ARK stream does not end at its declared bounds")
    return bytes(result)


def read_ark(path):
    data = path.read_bytes()
    count, record_size = unpack(data, 8, "<II")
    if not 0 < count <= 4096 or record_size != 16:
        raise ValueError("Unsupported MotoGP URT2 PC ARK table")
    end = 24 + count * record_size
    if end > len(data):
        raise ValueError("Truncated ARK record table")
    records = {}
    for index in range(count):
        compressed, offset, size, name = unpack(data, 24 + index * 16, "<IIII")
        if compressed not in (0, 1) or offset < end or offset + size > len(data):
            raise ValueError("Invalid ARK member bounds")
        if name in records:
            raise ValueError("Duplicate ARK filename hash")
        member = data[offset:offset + size]
        if compressed:
            decoded_size, = unpack(member, 0, "<I")
            member = lzss(member[4:], decoded_size)
        records[name] = member
    return records


def name_hash(name):
    result = 0
    for value in name.encode("ascii"):
        result = (result * 31 + value) & 0xFFFFFFFF
    return result


def read_tpage(data):
    extra, = unpack(data, 0, "<I")
    if extra not in (0, 1):
        raise ValueError("Unsupported texture page header")
    header = extra * 4
    kind, width, height = unpack(data, header, "<IHH")
    if not 0 < width <= 2048 or not 0 < height <= 2048:
        raise ValueError("Invalid texture dimensions")
    if kind == 0x01030000:
        # This PC format is linear A8, not BC3 despite its one-byte texel size.
        size = width * height
        pixels = data[header + 8:header + 8 + size]
        if len(pixels) != size:
            raise ValueError("Truncated font alpha texture")
        image = Image.new("RGBA", (width, height), "white")
        image.putalpha(Image.frombytes("L", (width, height), pixels))
    elif kind in (0x01010000, 0x01020000):
        # Native PC format tables (URT2 0x5DC418, URT3 0x61DE60) map
        # enum 2 to D3DFMT_DXT5. BC2 produces plausible RGB but corrupt alpha.
        fourcc = b"DXT1" if kind == 0x01010000 else b"DXT5"
        size = ((width + 3) // 4) * ((height + 3) // 4) * (8 if fourcc == b"DXT1" else 16)
        pixels = data[header + 8:header + 8 + size]
        if len(pixels) != size:
            raise ValueError("Truncated gauge texture")
        dds_header = [124, 0x81007, height, width, size, 0, 1] + [0] * 11
        dds_header += [32, 4, int.from_bytes(fourcc, "little"), 0, 0, 0, 0, 0]
        dds_header += [0x1000, 0, 0, 0, 0]
        image = Image.open(io.BytesIO(b"DDS " + struct.pack("<31I", *dds_header) + pixels)).convert("RGBA")
    else:
        raise ValueError("Unsupported MotoGP URT2 texture format")
    offset = header + 8 + size
    metrics = []
    if extra:
        count, = unpack(data, offset, "<I")
        offset += 4
        if count > 4096 or len(data) != offset + count * 32:
            raise ValueError("Invalid texture page metrics")
        for index in range(count):
            values = unpack(data, offset + index * 32, "<8f")
            if not all(math.isfinite(value) for value in values):
                raise ValueError("Invalid sprite coordinates")
            if not (0 <= values[0] < values[1] <= 1 and 0 <= values[2] < values[3] <= 1):
                raise ValueError("Invalid sprite UV bounds")
            metrics.append(values)
    elif offset != len(data):
        raise ValueError("Unexpected texture page trailer")
    return image, metrics


def crop_rect(image, values):
    return [round(values[0] * image.width), round(values[2] * image.height),
            round(values[1] * image.width), round(values[3] * image.height)]


def png_hex(image):
    stream = io.BytesIO()
    image.save(stream, format="PNG", optimize=False)
    return stream.getvalue().hex()


def sprite(texture, source, rect):
    return {"texture": texture, "source": source, "rect": rect}


def convert(overlays_path, fonts_path):
    overlays, fonts = read_ark(overlays_path), read_ark(fonts_path)
    face_data = overlays[name_hash("speedo.tpage")]
    font_data = fonts[name_hash("eurostile_bold_26pt.font")]
    # Original shared character-map record; do not silently assume another
    # language/build maps these codepoints to the same glyph indices.
    if unpack(fonts[0x1430AEB0], 0, "<4I") != (32, 399, 0, 0):
        raise ValueError("Unexpected original digital character-map range")
    atlas, sprites = read_tpage(face_data)
    font, metrics = read_tpage(font_data)
    if len(sprites) != 3 or font.size != (1024, 1024):
        raise ValueError("Unexpected original gauge or font layout")
    # Original character-map range 32..399 maps to glyph indices 0..367.
    # SinglePlayerSpeedo adds 0x155 to ASCII digits, yielding 389..398.
    faces = [atlas.crop(crop_rect(atlas, record)) for record in sprites]
    if [image.size for image in faces] != [(101, 101), (16, 128), (16, 16)]:
        raise ValueError("Unexpected original gauge sprite dimensions")
    font_strip = Image.new("RGBA", (300, 128))
    digits = []
    for digit in range(10):
        record = metrics[389 + digit - 32]
        crop = crop_rect(font, record)
        image = font.crop(crop)
        x, y = (digit % 5) * 60 + 2, (digit // 5) * 64 + 2
        if image.width > 56 or image.height > 60:
            raise ValueError("Unexpected original digital glyph dimensions")
        # No resampling or raster redesign: copy each original glyph byte-for-byte.
        font_strip.paste(image, (x, y))
        left, top = record[4] * font.width, record[6] * font.height
        item = sprite(3, [x, y, x + image.width, y + image.height],
                      [left, top, left + image.width, top + image.height])
        item["advance"] = (record[1] - record[0] + record[4] + record[5]) * font.width
        digits.append(item)
    # Native source geometry measured in the original 101x101 face crop.
    # pivot_bleugh offsets the needle by a fraction of its LENGTH, not face size.
    width, length = 0.02879999950528145 * 101, 0.33079999685287476 * 101
    pivot_offset = -0.16210000216960907 * length
    needle = sprite(1, [0, 0, 16, 128],
                    [50.5 - width / 2, 50.5 - length + pivot_offset,
                     50.5 + width / 2, 50.5 + pivot_offset])
    needle.update(pivot=[50.5, 50.5], normalized_rpm=True,
                  rpm_angles=[[0, math.degrees(-2.2878000736236572) + 360],
                              [1, math.degrees(2.299999952316284) + 360]])
    font_scale = 10 * 3.529999958118424e-5 * 101 / 0.15468749403953552
    speed = {"center": [50.5, 50.5 + 0.2533999979496002 * 101],
             "scale": font_scale, "ghost_alpha": 0.2, "cells": 3,
             "line_height": 54, "pad_zero": True, "color": [255, 255, 0]}
    gear = {**speed, "center": [50.5, 50.5], "cells": 1, "pad_zero": False}
    coordinates = [(0.32339999079704285, 0.9006999731063843),
                   (0.40849998593330383, 0.9348000288009644),
                   (0.5063999891281128, 0.9460999965667725),
                   (0.6043000221252441, 0.9348000288009644),
                   (0.6935999989509583, 0.9035000205039978)]
    off_colors = [[0, 32, 0], [32, 32, 0], [32, 16, 0], [32, 0, 0], [32, 0, 0]]
    on_colors = [[0, 128, 0], [128, 128, 0], [128, 64, 0], [128, 0, 0], [255, 0, 0]]
    radius = 0.009999999776482582 * 0.5 * 101 / 0.15468749403953552
    lights = []
    for index, (x, y) in enumerate(coordinates):
        lights.append({"sprite": sprite(2, [0, 0, 16, 16],
                          [x * 101 - radius, y * 101 - radius,
                           x * 101 + radius, y * 101 + radius]),
                       # The port maps the unnumbered needle to each bike's
                       # maximum. Match that scale: the reviewed face's first
                       # red-zone tick is at 77.5% of the authored sweep.
                       "threshold": (10000 + index * 500) / 12000 * 0.775,
                       "normalized_rpm": True,
                       "off_color": off_colors[index], "on_color": on_colors[index],
                       "blink_frames": 10 if index == 4 else 0})
    digest = lambda data: hashlib.sha256(data).hexdigest()
    return {"schema": 1, "kind": "classic-gauge", "style": "motogpurt2",
            "visible_bounds": [0, 0, 101, 101],
            "textures": [png_hex(image) for image in [*faces, font_strip]],
            "static": [sprite(0, [0, 0, 101, 101], [0, 0, 101, 101])],
            "needle": needle, "digits": digits, "speed": speed, "gear": gear,
            "lights": lights,
            "origin": {"source": "MotoGP URT2 PC", "overlays_sha256": digest(overlays_path.read_bytes()),
                           "fonts_sha256": digest(fonts_path.read_bytes()),
                           "speedo_tpage_sha256": digest(face_data), "font_sha256": digest(font_data),
                           "texture_format": "BC3/DXT5; native format table 0x5DC418, index 2",
                           "native_render": "0x004ad250", "native_font_shift": "0x004ad818",
                           "rpm_adaptation": "Unnumbered native full sweep follows actual RPM / current bike maximum RPM.",
                           "light_thresholds": "Normalized to current bike maximum; fifth light at 0.775 matches red-zone entry, retaining donor relative spacing and frame-based blink.",
                           "color_limit": "Native face/needle color 128 may be neutral with Climax shader gain; external shader not supplied. Raw artwork retained."}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--overlays", required=True, type=Path)
    parser.add_argument("--fonts", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = convert(args.overlays, args.fonts)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"Wrote {args.output} ({args.output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
