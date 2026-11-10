/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "storage16.h"
/* One full keyboard: sixteen white + eleven black keys; band kills omitted. */
#define GD_KEYBOARD_FX_MASK 0x3ff8ffffu
static inline unsigned gd_keyboard_effect(unsigned key){
 static const uint8_t effects[27]={0,27,1,28,2,29,3,4,19,5,20,6,21,7,8,22,9,23,10,11,24,12,25,13,14,26,15};return effects[key%27];
}
static inline unsigned gd_effect_key(unsigned effect){for(unsigned n=0;n<27;++n)if(gd_keyboard_effect(n)==effect)return n;return 3;}
static inline int gd_keyboard_effect_valid(unsigned effect){return effect<30&&(effect<16||effect>18);}
typedef struct {uint8_t bytes[33];} GDPackedSound;
typedef struct {uint8_t bytes[109];} GDFullScene;
typedef struct {GDPackedSound sound[8];GDFullScene scene;uint8_t mute[8];} GDFullKit;
typedef struct {
 uint32_t magic;uint16_t version;uint8_t active,theme,swing,quality;uint16_t bpm;
 GDFullKit project;GDPackedSound voice[8][2];GDFullKit kit;GDFullScene scene;
 uint8_t voice_used[8],kit_used,scene_used,voice_bind[8][2],kit_bind,scene_bind;
 uint8_t pattern[16][GD_PATTERN_BYTES];
} GDFullStoredSave;
typedef struct {GDFullStoredSave base;GDChain chain;} GDFullChainSave;
typedef struct {GDFullChainSave base;GDMidiMap map[16];uint8_t delay_send[8];} GDFullSave;
_Static_assert(sizeof(GDFullSave)==12268&&sizeof(GDFullSave)<=SETTINGS_MAX,"Full keyboard exceeds settings budget");
static inline GDPackedSound gd_pack_sound(const GDSound *s){
 GDPackedSound d={{0}};unsigned at=0;for(unsigned n=0;n<32;++n)gd_bits_write(d.bytes,&at,s->param[n],7);
 for(unsigned n=0;n<2;++n){gd_bits_write(d.bytes,&at,s->lfo[n].shape,3);gd_bits_write(d.bytes,&at,s->lfo[n].rate,4);gd_bits_write(d.bytes,&at,s->lfo[n].depth,7);gd_bits_write(d.bytes,&at,s->lfo[n].target,4);}return d;
}
static inline GDSound gd_unpack_sound(const GDPackedSound *s){
 GDSound d={0};unsigned at=0;for(unsigned n=0;n<32;++n)d.param[n]=(uint8_t)gd_bits_read(s->bytes,&at,7);
 for(unsigned n=0;n<2;++n){d.lfo[n].shape=(uint8_t)gd_bits_read(s->bytes,&at,3);d.lfo[n].rate=(uint8_t)gd_bits_read(s->bytes,&at,4);d.lfo[n].depth=(uint8_t)gd_bits_read(s->bytes,&at,7);d.lfo[n].target=(uint8_t)gd_bits_read(s->bytes,&at,4);}return d;
}
static inline GDFullScene gd_pack_full_scene(const GDScene *s){
 GDFullScene d={{0}};unsigned at=0;for(unsigned n=0;n<12;++n)gd_bits_write(d.bytes,&at,s->fx[n],7);for(unsigned n=0;n<4;++n)gd_bits_write(d.bytes,&at,s->punch[n],7);
 for(unsigned key=0;key<27;++key)for(unsigned p=0;p<4;++p)gd_bits_write(d.bytes,&at,s->effect[gd_keyboard_effect(key)][p],7);return d;
}
static inline GDScene gd_unpack_full_scene(const GDFullScene *s){
 GDProject p;gd_defaults(&p);GDScene d=gd_get_scene(&p);unsigned at=0;for(unsigned n=0;n<12;++n)d.fx[n]=(uint8_t)gd_bits_read(s->bytes,&at,7);for(unsigned n=0;n<4;++n)d.punch[n]=(uint8_t)gd_bits_read(s->bytes,&at,7);
 for(unsigned key=0;key<27;++key)for(unsigned p=0;p<4;++p)d.effect[gd_keyboard_effect(key)][p]=(uint8_t)gd_bits_read(s->bytes,&at,7);return d;
}
static inline GDFullKit gd_pack_full_kit(const GDKit *s){GDFullKit d;for(unsigned n=0;n<8;++n){d.sound[n]=gd_pack_sound(&s->sound[n]);d.mute[n]=s->mute[n];}d.scene=gd_pack_full_scene(&s->scene);return d;}
static inline GDKit gd_unpack_full_kit(const GDFullKit *s){GDKit d;for(unsigned n=0;n<8;++n){d.sound[n]=gd_unpack_sound(&s->sound[n]);d.mute[n]=s->mute[n];}d.scene=gd_unpack_full_scene(&s->scene);return d;}
