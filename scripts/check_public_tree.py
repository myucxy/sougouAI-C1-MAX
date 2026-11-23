#!/usr/bin/env python3
"""Reject private artifacts and credential signatures from the public file set."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
PRIVATE_DIRS = {'_local', 'build', '.venv', '__pycache__', '.git'}
FORBIDDEN_SUFFIXES = {'.bin', '.elf', '.a', '.o', '.mp3', '.wav', '.zip', '.cfg', '.key', '.p12', '.pfx'}
SIGNATURES = [
    re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
    re.compile(rb'gh[pousr]_[A-Za-z0-9]{30,}'),
    re.compile(rb'github_pat_[A-Za-z0-9_]{30,}'),
    re.compile(rb'AKIA[0-9A-Z]{16}'),
    re.compile(rb'/Users/' + rb'myucxy(?:/|\b)'),
]


def public_files(tracked=False):
    if tracked:
        result = subprocess.run(['git', 'ls-files', '-z'], cwd=ROOT, check=True, capture_output=True)
        return [ROOT / name.decode() for name in result.stdout.split(b'\0') if name]
    result = []
    def visit(path):
        for child in path.iterdir():
            if child.name in PRIVATE_DIRS:
                continue
            if child.is_symlink() or child.is_file():
                result.append(child)
            elif child.is_dir():
                visit(child)
    visit(ROOT)
    return result


def check(tracked=False):
    errors = []
    files = public_files(tracked)
    for path in files:
        rel = path.relative_to(ROOT)
        if any(part in PRIVATE_DIRS for part in rel.parts):
            errors.append(f'Private directory tracked: {rel}')
        if path.is_symlink():
            errors.append(f'External/symlink dependency: {rel}')
            continue
        if path.suffix.lower() in FORBIDDEN_SUFFIXES or path.name in {'mode_font.h', '.DS_Store'}:
            errors.append(f'Private/generated artifact: {rel}')
        if path.stat().st_size > 10 * 1024 * 1024:
            errors.append(f'Oversized public file: {rel}')
        data = path.read_bytes()
        if any(signature.search(data) for signature in SIGNATURES):
            errors.append(f'Credential or local-identity signature: {rel}')
    if errors:
        raise ValueError('\n'.join(errors))
    return {'public_files': len(files), 'symlinks': 0, 'private_artifacts': 0,
            'credential_signatures': 0,
            'scope': 'tracked files' if tracked else 'public tree excluding local build directories',
            'limitation': 'Signature checks do not prove absence of every possible secret.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tracked', action='store_true')
    args = parser.parse_args()
    print(json.dumps(check(args.tracked), indent=2))
