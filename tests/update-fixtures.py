#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Generate simulation-only packages; never install these on hardware."""
from pathlib import Path
import random
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build
import fm1pkg_make as pkg
root = Path(__file__).resolve().parents[1]
out = root / 'build/native'
out.mkdir(parents=True, exist_ok=True)
rng = random.Random(1973)
pattern = bytes(rng.randrange(256) for _ in range(2048))
image = (bytes.fromhex('04818000') + b'FELUCCA-LOADER-1\0' + pattern * 4)[:8192]
loader = build.ldr_wrap(image)
for name, data in [('old', b'OLD-FIRMWARE' * 1000), ('candidate', b'NEW-FIRMWARE' * 2000)]:
    (out / (name + '.fwsc')).write_bytes(pkg.ufw(pkg.flash_image(data, pkg.KEY), loader, 'FM-1_737'))
