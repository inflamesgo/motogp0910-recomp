#!/usr/bin/env python3
"""Pack the reviewed URT3 face and verified shared high-resolution sprites.

The original URT3/URT2 needle pixels are identical, and digital glyph metrics
and binary masks match. Pillow inspects; ImageMagick packs the approved face.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base-pack', 'shared-pack', 'face', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    original = json.loads(args.base_pack.read_text())
    shared = json.loads(args.shared_pack.read_text())
    if original.get('style') != 'motogpurt3' or 'restoration' in original:
        raise ValueError('Expected unrestored URT3 source pack')
    if shared.get('style') != 'motogpurt2' or 'restoration' not in shared:
        raise ValueError('Expected reviewed high-resolution URT2 pack')
    result = json.loads(json.dumps(original))
    for a, b in zip(result['digits'], shared['digits'], strict=True):
        if a['rect'] != b['rect'] or a['advance'] != b['advance']:
            raise ValueError('Shared digit geometry differs')
        a['source'] = b['source']
    result['textures'][1] = shared['textures'][1]
    result['needle']['source'] = shared['needle']['source']
    result['textures'][2] = shared['textures'][3]
    with tempfile.TemporaryDirectory(prefix='urt3-restoration-') as temp:
        work = Path(temp)
        original_face = work/'original.png'
        original_face.write_bytes(bytes.fromhex(original['textures'][0]))
        cutout = work/'cutout.png'
        # Flood only the connected exterior. The closed white silhouette
        # protects every approved interior pixel; do not regenerate the face.
        magick(args.face, '-alpha', 'set', '-fuzz', '40%', '-fill', 'none',
               '-draw', 'alpha 0,0 floodfill', cutout)
        with Image.open(cutout) as image:
            bounds = image.getchannel('A').getbbox()
        if not bounds:
            raise ValueError('No face silhouette')
        x0, y0, x1, y1 = bounds
        aligned = work/'aligned.png'
        # Original visible alpha bounds are (4,1)..(125,128) on 128x128.
        magick(cutout, '-crop', f'{x1-x0}x{y1-y0}+{x0}+{y0}', '+repage',
               '-filter', 'Lanczos', '-resize', '968x1016!', '-background', 'none',
               '-gravity', 'northwest', '-splice', '32x8', '-extent', '1024x1024', aligned)
        background_alpha = work/'background-alpha.png'
        # Strip old white-ink opacity from the source background field. The
        # reviewed high-resolution marks supply their own new opacity.
        magick(original_face, '-fx',
               'u.a==0?0:(min(u.r,min(u.g,u.b))>.35?.619607843:u.a)',
               '-alpha', 'off', '-filter', 'Lanczos', '-resize', '1024x1024!', background_alpha)
        alpha = work/'alpha.png'
        magick(aligned, background_alpha, '-fx',
               'u.a*(v+(1-v)*min(1,max(0,(max(u.r,max(u.g,u.b))-.012)/.6)))', alpha)
        face = work/'face.png'
        magick(aligned, alpha, '-alpha', 'off', '-compose', 'CopyOpacity', '-composite', face)
        result['textures'][0] = face.read_bytes().hex()
        result['static'][0]['source'] = [0, 0, 1024, 1024]
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    result['restoration'] = {
        'version': 1, 'approved_face_sha256': digest(args.face),
        'base_pack_sha256': digest(args.base_pack), 'shared_pack_sha256': digest(args.shared_pack),
        'texture_sizes': [[1024, 1024], [64, 512], [1160, 512]],
        'method': 'Reviewed face; original logical geometry and background opacity; verified shared needle and glyph restoration.',
        'note': 'Visual restoration, not pixel-identical donor artwork. Existing 16/18 labels remain.'
    }
    encoded = json.dumps(result, separators=(',', ':'))+'\n'
    if len(encoded.encode()) > 4*1024*1024:
        raise ValueError('Pack exceeds production loader limit')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(encoded)
    print(f'Wrote {args.output} ({args.output.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
