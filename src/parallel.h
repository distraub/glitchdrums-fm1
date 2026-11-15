/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Native one-job mailbox. The hardware implementation must hold the worker
 * before returning a failed join; callers may then retire partial state. */
#if GD_NATIVE_FM1 || GD_PARALLEL_TEST
#define GD_PARALLEL_ENABLED 1
int gd_parallel_begin(void (*fn)(uint32_t),uint32_t arg);
int gd_parallel_join(void);
void gd_parallel_reject(void);
unsigned gd_parallel_status(void);
#endif
#define GD_PARALLEL_COOKIE 0x47445031u
static inline uint32_t gd_parallel_check(uintptr_t d,uintptr_t p,uintptr_t b,unsigned n){
 return (uint32_t)d^(uint32_t)p^(uint32_t)b^(n*0x9E3779B1u)^GD_PARALLEL_COOKIE;
}
