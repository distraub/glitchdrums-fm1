#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "../src/integer_runtime.c"
static uint64_t rng=0x1967abcdefULL;
static uint64_t next(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
int main(void){
  const uint64_t edge[]={0,1,2,3,UINT32_MAX,1ULL<<32,1ULL<<63,UINT64_MAX};
  for(unsigned i=0;i<sizeof edge/sizeof *edge;++i)for(unsigned j=1;j<sizeof edge/sizeof *edge;++j){
    uint64_t a=edge[i],b=edge[j];assert(__udivdi3(a,b)==a/b);assert(__umoddi3(a,b)==a%b);
  }
  for(unsigned i=0;i<20000;++i){
    uint64_t a=next(),b=next()|1;assert(__udivdi3(a,b)==a/b);assert(__umoddi3(a,b)==a%b);
    int64_t x=(int64_t)a,y=(int64_t)b;if(x==INT64_MIN&&y==-1)continue;
    assert(__divdi3(x,y)==x/y);assert(__moddi3(x,y)==x%y);
  }
  assert(__divdi3(INT64_MIN,1)==INT64_MIN);
  assert(__divdi3(INT64_MIN,-1)==INT64_MIN);
  assert(__udivdi3(123,0)==0);
  return 0;
}
