#!/usr/bin/env python3
"""Conservative direct-call review of diagnostic symbols and compiler frames.
Adds 64 bytes per reported frame, 128 for assembly/unreported leaf frames.
Not a proof of arbitrary indirect calls or interrupt behavior.
Usage: python3 tools/review_stack.py <downloaded-native-symbols-directory>
"""
import re,pathlib,bisect,sys
root=pathlib.Path(sys.argv[1]); log=(root/'native-stack-diagnostics.log').read_text(); frames={f:int(n)+64 for n,f in re.findall(r"stack frame size of (\d+) bytes in function '([^']+)'",log)}
syms=[]
for l in (root/'build/fm1/symbols.txt').read_text().splitlines():
 m=re.match(r'([0-9a-f]+)\s+.*?\bF\s+\.\w+\s+([0-9a-f]+)\s+(\S+)$',l)
 if m:syms.append((int(m[1],16),int(m[2],16),m[3]))
syms.sort();starts=[x[0] for x in syms];edges={};ind=[]
def owner(a):
 i=bisect.bisect_right(starts,a)-1
 return syms[i][2] if i>=0 and a<syms[i][0]+syms[i][1] else None
for l in (root/'build/fm1/firmware.dis').read_text().splitlines():
 m=re.match(r'\s*([0-9a-f]+):.*?\t(.*)',l)
 if not m:continue
 f=owner(int(m[1],16));code=m[2]
 if not f:continue
 c=re.search(r'\b(?:call|goto)\s+[-\d]+ <.*? : ([0-9a-f]+) >',code)
 if c:
  target=owner(int(c[1],16))
  if target and target!=f:edges.setdefault(f,set()).add(target)
 if not c:
  rel=re.fullmatch(r'\s*call (-?\d+)\s*',code)
  if rel:
   target=owner(int(m[1],16)+4+int(rel[1]))
   if not target:raise ValueError('unresolved '+l)
   if target!=f:edges.setdefault(f,set()).add(target)
 if re.search(r'\b(?:call|goto) r\d',code):ind.append((f,code))
# RAM flash entry points are reached through relocated function pointers.
for caller, targets in {'st_read':['fl_read_ram'], 'fl_write':['fl_prog_ram'],
                        'settings_save':['fl_erase4k_ram'],
                        'ota_erase':['fl_erase4k_ram']}.items():
 edges.setdefault(caller,set()).update(targets)
def walk(f,seen=()):
 if f in seen:raise ValueError('recursive path '+str((*seen,f)))
 opts=[walk(c,(*seen,f)) for c in edges.get(f,())];b,p=max(opts,default=(0,[]),key=lambda x:x[0]);return frames.get(f,128)+b,[f]+p
for f in ['setup','loop','audio','fm1_alnk0_irq','fm1_timer5_irq']:
 n,p=walk(f);print(f,n,' -> '.join(p))
print('Indirect calls',ind)
