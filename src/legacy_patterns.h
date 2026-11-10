/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "engine.h"
/* Fixed RAM patterns. Only control-task serialization uses the sparse codec.
 * Active base sequence remains in GDProject; chord extras live with the bank. */
typedef struct {uint8_t step[64],chance[64],ratchet[64];int8_t pitch[64];uint8_t lock_id[64],lock_value[64],length;} GD64PatternLane;
typedef struct {GD64PatternLane lane[8];uint8_t pages;} GDLegacyABPattern;
typedef struct {GD64PatternLane lane[8];uint8_t pages;uint8_t chord_count[2][64];int8_t chord_pitch[2][64][3];} GD64Pattern;
static inline void gd64_pattern_get(GD64Pattern *p,const GDProject *project){
 p->pages=project->pages;
 for(unsigned l=0;l<8;++l){const GDTrack *t=&project->track[l];GD64PatternLane *d=&p->lane[l];
  d->length=t->length;
  for(unsigned s=0;s<64;++s){d->step[s]=t->step[s];d->chance[s]=t->chance[s];d->ratchet[s]=t->ratchet[s];d->pitch[s]=t->pitch[s];d->lock_id[s]=t->lock_id[s];d->lock_value[s]=t->lock_value[s];}}
}
static inline void gd64_pattern_put(GDProject *project,const GD64Pattern *p){
 project->pages=p->pages;
 for(unsigned l=0;l<8;++l){GDTrack *t=&project->track[l];const GD64PatternLane *d=&p->lane[l];t->length=d->length;
  for(unsigned s=0;s<64;++s){t->step[s]=d->step[s];t->chance[s]=d->chance[s];t->ratchet[s]=d->ratchet[s];t->pitch[s]=d->pitch[s];t->lock_id[s]=d->lock_id[s];t->lock_value[s]=d->lock_value[s];}}
}
static inline void gd64_pattern_blank(GD64Pattern *p){
 *p=(GD64Pattern){0};p->pages=1;
 for(unsigned l=0;l<8;++l){p->lane[l].length=64;for(unsigned s=0;s<64;++s){p->lane[l].chance[s]=100;p->lane[l].ratchet[s]=1;p->lane[l].lock_id[s]=255;}}
}
static inline int gd64_pattern_valid(const GD64Pattern *p){
 if(p->pages<1||p->pages>4)return 0;
 for(unsigned l=0;l<8;++l){const GD64PatternLane *t=&p->lane[l];if(!t->length||t->length>64)return 0;
  for(unsigned s=0;s<64;++s)if(t->step[s]>127||t->chance[s]>100||!t->ratchet[s]||t->ratchet[s]>4||t->pitch[s]<-48||t->pitch[s]>48||(t->lock_id[s]!=255&&t->lock_id[s]>=32)||t->lock_value[s]>127)return 0;}
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<64;++s){if(p->chord_count[l][s]>3)return 0;for(unsigned n=0;n<p->chord_count[l][s];++n)if(p->chord_pitch[l][s][n]<-48||p->chord_pitch[l][s][n]>48)return 0;}
 return 1;
}

#define GD64_PATTERN_COUNT 8
#define GD64_PATTERN_CAPACITY 3712
/* Sparse, lossless control-task codec. No codec work occurs in audio. */
static inline unsigned gd64_pattern_encode(const GD64Pattern *p,uint8_t *out,unsigned capacity){
 unsigned n=0;
 #define GD_WRITE(v) do{if(n>=capacity)return 0;if(out)out[n]=(uint8_t)(v);++n;}while(0)
 GD_WRITE(p->pages);
 for(unsigned l=0;l<8;++l){const GD64PatternLane *t=&p->lane[l];GD_WRITE(t->length);unsigned count_at=n;GD_WRITE(0);unsigned count=0;
  for(unsigned s=0;s<64;++s){unsigned flags=(t->pitch[s]?1u:0u)|(t->chance[s]!=100?2u:0u)|((t->lock_id[s]!=255||t->lock_value[s])?4u:0u)|((l>=6&&p->chord_count[l-6][s])?8u:0u);
   if(!t->step[s]&&!flags&&t->ratchet[s]==1)continue;
   ++count;GD_WRITE(s|((t->ratchet[s]-1u)<<6));GD_WRITE(t->step[s]|(flags?128u:0u));
   if(flags){GD_WRITE(flags);if(flags&1)GD_WRITE(t->pitch[s]);if(flags&2)GD_WRITE(t->chance[s]);if(flags&4){GD_WRITE(t->lock_id[s]);GD_WRITE(t->lock_value[s]);}if(flags&8){GD_WRITE(p->chord_count[l-6][s]);for(unsigned note=0;note<p->chord_count[l-6][s];++note)GD_WRITE(p->chord_pitch[l-6][s][note]);}}
  }if(out)out[count_at]=(uint8_t)count;
 }
 #undef GD_WRITE
 return n;
}
static inline int gd64_pattern_decode(GD64Pattern *p,const uint8_t *in,unsigned size){
 gd64_pattern_blank(p);unsigned n=0;
 #define GD_READ(v) do{if(n>=size)return 0;(v)=in[n++];}while(0)
 GD_READ(p->pages);if(p->pages<1||p->pages>4)return 0;
 for(unsigned l=0;l<8;++l){GD64PatternLane *t=&p->lane[l];unsigned count;GD_READ(t->length);GD_READ(count);if(!t->length||t->length>64||count>64)return 0;
  int previous=-1;
  for(unsigned j=0;j<count;++j){unsigned position,velocity,flags=0;GD_READ(position);GD_READ(velocity);unsigned s=position&63u;if((int)s<=previous)return 0;previous=(int)s;
   t->ratchet[s]=(uint8_t)(1+(position>>6));t->step[s]=(uint8_t)(velocity&127u);
   if(velocity&128u){GD_READ(flags);if(!flags||flags>15)return 0;
    if(flags&1){uint8_t pitch;GD_READ(pitch);t->pitch[s]=(int8_t)pitch;}
    if(flags&2)GD_READ(t->chance[s]);
    if(flags&4){GD_READ(t->lock_id[s]);GD_READ(t->lock_value[s]);}
    if(flags&8){if(l<6)return 0;GD_READ(p->chord_count[l-6][s]);if(!p->chord_count[l-6][s]||p->chord_count[l-6][s]>3)return 0;for(unsigned note=0;note<p->chord_count[l-6][s];++note){uint8_t pitch;GD_READ(pitch);p->chord_pitch[l-6][s][note]=(int8_t)pitch;}}
   }
  }
 }
 #undef GD_READ
 return n==size&&gd64_pattern_valid(p);
}

/* Active sequence already resides in GDProject; save only its chord extras. */
static inline unsigned gd64_chords_encode(const GD64Pattern *p,uint8_t *out,unsigned capacity){
 unsigned n=0;
 #define GD_WRITE(v) do{if(n>=capacity)return 0;if(out)out[n]=(uint8_t)(v);++n;}while(0)
 for(unsigned l=0;l<2;++l){unsigned at=n;GD_WRITE(0);unsigned count=0;
  for(unsigned s=0;s<64;++s)if(p->chord_count[l][s]){++count;GD_WRITE(s);GD_WRITE(p->chord_count[l][s]);for(unsigned note=0;note<p->chord_count[l][s];++note)GD_WRITE(p->chord_pitch[l][s][note]);}
  if(out)out[at]=(uint8_t)count;
 }
 #undef GD_WRITE
 return n;
}
static inline int gd64_chords_decode(GD64Pattern *p,const uint8_t *in,unsigned size){
 unsigned n=0;
 #define GD_READ(v) do{if(n>=size)return 0;(v)=in[n++];}while(0)
 for(unsigned l=0;l<2;++l){unsigned count;GD_READ(count);if(count>64)return 0;int previous=-1;
  for(unsigned j=0;j<count;++j){unsigned s;GD_READ(s);if(s>63||(int)s<=previous)return 0;previous=(int)s;
   GD_READ(p->chord_count[l][s]);if(!p->chord_count[l][s]||p->chord_count[l][s]>3)return 0;
   for(unsigned note=0;note<p->chord_count[l][s];++note){uint8_t pitch;GD_READ(pitch);p->chord_pitch[l][s][note]=(int8_t)pitch;}
  }
 }
 #undef GD_READ
 return n==size&&gd64_pattern_valid(p);
}

static inline void gd64_pattern_copy_chords(GD64Pattern *to,const GD64Pattern *from){
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<64;++s){to->chord_count[l][s]=from->chord_count[l][s];for(unsigned n=0;n<3;++n)to->chord_pitch[l][s][n]=from->chord_pitch[l][s][n];}
}
static inline void gd64_chord_add(uint8_t *count,int8_t pitches[3],int base,int pitch){
 if(pitch==base)return;for(unsigned n=0;n<*count;++n)if(pitches[n]==pitch)return;
 if(*count<3)pitches[(*count)++]=(int8_t)pitch;
}
