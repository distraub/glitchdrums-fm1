#include "fm1.h"
#include "../src/storage_fullfx.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static void write_file(const char *name,const void *data,unsigned n){FILE *f=fopen(name,"wb");assert(f);assert(fwrite(data,1,n,f)==n);fclose(f);}
static void convert(const char *from,const char *to){
 static GDMidiSave old;static GDFullSave packed;memset(&old,0,sizeof old);memset(&packed,0,sizeof packed);
 FILE *f=fopen(from,"rb");assert(f);assert(fread(&old.base.base,1,sizeof(GDStoredSave),f)==sizeof(GDStoredSave));fclose(f);gd_chain_defaults(&old.base.chain);
 GDStoredSave *s=&old.base.base;GDFullStoredSave *d=&packed.base.base;
 d->magic=s->magic;d->version=8;d->active=s->active;d->theme=s->theme;d->swing=s->swing;d->quality=s->quality;d->bpm=s->bpm;
 GDKit kit=gd_restore_kit(&s->project);d->project=gd_pack_full_kit(&kit);kit=gd_restore_kit(&s->kit);d->kit=gd_pack_full_kit(&kit);GDScene scene=gd_restore_scene(&s->scene);d->scene=gd_pack_full_scene(&scene);
 for(unsigned l=0;l<8;++l){d->voice_used[l]=s->voice_used[l];for(unsigned n=0;n<2;++n){d->voice[l][n]=gd_pack_sound(&s->voice[l][n]);d->voice_bind[l][n]=s->voice_bind[l][n];}}
 d->kit_used=s->kit_used;d->scene_used=s->scene_used;d->kit_bind=s->kit_bind;d->scene_bind=s->scene_bind;
 memcpy(d->pattern,s->pattern,sizeof d->pattern);for(unsigned lane=0;lane<8;++lane)packed.delay_send[lane]=127;packed.base.chain=old.base.chain;write_file(to,&packed,sizeof packed);
 if(strstr(from,"dense")){d->version=5;GDFullScene saved_scene=d->project.scene;GDScene old_roll=gd_unpack_full_scene(&saved_scene);old_roll.fx[9]=4;d->project.scene=gd_pack_full_scene(&old_roll);write_file("build/test-release/legacy-roll5.bin",&packed,sizeof packed);d->version=6;old_roll.fx[9]=26;d->project.scene=gd_pack_full_scene(&old_roll);write_file("build/test-release/legacy-glitch6.bin",&packed,sizeof packed);d->project.scene=saved_scene;d->version=4;write_file("build/test-release/legacy-fullfx.bin",&packed,12260);d->version=8;s->version=3;old.map[0]=(GDMidiMap){128|74,0x32,4};old.map[1]=(GDMidiMap){128|24,0x86,0};write_file("build/test-release/legacy-four-fx.bin",&old,sizeof old);}
}
int main(void){
 GDSound voice={0};uint32_t seed=0x27fa13;
 for(unsigned run=0;run<64;++run){for(unsigned n=0;n<32;++n)voice.param[n]=(uint8_t)(gd_random(&seed)%128);for(unsigned n=0;n<2;++n)voice.lfo[n]=(GDLFO){gd_random(&seed)%5,gd_random(&seed)%12,gd_random(&seed)%128,gd_random(&seed)%9};GDPackedSound p=gd_pack_sound(&voice);GDSound restored=gd_unpack_sound(&p);assert(!memcmp(&voice,&restored,sizeof voice));}
 GDProject project;gd_defaults(&project);GDScene scene=gd_get_scene(&project);
 for(unsigned run=0;run<64;++run){for(unsigned n=0;n<12;++n)scene.fx[n]=(uint8_t)(gd_random(&seed)%(1+gd_fxctrl[n].max));for(unsigned n=0;n<4;++n)scene.punch[n]=(uint8_t)(gd_random(&seed)%(n==1?2:128));for(unsigned e=0;e<30;++e)for(unsigned n=0;n<4;++n)scene.effect[e][n]=(uint8_t)(gd_random(&seed)%128);
  GDFullScene packed=gd_pack_full_scene(&scene);GDScene restored=gd_unpack_full_scene(&packed);assert(!memcmp(scene.fx,restored.fx,12));assert(!memcmp(scene.punch,restored.punch,4));for(unsigned key=0;key<27;++key){unsigned e=gd_keyboard_effect(key);assert(gd_keyboard_effect_valid(e));assert(!memcmp(scene.effect[e],restored.effect[e],4));}}
 convert("build/test-release/empty16.bin","build/test-release/empty-fullfx.bin");convert("build/test-release/dense16.bin","build/test-release/dense-fullfx.bin");
 FILE *f=fopen("build/test-release/fullfx-layout.json","w");assert(f);fprintf(f,"{\"size\":%zu,\"pattern\":%zu,\"chain\":%zu,\"map\":%zu,\"voiceUsed\":%zu,\"voiceBind\":%zu,\"kitUsed\":%zu,\"sceneUsed\":%zu,\"kitBind\":%zu,\"sceneBind\":%zu}\n",sizeof(GDFullSave),offsetof(GDFullSave,base.base.pattern),offsetof(GDFullSave,base.chain),offsetof(GDFullSave,map),offsetof(GDFullStoredSave,voice_used),offsetof(GDFullStoredSave,voice_bind),offsetof(GDFullStoredSave,kit_used),offsetof(GDFullStoredSave,scene_used),offsetof(GDFullStoredSave,kit_bind),offsetof(GDFullStoredSave,scene_bind));fclose(f);
 printf("PASS: lossless 33-byte sounds and 109-byte full FX scenes; complete save %zu / %u bytes\n",sizeof(GDFullSave),SETTINGS_MAX);
}
