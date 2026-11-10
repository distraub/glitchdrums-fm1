/* SPDX-License-Identifier: GPL-3.0-only
 * Integer interpretations of GlitchDrums' sound vocabulary. FM-1 has no FPU.
 * This reduced model is not bit-identical to the app's oversampled DSP. */
#ifndef GD_ENGINE_H
#define GD_ENGINE_H
#include "tables.h"
#include "swing.h"
#include "drum_character.h"
#include "kick_dsp.h"
#include "acoustic_state.h"
#include "synth_state.h"
#include <stdint.h>
#define GD_LANES 8
#define GD_PARAMS 32
#define GD_STEPS 64
enum { GD_KICK, GD_SNARE, GD_CH, GD_OH, GD_CLAP, GD_PERC, GD_GLITCH, GD_ANALOG };
typedef struct {
  uint8_t shape, rate, depth, target;
} GDLFO;
typedef struct {
  uint8_t param[32];
  GDLFO lfo[2];
  uint8_t step[64], chance[64], ratchet[64];
  int8_t pitch[64];
  uint8_t lock_id[64], lock_value[64], length, mute;
} GDTrack;
typedef struct {
  uint32_t magic, version;
  GDTrack track[8];
  uint8_t fx[12], theme, swing, pages, reserved;
  uint32_t bpm;
  uint8_t punch[4]; /* mix, quantize, fade, depth; transient gates are not saved */
  uint8_t effect[30][4];
  uint8_t slot[4];
} GDProject;
typedef struct {
  uint32_t trigger_serial;
  uint32_t phase[2], rng, age, hold, attack, decay, motion_decay;
  uint32_t lfo_phase[2], lfo_rng[2], inc[2], loop_size[2], filter_decay;
  int32_t filter_env;
  int32_t env, motion, velocity, pitch_offset, noise_low, filter[2], dc_in,
      dc_out;
  int32_t lfo_value[2], cutoff, reso, pan, gain, lock_id, lock_value;
  uint8_t p[32];
  int8_t mod[32];
  int16_t last_sample;
  int32_t body_env,wire_env,click_env,punch_env,wire_low;
  uint32_t body_decay,wire_decay,click_decay,punch_decay,pitch_hold_remaining;
} GDVoice;
typedef struct {
  uint8_t delay_send[8];
  int32_t delay_input;
  GDVoice voices[14];
  GDAcoustic acoustic[6];
  GDSynth synth[8];
  uint32_t counter, trigger_serial, solo_mask;
} GDEngine;
static inline int gd_clamp(int x, int lo, int hi) {
  return x < lo ? lo : x > hi ? hi : x;
}
/* Pattern span follows the longest lane; each lane can retain its own length. */
static inline unsigned gd_pattern_length(const GDProject *p){unsigned length=1;for(unsigned lane=0;lane<8;++lane)if(p->track[lane].length>length)length=p->track[lane].length;return (unsigned)gd_clamp((int)length,1,16);}
/* Keep division and clipping separate: pi32v2 otherwise folds a negative
 * threshold into the protected chip-ROM constant window. Same integer result. */
static __attribute__((noinline,unused)) int32_t gd_limit(int32_t x,int32_t bound) {
  return x < -bound ? -bound : x > bound ? bound : x;
}
static inline int32_t gd_sat(int32_t x) { return gd_limit(x, 30000); }
static inline uint32_t gd_random(uint32_t *s) {
  if (!*s)
    *s = 0x1234567u;
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}
/* One deterministic command mutates the UI mirror and audio project alike.
 * Scope: bit 7 = all lanes/active bars; otherwise lane bits 2..4, page 0..1. */
static inline void gd_mutate_pattern(GDProject *p,unsigned scope,uint32_t random_seed){
  unsigned first=scope&128?0:(scope>>2)&7,last=scope&128?8:first+1;
  unsigned start=scope&128?0:(scope&3)*16;
  unsigned end=scope&128?(unsigned)gd_clamp(scope&7,1,4)*16:start+16;
  for(unsigned lane=first;lane<last;++lane)for(unsigned step=start;step<end;++step)
    if(step%4&&gd_random(&random_seed)%100<35)
      p->track[lane].step[step]=p->track[lane].step[step]?0:90;
}
static inline int32_t gd_sine(uint32_t ph) {
  unsigned i = ph >> 21, f = (ph >> 9) & 4095;
  return gd_sines[i] +
         (gd_sines[(i + 1) & 2047] - gd_sines[i]) * (int32_t)f / 4096;
}
static inline int32_t gd_wave(uint32_t ph, int square) {
  if (square)
    return (ph >> 31) ? -22000 : 22000;
  int32_t x = (int32_t)(ph >> 16);
  return ((x < 32768 ? x : 65535 - x) - 16384) * 2;
}
/* Integer polyBLEP reduces edge aliasing for the ordinary saw/square sources.
 * Tape seams and extreme FM remain deliberately lo-fi, not alias-free. */
static inline int32_t gd_blep(uint32_t ph, uint32_t inc) {
  uint32_t scale = inc >> 15;
  if (!scale)
    return 0;
  if (ph < inc) {
    int32_t x = (int32_t)(ph / scale);
    return 2 * x - (int32_t)((int64_t)x * x >> 15) - 32768;
  }
  if (ph > 0xffffffffu - inc) {
    int32_t x = (int32_t)((0xffffffffu - ph) / scale);
    return 32768 - 2 * x + (int32_t)((int64_t)x * x >> 15);
  }
  return 0;
}
static inline int32_t gd_osc(uint32_t ph, uint32_t inc, int square) {
  if (!square)
    return gd_wave(ph, 0);
  int32_t s = (ph >> 31) ? -32767 : 32767;
  s += gd_blep(ph, inc) - gd_blep(ph + 0x80000000u, inc);
  return s * 22000 / 32768;
}
#include "acoustic.h"
#include "synth.h"
static inline void gd_extended_effect_defaults(uint8_t e[30][4]){
 for(unsigned effect=0;effect<8;++effect){e[effect][0]=64;e[effect][1]=64;e[effect][2]=effect==6?64:31;e[effect][3]=127;}
 const uint8_t lp[4]={96,127,64,127},hp[4]={96,24,127,127},phone[4]={96,64,64,127},wobble[4]={30,64,0,64},flange[4]={50,64,48,64},pump[4]={80,127,64,85},rise[4]={96,43,120,127},fall[4]={96,127,24,127};
 for(unsigned k=0;k<4;++k){e[8][k]=lp[k];e[9][k]=hp[k];e[10][k]=phone[k];e[15][k]=wobble[k];e[21][k]=flange[k];e[24][k]=pump[k];e[25][k]=rise[k];e[26][k]=fall[k];}
 for(unsigned effect=16;effect<=18;++effect){e[effect][0]=127;e[effect][1]=64;e[effect][2]=effect==17?64:0;e[effect][3]=85;}
 e[19][2]=64;e[19][3]=127;e[23][3]=127;
}
static inline void gd_defaults(GDProject *p) {
  *p = (GDProject){0};
  p->magic = 0x47444631u;
  p->version = 9;
  p->bpm = 120;
  p->swing = 128; /* Fresh grooves swing eighths; amount starts straight. */
  p->pages = 1;
  p->reserved=8; /* Balanced 2x, two Glitch notes; configurable up to four. */
  for (unsigned l = 0; l < 8; ++l) {
    GDTrack *t = &p->track[l];
    t->length = 16;
    for (unsigned j = 0; j < 32; ++j)
      t->param[j] = 64;
    t->param[0] = l == 0 ? 34 : l == 5 ? 43 : 64;
    t->param[1] = l == 2 ? 62 : l == 3 ? 82 : 88;
    t->param[2] = l == 1 ? 96 : l == 4 ? 110 : 64;
    t->param[3] = 14;
    if(l<2)gd_drum_character_defaults(t->param,l);
    t->param[29] = l == 6 ? 70 : l == 7 ? 90 : 110;
    t->param[30] = l == 2 ? 45 : l == 3 ? 85 : 64;
    t->param[31] = 64;
    for (unsigned s = 0; s < 64; ++s) {
      t->chance[s] = 100;
      t->ratchet[s] = 1;
      t->lock_id[s] = 255;
    }
    for (unsigned k = 0; k < 2; ++k) {
      t->lfo[k].rate = 5;
      t->lfo[k].target = 2;
    }
  }
  gd_init_kick_parameters(p->track[0].param);
  for(unsigned l=1;l<6;++l){for(unsigned j=0;j<16;++j)p->track[l].param[j]=gd_ac_init[l-1][j];p->track[l].param[29]=gd_ac_init[l-1][16];p->track[l].param[30]=64;}
  gd_init_perc(p->track[5].param);
  const uint8_t g[29] = {0, 1,  1,   42, 50, 3,  28, 0,  0, 64,
                         8, 30, 119, 12, 1,  64, 70, 15, 0, 84,
                         0, 2,  1,   49, 45, 18, 35, 0,  0};
  for (unsigned i = 0; i < 29; ++i)
    p->track[6].param[i] = g[i];
  for(unsigned i=0;i<29;++i)p->track[6].param[i]=gd_gl_init[i];
  p->track[6].param[29]=114;
  const uint8_t a[20] = {36, 48, 1,  0,  24, 64, 12, 82, 74, 95,
                         24, 96, 83, 64, 88, 5,  0,  25, 64, 64};
  for (unsigned i = 0; i < 20; ++i)
    p->track[7].param[i] = a[i];
  for(unsigned i=0;i<29;++i)p->track[7].param[i]=gd_an_init[i];
  p->track[7].param[29]=122;
  for (unsigned s = 0; s < 64; ++s) {
    if (s % 4 == 0)
      p->track[0].step[s] = 110;
    if (s % 8 == 4)
      p->track[1].step[s] = 105;
    if (s % 2 == 0)
      p->track[2].step[s] = s % 4 == 0 ? 65 : 95;
    if (s % 8 == 6)
      p->track[3].step[s] = 65;
    if (s % 16 == 12)
      p->track[4].step[s] = 65;
    if (s % 16 == 14)
      p->track[5].step[s] = 65;
    if (s % 16 == 7 || s % 16 == 15)
      p->track[6].step[s] = 90;
    if (s % 8 == 2)
      p->track[7].step[s] = 85;
  }
  p->fx[0] = 0;
  p->fx[1] = 2;
  p->fx[2] = 35;
  p->fx[3] = 80;
  p->fx[4] = 12;
  p->fx[7] = 127;
  p->fx[10] = 127; /* Glitch is gated; full wet is the performance default. */
  p->fx[11] = 100;
  for(unsigned e=0;e<30;++e){p->effect[e][0]=96;p->effect[e][1]=64;p->effect[e][2]=64;p->effect[e][3]=64;}
  const uint8_t signature_defaults[3][4]={{64,74,53,48},{44,74,53,48},{66,79,44,42}};
  for(unsigned e=0;e<3;++e)for(unsigned k=0;k<4;++k)p->effect[27+e][k]=signature_defaults[e][k];
  const uint8_t crush[4]={96,127,127,85},gate[4]={16,64,8,85},echo[4]={96,32,127,64},ring[4]={80,127,0,64},pan[4]={48,127,0,85};
  for(unsigned k=0;k<4;++k){p->effect[11][k]=crush[k];p->effect[13][k]=gate[k];p->effect[14][k]=echo[k];p->effect[20][k]=ring[k];p->effect[22][k]=pan[k];}
  p->effect[12][3]=127; /* Sample Alias defaults fully wet within the punch mix. */
  p->effect[19][1]=96; /* fold output trim, unity at 64 */
  gd_extended_effect_defaults(p->effect);
  p->slot[0]=27;p->slot[1]=28;p->slot[2]=29;p->slot[3]=6;
  p->punch[0]=127;p->punch[1]=1;p->punch[2]=8;p->punch[3]=96;
}
static inline void gd_hit(GDEngine *d, const GDProject *p, unsigned lane,
                          unsigned vel, int pitch, int lock, int value) {
  if (lane >= 8)
    return;
  unsigned slot=lane;
  if(lane>=6){
    unsigned candidates[4]={lane,8+(lane-6)*3,9+(lane-6)*3,10+(lane-6)*3};
    unsigned capacity=lane==6?(p->reserved>>2&3u):4;if(!capacity)capacity=4;
    for(unsigned n=0;n<capacity;++n){unsigned k=candidates[n];
      if(d->voices[k].env<=1){slot=k;break;}
      if(d->voices[k].env<d->voices[slot].env ||
         (d->voices[k].env==d->voices[slot].env&&(int32_t)(d->voices[k].trigger_serial-d->voices[slot].trigger_serial)<0))slot=k;
    }
    /* A stolen voice starts clean; its siblings keep their filters/tails. */
    uint32_t phase0=d->voices[lane].lfo_phase[0],phase1=d->voices[lane].lfo_phase[1];
    int decay_mod=d->voices[lane].mod[lane==6?19:14];
    d->voices[slot]=(GDVoice){0};
    d->voices[slot].lfo_phase[0]=phase0;d->voices[slot].lfo_phase[1]=phase1;
    d->voices[slot].mod[lane==6?19:14]=(int8_t)decay_mod;
  }
  GDVoice *v = &d->voices[slot];
  v->trigger_serial=++d->trigger_serial;
  const uint8_t *q = p->track[lane].param;
  v->phase[0] = v->phase[1] = v->age = 0;
  v->velocity = (int32_t)vel * 258;
  v->env = 1073709056;
  v->motion = 1073709056;
  v->filter_env = 1073709056;
  v->pitch_offset = pitch;
  v->lock_id = lock;
  v->lock_value = value;
  v->rng = 0x511a23u + lane * 173;
  unsigned idx = lane == 6 ? 19 : lane == 7 ? 14 : 1;
  unsigned decay = (unsigned)gd_clamp(
      (lock == (int)idx ? value : q[idx]) + v->mod[idx], 0, 127);
  v->decay = gd_decay_coeff[gd_clamp((int)decay, 0, 127)];
  v->motion_decay = gd_decay_coeff[lane == 6 ? q[16] : lane == 7 ? q[8] : 55];
  v->attack = gd_time_frames[lane == 6 ? q[17] : lane == 7 ? q[15] : 0];
  if (v->attack > 88200)
    v->attack = 88200;
  v->filter_decay = gd_decay_coeff[lane == 7 ? q[12] : lane == 6 ? q[16] : 55];
  v->hold = lane == 6 ? (uint32_t)q[18] * 1389u : 0;
  if(lane<2){
    uint8_t c[16];for(unsigned j=0;j<16;++j)c[j]=(uint8_t)gd_clamp(lock==(int)j?value:q[j],0,127);
    v->attack=lane?26u+c[12]*1297u/127u:(uint32_t)gd_kick_units[12][c[12]];
    v->hold=lane?c[13]*3528u/127u:(uint32_t)gd_kick_units[13][c[13]];
    v->body_env=v->wire_env=v->click_env=v->punch_env=1073741824;
    if(lane==0){
      v->noise_low=v->wire_low=v->filter[0]=v->filter[1]=v->dc_in=v->dc_out=0;
      v->wire_decay=(uint32_t)((1073741824ull+(unsigned)gd_kick_units[1][decay]/2)/(unsigned)gd_kick_units[1][decay]);
      v->decay=1073741824u-(uint32_t)((uint64_t)(1073741824u-gd_kick_decay_coeff[decay])*gd_kick_curve_rate[c[14]]>>15);
      v->body_decay=gd_kick_round_coeff[decay];
      v->motion_decay=gd_kick_coeff((unsigned)gd_kick_units[5][c[5]]);
      v->click_decay=gd_kick_coeff((unsigned)gd_kick_units[7][c[7]]);
      v->punch_decay=gd_kick_coeff(1544);v->pitch_hold_remaining=(uint32_t)gd_kick_units[10][c[10]];
    }else{
      v->motion_decay=gd_ms_coeff(8000);
      v->body_decay=gd_scaled_decay(v->decay,c[6]);
      v->wire_decay=gd_scaled_decay(v->decay,c[8]);
    }
  }
  if(lane>=6){uint8_t params[32];for(unsigned j=0;j<32;++j)params[j]=(uint8_t)(lock==(int)j?value:q[j]);unsigned index=slot-6;int transpose=((int)q[31]-64)*24/63;gds_hit(&d->synth[index],lane,params,pitch+transpose,p->reserved&3u);}
  if(lane>=1&&lane<=5){uint8_t params[20];for(unsigned j=0;j<20;++j)params[j]=(uint8_t)(lock==(int)j?value:q[j]);int transpose=((int)q[31]-64)*24/63;gda_hit(&d->acoustic[lane],lane,params,pitch+transpose);}
  if(lane==GD_CH&&(!d->solo_mask||(d->solo_mask&(1u<<GD_CH)))){
    GDAcoustic *hat=&d->acoustic[GD_OH];unsigned fade=(unsigned)hat->c[9];
    if(fade){hat->choke_frames=hat->choke_remaining=fade;}else{hat->active=0;d->voices[GD_OH].env=0;}
  }
}
static inline void gd_release(GDEngine *d,unsigned lane,int pitch){
 if(lane!=7)return;for(unsigned slot=0;slot<8;++slot){GDSynth *s=&d->synth[slot];if(s->kind==7&&s->active&&s->held&&d->voices[slot+6].pitch_offset==pitch)s->releasing=1;}
}
/* Coefficients and two tempo-synced LFOs are updated in audio every 32 frames.
 */
static inline void gd_prepare(GDEngine *d, const GDProject *p) {
  for (unsigned slot = 0; slot < 14; ++slot) {
    unsigned l=slot<8?slot:6+(slot-8)/3;
    GDVoice *v = &d->voices[slot];
    const GDTrack *t = &p->track[l];
    for (unsigned j = 0; j < 32; ++j) {
      v->p[j] = t->param[j];
      v->mod[j] = 0;
    }
    if (v->lock_id >= 0 && v->lock_id < 32)
      v->p[v->lock_id] = (uint8_t)v->lock_value;
    for (unsigned k = 0; k < 2; ++k) {
      const GDLFO *m = &t->lfo[k];
      static const uint16_t periods[12] = {256, 128, 64, 32, 16, 8,
                                           4,   2,   1,  12, 6,  3};
      uint32_t n = (44100u * 60u / (p->bpm * 4u)) * periods[m->rate % 12] / 4u,
               before = v->lfo_phase[k];
      v->lfo_phase[k] += (0xffffffffu / (n ? n : 1)) * 32u;
      int32_t x = gd_sine(v->lfo_phase[k]);
      if (m->shape == 1)
        x = gd_wave(v->lfo_phase[k], 0);
      if (m->shape == 2)
        x = (v->lfo_phase[k] >> 31) ? -32767 : 32767;
      if (m->shape == 3)
        x = (int32_t)(v->lfo_phase[k] >> 16) - 32768;
      if (m->shape == 4) {
        if (v->lfo_phase[k] < before || !v->lfo_rng[k])
          v->lfo_value[k] =
              (int32_t)(gd_random(&v->lfo_rng[k]) & 65535) - 32768;
        x = v->lfo_value[k];
      }
      v->lfo_value[k]=x;
      int amount = x * (int32_t)m->depth / 32768, idx = -1;
      switch (m->target) {
      case 1:
        idx = l == 6 ? 3 : 0;
        amount /= 4;
        break;
      case 2:
        idx = l == 6 ? 12 : l == 7 ? 9 : 2;
        break;
      case 3:
        idx = l == 6 ? 4 : l == 7 ? 4 : 1;
        break;
      case 4:
        idx = l == 6 ? 5 : l == 7 ? 5 : 2;
        break;
      case 5:
        idx = l==6?10:l==7?18:l==0?3:l==1?3:l==4?9:15;
        break;
      case 6:
        idx = 30;
        break;
      case 7:
        idx = 29;
        break;
      case 8:
        idx = l == 6 ? 28 : l == 7 ? 6 : 2;
        break;
      }
      if (idx >= 0) {
        v->p[idx] = (uint8_t)gd_clamp(v->p[idx] + amount, 0, 127);
        v->mod[idx] = (int8_t)gd_clamp(v->mod[idx] + amount, -127, 127);
      }
      if (m->target == 1 && l >= 6) {
        unsigned other = l == 6 ? 23 : 1;
        v->p[other] = (uint8_t)gd_clamp(v->p[other] + amount, 0, 127);
      }
    }
    uint8_t *q = v->p;
    int transpose=((int)q[31]-64)*24/63;
    v->pan=q[30];v->gain=l==0?gd_kick_level[q[29]]:l==5?gd_perc_level[q[29]]:l<6?gd_ac_level[l-1][q[29]]:gd_synth_level[l-6][q[29]];
    if(l>=1&&l<=5){gda_config(&d->acoustic[l],l,q,v->pitch_offset+transpose);continue;}
    if(l>=6){gds_config(&d->synth[slot-6],l,q,v->pitch_offset+transpose);continue;}
    uint32_t root=(uint32_t)((uint64_t)gd_kick_root_inc[q[0]]*gd_note_inc[gd_clamp(60+v->pitch_offset+transpose,0,120)]/gd_note_inc[60]);
    v->loop_size[0]=root;v->loop_size[1]=(uint32_t)gd_kick_units[4][q[4]];
    v->inc[0]=gd_kick_sweep(root,gd_kick_units[4][q[4]],v->motion);
    uint64_t punch=(uint64_t)root*(131072u+(unsigned)(v->motion>>14))>>15;
    v->inc[1]=(uint32_t)(punch<7791324?7791324:punch>38956619?38956619:punch);
  }
}
static inline void gd_render_sample(GDEngine *d, const GDProject *p,
                                    int32_t *left, int32_t *right) {
  if ((d->counter++ & 31u) == 0)
    gd_prepare(d, p);
  int32_t L = 0, R = 0;d->delay_input=0;
  for (unsigned slot = 0; slot < 14; ++slot) {
    unsigned l=slot<8?slot:6+(slot-8)/3;
    GDVoice *v = &d->voices[slot];
    v->last_sample=0;
    if (v->env <= 1)
      continue;
    const uint8_t *q = v->p;
    if(l>=6){
      GDSynth *synth=&d->synth[slot-6];int sample=l==7?gds_analog(synth,&v->rng):gds_glitch(synth,&v->rng);
      if(l==7){sample=gd_kick_tanh((int32_t)((int64_t)sample*v->velocity*7/(32768*5)));if(synth->releasing)sample=gda_env(sample,synth->release_gain);}
      else sample=(int32_t)((int64_t)sample*v->velocity>>15);
      sample=(int32_t)((int64_t)sample*v->gain>>15)/3;
      int amplitude=synth->env[l==7?2:0];if(l==7&&synth->releasing)amplitude=gda_env(amplitude,synth->release_gain);
      v->age=synth->age;v->env=synth->active?gd_clamp(amplitude,2,1073741824):0;v->last_sample=(int16_t)sample;
      if(d->solo_mask?!(d->solo_mask&(1u<<l)):p->track[l].mute)continue;
      d->delay_input+=sample*d->delay_send[l]/127;
      L+=sample*(127-v->pan)/127;R+=sample*v->pan/127;continue;
    }
    if(l>=1&&l<=5){
      GDAcoustic *ac=&d->acoustic[l];int raw=gda_render(ac,(int32_t)gd_random(&v->rng)>>16);
      int sample=gd_kick_tanh((int32_t)((int64_t)raw*v->velocity*7/(32768*5)));
      sample=(int32_t)((int64_t)sample*v->gain>>15)/3;
      v->age=ac->age;v->env=ac->active?1073741824:0;v->last_sample=(int16_t)sample;
      if(d->solo_mask?!(d->solo_mask&(1u<<l)):p->track[l].mute)continue;
      d->delay_input+=sample*d->delay_send[l]/127;
      L+=sample*(127-v->pan)/127;R+=sample*v->pan/127;continue;
    }
    uint32_t random=gd_random(&v->rng);
    int32_t noise = l==0?(int32_t)random>>16:(int32_t)(random&65535)-32768,s=0;
    if(l==0){
      v->inc[0]=gd_kick_sweep(v->loop_size[0],(int)v->loop_size[1],v->motion);
      uint64_t punch=(uint64_t)v->loop_size[0]*(131072u+(unsigned)(v->motion>>14))>>15;
      v->inc[1]=(uint32_t)(punch<7791324?7791324:punch>38956619?38956619:punch);
      if(v->age>=(unsigned)gd_kick_units[8][q[8]])v->phase[0]+=v->inc[0];
      if(gd_kick_units[11][q[11]])v->phase[1]+=v->inc[1];
    }else{v->phase[0]+=v->inc[0];v->phase[1]+=v->inc[1];}
      if(l==0){
        unsigned delay=(unsigned)gd_kick_units[8][q[8]];
        int bodyAttack=v->age<=delay?0:v->age-delay>=v->attack?32767:(int)((uint64_t)(v->age-delay)*32767u/(v->attack?v->attack:1));
        int envelope=v->env>>15,curve=gd_kick_units[14][q[14]];
        unsigned decay_start=delay+v->attack+v->hold;
        if(curve>0&&v->age>=decay_start){
          uint64_t elapsed=(uint64_t)(v->age-decay_start)*v->wire_decay>>15;
          int progress=elapsed>=32768?32768:(int)elapsed;
          int cutoff=gd_clamp((progress-28180)*32768/4588,0,32768);
          int smooth=(int)((int64_t)cutoff*cutoff*(98304-2*cutoff)>>30);
          int rounded=(int)((int64_t)(v->body_env>>15)*(32768-smooth)>>15);
          envelope+=(int)((int64_t)(rounded-envelope)*curve>>15);
        }
        int b=(int)((int64_t)gd_sine(v->phase[0])*bodyAttack*envelope>>30);
        b+=(int)((int64_t)gd_sine(v->phase[1])*(v->punch_env>>15)*gd_kick_units[11][q[11]]*16/((int64_t)32768*32768*5));
        unsigned drive=q[3];
        if(gd_kick_drive_wet[drive]){
          int soft=(int)((int64_t)gd_kick_tanh((int)((int64_t)b*gd_kick_drive_gain[drive]>>15))*gd_kick_drive_norm[drive]>>15);
          int shape=gd_kick_units[9][q[9]],delta=0;
          if(shape<0){
            int squared=(int)((int64_t)soft*soft*3/327680);
            int dc=squared-v->filter[0]+(int)((int64_t)v->filter[1]*GD_KICK_DELTA_DC_COEFF>>15);
            v->filter[0]=squared;v->filter[1]=dc;delta=(int)((int64_t)dc*(-shape)>>15);
          }else if(shape>0){
            int cubic=(int)((int64_t)soft*soft*soft>>30);
            delta=(int)((int64_t)(soft-cubic)*shape*7/((int64_t)32768*20));
          }
          b+=(int)((int64_t)(soft-b)*gd_kick_drive_wet[drive]>>15)+(int)((int64_t)delta*gd_kick_shape_wet[drive]>>15);
          b=(int)((int64_t)b*gd_kick_drive_comp[drive]>>15);
        }
        int color=gd_kick_units[15][q[15]],colored=noise;
        if(color){
          v->noise_low+=(int32_t)((int64_t)(noise-v->noise_low)*GD_KICK_COLOR_COEFF>>15);
          int target=color<0?v->noise_low*27/20:(noise-v->noise_low)*19/20;
          colored+=(int)((int64_t)(target-noise)*(color<0?-color:color)>>15);
        }
        v->wire_low+=(int32_t)((int64_t)(colored-v->wire_low)*gd_kick_tone_coeff[q[2]]>>15);
        int click=v->wire_low;
        click=(int)((int64_t)click*(v->click_env>>15)*gd_kick_units[6][q[6]]*11/((int64_t)32768*32768*20));
        if(v->age<9)click=click*(int)v->age/9;
        s=b+click;
        v->click_env=(int32_t)((int64_t)v->click_env*v->click_decay>>30);
        v->punch_env=(int32_t)((int64_t)v->punch_env*v->punch_decay>>30);
      }
    int32_t env = v->env >> 15;
    if (v->age < v->attack)
      env = (int32_t)((v->age * 32767u) / (v->attack ? v->attack : 1));
    if (v->age >= v->attack + v->hold + (l==0?(unsigned)gd_kick_units[8][q[8]]:0u)){
      v->env = (int32_t)((int64_t)v->env * v->decay >> 30);
      if(l==0)v->body_env=(int32_t)((int64_t)v->body_env*v->body_decay>>30);
    }
    if(l==0){
      if(v->age>=(unsigned)gd_kick_units[8][q[8]]){
        if(v->motion<=193273528&&v->pitch_hold_remaining){if(!--v->pitch_hold_remaining)v->motion=(int32_t)((int64_t)v->motion*v->motion_decay>>30);}
        else{v->motion=(int32_t)((int64_t)v->motion*v->motion_decay>>30);if(v->pitch_hold_remaining&&v->motion<193273528)v->motion=193273528;}
      }
    }else v->motion=(int32_t)((int64_t)v->motion*v->motion_decay>>30);
    v->filter_env = (int32_t)((int64_t)v->filter_env * v->filter_decay >> 30);
    ++v->age;
    if(l==0)env=32767;
    else if(l==1)env=gd_envelope_curve(env,q[14]);
    s = (int32_t)((int64_t)s * env >> 15);
    s = l==0?gd_kick_tanh((int32_t)((int64_t)s*v->velocity*7/(32768*5))):(int32_t)((int64_t)s*v->velocity>>15);
    s = l==0?(int32_t)((int64_t)s*v->gain>>15):s*v->gain/127;
    if(l!=0){
    int32_t dc = s - v->dc_in + (int32_t)((int64_t)v->dc_out * 32676 >> 15);
    v->dc_in = s;
    v->dc_out = gd_sat(dc);
    s=v->dc_out;
    }
    s/=3;
    v->last_sample=(int16_t)s;
    if (d->solo_mask ? !(d->solo_mask&(1u<<l)) : p->track[l].mute)
      continue;
    d->delay_input+=s*d->delay_send[l]/127;
    L += s * (127 - v->pan) / 127;
    R += s * v->pan / 127;
  }
  d->delay_input=gd_sat(d->delay_input/2);
  *left = gd_sat(L);
  *right = gd_sat(R);
}
#endif
