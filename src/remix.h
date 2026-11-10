/* SPDX-License-Identifier: GPL-3.0-only
 * Transient clock-to-source routing, inspired by the Glitch apps' slice maps. */
#pragma once
#include "engine.h"
typedef struct {uint8_t map[16],length,mode,variation,depth,gate,dirty;} GDRemix;
static const char *const gd_remix_names[11]={"SHUFFLE","REVERSE","PAIRS","REPEAT","SKIP","BOUNCE","ROTATE","CHUNKS","WALK","BREAKS","4 TO FLOOR"};
static inline void gd_remix_make(GDRemix *r,unsigned length){
 length=(unsigned)gd_clamp((int)length,1,16);uint8_t shuffled[16],blocks[8],walk[16];
 uint32_t random=0x6d2b79f5u^((uint32_t)r->variation*0x9e3779b9u)^((uint32_t)r->mode*0x85ebca6bu);
 for(unsigned i=0;i<length;++i)shuffled[i]=(uint8_t)i;
 for(unsigned i=length-1;i>0;--i){unsigned j=gd_random(&random)%(i+1);uint8_t tmp=shuffled[i];shuffled[i]=shuffled[j];shuffled[j]=tmp;}
 unsigned group=(r->variation&1)?4:2,count=(length+group-1)/group;
 for(unsigned i=0;i<count;++i)blocks[i]=(uint8_t)i;
 for(unsigned i=count-1;i>0;--i){unsigned j=gd_random(&random)%(i+1);uint8_t tmp=blocks[i];blocks[i]=blocks[j];blocks[j]=tmp;}
 unsigned previous=length;
 for(unsigned i=0;i<length;++i){unsigned roll=gd_random(&random)%11,source;if(!roll)source=0;else if(roll&1)source=previous>=length?gd_random(&random)%length:(previous+1)%length;else source=gd_random(&random)%length;walk[i]=(uint8_t)source;previous=source;}
 uint32_t density=0x731a92cbu^((uint32_t)r->variation*0x27d4eb2du)^r->mode;
 for(unsigned i=0;i<16;++i){unsigned n=i%length,source=n;
  switch(r->mode%11){
   case 0:source=shuffled[n];if(!n&&r->depth<96)source=0;break;
   case 1:source=(length-1-n+r->variation%length)%length;break;
   case 2:source=(n^1u)%length;break;
   case 3:{unsigned size=2;source=((n/size)*size)%length;break;}
   case 4:source=(n*2)%length;break;
   case 5:{unsigned period=length>1?2*length-2:1;source=(n+length/2)%period;if(source>=length)source=period-source;break;}
   case 6:source=(n+1+r->variation)%length;break;
   case 8:source=walk[n];break;
   case 9:{static const uint8_t breaks[16]={0,1,2,3,4,5,0,1,2,3,4,5,6,7,0,1};source=breaks[n]%length;break;}
   case 10:source=(n%4)%length;break;
   case 7:source=(blocks[n/group]*group+n%group)%length;break;
  }
  r->map[i]=(uint8_t)source;
 }
 /* Favorites retain their existing algorithms. Other modes mutate their base
  * phrase with repeatable local edits rather than uniform source transposition. */
 if(r->variation&&r->mode!=0&&r->mode!=1&&r->mode!=6&&r->mode!=7&&r->mode!=8){
  uint8_t base[16];for(unsigned i=0;i<length;++i)base[i]=r->map[i];
  unsigned edits=3+r->variation%5;
  for(unsigned edit=0;edit<edits;++edit){
   unsigned start=gd_random(&random)%length,from=gd_random(&random)%length,size=2+gd_random(&random)%4,op=gd_random(&random)%6;
   uint8_t before[16];for(unsigned i=0;i<length;++i)before[i]=r->map[i];
   for(unsigned n=0;n<size;++n){unsigned at=(start+n)%length,source;
    switch(op){
     case 0:source=before[(from+n)%length];break;
     case 1:source=before[(start+size-1-n)%length];break;
     case 2:source=before[from];break;
     case 3:source=(before[from]+n)%length;break;
     case 4:source=before[(from+n*2)%length];break;
     default:source=n&1?before[from]:before[(from+1)%length];break;
    }
    r->map[at]=(uint8_t)source;
   }
  }
  if(r->mode==10)for(unsigned i=0;i<length;i+=4)r->map[i]=0;
  if(r->mode==9)r->map[0]=0;
  /* Guarantee a non-transposition variant if random local edits cancel out. */
  int shifted=1;unsigned offset=(r->map[0]+length-base[0])%length;
  for(unsigned i=1;i<length;++i)if((r->map[i]+length-base[i])%length!=offset)shifted=0;
  if(shifted&&length>1){unsigned at=(r->mode==9||r->mode==10)?1:gd_random(&random)%length;r->map[at]=(uint8_t)((r->map[at]+1)%length);}
 }
 for(unsigned i=0;i<16;++i){unsigned n=i%length,choose=gd_random(&density)%127;r->map[i]=(uint8_t)(r->depth==127||choose<r->depth?r->map[n]:n);}
 r->length=(uint8_t)length;r->dirty=0;
}
static inline unsigned gd_remix_source(GDRemix *r,unsigned clock,unsigned length){
 if(!r->gate)return clock%length;
 if(r->dirty||r->length!=length)gd_remix_make(r,length);
 return r->map[clock%length];
}
/* Render the temporary route into ordinary pattern events on the control side. */
#include "patterns.h"
static inline int gd_remix_print(GDPattern *to,const GDPattern *from,const GDRemix *route){
 if(to==from||!gd_pattern_valid(from))return 0;
 unsigned length=1;for(unsigned l=0;l<8;++l)if(from->lane[l].length>length)length=from->lane[l].length;
 GDRemix path=*route;gd_remix_make(&path,length);gd_pattern_blank(to);
 for(unsigned l=0;l<8;++l){to->lane[l].length=(uint8_t)length;
  for(unsigned step=0;step<length;++step){unsigned source=path.map[step]%from->lane[l].length;
   to->lane[l].step[step]=from->lane[l].step[source];to->lane[l].chance[step]=from->lane[l].chance[source];to->lane[l].ratchet[step]=from->lane[l].ratchet[source];to->lane[l].pitch[step]=from->lane[l].pitch[source];to->lane[l].lock_id[step]=from->lane[l].lock_id[source];to->lane[l].lock_value[step]=from->lane[l].lock_value[source];
   if(l>=6){to->chord_count[l-6][step]=from->chord_count[l-6][source];for(unsigned n=0;n<3;++n)to->chord_pitch[l-6][step][n]=from->chord_pitch[l-6][source][n];}
  }
 }
 return gd_pattern_valid(to);
}
static inline int gd_remix_slot_used(const GDPattern *p){
 for(unsigned l=0;l<8;++l){const GDPatternLane *t=&p->lane[l];if(t->length!=16)return 1;for(unsigned s=0;s<16;++s)if(t->step[s]||t->chance[s]!=100||t->ratchet[s]!=1||t->pitch[s]||t->lock_id[s]!=255||t->lock_value[s])return 1;}
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s)if(p->chord_count[l][s])return 1;return 0;
}
