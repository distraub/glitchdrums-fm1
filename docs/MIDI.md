# MIDI and Learn — 0.13.0

0.15.0 adds eight project-level delay sends, with MIDI Learn. Complete save:
**12,268 / 12,276 bytes**, eight bytes free. See [Delay sends](DELAY_SENDS.md).
The earlier release/storage details below describe their respective schemas.

## Learn a controller

1. Open the voice, mixer, LFO or FX controls you want to map.
2. Press GLO until the third page, **MIDI LEARN**. SEL starts Learn and returns
   to the previous controls. KN1 can also enable/disable Learn there.
3. Turn a firmware knob. The header identifies its target and asks for a CC.
4. Move an external controller. The header shows the learned channel and CC.
5. Turn another firmware knob to learn another target, or press **OCT+** to
   finish. **SAVE** keeps the mappings with the project.

While Learn is on, OCT− clears the picked target's mapping; neither octave key
transposes notes in Learn. In the MIDI page, KN2 or SELECT scrolls the sixteen
records, and OCT− clears the selected one. There is no automatic flash save on
controller movement or binding: SAVE is explicit, like the rest of the project.

Firmware knobs still edit their values when touched during Learn. The first CC
binds the target without jumping its value; later messages apply the CC's
absolute value across that control's actual range. This pass supports ordinary
7-bit absolute CCs; relative encoders, pickup and 14-bit controllers are not
implemented. Incoming CCs are never echoed out.

## Targets and routing

Learn supports voice parameters (all voices), mixer level/pan/mute/lane length,
the two LFOs' controls, master FX, all 27 performance FX controls,
punch mix/quantize/fade, swing and tempo. Voice/LFO/mixer targets stay pinned to
their chosen track when another track is selected. A performance-FX control is
bound to its effect identity and knob. Discrete
controls scale to their discrete range; musical note controls retain their
allowed minimum/maximum. Length edits respect chain/pattern ownership guards.

To learn a momentary FX gate, enable Learn, then tap any FX key
before sending a CC. Values 64–127 hold that pad's effect; values 0–63 release it,
including away from the FX page. Normal punch fade/tail behavior remains.
STOP clears MIDI gates; reload/clear does not leave them latched. Gate state is
transient, not part of the save. Ordinary effect latch mode is separate.

Each mapping remembers the exact MIDI channel plus CC. One channel/CC addresses
one target, and a target has one mapping. Learning either again replaces that
binding; it does not evict an unrelated mapping. Sixteen unrelated mappings fill
the table: clear one before adding another. The same CC on two different channels
may control different targets. Channel-mode, bank-select and data-entry CCs are
reserved: 0, 6, 32, 38, 64, 96–101, 120–127 cannot be learned. CC1 and CC11 are
available for learning in this firmware.

## Current MIDI implementation

| Input | Behavior |
|---|---|
| Note on/off, velocity | Channels 1–8 address Kick, Snare, C Hat, O Hat, Clap, Tom, Glitch, Analog; 9–16 address the selected voice |
| Notes while recording | Existing recording behavior, including four-note Glitch/Analog steps |
| CC7 / CC10 | Track level / pan, unless that channel/CC has a learned destination |
| Learned CC | Exact channel and pinned target; learned routing takes precedence |
| Start / Stop | Existing transport support |
| FX learned gate | Momentary performance-pad control |

External MIDI clock following, MIDI note/CC output, pitch bend, sustain, RPN,
Program Change and transport Continue/Song Position are separate work; they are
not added by MIDI Learn. The platform exposes USB/TRS input on hardware; the
RemoteFM1 host must route its MIDI input into the module. Tests inject messages
through the real `rfm1_midi_in` ABI. A physical controller/app routing check is
still needed on the user's setup.

## Storage, CPU and verification

Sixteen maps use three bytes each: occupied flag/CC, target-kind/channel, target
index. Schema 4 retains all **48 bytes** of mappings and losslessly packs sounds/FX. Full
size is **12,260 / 12,276 bytes**, leaving **16 bytes**. Existing schema-1/2/3
projects migrate with empty maps and retain the complete bank, chain and custom
factory replacements. Invalid records are discarded without resetting music;
source/target duplicates are normalized deterministically.

CC routing and learning run in control code. Audio receives the existing bounded
scalar commands; it never scans or writes the map table. Redundant scaled values
are suppressed. Continuous MIDI display feedback is limited to about 30 Hz and
avoids duplicate mixer drawing. On the desktop wasm3 check, idle stayed around
0.050 CPU seconds per audio second; demo playback was about 0.194, and a stream
of roughly 115 learned CC updates/second was about 0.213 (1.397 ms maximum audio
block on this host). The explicit SAVE tick measured about 0.980 ms. These are
regression measurements, not device guarantees.

Native sanitizer/unit tests verify table capacity, identity, reassignment,
reserved messages and target bounds. Module tests cover Learn/clear, exact
channels, non-selected tracks, range endpoints, discrete controls, assigned FX,
momentary gates, malformed data, full table preservation and full-density bank
plus all sixteen mappings save/reload. No DSP algorithms are changed.

## Reference examined

The user supplied [Skiff v2.2](https://github.com/illuminus5/skiff/tree/v2.2).
Its [MIDI Learn source](https://github.com/illuminus5/skiff/blob/v2.2/firmware/src/midi_learn.c)
and [controller source](https://github.com/illuminus5/skiff/blob/v2.2/firmware/src/midi_control.c)
were reviewed for the knob-then-CC interaction, clear/done gestures and reserved
controllers. Skiff credits that interaction to Felucca 1.5. The code here is an
independent implementation for our compact save format and control/audio queue;
Skiff's engine/UI source was not copied into this firmware.
