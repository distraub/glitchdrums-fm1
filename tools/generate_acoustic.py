#!/usr/bin/env python3
"""Generate fixed-point drum controls from app model-specific INIT defaults."""
import re,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1]
text=(REPO/'GlitchDrumsExtension/Model/PadParameter.swift').read_text()
models=['snare','closedHat','openHat','clap','lowTom']
base=dict(tune=0,decay=.55,tone=.55,noise=.08,snap=.42,level=.82,ampAttack=.0006,ampHold=0,decayCurve=0,noiseColor=0,snarePunch=.35,snareRing=1.62,snareBodyDecay=1,snareWireAttack=0,snareWireDecay=1,hatMetalA=0,hatMetalB=0,hatDensity=0,hatMetalDecay=1,hatNoiseDecay=1,hatChokeTime=0,hatBite=0,clapFlam=.012,clapBursts=4,clapBurstDecay=1,clapTailDecay=1,clapSmack=0,tomBend=.45,tomBendDecay=1,tomBody=0,tomLaser=0,tomPunch=0,bodyTrim=1,wireTrim=1,extraCutoff=18000,extraDrive=0,tomType=0,unused=0)
keys=[['snarePunch','snareRing','snareBodyDecay','snareWireAttack','snareWireDecay','bodyTrim','wireTrim'],['hatMetalA','hatMetalB','hatDensity','hatMetalDecay','hatNoiseDecay','hatChokeTime','hatBite'],['hatMetalA','hatMetalB','hatDensity','hatMetalDecay','hatNoiseDecay','hatChokeTime','hatBite'],['clapFlam','clapBursts','clapBurstDecay','clapTailDecay','clapSmack','extraDrive','unused'],['tomBend','tomBendDecay','tomBody','tomLaser','tomPunch','snap','tomType']]
names=[['PUNCH','RING','BD DEC','WR ATK','WR DEC','BODY','WIRE'],['MET A','MET B','DENS','MT DEC','NS DEC','CHOKE','BITE'],['MET A','MET B','DENS','MT DEC','NS DEC','CHOKE','BITE'],['FLAM','BURSTS','BR DEC','TL DEC','SMACK','DRIVE',''],['BEND','BN DEC','BODY','LASER','PUNCH','SNAP','TYPE']]
ranges=dict(tune=(-24,24),decay=(.02,2),tone=(0,1),noise=(0,1),snarePunch=(0,1.2),snareRing=(1,2.8),snareBodyDecay=(.25,4),snareWireAttack=(0,.05),snareWireDecay=(.25,4),bodyTrim=(0,2),wireTrim=(0,2),hatMetalA=(-4,4),hatMetalB=(-4,4),hatDensity=(0,1),hatMetalDecay=(.25,4),hatNoiseDecay=(.25,4),hatChokeTime=(0,.2),hatBite=(0,1),clapFlam=(.004,.03),clapBursts=(2,5),clapBurstDecay=(.25,4),clapTailDecay=(.25,4),clapSmack=(0,1),tomBend=(0,1.2),tomBendDecay=(.25,4),tomBody=(0,1),tomLaser=(0,1),tomPunch=(0,1),snap=(0,1),tomType=(0,1),noiseColor=(-1,1),ampAttack=(.0006,.12),ampHold=(0,.08),decayCurve=(-1,1),extraDrive=(0,1),extraCutoff=(20,18000),unused=(0,0))
timed={'decay','snareWireAttack','hatChokeTime','clapFlam','ampAttack','ampHold'}
# Units: times = sample frames, pitch = cents, other physical scalars = Q15.
all_units=[];all_root=[];all_tone=[];all_curve=[];all_level=[];all_hat=[];all_init=[];info=[];descriptors=[]
for model,custom,labels in zip(models,keys,names):
 values=base.copy();values.update({k:float(v) for k,v in re.findall(r'case \(\.(\w+), \.'+model+r'\): ([-\d.]+)',text)})
 fields=['tune','decay','tone','noise']+custom+['noiseColor','ampAttack','ampHold','decayCurve','extraCutoff' if model in ['snare','clap'] else 'extraDrive']
 physical=[];units=[];init=[];controls=[]
 for i,key in enumerate(fields):
  lo,hi=ranges[key];maximum=3 if key=='clapBursts' else 1 if key=='tomType' else 0 if key=='unused' or (key=='hatChokeTime' and model=='closedHat') else 127
  row=[lo+(hi-lo)*q/(maximum or 127) for q in range(128)] if key in ['clapBursts','tomType'] else [lo*(64-q)/64 if q<64 else hi*(q-64)/63 for q in range(128)] if lo==-hi and lo<0 else [lo+(hi-lo)*q/127 for q in range(128)]
  row=[max(lo,min(hi,v)) for v in row];q=min(range(maximum+1 if maximum else 128),key=lambda q:abs(row[q]-values[key]));row[q]=values[key]
  init.append(q);physical.append(row)
  units.append([round(v*44100) if key in timed else round(v*100) if key in ['tune','hatMetalA','hatMetalB'] else round(v) if key in ['clapBursts','tomType','extraCutoff'] else round(v*32768) for v in row])
  name=(['TUNE','DECAY','TONE','NOISE']+labels+['COLOR','ATTACK','HOLD','CURVE','CUTOFF' if model in ['snare','clap'] else 'DRIVE'])[i]
  controls.append((name,maximum))
 root={'snare':185,'closedHat':5200,'openHat':4300,'clap':1200,'lowTom':92}[model]
 all_root.append([round(root*2**(v/12)*2**32/44100) for v in physical[0]])
 all_tone.append([round((1-math.exp(-2*math.pi*(250+v*v*17250)/44100))*32768) for v in physical[2]])
 all_curve.append([round(2**(-v)*32768) for v in physical[14]])
 lv=[q/127 for q in range(128)];lq=min(range(128),key=lambda q:abs(lv[q]-values['level']));lv[lq]=values['level'];all_level.append([round(v*32768) for v in lv])
 all_hat.append([[round(math.cos(v*math.pi/2)*.82*32768),round(math.sin(v*math.pi/2)*1.12*32768)] for v in physical[3]])
 all_units.append(units);all_init.append(init+[lq]);descriptors.append(controls)
 info.append(dict(model=model,fields=fields,init={k:values[k] for k in fields+['level']},control_bytes=init,level_byte=lq,units=['frames' if k in timed else 'cents' if k in ['tune','hatMetalA','hatMetalB'] else 'integer' if k in ['clapBursts','tomType','extraCutoff'] else 'q15' for k in fields],max=[x[1] for x in controls]))
def arr(name,data,typ='int32_t'):
 def shape(a):return '['+str(len(a))+']'+(shape(a[0]) if isinstance(a[0],list) else '')
 def val(a):return '{'+','.join(val(v) if isinstance(v,list) else str(v) for v in a)+'}'
 return f'static const {typ} {name}{shape(data)}='+val(data)+';\n'
s='/* Generated from PadParameter model-specific INIT defaults. */\n#pragma once\n#include <stdint.h>\n#include "controls.h"\n'
for name,data,typ in [('units',all_units,'int32_t'),('root',all_root,'uint32_t'),('tone',all_tone,'uint16_t'),('curve',all_curve,'uint32_t'),('level',all_level,'uint16_t'),('hat_mix',all_hat,'int32_t'),('init',all_init,'uint8_t')]:s+=arr('gd_ac_'+name,data,typ)
s+='static const GDControl gd_ac_controls[5][16]={'+','.join('{'+','.join('{"'+name+'",'+str(maximum)+',F_AC+'+str(i)+'}' for i,(name,maximum) in enumerate(row))+'}' for row in descriptors)+'};\n'
s+=arr('gd_ac_pitch_ratio',[round(2**(c/1200)*65536) for c in range(-400,401)],'uint32_t')
(ROOT/'src/acoustic_tables.h').write_text(s);(ROOT/'docs/ACOUSTIC_INIT.json').write_text(json.dumps(info,indent=2)+'\n')
print('Generated controls and INIT tables for',', '.join(models))
with (ROOT/'src/acoustic_tables.h').open('a') as f:f.write(arr('gd_ac_lp',[round((1-math.exp(-2*math.pi*i*32/44100))*32768) for i in range(642)],'uint16_t'))
