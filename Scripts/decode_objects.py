"""Decode extracted Warcraft III object modifications, without engine defaults."""
import json
import struct
from pathlib import Path

from inspect_map import MPQ


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, size):
        result = self.data[self.pos:self.pos + size]
        if len(result) != size:
            raise ValueError('Unexpected end of file')
        self.pos += size
        return result

    def integer(self):
        return struct.unpack('<i', self.take(4))[0]

    def text(self):
        end = self.data.index(0, self.pos)
        raw = self.take(end - self.pos + 1)[:-1]
        try:
            return raw.decode('utf-8')
        except UnicodeDecodeError:
            return raw.decode('gb18030', errors='replace')

    def id(self):
        return self.take(4).decode('latin1')


def parse(data, leveled):
    r = Reader(data)
    version = r.integer()
    if version not in (1, 2):
        raise ValueError(f'Unsupported object version: {version}')
    tables = {}
    for group in ('original', 'custom'):
        objects = []
        for _ in range(r.integer()):
            obj = {'base_id': r.id(), 'new_id': r.id(), 'modifications': []}
            for _ in range(r.integer()):
                mod = {'field': r.id(), 'type': r.integer()}
                if leveled:
                    mod.update(level=r.integer(), data_column=r.integer())
                kind = mod['type']
                if kind == 0:
                    mod['value'] = r.integer()
                elif kind in (1, 2):
                    mod['value'] = struct.unpack('<f', r.take(4))[0]
                elif kind == 3:
                    mod['value'] = r.text()
                else:
                    raise ValueError(f'Unknown value type: {kind}')
                mod['end_id'] = r.id()
                obj['modifications'].append(mod)
            objects.append(obj)
        tables[group] = objects
    if r.pos != len(data):
        raise ValueError(f'{len(data) - r.pos} unparsed bytes')
    return {'version': version, **tables}


def main():
    root = Path(__file__).resolve().parents[1]
    source = next(root.glob('*.w3x'))
    archive = MPQ(source)
    output = root / 'Analysis' / source.stem
    for ext in ('w3a', 'w3u', 'w3t', 'w3q', 'w3h', 'w3b'):
        name = 'war3map.' + ext
        result = parse(archive.read(name), ext in ('w3a', 'w3q'))
        (output / (name + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        print(name, {k: len(result[k]) for k in ('original', 'custom')})
        if ext == 'w3a':
            names = [m['value'] for group in ('original', 'custom') for obj in result[group] for m in obj['modifications'] if m['field'] == 'anam']
            print('Ability name sample:', json.dumps(names[:25], ensure_ascii=False))
    script = archive.read('Scripts\\war3map.j').decode('utf-8')
    normalized = script.replace('\r\n', '\n').replace('\r', '\n')
    (output / 'Scripts' / 'war3map.readable.j').write_text(normalized, encoding='utf-8')
    print('Script lines:', len(normalized.splitlines()))


if __name__ == '__main__':
    main()
