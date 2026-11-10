/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Chromatic indices start on F3, matching FM-1 and RemoteFM1. */
static int white_index(int key) {
  static const uint8_t white[16] = {0,  2,  4,  6,  7,  9,  11, 12,
                                    14, 16, 18, 19, 21, 23, 24, 26};
  for (int i = 0; i < 16; ++i)
    if (white[i] == key)
      return i;
  return -1;
}
static int black_index(int key) {
  static const uint8_t black[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
  for (int i = 0; i < 11; ++i)
    if (black[i] == key)
      return i;
  return -1;
}
/* Stable effect IDs retain settings when performance banks are rearranged. */
static inline unsigned gd_punch_key(unsigned bank,int white,int black){
  if(bank%3==2)return white>=0&&white<3?16u+(unsigned)white:32u;
  if(white>=0)return (unsigned)white;
  return black>=0&&black<3?27u+(unsigned)black:16u+(unsigned)black;
}
