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
