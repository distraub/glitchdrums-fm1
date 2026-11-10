/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include "fx.h"
#include "punch.h"
static uint64_t alias_hash(unsigned parameter,unsigned value){
 GDAlias a={0};uint8_t q[4]={96,64,64,127};q[parameter]=(uint8_t)value;uint64_t hash=1;
 for(unsigned i=0;i<8192;++i){int l=gd_sine(i*27000000u)/3,r=gd_sine(i*37000000u)/3,original=l;
  gd_alias(&a,q,&l,&r);if(parameter==3&&!value)assert(l==original);
  assert(l>=-30000&&l<=30000&&r>=-30000&&r<=30000);hash=(hash^(uint32_t)l)*1099511628211ull;
 }return hash;
}
int main(void){
 for(unsigned parameter=0;parameter<4;++parameter)assert(alias_hash(parameter,0)!=alias_hash(parameter,127));
 GDProject p;gd_defaults(&p);unsigned quarter=44100u*60u/p.bpm;
 for(unsigned effect=5;effect<=7;++effect){
  GDPunch f={0};
  for(unsigned i=0;i<44100;++i){int32_t l=gd_sine(i*17000000u)/3,r=l;gd_punch(&f,&p,1u<<23,i,&l,&r);}
  uint32_t write=f.write;int32_t l=1200,r=1200;gd_punch(&f,&p,(1u<<23)|(1u<<effect),44100,&l,&r);
  assert(f.length==quarter);assert(f.start==write-quarter);
  assert(f.phase==(effect==7?32768u:65536u));
  if(effect==6){for(unsigned i=0;i<quarter+1;++i){l=r=0;gd_punch(&f,&p,(1u<<23)|(1u<<6),i,&l,&r);}uint32_t phase=f.phase;l=r=0;gd_punch(&f,&p,(1u<<23)|(1u<<6),0,&l,&r);assert(f.phase==phase);}
 }
 /* Delay modulation has its own live history even during tape capture. */
 for(unsigned mod=0;mod<2;++mod){GDPunch f={0};unsigned effect=mod?21:15;
  for(unsigned i=0;i<6000;++i){int32_t l=gd_sine(i*17000000u)/3,r=l;gd_punch(&f,&p,(1u<<5)|(1u<<effect),i,&l,&r);}
  assert(f.mod[mod].write==6000&&f.mod[mod].age>2000);
  uint32_t write=f.mod[mod].write;p.effect[effect][1]=127;int32_t l=1000,r=1000;gd_punch(&f,&p,(1u<<5)|(1u<<effect),0,&l,&r);assert(f.mod[mod].write==write+1);
 }
 return 0;
}
