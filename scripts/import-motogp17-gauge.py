#!/usr/bin/env python3
"""Convert supplied MotoGP 17 PC HUD resources into a self-contained gauge pack.

Requires Pillow. Original images, authored animation geometry and bitmap font
metrics are preserved; this does not recreate the artwork. Raw donor files and
intermediates stay private. The user-approved converted JSON is bundled under
resources/hud/motogp17; see docs/motogp17-gauge.md.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path

from PIL import Image


class Reader:
    def __init__(self, data: bytes):
        self.data, self.at = data, 0

    def take(self, size):
        if size < 0 or self.at + size > len(self.data):
            raise ValueError('Truncated resource')
        result = self.data[self.at:self.at + size]
        self.at += size
        return result

    def number(self, kind):
        return struct.unpack('<' + kind, self.take(struct.calcsize('<' + kind)))[0]

    def string(self):
        return self.take(self.number('H')).rstrip(b'\0').decode('utf-8')


IDENTITY = (1., 0., 0., 1., 0., 0.)
WHITE = ((1., 1., 1., 1.), (0., 0., 0., 0.))


def matrix(reader):
    flags = reader.number('B')
    a, b, c, d, x, y = IDENTITY
    if flags & 0x80:
        a, d = reader.number('f'), reader.number('f')
    if flags & 0x40:
        b, c = reader.number('f'), reader.number('f')
    if flags & 0x20:
        x, y = reader.number('f'), reader.number('f')
    if flags & 0x1f:
        raise ValueError('Unsupported expanded matrix flags')
    return a, b, c, d, x, y


def compose(parent, child):
    a, b, c, d, x, y = parent
    e, f, g, h, u, v = child
    return (a*e+c*f, b*e+d*f, a*g+c*h, b*g+d*h, a*u+c*v+x, b*u+d*v+y)


def point(transform, position):
    a, b, c, d, x, y = transform
    u, v = position
    return a*u+c*v+x, b*u+d*v+y


def inverse(transform):
    a, b, c, d, x, y = transform
    determinant = a*d-b*c
    if abs(determinant) < 1e-10:
        raise ValueError('Singular bitmap transform')
    return d/determinant, -b/determinant, -c/determinant, a/determinant, (c*y-d*x)/determinant, (b*x-a*y)/determinant


def color_transform(reader):
    return tuple(reader.number('h')/256. for _ in range(4)), tuple(reader.number('h')/255. for _ in range(4))


def combine_color(parent, child):
    return (tuple(a*b for a, b in zip(parent[0], child[0])),
            tuple(a*b+c for a, b, c in zip(child[1], parent[0], parent[1])))


def rgba(color, transform):
    return tuple(max(0., min(1., a*b+c)) for a, b, c in zip(color, transform[0], transform[1]))


def tags(data, begin=0):
    reader = Reader(data)
    reader.at = begin
    while reader.at < len(data):
        kind, size = reader.number('H'), reader.number('I')
        yield kind, reader.take(size)


def placement(data, previous=None):
    reader = Reader(data)
    flags, depth = reader.number('B'), reader.number('H')
    result = copy.deepcopy(previous) if flags & 1 and previous else {'matrix': IDENTITY, 'color': WHITE}
    result['depth'] = depth
    if flags & 2:
        result['character'] = reader.number('H')
    if flags & 4:
        result['matrix'] = matrix(reader)
    if flags & 8:
        result['color'] = color_transform(reader)
    if flags & 16:
        result['ratio'] = reader.number('H')
    if flags & 32:
        result['name'] = reader.string()
    if flags & 64:
        result['clip'] = reader.number('H')
    if flags & 128 or reader.at != len(data):
        raise ValueError('Unsupported placement payload')
    return result


class Movie:
    def __init__(self, path):
        self.raw = path.read_bytes()
        if self.raw[:5] != b'ZZ\x02\x00\x08' or struct.unpack_from('<I', self.raw, 5)[0] != len(self.raw):
            raise ValueError('Expected supplied expanded MotoGP 17 MWF format')
        self.shapes, self.sprites, self.imports, self.texts = {}, {}, {}, {}
        for kind, payload in tags(self.raw, 53):
            reader = Reader(payload)
            if kind == 2:
                identity = reader.number('H')
                self.shapes[identity] = payload
            elif kind == 39:
                identity, count = reader.number('H'), reader.number('H')
                display, frames = {}, []
                for child_kind, child_payload in tags(payload, 4):
                    if child_kind == 26:
                        depth = struct.unpack_from('<H', child_payload, 1)[0]
                        display[depth] = placement(child_payload, display.get(depth))
                    elif child_kind == 28:
                        display.pop(struct.unpack('<H', child_payload)[0], None)
                    elif child_kind == 1:
                        frames.append(copy.deepcopy(display))
                if len(frames) != count:
                    raise ValueError('Sprite frame count mismatch')
                self.sprites[identity] = frames
            elif kind == 71:
                reader.string()
                for _ in range(reader.number('H')):
                    identity = reader.number('H')
                    self.imports[identity] = reader.string().removesuffix('.png')
            elif kind == 37:
                identity = reader.number('H')
                bounds = [reader.number('i') for _ in range(4)]
                reader.number('H')
                font, height = reader.string(), reader.number('H')
                color = tuple(reader.number('B')/255. for _ in range(4))
                self.texts[identity] = {'bounds': bounds, 'font': font, 'height': height,
                                        'color': color, 'alignment': reader.number('B')}

    def shape(self, identity):
        reader = Reader(self.shapes[identity])
        assert reader.number('H') == identity
        bounds = tuple(reader.number('i') for _ in range(4))
        fills = []
        for _ in range(reader.number('H')):
            kind = reader.number('B')
            if kind == 0:
                fills.append({'color': tuple(reader.number('B')/255. for _ in range(4))})
            elif kind in (0x40, 0x41, 0x42, 0x43):
                bitmap = reader.number('H')
                # Some source shapes retain an unused reserved FFFF style
                # before the actual imported bitmap; resolve only used fills.
                fills.append({'bitmap': self.imports.get(bitmap, f'!unresolved:{bitmap}'), 'matrix': matrix(reader)})
            else:
                raise ValueError(f'Unsupported shape fill {kind}')
        lines = reader.number('H')
        for _ in range(lines):
            reader.take(6)
        count = reader.number('H')
        paths, current, position, fill = [], [], (0., 0.), None

        def finish():
            nonlocal current
            if current and fill is not None:
                if abs(current[-1][0]-current[0][0])+abs(current[-1][1]-current[0][1]) < .01:
                    current.pop()
                if len(current) >= 3:
                    paths.append((fills[fill], current))
            current = []

        for _ in range(count):
            flags = reader.number('B')
            if not flags:
                finish()
                break
            if flags & 0x80:
                if not current:
                    current = [position]
                if flags & 0x40:
                    dx = reader.number('f') if flags & 0x20 or not flags & 0x10 else 0.
                    dy = reader.number('f') if flags & 0x20 or flags & 0x10 else 0.
                    position = position[0]+dx, position[1]+dy
                    current.append(position)
                else:
                    dx, dy, ax, ay = (reader.number('f') for _ in range(4))
                    start = position
                    control = start[0]+dx, start[1]+dy
                    position = control[0]+ax, control[1]+ay
                    for step in range(1, 13):
                        t = step/12.
                        current.append(((1-t)**2*start[0]+2*(1-t)*t*control[0]+t*t*position[0],
                                        (1-t)**2*start[1]+2*(1-t)*t*control[1]+t*t*position[1]))
            else:
                if flags & 4:
                    finish()
                    position = reader.number('f'), reader.number('f')
                if flags & 8:
                    value = reader.number('H')
                    fill = value-1 if value else None
                if flags & 16:
                    value = reader.number('H')
                    fill = value-1 if value else None
                if flags & 32:
                    reader.number('H')
                if flags & 64:
                    raise ValueError('Unsupported in-record fill table')
        if reader.at != len(reader.data):
            raise ValueError(f'Shape {identity} has unparsed data')
        return bounds, paths


def cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def area(polygon):
    return sum(a[0]*b[1]-b[0]*a[1] for a, b in zip(polygon, polygon[1:]+polygon[:1]))*.5


def clip(polygon, boundary):
    orientation = 1 if area(boundary) >= 0 else -1
    for a, b in zip(boundary, boundary[1:]+boundary[:1]):
        incoming, polygon = polygon, []
        if not incoming:
            break
        previous = incoming[-1]
        before = cross(a, b, previous)*orientation
        for current in incoming:
            after = cross(a, b, current)*orientation
            if (after >= -1e-7) != (before >= -1e-7):
                t = before/(before-after)
                polygon.append(tuple(x+(y-x)*t for x, y in zip(previous, current)))
            if after >= -1e-7:
                polygon.append(current)
            previous, before = current, after
    return polygon


def triangles(polygon):
    vertices = list(range(len(polygon)))
    direction = 1 if area(polygon) >= 0 else -1
    result = []
    while len(vertices) > 3:
        for i, middle in enumerate(vertices):
            first, last = vertices[i-1], vertices[(i+1) % len(vertices)]
            if cross(polygon[first], polygon[middle], polygon[last])*direction <= 1e-7:
                continue
            if any(all(cross(polygon[a], polygon[b], polygon[v])*direction >= -1e-7
                       for a, b in ((first, middle), (middle, last), (last, first)))
                   for v in vertices if v not in (first, middle, last)):
                continue
            result.extend((first, middle, last))
            vertices.pop(i)
            break
        else:
            raise ValueError('Unsupported self-intersecting shape')
    if len(vertices) == 3:
        result.extend(vertices)
    return result


def bml(path):
    raw = path.read_bytes()

    def word(at):
        return struct.unpack_from('<I', raw, at)[0]

    def text(at, size):
        if size % 2 or at+size > len(raw):
            raise ValueError('Invalid BML string')
        return raw[at:at+size].decode('utf-16le').rstrip('\0')

    def node(at):
        end = at+word(at+8)
        if end > len(raw) or word(at+4) != 36:
            raise ValueError('Invalid BML node')
        name = text(at+40, word(at+36))
        cursor = at+40+word(at+36)
        child_count, child_relative = word(at+28), word(at+32)
        attributes = {}
        while cursor < (at+child_relative if child_count else end):
            length, key_size = word(cursor), word(cursor+8)
            if length < 16 or cursor+length > end:
                raise ValueError('Invalid BML attribute')
            key = text(cursor+12, key_size)
            value_size = word(cursor+12+key_size)
            attributes[key] = text(cursor+16+key_size, value_size)
            cursor += length
        children = []
        if child_count:
            cursor = at+child_relative
            for _ in range(child_count):
                child, cursor = node(cursor)
                children.append(child)
        if cursor != end:
            raise ValueError('Unparsed BML node bytes')
        return {'name': name, 'attributes': attributes, 'children': children}, end

    result, end = node(word(4))
    if end != len(raw):
        raise ValueError('Unexpected BML trailing data')
    return result


class Exporter:
    def __init__(self, movie, gfxpak, output):
        self.movie, self.output = movie, output
        self.texture_files, self.bitmaps, self.sources = [], {}, []
        library = gfxpak/'GFX_RACE/GFX_RACE.LIB'
        atlas_path = library.with_name('GFX_RACE00.DDS')
        atlas = Image.open(atlas_path).convert('RGBA')
        self.source(library)
        self.source(atlas_path)
        for section in library.read_text().split('EndStruct'):
            fields = {}
            for line in section.splitlines():
                parts = line.split()
                if parts:
                    fields[parts[0]] = parts[1:]
            if 'szSprite' not in fields:
                continue
            name = fields['szSprite'][0]
            x0, x1 = map(int, fields['aiX'])
            y0, y1 = map(int, fields['aiY'])
            if not (0 <= x0 < x1 <= atlas.width and 0 <= y0 < y1 <= atlas.height):
                raise ValueError('Atlas sprite outside texture')
            image = atlas.crop((x0, y0, x1, y1))
            if fields['bRot'] == ['1']:
                image = image.transpose(Image.Transpose.ROTATE_90)
            self.bitmaps[name] = image
        self.textures = {}

    def source(self, path):
        self.sources.append({'name': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})

    def texture(self, name, transform=WHITE):
        key = name, transform
        if key not in self.textures:
            image = self.bitmaps[name]
            # Multiplication normally stays in the vertex color. Additive or
            # overbright authored transforms require exact image variants.
            if any(transform[1]) or any(value > 1 or value < 0 for value in transform[0]):
                channels = image.split()
                image = Image.merge('RGBA', tuple(channel.point([round(max(0., min(255., value*mul+add*255)))
                    for value in range(256)]) for channel, mul, add in zip(channels, *transform)))
                vertex_color = (1., 1., 1., 1.)
            else:
                vertex_color = transform[0]
            index = len(self.texture_files)
            filename = f'texture-{index:02}.png'
            image.save(self.output/filename)
            self.texture_files.append(filename)
            self.textures[key] = (index, image.size, vertex_color)
        return self.textures[key]

    def mask(self, identity, transform):
        if identity in self.movie.sprites:
            children = list(self.movie.sprites[identity][0].values())
            if len(children) != 1:
                raise ValueError('Unsupported compound mask')
            child = children[0]
            return self.mask(child['character'], compose(transform, child['matrix']))
        _, paths = self.movie.shape(identity)
        if len(paths) != 1:
            raise ValueError('Unsupported multi-path mask')
        polygon = [point(transform, p) for p in paths[0][1]]
        return polygon

    def flatten(self, identity, frame=0, transform=IDENTITY, color=WHITE, masks=(), text_role=None):
        if identity in self.movie.sprites:
            display = self.movie.sprites[identity][frame]
            clips, result = [], []
            for depth, child in sorted(display.items()):
                clips = [(end, polygon) for end, polygon in clips if end >= depth]
                local = compose(transform, child['matrix'])
                combined = combine_color(color, child['color'])
                if 'clip' in child:
                    clips.append((child['clip'], self.mask(child['character'], local)))
                else:
                    result.extend(self.flatten(child['character'], transform=local, color=combined,
                        masks=masks+tuple(p for _, p in clips), text_role=text_role))
            return result
        if identity in self.movie.texts:
            return [{'text': self.movie.texts[identity], 'matrix': transform, 'color_transform': color,
                     'role': text_role or 'speed'}]
        _, paths = self.movie.shape(identity)
        result = []
        for fill, polygon in paths:
            if 'bitmap' in fill:
                texture, size, tint = self.texture(fill['bitmap'], color)
                sampling = inverse(fill['matrix'])
                vertices = [(*point(transform, p), point(sampling, p)[0]/size[0], point(sampling, p)[1]/size[1], *tint) for p in polygon]
            else:
                texture = -1
                vertices = [(*point(transform, p), 0., 0., *rgba(fill['color'], color)) for p in polygon]
            pieces = [vertices]
            for boundary in masks:
                # Several authored RPM masks are concave eight-point shapes.
                # Triangulate their exact outline and intersect each disjoint
                # triangle, rather than replacing the outline by a convex hull.
                convex = all(cross(boundary[i-1], boundary[i], boundary[(i+1) % len(boundary)])*area(boundary) >= -1e-5
                             for i in range(len(boundary)))
                indices = triangles(boundary) if not convex else []
                regions = [boundary] if convex else [[boundary[v] for v in indices[i:i+3]] for i in range(0, len(indices), 3)]
                pieces = [intersection for piece in pieces for region in regions
                          if len(intersection := clip(piece, region)) >= 3 and abs(area(intersection)) > 1e-5]
            for piece in pieces:
                if len(piece) >= 3 and abs(area(piece)) > 1e-5:
                    result.append({'texture': texture, 'vertices': piece, 'indices': triangles(piece)})
        return result

    def component(self, root, name, count=1):
        child = next(c for c in root.values() if c.get('name') == name)
        return [self.flatten(child['character'], frame, child['matrix'], child['color']) for frame in range(count)]

    def tyre_status(self, root):
        # Original donor panel: damage body, wear and temperatures are separate
        # named components. Export only the two tyre silhouettes and their
        # original bitmap backing; do not activate unrelated damage/temp data.
        damage = next(c for c in root.values() if c.get('name') == 'MOV_Damage')
        children = self.movie.sprites[damage['character']][0]
        wear = next(c for c in children.values() if c.get('name') == 'MOV_WearTyre')
        tyres = self.movie.sprites[wear['character']][0]
        transform = compose(damage['matrix'], wear['matrix'])
        color = combine_color(damage['color'], wear['color'])
        result = {}
        for key, name in [('front', 'MOV_TyreFront'), ('rear', 'MOV_TyreRear')]:
            child = next(c for c in tyres.values() if c.get('name') == name)
            result[key] = self.flatten(child['character'],
                transform=compose(transform, child['matrix']),
                color=combine_color(color, child['color']))
            vertices = [v for mesh in result[key] for v in mesh['vertices']]
            result[key+'_bounds'] = [min(v[0] for v in vertices), min(v[1] for v in vertices),
                                     max(v[0] for v in vertices), max(v[1] for v in vertices)]
        # The authored panel bitmap is above the filled silhouettes in source
        # depth order, preserving the wheel outlines at every remaining value.
        backing = children[7]
        result['background'] = self.flatten(backing['character'],
            transform=compose(damage['matrix'], backing['matrix']),
            color=combine_color(damage['color'], backing['color']))
        # The source body rectangle sits beneath GaugeDamage, whose alpha
        # cutout supplies the motorcycle silhouette. Use a fixed neutral gray;
        # this is decoration, not an invented body-damage reading.
        body = next(c for c in children.values() if c.get('name') == 'MOV_DamageBike')
        result['neutral_body'] = self.flatten(body['character'],
            transform=compose(damage['matrix'], body['matrix']))
        for mesh in result['neutral_body']:
            mesh['vertices'] = [list(vertex[:4])+[128/255., 128/255., 128/255., 1.]
                                for vertex in mesh['vertices']]
        # Temperature is unavailable. Cover both baked thermometer cutouts and
        # their bright edges with the bitmap's opaque 60/255 panel gray.
        # Coordinates are local to the original 160x110 GaugeDamage bitmap.
        result['temperature_covers'] = []
        for x0, y0, x1, y1 in [(23, 52, 35, 75), (93, 52, 107, 75)]:
            vertices = [(*point(damage['matrix'], p), 0., 0., 60/255., 60/255., 60/255., 1.)
                        for p in [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]]
            result['temperature_covers'].append({'texture': -1, 'vertices': vertices,
                                                  'indices': [0, 1, 2, 0, 2, 3]})
        result['draw_order'] = ['front', 'rear', 'background']
        result['source'] = {'panel': 'MOV_Damage', 'wear': 'MOV_WearTyre',
                            'front': 'MOV_TyreFront', 'rear': 'MOV_TyreRear',
                            'remaining_axis': 'bottom-up in each exported bounds rectangle',
                            'mapping': 'Host title supplies verified tyre remaining/grip telemetry; no donor simulation or temperature is inferred.'}
        return result

    def font(self, menu, name):
        path = menu/'FONT'/f'{name.upper().replace(" ", "_")}.BML'
        tree = bml(path)
        info = next(n['attributes'] for n in tree['children'] if n['name'] == 'info')
        common = next(n['attributes'] for n in tree['children'] if n['name'] == 'common')
        chars = next(n for n in tree['children'] if n['name'] == 'chars')
        image_path = path.with_name(path.stem+'_0.DDS')
        image = Image.open(image_path).convert('RGBA')
        self.source(path)
        self.source(image_path)
        bitmap_name = 'font-'+path.stem
        self.bitmaps[bitmap_name] = image
        texture, size, _ = self.texture(bitmap_name)
        if size != (int(common['scaleW']), int(common['scaleH'])):
            raise ValueError('Font dimensions disagree with metrics')
        glyphs = {c['attributes']['id']: {key: int(value) for key, value in c['attributes'].items()}
                  for c in chars['children']}
        for char in '0123456789N':
            if str(ord(char)) not in glyphs:
                raise ValueError(f'Missing glyph {char} in {name}')
        return {'texture': texture, 'width': size[0], 'height': size[1], 'size': abs(int(info['size'])),
                'line_height': int(common['lineHeight']), 'base': int(common['base']), 'glyphs': glyphs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--menu-dir', type=Path, required=True)
    parser.add_argument('--gfxpak-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output must be a new directory; existing original data is preserved.')
    movie_path = args.menu_dir/'HUD_GAUGE.MWF'
    movie = Movie(movie_path)
    if (len(movie.sprites[78]) != 68 or len(movie.sprites[93]) != 8 or
            len(movie.sprites[111]) != 17 or len(movie.sprites[128]) != 17):
        raise ValueError('Unexpected source gauge animation')
    args.output.mkdir(parents=True)
    exporter = Exporter(movie, args.gfxpak_dir, args.output)
    exporter.source(movie_path)
    root = movie.sprites[166][0]
    data = {'schema': 1, 'canvas': [1920, 1080], 'anchor': [1519, 727], 'bounds': [280, 280],
            'rpm_frames': exporter.component(root, 'PROGRESSBAR', 68),
            'gear_frames': exporter.component(root, 'PROGRESSBAR_GEARS', 8),
            'brake_frames': exporter.component(root, 'PROGRESSBAR_Brake', 17),
            'throttle_frames': exporter.component(root, 'PROGRESSBAR_Gas', 17),
            'units_kmh': exporter.component(root, 'MOV_Kmh')[0],
            'units_mph': exporter.component(root, 'MOV_mph')[0],
            'speed': exporter.flatten(144, transform=root[43]['matrix']),
            'gear_labels': [],
            'static': []}
    # Authored current/adjacent gear typography. The item names identify its
    # offset in the five-item list; values come from the actual current gear.
    gear_root = next(c for c in root.values() if c.get('name') == 'LIST_Gears')
    for child in movie.sprites[gear_root['character']][0].values():
        if child.get('name', '').startswith('ITEM_'):
            delta = int(child['name'][-2:])-2
            data['gear_labels'].extend(exporter.flatten(child['character'],
                transform=compose(gear_root['matrix'], child['matrix']), text_role=f'gear:{delta}'))
    data['fonts'] = {name: exporter.font(args.menu_dir, name)
                     for name in ['Geogrotesque Regular61', 'Geogrotesque Light40']}
    data['tyre_status'] = exporter.tyre_status(root)
    data['textures'] = exporter.texture_files
    # The approved bundled runtime JSON embeds all original texture variants.
    data['embedded_textures'] = {name: (args.output/name).read_bytes().hex() for name in exporter.texture_files}
    data['sources'] = exporter.sources
    data['limits'] = ['Donor body-damage and temperature telemetry are unavailable; their components remain inactive.',
                      'Donor limiter light is not activated without verified telemetry.']
    (args.output/'gauge.json').write_text(json.dumps(data, separators=(',', ':'), allow_nan=False)+'\n')
    print(f'Imported original gauge: {len(data["rpm_frames"])} RPM frames, '
          f'{len(data["gear_frames"])} gear frames, {len(data["textures"])} textures -> {args.output}')


if __name__ == '__main__':
    main()
