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
| FEEDBACK | Wear. Fully left: the loop repeats forever, bit for bit. Fully right: mostly gone in about 10 passes. |
| FEEDBACK MOD | How much the FEEDBACK CV moves the wear. |
| AMOUNT | Dry/wet. Fully right is the loop only. |
| SHIFT + AMOUNT | How much the AMOUNT CV moves the dry/wet. |
| SHIFT + FEEDBACK (Filter) | Bastl's DJ filter on the loop, as on FX Wizard: lowpass to the left, highpass to the right, off in the middle. It only colours what you hear; the tape itself isn't filtered. |
| SHIFT + FEEDBACK MOD (Stereo) | Stereo spread: the right channel is read by a second head up to 150 ms behind the first. The first half of the knob is a few ms (wider), the top end a short echo. Moving it bends the right channel's pitch for a moment, like sliding a tape head. |

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
- Wear has five parts, all from the FEEDBACK knob. At full wear: level loss
  (keep 0.85 per pass), dulling (a one-pole lowpass run along the tape),
  flakes (one per 4000 frames, 3 to 49 ms long, a full dropout at the deepest),
  fresh tape hiss each pass (white noise up to +-256 steps, darkened by the
  dulling like the music), and re-rounding with TPDF dither.
- The damage comes before the fade: at half wear the level drops only 0.16 dB
  a pass, while the hiss builds and the flakes pile up. Level loss grows with
  wear cubed, dulling, flakes and hiss with wear squared, so the bottom half of
  the knob is gentle. Tuned once by ear (2026-11-27); still open to change.

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
- The filter runs on the same core as the tape. It fits on paper, but the audio
  loop hasn't been timed on the device yet.
