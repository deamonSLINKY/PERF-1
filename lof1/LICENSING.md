# LOF-1 licensing

LOF-1 is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is built on Felucca's platform layer (through X0X and FoMni)
and keeps Felucca's licence. If you distribute LOF-1, or firmware derived from it, you must give
your recipients its complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x) and FoMni, then AMB-1 | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `host/amb_host.c`, `web/emu/`, `tools/{make_pages,manual_html}.py`, `tools/publish_pages.sh` | adapted from X0X (itself from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| Dattorro's plate reverb in `firmware/src/dsp/amb.c` | from [FoMni](https://github.com/charlesvestal/fm1-fomni) | GPL-3.0-only |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| The band (`firmware/src/dsp/amb.c` but its plate), the scenes, the CRT and the screensaver (`firmware/src/app/{scene,crt}.c`), the pages, the boot cards, `tests/`, `tools/site/`, `tools/gen_saver.py`, `tools/samples/` | LOF-1 (RedtailWorks), from AMB-1 by way of HYP-1 and LOF-1 | GPL-3.0-only |
| The screensaver's art (`tools/art/`, `firmware/src/app/saver_art.h`): Red, RedtailWorks' red panda | RedtailWorks | GPL-3.0-only, with this tree |
| `firmware/src/app/` and the rest not named above | AMB-1 (Charles Vestal), changed for LOF-1 | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| The sampled instruments in `firmware/src/dsp/lof_samples.h` (re-encoded by `tools/samples/lof1_samples.py`): Versilian Studios' Community Sample Library (VCSL) and VSCO-2 Community Edition | CC0 1.0 | <https://github.com/sgossner/VCSL>, <https://github.com/sgossner/VSCO-2-CE> |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.
"M-VAVE" and "FM-1" are trademarks of their respective owners. LOF-1 is independent firmware; it is
not affiliated with, endorsed by or supported by any of them.

## Radio

AMB-1 never enables the Bluetooth / Wi-Fi radio of the hardware.
