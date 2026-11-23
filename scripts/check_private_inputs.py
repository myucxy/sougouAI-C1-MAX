#!/usr/bin/env python3
"""Read-only check of user-supplied baseline inputs; never access a device."""
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent

if __name__ == '__main__':
    expected = json.loads((ROOT/'build-support/private-inputs.json').read_text())
    for name, spec in expected.items():
        path = ROOT/'_local/inputs'/name
        if not path.is_file():
            raise SystemExit(f'Missing local input: {name}; see docs/BUILDING.md')
        data = path.read_bytes()
        if len(data) != spec['bytes'] or hashlib.sha256(data).hexdigest() != spec['sha256']:
            raise SystemExit(f'Local input differs: {name}')
    print(f'PASS: {len(expected)} local input files match the recorded hashes; no device accessed.')
