#include "controls.h"
#include "engine.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static GDProject p;static GDEngine reference,blocked;
int main(void){uint32_t rng=0x7859;
 for(unsigned run=0;run<16;++run){gd_defaults(&p);reference=(GDEngine){0};blocked=(GDEngine){0};p.reserved=run%3;
  for(unsigned lane=0;lane<8;++lane){for(unsigned k=0;k<2;++k)p.track[lane].lfo[k]=(GDLFO){run%5,run%12,(run*9)%128,(run+k)%9};}
  for(unsigned at=0;at<10000;){
   if(at%137==0){p.bpm=40+gd_random(&rng)%201;p.track[run%8].mute=(at/137)%3==1;unsigned lane=gd_random(&rng)%8;
    unsigned lock_value=gd_random(&rng)%128;gd_hit(&reference,&p,lane,90+(at%38),(int)(at%25)-12,29+(int)(at%2),lock_value);
    gd_hit(&blocked,&p,lane,90+(at%38),(int)(at%25)-12,29+(int)(at%2),lock_value);
    for(unsigned l=0;l<8;++l){reference.delay_send[l]=blocked.delay_send[l]=(uint8_t)(gd_random(&rng)%128);p.track[l].lfo[0].rate=(uint8_t)(gd_random(&rng)%12);}
   }
   unsigned count=1+gd_random(&rng)%16;if(count>137-at%137)count=137-at%137;if(count>10000-at)count=10000-at;
   GDRenderBlock b;count=gd_render_block(&blocked,&p,&b,count);assert(count);
   for(unsigned i=0;i<count;++i){int32_t l,r;gd_render_sample(&reference,&p,&l,&r);assert(l==b.left[i]);assert(r==b.right[i]);assert(reference.delay_input==b.send[i]);}
   assert(!memcmp(&reference,&blocked,sizeof reference));at+=count;
  }
 }
 puts("PASS: 16-frame voice-major blocks preserve PCM and complete state across triggers, LFO/tempo changes, mutes and sends");return 0;}
