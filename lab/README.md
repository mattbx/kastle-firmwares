# lab - our side of kastle-firmwares

Everything here is ours. Bastl's code lives in the rest of the repo, unchanged.

| Where | What |
|---|---|
| `docs/getting-started.md` | Toolchain, build, flash, and how to go back to stock |
| `docs/hardware.md` | The cheat sheet: chip, RAM and flash budgets, pins, codec, controls |
| `docs/learnings.md` | Rules we learned building firmwares ("when you do X, do Y, because Z") |
| `docs/app-ids.md` | Which app IDs our firmwares use |
| `harness/` | Desktop harness: our DSP on a computer, known-answer tests, WAV in, WAV out |
| `samples/` | Built-in sounds (our own or CC0) and the script that packs them (planned) |
| `renders/` | Renders and spectrograms, not committed |

## Our firmwares

| App folder | Name | Status | App ID |
|---|---|---|---|
| `code/src/apps/LastingDamage/` | Lasting Damage: a disintegration loop | first build, waiting on ears | 0x6F |

## Starting a new firmware

1. Pick a name. So far they are ship names from Iain M. Banks' Culture novels.
2. Run Bastl's `./create-app.sh <ShipName>` for their bare template, then
   follow Lasting Damage's pattern: the DSP in one portable header with no
   Pico SDK in it, tested in `harness/`, and a thin app around it.
3. Give it the next free ID in `docs/app-ids.md`.
4. Add it to the table above.

## Where else to look

- Bastl's own guides at the repo root: `GLOSSARY_EXAMPLES.md` (best intro to the
  app framework), `TOOLCHAIN_INSTALL.md`, `CODING_STYLE.md`, `FAQ.md`,
  `APP_LIST.md`.
- Bastl's other Kastle 2 repos on GitHub (`bastl-instruments`), such as Citadel.
- The Kastle 2 manuals and the stock firmware: Bastl's product page.
