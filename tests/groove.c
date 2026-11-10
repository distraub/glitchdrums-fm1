/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "fx.h"
#include "punch.h"
int main(int argc,char **argv){
 (void)argv;
 for(unsigned effect=27;effect<30;++effect)for(unsigned extreme=0;extreme<2;++extreme){
  GDPunch f={0};GDProject p;gd_defaults(&p);p.punch[0]=127;
  for(unsigned k=0;k<4;++k)p.effect[effect][k]=(uint8_t)(extreme?127:64);
  unsigned mask=1u<<effect;double difference=0,energy=0,min_hit=10;unsigned shifted=0;
  int peak=0,peak_at=0;
  for(unsigned i=0;i<88200;++i){
   unsigned step=i%5512;int32_t dry=step==0?12000:step<1800?(int)((int64_t)gd_sine(step*6500000u)*(1800-step)*2600/(1800*32768)):0;
   int32_t l=dry,r=dry;
   gd_punch(&f,&p,mask,i,&l,&r);
   assert(l<=30000&&l>=-30000&&r<=30000&&r>=-30000);
   if(i>11024){difference+=(double)(l-dry)*(l-dry);energy+=(double)dry*dry;}
   if(step==0){peak=abs(l);peak_at=0;if(i>11024){double ratio=(double)l/12000;if(ratio<min_hit)min_hit=ratio;}}
   if(step<32&&abs(l)>peak){peak=abs(l);peak_at=(int)step;}
   if(step==31&&i>11024&&peak_at)++shifted;
  }
  printf("effect %u %s: hit floor %.3f, shifted attacks %u, effect/dry RMS %.3f\n",effect,extreme?"MAX":"DEFAULT",min_hit,shifted,sqrt(difference/energy));
  if(argc==1){assert(min_hit>.85);assert(!shifted);assert(difference>energy*.000001);}
 }
 return 0;
}
