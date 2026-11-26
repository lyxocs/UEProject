"""Read-only MPQ inspection; extracted files go to a separate analysis directory."""
import bz2
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / 'vendor'))


def explode(data):
    import dclimplode
    return dclimplode.decompressobj().decompress(data)


MASK = 0xFFFFFFFF
CRYPT = [0] * 0x500
seed = 0x100001
for low in range(256):
    for high in range(5):
        seed = (seed * 125 + 3) % 0x2AAAAB
        a = (seed & 0xFFFF) << 16
        seed = (seed * 125 + 3) % 0x2AAAAB
        CRYPT[low + high * 256] = a | (seed & 0xFFFF)


def hash_name(name, kind):
    s1, s2 = 0x7FED7FED, 0xEEEEEEEE
    for c in name.replace('/', '\\').upper().encode('utf-8'):
        s1 = (CRYPT[(kind << 8) + c] ^ (s1 + s2)) & MASK
        s2 = (c + s1 + s2 + (s2 << 5) + 3) & MASK
    return s1


def decrypt(data, key):
    out = bytearray(data)
    s = 0xEEEEEEEE
    for pos in range(0, len(data) - 3, 4):
        s = (s + CRYPT[0x400 + (key & 255)]) & MASK
        value = struct.unpack_from('<I', data, pos)[0] ^ ((key + s) & MASK)
        struct.pack_into('<I', out, pos, value)
        key = (((~key << 21) + 0x11111111) | (key >> 11)) & MASK
        s = (value + s + (s << 5) + 3) & MASK
    return bytes(out)


class MPQ:
    def __init__(self, path):
        self.data = path.read_bytes()
        self.base = self.data.index(b'MPQ\x1a')
        h = struct.unpack_from('<4sIIHHIIII', self.data, self.base)
        _, self.header_size, self.archive_size, self.version, shift, hp, bp, hn, bn = h
        self.sector_size = 512 << shift
        self.hashes = list(struct.iter_unpack('<IIHHI', decrypt(self.chunk(hp, hn * 16), hash_name('(hash table)', 3))))
        self.blocks = list(struct.iter_unpack('<IIII', decrypt(self.chunk(bp, bn * 16), hash_name('(block table)', 3))))

    def chunk(self, offset, size):
        return self.data[self.base + offset:self.base + offset + size]

    def read(self, name):
        ha, hb = hash_name(name, 1), hash_name(name, 2)
        matches = [row for row in self.hashes if row[:2] == (ha, hb) and row[4] < len(self.blocks)]
        if not matches:
            raise FileNotFoundError(name)
        offset, packed, size, flags = self.blocks[matches[0][4]]
        raw = self.chunk(offset, packed)
        key = hash_name(name.replace('/', '\\').split('\\')[-1], 3)
        if flags & 0x20000:
            key = ((key + offset) ^ size) & MASK
        encrypted = bool(flags & 0x10000)
        if flags & 0x1000000:
            raw = decrypt(raw, key) if encrypted else raw
            result = self.expand(raw, size, flags)
        elif flags & 0x300:
            n = (size + self.sector_size - 1) // self.sector_size
            table = raw[:4 * (n + 1)]
            table = decrypt(table, (key - 1) & MASK) if encrypted else table
            offsets = struct.unpack('<' + 'I' * (n + 1), table)
            sectors = []
            for i in range(n):
                part = raw[offsets[i]:offsets[i + 1]]
                part = decrypt(part, (key + i) & MASK) if encrypted else part
                sectors.append(self.expand(part, min(self.sector_size, size - i * self.sector_size), flags))
            result = b''.join(sectors)
        else:
            if encrypted:
                raw = b''.join(decrypt(raw[i:i + self.sector_size], (key + i // self.sector_size) & MASK) for i in range(0, len(raw), self.sector_size))
            result = raw
        if len(result) != size:
            raise ValueError(f'{name}: expected {size} bytes, got {len(result)}')
        return result

    @staticmethod
    def expand(raw, expected, flags):
        if len(raw) == expected:
            return raw
        if flags & 0x100:
            return explode(raw)
        method, raw = raw[0], raw[1:]
        if method & ~0x1A:
            raise NotImplementedError(f'Compression mask {method:#x}')
        if method & 0x10:
            raw = bz2.decompress(raw)
        if method & 0x08:
            raw = explode(raw)
        if method & 0x02:
            raw = zlib.decompress(raw)
        return raw


def main():
    root = Path(__file__).resolve().parents[1]
    maps = list(root.glob('*.w3x')) + list(root.glob('*.w3m'))
    if len(maps) != 1:
        raise ValueError('Expected exactly one map in project root')
    source = maps[0]
    archive = MPQ(source)
    output = root / 'Analysis' / source.stem
    output.mkdir(parents=True, exist_ok=True)
    names = {'(listfile)', '(attributes)', 'war3map.j', 'Scripts\\war3map.j', 'war3map.lua', 'war3map.w3i', 'war3map.wts', 'war3map.w3a', 'war3map.w3u', 'war3map.w3t', 'war3map.w3q', 'war3map.w3h', 'war3map.w3b', 'war3map.w3d', 'war3map.w3e', 'war3map.w3r', 'war3map.w3s', 'war3map.w3c', 'war3map.wtg', 'war3map.wct', 'war3map.imp', 'war3mapMisc.txt', 'war3mapSkin.txt'}
    try:
        listing = archive.read('(listfile)').decode('utf-8', errors='replace')
        names.update(line.strip() for line in listing.splitlines() if line.strip())
    except FileNotFoundError:
        pass
    extracted, errors = [], {}
    for name in sorted(names):
        # Extract data and scripts only; leave imported art/audio untouched.
        if name not in {'(listfile)', '(attributes)'} and not name.lower().endswith(('.j', '.lua', '.w3i', '.wts', '.w3a', '.w3u', '.w3t', '.w3q', '.w3h', '.w3b', '.w3d', '.w3e', '.w3r', '.w3s', '.w3c', '.wtg', '.wct', '.imp', '.txt', '.slk')):
            continue
        relative = Path(name.replace('\\', '/'))
        if relative.is_absolute() or '..' in relative.parts or ':' in name:
            errors[name] = 'Unsafe archive path'
            continue
        try:
            data = archive.read(name)
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            extracted.append({'name': name, 'bytes': len(data)})
        except FileNotFoundError:
            pass
        except Exception as exc:
            errors[name] = str(exc)
    report = {'source': source.name, 'sha256': hashlib.sha256(archive.data).hexdigest(), 'mpq_version': archive.version, 'block_entries': len(archive.blocks), 'listed_names': len(names), 'extracted': extracted, 'errors': errors}
    (output / 'inspection.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
