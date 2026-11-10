import fs from 'node:fs';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
const root=new URL('../build/test-release/',import.meta.url);
const O=JSON.parse(fs.readFileSync(new URL('fullfx-layout.json',root)));
const effectKeys=[0,27,1,28,2,29,3,4,19,5,20,6,21,7,8,22,9,23,10,11,24,12,25,13,14,26,15];
const module=new WebAssembly.Module(fs.readFileSync(process.argv[2]||new URL('glitchdrums-'+JSON.parse(fs.readFileSync(new URL('../firmware.json',import.meta.url))).version+'.rfm1',root)));
assert.deepEqual(WebAssembly.Module.imports(module),[]);
const manifest=JSON.parse(new TextDecoder().decode(WebAssembly.Module.customSections(module,'rfm1')[0]));assert.equal(manifest.abi,6);assert.equal(manifest.id,'glitchdrums');
function boot(flash){const x=new WebAssembly.Instance(module).exports,m=new DataView(x.memory.buffer),d=x.rfm1_get(6),panel=m.getUint32(d+x.rfm1_layout(30),true),h={x,m,at:i=>panel+x.rfm1_layout(i),tick:0};
 assert.equal(x.memory.buffer.byteLength,8*1024*1024);assert.throws(()=>x.memory.grow(1));
 if(flash){const ptr=x.rfm1_alloc(flash.length);new Uint8Array(x.memory.buffer,ptr,flash.length).set(flash);assert.equal(x.rfm1_flash_stage(ptr,flash.length),1);x.rfm1_free(ptr);}else x.rfm1_flash_stage(0,0);
 x.rfm1_init(0);h.ptr=x.rfm1_alloc(512);return h;}
function button(h,k){h.m.setUint32(h.at(2),1<<k,true);h.x.rfm1_tick(++h.tick);h.m.setUint32(h.at(2),0,true);h.x.rfm1_tick(++h.tick);}
function combo(h,a,b){h.m.setUint32(h.at(2),(1<<a)|(1<<b),true);h.x.rfm1_tick(++h.tick);h.m.setUint32(h.at(2),0,true);h.x.rfm1_tick(++h.tick);}
function turn(h,k,delta){h.m.setInt32(h.at(5)+k*4,delta,true);h.x.rfm1_tick(++h.tick);}
function key(h,k,down){h.m.setUint32(h.at(1),down?1<<k:0,true);h.x.rfm1_tick(++h.tick);}
function tap(h,k){key(h,k,true);key(h,k,false);}
function render(h,n=1536){while(n){const f=Math.min(n,128);h.x.rfm1_audio(h.ptr,f);n-=f;}h.x.rfm1_tick(++h.tick);}
function flash(h){const d=h.x.rfm1_get(6),n=h.m.getUint32(d+h.x.rfm1_layout(26),true);return new Uint8Array(h.x.memory.buffer,h.x.rfm1_flash(),n).slice();}
function save(h){render(h);button(h,9);return flash(h);}
function payload(f){return f.slice(8,8+new DataView(f.buffer,f.byteOffset,f.byteLength).getUint32(4,true));}
function migratedPayload(f){const p=payload(f);assert.equal(p.length,O.size);return p;}
function fixture(base,name){const f=base.slice(),bytes=fs.readFileSync(new URL(name,root));new DataView(f.buffer).setUint32(4,bytes.length,true);f.set(bytes,8);return f;}
function readPattern(f,slot){const bytes=f.subarray(8+O.pattern+slot*672,8+O.pattern+(slot+1)*672);let bit=0;const read=n=>{let v=0;for(let i=0;i<n;++i,++bit)v|=((bytes[bit>>3]>>(bit&7))&1)<<i;return v;};const p={lane:[],chord:[]};
 for(let lane=0;lane<8;++lane){const t={length:read(4)+1,step:[]};for(let s=0;s<16;++s)t.step.push({velocity:read(7),chance:read(7),ratchet:read(2)+1,pitch:read(7)-48,lock:read(6),value:read(7)});p.lane.push(t);}
 for(let lane=0;lane<2;++lane){const steps=[];for(let s=0;s<16;++s){const count=read(2),notes=[read(7)-48,read(7)-48,read(7)-48];steps.push(notes.slice(0,count));}p.chord.push(steps);}assert.equal(bit,672*8);return p;}
function writePattern(f,slot,p){const bytes=new Uint8Array(672);let bit=0;const write=(v,n)=>{for(let i=0;i<n;++i,++bit)if(v&(1<<i))bytes[bit>>3]|=1<<(bit&7);};
 for(const t of p.lane){write(t.length-1,4);for(const s of t.step){write(s.velocity,7);write(s.chance,7);write(s.ratchet-1,2);write(s.pitch+48,7);write(s.lock,6);write(s.value,7);}}
 for(const lane of p.chord)for(const notes of lane){write(notes.length,2);for(let n=0;n<3;++n)write(48+(notes[n]??0),7);}assert.equal(bit,672*8);f.set(bytes,8+O.pattern+slot*672);}
const active=f=>f[8+6];
function bits(bytes){let at=0;return n=>{let v=0;for(let i=0;i<n;++i,++at)v|=((bytes[at>>3]>>(at&7))&1)<<i;return v;};}
function sound(f,lane){const read=bits(f.subarray(8+12+lane*33,8+12+(lane+1)*33)),out=new Uint8Array(40);for(let n=0;n<32;++n)out[n]=read(7);for(let n=0;n<2;++n){out[32+n*4]=read(3);out[33+n*4]=read(4);out[34+n*4]=read(7);out[35+n*4]=read(4);}return out;}
function scene(f){const read=bits(f.subarray(8+12+264,8+12+373)),s={fx:[],punch:[],effect:Array(30).fill(null)};for(let n=0;n<12;++n)s.fx.push(read(7));for(let n=0;n<4;++n)s.punch.push(read(7));for(const effect of effectKeys)s.effect[effect]=[read(7),read(7),read(7),read(7)];return s;}
const whites=[0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26];
function bank(h){button(h,11);button(h,11);button(h,11);}
function screenshot(h,name){h.tick+=2000;h.x.rfm1_tick(h.tick);h.x.rfm1_frame();fs.writeFileSync(new URL(name,root),new Uint8Array(h.x.memory.buffer,h.at(11),240*240*2));}
const fresh=boot();render(fresh);assert(new Int16Array(fresh.x.memory.buffer,fresh.ptr,256).every(v=>v===0));
const baseline=save(fresh);assert.equal(payload(baseline).length,12268);assert.equal(active(baseline),0);
button(fresh,7);turn(fresh,2,1);const themed=save(fresh);assert.notEqual(themed[15],baseline[15]);assert.equal(save(boot(themed))[15],themed[15]);
turn(fresh,2,1);fresh.x.rfm1_flash_sync();assert.equal(flash(fresh)[15],themed[15]);
button(fresh,12);render(fresh);button(fresh,9);const liveSave=flash(fresh);assert.equal(payload(liveSave).length,12268);assert.deepEqual(payload(save(boot(liveSave))),payload(liveSave));
console.log('PASS: ABI6 fixed 8MiB, silent boot, explicit live SAVE, fresh-instance restore and unsaved-edit semantics');

const empty=fixture(baseline,'empty-fullfx.bin'),dense=fixture(baseline,'dense-fullfx.bin');
const denseHost=boot(dense);assert.deepEqual(payload(save(denseHost)),migratedPayload(dense));
const denseReload=boot(flash(denseHost));assert.deepEqual(payload(save(denseReload)),migratedPayload(dense));
for(let slot=0;slot<16;++slot){const p=readPattern(flash(denseReload),slot);assert.equal(p.lane[0].step[0].velocity,100+slot);assert.equal(p.chord[0][0].length,3);}
console.log('PASS: all sixteen fully populated chord/p-lock patterns and full custom library plus chain/MIDI maps save losslessly within 12268 bytes');

const slots=boot(empty);bank(slots);const expected=[];
for(let slot=0;slot<16;++slot){tap(slots,whites[slot]);render(slots);button(slots,11);button(slots,11);tap(slots,0);turn(slots,3,slot);turn(slots,4,-slot); // velocity/chance
 button(slots,6);key(slots,0,true);turn(slots,2,1);key(slots,0,false); // lock the selected kick parameter
 button(slots,11);button(slots,11);button(slots,11);const f=save(slots);expected.push(readPattern(f,slot));assert.equal(expected[slot].lane[0].step[0].velocity,100+slot);assert.equal(expected[slot].lane[0].step[0].lock,0);
}
screenshot(slots,'patterns-0.13.0.rgb565');const slotReload=boot(flash(slots));bank(slotReload);
for(let slot=0;slot<16;++slot){tap(slotReload,whites[slot]);render(slotReload);const f=save(slotReload);assert.equal(active(f),slot);assert.deepEqual(readPattern(f,slot),expected[slot]);}
combo(slotReload,4,10);assert.deepEqual(readPattern(save(slotReload),15),expected[15]); // active slot cannot be cleared
turn(slotReload,0,-1);combo(slotReload,4,10);assert(readPattern(save(slotReload),14).lane.every(t=>t.step.every(s=>s.velocity===0)));
combo(slotReload,4,3);assert.deepEqual(readPattern(save(slotReload),14),expected[15]);
button(slotReload,11);button(slotReload,11);button(slotReload,1);turn(slotReload,3,1);assert.equal(readPattern(save(slotReload),15).lane[0].step[0].velocity,116); // OCT cannot move to hidden steps
console.log('PASS: A-P white-key selection, per-slot edits/p-locks, copy/clear protection, reload and fixed 16-step editing');

const launch=empty.slice(),a=readPattern(launch,0),b=readPattern(launch,1);b.lane[0].step[0].velocity=127;writePattern(launch,1,b);
const quant=boot(launch);bank(quant);button(quant,12);render(quant,128);tap(quant,2);render(quant,16*5512-128);button(quant,9);assert.equal(active(flash(quant)),0);
render(quant,1);button(quant,9);assert.equal(active(flash(quant)),1);
const instant=boot(launch);bank(instant);button(instant,12);render(instant,128);tap(instant,2);render(instant,128);tap(instant,2);render(instant,1);button(instant,9);assert.equal(active(flash(instant)),1);
const stopped=boot(launch);bank(stopped);button(stopped,12);render(stopped,128);tap(stopped,2);render(stopped,128);button(stopped,12);render(stopped,1);button(stopped,9);assert.equal(active(flash(stopped)),1);
console.log('PASS: sample-exact 16-step queued launch, double-tap immediate launch and STOP adoption');

for(const lane of [6,7])for(const input of ['keys','midi']){
 const h=boot(empty);turn(h,0,lane);button(h,12);render(h,128);button(h,13);
 for(const pitch of [0,4,7,12,4,16]){if(input==='keys')tap(h,pitch);else{h.x.rfm1_midi_in((100<<24)|((60+pitch)<<16)|((0x90+lane)<<8)|9);h.x.rfm1_tick(++h.tick);h.x.rfm1_midi_in(((60+pitch)<<16)|((0x80+lane)<<8)|8);h.x.rfm1_tick(++h.tick);}}
 render(h,128);button(h,13);button(h,12);const f=save(h);assert.deepEqual(readPattern(f,0).chord[lane-6][0],[4,7,12]);assert.deepEqual(readPattern(save(boot(f)),0),readPattern(f,0));
 bank(h);turn(h,0,15);combo(h,4,3);button(h,3);render(h);assert.deepEqual(readPattern(save(h),15),readPattern(f,0));
 button(h,11);button(h,11);tap(h,0);assert.deepEqual(readPattern(save(h),15).chord[lane-6][0],[]);
 const mono=f.slice(),p=readPattern(mono,0);p.chord[lane-6][0]=[];writePattern(mono,0,p);
 function replay(image){const r=boot(image),hash=createHash('sha256');button(r,12);for(let n=0;n<345;++n){render(r,128);const pcm=new Int16Array(r.x.memory.buffer,r.ptr,256);for(const v of pcm)assert(Math.abs(v)<=28000);hash.update(new Uint8Array(r.x.memory.buffer,r.ptr,512));}return hash.digest('hex');}
 assert.notEqual(replay(f),replay(mono));
}
console.log('PASS: Glitch/Analog four-note key/MIDI recording, dedup/limit, sequencer chord audio, copy to P, clear and reload');

const fx=boot(empty);button(fx,2);let currentKey=3;
for(const effect of effectKeys){const keyIndex=effectKeys.indexOf(effect);turn(fx,1,keyIndex-currentKey);currentKey=keyIndex;button(fx,6);
 const before=scene(save(fx)).effect[effect];for(let knob=0;knob<4;++knob){turn(fx,2+knob,before[knob]>=64?-1:1);render(fx);}
 const f=save(fx),after=scene(f).effect[effect];for(let knob=0;knob<4;++knob)assert.notEqual(after[knob],before[knob],`effect ${effect} knob ${knob}`);
 assert.deepEqual(scene(save(boot(f))),scene(f));button(fx,6);
}
screenshot(fx,'fx-keyboard-0.13.0.rgb565');button(fx,6);screenshot(fx,'fx-editor-0.13.0.rgb565');
console.log('PASS: all 27 fixed-key effects, 108 live editor controls and full-keyboard parameter persistence');

// A custom record can replace a factory entry or occupy a separate user entry.
const custom=boot(empty),original=sound(save(custom),0);button(custom,6);turn(custom,2,1);const edited=sound(save(custom),0);
turn(custom,6,1);turn(custom,6,-1);button(custom,9);let cf=flash(custom);assert.equal(cf[8+O.voiceUsed],1);assert.equal(cf[8+O.voiceBind],0);
let cr=boot(cf);button(cr,6);turn(cr,6,1);turn(cr,6,-1);button(cr,3);assert.deepEqual(sound(flashAfterProjectSave(cr),0),edited);
function flashAfterProjectSave(h){button(h,8);return save(h);}
button(cr,6);turn(cr,6,1);turn(cr,6,-1);combo(cr,4,9);render(cr);assert.deepEqual(sound(flash(cr),0),original);assert.equal(flash(cr)[8+O.voiceUsed],0);
// Fill both custom records at F2/F3, then ensure a third overwrite cannot evict.
turn(cr,6,1);button(cr,9);turn(cr,6,1);button(cr,9);const two=flash(cr),generation=cr.x.rfm1_flash_dirty();turn(cr,6,1);button(cr,9);assert.equal(cr.x.rfm1_flash_dirty(),generation);assert.deepEqual(flash(cr),two);
turn(cr,6,-1);combo(cr,4,9);turn(cr,6,1);button(cr,9);assert(cr.x.rfm1_flash_dirty()>generation);
screenshot(cr,'custom-preset-0.13.0.rgb565');
for(const type of [1,2]){const h=boot(empty);if(type===2)button(h,2);turn(h,6,1);turn(h,6,-1);button(h,9);const f=flash(h);assert.equal(f[8+(type===1?O.kitUsed:O.sceneUsed)],1);assert.equal(f[8+(type===1?O.kitBind:O.sceneBind)],0);assert.deepEqual(payload(flashAfterProjectSave(boot(f))),payload(f));combo(h,4,9);assert.equal(flash(h)[8+(type===1?O.kitUsed:O.sceneUsed)],0);}
console.log('PASS: factory voice/kit/FX overwrite mappings, original restoration, shared custom capacity and no silent eviction');

const migrated=boot(fixture(baseline,'legacy-eight.bin')),mf=save(migrated);assert.equal(readPattern(mf,0).lane[0].step[0].velocity,110);assert.equal(readPattern(mf,8).lane[0].step[0].velocity,99);assert.equal(readPattern(mf,1).lane[0].step[1].velocity,81);assert.equal(mf[8+O.voiceUsed],1);assert.equal(mf[8+O.voiceBind],8);
const overflow=boot(fixture(baseline,'legacy-overflow.bin'));button(overflow,9);assert.equal(overflow.x.rfm1_flash_dirty(),0);assert.equal(new DataView(flash(overflow).buffer).getUint32(4,true),12276);
console.log('PASS: legacy bank/bar import and preservation of original flash when more than sixteen bars would be lost');
const corrupt=dense.slice();corrupt[8+12+28]|=7;const recovered=boot(corrupt);assert.deepEqual(sound(save(recovered),0),sound(baseline,0));
console.log('PASS: corrupt packed voice rejection with clean startup recovery');

const mutation=boot(dense),beforeMutation=readPattern(save(mutation),0);combo(mutation,7,10);const changed=readPattern(save(mutation),0);assert.notDeepEqual(changed,beforeMutation);combo(mutation,4,10);assert.deepEqual(readPattern(save(mutation),0),beforeMutation);combo(mutation,4,10);assert.deepEqual(readPattern(save(mutation),0),changed);
console.log('PASS: sixteen-step whole-pattern mutation, undo and redo preserve chord/p-lock metadata');

// Exercise the actual Global knob -> audio-command route, not a preloaded
// swing byte. Step 2 is the offbeat sixteenth; step 3 is an eighth-note boundary.
for(const [step,moves] of [[1,true],[2,false]])for(const swing of [0,25,50]){
 const f=empty.slice(),p=readPattern(f,0);p.lane[0].step[step].velocity=127;writePattern(f,0,p);
 const h=boot(f);button(h,7);turn(h,3,swing);render(h);button(h,9);
 assert.equal(flash(h)[8+8],swing,'Global KN2 stores the edited swing amount');
 button(h,12);const base=5512,onset=step*base+(moves?Math.floor(base*swing/100):0);
 let n=onset;while(n){const count=Math.min(n,128);h.x.rfm1_audio(h.ptr,count);assert(new Int16Array(h.x.memory.buffer,h.ptr,count*2).every(v=>v===0),'no hit before swung boundary');n-=count;}
 h.x.rfm1_audio(h.ptr,128);assert(new Int16Array(h.x.memory.buffer,h.ptr,256).some(v=>v!==0),'hit starts at the exact swung boundary');
}
console.log('PASS: Global swing knob changes offbeat-sixteenth audio timing while keeping eighth-note boundaries straight');

// A mid-step double tap must retain both swing countdown and bar position.
// B's step zero is a poison trigger for any accidental restart; step five is
// the next legitimate hit. Returning to A must still use the original wrap.
for(const swing of [0,37]){
 const f=empty.slice();f[8+8]=swing;const p=readPattern(f,1);p.lane[0].step[0].velocity=p.lane[0].step[5].velocity=127;writePattern(f,1,p);
 const h=boot(f);bank(h);button(h,12);const base=5512,delta=Math.floor(base*swing/100),duration=base+delta;
 render(h,4*base+123);tap(h,2);render(h,128);tap(h,2);
 let peak=0,n=duration-123-128;
 while(n){const count=Math.min(n,128);h.x.rfm1_audio(h.ptr,count);for(const v of new Int16Array(h.x.memory.buffer,h.ptr,count*2))peak=Math.max(peak,Math.abs(v));n-=count;}
 h.x.rfm1_tick(++h.tick);button(h,9);assert.equal(active(flash(h)),1);assert.equal(peak,0,'no early step-zero or step-five retrigger');
 render(h,128);assert(new Int16Array(h.x.memory.buffer,h.ptr,256).some(v=>Math.abs(v)>100),'step five fires on its original swung boundary');
 tap(h,0);render(h,16*base-(4*base+duration+128));button(h,9);assert.equal(active(flash(h)),1);
 render(h,1);button(h,9);assert.equal(active(flash(h)),0,'bar wrap retains original sample phase');
}
console.log('PASS: immediate pattern swaps retain step countdown, swing phase and original bar boundary without retriggering');

// Three rows: A twice, C once, P three times. Changes occur on exact loop
// boundaries; ONCE must stop and PLAY must restart at row one.
const ch=boot(empty);button(ch,11);button(ch,11);button(ch,11);button(ch,11);
turn(ch,4,1);turn(ch,0,1);turn(ch,3,1);turn(ch,0,1);turn(ch,3,13);turn(ch,4,2);button(ch,3);turn(ch,5,1);
const chainSaved=save(ch);assert.equal(payload(chainSaved).length,12268);assert.deepEqual(Array.from(payload(chainSaved).slice(O.chain,O.chain+3)),[16,2,47]);assert.equal(chainSaved[8+O.chain+16],3);assert.equal(chainSaved[8+O.chain+17],1);
const chainReload=save(boot(chainSaved));assert.deepEqual(payload(chainReload),payload(chainSaved));screenshot(ch,'chain-0.13.0.rgb565');
const bar=16*5512;button(ch,12);render(ch,1);button(ch,9);assert.equal(active(flash(ch)),0);
for(const expected of [0,2,15,15,15,0]){
 render(ch,bar-1);button(ch,9);const before=active(flash(ch));render(ch,1);button(ch,9);assert.equal(active(flash(ch)),expected,`chain row boundary after ${before}`);
}
button(ch,12);render(ch,1);turn(ch,5,1);assert.equal(save(ch)[8+O.chain+17],2);button(ch,12);render(ch,6*bar);button(ch,9);assert.equal(active(flash(ch)),15);render(ch,1);button(ch,9);assert.equal(active(flash(ch)),15);render(ch,bar);button(ch,9);assert.equal(active(flash(ch)),15);
button(ch,12);render(ch,1);button(ch,9);assert.equal(active(flash(ch)),0,'PLAY after ONCE completion restarts chain');
turn(ch,5,-2);render(ch,1);button(ch,9);assert.equal(flash(ch)[8+O.chain+17],0);render(ch,3*bar);button(ch,9);assert.equal(active(flash(ch)),0,'live OFF returns to current-pattern looping');
console.log('PASS: chain repeat/row loop boundaries, save/reload, ONCE stop/restart and live OFF');

const guard=boot(chainSaved);button(guard,11);button(guard,11);button(guard,11);button(guard,11);button(guard,12);render(guard,128);const immutable=readPattern(save(guard),0);
turn(guard,3,1);button(guard,9);assert.equal(flash(guard)[8+O.chain],16,'playing chain row configuration is immutable');
button(guard,11);tap(guard,0);turn(guard,3,20);assert.deepEqual(readPattern(save(guard),0),immutable,'sequence editing paused during chain');
button(guard,11);button(guard,11);turn(guard,0,1);combo(guard,4,3);assert.deepEqual(readPattern(save(guard),1),readPattern(empty,1),'copy cannot change the bank during chain');
// Manual selection exits the chain and retains immediate-switch phase.
tap(guard,2);render(guard,128);tap(guard,2);render(guard,1);button(guard,9);assert.equal(active(flash(guard)),1);assert.equal(flash(guard)[8+O.chain+17],0);render(guard,3*bar);button(guard,9);assert.equal(active(flash(guard)),1);
console.log('PASS: immutable playback bank/config, copy/step guards and manual double-tap override exits chain');
const badChain=chainSaved.slice();badChain[8+O.chain+17]=255;const resetChain=save(boot(badChain));assert.equal(resetChain[8+O.chain+17],0);assert.deepEqual(readPattern(resetChain,0),readPattern(chainSaved,0));
console.log('PASS: invalid chain configuration recovers OFF without discarding patterns');

// The audible kick must start on C's first sample boundary, not while the UI
// reports silent A repeats. This also verifies adoption changes DSP data.
const audibleChain=chainSaved.slice(),kickRow=readPattern(audibleChain,2);kickRow.lane[0].step[0].velocity=127;writePattern(audibleChain,2,kickRow);
const audible=boot(audibleChain);button(audible,12);let silencePeak=0,frames=2*bar;
while(frames){const n=Math.min(frames,128);audible.x.rfm1_audio(audible.ptr,n);for(const value of new Int16Array(audible.x.memory.buffer,audible.ptr,n*2))silencePeak=Math.max(silencePeak,Math.abs(value));frames-=n;}
assert.equal(silencePeak,0);render(audible,128);assert(new Int16Array(audible.x.memory.buffer,audible.ptr,256).some(value=>Math.abs(value)>100));
console.log('PASS: chained pattern audio starts on the exact scheduled boundary');

function midi(h,status,d1,d2){assert.equal(h.x.rfm1_midi_in((d2<<24)|(d1<<16)|(status<<8)|(status>>4)),1);h.x.rfm1_tick(++h.tick);}
function cc(h,channel,number,value){midi(h,0xb0+channel,number,value);render(h,128);}
function startLearn(h){button(h,7);button(h,7);button(h,7);button(h,3);}
const maps=f=>f.slice(8+O.map,8+O.map+48);
const occupied=f=>Array.from({length:16},(_,i)=>Array.from(maps(f).slice(i*3,i*3+3))).filter(m=>m[0]&128);
// Touch Kick's decay, learn CH3 CC74, then leave Learn. Selecting another
// voice afterwards must not redirect the CC to the newly selected lane.
const learned=boot(empty);button(learned,6);startLearn(learned);turn(learned,3,1);cc(learned,2,74,80);button(learned,1);
let learnedFlash=save(learned);assert.equal(payload(learnedFlash).length,12268);assert.deepEqual(occupied(learnedFlash),[[128|74,2,1]]);
turn(learned,0,1);const snareBefore=sound(save(learned),1);cc(learned,1,74,127);assert.notEqual(sound(save(learned),0)[1],127);cc(learned,2,74,127);learnedFlash=save(learned);assert.equal(sound(learnedFlash,0)[1],127);assert.deepEqual(sound(learnedFlash,1),snareBefore);
const learnedReload=boot(learnedFlash);cc(learnedReload,2,74,0);assert.equal(sound(save(learnedReload),0)[1],0);
// Reserved bank/data/channel-mode CCs cannot consume a Learn slot.
const reserved=boot(empty);button(reserved,6);startLearn(reserved);turn(reserved,2,1);cc(reserved,0,123,127);assert.equal(occupied(save(reserved)).length,0);cc(reserved,0,1,127);button(reserved,1);assert.equal(occupied(save(reserved)).length,1);
// Clear by picked target, then by selecting a stored mapping in the manager.
const cleared=boot(learnedFlash);button(cleared,6);startLearn(cleared);turn(cleared,3,1);button(cleared,0);button(cleared,1);assert.equal(occupied(save(cleared)).length,0);
const manager=boot(learnedFlash);button(manager,7);button(manager,7);button(manager,7);screenshot(manager,'midi-maps-0.13.0.rgb565');button(manager,0);assert.equal(occupied(save(manager)).length,0);
console.log('PASS: MIDI Learn workflow, exact channel/pinned track, scaling, save/reload, reserved CCs and clear without octave side effects');

// The same CC may be assigned on different channels; remapping a source or a
// destination replaces only those bindings. Fill all sixteen without eviction.
const fullLearn=boot(empty);button(fullLearn,6);startLearn(fullLearn);
for(let lane=0;lane<4;++lane){if(lane)turn(fullLearn,0,1);for(let knob=0;knob<4;++knob){turn(fullLearn,2+knob,1);cc(fullLearn,lane,40+knob,127);}}
let allMaps=save(fullLearn);assert.equal(occupied(allMaps).length,16);const oldMaps=maps(allMaps);turn(fullLearn,0,1);turn(fullLearn,2,1);cc(fullLearn,9,79,127);assert.deepEqual(maps(save(fullLearn)),oldMaps);
cc(fullLearn,0,40,127);button(fullLearn,1);allMaps=save(fullLearn);assert.equal(occupied(allMaps).length,16);assert(occupied(allMaps).some(m=>m[1]===0&&m[2]===(4<<5)));
assert.deepEqual(maps(save(boot(allMaps))),maps(allMaps));
console.log('PASS: full MIDI table fits, no unrelated eviction, same CC on separate channels and explicit source reassignment');

// Voice endpoints with nonzero minima, LFO discrete ranges, master effects,
// effect-pad assignment following, punch gates and standard volume/pan.
const standard=boot(empty);cc(standard,1,7,127);cc(standard,1,10,0);const sf=save(standard);assert.equal(sound(sf,1)[29],127);assert.equal(sound(sf,1)[30],0);
const analogMap=boot(empty);turn(analogMap,0,7);button(analogMap,6);startLearn(analogMap);turn(analogMap,2,1);cc(analogMap,8,20,64);button(analogMap,1);cc(analogMap,8,20,0);assert.equal(sound(save(analogMap),7)[0],12);cc(analogMap,8,20,127);assert.equal(sound(save(analogMap),7)[0],96);
const lfoMap=boot(empty);button(lfoMap,5);startLearn(lfoMap);turn(lfoMap,2,1);cc(lfoMap,3,21,1);button(lfoMap,1);cc(lfoMap,3,21,127);assert.equal(sound(save(lfoMap),0)[32],4);
const mainMap=boot(empty);button(mainMap,2);button(mainMap,2);startLearn(mainMap);turn(mainMap,2,1);cc(mainMap,4,22,10);button(mainMap,1);cc(mainMap,4,22,0);assert.equal(scene(save(mainMap)).fx[0],0);
const effectMap=boot(empty);button(effectMap,2);button(effectMap,6);startLearn(effectMap);turn(effectMap,2,1);cc(effectMap,5,23,60);button(effectMap,1);cc(effectMap,5,23,127);assert.equal(scene(save(effectMap)).effect[28][0],127);
turn(effectMap,1,effectKeys.indexOf(20)-3);const ringBefore=scene(save(effectMap)).effect[20];cc(effectMap,5,23,0);assert.equal(scene(save(effectMap)).effect[28][0],0);assert.deepEqual(scene(flash(effectMap)).effect[20],ringBefore,'mapping stays pinned to its effect when another is selected');
console.log('PASS: standard CC7/10, Analog note minima, discrete LFO range, master FX and fixed-effect parameter scaling');

const gate=boot(empty);button(gate,2);startLearn(gate);tap(gate,1);cc(gate,6,24,127);button(gate,1);button(gate,8);
const gateFlash=save(gate);assert(occupied(gateFlash).some(m=>(m[1]>>4)===8));
// Compare an audible note through mapped AntiVerb to dry, then prove release
// and a fresh boot do not leave the learned momentary gate latched.
function gateAudio(h,on){cc(h,6,24,on?127:0);midi(h,0x90,60,110);const hash=createHash('sha256');for(let n=0;n<345;++n){h.x.rfm1_audio(h.ptr,128);hash.update(new Uint8Array(h.x.memory.buffer,h.ptr,512));}return hash.digest('hex');}
assert.notEqual(gateAudio(boot(gateFlash),true),gateAudio(boot(gateFlash),false));
const released=boot(gateFlash);cc(released,6,24,127);cc(released,6,24,0);assert.equal(gateAudio(released,false),gateAudio(boot(gateFlash),false));
console.log('PASS: learned FX gates work away from FX screen, release promptly and remain transient across reload');

const malformed=learnedFlash.slice();malformed[8+O.map+1]=255;assert.equal(occupied(save(boot(malformed))).length,0);assert.deepEqual(readPattern(save(boot(malformed)),0),readPattern(learnedFlash,0));
console.log('PASS: malformed MIDI destinations discarded without resetting the musical project');

const combined=baseline.slice();combined.set(migratedPayload(dense),8);combined.set(maps(allMaps),8+O.map);
assert.deepEqual(payload(save(boot(combined))),payload(combined));
console.log('PASS: maximum-density sixteen-pattern/custom library/chain plus all sixteen MIDI maps fits and reloads losslessly');
const learnScreen=boot(empty);button(learnScreen,6);startLearn(learnScreen);turn(learnScreen,3,1);screenshot(learnScreen,'midi-learn-wait-0.13.0.rgb565');cc(learnScreen,2,74,80);screenshot(learnScreen,'midi-learn-bound-0.13.0.rgb565');

const fourPads=boot(fixture(baseline,'legacy-four-fx.bin')),fourImported=save(fourPads);assert(occupied(fourImported).some(m=>m[0]===(128|74)&&m[1]===0x32&&m[2]===28*4));assert(occupied(fourImported).some(m=>m[0]===(128|24)&&m[1]===0x86&&m[2]===27));
console.log('PASS: four-pad MIDI parameter/gate mappings migrate to their original effects');
// Fresh delay is dry; an intentional saved send survives reload. Filter pair shares a page.
const masterUI=boot();assert.equal(scene(save(masterUI)).fx[0],0);
button(masterUI,2);button(masterUI,2);turn(masterUI,2,35);render(masterUI);
screenshot(masterUI,'master-delay-0.13.0.rgb565');
const wetSave=save(masterUI);assert.equal(scene(wetSave).fx[0],35);assert.equal(scene(save(boot(wetSave))).fx[0],35);
button(masterUI,2);const filterBefore=scene(save(masterUI)).fx;
turn(masterUI,2,-1);turn(masterUI,3,1);render(masterUI);const filterAfter=scene(save(masterUI)).fx;
assert.equal(filterAfter[7],filterBefore[7]-1);assert.equal(filterAfter[8],filterBefore[8]+1);
assert.equal(filterAfter[4],filterBefore[4]);screenshot(masterUI,'master-filter-0.13.0.rgb565');
console.log('PASS: fresh dry delay, saved mix persistence and paired cutoff/resonance controls');

// Visual regression captures: redraw the same header through OCT -> MUTE -> OCT.
const muteUI=boot();button(muteUI,8);screenshot(muteUI,'header-oct-0.13.1.rgb565');
button(muteUI,3);render(muteUI);screenshot(muteUI,'header-mute-0.13.1.rgb565');
button(muteUI,3);render(muteUI);screenshot(muteUI,'header-unmute-0.13.1.rgb565');

const perc=boot();turn(perc,0,5);button(perc,6);
for(let row=0;row<5;++row){const before=sound(save(perc),5);for(let k=0;k<4;++k)turn(perc,2+k,before[row*4+k]>64?-1:1);render(perc);const after=sound(save(perc),5);for(let k=0;k<4;++k)assert.notEqual(after[row*4+k],before[row*4+k]);if(row<4)turn(perc,1,1);}
const percFlash=save(perc);assert.deepEqual(sound(save(boot(percFlash)),5),sound(percFlash,5));screenshot(perc,'perc-controls-0.14.0.rgb565');
console.log('PASS: all twenty Perc controls edit and persist across reload');
// User's PRC INIT screenshots: all displayed values, neutral pan/transpose.
const userInit=boot(),userSound=sound(save(userInit),5);
assert.deepEqual(Array.from(userSound.subarray(0,20)),[48,17,28,92,11,29,49,40,47,21,51,122,86,47,75,49,14,12,100,84]);
assert.deepEqual(Array.from(userSound.subarray(29,32)),[72,64,64]);
turn(userInit,0,5);button(userInit,6);screenshot(userInit,'prc-user-init-top.rgb565');turn(userInit,1,5);screenshot(userInit,'prc-user-init-bottom.rgb565');
console.log('PASS: user PRC INIT startup matches all recovered screenshot controls');

const sends=boot();button(sends,8);button(sends,8); // TRACK -> D SEND
for(let k=0;k<4;++k)turn(sends,2+k,-127+k*31);render(sends);turn(sends,1,1);
for(let k=0;k<4;++k)turn(sends,2+k,-127+k*41);render(sends);
const sendsFlash=save(sends),sendValues=[0,31,62,93,0,41,82,123];assert.deepEqual(Array.from(payload(sendsFlash).slice(-8)),sendValues);
assert.deepEqual(payload(save(boot(sendsFlash))),payload(sendsFlash));screenshot(sends,'delay-sends-0.15.0.rgb565');
const oldFull=boot(fixture(baseline,'legacy-fullfx.bin'));assert.deepEqual(Array.from(payload(save(oldFull)).slice(-8)),Array(8).fill(127));
startLearn(sends);turn(sends,2,1);cc(sends,3,25,50);button(sends,1);button(sends,6);cc(sends,3,25,127);assert.equal(payload(save(sends)).at(-4),127);
console.log('PASS: eight independent delay sends, save/reload, schema-4 migration and MIDI Learn');

// Factory scenes set musical delay routes; A/B restores the previous mixer sends.
const sceneRoutes=boot();button(sceneRoutes,2);button(sceneRoutes,9);turn(sceneRoutes,6,1);button(sceneRoutes,3);render(sceneRoutes);
const routeA=Array.from(payload(save(sceneRoutes)).slice(-8));assert.deepEqual(routeA,[0,48,8,24,36,44,64,0]);
button(sceneRoutes,10);render(sceneRoutes);assert.deepEqual(Array.from(payload(save(sceneRoutes)).slice(-8)),Array(8).fill(127));
button(sceneRoutes,10);render(sceneRoutes);assert.deepEqual(Array.from(payload(save(sceneRoutes)).slice(-8)),routeA);
console.log('PASS: factory delay scenes apply dry kick/bass routes and A/B restores sends');

for(const type of [1,2])for(let slot=1;slot<4;++slot){const h=boot();if(type==2)button(h,2);button(h,9);turn(h,6,slot);button(h,3);render(h);const routes=Array.from(payload(save(h)).slice(-8));assert.equal(routes[0],0);assert.equal(routes[7],0);assert(routes[1]>0&&routes[6]>0);assert(routes[2]<=8);}
console.log('PASS: all six delay-enabled factory kits/scenes keep kick and bass dry');

const cursor=boot();button(cursor,11);const cursorBefore=readPattern(save(cursor),0);
turn(cursor,1,4);assert.deepEqual(readPattern(save(cursor),0),cursorBefore,'cursor movement cannot toggle notes');
turn(cursor,3,-1);render(cursor);let cursorPattern=readPattern(save(cursor),0);assert.equal(cursorPattern.lane[0].step[4].velocity,cursorBefore.lane[0].step[4].velocity-1);assert.deepEqual(cursorPattern.lane[0].step[0],cursorBefore.lane[0].step[0]);
screenshot(cursor,'step-cursor-0.15.2.rgb565');const directCursor=boot();button(directCursor,11);directCursor.m.setUint32(directCursor.at(2),1<<4,true);directCursor.x.rfm1_tick(++directCursor.tick);tap(directCursor,7);directCursor.m.setUint32(directCursor.at(2),0,true);directCursor.x.rfm1_tick(++directCursor.tick);turn(directCursor,3,-1);render(directCursor);const directPattern=readPattern(save(directCursor),0);assert.equal(directPattern.lane[0].step[4].velocity,cursorBefore.lane[0].step[4].velocity-1);
console.log('PASS: ALGORITHM selects steps without toggling and velocity edits the selected cursor step');

const shortPattern=boot();const originalShort=readPattern(save(shortPattern),0);button(shortPattern,7);turn(shortPattern,4,-9);render(shortPattern);let shortFlash=save(shortPattern);assert(readPattern(shortFlash,0).lane.every(l=>l.length==7));assert.deepEqual(readPattern(shortFlash,0).lane.map(l=>l.step),originalShort.lane.map(l=>l.step));assert(readPattern(save(boot(shortFlash)),0).lane.every(l=>l.length==7));screenshot(shortPattern,'short-pattern-0.15.3.rgb565');turn(shortPattern,4,9);assert(readPattern(save(shortPattern),0).lane.every(l=>l.length==16));
const shortBoundary=boot(shortFlash);bank(shortBoundary);tap(shortBoundary,2); // stopped selection B; return to A, then start and queue B
render(shortBoundary);tap(shortBoundary,0);render(shortBoundary);button(shortBoundary,12);render(shortBoundary,1);tap(shortBoundary,2);
render(shortBoundary,7*5512-1);button(shortBoundary,9);assert.equal(active(flash(shortBoundary)),0);render(shortBoundary,1);button(shortBoundary,9);assert.equal(active(flash(shortBoundary)),1);
console.log('PASS: global 1–16 steps, hidden-note retention, save/reload and queued switch on the shortened boundary');
const shortChainFlash=shortFlash.slice();const shortB=readPattern(shortChainFlash,1);for(const lane of shortB.lane)lane.length=3;writePattern(shortChainFlash,1,shortB);shortChainFlash[8+O.chain]=0;shortChainFlash[8+O.chain+1]=1;shortChainFlash[8+O.chain+16]=2;shortChainFlash[8+O.chain+17]=2;
const shortChain=boot(shortChainFlash);button(shortChain,12);render(shortChain,7*5512);button(shortChain,9);assert.equal(active(flash(shortChain)),0);render(shortChain,1);button(shortChain,9);assert.equal(active(flash(shortChain)),1);render(shortChain,3*5512);button(shortChain,9);assert.equal(active(flash(shortChain)),1); // ONCE completes B without returning to A.
button(shortChain,7);turn(shortChain,4,-1);render(shortChain);assert(readPattern(save(shortChain),1).lane.every(l=>l.length==2)); // ONCE stopped: bank editing is available again.
console.log('PASS: chain advances at a seven-step boundary into a three-step pattern');

const lengthGridFlash=baseline.slice(),lengthGridPattern=readPattern(lengthGridFlash,0);[8,3,5,7,12,4,16,9].forEach((n,i)=>lengthGridPattern.lane[i].length=n);writePattern(lengthGridFlash,0,lengthGridPattern);
const lengthGrid=boot(lengthGridFlash);button(lengthGrid,11);screenshot(lengthGrid,'track-lengths-stopped-0.15.4.rgb565');button(lengthGrid,12);render(lengthGrid,4*5512+1);screenshot(lengthGrid,'track-lengths-playing-0.15.4.rgb565');
const lcdPixel=(h,x,y)=>h.m.getUint16(h.at(11)+2*(y*240+x),true);
// Snare length 3 wraps to step 2 at global step 5, while CH length 5 is at step 5.
assert.equal(lcdPixel(lengthGrid,33,60),lcdPixel(lengthGrid,72,69),'lane playheads follow their modulo positions');
assert.notEqual(lcdPixel(lengthGrid,72,60),lcdPixel(lengthGrid,33,60),'snare playhead cannot enter its shaded cells');
console.log('PASS: independent lane playheads wrap within their visible track lengths');

screenshot(lengthGrid,'compact-seq-playing-0.15.5.rgb565');
button(lengthGrid,12);turn(lengthGrid,0,5);turn(lengthGrid,1,6);screenshot(lengthGrid,'compact-seq-prc-0.15.5.rgb565');
// GLO's MIDI map page occupies the middle region too; cycling must erase it.
function lcdBytes(h){h.x.rfm1_frame();return Buffer.from(new Uint8Array(h.x.memory.buffer,h.at(11),240*240*2));}
const globalCycle=boot();button(globalCycle,7);const globalClean=lcdBytes(globalCycle);button(globalCycle,7);const qualityClean=lcdBytes(globalCycle);button(globalCycle,7);button(globalCycle,7);assert.deepEqual(lcdBytes(globalCycle),globalClean,'GLO cycle must erase the MIDI page completely');turn(globalCycle,1,-1);turn(globalCycle,1,-1);assert.deepEqual(lcdBytes(globalCycle),qualityClean,'Algorithm MIDI-to-quality transition must be clean');
screenshot(globalCycle,'global-clean-transition-0.15.6.rgb565');console.log('PASS: GLO and ALGORITHM erase all MIDI Learn page residue');
const punchRoll=boot();button(punchRoll,2);button(punchRoll,2);button(punchRoll,2);button(punchRoll,2); // master page 3
key(punchRoll,0,true);render(punchRoll);let rollState=scene(save(punchRoll));assert.equal(rollState.fx[9],1);assert.equal(rollState.fx[10],127);turn(punchRoll,3,2);render(punchRoll);assert.equal(scene(save(punchRoll)).fx[9],3);screenshot(punchRoll,'roll-held-0.16.0.rgb565');key(punchRoll,0,false);render(punchRoll);screenshot(punchRoll,'roll-released-0.16.0.rgb565');
console.log('PASS: Roll page keys arm/capture with audible defaults and KN2 changes rate live');

const oldRoll=boot(fixture(baseline,'legacy-roll5.bin'));assert.equal(scene(save(oldRoll)).fx[9],109,'old 1/256 rate migrates to matching continuous position');
turn(punchRoll,3,127);render(punchRoll);assert.equal(scene(save(punchRoll)).fx[9],127);key(punchRoll,0,true);render(punchRoll);screenshot(punchRoll,'continuous-roll-0.16.1.rgb565');key(punchRoll,0,false);
console.log('PASS: legacy roll rate migration and continuous 127-position rate range');
function remixPage(h){for(let n=0;n<2;++n)button(h,11);}
function pcmHash(h,frames){const hash=createHash('sha256');while(frames){const n=Math.min(128,frames);h.x.rfm1_audio(h.ptr,n);hash.update(new Uint8Array(h.x.memory.buffer,h.ptr,n*4));frames-=n;}h.x.rfm1_tick(++h.tick);return hash.digest('hex');}
const routeOriginal=baseline.slice(),routePattern=readPattern(routeOriginal,0);Object.assign(routePattern.lane[0].step[3],{velocity:99,chance:87,ratchet:3,pitch:7,lock:1,value:80});routePattern.lane[6].step[5].velocity=95;routePattern.chord[0][5]=[4,7,12];writePattern(routeOriginal,0,routePattern);
const reversedImage=routeOriginal.slice(),reversedPattern=readPattern(reversedImage,0);for(const lane of reversedPattern.lane)lane.step.reverse();for(const chord of reversedPattern.chord)chord.reverse();writePattern(reversedImage,0,reversedPattern);
const routed=boot(routeOriginal),manualReverse=boot(reversedImage);remixPage(routed);turn(routed,1,1);turn(routed,5,127);button(routed,3);button(routed,12);button(manualReverse,12);
assert.equal(pcmHash(routed,16*5512),pcmHash(manualReverse,16*5512),'routed reverse reads notes, p-locks, ratchets and chords from the mapped source');
assert.deepEqual(readPattern(save(routed),0),routePattern);turn(routed,2,17);render(routed);assert.deepEqual(readPattern(save(routed),0),routePattern);
key(routed,0,true);render(routed);screenshot(routed,'remix-glitch-0.17.0.rgb565');key(routed,0,false);render(routed);button(routed,3);render(routed);assert.deepEqual(readPattern(save(routed),0),routePattern);
const routeSaved=save(routed);assert.deepEqual(payload(routeSaved).slice(O.pattern,O.pattern+16*672),payload(routeOriginal).slice(O.pattern,O.pattern+16*672));const routeReload=boot(routeSaved),routeStraight=boot(routeOriginal);button(routeReload,12);button(routeStraight,12);assert.equal(pcmHash(routeReload,16*5512),pcmHash(routeStraight,16*5512));
console.log('PASS: live route audio matches an explicit reversed pattern including chords/locks, while source data remains unchanged');
const chainRemix=boot(chainSaved);remixPage(chainRemix);turn(chainRemix,1,1);turn(chainRemix,5,127);button(chainRemix,3);button(chainRemix,12);render(chainRemix,2*bar);button(chainRemix,9);assert.equal(active(flash(chainRemix)),0);render(chainRemix,1);button(chainRemix,9);assert.equal(active(flash(chainRemix)),2);
console.log('PASS: route jumps leave chain repeats and pattern boundaries on the original clock');

const remixKeys=boot(routeOriginal);remixPage(remixKeys);button(remixKeys,12);key(remixKeys,3,true);render(remixKeys,100);remixKeys.m.setUint32(remixKeys.at(1),(1<<3)|1,true);remixKeys.x.rfm1_tick(++remixKeys.tick);render(remixKeys,100);assert.equal(scene(save(remixKeys)).fx[9],1);remixKeys.m.setUint32(remixKeys.at(1),0,true);remixKeys.x.rfm1_tick(++remixKeys.tick);render(remixKeys,100);assert.deepEqual(readPattern(save(remixKeys),0),routePattern);screenshot(remixKeys,'remix-released-0.17.0.rgb565');
console.log('PASS: black route and white Glitch gates coexist without editing pattern data');
function printTo(h,slot){h.m.setUint32(h.at(2),1<<4,true);h.x.rfm1_tick(++h.tick);tap(h,whites[slot]);h.m.setUint32(h.at(2),0,true);h.x.rfm1_tick(++h.tick);}
const printHost=boot(routeOriginal);remixPage(printHost);turn(printHost,1,1);turn(printHost,5,127);button(printHost,3);button(printHost,12);render(printHost,128);printTo(printHost,1);const printedFlash=save(printHost);assert.equal(active(printedFlash),0);assert.deepEqual(readPattern(printedFlash,0),routePattern);assert.deepEqual(readPattern(printedFlash,1),reversedPattern);assert.deepEqual(readPattern(save(boot(printedFlash)),1),reversedPattern);
printTo(printHost,0);assert.deepEqual(readPattern(save(printHost),0),routePattern,'cannot replace source');turn(printHost,1,1);printTo(printHost,1);assert.deepEqual(readPattern(save(printHost),1),reversedPattern,'occupied slot requires a repeat');printTo(printHost,1);assert.notDeepEqual(readPattern(save(printHost),1),reversedPattern,'repeat explicitly overwrites destination');
printTo(chainRemix,3);assert.deepEqual(readPattern(save(chainRemix),3),readPattern(chainSaved,3),'printing cannot alter the immutable playback bank during a chain');
console.log('PASS: print routed pattern to another slot during playback, retain source, save/reload and protect source/occupied/chain slots');
const remixLearn=boot();remixPage(remixLearn);startLearn(remixLearn);turn(remixLearn,3,1);cc(remixLearn,7,26,60);button(remixLearn,1);cc(remixLearn,7,26,127);assert.equal(scene(save(remixLearn)).fx[9],127);console.log('PASS: REMIX Glitch Rate retains global FX MIDI Learn routing');
const keySizes=boot();remixPage(keySizes);for(let w=0;w<16;++w){key(keySizes,whites[w],true);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[9],1+Math.floor((w*126+7)/15));key(keySizes,whites[w],false);render(keySizes,1);}
key(keySizes,whites[0],true);render(keySizes,1);keySizes.m.setUint32(keySizes.at(1),(1<<whites[0])|(1<<whites[15]),true);keySizes.x.rfm1_tick(++keySizes.tick);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[9],127);keySizes.m.setUint32(keySizes.at(1),1<<whites[0],true);keySizes.x.rfm1_tick(++keySizes.tick);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[9],1);turn(keySizes,3,10);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[9],11);keySizes.m.setUint32(keySizes.at(1),(1<<whites[0])|(1<<whites[3]),true);keySizes.x.rfm1_tick(++keySizes.tick);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[9],36);keySizes.m.setUint32(keySizes.at(1),1<<whites[0],true);keySizes.x.rfm1_tick(++keySizes.tick);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[9],11);key(keySizes,0,false);
console.log('PASS: sixteen large-to-small Glitch key sizes, last-held priority/fallback and shared knob offset');
const gridHost=boot(routeOriginal);remixPage(gridHost);turn(gridHost,1,1);turn(gridHost,5,127);button(gridHost,3);button(gridHost,12);render(gridHost,1);screenshot(gridHost,'slice-grid-reverse-0.17.1.rgb565');const selectedColor=lcdPixel(gridHost,39,60);for(let i=0;i<16;++i)assert.equal(lcdPixel(gridHost,26+i*13,52+i*8),selectedColor);assert.notEqual(lcdPixel(gridHost,26,172),selectedColor);console.log('PASS: sixteen-by-sixteen slice grid renders reverse routing on the correct axes');
for(const [mode,keyIndex,expected] of [[9,22,[0,1,2,3,4,5,0,1,2,3,4,5,6,7,0,1]],[10,25,Array.from({length:16},(_,i)=>i%4)]]){const h=boot(routeOriginal);remixPage(h);turn(h,5,127);key(h,keyIndex,true);render(h,1);printTo(h,1);const p=readPattern(save(h),1);for(let l=0;l<8;++l)assert.deepEqual(p.lane[l].step,expected.map(i=>routePattern.lane[l].step[i]));screenshot(h,mode==9?'slice-grid-breaks-0.17.1.rgb565':'slice-grid-floor-0.17.1.rgb565');key(h,keyIndex,false);}
console.log('PASS: last two black keys select exact Breaks and 4 to the Floor templates from screenshots');
key(keySizes,0,true);turn(keySizes,4,-127);render(keySizes,1);key(keySizes,0,false);key(keySizes,whites[15],true);render(keySizes,1);assert.equal(scene(save(keySizes)).fx[10],0,'key changes retain an intentionally dry Glitch mix');key(keySizes,whites[15],false);
console.log('PASS: Glitch key changes preserve the live mix setting');
const keepVariation=boot(routeOriginal);remixPage(keepVariation);turn(keepVariation,2,17);turn(keepVariation,5,-60);key(keepVariation,22,true);render(keepVariation,1);printTo(keepVariation,1);const firstKept=readPattern(save(keepVariation),1);key(keepVariation,22,false);key(keepVariation,25,true);render(keepVariation,1);key(keepVariation,25,false);key(keepVariation,22,true);render(keepVariation,1);printTo(keepVariation,2);assert.deepEqual(readPattern(save(keepVariation),2),firstKept,'mode keys preserve VAR and DEPTH');key(keepVariation,22,false);turn(keepVariation,2,-127);turn(keepVariation,5,127);printTo(keepVariation,3);const zeroTemplate=readPattern(save(keepVariation),3),breaksExact=[0,1,2,3,4,5,0,1,2,3,4,5,6,7,0,1];for(let l=0;l<8;++l)assert.deepEqual(zeroTemplate.lane[l].step,breaksExact.map(i=>routePattern.lane[l].step[i]));
console.log('PASS: mode keys retain VAR/depth and VAR zero remains the exact supplied template');
const previousContinuous=boot(fixture(baseline,'legacy-glitch6.bin'));assert.equal(scene(save(previousContinuous)).fx[9],73,'previous continuous 1/64 migrates to new 1/64 detent');
console.log('PASS: previous continuous Glitch rates migrate into the larger range');

const sliceDefaults=boot();remixPage(sliceDefaults);assert.equal(scene(save(sliceDefaults)).fx[10],127);turn(sliceDefaults,4,-37);turn(sliceDefaults,5,-27);const mixEdited=scene(save(sliceDefaults)).fx[10];assert.equal(mixEdited,90);for(let n=0;n<4;++n)button(sliceDefaults,11);assert.equal(scene(save(sliceDefaults)).fx[10],90);screenshot(sliceDefaults,'slice-defaults-edited-0.17.6.rgb565');console.log('PASS: Slice defaults to full mix/depth and preserves deliberate edits on return');
