// Output-level regression at default, mute and maximum master/drive.
import assert from 'node:assert/strict';
import fs from 'node:fs';
const mod=new WebAssembly.Module(fs.readFileSync(new URL('../build/test-release/glitchdrums-0.15.0.rfm1',import.meta.url)));
for(const mode of ['default','silent','maximum']){
 const x=new WebAssembly.Instance(mod).exports,m=new DataView(x.memory.buffer);
 const d=x.rfm1_get(6),p=m.getUint32(d+x.rfm1_layout(30),true),at=i=>p+x.rfm1_layout(i);let tick=0;
 x.rfm1_flash_stage(0,0);x.rfm1_init(0);
 const button=k=>{m.setUint32(at(2),1<<k,true);x.rfm1_tick(++tick);m.setUint32(at(2),0,true);x.rfm1_tick(++tick);};
 const turn=(k,v)=>{m.setInt32(at(5)+k*4,v,true);x.rfm1_tick(++tick);};
 if(mode!=='default'){m.setInt32(at(6),mode==='silent'?0:1023,true);x.rfm1_tick(++tick);}
 if(mode==='maximum'){
  turn(2,127);turn(2,-17); // kick 110/127 = 86 percent
  button(2);button(2);turn(2,-127); // delay off
  button(2);turn(4,127); // drive full
 }
 button(12);
 const ptr=x.rfm1_alloc(512);let peak=0,sum=0,n=0,rails=0;
 for(let block=0;block<2068;++block){
  if(mode==='maximum'&&block%100===0)for(let lane=0;lane<8;++lane){x.rfm1_midi_in((127<<24)|(60<<16)|((0x90+lane)<<8)|9);x.rfm1_tick(++tick);}
  x.rfm1_audio(ptr,128);
  for(const v of new Int16Array(x.memory.buffer,ptr,256)){peak=Math.max(peak,Math.abs(v));sum+=v*v;++n;if(Math.abs(v)>=32767)++rails;}
 }
 const rms=Math.sqrt(sum/n),db=20*Math.log10(rms/32768);
 assert(peak<=28000);assert.equal(rails,0);
 if(mode==='silent')assert.equal(peak,0);
 if(mode==='default'){assert(db>-23&&db<-19);assert(peak>16000&&peak<23000);}
 if(mode==='maximum')assert(peak>27000);
 console.log(`PASS: ${mode}: RMS ${db.toFixed(2)} dBFS, peak ${peak}, ${rails} PCM rail samples`);
}
