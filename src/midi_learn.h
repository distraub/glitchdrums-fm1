/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define GD_MIDI_MAPS 16
enum {M_VOICE,M_LFO,M_MASTER,M_EFFECT,M_PUNCH,M_GLOBAL,M_MUTE,M_LENGTH,M_GATE,M_SEND,M_INVALID=255};
typedef struct {uint8_t kind,index;} GDMidiTarget;
typedef struct {uint8_t cc,route,index;} GDMidiMap; /* cc bit 7 = occupied; route = kind:channel */
_Static_assert(sizeof(GDMidiMap)==3,"MIDI mapping size changed");
static inline int gd_midi_cc_allowed(unsigned cc){
 return cc<120&&cc!=0&&cc!=32&&cc!=6&&cc!=38&&cc!=64&&(cc<96||cc>101);
}
static inline int gd_midi_target_valid(GDMidiTarget t){
 switch(t.kind){case M_VOICE:return (t.index>>5)>=6||(t.index&31)<((t.index>>5)==5?20u:16u)||(t.index&31)>=29;
 case M_LFO:return t.index<64;case M_MASTER:return t.index<12;case M_EFFECT:return t.index<120&&(t.index/4<16||t.index/4>18);
 case M_PUNCH:return t.index<3;case M_GLOBAL:return t.index<2;case M_SEND:case M_MUTE:case M_LENGTH:return t.index<8;case M_GATE:return t.index<30&&(t.index<16||t.index>18);default:return 0;}
}
static inline int gd_midi_target_is(const GDMidiMap *m,GDMidiTarget t){return (m->cc&128)&&(m->route>>4)==t.kind&&m->index==t.index;}
static inline int gd_midi_map_valid(const GDMidiMap *m){return !(m->cc&128)||(gd_midi_cc_allowed(m->cc&127)&&gd_midi_target_valid((GDMidiTarget){m->route>>4,m->index}));}
/* One channel/CC and one destination each have at most one binding. Full does
 * not evict an unrelated mapping. Table is read/written only by control code. */
static inline int gd_midi_bind(GDMidiMap map[16],unsigned channel,unsigned cc,GDMidiTarget target){
 if(channel>15||!gd_midi_cc_allowed(cc)||!gd_midi_target_valid(target))return -2;
 int match=-1,source=-1,free_slot=-1;
 for(unsigned n=0;n<16;++n){if(!(map[n].cc&128)){if(free_slot<0)free_slot=(int)n;continue;}
  if(gd_midi_target_is(&map[n],target))match=(int)n;
  if((map[n].route&15)==channel&&(map[n].cc&127)==cc)source=(int)n;
 }
 int at=match>=0?match:source>=0?source:free_slot;if(at<0)return -1;
 if(source>=0&&source!=at)map[source]=(GDMidiMap){0};
 map[at]=(GDMidiMap){(uint8_t)(128|cc),(uint8_t)((target.kind<<4)|channel),target.index};return at;
}
static inline unsigned gd_midi_count(const GDMidiMap map[16]){unsigned n=0;for(unsigned i=0;i<16;++i)n+=(map[i].cc&128)!=0;return n;}
