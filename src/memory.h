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
#else
#define GD_DSP_CODE inline
#endif
