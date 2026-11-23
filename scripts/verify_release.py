#!/usr/bin/env python3
"""Verify the exact V0141 OTA and optionally compare protected SPI ranges."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from firmware_format import ROOT, verify_resc, elf_sections

EXPECTED = '4a91383c0ae06ba16aa0772c42619ced8e4800b1d1d48dc9c75a106ee060d6f7'
ORIGINAL = '20d63efd687fdfbf76ae979bc04ca472162d07d5f6addc1c53fc9e21ed7cb204'


def verify(path, original=None, elf=None):
    path = Path(path)
    if path.stat().st_size != 8388624:
        raise ValueError('V0141 OTA must be exactly 8388624 bytes')
    blob = path.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    if digest != EXPECTED:
        raise ValueError('OTA SHA-256 does not match the published V0141')
    if struct.unpack_from('<II', blob) != (8388608, 0):
        raise ValueError('Invalid outer header')
    if struct.unpack_from('<II', blob, len(blob) - 8) != (0x5600008d, 0):
        raise ValueError('Invalid V0141 trailer')
    host = blob[8:-8]
    if host[:4] != b'RKFP' or host[0x416c4] != 141 or host[0x416cc] != 141:
        raise ValueError('Unexpected inner image/version')
    resc = [verify_resc(host, 0x20000, 0x700000), verify_resc(host, 0x720000, 0x80000)]
    result = {'version': 'V0141', 'bytes': len(blob), 'sha256': digest, 'RESC': resc,
              'physical_device_verified': False, 'protected_ranges_compared': False}
    if original:
        source = Path(original).read_bytes()
        if len(source) != 8388608 or hashlib.sha256(source).hexdigest() != ORIGINAL:
            raise ValueError('Unexpected original SPI image')
        for lo, hi in [(0, 0x20000), (0x720000, 0x800000)]:
            if source[lo:hi] != host[lo:hi]:
                raise ValueError('Protected image range differs from original')
        result['protected_ranges_compared'] = True
    if elf:
        named, _ = elf_sections(Path(elf).read_bytes())
        sections = {name: entry for name, entry in named}
        bss = sections['.bss']
        if bss[3] != 0x2002f99c or bss[5] != 3016:
            raise ValueError('Unexpected V0141 BSS layout')
        if '.data' in sections and sections['.data'][5]:
            raise ValueError('Initialized writable RAM is unsupported')
        result['static_bss_bytes'] = bss[5]
        result['reserved_static_bytes'] = 3072
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, default=ROOT/'_local/firmware/V0141/SOGOUOTA.BIN')
    parser.add_argument('--original', type=Path)
    parser.add_argument('--elf', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(args.package, args.original, args.elf), ensure_ascii=False, indent=2))
