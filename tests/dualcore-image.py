#!/usr/bin/env python3
"""Check native ELF sections against the exact packaged app and startup copy map.
Usage: python3 tests/dualcore-image.py symbols-directory candidate.fwsc
"""
from pathlib import Path
import re,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import flash,fm1pkg_make as pkg
root=Path(sys.argv[1]);elf=(root/'build/fm1/firmware.elf').read_bytes()
assert elf[:6]==b'\x7fELF\x01\x01','expected little-endian ELF32'
shoff=struct.unpack_from('<I',elf,32)[0];ents,num,names=struct.unpack_from('<HHH',elf,46)
headers=[struct.unpack_from('<10I',elf,shoff+i*ents) for i in range(num)]
strings=elf[headers[names][4]:headers[names][4]+headers[names][5]]
sections={strings[h[0]:].split(b'\0',1)[0].decode():h for h in headers}
syms=(root/'build/fm1/symbols.txt').read_text()
def symbol(name):
 m=re.search(r'^([0-9a-f]+) .*\s'+re.escape(name)+r'$',syms,re.M);assert m,name;return int(m[1],16)
image=flash.logical_image(Path(sys.argv[2]).read_bytes());assert flash.files_ok(image)
header=flash.enc(bytearray(image[:64]));count=int.from_bytes(header[8:10],'little');raw=None
for i in range(count):
 entry=flash.enc(bytearray(image[64+i*80:144+i*80]))
 if int.from_bytes(entry[:2],'little')==0:
  off=int.from_bytes(entry[8:12],'little');size=int.from_bytes(entry[12:16],'little');raw=image[off:off+size]
assert raw is not None
region=bytearray(raw[0x4000:]);pkg.sfc(region,0,len(region),0,pkg.KEY);app=region[0x120:0x120+pkg.APP_SLOT]
ram=bytearray(0x6000)
for name,load in [('.text',None),('.ram_text','_rt_load'),('.c1_text','_c1_load'),('.data','_data_load')]:
 h=sections[name];blob=elf[h[4]:h[4]+h[5]];address=symbol(load) if load else h[3];at=address-0x02000120
 assert at>=0 and at+len(blob)<=len(app),name
 assert app[at:at+len(blob)]==blob,'packaged section differs: '+name
 if name in ('.ram_text','.c1_text'):
  dest=h[3]-0x01c00000;assert dest>=0 and dest+len(blob)<=len(ram)
  assert h[3]%4==0 and len(blob)%4==0
  for n in range(0,len(blob),4):ram[dest+n:dest+n+4]=app[at+n:at+n+4]
  assert ram[dest:dest+len(blob)]==blob
h=sections['.c1_text'];assert h[3]==symbol('_c1_start') and h[3]+h[5]==symbol('_c1_end')
assert symbol('_rt_end')<=symbol('_c1_start')<symbol('_c1_end')<=0x01c06000
assert symbol('_c1_start')<=symbol('fm1_c1_main')<symbol('_c1_end')
assert 0x02000120<=symbol('gd_render_synth_job')<symbol('_etext')
stack=sections['.c1stack'];assert stack[3]==0x01c06000 and stack[5]==0x1f00
assert symbol('_c1_ustack')==0x01c06000 and symbol('_c1_sstack_top')==0x01c07f00
assert symbol('_bss_end')<=0x01c20000 and symbol('_pool_end')<=0x01c74000
# RAM idle loop must not outline any helper into XIP. Its only call is the
# job function, invoked after a checked new job while flash is available.
dis=(root/'build/fm1/firmware.dis').read_text();calls=[]
for line in dis.splitlines():
 m=re.match(r'\s*([0-9a-f]+):.*?\t(.*)',line)
 if not m:continue
 address=int(m[1],16);instruction=m[2]
 if symbol('_c1_start')<=address<symbol('_c1_end') and re.search(r'\bcall\b',instruction):
  assert re.search(r'\bcall r\d+\s*$',instruction),line
  calls.append(line)
assert len(calls)==1,calls
print('PASS: exact packaged text/data/worker images, bounded word copies, dedicated core-1 stack and RAM-only idle loop with one indirect job call')
