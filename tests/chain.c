#include "../src/chain.h"
#include <assert.h>
#include <stdio.h>
int main(void){GDChain c;GDChainClock s;gd_chain_defaults(&c);assert(gd_chain_valid(&c));gd_chain_start(&s,&c);assert(!s.running);
 c.mode=1;c.length=3;c.row[0]=0x10;c.row[1]=2;c.row[2]=0x2f;gd_chain_start(&s,&c);
 assert(gd_chain_boundary(&s,&c)==0&&s.row==0&&s.pass==1);
 assert(gd_chain_boundary(&s,&c)==0&&s.pass==2);assert(gd_chain_boundary(&s,&c)==1&&s.row==1);
 assert(gd_chain_boundary(&s,&c)==1&&s.row==2&&s.pass==1);assert(gd_chain_boundary(&s,&c)==0&&s.pass==2);assert(gd_chain_boundary(&s,&c)==0&&s.pass==3);assert(gd_chain_boundary(&s,&c)==1&&s.row==0);
 c.mode=2;gd_chain_start(&s,&c);assert(gd_chain_boundary(&s,&c)==0);for(unsigned n=0;n<5;++n)assert(gd_chain_boundary(&s,&c)!=2);assert(gd_chain_boundary(&s,&c)==2&&!s.running);
 c.mode=1;c.length=16;for(unsigned n=0;n<16;++n)c.row[n]=(uint8_t)(240|n);gd_chain_start(&s,&c);assert(gd_chain_boundary(&s,&c)==0);
 for(unsigned n=1;n<=256;++n){unsigned event=gd_chain_boundary(&s,&c);assert(s.row==(n/16)%16);assert(s.pass==1+n%16);assert(event==(n%16?0u:1u));}
 c.length=0;assert(!gd_chain_valid(&c));c.length=16;c.mode=3;assert(!gd_chain_valid(&c));puts("PASS: chain OFF/LOOP/ONCE, exact repeats, sixteen rows/repeats, fresh start and validation");}
