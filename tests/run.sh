#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/native build/test-release
clang -std=c11 -Wall -Wextra -Werror -I platform tests/flash-ids.c -o build/native/flash-ids-test
build/native/flash-ids-test
clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address tests/integer-runtime.c -o build/native/integer-runtime-test
build/native/integer-runtime-test
clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address tests/swing.c -o build/native/swing-test
build/native/swing-test
clang -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/memory-parity.c -o build/native/memory-parity-test
build/native/memory-parity-test
clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/core.c -lm -o build/native/core-test
build/native/core-test
clang -std=c11 -Wall -Wextra -Werror -DFOMA_MVAVE=1 -I platform -I src -fsyntax-only src/main.c

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -I platform tests/output.c -o build/native/output-test
build/native/output-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/signature.c -o build/native/signature-test
build/native/signature-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/groove.c -lm -o build/native/groove-test
build/native/groove-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/alias-comb.c -o build/native/alias-comb-test
build/native/alias-comb-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/fx-controls.c -o build/native/fx-controls-test
build/native/fx-controls-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/patterns.c -o build/native/patterns-test
build/native/patterns-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address tests/chain.c -o build/native/chain-test
build/native/chain-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address tests/midi.c -o build/native/midi-test
build/native/midi-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/fullfx.c -o build/native/fullfx-test
build/native/fullfx-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/delay-sends.c -o build/native/delay-sends-test
build/native/delay-sends-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/roll-punch.c -o build/native/roll-punch-test
build/native/roll-punch-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/remix.c -o build/native/remix-test
build/native/remix-test

# Golden output from 0.17.11: preserve PCM while caching synth work.
clang -O2 -std=c11 -fsanitize=undefined,address -DGD_ECO_VOICES=0 -I src tests/render-parity.c -o build/native/render-parity-test
build/native/render-parity-test > build/native/render-parity.txt
cmp tests/render-parity.txt build/native/render-parity.txt
echo "PASS: cached synthesis matches 32 pre-optimization PCM vectors"

clang -O2 -std=c11 -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/fx-cache-vectors.c -o build/native/fx-cache-vectors
build/native/fx-cache-vectors > build/native/fx-cache-vectors.txt
cmp tests/fx-cache-vectors.txt build/native/fx-cache-vectors.txt
echo "PASS: cached global effects preserve PCM across live tempo/control changes"

clang -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=undefined,address -I src tests/block-render.c -o build/native/block-render-test
build/native/block-render-test

clang -std=c11 -Wall -Wextra -Werror -fsanitize=undefined,address tests/profile-state.c -o build/native/profile-state-test
build/native/profile-state-test

clang -O2 -std=c11 -fsanitize=undefined,address -I src tests/render-parity.c -o build/native/quality-parity-test
build/native/quality-parity-test > build/native/quality-parity.txt
python3 - <<'PYQUALITY'
from pathlib import Path
old=Path('tests/render-parity.txt').read_text().splitlines()
new=Path('build/native/quality-parity.txt').read_text().splitlines()
assert len(old)==len(new)==32
for i,(a,b) in enumerate(zip(old,new)):
 if i%3!=2:assert a==b,(i,a,b)
assert any(a!=b for i,(a,b) in enumerate(zip(old,new)) if i%3==2)
print('PASS: BAL/HIGH retain exact PCM with ECO enabled; ECO changes are explicit')
PYQUALITY
clang -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=undefined,address -I src tests/eco-voices.c -o build/native/eco-voices-test
build/native/eco-voices-test

clang -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=undefined,address -I src tests/hat-sign.c -o build/native/hat-sign-test
build/native/hat-sign-test

# Actual concurrent partitions: sanitizer checks and exact PCM/full-state parity.
clang -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=undefined,address -pthread -I src tests/parallel-render.c -o build/native/parallel-render-test
build/native/parallel-render-test

clang -O2 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=undefined,address -pthread -I src -I platform/hal tests/dualcore.c -o build/native/dualcore-test
build/native/dualcore-test

# Exact 0.17.19 PCM/history vectors: signature-only gates, stacks, controls,
# capture transitions and release fades must survive dispatch optimization.
clang -O2 -std=c11 -fsanitize=undefined,address -DFOMA_MVAVE=1 -I platform -I src tests/punch-cache-vectors.c -o build/native/punch-cache-vectors
build/native/punch-cache-vectors > build/native/punch-cache-vectors.txt
cmp tests/punch-cache-vectors.txt build/native/punch-cache-vectors.txt
clang -O2 -std=c11 -fsanitize=undefined,address -DGD_PUNCH_SIGNATURE_FAST=0 -DFOMA_MVAVE=1 -I platform -I src tests/punch-cache-vectors.c -o build/native/punch-general-vectors
build/native/punch-general-vectors > build/native/punch-general-vectors.txt
cmp tests/punch-cache-vectors.txt build/native/punch-general-vectors.txt
echo 'PASS: signature dispatch and cached fades preserve 36 pre-change PCM/history vectors'
