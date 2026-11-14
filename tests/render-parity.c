#include "controls.h"
#include "engine.h"
#include <stdio.h>
#include <string.h>
static GDProject p;static GDEngine e;
int main(void){uint32_t rng=0x44aabb;
for(unsigned run=0;run<32;++run){gd_defaults(&p);memset(&e,0,sizeof e);p.reserved=run%3;
 if(run)for(unsigned l=0;l<8;++l){for(unsigned j=0;j<29;++j)p.track[l].param[j]=gd_random(&rng)%((l==6?gd_gctrl[j].max:l==7?gd_actrl[j].max:127)+1);for(unsigned k=0;k<2;++k)p.track[l].lfo[k]=(GDLFO){run%5,run%12,run%128,(run+k)%9};}
 uint64_t hash=1469598103934665603ull;
 for(unsigned f=0;f<16384;++f){if(f%2048==0)for(unsigned l=0;l<8;++l)gd_hit(&e,&p,l,100,(int)(f/2048)-4,-1,0);
 if(f==8192)for(unsigned l=0;l<8;++l)p.track[l].param[29]=(run*3)&127;
 int32_t l,r;gd_render_sample(&e,&p,&l,&r);hash=(hash^(uint32_t)l)*1099511628211ull;hash=(hash^(uint32_t)r)*1099511628211ull;
 }printf("%u %016llx\n",run,(unsigned long long)hash);
}return 0;}
