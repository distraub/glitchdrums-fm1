# Second-core investigation

Status: 0.17.19 is the latest guarded native hardware trial. Host concurrency/fault tests
and independent native builds pass; physical CPU improvement is not yet known.
Installing this trial enables a core-1 startup attempt. Hold OCT+ during power-on
to keep it off for comparison. OCT- rescue and the bootloader recovery remain.

## Source and suitability

Skiff v2.2 provides an optional AC79 second-core audio implementation:
https://github.com/illuminus5/skiff/blob/v2.2/firmware/hal/fm1_cpu1.h
https://github.com/illuminus5/skiff/blob/v2.2/firmware/hal/fm1_cpu1.S
https://github.com/illuminus5/skiff/blob/v2.2/firmware/fx.c

Its code credits X0X, Melodee and Jangada under GPL-3.0. Preserve those
attributions when adapting the platform code. It reports unreliable second
cores on some units; checksummed, sequenced jobs and bounded waits are
necessary. No voltage or processor-frequency increase is proposed.

## Validated renderer boundary

`gd_render_partition` writes only its assigned voice states and private raw
stereo/send sums. Core 0 prepares controls and note events first. A worker
can render synth slots 6–13 while core 0 renders drums 0–5. Both must finish
before saturation, delay-send scaling, counter advancement, FX, scopes or
subsequent events. `gd_render_commit` performs that final merge once.

`tests/parallel-render.c` runs the two partitions simultaneously with a host
thread and checks exact samples and complete engine state against the normal
block renderer. It covers all quality modes, four-note synth triggers, solo,
mutes, LFO/tempo changes and sends. ASan/UBSan and ThreadSanitizer pass. The
host uses pthread condition variables solely to model publish/join; those
primitives must never enter the firmware audio callback.

The native path now submits active synth work to core 1 and renders drums on
core 0. With no active synth it submits no job. RemoteFM1 remains single-core.
No persistent state, voice limit, sample-rate or sound-model change is included.
The main render remains a reference fallback when the worker is unavailable.

## Implemented platform controls and verification gates

- Dedicated core-1 stack, with a linker-enforced size and static call-stack
  audit. The previously unused RAM-run debug region 0x01C06000–0x01C07EFF
  now holds 7,680 bytes of user stack (including the low 256-byte guard) and
  256 bytes of exception stack. 0x01C07F00 boot status remains untouched.
  Existing main/audio stacks and guard bands are not reduced. RAM-run debug
  cannot use that region concurrently with this trial.
- Halt core 1 before RAM clearing on every boot, and on every reset/recovery
  or updater path. Existing stock recovery and boot-area protection remain.
- RAM-resident idle worker, separate from the flash driver's call-free RAM
  section. Audio must join all jobs before returning: saves disable IRQs and
  can then turn off XIP only while the worker is idle in RAM.
- Exact package verification of worker load/copy extents and RAM PC window;
  retain the existing stack, write and bus guards. Skiff bootstraps through
  ROM with a briefly open PC window, which must be restored after a bounded
  startup handshake, including failure paths. Startup is bounded at 20 ms;
  audio job joins time out at 2 ms and hold the core until the next boot.
- Per-writer cache-line mailbox with checksummed function/argument/sequence,
  explicit cache barriers, completion validation and timeout. Validate job
  frame/slot bounds as well. A checksum is not proof of reliable DSP reads.
- On timeout or corrupt job, halt the worker before touching its state.
  Discard incomplete sums and retire affected synth tails before resuming
  single-core rendering. Do not rerender already-mutated state as if no work
  occurred. Project, patterns and saved parameters must survive the fallback.
- Visible worker status and failure reason, plus an explicit opt-in trial.
  A failed worker is not restarted during playback. GLO+EDIT CPU PROFILE shows
  C1 ON, C1 OFF, C1 BOOT FAIL, C1 BAD JOB, C1 TIMEOUT or C1 BAD RESULT.
  On a job failure, the current partial synth block is discarded and its
  voices marked inactive; drums continue. New synth hits use core 0. This is
  a recovery path, not a promise that single-core overload becomes playable.
- Host protocol fault-injection tests, including stale/duplicate/corrupt jobs,
  no completion and partial DSP progress; flash-idle and reset invariants.
  `tests/dualcore.c` exercises the actual renderer hook plus shared mailbox
  validation, startup fallback, corrupt arguments, duplicate/skipped jobs,
  sequence/timer wrap, partial-work retirement and disabled-worker behavior.
  ASan/UBSan and a separate ThreadSanitizer run pass.
- Profile accounting must remain per-writer; current lane timers are disjoint
  for the drum/synth split. A future voice-balanced split needs private timer
  sums before merging, since two cores may otherwise update the same lane.
- Independent native builds, stack/section checks, package update fault tests,
  then user hardware timing with the exact same sequence and quality setting.

A fixed drum/synth split is a useful first boundary, not a headroom guarantee.
BAL 2X has considerably more Glitch work than ECO; both cores share caches
and flash bandwidth. Hardware measurements may require balancing voice work
between cores. Do not extrapolate two-times speedup or sum independent peak
percentages. No Hub-ready or hardware-stable claim until actual playback,
FX headroom, saving, power cycling and leaving the firmware are verified.


## Candidate 0.17.18 checks

Source d8b9ab5. Native packaging run 43677063177 and independent diagnostic
run 43677065467 reproduce SHA-256
`8de55220aea66480e1e6b7e3e0cc5f7119b82b36f0f021dfdef4ed4b35f1d9e8`.
Code 412/567 KiB, normal RAM 90/96 KiB and pool 334/336 KiB.
Worker idle text occupies 0x01C00B20–0x01C00BBF, following the isolated
call-free flash driver. Its only call is the validated indirect job call;
voice models remain in XIP. The exact packaged load bytes and word-copy map
are checked by `tests/dualcore-image.py` against the diagnostic ELF.

Conservative direct-call stack review: core-1 job 912 bytes plus 128-byte
worker allowance, well below the 7,424-byte usable user stack. Audio IRQ
3,556 plus timer 336 and context allowance 256 is below 7,936; foreground
10,984 below 24,320. This is static review, not measured stack high-water.
Full local regression, exact PCM/state concurrency and fault-injection tests,
ThreadSanitizer and RemoteFM1 import/integration pass. Native CI and a local run against this exact package pass
updater fault tests (wrong key/CRC, disconnect, power cuts and boot-area bounds). No physical performance result is claimed.

Start testing with SYN QUAL = ECO 1X set while stopped, full stock sequence,
no punch effects. GLO+EDIT should show C1 ON. Record TOTAL and stage peaks
before comparing BAL 2X. If status reports a failure, stop and report the
status; repeated flashing cannot establish whether that unit's core is reliable.


## Balanced lane trial 0.17.19

0.17.18 physical captures show C1 ON with TOTAL 143% at BAL 2X and 101%
in the ECO follow-up. The fixed drum/synth split helps but does not establish
headroom. Version 0.17.19 estimates cost from active voices and their actual
quality, then greedily assigns whole lanes across cores every render block.
It uses eight bounded selection passes, fixed scratch and integer weights.
These are approximate scheduling weights derived from the hardware profiles,
not continuously measured CPU cost. No voice model or polyphony reduction.

All siblings of a lane stay together so profile accumulation still has one
writer per lane during each job. The worker lane mask participates in the
job checksum and is bounded to eight lanes. A failed job retires only its
assigned lanes after holding the worker, discards their incomplete sums,
and keeps the other core's completed lanes. This now applies to drum lanes
as well as synth lanes. No restart is attempted in the audio path. When the
worker is disabled, no scheduling work is performed.

Tests cover all 255 nonempty active-lane combinations at varying frame
boundaries, all qualities, chords, exact PCM and complete state, worker/start
failure and partial-work retirement. ASan/UBSan and ThreadSanitizer pass.
Full regression and RemoteFM1 import/integration also pass.

Source 762650e. Native CI 43678065692 and independent diagnostic 43678067390
reproduce SHA-256
`2ae5c9d7685a6db8e0166ba11a28fedcc0476ed511511a10a9eb0511452f4839`.
Code 412/567 KiB, RAM 90/96 KiB, pool 334/336 KiB. Conservative stack:
worker job 932 plus worker loop allowance 128, below 7,424 usable bytes;
audio IRQ 3,596 plus timer 336 and context allowance 256 below 7,936;
foreground 10,984 below 24,320. Stack regions, guards, bootstrap, clock and
save format are unchanged from 0.17.18. Exact-package image and local updater
fault checks pass. The iCloud copy was SHA-256 verified. Physical improvement over
the 101% ECO result remains to be measured.
