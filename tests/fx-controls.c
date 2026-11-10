/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "fx.h"
#include "punch.h"
static uint64_t effect_hash(unsigned effect,unsigned parameter,unsigned value){
 GDPunch f={0};GDFX fx={0};GDProject p;gd_defaults(&p);p.punch[0]=127;p.effect[effect][parameter]=(uint8_t)value;
 p.fx[0]=p.fx[4]=p.fx[5]=p.fx[6]=p.fx[8]=p.fx[9]=0;p.fx[7]=127;p.fx[11]=64;
 memset(gd_delay,0,sizeof gd_delay);memset(gd_punch_tape,0,sizeof gd_punch_tape);uint64_t hash=1;
 if(effect<8)for(unsigned i=0;i<44100;++i){int32_t l=(gd_sine(i*17000000u)+gd_sine(i*57000000u))/5,r=l;gd_punch(&f,&p,0,0,&l,&r);}
 for(unsigned i=0;i<44100;++i){int32_t l=(gd_sine(i*17000000u)+gd_sine(i*57000000u))/5,r=gd_sine(i*23000000u)/4;
  gd_fx(&fx,&p,&l,&r);gd_punch(&f,&p,1u<<effect,fx.write,&l,&r);
  assert(l<=30000&&l>=-30000&&r<=30000&&r>=-30000);
  hash=(hash^(uint32_t)l)*1099511628211ull;hash=(hash^(uint32_t)r)*1099511628211ull;
 }
 return hash;
}
int main(void){
 for(unsigned effect=0;effect<30;++effect)for(unsigned k=0;k<4;++k){
  uint64_t a=effect_hash(effect,k,0),b=effect_hash(effect,k,127);
  if(a==b)fprintf(stderr,"No audible control: effect %u parameter %u\n",effect,k);
  assert(a!=b);
 }
 /* All carrier waves at a full-wet default trim cannot boost instantaneous
  * input peaks; LEVEL remains adjustable above this calibrated starting point. */
 for(unsigned wave=0;wave<3;++wave){GDPunch f={0};GDProject p;gd_defaults(&p);p.effect[20][2]=(uint8_t)(wave*43);
  for(unsigned i=0;i<10000;++i){int32_t l=5000,r=-5000;gd_punch(&f,&p,1u<<20,0,&l,&r);assert(l<=5000&&l>=-5000&&r<=5000&&r>=-5000);}
 }
 return 0;
}
