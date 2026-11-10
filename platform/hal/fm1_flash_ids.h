/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* FM-1 8-Mbit NOR revisions: Puya P25Q80 and UC25HQ80. */
static inline int fl_jedec_known(uint32_t id) {
  return id == 0x856014u || id == 0xB36014u;
}
