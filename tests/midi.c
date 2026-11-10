#include "../src/midi_learn.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){GDMidiMap map[16]={0},before[16];
 for(unsigned n=0;n<16;++n)assert(gd_midi_bind(map,n,40+n,(GDMidiTarget){M_VOICE,(uint8_t)n})==(int)n);
 assert(gd_midi_count(map)==16);memcpy(before,map,sizeof map);
 assert(gd_midi_bind(map,0,79,(GDMidiTarget){M_MUTE,7})==-1);assert(!memcmp(map,before,sizeof map));
 assert(gd_midi_bind(map,5,45,(GDMidiTarget){M_MUTE,7})==5);assert(gd_midi_count(map)==16);
 assert(gd_midi_bind(map,5,45,(GDMidiTarget){M_VOICE,3})==3);assert(gd_midi_count(map)==15);assert(!(map[5].cc&128));
 assert(gd_midi_bind(map,1,1,(GDMidiTarget){M_GATE,0})==5);assert(gd_midi_count(map)==16);
 const unsigned reserved[]={0,6,32,38,64,96,97,98,99,100,101,120,123,127};
 memcpy(before,map,sizeof map);for(unsigned n=0;n<sizeof reserved/sizeof reserved[0];++n)assert(gd_midi_bind(map,0,reserved[n],(GDMidiTarget){M_GATE,0})==-2);assert(!memcmp(map,before,sizeof map));
 assert(!gd_midi_target_valid((GDMidiTarget){M_VOICE,16}));assert(!gd_midi_target_valid((GDMidiTarget){M_LFO,64}));assert(!gd_midi_target_valid((GDMidiTarget){M_EFFECT,120}));assert(!gd_midi_target_valid((GDMidiTarget){M_GATE,16}));assert(!gd_midi_map_valid(&(GDMidiMap){128|74,255,0}));
 for(unsigned n=0;n<16;++n)assert(gd_midi_map_valid(&map[n]));
 puts("PASS: sixteen compact MIDI maps, channel routing, replacement/dedup, full-table preservation and reserved/invalid targets");
}
