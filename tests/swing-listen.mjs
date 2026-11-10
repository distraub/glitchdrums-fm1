// Reproduce the default-pattern swing report using the actual RemoteFM1 ABI.
// No normalization or effect changes: every file uses the same gain and seed.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
const version=JSON.parse(fs.readFileSync(new URL('../firmware.json',import.meta.url))).version;
const input=process.argv[2]||new URL(`../build/test-release/glitchdrums-${version}.rfm1`,import.meta.url);
const out=path.resolve(process.argv[3]||new URL('../build/swing-comparison/',import.meta.url).pathname);
fs.mkdirSync(out,{recursive:true});
const module=new WebAssembly.Module(fs.readFileSync(input)),rate=44100,bar=16*Math.floor(rate*60/(120*4));
function render(swing,drumsOnly=false){
 const x=new WebAssembly.Instance(module).exports,m=new DataView(x.memory.buffer),d=x.rfm1_get(6),p=m.getUint32(d+x.rfm1_layout(30),true),at=i=>p+x.rfm1_layout(i);let tick=0;
 x.rfm1_flash_stage(0,0);x.rfm1_init(0);const ptr=x.rfm1_alloc(512);
 const button=k=>{m.setUint32(at(2),1<<k,true);x.rfm1_tick(++tick);m.setUint32(at(2),0,true);x.rfm1_tick(++tick);};
 const turn=(k,v)=>{m.setInt32(at(5)+k*4,v,true);x.rfm1_tick(++tick);};
 if(drumsOnly){turn(0,6);button(3);turn(0,1);button(3);}
 button(7);turn(3,swing);button(12);
 const pcm=Buffer.alloc(bar*4*4);
 for(let n=0;n<bar*4;){const count=Math.min(128,bar*4-n);x.rfm1_audio(ptr,count);const samples=new Int16Array(x.memory.buffer,ptr,count*2);for(let i=0;i<samples.length;++i)pcm.writeInt16LE(samples[i],n*4+i*2);n+=count;}
 return pcm;
}
function wav(name,pcm){const h=Buffer.alloc(44);h.write('RIFF');h.writeUInt32LE(pcm.length+36,4);h.write('WAVEfmt ',8);h.writeUInt32LE(16,16);h.writeUInt16LE(1,20);h.writeUInt16LE(2,22);h.writeUInt32LE(rate,24);h.writeUInt32LE(rate*4,28);h.writeUInt16LE(4,32);h.writeUInt16LE(16,34);h.write('data',36);h.writeUInt32LE(pcm.length,40);fs.writeFileSync(path.join(out,name),Buffer.concat([h,pcm]));}
const straight=render(0),swung=render(50),drums0=render(0,true),drums50=render(50,true);
wav('default-swing-0.wav',straight);wav('default-swing-50.wav',swung);
wav('default-AB-0-50-0-50.wav',Buffer.concat([straight.subarray(0,bar*8),swung.subarray(0,bar*8),straight.subarray(0,bar*8),swung.subarray(0,bar*8)]));
wav('drums-only-swing-0.wav',drums0);wav('drums-only-swing-50.wav',drums50);
let a=0,b=0,diff=0,cross=0,peakA=0,peakB=0;
for(let i=0;i<straight.length;i+=2){const x=straight.readInt16LE(i),y=swung.readInt16LE(i);a+=x*x;b+=y*y;diff+=(x-y)**2;cross+=x*y;peakA=Math.max(peakA,Math.abs(x));peakB=Math.max(peakB,Math.abs(y));}
const report={firmware:version,tempo:120,bars:4,seconds:bar*4/rate,AB:'two bars at 0, two bars at 50, repeat',fullMixIdentical:straight.equals(swung),swingBasis:'1/8',drumsOnlyIdentical:drums0.equals(drums50),correlation:cross/Math.sqrt(a*b),differenceRmsRelativeToStraight:Math.sqrt(diff/a),peakA,peakB};
assert(!report.drumsOnlyIdentical,'Eighth-note swing must change the default acoustic drum groove');
fs.writeFileSync(path.join(out,'comparison.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));console.log(out);
