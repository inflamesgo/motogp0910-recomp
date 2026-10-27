#!/usr/bin/env python3
"""Convert the supplied MotoGP URT3 PC gauge and adapt its last two RPM labels.

Requires Pillow and the adjacent URT2 importer for the shared PC ARK/TPAGE
decoder. Original donor files remain read-only. The two changed digits reuse
the face's own numeral pixels; the remaining artwork is unchanged.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path

from PIL import Image, ImageFilter


_spec = importlib.util.spec_from_file_location(
    'climax_pc_gauge', Path(__file__).with_name('import-motogpurt2-gauge.py'))
pc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pc)


def digest(data):
    return hashlib.sha256(data).hexdigest()


LABEL_PATCHES = [
    {'old': '15', 'new': '16', 'target': [96, 39, 104, 48],
     'source': [100, 59, 108, 68], 'source_digit': '6'},
    {'old': '16', 'new': '18', 'target': [100, 59, 108, 68],
     'source': [44, 93, 52, 102], 'source_digit': '8'},
]


def ink_mask(image):
    # The white numeral is brighter than the dark carbon backing. Include its
    # original antialias fringe without resampling the original glyph.
    mask = Image.new('L', image.size)
    pixels = (image.getpixel((x, y)) for y in range(image.height) for x in range(image.width))
    mask.putdata([255 if min(pixel[:3]) * pixel[3] / 255 > 72 else 0 for pixel in pixels])
    return mask.filter(ImageFilter.MaxFilter(3))


def relabel(face):
    original = face.copy()
    result = face.copy()
    for patch in LABEL_PATCHES:
        left, top, right, bottom = patch['target']
        previous = original.crop(patch['target'])
        glyph = original.crop(patch['source'])
        if glyph.size != previous.size:
            raise ValueError('Original face numeral cells have changed')
        # Restore only the old digit's ink footprint from a nearby untouched
        # carbon strip. The first digit, ticks, rims and other labels stay put.
        backing = original.crop((left - 12, top, right - 12, bottom))
        result.paste(backing, (left, top), ink_mask(previous))
        result.paste(glyph, (left, top), ink_mask(glyph))
    return result


def convert(overlays_path, fonts_path, shaders_path):
    overlays, fonts = pc.read_ark(overlays_path), pc.read_ark(fonts_path)
    shaders = pc.read_ark(shaders_path)
    speedo = overlays[pc.name_hash('speedo.tpage')]
    font_data = fonts[pc.name_hash('eurostile_bold_26pt.font')]
    shader = shaders[pc.name_hash('tex_gouraudalpha.pso')]
    if digest(shader) != 'a3476d5552f77e1c68f859670d9cf642e71b2dff64beb9d256a70861d94df266':
        raise ValueError('Unsupported original URT3 gauge pixel shader')
    if pc.unpack(fonts[0x1430AEB0], 0, '<4I') != (32, 399, 0, 0):
        raise ValueError('Unsupported original URT3 font map')
    atlas, sprites = pc.read_tpage(speedo)
    font, metrics = pc.read_tpage(font_data)
    if atlas.size != (128, 256) or len(sprites) != 3 or font.size != (1024, 1024) or len(metrics) != 981:
        raise ValueError('Unsupported original URT3 texture layout')
    crops = [atlas.crop(pc.crop_rect(atlas, record)) for record in sprites]
    if [image.size for image in crops] != [(128, 128), (16, 128), (16, 16)]:
        raise ValueError('Unsupported original URT3 sprite bounds')
    original_face, needle_image, _ = crops
    face = relabel(original_face)

    digits_atlas = Image.new('RGBA', (300, 128))
    digits = []
    for digit in range(10):
        record = metrics[357 + digit]
        image = font.crop(pc.crop_rect(font, record))
        left, top = record[4] * 1024, record[6] * 1024
        advance = image.width + left + record[5] * 1024
        line_height = image.height + top + record[7] * 1024
        if image.width > 56 or image.height > 60 or advance != 53 or line_height != 54:
            raise ValueError('Unsupported original URT3 digital glyph metrics')
        x, y = (digit % 5) * 60 + 2, (digit // 5) * 64 + 2
        digits_atlas.paste(image, (x, y))
        glyph = pc.sprite(2, [x, y, x + image.width, y + image.height],
                          [left, top, left + image.width, top + image.height])
        glyph['advance'] = advance
        digits.append(glyph)

    needle = pc.sprite(1, [0, 0, 16, 128],
                       [60.428800106048584, 6.284801244735718,
                        65.54879999160767, 69.65760016441345])
    needle.update(pivot=[62.988800048828125, 60.35199999809265],
                  rpm_angles=[[0, 181.1], [8000, 202.4], [9000, 234.9],
                              [10000, 267], [11000, 298.7], [12000, 329.4],
                              [13000, 360.5], [14000, 391.8], [16000, 423.2],
                              [18000, 454.6], [19000, 360 + math.degrees(1.7209999561309814)]])
    text = {'scale': 0.2557889049135699, 'line_height': 54,
            'ghost_alpha': .2, 'color': [255, 255, 0]}
    return {
        'schema': 1, 'kind': 'classic-gauge', 'style': 'motogpurt3',
        'visible_bounds': [0, 0, 128, 128],
        'textures': [pc.png_hex(image) for image in (face, needle_image, digits_atlas)],
        'static': [pc.sprite(0, [0, 0, 128, 128], [0, 0, 128, 128])],
        'needle': needle, 'digits': digits,
        'speed': {**text, 'center': [95.25760078430176, 102.13119888305664],
                  'cells': 3, 'pad_zero': True},
        'gear': {**text, 'center': [110.78400039672852, 80.16640090942383],
                 'cells': 1, 'pad_zero': False},
        'origin': {
            'title': 'MotoGP URT 3', 'platform': 'PC',
            'overlays_sha256': digest(overlays_path.read_bytes()),
            'fonts_sha256': digest(fonts_path.read_bytes()),
            'pshader_sha256': digest(shaders_path.read_bytes()),
            'speedo_record': '51b4189d', 'speedo_sha256': digest(speedo),
            'font_record': 'ae08fec6', 'font_sha256': digest(font_data),
            'shader_record': '959470a9', 'shader_sha256': digest(shader),
            'texture_format': 'BC3/DXT5; native format table 0x61DE60, index 2',
            'original_face_rgba_sha256': digest(original_face.tobytes()),
            'modified_face_rgba_sha256': digest(face.tobytes()),
            'label_patches': LABEL_PATCHES,
            'label_method': 'Original baked glyph pixels, no resampling. Only second-digit cells change; nearby carbon restores the removed ink.',
            'calibration': 'Absolute RPM follows printed ticks, including changed 16 and 18. 19000 uses the remaining native capped sweep after 18, keeping the needle clear of the gear bubble.',
            'native_render': '0x004CDEB0',
            'shader': 'ps.1.1: saturate(2*texture.rgb*vertex.rgb), texture.a*vertex.a. Source sprite RGB128 is neutral; original PNG RGB retained.',
            'lights': 'Original lights_enabled=false; no shift lights fabricated.'
        }
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--overlays', type=Path, required=True)
    parser.add_argument('--fonts', type=Path, required=True)
    parser.add_argument('--pshader', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    pack = convert(args.overlays, args.fonts, args.pshader)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(pack, separators=(',', ':')) + '\n', encoding='utf-8')
    print(f'Wrote {args.output} ({args.output.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
