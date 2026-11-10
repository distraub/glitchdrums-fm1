# Performance FX controls — 0.13.0

0.15.0 adds eight project-level delay sends, with MIDI Learn. Complete save:
**12,268 / 12,276 bytes**, eight bytes free. See [Delay sends](DELAY_SENDS.md).
The earlier release/storage details below describe their respective schemas.

Press FX. All 27 keyboard keys trigger fixed effects; the three Kill effects
are omitted. The first three black keys are AntiVerb, Glitchstortion and Grain
Cloud. SELECT or ALGORITHM chooses an effect; EDIT opens its four controls.
ENV toggles hold/latch mode, SEL clears latches. All 27 parameter sets persist
in the project and custom kits/scenes. Routing, capture priority and DSP buffers
remain unchanged. More simultaneous effects can consume more CPU; this restores
access, not a guarantee that every combination meets the audio deadline.
The global punch MIX controls the whole performance layer.

Master FX pages: Delay Mix / DLY SYNC / Feedback / Tone; then Cutoff /
Resonance / Drive / Bits; then Hold / Roll Rate / Roll Mix / Width.
Fresh projects start with Delay Mix 0. Existing saved mixes are retained.
Delay time follows BPM and displays musical divisions. Divisions exceeding the
fixed delay buffer are halved until they fit; the displayed fraction is the
actual division used. No extra delay memory is allocated.

| Effect | KN1 | KN2 | KN3 | KN4 |
|---|---|---|---|---|
| Capture rolls (first five keys) | Size | Speed | Seam | Blend |
| Reverse / Half Speed | Size | Speed | Seam | Blend |
| Tape Stop | Size | Brake time | Curve | Blend |
| LP Sweep / HP Sweep | Time | Start frequency | End frequency | Blend |
| Phone | Low cutoff | High cutoff | Gain | Blend |
| Crush | Bits | Tone | Blend | Level |
| Sample Alias | Rate | Grit | Tone | Blend |
| Stutter Gate | Synced rate | Width | Fade | Level |
| Echo Throw | Amount | Synced time | Tone | Stereo width |
| Wobble | Rate | Depth | Regen | Blend |
| Wave Fold | Fold | Trim | Bias | Blend |
| Ring Mod | Rate | Depth | Waveform | Level |
| Flanger | Rate | Depth | Regen | Blend |
| Auto Pan | Synced rate | Depth | Phase | Level |
| Comb Freeze | Pitch | Freeze | Damping | Blend |
| Pump | Synced rate | Depth | Curve | Level |
| Rise / Fall | Time | Start frequency | End frequency | Blend |
| AntiVerb | Size | Smear | Squawk | Decay |
| Glitchstortion | Color | Grit | Drive | Tone |
| Grain Cloud | Size | Density | Spray | Rate |

All 27 available processors expose four controls. The display shows physical units where
appropriate; the native regression exercises all 108 available parameter endpoints.
Wobble and Flanger have independent stereo delay buffers (16 KiB combined), so
freezing the shared capture tape cannot freeze their modulation. Parameter edits
retain running processor state. Capture effects still share one tape and follow
the existing capture-priority behavior when stacked.

Project schema 8 initializes formerly unavailable parameters in older projects
and populated kit/scene slots. Previously editable values, sounds, patterns and
signature-effect tuning remain intact. No persistent structure grows. Render
state uses fixed storage; the audio path performs no allocation or file access.

## Momentary global microloop — 0.16.0

Cycle FX to master page 3 (Hold Frames / Roll Rate / Roll Mix / Width).
Hold any white/bottom-row key to capture and repeat the current short slice.
KN2 changes its rate live (1/32–1/256); KN3 adjusts the roll/live mix. Release
all white keys to bypass the loop and resume recording history. Leaving the
page also releases it. Multiple held white keys behave as one hold gate.
Black keys retain normal voice playing; MIDI note routing is unchanged.

Rate now arms the microloop instead of engaging a continuous global roll.
The first key press changes OFF Rate to 1/32 and zero Mix to 100%, making an
otherwise silent/unconfigured punch audible. Nonzero user settings are retained.
Rate and mix remain saved parameters, but the hold gate is transient and never
restored as active. Hold Frames and Width retain their existing meanings.

This uses the existing 4,096-sample mono roll buffer, with no new processor,
per-track buffer, preset field or save-space use. Existing performance-FX key
assignments remain unchanged. Native tests verify armed bypass, capture, live
rate changes without recapture and release bypass; WASM checks page controls.

## Sweeping microloop rate — 0.16.1

KN2 now has 127 logarithmically spaced rate positions instead of four divisions.
The nominal range follows tempo from 1/32 through 1/1024, including intermediate
values; its readout shows the actual repetition frequency in Hz. OFF remains
position zero. The 4,096-frame capture buffer limits the slowest repetition to
about 10.77 Hz at low tempos; the readout reflects that limit.

Loop length glides between targets per sample, with fractional position reads
and a short end seam blend. Changing rate retains a fixed captured-end anchor;
it does not recapture history. Coefficients are cached until rate/BPM changes.
No additional audio buffer or persistent bytes. Active state adds four scalars.
Schema 6 uses the same save layout/size; old four-position project/custom
kit/scene rates migrate to matching musical detents (0, 1, 26, 51, 77).

Native ASan/UBSan tests sweep all rates both directions, check bounded read
positions and fixed capture, and verify release bypass. WASM integration checks
legacy migration, the expanded knob range and saved settings.

Host wasm3 sequencer plus held fastest roll: 1.001 audio seconds in .201 CPU
seconds; maximum measured audio block 1.580 ms. Hardware validation is still
separate. Readout uses compact Hz values so the large value font stays usable.

## Glitch naming and live routing — 0.17.0

The global Roll controls are now labelled GLITCH RATE and GLITCH MIX. The
capture/gating behavior remains the same. The new fourth SEQ page combines
these controls with temporary source-step routes; see [SLICE](REMIX.md).

## Glitch keyboard sizes — 0.17.1

On SLICE or master Glitch page 3, white keys choose sixteen large-to-small chunk
sizes. KN2 offsets the whole bank; the SHIFT value is shown in the footer/hint.
Last-pressed held key wins, falling back to the previous held key on release.
Changing held keys updates the smoothed rate without recapturing audio. Release
all keys to punch out. No additional audio buffer or persistent storage is used.
