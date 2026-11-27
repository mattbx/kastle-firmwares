# kastle-firmwares

unofficial firmwares for the Bastl Kastle 2 - starting with lasting damage, a tape loop that wears out a little every pass.

The Kastle 2 is a small modular synth/effect built around an RP2040, and Bastl publishes its firmware under MIT ([bastl-instruments/kastle2](https://github.com/bastl-instruments/kastle2)). This repo is a fork of it with our own apps added, one per folder, each mapped onto the FX Wizard's printed panel so the labels still make sense. Bastl's own apps and files are left untouched, so their fixes keep merging in.

The firmwares are named after ships in Iain M. Banks' Culture novels.

**work in progress** - lasting damage builds cleanly and passes its desktop tests, but hasn't been tuned on the hardware yet. Controls and wear amounts may change.

## firmwares

| firmware | what it does | status |
|---|---|---|
| [lasting damage](../code/src/apps/LastingDamage/) | a disintegration loop: record about a second, and every pass writes it back duller, quieter and a little flaked, until it's a ghost in hiss | first build, not yet tried on the device |

## requirements

- a Bastl Kastle 2 (built for the FX Wizard panel)
- a USB-C cable
- to build: the ARM GNU toolchain 14.2 or later, the Raspberry Pi Pico SDK, CMake and Ninja (see [lab/docs/getting-started.md](../lab/docs/getting-started.md))

## install

There are no prebuilt UF2s here yet, so build one:

```
./configure.sh            # once
cd code/build
make lasting-damage       # writes output/kastle2-lasting-damage.uf2
```

Then flash it:

1. Power the Kastle 2 off, hold SHIFT and power it on. It boots into update mode.
2. Copy the `.uf2` onto the RPI-RP2 drive that appears. It reboots into the new firmware.

The bootloader lives in the RP2040's ROM, so a bad firmware can't brick it. To go back, flash the official FX Wizard UF2 from Bastl's Kastle 2 product page the same way.

## controls

Each firmware's README has its controls. Lasting Damage: [code/src/apps/LastingDamage/README.md](../code/src/apps/LastingDamage/README.md).

## how it's built

- each firmware keeps its DSP in one portable header with no Pico SDK in it, so a desktop harness ([lab/harness/](../lab/harness/)) can test the exact code the device runs, with checks whose answers are worked out from the maths first, and render audio from any WAV
- audio is 16-bit fixed point (the RP2040 has no floating-point unit), and the per-sample code runs from RAM
- `lab/` holds our docs: a hardware cheat sheet, the toolchain steps, the app IDs we use and the lessons learned along the way

## how it was made

Written with Claude (Anthropic's AI, in Claude Code), directed and reviewed by the maintainer. Claude wrote most of the code and docs. Each change goes through a pull request with the desktop harness's tests and a clean firmware build; anything that needs ears or the device is listed for the maintainer to check by hand.

## credits

- Bastl Instruments (Vaclav Mach, Marek Mach and the Bastl team) for the Kastle 2 hardware and its open firmware, [bastl-instruments/kastle2](https://github.com/bastl-instruments/kastle2), MIT. Our apps start from their app template and FX Wizard, and keep their copyright notice.
- Raspberry Pi's [Pico SDK](https://github.com/raspberrypi/pico-sdk), BSD-3-Clause, used to build.

This is unofficial firmware for the Kastle 2, not affiliated with or endorsed by Bastl Instruments. Problems with these firmwares belong here, not with Bastl.

## licence

Our code is MIT, like Bastl's, see `LICENSE`. Bastl's licence terms for their own material still apply, as set out in the root README: schematics and docs are CC BY-SA 4.0, the panel and case artwork are all rights reserved, and the Wave Bard factory samples (`SAMPLES.bin`) are all rights reserved and are not part of any firmware we publish.
