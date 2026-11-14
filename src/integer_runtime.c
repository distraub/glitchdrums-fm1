/* SPDX-License-Identifier: GPL-3.0-only
 * Portable compiler-runtime support for the bare-metal pi32v2 target.
 * No floating point, allocation, division recursion or external runtime.
 * Common audio cases fit 32 bits and use the chip's native divide.
 */
#include <stdint.h>

static unsigned gd_bits32(uint32_t x) {
  unsigned n=0;
  if(x>=65536u){x>>=16;n+=16;}
  if(x>=256u){x>>=8;n+=8;}
  if(x>=16u){x>>=4;n+=4;}
  if(x>=4u){x>>=2;n+=2;}
  if(x>=2u){x>>=1;n+=1;}
  return n+(x!=0);
}
static unsigned gd_bits64(uint64_t x) {
  uint32_t hi=(uint32_t)(x>>32);
  return hi?32u+gd_bits32(hi):gd_bits32((uint32_t)x);
}
/* Two base-65536 quotient digits for a 64/32 divide whose quotient fits
 * 32 bits. Normalization bounds each estimate correction to at most two.
 * All divides below are 32-bit, handled directly by the target CPU. */
static uint32_t gd_div64_32(uint64_t n,uint32_t divisor) {
  unsigned shift=32u-gd_bits32(divisor);
  uint32_t v=divisor<<shift,v1=v>>16,v0=v&65535u;
  uint64_t normalized=n<<shift;
  uint32_t top=(uint32_t)(normalized>>32);
  uint32_t mid=(uint32_t)(normalized>>16)&65535u,low=(uint32_t)normalized&65535u;
  uint32_t q1=top/v1,r=top-q1*v1;
  while(q1>=65536u||(uint64_t)q1*v0>((uint64_t)r<<16)+mid){
    --q1;r+=v1;if(r>=65536u)break;
  }
  uint32_t rest=(uint32_t)(((uint64_t)top<<16)+mid-(uint64_t)q1*v);
  uint32_t q0=rest/v1;r=rest-q0*v1;
  while(q0>=65536u||(uint64_t)q0*v0>((uint64_t)r<<16)+low){
    --q0;r+=v1;if(r>=65536u)break;
  }
  return (q1<<16)+q0;
}
uint64_t __udivdi3(uint64_t n, uint64_t d) {
  if (!d) return 0;
  /* floor(n/(odd*2^k)) == floor((n>>k)/odd). Fixed-point audio
   * frequently becomes one native 32-bit divide after cancelling 2^k. */
  if(!(d&1u)){
    unsigned shift=gd_bits64(d&((uint64_t)0-d))-1u;
    n>>=shift;d>>=shift;
  }
  if ((n >> 32) == 0 && (d >> 32) == 0)
    return (uint32_t)n / (uint32_t)d;
  if (n < d) return 0;
  if(!(d>>32)&&(uint32_t)(n>>32)<(uint32_t)d)
    return gd_div64_32(n,(uint32_t)d);
  unsigned bits=gd_bits64(d);
  /* Q-format divisors are common in audio: no restoring-division loop. */
  if (!(d & (d-1u))) return n>>(bits-1u);
  /* Align once using bounded 32-bit tests instead of up to 63 64-bit
   * shifts/comparisons before the quotient loop even starts. */
  unsigned shift=gd_bits64(n)-bits;
  uint64_t bit=(uint64_t)1u<<shift, q=0;
  d<<=shift;
  do {
    if (n >= d) { n -= d; q |= bit; }
    d >>= 1;
    bit >>= 1;
  } while (bit);
  return q;
}

int64_t __divdi3(int64_t n, int64_t d) {
  uint64_t un = n < 0 ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
  uint64_t ud = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d;
  uint64_t q = __udivdi3(un, ud);
  return (int64_t)((n < 0) != (d < 0) ? (uint64_t)0 - q : q);
}

uint64_t __umoddi3(uint64_t n, uint64_t d) {
  return d ? n - __udivdi3(n, d) * d : 0;
}

int64_t __moddi3(int64_t n, int64_t d) {
  uint64_t un = n < 0 ? (uint64_t)0 - (uint64_t)n : (uint64_t)n;
  uint64_t ud = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d;
  uint64_t rem = __umoddi3(un, ud);
  return (int64_t)(n < 0 ? (uint64_t)0 - rem : rem);
}
