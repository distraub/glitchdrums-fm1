/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
typedef struct {
 uint8_t q[32];unsigned factor,valid,kind,age,active,attack[2],hold,cursor,drain,held,releasing;
 int pitch,c[29];uint32_t phase[2],root[2],coef[4];int32_t env[3],ladder[4],noise_low,history[256],dcin,dcout,release_gain;
} GDSynth;
