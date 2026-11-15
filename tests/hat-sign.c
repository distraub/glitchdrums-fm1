#include "engine.h"
#include <assert.h>
#include <stdio.h>
int main(void){GDAcoustic a={0};assert(gd_sines[1024]==0&&gd_sines[1025]<0);
for(uint64_t p=0;p<4294967296ull;p+=512){a.phase[0]=(uint32_t)p;int value=gda_square(&a,0,0);assert(value==(gd_sine((uint32_t)p)>=0?32768:-32768));}
a.phase[0]=0xffffffffu;assert(gda_square(&a,0,1)==32768&&a.phase[0]==0);puts("PASS: exact hat-square sign across every sine interpolation input and phase wrap");return 0;}
