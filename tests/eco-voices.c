#include "controls.h"
#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static GDProject p;static GDEngine e,reference;
int main(void){
 for(int exponent=-196608;exponent<=196608;exponent+=7){int neg=exponent<0;unsigned x=neg?(unsigned)-exponent:(unsigned)exponent,pos=x*192,index=pos>>15,f=pos&32767;uint32_t value=gd_kick_sweep_ratio[index]+(uint32_t)((uint64_t)(gd_kick_sweep_ratio[index+1]-gd_kick_sweep_ratio[index])*f>>15);assert(gds_pow2(exponent)==(neg?(uint32_t)(4294967296ull/value):value));}
 uint32_t rng=0x7943;
 for(unsigned i=0;i<100000;++i){int32_t x=(int32_t)gd_random(&rng);uint64_t magnitude=x<0?0ULL-(uint64_t)(int64_t)x:(uint64_t)x;int reference=(int)(magnitude*32768/(32768+magnitude));if(x<0)reference=-reference;assert(gds_softsign(x)==reference);}
 for(unsigned run=0;run<32;++run){gd_defaults(&p);memset(&e,0,sizeof e);p.reserved=10;
  for(unsigned lane=6;lane<8;++lane){if(run)for(unsigned i=0;i<29;++i){unsigned max=lane==6?gd_gctrl[i].max:gd_actrl[i].max;p.track[lane].param[i]=gd_random(&rng)%(max+1);}gd_hit(&e,&p,lane,127,(int)(run%49)-24,-1,0);assert(e.synth[lane-6].eco);}
  for(unsigned f=0;f<8192;++f){if(f==2048){p.track[6].param[12]=p.track[7].param[9]=127;}if(f==4096){p.track[6].param[13]=p.track[7].param[10]=127;}int32_t l,r;gd_render_sample(&e,&p,&l,&r);assert(l>=-30000&&l<=30000&&r>=-30000&&r<=30000);}
 }
 for(unsigned lane=6;lane<8;++lane){gd_defaults(&p);p.reserved=10;memset(&e,0,sizeof e);p.track[6].param[14]=3;gd_hit(&e,&p,lane,100,7,-1,0);reference=e;for(unsigned i=0;i<8;++i)reference.synth[i].eco=0;
  for(unsigned f=0;f<4410;++f){int32_t l,r;gd_render_sample(&e,&p,&l,&r);gd_render_sample(&reference,&p,&l,&r);unsigned slot=lane-6;assert(e.synth[slot].age==reference.synth[slot].age);assert(!memcmp(e.synth[slot].phase,reference.synth[slot].phase,sizeof e.synth[slot].phase));assert(!memcmp(e.synth[slot].env,reference.synth[slot].env,sizeof e.synth[slot].env));}
 }
 puts("PASS: ECO preserves static-pitch oscillator phases and envelope/age timing");
 puts("PASS: exact 32-bit synth ratios/saturation and bounded ECO voices across legal controls");return 0;
}
