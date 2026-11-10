# PERC — firmware 0.14.0

Track 6 replaces TOM with GlitchDrums Percussion, keeping its keyboard location,
mixer lane, MIDI channel 6, mute/solo, two LFOs and step recording. Twenty voice
controls work with p-locks and MIDI Learn, using the existing storage budget.

| Row | KN1 | KN2 | KN3 | KN4 |
|---|---|---|---|---|
| 1 | Tune | Decay | Tone | Noise |
| 2 | Ratio | XMod | XMod Decay | Shape |
| 3 | Dual | Bend | Bend Decay | Strike |
| 4 | Tone/Noise | Thud | Snap | Noise Color |
| 5 | Attack | Hold | Curve | Drive |
| 6 | Level | Pan | Transpose | — |

EDIT opens voice controls; ALGORITHM selects a row. ENV jumps to the amplitude
row. EDIT switches between the scrolling rows and scope view. Factory bank is
INIT, Wood Block, Cowbell, Clave, FM Bell, Metal Hit, Shaker, Conga Low.

The fixed-point port follows the app's two-oscillator ring/dual mix, independently
decaying cross-modulation, pitch bend, strike transient, normalized shape,
tone/noise balance and separate delayed-onset thud. INIT has exact detents from
PadParameter.swift. Coefficients are prepared when controls change; rendering
uses fixed-size state and integer oscillator/shape tables with no allocation.
Twenty parameters require 20 extra state bytes per acoustic slot (100 bytes
across the existing five slots); saved sound/pattern structures do not grow.

Previous Tom sounds and locks are not translated. On an old saved project load
PERC INIT (ENV + SEL in its voice preset browser), or a new factory kit, and
clear old lane-6 locks if used. Factory presets are regenerated from the app's
Perc catalog. Low Tom/High Tom no longer have dedicated FM-1 lanes.

Validation: ASan/UBSan core and codec suites; all 20 controls change isolated
audio and can be locked without changing the base sound; WASM UI edits and
save/reload for all 20 controls; factory voice generation; output gain and FX
level/DC regressions. App DrumVoice versus firmware INIT at velocity 64/110,
and velocity 127 one octave up: correlation .99992–.99999, RMS gain ratio
.99756–.99844, high-frequency energy ratio .99675–.99822. This is an adaptation,
not a bit-identical port across all parameter combinations. Reproduce with
`python3 tests/voice-compare.py`; stereo A/B files are generated in build/test-release.

Host wasm3 demo: .201 CPU seconds per 1.001 audio seconds, max block 1.444 ms
(previous .196 CPU seconds). Hardware performance remains to be checked.

## User INIT — 0.14.2

PRC startup and factory INIT now use the user's two firmware screenshots:
Tune -6.00, Decay 285.03 ms, Tone .22, Noise .72; Ratio .57, XMod .45,
XMod Decay 1.69, Shape .31; Dual .37, Bend .19, Bend Decay 1.75, Strike .96;
Tone/Noise .34, Thud .37, Snap .59, Color -.23; Attack 13.76 ms, Hold 7.55 ms,
Curve .57, Drive .66; Level 56%, Pan 0, Transpose 0. Where two control positions
share a rounded display value, the closest physical value is selected. Existing
sound tables and named factory sounds are unchanged. LFO/global FX defaults
are unchanged. Saved projects/custom INIT replacements retain their saved sound.
The original app INIT remains available to the comparison harness independently
of this user-facing preset, via gd_perc_app_init / PERC_INIT.json app_init.
