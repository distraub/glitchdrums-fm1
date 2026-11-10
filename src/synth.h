/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point app-inspired ANALOG and 4x GLITCH render paths. */
#pragma once
#include "synth_tables.h"
#include "glitch_materials.h"
static inline uint32_t gds_pow2(int exponent){
 int negative=exponent<0;unsigned x=(unsigned)(negative?-exponent:exponent);if(x>196608)x=196608;
 unsigned pos=x*192,index=pos>>15,frac=pos&32767;
 uint32_t value=gd_kick_sweep_ratio[index]+(uint32_t)((uint64_t)(gd_kick_sweep_ratio[index+1]-gd_kick_sweep_ratio[index])*frac>>15);
 return negative?(uint32_t)(4294967296ull/value):value;
}
static inline int32_t gds_tanh20(int32_t x){
 unsigned sign=x<0,magnitude=sign?0u-(unsigned)x:(unsigned)x;
 if(magnitude>=8388608u)return sign?-1048544:1048544;
 unsigned index=magnitude>>13,fraction=magnitude&8191;
 int value=gd_kick_tanh_table[index]*32+(gd_kick_tanh_table[index+1]-gd_kick_tanh_table[index])*(int)fraction/256;
 return sign?-value:value;
}
static inline int32_t gds_ladder(GDSynth *a,int32_t input,unsigned hz,unsigned factor){
 if(hz<20)hz=20;if(hz>19845)hz=19845;unsigned i=hz>>4,f=hz&15;
 const uint32_t *table=factor==4?gd_ladder_g4:factor==2?gd_ladder_g2:gd_ladder_g1;
 uint32_t g=table[i]+(table[i+1]-table[i])*f/16,g2=(uint32_t)((uint64_t)g*g>>24),g3=(uint32_t)((uint64_t)g2*g>>24),g4=(uint32_t)((uint64_t)g3*g>>24);
 int32_t *s=a->ladder;int rem=16777216-(int)g;
 int ss[4];for(unsigned n=0;n<4;++n)ss[n]=(int)((int64_t)s[n]*rem>>24);
 int sum=(int)(((int64_t)g3*ss[0]+(int64_t)g2*ss[1]+(int64_t)g*ss[2])>>24)+ss[3];
 int feedback=(a->kind==7?a->c[10]:a->c[13])*21/5;
 int64_t num=(int64_t)input*32-((int64_t)feedback*sum>>15);
 int32_t u=(int32_t)(num*16777216/(16777216+((int64_t)feedback*g4>>15)));
 int32_t y=u;
 for(unsigned n=0;n<4;++n){int target=gds_tanh20(y);int v=(int)((int64_t)(target-s[n])*g>>24);y=v+s[n];s[n]=y+v;}
 return y/32;
}
static inline void gds_config(GDSynth *a,unsigned kind,const uint8_t *q,int pitch){
 int changed=!a->valid||a->pitch!=pitch;for(unsigned n=0;n<32;++n)if(a->q[n]!=q[n])changed=1;
 if(!changed)return;a->valid=1;a->pitch=pitch;a->kind=kind;
 for(unsigned n=0;n<32;++n)a->q[n]=q[n];for(unsigned n=0;n<29;++n)a->c[n]=kind==7?gd_an_units[n][q[n]]:gd_gl_units[n][q[n]];
 if(kind==7){
  for(unsigned n=0;n<2;++n)a->root[n]=gd_note_inc[q[n]];
 }else{
  for(unsigned n=0;n<2;++n)a->root[n]=gd_gl_note_inc[n][q[n?23:3]];
 }
}
static inline void gds_hit(GDSynth *a,unsigned kind,const uint8_t *q,int pitch,unsigned quality){
 *a=(GDSynth){0};gds_config(a,kind,q,pitch);a->factor=quality==1?4:quality==2?1:2;a->active=1;a->release_gain=1073741824;
 if(kind==7){
  a->env[0]=1073741824;a->attack[0]=(unsigned)a->c[20];a->attack[1]=(unsigned)a->c[15];
  a->env[1]=a->attack[0]?0:1073741824;a->env[2]=a->attack[1]?0:1073741824;
  int macro=gd_clamp((int)((int64_t)a->c[23]*32768/17640),1638,163840);
  for(unsigned n=0;n<3;++n){unsigned param=n==0?8:n==1?12:14;int curve=n==0?0:a->c[n==1?21:19];uint32_t rate=gds_pow2(-curve);
   unsigned frames=(unsigned)((uint64_t)gd_an_time[n][q[param]]*(unsigned)macro>>15);a->coef[n]=gda_coeff(frames,(int)((uint64_t)226099*rate>>16));
  }
  a->coef[3]=gda_coeff((unsigned)a->c[26],377255);a->held=q[27]>0;
 }else{
  a->attack[0]=(unsigned)a->c[17]*a->factor;if(!a->attack[0])a->attack[0]=1;a->hold=(unsigned)a->c[18]*a->factor;
  a->coef[0]=gda_coeff((unsigned)a->c[19]*a->factor,301804);a->coef[1]=gda_coeff((unsigned)a->c[16]*a->factor,226354);
  a->env[1]=1073741824;
 }
}
static inline int32_t gds_square(uint32_t phase,uint32_t dt){return (phase>>31?-32768:32768)+gd_blep(phase,dt)-gd_blep(phase+0x80000000u,dt);}
static inline int32_t gds_triangle(uint32_t phase){return -gd_wave(phase,0);}
static inline int32_t gds_attack(unsigned age,unsigned frames,int curve){
 int x=age>=frames?32768:(int)((uint64_t)age*32768/(frames?frames:1));
 int shaped=curve>=0?gda_mul(x,x):32768-gda_mul(32768-x,32768-x);
 return x+gda_mul(shaped-x,curve<0?-curve:curve);
}
static inline int32_t gds_analog(GDSynth *a,uint32_t *rng){
 int *c=a->c;const uint8_t *q=a->q;
 a->env[0]=gda_env(a->env[0],(int)a->coef[0]);
 for(unsigned n=1;n<3;++n){unsigned frame=a->attack[n-1];int curve=c[n==1?21:19];
  if(n==2&&a->releasing){}
  else if(frame&&a->age<frame)a->env[n]=gds_attack(a->age,frame,curve)*32768;
  else if(n==2&&a->held){int target=c[25]*32768;a->env[n]=target+gda_env(a->env[n]-target,(int)a->coef[n]);}
  else a->env[n]=gda_env(a->env[n],(int)a->coef[n]);
 }
 int scale=gd_clamp(32768+gda_env(c[7]*14,a->env[0]),655,491520);
 int tune=(int)((int64_t)c[24]*32768/1200)+(a->pitch*32768/12);uint32_t ratio=gds_pow2(tune);
 int64_t inc1=(int64_t)a->root[0]*scale/32768;inc1=inc1*ratio>>16;a->phase[0]+=(uint32_t)inc1;
 uint32_t dt=(uint32_t)(inc1>2147483647?2147483647:inc1<4295?4295:inc1);
 int vco1=q[2]?gds_square(a->phase[0],dt):gds_triangle(a->phase[0]);
 int64_t inc2=(int64_t)a->root[1]*gd_an_fine[q[16]]>>16;inc2=inc2*scale>>15;inc2=inc2*ratio>>16;inc2=inc2*(32768+gda_mul(c[4],vco1)*3)>>15;
 a->phase[1]+=(uint32_t)inc2;uint64_t mag=(uint64_t)(inc2<0?-inc2:inc2);dt=(uint32_t)(mag>2147483647?2147483647:mag<4295?4295:mag);
 int vco2=q[3]?gds_square(a->phase[1],dt):gds_triangle(a->phase[1]);
 int noise=(int32_t)gd_random(rng)>>16;
 if(c[22]){a->noise_low+=gda_mul(noise-a->noise_low,GD_KICK_COLOR_COEFF);int target=c[22]<0?a->noise_low*27/20:(noise-a->noise_low)*19/20;noise+=gda_mul(target-noise,c[22]<0?-c[22]:c[22]);}
 int noise_amount=gd_clamp(c[6]+gda_env(c[28],a->env[0]),0,32768);
 int mixed=gda_mul(vco1,32768-c[5])+gda_mul(vco2,c[5])+gda_mul(noise,noise_amount);
 int driven=c[18]==32768?mixed:gda_mul(gd_kick_tanh(gda_mul(mixed,c[18])),(int)gd_an_drive_norm[q[18]]);
 int routed=(int)((uint64_t)(unsigned)c[9]*gds_pow2(c[17]*a->pitch/12)>>16);routed=gd_clamp(routed,20,18000);
 int cutoff=(int)((int64_t)routed*(32768+gda_env(c[11]*16,a->env[1]))>>15);if(cutoff<20)cutoff=20;
 int source=c[13]>=0?vco2:noise,depth=c[13]<0?-c[13]:c[13];cutoff=(int)((int64_t)cutoff*(32768+gda_mul(source,depth)*3/2)>>15);
 int sample=gds_ladder(a,driven,(unsigned)(cutoff<20?20:cutoff),1);sample=gda_env(sample,a->env[2])*9/10;
 if(a->age<27)sample=(int)((int64_t)sample*a->age*100/2646); /* shared 0.6ms onset */
 if(a->releasing){a->release_gain=gda_env(a->release_gain,(int)a->coef[3]);if(a->release_gain<10737)a->active=0;}
 else if(!a->held&&a->age>=a->attack[1]&&a->env[2]<214748)a->active=0;
 ++a->age;return sample;
}
static inline int32_t gds_read(unsigned table,uint32_t position){unsigned i=position>>20,frac=(position>>4)&65535;return gd_gl_material[table][i]+(int)((int64_t)(gd_gl_material[table][(i+1)&4095]-gd_gl_material[table][i])*frac>>16);}
static inline int32_t gds_color(int noise,int low,int color){int target=color<0?low*8/5:(noise-low)*4/5;return noise+gda_mul(target-noise,color<0?-color:color);}
static inline int32_t gds_material(unsigned table,uint32_t position,int frozen,int color){
 int n=gds_read(8,position),low=(gds_read(8,position-8589935u)+2*n+gds_read(8,position+8589935u))/4;
 return gda_mul(gds_read(table,position),32768-frozen)+gda_mul(gds_color(n,low,color),frozen);
}
static inline int32_t gds_loop(unsigned table,uint32_t phase,uint32_t position,uint32_t span,int edge,int frozen,int color){
 uint32_t width=(uint32_t)((uint64_t)(unsigned)edge*1030792151u>>15);
 if(width<42950u||(phase>=width&&phase<=0xffffffffu-width))return gds_material(table,position+(uint32_t)((uint64_t)phase*span>>31),frozen,color);
 int64_t u=phase>0x80000000u?(int64_t)phase-4294967296ll:phase;
 int blend=gd_clamp(16384+(int)(u*16384/width),0,32768),smooth=gda_mul(gda_mul(blend,blend),98304-2*blend);
 int64_t offset=u*span>>31;
 int end=gds_material(table,position+(uint32_t)offset+span*2u,frozen,color),start=gds_material(table,position+(uint32_t)offset,frozen,color);
 return gda_mul(end,32768-smooth)+gda_mul(start,smooth);
}
static inline int32_t gds_glitch(GDSynth *a,uint32_t *rng){
 int *c=a->c;const uint8_t *q=a->q;int amount=gda_env(c[15],a->env[1]);
 int size_motion=q[14]==1?amount*5:0,pitch_motion=q[14]==0?amount*4:0;
 uint32_t span[2],inc[2],pos[2];
 for(unsigned o=0;o<2;++o){unsigned size=o?24:4,note=o?23:3,position=o?25:5,tuning=o?22:2,source=o?20:0;
  uint64_t sized=(uint64_t)(unsigned)c[size]*gds_pow2(size_motion)>>16;unsigned ceiling=c[size]>16384?32768:16384;if(sized>ceiling)sized=ceiling;if(sized<256)sized=256;span[o]=(uint32_t)sized<<16;
  int p=c[position]+(q[14]==2?amount/2:0);pos[o]=(uint32_t)((int64_t)p*131072);
  int exponent=pitch_motion+a->pitch*32768/12;uint64_t increment=(uint64_t)a->root[o]*(4/a->factor)*gds_pow2(exponent)>>16;
  if(q[source]==0&&q[tuning])increment=increment*4096/(unsigned)sized;
  inc[o]=(uint32_t)(increment>858993456u/a->factor?858993456u/a->factor:increment<486956u/a->factor?486956u/a->factor:increment);
  (void)note;
 }
 int cutoff=c[12];if(q[14]==3)cutoff=(int)((uint64_t)(unsigned)cutoff*gds_pow2(amount*6)>>16);
 if(cutoff<20)cutoff=20;if(cutoff>17640)cutoff=17640;
 for(unsigned sub=0;sub<a->factor;++sub){int sample=0;
  if(!a->drain){
   int oscillator[2]={0,0};for(unsigned o=0;o<2;++o){unsigned source=o?20:0,material=o?21:1,edge=o?26:6,frozen=o?27:7;
    oscillator[o]=q[source]==2?gd_sine(a->phase[o]):q[source]==1?(int)(a->phase[o]>>16)-32768-gd_blep(a->phase[o],inc[o]):gds_loop(q[material],a->phase[o],pos[o],span[o],c[edge],c[frozen],c[9]);
   }
   sample=gda_mul(oscillator[0],32768-c[28])+gda_mul(oscillator[1],c[28]);
   int n=(int)(gd_random(rng)>>16)-32768; a->noise_low+=gda_mul(n-a->noise_low,a->factor==4?2035:a->factor==2?3943:7412);
   sample=gda_mul(sample,32768-c[8]/2)+gda_mul(gds_color(n,a->noise_low,c[9]),c[8]*13/20);
   int x=gda_mul(sample,32768+c[10]*7);int64_t folded=(int64_t)x+32768;unsigned fraction=(unsigned)(folded&131071);int fold=32768-(int)(fraction>65536?fraction-65536:65536-fraction);
   /* triangleFold: 1 - 4*abs(frac((x+1)/4)-.5), Q15 */
   fold=32768-(int)(fraction>65536?fraction-65536:65536-fraction);
   sample+=gda_mul(fold-sample,c[10]);int driven=gda_mul(sample,32768+c[11]*12);
   int saturated=(int)((int64_t)driven*32768/(32768+(driven<0?-driven:driven)));sample+=gda_mul(saturated-sample,c[11]);
   sample=gds_ladder(a,sample,(unsigned)cutoff,a->factor);
   if(a->age<a->attack[0])a->env[0]=(int32_t)((uint64_t)(a->age+1)*1073741824u/a->attack[0]);
   else if(a->hold)--a->hold;else a->env[0]=gda_env(a->env[0],(int)a->coef[0]);
   sample=gda_env(sample,a->env[0])*4/5;
   a->phase[0]+=inc[0];a->phase[1]+=inc[1];a->env[1]=gda_env(a->env[1],(int)a->coef[1]);++a->age;
  }
  a->history[a->cursor]=sample*32;a->cursor=(a->cursor+1)&255;
 }
 unsigned half=16*a->factor;const int32_t *fir=a->factor==4?gd_gl_fir:a->factor==2?gd_gl_fir2:gd_gl_fir1;
 int64_t sum=(int64_t)fir[half]*a->history[(a->cursor-1-half)&255];
 for(unsigned i=0;i<half;++i)sum+=(int64_t)fir[i]*(a->history[(a->cursor-1-i)&255]+a->history[(a->cursor-1-2*half+i)&255]);
 int output=(int)(sum>>30),dc=output-a->dcin+(int)((int64_t)a->dcout*32712>>15);a->dcin=output;a->dcout=dc;
 if(a->drain){if(++a->drain>2048||(a->drain>129&&dc<2&&dc>-2))a->active=0;}
 else if(a->age>=a->attack[0]&&!a->hold&&a->env[0]<10737)a->drain=1;
 return dc/32;
}
