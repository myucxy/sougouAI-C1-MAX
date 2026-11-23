"""C208 image validation, ELF layout, and Thumb branch encoding; no device I/O."""
from pathlib import Path
import struct
ROOT = Path(__file__).resolve().parent.parent
OSA = 0x20000
SIZE = 0x800000
VERSION_OFFSET = 0x416c4
COMBINED_VERSION_OFFSET = 0x416cc

def jshash(data):
    value = 0
    for byte in data:
        value = (value ^ ((value << 5) + byte + (value >> 2))) & 0xffffffff
    return value

def elf_sections(data):
    if data[:7]!=b'\x7fELF\x01\x01\x01' or struct.unpack_from('<H',data,18)[0]!=40:raise ValueError('Expected ELF32 ARM')
    table=struct.unpack_from('<I',data,32)[0];stride,count,names=struct.unpack_from('<HHH',data,46)
    sections=[struct.unpack_from('<10I',data,table+i*stride) for i in range(count)]
    ns=sections[names];st=data[ns[4]:ns[4]+ns[5]]
    return [(st[s[0]:].split(b'\0')[0].decode(),s) for s in sections],sections

def encode_bl(source, target):
    delta = target - (source + 4)
    if delta & 1 or not -(1 << 24) <= delta < (1 << 24):
        raise ValueError('Thumb BL target out of range or unaligned')
    value = delta & 0x1ffffff
    sign, i1, i2 = value >> 24, (value >> 23) & 1, (value >> 22) & 1
    j1, j2 = (1 ^ i1 ^ sign), (1 ^ i2 ^ sign)
    return struct.pack('<HH', 0xf000 | (sign << 10) | ((value >> 12) & 1023),
                       0xd000 | (j1 << 13) | (j2 << 11) | ((value >> 1) & 2047))

def encode_bw(source, target):
    first, second = struct.unpack('<HH', encode_bl(source, target))
    return struct.pack('<HH', first, second & ~0x4000)

def verify_resc(data, offset, maximum):
    if data[offset:offset + 8] != b'RESC\0\0\0\0':
        raise ValueError('Missing full RESC magic')
    data_offset, data_size = struct.unpack_from('<II', data, offset + 0x50)
    if data_offset != 512 or data[offset + 0x5c] != 1:
        raise ValueError('Unsupported RESC layout or digest algorithm')
    end = offset + data_offset + data_size
    if end + 4 > min(len(data), offset + maximum):
        raise ValueError('RESC image exceeds file or partition')
    expected = struct.unpack_from('<I', data, end)[0]
    actual = jshash(data[offset:end])
    if expected != actual:
        raise ValueError(f'RESC hash mismatch at 0x{offset:x}')
    return {'offset': offset, 'image_bytes': data_offset + data_size + 4,
            'digest_offset': end, 'jshash': f'0x{actual:08x}'}
