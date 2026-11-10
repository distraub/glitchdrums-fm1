#!/usr/bin/env python3
"""Compare app and fixed-point synth INIT voices; metrics describe differences."""
import json,math,array,subprocess,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1];B=ROOT/'build/native';O=ROOT/'build/test-release';B.mkdir(parents=True,exist_ok=True)
reference=json.loads((ROOT/'docs/SYNTH_INIT.json').read_text())
fields={'analogVCO1Pitch':'analog.vco1PitchNote','analogVCO2Pitch':'analog.vco2PitchNote','analogVCO1Wave':'analog.vco1Wave','analogVCO2Wave':'analog.vco2Wave','analogFMAmt':'analog.fmAmount','analogOscMix':'analog.oscMix','analogNoise':'analog.noise','analogPitchEGAmt':'analog.pitchEGAmount','analogPitchDecay':'analog.pitchDecay','analogCutoff':'analog.cutoffHz','analogReso':'analog.resonance','analogVCFEGAmt':'analog.vcfEGAmount','analogVCFDecay':'analog.vcfDecay','analogVCFMod':'analog.vcfMod','analogVCADecay':'analog.vcaDecay','analogVCAAttack':'character.analog.vcaAttackSeconds','analogVCO2Fine':'character.analog.vco2FineSemitones','analogVCFDrive':'character.analog.vcfDrive','analogVCACurve':'character.analog.vcaCurve','analogVCFAttack':'character.analog.vcfAttackSeconds','analogVCFCurve':'character.analog.vcfCurve','noiseColor':'character.noiseColor','decay':'decaySeconds','tune':'tuneSemitones','ampSustain':'ampSustain','level':'level'}
a=''.join(f'p.{fields[k]}={v:.12f}f;' for k,v in reference['analog']['init'].items() if k in fields)
g=''.join(f'p.glitch.values[{i}]={reference["glitch"]["init"][key]:.12f}f;' for i,key in enumerate(reference['glitch']['fields']))
cpp='''#include "DrumVoice.hpp"
#include <cstdio>
#include <cstdlib>
using namespace GlitchDrums;
int main(int argc,char**argv){if(argc!=5)return 1;PadSnapshot p;int lane=atoi(argv[2]);if(lane==7){p.model=DrumModel::analog;ANALOG}else{p.model=DrumModel::glitch;p.level=.9f;GLITCH}
DrumVoice v;v.prepare(44100);v.trigger(p,atoi(argv[3])/127.f,0x511a23u+lane*173,nullptr,atoi(argv[4]));FILE*f=fopen(argv[1],"wb");for(int i=0;i<88200;++i){float s=v.process()*p.level;fwrite(&s,4,1,f);}fclose(f);}
'''.replace('ANALOG',a).replace('GLITCH',g)
c='''#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char**argv){if(argc!=5)return 1;GDProject p;gd_defaults(&p);p.reserved=1;GDEngine d={0};unsigned lane=atoi(argv[2]);gd_hit(&d,&p,lane,atoi(argv[3]),atoi(argv[4]),-1,0);FILE*f=fopen(argv[1],"wb");for(int i=0;i<88200;++i){int32_t l,r;gd_render_sample(&d,&p,&l,&r);float s=(l+r)*3.f/32768;fwrite(&s,4,1,f);}fclose(f);}
'''
(B/'synth-app.cpp').write_text(cpp);(B/'synth-fw.c').write_text(c)
subprocess.run(['clang++','-std=c++20','-O2','-I',str(REPO/'GlitchDrumsExtension/DSP'),str(B/'synth-app.cpp'),'-o',str(B/'synth-app')],check=True)
subprocess.run(['clang','-std=c11','-O2','-I',str(ROOT/'src'),str(B/'synth-fw.c'),'-o',str(B/'synth-fw')],check=True)
def read(p):
 a=array.array('f');a.frombytes(p.read_bytes());return a
def wav(path,s):
 data=array.array('h',(max(-32767,min(32767,round(v*32767))) for v in s))
 with wave.open(str(path),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(44100);f.writeframes(data.tobytes())
results=[]
for lane in [6,7]:
 for velocity,pitch in [(110,0),(64,0),(127,12)]:
  for name in ['app','fw']:subprocess.run([str(B/('synth-'+name)),str(O/(name+'-synth.f32')),str(lane),str(velocity),str(pitch)],check=True)
  a,b=read(O/'app-synth.f32'),read(O/'fw-synth.f32');n=22050;aa=sum(x*x for x in a[:n]);bb=sum(x*x for x in b[:n]);dot=sum(x*y for x,y in zip(a[:n],b[:n]));stats=dict(model='glitch' if lane==6 else 'analog',velocity=velocity,pitch=pitch,correlation=dot/math.sqrt(aa*bb),gain_ratio=math.sqrt(bb/aa));print(json.dumps(stats),flush=True);results.append(stats)
  assert stats['correlation']>.999 and .99<stats['gain_ratio']<1.01,stats
  if velocity==110 and pitch==0:wav(O/(stats['model']+'-app-firmware.wav'),list(a)+[0.]*11025+list(b))
(O/'synth-comparison.json').write_text(json.dumps(results,indent=2)+'\n')
