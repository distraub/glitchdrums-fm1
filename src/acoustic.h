/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point snare/hat/clap/Perc paths adapted from app DrumVoice.hpp. */
#pragma once
#include "acoustic_tables.h"
#include "perc_tables.h"
static inline int32_t gda_mul(int32_t a,int32_t b){return (int32_t)((int64_t)a*b>>15);}
static inline int32_t gda_env(int32_t x,int32_t e){return (int32_t)((int64_t)x*e>>30);}
static inline uint32_t gda_coeff(unsigned frames,int32_t rate){
 if(!frames)frames=1;
 uint64_t den=(uint64_t)frames*65536u+(unsigned)rate;
 uint64_t delta=(uint64_t)1073741824u*2u*(unsigned)rate/den;
 return delta>=1073741824u?0:1073741824u-(uint32_t)delta;
}
static inline uint32_t gda_inc(uint32_t root,int32_t ratio){uint64_t v=(uint64_t)root*(unsigned)gd_clamp(ratio,0,8388608)>>15;return (uint32_t)(v>1932735283u?1932735283u:v);}
static inline int32_t gda_osc(GDAcoustic *a,unsigned oscillator,uint32_t inc){a->phase[oscillator]+=inc;return gd_sine(a->phase[oscillator]);}
static inline unsigned gda_lp(unsigned hz){if(hz>19845)hz=19845;unsigned index=hz>>5;return gd_ac_lp[index]+(gd_ac_lp[index+1]-gd_ac_lp[index])*(hz&31)/32;}
static inline void gda_config(GDAcoustic *a,unsigned lane,const uint8_t *q,int pitch){
 unsigned m=lane-1;int changed=!a->valid||a->pitch!=pitch;
 for(unsigned i=0;i<(m==4?20u:16u);++i)if(a->q[i]!=q[i])changed=1;
 if(!changed)return;a->valid=1;a->model=m;a->pitch=pitch;
 if(m==4){
  for(unsigned i=0;i<20;++i){a->q[i]=q[i];a->c[i]=gd_perc_units[i][q[i]];}
  int *c=a->c;uint64_t root=(uint64_t)gd_perc_root[q[0]]*gd_note_inc[gd_clamp(60+pitch,0,120)]/gd_note_inc[60];
  a->root=(uint32_t)(root>1932735283u?1932735283u:root);
  a->attack=(unsigned)c[16];a->hold=(unsigned)c[17];
  int rate=gda_mul(226099,(int)gd_perc_curve[q[18]]);
  a->coef[0]=gda_coeff((unsigned)c[1],rate);
  a->coef[1]=gda_coeff((unsigned)((int64_t)c[1]*c[6]>>15),rate);
  a->coef[3]=gda_coeff((unsigned)((int64_t)2205*c[10]>>15),226099);
  a->coef[4]=gda_coeff(529,226099);a->coef[2]=gda_coeff(1985,226099);
  a->inc[0]=gda_inc(a->root,32768+c[14]);
  a->inc[1]=gda_inc(a->root,gda_mul(56689+c[2],c[4]));
  unsigned hz=(unsigned)((uint64_t)a->root*44100u>>32);hz=(unsigned)gd_clamp((int)(hz*65/100),160,360);a->inc[2]=(uint32_t)((uint64_t)hz*4294967296ull/44100u);
  return;
 }
 for(unsigned i=0;i<16;++i){a->q[i]=q[i];a->c[i]=gd_ac_units[m][i][q[i]];}
 int *c=a->c;a->coef[5]=gd_ac_tone[m][q[2]];uint64_t root=(uint64_t)gd_ac_root[m][q[0]]*gd_note_inc[gd_clamp(60+pitch,0,120)]/gd_note_inc[60];
 a->root=(uint32_t)(root>1932735283u?1932735283u:root);
 a->attack=(unsigned)c[12];a->hold=(unsigned)c[13];unsigned decay=(unsigned)c[1];int rate=(int)gd_ac_curve[m][q[14]];
 a->coef[0]=gda_coeff(decay,gda_mul(226099,rate));
 a->coef[3]=gda_coeff(44100,25*32768);
 a->coef[4]=gda_coeff(1544,226099);
 if(m==0){
  a->coef[1]=gda_coeff((unsigned)((uint64_t)decay*c[6]>>15),gda_mul(226099,rate));
  unsigned wire=(unsigned)gd_clamp((int)(decay*72/100),882,1000000);
  a->coef[2]=gda_coeff((unsigned)((uint64_t)wire*c[8]>>15),gda_mul(5*32768,rate));
  a->inc[1]=gda_inc(a->root,c[5]);
 }else if(m==1||m==2){
  a->inc[0]=gda_inc(a->root,(int)(gd_ac_pitch_ratio[gd_clamp(c[4],-400,400)+400]/2));
  a->inc[1]=(uint32_t)((uint64_t)a->root*gd_ac_pitch_ratio[gd_clamp(c[5],-400,400)+400]*1447/65536000);
  static const int ratios[4]={1173,1319,1777,2071};for(unsigned n=0;n<4;++n){uint64_t inc=(uint64_t)a->root*ratios[n]/1000;a->inc[n+2]=(uint32_t)(inc>1932735283u?1932735283u:inc);}
  int density=c[6];a->texture=gda_mul(gda_mul(density,density),98304-2*density);
  a->metal_gain=23593-gda_mul(c[3],11469);a->noise_gain=11469+c[3];
  a->metal_gain+=gda_mul(gd_ac_hat_mix[m][q[3]][0]-a->metal_gain,a->texture);
  a->noise_gain+=gda_mul(gd_ac_hat_mix[m][q[3]][1]-a->noise_gain,a->texture);
  unsigned duration=m==1?decay*42/100:decay;if(duration<529)duration=529;
  a->coef[1]=gda_coeff((unsigned)((uint64_t)duration*c[7]>>15),gda_mul(235930,rate));
  a->coef[2]=gda_coeff((unsigned)((uint64_t)duration*c[8]>>15),gda_mul(235930,rate));
  a->coef[4]=gda_coeff(441,226099);
 }else if(m==3){
  uint32_t ratio=(uint32_t)((uint64_t)a->root*32768/gd_ac_root[m][64]);
  if(ratio!=32768){unsigned hz=250+(unsigned)((int64_t)c[2]*c[2]*17250>>30);a->coef[5]=gda_lp((unsigned)((uint64_t)hz*ratio>>15));}
  a->coef[6]=gda_lp((unsigned)((uint64_t)600*ratio>>15));a->coef[7]=gda_lp((unsigned)((uint64_t)2200*ratio>>15));
  a->interval=(unsigned)c[4];if(!a->interval)a->interval=1;
  a->early_end=a->interval*(unsigned)(2*c[5]-1)/2;
  a->coef[1]=gda_coeff((unsigned)((uint64_t)44100*c[6]>>15),260*32768);
  a->coef[2]=gda_coeff((unsigned)((uint64_t)decay*c[7]>>15),gda_mul(226099,rate));

 }
}
static inline void gda_hit(GDAcoustic *a,unsigned lane,const uint8_t *q,int pitch){
 *a=(GDAcoustic){0};gda_config(a,lane,q,pitch);a->active=1;a->bursts=1;a->next_burst=a->interval;
 for(unsigned n=0;n<8;++n)a->env[n]=1073741824;
}
static inline int32_t gda_render(GDAcoustic *a,int32_t white){
 int *c=a->c;unsigned m=a->model;int color=c[m==4?15:11],noise=white;
 if(color){a->color+=gda_mul(white-a->color,GD_KICK_COLOR_COEFF);int target=color<0?a->color*27/20:(white-a->color)*19/20;noise+=gda_mul(target-white,color<0?-color:color);}
 a->filter+=gda_mul(noise-a->filter,(int)a->coef[5]);
 int low=a->filter,high=noise-low,signal=0,bite=0;
 if(m==0){
  int freq=32768+gda_env(c[4],a->env[3]);
  int tonal=(gda_osc(a,0,gda_inc(a->root,freq))*7+gda_osc(a,1,a->inc[1])*3)/10;
  int env=c[6]==32768?a->env[0]:a->env[1];tonal=gda_env(tonal,env);
  int wire=gda_env(low,a->env[2]);unsigned attack=(unsigned)c[7];if(attack&&a->age<attack)wire=(int)((int64_t)wire*a->age/attack);
  signal=gda_mul(gda_mul(tonal,32768-gda_mul(c[3],22938)),c[9])+gda_mul(gda_mul(wire,8192+gda_mul(c[3],29491)),c[10]);
  if(a->q[15]!=127){int coef=gd_cutoff_hz[a->q[15]]*32768/(gd_cutoff_hz[a->q[15]]+7019);a->metal_filter+=gda_mul(signal-a->metal_filter,coef);signal=a->metal_filter;}
 }else if(m==1||m==2){
  int aa=gda_osc(a,0,a->inc[0])>=0?32768:-32768,bb=gda_osc(a,1,a->inc[1])>=0?32768:-32768;
  int metal=gda_mul(aa,bb);
  if(a->texture){
   int bank=aa+bb;for(unsigned n=2;n<6;++n)bank+=gda_osc(a,n,a->inc[n])>=0?32768:-32768;
   bank=gd_limit(bank*33/120,32768);a->metal_filter+=gda_mul(bank-a->metal_filter,(int)a->coef[5]);
   int hashed=gd_clamp((bank-a->metal_filter)*19/10,-32768,32768);metal+=gda_mul(hashed-metal,a->texture);
  }
  signal=gda_env(gda_mul(metal,a->metal_gain),a->env[1])+gda_env(gda_mul(high,a->noise_gain),a->env[2]);
  bite=gda_env(gda_mul(high,c[10]*27/20),a->env[4]);signal+=bite;
 }else if(m==3){
  if(a->age>=a->next_burst&&a->bursts<(unsigned)c[5]){a->env[1]=a->env[4]=1073741824;++a->bursts;a->next_burst+=a->interval;}
  int burst=a->age<a->early_end?a->env[1]>>15:(a->env[2]>>15)*3/5;
  signal=gda_mul(gda_mul(high,burst),18022+gda_mul(c[3],21299));
  int smack=c[8];if(smack){
   int hpcoef=(int)a->coef[6],lpcoef=(int)a->coef[7];
   a->smack_hp+=gda_mul(noise-a->smack_hp,hpcoef);a->smack_lp+=gda_mul(noise-a->smack_hp-a->smack_lp,lpcoef);
   int driven=gda_mul(a->smack_lp,52429+gda_mul(smack,58982));
   int dense=(int)((int64_t)driven*32768/(32768+gda_mul(driven<0?-driven:driven,14746)));
   int env=(a->env[4]>>15)*9/10+burst*3/25;
   int hiss=32768-gda_mul(smack,5898+gda_env(25231,a->env[4]));
   signal=gda_mul(signal,hiss)+gda_mul(gda_mul(dense,env),gda_mul(smack,27853+gda_mul(c[3],11469)));
  }
  if(c[9])signal=gd_kick_tanh(gda_mul(signal,32768+c[9]*3));
  if(a->q[15]!=127){int coef=gd_cutoff_hz[a->q[15]]*32768/(gd_cutoff_hz[a->q[15]]+7019);a->metal_filter+=gda_mul(signal-a->metal_filter,coef);signal=a->metal_filter;}
 }else{
  int bend=32768+gda_env(c[9],a->env[3]);
  int carrier=gda_osc(a,0,gda_inc(a->inc[0],bend));
  int modulator=gda_osc(a,1,gda_inc(a->inc[1],bend));
  int depth=c[6]==32768?c[5]:gda_env(c[5],a->env[1]);
  int ring=gda_mul(carrier,32768+gda_mul(modulator-32768,depth));
  int mix=gd_clamp(depth,0,32768),dual=gda_mul(carrier,32768-mix)+gda_mul(modulator,mix);
  int tonal=ring+gda_mul(dual-ring,c[8]);
  if(c[11]){
   tonal=gda_mul(tonal,32768-gda_mul(gda_mul(c[11],28836),32768-(a->env[4]>>15)));
   tonal+=gda_env(gda_mul(gd_kick_tanh(gda_mul(ring,32768+c[11]*8)),gda_mul(c[11],91750)),a->env[4]);
  }
  if(c[7]){int drive=32768+c[7]*4,den=gd_kick_tanh(drive);int shaped=(int)((int64_t)gd_kick_tanh(gda_mul(tonal,drive))*32768/(den?den:1));tonal+=gda_mul(shaped-tonal,c[7]);}
  int balance=c[12],tonalGain=balance>0?32768-balance:32768;
  int noiseAmount=balance>0?c[3]+gda_mul(32768-c[3],balance):gda_mul(c[3],32768+balance);
  signal=gda_env(gda_mul(tonal,tonalGain)+gda_mul(noise,gda_mul(noiseAmount,7209)),a->env[0]);
  if(c[13]){int attack=(int)(a->age>=309?32768:a->age*32768/309);attack=gda_mul(attack,attack);
   signal=gda_mul(signal,32768-gda_mul(gda_mul(c[13],26870),32768-attack));
   signal+=gda_env(gda_env(gda_mul(gda_osc(a,2,a->inc[2]),gda_mul(gda_mul(c[13],attack),19661)),a->env[2]),a->env[0]);}
 }

 unsigned lead=a->attack+a->hold;
 int attack=a->age>=a->attack?32768:(int)((uint64_t)a->age*32768/(a->attack?a->attack:1));
 if(m==4&&c[11]){int strikeAttack=(int)(a->age>=7?32768:a->age*32768/7);attack+=gda_mul(gda_mul(32768-attack,gda_env(c[11],a->env[4])),strikeAttack);}
 signal=gda_mul(signal,attack);if(bite)signal+=gda_mul(bite,32768-attack);
 if((m==1||m==2||m==4)&&c[m==4?19:15])signal=gd_kick_tanh(gda_mul(signal,32768+c[m==4?19:15]*3));
 if(a->choke_frames){signal=(int)((int64_t)signal*a->choke_remaining/a->choke_frames);if(a->choke_remaining)--a->choke_remaining;else a->active=0;}
 if(a->age>=lead){a->env[0]=gda_env(a->env[0],(int32_t)a->coef[0]);for(unsigned i=1;i<=(m==4?1u:2u);++i)a->env[i]=gda_env(a->env[i],(int32_t)a->coef[i]);}
 if(m==4)a->env[2]=gda_env(a->env[2],(int32_t)a->coef[2]);
 a->env[3]=gda_env(a->env[3],(int32_t)a->coef[3]);a->env[4]=gda_env(a->env[4],(int32_t)a->coef[4]);
 if(m==3&&a->age<lead)a->env[1]=gda_env(a->env[1],(int32_t)a->coef[1]);
 ++a->age;if(a->env[0]<85899)a->active=0;
 return signal;
}
