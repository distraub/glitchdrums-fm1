/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Each byte packs pattern A-P and repeat count 1-16. */
typedef struct {uint8_t row[16],length,mode;} GDChain; /* OFF / LOOP / ONCE */
typedef struct {uint8_t row,pass,fresh,running;} GDChainClock;
static inline void gd_chain_defaults(GDChain *c){for(unsigned n=0;n<16;++n)c->row[n]=(uint8_t)n;c->length=1;c->mode=0;}
static inline int gd_chain_valid(const GDChain *c){return c->length>=1&&c->length<=16&&c->mode<=2;}
static inline void gd_chain_start(GDChainClock *s,const GDChain *c){*s=(GDChainClock){0,1,1,c->mode?1:0};}
/* Called only at step zero. 0 = same row, 1 = next row, 2 = finished. */
static inline unsigned gd_chain_boundary(GDChainClock *s,const GDChain *c){
 if(!s->running)return 0;if(s->fresh){s->fresh=0;return 0;}
 if(s->pass<1u+(c->row[s->row]>>4)){++s->pass;return 0;}
 if(s->row+1u>=c->length){if(c->mode==2){s->running=0;return 2;}s->row=0;}else ++s->row;
 s->pass=1;return 1;
}
