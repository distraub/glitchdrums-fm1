/* SPDX-License-Identifier: GPL-3.0-only
 * Portable compiler-runtime support for the bare-metal pi32v2 target.
 * No floating point, allocation, division recursion or external runtime.
 * Common audio cases fit 32 bits and use the chip's native divide.
 */
#include <stdint.h>

uint64_t __udivdi3(uint64_t n, uint64_t d) {
  if (!d) return 0; /* defined defensive behavior; callers must use nonzero divisors */
  if ((n >> 32) == 0 && (d >> 32) == 0)
    return (uint32_t)n / (uint32_t)d;
  if (n < d) return 0;
  uint64_t bit = 1, q = 0;
  while (d <= (n >> 1)) { d <<= 1; bit <<= 1; }
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
