# Hardware validation — blocked

14 November 2026, FM-1 with Puya 0x856014, chip key 0x980F, m-bootloader 2.4.

**0.17.8 fails immediately at startup. Do not install it for normal use.**
The package writes and verifies, then the application hits its stack guard and
enters USB rescue after repeated startup failures. Backups of the original stock
and m-bootloader configurations were verified before testing.

Retained USB diagnostic, read without writing or resetting:

```
crashes 2  vec 0x01  pc 0x02012f94  rets 0x02003706
emu 0x00000008  dbg 0x00000000  at 604 ms  etm 0x02012f82
```

Rebuilt exact source dc0ea6f with symbols; package SHA-256 reproduced exactly:
`4b4022d8e9da4083726211f54cd316c852834c5abbdb7e2e62786242fdd693a8`.

At `setup` 0x02012f82, native code moves SP down 59,220 bytes. The exception PC
is immediately after that SP change; return address is the call from fm1_cstart.
The main stack reservation is 24,320 bytes. EMU bit 3 is the stack-limit fault.
The firmware/bootloader regions were not the cause of this startup exception.

Fix under verification: clear large objects in place, keep startup restore
branches out of line, bound target stack frames in the build and report usage.
Audio must remain silent until setup publishes initialized state, because native
IRQs begin before setup returns. Existing stack/memory/flash guards stay enabled.

No replacement should be called hardware-validated until it boots on the unit,
plays within its deadline, saves across a power cycle and can be left again.

## Recovery confirmed

Alex restored the 16:05 stock V15 backup through M-Flasher on 14 November
2026 and confirmed the unit is back to stock. The review listed only application
sectors 0x04000–0x92FFF (143 sectors); the installed bootloader was preserved.

## Replacement target measurements

The 0.17.10 diagnostic build (GitHub run 43643528831) reports a largest
individual frame of 5,848 bytes (`loop`), with `audio` at 616 bytes. Large
project construction helpers now use separate frames of at most 3,880 bytes.
These are individual frame measurements, not complete interrupt or nested
call-chain bounds. Production build 43643343532 passed the target frame
limit check and its deliberate oversized-frame negative probe.
The replacement remains untested on physical hardware.

## 0.17.10 candidate review

`tools/review_stack.py build/stack-review-01710` uses ELF function extents
(including compiler jump-table labels), direct/tail/relative calls and target
compiler frame measurements. It adds 64 bytes per measured frame and 128
bytes for unreported/assembly frames, and includes relocated RAM flash calls.
No recursive call paths were found from the reviewed roots. Results:

- Setup: 8,300 bytes, through redraw/factory construction.
- Loop: 14,080 bytes, through redraw/factory construction.
- Audio IRQ: 3,332 bytes, through note triggering and integer division.
- Timer IRQ: 540 bytes, conservatively including USB polling.

Adding the caller frame to foreground paths remains below the 24,320-byte
user stack. Adding both IRQ paths and 256 bytes for wrappers/context gives
4,128 bytes, below the 7,936-byte supervisor stack. Boot dispatch pointers
were inspected separately; these estimates are not a guarantee of all runtime
behavior. Target stack/write/bus/PC guards remain enabled.

Exact candidate SHA-256:
`eedfb3d85b670301e2988b2a39f9b28ee2df6a13911cfdd68ab32949158762e8`.
Production and symbolized builds reproduce the same package bytes. Native
CI engine and updater tests pass; local updater fault tests also pass against
this exact package. Source commit: 919a357eed000f9b1c8d136e456d43c4fb3b5c33.
GitHub attestation verification could not complete (issuer verification error);
no successful attestation verification is claimed for this candidate.

Ready for a supervised hardware trial, not yet hardware validated. First test
boot and a short playback before testing saves or sustained effects load.

## First 0.17.10 device playback

Alex confirmed that 0.17.10 boots and individual instruments sound. The default
pattern plays briefly, then slows and glitches without punch effects. Thus the
startup fix is confirmed, but sustained playback remains blocked. No new crash
record or measured deadline result is available yet.

0.17.11 investigation adds native audio render-time/peak/late-block counters
(read-only `python3 tools/flash.py --performance`) and optimizes exact unsigned
64-bit division alignment and power-of-two divisors. Native compilation uses
-O2 rather than -Os, retaining all stack, RAM, ROM-window and flash checks.
Timing telemetry counts over-budget blocks, not exact hardware dropouts; peak
and late-block counts are cumulative since boot.

Alex further isolated 0.17.10: the pattern runs with PRC, Glitch and Analog
steps removed; PRC slows it, and Glitch/Analog cause severe degradation even
when played individually. Other drum voices sound okay. This supports an
audio workload problem but does not establish measured CPU usage.

0.17.11 source 89d00f1 additionally uses normalized base-65536 quotient
estimation for 64/32 divisions whose quotient fits 32 bits. This removes the
long bitwise quotient loop from common synth ladder calculations; all results
remain integer-exact. One million additional randomized cases exercise this
path, alongside signed, unsigned, Q-format and edge-case tests.
Native CI 43645305423 passes with the stack-limit negative probe. Symbols
run 43645308409 reproduces the same package. Conservative direct-call review:
setup 7,688 bytes, loop 10,928 bytes, audio IRQ 3,184 bytes, timer 340 bytes.
These are estimates with per-frame allowances, not runtime high-water marks.
Hardware performance must be measured after loading; no claim of a resolved
audio deadline is made before that test.
Candidate SHA-256: fd02d4a1c9cde3063811e63657fdcd428f63ae9a400de0b79fc28e7e706f511d.

## 0.17.11 playback result and next investigation

Alex reports the same PRC/Glitch/Analog slowdown in 0.17.11. USB MIDI
enumerates as Felucca but neither identity nor direct performance queries
answered, even while stopped. Thus no valid native timing values were read.

0.17.12 caches invariant Analog tuning/key tracking and unchanged ladder
coefficients, skips Glitch noise-material reads when their contribution is
zero, and uses a proven 32-bit numerator in PRC shaping. The rendered PCM
matches 32 deterministic baseline vectors from 0.17.11 across quality modes,
pitch, retriggering, live parameter changes and legal randomized controls/LFOs.
Global page 2 now displays native audio rendering time as AUDIO/CPU percent
in its unused fourth cell, refreshed at 5 Hz, without USB access. KN4 is
read-only there. Native render timing remains distinct from total system CPU.
No claim of hardware deadline success until measured on the unit.

0.17.12 candidate validation: native CI 43646618185 and symbols run
43646620533 pass and produce identical packages. Code 401/567 KiB,
RAM 90/96 KiB, pool 334/336 KiB; guards unchanged. Conservative stack review
reports foreground maximum 10,928 bytes, audio IRQ 3,232 bytes and timer
340 bytes. Exact-package updater fault tests and RemoteFM1 regression tests
pass. Package SHA-256: `a4b3f9cde6ba1ca80740b538c956fbcac71d5100ae14fad0c7e3f211633118e7`. Ready for a hardware measurement trial; playback
headroom remains unconfirmed.

## First measured 0.17.12 audio load

Alex read the on-device meter: stopped 32%, reduced pattern without PRC,
Glitch or Analog 64–88%, and adding one Analog hit on step 1 reaches 113%.
These confirm render deadlines are exceeded; improvements claimed only from
host tests would not establish native headroom.

0.17.13 caches tempo/rate LFO increments per track while preserving individual
voice phases and modulation, skips inactive acoustic/synth preparation (hits
initialize these voices), and caches global delay/filter coefficients. Delay
feedback, capture recording and tails still advance. Zero delay mix skips only
inaudible wet contributions; disabled bit reduction skips divide-by-one work.
32 synth PCM vectors and 8 global-FX vectors from 0.17.12 verify output.
The FX vectors include live tempo/filter/mix changes, hold and Glitch captures.

Skiff v2.2's `firmware/hal/fm1_clk.h` and startup were inspected: its optional
clock switch reduces the V15 boot clock from 360 to 320 MHz. No clock-register
changes are included here. No native performance gain is claimed until the
new package is measured on the physical unit.

0.17.13 native CI 43648361540 and symbol run 43648365049 pass and reproduce
identical package bytes. Code 401/567 KiB, RAM 90/96 KiB, pool 334/336 KiB.
Conservative stack estimates: foreground 10,928 bytes, audio IRQ 3,200 bytes,
timer 340 bytes; target guards remain enabled. All native tests, PCM vectors
and exact-package updater fault tests pass. Candidate SHA-256: `a9de1e22b698ea4a8ee68d6056f580c9f18f53ba2854ec85f85f94e857a679c8`.
Hardware load reduction remains unmeasured.

## Measured 0.17.13 result

Alex reports stopped load 28%, reduced pattern plus one Analog hit 109%,
and stock sequence approximately 200%. The cache pass improves baseline
by four points but does not solve sustained playback.

Clock handoff investigation: exact installed m-bootloader commit 395e6cc
`src/platform.c` hal_clock_a establishes the normal 360 MHz boot contract;
hal_clock_b describes a 96 MHz updater/ROM-loader context. These source
paths do not establish which clock is running on this unit. A full power-off
and repeat meter reading was requested before any clock or voice-quality
changes. No clock-register writes or further hardware packages were made.
Sources: https://github.com/jvitkauskas/m-bootloader/tree/395e6cc
and https://github.com/illuminus5/skiff/blob/v2.2/firmware/hal/fm1_clk.h .

## 0.17.14 voice-major rendering trial

Alex cold-power-cycled and still measured roughly 200% on the default
sequence. ECO 1X measures 106–174%, and omitting PRC/Glitch/Analog still
reaches 84%. Oversampling or a simple post-flash cold start cannot resolve
this workload. No clock registers or bootloader changes are included.

The next change renders up to sixteen consecutive samples of one voice
before switching models, then processes the global effects in sample order.
This addresses repeated model/code switching from XIP; a speedup remains a
hypothesis until the unit is measured. Chunk boundaries honor every note,
ratchet, pattern/chain change, and 32-frame modulation update. Meter/scope
frames retain their original 16-frame positions. The fixed scratch buffer is
192 bytes on the audio stack; there is no render allocation or lock.

Tests compare block output and complete engine state to the sample renderer
across legal triggers, p-locks, tempo/LFO changes, mutes and sends. Existing
32 voice and 8 global-effect PCM vectors pass. `tests/block-audio-parity.mjs`
also compares actual old/new RemoteFM1 executables, using varying 1–128-frame
callback lengths and mid-playback tempo/STOP changes. All PCM bytes match for
stock, 1/16 swing ratchets, 1/8 swing ratchets, dense chords/locks and punch FX.
The full RemoteFM1 sequencer/chain/Slice/FX/MIDI/save regression suite passes.
No sample rate, voice model, oversampling or polyphony reduction is included.

0.17.14 candidate: native CI 43650163892 and diagnostic 43650167407
pass, with identical package SHA-256 `fe5ee9503cc950cc0c22e0491b62239078cf88a2bae4abf9e54bb2784a0f74dd`. Code 402/567 KiB,
RAM 90/96 KiB, pool 334/336 KiB. Conservative audio IRQ call-path
estimate 3,408 bytes plus timer 340 bytes and context allowance remains
below the 7,936-byte supervisor stack; foreground maximum 10,928 bytes.
The deliberate oversized-frame probe is rejected. Exact-package updater
fault tests pass. Hardware speedup remains unmeasured. Candidate delivered
to Alex's iCloud Firmware folder while the unit is available for other firmware.

## Measured 0.17.14 result and executable-RAM trial

Alex reports idle 18%; the stock drum pattern with no PRC or Glitch steps and
one Analog hit at step 1 still reaches 101% in ECO mode. This is not a solo
Analog reading. The block renderer improves idle load but does not establish
adequate musical headroom.

0.17.15 places gd_render_block in .ram_dsp, using the existing reserved
24 KiB executable-RAM region after the flash-busy driver. The measured
0.17.14 renderer is approximately 11 KiB and the driver approximately 3 KiB.
No memory region, stack, pool, flash or bootloader boundary is enlarged.
Startup copies the new section before guards/audio start; PC protection
bounds the contiguous driver-plus-DSP code region. The flash-busy driver
stays in its separate .ram_text section with the no-call check unchanged;
normal DSP may access XIP while audio interrupts cannot run during flash
operations. All instructions, including DSP, remain subject to ROM-window
refusal checks. New tests refuse overlaps, code-RAM overflow and a renderer
placed outside its copied section. No DSP algorithm or quality setting is
changed. Hardware performance gain remains unmeasured.

The target's 4 MiB direct-call range also requires an indirect XIP entry into
DSP RAM. The model/sample loops live at RAM +8 KiB; their small XIP math
helpers are linked below XIP +8 KiB, with an explicit linker assertion.
Control preparation uses an indirect call. Direct-call range failures remain
link errors. Native checks caught intermediate layout and HAL-boundary errors;
none of those failed packages was delivered. Startup instruction sync stays
in the HAL. CI now compares all ELF load sections to decrypted package bytes
and simulates the word-aligned copies into bounded executable RAM.

0.17.15 final validation: production CI 43652337359 and diagnostic
43652340290 pass and reproduce package SHA-256 `7fdec707a6f811480a33826d793616bcf21100f051b5d7e5ac5c2ec3812c4169`.
Sample loops gds_analog/gds_glitch/gda_render/gd_render_voice/gd_render_block
are linked inside .ram_dsp at 0x01C02000–0x01C038EB (6,380 bytes), with
XIP helpers below 0x02000D7A. The occupied executable-RAM extent is
14/24 KiB; normal RAM 90/96 KiB and pool 334/336 KiB remain within bounds.
Code is 399/567 KiB. Stack estimates explicitly include the indirect block
and control-preparation calls: audio IRQ maximum 3,400 bytes plus timer
340 bytes and context allowance, below 7,936 bytes. Foreground 10,928 bytes.
Native regression/golden PCM tests, exact ELF/package/copy-map verification
and updater fault tests pass, including local checks against the downloaded
package. Delivered to iCloud Firmware for a hardware trial; speedup remains
unmeasured. Compare ECO 1X with the same stock drums, no PRC/Glitch and one
Analog note on step 1 (previously 101%, idle 18%).

## 0.17.15 rejected on hardware

Alex reports stock drums without PRC, Glitch or Analog reach 101% in BAL 2X
and distort; adding one Analog step reaches 127%, or 128% in ECO 1X.
The comparable 0.17.14 ECO workload was 101%. The executable-RAM experiment
is a measured regression despite all static/package and PCM checks passing.
Do not install 0.17.15. It was removed from the main iCloud test folder and
archived as a failed hardware trial.

Native DSP placement/startup/PC boundaries/tooling were restored to 0.17.14
(commit 39dc2ea). That remains a reference build, not a playback-stable release.
Next work requires per-voice/shared-stage hardware timing before another
performance optimization. No unverified new optimization package is delivered.

## 0.17.16 opt-in timing diagnostic

This build starts from the restored 0.17.14 code and adds measurements, not
a performance fix. Hold GLO and tap EDIT to open CPU PROFILE. Instrumentation
is active only on that screen. The IRQ accumulates each lane's rendering,
32-frame control preparation (CTRL), sequence note/chord triggers (HITS),
and shared global/punch processing (FX). OTHER is the remaining callback
time including output limiting, timer preemption and diagnostic bookkeeping.
Peaks are per-callback percentages of the 256-frame audio deadline, retained
after STOP; independent stage peaks must not be added. Leaving and re-entering
resets peaks. Taking a snapshot briefly masks IRQs to copy 52 bytes. The
render path performs no allocation, logging or UI work. Timer reads are skipped
when the screen is closed, but instrumentation has overhead while active.

Normal GLO page cycling remains unchanged. Parameter knobs are read-only on
the profile screen; GLO exits. No DSP model, sample rate, oversampling, save
layout, memory region, stack boundary, bootloader or PC-guard change is included.
The screen is for identifying the next optimization, not claiming lower CPU.

0.17.16 final diagnostic verification: native CI 43666184172 and symbols
43666186689 pass with identical package SHA-256 `1aac757cf10fd440c7d3405d573f4e7390e2d671ca6a7ae658153277553fabeb`.
Code 401/567 KiB, RAM 90/96 KiB and pool 334/336 KiB. Conservative stack
estimates: audio IRQ 3,532 bytes plus timer 336 bytes and context allowance
remain below 7,936 bytes; foreground 10,984 bytes below 24,320. All guards
remain enabled. Native audio golden vectors, block/state parity, profiler
accounting/reset tests and exact-package updater fault tests pass. RemoteFM1
tests verify read-only GLO+EDIT entry/exit; screen render was checked for
clipping/contrast. Delivered only in the Diagnostics subfolder; no playback
CPU improvement is claimed. Hardware stage readings are pending.

## Full stock BAL 2X per-stage baseline

Alex confirmed the 0.17.16 photo used the full stock sequence at BAL 2X:
TOTAL 198%; KIK 19, SNR 11, CH 15, OH 15, CLP 10, PRC 18, GLI 75,
ANA 49, CTRL 7, HITS 4, FX 8, OTHER 7 percent. These are independent
per-callback maxima, not additive simultaneous costs. Synth lanes dominate;
shared FX are not the first target for this pass.

0.17.17 is an explicit ECO sound/performance trial. BAL/HIGH retain exact
reference PCM; ECO uses the lower-cost voice paths described in ECO_VOICES.md.
It is not a measured hardware speedup. No new physical validation is claimed.

0.17.17 final candidate verification: native CI 43672205124 and diagnostic
43672207448 reproduce SHA-256 `f9311bdc3ffdf999f6c323a1dfc35b013d17eec0012789a0913a3756dffb8c35`.
Code 410/567 KiB, RAM 90/96 KiB, pool 334/336 KiB. Conservative audio IRQ
stack 3,564 bytes plus timer 336 and context allowance is within 7,936 bytes;
foreground 10,984 within 24,320. All guards remain enabled. Golden reference
PCM, explicit BAL/HIGH equality with ECO enabled, legal-control sanitizer
stress, pitch/envelope timing, exhaustive exact hat sign, block/state parity
and profiler tests pass. Exact-package updater fault tests also pass locally.
RemoteFM1 integration passes. Package delivered under Trials; previews under
Previews/0.17.17. User must choose ECO 1X before playback; no native CPU
improvement is claimed until the same device/workload is measured.


## 0.17.17 physical ECO 1X stock-sequence result

Alex's new photo explicitly uses the full stock sequence at ECO 1X:
TOTAL 138%; KIK 18, SNR 12, CH 12, OH 12, CLP 10, PRC 17, GLI 32,
ANA 35, CTRL 7, HITS 4, FX 8, OTHER 7. Playback remains over deadline;
this is not a stable hardware candidate. Compare against the earlier ECO
range 106–174%, not just the 198% BAL profile with a different quality mode.
Independent per-stage maxima cannot be summed into a simultaneous profile.

A host simulation of stock note spacing at 120 BPM found at most one Glitch
voice and two Analog voices active. Across 20 seconds, Analog had two active
voices about 41% of the time; reducing polyphony alone cannot be assumed to
recover enough headroom, especially for FX. Alex chose investigation of a
second-core renderer rather than reducing synth voice limits.

The first implementation step separates raw voice sums from final saturation
and counter/send commit. A real two-thread host test renders slots 0–5 and
6–13 concurrently and verifies exact PCM plus complete engine-state parity
against the single-core block renderer, with all qualities, chords, solo,
mutes, LFO/tempo and send changes. ASan/UBSan and a separate ThreadSanitizer
run pass. This does not activate core 1 on the FM-1 or establish device CPU
headroom. See SECOND_CORE.md for the remaining platform gates.


## 0.17.18 guarded second-core candidate

Native synth jobs run concurrently with drum rendering. PCM/state parity and
partial-worker failure retirement are tested; current voices/polyphony and
sound models are retained. Native CI 43677063177 and independent diagnostic
43677065467 reproduce SHA-256
`8de55220aea66480e1e6b7e3e0cc5f7119b82b36f0f021dfdef4ed4b35f1d9e8`.
Source d8b9ab5; see SECOND_CORE.md for stack, memory and recovery controls.
The worker has a dedicated stack in previously reserved RAM-run debug space;
main/audio stacks and guards are not reduced. No clock/voltage change or
bootloader write is included. Hold OCT+ at boot for core-0-only comparison;
OCT- still enters rescue. CPU PROFILE footer shows worker status.

Physical CPU headroom, playback, FX, save/power-cycle and firmware switching
remain pending. First workload: full stock sequence, ECO 1X selected while
stopped, no punch effects; capture CPU PROFILE and C1 status. The single-core
138% photo is the matching prior baseline, not the 198% BAL result.


Final delivery checks for 0.17.18: exact packaged .text/.data/.ram_text and
.c1_text match the independently built ELF; worker word-copy extents and its
single indirect call pass the RAM-idle image audit. Both native build hashes
match. Local exact-package loader/OTA fault tests pass in addition to CI,
including unchanged boot area, wrong key/CRC refusal, disconnect and resumed
power cuts. Copied to iCloud Firmware/Trials after verifying SHA-256. Hardware
playback and save/switch checks remain for Alex's unit.

## 0.17.18 physical results with C1 ON

Alex confirmed BAL 2X for the first new photo: TOTAL 143%, KIK 17, SNR 12,
CH 12, OH 12, CLP 10, PRC 17, GLI 74, ANA 47, CTRL 6, HITS 4, FX 8,
OTHER 5. The follow-up to the ECO 1X request shows TOTAL 101%, KIK 18,
SNR 12, CH 12, OH 12, CLP 11, PRC 17, GLI 30, ANA 32, CTRL 7, HITS 4,
FX 8, OTHER 7. Both show C1 ON, so startup/completion did not trigger the
fallback during these captures. These are retained per-stage peaks, not a
sum or proof of sustained error-free execution. ECO improved from 138 to
101 but remains over deadline; BAL from 198 to 143 (different earlier build).

0.17.19 replaces the fixed drums/synth split with bounded greedy assignment
of whole active lanes using quality-aware approximate weights and the active
polyphonic voice count. This is load estimation, not measured adaptive
per-voice timing. Each lane, including all siblings, still has one owner
through a block, keeping its profiler updates disjoint. Worker mask is
included in job validation. On failure, only worker-owned lanes are retired;
partial sums are discarded. Core-0-owned lanes continue once. No sound,
polyphony, save-format, clock, boot or stack change is included in this pass.
