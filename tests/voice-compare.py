#!/usr/bin/env python3
"""Render app DrumVoice and integer acoustic voices with matched INIT controls.
Isolated 44.1 kHz voices, no master FX; remove only fixed 1/3 mixer headroom.
"""
import json,math,array,subprocess,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1];BUILD=ROOT/'build/native';OUT=ROOT/'build/test-release'
BUILD.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
models=json.loads((ROOT/'docs/ACOUSTIC_INIT.json').read_text())
perc=json.loads((ROOT/'docs/PERC_INIT.json').read_text());models[-1]=dict(perc,init=perc['app_init'],model='percussion')
fields={'tune':'tuneSemitones','decay':'decaySeconds','tone':'tone','noise':'noise','snap':'snap','level':'level','ampAttack':'character.amplitude.attackSeconds','ampHold':'character.amplitude.holdSeconds','decayCurve':'character.amplitude.decayCurve','noiseColor':'character.noiseColor','snarePunch':'character.snare.punch','snareRing':'character.snare.ringRatio','snareBodyDecay':'character.snare.bodyDecayScale','snareWireAttack':'character.snare.wireAttackSeconds','snareWireDecay':'character.snare.wireDecayScale','hatMetalA':'character.hat.metalASemitones','hatMetalB':'character.hat.metalBSemitones','hatDensity':'character.hat.density','hatMetalDecay':'character.hat.metalDecayScale','hatNoiseDecay':'character.hat.noiseDecayScale','hatChokeTime':'character.hat.chokeSeconds','hatBite':'character.hat.bite','clapFlam':'character.clap.flamSeconds','clapBursts':'character.clap.burstCount','clapBurstDecay':'character.clap.burstDecayScale','clapTailDecay':'character.clap.tailDecayScale','clapSmack':'character.clap.smack','tomBend':'character.tom.bendAmount','tomBendDecay':'character.tom.bendDecayScale','tomBody':'character.tom.body','tomLaser':'character.tom.laser','tomPunch':'character.tom.punch'}
fields.update(dict(percRatio='character.perc.ratioScale',percXMod='character.perc.crossMod',percXModDecay='character.perc.crossModDecayScale',percShape='character.perc.shape',percDual='character.perc.dual',percBend='character.perc.bendAmount',percBendDecay='character.perc.bendDecayScale',percStrike='character.perc.strike',percToneNoise='character.perc.toneNoise',percThud='character.perc.thud'))
cases=[]
for i,model in enumerate(models):
 assign=''.join(f'p.{fields[k]}={v:.12f}f;' for k,v in model['init'].items() if k in fields)
 cases.append(f'case {i+1}:p.model=DrumModel::{model["model"]};{assign}break;')
app='''#include "DrumVoice.hpp"
#include <cstdio>
#include <cstdlib>
using namespace GlitchDrums;
int main(int argc,char **argv){if(argc!=6)return 1;PadSnapshot p;switch(atoi(argv[2])){CASES}
DrumVoice v;v.prepare(44100);v.trigger(p,atoi(argv[3])/127.f,strtoul(argv[5],0,0),nullptr,atoi(argv[4]));FILE*f=fopen(argv[1],"wb");for(int i=0;i<88200;++i){float x=v.process()*p.level;fwrite(&x,4,1,f);}fclose(f);}
'''.replace('CASES',''.join(cases))
fw='''#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char**argv){if(argc!=6)return 1;GDProject p;gd_defaults(&p);GDEngine d={0};unsigned lane=atoi(argv[2]);if(lane==5){for(unsigned i=0;i<20;++i)p.track[5].param[i]=gd_perc_app_init[i];p.track[5].param[29]=gd_perc_app_init[20];}gd_hit(&d,&p,lane,atoi(argv[3]),atoi(argv[4]),-1,0);d.voices[lane].rng=strtoul(argv[5],0,0);FILE*f=fopen(argv[1],"wb");for(int i=0;i<88200;++i){int32_t l,r;gd_render_sample(&d,&p,&l,&r);float x=(l+r)*3.f/32768;fwrite(&x,4,1,f);}fclose(f);}
'''
(BUILD/'voices-app.cpp').write_text(app);(BUILD/'voices-fw.c').write_text(fw)
subprocess.run(['clang++','-std=c++20','-O2','-I',str(REPO/'GlitchDrumsExtension/DSP'),str(BUILD/'voices-app.cpp'),'-o',str(BUILD/'voices-app')],check=True)
subprocess.run(['clang','-std=c11','-O2','-I',str(ROOT/'src'),str(BUILD/'voices-fw.c'),'-o',str(BUILD/'voices-fw')],check=True)
def read(p):
 a=array.array('f');a.frombytes(p.read_bytes());return a
def band(s,hz):
 hp=prev=power=0.;a=math.exp(-2*math.pi*hz/44100)
 for v in s[:8820]:hp=a*(hp+v-prev);prev=v;power+=hp*hp
 return math.sqrt(power/8820)
def wav(path,s):
 data=array.array('h',(max(-32767,min(32767,round(v*32767))) for v in s))
 with wave.open(str(path),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(44100);f.writeframes(data.tobytes())
results=[]
for lane,model in enumerate(models,1):
 for velocity,pitch in [(110,0),(64,0),(127,12)]:
  for name in ['app','fw']:subprocess.run([str(BUILD/('voices-'+name)),str(OUT/(name+'-voice.f32')),str(lane),str(velocity),str(pitch),str(0x511a23+lane*173)],check=True)
  a,b=read(OUT/'app-voice.f32'),read(OUT/'fw-voice.f32');n=22050;aa=sum(x*x for x in a[:n]);bb=sum(x*x for x in b[:n]);dot=sum(x*y for x,y in zip(a[:n],b[:n]))
  stats=dict(model=model['model'],velocity=velocity,pitch=pitch,correlation=dot/math.sqrt(aa*bb),gain_ratio=math.sqrt(bb/aa),presence_ratio=band(b,1500)/band(a,1500));results.append(stats);print(json.dumps(stats),flush=True)
  assert stats['correlation']>(.99 if model['model']=='closedHat' and pitch else .999),stats
  assert .98<stats['gain_ratio']<1.02 and .98<stats['presence_ratio']<1.02,stats
  if velocity==110 and pitch==0:wav(OUT/(model['model']+'-app-firmware.wav'),list(a)+[0.]*11025+list(b))
(OUT/'voice-comparison.json').write_text(json.dumps(results,indent=2)+'\n')
