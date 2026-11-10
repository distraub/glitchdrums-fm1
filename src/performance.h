/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "engine.h"
typedef struct {uint32_t write,phase,age;int16_t data[2][2048];int low[2];} GDModDelay;
static inline int gd_performance_curve(int t,unsigned curve){
 int square=t*t/32768;
 return curve>=64?t+(square-t)*(int)(curve-64)/63:t+(t-square)*(int)(64-curve)/64;
}
static inline unsigned gd_capture_size(const GDProject *p,unsigned mode){
 unsigned quarter=44100u*60u/p->bpm,base=mode<4?quarter>>mode:mode==4?quarter/6:quarter;
 unsigned n=base*(64u+p->effect[mode][0])/128u;return n<32?32:n>32768?32768:n;
}
static inline unsigned gd_brake_time(const GDProject *p){return (44100u*60u/p->bpm)*(32u+p->effect[6][1]*3u)/224u;}
static inline void gd_mod_delay(GDModDelay *s,const uint8_t q[4],int wobble,int down,int *l,int *r){
 if(down){s->age=0;s->phase=0;s->low[0]=s->low[1]=0;}
 s->phase+=(wobble?(1u+q[0]*3u)*97391u/10u:(5u+q[0]*3u)*97391u/100u);
 int source[2]={*l,*r};unsigned span=wobble?1u+q[1]:1u+q[1]*6u,base=wobble?40:16;
 for(unsigned k=0;k<2;++k){
  unsigned sine=(unsigned)(gd_sine(s->phase+(k?0x40000000u:0))+32768);
  unsigned position=base*256u+sine*span/256u,delay=position>>8,frac=position&255u;
  int wet=0;if(s->age>delay+1){int a=s->data[k][(s->write-delay)&2047],b=s->data[k][(s->write-delay-1)&2047];wet=a+(b-a)*(int)frac/256;}
  s->low[k]=(s->low[k]*3+wet)/4;
  s->data[k][s->write&2047]=(int16_t)gd_sat(source[k]+s->low[k]*(int)q[2]/170);
  source[k]+=(wet-source[k])*(int)q[3]/127;
 }
 ++s->write;if(s->age<4096)++s->age;*l=source[0];*r=source[1];
}
