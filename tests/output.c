/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include "output.h"
int main(void) {
  GDOutput s={65536};int32_t l=1000,r=-500;
  gd_output_frame(&s,&l,&r,4096);assert(l==4000&&r==-2000);
  l=30000;r=15000;gd_output_frame(&s,&l,&r,4096);
  assert(l<=28000&&l>=27998);assert(r>=13998&&r<=14000);
  for(unsigned i=0;i<20000;++i){l=i&1?30000:-30000;r=-l/2;
    gd_output_frame(&s,&l,&r,4088);assert(l<=28000&&l>=-28000);assert(r<=14000&&r>=-14000);}
  for(unsigned i=0;i<24000;++i){l=0;r=0;gd_output_frame(&s,&l,&r,4096);assert(l==0&&r==0);}
  assert(s.gain_q16==65536);
  l=30000;r=-30000;gd_output_frame(&s,&l,&r,0);assert(l==0&&r==0);
  return 0;
}
