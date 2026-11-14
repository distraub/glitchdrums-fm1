/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* Clear in place. Large C compound-literal assignments can reserve a whole
 * temporary on pi32v2's stack even for globals; do not use them for projects. */
static __attribute__((noinline,unused)) void gd_clear(void *ptr,unsigned bytes){
 unsigned char *out=ptr;
 while(bytes--)*out++=0;
}
