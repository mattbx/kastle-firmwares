# App IDs

Each Kastle 2 firmware has a one-byte ID (`GetId()`), which keys its saved
settings. Bastl uses 0xB0-0xCF (official) and 0xD0-0xEF (their other apps).
Community firmwares pick from 0x10-0x9F (see `APP_LIST.md`); 0x3D is taken by
Knoscillator. Listing ours upstream is optional and public, so the maintainer decides.

| ID | Firmware | Since |
|---|---|---|
| 0x6F | Lasting Damage (`code/src/apps/LastingDamage/`; reserved for a starter app, used by the first firmware instead) | 2026-11-27 |
