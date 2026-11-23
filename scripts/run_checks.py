#!/usr/bin/env python3
"""Run hardware-independent C tests and the public-file check without private inputs."""
from pathlib import Path
import subprocess
import sys
import tempfile
from toolchain import tool

ROOT = Path(__file__).resolve().parent.parent
CASES = [
    ('http', 'test_xiaozhi_http.c', ['patches/xiaozhi-official/http.c']),
    ('official-protocol', 'test_xiaozhi_official.c', ['patches/xiaozhi-official/official.c']),
    ('websocket', 'test_xiaozhi_websocket.c', ['patches/xiaozhi-official/websocket.c', 'patches/xiaozhi-official/official.c', 'third_party/bearssl-0.6/src/hash/sha1.c', 'third_party/bearssl-0.6/src/codec/dec32be.c', 'third_party/bearssl-0.6/src/codec/enc32be.c']),
    ('audio-level', 'test_xiaozhi_level.c', ['patches/xiaozhi-official/level.c']),
    ('wifi-protocol', 'test_wifi_protocol.c', ['patches/wifi/protocol.c']),
]


def main():
    with tempfile.TemporaryDirectory(prefix='c208-host-tests-') as tmp:
        for name, test, sources in CASES:
            output = Path(tmp) / name
            command = [tool('clang'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                       '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                       '-I'+str(ROOT/'third_party/bearssl-0.6/inc'), '-I'+str(ROOT/'third_party/bearssl-0.6/src'),
                       str(ROOT/'tests'/test), *[str(ROOT/source) for source in sources], '-o', str(output)]
            subprocess.run(command, check=True)
            subprocess.run([str(output)], check=True)
            print('PASS:', name, flush=True)
    subprocess.run([sys.executable, str(ROOT/'tests/test_release_tools.py')], check=True)
    subprocess.run([sys.executable, str(ROOT/'scripts/check_public_tree.py')], check=True)


if __name__ == '__main__':
    main()
