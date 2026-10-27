#!/usr/bin/env python3
"""Pack reviewed URT2 artwork at unchanged original logical coordinates.

Pillow inspects inputs; ImageMagick converts and packs reviewed image assets.
The original 06 and URT2 digit masks have identical thresholded shapes and
metrics; the reviewed 06 digit pack supplies the shared high-resolution glyphs.
"""
import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image


def magick(*args):
    subprocess.run(['magick', *(str(x) for x in args)], check=True)


def crop(path, channel=None, threshold=16):
    with Image.open(path) as image:
        mask = image.getchannel(channel) if channel else image.convert('L')
        bounds = mask.point(lambda x: 255 if x > threshold else 0).getbbox()
    if not bounds:
        raise ValueError(f'No ink in {path}')
    x0, y0, x1, y1 = bounds
    return f'{x1-x0}x{y1-y0}+{x0}+{y0}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base-pack', 'digit-pack', 'face', 'needle', 'light', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    original = json.loads(args.base_pack.read_text())
    digit_pack = json.loads(args.digit_pack.read_text())
    if original.get('style') != 'motogpurt2' or 'restoration' in original:
        raise ValueError('Expected an unrestored URT2 base pack')
    if digit_pack.get('style') != 'motogp06' or 'restoration' not in digit_pack:
        raise ValueError('Expected the reviewed high-resolution 06 digit pack')
    result = json.loads(json.dumps(original))
    for old, new in zip(original['digits'], digit_pack['digits'], strict=True):
        if old['rect'] != new['rect'] or old['advance'] != new['advance']:
            raise ValueError('Shared glyph metrics differ')

    with tempfile.TemporaryDirectory(prefix='urt2-restoration-') as temp:
        work = Path(temp)
        source_face = work/'original-face.png'
        source_face.write_bytes(bytes.fromhex(original['textures'][0]))
        face_rgb, face_alpha, face = [work/(name+'.png') for name in ('face-rgb', 'face-alpha', 'face')]
        magick(args.face, '-crop', crop(args.face, threshold=32), '+repage',
               '-filter', 'Lanczos', '-resize', '808x808!', face_rgb)
        magick(source_face, '-alpha', 'extract', '-evaluate', 'Min', '59.215686275%',
               '-filter', 'Lanczos', '-resize', '808x808!', face_alpha)
        alpha = work/'combined-alpha.png'
        # The high-resolution marks provide their own opacity; the original
        # translucent interior field remains visible between them.
        magick(face_rgb, face_alpha, '-fx',
               'v+(1-v)*min(1,max(0,(max(u.r,max(u.g,u.b))-0.012)/0.12))', alpha)
        magick(face_rgb, alpha, '-alpha', 'off', '-compose', 'CopyOpacity', '-composite', face)
        result['textures'][0] = face.read_bytes().hex()
        result['static'][0]['source'] = [0, 0, 808, 808]

        needle = work/'needle.png'
        magick(args.needle, '-crop', crop(args.needle, 'R', 16), '+repage',
               '-channel', 'R', '-separate', '+channel', '-filter', 'Lanczos',
               '-resize', '64x512!', '-alpha', 'Copy', '-channel', 'RGB',
               '-fill', 'rgb(206,4,0)', '-colorize', '100', '+channel', needle)
        result['textures'][1] = needle.read_bytes().hex()
        result['needle']['source'] = [0, 0, 64, 512]

        light = work/'light.png'
        magick(args.light, '-crop', crop(args.light, threshold=2), '+repage',
               '-colorspace', 'Gray', '-filter', 'Lanczos', '-resize', '96x96!',
               '-alpha', 'Copy', '-channel', 'RGB', '-fill', 'white', '-colorize', '100',
               '+channel', '-background', 'none', '-gravity', 'northwest',
               '-extent', '128x128', light)
        result['textures'][2] = light.read_bytes().hex()
        for item in result['lights']:
            item['sprite']['source'] = [0, 0, 128, 128]

        result['textures'][3] = digit_pack['textures'][2]
        for old, new in zip(result['digits'], digit_pack['digits'], strict=True):
            old['source'] = new['source']

    # Older private base packs contain absolute donor thresholds. Migrate once
    # so repacking reviewed artwork cannot restore the early shift warning.
    for item in result['lights']:
        if not item.get('normalized_rpm', False):
            item['threshold'] = item['threshold'] / 12000 * 0.775
            item['normalized_rpm'] = True
    result['origin']['light_thresholds'] = (
        'Normalized to current bike maximum; fifth light at 0.775 matches '
        'red-zone entry, retaining donor relative spacing and frame-based blink.')

    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    result['restoration'] = {
        'version': 1,
        'method': 'Reviewed generated artwork with original logical geometry, telemetry and color settings.',
        'inputs': {name: digest(getattr(args, name))
                   for name in ('base_pack', 'digit_pack', 'face', 'needle', 'light')},
        'texture_sizes': [[808, 808], [64, 512], [128, 128], [1160, 512]],
        'shared_font': 'MotoGP06 and URT2 original glyph metrics and binary masks match; original alpha encoding differs.',
        'note': 'Visual restoration, not pixel-identical donor artwork.'
    }
    encoded = json.dumps(result, separators=(',', ':'))+'\n'
    if len(encoded.encode()) > 4*1024*1024:
        raise ValueError('Pack exceeds production loader limit')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(encoded)
    print(f'Wrote {args.output} ({args.output.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
