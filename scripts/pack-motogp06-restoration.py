#!/usr/bin/env python3
"""Pack reviewed MotoGP 06 restoration images without changing HUD geometry.

Requires Pillow (inspection only) and ImageMagick 7 (texture packing). The three
artwork inputs are reviewed restoration candidates, not original donor data.
The original pack supplies logical geometry and the original opacity field.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image


def magick(*args: object) -> None:
    subprocess.run(['magick', *(str(arg) for arg in args)], check=True)


def ink_bounds(path: Path, threshold: int = 96) -> tuple[int, int, int, int]:
    with Image.open(path) as image:
        bounds = image.convert('L').point(lambda x: 255 if x > threshold else 0).getbbox()
    if not bounds:
        raise ValueError(f'No usable ink in {path}')
    return bounds


def crop_string(bounds: tuple[int, int, int, int]) -> str:
    x0, y0, x1, y1 = bounds
    return f'{x1-x0}x{y1-y0}+{x0}+{y0}'


def pack(args: argparse.Namespace, directory: Path) -> dict:
    original = json.loads(args.base_pack.read_text())
    if original.get('style') != 'motogp06' or 'restoration' in original:
        raise ValueError('Supply an original, unrestored MotoGP 06 pack')
    pack = json.loads(json.dumps(original))
    old_face = directory / 'original-face.png'
    old_face.write_bytes(bytes.fromhex(original['textures'][0]))

    # Align the reviewed art to the same logical visible bounds. This preserves
    # the unchanged native pivot, digital readout centers and HUD placement.
    with Image.open(args.face) as image:
        bounds = image.getchannel('A').point(lambda x: 255 if x >= 16 else 0).getbbox()
    if not bounds:
        raise ValueError('Face has no alpha coverage')
    aligned = directory / 'aligned-face.png'
    magick(args.face, '-crop', crop_string(bounds), '+repage', '-resize', '784x784!',
           '-background', 'none', '-gravity', 'northwest', '-splice', '128x120',
           '-gravity', 'northwest', '-extent', '1024x1024', aligned)
    base_alpha = directory / 'base-alpha.png'
    # The donor's black interior is translucent. Clamp source ink out of the
    # background field; the restored art supplies the new opaque printed marks.
    magick(old_face, '-alpha', 'extract', '-evaluate', 'Min', '41.176470588%',
           '-filter', 'Lanczos', '-resize', '1024x1024!', base_alpha)
    alpha = directory / 'face-alpha.png'
    magick(aligned, base_alpha, '-fx',
           'u.a*(v+(1-v)*min(1,max(0,(max(u.r,max(u.g,u.b))-0.012)/0.12)))', alpha)
    face = directory / 'face.png'
    magick(aligned, alpha, '-alpha', 'off', '-compose', 'CopyOpacity', '-composite', face)
    pack['textures'][0] = face.read_bytes().hex()
    pack['static'][0]['source'] = [0, 0, 1024, 1024]

    # Segment the reviewed opaque presentation strip into ten glyphs. Convert
    # white intensity to alpha, then repack at 4x each original crop's size.
    with Image.open(args.digits) as image:
        luminance = image.convert('L')
        width, height = image.size
        columns = [luminance.crop((x, 0, x+1, height)).getextrema()[1] for x in range(width)]
    runs, start = [], None
    for x, value in enumerate(columns + [0]):
        if value > 96 and start is None:
            start = x
        elif value <= 96 and start is not None:
            if x-start >= 8:
                runs.append((start, x))
            start = None
    if len(runs) != 10:
        raise ValueError(f'Expected ten isolated numeral runs, found {len(runs)}')
    atlas = directory / 'digits.png'
    magick('-size', '1160x512', 'xc:none', atlas)
    for index, (left, right) in enumerate(runs):
        selected = directory / f'digit-{index}-selected.png'
        magick(args.digits, '-crop', f'{right-left}x{height}+{left}+0', '+repage', selected)
        b = ink_bounds(selected)
        # Retain a small antialias margin on the reviewed generated glyph.
        b = (0, max(0, b[1]-2), right-left, min(height, b[3]+2))
        sprite = pack['digits'][index]
        old = original['digits'][index]['source']
        w, h = (old[2]-old[0])*4, (old[3]-old[1])*4
        glyph = directory / f'digit-{index}.png'
        magick(selected, '-crop', crop_string(b), '+repage', '-colorspace', 'Gray',
               '-filter', 'Lanczos', '-resize', f'{w}x{h}!', '-alpha', 'Copy',
               '-channel', 'RGB', '-fill', 'white', '-colorize', '100', '+channel', glyph)
        x, y = (index % 5)*232+8, (index // 5)*256+8
        magick(atlas, glyph, '-geometry', f'+{x}+{y}', '-compose', 'Over', '-composite', atlas)
        sprite['source'] = [x, y, x+w, y+h]
    pack['textures'][2] = atlas.read_bytes().hex()

    # The approved needle preview is already stretched. Normalize its ink into
    # the original sprite's upper-left 12x12 coverage, at 4x resolution. Its
    # geometry and pivot remain those of the original 16x16 texture's quad.
    needle = directory / 'needle.png'
    magick(args.needle, '-crop', crop_string(ink_bounds(args.needle, 8)), '+repage',
           '-colorspace', 'Gray', '-filter', 'Lanczos', '-resize', '48x48!',
           '-alpha', 'Copy', '-channel', 'RGB', '-fill', 'white', '-colorize', '100',
           '+channel', '-background', 'none', '-gravity', 'northwest',
           '-extent', '64x64', needle)
    pack['textures'][1] = needle.read_bytes().hex()
    pack['needle']['source'] = [0, 0, 64, 64]
    pack['restoration'] = {
        'version': 1,
        'method': 'Reviewed generated artwork; original logical geometry and translucent background field retained.',
        'base_pack_sha256': hashlib.sha256(args.base_pack.read_bytes()).hexdigest(),
        'inputs': {name: hashlib.sha256(getattr(args, name).read_bytes()).hexdigest()
                   for name in ('face', 'digits', 'needle')},
        'texture_sizes': [[1024, 1024], [64, 64], [1160, 512]],
        'note': 'A visual restoration, not a pixel-exact reproduction of original sprite pixels.'
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(pack, separators=(',', ':')) + '\n'
    if len(encoded.encode()) > 4*1024*1024:
        raise ValueError('Pack exceeds the production loader size limit')
    args.output.write_text(encoded)
    return pack


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base-pack', 'face', 'digits', 'needle', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--export-textures', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='motogp06-restoration-') as temp:
        directory = Path(temp)
        pack(args, directory)
        if args.export_textures:
            args.export_textures.mkdir(parents=True, exist_ok=True)
            for name in ('face', 'digits', 'needle'):
                (args.export_textures / f'{name}.png').write_bytes((directory / f'{name}.png').read_bytes())
    print(f'Wrote {args.output} ({args.output.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
