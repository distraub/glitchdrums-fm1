# Second-core investigation

Status: host renderer partitioning verified; hardware integration not enabled.
No new installable performance candidate is produced by this investigation.

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

The production path still renders one partition on core 0. Its sample/state
parity tests remain required. This refactoring adds no persistent state,
voice limit, sound-model change or hardware-register access.

## Required platform work before a hardware trial

- Dedicated core-1 stack, with a linker-enforced size and static call-stack
  audit. Current main RAM is already about 90/96 KiB; a 4 KiB worker stack,
  256-byte exception stack, mailbox and scratch sums are a tight fit.
- Halt core 1 before RAM clearing on every boot, and on every reset/recovery
  or updater path. Existing stock recovery and boot-area protection remain.
- RAM-resident idle worker, separate from the flash driver's call-free RAM
  section. Audio must join all jobs before returning: saves disable IRQs and
  can then turn off XIP only while the worker is idle in RAM.
- Exact package verification of worker load/copy extents and RAM PC window;
  retain the existing stack, write and bus guards. Skiff bootstraps through
  ROM with a briefly open PC window, which must be restored after a bounded
  startup handshake, including failure paths.
- Per-writer cache-line mailbox with checksummed function/argument/sequence,
  explicit cache barriers, completion validation and timeout. Validate job
  frame/slot bounds as well. A checksum is not proof of reliable DSP reads.
- On timeout or corrupt job, halt the worker before touching its state.
  Discard incomplete sums and reset affected synth runtime before resuming
  single-core rendering. Do not rerender already-mutated state as if no work
  occurred. Project, patterns and saved parameters must survive the fallback.
- Visible worker status and failure reason, plus an explicit opt-in trial.
  A failed worker must not be silently restarted repeatedly during playback.
- Host protocol fault-injection tests, including stale/duplicate/corrupt jobs,
  no completion and partial DSP progress; flash-idle and reset invariants.
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
