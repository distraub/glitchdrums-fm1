/* Shared delay routing: sends never gate the dry bus or bypass master filtering. */
#include "engine.h"
#include "fx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 GDProject p;gd_defaults(&p);GDEngine both={0},snare={0};
 both.delay_send[1]=snare.delay_send[1]=127;
 gd_hit(&both,&p,0,110,0,-1,0);gd_hit(&both,&p,1,110,0,-1,0);gd_hit(&snare,&p,1,110,0,-1,0);
 int differentDry=0,wet=0;
 for(unsigned n=0;n<22050;++n){int32_t l,r,a,b;gd_render_sample(&both,&p,&l,&r);gd_render_sample(&snare,&p,&a,&b);assert(both.delay_input==snare.delay_input);differentDry|=l!=a||r!=b;wet|=snare.delay_input!=0;}
 assert(differentDry&&wet);
 /* Eight buses include every polyphonic slot; zero send preserves audible dry output. */
 for(unsigned lane=0;lane<8;++lane){GDEngine dry={0};gd_hit(&dry,&p,lane,110,0,-1,0);int audible=0;
  for(unsigned n=0;n<2048;++n){int32_t l,r;gd_render_sample(&dry,&p,&l,&r);assert(dry.delay_input==0);audible|=l!=0||r!=0;}assert(audible);
 }
 /* No input to the delay means no new echoes, independent of its return mix. */
 memset(gd_delay,0,sizeof gd_delay);GDFX fx={0};p.fx[0]=127;
 for(unsigned n=0;n<40000;++n){int32_t l=100,r=-50;gd_fx_send(&fx,&p,&l,&r,0);assert(gd_delay[(fx.write-1)&32767]==0);}
 /* An impulse returns from the single buffer, and reducing send retains its tail. */
 memset(gd_delay,0,sizeof gd_delay);fx=(GDFX){0};p.fx[2]=0;p.fx[4]=0;p.fx[7]=127;int returned=0;
 for(unsigned n=0;n<40000;++n){int32_t l=0,r=0;gd_fx_send(&fx,&p,&l,&r,n==0?10000:0);if(n>0)returned|=l!=0||r!=0;}assert(returned);
 /* Global cutoff still affects dry voices when every delay send is zero. */
 memset(gd_delay,0,sizeof gd_delay);fx=(GDFX){0};p.fx[0]=0;p.fx[7]=0;int32_t l=10000,r=10000;gd_fx_send(&fx,&p,&l,&r,0);assert(l<1000&&r<1000);
 puts("PASS: isolated track sends, dry voices on every lane, shared delay tail and global filter routing");
}
