/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include "../platform/src/bootguard.h"
int main(void) {
  bootguard_t b={UINT32_MAX,UINT32_MAX,UINT32_MAX};
  assert(bootguard_begin(&b)==BOOT_NORMAL);
  assert(bootguard_begin(&b)==BOOT_NORMAL);
  assert(bootguard_begin(&b)==BOOT_RECOVERY);
  assert(bootguard_begin(&b)==BOOT_ROM);
  assert(!b.pending&&!b.failed);
  assert(bootguard_begin(&b)==BOOT_NORMAL);
  bootguard_clear(&b);
  assert(bootguard_begin(&b)==BOOT_NORMAL);
  assert(bootguard_manual(1));
  assert(!bootguard_manual(0)&&!bootguard_manual(2)&&!bootguard_manual(3));
  return 0;
}
