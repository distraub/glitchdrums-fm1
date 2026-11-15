/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#if GD_NATIVE_FM1
extern volatile uint32_t gd_profile_active,gd_profile_requested;
uint32_t gd_profile_now(void);
void gd_profile_add(unsigned stage,uint32_t ticks);
void gd_profile_read(uint32_t out[13]);
#define GD_PROFILE_BEGIN() (gd_profile_active?gd_profile_now():0u)
#define GD_PROFILE_END(stage,start) do{if(gd_profile_active)gd_profile_add((stage),gd_profile_now()-(start));}while(0)
#else
#define GD_PROFILE_BEGIN() 0u
#define GD_PROFILE_END(stage,start) do{(void)(stage);(void)(start);}while(0)
#endif
