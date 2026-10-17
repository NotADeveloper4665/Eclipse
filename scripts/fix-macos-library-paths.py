#!/usr/bin/env python3
"""Repair already-bundled dylib aliases that macdeployqt leaves absolute."""
import pathlib
import subprocess
import sys
app = pathlib.Path(sys.argv[1]).resolve()
frameworks = app / 'Contents/Frameworks'
for path in app.rglob('*'):
    if not path.is_file() or path.is_symlink():
        continue
    if 'Mach-O' not in subprocess.check_output(['file', '-b', str(path)], text=True):
        continue
    ids = subprocess.check_output(['otool', '-arch', 'arm64', '-D', str(path)], text=True).splitlines()[1:]
    identities = {line.strip() for line in ids}
    lines = subprocess.check_output(['otool', '-arch', 'arm64', '-L', str(path)], text=True).splitlines()[1:]
    for line in lines:
        dep = line.strip().split(' (')[0]
        if dep in identities or not dep.startswith('/') or dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        target = frameworks / pathlib.Path(dep).name
        if target.is_file():
            replacement = '@executable_path/../Frameworks/' + target.name
            subprocess.run(['install_name_tool', '-change', dep, replacement, str(path)], check=True)
            print(f'Repaired {path.name}: {dep} -> {replacement}')
