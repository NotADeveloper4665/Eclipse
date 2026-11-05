#!/usr/bin/env python3
"""Prove the bundled SDL3 dylib can be loaded by the native arm64 process."""
import ctypes
import pathlib
import subprocess
import sys

library = pathlib.Path(sys.argv[1]).resolve()
if not library.is_file():
    raise SystemExit(f"SDL3 runtime missing: {library}")
subprocess.run(["lipo", str(library), "-verify_arch", "arm64"], check=True)
ctypes.CDLL(str(library), mode=ctypes.RTLD_LOCAL)
print(f"SDL3 dynamic load passed: {library}")
