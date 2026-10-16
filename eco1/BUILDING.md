# Building ECO-1

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `amb.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `amb.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.9` makes a release build; the package is `build/amb-0.9.fwsc`.

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `EN_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `EN_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/` (the font). It fails if a soft-double routine is linked
(a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the instrument (`tests/host/amb_test.c`:
Euclidean patterns, scales and chords, voice leading, the render's levels, drift, evolving loops, freeze); flash
storage; Felucca's update-path tests against `build/amb.fwsc`; and the whole app in the simulator
(`tests/scenarios/*.amb`), with its screenshots and audio in `build/scenarios/`.

`host/build_host.sh` builds the simulator alone; `build/host/amb_host SCRIPT OUTDIR` runs one
script (the command list is at the top of `host/amb_host.c`).

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/amb.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## On GitHub

ECO-1 lives in `eco1/` of HYP-1's repository. HYP-1's workflow (`.github/workflows/firmware.yml`) builds it
on every push, after HYP-1: a release build numbered `0.B.N` (the run number; 0.x while ECO-1 is
experimental), the tests, the renders, and its pages under `eco/` of HYP-1's site
(<https://rustypawz.github.io/fm1-HYP1/eco/>). `tools/site/switcher.py` (HYP-1's) then adds the HYP-1 /
ECO-1 dropdown to every page. If ECO-1 fails to build, HYP-1's site goes out without it.

## Renders

All drawn from the firmware itself, run in the simulator:

```
sh host/build_host.sh
mkdir -p build/site_shots && build/host/amb_host tools/site/shots.amb build/site_shots > build/site_shots/log.txt
python3 tools/site/mockups.py          # docs/img/eco-*.png: the device, every screen, the sheets
python3 tools/site/panda.py            # docs/img/panda.png, favicon.png
python3 tools/gen_manual_figures.py    # docs/img/fm1-panel.svg: every control labelled
```

`mockups.py` needs `pip3 install cairosvg Pillow` and Barlow Semi Condensed installed (`assets/fonts`).
The browser emulator alone: `sh web/emu/build_clang.sh` (plain clang with wasm-ld) makes `build/emu/`.
