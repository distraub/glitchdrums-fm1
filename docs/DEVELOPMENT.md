> Historical development notes. For current hardware packaging status, see [PREHARDWARE.md](PREHARDWARE.md) and [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).

# Firmware research and development

## References examined

- [Skiff v2.2](https://github.com/illuminus5/skiff/tree/v2.2): MIDI Learn workflow
  and block/voice/tail performance review; see [MIDI](MIDI.md) and
  [performance findings](SKIFF_PERFORMANCE.md). Independent implementation;
  Skiff engine code was not copied.

- [X0X manual](https://charlesvestal.github.io/fm1-x0x/manual/): white-key steps,
  black-key tracks, four controls per page, per-step probability/locks,
  64-step paging, muted selection while running, explicit save and undo.
- [SLOOP guide](https://github.com/isod89/sloop-fm1/blob/main/GUIDE.md): live
  recording, multitrack groovebox composition, modulation, effects and fills.
- [Felucca](https://github.com/hugelton/Felucca): separate engine/hardware layers,
  flash-revision support and resource-conscious synthesis.
- [RemoteFM1 format](https://github.com/Quixotic7/RemoteFM1IOS/blob/main/docs/RFM1-FORMAT.md):
  ABI 6, fixed WASM memory and interpreter feature limits.

These manuals informed interaction patterns; their voice/sequencer application
code was not copied. The existing foma hardware layer is retained. GlitchDrums'
GlitchParameters.hpp, GlitchVoice.hpp, ModulationTarget.hpp, AnalogEditorPanel.swift
and GlitchDrumsTheme.swift supplied the sound vocabulary, original synthetic
tape formulas and exact palette values.

## 0.1 architecture

- `src/engine.h`: eight fixed voices, offline integer tables, two LFOs per lane.
- `src/main.c`: editor state, SPSC command queue, render-owned project, sequencer,
  parameter locks, live note recording and bounded redraws.
- `src/fx.h`: fixed delay/roll buffers; no live buffer reallocation or clearing.
- `src/controls.h`: page metadata shared by validation and UI.
- `src/themes.h`: 38 RGB565 palettes generated from the app source.
- `remote/adapter.c`: RemoteFM1 exports, panel mapping and framed settings image.

The main loop owns editor changes. Audio adopts complete 32-bit commands at
block boundaries, owns voices/clock/LFOs/effects, and publishes playhead/silence.
Saving occurs on the main loop after transport and tails stop. LFO processing
is at 32-sample cadence; sound controls do not rebuild voices or effects.

## Remaining work toward a full release

1. Hardware `.fwsc` build, resource-map checks and audio ISR timing. Verify both
   flash revisions and updates/recovery when the FM-1 arrives.
2. External MIDI clock with proper event timing, MIDI note output and USB audio.
3. Pattern banks, quantized pattern changes, chaining/song mode and undo.
4. More than one parameter lock per step, knob automation and microtiming.
5. Per-lane FX sends, reverb, selectable FX slots and more performance gestures.
6. Deeper Glitch/Analog routing, presets and comparison renders against the app.
7. Anti-aliasing/voice quality improvements within measured CPU headroom.
8. Optional RemoteFM1 host skin/catalog metadata. Firmware themes currently
   change the LCD only; the host's surrounding panel skin is its own setting.

## Validation boundary

Native sanitizer, parameter-response and WASM/interpreter checks pass. The
0.1 build was imported on remotefm1.com; voice, LFO, FX, transport and light/dark
theme pages were inspected there. iOS app and physical FM-1 testing remain.

## 0.3 signature effects

`src/signature.h` implements reduced fixed-point versions of the musical
models in `GBAntiVerbEffect.hpp` (including GlitchstortionEffect and the spatial
wrapper) and `GBGrainCloudEffect.hpp`. It uses 8192 int16 capture samples,
three independent four-fragment engines, and 2048 reflection samples per
engine. Pitch reads interpolate in fixed point; windows and overlap scaling
bound fragment sums. Gate epochs bound reads to fresh input without clearing
the whole capture. Reflection state resets on a new press. UI character edits
never rebuild buffers or reset a playing capture.

Punch mask bits 27–29 hold the signatures; bit 31 is reserved for urgent clear.
The bank selects the mapping of physical keys only; timing stays in audio.
Tests explicitly cover reopen-on-silence and uint32 capture-index rollover.
Host WASM uses fixed 8 MiB memory. Added signature storage requires a new
hardware linker-map audit; do not infer hardware fit from a RemoteFM1 pass.

## 0.4 allocation and persistence

The engine has 14 fixed voice slots: six drum voices, four Glitch and four
Analog. The first eight retain their historical indices; slots 8–10 extend
Glitch, 11–13 extend Analog. Allocation prefers an inactive slot, then the
quietest envelope and oldest age. Render/prepare map each slot back to its lane.
No heap allocation or dynamic voice containers enter audio.

Project version 4 appends 120 bytes of effect parameters after the old v3
prefix. Version 3 loads that prefix; version 2 also supplies punch defaults.
A C_EFFECT command updates one byte at an audio-block boundary. Gate bits and
latch mode are UI performance state and are never serialized. Bit 30 of the
punch request forces immediate adoption on navigation; bit 31 remains clear-all.
STOP clears latch state and pending audio gates. Editor navigation preserves
latched gates while discarding momentary held keys.

## 0.5 bank persistence and visual timing

`src/presets.h` separates sound (40 bytes), FX scene (136 bytes), kit (464
bytes) and library storage from the render project. GDSave wraps GDProject
and the user bank with magic/version/project-size metadata; sizeof is 8544
bytes, checked against SETTINGS_MAX. The UI edits storage.project directly,
avoiding a second whole-library copy. The audio thread receives no bank data.

A recall copies a bounded GDKit into one of two inactive mailboxes, publishes
a C_RECALL command, then adopts the relevant sound bytes at an audio-block
boundary. Busy mailboxes are never overwritten. Compiler memory barriers
surround publication/consumption under the existing single-core IRQ contract.
The third pending recall is refused until audio acknowledges a mailbox. No
allocation, file operation or DSP-buffer reset occurs in the callback.

The waveform and orbital pages poll audio-published phase/value cells. Their
20 Hz refresh only requests drawing. Random output history and orbit trails
are fixed UI buffers; they do not feed LFO parameters back into audio. Tests
also verify that an advancing UI clock cannot move a waveform without audio.

## Mixer / scope monitors (0.7.0)

Audio sums `GDVoice.last_sample` for all 14 fixed slots into eight mono lanes
every 16 samples, applies lane mute/solo visibility, captures the UI-selected
lane into a 128 × int16 ring and accumulates per-block lane peaks. The display
reads scalar levels and a bounded local ring copy; no display function runs
in audio. The selected-lane scalar and ring/write counter follow the same
single-core IRQ / serialized RemoteFM1 contract as the existing LFO monitors.
This is not a portable multicore seqlock; a multicore port must supply a proper
snapshot protocol. A possible IRQ overlap can tear a visual frame but cannot
change rendering state or access outside the masked ring.

SCOPE and ROWS edit the same row/parameters. UI navigation only changes the
view; it does not reset voice, oscillator, filter, FX, or LFO state. Mixer LEVELS
uses one C_PARAM command per fader; TRACK retains the earlier lane controls.

## Mutation history and navigation (0.7.1)

UI and audio each own a fixed 8 × 64 byte mutation snapshot. C_MUTATE snapshots
each side before applying the deterministic mutation; C_MUTATE_SWAP exchanges
the snapshot and step-velocity arrays in the next audio block. UI changes history
only after enqueue succeeds. Manual C_STEP commands invalidate history on each
side. No parameter/FX/envelope state is copied or reset; one swap is 512 bounded
iterations. Navigation memory is UI-only and transient, with per-lane voice and
LFO state. ENV is a tap-on-release control whose recognized combinations consume
the tap to avoid an extra screen/latch action. FX status reads actual gate state
separately from requested latches and physical keys.

## Kick fidelity regression (0.8.1)

`tests/kick-compare.py` builds host-only C++ against the actual app DrumVoice.hpp
and C against engine.h. The INIT source values are taken from KICK_INIT.json,
which remote.mjs checks against Swift model defaults. Four pitch/velocity/seed
cases are compared without per-render gain fitting. App/reference uses real
float DSP; target uses only fixed-point tables and integers. Generated
kick_dsp_tables.h holds the tanh/sweep/drive/filter coefficients. Target render
uses fixed existing voice state; body progress uses a trigger-cached reciprocal
to avoid per-sample division by decay time. No target audio buffers or heap
allocations were added. Hardware cost still needs profiling.

## Voice fidelity engines (0.9.0)

Acoustic state is fixed in GDEngine.acoustic; polyphonic synth state uses eight
fixed GDSynth records corresponding to physical voice slots 6–13. Source tables
and coefficients are generated offline from local app INIT definitions and
GlitchSourceTables. Target render contains integer math only. Continuous
configuration preserves envelopes/oscillator/filter histories; hit-latched
envelopes are adopted on a new hit. LFO values still compose on the audio side.

Glitch's three profiles use the same output-rate FIR latency: 33/65/129 taps
at 1×/2×/4×, with coefficient tables at the matching internal rate. Quality is
latched per voice; reducing the voice limit retains existing tails and affects
new allocations. The project reserved byte stores quality in bits 0–1
(0 Balanced, 1 High, 2 Eco) and Glitch voice count in bits 2–3 (0 means 4,
otherwise 1–3). Default 8 means Balanced/two voices. Preset scopes exclude it.

Analog held gates are optional; note-off identity uses the original voice's
pitch offset, so later transpose edits cannot strand a note. Physical-key lane/
pitch identities are captured on press. MIDI Note Off and velocity-zero Note On
release matching held voices; STOP releases all. Sequencer triggers explicitly
disable held mode. Each audio-side action remains one queued scalar command.

The source package includes generated tables; rebuilding the target needs no
C++ runtime. Regenerating/auditing tables uses tools/generate_acoustic.py and
tools/generate_synth.py against this repository's app source. Comparison tests
compile host-only C++ references and C target paths; they are not app-host
integration or device timing tests. Project/library size remains 8544 bytes.
