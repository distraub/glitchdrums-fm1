#!/usr/bin/env python3
"""Regenerate ECO-only noise coloring from the existing quantized material bank."""
from pathlib import Path
import json,math
root=Path(__file__).resolve().parents[1]
source=(root/'src/glitch_materials.h').read_text().split('=',1)[1].split(';',1)[0]
material=json.loads(source.replace('{','[').replace('}',']'));assert len(material)==9
noise=material[8];assert len(noise)==4096
def read(pos):
 pos&=0xffffffff;i=pos>>20;f=(pos>>4)&65535
 return noise[i]+((noise[(i+1)&4095]-noise[i])*f>>16)
def trunc(x,d):return x//d if x>=0 else -((-x)//d)
low=[trunc(read((i<<20)-8589935)+2*noise[i]+read((i<<20)+8589935),4) for i in range(4096)]
comp=[round(32768/math.sqrt(1+i/16)) for i in range(69)]
text='/* SPDX-License-Identifier: GPL-3.0-only\n * ECO-only colored-noise cache and two-pole resonance frequency compensation.\n * Generated from the existing material bank; BAL/HIGH use the reference DSP. */\n#pragma once\n#include <stdint.h>\n'
for name,values in [('gd_eco_noise_low',low),('gd_eco_cut_comp',comp)]:
 text+='static const int16_t '+name+'['+str(len(values))+']={\n'+''.join(','.join(str(min(v,32767)) for v in values[i:i+16])+',\n' for i in range(0,len(values),16))+'};\n'
(root/'src/eco_tables.h').write_text(text)
