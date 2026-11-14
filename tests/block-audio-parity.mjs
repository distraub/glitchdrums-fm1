// Compare old/new RemoteFM1 packages after a render-scheduling change.
// Usage: node tests/block-audio-parity.mjs old.rfm1 new.rfm1
import fs from 'node:fs';import assert from 'node:assert/strict';import{createHash}from'node:crypto';
const base=new URL('../',import.meta.url).pathname;const O=JSON.parse(fs.readFileSync(base+'build/test-release/fullfx-layout.json'));
const mods=process.argv.slice(2).map(p=>new WebAssembly.Module(fs.readFileSync(p)));
function boot(mod,image){const x=new WebAssembly.Instance(mod).exports,m=new DataView(x.memory.buffer),d=x.rfm1_get(6),panel=m.getUint32(d+x.rfm1_layout(30),true),h={x,m,at:i=>panel+x.rfm1_layout(i),tick:0};if(image){const ptr=x.rfm1_alloc(image.length);new Uint8Array(x.memory.buffer,ptr,image.length).set(image);assert.equal(x.rfm1_flash_stage(ptr,image.length),1);x.rfm1_free(ptr);}x.rfm1_init(0);h.ptr=x.rfm1_alloc(512);return h;}
function button(h,k){h.m.setUint32(h.at(2),1<<k,true);h.x.rfm1_tick(++h.tick);h.m.setUint32(h.at(2),0,true);h.x.rfm1_tick(++h.tick);}
function turn(h,k,delta){h.m.setInt32(h.at(5)+k*4,delta,true);h.x.rfm1_tick(++h.tick);}
function key(h,k,down){h.m.setUint32(h.at(1),down?1<<k:0,true);h.x.rfm1_tick(++h.tick);}
function flash(h){const d=h.x.rfm1_get(6),n=h.m.getUint32(d+h.x.rfm1_layout(26),true);return new Uint8Array(h.x.memory.buffer,h.x.rfm1_flash(),n).slice();}
const fresh=boot(mods[0]);fresh.x.rfm1_audio(fresh.ptr,128);button(fresh,9);const original=flash(fresh);
function fixture(name){const image=original.slice(),raw=fs.readFileSync(base+'build/test-release/'+name);new DataView(image.buffer).setUint32(4,raw.length,true);image.set(raw,8);return image;}
function patternRatchets(image){let bit=0;const b=image.subarray(8+O.pattern,8+O.pattern+672);const put=(value,n)=>{for(let i=0;i<n;++i,++bit){const mask=1<<(bit&7);b[bit>>3]=(b[bit>>3]&~mask)|(((value>>i)&1)?mask:0);}};
 for(let lane=0;lane<8;++lane){put(15,4);for(let step=0;step<16;++step){put(100,7);put(100,7);put(step%4,2);put(48+(step%13)-6,7);put(29,6);put(50+step*3,7);}}
 for(let lane=0;lane<2;++lane)for(let step=0;step<16;++step){put(step%4==0?3:0,2);for(const pitch of [52,55,60])put(pitch,7);}return image;}
for(const scenario of ['stock','ratchets16','ratchets8','dense','punch']){
 let image=scenario==='dense'?fixture('dense-fullfx.bin'):scenario.startsWith('ratchets')?patternRatchets(original.slice()):original.slice();
 if(scenario.startsWith('ratchets'))image[16]=(scenario.endsWith('8')?128:0)|73;
 const hosts=mods.map(m=>boot(m,image));for(const h of hosts){if(scenario==='punch')button(h,2);button(h,12);}
 const sizes=[1,7,128,3,31,64,127,16];const hash=createHash('sha256');
 for(let block=0;block<3000;++block){const n=sizes[block%sizes.length];
  for(const h of hosts){if(scenario==='punch'&&block%100===0){key(h,(block/100)%27,block%200===0);}if(block===1100){button(h,7);turn(h,5,13);}if(block===2200)button(h,12);h.x.rfm1_audio(h.ptr,n);}
  const chunks=hosts.map(h=>new Uint8Array(h.x.memory.buffer,h.ptr,n*4));assert.deepEqual(chunks[1],chunks[0],scenario+' block '+block);hash.update(chunks[0]);
 }
 console.log('PASS',scenario,hash.digest('hex'));
}
