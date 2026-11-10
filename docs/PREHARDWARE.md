# 0.17.7 hardware candidate — offline verification

By Alex Matheu. 10 November 2026.

A real `.fwsc` now builds. It is a **hardware-unvalidated development candidate**,
not a claim that installation, timing or recovery has been proven on a device.
No FM-1 was connected or flashed during this work.

## Verified before hardware

- Native build uses the Hub's shared GitHub workflow and JieLi compiler.
- Entry stub, application size, main RAM, pool, forbidden ROM-window constants/
  calls, RAM-resident flash code and MMIO access checks all pass. Regression
  tests demonstrate that invalid entry, size, RAM/pool and ROM cases are refused.
- Linker memory regions, stack reservations and bootloader boundaries are unchanged.
- Native output: code 372/567 KiB, main RAM 89/96 KiB, pool 334/336 KiB.
  Pool headroom is small; future buffer additions must pass the same linker gates.
- Removed unused grain/reflection state and reduced modulation-delay storage
  without reducing its maximum delay. Before/after sample hash matches across
  every width, both modulation effects and every signature-effect combination.
- Engine tests and RemoteFM1 integration tests pass, including save/restore,
  sixteen chord/p-lock patterns, chains and Slice performance controls.
- Actual packaged firmware passes simulated installation: byte-identical app
  region after writing; original flash head untouched; writes inside allowed
  app/staging/record ranges; wrong chip key refused before erasing the app.
- Corruption/retry, missing host confirmation, disconnect during loader staging,
  damaged loader CRC and power loss after page writes are covered. Persistent
  recovery records permit the next simulated session to complete.
- Warm-reset boot guard tests cover repeated startup failures, USB rescue,
  subsequent ROM fallback and the physical OCT− recovery request.
- App, recovery and loader recognize Puya `0x856014` and UC `0xB36014` flash IDs;
  unknown IDs remain refused. This is source/simulation coverage, not device proof.
- SHA-256 and GitHub build attestation verified for the downloaded package.

First successful candidate (source commit `a9fcf09`):
https://github.com/distraub/glitchdrums-fm1/actions/runs/42834570562

Package: `glitchdrums-fm1-0.17.7.fwsc`

SHA-256:
`c2f318ae6ef22f093ab45390199395ba9c700a809938494e78c7548997d1fb98`

The GitHub Actions `packages` artifact includes SHA256SUMS. Later successful runs
must be verified against their own attestation; do not substitute an unrelated
local rebuild. Artifacts are retained for a limited time; a public release should
be built from the final hardware-tested tag.

## When the unit arrives — not performed yet

1. Confirm it works with stock firmware. Charge it and use a reliable USB data
   cable, direct connection and an awake computer.
2. Before writing anything, identify its flash chip and make a verified **whole
   flash backup**, keeping the original safely outside this repo. Keep a known-good
   stock package and the recovery documentation available.
3. Confirm the normal USB connection and available recovery/download mode while
   the original firmware still works. Do not erase or format a bootloader disk.
4. Review the candidate checksum, build attestation and the unit's flash revision.
   Use the supported update route; do not interrupt a write. Start with low audio
   output and no performance effects.
5. Check audio/controls first, then SAVE and a complete power cycle. Check pattern
   banks, MIDI, chords, Slice and worst-case effect combinations for dropouts.
6. Prove that another known-good firmware installs again. Test installation from
   both stock V15 and another firmware before submitting to The Firmware Hub.

Use the current [Firmware Hub rescue guide](https://thefirmwarehub.com/tools/rescue/)
for the observed unit/chip and computer. The guide recommends a full backup first
and documents Mac recovery tools as well as the newer flash revision.

## Limits

Offline checks reduce avoidable risk; they cannot make a zero-brick guarantee.
USB enumeration, bootloader/flash revisions, electrical/power behavior, CPU timing
and real recovery still need physical testing. App USB rescue requires intact
startup, flash and USB code; it is not a second independent firmware bank. No
intentional power-cut tests on the first unit are needed to validate normal use.

The Hub submission remains pending those physical checks.
