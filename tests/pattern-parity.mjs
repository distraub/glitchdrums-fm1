// Artifact comparison: unchanged audio in HOME / pattern browser, host cadence.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
const baseline=process.argv[2]||new URL('../build/test-release/glitchdrums-0.10.8.rfm1',import.meta.url);
const candidate=process.argv[3]||new URL('../build/test-release/glitchdrums-0.13.0.rfm1',import.meta.url);
function render(path,bank=false){
 const e=new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(path))).exports,m=new DataView(e.memory.buffer);
 e.rfm1_flash_stage(0,0);e.rfm1_init(0);
 const d=e.rfm1_get(6),panel=m.getUint32(d+e.rfm1_layout(30),true),at=i=>panel+e.rfm1_layout(i);
 let ms=0,frameAt=0;
 const button=k=>{m.setUint32(at(2),1<<k,true);e.rfm1_tick(++ms);m.setUint32(at(2),0,true);e.rfm1_tick(++ms);};
 // Match the intentional new dry default on the historical baseline too.
 button(2);button(2);m.setInt32(at(5)+2*4,-127,true);e.rfm1_tick(++ms);button(8);
 // Both runs receive identical inputs; no notes or edits on either SEQ page.
 if(bank){button(11);button(11);}
 button(12);
 const start=ms,ptr=e.rfm1_alloc(512),hash=createHash('sha256');
 for(let block=0;block<1379;++block){
  const until=start+Math.floor(block*128*1000/44100);
  while(ms<until)e.rfm1_tick(++ms);
  if(ms-frameAt>=20){e.rfm1_frame();frameAt=ms;}
  e.rfm1_audio(ptr,128);hash.update(new Uint8Array(e.memory.buffer,ptr,512));
 }
 return hash.digest('hex');
}
const expected=render(baseline);
assert.equal(render(candidate),expected);
assert.equal(render(candidate,true),expected);
console.log(`PASS: four-second sequencer PCM identical to delay-disabled baseline, including pattern-browser playback (${expected})`);
