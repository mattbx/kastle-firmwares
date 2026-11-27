# CLAUDE.md - kastle-firmwares

Read this first, then `lab/README.md` and `lab/docs/learnings.md`. This repo is
built with Claude Code (see "How it was made" in `.github/README.md`), and this
file is the brief every session starts from.

## What this is

Unofficial firmwares for the Bastl Kastle 2 (RP2040). This repo is a GitHub
fork of `bastl-instruments/kastle2` (MIT) with our own apps added. Bastl's repo
is the `upstream` remote (fetch only, pushing to it is disabled); pull their
fixes with `git fetch upstream && git merge upstream/main` on a branch.

- Bastl's files: everything outside `lab/`, `.github/` and our app folders.
  Leave them unchanged so upstream merges stay clean. Their root README.md is
  theirs; our front page is `.github/README.md`.
- Ours: `lab/` (docs, harness, samples, renders), `.github/README.md`, this
  file, and one folder per firmware in `code/src/apps/<Name>/` (the build needs
  apps there).
- Level: Standard plus merges. Every change goes through a branch and a PR to
  main. The maintainer has said (2026-11-27) that Claude may create repos,
  push, review and merge PRs for the Kastle firmware projects. Merge only once
  the build passes and anything needing ears or the device is listed for the
  maintainer. Never push straight to main, never force push, never tag.

## Commands

See `lab/docs/getting-started.md` for the toolchain. Build one app:

```
cd code/build
make <app-name>            # kebab-case, e.g. make fx-wizard
ls output/                 # the .uf2 files
```

Flash: power off, hold SHIFT, power on, copy the .uf2 to the RPI-RP2 drive.
Go back to stock any time with the official UF2 from Bastl's product page.

## Hard rules

- **Licence.** Bastl's code is MIT: keep their copyright and licence notice in
  every file copied or adapted from theirs. Docs and schematics are CC BY-SA 4.0.
- **Never ship Bastl's samples.** `code/src/apps/WaveBard/SAMPLES.bin`, panel and
  case artwork are all rights reserved. Never publish a UF2 or release that
  bundles SAMPLES.bin. Built-in sounds are our own recordings or CC0, kept in
  `lab/samples/` with their source and licence.
- **Unofficial.** Say "unofficial firmware for the Kastle 2", never anything
  that reads as made or endorsed by Bastl.
- **No GPL code**, no decoded commercial material, no manuals in this repo.
- **Public repo: no personal details.** No home paths, emails, names or
  pointers to private notes in files, commits or PRs. Commit as the GitHub
  noreply address.
- **Fixed point in the audio path.** No floating-point unit on the RP2040: audio
  is Q15 (`q15_t`, `q15_mult`...). Float is fine once per block for knob maths.
- **AudioLoop runs from RAM** (`FASTCODE`, 16 kB total). No allocation, no
  flash writes, no blocking calls in it.
- **App IDs** for our firmwares come from `lab/docs/app-ids.md` (community
  range 0x10-0x9F). Record a new one there before first use.

## How we verify

1. Build after every edit (`make <app>`), with no new warnings.
2. Prefer checks whose answer you can state beforehand, run on the desktop
   render harness (`lab/harness/`) before the device. The null case first:
   effect off must give bit-identical passthrough.
3. Time the audio loop on the debug pin (`MEASURE_AUDIO_LOOP`) when adding DSP.
4. Anything that needs ears or the device goes in the PR as "for the
   maintainer to check", with what to listen for and the exact build and flash
   steps.
5. A gotcha that cost time goes in `lab/docs/learnings.md` as a rule, in the
   same commit as its fix.

## Working with the maintainer

Experienced product designer and developer, fluent in TypeScript, learning C++
and DSP: explain a concept briefly the first time it comes up, in
TypeScript-friendly terms. Small steps, one at a time. Short hyphens, never em
dashes.
