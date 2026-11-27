# Kastle 2 hardware cheat sheet

Sources: Bastl's README, `code/src/common/config.hpp`, `Hardware.hpp`,
`code/board/memmap_kastle2.ld`, the FX Wizard source and manual v1.7 (read
2026-11-26). "Our arithmetic" marks numbers we worked out.

## The chip

- RP2040: two ARM Cortex-M0+ cores at 176 MHz (overclocked from 133 MHz so the
  clock divides evenly into the sample rate).
- **No floating-point unit.** `float` is emulated in software. Audio is fixed
  point: Q15 is an integer -32768..32767 standing for -1.0..1.0 (like storing
  money as integer cents). Multiply as integers, shift right 15. Helpers in
  `common/dsp/math/qmath.hpp`.
- 32x32 multiply gives only the low 32 bits, so Q15 (fits in 32-bit maths) is
  much cheaper than Q31 (needs 64-bit).
- CPU budget: 176 MHz / 44,000 Hz = **4,000 cycles per stereo sample per core**
  (our arithmetic). FX Wizard runs the effect on core 0 and the filters and mix
  on core 1.

## Memory

| What | Size | Notes |
|---|---|---|
| RAM | 264 kB | everything: code copied to RAM, buffers, stack |
| FASTCODE | 16 kB of that | functions copied to RAM for steady timing; AudioLoop lives here |
| Flash, program | 512 kB | 0x10000000 |
| Flash, user data | 7.5 MB | 0x10080000, read directly like memory (Wave Bard samples) |
| EEPROM | 256 bytes | AT24C chip, calibration and settings |

**RAM is the tape.** FX Wizard spends about 211 kB on delay lines, hence its
1.15 s stereo maximum. Treat about 200 kB as the audio budget (our arithmetic,
44 kHz unless noted):

| Storage format | Length in 200 kB |
|---|---|
| 16-bit stereo | 1.15 s |
| 16-bit mono | 2.3 s |
| 8-bit mono | 4.6 s |
| 8-bit mono at 22 kHz | 9.2 s |
| 4-bit ADPCM mono at 22 kHz | 18 s |
| 4-bit mono at 11 kHz | 36 s |
| 1-bit delta modulation mono | 36 s |

Use `q15least_t` (real 16-bit storage) for big buffers; `q15_t` is 32-bit for speed.

**Flash at runtime** is read-only in practice. Writing means erasing 4 kB
sectors first, and while flash is erased or written neither core can read
anything from it (code or tables). Nothing in Bastl's code does it. Untested.

## Audio

- 16-bit stereo at **44,000 Hz** (not 44,100), NAU88C22 codec over I2S.
- Blocks of 48 stereo frames, about 1.1 ms. `AUDIO_LOOP_RATE` = about 916 Hz.
- Input: jack and AUDIO patch points, up to 6 Vpp, gain up to +12 dB,
  jack-detect (`AUDIO_IN_JACK_DETECT`).
- Output: headphones up to 250 ohm.
- The codec has hardware EQ (5 band), notch filters, automatic level control
  and a 3D widener over I2C. No CPU cost, but slow, steppy changes.

## Controls and patching

- 7 pots (read at about 100 Hz), 2 buttons (SHIFT, MODE) giving 6 layers
  (normal, shift, mode, settings and their shifted forms), 3 RGB LEDs.
- CV in: 0-5 V, 12-bit. 2 direct inputs above 1 kHz, 16 multiplexed ones at
  about 100 Hz. Values 0-4095 (Citadel: -4095 to about 5733).
- CV out: 3 analog (LFO triangle, envelope, sequencer CV), 10-bit PWM, 0-1023 as
  a share of the supply (lower on batteries).
- Gate: 3 digital outs (gate, sync, pulse), 3 digital ins (trigger, reset,
  sync). Digital outs are plain pins and can switch every sample.
- USB MIDI in and out. Knobs send CCs 14-18, 22, 23 by default.

FX Wizard pot roles (the printed panel): TIME, TIME MOD, FEEDBACK, FEEDBACK
MOD, AMOUNT, AMOUNT MOD, LFO, LFO MOD; STEREO and FILTER on the shift layer;
FX MODE button. Map new firmwares onto these labels so the panel still reads.

## Software framework

Subclass `App`: `Init`, `AudioLoop` (real time, FASTCODE), `UiLoop` (knobs,
LEDs), optional `SecondCoreWorker`, `MidiCallback`, `MemoryInitialization`.
`Base` runs the LFO, clock, sequencer and gain. `FancyPot` and `FancyMode`
handle pots, layers, MIDI CCs and saved settings. DSP classes: delay lines,
SVF and DJ filters, bit crusher, clippers, sample player, envelopes, followers.
