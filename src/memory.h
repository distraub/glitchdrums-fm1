/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* Clear in place. Large C compound-literal assignments can reserve a whole
 * temporary on pi32v2's stack even for globals; do not use them for projects. */
static __attribute__((noinline,unused)) void gd_clear(void *ptr,unsigned bytes){
 unsigned char *out=ptr;
 while(bytes--)*out++=0;
}

/* Dedicated native executable RAM. Flash-busy driver code remains in its
 * separate call-free .ram_text section; DSP runs only with flash available. */
#if GD_NATIVE_FM1
#define GD_DSP_CODE __attribute__((noinline,section(".ram_dsp")))
/* XIP -> RAM can exceed pi32v2's direct-call reach. As for the flash
 * driver, a volatile function pointer prevents direct-call folding. */
static inline __attribute__((always_inline)) void *gd_dsp_address(void *p){void *volatile q=p;return q;}
#define GD_DSP_INVOKE(fn) ((__typeof__(&fn))gd_dsp_address((void *)&fn))
#else
#define GD_DSP_CODE inline
#define GD_DSP_INVOKE(fn) fn
#endif
