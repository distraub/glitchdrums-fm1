#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/native
python3 tests/build-safety.py
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/bootguard.c -o build/native/bootguard-test
build/native/bootguard-test
python3 tests/update-fixtures.py
clang -O1 -g -fsanitize=address,undefined -Wno-unused-function -Wno-unused-variable tests/ldr-test.c -o build/native/ldr-test
clang -O1 -g -fsanitize=address,undefined -Wno-unused-function tests/ota-test.c -o build/native/ota-test
# Upstream harnesses allocate their simulated NOR for process lifetime.
export ASAN_OPTIONS=detect_leaks=0
build/native/ldr-test build/native/old.fwsc build/native/candidate.fwsc
build/native/ota-test build/native/candidate.fwsc
# Also exercise the exact native package when supplied by the packaging workflow.
if [ "$#" -gt 0 ]; then
  build/native/ldr-test build/native/old.fwsc "$1"
  build/native/ota-test "$1"
fi
