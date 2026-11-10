#!/usr/bin/env python3
"""Map the app's INIT kick into integer controls; no target floating point.
Init's nearest 7-bit detents are calibrated to the exact source values. Other
positions span the published ranges; neutral bipolar detents remain at 64.
"""
import json, math, re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parents[1]
source=REPO/'GlitchDrumsExtension/Model/PadParameter.swift'
text=source.read_text()
values={k:float(v) for k,v in re.findall(r'case \(\.(\w+), \.kick\): ([-\d.]+)',text)}
keys=['tune','decay','tone','kickBodyDrive','kickPitchDrop','kickPitchDecay','kickClickLevel','kickClickDecay','kickBodyDelay','kickBodyShape','kickPitchHold','kickBodyPunch','ampAttack','ampHold','decayCurve','noiseColor']
ranges=[(-24,24),(.0005,16),(0,1),(0,1),(0,72),(.002,.2),(0,4),(.0005,.05),(0,.03),(-1,1),(0,.08),(0,1),(.0006,.12),(0,.08),(-1,1),(-1,1)]
timed={1,5,7,8,10,12,13}
def controls(i):
 lo,hi=ranges[i]
 if i==1:return [.0005*32000**(q/127) for q in range(128)]
 if lo==-hi:return [lo*(64-q)/64 if q<64 else hi*(q-64)/63 for q in range(128)]
 return [lo+(hi-lo)*q/127 for q in range(128)]
def array(name,vals,kind='int32_t'):
 return f'static const {kind} {name}[{len(vals)}]={{\n'+',\n'.join(','.join(str(v) for v in vals[j:j+16]) for j in range(0,len(vals),16))+'\n};\n'
rows=[];defaults=[];physical=[]
for i,key in enumerate(keys):
 row=controls(i);init=values[key];q=min(range(128),key=lambda n:abs(row[n]-init));row[q]=init
 defaults.append(q);physical.append(row)
 rows.append([round(v*44100) if i in timed else round(v*100) if i==0 else round(v*256) if i==4 else round(v*32768) for v in row])
out='/* Generated from PadParameter.defaultValue(for: .kick). */\n#pragma once\n#include <stdint.h>\n'
out+=array('gd_kick_init',defaults,'uint8_t')
out+='static const int32_t gd_kick_units[16][128]={\n'+',\n'.join('{'+','.join(map(str,row))+'}' for row in rows)+'\n};\n'
out+=array('gd_kick_root_inc',[round(52*2**(v/12)*2**32/44100) for v in physical[0]],'uint32_t')
out+=array('gd_kick_decay_coeff',[round(math.exp(-6.9/frames)*2**30) for frames in rows[1]],'uint32_t')
level=[q/127 for q in range(128)];level_q=min(range(128),key=lambda q:abs(level[q]-values['level']));level[level_q]=values['level']
out+=array('gd_kick_level',[round(v*32768) for v in level],'uint16_t')
out+=f'#define GD_KICK_INIT_LEVEL {level_q}\n'
(ROOT/'src/kick_init.h').write_text(out)
reference={'source':str(source.relative_to(REPO)),'factory':'VoicePresetParameterCatalog.factoryPreset(for: .kick)','base_hz':52,'init':{k:values[k] for k in keys+['level','noise']},'control_bytes':defaults,'level_byte':level_q,'root_hz':52*2**(values['tune']/12),'note':'Integer conversion is rounded to nearest frame/Q15 unit; synth/body shaping remains an adaptation.'}
(ROOT/'docs/KICK_INIT.json').write_text(json.dumps(reference,indent=2)+'\n')
print('Generated app INIT kick:',defaults,'level',level_q)
# Fixed-point versions of the app's kick filters and shape transfer functions.
# These tables contain coefficients only; no sample playback or audio assets.
extra='/* Generated app kick DSP coefficients. */\n#pragma once\n#include <stdint.h>\n'
extra+=array('gd_kick_tanh_table',[round(math.tanh(i/128)*32767) for i in range(1025)],'int16_t')
extra+=array('gd_kick_sweep_ratio',[round(2**(i/192)*65536) for i in range(1154)],'uint32_t')
for name,fn in {
 'tone_coeff':lambda q:1-math.exp(-2*math.pi*(250+physical[2][q]**2*17250)/44100),
 'round_coeff':lambda q:math.exp(-.8/rows[1][q]),
 'curve_rate':lambda q:2**(-min(0,physical[14][q])),
 'drive_gain':lambda q:1+physical[3][q]**3*3,
 'drive_wet':lambda q:physical[3][q]*.04+(physical[3][q]**12*(3-2*physical[3][q]**6))*.96,
 'drive_norm':lambda q:1/math.tanh(1+physical[3][q]**3*3),
 'drive_comp':lambda q:1/(1+.12*(physical[3][q]*.04+physical[3][q]**12*(3-2*physical[3][q]**6)*.96)*physical[3][q]**3),
 'shape_wet':lambda q:.30*(1-math.exp(-10*physical[3][q])),
}.items():
 scale=2**30 if name=='round_coeff' else 32768
 extra+=array('gd_kick_'+name,[round(fn(q)*scale) for q in range(128)],'uint32_t')
extra+=f'#define GD_KICK_COLOR_COEFF {round((1-math.exp(-2*math.pi*4200/44100))*32768)}\n'
extra+=f'#define GD_KICK_DELTA_DC_COEFF {round(math.exp(-2*math.pi*12/44100)*32768)}\n'
(ROOT/'src/kick_dsp_tables.h').write_text(extra)
