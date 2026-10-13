#!/usr/bin/env python3
"""Publish the current main commit only when all three required workflows pass."""
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


WORKFLOWS = ('Build Fedora RPM', 'Build and publish Flatpak', 'Source and interaction checks')


def gh(*args):
    return subprocess.check_output(['gh', *map(str, args)], text=True)


def api(path):
    return json.loads(gh('api', path))


def select_runs(runs, sha):
    """A failed/pending newer attempt must not be replaced by an older success."""
    selected = {}
    for run in runs:
        name = run['name']
        if (name not in WORKFLOWS or run['head_sha'] != sha or
                run['head_branch'] != 'main' or run['event'] != 'push'):
            continue
        previous = selected.get(name)
        if previous is None or (run['id'], run.get('run_attempt', 1)) > (previous['id'], previous.get('run_attempt', 1)):
            selected[name] = run
    if set(selected) != set(WORKFLOWS):
        return None
    if any(r['status'] != 'completed' or r['conclusion'] != 'success' for r in selected.values()):
        return None
    return selected


def collect_assets(root, version):
    files = [p for p in root.rglob('*') if p.is_file() and p.suffix in ('.rpm', '.zip', '.flatpak')]
    names = [p.name for p in files]
    if len(names) != len(set(names)):
        raise ValueError('Duplicate release asset names')
    if not any(re.fullmatch(r'moonlight-eclipse-' + re.escape(version) + r'-[^/]+\.rpm', n) for n in names):
        raise ValueError('Missing installable RPM')
    for required in (f'Eclipse-source-{version}.zip', f'Eclipse-{version}-x86_64.flatpak'):
        if required not in names:
            raise ValueError(f'Missing release asset: {required}')
    for name in names:
        if not re.fullmatch(r'(moonlight-eclipse(?:-debuginfo|-debugsource)?-' + re.escape(version) +
                            r'-[^/]+\.rpm|Eclipse-source-' + re.escape(version) +
                            r'\.zip|Eclipse-' + re.escape(version) + r'-x86_64\.flatpak)', name):
            raise ValueError(f'Unexpected release asset: {name}')
    return sorted(files, key=lambda p: p.name)


def main():
    repo = os.environ['GITHUB_REPOSITORY']
    prefix = f'repos/{repo}'
    sha = api(f'{prefix}/branches/main')['commit']['sha']
    runs = api(f'{prefix}/actions/runs?branch=main&event=push&head_sha={sha}&per_page=100')['workflow_runs']
    selected = select_runs(runs, sha)
    if selected is None:
        print(f'Waiting for successful RPM, Flatpak and source checks on {sha}; release retained.')
        return
    spec = api(f'{prefix}/contents/packaging/eclipse.spec?ref={sha}')
    text = base64.b64decode(spec['content']).decode()
    match = re.search(r'^Version:\s*([0-9]+\.[0-9]+\.[0-9]+)\s*$', text, re.MULTILINE)
    if match is None:
        raise ValueError('Cannot determine package version')
    version = match.group(1)
    with tempfile.TemporaryDirectory(prefix='eclipse-dev-') as directory:
        root = Path(directory)
        downloads = root / 'downloads'
        for workflow, artifact in ((WORKFLOWS[0], 'Eclipse-Fedora-44'), (WORKFLOWS[1], 'Eclipse-Flatpak-x86_64')):
            gh('run', 'download', selected[workflow]['id'], '--repo', repo, '--name', artifact,
               '--dir', downloads / artifact)
        assets = root / 'assets'
        assets.mkdir()
        for source in collect_assets(downloads, version):
            shutil.copy2(source, assets / source.name)
        manifest = {'schema': 1, 'version': version, 'commit': sha,
                    'checks': {name: selected[name]['html_url'] for name in WORKFLOWS},
                    'sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(assets.iterdir())}}
        (assets / 'dev-build.json').write_text(json.dumps(manifest, indent=2) + '\n')
        checksums = [f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(assets.iterdir())]
        (assets / 'SHA256SUMS').write_text(''.join(checksums))
        notes = root / 'notes.md'
        notes.write_text(f'''Eclipse {version} development build

- WAN / 5G Optimization with negotiated host support and per-feature fallbacks.
- Feedback recovers after temporary network failures with bounded retries.
- Fedora 44 x86_64 RPM, x86_64 Flatpak, and complete source archive.

Install the `moonlight-eclipse` RPM; debuginfo/debugsource packages are optional.
The RPM includes dependencies for experimental authenticated QUIC. QUIC dependencies are not bundled in Flatpak.
Real cellular streaming performance remains unverified. This is a development preview.

Source: {sha}

''' + '\n'.join(f'- {name}: {selected[name]["html_url"]}' for name in WORKFLOWS) + '\n\nSHA256SUMS and dev-build.json record integrity and build provenance.\n')
        # Recheck after downloading: an older build must not replace a newer main.
        if api(f'{prefix}/branches/main')['commit']['sha'] != sha:
            print('Main advanced during artifact download; waiting for the newer builds.')
            return
        result = subprocess.run(['gh', 'release', 'view', 'dev', '--repo', repo],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        payload = [str(p) for p in sorted(assets.iterdir())]
        if result.returncode:
            gh('release', 'create', 'dev', *payload, '--repo', repo, '--target', sha,
               '--title', f'Eclipse {version} Development', '--notes-file', notes, '--prerelease', '--latest=false')
        else:
            gh('release', 'upload', 'dev', *payload, '--repo', repo, '--clobber')
            gh('release', 'edit', 'dev', '--repo', repo, '--title', f'Eclipse {version} Development',
               '--notes-file', notes, '--prerelease', '--latest=false')
            # Keep only current managed packages; leave unrelated user attachments alone.
            current = {p.name for p in assets.iterdir()}
            for asset in api(f'{prefix}/releases/tags/dev')['assets']:
                name = asset['name']
                if name not in current and re.fullmatch(r'(moonlight-eclipse-.*\.rpm|Eclipse-.*\.(zip|flatpak))', name):
                    gh('release', 'delete-asset', 'dev', name, '--repo', repo, '--yes')
        # The rolling dev tag must also identify the actual downloadable source.
        gh('api', '--method', 'PATCH', f'{prefix}/git/refs/tags/dev', '-f', f'sha={sha}', '-F', 'force=true')
        print(f'Published Eclipse {version} development downloads from {sha}')


if __name__ == '__main__':
    main()
