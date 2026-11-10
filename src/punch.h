/* SPDX-License-Identifier: GPL-3.0-only
 * Original fixed-point punch-in processors informed by SLOOP's key layout
 * and GlitchBreaks' gate-first performance model. No SLOOP DSP is copied. */
#ifndef GD_PUNCH_H
#define GD_PUNCH_H
#include "engine.h"
#include "signature.h"
#include "performance.h"
/* The native pool holds LCD, delay/Glitch capture, punch tape and pattern bank. */
static int16_t gd_punch_tape[32768] POOL;
static const char *const gd_punch_names[30]={
 "LOOP 1/4","LOOP 1/8","LOOP 1/16","LOOP 1/32","TRIP STUT","REVERSE","TAPE STOP","HALF SPEED",
 "LP SWEEP","HP SWEEP","PHONE","CRUSH","ALIAS","GATE 1/16","ECHO THROW","WOBBLE",
 "KILL LOW","KILL MID","KILL HIGH","FOLD","RING","FLANGER","AUTO PAN","COMB FREEZE","PUMP","RISE","FALL","ANTIVERB","GLITCHSTORTION","GRAIN CLOUD"};
static const char *const gd_pad_top[30]={"LOOP","LOOP","LOOP","LOOP","TRIP","REV","TAPE","HALF","LP","HP","PHONE","BIT","SAMPLE","GATE","ECHO","TAPE","KILL","KILL","KILL","WAVE","RING","FLANGE","AUTO","COMB","SIDE","RISE","FALL","ANTI","GLITCH","GRAIN"};
static const char *const gd_pad_bottom[30]={"1/4","1/8","1/16","1/32","STUT","ERSE","STOP","SPEED","SWEEP","SWEEP","BAND","CRUSH","ALIAS","1/16","THROW","WOBBLE","LOW","MID","HIGH","FOLD","MOD","JET","PAN","FREEZE","PUMP","UP","DOWN","VERB","STORT","CLOUD"};
typedef struct {
 GDSignature signature;GDAlias alias;GDModDelay mod[2];
 unsigned capture_mode,capture_speed,capture_blend;
 int filter[4][2],filter_coeff[4],phone[2][2],kill[3][2][2];
 uint32_t write, start, phase, length, previous, gate_previous, age[30], ring_phase,pan_phase, hold;
 int16_t comb[2048];
 uint32_t comb_write;
 int32_t comb_low;
 int32_t mix, low250, low2500, sweep[2], held[2],crush_low[2],echo_low[2];
} GDPunch;
static inline int gd_effect_level(int v,unsigned level){return (int)((int64_t)v*level*3/254);}
static const uint8_t gd_time_mult[8]={1,2,3,4,6,8,12,16};
static const int16_t gd_pan_gain[129]={0,2896,4096,5016,5792,6476,7094,7663,8192,8689,9159,9606,10033,10442,10837,11217,11585,11941,12288,12624,12952,13272,13584,13890,14189,14481,14768,15049,15325,15597,15863,16125,16384,16638,16888,17134,17377,17617,17854,18087,18317,18545,18770,18992,19211,19428,19643,19855,20066,20274,20479,20683,20885,21085,21283,21479,21673,21866,22057,22246,22434,22620,22805,22988,23170,23350,23529,23707,23883,24058,24232,24404,24575,24745,24914,25082,25249,25414,25579,25742,25905,26066,26226,26386,26544,26702,26858,27014,27169,27323,27476,27628,27780,27930,28080,28229,28377,28524,28671,28817,28962,29107,29250,29393,29536,29677,29818,29959,30098,30237,30376,30514,30651,30787,30923,31059,31193,31327,31461,31594,31727,31858,31990,32121,32251,32381,32510,32639,32767};
static inline int32_t gd_capture_read(uint32_t start,uint32_t phase,uint32_t length,int reverse){
 unsigned i=phase>>16, frac=(phase>>4)&4095;
 if(i>=length)i%=length;
 unsigned a=reverse?length-1-i:i,b=reverse?(a?a-1:length-1):(a+1)%length;
 int32_t x=gd_punch_tape[(start+a)&32767],y=gd_punch_tape[(start+b)&32767];
 return x+(y-x)*(int32_t)frac/4096;
}
static inline void gd_punch(GDPunch *f,const GDProject *p,uint32_t mask,uint32_t delay_write,int32_t *left,int32_t *right){
 /* Park inactive processors, but keep recording history for the next punch. */
 if(!mask&&!f->mix){f->gate_previous=f->previous=0;gd_punch_tape[f->write++&32767]=(int16_t)((*left+*right)/2);*left=gd_sat(*left);*right=gd_sat(*right);return;}
 uint32_t gate=mask;if(!mask&&f->mix)mask=f->previous;
 uint32_t down=gate&~f->gate_previous;f->gate_previous=gate;for(unsigned i=0;i<30;++i){if(down&(1u<<i))f->age[i]=0;if(mask&(1u<<i))++f->age[i];}
 uint32_t quarter=44100u*60u/p->bpm,step=quarter/4u;
 int32_t dryL=*left,dryR=*right,l=dryL,r=dryR,mono=(l+r)/2;
 uint32_t captures=mask&0x00ffu;
 unsigned mode=0;for(unsigned i=0;i<8;++i)if(captures&(1u<<i))mode=i;

 uint32_t length=gd_capture_size(p,mode);
 if(captures&&(!(f->previous&0x000000ffu)||(down&0x000000ffu))){f->length=length;f->start=f->write-length;f->phase=0;}
 if(captures){
   const uint8_t *q=p->effect[mode];
   if(f->length!=length){f->length=length;f->start=f->write-length;f->phase%=length<<16;}
   uint32_t speed=32768u+q[1]*512u;if(mode==7)speed/=2;
   if(mode==6){unsigned duration=gd_brake_time(p),age=f->age[6];
     int progress=age>=duration?32768:(int)(age*16384u/duration)*2;
     speed=(uint32_t)(65536-2*gd_performance_curve(progress,q[2]));}
   f->capture_mode=mode;f->capture_speed=speed;f->capture_blend=q[3];
   int32_t captured=gd_capture_read(f->start,f->phase,length,mode==5);
   uint32_t edge=length-(f->phase>>16);
   unsigned seam=2u+q[2]*2u;if(seam>length/2)seam=length/2;
   if(mode!=6&&edge<seam){int32_t first=gd_capture_read(f->start,0,length,mode==5);captured+=(first-captured)*(int32_t)(seam-edge)/(int32_t)seam;}
   if(mode==6&&!speed)captured=0;
   f->phase+=speed;uint32_t end=length<<16;if(f->phase>=end)f->phase-=end;
   l=dryL+(captured-dryL)*(int)q[3]/127;r=dryR+(captured-dryR)*(int)q[3]/127;
 }else if(!f->mix){gd_punch_tape[f->write++&32767]=(int16_t)mono;}
 /* Keep a stopped capture intact through its release fade, then resume live
  * history. Delay/filter state in the normal FX bus always keeps advancing. */
 if(!captures&&f->mix&& (f->previous&0x000000ffu)){
   int32_t s=gd_capture_read(f->start,f->phase,f->length?f->length:32,f->capture_mode==5);if(!f->capture_speed)s=0;
   l=dryL+(s-dryL)*(int)f->capture_blend/127;r=dryR+(s-dryR)*(int)f->capture_blend/127;
   f->phase+=f->capture_speed;uint32_t end=(f->length?f->length:32)<<16;if(f->phase>=end)f->phase-=end;
 }
 if(!captures&&!(f->previous&0x000000ffu)&&f->mix)gd_punch_tape[f->write++&32767]=(int16_t)mono;
 unsigned depth=p->punch[3];
 for(unsigned effect=16;effect<=18;++effect)if(mask&(1u<<effect)){
   const uint8_t *q=p->effect[effect];unsigned index=effect-16,low=effect==18?256u+q[1]*q[1]*9u/16u:50u+q[1]*q[1]/20u;
   unsigned high=256u+q[2]*q[2]*9u/16u;if(high<low+16)high=low+16;
   int a=(int)(low*32768u/(low+7020u)),b=effect==17?(int)(high*32768u/(high+7020u)):a,in[2]={l,r};
   for(unsigned k=0;k<2;++k){int *state=f->kill[index][k];state[0]+=(int)((int64_t)(in[k]-state[0])*a/32768);state[1]+=(int)((int64_t)((effect==17?in[k]:state[0])-state[1])*b/32768);
    int band=effect==17?state[1]-state[0]:state[0]+(state[1]-state[0])*(int)q[2]/127;if(effect==18)band=in[k]-band;
    in[k]=gd_effect_level(in[k]-band*(int)q[0]/127,q[3]);
   }l=in[0];r=in[1];
 }
 if(mask&(1u<<10)){
   const uint8_t *q=p->effect[10];unsigned low=50u+q[0]*q[0]/48u,high=512u+q[1]*q[1]/2u;int a=(int)(low*32768u/(low+7020u)),b=(int)(high*32768u/(high+7020u)),in[2]={l,r};
   for(unsigned k=0;k<2;++k){f->phone[k][0]+=(int)((int64_t)(in[k]-f->phone[k][0])*a/32768);f->phone[k][1]+=(int)((int64_t)(in[k]-f->phone[k][1])*b/32768);int wet=gd_sat((f->phone[k][1]-f->phone[k][0])*(64+(int)q[2])/64);in[k]+=(wet-in[k])*(int)q[3]/127;}l=in[0];r=in[1];
 }
 for(unsigned effect=8;effect<=9;++effect)if(mask&(1u<<effect)){
   const uint8_t *q=p->effect[effect];unsigned index=effect-8,duration=quarter*(32u+q[0])/64u,age=f->age[effect];if(age>duration)age=duration;
   if((f->age[effect]&15u)==1u){int from=gd_cutoff_hz[q[1]],to=gd_cutoff_hz[q[2]],hz=from+(int)((int64_t)(to-from)*age/duration);f->filter_coeff[index]=hz*32768/(hz+7020);}
   int in[2]={l,r};for(unsigned k=0;k<2;++k){f->filter[index][k]+=(int)((int64_t)(in[k]-f->filter[index][k])*f->filter_coeff[index]/32768);int wet=effect==8?f->filter[index][k]:in[k]-f->filter[index][k];in[k]+=(wet-in[k])*(int)q[3]/127;}l=in[0];r=in[1];
 }
 if(mask&(1u<<11)){
   const uint8_t *q=p->effect[11];int quantum=1<<(1+q[0]/12),a=4096+(int)q[1]*224,in[2]={l,r};
   for(unsigned k=0;k<2;++k){int wet=in[k]/quantum*quantum;f->crush_low[k]=(int)(((int64_t)f->crush_low[k]*(32768-a)+(int64_t)wet*a)/32768);in[k]+=(f->crush_low[k]-in[k])*(int)q[2]/127;in[k]=gd_effect_level(in[k],q[3]);}l=in[0];r=in[1];
 }
 if(mask&(1u<<12)){if(down&(1u<<12))f->alias=(GDAlias){0};gd_alias(&f->alias,p->effect[12],&l,&r);}
 if(mask&(1u<<13)){
   const uint8_t *q=p->effect[13];unsigned period=step*gd_time_mult[q[0]/16]/2u;if(!period)period=1;
   unsigned phase=f->age[13]%period,open=period*(4u+q[1]*120u/127u)/128u,fade=1u+period*q[2]/(127u*4u);int gain=0;
   if(phase<open){unsigned edge=phase<open-phase?phase:open-phase;gain=edge>=fade?32768:(int)(edge*32768u/fade);}
   l=gd_effect_level((int)((int64_t)l*gain/32768),q[3]);r=gd_effect_level((int)((int64_t)r*gain/32768),q[3]);
 }
 if(mask&(1u<<14)){
   const uint8_t *q=p->effect[14];unsigned distance=step*gd_time_mult[q[1]/16];if(distance>32767)distance=32767;
   int tap=gd_delay[(delay_write-distance)&32767],other=gd_delay[(delay_write-distance/2u)&32767];
   int in[2]={tap,tap+(other-tap)*(int)q[3]/127},a=4096+(int)q[2]*224;
   for(unsigned k=0;k<2;++k)f->echo_low[k]=(int)(((int64_t)f->echo_low[k]*(32768-a)+(int64_t)in[k]*a)/32768);
   l+=f->echo_low[0]*(int)q[0]/127;r+=f->echo_low[1]*(int)q[0]/127;
 }
 if(mask&(1u<<15))gd_mod_delay(&f->mod[0],p->effect[15],1,(down&(1u<<15))!=0,&l,&r);
 if(mask&(1u<<19)){
   depth=p->effect[19][0];
   /* Symmetric triangle fold. Inverse drive trim guarantees |wet| <= |in|,
    * including quiet input; drive changes harmonics rather than loudness. */
   int drive=256+(int)depth*12;
   int in[2]={l,r};
   for(unsigned c=0;c<2;++c){
     int bias=((int)p->effect[19][2]-64)*48;
     int64_t driven=(int64_t)in[c]*drive/256+bias;
     int phase=(int)((driven+12000)%48000);if(phase<0)phase+=48000;
     int folded=phase<24000?phase-12000:36000-phase;
     int zero_phase=(bias+12000)%48000;if(zero_phase<0)zero_phase+=48000;int zero=zero_phase<24000?zero_phase-12000:36000-zero_phase;
     int wet=gd_sat((folded-zero)*256/drive*(64+(int)p->effect[19][1])/128);
     in[c]+=(wet-in[c])*(int)p->effect[19][3]/127;
   }l=in[0];r=in[1];
 }
 if(mask&(1u<<20)){
   const uint8_t *q=p->effect[20];f->ring_phase+=(4u+q[0]*q[0]/8u)*97391u;
   unsigned wave=q[2]/43u,phase16=f->ring_phase>>16;
   int ring=wave==0?gd_sine(f->ring_phase):wave==1?(phase16<32768u?-32767+(int)phase16*2:98303-(int)phase16*2):(f->ring_phase&0x80000000u)?-32767:32767;
   int wetL=(int)((int64_t)l*ring/32768),wetR=(int)((int64_t)r*ring/32768);
   l=gd_effect_level(l+(wetL-l)*(int)q[1]/127,q[3]);r=gd_effect_level(r+(wetR-r)*(int)q[1]/127,q[3]);
 }
 if(mask&(1u<<21))gd_mod_delay(&f->mod[1],p->effect[21],0,(down&(1u<<21))!=0,&l,&r);
 if(mask&(1u<<22)){
   const uint8_t *q=p->effect[22];unsigned period=quarter*16u/gd_time_mult[q[0]/16];if(!period)period=1;
   f->pan_phase+=0xffffffffu/period;unsigned pan=(unsigned)(gd_sine(f->pan_phase+((uint32_t)q[2]<<25))+33024u)>>9;
   int gl=32767+(gd_pan_gain[128-pan]-32767)*(int)q[1]/127,gr=32767+(gd_pan_gain[pan]-32767)*(int)q[1]/127;
   l=gd_effect_level((int)((int64_t)l*gl/32768),q[3]);r=gd_effect_level((int)((int64_t)r*gr/32768),q[3]);
 }
 /* Comb Freeze: damped, bounded pitched feedback, restarted on each press.
  * Integer adaptation of GBCombFreezeEffect's musical model. */
 if(mask&(1u<<23)){
   if(down&(1u<<23)){for(unsigned i=0;i<2048;++i)f->comb[i]=0;f->comb_write=0;f->comb_low=0;}
   unsigned period=44100u/(55u+p->effect[23][0]*6u);
   int delayed=f->comb[(f->comb_write-period)&2047];
   f->comb_low+=(delayed-f->comb_low)/(1+p->effect[23][2]/16);
   int feedback=18000+(int)p->effect[23][1]*112;
   int input=(l+r)/4;
   int loop=input+(int)((int64_t)f->comb_low*feedback>>15);
   /* Soft bounding prevents runaway feedback under stacked punch effects. */
   loop=(int)((int64_t)loop*30000/(30000+(loop<0?-loop:loop)));
   f->comb[f->comb_write++&2047]=(int16_t)loop;
   int wet=gd_sat(input+f->comb_low);l+=(wet-l)*(int)p->effect[23][3]/127;r+=(wet-r)*(int)p->effect[23][3]/127;
 }
 if(mask&(1u<<24)){
   const uint8_t *q=p->effect[24];unsigned period=step*gd_time_mult[q[0]/16]/2u;if(!period)period=1;
   int t=(int)((f->age[24]%period)*16384u/period)*2;t=gd_performance_curve(t,q[2]);int gain=32768-(32768-t)*(int)q[1]/127;
   l=gd_effect_level((int)((int64_t)l*gain/32768),q[3]);r=gd_effect_level((int)((int64_t)r*gain/32768),q[3]);
 }
 for(unsigned effect=25;effect<=26;++effect)if(mask&(1u<<effect)){
   const uint8_t *q=p->effect[effect];unsigned index=effect-23,duration=quarter*(32u+q[0])/32u,age=f->age[effect];if(age>duration)age=duration;
   int ramp=(int)(age*8192u/duration)*4,gain=effect==25?8192+ramp*3/4:32768-ramp;
   if((f->age[effect]&15u)==1u){int from=gd_cutoff_hz[q[1]],to=gd_cutoff_hz[q[2]],hz=from+(int)((int64_t)(to-from)*age/duration);f->filter_coeff[index]=hz*32768/(hz+7020);}
   int in[2]={l,r};for(unsigned k=0;k<2;++k){f->filter[index][k]+=(int)((int64_t)(in[k]-f->filter[index][k])*f->filter_coeff[index]/32768);int wet=(int)((int64_t)(effect==25?in[k]-f->filter[index][k]:f->filter[index][k])*gain/32768);in[k]+=(wet-in[k])*(int)q[3]/127;}l=in[0];r=in[1];
 }
 gd_signature_controls(&f->signature,mask,down,&p->effect[27],&l,&r);
 int target=gate?(int)p->punch[0]*258:0;
 int fade=90+(int)p->punch[2]*70,rate=32767/fade;if(rate<1)rate=1;
 if(f->mix<target)f->mix=gd_clamp(f->mix+rate,0,target);if(f->mix>target)f->mix=gd_clamp(f->mix-rate,target,32767);
 *left=gd_sat(dryL+(int32_t)((int64_t)(gd_sat(l)-dryL)*f->mix>>15));
 *right=gd_sat(dryR+(int32_t)((int64_t)(gd_sat(r)-dryR)*f->mix>>15));
 if(!gate&&!f->mix)f->previous=0;else if(gate)f->previous=gate;
}
typedef struct {uint32_t last,active,on,off,lockout;} GDPunchGate;
static inline void gd_gate_input(GDPunchGate *g,uint32_t raw,int quantize){
 if(raw&0x80000000u){*g=(GDPunchGate){0};return;}
 if(raw&0x40000000u)quantize=0;
 raw&=0x3fffffffu;g->lockout&=raw;raw&=~g->lockout;
 uint32_t down=raw&~g->last,up=g->last&~raw;g->last=raw;
 if(quantize){g->on|=down;g->off=(g->off|up)&~down;}
 else{g->active=raw;g->on=g->off=0;}
}
static inline void gd_gate_boundary(GDPunchGate *g){
 uint32_t activated=g->on;g->active|=activated;g->on=0;
 g->active&=~(g->off&~activated);g->off&=activated;
}
static inline void gd_gate_stop(GDPunchGate *g){
 g->lockout=g->last;g->active=g->on=g->off=0;
}
#endif
