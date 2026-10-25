# PERF-1 licensing

PERF-1 (by deamonSLINKY) is a fork of HYP-1 by RedtailWorks, itself a fork of Charles Vestal's AMB-1. It is free software under the GNU General Public
License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is built on Felucca's platform layer (through X0X and FoMni)
and keeps Felucca's licence. If you distribute PERF-1, or firmware derived from it, you must give
your recipients its complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x) and FoMni, then AMB-1 | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `host/amb_host.c`, `web/emu/`, `tools/manual_html.py` | adapted from X0X (itself from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| The six-operator FM engine and the DX7 voices (`firmware/src/dsp/fm6.inc`, `fm6_core.inc`, `fm6_tables.h`, `fm6_rom.h`, `fm6_patches.h`), the second core's launch and job handoff (`firmware/hal/fm1_core1.h`), the safe-core hold | **Melodee** by Kerem Kilic (Ellic Studio), Copyright (C) 2026 Kerem Kilic; its synthesis is after **Dexed** (Pascal Gauthier, GPL-3.0-or-later) on **MSFA** (Raph Levien / Google, Apache-2.0), and its tables are Dexed's; the source headers keep these notices | GPL-3.0-only (Apache-2.0 for MSFA's parts) |
| Dattorro's plate reverb in `firmware/src/dsp/amb.c` | from [FoMni](https://github.com/charlesvestal/fm1-fomni) | GPL-3.0-only |
| The rescue route's protocol and flash loader (`tools/fm1_rescue.*`) | [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter)'s protocol and kagaimiq's jl-uboot-tool | see those projects |
| What is known about the FM-1's hardware | the [FM-1-RE](https://github.com/AL-255/FM-1-RE) reverse-engineering work | credit only |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| Everything else in `firmware/src/{app,dsp}`, `tests/` | AMB-1 (Charles Vestal), HYP-1 changes (RedtailWorks), PERF-1 changes (deamonSLINKY) | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Chakra Petch (Cadson Demak), the screen's face and the site's headings | SIL OFL 1.1 | `assets/fonts/ChakraPetch-*.ttf`, `assets/fonts/ChakraPetch-OFL.txt` |
| Barlow Semi Condensed (The Barlow Project Authors), the site's text and labels | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.
"M-VAVE" and "FM-1" are trademarks of their respective owners. PERF-1 is independent firmware; it is
not affiliated with, endorsed by or supported by any of them.

## Radio

PERF-1 never enables the Bluetooth / Wi-Fi radio of the hardware.
