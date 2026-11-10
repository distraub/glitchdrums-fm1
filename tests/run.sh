#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/native
clang -std=c11 -Wall -Wextra -Werror -I platform tests/flash-ids.c -o build/native/flash-ids-test
build/native/flash-ids-test
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
