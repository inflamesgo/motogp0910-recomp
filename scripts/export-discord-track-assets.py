#!/usr/bin/env python3
"""Export the owner's decoded frontend banks as square Discord track assets.

Requires Pillow. Decompression is a separate local step; no decompressor or
original artwork is committed or included in the application package.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile

# Index, Discord key, title, bank, native entry, crop's left edge. Crops keep
# identifiable buildings/turns instead of blindly centering all twenty images.
TRACKS = [
    (0, 'losail', 'Losail International Circuit', 'base', 0, 100),
    (1, 'motegi', 'Twin Ring Motegi', 'base', 1, 110),
    (2, 'jerez', 'Jerez', 'base', 2, 175),
    (3, 'le_mans', 'Le Mans', 'base', 3, 65),
    (4, 'mugello', 'Mugello', 'base', 4, 100),
    (5, 'catalunya', 'Catalunya', 'base', 5, 100),
    (6, 'assen', 'TT Circuit Assen', 'base', 6, 80),
    (7, 'laguna_seca', 'Laguna Seca', 'base', 7, 60),
    (8, 'sachsenring', 'Sachsenring', 'base', 8, 155),
    (9, 'donington', 'Donington Park', 'base', 9, 100),
    (10, 'brno', 'Brno', 'base', 10, 100),
    (11, 'indianapolis', 'Indianapolis', 'base', 11, 55),
    (12, 'misano', 'Misano', 'base', 12, 70),
    (13, 'estoril', 'Estoril', 'base', 13, 75),
    (14, 'phillip_island', 'Phillip Island', 'base', 14, 100),
    (15, 'sepang', 'Sepang', 'base', 15, 140),
    (16, 'valencia', 'Valencia', 'base', 16, 90),
    (18, 'silverstone', 'Silverstone', 'gp_dlc', 0, 55),
    (19, 'shanghai', 'Shanghai', 'twin_dlc', 0, 55),
    (20, 'istanbul', 'Istanbul Park', 'twin_dlc', 1, 65),
]


def read_bank(path):
    data = path.read_bytes()
    if len(data) < 4:
        raise ValueError(f'Truncated bank: {path}')
    count, = struct.unpack_from('>I', data)
    if count not in (2, 4, 34):
        raise ValueError(f'Expected a decoded frontend track bank: {path}')
    sizes = struct.unpack_from(f'>{count}I', data, 4)
    offset = 4 + count * 4
    if offset + sum(sizes) != len(data):
        raise ValueError(f'Invalid bank lengths: {path}')
    records = []
    for size in sizes:
        records.append(data[offset:offset + size])
        offset += size
    return records


def decode_photo(record):
    from PIL import Image
    if len(record) != 262541 or struct.unpack_from('>HH', record, 28) != (512, 512):
        raise ValueError('Unsupported native track texture layout')
    # Xbox UI BC3 words are swapped, but these linear textures need no tiling
    # transform. Decode the exact native blocks; do not redraw the artwork.
    source = record[32:32 + 512 * 512]
    pixels = bytearray(len(source))
    pixels[::2], pixels[1::2] = source[1::2], source[::2]
    header = (b'DDS ' + struct.pack('<7I', 124, 0x81007, 512, 512, len(pixels), 0, 1)
              + bytes(44) + struct.pack('<II4s5I', 32, 4, b'DXT5', 0, 0, 0, 0, 0)
              + struct.pack('<5I', 0x1000, 0, 0, 0, 0))
    return Image.open(io.BytesIO(header + pixels)).convert('RGB')


def main():
    from PIL import Image, ImageDraw
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--gp-dlc', type=Path, required=True)
    parser.add_argument('--twin-dlc', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Use a new output directory; existing exports are preserved')
    banks = {key: read_bank(getattr(args, key)) for key in ('base', 'gp_dlc', 'twin_dlc')}
    args.output.mkdir(parents=True)
    upload = args.output / 'upload'
    upload.mkdir()
    previews = []
    manifest = {'size': [1024, 1024], 'resampling': 'Lanczos', 'tracks': []}
    for slot, key, title, bank, index, left in TRACKS:
        image = decode_photo(banks[bank][index])
        # Every crop is square in source pixels. This removes the native menu
        # frame/unused lower half while preserving geometric proportions.
        crop = (left, 16, left + 300, 316)
        image = image.crop(crop).resize((1024, 1024), Image.Resampling.LANCZOS)
        path = upload / f'track_{key}.png'
        image.save(path, optimize=True)
        manifest['tracks'].append({'slot': slot, 'key': path.stem, 'title': title,
                                  'source_bank': bank, 'source_entry': index,
                                  'crop_box': crop, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        previews.append((title, image))
    manifest['source_sha256'] = {key: hashlib.sha256(getattr(args, key).read_bytes()).hexdigest()
                                 for key in banks}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    preview = Image.new('RGB', (1000, 4 * 226), '#101216')
    draw = ImageDraw.Draw(preview)
    for index, (title, image) in enumerate(previews):
        x, y = index % 5 * 200, index // 5 * 226
        preview.paste(image.resize((192, 192), Image.Resampling.LANCZOS), (x + 4, y))
        draw.text((x + 4, y + 198), title, fill='white')
    preview.save(args.output / 'preview.png')
    with zipfile.ZipFile(args.output / 'discord-track-assets.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(upload.glob('*.png')):
            archive.write(path, path.name)
    print(f'Exported {len(previews)} square track assets to {args.output}')


if __name__ == '__main__':
    main()
