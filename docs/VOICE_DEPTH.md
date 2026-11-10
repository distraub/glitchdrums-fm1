# Voice depth and fidelity — 0.9.0

All eight existing FM-1 lanes now have app-derived DSP paths and INIT references.
These are integer interpretations of the actual local app source, not imported
samples. Controls are quantized to 7-bit detents; INIT detents are calibrated to
the source values. A close isolated INIT comparison does not prove identical
arbitrary presets, automation, stereo mixing, master effects or hardware timing.

| Voice | What is implemented | Exposed controls |
| --- | --- | --- |
| Kick | App click/color/tone filters, pitch/hold, punch, body shaping/curve and saturation | 16 sound + level/pan/transpose |
| Snare | Pitched body, ringing overtone, independent body/wire tails and attack, color/filter, saturation | 16 sound + mixer, including extra body/wire trims and cutoff |
| Closed hat | Six-oscillator metallic hash, density, metal/noise blend and separate tails, bite, color/envelope | 15 sound + mixer; choke belongs to the open hat |
| Open hat | Same metallic architecture with open-tail timing, adjustable choke | 16 sound + mixer |
| Clap | Flam scheduler, burst count/decay, separate tail, tuning-dependent filter, smack band/envelope | 15 sound + mixer; one unused cell |
| Tom | Bend/decay, snap/noise, nonlinear body, laser, punch and low/high type | 16 sound + mixer |
| Glitch | Original generated material tables, dual loop/saw/sine sources, frozen/fresh noise, fold/drive, ladder, envelopes and FIR decimation | 29 core sound + mixer; 1×/2×/4× quality and 1–4 notes globally |
| Analog | Dual VCO with through-zero FM, fine, noise/color, four-stage nonlinear ladder, filter drive/EG/FM, separate attack/curve and VCA | 29 sound/mode controls + mixer, including held-note sustain/release and EG→noise |

## App comparison

The reference harness compiles the actual app `DrumVoice.hpp`, not a formula
reconstruction. Native firmware rendering uses its actual integer `engine.h`.
Both use 44.1 kHz, the same seed/velocity/pitch, and no samples, modulation matrix,
channel strip or master FX. Only the firmware's fixed 1/3 summing headroom is
removed; there is no fitted gain or individual normalization. Correlation and
RMS gain use the first 500 ms. Acoustic presence measures a 1.5 kHz high-pass in
the first 200 ms. Kick presence uses the first 50 ms.

Representative INIT results at velocity 110 and zero step pitch:

| Voice | Waveform correlation | Firmware/app RMS gain |
| --- | ---: | ---: |
| Kick | 0.99997 | 0.9993 |
| Snare | 0.999998 | 0.9990 |
| Closed hat | 0.999999 | 0.9993 |
| Open hat | 0.999742 | 0.9991 |
| Clap | 0.999997 | 0.9992 |
| Low tom | 0.999999 | 0.9992 |
| Glitch, High 4× | >0.9998 | within 0.4% |
| Analog | 0.999735 | 0.9993 |

Additional tests cover lower velocity and +12 semitones. The octave-up closed
hat is the least sample-correlated acoustic case (~0.9919); its RMS/presence
levels remain within 0.3%. Square-bank quantization makes individual edge samples
sensitive at high pitches. All tested other acoustic cases exceed 0.999.
Synth cases exceed 0.999 in High 4× mode. These figures describe tested INITs,
not every possible knob combination. Full-range controls also have sanitizer,
bounded-output and audio-response tests.

Reproduce from `Firmware/FM1`:

- `python3 tests/kick-compare.py`
- `python3 tests/voice-compare.py`
- `python3 tests/synth-compare.py`
- `sh tests/run.sh`
- `node tests/remote.mjs`
- `node tests/fx-listen.mjs`

JSON metrics and paired WAVs go into `build/test-release`. Each voice WAV plays
**app first, then firmware**, with a short silence between them. Glitch's reference
clip uses High 4×. Balanced/Eco use fewer oversampling/noise updates and are not
sample-identical to that reference.

## Controls and performance

EDIT opens the scope, EDIT again the scrolling rows. ALGORITHM chooses a row;
KN1–KN4 edit its values. All acoustic voices have five rows; Analog and Glitch
have eight. The final row includes mixer controls. ENV jumps to envelope controls.
The SEQ → EDIT P-lock workflow remains available.

GLO cycles GLOBAL and PERFORMANCE; ALGORITHM also chooses these two pages.
PERFORMANCE KN1: Balanced 2× / High 4× / Eco 1× Glitch rendering.
KN2: 1–4 Glitch notes. Default is Balanced 2× with two Glitch notes; Analog
remains four-note polyphonic. Quality/limit changes affect new notes; existing
tails retain their adopted quality and are not reset. The profile persists with
the project and is not changed by voice/kit/FX recalls.

A wasm3 host stress run with 14 requested notes (limited to two Glitch notes by
the default profile) rendered 1.001 seconds in ~0.698 CPU seconds (~1.43× real time).
The full High/four-Glitch case was slower than real time on that host. This is
not an iPad or hardware performance guarantee. Actual FM-1 CPU/redraw headroom
and firmware packaging remain unvalidated; only the RemoteFM1 module is built.

## Remaining differences and limits

- Glitch has finite one-shots; the app's held MIDI sustain/release and full routing
  matrix are not exposed. The core 29 controls are present; shared tune uses the
  firmware's transpose control. Live parameter smoothing differs from the app.
- Analog HOLD mode enables held keyboard/MIDI notes. Sustain/release apply in
  that mode; note-off and STOP release them. Sequencer hits remain one-shots.
  The full eight-leg app routing matrix is not exposed; EG→noise, pitch→VCF and the FM-1's
  two LFOs are available. Arbitrary app routing patches are not equivalent.
- Tom TYPE switches low/high physical roots; there is still one tom lane.
- Open-hat CHOKE zero preserves this firmware's immediate choke; positive values
  fade the open hat over 0–200 ms when closed hat plays. This differs from the
  app's zero-value choke convention. A silent closed hat excluded by solo does
  not choke the audible open hat.
- No separate percussion model/lane, sample import, per-pad EQ/compression or
  app master-chain equivalence was added in this pass.
- Only INIT slots are app references. Other factory recipes remain provisional
  until the user chooses the app presets to translate.

Private version-6 saves upgrade to version 7 by resetting lanes 2–8 to their new
INIT control maps and setting the default performance profile. Patterns, locks,
lengths, kick, tempo, FX and theme are preserved. Old user sounds for those lanes
are cleared; their kit sounds reset to INIT. Project/library storage remains
8544 bytes. DSP histories are fixed allocation and never saved.
