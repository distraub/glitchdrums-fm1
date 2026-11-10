/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef GD_FX_H
#define GD_FX_H
#include "engine.h"
#include "fm1.h"
#include "roll_tables.h"
/* The framebuffer and these buffers share foma's external RAM pool. */
static int16_t gd_delay[32768] POOL;
static int16_t gd_roll[GD_GLITCH_FRAMES] POOL;
typedef struct {
  uint32_t write, roll_write, roll_read, roll_start, hold_count, was_roll, roll_gate;
  uint32_t roll_length,roll_target,roll_rate,roll_bpm;
  int32_t damp, low[2], held[2];
} GDFX;
/* Fold long divisions by octaves to stay on-grid within the fixed delay RAM. */
static inline uint32_t gd_delay_frames(unsigned bpm, unsigned division, unsigned *folds) {
  static const uint8_t mult[6] = {1,2,3,4,6,8};
  uint32_t frames=(44100u*60u/(bpm*4u))*mult[division%6];
  unsigned n=0;
  while(frames>32767u){frames>>=1;++n;}
  if(folds)*folds=n;
  return frames;
}
static inline uint32_t gd_roll_frames(unsigned bpm,unsigned rate){
 uint64_t length=(uint64_t)44100u*240u*4294967296ull/((uint64_t)bpm*gd_roll_division[gd_glitch_range(bpm)][rate?rate:1]);
 return (uint32_t)(length<(16u<<16)?16u<<16:length>((GD_GLITCH_FRAMES-1u)<<16)?(GD_GLITCH_FRAMES-1u)<<16:length);
}
static inline void gd_fx_send(GDFX *f, const GDProject *p, int32_t *left,
                         int32_t *right, int32_t send) {
  const uint8_t *q = p->fx;
  int32_t l = *left, r = *right;
  uint32_t distance = gd_delay_frames(p->bpm,q[1],0);
  int32_t echo = gd_delay[(f->write - distance) & 32767];
  f->damp += (echo - f->damp) * (128 - (int)q[3]) / 128;
  gd_delay[f->write & 32767] =
      (int16_t)gd_sat(send + f->damp * (int)q[2] / 128);
  ++f->write;
  l += f->damp * (int)q[0] / 127;
  r += gd_delay[(f->write - distance / 2u) & 32767] * (int)q[0] / 127;
  l = gd_sat(l * (128 + (int)q[4] * 3) / 128);
  r = gd_sat(r * (128 + (int)q[4] * 3) / 128);
  uint32_t quantum = 1u << (q[5] > 12 ? 12 : q[5]);
  l = l / (int32_t)quantum * (int32_t)quantum;
  r = r / (int32_t)quantum * (int32_t)quantum;
  if (!f->hold_count) {
    f->held[0] = l;
    f->held[1] = r;
    f->hold_count = 1 + q[6] / 2u;
  }
  --f->hold_count;
  l = f->held[0];
  r = f->held[1];
  if (q[7] < 127) {
    unsigned hz = gd_cutoff_hz[q[7]];
    int a = (int)(hz * 32768u / (hz + 7020u));
    int32_t x[2] = {l, r};
    for (unsigned k = 0; k < 2; ++k) {
      int32_t v = gd_sat(x[k] - f->low[k] * (int)q[8] / 256);
      f->low[k] += (int32_t)((int64_t)(v - f->low[k]) * a >> 15);
      x[k] = f->low[k];
    }
    l = x[0];
    r = x[1];
  }
  uint32_t roll = f->roll_gate ? q[9] : 0;
  if (!roll) {
    gd_roll[f->roll_write++ & GD_GLITCH_MASK] = (int16_t)((l + r) / 2);
    f->was_roll = 0;
  } else {
    if(f->roll_rate!=roll||f->roll_bpm!=p->bpm||!f->roll_target){
      f->roll_rate=roll;f->roll_bpm=p->bpm;
      f->roll_target=gd_roll_frames(p->bpm,roll);
    }
    if (!f->was_roll) {
      f->roll_start = f->roll_write; /* fixed end anchor; history remains frozen */
      f->roll_read = 0;f->roll_length=f->roll_target;f->was_roll = 1;
    }
    int32_t change=(int32_t)f->roll_target-(int32_t)f->roll_length;
    f->roll_length+=(change/128?change/128:change?change>0?1:-1:0);
    if(f->roll_read>=f->roll_length)f->roll_read-=f->roll_length;
    uint32_t position=(f->roll_start<<16)-f->roll_length+f->roll_read;
    unsigned index=(position>>16)&GD_GLITCH_MASK,frac=position&65535;
    int32_t wet=gd_roll[index]+(int32_t)((int64_t)(gd_roll[(index+1)&GD_GLITCH_MASK]-gd_roll[index])*frac/65536);
    /* End-of-loop blend approaches the first sample, avoiding an abrupt seam. */
    unsigned seam_shift=f->roll_length>=(64u<<16)?3:0,seam=65536u<<seam_shift;
    unsigned edge=f->roll_length-f->roll_read;
    if(edge<seam){uint32_t first=(f->roll_start<<16)-f->roll_length;unsigned at=(first>>16)&GD_GLITCH_MASK;
      int32_t begin=gd_roll[at]+(int32_t)((int64_t)(gd_roll[(at+1)&GD_GLITCH_MASK]-gd_roll[at])*(first&65535)/65536);
      wet+=(int32_t)((int64_t)(begin-wet)*((seam-edge)>>seam_shift)/65536);
    }
    f->roll_read+=65536u;if(f->roll_read>=f->roll_length)f->roll_read-=f->roll_length;
    l += (wet - l) * (int)q[10] / 127;
    r += (wet - r) * (int)q[10] / 127;
  }
  int32_t mid = (l + r) / 2, side = (l - r) / 2;
  side = side * (int)q[11] / 64;
  *left = gd_sat(mid + side);
  *right = gd_sat(mid - side);
}
/* Legacy full-mix entry point for isolated processor tests. */
static inline void gd_fx(GDFX *f,const GDProject *p,int32_t *left,int32_t *right){gd_fx_send(f,p,left,right,(*left+*right)/2);}
#endif
