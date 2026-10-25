# Building PERF-1

PERF-1 builds on Linux x86-64 (or macOS/Windows through Docker or WSL: JieLi's toolchain is Linux x86-64 only). A release build makes:

| File | What |
| --- | --- |
| `build/amb.bin` | the firmware app |
| `build/loader/ota.bin` | the update loader |
| `build/amb-VERSION.fwsc` | the installable package (app + loader) |
| `build/emu/` | the browser emulator (WebAssembly) |

## Prerequisites

- Python 3 with Pillow (`pip3 install Pillow`).
- **The JieLi Linux toolchain** (clang 4.0.1 for pi32v2, from JieLi's package server): `tools/get_toolchain.sh` installs it to `~/.jieli/toolchain` (`JIELI_TOOLCHAIN` overrides). On macOS or with `JIELI_DOCKER=1` each tool runs in a `linux/amd64` `debian:bookworm-slim` container.
- **The JieLi AC79 SDK** (Apache-2.0). The package uses three of its files (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree:

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

  (`AC79_SDK` overrides the location.)
- A clang with the `wasm32` target (for the emulator), and Node.js (the web tests).

## Build and test

```
tools/release.sh 0.13.0            # parameter data, the manual's page, the tests, the package, the emulator
tools/release.sh 0.13.0 --no-tests # without the tests
./build.sh                        # a development build (version DEV)
tests/run_tests.sh                # the tests alone
```

The tests run, on the build machine: the maths library against libm; the instrument (`tests/host/amb_test.c`); flash storage; the SysEx protocol and the editor's parameter and DX7 voice code; Felucca's update path against the package; the dual-core job handoff (identical output with the work on either core); and the whole app in a simulator (`tests/scenarios/*.amb`, about 70 scripts), with screenshots and audio in `build/scenarios/`. `host/build_host.sh` builds the simulator alone; `build/host/amb_host SCRIPT OUTDIR` runs one script (the command list is at the top of `host/amb_host.c`).

The build fails if a soft-double routine is linked (a `double` crept in) and checks the image, RAM and pool sizes. Build options: `EN_CORE2=0` builds single-core (the reverb on the second core is the default), `EN_CDC=1` adds Felucca's USB serial function.

## The site

```
python3 tools/make_pages_site.py 0.13.0 build/site
```

makes the landing page, the web installer with the package in it, the emulator, the editor and the manual as a static folder with relative links. Publish it with GitHub Pages (a `gh-pages` branch holding the folder's contents works) or any static host.

## Install

From the site, or from the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/amb-0.13.0.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT- and OCT+ for 5 seconds for the update mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB), `tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## What protects an install, and what does not

The installer's checks (identity read from the package's own markers, the loader mark, the CRCs, the head-equal check, the identity read back after the reboot) and the firmware's safe mode (two failed boots, then USB-only, then UBOOT by SysEx or OCT- + OCT+ held for five seconds) cover a bad *app*. They do not cover a corrupt start-up sector, a bad SPL, or power lost partway through the write with no resume record: no hardware recovery (ISP pads, boot strapping, a key chord) has been demonstrated for this chip, and the interrupted-write safety is by design, not yet tested with a power cut. Keep a way to restore the stock firmware to hand, and do not use `--force` on a package that was not built by this tree.

## Where things are

- `firmware/src/dsp/` the engine (`amb.c`: voices, drums, effects, the master bus; `fm6*.inc`: the DX7-style FM engine), `firmware/src/app/` the UI and the project (`ui.c`, `project.c`, `viz.c`), `firmware/hal/` the hardware layer, `firmware/loader/` the update loader.
- `web/` the installer, emulator, editor; `tools/` build, package, install, telemetry (`fm1log.py`, `fm1sum.py`); `docs/MANUAL.md` the manual.
- Stored data: `firmware/src/app/project.c` (the packed 376-byte piece records, one flash object per genre), `firmware/src/storage.c` (the A/B sector objects and the flash map).
