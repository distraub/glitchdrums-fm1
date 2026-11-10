# GlitchDrums FM1 — 0.17.7

By Alex Matheu.

An eight-lane drum synthesizer for the M-VAVE FM-1, currently testable as a
RemoteFM1 `.rfm1` and a local browser build. The same integer instrument code
targets foma's hardware platform. A hardware `.fwsc` has **not** been built or
validated yet; CPU, flash revisions, USB and recovery still need device checks.

## Release status and source

This repository contains the standalone FM-1 firmware, not the GlitchDrums iOS
application. Physical FM-1 validation is pending. A `.rfm1` file runs in
RemoteFM1; only a `.fwsc` file is a hardware update. Projects and samples from
other firmwares are not imported.

See [the release checklist](docs/RELEASE_CHECKLIST.md) before installing or
submitting to The Firmware Hub. Please report issues with firmware version,
RemoteFM1 or device model, reproduction steps, and audio-load/dropout readings.

## Build from this repository

Generated voice, preset and theme tables are checked in, so building the
instrument does **not** require the private GlitchDrums app sources. The
`generate_*` tools and app-comparison scripts are optional development tools;
some require a separate parent-app checkout and are not part of the build.

For RemoteFM1, install an official [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases)
and run from the repository root:

```sh
WASI_SDK_PATH=/path/to/wasi-sdk python3 tools/build_remote.py
sh tests/run.sh
node tests/remote.mjs
```

Output: `build/test-release/glitchdrums-0.17.7.rfm1`, its source archive and
checksums. Local builds use a local source archive URL. For a distributed build,
set `GD_SOURCE_URL` to the public immutable source URL for the exact commit:

```sh
GD_SOURCE_URL=https://github.com/distraub/glitchdrums-fm1/tree/COMMIT_SHA \
WASI_SDK_PATH=/path/to/wasi-sdk python3 tools/build_remote.py
```

For a native hardware package, run the **Native FM-1 package** GitHub workflow
manually. Workflow sources are in `.github/workflows/`. It obtains the JieLi compiler through the Hub's shared build workflow
and runs `python3 tools/build_native.py`, including the update loader and safety
checks. Alternatively set `JIELI_TOOLCHAIN` locally to that toolchain. Native
builds are experimental until the checklist is complete. The legacy `./fm1`
Docker helper requires the upstream foma Dockerfile/environment, not supplied
in this standalone snapshot; use the documented commands above.

## Try it

File: `build/test-release/glitchdrums-0.17.7.rfm1`.

- On https://remotefm1.com/ choose the firmware picker → **Open a file...**, select
  the `.rfm1`, then start it. Press the instrument's **PLAY** for the demo.
- In RemoteFM1 iOS use **Add firmware...** or open the file from Files/AirDrop.
- Importing runs the instrument on your computer/phone. This file is not a
  hardware update package. The actual iOS app has not been tested here.

0.17.0 adds [live SLICE routing](docs/REMIX.md), the second SEQ page.
Black keys punch source-step paths; white keys punch the audio Glitch buffer.
The stored pattern stays intact. Roll Rate/Mix are now Glitch Rate/Mix.

0.15.0 adds eight [shared-delay sends](docs/DELAY_SENDS.md). HOME cycles
LEVELS → TRACK → D SEND. ALGORITHM switches four-track banks; KN1–KN4
adjust sends. The shared filter/drive still processes dry voices plus delay.

0.14.0 replaces track 6 TOM with the app-inspired PERC engine and 20 controls.
See [Perc controls and validation](docs/PERC.md). Existing Tom sound bytes are
not translated: load PERC INIT (ENV + SEL in its voice preset browser) or a
new factory kit when resuming an old save.

0.13.1 fixes status-header redraw residue when switching OCT/MUTE/SOLO.

0.13.0 adds **16 channel-specific MIDI Learn mappings**. Open the controls you
want, press GLO to its third **MIDI LEARN** page, then SEL. Turn a firmware knob,
move a controller CC, and use OCT+ to finish. OCT− clears the picked mapping;
SAVE keeps it. Voice/LFO/mixer, master FX, performance FX controls, swing/tempo
and momentary FX pad gates can be learned. Standard CC7/CC10 control level/pan.
The complete save is **12,260 / 12,276 bytes**, retaining all patterns, chains,
chords, p-locks and custom presets. See [MIDI controls and scope](docs/MIDI.md).

The supplied Skiff v2.2 source was also reviewed. Candidate DSP optimizations
and the measured MIDI-screen improvement are recorded in
[the performance findings](docs/SKIFF_PERFORMANCE.md). DSP sound/timing is unchanged.

0.11.2 adds a **16-row pattern chain**, with the existing p-lock and chord capacity:

- Press SEQ until **CHAIN** (steps → patterns → chain).
- KN1 / SELECT chooses a row; KN2 / ALGORITHM chooses A–P; KN3 sets 1–16 repeats.
- Press **SEL** on the last wanted row to set the chain length.
- KN4 chooses **OFF / LOOP / ONCE**. With LOOP/ONCE enabled, PLAY starts row one.
- SAVE persists the chain. Row/length edits require stopped playback. MODE OFF
  works live; manual pattern selection exits the chain and retains the existing
  queued/double-tap behavior. Voice/FX controls remain live; sequence edits and
  recording pause while a chain runs.

The complete save is now **12,216 / 12,276 bytes**, leaving **60 bytes**.
Older 0.11.0/1 saves gain a default OFF chain without changing their contents.
The audio task uses the existing immutable decoded bank at loop boundaries;
there is no extra audio bank or bank serialization during playback.

0.11.1 keeps the current transport phase during a double-tap pattern switch.
New notes begin on their next scheduled step; the current step is not retriggered.
Single taps still queue to the loop boundary.

0.11.0 introduced the **sixteen-pattern instrument layout**:

- **16 patterns A–P, fixed at 16 steps.** SEQ twice opens the bank; all sixteen
  white keys select patterns. One tap queues to the boundary; double tap within
  300 ms switches immediately while preserving the current step and swing phase. ENV + SEL copies to the selected
  slot; ENV + ARP clears an inactive slot. SAVE stores the project and bank.
- **27 fixed performance FX keys.** SELECT/ALGORITHM selects an effect; EDIT
  opens its four controls. All keyboard keys punch/latch effects; ENV toggles
  latch mode. Kill Low/Mid/High are omitted. All 27 settings persist using
  lossless packing, preserving the complete pattern and preset capacity.
- **Master FX refinements.** Fresh Delay Mix is 0; Delay Time displays its
  tempo-synced division (long divisions halve to fit the existing buffer).
  Cutoff and Resonance are together on the second master FX page.
- **Editable factory presets.** In the preset browser SAVE stores a custom
  version at the selected entry; `*` identifies a replacement. ENV + SAVE
  restores the factory original or clears a user entry. ENV + SEL still recalls
  the original INIT voice. Custom records can target either factory or user
  entries: two per voice, one kit and one FX scene total. No silent eviction.
- **Guaranteed bank capacity.** Every pattern always takes 672 bytes, retaining
  all velocity, probability, ratchet, pitch, p-lock, lane-length and four-note
  Glitch/Analog chord data. The complete save is **12,196 / 12,276 bytes**, even
  at maximum density; the former bank-full limit is gone for new banks.

Older A–H patterns import into A–H. Additional bars become separate patterns
in I–P. If the old bank needs more than sixteen patterns, SAVE is blocked and
its original flash remains untouched; trim it in the prior firmware first.
The first two occupied user voice entries and first occupied kit/FX entry are
retained and renumbered into the smaller custom library.
All factory presets remain available. Chaining is available from 0.11.2.
See [the pattern/storage design](docs/PATTERNS.md) for import details and checks.

Previous development changes follow; their older layouts describe those versions.

0.10.11 adds **eight pattern slots A–H**, **double-tap launch**, and
**four-note recording on Glitch/Analog**:

- SEQ twice opens the bank. The first eight white keys select A–H.
- Tap once to queue for the full-pattern boundary; tap the same key twice
  within 300 ms to launch immediately from step one at the next audio block.
- SELECT/KN1 browse; SEL loads/queues. ENV + SEL copies the active pattern to
  the selected slot. ENV + ARP clears an inactive slot. SAVE stores the bank.
- The storage meter shows encoded bytes out of 3,712. A bank that exceeds the
  limit displays **BANK FULL - NOT SAVED**, keeping the last successful save.
  Clear or simplify inactive patterns to make room, then save again.
- With PLAY + REC on Glitch or Analog, keyboard/MIDI notes overdub up to four
  distinct pitches per step. They share step velocity, probability, ratchets
  and p-locks. The sequencer header shows N:1–4; clearing the step clears its
  chord. The other drum tracks retain their existing recording behavior.

Older A/B saves retain both patterns; C–H start blank. All eight patterns,
including chords, use lossless control-task serialization. No codec runs in
sample rendering. The old song arrangement engine remains isolated.

0.10.10 enables **live A/B switching**. Press SEQ twice, then tap the first
white key for A or the second for B. Or browse with SELECT/KN1 and press SEL.
While playing, the target shows QUEUED and becomes active at the end of the
current pattern (all of its pages). Stopped loads apply immediately; STOP
also applies an already queued selection. Sounds and FX stay live; sequence
edits pause while a switch is pending to protect pattern ownership. SAVE keeps
both patterns and the currently active slot. No save format changes.

0.10.9 adds two independent **A/B patterns** as the first step toward chaining.
Press **SEQ twice** to open the bank. Choose A/B with SELECT, KN1 or the first
two white keys; **SEL** loads the selection while stopped. **ENV + SEL** copies
the current pattern to the other selected slot; **SAVE** saves both patterns.
Press SEQ again to return to steps; edits stay with the active pattern.
A retains the existing sequence on upgrade and B starts blank. Sounds, FX,
LFOs, mixer, tempo and swing are shared; step lengths, pages, probability,
ratchets, pitches and p-locks belong to each pattern. Live switching is available from 0.10.10. See [profiling and design notes](docs/PATTERNS.md).

0.10.8 completes the control audit: every punch effect now has four editable
parameters. Flanger and Wobble use independent stereo modulation buffers,
including while a capture effect is held. See [the full control map](docs/PERFORMANCE_FX.md).
Older saves initialize only previously unavailable controls; existing editable
settings and the three signature effects retain their tuning.

0.10.7 expands five performance processors to four editable controls:

| Effect | KN1 | KN2 | KN3 | KN4 |
|---|---|---|---|---|
| Ring Mod | Rate (4–2020 Hz) | Depth | Sine/triangle/square | Level |
| Crush | Bits (15–5) | Tone | Blend | Level |
| Stutter Gate | Synced rate | Width | Fade | Level |
| Echo Throw | Amount | Synced time (743 ms maximum) | Tone | Stereo width |
| Auto Pan | Synced rate | Depth | Phase | Level |

Use FX → EDIT and SELECT the effect to tweak it, including while latched.
Each setting persists with SAVE. Ring's default output is about -2.4 dB
relative to its original full-wet level; LEVEL is adjustable from 0–150%.
Auto Pan uses constant-power panning, avoiding the previous 2x side gain.
New projects get calibrated defaults.
The signature-effect mix tuning and song-mode rollback remain intact.

0.10.6 adds a further approximately 15% signature-effect contribution over
0.10.5, with the same attack path, ducking and output limiting.

0.10.5 adds another 15% internal contribution to AntiVerb, Glitchstortion
and Grain Cloud (about 1.2 dB over 0.10.4), retaining the dry attack path and
output limiting.

0.10.4 raises the internal effect contribution of AntiVerb, Glitchstortion
and Grain Cloud by 25% (about 1.9 dB), including stacked ambience. The dry
path, transient ducking, timing, controls and global punch MIX are unchanged.
The final limiter remains in place. Sample Alias itself is unchanged.

0.10.3 adds editable Sample Alias RATE, GRIT, TONE and BLEND parameters.
The shared alias processor also colours Glitchstortion as GRIT is increased;
its clean attack anchor and COLOR/DRIVE/TONE controls remain. A fresh transient
refreshes the sample hold immediately. Set Sample Alias BLEND to 127 for its
full effect (new projects default to 127; saved settings retain their bytes).
Comb Freeze no longer overrides the independent Reverse/Half Speed/Tape Stop
capture length: they use a full quarter-note window and their own direction,
speed and stop ramp, while Comb Freeze retains its separate resonator buffer.
Song mode remains rolled back. Tests cover all alias controls, comb stacking,
capture length/speed/stop, attack alignment and the existing regressions.

0.10.2 rebalances AntiVerb, Glitchstortion and Grain Cloud for punch-in MIX
127. All three retain the full-rate, immediate dry beat. AntiVerb/Grain Cloud
are filtered, transient-ducked parallel returns; pitched/reverse fragments
no longer replace or subtract the dry drums. Grain Cloud spray is bounded to
20 ms. Glitchstortion is now a stereo current-signal shaper with COLOR, GRIT,
DRIVE and TONE, not a reverse tank; its distortion delta is DC filtered and
level compensated. Its four saved bytes remain valid with these new meanings.
The 22.05 kHz ambience engine and independent 44.1 kHz voices are retained.
Song mode remains rolled back. This is a musical adaptation, not DSP parity
with the app. Render tests cover dry-hit alignment, audible effect residual,
levels/DC, parameter extremes and the existing voice/save/preset regressions.

0.10.1 rolls back the experimental 0.10.0 song/pattern engine to the tested
0.9.6 sequencer. SEQ opens the step sequencer again. INIT remains F1, followed
by the first seven app presets, and startup selects Kick. Loading a 0.10.0
save retains its working sequence, sounds, FX, theme and user presets;
arrangement slots/chain are not loaded. The 0.10.0 source archive remains in
build/test-release for later diagnosis. No further effects changes are included.

0.9.6 restores INIT as F1 for every voice. F2–F8 contain the app's first
seven named presets in their original order. F1 recalls the exact complete
startup sound, including its deeper controls, level, pan and LFO defaults.
The ENV+SEL shortcut remains available. Voice 1/Kick is selected on startup.

0.9.5 replaces the eight placeholder factory voice slots per lane with the
app's first eight named factory sounds, in the original order (64 total).
Presets are exported from the compiled Swift catalog and quantized through the
same physical lookup tables as INIT. The TOM lane uses the Low Tom bank.
Browse with PRESETS and press SEL to load; hold ENV and press SEL in VOICE
PRESETS to recall INIT, which remains available independently of F1–F8.
ORIGINAL kit and startup defaults remain INIT. Fresh launches select Voice 1
(Kick) and the first mixer bank. Eight user voice slots remain.
See docs/FACTORY_VOICES.md for the complete names and mapping details.

0.9.4 optimizes AntiVerb, Glitchstortion and Grain Cloud: two overlapping
fragments per effect, 22.05 kHz wet processing with filtered capture and
full-rate reconstruction, division-free grain windows/positions, lookup-based
soft bounding and slower control-rate gain tracking. Dry audio and voices stay
at 44.1 kHz. Grain overlap uses fixed normalization to remove boundary gain
steps; capture startup blooms in smoothly. Wet DC rejection reduces low-frequency
bias. These adaptations sound darker and less dense; controls and saved settings
remain compatible. The combined wasm3 stress benchmark on this Mac improved
from a median 0.916 to 0.873 CPU seconds per 1.001 seconds of audio over three
runs (about 5% total CPU). This is not an iPad underrun or hardware measurement.

0.9.3 adds +18.06 dB clean output trim after MASTER and all FX, with a linked
stereo peak limiter (28000 PCM ceiling, about -1.4 dBFS, immediate attack and
46 ms release). Default voice levels and sound DSP remain unchanged. Remote,
local web and both hardware conversion paths share the same output stage;
hardware still requires a build and device validation. Drive is for colour.

RemoteFM1 0.9.2 keeps the host's cached flash-image address current on every SAVE.
Previously the adapter returned a snapshot that real hosts could keep reading
unchanged, even though the settings record and dirty counter had updated.

RemoteFM1 0.9.1 fixes SAVE waiting indefinitely during playback or held notes,
and flushes pending SAVE requests when the host synchronizes flash or shuts down.
The host owns disk persistence; let the app finish its save before force-quitting.
Navigation position and held performance gates are transient.

## What's in this version

- Kick, snare, closed/open hats with choke, clap, tom, Glitch and Analog.
- Independent lane sounds, levels, pans, mutes and lengths.
- 64-step sequence over four 16-step pages; 1–4 active bars, swing, velocity,
  step pitch, probability, ratchets, live note recording and mutation.
- One sound parameter lock per step, with all voice parameters available.
- Two independent tempo-synced LFOs per lane, five waves and eight destinations.
- A shared serial FX bus: delay, drive, bit reduction, sample-and-hold, filter,
  captured roll and stereo width. Edits preserve delay/filter/capture state.
- All 38 GlitchDrums palettes, starting with **GlitchBreaks**, quantized to RGB565.
- SAVE persists the project, sounds, step settings, LFOs, FX and theme together.

## Panel guide

| Control | Action |
| --- | --- |
| PLAY | Start/stop; tails continue after stopping |
| SELECT encoder | Select one of eight lanes |
| ALGORITHM encoder | Editor rows/pages; mixer fader bank 1–4 / 5–8 |
| PRESETS encoder | Browse presets; hold GLO while turning for tempo |
| KN1–KN4 | Four displayed values; mixer LEVELS uses four direct track faders |
| EDIT | Voice SCOPE; press again for detailed ROWS |
| ENV | Jump to its envelope controls |
| LFO | Cycle LFO 1 waveform, LFO 2 waveform and dual phase orbits |
| FX | Open FX; press again or turn ALGORITHM for its three pages |
| HOME | Eight-lane mixer; press again to toggle LEVELS / TRACK controls |
| SEL | Tap: mute/unmute; hold 400 ms: solo selected lane / restore mix |
| GLO | Cycle GLOBAL (theme/swing/bars/tempo) and PERFORMANCE (quality/polyphony) |
| SEQ | Step editor; turn keys into step/track controls |
| OCT− / OCT+ | Live keys: transpose ±1 octave (−3…+3); SEQ: previous/next 16-step editing page |
| REC | Toggle quantized live note recording while playing |
| ARP | Mutate non-quarter-note steps of the selected lane and editing page |
| Hold GLO + ARP | Mutate all eight lanes across all active bars |
| Hold ENV + ARP | Undo / redo the latest mutation |
| SAVE | Save the project immediately in RemoteFM1, including during playback. Hardware writes wait for stopped transport and silent tails. |
| MASTER | Volume |

### Playing and editing

In live mode the keyboard triggers the selected lane. OCT−/OCT+ transpose new
keyboard hits by octaves, shown as OCT in the header. Recorded hits include
that transpose; existing sequences and incoming MIDI retain their pitch.
The keyboard octave starts at zero on boot. Glitch and Analog play
chromatically from the voice's stored root; the six drum lanes retain their
stored pitch. MIDI channels 1–8 address lanes 1–8, with note 60 preserving the
Glitch/Analog root. Other channels address the selected lane. MIDI START/STOP
work; external MIDI clock following is not implemented yet.

On **SEQ**, the 16 white keys toggle the current page's steps; the first eight
black keys select lanes. The knobs edit the selected step's pitch offset,
velocity, chance and ratchet count (1–4). Hold ENV while pressing a white key
to select a step without toggling it. Lane lengths cycle independently inside
the active bar count; changing the displayed page does not change playback.

For a parameter lock: enter SEQ, select a lane, then press EDIT. Hold a white
step key and turn a sound knob. The step captures that parameter/value. This
version stores **one lock per step**; another parameter replaces it. Hold ENV
and press SAVE to clear the selected step's lock. Locks are latched on a hit;
base sound controls return on the next unlocked hit. Hold ENV before selecting
a step to avoid toggling its existing trigger.

Live recording writes into the current playback step. It quantizes to the step
under the playhead; it does not capture microtiming or knob automation yet.

In detailed ROWS mode (press EDIT again from SCOPE), the voice screen shows
four parameter columns and up to four rows at once. Turn ALGORITHM to choose the highlighted row; the view scrolls through
the remaining rows. KN1–KN4 edit the highlighted row. The sequencer is hidden
on this screen to give sound controls more room.

### Kick and snare: five sound rows each

Select KICK or SNARE and press EDIT. Turn ALGORITHM to choose a row; KN1–KN4
edit its four values. ENV jumps to the amplitude-envelope row. The final row
contains Level, Pan, Transpose and an unused fourth cell.

| Row | Kick KN1–KN4 | Snare KN1–KN4 |
| --- | --- | --- |
| 1 | Tune, amp decay, tone, body drive | Tune, amp decay, tone, noise |
| 2 | Pitch drop, pitch decay, click level, click decay | Punch, ring ratio, body decay, wire attack |
| 3 | Body delay, body shape, pitch hold, body punch | Wire decay, body level, wire level, wire color |
| 4 | Attack, hold, envelope curve, noise color | Attack, hold, envelope curve, cutoff |
| 5 | Level, pan, transpose, — | Level, pan, transpose, — |

Pitch drop displays semitones (0–72); pitch/click decay, body delay, pitch hold,
attack and hold display integer milliseconds. Kick body delay is 0–30 ms and
pitch hold 0–80 ms. Click decay starts at 0.5 ms, rounded down in the display.
Snare ring is displayed as ratio ×100 (100–280); body/wire decay as percent
of the base decay (25–400%). The master amplitude envelope also limits these
tails. Kick click level displays the app's ×0–4 scale.
Kick pitch hold pauses the drop near its last 10%, then lets it settle.
Kick tune is relative to the app's 52 Hz body root; kick attack spans 0.6–120 ms.
All exposed parameters support the existing one-parameter-per-step locks.

These are reduced integer instruments, not exact app voices. See
[the voice-depth comparison](docs/VOICE_DEPTH.md) for differences and next steps.

### Glitch: eight sound pages

| Page | KN1 | KN2 | KN3 | KN4 |
| --- | --- | --- | --- | --- |
| 1 | Source 1 | Material 1 | Pitched/raw | Note 1 |
| 2 | Size 1 | Position 1 | Edge 1 | Frozen noise 1 |
| 3 | Fresh noise | Noise color | Fold | Drive |
| 4 | Cutoff | Resonance | Motion target | Bipolar motion |
| 5 | Motion decay | Attack | Hold | Decay |
| 6 | Source 2 | Material 2 | Pitched/raw | Note 2 |
| 7 | Size 2 | Position 2 | Edge 2 | Frozen noise 2 |
| 8 | Oscillator mix | Level | Pan | Transpose |

Each oscillator chooses Glitch, saw or sine. Both Glitch oscillators select
Complex, Transient, Metal, Tonal, Dust, Hollow, Digital or Glass. These are
original synthetic material formulas derived from GlitchDrums' source,
quantized offline. SIZE/POSITION/EDGE/FROZEN apply to the Glitch tape source.
Fresh noise, shaping, filter and envelope are shared after oscillator mixing.
Envelope times show milliseconds; size/position/noise/shaping show percentages.
HOLD is currently displayed as a 0–127 control (about 0–4 seconds).

### Analog: eight sound pages

| Page | KN1 | KN2 | KN3 | KN4 |
| --- | --- | --- | --- | --- |
| 1 | VCO 1 note | VCO 2 note | Triangle/square 1 | Triangle/square 2 |
| 2 | FM | Oscillator mix | Noise | Bipolar pitch envelope |
| 3 | Pitch decay | Filter cutoff | Resonance | Bipolar filter envelope |
| 4 | Filter decay | Bipolar VCF modulation | Amplitude decay | Attack |
| 5 | VCO2 fine | VCF drive | VCA curve | VCF attack |
| 6 | VCF curve | Noise color | Decay macro | Tune |
| 7 | Sustain | Release | Hold mode | EG→noise |
| 8 | Pitch→VCF | Level | Pan | Transpose |

The pitch and filter envelopes have independent decays. Saw and square edges
use integer polyBLEP correction. The FM-1 version uses a reduced two-pole
resonant filter and single-rate tape synthesis; it is **not sonically identical**
to the app's oversampled Glitch oscillator and ladder filter. Extreme FM and
short/hard tape loops can alias. Hardware profiling will guide further quality
work. Continuous timbre changes do not reset oscillator/filter state.

### LFOs

Each lane has two LFOs: sine, triangle, square, saw, or sample-and-hold random.
Rates range from four bars to 1/64, plus dotted rates. They reset phase when
transport starts and keep running while stopped. DEPTH is bipolar around the
stored control. Destinations adapt to the lane: pitch, tone/filter, size/FM,
position/mix, fold/drive, pan, level and oscillator mix. A destination with zero
depth is inactive. LFO modulation never changes the saved base sound.

### FX

1. Delay mix, tempo division (displayed as actual milliseconds), feedback, damp.
2. Drive, bit depth (16–4), sample hold in frames, low-pass filter frequency.
3. Filter resonance, roll rate, roll mix, stereo width (100% preserves stereo).

Delay is capped at 743 ms and roll capture at 93 ms to bound RAM. Slow tempos
can hit these caps. Turn ROLL RATE to OFF to release a capture. The fixed chain
is shared by the stereo master; individual sends, reverb and reorderable slots
are not implemented. Saving waits for silence, so release a latched roll before
saving. Delay/filter/capture buffers live in the platform's external RAM pool.

## Build and verify

Without Docker, unpack an official WebAssembly/wasi-sdk release:

```sh
WASI_SDK_PATH=/path/to/wasi-sdk python3 tools/build_remote.py
sh tests/run.sh
node tests/remote.mjs
python3 -m http.server 8000 --bind 127.0.0.1 --directory build/web
```

`tests/wasm3.c` also checks boot and audio against upstream wasm3 0.5.0 (the iOS
interpreter). The native tests use ASan/UBSan, exercise every Glitch/Analog
control and stress LFO/FX bounds. The module tests cover MIDI, live controls,
38 themes, save/reload and corrupt saved data. A four-second `demo.wav` is
rendered from the actual `.rfm1`.

For a future hardware package, install/start Docker Desktop, then `./fm1 build`.
That route remains unverified here. Assign a unique hardware identity before
publishing; the config still carries the template's development identity.

Generated tables/themes ship in the source. To regenerate them from the parent
GlitchDrums checkout: `python3 tools/generate_assets.py`. The `.rfm1` includes a
local, SHA-256-pinned source archive URL; keep the source archive with it. A public
release needs an accessible pinned source URL.

## Next milestones

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for research and remaining work:
hardware packaging/profiling, external clock, multiple patterns/song mode,
multiple simultaneous step locks, automation, deeper modulation routing,
per-lane sends/reverb, undo, and richer voice presets. This is a playable 0.1
instrument, not feature parity with the iOS app.

## Upstream and licensing

Platform/tools/browser: https://github.com/konsumer/foma at
`f81e1e86298f6f69e7a59a4c781964f34ac7a050` with its notices.
ABI header: Quixotic7/FM1VST at
`177e27a7643284833f06b37435faf7a611ecf065`.
Instrument, colors and material formulas are adapted from this GlitchDrums
checkout. Firmware subtree: GPL-3.0-only; see LICENSE and LICENSING.md.
The existing iOS project is not modified by this firmware work.

### Punch-in FX (0.3.0)

Press FX once to open PUNCH. The keyboard controls effects here rather than
playing notes. Hold a key to engage; release to disengage. Leave this screen
to release every effect. Multiple keys stack. While transport runs, QUANT
aligns presses/releases to the sixteenth-note grid; a short tap plays one cell.
Stopped transport applies gates immediately. STOP clears held gates until release.

White keys 1–16: loop 1/4, loop 1/8, loop 1/16, loop 1/32, triplet stutter,
reverse, Tape Stop, half speed, LP sweep, HP sweep, phone band, crush, alias,
Stutter Gate, echo throw, wobble.

Black keys 1–11: AntiVerb, Glitchstortion, Grain Cloud, fold, ring mod, flanger,
auto pan, Comb Freeze, pump, RISE, FALL. Turn ALGORITHM to show either bank;
all keys remain playable in both banks. RISE/FALL are filter/level sweeps.

KN1 is punch MIX (zero bypasses the layer), KN2 QUANT, KN3 release/attack
FADE, KN4 DEPTH (also sets Comb Freeze pitch). Press FX again for the three
normal bus FX pages; another press returns to PUNCH. Use HOME/EDIT/SEQ to
return to drum playing/editing. The normal bus effects remain independent.

These are reduced fixed-point FM-1 adaptations, not exact ports of the app's
processors. Comb Freeze uses a damped, bounded pitched feedback loop restarted
on each press. AntiVerb, Glitchstortion and Grain Cloud are available in the signature bank below.
Hardware CPU/RAM verification is still pending; this release targets RemoteFM1.

### Signature bank (0.3.0)

Open FX → PUNCH. Hold the first three BLACK keys to play AntiVerb,
Glitchstortion and Grain Cloud respectively. Bank 2 displays their pads.
The black keys work in either of the first two banks. Bank 3 now places
Kill Low, Kill Mid and Kill High on its first three WHITE keys; other keys
are inactive there. Effect settings keep their stable internal IDs.
Each key releases cleanly, leaving the normal bus FX running. Quantize, mix and
fade work exactly as in the other punch banks. Gates are never saved.

KN4 becomes CHAR: a musical macro rather than the app's full parameter set.

| Effect | Character | CHAR macro |
| --- | --- | --- |
| AntiVerb | Overlapping reverse fragments and a damped reflection wash, ducked around new transients | Fragment size ~20–158 ms, overlap density and reflection feedback |
| Glitchstortion | Short reverse fragments through a nonlinear feedback tank, post smoothing and attenuation-only envelope compensation | Body ~5–48 ms, overlap density, bite and smoothing |
| Grain Cloud | Deterministic scattered forward grains, triangular windows and stereo spread | Size ~12–81 ms, 1–4 overlaps and rate 0.5–2× |

The three effects share an 8192-sample mono capture but maintain independent
fragment and reflection states. Each press begins a fresh capture epoch;
until enough new audio exists, the effect passes the input. Reopening on
silence cannot play stale fragments. Capture is limited to ~186 ms and there
are four overlapping fragments per processor. These are reduced integer
adaptations informed by the app DSP, not sonically identical ports. The app's
full size ranges, modes, freeze control and independently editable parameters
are not present. Existing saved 0.2 projects remain compatible.

0.3 validation covers all 30 punch effects and stacked output bounds, signature
CHAR response, capture freshness across counter wrap, distinct audible key
outputs, release tails, and the wasm3 0.5 interpreter. Hardware performance and
memory-fit testing are still required before producing a hardware package.

### 0.3.1 audio fixes

Glitchstortion's post smoother now has its own state rather than feeding the
reflection lowpass. The wet output is DC blocked; its input envelope begins at
touch-down rather than after capture seeds. At full punch MIX it now reaches
the processed branch completely, with a 3 ms bloom as the fresh capture starts.

Wavefold uses a symmetric, zero-preserving triangle fold and inverse-drive
trim. Its wet sample magnitude cannot exceed the input magnitude. Quiet input
stays quiet; the DEPTH knob controls folding rather than adding gain.

`node tests/fx-listen.mjs` renders the same six-second demo dry, with
Glitchstortion, and with wavefold, then writes a combined 18-second WAV. It
checks fold RMS/peak against dry and Glitchstortion DC and level. The test is a
repeatable measured comparison; listening on RemoteFM1 and the physical device
remains the final sound-quality check.

### 0.4.0: FX editing, latch and polyphony

Analog and Glitch now have **four independent voices each**. Play keyboard or
MIDI chords; notes and sequencer hits overlap through their own one-shot
envelopes. A fifth hit steals the quietest envelope (oldest on a tie) on that
lane. Drum lanes retain their previous behavior and hat choke. The step editor
still stores one pitch per step; live polyphony does not add chord-step storage.
Polyphony increases CPU demand; physical FM-1 timing/memory checks remain pending.

**To tweak an effect while performing:**

1. Open FX → PUNCH and press an effect key to select it.
2. Press EDIT. The FX editor shows its controls; the keyboard keeps its current
   punch bank and remains playable. KN1–KN4 edit the displayed parameters.
3. SELECT or ALGORITHM browses effects in the editor. EDIT or FX returns to pads.
   On the pad screen, ALGORITHM still changes banks and KN4 edits the selected
   effect's first parameter.

| Effect | KN1 | KN2 | KN3 | KN4 |
| --- | --- | --- | --- | --- |
| AntiVerb | Size (ms) | Smear | Squawk | Decay |
| Glitchstortion | Body (ms) | Density | Bite | Smooth |
| Grain Cloud | Size (ms) | Density | Spray | Rate (%) |
| Comb Freeze | Pitch (Hz) | Freeze | Damping | — |
| Wavefold | Fold | Output trim (%) | — | — |

Crush, Alias, Echo Throw and Ring Mod also expose a DEPTH control. Other
performance effects currently show FIXED; they retain the shared punch Mix,
Quantize and Fade settings. Blank controls have no assigned parameter.
These remain reduced FM-1 adaptations; the full app mode selectors and exact
nonlinear tank/diffusion topology are not part of this build.

**To lock effects on:** press ENV in PUNCH or its editor to switch HOLD KEYS to
LATCH KEYS. Tap a key to latch, tap again to release. Latched effects survive
bank changes and navigation, so you can return to a voice screen and play
chords through them. SEL in either FX performance screen clears every latch.
STOP also clears latches. ENV switches back to momentary mode; existing latches
remain until explicitly released or cleared. Latches are never saved or restored.

Each effect's controls persist independently with SAVE. Old project versions
2/3 migrate their tracks, sequence, theme and bus FX, while new effect controls
receive defaults. New signature defaults approximate the app's starting values:
AntiVerb ~90 ms, Glitchstortion ~20 ms, Grain Cloud ~48 ms at ~1× rate.

Fold's default trim is now 125% (about +2 dB relative to 0.3.1); the editor offers
50–149% trim. With maximum trim the folded sample remains at most 1.5× the dry
sample magnitude. Glitchstortion can now be tuned with independent controls
without resetting its active capture or smoothing state.

### 0.4.1 bank swap

The first three black punch keys now access AntiVerb, Glitchstortion and Grain
Cloud. Kill Low/Mid/High move to the first three white keys of bank 3.
Wavefold and every other performance key retain their positions. This changes
key assignments and pad labels only; sound processors and saved effect settings
retain their IDs and values. Latch and FX editing use the new assignments.

### 0.5.0: preset library and visual LFOs

The library includes **16 voice slots per lane** (8 factory + 8 user),
**8 kit slots** (4 factory + 4 user), and **8 FX scenes** (4 factory + 4 user).
Factory slots are read-only starting points. User slots hold your edits; slots
use numbered names in this first version. Voice families include low/sub,
sharp/metallic, dusty, bent and long sounds, plus pitched Glitch/Analog variants.
Factory kits are Original, Dust Works, Metal Room and Acid Tape; scenes are
Clean, Dust Delay, Metal Space and Tape Damage.

Turn PRESETS to open the browser and move through slots. It starts in Voice
mode from a sound editor, Kit mode from HOME/mixer, or Scene mode from FX.
ALGORITHM changes browser type (Voice / Kit / FX). SELECT chooses the drum
lane for voice presets. Browsing does not change the sound. SEL loads the
highlighted slot; keys still audition the selected lane. HOME or EDIT leaves
the browser. Tempo remains available on GLO's fourth knob, or by holding GLO
while turning PRESETS.

To keep a sound, browse to a USER slot and press SAVE. This stores the current
voice, kit or scene and queues the complete project/library for flash saving.
While a factory slot is selected, SAVE saves the current project/library only;
it never overwrites the factory sound. Flash writes wait for stopped transport
and silent tails, as before. Empty slots never alter the project on load.
ARP in the browser performs A/B compare, swapping the most recently loaded
preset's scope with the sound that preceded it. Edits on either side stay in
that A/B pair; compare memory itself is not persisted. This is sound compare,
not pattern undo.

Voice presets store 32 sound/mixer parameters and both LFO settings. Kits store
all eight voices, lane mutes, master FX and punch settings. Scenes store FX and
punch settings only. **No preset changes sequence steps, locks, lane lengths,
tempo, swing, bar count, theme, transport, or latched gate state.** Kit recall
is adopted atomically at the next audio block; existing voice envelopes and FX
buffers are not reset. Preset names do not imply an exact emulation of another
instrument. Recall is immediate rather than quantized to a musical boundary.

The complete save record uses **8544 / 12276 bytes**. Older v2/v3/v4 projects
upgrade into this record with empty user banks. Factory sounds remain generated
from fixed recipes, saving flash space. Bank values are validated at boot.

The LFO screen replaces the sequence with a waveform, its depth-scaled trace,
actual audio phase cursor and current-value dot. Random mode shows sampled
output history. Repeated LFO presses cycle **LFO 1 → LFO 2 → dual orbits**;
ALGORITHM also chooses these views. The first two views keep all four LFO
controls visible. The orbital view shows both audio phases with short trails;
return to an individual wave page to edit it. Visuals poll at 20 Hz and never
supply modulation timing. With audio paused, the phase does not advance merely
because UI time advances. Orbital radius responds to the corresponding depth.

Tests cover every factory recipe's bounds, user slot persistence, factory
read-only behavior, A/B comparison, kit/scene scope, bounded two-mailbox audio
adoption, old-project upgrades and actual phase-driven LCD animation. The
`tools/render_lcd.py` helper converts LCD dumps into PNG or animated PNG;
`tests/remote.mjs` captures frames by advancing exactly 2205 audio samples per
50 ms frame. Physical FM-1 performance/memory testing remains pending.

### 0.5.1: track solo

From HOME, a voice, SEQ, LFO or the global screen, **hold SEL for 400 ms** to
solo the selected track. Hold again on that track to restore the mix. Selecting
another track and holding SEL transfers the solo. A quick SEL tap still toggles
mute (on release); a long hold never changes its stored mute setting.

Solo temporarily overrides the selected track's mute, including sequencer
triggers, and suppresses the other lanes. Their envelopes and LFOs keep
advancing. A silent closed hat cannot choke a soloed open hat. Shared master
FX tails are preserved. The header shows SOLO for the selected solo track,
or S plus its track number when another lane is selected; SEL lights while
any track is soloed.

Solo is transient and is never saved into a project or preset. It stays active
through transport stop and preset recall until explicitly cleared. In the FX
performance/editor screens SEL retains its latch-clear action; in the preset
browser it still loads the highlighted preset. Return to a track screen to
use hold-to-solo.

### 0.5.2: P-lock editor on every voice

1. Press SEQ, choose a lane, and enable the step(s) to play. OCT buttons select
   the current 16-step page as usual.
2. Press EDIT directly from SEQ. The voice rows appear with LOCK KEYS in the
   header; keys stay in step/track mode rather than playing chromatic notes.
3. Hold a white step key and turn KN1–KN4 to lock the chosen parameter. Turn
   ALGORITHM to choose another row while holding the step. A locked value and
   cell outline appear for the held step; release returns the display to the
   base sound. Pressing a step here never toggles its trigger.
4. Hold ENV and press SAVE to clear the selected step's lock.
5. Press EDIT again, or HOME, to return to normal live-key playing. SEQ returns
   to the sequencer. Navigating to other sections leaves lock editing.

This workflow applies to kick, snare, both hats, clap, tom, Glitch and Analog.
It supports the parameters available on each voice's rows, with **one parameter
lock per step**. Locking another parameter on that step replaces the first.
LFO settings and master FX are not step-lockable in this version. A lock does
not create a trigger: an empty step remains silent. Base sound values and the
sequence's triggers are preserved. Locks persist with the project and are
preserved by preset/kit recalls. On an unlocked hit the voice uses its base
parameter again. Existing active polyphonic tails retain their own hit's lock.

As of 0.8.0, INIT KICK uses the app's model-specific INIT parameters. Its
integer oscillators, body shaping and noise/filter response still differ from
the app; this is parameter parity, not a bit-identical DSP port.

The preset browser now presents a three-item list with a highlighted selection.
CURRENT SOUND / LOADED means the selected preset's stored values match the
current sound scope; BROWSING / NOT LOADED means they differ. Empty user slots
are marked separately. PRESETS only browses; SEL explicitly loads. F-prefixed
slots are factory recipes and U-prefixed slots are user memories. The status
is computed from the current parameters, so edits do not leave a stale loaded
indicator. SAVE:PROJECT on factory slots and SAVE:STORE on user slots distinguish
the two save operations.

### Firmware card artwork

`art/glitchdrums-logo.png` is the existing 512×512 GlitchDrums app logo, copied
unchanged. `RemoteFM1Metadata.json` points `card.image` to that asset for catalog
ingestion. The path is relative to the standalone firmware source root, matching
the directory shipped in the source archive. When publishing from the whole
GlitchDrums monorepo, the metadata and path must be placed at that repository's
root as required by the catalog.

This metadata does not automatically customize the card of a locally imported
.rfm1. RemoteFM1's catalog pipeline copies the image when ingesting a published
firmware source/release. No catalog submission or publication has been made.

### 0.6.0: deeper kick and snare voices

Kick and snare each gain twelve character/envelope parameters, giving sixteen
sound controls plus level, pan and transpose. The kick now uses a fractional
semitone pitch sweep and separate click/punch envelopes. The snare has a
frequency-ratio punch, ringing overtone and independently scaled body/wire
envelopes. All new parameters are exposed in the scrolling editor and P-locks.
Project version 5 initializes previously unused character bytes; preset storage
remains 8544 bytes. Old private test preset compatibility is best-effort during
development, and will not constrain subsequent sound-design changes.

Validated with sanitizer core tests (including every new control's audio
response and extreme settings), full WASM UI/audio tests (all new editor cells
and deep P-locks), FX level regressions and wasm3 interpreter smoke checks.
Hardware performance and subjective listening still require user testing.

### 0.6.1: whole-pattern mutation

Hold GLO and press ARP to mutate all eight lanes across the active 1–4 bars.
Every non-quarter-note step has a 35% chance of toggling: existing hits turn
off, empty steps gain a velocity-90 hit. Quarter-note anchors, inactive bars,
sounds, mutes, lane lengths, pitch, probability, ratchets and locks are retained.
Muted lanes are included; steps beyond an individual lane length can change
inside the active bars but remain unheard until that lane is lengthened.

Plain ARP still affects only the selected lane's displayed 16-step page. In
the preset browser plain ARP remains A/B compare. Mutation runs as one queued
audio command so all lanes adopt the change together at the next audio block.
It works during playback. There is currently no mutation undo.

### 0.7.0: mixer and voice scopes

Inspired by the screen-purpose and all-track mixer ideas in
[JIANT FM](https://github.com/juanjiant-bit/JIANT), implemented here with original
GlitchDrums rendering code and the existing theme palettes.

- Boot / HOME: eight track strips, with stored level, a separate live signal
  meter, pan marker, selected-track frame and mute/solo status.
- LEVELS mode: KN1–KN4 are four direct track faders. ALGORITHM switches between
  lanes 1–4 and 5–8; the underline and footer show the controlled bank. SELECT
  selects a voice and follows it to the corresponding bank. All eight strips
  stay visible.
- HOME again switches between LEVELS and TRACK. TRACK retains selected-track
  level, pan, mute and length on KN1–KN4. From another section HOME enters LEVELS.
- EDIT from another section opens SCOPE: the selected voice's live waveform
  above four controls. ALGORITHM walks all its existing parameter rows; the
  header gives row / total. EDIT again toggles between SCOPE and detailed ROWS,
  preserving the current parameter row. ENV jumps to envelope controls in ROWS.
- SEQ → EDIT still opens the detailed P-lock editor. EDIT again leaves lock
  editing and opens the live scope on row one.
- The sequence grid is drawn only on SEQ. FX and GLOBAL no longer repeat it.
  LFO keeps its dedicated wave/orbit displays.

The scope is a 128-point, 16:1-decimated mono capture (about 46 ms at 44.1 kHz),
with automatic visual gain and a rising-crossing alignment search. It follows
the selected lane, sums all four polyphonic slots where applicable, and is
post-voice-gain/mute/solo but before pan and master FX. It is a performance
visual rather than a calibrated or anti-aliased measurement scope. On changing
lanes the previous capture flushes within one capture window.

Meters use decimated signal peaks with audio-block falloff, rather than envelope
values. Neither meter nor scope advances from UI clock ticks alone. All drawing
stays on the 20 Hz UI side, with fixed capture storage and no audio allocation.

Validated: all eight voice scopes respond to their own lane; UI-only ticks do
not move meters/scopes; all eight faders and selected-track pan edit correctly;
voice-row editing and P-locks still work. Scope/rows/HOME navigation produces
bit-identical audio to an untouched instance. Sanitizer, FX and interpreter
regressions are also checked. Hardware redraw/CPU headroom still needs testing.

### 0.7.1: navigation memory, mutation undo and FX state

The instrument remembers navigation for the current session: each voice's sound
row and SCOPE/ROWS preference, each lane's LFO page, the last FX section/editor,
master FX page and punch bank, plus the mixer mode/bank. SELECT still chooses
voices in normal sections and the edit target on punch FX screens. ALGORITHM
still walks sound rows, LFO displays or the current section's bank/pages. ENV
remains an explicit envelope shortcut; it intentionally changes the voice row.
SEQ → EDIT uses that voice's remembered row in P-lock mode. Leaving P-lock mode
returns to its normal remembered row/view. These UI positions are not saved to
flash. LFO button page cycling now redraws its graph immediately.

Hold ENV and press ARP to undo the last local or all-lane mutation. Repeat to
redo, and repeat again to undo. A new mutation replaces this one-level history.
It restores hit on/off and exact velocities across the stored pattern, preserving
subsequent sound, FX, pitch, probability, ratchet, lock and lane-length edits.
Adding/removing a hit, editing step velocity or recording a hit clears the history
to protect the newer pattern edit. History is transient and lost at reboot.
Undo/redo works while playing and is adopted as one command at the next audio
block; sounding tails and the playhead keep running. It does not rewind time.

ENV's normal action occurs on release, so holding it for ARP, SAVE or a step
selection performs that combination without also jumping screens or toggling
FX latch mode. Tap ENV on an FX screen to switch HOLD/LATCH; tap it elsewhere
to jump to the selected voice's envelope controls.

FX feedback now distinguishes the target from the gates:

- Magenta pad border: selected edit target. A selected target can be OFF.
- Bright inner border: physically held key.
- Magenta underline: latched effect, retained after releasing its key.
- Filled pad / key LED: audio gate active.
- Header OFF / HELD / LOCK / WAIT / TAIL: the selected effect's state; WAIT
  means a requested gate awaits quantization, TAIL means its previous active
  gate is finishing after release. L and H count latched effects and held
  unlatched keys. FXn in the top bar keeps latch count visible on other screens.
- FX editor names the edit target gesture and reminds you how to release its
  latch or clear all latches. SEL clears all; STOP clears as before.

Validation includes exact saved-pattern round trips, undo/redo playback matching
reference audio bit for bit, preserved later sound edits, history invalidation,
remembered sections/voices and distinct held/locked LCD captures. Extra transient
undo storage is 1024 bytes, fixed at build time; project/save layout is unchanged.

### 0.7.2: mixer label spacing

The direct-fader footer uses C HAT and O HAT so both hat labels fit their columns.

### 0.8.0: GlitchDrums INIT kick parameters

The default kick and first factory kick slot (INIT KICK) now use
`PadParameter.defaultValue(for: .kick)`, the same values used by the app's
`VoicePresetParameterCatalog.factoryPreset`. This is the model-specific INIT,
not the older generic KickCharacterSnapshot defaults.

| Parameter | App INIT target |
| --- | --- |
| Tune / settled root | +0.9 semitones / approximately 54.77 Hz |
| Body decay / tone | 500 ms / 0.36 |
| Body drive / shape / punch | 0.33 / −0.37 / 0.11 |
| Pitch drop / decay / hold | 45 semitones / 200 ms / 8.4 ms |
| Click level / decay | 4× / 50 ms |
| Body delay | 0 ms |
| Body attack / hold / curve | 61 ms / 0 ms / +0.07 |
| Noise color / level | −1 / 0.66 |

`tools/generate_kick_init.py` reads those app defaults and generates integer
conversion tables plus `docs/KICK_INIT.json` for review. Each INIT detent is
calibrated to its source value, rounded only to integer samples/Q15 precision;
other detents span the control's range. The kick TUNE knob now spans ±24
semitones from 52 Hz instead of MIDI notes. Attack now reaches 120 ms, click
level reaches 4×, body delay reaches 30 ms and pitch hold reaches 80 ms.
Kick decay and component time conversions use the app's −6.9 exponential rate
rather than the firmware's earlier −80 dB timing scale. The body attack shapes
the body, while click and pitched punch have their own fast envelopes.

The firmware still uses its reduced body shaper, noise/filter response and
integer envelope-curve approximation. Matching these parameters does not mean
the audio is identical to the app. No other app factory sounds have been chosen
or imported in this pass; remaining kick slots are provisional recipes.

Private project version 5 upgrades to 6 by initializing the kick and disabling
its old modulation depths, while preserving sequence, other voices, FX and theme.
Old user kick sound slots are cleared and kick sounds in user kits reset to INIT
because their old tuning/time bytes mean different things. Save size is unchanged.

Tests pin the INIT reference to the app source, verify physical DSP units, check
factory recall and the kick-only save upgrade, exercise every voice control and
P-lock, and run sanitizer, UI/audio, FX and wasm3 regressions.

### 0.8.1: match the app kick's attack and body paths

The 0.8.0 parameter match exposed a DSP mismatch: its dark-noise click filter
was much too dull, body shaping used a different transfer function, and the
app's final tanh saturation was absent. These paths now follow DrumVoice.hpp
using generated integer coefficient/transfer tables:

- 4.2 kHz noise-color filter, dark/bright blends and the tone-dependent click
  filter (250 + tone² × 17250 Hz).
- App body-drive wet/gain/compensation, DC-blocked round delta and hard shape.
- Rounded positive body curve and matching negative-curve decay rate.
- Per-sample pitch sweep, 18% pitch-hold knee and swept 80–400 Hz punch layer.
- Independent click envelope and final 1.4× velocity/tanh stage before level.
- Fresh kick filter/shaper state on every retrigger. Other lanes retain their
  existing processing. Fixed mixer headroom is retained.

`python3 tests/kick-compare.py` compiles the actual app C++ DrumVoice and the
firmware integer voice, rendering the same INIT at 44.1 kHz with matched seeds,
velocities and step pitches. Analysis removes only the firmware's fixed 1/3
mixer headroom, with no per-render normalization. Four INIT test cases yield
correlation above 0.99995 over the first 500 ms, gain within 0.1%, and 1.5 kHz
high-pass onset RMS within 0.03% over the first 50 ms. This is an isolated voice
comparison, not a full AU/master-FX or hardware equivalence claim.

The old 0.8.0 INIT render had correlation 0.3486 and attack-band RMS about
5.7 dB below the app reference. Optional before/after WAVs and JSON metrics are
written in build/test-release. Existing 0.8.0 parameters/presets remain valid;
no sound reselection is required. Device CPU/redraw headroom remains untested.

### 0.10.8: app-derived voices across the instrument

The snare, hats, clap and tom now use fixed-point app render paths and
model-specific INIT parameters, with five scrolling rows of controls each.
Snare adds its exact body/wire blend, independent tails and onset. Hats gain
the six-oscillator metallic bank, density, separate metal/noise decay, tuning,
bite and adjustable open-hat choke. Clap gains flam/burst/tail/smack shaping;
tom gains bend, body, laser, punch and low/high type.

Glitch now uses the app's original synthetic material tables, loop seams,
frozen/fresh noise processing, fold/drive, nonlinear four-pole ladder and
quality-selectable FIR decimation. Analog gains the app-style four-stage
nonlinear ladder, through-zero FM, fine tuning, filter drive, separate envelope
attack/curve controls, noise color, decay macro, tune, EG→noise, pitch→VCF and optional
held sustain/release. Both have eight editor rows; the remaining first factory
slots are named INIT so they can be auditioned consistently.

New Analog rows after the original four:

| Row | KN1 | KN2 | KN3 | KN4 |
| --- | --- | --- | --- | --- |
| 5 | VCO2 fine | VCF drive | VCA curve | VCF attack |
| 6 | VCF curve | Noise color | Decay macro | Tune |
| 7 | Sustain | Release | Hold mode | EG→noise |
| 8 | Pitch→VCF | Level | Pan | Transpose |

Analog HOLD mode is for keyboard/MIDI gates; release a key/note or STOP to end
it. Sequencer steps stay one-shots. Sustain/release controls have no one-shot
effect. Glitch continues to use finite one-shots. Full app routing matrices,
samples and channel-strip processing remain outside this firmware.

PERFORMANCE (GLO again or ALGORITHM): KN1 chooses Balanced 2×, High 4× or Eco 1×
Glitch quality; KN2 sets 1–4 Glitch notes. Defaults are Balanced/two Glitch notes
and four Analog notes to give the interpreter more headroom. New notes adopt
quality/voice-limit edits without resetting existing tails. High 4× is used for
the closest app-reference comparisons; Balanced/Eco change the oversampled
noise/alias behavior. These settings persist with the project.

All eight voices have been compared against actual C++ app voice renders.
See [VOICE_DEPTH.md](docs/VOICE_DEPTH.md) for measured results, limitations,
control details and reproduction commands. Paired audition WAVs play app first,
firmware second. Existing kick fidelity remains regression-tested.

No hardware `.fwsc` is provided. Device CPU headroom still needs profiling,
especially High/four-Glitch polyphony. The default profile passed a wasm3 host
stress run faster than real time; that is not an iPad/device guarantee.
