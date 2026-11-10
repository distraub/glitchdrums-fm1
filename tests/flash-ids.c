#include <assert.h>
#include "hal/fm1_flash_ids.h"
int main(void) {
  assert(fl_jedec_known(0x856014u));
  assert(fl_jedec_known(0xB36014u));
  assert(!fl_jedec_known(0));
  assert(!fl_jedec_known(0xFFFFFFu));
  assert(!fl_jedec_known(0x856015u));
  return 0;
}
