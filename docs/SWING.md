# Swing — 0.17.8

Swing follows GlitchStep: delay alternate notes at the selected division by
`division duration × amount × 0.5`. Amount is 0–100 in the firmware and app.

GLOBAL now puts **SW GRID** (KN1: 1/8 or 1/16) beside **SWING %** (KN2).
KN3 is pattern length and KN4 is BPM. Theme moved to the Performance page's KN3.

Fresh projects use 1/8. The default hat offbeats (steps 3, 7, 11, 15) now move
when swing increases. The sixteenths between eighth-note boundaries interpolate
proportionally, preserving note order, quarter-note anchors and total loop length.
1/16 retains the earlier subdivision behavior. Pattern contents remain unchanged.

Older saves keep their original sixteenth-note timing: former amount 25 becomes
50, and 50 becomes 100. This is the normalized GlitchStep amount, not a stronger
saved groove. The packed amount/basis uses the existing swing byte; full save
size remains **12,268 bytes**, with no reduction in patterns, voices or locks.

Live changes keep the pending boundary and reconcile the following boundary
against the unswung clock, avoiding an accumulated beat-phase shift. Slice routing
and immediate pattern changes keep their existing clock-preservation behavior.

GlitchDrums app uses the same formula in its live and offline renderers, with an
AU-owned division parameter and a small division control beside the swing slider.
Legacy app archives without that parameter restore 1/16; new sessions use 1/8.

Tests cover the Global knobs, sample-exact onsets at both divisions, stable beat
anchors, rapid amount changes, save/reload and legacy migration. The standalone
`tests/swing-listen.mjs` renders the default groove at 0 and 50 plus an A/B WAV.
