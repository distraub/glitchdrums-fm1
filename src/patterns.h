/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "engine.h"
#define GD_PATTERN_COUNT 16
#define GD_PATTERN_STEPS 16
#define GD_PATTERN_BYTES 672
/* 8 lengths x 4 bits + 128 steps x 36 bits + 32 chords x 23 bits. */
typedef struct {uint8_t step[16],chance[16],ratchet[16];int8_t pitch[16];uint8_t lock_id[16],lock_value[16],length;} GDPatternLane;
typedef struct {GDPatternLane lane[8];uint8_t pages,chord_count[2][16];int8_t chord_pitch[2][16][3];} GDPattern;
static inline void gd_pattern_blank(GDPattern *p){
 *p=(GDPattern){0};p->pages=1;
 for(unsigned l=0;l<8;++l){p->lane[l].length=16;for(unsigned s=0;s<16;++s){p->lane[l].chance[s]=100;p->lane[l].ratchet[s]=1;p->lane[l].lock_id[s]=255;}}
}
static inline void gd_pattern_get(GDPattern *p,const GDProject *project){
 p->pages=1;
 for(unsigned l=0;l<8;++l){const GDTrack *t=&project->track[l];GDPatternLane *d=&p->lane[l];d->length=(uint8_t)gd_clamp(t->length,1,16);
  for(unsigned s=0;s<16;++s){d->step[s]=t->step[s];d->chance[s]=t->chance[s];d->ratchet[s]=t->ratchet[s];d->pitch[s]=t->pitch[s];d->lock_id[s]=t->lock_id[s];d->lock_value[s]=t->lock_value[s];}}
}
static inline void gd_pattern_put(GDProject *project,const GDPattern *p){
 project->pages=1;
 for(unsigned l=0;l<8;++l){GDTrack *t=&project->track[l];const GDPatternLane *d=&p->lane[l];t->length=d->length;
  for(unsigned s=0;s<16;++s){t->step[s]=d->step[s];t->chance[s]=d->chance[s];t->ratchet[s]=d->ratchet[s];t->pitch[s]=d->pitch[s];t->lock_id[s]=d->lock_id[s];t->lock_value[s]=d->lock_value[s];}}
}
static inline int gd_pattern_valid(const GDPattern *p){
 if(p->pages!=1)return 0;
 for(unsigned l=0;l<8;++l){const GDPatternLane *t=&p->lane[l];if(!t->length||t->length>16)return 0;
  for(unsigned s=0;s<16;++s)if(t->step[s]>127||t->chance[s]>100||!t->ratchet[s]||t->ratchet[s]>4||t->pitch[s]<-48||t->pitch[s]>48||(t->lock_id[s]!=255&&t->lock_id[s]>=32)||t->lock_value[s]>127)return 0;}
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s){if(p->chord_count[l][s]>3)return 0;for(unsigned n=0;n<p->chord_count[l][s];++n)if(p->chord_pitch[l][s][n]<-48||p->chord_pitch[l][s][n]>48)return 0;}
 return 1;
}
/* Fields are at most seven bits, so a field touches at most two bytes. */
static inline void gd_bits_write(uint8_t *out,unsigned *at,unsigned value,unsigned count){
 unsigned index=*at>>3,shift=*at&7,bits=value<<shift;out[index]|=(uint8_t)bits;
 if(shift+count>8)out[index+1]|=(uint8_t)(bits>>8);*at+=count;
}
static inline unsigned gd_bits_read(const uint8_t *in,unsigned *at,unsigned count){
 unsigned index=*at>>3,shift=*at&7,value=in[index];if(shift+count>8)value|=(unsigned)in[index+1]<<8;
 *at+=count;return (value>>shift)&((1u<<count)-1u);
}
static inline unsigned gd_pattern_encode(const GDPattern *p,uint8_t *out,unsigned capacity){
 if(capacity<GD_PATTERN_BYTES||!gd_pattern_valid(p))return 0;if(!out)return GD_PATTERN_BYTES;
 for(unsigned n=0;n<GD_PATTERN_BYTES;++n)out[n]=0;unsigned at=0;
 for(unsigned l=0;l<8;++l){const GDPatternLane *t=&p->lane[l];gd_bits_write(out,&at,t->length-1,4);
  for(unsigned s=0;s<16;++s){gd_bits_write(out,&at,t->step[s],7);gd_bits_write(out,&at,t->chance[s],7);gd_bits_write(out,&at,t->ratchet[s]-1,2);gd_bits_write(out,&at,(unsigned)(t->pitch[s]+48),7);gd_bits_write(out,&at,t->lock_id[s]==255?32:t->lock_id[s],6);gd_bits_write(out,&at,t->lock_value[s],7);}}
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s){unsigned count=p->chord_count[l][s];gd_bits_write(out,&at,count,2);for(unsigned n=0;n<3;++n)gd_bits_write(out,&at,(unsigned)(48+(n<count?p->chord_pitch[l][s][n]:0)),7);}
 return at==GD_PATTERN_BYTES*8?GD_PATTERN_BYTES:0;
}
static inline int gd_pattern_decode(GDPattern *p,const uint8_t *in,unsigned size){
 if(size!=GD_PATTERN_BYTES)return 0;gd_pattern_blank(p);unsigned at=0;
 for(unsigned l=0;l<8;++l){GDPatternLane *t=&p->lane[l];t->length=(uint8_t)(1+gd_bits_read(in,&at,4));
  for(unsigned s=0;s<16;++s){t->step[s]=(uint8_t)gd_bits_read(in,&at,7);t->chance[s]=(uint8_t)gd_bits_read(in,&at,7);t->ratchet[s]=(uint8_t)(1+gd_bits_read(in,&at,2));t->pitch[s]=(int8_t)((int)gd_bits_read(in,&at,7)-48);unsigned lock=gd_bits_read(in,&at,6);if(lock>32)return 0;t->lock_id[s]=(uint8_t)(lock==32?255:lock);t->lock_value[s]=(uint8_t)gd_bits_read(in,&at,7);}}
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s){p->chord_count[l][s]=(uint8_t)gd_bits_read(in,&at,2);for(unsigned n=0;n<3;++n){int pitch=(int)gd_bits_read(in,&at,7)-48;if(pitch<-48||pitch>48)return 0;p->chord_pitch[l][s][n]=n<p->chord_count[l][s]?(int8_t)pitch:0;}}
 return gd_pattern_valid(p);
}
static inline void gd_pattern_copy_chords(GDPattern *to,const GDPattern *from){
 for(unsigned l=0;l<2;++l)for(unsigned s=0;s<16;++s){to->chord_count[l][s]=from->chord_count[l][s];for(unsigned n=0;n<3;++n)to->chord_pitch[l][s][n]=from->chord_pitch[l][s][n];}
}
static inline void gd_chord_add(uint8_t *count,int8_t pitches[3],int base,int pitch){
 if(pitch==base)return;for(unsigned n=0;n<*count;++n)if(pitches[n]==pitch)return;if(*count<3)pitches[(*count)++]=(int8_t)pitch;
}
