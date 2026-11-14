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
uint64_t __udivdi3(uint64_t n, uint64_t d) {
  if (!d) return 0;
  if ((n >> 32) == 0 && (d >> 32) == 0)
    return (uint32_t)n / (uint32_t)d;
  if (n < d) return 0;
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
