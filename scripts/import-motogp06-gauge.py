#!/usr/bin/env python3
"""Extract the original MotoGP 06 Xbox 360 gauge into a bundled HUD resource.

Requires Pillow. Reads supplied overlays.ark and frontend.ark without executing
the donor. Keeps original face/needle pixels and original seven-segment glyphs;
the needle's RPM calibration intentionally follows the printed face scale.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
import struct
from pathlib import Path

from PIL import Image


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def lzss(data: bytes, decoded_size: int) -> bytes:
    """Decode the ARK 4 KiB zero-initialized LZSS ring, with exact consumption."""
    ring = bytearray(4096)
    ring_at, at = 0xFEE, 0
    result = bytearray()
    while len(result) < decoded_size:
        if at >= len(data):
            raise ValueError('Truncated ARK LZSS flags')
        flags = data[at]
        at += 1
        for bit in range(8):
            if flags & (1 << bit):
                if at >= len(data):
                    raise ValueError('Truncated ARK literal')
                value = data[at]
                at += 1
                result.append(value)
                ring[ring_at] = value
                ring_at = (ring_at + 1) & 4095
            else:
                if at + 2 > len(data):
                    raise ValueError('Truncated ARK back-reference')
                a, b = data[at:at + 2]
                at += 2
                offset, length = a | ((b & 0xF0) << 4), (b & 15) + 3
                if len(result) + length > decoded_size:
                    raise ValueError('ARK back-reference exceeds decoded size')
                for index in range(length):
                    value = ring[(offset + index) & 4095]
                    result.append(value)
                    ring[ring_at] = value
                    ring_at = (ring_at + 1) & 4095
            if len(result) == decoded_size:
                break
    if at != len(data):
        raise ValueError('Trailing bytes after ARK LZSS payload')
    return bytes(result)


class Ark:
    """Read selected records from the supplied big-endian Xbox 360 archive."""
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        self.sha256 = sha256(self.data)
        if len(self.data) < 24:
            raise ValueError('Truncated ARK header')
        count = struct.unpack_from('>I', self.data, 8)[0]
        table_end = 24 + count * 16
        if count > 100000 or table_end > len(self.data):
            raise ValueError('Invalid ARK record count')
        self.records = {}
        for index in range(count):
            flags, offset, size, identity = struct.unpack_from(
                '>4I', self.data, 24 + index * 16)
            if flags not in (0, 1) or offset < table_end or offset + size > len(self.data):
                raise ValueError('Invalid ARK record bounds or compression')
            if identity in self.records:
                raise ValueError('Duplicate ARK record identity')
            self.records[identity] = (flags, offset, size)

    def read(self, identity: int, expected_hash: str) -> tuple[bytes, dict]:
        if identity not in self.records:
            raise ValueError(f'Missing required ARK record {identity:08x}')
        flags, offset, size = self.records[identity]
        data = self.data[offset:offset + size]
        if flags:
            if len(data) < 4:
                raise ValueError('Truncated compressed ARK record')
            decoded_size = struct.unpack_from('>I', data)[0]
            if decoded_size > 16 * 1024 * 1024:
                raise ValueError('Required ARK record is too large')
            data = lzss(data[4:], decoded_size)
        digest = sha256(data)
        if digest != expected_hash:
            raise ValueError(f'Unsupported donor record {identity:08x}: {digest}')
        return data, {'record': f'{identity:08x}', 'offset': offset,
                      'stored_size': size, 'decoded_size': len(data), 'sha256': digest}


def decode_face(data: bytes) -> Image.Image:
    if struct.unpack_from('>IIHH', data) != (1, 0x201, 512, 512):
        raise ValueError('Unsupported MotoGP 06 speedo texture')
    # Native TPAGE: linear BC3 blocks, 16-bit byte swap; mip zero begins at 12.
    pixels = bytearray(data[12:12 + 512 * 512])
    pixels[0::2], pixels[1::2] = pixels[1::2], pixels[0::2]
    header = ([124, 0x81007, 512, 512, len(pixels), 0, 1] + [0] * 11 +
              [32, 4, int.from_bytes(b'DXT5', 'little'), 0, 0, 0, 0, 0] +
              [0x1000, 0, 0, 0, 0])
    return Image.open(io.BytesIO(b'DDS ' + struct.pack('<31I', *header) + pixels)).convert('RGBA')


def decode_digits(data: bytes, fontmap: bytes) -> tuple[Image.Image, list[dict]]:
    if struct.unpack_from('>IIHH', data) != (1, 0x801, 1024, 1024):
        raise ValueError('Unsupported MotoGP 06 custom font')
    if struct.unpack_from('>4I', fontmap) != (32, 399, 0, 0):
        raise ValueError('Unsupported MotoGP 06 font map')
    width = height = 1024
    texture_end = 12 + width * height * 2
    count = struct.unpack_from('>I', data, texture_end)[0]
    metrics_base = texture_end + 4
    if count != 1094 or metrics_base + count * 32 != len(data):
        raise ValueError('Invalid MotoGP 06 font metrics')
    # The high nibble is alpha; preserve all original 4-bit channels exactly.
    rgba = bytearray()
    for value, in struct.iter_unpack('>H', data[12:texture_end]):
        rgba.extend((((value >> 8) & 15) * 17, ((value >> 4) & 15) * 17,
                     (value & 15) * 17, ((value >> 12) & 15) * 17))
    atlas = Image.frombytes('RGBA', (width, height), bytes(rgba))
    selected = []
    for digit in range(10):
        # SinglePlayerSpeedo adds U+0155 to ASCII digits. Font map starts at 32.
        index = 389 + digit - 32
        values = struct.unpack_from('>8f', data, metrics_base + index * 32)
        if not all(math.isfinite(value) for value in values):
            raise ValueError('Non-finite custom font metric')
        u0, u1, v0, v1, left, right, top, bottom = values
        bounds = [round(u0 * width), round(v0 * height),
                  round(u1 * width), round(v1 * height)]
        if not (0 <= bounds[0] < bounds[2] <= width and
                0 <= bounds[1] < bounds[3] <= height):
            raise ValueError('Invalid custom font UV rectangle')
        image = atlas.crop(bounds)
        left, right, top, bottom = left * width, right * width, top * height, bottom * height
        advance = image.width + left + right
        line_height = image.height + top + bottom
        if advance != 53 or line_height != 54:
            raise ValueError('Unexpected original digital font cell metrics')
        selected.append((image, left, top, advance))

    # Lossless re-packing with a two-pixel transparent gutter on every side.
    packed = Image.new('RGBA', (sum(image.width + 4 for image, *_ in selected), 62))
    sprites, x = [], 2
    for image, left, top, advance in selected:
        packed.paste(image, (x, 2))
        sprites.append({'texture': 2, 'source': [x, 2, x + image.width, 2 + image.height],
                        'rect': [left, top, left + image.width, top + image.height],
                        'advance': advance})
        x += image.width + 4
    return packed, sprites


def png_hex(image: Image.Image) -> str:
    stream = io.BytesIO()
    image.save(stream, format='PNG')
    return stream.getvalue().hex()


def convert(overlays: Path, frontend: Path) -> dict:
    overlays, frontend = Ark(overlays), Ark(frontend)
    speedo, speedo_source = overlays.read(
        0x51B4189D, '34cdaf8e9472996e7b4b4b92717d088b703ba666a36efefb59bc603cb3cc4a95')
    font, font_source = frontend.read(
        0xAE08FEC6, '1a3ebe35aa310afabaef51c4d6339001bc60f0abb053e92af73434fe3ae5614e')
    fontmap, fontmap_source = frontend.read(
        0x1430AEB0, '142aac74eaad5e0403563cf3ef47784765032a0501ae6e569fd9ec9d1aa0a125')
    atlas = decode_face(speedo)
    face = atlas.crop((16, 0, 272, 256))
    # Native rotating helper 827D4738 reads sprite record 1 (UV offset 0x20),
    # the soft 16x16 sprite. Record 2 is the solid strip used by other quads;
    # using it here incorrectly produces a long opaque bar through the pivot.
    needle = atlas.crop((272, 0, 288, 16))
    digits_atlas, digits = decode_digits(font, fontmap)
    logical_font_scale = (10.172200202941895 * 3.529999958118424e-5 /
                          0.2777777910232544 * 256)
    text = {'scale': logical_font_scale, 'ghost_alpha': .2, 'line_height': 54,
            'color': [255, 255, 255]}
    return {
        'schema': 1, 'kind': 'classic-gauge', 'style': 'motogp06',
        'textures': [png_hex(face), png_hex(needle), png_hex(digits_atlas)],
        'visible_bounds': [32, 30, 228, 226],
        'static': [{'texture': 0, 'source': [0, 0, 256, 256], 'rect': [0, 0, 256, 256]}],
        'needle': {'texture': 1, 'source': [0, 0, 16, 16],
                   'rect': [125.44, 48.64, 130.56, 163.84], 'pivot': [128, 128],
                   'rpm_angles': [[0, 180.7], [6000, 203], [7000, 225], [8000, 246.7],
                                  [9000, 268.6], [10000, 290.1], [11000, 311.6],
                                  [12000, 333.4], [13000, 355.2], [14000, 377],
                                  [15000, 399.1], [16000, 421.3], [17000, 443.6],
                                  [18000, 465.9], [19000, 470]]},
        'digits': digits,
        'speed': {**text, 'center': [185.6, 195.84], 'cells': 3, 'pad_zero': True},
        'gear': {**text, 'center': [166.4, 130.56], 'cells': 1, 'pad_zero': True},
        'origin': {'title': 'MotoGP 06', 'platform': 'Xbox 360',
                   'overlays_sha256': overlays.sha256, 'frontend_sha256': frontend.sha256,
                   'speedo': speedo_source, 'font': font_source, 'fontmap': fontmap_source,
                   'face_rgba_sha256': sha256(face.tobytes()),
                   'needle_rgba_sha256': sha256(needle.tobytes()),
                   'needle_sprite_index': 1, 'native_rotating_helper': '0x827D4738',
                   'needle_rotation': 'clockwise degrees from upward, unwrapped',
                   'calibration': 'Absolute RPM mapped to printed source ticks; 0..6000 uses the bottom zero and 6000 marks. 19000 uses the remaining face arc beyond 18000, keeping the needle clear of the speed box. Deliberately differs from the donor fixed native rotation formula.',
                   'font_codepoints': [389, 398], 'font_glyph_indices': [357, 366]}
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--overlays', type=Path, required=True)
    parser.add_argument('--frontend', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('resources/hud/motogp06/gauge.json'))
    args = parser.parse_args()
    pack = convert(args.overlays, args.frontend)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(pack, separators=(',', ':'), ensure_ascii=True) + '\n', encoding='utf-8')
    print(f'Wrote {args.output} ({args.output.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
