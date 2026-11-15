#include "../src/profile_state.h"
#include <assert.h>
#include <stdio.h>
int main(void){GDProfiler s={0};gd_profiler_begin(&s,0);gd_profiler_add(&s,0,123);gd_profiler_end(&s,200);assert(s.peak[12]==0);
 gd_profiler_begin(&s,1);gd_profiler_add(&s,0,20);gd_profiler_add(&s,1,30);gd_profiler_end(&s,100);assert(s.peak[0]==20&&s.peak[1]==30&&s.peak[11]==50&&s.peak[12]==100);
 gd_profiler_begin(&s,1);gd_profiler_add(&s,0,40);gd_profiler_add(&s,1,10);gd_profiler_end(&s,90);assert(s.peak[0]==40&&s.peak[1]==30&&s.peak[11]==50&&s.peak[12]==100);
 gd_profiler_begin(&s,0);gd_profiler_end(&s,200);assert(s.peak[12]==100);
 gd_profiler_begin(&s,1);for(unsigned i=0;i<13;++i)assert(!s.peak[i]);gd_profiler_add(&s,100,999);gd_profiler_add(&s,10,120);gd_profiler_end(&s,100);assert(s.peak[11]==0&&s.peak[10]==120);
 puts("PASS: opt-in timing accumulation, independent peaks, residual clamp and re-entry reset");return 0;}
