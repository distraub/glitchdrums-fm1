#!/usr/bin/env python3
"""Compare the actual app DrumVoice to the integer INIT kick, isolated at 44.1k.
Both use the same seed, velocity and pitch. Firmware's fixed 1/3 summing headroom
is removed for analysis; no per-render gain normalization is used. Native only.
"""
import array, json, math, subprocess, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1]
OUT=ROOT/'build/test-release';BUILD=ROOT/'build/native';BUILD.mkdir(parents=True,exist_ok=True)
reference=json.loads((ROOT/'docs/KICK_INIT.json').read_text())['init']
properties={'tune':'tuneSemitones','decay':'decaySeconds','tone':'tone','noise':'noise','level':'level','ampAttack':'character.amplitude.attackSeconds','ampHold':'character.amplitude.holdSeconds','decayCurve':'character.amplitude.decayCurve','noiseColor':'character.noiseColor','kickPitchDecay':'character.kick.pitchDecaySeconds','kickClickLevel':'character.kick.clickLevelScale','kickClickDecay':'character.kick.clickDecaySeconds','kickBodyDelay':'character.kick.bodyDelaySeconds','kickPitchDrop':'character.kick.pitchDropSemitones','kickBodyDrive':'character.kick.bodyDrive','kickBodyShape':'character.kick.bodyShape','kickPitchHold':'character.kick.pitchHoldSeconds','kickBodyPunch':'character.kick.bodyPunch'}
assign=''.join(f'p.{field}={reference[key]:.12f}f;' for key,field in properties.items())
app='''#include "DrumVoice.hpp"
#include <cstdio>
#include <cstdlib>
using namespace GlitchDrums;
int main(int argc,char **argv){if(argc!=5)return 1;PadSnapshot p;p.model=DrumModel::kick;ASSIGN
DrumVoice voice;voice.prepare(44100);voice.trigger(p,std::atoi(argv[2])/127.f,std::strtoul(argv[4],nullptr,0),nullptr,std::atoi(argv[3]));
FILE *f=fopen(argv[1],"wb");for(int i=0;i<88200;++i){float v=voice.process()*p.level;fwrite(&v,4,1,f);}fclose(f);}
'''.replace('ASSIGN',assign)
firmware='''#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){if(argc!=5)return 1;GDProject p;gd_defaults(&p);GDEngine d={0};gd_hit(&d,&p,0,atoi(argv[2]),atoi(argv[3]),-1,0);d.voices[0].rng=(uint32_t)strtoul(argv[4],0,0);
FILE *f=fopen(argv[1],"wb");for(int i=0;i<88200;++i){int32_t l,r;gd_render_sample(&d,&p,&l,&r);float v=(l+r)*3.f/32768;fwrite(&v,4,1,f);}fclose(f);}
'''
(BUILD/'kick-app.cpp').write_text(app);(BUILD/'kick-firmware.c').write_text(firmware)
subprocess.run(['clang++','-std=c++20','-O2','-I',str(REPO/'GlitchDrumsExtension/DSP'),str(BUILD/'kick-app.cpp'),'-o',str(BUILD/'kick-app')],check=True)
subprocess.run(['clang','-std=c11','-O2','-I',str(ROOT/'src'),str(BUILD/'kick-firmware.c'),'-o',str(BUILD/'kick-firmware')],check=True)
def read(path):
 a=array.array('f');a.frombytes(path.read_bytes());return a
def presence(samples):
 prev=hp=energy=0.;coefficient=math.exp(-2*math.pi*1500/44100)
 for v in samples[:2205]:hp=coefficient*(hp+v-prev);prev=v;energy+=hp*hp
 return math.sqrt(energy/2205)
def compare(a,b):
 n=22050;aa=sum(x*x for x in a[:n]);bb=sum(x*x for x in b[:n]);dot=sum(x*y for x,y in zip(a[:n],b[:n]))
 return dict(correlation=dot/math.sqrt(aa*bb),gain_ratio=math.sqrt(bb/aa),relative_rms_error=math.sqrt(sum((x-y)**2 for x,y in zip(a[:n],b[:n]))/aa),presence_ratio=presence(b)/presence(a))
def wav(path,samples):
 data=array.array('h',(max(-32767,min(32767,round(v*32767))) for v in samples))
 with wave.open(str(path),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(44100);f.writeframes(data.tobytes())
results=[]
for velocity,pitch,seed in [(110,0,0x511a23),(64,0,0x1234abcd),(127,12,0x7654321),(110,-12,0x1234abcd)]:
 for name in ['app','firmware']:subprocess.run([str(BUILD/('kick-'+name)),str(OUT/(name+'-kick.f32')),str(velocity),str(pitch),str(seed)],check=True)
 a,b=read(OUT/'app-kick.f32'),read(OUT/'firmware-kick.f32');stats=compare(a,b);stats.update(velocity=velocity,pitch=pitch,seed=seed);results.append(stats)
 print(json.dumps(stats))
 assert stats['correlation']>.995,stats
 assert .98<stats['gain_ratio']<1.02,stats
 assert .95<stats['presence_ratio']<1.05,stats
 if velocity==110 and pitch==0:
  wav(OUT/'kick-app-reference.wav',a);wav(OUT/'kick-firmware-fixed.wav',b)
  before=OUT/'firmware-init-kick-before.f32'
  if before.exists():
   old=read(before);oldstats=compare(a,old);results.append(dict(before=oldstats));print('before',json.dumps(oldstats))
   silence=[0.]*11025
   wav(OUT/'kick-app-before-after.wav',list(a)+silence+list(old)+silence+list(b))
(OUT/'kick-comparison.json').write_text(json.dumps(results,indent=2)+'\n')
print('PASS: actual app INIT kick vs integer voice at four velocity/pitch/seed settings')
