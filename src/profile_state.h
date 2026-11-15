/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Audio IRQ owns accumulation. UI snapshots are copied with IRQs masked. */
typedef struct {uint32_t work[11],peak[13];unsigned enabled;} GDProfiler;
static inline void gd_profiler_begin(GDProfiler *s,unsigned enabled){
 if(enabled){for(unsigned i=0;i<11;++i)s->work[i]=0;if(!s->enabled)for(unsigned i=0;i<13;++i)s->peak[i]=0;}
 s->enabled=enabled;
}
static inline void gd_profiler_add(GDProfiler *s,unsigned stage,uint32_t ticks){if(s->enabled&&stage<11)s->work[stage]+=ticks;}
static inline void gd_profiler_end(GDProfiler *s,uint32_t total){
 if(!s->enabled)return;uint32_t sum=0;
 for(unsigned i=0;i<11;++i){sum+=s->work[i];if(s->work[i]>s->peak[i])s->peak[i]=s->work[i];}
 uint32_t other=total>sum?total-sum:0;if(other>s->peak[11])s->peak[11]=other;if(total>s->peak[12])s->peak[12]=total;
}
