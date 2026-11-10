#include "engine.h"
#include "fx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 GDProject p;gd_defaults(&p);p.fx[4]=0;p.fx[11]=64;p.fx[9]=1;p.fx[10]=127;
 GDFX f={0};memset(gd_roll,0,sizeof gd_roll);int32_t l,r;
 for(unsigned n=0;n<40000;++n){l=r=(int)(n%400)*20-4000;int input=l;gd_fx_send(&f,&p,&l,&r,0);assert(l==input&&r==input);}
 f.roll_gate=1;l=r=12345;gd_fx_send(&f,&p,&l,&r,0);assert(f.was_roll&&l!=12345&&f.roll_length==22050u*65536);unsigned start=f.roll_start,read=f.roll_read;
 p.fx[9]=127;l=r=12345;gd_fx_send(&f,&p,&l,&r,0);assert(f.roll_start==start&&f.roll_read>read);
 for(unsigned rate=1;rate<128;++rate){p.fx[9]=(uint8_t)rate;for(unsigned n=0;n<512;++n){l=r=1000;gd_fx_send(&f,&p,&l,&r,0);assert(f.roll_start==start&&f.roll_read<f.roll_length);assert(f.roll_length>=(16u<<16)&&f.roll_length<=((GD_GLITCH_FRAMES-1u)<<16));}}
 for(unsigned rate=127;rate>0;--rate){p.fx[9]=(uint8_t)rate;for(unsigned n=0;n<512;++n){l=r=1000;gd_fx_send(&f,&p,&l,&r,0);assert(f.roll_read<f.roll_length);}}
 for(unsigned i=1;i<16;++i)assert(gd_glitch_key_rate(i)>gd_glitch_key_rate(i-1));assert(gd_glitch_key_rate(0)==1&&gd_glitch_key_rate(15)==127);
 assert(gd_roll_division[0][1]==4u*65536&&gd_roll_division[0][127]==512u*65536);assert(gd_old_roll_rate(2,120)==73&&gd_old_roll_rate(4,120)==109);
 for(unsigned bpm=40;bpm<=240;++bpm){uint32_t previous=gd_roll_frames(bpm,1);for(unsigned rate=2;rate<128;++rate){uint32_t next=gd_roll_frames(bpm,rate);assert(next<previous);previous=next;}assert(gd_roll_division[gd_glitch_range(bpm)][127]==512u*65536);}
 assert(gd_roll_frames(120,1)==22050u*65536&&gd_glitch_range(120)==0&&gd_glitch_range(60)==1&&gd_glitch_range(40)==2);
 f.roll_gate=0;l=r=12345;gd_fx_send(&f,&p,&l,&r,0);assert(!f.was_roll&&l==12345&&r==12345);
 puts("PASS: roll bypass while armed, hold capture, live rate without recapture and release bypass");
}
