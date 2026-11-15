/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-only concurrency harness. pthread barriers model publish/join; firmware
 * must supply its own bounded hardware mailbox and cache barriers. */
#define _XOPEN_SOURCE 700
#include "controls.h"
#include "engine.h"
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static GDProject p;static GDEngine ref,split;
static GDRenderBlock a,b;static unsigned frames;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER,done=PTHREAD_COND_INITIALIZER;
static int pending,quit;
static void *worker(void *unused){(void)unused;pthread_mutex_lock(&mutex);
 for(;;){while(!pending&&!quit)pthread_cond_wait(&wake,&mutex);if(quit)break;
  pthread_mutex_unlock(&mutex);gd_render_partition(&split,&p,&b,frames,6,14);
  pthread_mutex_lock(&mutex);pending=0;pthread_cond_signal(&done);
 }pthread_mutex_unlock(&mutex);return 0;
}
int main(void){pthread_t thread;assert(!pthread_create(&thread,0,worker,0));uint32_t rng=0x718927;
 for(unsigned run=0;run<12;++run){gd_defaults(&p);memset(&ref,0,sizeof ref);memset(&split,0,sizeof split);p.reserved=run%3;
  for(unsigned at=0;at<16000;){if(at%97==0){
   p.bpm=40+gd_random(&rng)%201;split.solo_mask=ref.solo_mask=at%3?0:1u<<(run%8);
   for(unsigned lane=0;lane<8;++lane){p.track[lane].mute=gd_random(&rng)%5==0;
    p.track[lane].lfo[0]=(GDLFO){run%5,(at/97)%12,64,run%9};
    split.delay_send[lane]=ref.delay_send[lane]=gd_random(&rng)%128;
    for(unsigned chord=0;chord<(lane>=6?4u:1u);++chord){int pitch=(int)(gd_random(&rng)%25)-12;unsigned velocity=40+gd_random(&rng)%88;
     gd_hit(&ref,&p,lane,velocity,pitch,-1,0);gd_hit(&split,&p,lane,velocity,pitch,-1,0);
    }
   }
  }
  frames=1+gd_random(&rng)%16;unsigned boundary=16-(split.counter&15u);
  if(frames>boundary)frames=boundary;if(frames>97-at%97)frames=97-at%97;if(frames>16000-at)frames=16000-at;
  if(!(split.counter&31u))gd_prepare(&split,&p);
  pthread_mutex_lock(&mutex);pending=1;pthread_cond_signal(&wake);pthread_mutex_unlock(&mutex);
  gd_render_partition(&split,&p,&a,frames,0,6);
  pthread_mutex_lock(&mutex);while(pending)pthread_cond_wait(&done,&mutex);pthread_mutex_unlock(&mutex);
  gd_render_commit(&split,&a,&b,frames);
  GDRenderBlock expected;unsigned n=gd_render_block(&ref,&p,&expected,frames);assert(n==frames);
  for(unsigned i=0;i<n;++i){assert(a.left[i]==expected.left[i]);assert(a.right[i]==expected.right[i]);assert(a.send[i]==expected.send[i]);}
  assert(!memcmp(&ref,&split,sizeof ref));at+=frames;
 }}
 pthread_mutex_lock(&mutex);quit=1;pthread_cond_signal(&wake);pthread_mutex_unlock(&mutex);pthread_join(thread,0);
 puts("PASS: simultaneous drum/synth partitions preserve exact PCM and complete engine state across qualities, chords, mutes, solo, LFO/tempo and sends");return 0;
}
