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
