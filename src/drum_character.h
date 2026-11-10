/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point kick/snare character controls inspired by DrumVoice.hpp.
 * Indices 0..3 and 29..31 retain their existing meanings. */
#pragma once
#include "controls.h"
#include "kick_init.h"
static const GDControl gd_kickctrl[16]={
{"TUNE",127,F_KICK+0},
{"AMP DECAY",127,F_KICK+1},
{"TONE",127,F_KICK+2},
{"BODY DRIVE",127,F_KICK+3},
{"PITCH DROP",127,F_KICK+4},
{"PITCH DEC",127,F_KICK+5},
{"CLICK LEVEL",127,F_KICK+6},
{"CLICK DEC",127,F_KICK+7},
{"BODY DELAY",127,F_KICK+8},
{"BODY SHAPE",127,F_KICK+9},
{"PITCH HOLD",127,F_KICK+10},
{"BODY PUNCH",127,F_KICK+11},
{"ATTACK",127,F_KICK+12},
{"HOLD",127,F_KICK+13},
{"ENV CURVE",127,F_KICK+14},
{"NOISE COLOR",127,F_KICK+15}};
static const GDControl gd_snarectrl[16]={
 {"NOTE",96,F_NOTE},{"AMP DECAY",127,F_TIME},{"TONE",127,F_PERCENT},{"DRIVE",127,F_PERCENT},
 {"PUNCH",127,F_PERCENT},{"RING RATIO",127,F_NUMBER},{"BODY DECAY",127,F_NUMBER},{"WIRE ATTACK",127,F_NUMBER},
 {"WIRE DECAY",127,F_NUMBER},{"BODY LEVEL",127,F_PERCENT},{"WIRE LEVEL",127,F_PERCENT},{"WIRE COLOR",127,F_BIPOLAR},
 {"ATTACK",127,F_NUMBER},{"HOLD",127,F_NUMBER},{"ENV CURVE",127,F_BIPOLAR},{"CUTOFF",127,F_HZ}};
static inline void gd_drum_character_defaults(uint8_t *q,unsigned lane){
 static const uint8_t snare[12]={37,44,64,0,64,89,100,64,0,0,64,110};
 for(unsigned i=0;i<12;++i)q[4+i]=lane?snare[i]:gd_kick_init[4+i];
}
static inline uint32_t gd_ms_coeff(unsigned frames){
 /* Rational approximation of an exponential with -80 dB time = frames. */
 return (uint32_t)((uint64_t)1073741824u*(frames?frames:1)/(frames+9u));
}
static inline uint32_t gd_scaled_decay(uint32_t base,unsigned control){
 unsigned scale=control<64?64+control*3:256+(control-64)*768/63;
 return 1073741824u-(uint32_t)((uint64_t)(1073741824u-base)*256/scale);
}
static inline int gd_envelope_curve(int env,unsigned control){
 int amount=control<64?(64-(int)control)*2:((int)control-64)*2;
 if(amount>127)amount=127;
 int curved=control<64?32767-(int)((int64_t)(32767-env)*(32767-env)>>15):(int)((int64_t)env*env>>15);
 return (env*(127-amount)+curved*amount)/127;
}

static inline uint32_t gd_kick_coeff(unsigned frames){
 return (uint32_t)((uint64_t)1073741824u*frames*10u/(frames*10u+69u));
}

static inline void gd_init_kick_parameters(uint8_t *q){
 for(unsigned i=0;i<16;++i)q[i]=gd_kick_init[i];
 q[29]=GD_KICK_INIT_LEVEL;q[30]=q[31]=64;
}
