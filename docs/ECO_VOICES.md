# ECO voice trial — 0.17.17

Select **SYN QUAL → ECO 1X** on the second Global page while stopped, before
triggering notes. The choice applies on the next note hit. BAL 2X / HIGH 4X
retain the reference synth models and their exact tested PCM. These rate labels
refer to Glitch; Analog stays at 44.1 kHz in every mode.

ECO changes the sound deliberately to reduce the measured synth workload:

- Both synths use a bounded nonlinear two-pole filter instead of the reference
  four-pole ladder, with Q15 coefficients and native 32-bit normalization.
  Cutoff/resonance and audio-rate oscillator/filter modulation remain available,
  but the slope and resonance character differ.
- Glitch updates envelope-derived loop geometry and tuning every eight samples
  (about 0.18 ms), refreshing immediately after parameter reconfiguration.
  Oscillators, noise, amplitude envelopes and filter still run each output sample.
- Glitch's 1X path replaces its unnecessary downsampling convolution with the
  former 16-frame group delay, and uses a precomputed frozen-noise color cache.
  High-frequency/aliasing texture can differ. BAL/HIGH retain their FIR paths.

Output stays at 44.1 kHz. No parameter, polyphony, pattern, p-lock, FX or save
capacity is removed. Kick, snare, hats, clap and PRC retain identical audio. Hat metal oscillators
now compute the exact interpolated-sine sign directly, removing full sine
lookup/interpolation when only a square-wave sign was needed. An exhaustive
test covers all 8,388,608 interpolation inputs and phase wrap. All RAM/code,
stack, updater and PC guards use the reference layout; the failed RAM-code
placement experiment remains reverted.

Two exact integer identities also replace wide division in synth pitch ratios
and Glitch saturation. Tests check them against 64-bit reference calculations;
these changes do not alter their mathematical results.

## Checks and previews

The original three-quality PCM vectors still pass with ECO adaptation disabled.
With the feature enabled, every BAL/HIGH vector remains bit-identical; ECO
changes are tested explicitly. Sanitizers cover legal randomized controls,
cutoff/resonance extremes, and comparison of static-pitch phase/envelope timing.
Block/sample state parity and the RemoteFM1 integration suite pass.

`tools/render_eco_preview.c` renders four-second reference/ECO comparisons with
the same notes and factory controls. Both Glitch files receive the same 16x
listening gain; both Analog files receive 6x. This gain is only in the preview
writer, never the firmware. ECO/reference RMS ratios are about -2.3 dB for
Glitch and -0.5 dB for Analog; neither preview clips. Analog onset is identical;
Glitch onset differs by one sample in this preview, with the original group
delay retained. These checks do not establish native CPU headroom.

The prior full-stock BAL 2X device profile peaked at 198%, with independent
Glitch 75% and Analog 49% lane maxima. Measure the ECO path on the same unit
using GLO+EDIT, play, STOP and retained profile peaks before calling it stable.
