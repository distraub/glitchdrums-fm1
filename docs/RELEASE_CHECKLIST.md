# Firmware Hub release checklist

Status: emulator-tested development firmware; no physical FM-1 tests yet.
Do not describe a successful native build as hardware validation.

## Prepared

- Standalone public GPL-3.0-only firmware source and third-party notices.
- Root fmhub.json and 512 × 512 logo, under 300 KB.
- Native package workflow with the upstream package, loader and MMIO safety checks.
- Native engine regression tests and RemoteFM1 build instructions.

## Native build findings (10 November 2026)

The GitHub native workflow is enabled. The first build exposed missing bare-metal
64-bit integer division helpers; these now have a portable integer-only runtime
and randomized/boundary regression tests. Emulator sound formulas are unchanged.

The next build reaches linking but fails the native RAM layout: `.bss` exceeds
the 96 KiB RAM region by 90,996 bytes and overlaps the separately assigned pool.
The pool has spare capacity, but moving state must account for total RAM as well
as placement. No hardware package has been produced. This needs a measured RAM
allocation pass; do not weaken linker bounds or flash safety checks to bypass it.

Failure log: https://github.com/distraub/glitchdrums-fm1/actions/runs/42829846697

## Before submitting

- [ ] Native workflow passes; inspect linker RAM/flash report and package checks.
- [ ] Check the actual product identity from firmware.json/build output at
      https://thefirmwarehub.com/submit/#check; resolve any collision before flashing.
      The current build appends the minor version digit to FM-1_73 (currently FM-1_737).
- [ ] Install the candidate .fwsc on a real FM-1 starting from stock V15.
- [ ] Install it starting from another firmware.
- [ ] Confirm another firmware installs again using the included update loader.
- [ ] Confirm SAVE survives a full power cycle, including pattern banks and voice settings.
- [ ] Check audio load/dropouts with all voices, chords, Slice, MIDI and signature effects.
- [ ] Verify USB, TRS MIDI and both supported flash revisions where hardware is available.
- [ ] Tag the tested commit and attach its .fwsc to the corresponding GitHub release.
- [ ] Submit the repository at https://thefirmwarehub.com/submit/ with truthful device-test results.

This instrument does not import projects or samples from other firmwares.
Keep a known-good firmware and recovery instructions available during device tests.

Requirements: https://thefirmwarehub.com/fmhub.md
