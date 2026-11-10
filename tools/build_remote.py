#!/usr/bin/env python3
"""Build local RemoteFM1 test module and foma browser preview without Docker.
Set WASI_SDK_PATH to an unpacked official WebAssembly/wasi-sdk release.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'test-release'

def leb(n):
    out = bytearray()
    while n >= 128:
        out.append((n & 127) | 128)
        n >>= 7
    out.append(n)
    return bytes(out)

def section(name, payload):
    name = name.encode()
    body = leb(len(name)) + name + payload
    return b'\0' + leb(len(body)) + body

def main():
    sdk = Path(os.environ['WASI_SDK_PATH'])
    cc = str(sdk / 'bin' / 'clang')
    OUT.mkdir(parents=True, exist_ok=True)
    cfg = json.loads((ROOT / 'firmware.json').read_text())
    stem = 'glitchdrums-' + cfg['version']
    source = OUT / (stem + '-source.tar.gz')
    files = sorted(p for p in ROOT.rglob('*') if p.is_file() and
                   not any(s in {'build', '__pycache__', '.git', '.local-archive', '.DS_Store'} for s in p.relative_to(ROOT).parts))
    with tarfile.open(source, 'w:gz') as tar:
        for p in files:
            tar.add(p, arcname='glitchdrums-fm1/' + p.relative_to(ROOT).as_posix())
    sha = hashlib.sha256(source.read_bytes()).hexdigest()
    # Private development build: exact complete source travels with the module.
    # A public release must replace this with an accessible pinned source URL.
    source_url = os.environ.get('GD_SOURCE_URL', source.as_uri() + '#sha256=' + sha)
    common = [cc, '-O2', '-nostdlib', '-fno-builtin', '-ffp-contract=off',
              '-mno-bulk-memory', '-mno-reference-types', '-mno-multivalue',
              '-Wall', '-Wextra', '-Wno-unused-function',
              '-Wno-incompatible-library-redeclaration',
              '-Iplatform', '-Iplatform/hal', '-Iplatform/src', '-Iplatform/gen',
              '-DFOMA_USB_AUDIO=0', '-DFOMA_MIDI_CLOCK=0',
              '-Wl,--no-entry', '-Wl,-z,stack-size=262144',
              '-Wl,--initial-memory=8388608', '-Wl,--max-memory=8388608',
              '-Wl,--export-memory']
    raw = OUT / (stem + '.wasm')
    subprocess.run(common + ['-DGD_REMOTE=1', '-DGD_SOURCE_URL=' + json.dumps(source_url),
                            '-DGD_FIRMWARE_VERSION=' + json.dumps(cfg['version']),
                            '-DGD_FIRMWARE_NAME=' + json.dumps(cfg['name']),
                            'remote/adapter.c', 'src/main.c', '-o', str(raw)], cwd=ROOT, check=True)
    manifest = dict(rfm1=1, abi=6, id='glitchdrums', name=cfg['name'], version=cfg['version'],
                    license='GPL-3.0-only', source_url=source_url,
                    authors=['Alex Matheu'], memory_mib=8,
                    built_by='tools/build_remote.py / WASI SDK')
    data = raw.read_bytes() + section('rfm1', json.dumps(manifest).encode())
    data += section('rfm1.credits', (ROOT / 'LICENSING.md').read_bytes())
    data += section('rfm1.license.0', b'GPL-3.0-only\n' + (ROOT / 'LICENSE').read_bytes())
    rfm = OUT / (stem + '.rfm1')
    node = shutil.which('node')
    if not node:
        raise SystemExit('Node.js is required to validate the RemoteFM1 import contract')
    pending = rfm.with_suffix('.rfm1.pending')
    pending.write_bytes(data)
    subprocess.run([node, 'tests/import-contract.mjs', str(pending)], cwd=ROOT, check=True)
    pending.replace(rfm)
    site = ROOT / 'build' / 'web'
    shutil.copytree(ROOT / 'web', site, dirs_exist_ok=True)
    subprocess.run(common + ['platform/web.c', 'src/main.c', '-o', str(site / 'firmware.wasm')],
                   cwd=ROOT, check=True)
    (site / 'firmware.json').write_text(json.dumps(cfg))
    (site / 'build.json').write_text(json.dumps(dict(id=str(time.time_ns()), name=cfg['name'],
        version=cfg['version'], identity='HOST PREVIEW', wasm=True, fwsc=False, ok=True,
        log='Browser test build; no hardware package.')))
    (OUT / 'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n'
                                             for p in [rfm, source]))
    print(rfm)
    print('Browser preview: python3 -m http.server 8000 --directory ' + str(site))

if __name__ == '__main__':
    main()
