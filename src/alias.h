/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
typedef struct {unsigned hold;int held[2],low[2],envelope;} GDAlias;
/* Shared digital colour: sample-rate hold, word reduction and reconstruction
 * tone. A fresh transient refreshes the hold immediately to anchor the beat. */
static inline void gd_alias(GDAlias *s,const uint8_t q[4],int *l,int *r){
 if(!q[3]){s->hold=0;return;}
 int source[2]={*l,*r},magnitude=(*l<0?-*l:*l),other=*r<0?-*r:*r;if(other>magnitude)magnitude=other;
 int attack=magnitude>s->envelope+1024;s->envelope=s->envelope*255/256;if(magnitude>s->envelope)s->envelope=magnitude;
 if(!s->hold||attack){s->held[0]=*l;s->held[1]=*r;s->hold=3+q[0]/3;}--s->hold;
 int quantum=1<<(q[1]/16),a=4096+(int)q[2]*224;
 for(unsigned k=0;k<2;++k){int wet=s->held[k]/quantum*quantum;
  s->low[k]=(int)(((int64_t)s->low[k]*(32768-a)+(int64_t)wet*a)/32768);
  source[k]+=(s->low[k]-source[k])*(int)q[3]/127;
 }
 *l=source[0];*r=source[1];
}
