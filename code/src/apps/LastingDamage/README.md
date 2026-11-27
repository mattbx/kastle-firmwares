# Lasting Damage

A disintegration loop for the Kastle 2 FX Wizard hardware. Record about a
second of audio into RAM, and every time the loop plays, the head writes it
back a little damaged: duller, quieter, now and then hit by a flake (a short
dip in level), and re-rounded with a little noise. The damage builds up pass
after pass until the loop is a ghost in a bed of hiss.

Named after a ship in Iain M. Banks' Culture novels.

## Controls

Uses the FX Wizard panel's printed labels.

| Control | Does |
|---|---|
| MODE, tap | Record. Tap again to close the loop and start playing it. Tapping while a loop plays records a new one. |
| MODE, hold about a second | Reverse the tape (LEDs turn cyan). Hold again to go forwards. |
| TRIG IN | Same as a MODE tap, so a clock or gate can record and close loops. |
| TIME | Tape speed, half to double. 1x sits in the middle, with a small flat spot. |
| TIME MOD | How much the FREE and STEP CVs move the speed. Middle = none, left inverts. |
| FEEDBACK | Wear. Fully left: the loop repeats forever, bit for bit. Fully right: mostly gone in about 8 passes. |
| FEEDBACK MOD | How much the FEEDBACK CV moves the wear. |
| AMOUNT | Dry/wet. Fully right is the loop only. |
| SHIFT + AMOUNT | How much the AMOUNT CV moves the dry/wet. |

Everything else (input and output gain, tempo, LFO, rhythm, swing, settings)
is Bastl's shared Kastle 2 layer, unchanged.

LEDs: dim white when empty (the input passes straight through), red while
recording, orange while looping with a bright flash at the start of each pass
(cyan when reversed).

The longest loop is 1.08 s (48,000 frames minus a 10 ms seam). Recording past
that closes the loop by itself.

## How it works

- `TapeLoop.hpp` is the whole DSP, in one portable header with no Pico SDK in
  it, so the desktop harness (`lab/harness/`) tests the exact code the device
  runs. Its comments explain each step.
- The loop seam is baked once: when you close the loop, 10 ms more is
  recorded and crossfaded into the start, so the end flows into the beginning
  on every pass.
- The wear head rides on the play head and damages each frame it passes, once
  per pass, in either direction. Faster playback wears faster, like real tape.
- Wear has four parts, all from the FEEDBACK knob: level loss (keep 1.0 down
  to 0.75 per pass), dulling (a one-pole lowpass run along the tape), flakes
  (up to one per 2000 frames, 1.5 to 25 ms long, 0.3 to 0.9 deep) and
  re-rounding with TPDF dither. These numbers are first guesses to tune by ear.

## Build and flash

```
cd code/build
make lasting-damage
```

The UF2 is `code/build/output/kastle2-lasting-damage.uf2`. To flash: power
off, hold SHIFT, power on, copy the UF2 to the RPI-RP2 drive. Go back to stock
any time with the official FX Wizard UF2.

App ID 0x6F (see `lab/docs/app-ids.md`).

## Known limits

- Speed changes use linear interpolation, which adds faint mirrored images
  (around -80 dB) when slowing down. Lo-fi on purpose, for now.
- No settings are saved yet: after power-off the loop and the direction are
  gone (idea J in the plan covers committing a loop to flash).
- SHIFT + FEEDBACK MOD (Stereo) and SHIFT + FEEDBACK (Filter) do nothing yet.
