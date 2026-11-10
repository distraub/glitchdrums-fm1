# Skiff v2.2 performance findings

Reviewed the exact [v2.2 tag](https://github.com/illuminus5/skiff/tree/v2.2),
including `firmware/src/voice.c`, `fx.c`, `audio.c` and the MIDI Learn/controller
files. These are source observations and candidate optimizations, not measured
speedups of a DSP port into GlitchDrums.

## Useful techniques

1. **Prepare control data once per block.** Skiff's
   [track renderer](https://github.com/illuminus5/skiff/blob/v2.2/firmware/src/voice.c)
   advances track LFOs, computes envelope coefficients/modulation/pitch and
   dispatches a voice's block renderer. Our `gd_prepare` runs every 32 frames
   and copies 32 parameters/zeros modulation for all fourteen slots, including
   inactive ones. It recomputes tempo/rate-derived LFO increments repeatedly.
   First candidates: cache increments per track, cache unchanged coefficients,
   and skip inactive parameter/synthesis preparation while retaining LFO clocks
   and display motion. Preserve each voice's existing phase and p-lock behavior;
   sharing Skiff's track-LFO model wholesale would change our semantics.
2. **Hoist invariant work out of sample loops.** Our master FX recomputes
   tempo-derived delay distances in its sample helper; much of that is constant
   until a block-boundary command changes tempo or controls. Prepared block
   coefficients are a promising low-risk target, with bit-identical output as
   the validation criterion. Compiler output/profiling should confirm what is
   already hoisted before attributing a gain.
3. **Park silent processors after tails decay.** Skiff's
   [mix-part path](https://github.com/illuminus5/skiff/blob/v2.2/firmware/src/fx.c)
   skips insert/distortion/mix work once voices and their tails finish. We
   already skip inactive voice synthesis and idle punch processors, but master
   delays and some control preparation continue. Extend parking only with
   explicit tail/fresh-capture rules; a fast bypass that revives stale capture
   audio would undo the earlier Comb Freeze/capture fixes.
4. **Share expensive ambience and reduce its rate where appropriate.** Skiff's
   HALL runs its main network at half rate. Our signature ambience already uses
   shared capture and half-rate processing, so that technique is already in
   place. More rate reduction would change the sound and needs listening tests.

## Hardware advantages are a separate comparison

Skiff's source defines a shared voice budget: eight by default, twelve with its
optional second core. Fourteen engine types do not mean fourteen arbitrarily
expensive simultaneous synth voices. Our allocation allows six drum voices plus
four Glitch and four Analog voices, with the current quality/oversampling cost.
Compare the same voice workload and effects before interpreting CPU percentages.

Skiff's optional second-core mixer distributes parts across cores and has a
fallback if the second core fails. That is a hardware/HAL project, not a speedup
available to the current single-instance RemoteFM1 wasm3 execution model. Its
clock configuration and native memory placement likewise cannot be treated as
interpreter gains. The actual FM-1 hardware build still needs its own timing,
RAM map and recovery validation.

## Small improvement included in 0.12.0

MIDI feedback now draws at about 30 Hz, and mixer feedback avoids drawing the
same screen twice per update. In the controller-stream scenario, the desktop
check moved from 0.228 to 0.213 CPU seconds per audio second after that change;
normal playback remained around 0.194. This is specific to the measured UI
scenario, not an overall DSP speedup claim. Source audio algorithms and default
rendered PCM remain unchanged.

Next DSP pass should baseline cold/idle, demo, four-note Glitch/Analog and heavy
FX stacks, then change one cache/preparation path at a time. Keep exact PCM tests
where possible and compare attack timing, LFO behavior and delay/capture tails
when a change cannot be bit-identical. No DSP optimization from this list has
been silently folded into the MIDI feature release.
