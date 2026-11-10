/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "engine.h"
#include "controls.h"
#define GD_SAVE_MAGIC 0x47504235u
/* Sound/kit records deliberately exclude sequence, transport and gate state. */
typedef struct {uint8_t param[32];GDLFO lfo[2];} GDSound;
typedef struct {uint8_t fx[12],effect[30][4],punch[4],slot[4];} GDScene;
typedef struct {GDSound sound[8];GDScene scene;uint8_t mute[8];} GDKit;
typedef struct {
 GDSound voice[8][8];GDKit kit[4];GDScene scene[4];
 uint8_t voice_used[8],kit_used,scene_used;
} GDPresetBank;
typedef struct {uint32_t magic,version,project_size;GDProject project;GDPresetBank bank;} GDSave;
static const char *const gd_preset_types[3]={"VOICE PRESETS","KIT PRESETS","FX SCENES"};
#include "factory_voices.h"
static const char *const gd_factory_kit_names[4]={"ORIGINAL","DUST WORKS","METAL ROOM","ACID TAPE"};
static const char *const gd_factory_scene_names[4]={"CLEAN","DUST DELAY","METAL SPACE","TAPE DAMAGE"};
static inline GDSound gd_get_sound(const GDProject *p,unsigned lane){
 GDSound s;for(unsigned j=0;j<32;++j)s.param[j]=p->track[lane].param[j];
 for(unsigned k=0;k<2;++k)s.lfo[k]=p->track[lane].lfo[k];return s;
}
static inline void gd_put_sound(GDProject *p,unsigned lane,const GDSound *s){
 for(unsigned j=0;j<32;++j)p->track[lane].param[j]=s->param[j];
 for(unsigned k=0;k<2;++k)p->track[lane].lfo[k]=s->lfo[k];
}
static inline GDScene gd_get_scene(const GDProject *p){
 GDScene s;for(unsigned j=0;j<12;++j)s.fx[j]=p->fx[j];
 for(unsigned e=0;e<30;++e)for(unsigned k=0;k<4;++k)s.effect[e][k]=p->effect[e][k];
 for(unsigned k=0;k<4;++k){s.punch[k]=p->punch[k];s.slot[k]=p->slot[k];}return s;
}
static inline void gd_put_scene(GDProject *p,const GDScene *s){
 for(unsigned j=0;j<12;++j)p->fx[j]=s->fx[j];
 for(unsigned e=0;e<30;++e)for(unsigned k=0;k<4;++k)p->effect[e][k]=s->effect[e][k];
 for(unsigned k=0;k<4;++k){p->punch[k]=s->punch[k];p->slot[k]=s->slot[k];}
}
static inline GDKit gd_get_kit(const GDProject *p){
 GDKit kit;for(unsigned l=0;l<8;++l){kit.sound[l]=gd_get_sound(p,l);kit.mute[l]=p->track[l].mute;}
 kit.scene=gd_get_scene(p);return kit;
}
/* Audio-block adoption changes only sound/mixer/FX bytes. No processor reset. */
static inline void gd_apply_preset(GDProject *p,const GDKit *k,unsigned type,unsigned lane){
 if(type==0)gd_put_sound(p,lane,&k->sound[lane]);
 else if(type==1){for(unsigned l=0;l<8;++l){gd_put_sound(p,l,&k->sound[l]);p->track[l].mute=k->mute[l];}gd_put_scene(p,&k->scene);}
 else gd_put_scene(p,&k->scene);
}
/* F1 is always the exact startup INIT; F2-F8 are the first seven app sounds. */
static inline GDSound gd_init_sound(unsigned lane){
 GDProject p;gd_defaults(&p);return gd_get_sound(&p,lane%8);
}
static inline GDSound gd_factory_sound(unsigned lane,unsigned preset){
 lane%=8;preset%=8;GDSound s=gd_init_sound(lane);if(!preset)return s;
 for(unsigned j=0;j<30;++j)s.param[j]=gd_factory_voice_bytes[lane][preset-1][j];
 return s;
}
static inline GDScene gd_factory_scene(unsigned preset){
 GDProject p;gd_defaults(&p);GDScene s=gd_get_scene(&p);
 s.fx[0]=s.fx[4]=s.fx[5]=s.fx[6]=s.fx[8]=s.fx[9]=0;s.fx[10]=127;s.fx[7]=127;s.fx[11]=64;
 if(preset==1){s.slot[0]=12;s.slot[1]=19;s.slot[2]=28;s.slot[3]=14;s.fx[0]=24;s.fx[2]=28;s.fx[4]=10;s.fx[5]=4;s.fx[7]=105;}
 if(preset==2){s.slot[0]=23;s.slot[1]=20;s.slot[2]=27;s.slot[3]=29;s.fx[0]=32;s.fx[1]=1;s.fx[2]=42;s.effect[23][0]=80;s.effect[23][1]=110;}
 if(preset==3){s.slot[0]=5;s.slot[1]=6;s.slot[2]=15;s.slot[3]=14;s.fx[0]=42;s.fx[1]=3;s.fx[2]=48;s.fx[3]=100;s.fx[6]=4;s.fx[11]=84;}
 return s;
}
/* Factory delay routing: dry kick/bass, restrained hats, rhythmic accents/texture. */
static inline void gd_factory_delay_sends(unsigned preset,const GDKit *kit,uint8_t sends[8]){
 static const uint8_t profile[3][8]={{0,48,8,24,36,44,64,0},{0,64,0,32,48,64,80,0},{0,40,0,16,32,48,64,0}};
 for(unsigned lane=0;lane<8;++lane)sends[lane]=profile[(preset-1)%3][lane];
 if(kit&&kit->sound[5].param[0]<40)sends[5]=0;
}
static inline GDKit gd_factory_kit(unsigned preset){
 GDKit k={0};for(unsigned l=0;l<8;++l)k.sound[l]=preset?gd_factory_sound(l,((preset*2+l)%7)+1):gd_init_sound(l);
 k.scene=gd_factory_scene(preset);return k;
}
static inline int gd_sound_valid(const GDSound *s,unsigned lane){
 for(unsigned j=0;j<32;++j){unsigned max=lane==6?gd_gctrl[j].max:lane==7?gd_actrl[j].max:127;
  if(lane==7&&!max)max=127;if(s->param[j]>max)return 0;}
 for(unsigned k=0;k<2;++k){GDLFO m=s->lfo[k];if(m.shape>4||m.rate>11||m.depth>127||m.target>8)return 0;}return 1;
}
static inline int gd_scene_valid(const GDScene *s){
 for(unsigned k=0;k<4;++k){if(s->slot[k]>=30)return 0;for(unsigned n=0;n<k;++n)if(s->slot[k]==s->slot[n])return 0;}
 for(unsigned j=0;j<12;++j)if(s->fx[j]>gd_fxctrl[j].max)return 0;
 for(unsigned e=0;e<30;++e)for(unsigned k=0;k<4;++k)if(s->effect[e][k]>127)return 0;
 for(unsigned k=0;k<4;++k)if(s->punch[k]>(k==1?1:127))return 0;return 1;
}
static inline int gd_bank_valid(const GDPresetBank *b){
 if(b->kit_used>15||b->scene_used>15)return 0;
 for(unsigned l=0;l<8;++l)for(unsigned n=0;n<8;++n)if((b->voice_used[l]>>n&1)&&!gd_sound_valid(&b->voice[l][n],l))return 0;
 for(unsigned n=0;n<4;++n){
  if((b->scene_used>>n&1)&&!gd_scene_valid(&b->scene[n]))return 0;
  if(b->kit_used>>n&1){if(!gd_scene_valid(&b->kit[n].scene))return 0;
   for(unsigned l=0;l<8;++l)if(b->kit[n].mute[l]>1||!gd_sound_valid(&b->kit[n].sound[l],l))return 0;}
 }return 1;
}

/* Version 4 had no editable basic-drum character fields. Initialize those
 * previously unused bytes while retaining the old four knobs and mixer. */
static inline void gd_upgrade_drum_project(GDProject *p){
 for(unsigned lane=0;lane<2;++lane)gd_drum_character_defaults(p->track[lane].param,lane);
 p->version=5;
}
static inline void gd_upgrade_drum_bank(GDPresetBank *b){
 for(unsigned lane=0;lane<2;++lane){
  for(unsigned n=0;n<8;++n)if(b->voice_used[lane]>>n&1)gd_drum_character_defaults(b->voice[lane][n].param,lane);
  for(unsigned n=0;n<4;++n)if(b->kit_used>>n&1)gd_drum_character_defaults(b->kit[n].sound[lane].param,lane);
 }
}

/* Private v5 kick bytes used MIDI pitch and different time/click scales.
 * Replace that voice with INIT while retaining the project's arrangement. */
static inline void gd_upgrade_init_kick(GDProject *p){
 gd_init_kick_parameters(p->track[0].param);
 for(unsigned k=0;k<2;++k)p->track[0].lfo[k].depth=0;
 p->version=6;
}
static inline void gd_upgrade_init_kick_bank(GDPresetBank *b){
 b->voice_used[0]=0;
 for(unsigned n=0;n<4;++n)if(b->kit_used>>n&1){gd_init_kick_parameters(b->kit[n].sound[0].param);for(unsigned k=0;k<2;++k)b->kit[n].sound[0].lfo[k].depth=0;}
}

static inline void gd_init_remaining_voice(uint8_t *q,unsigned lane){
 if(lane==5){gd_init_perc(q);return;}
 if(lane<6){for(unsigned i=0;i<16;++i)q[i]=gd_ac_init[lane-1][i];q[29]=gd_ac_init[lane-1][16];}
 else{for(unsigned i=0;i<29;++i)q[i]=lane==6?gd_gl_init[i]:gd_an_init[i];q[29]=lane==6?114:122;}
 q[30]=q[31]=64;
}
static inline void gd_upgrade_voice_dsp(GDProject *p){
 for(unsigned l=1;l<8;++l){gd_init_remaining_voice(p->track[l].param,l);for(unsigned k=0;k<2;++k)p->track[l].lfo[k].depth=0;}
 p->reserved=8;p->version=7;
}
static inline void gd_upgrade_voice_bank(GDPresetBank *b){
 for(unsigned l=1;l<8;++l){b->voice_used[l]=0;for(unsigned n=0;n<4;++n)if(b->kit_used>>n&1){gd_init_remaining_voice(b->kit[n].sound[l].param,l);for(unsigned k=0;k<2;++k)b->kit[n].sound[l].lfo[k].depth=0;}}
}

/* v8 makes previously fixed performance controls editable; preserve every
 * already editable parameter, source sound, sequence and user preset. */
static inline void gd_upgrade_performance(GDProject *p){gd_extended_effect_defaults(p->effect);p->version=8;}
static inline void gd_upgrade_performance_bank(GDPresetBank *b){
 for(unsigned n=0;n<4;++n){if(b->kit_used>>n&1)gd_extended_effect_defaults(b->kit[n].scene.effect);if(b->scene_used>>n&1)gd_extended_effect_defaults(b->scene[n].effect);}
}
