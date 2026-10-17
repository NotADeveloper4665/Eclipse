#!/usr/bin/env python3
"""Reject missing/external runtime dependencies and non-arm64 bundled Mach-O files."""
import pathlib
import plistlib
import subprocess
import sys
app = pathlib.Path(sys.argv[1]).resolve()
plist = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
assert plist['CFBundleIdentifier'] == 'io.github.notadeveloper4665.Eclipse'
assert plist['CFBundleExecutable'] == 'Eclipse'
errors = []
count = 0
for path in app.rglob('*'):
    if not path.is_file() or path.is_symlink():
        continue
    kind = subprocess.check_output(['file', '-b', str(path)], text=True)
    if 'Mach-O' not in kind:
        continue
    count += 1
    if subprocess.run(['lipo', str(path), '-verify_arch', 'arm64'], capture_output=True).returncode:
        errors.append(f'No arm64 slice: {path}')
    linked = subprocess.check_output(['otool', '-L', str(path)], text=True).splitlines()[1:]
    for line in linked:
        dep = line.strip().split(' (')[0]
        if dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        if dep.startswith('@rpath/'):
            target = app / 'Contents/Frameworks' / dep[len('@rpath/'):]
        elif dep.startswith('@loader_path/'):
            target = path.parent / dep[len('@loader_path/'):]
        elif dep.startswith('@executable_path/'):
            target = app / 'Contents/MacOS' / dep[len('@executable_path/'):]
        else:
            errors.append(f'External dependency: {path}: {dep}')
            continue
        if not target.exists():
            errors.append(f'Missing dependency: {path}: {dep}')
if not count:
    errors.append('No Mach-O binaries found')
if errors:
    raise SystemExit('\n'.join(errors))
print(f'Checked {count} arm64 Mach-O binaries and their runtime dependencies')
