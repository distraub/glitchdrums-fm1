/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define FM1_C1_CHECK(fn,arg,job) ((uint32_t)(uintptr_t)(fn)^(arg)^(job)*0x9E3779B1u)
static inline int fm1_c1_job_valid(uint32_t job,uint32_t done,uintptr_t fn,uint32_t arg,uint32_t check){
 return job==done+1u&&fn&&check==FM1_C1_CHECK(fn,arg,job);
}
/* Success, bad mailbox, timeout, pending. Elapsed uses unsigned subtraction
 * in the caller, so the timer wrap cannot extend the deadline. */
static inline unsigned fm1_c1_poll(uint32_t bad,uint32_t done,uint32_t job,uint32_t elapsed){
 if(bad)return 1;if(done==job)return 0;if(elapsed>48000u)return 2;return 3;
}
