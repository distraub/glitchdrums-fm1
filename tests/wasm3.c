/* Exercise the built artifact in wasm3 0.5.0, the RemoteFM1 iOS interpreter.
 * Compile with the wasm3 source C files and its source include directory. */
#include "wasm3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static void check(M3Result result) {
  if (result) {
    fprintf(stderr, "wasm3: %s\n", result);
    exit(1);
  }
}
static uint32_t call(IM3Runtime rt, const char *name, unsigned argc,
                     const char **argv) {
  IM3Function fn;
  uint32_t value = 0;
  check(m3_FindFunction(&fn, rt, name));
  check(m3_CallArgv(fn, argc, argv));
  if (m3_GetRetCount(fn))
    check(m3_GetResultsV(fn, &value));
  return value;
}
static void input_button(IM3Runtime rt,uint8_t *memory,uint32_t panel,unsigned key,unsigned *ms){
 for(unsigned edge=0;edge<2;++edge){uint32_t value=edge?0:1u<<key;memcpy(memory+panel+4,&value,4);char now[16];snprintf(now,sizeof now,"%u",++*ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);}
}
static void input_turn(IM3Runtime rt,uint8_t *memory,uint32_t panel,unsigned encoder,int32_t value,unsigned *ms){
 const char *five[]={"5"};uint32_t offset=call(rt,"rfm1_layout",1,five);memcpy(memory+panel+offset+encoder*4,&value,4);char now[16];snprintf(now,sizeof now,"%u",++*ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);
}
int main(int argc, char **argv) {
  assert(argc == 2 || argc == 3);
  FILE *f = fopen(argv[1], "rb");
  assert(f);
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  uint8_t *bytes = malloc((size_t)n);
  assert(bytes);
  assert(fread(bytes, 1, (size_t)n, f) == (size_t)n);
  fclose(f);
  IM3Environment env = m3_NewEnvironment();
  IM3Runtime rt = m3_NewRuntime(env, 256 * 1024, NULL);
  IM3Module module;
  check(m3_ParseModule(env, &module, bytes, (uint32_t)n));
  check(m3_LoadModule(rt, module));
  assert(call(rt, "rfm1_version", 0, NULL) == 1);
  const char *six[] = {"6"};
  uint32_t desc = call(rt, "rfm1_get", 1, six);
  const char *thirty[] = {"30"};
  uint32_t panel_offset = call(rt, "rfm1_layout", 1, thirty);
  uint32_t size;
  uint8_t *memory = m3_GetMemory(rt, &size, 0);
  assert(size == 8 * 1024 * 1024);
  uint32_t panel;
  memcpy(&panel, memory + desc + panel_offset, 4);
  const char *zero[] = {"0"};
  call(rt, "rfm1_init", 1, zero);
  uint32_t keys = 1;
  memcpy(memory + panel, &keys, 4);
  const char *one[] = {"1"};
  call(rt, "rfm1_tick", 1, one);
  const char *alloc[] = {"512"};
  uint32_t out = call(rt, "rfm1_alloc", 1, alloc);
  char buffer[16];
  snprintf(buffer, sizeof buffer, "%u", out);
  const char *render[] = {buffer, "128"};
  call(rt, "rfm1_audio", 2, render);
  int audible = 0;
  for (int i = 0; i < 256; ++i) {
    int16_t s;
    memcpy(&s, memory + out + i * 2, 2);
    audible |= s != 0;
  }
  assert(audible);
  if(argc==3){
    keys=0;memcpy(memory+panel,&keys,4);
    if(strstr(argv[2],"seq")){
      uint32_t buttons=1u<<12;memcpy(memory+panel+4,&buttons,4);
      const char *t2[]={"2"};call(rt,"rfm1_tick",1,t2);buttons=0;memcpy(memory+panel+4,&buttons,4);
    }
    else if(strcmp(argv[2],"--idle")!=0)for(unsigned lane=0;lane<8;++lane)for(unsigned note=0;note<(lane>=6?4u:1u);++note){
      uint32_t midi=(100u<<24)|((60u+note*3u)<<16)|((0x90u+lane)<<8)|9u;char packet[16];snprintf(packet,sizeof packet,"%u",midi);const char *arg[]={packet};call(rt,"rfm1_midi_in",1,arg);
    }
    if(strstr(argv[2],"fx")){
      uint32_t buttons=1u<<2;memcpy(memory+panel+4,&buttons,4);
      const char *t3[]={"3"};call(rt,"rfm1_tick",1,t3);
      buttons=0;memcpy(memory+panel+4,&buttons,4);
      unsigned effect=(unsigned)atoi(strstr(argv[2],"fx")+2);
      static const unsigned fxkeys[27]={0,27,1,28,2,29,3,4,19,5,20,6,21,7,8,22,9,23,10,11,24,12,25,13,14,26,15};
      keys=42u;
      if(effect<30)for(unsigned k=0;k<27;++k)if(fxkeys[k]==effect)keys=1u<<k;
      memcpy(memory+panel,&keys,4);
    }
    if(strcmp(argv[2],"--seqroll")==0){
      const char *t[]={"3"};for(unsigned press=0;press<4;++press){uint32_t buttons=1u<<2;memcpy(memory+panel+4,&buttons,4);call(rt,"rfm1_tick",1,t);buttons=0;memcpy(memory+panel+4,&buttons,4);call(rt,"rfm1_tick",1,t);}
      keys=1;memcpy(memory+panel,&keys,4);call(rt,"rfm1_tick",1,t);
      const char *five[]={"5"};uint32_t enc=call(rt,"rfm1_layout",1,five);int32_t delta=126;memcpy(memory+panel+enc+3*4,&delta,4);call(rt,"rfm1_tick",1,t);
    }
    if(strstr(argv[2],"remix")){
      const char *t[]={"3"};for(unsigned press=0;press<2;++press){uint32_t buttons=1u<<11;memcpy(memory+panel+4,&buttons,4);call(rt,"rfm1_tick",1,t);buttons=0;memcpy(memory+panel+4,&buttons,4);call(rt,"rfm1_tick",1,t);}
      uint32_t buttons=1u<<3;memcpy(memory+panel+4,&buttons,4);call(rt,"rfm1_tick",1,t);buttons=0;memcpy(memory+panel+4,&buttons,4);call(rt,"rfm1_tick",1,t);
      const char *five[]={"5"};uint32_t enc=call(rt,"rfm1_layout",1,five);int32_t delta=strstr(argv[2],"sweep")?0:127;memcpy(memory+panel+enc+2*4,&delta,4);delta=127;memcpy(memory+panel+enc+5*4,&delta,4);call(rt,"rfm1_tick",1,t);
      if(strstr(argv[2],"walk")||strstr(argv[2],"repeat")){delta=strstr(argv[2],"walk")?8:3;memcpy(memory+panel+enc+4,&delta,4);call(rt,"rfm1_tick",1,t);if(strstr(argv[2],"repeat")){delta=-125;memcpy(memory+panel+enc+2*4,&delta,4);call(rt,"rfm1_tick",1,t);}}
      if(strstr(argv[2],"glitch")){keys=1;memcpy(memory+panel,&keys,4);call(rt,"rfm1_tick",1,t);delta=127;memcpy(memory+panel+enc+3*4,&delta,4);call(rt,"rfm1_tick",1,t);}
    }
    const char *tick[]={"4"};call(rt,"rfm1_tick",1,tick);
    if(strcmp(argv[2],"--idle")==0)for(unsigned block=0;block<2000;++block)call(rt,"rfm1_audio",2,render);
    unsigned start_ms=4,blocks=345;
    if(strcmp(argv[2],"--seqswitch")==0){
      /* Start A, then queue B; the default A wraps at two seconds. */
      call(rt,"rfm1_audio",2,render);
      for(unsigned tap=0;tap<3;++tap){
        for(unsigned down=0;down<2;++down){uint32_t buttons=down?0:1u<<11;memcpy(memory+panel+4,&buttons,4);
          char now[16];snprintf(now,sizeof now,"%u",++start_ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);}
      }
      keys=1u<<2;memcpy(memory+panel,&keys,4);
      char now[16];snprintf(now,sizeof now,"%u",++start_ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);
      blocks=1035;
    }
    if(strcmp(argv[2],"--seqchain")==0){
      input_button(rt,memory,panel,12,&start_ms);call(rt,"rfm1_audio",2,render); /* stop before editing */
      input_button(rt,memory,panel,11,&start_ms);input_button(rt,memory,panel,11,&start_ms);input_button(rt,memory,panel,11,&start_ms);
      input_turn(rt,memory,panel,0,1,&start_ms); /* bank B */
      uint32_t buttons=(1u<<4)|(1u<<3);memcpy(memory+panel+4,&buttons,4);char now[16];snprintf(now,sizeof now,"%u",++start_ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);buttons=0;memcpy(memory+panel+4,&buttons,4);snprintf(now,sizeof now,"%u",++start_ms);call(rt,"rfm1_tick",1,arg);
      input_button(rt,memory,panel,11,&start_ms); /* chain */
      input_turn(rt,memory,panel,0,1,&start_ms);input_button(rt,memory,panel,3,&start_ms); /* length two */
      input_turn(rt,memory,panel,5,1,&start_ms);input_button(rt,memory,panel,12,&start_ms);blocks=2070;
    }
    if(strcmp(argv[2],"--seqmidi")==0){
      input_button(rt,memory,panel,7,&start_ms);input_button(rt,memory,panel,7,&start_ms);input_button(rt,memory,panel,7,&start_ms);input_button(rt,memory,panel,3,&start_ms);
      input_turn(rt,memory,panel,2,1,&start_ms);uint32_t packet=(64u<<24)|(74u<<16)|(0xB0u<<8)|11u;char message[24];snprintf(message,sizeof message,"%u",packet);const char *arg[]={message};call(rt,"rfm1_midi_in",1,arg);
      char now[16];snprintf(now,sizeof now,"%u",++start_ms);const char *time[]={now};call(rt,"rfm1_tick",1,time);input_button(rt,memory,panel,1,&start_ms);
    }
    if(strcmp(argv[2],"--seqbank")==0){input_button(rt,memory,panel,11,&start_ms);input_button(rt,memory,panel,11,&start_ms);input_button(rt,memory,panel,11,&start_ms);blocks=2070;}
    unsigned ms=start_ms,frame_at=start_ms;clock_t start=clock(),max_audio=0,max_save=0;
    for(unsigned block=0;block<blocks;++block){
      if(strstr(argv[2],"sweep")&&block%4==0){const char *five[]={"5"};uint32_t enc=call(rt,"rfm1_layout",1,five);int32_t delta=1;memcpy(memory+panel+enc+2*4,&delta,4);char now[16];snprintf(now,sizeof now,"%u",ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);}

      if(strcmp(argv[2],"--seqmidi")==0&&block%3==0){uint32_t packet=((block%128u)<<24)|(74u<<16)|(0xB0u<<8)|11u;char msg[24];snprintf(msg,sizeof msg,"%u",packet);const char *arg[]={msg};call(rt,"rfm1_midi_in",1,arg);}

      if(strcmp(argv[2],"--seqsave")==0&&block==100){
        uint32_t buttons=1u<<9;memcpy(memory+panel+4,&buttons,4);char now[16];snprintf(now,sizeof now,"%u",++ms);const char *arg[]={now};
        clock_t saved_at=clock();call(rt,"rfm1_tick",1,arg);max_save=clock()-saved_at;buttons=0;memcpy(memory+panel+4,&buttons,4);
      }
      if(strcmp(argv[2],"--idle")==0||strstr(argv[2],"live")||strstr(argv[2],"seq")){
        unsigned until=start_ms+block*128u*1000u/44100u;
        while(ms<until){char now[16];snprintf(now,sizeof now,"%u",++ms);const char *arg[]={now};call(rt,"rfm1_tick",1,arg);}
        if(ms-frame_at>=20){call(rt,"rfm1_frame",0,NULL);frame_at=ms;}
      }
      clock_t audio_start=clock();call(rt,"rfm1_audio",2,render);
      clock_t elapsed=clock()-audio_start;if(elapsed>max_audio)max_audio=elapsed;
    }
    double seconds=(double)(clock()-start)/CLOCKS_PER_SEC,duration=blocks*128.0/44100.0;
    printf("wasm3 %s: %.3f audio seconds in %.3f CPU seconds (%.2fx realtime), max audio block %.3f ms on this host\n",argv[2],duration,seconds,duration/seconds,1000.0*max_audio/CLOCKS_PER_SEC);
    if(max_save)printf("Explicit SAVE control tick: %.3f ms on this host\n",1000.0*max_save/CLOCKS_PER_SEC);
  }
  m3_FreeRuntime(rt);
  m3_FreeEnvironment(env);
  free(bytes);
  puts("PASS: wasm3 0.5.0 load, ABI, screen boot, key trigger, audio");
}
