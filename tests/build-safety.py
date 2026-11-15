#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Regression tests for native package refusal gates, without a toolchain."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import build
syms='02000120 g .text _start\n01c1e000 g .bss _bss_end\n01c73a00 g .pool _pool_end\n'
stub=bytes.fromhex('04818000')
def errors(dis='',rt='',image=stub,symbols=syms):
    return build.check(image,symbols,dis,rt)[0]
assert not errors()
for value in ['4291035017','-3932279']:
    dis='gd_config:\n 2007052: c2 ff 89 ff c3 ff \tr2 = '+value+' <target : ffc3ff88 >\n'
    out=errors(dis=dis)
    assert len(out)==1 and 'chip ROM' in out[0] and 'gd_config' in out[0],out
assert any('chip ROM' in e for e in errors(dis='bad_call:\n 2007052: c2 ff 89 ff c3 ff \tcall 1 <target: ffc00000 >\n'))
assert any('.ram_text contains calls' in e for e in errors(rt=' call into_xip'))
assert any('entry stub' in e for e in errors(image=b'BAD!'))
assert any('not at' in e for e in errors(symbols=syms.replace('02000120','02000124')))
assert any('RAM (.data + .bss)' in e for e in errors(symbols=syms.replace('01c1e000','01c20004')))
assert any('pool' in e for e in errors(symbols=syms.replace('01c73a00','01c74004')))
assert any('exceeds the app slot' in e for e in errors(image=stub+bytes(build.APP_SLOT)))
print('PASS: entry, app size, ROM-window constants/calls, RAM-call isolation and RAM/pool refusal gates')
