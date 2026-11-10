/* SPDX-License-Identifier: GPL-3.0-only
 * Reduced fixed-point adaptations of GB AntiVerb/Glitchstortion/GrainCloud.
 * Source: GlitchDrumsExtension/DSP/Effects/processors/GB{AntiVerb,GrainCloud}Effect.hpp.
 * Two fragments per effect, shared 8192-sample capture. Wet processing at
 * 22.05 kHz for ambience only; Glitchstortion processes the current signal
 * at 44.1 kHz. The dry beat is never replaced by delayed grains.
 * No allocation; fresh capture epochs prevent replay of previous gate audio.
 */
#pragma once
#include "engine.h"
#include "signature_tables.h"
#include "alias.h"
typedef struct {uint32_t start,age,length,rate,position,window_phase,window_step;int reverse,pan;} GDFragment;
typedef struct {
 GDFragment fragment[2];
 uint32_t epoch, countdown, serial, rng, reflection_write, control_tick;
 uint32_t length,rate,span,interval;
 int normalization,smooth_coeff,gain_target,wet_dc_in[2],wet_dc_out[2],wash[2];
 int16_t reflection[2048];
 int32_t low, smooth, dc_input, dc_output, bloom, input_env, wet_env, gain;
} GDSignatureVoice;
typedef struct {
 int16_t capture[8192];
 GDAlias shape_alias;
 uint8_t alias_controls[4];
 uint32_t write,pending_down,shape_tick;
 int shape_drive,shape_norm,shape_amount,shape_tone,shape_quantum,shape_low[2],shape_dc_in[2],shape_dc_out[2],transient_env,capture_hp;
 int capture_low,dry_low[2],target_delta[2],smooth_delta[2];
 GDSignatureVoice voice[3];
} GDSignature;
static inline int gd_signature_bound(int x){
 unsigned mag=x<0?0u-(unsigned)x:(unsigned)x;
 if(mag>=524288u)return (int)((int64_t)x*30000/(30000+(int64_t)mag));
 unsigned i=mag>>9,f=mag&511u;
 int y=gd_signature_curve[i]+(gd_signature_curve[i+1]-gd_signature_curve[i])*(int)f/512;
 return x<0?-y:y;
}
static inline void gd_signature_controls(GDSignature *s,uint32_t mask,uint32_t down,const uint8_t controls[3][4],int32_t *left,int32_t *right){
 /* Epoch-based capture needs no history while all signature gates are off. */
 if(!(mask&0x38000000u))return;
 int originalL=*left,originalR=*right,input=(originalL+originalR)/2;
 int distortion[2]={0,0};
 /* Glitchstortion is immediate and stereo. No reverse capture, automatic
  * gain chasing, hold/retrigger, or delayed subtraction of the dry signal. */
 if(mask&(1u<<28)){
  if(down&(1u<<28)){s->shape_tick=0;s->shape_alias=(GDAlias){0};for(unsigned k=0;k<2;++k)s->shape_low[k]=s->shape_dc_in[k]=s->shape_dc_out[k]=0;}
  if(!(s->shape_tick++&31u)){
   s->shape_drive=1+(int)controls[1][2]/12;
   s->shape_norm=(int)((int64_t)(30000+8192*s->shape_drive)*32768/(30000*s->shape_drive));
   s->shape_amount=13542+(int)controls[1][0]*106;
   s->shape_tone=4096+(int)controls[1][3]*96;
   s->shape_quantum=1<<(controls[1][1]/16);
   s->alias_controls[0]=controls[1][1];s->alias_controls[1]=controls[1][1];
   s->alias_controls[2]=controls[1][3];s->alias_controls[3]=(uint8_t)(controls[1][1]*3/4);
  }
  int source[2]={originalL,originalR},shaped_pair[2];
  for(unsigned k=0;k<2;++k)shaped_pair[k]=(int)((int64_t)gd_signature_bound(source[k]*s->shape_drive)*s->shape_norm/32768);
  gd_alias(&s->shape_alias,s->alias_controls,&shaped_pair[0],&shaped_pair[1]);
  for(unsigned k=0;k<2;++k){
   int shaped=shaped_pair[k];
   int delta=shaped-source[k];
   int dc=delta-s->shape_dc_in[k]+(int)((int64_t)s->shape_dc_out[k]*32676/32768);s->shape_dc_in[k]=delta;s->shape_dc_out[k]=dc;delta=dc;
   s->shape_low[k]=(int)(((int64_t)s->shape_low[k]*(32768-s->shape_tone)+(int64_t)delta*s->shape_tone)/32768);
   distortion[k]=(int)((int64_t)s->shape_low[k]*s->shape_amount/32768);
  }
 }
 if(!(mask&0x28000000u)){
  s->target_delta[0]=s->target_delta[1]=s->smooth_delta[0]=s->smooth_delta[1]=0;
  *left=gd_sat(originalL+distortion[0]);*right=gd_sat(originalR+distortion[1]);return;
 }
 if(down&0x28000000u){s->pending_down|=down&0x28000000u;s->capture_low=0;s->capture_hp=input;
  if((mask&0x28000000u)==(down&0x28000000u))for(unsigned k=0;k<2;++k)s->target_delta[k]=s->smooth_delta[k]=0;
 }
 int magnitude=input<0?-input:input;
 s->transient_env=(int)((int64_t)s->transient_env*32756/32768);
 if(magnitude>s->transient_env)s->transient_env=magnitude;
 /* Keep sub-bass in the dry lane; soften captured attacks before pitching. */
 s->capture_hp+=(input-s->capture_hp)*768/32768;
 s->capture_low+=(input-s->capture_hp-s->capture_low)/2;
 s->capture[s->write&8191]=(int16_t)gd_sat(s->capture_low);++s->write;
 if(s->write&1u)goto reconstruct;
 down=s->pending_down;s->pending_down=0;
 int ambienceL=0,ambienceR=0;
 for(unsigned effect=0;effect<3;++effect){
  if(effect==1)continue;
  unsigned depth=controls[effect][0],density=controls[effect][1],texture=controls[effect][2],tail=controls[effect][3];
  uint32_t bit=1u<<(27+effect);if(!(mask&bit))continue;
  GDSignatureVoice *v=&s->voice[effect];
  if(down&bit){
   *v=(GDSignatureVoice){0};v->epoch=s->write;v->rng=0x731a23u+effect*173;v->gain=v->gain_target=32767;
  }
  /* Read span stays under ring size even for double-speed grains. */
  if(!(v->control_tick++&15u)){
   v->length=effect==1?220u+depth*15u:effect==0?882u+depth*48u:530u+depth*24u;
   v->rate=effect==2?32768u+tail*774u:65536u;
   v->span=(v->length*v->rate+65535u)/65536u+2;
   unsigned overlap=1u+density/64u;
   v->interval=v->length/overlap;
   v->normalization=32768/(int)overlap;
   int c=32768/(2+(int)tail/8);v->smooth_coeff=2*c-c*c/32768;
   if(effect==1)v->gain_target=gd_clamp((v->input_env+32)*32767/(v->wet_env+32),11141,32767);
  }
  uint32_t length=v->length,rate=v->rate,span=v->span;
  uint32_t fresh=s->write-v->epoch;
  if(fresh<span)continue; /* pass fresh input while capture seeds */
  if(!v->countdown){
   GDFragment *g=&v->fragment[v->serial++&1];
   uint32_t available=fresh>8192?8192:fresh;
   uint32_t room=available>span?available-span:0;if(effect==2&&room>882)room=882;
   uint32_t spread=room*texture/127u;
   uint32_t scatter=spread?gd_random(&v->rng)%(spread+1):0;
   *g=(GDFragment){.start=s->write-span-scatter,.length=length,.rate=rate,
                  .window_step=1073741824u/length,.reverse=effect!=2,
                  .pan=(int)(gd_random(&v->rng)%193)-96};
   v->countdown=v->interval;
  }
  v->countdown=v->countdown>2?v->countdown-2:0;
  int32_t sumL=0,sumR=0;
  for(unsigned i=0;i<2;++i){
   GDFragment *g=&v->fragment[i];if(!g->length||g->age>=g->length)continue;
   uint32_t position=g->position;g->position+=g->rate*2;
   uint32_t travel=(uint32_t)(position>>16),fraction=(uint32_t)position&65535;
   uint32_t read=g->reverse?g->start+g->length-1-travel:g->start+travel;
   int sample=s->capture[read&8191];
   int next=s->capture[(g->reverse?read-1:read+1)&8191];
   if(fraction)sample+=(int)((int64_t)(next-sample)*fraction>>16);
   /* Triangular window and overlap normalization suppress splice clicks. */
   uint32_t phase=g->window_phase>>14;g->window_phase+=g->window_step*2;
   int window=(int)(phase<32768?phase:65535-phase);
   int grain=(int)((int64_t)sample*window>>15);
   sumL+=grain*(128-g->pan)/128;sumR+=grain*(128+g->pan)/128;
   g->age+=2;
  }
  /* Fixed overlap normalization removes gain steps when grains enter/leave. */
  int l=(int)((int64_t)sumL*v->normalization>>15),r=(int)((int64_t)sumR*v->normalization>>15);
  if(effect==0){
   unsigned delay=(311u+tail*7u)/2;
   int tap=v->reflection[(v->reflection_write-delay)&2047];
   v->low+=(tap-v->low)*7/16;
   int tank=gd_signature_bound((l+r)/2+(int)((int64_t)v->low*(12000+texture*60+tail*60)/32768));
   v->reflection[v->reflection_write++&2047]=(int16_t)tank;
   l=(l+tap)/2;r=(r+v->low)/2;
  }
  if(effect!=1){
   int wet[2]={l,r};
   for(unsigned k=0;k<2;++k){
    int dc=wet[k]-v->wet_dc_in[k]+(int)((int64_t)v->wet_dc_out[k]*32433/32768);
    v->wet_dc_in[k]=wet[k];v->wet_dc_out[k]=dc;wet[k]=dc;
   }
   l=wet[0];r=wet[1];
  }
  /* A send return, never a dry/wet replacement. Low-pass and transient
   * ducking prevent a delayed grain from competing with the next drum hit. */
  v->bloom=gd_clamp(v->bloom+128,0,32767);
  int wet[2]={l,r},duck=32768-gd_clamp(s->transient_env*2,0,24576);
  int amount=(mask&0x28000000u)==0x28000000u?27085:40627;
  for(unsigned k=0;k<2;++k){
   int pole=effect==0?8192:12288;
   v->wash[k]=(int)(((int64_t)v->wash[k]*(32768-pole)+(int64_t)wet[k]*pole)/32768);
   wet[k]=(int)((int64_t)v->wash[k]*2*duck/32768);
   wet[k]=(int)((int64_t)wet[k]*v->bloom/32768);
   wet[k]=(int)((int64_t)wet[k]*amount/32768);
  }
  ambienceL+=wet[0];ambienceR+=wet[1];
 }
 s->target_delta[0]=ambienceL;s->target_delta[1]=ambienceR;
reconstruct:
 for(unsigned k=0;k<2;++k)s->smooth_delta[k]+=(s->target_delta[k]-s->smooth_delta[k])/2;
 *left=gd_sat(originalL+distortion[0]+s->smooth_delta[0]);*right=gd_sat(originalR+distortion[1]+s->smooth_delta[1]);
}

/* Compatibility macro for direct processor tests. Firmware uses independent controls. */
static inline void gd_signature(GDSignature *s,uint32_t mask,uint32_t down,unsigned depth,int32_t *left,int32_t *right){
 uint8_t controls[3][4];for(unsigned e=0;e<3;++e)for(unsigned k=0;k<4;++k)controls[e][k]=(uint8_t)depth;
 gd_signature_controls(s,mask,down,controls,left,right);
}
