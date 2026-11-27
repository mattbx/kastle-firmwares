# Getting started

From nothing to our own firmware running on the Kastle 2, and back to stock.
Bastl's full guide is `TOOLCHAIN_INSTALL.md` at the repo root (macOS, Linux and
Windows); this is the short path for macOS with Homebrew.

You need Homebrew, CMake, Ninja and Python 3, plus the three things below: the
ARM compiler, the Pico SDK and picotool.

## 1. Install the toolchain (once)

```
brew install picotool
brew install --cask gcc-arm-embedded
arm-none-eabi-gcc --version        # needs 14.2.1 or later
```

If the cask fails, download "arm-none-eabi" for macOS Apple silicon from the
ARM GNU toolchain page, unpack it and add
`export PICO_TOOLCHAIN_PATH=<that folder>/bin` to `~/.zshrc`.

## 2. Get the Pico SDK (once)

Anywhere outside this repo works; `~/pico-sdk` here:

```
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
git -C ~/pico-sdk submodule update --init
echo 'export PICO_SDK_PATH=$HOME/pico-sdk' >> ~/.zshrc
```

Open a new terminal so the variable is set.

## 3. Build

From the repo root:

```
./configure.sh                     # once: makes code/build
cd code/build
make template                      # Bastl's bare template, a safe first build
ls output/                         # kastle2-template.uf2 (or similar)
```

Bastl builds with -O3 even for Debug: the audio code doesn't keep up without it.

## 4. Flash

1. Connect the Kastle 2 with USB-C.
2. Power switch off. Hold SHIFT and switch on. It boots into update mode (no sound).
3. Copy the .uf2 onto the RPI-RP2 drive that appears. Big files can take up to
   2 minutes. It reboots into the new firmware.

The bootloader is in the RP2040's ROM, so a broken firmware can't brick it: you
can always enter update mode and copy another UF2.

## 5. Go back to stock

Copy the official FX Wizard UF2 (from Bastl's Kastle 2 product page) the same
way. Do this once right after step 4, so the round trip is
proven before any of our code runs on the device.

## Debugging without soldering

- The debug pin on TX (GPIO0) toggles around the audio loop when
  `MEASURE_AUDIO_LOOP` is on in `config.hpp`. Read it with a scope or logic
  probe to see how much of each 1.1 ms block the DSP uses.
- USB serial output (`UsbSerial`) at 115200 baud.
- The three RGB LEDs.
- Step-through debugging needs wires soldered to the SWD pads, which voids the
  warranty. Not planned.
