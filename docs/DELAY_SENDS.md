# Shared delay sends — 0.15.0

Each track has an independent 0–127 send to the existing single global delay.
Dry voices remain audible at zero send. Delay return joins the dry mix before
the existing global drive, bit/hold processing and cutoff/resonance. Performance
punch effects still process the resulting master signal afterward.

Repeated HOME presses cycle LEVELS → TRACK → D SEND. The D SEND mixer shows all
eight sends as percentages; KN1–KN4 edit the underlined bank of four tracks.
ALGORITHM switches banks. Level meters continue to show dry voice activity;
selecting another track, muting or soloing uses the existing controls. Send
levels follow track volume, include all polyphonic notes, and respect mute/solo.
They feed a mono delay input, as the original shared delay did.

Keep the global Delay Mix above zero to hear returns; it still starts at zero
on fresh projects. For a dry kick with echoed snare, set kick send to zero,
snare send to taste, and raise global Delay Mix. The filter/drive apply to both.
Sends default to 127 for all tracks, retaining existing full-mix delay routing.
Reducing a send stops new input without clearing feedback or existing tails.

SAVE retains all eight sends. They are project mixer settings shared across
patterns; sound/kit/FX-scene recall leaves them alone. They can be MIDI learned
from the D SEND controls; mappings stay pinned to their chosen track/channel.
No send p-lock or LFO destination is added in this pass.

Schema 5 appends eight bytes to schema 4. Save size: 12,268 / 12,276 bytes;
eight bytes remain. Older saves migrate with sends at 127. Patterns, chords,
p-locks, custom presets and all sixteen MIDI maps retain their capacities.
The existing delay RAM remains 65,536 bytes; there are no per-track delay buffers
or processor instances. Eight control bytes plus a fixed engine accumulator are
added in RAM; voice routing adds multiply/divide-and-accumulate work only.

Validation: native ASan/UBSan routing tests isolate kick/snare sends, ensure all
eight tracks remain dry-audible at zero send, check tail preservation and master
filter processing; WASM tests edit all eight sends, save/reload, import schema-4
saves, and MIDI learn a send. Existing native/integration suites also run.

Host wasm3 measurements: demo .201 CPU seconds for 1.001 audio seconds (maximum
block 1.415 ms); chain stress 2.259 CPU seconds for 6.008 audio seconds (maximum
block 1.900 ms). Same-host pre-send chain run: 2.171 CPU seconds. These are host
measurements with scheduling variability, not hardware real-time guarantees.

## Factory routing — 0.15.1

The delay-enabled factory kits and scenes now recall these send values (0–127):

| Scene / corresponding kit | Kick | Snare | C Hat | O Hat | Clap | PRC | Glitch | Analog |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Dust Delay / Dust Works | 0 | 48 | 8 | 24 | 36 | 44 | 64 | 0 |
| Metal Space / Metal Room | 0 | 64 | 0 | 32 | 48 | 64 | 80 | 0 |
| Tape Damage / Acid Tape | 0 | 40 | 0 | 16 | 32 | 48 | 64 | 0 |

Factory kit recall additionally dries PRC if that kit's PRC tune is below the
control-byte-40 bass threshold (about -9 semitones). Factory scene recall uses
the fixed profile, leaving voice parameters untouched. ORIGINAL/CLEAN and
individual voice recall leave current sends alone. Custom presets continue to
leave project-level sends untouched; SAVE stores the project routing. No save
layout or capacity changes. A/B for kit/scene loads swaps the prior send levels
with the sound using the existing audio-block mailbox, without resetting delay
tails. Reload a factory entry to apply its new routing to an existing project.
