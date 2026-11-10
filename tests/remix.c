#include "remix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 GDProject project;gd_defaults(&project);GDProject before=project;
 const unsigned depths[]={0,1,64,96,127};
 for(unsigned length=1;length<=16;++length)for(unsigned mode=0;mode<11;++mode)for(unsigned variant=0;variant<128;++variant)for(unsigned d=0;d<5;++d){
  GDRemix r={.mode=(uint8_t)mode,.variation=(uint8_t)variant,.depth=(uint8_t)depths[d],.gate=1,.dirty=1},copy=r;
  gd_remix_make(&r,length);gd_remix_make(&copy,length);assert(!memcmp(r.map,copy.map,16));
  for(unsigned clock=0;clock<64;++clock){unsigned source=gd_remix_source(&r,clock,length);assert(source<length);if(!d)assert(source==clock%length);}
  r.gate=0;for(unsigned clock=0;clock<64;++clock)assert(gd_remix_source(&r,clock,length)==clock%length);
 }
 for(unsigned mode=1;mode<11;++mode){if(mode==1||mode==6||mode==7||mode==8)continue;GDRemix base={.mode=(uint8_t)mode,.depth=127};gd_remix_make(&base,16);for(unsigned variant=1;variant<128;++variant){GDRemix varied={.mode=(uint8_t)mode,.variation=(uint8_t)variant,.depth=127};gd_remix_make(&varied,16);unsigned offset=(varied.map[0]+16-base.map[0])%16;int uniform=1;for(unsigned i=1;i<16;++i)if((varied.map[i]+16-base.map[i])%16!=offset)uniform=0;assert(!uniform);if(mode==10)for(unsigned i=0;i<16;i+=4)assert(varied.map[i]==0);}}
 for(unsigned length=1;length<=16;++length)for(unsigned variant=0;variant<128;++variant){GDRemix rev={.mode=1,.variation=(uint8_t)variant,.depth=127,.gate=1},rot={.mode=6,.variation=(uint8_t)variant,.depth=127,.gate=1};for(unsigned step=0;step<length;++step){assert(gd_remix_source(&rev,step,length)==(length-1-step+variant%length)%length);assert(gd_remix_source(&rot,step,length)==(step+1+variant)%length);}}
 GDRemix breaks={.mode=9,.depth=127,.gate=1,.dirty=1};static const unsigned path[16]={0,1,2,3,4,5,0,1,2,3,4,5,6,7,0,1};for(unsigned i=0;i<16;++i)assert(gd_remix_source(&breaks,i,16)==path[i]);GDRemix floor={.mode=10,.depth=127,.gate=1,.dirty=1};for(unsigned i=0;i<16;++i)assert(gd_remix_source(&floor,i,16)==i%4);
 GDRemix reverse={.mode=1,.depth=127,.gate=1,.dirty=1};for(unsigned i=0;i<16;++i)assert(gd_remix_source(&reverse,i,16)==15-i);
 GDPattern source,printed;gd_pattern_blank(&source);source.lane[0].length=3;source.lane[0].step[1]=105;source.lane[0].lock_id[1]=7;source.lane[0].lock_value[1]=81;source.lane[6].step[5]=100;source.chord_count[0][5]=2;source.chord_pitch[0][5][0]=4;source.chord_pitch[0][5][1]=7;GDPattern untouched=source;
 assert(gd_remix_print(&printed,&source,&reverse));assert(!memcmp(&source,&untouched,sizeof source));
 for(unsigned l=0;l<8;++l)for(unsigned step=0;step<16;++step){unsigned from=(15-step)%source.lane[l].length;assert(printed.lane[l].length==16);assert(printed.lane[l].step[step]==source.lane[l].step[from]);assert(printed.lane[l].lock_id[step]==source.lane[l].lock_id[from]);}
 assert(printed.chord_count[0][10]==2&&printed.chord_pitch[0][10][1]==7);assert(gd_remix_slot_used(&printed));gd_pattern_blank(&printed);assert(!gd_remix_slot_used(&printed));
 assert(!memcmp(&before,&project,sizeof project));
 puts("PASS: bounded deterministic remix routes at every length/mode/variation/depth, bypass and reverse");
}
