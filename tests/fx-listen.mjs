// Repeatable audio comparison: dry / Glitchstortion / fold, same demo input.
import fs from 'node:fs';
const bytes=fs.readFileSync(process.argv[2] || new URL('../build/test-release/glitchdrums-0.15.0.rfm1',import.meta.url));
const module=new WebAssembly.Module(bytes);
const seconds=6,frames=44100*seconds,all=[],results={};
for(const effect of ['dry','antiverb','glitchstortion','graincloud','stack','samplealias','ring','fold','flanger','wobble']){
 const e=new WebAssembly.Instance(module).exports,m=new DataView(e.memory.buffer);
 const d=e.rfm1_get(6),p=m.getUint32(d+e.rfm1_layout(30),true),at=i=>p+e.rfm1_layout(i);let tick=0;
 e.rfm1_flash_stage(0,0);e.rfm1_init(0);
 const button=k=>{m.setUint32(at(2),1<<k,true);e.rfm1_tick(++tick);m.setUint32(at(2),0,true);e.rfm1_tick(++tick);};
 button(12);
 if(effect!=='dry'){
  button(2);
  
  const id={antiverb:27,glitchstortion:28,graincloud:29,samplealias:12,ring:20,fold:19,flanger:21,wobble:15}[effect];
  const effectKeys=[0,27,1,28,2,29,3,4,19,5,20,6,21,7,8,22,9,23,10,11,24,12,25,13,14,26,15];
  const keyMask=effect==='stack'?42:1<<effectKeys.indexOf(id);
  m.setUint32(at(1),keyMask,true);e.rfm1_tick(++tick);
 }
 const ptr=e.rfm1_alloc(512),pcm=Buffer.alloc(frames*4);let sum=0,peak=0,dc=0;
 for(let offset=0;offset<frames;offset+=128){
  const n=Math.min(128,frames-offset);e.rfm1_audio(ptr,n);const a=new Int16Array(e.memory.buffer,ptr,n*2);
  for(let i=0;i<a.length;++i){const v=a[i];if(Math.abs(v)>28000)throw Error(`${effect} exceeds the output ceiling`);pcm.writeInt16LE(v,(offset*2+i)*2);if(offset>44100){sum+=v*v;peak=Math.max(peak,Math.abs(v));dc+=v;}}
 }
 const n=(frames-44101)*2;
 results[effect]={rms:Math.sqrt(sum/n),peak,dc:dc/n};
 console.log(effect,JSON.stringify(results[effect]));
 all.push(pcm);const h=Buffer.alloc(44);h.write('RIFF');h.writeUInt32LE(pcm.length+36,4);h.write('WAVEfmt ',8);h.writeUInt32LE(16,16);h.writeUInt16LE(1,20);h.writeUInt16LE(2,22);h.writeUInt32LE(44100,24);h.writeUInt32LE(176400,28);h.writeUInt16LE(4,32);h.writeUInt16LE(16,34);h.write('data',36);h.writeUInt32LE(pcm.length,40);
 fs.writeFileSync(new URL(`../build/test-release/${effect}-comparison.wav`,import.meta.url),Buffer.concat([h,pcm]));
}

const combined=Buffer.concat(all),header=Buffer.alloc(44);
header.write('RIFF');header.writeUInt32LE(combined.length+36,4);header.write('WAVEfmt ',8);
header.writeUInt32LE(16,16);header.writeUInt16LE(1,20);header.writeUInt16LE(2,22);
header.writeUInt32LE(44100,24);header.writeUInt32LE(176400,28);header.writeUInt16LE(4,32);header.writeUInt16LE(16,34);
header.write('data',36);header.writeUInt32LE(combined.length,40);
fs.writeFileSync(new URL('../build/test-release/fx-comparison-0.13.0.wav',import.meta.url),Buffer.concat([header,combined]));
if(!process.argv[2]){
 const dry=results.dry,glitch=results.glitchstortion,fold=results.fold;
 if(results.ring.rms>dry.rms*.8||results.ring.peak>dry.peak*1.01)throw Error("Ring default level exceeds its calibrated range");
 for(const effect of ['antiverb','graincloud','stack'])if(Math.abs(results[effect].dc)>results[effect].rms*0.06)throw Error(`${effect} DC bias`);
 for(const [effect,result] of Object.entries(results)){if(!Number.isFinite(result.rms)||result.peak>28000)throw Error(`${effect} exceeds output bounds`);}
 if(fold.rms>dry.rms*1.05 || fold.peak>dry.peak*1.05)throw Error('Fold level exceeds dry');
 if(Math.abs(glitch.dc-dry.dc)>glitch.rms*0.02)throw Error('Glitchstortion has a DC offset');
 if(glitch.rms<dry.rms*0.3||glitch.rms>dry.rms*1.5)throw Error('Glitchstortion level mismatch');
 console.log('PASS: demo fold loudness and Glitchstortion DC/level regression');
}
