/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
typedef struct {
 uint8_t q[20];int pitch,valid,c[20];unsigned model,age,attack,hold,active;
 uint32_t root,phase[6],inc[6],coef[8];int32_t env[8];
 int32_t color,filter,metal_filter,smack_hp,smack_lp;
 int32_t metal_gain,noise_gain,texture;
 unsigned interval,next_burst,bursts,early_end,choke_remaining,choke_frames;
} GDAcoustic;
