// RemoteFM1 checks manifest identity against the instantiated ABI descriptor.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const file=process.argv[2];
assert(file,'usage: node tests/import-contract.mjs FILE.rfm1');
const module=new WebAssembly.Module(fs.readFileSync(file));
const sections=WebAssembly.Module.customSections(module,'rfm1');
assert.equal(sections.length,1,'one firmware manifest');
const manifest=JSON.parse(new TextDecoder().decode(sections[0]));
const x=new WebAssembly.Instance(module).exports;
assert.equal(x.rfm1_version(),1);
const descriptor=x.rfm1_get(6),memory=new DataView(x.memory.buffer);
assert(descriptor,'ABI 6 descriptor');
assert.equal(memory.getUint32(descriptor+x.rfm1_layout(18),true),manifest.abi);
for(const [field,index] of [['id',19],['name',20],['version',21],['source_url',22],['license',24]]){
 const ptr=memory.getUint32(descriptor+x.rfm1_layout(index),true);
 assert(ptr>0&&ptr<x.memory.buffer.byteLength,'descriptor string pointer');
 const bytes=new Uint8Array(x.memory.buffer,ptr),end=bytes.indexOf(0);
 assert(end>=0&&end<4096,'terminated descriptor string');
 const value=new TextDecoder().decode(bytes.subarray(0,end));
 assert.equal(value,manifest[field],`manifest/descriptor ${field} mismatch`);
}
console.log(`PASS: RemoteFM1 import identity agrees (${manifest.name} ${manifest.version})`);
