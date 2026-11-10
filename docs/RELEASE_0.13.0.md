# 0.13.0 — full keyboard FX and master controls

Restores 27 fixed performance effects across the complete keyboard. Omits the
three band Kill effects. First three black keys: AntiVerb, Glitchstortion,
Grain Cloud. SELECT/ALGORITHM selects; EDIT exposes four controls; ENV toggles
latching; SEL clears latches. MIDI mappings pin to effect identity. Old four-pad
parameter/gate mappings migrate to their original effects.

Lossless fixed-width packing stores all voice parameters, LFOs and all 27 FX
parameter sets. Complete schema-4 save: 12,260 / 12,276 bytes. No reduction to
sixteen patterns, chain, chords, p-locks, MIDI maps or custom preset capacity.

Fresh Delay Mix is zero; saved mixes retain their values. DLY SYNC shows the
actual BPM-relative fraction. Times longer than the existing 32,768-frame
buffer halve until they fit, rather than clipping to an off-grid time. Master
page two pairs cutoff/resonance on KN1/KN2, then drive/bit depth on KN3/KN4.

Validation: native ASan/UBSan suites; ABI-6 WASM integration; dense save and
legacy migration; every FX control persistence; fresh/saved delay mix; filter
pair controls; default gain -21.41 dBFS RMS without rail samples; waveform parity
against the earlier baseline with delay disabled on both; FX level/DC regression.
LCD delay, filter and full-keyboard pages rendered and inspected.

Host wasm3 CPU seconds per audio second: idle .051; demo .196; chain .340.
Maximum measured audio blocks: .196 / 1.386 / 1.676 ms respectively. These are
host measurements, not FM-1 hardware certification. Existing worst-case fourteen
voices plus three signature effects: .924 CPU sec/audio sec, maximum 3.553 ms,
which exceeds the 128-frame deadline in some blocks. No new FX audio buffers;
more simultaneously enabled effects can still increase CPU cost.
