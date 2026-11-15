#include "controls.h"
#include "engine.h"
#include "fx.h"
#ifndef PUNCH_HEADER
#define PUNCH_HEADER "punch.h"
#endif
#include PUNCH_HEADER
#include <stdio.h>
#include <string.h>
static GDPunch f;static GDProject p;
int main(void){for(unsigned run=0;run<36;++run){gd_defaults(&p);memset(&f,0,sizeof f);memset(gd_punch_tape,0,sizeof gd_punch_tape);uint32_t seed=0x4271+run;uint64_t hash=1469598103934665603ull;
 for(unsigned at=0;at<40000;++at){if(at%117==0){p.bpm=40+gd_random(&seed)%201;p.punch[0]=gd_random(&seed)%128;p.punch[2]=gd_random(&seed)%128;for(unsigned e=0;e<30;++e)for(unsigned k=0;k<4;++k)p.effect[e][k]=gd_random(&seed)%128;}
  unsigned phase=at/4000;uint32_t mask=phase%3==2?0:run<7?(run+1u)<<27:run<30?1u<<run:(1u<<27)|(1u<<12)|(phase&1?1u<<23:0);
  if(phase==3)mask=1u<<(run%8);/* transition into/out of capture */
  int32_t l=(int)(gd_random(&seed)%50001)-25000,r=(int)(gd_random(&seed)%50001)-25000;
  gd_punch(&f,&p,mask,at,&l,&r);hash=(hash^(uint32_t)l)*1099511628211ull;hash=(hash^(uint32_t)r)*1099511628211ull;
 }
 for(unsigned i=0;i<30;++i)hash=(hash^f.age[i])*1099511628211ull;
 hash=(hash^f.write)*1099511628211ull;hash=(hash^f.previous)*1099511628211ull;hash=(hash^f.gate_previous)*1099511628211ull;
 for(unsigned i=0;i<32768;++i)hash=(hash^(uint16_t)gd_punch_tape[i])*1099511628211ull;
 printf("%u %016llx\n",run,(unsigned long long)hash);
 }return 0;}
