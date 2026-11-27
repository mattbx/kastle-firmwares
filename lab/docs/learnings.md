# Learnings

Rules learned while building Kastle 2 firmwares, as "when you do X, do Y,
because Z", each with its evidence.

## Getting header-only DSP into RAM

When a portable header's per-sample functions need to run from RAM, mark them
`__attribute__((always_inline))` (through a macro the header leaves empty on
the desktop) and let them fold into the app's `FASTCODE AudioLoop`. Don't put
Bastl's `FASTCODE` on them: functions defined in a header are inline, inline
functions get their own sections, and GCC stops with "section type conflict".

Evidence: Lasting Damage, first build (2026-11-27). With `FASTCODE` on
`TapeLoop::Process` the build failed with that error; with `always_inline`,
`nm` shows no separate `Damage` symbol and `AppLastingDamage::AudioLoop` at
0x2003d198 (RAM) in the FASTCODE region, 4110 bytes.

## Anything written back into a buffer must finish its envelope

When an effect writes a shaped change into stored audio (a dip, a fade), let
the shape finish even if the effect is switched off half way, because the
stored buffer keeps a step where it stopped, and that step clicks on every
later pass.

Evidence: Lasting Damage's flakes (2026-11-27). The harness check "flakes dip
smoothly" measured a 5395-step jump on a DC loop when wear was turned off
mid-flake; letting a running flake finish brought the worst step to 286
(the triangle's own slope is 281).
