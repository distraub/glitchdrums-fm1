#include "fm1.h"
#include "../src/storage16.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
static GDStoredSave save;
static GDOldEightSave old;
static void write_bytes(const char *name,const void *bytes,size_t count){FILE *f=fopen(name,"wb");assert(f);assert(fwrite(bytes,1,count,f)==count);fclose(f);}
int main(void){
 static GDPattern p,q;static uint8_t bytes[GD_PATTERN_BYTES];
 gd_pattern_blank(&p);
 for(unsigned l=0;l<8;++l)for(unsigned s=0;s<16;++s){GDPatternLane *t=&p.lane[l];t->step[s]=(s*7+l)%128;t->chance[s]=(s+l)%101;t->ratchet[s]=1+(s+l)%4;t->pitch[s]=(int8_t)((int)((s*7+l)%97)-48);t->lock_id[s]=(s+l)%32;t->lock_value[s]=(s*3+l)%128;
  if(l>=6){p.chord_count[l-6][s]=3;for(unsigned n=0;n<3;++n)p.chord_pitch[l-6][s][n]=(int8_t)(n*7);}}
 assert(gd_pattern_valid(&p));
 uint32_t rng=0x19616u;
 for(unsigned trial=0;trial<64;++trial){
  gd_pattern_blank(&q);
  for(unsigned l=0;l<8;++l){q.lane[l].length=(uint8_t)(1+gd_random(&rng)%16);for(unsigned n=0;n<16;++n){GDPatternLane *t=&q.lane[l];t->step[n]=(uint8_t)(gd_random(&rng)%128);t->chance[n]=(uint8_t)(gd_random(&rng)%101);t->ratchet[n]=(uint8_t)(1+gd_random(&rng)%4);t->pitch[n]=(int8_t)((int)(gd_random(&rng)%97)-48);unsigned lock=gd_random(&rng)%33;t->lock_id[n]=(uint8_t)(lock==32?255:lock);t->lock_value[n]=(uint8_t)(gd_random(&rng)%128);
   if(l>=6){q.chord_count[l-6][n]=(uint8_t)(gd_random(&rng)%4);for(unsigned j=0;j<q.chord_count[l-6][n];++j)q.chord_pitch[l-6][n][j]=(int8_t)((int)(gd_random(&rng)%97)-48);}}}
  GDPattern decoded;assert(gd_pattern_encode(&q,bytes,sizeof bytes)==672);assert(gd_pattern_decode(&decoded,bytes,sizeof bytes));assert(!memcmp(&q,&decoded,sizeof q));
 }
 assert(gd_pattern_encode(&p,bytes,sizeof bytes)==672);assert(gd_pattern_decode(&q,bytes,sizeof bytes));assert(!memcmp(&p,&q,sizeof p));
 for(unsigned n=0;n<672;++n){assert(!gd_pattern_encode(&p,bytes,n));assert(!gd_pattern_decode(&q,bytes,n));}
 memset(bytes,255,sizeof bytes);assert(!gd_pattern_decode(&q,bytes,sizeof bytes));
 GDProject project;gd_defaults(&project);GDKit kit=gd_get_kit(&project);save.magic=GD16_SAVE_MAGIC;save.version=1;save.bpm=120;save.quality=8;save.project=gd_store_kit(&kit);
 for(unsigned l=0;l<8;++l)for(unsigned n=0;n<2;++n){save.voice[l][n]=kit.sound[l];save.voice_bind[l][n]=(uint8_t)(8+n);}save.kit=gd_store_kit(&kit);save.scene=gd_store_scene(&kit.scene);save.kit_bind=save.scene_bind=4;
 for(unsigned slot=0;slot<16;++slot){gd_pattern_blank(&q);assert(gd_pattern_encode(&q,save.pattern[slot],672));}
 write_bytes("build/test-release/empty16.bin",&save,sizeof save);
 for(unsigned slot=0;slot<16;++slot){p.lane[0].step[0]=(uint8_t)(100+slot);assert(gd_pattern_encode(&p,save.pattern[slot],672));}
 for(unsigned l=0;l<8;++l)save.voice_used[l]=3;save.kit_used=save.scene_used=1;
 write_bytes("build/test-release/dense16.bin",&save,sizeof save);
 printf("Fixed bank save: %zu / %u bytes, pattern %zu RAM / %u stored\n",sizeof save,SETTINGS_MAX,sizeof(GDPattern),GD_PATTERN_BYTES);
 printf("Offsets: project=%zu voices=%zu kit=%zu scene=%zu used=%zu bind=%zu patterns=%zu\n",offsetof(GDStoredSave,project),offsetof(GDStoredSave,voice),offsetof(GDStoredSave,kit),offsetof(GDStoredSave,scene),offsetof(GDStoredSave,voice_used),offsetof(GDStoredSave,voice_bind),offsetof(GDStoredSave,pattern));
 old.prefix.magic=GD_SAVE_MAGIC;old.prefix.version=4;old.prefix.project_size=sizeof(GDOldProject);memcpy(&old.prefix.project,&project,sizeof(GDOldProject));old.prefix.project.version=8;
 old.prefix.bank.voice_used[0]=128;old.prefix.bank.voice[0][7]=gd_factory_sound(0,7);
 old.prefix.project.pages=2;old.prefix.project.track[0].step[16]=99;old.prefix.project.track[0].length=64;
 static GD64Pattern prior;unsigned at=0;
 for(unsigned slot=0;slot<8;++slot){gd64_pattern_blank(&prior);if(!slot){unsigned n=gd64_chords_encode(&prior,old.bytes+at,3712-at);old.size[slot]=(uint16_t)n;at+=n;}else{prior.lane[0].step[slot]=(uint8_t)(80+slot);unsigned n=gd64_pattern_encode(&prior,old.bytes+at,3712-at);assert(n);old.size[slot]=(uint16_t)n;at+=n;}}
 write_bytes("build/test-release/legacy-eight.bin",&old,sizeof old);
 at=0;old.prefix.project.pages=4;
 for(unsigned slot=0;slot<8;++slot){gd64_pattern_blank(&prior);prior.pages=4;unsigned n=slot?gd64_pattern_encode(&prior,old.bytes+at,3712-at):gd64_chords_encode(&prior,old.bytes+at,3712-at);assert(n);old.size[slot]=(uint16_t)n;at+=n;}
 write_bytes("build/test-release/legacy-overflow.bin",&old,sizeof old);
 puts("PASS: worst-case fixed bank fits, lossless chord/p-lock roundtrip and malformed/truncated input guards");
}
