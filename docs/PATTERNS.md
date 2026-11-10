# Sixteen-pattern instrument — 0.13.0

0.15.0 adds eight project-level delay sends, with MIDI Learn. Complete save:
**12,268 / 12,276 bytes**, eight bytes free. See [Delay sends](DELAY_SENDS.md).
The earlier release/storage details below describe their respective schemas.

The pattern/chain format remains intact. Schema 4 losslessly packs sounds and FX,
retaining 48 MIDI-mapping bytes: **12,260 bytes** with **16 bytes free**.
See [MIDI Learn](MIDI.md); the 12,216-byte figures below describe the chain-only
schema used before that extension.

## Performance controls

SEQ cycles between steps, the A–P bank and CHAIN. The sixteen white keys select the
sixteen patterns; SELECT/KN1 browse and SEL confirms. One tap queues the next
pattern for step zero after the current 16-step loop. A second tap of the same
key within 300 ms switches immediately at the next audio block while retaining
the current step, its remaining sample countdown, swing parity and bar phase.
The current step is not retriggered: new notes begin on the next scheduled step.
Old-pattern ratchets are discarded.
STOP adopts a queued selection. Existing voices, tails, FX and LFOs keep running.

ENV + SEL copies the active pattern to the selected slot; ENV + ARP clears an
inactive slot. SAVE writes the whole project and bank. The active slot cannot
be cleared. Sequence edits/copy pause during a pending switch; shared voice/FX
controls remain live. The queued tile/header and blinking key show the target.
SAVE before adoption records the current pattern, not the future queued choice.

Every pattern is fixed at 16 steps, with lane lengths independently adjustable
from 1–16. There is no bar-count control or step paging. Octave buttons still
transpose live notes outside SEQ. Four-note-per-step recording remains available
on Glitch/Analog via keyboard or MIDI with PLAY + REC. Chord notes share velocity,
chance, ratchets and p-locks. Duplicates are ignored; a full chord retains its
first four pitches. Clearing the step clears the chord. The header shows N:1–4.
Held durations and per-note velocities are not recorded in this pass.

## Sixteen-row chain

In CHAIN, KN1/SELECT chooses a row (or tap its white key). KN2/ALGORITHM assigns
A–P; KN3 sets 1–16 repeats. SEL marks the selected row as the chain end. KN4
chooses OFF, LOOP or ONCE. Configure while stopped, SAVE, then PLAY: every start
begins at row one. LOOP wraps to row one; ONCE stops after the final repeat,
letting existing voice/FX tails continue naturally. STOP pauses transport;
starting again begins the chain anew rather than resuming a saved play position.
The active row/pass are shown on screen, and the running row's key lights.

Row selection can be browsed during playback. Row contents and chain length
require stopped transport. KN4 OFF can disable a running chain; its current
pattern then loops normally. Manual selection from the pattern bank also turns
the chain OFF, with the usual single-tap boundary queue or double-tap phase-safe
immediate switch. SAVE keeps the configured rows, length and mode; running row
and repeat counters are transient.

The decoded pattern bank is immutable while chain playback owns it: sequence
editing, copying, clearing, mutation and recording are paused. Shared voice,
mixer, LFO and FX controls remain live. Turning the chain OFF or stopping returns
bank ownership to the editor after the audio command is adopted. Audio reads
only the selected pattern at loop boundaries, not on every sample/ratchet. It
copies the chosen 905-byte pattern into the existing live state, and UI adopts
that selection via acknowledgement. There is no second decoded audio bank,
no background re-encoding, and no flash operation on row changes.

Each chain row packs a 4-bit pattern and a 4-bit repeat-minus-one. Sixteen rows
plus length/mode take 18 bytes; alignment adds two, for **20 additional saved
bytes**. The complete schema-2 save is **12,216 bytes**, leaving **60 bytes** of
the 12,276-byte budget. All pattern/chord/p-lock fields remain unchanged. Schema-1
0.11.0/1 images migrate to an OFF chain with length one; no user edits or custom
factory mappings are discarded.

## Full keyboard FX

All 27 physical keys trigger fixed effects. Kill Low/Mid/High are omitted.
SELECT/ALGORITHM chooses an effect; EDIT opens its four controls. ENV toggles
latching, SEL clears latches. All 27 settings persist in projects/kits/scenes.
No pattern, chord, p-lock, MIDI or custom preset capacity is removed. Lossless
7-bit packing of sounds and FX reduces the complete save to 12,260 bytes.
See [performance controls](PERFORMANCE_FX.md).

## Editable factory entries and custom capacity

All eight factory voices per lane and four factory kits/scenes remain compiled
into the firmware. SAVE in the browser stores a custom replacement of the
selected factory entry (marked `*`), or stores to a separate user entry.
ENV + SAVE restores the original factory entry or clears the custom user entry;
ENV + SEL remains an immediate original INIT voice recall.

There are **two custom voice records per voice, one custom kit and one custom
FX scene**, shared between factory replacements and separate user entries.
Two custom factory voices therefore occupy that voice's two records. A third
new replacement is refused until a record is cleared; an existing replacement
can always be updated. Nothing is silently evicted. SAVE outside the preset
browser continues to save the project normally.

## Fixed, worst-case storage budget

A fixed pattern encodes:

| Content | Bits |
|---|---:|
| Eight lane lengths, 4 bits each | 32 |
| 128 steps: velocity 7, chance 7, ratchet 2, pitch 7, lock ID 6, lock value 7 | 4,608 |
| 32 synth steps: extra-note count 2, three pitches at 7 bits each | 736 |
| **Total per pattern** | **5,376 = 672 bytes** |

All ranges are lossless, including the no-lock sentinel and edited empty steps.
All sixteen patterns occupy **10,752 bytes**. Project sound/FX/settings, the
custom library, occupancy flags and factory-replacement mappings bring the base
save to **12,196 bytes**. The 20-byte chain extension makes the full current
save **12,216 bytes**, within the FM-1's **12,276-byte** limit with 60 bytes spare. A dense bank is the same size as an empty one. No sparse
capacity failure or data-dependent overflow remains for a valid new bank.
Compile-time layout assertions and a fully populated save/relaunch fixture
check this guarantee.

Each decoded bank pattern now uses 905 RAM bytes: sixteen take 14,480 bytes,
versus 28,744 bytes for the previous eight 64-step/chord patterns. Live chord
metadata is 128 bytes; mutation snapshots use 16 steps. The active GDProject
working structure retains legacy track arrays for DSP/import compatibility,
but only its first sixteen steps are editable, played or serialized.

There is one immutable audio launch mailbox and no second audio-side bank.
Packed encoding/decoding belongs to explicit save/load work, never sample
rendering. Byte-wise bit-field operations touch at most two bytes per field.
Normal UI ticks do not scan or pack the bank. Audio copies a pattern once at
adoption; clocking otherwise follows the existing sequencer. No allocations,
locks or file access are introduced in audio.

## Importing older private saves

0.10.11 eight-pattern, 0.10.9/10 A/B, the older project/library prefix and the
preserved song-release prefix are recognized. The first bars stay in their
original A–H positions; additional bars are split into I–P in source-slot order.
Used hidden bars are retained too. Longer lane lengths become 16 in the fixed
layout; shorter lengths remain. Split bars are independent patterns, not an
automatically chained recreation of the old arrangement.

If more than sixteen slots are required, the firmware shows **OLD BANK >16
BARS: NO SAVE** and refuses to overwrite the original flash. Return to the prior
firmware, trim unused patterns/bars, save, then import again. The old image is
untouched until a successful new SAVE. Import preserves the first two occupied
user records per voice and first occupied kit/FX record, renumbered into the
smaller custom library; the other custom
records do not fit the chosen budget. The original 0.10.11 release remains in
the testing folder and Git history.

## Verification and CPU scope

Native ASan/UBSan tests cover DSP bounds, the fixed codec, randomized full-range
round trips, truncation/corruption and the exact worst-case budget. RemoteFM1
integration covers dense-bank save/relaunch, all sixteen key mappings,
independent p-lock edits, copy/clear, exact quantized and immediate launches (including swung phase preservation),
chord recording/replay, all 120 effect controls, assignment swaps, factory
replacement/reset/capacity, old-bank import/overflow protection and mutation
undo/redo.

Without new input, the four-second demo PCM remains sample-identical to 0.10.8.
Signature-effect comparison renders and gain/ceiling checks retain their prior
results. Desktop wasm3 0.5.0 checks use 128-frame audio, 1 kHz ticks and 50 Hz
frames: about 0.050 CPU seconds per idle audio second, 0.196 for the demo, and
0.795 for a three-second bank-switch render. An explicit SAVE tick measured
1.022 ms on this host. Device/app performance still needs the user's real-time
check. A maximum 14-note/three-signature-effect stack remains an existing heavy
case (~0.896 CPU seconds per audio second here, with individual blocks over
2.9 ms); the slot layout does not make every extreme combination inexpensive.

Active-chain wasm3 comparison (same six-second musical material, 128-frame
blocks, 1 kHz UI ticks, 50 Hz frames) measured 2.053 CPU seconds for ordinary
pattern playback versus 2.055 for chaining; maximum audio blocks were 1.631 ms
and 1.676 ms respectively on this development host. Idle remained about
0.051 CPU seconds per audio second. Chain tests additionally cover exact
repeat/row/audio boundaries, LOOP/ONCE, OFF and manual override, bank ownership,
configuration persistence/migration, corruption recovery and all 16 × 16 repeat
limits. These comparisons establish no device guarantee.

The earlier experimental song idle overload remains unreproduced in clean
host tests, and that arrangement engine stays isolated at `b2d2f3f` on
the archived song-mode experiment. Chaining here uses the smaller immutable-bank
design described above.

## Step cursor — 0.15.2

On SEQ's step-edit screen ALGORITHM moves the edit cursor through steps 1–16,
clamping at either end without toggling notes. The selected voice/step cell has
a bright outline and its column has top/bottom markers. The header shows the
selected step number after the pattern letter, independent of the playhead.
KN1–KN4 edit pitch, velocity, probability and ratchets for this selected step.
ENV + a bottom-row key selects that step without toggling it and updates the
same cursor/header. Normal bottom-row taps still toggle steps.

## Short patterns — 0.15.3

GLO's first page KN3 STEPS now sets all eight lane lengths together from 1–16.
The pattern boundary follows the longest lane, so an eight-step pattern loops,
queues the next pattern and advances chain repeats after eight steps. The mixer
TRACK length remains available for independent lane lengths; with different
lengths, STEPS displays the longest one. Editing global STEPS replaces all eight
lengths together. Notes beyond a shortened lane length remain saved and become
active again when lengthened; they are dimmed in the step grid.

Lengths stay with each pattern using the existing packed lane-length fields;
there is no save-size increase or preset/lock reduction. Length changes are
blocked during chain playback, like other bank edits. While playing an ordinary
pattern, a shortened span takes effect at the next scheduled step without an
early note retrigger. Phase-preserving immediate switches map the step phase
into the destination pattern's length at its next scheduled step boundary.

## Visible lane lengths — 0.15.4

SEQ shows LEN for the selected track in its header. Each lane's inactive cells
are shaded with diagonal hatching and its endpoint has a vertical marker.
Per-row playback markers follow `playhead % track length`, matching the played
step, rather than traversing inactive cells. Bottom-key playback LEDs follow
the selected track's loop, and inactive stored notes no longer light those keys.
The bright edit cursor remains independent and can still select stored steps
beyond the active length. No note, clock or save layout changes are made.

## Compact sequencer header — 0.15.5

SEQ uses one small-text line below the transport: pattern letter, S01 selected
step, L16 selected-lane length, and voice name. The large voice number/title is
removed on this screen. Mute/solo/chord indicators remain at the right. The
recovered height grows row spacing from 6 to 9 pixels and note cells from 4 to
7 pixels tall. High-contrast parameter captions, edit cursor, hatched inactive
cells and per-track playheads remain visible above the four large editing
values. Other screens retain their existing headings. SEQ MIDI Learn uses a
single-line prompt to preserve the expanded grid. No audio/save changes.

## Non-destructive live paths — 0.17.0

SEQ now cycles STEP → PATTERNS → CHAIN → SLICE → STEP. SLICE can reorder the
source playhead temporarily without editing pattern data or altering the chain
clock. See [SLICE controls and behavior](REMIX.md).

## Performance layout — 0.17.1

SEQ cycles STEP → SLICE → PATTERNS → CHAIN. SLICE is now the second page.
Its number table is replaced by a sixteen-column/sixteen-row slice grid, with
slice 1 at the bottom and slice 16 at the top. The final two black keys select
the Breaks and 4 to the Floor paths from the supplied GlitchBreaks screenshots.
