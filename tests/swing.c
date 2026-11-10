/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include "../src/swing.h"
int main(void){
 for(unsigned base=2756;base<=16537;base+=47)for(unsigned grid=0;grid<2;++grid)for(unsigned amount=0;amount<=100;++amount){
  unsigned encoded=(grid?128u:0u)|amount,previous=0,total=0;
  for(unsigned n=0;n<128;++n){
   unsigned length=gd_swing_interval(base,n,encoded,&previous);assert(length>0);total+=length;
   assert(total==(n+1)*base+gd_swing_delay(base,n+1,encoded));
   if((n&3)==3)assert(total==(n+1)*base);
  }
 }
 /* Change amount mid-pair: beat anchors must recover without a phase shift. */
 unsigned previous=0,total=0,base=5512;
 for(unsigned n=0;n<64;++n){unsigned setting=n<1?0:n<17?228:n<33?50:128;total+=gd_swing_interval(base,n,setting,&previous);assert(total==(n+1)*base+gd_swing_delay(base,n+1,setting));}
 /* Tempo jump from the slowest to fastest setting cannot underflow a timer. */
 previous=16537;assert(gd_swing_interval(2756,0,0,&previous)==1);
 return 0;
}
