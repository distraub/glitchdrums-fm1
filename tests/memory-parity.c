/* SPDX-License-Identifier: GPL-3.0-only
 * Bit-exact baseline before native buffer packing (0.17.6). */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "fm1.h"
#include "fx.h"
#include "punch.h"
static GDModDelay mod;
static GDSignature sig;
static uint64_t hash=1469598103934665603ULL;
static void sample(int l,int r){hash^=(uint32_t)l;hash*=1099511628211ULL;hash^=(uint32_t)r;hash*=1099511628211ULL;}
int main(void){
 for(unsigned wobble=0;wobble<2;++wobble)for(unsigned width=0;width<128;++width){
  mod=(GDModDelay){0};uint8_t q[4]={71,width,112,127};
  for(unsigned n=0;n<7000;++n){int l=(int)((n*7919)%45001)-22500,r=(int)((n*3671)%39001)-19500;
   if(n==3300)q[1]=(uint8_t)(127-width);
   gd_mod_delay(&mod,q,wobble,n==0||n==5000,&l,&r);sample(l,r);
  }
 }
 for(unsigned e=0;e<8;++e){sig=(GDSignature){0};uint8_t q[3][4]={{91,127,88,111},{111,87,67,127},{119,127,111,127}};
  for(unsigned n=0;n<50000;++n){int32_t l=(int)((n*7919)%22001)-11000,r=(int)((n*3671)%16001)-8000;
   uint32_t mask=e<<27;uint32_t down=n==0||n==18000?mask:0;
   if(n==30000){q[0][3]=3;q[2][1]=7;}
   gd_signature_controls(&sig,mask,down,q,&l,&r);sample(l,r);
  }
 }
 assert(hash==UINT64_C(0xdd668df9efc4a4fd));
 return 0;
}
