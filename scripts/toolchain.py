"""Resolve user-selected LLVM tools without machine-specific paths."""
import os
from pathlib import Path
import shutil


def tool(name):
    variable = {'clang': 'C208_CLANG', 'ld.lld': 'C208_LD_LLD', 'llvm-ar': 'C208_LLVM_AR'}[name]
    value = os.environ.get(variable)
    candidates = [value] if value else []
    candidates.append(shutil.which(name))
    if os.environ.get('LLVM_BIN'):
        candidates.append(str(Path(os.environ['LLVM_BIN']) / name))
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(Path(candidate).absolute())
    raise RuntimeError(f'Missing {name}; set {variable} or put LLVM tools in PATH/LLVM_BIN')
