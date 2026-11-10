/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdlib.h>
#include "signature.h"
int main(void){
 for(int x=-400000;x<=400000;x+=73){
  int exact=(int)((int64_t)x*30000/(30000+abs(x)));
  assert(abs(gd_signature_bound(x)-exact)<=4);
 }
 /* Constant capture makes any grain-edge jump an algorithm artifact.
  * Include capture seeding and grain churn, all density/pitch extremes. */
 for(unsigned density=0;density<=127;density+=127)for(unsigned pitch=0;pitch<=127;pitch+=127){
  GDSignature s={0};uint8_t c[3][4]={{64,64,64,64},{64,64,64,64},{64,density,127,pitch}};
  int previous=6000,max_delta=0;
  for(unsigned n=0;n<50000;++n){int32_t l=6000,r=6000;
   gd_signature_controls(&s,1u<<29,n?0:1u<<29,c,&l,&r);
   int delta=abs(l-previous);if(delta>max_delta)max_delta=delta;previous=l;
   assert(abs(l)<=30000&&abs(r)<=30000);
  }
  assert(max_delta<100);
 }
 return 0;
}
