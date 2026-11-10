/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* One saved byte: high bit selects eighths; lower seven bits are the same
 * 0..100 amount as GlitchStep. Legacy schemas stored 0..50 at sixteenths. */
static inline unsigned gd_swing_amount(unsigned encoded){return encoded&127u;}
static inline unsigned gd_swing_eighth(unsigned encoded){return !!(encoded&128u);}
static inline uint32_t gd_swing_delay(uint32_t base,uint32_t ordinal,unsigned encoded){
 unsigned phase=ordinal&3u,amount=gd_swing_amount(encoded);
 if(gd_swing_eighth(encoded)&&phase==2)return base*amount/100u;
 return phase&1u?base*amount/200u:0;
}
/* Anchor the next boundary to the unswung clock. On live changes subtract
 * the delay actually used for this boundary, rather than shifting the beat. */
static inline uint32_t gd_swing_interval(uint32_t base,uint32_t ordinal,unsigned encoded,uint32_t *previous_delay){
 uint32_t next=gd_swing_delay(base,ordinal+1u,encoded);
 int32_t length=(int32_t)base+(int32_t)next-(int32_t)*previous_delay;
 *previous_delay=next;
 return length>0?(uint32_t)length:1u;
}
