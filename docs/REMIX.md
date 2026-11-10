# Live SLICE — 0.17.6

The clock keeps counting normally; a temporary route chooses which source step
it reads. Source notes, velocities, pitches, ratchets, probability, chords and
p-locks travel together. The original pattern bytes are preserved. This is note/event
routing; the separate Glitch buffer chops captured audio. They work together.

SEQ cycles STEP → SLICE → PATTERNS → CHAIN. From HOME, press SEQ twice.

| Control | Action |
|---|---|
| All eleven black keys | Select and momentarily hold a route mode |
| ALGORITHM | Select route mode without needing to hold a key |
| SEL | Latch/unlatch the selected route; can be armed before PLAY |
| KN1 VAR | Browse 128 deterministic variations |
| KN2 GLITCH RATE | Fine offset for the whole white-key size bank |
| KN3 GLITCH MIX | Blend captured audio with the live routed voices |
| KN4 DEPTH | 0% original route → 100% full route |
| White/bottom keys | Largest chunk on key 1 through smallest on key 16; hold to punch |
| ENV + white key A–P | Print the current preview into that pattern slot |
| SAVE | Persist the project and printed pattern bank |
| PLAY/STOP or leaving SLICE | Release the temporary route; leaving also releases Glitch |

Modes are Shuffle, Reverse, Pairs, Repeat, Skip, Bounce, Rotate, Chunks, Walk, Breaks and 4 to the Floor.
Walk follows GlitchBreaks’ musical random-walk idea: occasional returns to zero,
forward continuations and random jumps. At lower depth Shuffle preserves the opening downbeat; at full depth it can
rearrange everything. Modes/variations can repeat or skip source steps. The same
mode, variation, depth and pattern span recreate the same route. Try Repeat or
Chunks with VAR, latch it, then sweep GLITCH RATE while holding a white key.
The final two black keys are Breaks and 4 to the Floor. REC is disabled here.
Glitch Rate/Mix can be MIDI learned using their existing master FX targets;
route mode/variation/depth and print are not MIDI Learn targets in this pass.

The 16×16 slice grid reads clock steps left-to-right and source slices bottom-to-top,
matching the sister apps. Filled cells show each clock-to-source connection. The
vertical outline is the clock; the bright cell outline is the current source.
Inactive preview routes are dimmer, with only the clock column outlined; they
do not add an extra straight-through cell to the planned path. A black key release (without latch), SEL unlatch or page exit
returns to the straight path at the next scheduled step, without resetting tempo,
swing or the remaining sample countdown. Already-triggered ratchets/tails can
finish normally. Chains/repeats use the original clock, not the routed source.
Short patterns rebuild the route for their span; each lane reads source modulo
its own length. Entering SLICE turns recording off to protect the source pattern.

Routes, mode, variation, depth and gates are session-only and are not saved.
SAVE stores the source pattern, any explicitly printed destinations and ordinary
sound/FX settings. Changing
Glitch Rate/Mix still saves those existing global parameters. Custom/factory
pattern capacity and the 12,268-byte save layout remain unchanged. No audio
buffer is added: routing uses a 16-byte table in fixed-size control/audio state.
The audio table rebuilds only when configuration or pattern span changes; edits
are delivered by the existing bounded command queue. The map UI is limited to
about 30 Hz; audio controls apply independently of that display refresh.

The source model was studied in the local sister apps: GlitchStep's
`GlitchStepExtensionDSPKernel.hpp` selects `slices[slicePattern][sliceStep]`
independently of its step clock; GlitchBreaks2's kernel separately computes
`currentSlice` and `targetSlice` and routes through the active slice pattern.
This firmware uses that separation with independently implemented bounded integer
route generation, not their sample grids or dynamic container storage.

Validation: ASan/UBSan tests cover all 16 lengths, eleven modes, 128 variations
and representative depths. Full reverse audio matches a manually reversed bank,
including a p-lock, ratchets and a polyphonic chord; all sixteen source pattern
payloads remain byte-identical. Integration tests cover latch/release, concurrent
black/white gates, reload bypass and chain repeats on the original boundaries.

Host wasm3: active route .129 CPU sec per 1.001 audio sec, maximum block .533 ms;
route plus Glitch .136 CPU sec, maximum 1.048 ms; roughly 86 variation updates
per second with Glitch .380 CPU sec, maximum 1.685 ms. These are representative
host tests, not all-effect/all-voice hardware guarantees.

## Print the current path

Hold ENV and tap a white key corresponding to destination A–P. It prints the
current preview even when the route is not being held/latched. The source slot
cannot be overwritten. An occupied destination requires the same tap again
within 1.5 seconds; another slot or an expired confirmation needs a new first
tap. Press SAVE afterward to persist the bank. Printing does not switch the
active slot, reset timing or release an already held Glitch key.

Printing works during ordinary playback, but waits for queued pattern switches
and is blocked while a chain is playing. It runs on the control side and uses
existing bank slots, adding no persistent bytes or audio buffers. Each lane of
the destination uses the overall pattern span, with its mapped events expanded
to that span; this preserves routes that jump among different short lane loops.
Only the active span is printed. Source notes and hidden steps remain intact.
Chords, velocity, pitch, chance, ratchets and locks are copied from each mapped
source step. The destination is a normal editable/chained pattern afterward.

## Key-sized Glitch and the two fixed walks

White keys now choose sixteen logarithmically spaced rates, slow/large on the
left to fast/small on the right. KN2 offsets that entire bank, with SHIFT shown
on screen. The most recently pressed held key wins; releasing it falls back to
the previous held key. Changing held keys preserves the same captured buffer.
Release all white keys to punch out. This works on SLICE and master FX page 3.
The offset is performance/session state and resets on reload; the last effective
rate and mix still use their existing saved global FX fields. No save bytes grow.

At VAR 0 and DEPTH 100%, the second-last black key BREAKS reads:
1,2,3,4,5,6,1,2,3,4,5,6,7,8,1,2.
The last black key 4 TO FLOOR reads:
1,2,3,4,1,2,3,4,1,2,3,4,1,2,3,4.
These match the supplied screenshots. VAR now creates seeded phrase mutations around each base path; DEPTH blends
toward the original route. Short patterns
wrap the template source into their available steps. Both can be printed normally.

The initial zero mix is prepared to full on the first unconfigured Glitch press;
a deliberately edited mix (including zero) is retained across subsequent key
changes. The 0.17.1 host stress run with Walk, Glitch and rapid VAR sweeps used
.498 CPU seconds per 1.001 audio seconds, maximum audio block 2.063 ms.

## Exact fixed paths and longer Glitch — 0.17.2

SLICE defaults to full Depth. Selecting Breaks or 4 to the Floor, by its black
key or ALGORITHM, resets VAR to zero and DEPTH to 100% so the supplied shape
appears exactly. Subsequent knob changes still allow intentional variations or
blending. The earlier default Depth of 80/127 caused the mismatch at VAR zero.

The Glitch bank now spans 1/4–1/512 at normal tempos. Its mono capture buffer
has grown from 4,096 to 32,768 samples: 64 KiB total, an increase of 56 KiB.
It remains one independent buffer, with no delay/capture tape sharing and no
persistent-storage increase. At 120 BPM, the slowest capture is exactly 22,050
frames (500 ms); the fastest is 1/512. The buffer maximum is about 743 ms.

At 81–240 BPM the first key is 1/4. At 41–80 BPM it uses 1/8, and at 40 BPM it
uses 1/16. Each tempo range has its own logarithmic table ending at 1/512, so
keys and knob movement remain monotonic without duplicated/clipped rate positions.
The readout shows the actual frequency. Note that changing tempo across these
buffer-limit thresholds changes the available slow endpoint.

Schema 7 retains the same 12,268-byte save layout. Previous discrete and
continuous rates migrate to the nearest available musical rate; values faster
than the requested 1/512 limit are capped. Native tests verify the full
quarter-note capture, buffer wrap, all 127 rates at every BPM from 40–240,
held sweeps and both older save migrations.

Host stress check for 0.17.2 (Walk + Glitch + rapid VAR sweeps): 1.001 audio
seconds in .456 CPU seconds; maximum audio block 1.905 ms. Hardware validation
remains separate from these interpreter measurements.

## Preserved VAR and phrase mutations — 0.17.4

Switching any path mode preserves both VAR and DEPTH, including Breaks and
4 to the Floor. The automatic resets introduced in 0.17.2 are removed. VAR zero
at full Depth still selects the original base path, including both screenshot
templates. The default remains full Depth; lower Depth intentionally blends.

Shuffle, Chunks and Walk retain their previous algorithms exactly. Other modes
now use seeded local phrase copies, reversals, repeats, forward runs, skips and
alternations for nonzero VAR, rather than uniform vertical transposition. Each
VAR position is repeatable and independent of the direction you turn the knob.
4 to the Floor retains its source-zero quarter-note anchors at full Depth;
Breaks retains its opening source-zero anchor. These are the default variations,
with no extra setting to enable. Printing uses the same generated route.

Validation includes exhaustive bounded/deterministic route checks, proof that
nonzero variants of the changed modes are not simple uniform source shifts,
unchanged favorite-mode vectors for all lengths/variations and representative
depths, and UI tests that switching modes preserves settings and printed paths.
No stored pattern, audio buffer or save layout is changed.

## Predictable Reverse and Rotate — 0.17.5

Reverse and Rotate are exceptions to the phrase mutations introduced in 0.17.4.
At full Depth, VAR shifts Reverse's backward path or Rotate's forward path up
or down the source slices, wrapping within the pattern span. Direction and
ordering stay predictable. VAR zero retains each original base shape. Other
modes retain their current variation algorithms. Native tests cover every length,
VAR position and source step for both offset formulas.

## Full-strength defaults — 0.17.6

Fresh projects and factory FX scenes default Glitch Mix to 100%; Slice Depth
already defaults to 100%. The first Slice visit prepares an unconfigured zero
mix to 100%, so older zero-default projects show the ready value before a key
is pressed. Edits made during the session are preserved when returning to Slice.
The Glitch gate stays bypassed until a white key is held, regardless of Mix.
