/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef GD_OUTPUT_H
#define GD_OUTPUT_H
#include <stdint.h>
/* Final stereo output, in signed 16-bit PCM units. +18.06 dB clean trim
 * relative to the original -6 dB DAC conversion, after MASTER and all FX.
 * Linked peak limiting: immediate attack, about 46 ms release at 44.1 kHz.
 * No allocation, lookahead, state resets on edits, or per-channel image shift. */
typedef struct { int32_t gain_q16; } GDOutput;
static inline void gd_output_frame(GDOutput *s, int32_t *l, int32_t *r,
                                   uint32_t master_q12) {
  int32_t a=(int32_t)((int64_t)*l*master_q12*4/4096);
  int32_t b=(int32_t)((int64_t)*r*master_q12*4/4096);
  int32_t peak=a<0?-a:a, other=b<0?-b:b;
  if(other>peak)peak=other;
  int32_t target=peak>28000?(int32_t)((int64_t)28000*65536/peak):65536;
  if(target<s->gain_q16)s->gain_q16=target;
  else {int32_t release=(65536-s->gain_q16+2047)/2048;s->gain_q16+=release;}
  /* The release must never overshoot the instantaneous safe gain. */
  if(s->gain_q16>target)s->gain_q16=target;
  *l=(int32_t)((int64_t)a*s->gain_q16/65536);
  *r=(int32_t)((int64_t)b*s->gain_q16/65536);
}
#endif
