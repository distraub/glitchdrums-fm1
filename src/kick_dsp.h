/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-point kick paths adapted from DrumVoice.hpp; Q15 audio, Q30 envelopes. */
#pragma once
#include "kick_dsp_tables.h"
static inline int32_t gd_kick_tanh(int32_t input){
 int sign=input<0?-1:1;unsigned magnitude=input<0?0u-(unsigned)input:(unsigned)input;
 if(magnitude>=262144)return sign*32767;
 unsigned index=magnitude>>8,fraction=magnitude&255;
 return sign*(gd_kick_tanh_table[index]+(gd_kick_tanh_table[index+1]-gd_kick_tanh_table[index])*(int)fraction/256);
}
static inline uint32_t gd_kick_sweep(uint32_t root,int drop,int32_t motion){
 unsigned position=(unsigned)((int64_t)drop*(motion>>15)>>15),index=position>>4;
 uint32_t ratio=gd_kick_sweep_ratio[index]+(gd_kick_sweep_ratio[index+1]-gd_kick_sweep_ratio[index])*(position&15)/16;
 uint64_t increment=(uint64_t)root*ratio>>16;
 return (uint32_t)(increment>1932735283u?1932735283u:increment);
}
