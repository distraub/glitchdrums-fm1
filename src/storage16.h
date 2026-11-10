/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "presets.h"
#include "patterns.h"
#include "chain.h"
#include "midi_learn.h"
#include "legacy_patterns.h"
#define GD16_SAVE_MAGIC 0x47313650u
#define GD_USER_VOICES 2
/* The original project prefix is retained solely for importing older files. */
typedef struct {uint32_t magic,version;GDTrack track[8];uint8_t fx[12],theme,swing,pages,reserved;uint32_t bpm;uint8_t punch[4],effect[30][4];} GDOldProject;
typedef struct {uint8_t fx[12],effect[30][4],punch[4];} GDOldScene;
typedef struct {GDSound sound[8];GDOldScene scene;uint8_t mute[8];} GDOldKit;
typedef struct {GDSound voice[8][8];GDOldKit kit[4];GDOldScene scene[4];uint8_t voice_used[8],kit_used,scene_used;} GDOldBank;
typedef struct {uint32_t magic,version,project_size;GDOldProject project;GDOldBank bank;} GDOldSave;
typedef struct {GDOldSave prefix;GDLegacyABPattern alternate;uint8_t active;} GDOldABSave;
typedef struct {GDOldSave prefix;uint16_t size[8];uint8_t active,bytes[3712];} GDOldEightSave;
typedef struct {GDOldSave prefix;uint8_t arrangement[3576];} GDOldSongSave;
_Static_assert(sizeof(GDOldSave)==8544&&sizeof(GDOldABSave)==11628&&sizeof(GDOldEightSave)==12276&&sizeof(GDOldSongSave)==12120,"Old layouts changed");
typedef struct {uint8_t fx[12],punch[4],slot[4],param[4][4];} GDStoredScene;
typedef struct {GDSound sound[8];GDStoredScene scene;uint8_t mute[8];} GDStoredKit;
typedef struct {
 uint32_t magic;uint16_t version;uint8_t active,theme,swing,quality;uint16_t bpm;
 GDStoredKit project;GDSound voice[8][2];GDStoredKit kit;GDStoredScene scene;
 uint8_t voice_used[8],kit_used,scene_used,voice_bind[8][2],kit_bind,scene_bind;
 uint8_t pattern[16][GD_PATTERN_BYTES];
} GDStoredSave;
_Static_assert(sizeof(GDStoredSave)==12196,"Sixteen-pattern budget changed");
_Static_assert(sizeof(GDStoredSave)<=SETTINGS_MAX,"Sixteen patterns exceed NOR");
typedef struct {GDStoredSave base;GDChain chain;} GDChainSave;
_Static_assert(sizeof(GDChainSave)==12216&&sizeof(GDChainSave)<=SETTINGS_MAX,"Chain exceeds NOR budget");
typedef struct {GDChainSave base;GDMidiMap map[16];} GDMidiSave;
_Static_assert(sizeof(GDMidiSave)==12264&&sizeof(GDMidiSave)<=SETTINGS_MAX,"MIDI map exceeds NOR budget");
static inline GDStoredScene gd_store_scene(const GDScene *s){
 GDStoredScene d={0};for(unsigned i=0;i<12;++i)d.fx[i]=s->fx[i];
 for(unsigned k=0;k<4;++k){d.punch[k]=s->punch[k];d.slot[k]=s->slot[k];for(unsigned j=0;j<4;++j)d.param[k][j]=s->effect[s->slot[k]][j];}return d;
}
static inline GDScene gd_restore_scene(const GDStoredScene *s){
 GDProject p;gd_defaults(&p);GDScene d=gd_get_scene(&p);
 for(unsigned i=0;i<12;++i)d.fx[i]=s->fx[i];
 for(unsigned k=0;k<4;++k){d.punch[k]=s->punch[k];d.slot[k]=s->slot[k];if(s->slot[k]<30)for(unsigned j=0;j<4;++j)d.effect[s->slot[k]][j]=s->param[k][j];}return d;
}
static inline GDStoredKit gd_store_kit(const GDKit *s){GDStoredKit d;for(unsigned l=0;l<8;++l){d.sound[l]=s->sound[l];d.mute[l]=s->mute[l];}d.scene=gd_store_scene(&s->scene);return d;}
static inline GDKit gd_restore_kit(const GDStoredKit *s){GDKit d;for(unsigned l=0;l<8;++l){d.sound[l]=s->sound[l];d.mute[l]=s->mute[l];}d.scene=gd_restore_scene(&s->scene);return d;}
static inline void gd_import_scene(GDScene *d,const GDOldScene *s){
 GDProject p;gd_defaults(&p);*d=gd_get_scene(&p);for(unsigned i=0;i<12;++i)d->fx[i]=s->fx[i];
 for(unsigned e=0;e<30;++e)for(unsigned k=0;k<4;++k)d->effect[e][k]=s->effect[e][k];for(unsigned k=0;k<4;++k)d->punch[k]=s->punch[k];
}
static inline void gd_import_prefix(GDSave *d,const GDOldSave *s){
 gd_defaults(&d->project);const uint8_t *src=(const uint8_t*)&s->project;uint8_t *dst=(uint8_t*)&d->project;for(unsigned n=0;n<sizeof(GDOldProject);++n)dst[n]=src[n];
 d->magic=GD_SAVE_MAGIC;d->version=1;d->project_size=sizeof(GDProject);d->bank=(GDPresetBank){0};
 for(unsigned l=0;l<8;++l){unsigned out=0;for(unsigned n=0;n<8&&out<2;++n)if(s->bank.voice_used[l]&(1u<<n)){d->bank.voice[l][out]=s->bank.voice[l][n];d->bank.voice_used[l]|=(uint8_t)(1u<<out);++out;}}
 for(unsigned n=0;n<4;++n)if(s->bank.kit_used&(1u<<n)){
  d->bank.kit_used=1;for(unsigned l=0;l<8;++l){d->bank.kit[0].sound[l]=s->bank.kit[n].sound[l];d->bank.kit[0].mute[l]=s->bank.kit[n].mute[l];}gd_import_scene(&d->bank.kit[0].scene,&s->bank.kit[n].scene);break;
 }
 for(unsigned n=0;n<4;++n)if(s->bank.scene_used&(1u<<n)){d->bank.scene_used=1;gd_import_scene(&d->bank.scene[0],&s->bank.scene[n]);break;}
}
static inline void gd_import_bar(GDPattern *d,const GD64Pattern *s,unsigned bar){
 gd_pattern_blank(d);
 for(unsigned l=0;l<8;++l){d->lane[l].length=(uint8_t)gd_clamp(s->lane[l].length,1,16);for(unsigned n=0;n<16;++n){unsigned at=bar*16+n;
  d->lane[l].step[n]=s->lane[l].step[at];d->lane[l].chance[n]=s->lane[l].chance[at];d->lane[l].ratchet[n]=s->lane[l].ratchet[at];d->lane[l].pitch[n]=s->lane[l].pitch[at];d->lane[l].lock_id[n]=s->lane[l].lock_id[at];d->lane[l].lock_value[n]=s->lane[l].lock_value[at];}}
 for(unsigned l=0;l<2;++l)for(unsigned n=0;n<16;++n){d->chord_count[l][n]=s->chord_count[l][bar*16+n];for(unsigned k=0;k<3;++k)d->chord_pitch[l][n][k]=s->chord_pitch[l][bar*16+n][k];}
}
static inline int gd_old_bar_used(const GD64Pattern *p,unsigned bar){
 if(bar<p->pages)return 1;
 for(unsigned l=0;l<8;++l)for(unsigned n=bar*16;n<(bar+1)*16;++n)if(p->lane[l].step[n]||p->lane[l].chance[n]!=100||p->lane[l].ratchet[n]!=1||p->lane[l].pitch[n]||p->lane[l].lock_id[n]!=255||p->lane[l].lock_value[n]||(l>=6&&p->chord_count[l-6][n]))return 1;return 0;
}
