# Desktop harness

Our firmwares' DSP compiled for a computer (macOS or Linux), with no
Pico SDK. Each firmware keeps its DSP in a portable header, so what the
harness tests is exactly what the device runs.

```
cmake -S lab/harness -B lab/harness/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build lab/harness/build
ctest --test-dir lab/harness/build --output-on-failure
```

The build also compiles each DSP header on its own under strict warnings
(`-Wconversion -Wsign-conversion -Werror`) with `-fno-exceptions -fno-rtti`,
as a portability check.

## Lasting Damage

- `lasting_damage_tests`: 14 known-answer checks. Each states its expected
  number from the maths first: the null case (wear off repeats bit for bit),
  the seam bake (DC stays DC, no click on a sine), speed (2x halves the period,
  0.5x interpolates halfway, -1x reverses), and each kind of wear (level falls
  by keep^passes, no creep below zero, the hiss floor settles at
  0.5 / sqrt(1 - keep^2), dulling matches the one-pole gain a / (2 - a),
  flakes never step by more than their slope), plus seeding and auto-close.
- `lasting_damage_render <input.wav> <out-dir>`: renders null, gentle, medium,
  heavy and varispeed scenarios from any 16-bit WAV (44 kHz is the Kastle's
  rate). Each plays half a second of input, records one second, then wears.

To see a render without listening:

```
ffmpeg -y -i x.wav -lavfi showspectrumpic=s=1400x500:legend=1:scale=log x-spec.png
```
