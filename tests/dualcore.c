/* SPDX-License-Identifier: GPL-3.0-only */
#define GD_PARALLEL_TEST 1
#include "controls.h"
#include "engine.h"
#include "fm1_cpu1_protocol.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static pthread_t worker;
static unsigned mode,enabled=1,joined,starts,retired;
static void (*task)(uint32_t);static uint32_t task_arg;
static void *run(void *unused){(void)unused;
 if(mode==2){/* Model a worker that changes voice state, then stalls. */
  gd_render_partition(gd_parallel_job.d,gd_parallel_job.p,&gd_parallel_job.sums,gd_parallel_job.n/2+1,6,14);
 }else if(mode==3)task(task_arg^1u);else task(task_arg);
 return 0;
}
int gd_parallel_begin(void (*fn)(uint32_t),uint32_t arg){
 if(!enabled)return 0;assert(joined==starts);++starts;task=fn;task_arg=arg;
 assert(!pthread_create(&worker,0,run,0));return 1;
}
int gd_parallel_join(void){assert(starts==joined+1);pthread_join(worker,0);++joined;
 if(mode==2){enabled=0;return -1;}return 0;
}
void gd_parallel_reject(void){assert(joined==starts);enabled=0;++retired;}
unsigned gd_parallel_status(void){return enabled;}
static GDProject p,before;static GDEngine actual,reference;
static void notes(void){for(unsigned lane=0;lane<8;++lane)for(unsigned n=0;n<(lane>=6?4u:1u);++n){
 gd_hit(&actual,&p,lane,100,(int)n*3,-1,0);gd_hit(&reference,&p,lane,100,(int)n*3,-1,0);
}}
int main(void){
 uint32_t arg=17,job=42,fn=0x02001234,check=FM1_C1_CHECK(fn,arg,job);
 assert(fm1_c1_job_valid(job,job-1,fn,arg,check));
 assert(!fm1_c1_job_valid(job,job,fn,arg,check));/* duplicate */
 assert(!fm1_c1_job_valid(job,job-2,fn,arg,check));/* skip */
 assert(!fm1_c1_job_valid(job,job-1,fn^4,arg,check));
 assert(!fm1_c1_job_valid(job,job-1,fn,arg^1,check));
 assert(!fm1_c1_job_valid(job,job-1,0,arg,check));
 assert(fm1_c1_job_valid(0,0xffffffffu,fn,arg,FM1_C1_CHECK(fn,arg,0)));
 assert(fm1_c1_poll(0,41,42,48000)==3);assert(fm1_c1_poll(0,41,42,48001)==2);
 assert(fm1_c1_poll(1,42,42,0)==1);assert(fm1_c1_poll(0,42,42,100)==0);
 uint32_t start=0xfffffff0u,now=48000;assert(fm1_c1_poll(0,41,42,now-start)==2);
 for(unsigned quality=0;quality<3;++quality){gd_defaults(&p);p.reserved=quality;
  memset(&actual,0,sizeof actual);memset(&reference,0,sizeof reference);mode=0;enabled=1;
  for(unsigned at=0;at<2048;at+=16){if(at%256==0)notes();GDRenderBlock b;
   assert(gd_render_block(&actual,&p,&b,16)==16);
   for(unsigned i=0;i<16;++i){int32_t l,r;gd_render_sample(&reference,&p,&l,&r);assert(l==b.left[i]&&r==b.right[i]&&reference.delay_input==b.send[i]);}
   assert(!memcmp(&actual,&reference,sizeof actual));
  }
 }
 /* No worker/start failure: exact reference fallback, not a silent synth. */
 gd_defaults(&p);memset(&actual,0,sizeof actual);memset(&reference,0,sizeof reference);enabled=0;notes();
 GDRenderBlock b;gd_render_block(&actual,&p,&b,16);for(unsigned i=0;i<16;++i){int32_t l,r;gd_render_sample(&reference,&p,&l,&r);assert(b.left[i]==l&&b.right[i]==r);}assert(!memcmp(&actual,&reference,sizeof actual));
 for(mode=2;mode<=3;++mode){gd_defaults(&p);before=p;memset(&actual,0,sizeof actual);memset(&reference,0,sizeof reference);enabled=1;notes();
  gd_render_block(&actual,&p,&b,16);assert(!enabled);assert(!memcmp(&p,&before,sizeof p));
  gd_prepare(&reference,&p);GDRenderBlock drums;gd_render_partition(&reference,&p,&drums,16,0,6);gd_render_commit(&reference,&drums,0,16);
  for(unsigned i=0;i<16;++i)assert(b.left[i]==drums.left[i]&&b.right[i]==drums.right[i]&&b.send[i]==drums.send[i]);
  for(unsigned i=0;i<6;++i)assert(!memcmp(&actual.voices[i],&reference.voices[i],sizeof(GDVoice)));
  for(unsigned i=6;i<14;++i)assert(!actual.voices[i].env&&!actual.synth[i-6].active);
  unsigned prior=starts;notes();gd_render_block(&actual,&p,&b,16);assert(starts==prior);/* stays disabled */
 }
 assert(retired==1&&joined==starts);
 puts("PASS: actual dual render hook exact PCM/state, failed startup fallback, corrupt/stale jobs, timer wrap, partial-work retirement and no repeated restart");
}
