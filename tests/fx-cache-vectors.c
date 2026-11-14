#include "fx.h"
#include <stdio.h>
static GDProject p;static GDFX f;
int main(void){
for(unsigned run=0;run<8;++run){gd_defaults(&p);f=(GDFX){0};for(unsigned i=0;i<32768;++i)gd_delay[i]=0;for(unsigned i=0;i<GD_GLITCH_FRAMES;++i)gd_roll[i]=0;uint32_t seed=123+run;uint64_t h=1469598103934665603ull;
 for(unsigned i=0;i<70000;++i){
  if(i%2500==0){p.bpm=40+gd_random(&seed)%201;p.fx[0]=(i/2500)%3?0:gd_random(&seed)%128;p.fx[1]=gd_random(&seed)%6;p.fx[2]=gd_random(&seed)%100;p.fx[3]=gd_random(&seed)%128;p.fx[4]=gd_random(&seed)%128;p.fx[5]=gd_random(&seed)%13;p.fx[6]=gd_random(&seed)%128;p.fx[7]=gd_random(&seed)%128;p.fx[8]=gd_random(&seed)%128;p.fx[9]=gd_random(&seed)%128;p.fx[10]=gd_random(&seed)%128;p.fx[11]=gd_random(&seed)%128;f.roll_gate=(i/2500)%5==1;}
  int32_t l=(int)(gd_random(&seed)%40001)-20000,r=(int)(gd_random(&seed)%40001)-20000;int send=i<40000?(l+r)/2:0;gd_fx_send(&f,&p,&l,&r,send);h=(h^(uint32_t)l)*1099511628211ull;h=(h^(uint32_t)r)*1099511628211ull;
 }printf("%u %016llx\n",run,(unsigned long long)h);
}return 0;}
