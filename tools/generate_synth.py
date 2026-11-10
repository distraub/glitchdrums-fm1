#!/usr/bin/env python3
"""Integer control/coefficient tables for app-derived ANALOG and GLITCH."""
import re,json,math,subprocess,array
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1];BUILD=ROOT/'build/native';BUILD.mkdir(parents=True,exist_ok=True)
swift=(REPO/'GlitchDrumsExtension/Model/PadParameter.swift').read_text()
# The model-independent defaults are the app's ANALOG factory values.
defaults={'analogVCO1Pitch':36,'analogVCO2Pitch':48,'analogVCO1Wave':1,'analogVCO2Wave':0,'analogFMAmt':.44,'analogOscMix':.5,'analogNoise':.58,'analogPitchEGAmt':.56,'analogPitchDecay':.64,'analogCutoff':284,'analogReso':.61,'analogVCFEGAmt':.70,'analogVCFDecay':.73,'analogVCFMod':.62,'analogVCADecay':.85,'analogVCAAttack':0,'analogVCO2Fine':0,'analogPitchToVCF':0,'analogVCFDrive':5.74,'analogVCACurve':.5,'analogVCFAttack':0,'analogVCFCurve':-.37,'noiseColor':0,'decay':.55,'tune':0,'ampSustain':1,'ampRelease':.15,'holdMode':0,'analogVCOEGToNoise':0,'level':.82}
# Keep source-derived numeric defaults in sync even when the app recipe evolves.
for key in list(defaults):
 match=re.search(r'case \.'+key+r': (-?\d+(?:\.\d+)?)\s*\n',swift[swift.index('var defaultValue: Double'):swift.index('func defaultValue(for')])
 if match:defaults[key]=float(match.group(1))
defaults.update({k:float(v) for k,v in re.findall(r'case \(\.(\w+), \.analog\): ([-\d.]+)',swift)})
a_keys=[k for k in defaults if k!='level']
a_ranges=[(12,96),(12,96),(0,1),(0,1),(0,1),(0,1),(0,1),(-1,1),(0,1),(20,18000),(0,1),(-1,1),(0,1),(-1,1),(0,1),(0,.1),(-1,1),(0,1),(1,8),(-1,1),(0,.1),(-1,1),(-1,1),(.02,2),(-24,24),(0,1),(.005,5),(0,1),(0,1)]
a_names=['VCO 1','VCO 2','WAVE 1','WAVE 2','FM','MIX','NOISE','PIT EG','PITDEC','CUTOFF','RESO','FLT EG','FLTDEC','VCFMOD','AMPDEC','ATTACK','FINE','PIT>CF','VCFDRV','AMPCRV','FLTATK','FLTCRV','COLOR','DECAY','TUNE','SUST','REL ms','HOLD','EG>NS']
text=(REPO/'GlitchDrumsExtension/Model/VoicePresetParameterCatalog.swift').read_text();section=text[text.index('private static let glitchInitValues'):text.index('static func mergedImage')]
ginit={k:float(v) for k,v in re.findall(r'\.(\w+): ([-\d.]+)',section)}
g_keys=['glitchSource','glitchMaterial','glitchTuning','glitchNote','glitchSize','glitchPosition','glitchEdge','glitchFrozenNoise','glitchFreshNoise','glitchNoiseColor','glitchFold','glitchDrive','glitchCutoff','glitchResonance','glitchMotionTarget','glitchMotion','glitchMotionDecay','glitchAttack','glitchHold','glitchDecay','glitchSource2','glitchMaterial2','glitchTuning2','glitchNote2','glitchSize2','glitchPosition2','glitchEdge2','glitchFrozenNoise2','glitchOscMix']
g_ranges=[(0,2),(0,7),(0,1),(0,127),(1/128,1),(0,1),(0,1),(0,1),(0,1),(-1,1),(0,1),(0,1),(20,18000),(0,1),(0,3),(-1,1),(.005,8),(.0005,2),(0,4),(.015,16),(0,2),(0,7),(0,1),(0,127),(1/128,1),(0,1),(0,1),(0,1),(0,1)]
def tables(keys,ranges,values,kind):
 physical=[];units=[];codes=[]
 for i,(key,(lo,hi)) in enumerate(zip(keys,ranges)):
  maximum=int(hi) if (kind=='g' and i in [0,1,2,3,14,20,21,22,23]) or (kind=='a' and i in [0,1,2,3,27]) else 127
  log=(kind=='g' and i in [4,12,16,17,19,24]) or(kind=='a' and i in [9,26])
  row=[min(hi,q) for q in range(128)] if maximum!=127 else [lo*(64-q)/64 if q<64 else hi*(q-64)/63 for q in range(128)] if lo<0 and lo==-hi else [lo*(hi/lo)**(q/127) if log else lo+(hi-lo)*q/127 for q in range(128)]
  v=values[key];q=min(range(maximum+1),key=lambda n:abs(row[n]-v));row[q]=v;codes.append(q);physical.append(row)
  # All scalars Q15 except frequencies in Hz, notes in cents and times in frames.
  freq=(kind=='a' and i==9)or(kind=='g' and i==12)
  note=(kind=='a' and i in [0,1,24])or(kind=='g' and i in [3,23])
  time=(kind=='a' and i in [15,20,23,26])or(kind=='g' and i in [16,17,18,19])
  choice=(kind=='a' and i in [2,3,27])or(kind=='g' and i in [0,1,2,14,20,21,22])
  units.append([round(x if freq or choice else x*100 if note else x*44100 if time else x*32768) for x in row])
 return physical,units,codes
ap,au,ai=tables(a_keys,a_ranges,defaults,'a');gp,gu,gi=tables(g_keys,g_ranges,ginit,'g')
def arr(name,data,typ='int32_t'):
 def shape(a):return '['+str(len(a))+']'+(shape(a[0]) if isinstance(a[0],list) else '')
 def val(a):return '{'+','.join(val(v) if isinstance(v,list) else str(v) for v in a)+'}'
 return f'static const {typ} {name}{shape(data)}='+val(data)+';\n'
s='/* Generated app-derived synth controls. */\n#pragma once\n#include <stdint.h>\n'
for name,data,typ in [('an_units',au,'int32_t'),('gl_units',gu,'int32_t'),('an_init',ai,'uint8_t'),('gl_init',gi,'uint8_t')]:s+=arr('gd_'+name,data,typ)
s+=arr('gd_an_time',[[round(.01*250**x*44100) for x in ap[i]] for i in [8,12,14]],'uint32_t')
for factor in [1,2,4]:s+=arr('gd_ladder_g'+str(factor),[round((lambda g:g/(1+g))(math.tan(math.pi*i*16/(44100*factor)))*2**24) for i in range(1243)],'uint32_t')
s+=arr('gd_gl_note_inc',[[round(440*2**((v-69)/12)*2**32/176400) for v in gp[i]] for i in [3,23]],'uint32_t')
levels=[]
for target in [.9,defaults['level']]:
 row=[q/127 for q in range(128)];code=min(range(128),key=lambda q:abs(row[q]-target));row[code]=target;levels.append([round(v*32768) for v in row])
s+=arr('gd_synth_level',levels,'uint16_t')
s+=arr('gd_an_fine',[round(2**(x/12)*65536) for x in ap[16]],'uint32_t')
s+=arr('gd_an_drive_norm',[round(32768/math.tanh(x)) for x in ap[18]],'uint32_t')
s+='static const char *const gd_an_names[29]={'+','.join('"'+x+'"' for x in a_names)+'};\n'
(ROOT/'src/synth_tables.h').write_text(s)
# Export the app's actual immutable synthetic materials, rather than rebuilding
# approximate formulas in Python. Host-only C++ construction; target is Q15 ROM.
cpp='#include "GlitchVoice.hpp"\n#include <cstdio>\nint main(int argc,char**argv){if(argc!=2)return 1;GlitchDrums::GlitchDSP::GlitchSourceTables t;FILE*f=fopen(argv[1],"wb");for(auto &a:t.materials)fwrite(a.data(),4,a.size(),f);fwrite(t.noiseTape.data(),4,t.noiseTape.size(),f);fclose(f);}\n'
(BUILD/'glitch-materials.cpp').write_text(cpp)
subprocess.run(['clang++','-std=c++20','-O2','-I',str(REPO/'GlitchDrumsExtension/DSP'),str(BUILD/'glitch-materials.cpp'),'-o',str(BUILD/'glitch-materials')],check=True)
subprocess.run([str(BUILD/'glitch-materials'),str(BUILD/'glitch-materials.f32')],check=True)
f=array.array('f');f.frombytes((BUILD/'glitch-materials.f32').read_bytes());material=[[max(-32767,min(32767,round(x*32768))) for x in f[i*4096:(i+1)*4096]] for i in range(9)]
coeff=[]
for i in range(129):
 m=i-64;sinc=.21 if m==0 else math.sin(2*math.pi*.105*m)/(math.pi*m);coeff.append(sinc*(.42-.5*math.cos(2*math.pi*i/128)+.08*math.cos(4*math.pi*i/128)))
total=sum(coeff);coeff=[round(v/total*2**30) for v in coeff]
s='/* App GlitchSourceTables, generated on host and quantized to Q15. */\n#pragma once\n#include <stdint.h>\n'+arr('gd_gl_material',material,'int16_t')+arr('gd_gl_fir',coeff)
for factor in [1,2]:
 count=32*factor+1;values=[]
 for i in range(count):
  m=i-count//2;cutoff=.42/factor;sinc=2*cutoff if m==0 else math.sin(2*math.pi*cutoff*m)/(math.pi*m)
  values.append(sinc*(.42-.5*math.cos(2*math.pi*i/(count-1))+.08*math.cos(4*math.pi*i/(count-1))))
 total=sum(values);s+=arr('gd_gl_fir'+str(factor),[round(v/total*2**30) for v in values])
(ROOT/'src/glitch_materials.h').write_text(s)
(ROOT/'docs/SYNTH_INIT.json').write_text(json.dumps({'analog':{'fields':a_keys,'init':defaults,'control_bytes':ai},'glitch':{'fields':g_keys,'init':ginit,'control_bytes':gi}},indent=2)+'\n')
print('Generated app-derived ANALOG/GLITCH controls, ladder and original material tables')
